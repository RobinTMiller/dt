/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2020			    *
 *			   This Software Provided			    *
 *				     By					    *
 *			  Robin's Nest Software Inc.			    *
 *									    *
 * Permission to use, copy, modify, distribute and sell this software and   *
 * its documentation for any purpose and without fee is hereby granted,	    *
 * provided that the above copyright notice appear in all copies and that   *
 * both that copyright notice and this permission notice appear in the	    *
 * supporting documentation, and that the name of the author not be used    *
 * in advertising or publicity pertaining to distribution of the software   *
 * without specific, written prior permission.				    *
 *									    *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 	    *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN	    *
 * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
 * THIS SOFTWARE.							    *
 *									    *
 ****************************************************************************/
/*
 * Module:	dtutil.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	Utility routines for generic data test program.
 * 
 * Modification History:
 * 
 * October 27th, 2020 by Robin T. Miller
 *      Add functions in support for additional trigger functionality.
 * 
 * September 6th, 2020 by Robin T. Miller
 *      For disk devices, when multiple threads are chosen, convert threads
 * to slices to avoid *false* data corruptions, due to thread overwrites.
 * This is happening due to folks selecting FS workloads instead of disk!
 * 
 * May 20th, 2020 by Robin T. Miller
 *      Update API's using popen to capture stderr along with stdout!
 * 
 * May 19th, 2020 by Robin T. Miller
 *      Update ExecuteCommand() to add prefix flag to control log prefix.
 * 
 * May 19th, 2020 by Robin T. Miller
 *      When not compiled with SCSI library, use dt's spt path as default.
 * 
 * May 13th, 2020 by Robin T. Miller
 *      Add the thread number to the pass command, since oftentimes we only
 * wish to perform an action on the first thread, and not all threads.
 * 
 * August 17th, 2019 by Robin T. Miller
 *      Update OS block tick conversion to avoid negative values, seen after
 * ~597 hours on Windows, calculated values went negative for "%et" format.
 * The new format is dayshoursminutessecondsfraction or ddhhmmss.ff
 * 
 * September 1st, 2017 by Robin T. Miller
 *      Update get_data_size() to handle separate read/write block sizes.
 * 
 * June 9th, 2015 by Robin T. Miller
 * 	Added support for block tags (btags).
 *
 * February 7th, 2015 by Robin T. Miller
 * 	Updated pattern file function to use OS specific API's.
 *
 * February 5th, 2015 by Robin T. Miller
 * 	Added several functions for file locking support.
 *
 * August 6th, 2014 by Robin T. Miller
 * 	Update variable I/O request function, get_variable(), to ensure
 * the length is modulo the disk device size, not the pattern buffer size.
 * This caused invalid requests when using 4 byte data patterns (IOT Ok).
 *
 * February 20th, 2014 by Robin T. Miller
 * 	For triggers panic'ing *all* nodes, set the return status to  
 * TRIGACT_TERMINATE, so jobs/threads will get stopped. This is not done
 * for all triggers, since we may wish to continue on one node panic'ing
 * and/or when performing triage. Note: Triage will use its' own status.
 * Note: This is *only* used for noprog's, errors have their own limit.
 * 
 * December 13th, 2013 by Robin T. Miller
 * 	Simplify and fix random offset calculations in do_random(). The bug
 * was rounding up the offset to the random limit, so writes would encounter
 * a premature end of media (ENOSPC) with raw disks. The rounding also caused
 * occasional short writes, which could result in reading too much and causing
 * a false data corrruption (reading more than written, will do that! :-().
 * 
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */
#include "dt.h"
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

#if !defined(WIN32)
#  include <pthread.h>
#  include <poll.h>
#  include <signal.h>
#  include <strings.h>
#  include <sys/param.h>
#  include <netdb.h>		/* for MAXHOSTNAMELEN */
#  include <sys/time.h>		/* for gettimeofday() */
#  include <sys/wait.h>
#endif /* !defined(WIN32) */

/*
 * External References:
 */
extern char *spt_path;		/* Defined in dtscsi.c */

/*
 * Forward References:
 */
void convert_clock_ticks(clock_t ticks, clock_t *days, clock_t *hours,
			 clock_t *minutes, clock_t *seconds, clock_t *frac);
int ExecuteBuffered(dinfo_t *dip, char *cmd, char *buffer, int bufsize);
int vSprintf(char *bufptr, const char *msg, va_list ap);
void report_EofMsg(dinfo_t *dip, ssize_t count, os_error_t error);

static char *bad_conversion_str = "Warning: unexpected conversion size of %d bytes.\n";

/*
 * mySleep() - Sleep in seconds and/or microseconds.
 *
 * Inputs:
 * 	dip = The device information pointer.
 *	sleep_time = The number of seconds/useconds to sleep.
 *	The global micro_delay flag if set selects microdelays.
 *
 * Return Value:
 *	void
 */
/* Note: RAND_MAX is 32767, so call it twice! */
#define os_random()	( (rand() << 16) + rand() )

/* TODO: Add sleep_min and sleep_max options! */
static unsigned int sleep_max = 10;	/* Random max seconds to sleep. */

void
mySleep(dinfo_t *dip, unsigned int sleep_time)
{
    unsigned int ms, timeout;

    /*
     * Allow a random delay. 
     */
    if (sleep_time == RANDOM_DELAY_VALUE) {
	/* Note: Not using get_random() to avoid our random generator. */
	/*       The reason being, we too may get called randomly! */
	sleep_time = (unsigned int)os_random();
	if (dip->di_sleep_res == SLEEP_USECS) {
	    sleep_time %= uSECS_PER_SEC;
	} else if (dip->di_sleep_res == SLEEP_MSECS) {
	    sleep_time %= mSECS_PER_SEC;
	} else { /* SLEEP_DEFAULT or SLEEP_SECS */
	    sleep_time %= sleep_max;
	}
    }
    /*
     * Note: Convert microseconds (us) to milliseconds (ms), until
     * we have true microsecond delays again (see notes below).
     */
    if (dip->di_sleep_res == SLEEP_MSECS) {
	timeout = sleep_time;		    /* Sleep is in msecs. */
    } else if (dip->di_sleep_res == SLEEP_USECS) {
	timeout = (sleep_time / MSECS);     /* Convert usecs to msecs. */
    } else { /* SLEEP_DEFAULT or SLEEP_SECS */
	timeout = (sleep_time * MSECS);	    /* Convert secs to msecs. */
    }
    if (timeout == 0) timeout++;	    /* Minimum of one msec! */

    if (dip->di_timerDebugFlag) {
	Printf(dip, "Delaying for %ums (or %.2f secs)...\n",
	       timeout, ((float)timeout / (float)MSECS));
    }
    do {
	/* Do short sleeps so we can detect when we're terminating! */
	ms = min(MSECS, timeout);
	(void)os_msleep(ms);
	if ( PROGRAM_TERMINATING || THREAD_TERMINATING(dip) ) break;
	timeout -= ms;
    } while (timeout);
    return;
}

/*
 * Simple function to sleeping in seconds (optionally random).
 *
 * Note: We don't use sleep() to avoid signal issues!
 */
void
SleepSecs(dinfo_t *dip, unsigned int sleep_time)
{
    unsigned int ms, timeout;

    /*
     * Allow a random delay. 
     */
    if (sleep_time == RANDOM_DELAY_VALUE) {
	sleep_time = (unsigned int)os_random();
	sleep_time %= sleep_max;
    }
    timeout = (sleep_time * MSECS);	/* convert secs to ms */
    if (timeout == 0) timeout++;	/* minimum of one ms! */
    if (dip->di_timerDebugFlag) {
	Printf(dip, "Delaying for %ums (or %.2f secs)...\n",
	       timeout, ((float)timeout / (float)MSECS));
    }
    do {
	/* Do short sleeps so we can detect when we're terminating! */
	ms = min(MSECS, timeout);
	(void)os_msleep(ms);
	if ( PROGRAM_TERMINATING || THREAD_TERMINATING(dip) ) break;
	timeout -= ms;
    } while (timeout);
    return;
}

/* Return the difference in usecs, of two timers. */
uint64_t
timer_diff(struct timeval *start, struct timeval *end)
{
    struct timeval delta;

    delta.tv_sec = end->tv_sec - start->tv_sec;
    delta.tv_usec = end->tv_usec - start->tv_usec;
    if (delta.tv_usec < 0) {
	delta.tv_sec -= 1;
	delta.tv_usec += 1000000;
    }
    return (((uint64_t)delta.tv_sec * (uint64_t)1000000) + (uint64_t)delta.tv_usec);
}

/* Return the difference in usecs, of previous timer vs. now. */
uint64_t
timer_now(struct timeval *timer)
{
    struct timeval now, delta;

    (void)gettimeofday(&now, NULL);
    delta.tv_sec = now.tv_sec - timer->tv_sec;
    delta.tv_usec = now.tv_usec - timer->tv_usec;
    if (delta.tv_usec < 0) {
	delta.tv_sec -= 1;
	delta.tv_usec += 1000000;
    }
    return (((uint64_t)delta.tv_sec * (uint64_t)1000000) + (uint64_t)delta.tv_usec);
}

/************************************************************************
 *									*
 * fill_buffer() - Fill Buffer with a Data Pattern.			*
 *									*
 * Description:								*
 *	If a pattern_buffer exists, then this data is used to fill the	*
 * buffer instead of the data pattern specified.			*
 *									*
 * Inputs:							        * 
 *	dip = The device information pointer.				*
 * 	buffer = Pointer to buffer to fill.				*
 *	byte_count = Number of bytes to fill.				*
 *	pattern = Data pattern to fill buffer with.			*
 *									*
 * Return Value:							*
 *		Void.							*
 *									*
 ************************************************************************/
void
fill_buffer (   dinfo_t     *dip,
		void        *buffer,
		size_t      byte_count,
		u_int32     pattern)
{
    register unsigned char *bptr = buffer;
    register unsigned char *pptr, *pend;
    register size_t bcount = byte_count;
    register lbdata_t lbdata_size = dip->di_lbdata_size;
    register size_t i;

    pptr = dip->di_pattern_bufptr;
    pend = dip->di_pattern_bufend;

    /*
     * Initialize the buffer with a data pattern and optional prefix.
     */
    if ( dip->di_btag_flag && (dip->di_fprefix_string == NULL) ) {
	size_t btag_size = getBtagSize(dip->di_btag);
	for (i = 0; i < bcount; ) {
	    if ((i % lbdata_size) == 0) {
		i += btag_size;
		bptr += btag_size;
		continue;
	    }
	    *bptr++ = *pptr++; i++;
	    if (pptr == pend) {
		pptr = dip->di_pattern_buffer;
	    }
	}
    } else if (dip->di_fprefix_string == NULL) {
	while (bcount--) {
            *bptr++ = *pptr++;
            if (pptr == pend) {
                pptr = dip->di_pattern_buffer;
            }
        }
    } else {
	/*
	 * Fill the buffer with a prefix and a data pattern.
	 */
	for (i = 0; i < bcount; ) {
	    /*
	     * Please Note: This code is a major performance bottleneck! ;(
	     * Also Note: 64-bit Linux binary is slower than 32-bit version!
	     * Note: The IOT loops are faster due to aligned 32-bit copies.
	     * Optimizing is for another day, but a block tag is preferred!
	     * Obviously, optimized compiled code also make a big difference.
	     * FWIW: Inlining the prefix copy, made very little difference.
	     */
	    if ((i % lbdata_size) == 0) {
		size_t pcount;
		if (dip->di_btag) {
		    btag_t *btag = dip->di_btag;
		    size_t btag_size = sizeof(*btag) + btag->btag_opaque_data_size;
		    i += btag_size;
		    bptr += btag_size;
		}
		pcount = copy_prefix(dip, bptr, (bcount - i));
		i += pcount;
		bptr += pcount;
		continue;
	    }
	    *bptr++ = *pptr++; i++;
	    if (pptr == pend) {
		pptr = dip->di_pattern_buffer;
	    }
	}
    }
    dip->di_pattern_bufptr = pptr;
    return;
}

/************************************************************************
 *									*
 * init_buffer() - Initialize Buffer with a Data Pattern.		*
 *									*
 * Inputs:							        * 
 * 	dip = The device information pointer.			        *
 *	buffer = Pointer to buffer to init.				*
 *	count = Number of bytes to initialize.				*
 *	pattern = Data pattern to init buffer with.			*
 *									*
 * Return Value:							*
 *		Void.							*
 *									*
 ************************************************************************/
void
init_buffer(    dinfo_t     *dip,
		void        *buffer,
		size_t      count,
		u_int32     pattern )
{
    register unsigned char *bp;
    union {
	u_char pat[sizeof(u_int32)];
	u_int32 pattern;
    } p;
    register size_t i;

    /*
     * Initialize the buffer with a data pattern.
     */
    p.pattern = pattern;
    bp = buffer;
    for (i = 0; i < count; i++) {
	*bp++ = p.pat[i & (sizeof(u_int32) - 1)];
    }
    return;
}

/************************************************************************
 *									*
 * poision_buffer() - Poison Buffer with a Data Pattern.		*
 *									*
 * Inputs:							        * 
 * 	dip = The device information pointer.			        *
 *	buffer = Pointer to buffer to init.				*
 *	count = Number of bytes to initialize.				*
 *	pattern = Data pattern to init buffer with.			*
 *									*
 * Return Value:							*
 *		Void.							*
 *									*
 ************************************************************************/
void
poison_buffer(  dinfo_t     *dip,
		void        *buffer,
		size_t      count,
		u_int32     pattern )
{
    uint8_t *bp = buffer;
    uint8_t *eptr = (bp + count) - sizeof(pattern);
    uint32_t dsize = (dip->di_dsize) ? dip->di_dsize : BLOCK_SIZE;
    union {
	u_char pat[sizeof(u_int32)];
	u_int32 pattern;
    } p;

    /* Varible length file records may be too short! */
    if (count < sizeof(pattern)) {
	return;
    }
    p.pattern = pattern;
    /*
     * Initialize the buffer with a data pattern, every dsize bytes.
     */
    while (bp < eptr) {
	register uint8_t *bptr = bp;
	register size_t i;
        /* Byte at a time, buffer may be misaligned! */
	for (i = 0; i < sizeof(pattern); i++) {
	    *bptr++ = p.pat[i & (sizeof(uint32_t) - 1)];
	}
        bp += dsize;
    }
    return;
}

#if _BIG_ENDIAN_
/************************************************************************
 *									*
 * init_swapped() - Initialize Buffer with a Swapped Data Pattern.	*
 *									*
 * Inputs:							        * 
 * 	dip = The device information pointer.			        *
 *	buffer = Pointer to buffer to init.				*
 *	count = Number of bytes to initialize.				*
 *	pattern = Data pattern to init buffer with.			*
 *									*
 * Return Value:							*
 *		Void.							*
 *									*
 ************************************************************************/
void
init_swapped (  dinfo_t         *dip,
		void            *buffer,
		size_t          count,
		u_int32         pattern )
{
    register unsigned char *bp;
    union {
	u_char pat[sizeof(u_int32)];
	u_int32 pattern;
    } p;
    register size_t i;

    /*
     * Initialize the buffer with a data pattern.
     */
    p.pattern = pattern;
    bp = buffer;
    for (i = count; i ; ) {
	*bp++ = p.pat[--i & (sizeof(u_int32) - 1)];
    }
    return;
}
#endif /* _BIG_ENDIAN_ */

/************************************************************************
 *									*
 * init_lbdata() - Initialize Data Buffer with Logical Block Data.	*
 *									*
 * Description:								*
 *	This function takes the starting logical block address, and	*
 * inserts it every logical block size bytes, overwriting the first 4	*
 * bytes of each logical block with its' address.			*
 *									*
 * Inputs:							        * 
 * 	dip = The device information pointer.			        *
 *	buffer = The data buffer to initialize.				*
 *	count = The data buffer size (in bytes).			*
 *	lba = The starting logical block address.			*
 *	lbsize = The logical block size (in bytes).			*
 *									*
 * Outputs:							        * 
 * 	Returns the next lba to use.					*
 *									*
 ************************************************************************/
u_int32
init_lbdata (
	    struct dinfo    *dip,
	    void            *buffer,
	    size_t          count,
	    u_int32         lba,
	    u_int32         lbsize )
{
    unsigned char *bp = buffer;
    register ssize_t i;

    /*
     * Initialize the buffer with logical block data.
     */
    if (dip->di_fprefix_string) {
	register size_t pcount = 0, scount = lbsize;
	/*
	 * The lba is encoded after the prefix string.
	 */
	pcount = MIN((size_t)dip->di_fprefix_size, count);
	scount -= pcount;
	for (i = 0; (i+pcount+sizeof(lba)) <= count; ) {
	    bp += pcount;
	    htos(bp, lba, sizeof(lba));
	    i += lbsize;
	    bp += scount;
	    lba++;
	}
    } else {
	for (i = 0; (i+sizeof(lba)) <= count; ) {
	    htos (bp, lba, sizeof(lba));
	    i += lbsize;
	    bp += lbsize;
	    lba++;
	}
    }
    return(lba);
}

#if defined(TIMESTAMP)
/************************************************************************
 *									*
 * init_timestamp() - Initialize Data Buffer with a Timestamp.          *
 *									*
 * Description:								*
 *	This function places a timestamp in the first 4 bytes of each   *
 * data block.                                                          *
 *									*
 * Inputs:							        * 
 * 	dip = The device information pointer.			        *
 *	buffer = The data buffer to initialize.				*
 *	count = The data buffer size (in bytes).			*
 *	lbsize = The logical block size (in bytes).			*
 *									*
 * Outputs:	Returns the next lba to use.				*
 *									*
 ************************************************************************/
void
init_timestamp (
	       dinfo_t         *dip,
	       void            *buffer,
	       size_t          count,
	       u_int32         lbsize )
{
    unsigned char *bptr = buffer;
    register ssize_t i;
    register iotlba_t timestamp = (iotlba_t)time((time_t *)0);

    /*
     * Initialize the buffer with a timestamp (in seconds).
     */
    if (dip->di_fprefix_string || dip->di_btag_flag) {
	register size_t pcount = 0, scount = lbsize;
	if ( dip->di_btag_flag ) {
	    pcount = getBtagSize(dip->di_btag);
	}
	/*
	 * The timestamp is encoded after the prefix string.
	 */
	pcount += MIN((size_t)dip->di_fprefix_size, count);
	scount -= pcount;
	for (i = 0; (i+pcount+sizeof(timestamp)) <= count; ) {
	    bptr += pcount;
	    htos(bptr, (large_t)timestamp, sizeof(timestamp));
	    i += lbsize;
	    bptr += scount;
	}
    } else {
	for (i = 0; (i+sizeof(timestamp)) <= count; ) {
	    htos(bptr, (large_t)timestamp, sizeof(timestamp));
	    i += lbsize;
	    bptr += lbsize;
	}
    }
    return;
}
#endif /* defined(TIMESTAMP) */

#if !defined(INLINE_FUNCS)
/*
 * Calculate the starting logical block number.
 */
u_int32
make_lba(
	struct dinfo    *dip,
	Offset_t        pos )
{
    return( (u_int32)((pos == (Offset_t)0) ? (u_int32)0 : (pos/dip->di_lbdata_size)) );
}

Offset_t
make_offset(struct dinfo *dip, u_int32 lba)
{
    return( (Offset_t)(lba * dip->di_lbdata_size) );
}

/*
 * Calculate the starting lbdata block number.
 */
u_int32
make_lbdata(
	   struct dinfo    *dip,
	   Offset_t        pos )
{
    return( (u_int32)((pos == (Offset_t)0) ? (u_int32)0 : (pos/dip->di_lbdata_size)) );
}
#endif /* !defined(INLINE_FUNCS) */

u_int32
winit_lbdata(
	    struct dinfo    *dip,
	    Offset_t        pos,
	    u_char          *buffer,
	    size_t          count,
	    u_int32         lba,
	    u_int32         lbsize )
{
    if (dip->di_user_lbdata) {
	/* Using user defined lba, not file position! */
	return(init_lbdata (dip, buffer, count, lba, lbsize));
    } else if (pos == (Offset_t) 0) {
	return(init_lbdata (dip, buffer, count, (u_int32) 0, lbsize));
    } else {
	return(init_lbdata (dip, buffer, count, (u_int32)(pos / lbsize), lbsize));
    }
}

/************************************************************************
 *									*
 * init_padbytes() - Initialize pad bytes at end of data buffer.	*
 *									*
 * Inputs:	buffer = Pointer to start of data buffer.		*
 *		offset = Offset to where pad bytes start.		*
 *		pattern = Data pattern to init buffer with.		*
 *									*
 * Return Value:							*
 *		Void.							*
 *									*
 ************************************************************************/
void
init_padbytes ( u_char          *buffer,
		size_t          offset,
		u_int32         pattern )
{
    size_t i;
    u_char *bptr;
    union {
	u_char pat[sizeof(u_int32)];
	u_int32 pattern;
    } p;

    p.pattern = pattern;
    bptr = buffer + offset;
    for (i = 0; i < PADBUFR_SIZE; i++) {
	*bptr++ = p.pat[i & (sizeof(u_int32) - 1)];
    }
}

/*
 * copy_prefix() - Copy Prefix String to Buffer.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	buffer = Pointer to buffer to copy prefix.
 *	bcount = Count of remaining buffer bytes.
 *
 * Outputs:
 *	Returns number of prefix bytes copied.
 */
size_t
copy_prefix( dinfo_t *dip, u_char *buffer, size_t bcount )
{
    size_t pcount = MIN((size_t)dip->di_fprefix_size, bcount);
    (void)memcpy(buffer, dip->di_fprefix_string, pcount);
    return(pcount);
}

/************************************************************************
 *									*
 * process_pfile() - Process a pattern file.				*
 *									*
 * Inputs:							        * 
 * 	dip = The device information pointer.			        *
 *	file = Pointer to pattern file name.				*
 *									*
 * Outputs:	Returns on success, exits on open failure.		*
 *									*
 * Return Value:							*
 *		Returns SUCCESS / FAILURE				*
 *									*
 ************************************************************************/
int
process_pfile(dinfo_t *dip, char *file)
{
    HANDLE fd;
    int oflags = O_RDONLY;
    size_t count, size;
    large_t filesize;
    u_char *buffer;

    if ( os_file_information(file, &filesize, NULL, NULL) == FAILURE ) {
	Fprintf(dip, "The pattern file '%s', cannot be accessed!\n", file);
	ReportErrorInfo(dip, file, os_get_error(), OS_GET_FILE_ATTR_OP, GETATTR_OP, True);
	return(FAILURE);
    }
    size = (size_t)filesize;

    fd = dt_open_file(dip, file, oflags, 0, NULL, NULL, True, False);
    if (fd == NoFd) {
	return(FAILURE);
    }
    buffer = malloc_palign(dip, size, 0);
    if ( (count = os_read_file(fd, buffer, size)) != size) {
	Fprintf (dip, "Pattern file '%s' read error!\n", file);
	if ((ssize_t)count == FAILURE) {
	    ReportErrorInfo(dip, file, os_get_error(), OS_READ_FILE_OP, READ_OP, True);
	    return(FAILURE);
	} else {
	    Eprintf(dip, "Attempted to read %d bytes, read only %d bytes.", size, count);
	    return(FAILURE);
	}
    }

    setup_pattern(dip, buffer, size, True);
    (void)os_close_file(fd);
    return(SUCCESS);
}

/*
 * process_iotune() - Process Parmaters from an IO Tune file.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	file = Pointer to IO tune file name.
 *
 * Outputs:
 * 	Sets up new IO delays.
 *
 * Return Value:
 *	none
 */
void
process_iotune(dinfo_t *dip, char *file)
{
    FILE        *fp;
    ssize_t     count;
    int         size = STRING_BUFFER_SIZE;
    char        buffer[STRING_BUFFER_SIZE];
    char        *mode = "r";
    struct stat statbuf, *sb = &statbuf;
    int         status;

    if (stat(file, sb) == FAILURE) return;
    if (dip->di_iotune_mtime == sb->st_mtime) return;
    dip->di_iotune_mtime = sb->st_mtime;

    if (dip->di_debug_flag || dip->di_tDebugFlag) {
	Printf(dip, "Processing I/O tune file '%s'...\n", file);
    }
    if ( (fp = fopen (file, mode)) == (FILE *) 0) {
	Perror(dip, "Unable to open script file '%s' for reading", file);
	return;
    }

    do {
	memset(buffer, '\0', size);
	if (fgets (buffer, size, fp) == NULL) {
	    count = -1;		/* Error or EOF, stop reading! */
	} else {
	    char *p;
	    if (p = strrchr(buffer, '\r')) *p = '\0';
	    if (p = strrchr(buffer, '\n')) *p = '\0';
	    count = (ssize_t)strlen(buffer);
	}
	/* Format: [jobid]|[tag] *_delay=value enable=flag... */
	if (count > 0 ) {
	    status = modify_jobs(dip, (job_id_t) 0, NULL, buffer);
	    if (status == FAILURE) break;
	}
    } while ( count > 0 );

#if defined(DEBUG)
    if (dip->di_debug_flag || dip->di_tDebugFlag) {
	Printf(dip, "Finished processing I/O tune file...\n");
    }
#endif
    (void)fclose(fp);
    return;
}

/*
 * setup_pattern() - Setup pattern variables.
 *
 * Inputs:
 * 	dip = The device information pointer.
 *	buffer = Pointer to pattern buffer.
 *	size = Size of pattern buffer.
 *
 * Outputs:
 * 	pattern variables setup with 1st 4 bytes of pattern. (if request)
 *
 * Return Value:
 *	void
 */
void
setup_pattern(dinfo_t *dip, uint8_t *buffer, size_t size, hbool_t init_pattern)
{
    dip->di_pattern_buffer = buffer;
    dip->di_pattern_bufptr = buffer;
    dip->di_pattern_bufend = (buffer + size);
    dip->di_pattern_bufsize = size;

    if (init_pattern == False) return;

    dip->di_pattern = (uint32_t)0;

    switch (size) {
	
	case sizeof(u_char):
	    dip->di_pattern = (uint32_t)buffer[0];
	    break;

	case sizeof(u_short):
	    dip->di_pattern = ( ((uint32_t)buffer[1] << 8) | (uint32_t)buffer[0] );
	    break;

	case 0x03:
	    dip->di_pattern = ( ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[1] << 8) |
				(uint32_t) buffer[0] );
	    break;

	default:
	    dip->di_pattern = ( ((uint32_t)buffer[3] << 24) | ((uint32_t)buffer[2] << 16) |
				((uint32_t)buffer[1] << 8) | (uint32_t)buffer[0]);
	    break;
    }
    return;
}

void
reset_pattern(dinfo_t *dip)
{
    if (dip->di_pattern_buffer) {
	free_palign(dip, dip->di_pattern_buffer);
	dip->di_pattern_buffer = NULL;
	dip->di_pattern_bufptr = NULL;
	dip->di_pattern_bufend = NULL;
	dip->di_pattern_bufsize = 0;
    }
    return;
}

/*
 * Copy pattern bytes to pattern buffer with proper byte ordering.
 */
void
copy_pattern (u_int32 pattern, u_char *buffer)
{
    buffer[0] = (u_char) pattern;
    buffer[1] = (u_char) (pattern >> 8);
    buffer[2] = (u_char) (pattern >> 16);
    buffer[3] = (u_char) (pattern >> 24);
}

/*
 * convert_clock_ticks() - Convert OS clock ticks to time values. 
 *  
 * Note: clock_ticks is defined as long on Linux and Windows, but this 
 * value is in system click ticks from sysconf(_SC_CLK_TCK), which on 
 * Linux is 100 while on Windows is 1000, so wrapping occurs sooner on 
 * Windows causing tme values to become negative. Therefore, the signed 
 * clock_t os cast to an unsigned long to avoid negative time values. 
 * In fact, the times() API used will return -1 when it fails. 
 *  
 * Clearly using times() needs replaced with a better method (see below):
 *  
 * On Linux, the "arbitrary point in the past" from which the return value of times()
 * is measured has varied across kernel versions. On Linux 2.4 and earlier this point
 * is the moment the system was booted. Since Linux 2.6, this point is (2^32/HZ) - 300
 * (i.e., about 429 million) seconds before system boot time. This variability across
 * kernel versions (and across UNIX implementations), combined with the fact that the
 * returned value may overflow the range of clock_t, means that a portable application
 * would be wise to avoid using this value.
 * To measure changes in elapsed time, use clock_gettime(2) instead. 
 *  
 * TODO:
 *     int clock_getres(clockid_t clk_id, struct timespec *res);
 *     int clock_gettime(clockid_t clk_id, struct timespec *tp);
 *  
 * The above API's do not exist on Windows, see this URL for guideance: 
 *     https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows 
 */
void
convert_clock_ticks(clock_t ticks, clock_t *days, clock_t *hours,
		    clock_t *minutes, clock_t *seconds, clock_t *frac)
{
    unsigned long clock_ticks = (unsigned long)ticks; /* Bandaide, may help! */
    *frac = (clock_ticks % hertz);
    *frac = (*frac * 100) / hertz;
    clock_ticks /= hertz;
    *seconds = (clock_ticks % SECS_PER_MIN);
    clock_ticks /= SECS_PER_MIN;
    *minutes = (clock_ticks % MINS_PER_HOUR);
    clock_ticks /= MINS_PER_HOUR;
    *hours = (clock_ticks % HOURS_PER_DAY);
    clock_ticks /= HOURS_PER_DAY;
    *days = clock_ticks;
    return;
}

/*
 * Function to display ASCII time.
 */
char *
bformat_time(char *bp, clock_t ticks)
{
    clock_t days, hours, minutes, seconds, frac;

    convert_clock_ticks(ticks, &days, &hours, &minutes, &seconds, &frac);
    if (days) {
	bp += Sprintf(bp, "%dd", days);
	bp += Sprintf(bp, "%02dh", hours);
    } else if (hours) {
	bp += Sprintf(bp, "%dh", hours);
    }
    bp += Sprintf(bp, "%02dm", minutes);
    bp += Sprintf(bp, "%02d.", seconds);
    bp += Sprintf(bp, "%02ds", frac);
    return(bp);
}

void
print_time(dinfo_t *dip, FILE *fp, clock_t ticks)
{
    clock_t days, hours, minutes, seconds, frac;
    int flags = (PRT_NOFLUSH|PRT_NOLEVEL|PRT_NOIDENT);

    convert_clock_ticks(ticks, &days, &hours, &minutes, &seconds, &frac);
    if (days) {
	LogMsg(dip, fp, logLevelError, flags, "%dd", days);
	LogMsg(dip, fp, logLevelError, flags, "%02dh", hours);
    } else {
	LogMsg(dip, fp, logLevelError, flags, "%dh", hours);
    }
    LogMsg(dip, fp, logLevelError, flags, "%02dm", minutes);
    LogMsg(dip, fp, logLevelError, flags, "%02d.", seconds);
    LogMsg(dip, fp, logLevelError, flags, "%02ds\n", frac);
    return;
}

void
format_time(dinfo_t *dip, clock_t ticks)
{
    clock_t days, hours, minutes, seconds, frac;

    convert_clock_ticks(ticks, &days, &hours, &minutes, &seconds, &frac);
    if (days) {
	Lprintf(dip, "%dd", days);
	Lprintf(dip, "%02dh", hours);
    } else if (hours) {
	Lprintf(dip, "%dh", hours);
    }
    Lprintf(dip, "%02dm", minutes);
    Lprintf(dip, "%02d.", seconds);
    Lprintf(dip, "%02ds\n", frac);
    return;
}

/*
 * Format the elapsed time. 
 *  
 * Inputs: 
 *      buffer = Buffer for time string.
 *      ticks = The elapsed time in ticks.
 *  
 * Return Value: 
 * 	Length of time string. 
 */
int
FormatElapstedTime(char *buffer, clock_t ticks)
{
    char *bp = buffer;
    clock_t days, hours, minutes, seconds, frac;

    convert_clock_ticks(ticks, &days, &hours, &minutes, &seconds, &frac);
    if (days) {
	bp += Sprintf(bp, "%dd", days);
	bp += Sprintf(bp, "%02dh", hours);
    } else if (hours) {
	bp += Sprintf(bp, "%dh", hours);
    }
    bp += Sprintf(bp, "%02dm", minutes);
    bp += Sprintf(bp, "%02d.", seconds);
    bp += Sprintf(bp, "%02ds", frac);
    return( (int)(bp - buffer) );
}

/*
 * seek_file() - Seeks to the specified file offset.
 *
 * Inputs:
 * 	dip	= The device information pointer.
 *	fd	= The file descriptor.
 *	records = The number of records.
 *	size    = The size of each record.
 *	whence  = The method of setting position:
 *		SEEK_SET (L_SET)  = Set to offset bytes.
 *		SEEK_CUR (L_INCR) = Increment by offset bytes.
 *		SEEK_END (L_XTND) = Extend by offset bytes.
 *
 *		offset = (record count * size of each record)
 *
 * Return Value:
 *	Returns file position on Success, (Offset_t)-1 on Failure.
 */
Offset_t
seek_file(dinfo_t *dip, HANDLE fd, u_long records, Offset_t size, int whence)
{
    Offset_t pos;

    if ( (pos = os_seek_file(fd, (Offset_t)(records * size), whence)) == (Offset_t)-1) {
	int error = os_get_error();
	Fprintf(dip, "seek failed (fd %d, offset " FUF ", whence %d)\n",
		fd, (Offset_t)(records * size), whence);
	ReportErrorInfo(dip, dip->di_dname, error, OS_SEEK_FILE_OP, SEEK_OP, True);
    }
    return(pos);
}

/*
 * Utility functions to handle file positioning.
 */
Offset_t
seek_position(struct dinfo *dip, Offset_t offset, int whence, hbool_t expect_error)
{
    Offset_t pos;

#if defined(DEBUG)
    if (dip->di_Debug_flag) {
	Printf(dip, "attempting os_seek_file(fd=%d, offset=" FUF ", whence=%d)\n",
	       dip->di_fd, offset, whence);
    }
#endif /* defined(DEBUG) */

#if defined(SCSI)
    /* Handle SCSI I/O. */
    if (dip->di_scsi_io_flag) {
	Offset_t disk_capacity = (Offset_t)(dip->di_device_capacity * dip->di_block_length);
	if (whence == SEEK_CUR) {
	    pos = (dip->di_offset + offset);
	} else if (whence == SEEK_SET) {
	    pos = offset;
	} else if (whence == SEEK_END) {
	    pos = disk_capacity;
	}
	if (pos > disk_capacity) {
	    if (expect_error == False) {
		Eprintf(dip, "The offset "FUF" exceeds the disk capacity of "LUF" bytes!\n",
			pos, disk_capacity);
	    }
	    return((Offset_t)-1);
	} else {
	    return(pos);
	}
    }
#endif /* defined(SCSI) */

    /*
     * Seek to the specifed file position.
     * Note: This will all go away once we switch to pread/pwrite!
     */
retry:
    if ((pos = os_seek_file(dip->di_fd, offset, whence)) == (Offset_t)-1) {
	if (expect_error == False) {
	    int rc;
	    INIT_ERROR_INFO(eip, dip->di_dname, OS_SEEK_FILE_OP, SEEK_OP, &dip->di_fd, dip->di_oflags,
			    offset, (size_t)0, os_get_error(), logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    Fprintf(dip, "failed seek (fd %d, offset " FUF ", whence %d)\n",
	    	    dip->di_fd, offset, whence);
	    rc = ReportRetryableError(dip, eip, "Failed seek on file %s", dip->di_dname);
	    if (rc == RETRYABLE) goto retry;
	    return( (Offset_t)-1 );
	} else {
	    return(pos); /* Let caller handle the error! */
	}
    }

#if defined(DEBUG)
    if (dip->di_Debug_flag) {
	Printf(dip, "returned pos -> " FUF " = os_seek_file(fd=%d, offset=%lu, whence=%d)\n",
	       pos, dip->di_fd, offset, whence);
    }
#endif /* defined(DEBUG) */

    return(pos);
}

Offset_t
dt_seek_position(dinfo_t *dip, char *file, HANDLE *fdp, Offset_t offset,
		 int whence, hbool_t errors, hbool_t retrys)
{
    Offset_t pos;
    int rc = SUCCESS;

    do {
	if ((pos = os_seek_file(*fdp, offset, whence)) == (Offset_t)FAILURE) {
	    INIT_ERROR_INFO(eip, dip->di_dname, OS_SEEK_FILE_OP, SEEK_OP, fdp, dip->di_oflags,
			    offset, (size_t)0, os_get_error(), logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    if (retrys == False) eip->ei_rpt_flags |= RPT_NORETRYS;
	    rc = ReportRetryableError(dip, eip, "Failed seek on file %s", file);
	}
    } while ( (pos == (Offset_t)FAILURE) && (rc == RETRYABLE) );
    return(pos);
}

#if 0
/* Note: This goes away once we fully switch to pread/write API's! */
hbool_t
is_correct_offset(dinfo_t *dip, Offset_t record_offset, ssize_t count, Offset_t *updated_offset)
{
    Offset_t expected_offset = (record_offset + count);

    if (dip->di_aio_flag == True) return(True);
    *updated_offset = get_updated_offset(dip, count);
    if (expected_offset == *updated_offset) {
	return(True);
    } else {
	return(False);
    }
}
#endif /* 0 */

Offset_t
get_current_offset(dinfo_t *dip, ssize_t count)
{
    if (dip->di_aio_flag || dip->di_mmap_flag || dip->di_scsi_io_flag) {
	return(dip->di_offset);
    } else { /* Normal API's (for now) */
	return(get_position(dip) - count);
    }
}

Offset_t
get_updated_offset(dinfo_t *dip, ssize_t count)
{
    if (dip->di_aio_flag || dip->di_mmap_flag || dip->di_scsi_io_flag) {
	return(dip->di_offset + count);
    } else { /* Normal API's (for now) */
	return(get_position(dip)); /* OS updated offset! */
    }
}

//#if !defined(INLINE_FUNCS)

Offset_t
get_position(struct dinfo *dip)
{
    return( seek_position(dip, (Offset_t) 0, SEEK_CUR, False) );
}

Offset_t
dt_get_position(dinfo_t *dip, char *file, HANDLE *fdp, hbool_t errors, hbool_t retrys)
{
    return( dt_seek_position(dip, file, fdp, (Offset_t)0, SEEK_CUR, errors, retrys) );
}

//#endif /* !defined(INLINE_FUNCS) */

u_int32
get_lba(struct dinfo *dip)
{
    Offset_t pos;
    if ( (pos = get_position(dip)) ) {
	return( (u_int32)(pos / dip->di_lbdata_size) );
    } else {
	return( (u_int32)0 );
    }
}

Offset_t
incr_position(struct dinfo *dip, Offset_t offset, hbool_t expect_error)
{
    Offset_t pos;

    pos = seek_position(dip, offset, SEEK_CUR, expect_error);
    if ( (dip->di_Debug_flag || dip->di_rDebugFlag) && (pos != (Offset_t)-1) ) {
	large_t lba = (pos / (Offset_t)dip->di_dsize);
	Printf(dip, "Seeked to block " LUF " (" LXF ") at offset " FUF "\n",
	       lba, lba, pos);
    }
    return(pos);
}

Offset_t
set_position(struct dinfo *dip, Offset_t offset, hbool_t expect_error)
{
    Offset_t pos;

    pos = seek_position(dip, offset, SEEK_SET, expect_error);
    if ( (dip->di_Debug_flag || dip->di_rDebugFlag) && (pos != (Offset_t)-1) ) {
	large_t lba = (pos / (Offset_t)dip->di_dsize);
	u_int32 boff = (pos % (Offset_t)dip->di_dsize);
	if (boff) { /* Show block offset too! Hex useful for traces! */
	    Printf(dip, "Seeked to block " LUF ".%u (" LXF ".%x) at offset " FUF "\n",
		   lba, boff, lba, boff, pos);
	} else {
	    Printf(dip, "Seeked to block " LUF " (" LXF ") at offset " FUF "\n",
		   lba, lba, pos);
	}
    }
    return(pos);
}

Offset_t
dt_set_position(struct dinfo *dip, char *file, HANDLE *fdp,
		Offset_t offset, hbool_t errors, hbool_t retrys)
{
    Offset_t pos;

    pos = dt_seek_position(dip, file, fdp, offset, SEEK_SET, errors, retrys);
    if ( (dip->di_Debug_flag || dip->di_rDebugFlag) && (pos != (Offset_t)FAILURE) ) {
	large_t lba = (pos / (Offset_t)dip->di_dsize);
	u_int32 boff = (pos % (Offset_t)dip->di_dsize);
	if (boff) { /* Show block offset too! Hex useful for traces! */
	    Printf(dip, "Seeked to block " LUF ".%u (" LXF ".%x) at offset " FUF "\n",
		   lba, boff, lba, boff, pos);
	} else {
	    Printf(dip, "Seeked to block " LUF " (" LXF ") at offset " FUF "\n",
		   lba, lba, pos);
	}
    }
    return(pos);
}

#if !defined(INLINE_FUNCS)
Offset_t
make_position(struct dinfo *dip, u_int32 lba)
{
    return( (Offset_t)(lba * dip->di_lbdata_size) );
}
#endif /* !defined(INLINE_FUNCS) */

void
show_position (struct dinfo *dip, Offset_t pos)
{
    if (dip->di_debug_flag || dip->di_rDebugFlag) {
	large_t lba = make_lba(dip, pos);
	Printf (dip, "%s: File offset is " FUF " (" FXF "), relative lba is %u (%#x)\n",
		dip->di_dname, pos, pos, lba, lba);
    }
}

#if !defined(INLINE_FUNCS)

uint32_t
get_random(dinfo_t *dip)
{
    return( (uint32_t)genrand64_int64(dip) );
}

large_t
get_random64(dinfo_t *dip)
{
    return( genrand64_int64(dip) );
}

/*
 * Function to set random number generator seed.
 */
void
set_rseed(dinfo_t *dip, uint64_t seed)
{
    init_genrand64(dip, seed);
}

/* lower <= rnd(lower,upper) <= upper */
int32_t
rnd(dinfo_t *dip, int32_t lower, int32_t upper)
{
    return( lower + (int32_t)( ((double)(upper - lower + 1) * get_random(dip)) / (UINT32_MAX + 1.0)) );
}

/* lower <= rnd(lower,upper) <= upper */
int64_t
rnd64(dinfo_t *dip, int64_t lower, int64_t upper)
{
    return( lower + (int64_t)( ((double)(upper - lower + 1) * genrand64_int64(dip)) / (UINT64_MAX + 1.0)) );
}

hbool_t
dt_test_lock_mode(dinfo_t *dip, int lock_mode)
{
    int32_t n;

    n = rnd(dip, 1, 100);
    if ( (n >= dip->di_lock_mode[lock_mode].lower) && (n <= dip->di_lock_mode[lock_mode].upper) ) {
	return True;
    } else {
	return False;
    }
}

hbool_t
dt_unlock_file_chance(dinfo_t *dip)
{
    int32_t n;

    if (dip->di_unlock_chance == 0) {
	return False;
    }
    n = rnd(dip, 1, 100);
    if (n <= dip->di_unlock_chance) {
	return True;
    } else {
	return False;
    }
}

#endif /* !defined(INLINE_FUNCS) */

size_t
get_data_size(dinfo_t *dip, optype_t optype)
{
    size_t data_size;

    if (dip->di_min_size) {
        if (dip->di_variable_flag) {
            data_size = get_variable(dip);
        } else {
	    data_size = dip->di_min_size;
        }
    } else {
	if ( (optype == READ_OP) && dip->di_iblock_size) {
	    data_size = dip->di_iblock_size;
	} else if ( (optype == WRITE_OP) && dip->di_oblock_size) {
	    data_size = dip->di_oblock_size;
	} else {
	    data_size = dip->di_block_size;
	}
    }
    return(data_size);
}

/*
 * Function to calculate variable length request size.
 */
size_t
get_variable(struct dinfo *dip)
{
    register size_t length;
    register uint32_t randum;

    randum = get_random(dip);
    length = (size_t)((randum % dip->di_max_size) + dip->di_min_size);
    if (dip->di_dsize) {
	if ( isDiskDevice(dip) ||
	     ( isFileSystemFile(dip) && (dip->di_fsalign_flag == True) ) ) {
	    length = roundup(length, dip->di_dsize);
	} else { /* Align to pattern file size for file systems. */
	    /* Note: I'm not sure this is good for large pattern files! */
	    length = roundup(length, dip->di_pattern_bufsize);
	}
    }
    if (length > dip->di_max_size) length = dip->di_max_size;
    return(length);
}

large_t
get_data_limit(dinfo_t *dip)
{
    large_t data_limit;

    if ( dip->di_min_limit && dip->di_max_limit ) {
	if (dip->di_variable_limit == True) {
	    data_limit = get_variable_limit(dip);
	    dip->di_data_limit = data_limit;
	} else {
	    /* Prime the data limit for the 1st file! */
	    /* Note: Allowing data limit growing across directories. */
	    if ( (dip->di_dir_number == 0) && (dip->di_subdir_number == 0) &&
		 (dip->di_file_number == 0) ) {
		data_limit = (dip->di_data_limit = dip->di_min_limit);
	    } else {
		data_limit = (dip->di_data_limit + dip->di_incr_limit);
		if (data_limit > dip->di_max_limit) data_limit = dip->di_min_limit;
		dip->di_data_limit = data_limit;
	    }
	}
    } else {
	data_limit = dip->di_data_limit;
    }
    return(data_limit);
}

large_t
get_variable_limit(dinfo_t *dip)
{
    large_t length;

    length = rnd64(dip, dip->di_min_limit, dip->di_max_limit);
    if (dip->di_dsize) {
	if ( isDiskDevice(dip) ||
	     ( isFileSystemFile(dip) && (dip->di_fsalign_flag == True) ) ) {
	    length = roundup(length, dip->di_dsize);
	}
    }
    if (length > dip->di_max_limit) length = dip->di_max_limit;
    return(length);
}

/*
 * Function to set position for random I/O.
 */
Offset_t
do_random(struct dinfo *dip, hbool_t doseek, size_t xfer_size)
{
    Offset_t pos = 0, dsize, ralign;
    large_t randum, rlimit;
    Offset_t align;

    dsize = (Offset_t)(dip->di_dsize);
    /* Set the random limit to a value that ensures we won't exceed it! */
    rlimit = (dip->di_rdata_limit - dip->di_file_position - xfer_size);

    /*
     * Ensure the random alignment size is modulo the device size
     * for raw devices or the pattern file for regular FS files.
     */
    if ( (dip->di_dtype->dt_dtype == DT_REGULAR) && (dip->di_fsalign_flag == False) ) {
	align = dip->di_pattern_bufsize;
    } else {
	/* Note: File system random I/O usually forced to dsize! */
	align = dsize;
    }
    ralign = (Offset_t)((dip->di_random_align) ? dip->di_random_align : align);
    /* The user alignment cannot be less than the required alignment! */
    ralign = roundup(ralign, align);

    randum = get_random64(dip);

    /*
     * Set position so that the I/O is in the range from file_position to the
     * random data limit and is aligned to device, pattern, or user alignment.
     */
    if (rlimit) {		/* This will be zero for a single block! */
	pos = (Offset_t)(randum % rlimit);
    }
    /* Round down, instead of up, to avoid end of file/media issues. */
    pos = rounddown(pos, ralign);

    if (dip->di_file_position) {
	Offset_t npos;
	pos += dip->di_file_position;
	/* Realign if possible, but near the end, we cannot! */
	npos = roundup(pos, ralign);
	if ( (large_t)npos <= (dip->di_rdata_limit - xfer_size) ) {
	    pos = npos;
	}
#if 0
	 else {
	    /*DEBUG*/
	    Printf(dip, "too close to end, not realigning...\n");
	    Printf(dip, " -> rlimit = "LUF", ralign = "LUF", pos = "FUF", ((rlimit + position) - ralign) = "LUF", position = "FUF"\n",
		   rlimit, ralign, pos, ((rlimit + dip->di_file_position) - ralign), dip->di_file_position );
	}
#endif /* DEBUG */
    }

    if (doseek) {
	return( set_position(dip, pos, False) );
    } else {
	/*
	 * Note:  For AIO, we just calculate the random position.
	 */
	if (dip->di_Debug_flag || dip->di_rDebugFlag) {
	    large_t lba = (pos / dsize);
	    Printf (dip, "Random position set to offset " FUF ", block " LUF " (" LXF ").\n",
		    pos, lba, lba);
	}
	return(pos);
    }
}

/************************************************************************
 *									*
 * skip_records() Skip past specified number of records.		*
 *									*
 * Inputs:	dip	= The device info pointer.			*
 *		records = The number of records.			*
 *		buffer  = The buffer to read into.			*
 *		size    = The size of each record.			*
 *									*
 * Return Value:							*
 *	Returns SUCCESS/FAILURE/WARNING = Ok/Read Failed/Partial Read.	*
 *									*
 ************************************************************************/
int
skip_records(   struct dinfo    *dip,
		u_long          records,
		u_char          *buffer,
		size_t          size)
{
    u_long i;
    size_t count;
    int status = SUCCESS;

    /*
     * Skip over the specified record(s).
     */
    for (i = 0; i < records; i++) {
	count = os_read_file(dip->di_fd, buffer, size);
	if ( (status = check_read(dip, (ssize_t)count, size)) == FAILURE) {
	    break;
	}
    }
    return(status);
}

/************************************************************************
 *									*
 * CvtStrtoValue() - Converts ASCII String into Numeric Value.		*
 *									*
 * Inputs:							        * 
 * 	dip = The device information pointer.			        *
 *	nstr = String to convert.					*
 *	eptr = Pointer for terminating character pointer.		*
 *	base = The base used for the conversion.			*
 *									*
 * Outputs:								*
 * 	eptr = Points to terminating character or nstr if an		*
 *		invalid if numeric value cannot be formed.		*
 *									*
 * Return Value:							*
 *	Returns converted number or -1 for FAILURE.			*
 *									*
 ************************************************************************/
unsigned long 
CvtStrtoValue (dinfo_t *dip, char *nstr, char **eptr, int base)
{
    unsigned long n = 0, val;

    errno = 0;
    if ( (n = strtoul (nstr, eptr, base)) == 0) {
	if (nstr == *eptr) {
	    n++;
	}
    } else if ( (errno == ERANGE) && (n == 0x7fffffff) ) {
	/*
	 * Solaris 8 strtoul() is broken!
	 * Converting "c6dec6de" returns a failure!
	 * Thus, this cludgy workaround (for now).
	 */
	return( (unsigned int)CvtStrtoLarge(dip, nstr,eptr,base) );
    }
#ifdef notdef
    if (nstr == *eptr) {
	return(n);
    }
#endif /* notdef */
    nstr = *eptr;
    for (;;) {

	switch (*nstr++) {
	    
	    case 'k':
	    case 'K':			    /* Kilobytes */
		n *= KBYTE_SIZE;
		continue;

	    case 'g':
	    case 'G':			    /* Gigibytes */
		n *= GBYTE_SIZE;
		continue;

	    case 'm':
	    case 'M':			    /* Megabytes */
		n *= MBYTE_SIZE;
		continue;

#if defined(QuadIsLong) && !defined(_WIN64)
	    case 't':
	    case 'T':
		n *= TBYTE_SIZE;
		continue;
#endif /* defined(QuadIsLong) && !defined(_WIN64) */

	    case 'w':
	    case 'W':			    /* Word count. */
		n *= sizeof(int);
		continue;

	    case 'q':
	    case 'Q':			    /* Quadword count. */
		n *= sizeof(large_t);
		continue;

	    case 'b':
	    case 'B':			    /* Block count. */
		n *= BLOCK_SIZE;
		continue;

	    case 'd':
	    case 'D':			    /* Device size. */
		n *= dip->di_device_size;
		continue;

	    case 'c':
	    case 'C':			    /* Core clicks. */
	    case 'p':
	    case 'P':			    /* Page size. */
		n *= page_size;
		continue;

	    case 'i':
	    case 'I':
		if ( ( ( nstr[0] == 'N' ) || ( nstr[0] == 'n' ) ) &&
		     ( ( nstr[1] == 'F' ) || ( nstr[1] == 'f' ) )) {
		    nstr += 2;
		    n = (unsigned int)INFINITY;
		    continue;
		} else {
		    goto error;
		}

	    case '+':
		n += CvtStrtoValue (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '-':
		n -= CvtStrtoValue (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '*':
	    case 'x':
	    case 'X':
		n *= CvtStrtoValue (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '/':
		val = CvtStrtoValue (dip, nstr, eptr, base);
		if (val) n /= val;
		nstr = *eptr;
		continue;

	    case '%':
		val = CvtStrtoValue (dip, nstr, eptr, base);
		if (val) n %= val;
		nstr = *eptr;
		continue;

	    case '~':
		n = ~CvtStrtoValue (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '|':
		n |= CvtStrtoValue (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '&':
		n &= CvtStrtoValue (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '^':
		n ^= CvtStrtoValue (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '<':
		if (*nstr++ != '<') goto error;
		n <<= CvtStrtoValue (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '>':
		if (*nstr++ != '>') goto error;
		n >>= CvtStrtoValue (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case ' ':
	    case '\t':
		continue;

	    case '\0':
		*eptr = --nstr;
		break;

	    default:
		error:
		n = 0L;
		*eptr = --nstr;
		break;
	}
	return(n);
    }
}

/************************************************************************
 *									*
 * CvtStrtoLarge() - Converts ASCII String into Large Value.		*
 *									*
  * Inputs:							        * 
 * 	dip = The device information pointer.			        *
 *	nstr = String to convert.					*
 *	eptr = Pointer for terminating character pointer.		*
 *	base = The base used for the conversion.			*
 *									*
 * Outputs:								*
 * 	eptr = Points to terminating character or nstr if an		*
 *		invalid if numeric value cannot be formed.		*
 *									*
 * Return Value:							*
 *	Returns converted number or -1 for FAILURE.			*
 *									*
 ************************************************************************/
large_t
CvtStrtoLarge (dinfo_t *dip, char *nstr, char **eptr, int base)
{
    large_t n = 0, val;

    /* Thankfully we have POSIX standards, eh? :-( */
#if defined(WIN32)
    if ( (n = _strtoui64(nstr, eptr, base)) == (large_t) 0) {
#elif defined(QuadIsLong)
    /* Note: The assumption here is that 64-bit OS's strtoul() return 64-bits! */
    /*       Now this was true on 64-bit Tru64 Unix on Alpha, but true for all OS's? */
    /*       FYI: This is NOT true for Windows! (grrr) */
    if ( (n = strtoul (nstr, eptr, base)) == (large_t) 0) {
#elif defined(QuadIsLongLong)
# if defined(HP_UX)
    if ( (n = strtoumax(nstr, eptr, base)) == (large_t) 0) {
# else /* All other OS's */
    if ( (n = strtoull(nstr, eptr, base)) == (large_t) 0) {
# endif /* defined(SCO) || defined(__QNXNTO__) || defined(SOLARIS) || defined(AIX) || defined(_NT_SOURCE) */
#else 
  #error "Define 64-bit string conversion API!"
#endif /* defined(WIN32) */
	/* TODO: Add error checking AFTER standardizing on API! */
	if (nstr == *eptr) {
	    n++;
	}
    }
#ifdef notdef
    if (nstr == *eptr) {
	return(n);
    }
#endif /* notdef */
    nstr = *eptr;
    for (;;) {

	switch (*nstr++) {
	    
	    case 'k':
	    case 'K':			    /* Kilobytes */
		n *= KBYTE_SIZE;
		continue;

	    case 'g':
	    case 'G':			    /* Gigibytes */
		n *= GBYTE_SIZE;
		continue;

	    case 'm':
	    case 'M':			    /* Megabytes */
		n *= MBYTE_SIZE;
		continue;

	    case 't':
	    case 'T':
		n *= TBYTE_SIZE;
		continue;

	    case 'w':
	    case 'W':			    /* Word count. */
		n *= sizeof(int);
		continue;

	    case 'q':
	    case 'Q':			    /* Quadword count. */
		n *= sizeof(large_t);
		continue;

	    case 'b':
	    case 'B':			    /* Block count. */
		n *= BLOCK_SIZE;
		continue;

	    case 'd':
	    case 'D':			    /* Device size. */
		n *= dip->di_device_size;
		continue;

	    case 'c':
	    case 'C':			    /* Core clicks. */
	    case 'p':
	    case 'P':			    /* Page size. */
		n *= page_size;
		continue;

	    case 'i':
	    case 'I':
		if ( ( ( nstr[0] == 'N' ) || ( nstr[0] == 'n' ) ) &&
		     ( ( nstr[1] == 'F' ) || ( nstr[1] == 'f' ) )) {
		    nstr += 2;
		    n = INFINITY;
		    continue;
		} else {
		    goto error;
		}

	    case '+':
		n += CvtStrtoLarge (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '-':
		n -= CvtStrtoLarge (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '*':
	    case 'x':
	    case 'X':
		n *= CvtStrtoLarge (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '/':
		val = CvtStrtoLarge (dip, nstr, eptr, base);
		if (val) n /= val;
		nstr = *eptr;
		continue;
#if !defined(QuadIsDouble)
	    case '%':
		val = CvtStrtoLarge (dip, nstr, eptr, base);
		if (val) n %= val;
		nstr = *eptr;
		continue;

	    case '~':
		n = ~CvtStrtoLarge (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '|':
		n |= CvtStrtoLarge (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '&':
		n &= CvtStrtoLarge (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '^':
		n ^= CvtStrtoLarge (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '<':
		if (*nstr++ != '<') goto error;
		n <<= CvtStrtoLarge (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;

	    case '>':
		if (*nstr++ != '>') goto error;
		n >>= CvtStrtoLarge (dip, nstr, eptr, base);
		nstr = *eptr;
		continue;
#endif /* !defined(QuadIsDouble) */
	    case ' ':
	    case '\t':
		continue;

	    case '\0':
		*eptr = --nstr;
		break;

	    default:
		error:
		n = 0;
		*eptr = --nstr;
		break;
	}
	return(n);
    }
}

/************************************************************************
 *									*
 * CvtTimetoValue() - Converts ASCII Time String to Numeric Value.	*
 *									*
 * Inputs:	nstr = String to convert.				*
 *		eptr = Pointer for terminating character pointer.	*
 *									*
 * Outputs:	eptr = Points to terminating character or nstr if an	*
 *			invalid if numeric value cannot be formed.	*
 *									*
 * Return Value:							*
 *		Returns converted number in seconds or -1 for FAILURE.	*
 *									*
 ************************************************************************/
time_t
CvtTimetoValue (char *nstr, char **eptr)
{
    time_t n = 0;
    int base = ANY_RADIX;

    if ( (n = strtoul (nstr, eptr, base)) == 0L) {
	if (nstr == *eptr) {
	    n++;
	}
    }
#ifdef notdef
    if (nstr == *eptr) {
	return(n);
    }
#endif /* notdef */
    nstr = *eptr;
    for (;;) {

	switch (*nstr++) {
	    
	    case 'd':
	    case 'D':			    /* Days */
		n *= SECS_PER_DAY;
		continue;

	    case 'h':
	    case 'H':			    /* Hours */
		n *= SECS_PER_HOUR;
		continue;

	    case 'm':
	    case 'M':			    /* Minutes */
		n *= SECS_PER_MIN;
		continue;

	    case 's':
	    case 'S':			    /* Seconds */
		continue;		/* default */

	    case '+':
		n += CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case '-':
		n -= CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case '*':
	    case 'x':
	    case 'X':
		n *= CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case '/':
		n /= CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case '%':
		n %= CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		nstr--;
		n += CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case ' ':
	    case '\t':
		continue;

	    case '\0':
		*eptr = --nstr;
		break;
	    default:
		n = 0L;
		*eptr = --nstr;
		break;
	}
	return(n);
    }
}

/*
 * Format & append time string to log file buffer.
 */
/*VARARGS*/
void
Ctime(dinfo_t *dip, time_t timer)
{
    char *buf;
    char *bp = dip->di_log_bufptr;

    buf = os_ctime(&timer, dip->di_time_buffer, sizeof(dip->di_time_buffer));
    (void)strcpy(bp, buf);
    bp += strlen(bp);
    dip->di_log_bufptr = bp;
}

/************************************************************************
 *									*
 * Fputs()	Common function to Write String to an Output Stream.	*
 *									*
 * Inputs:	str = The string buffer pointer.			*
 *		stream = The file stream to access.			*
 *									*
 * Return Value:							*
 *		Returns 0 / -1 = SUCCESS / FAILURE.			*
 * 									*
 ************************************************************************/
int
Fputs(char *str, FILE *stream)
{
    int status = SUCCESS;

    (void)fputs((char *)str, stream);
    if (ferror(stream) != 0) {
	clearerr(stream);
	status = FAILURE;
    }
    return(status);
}

/************************************************************************
 *									*
 * is_Eof() - Check For End Of File Condition.				*
 *									*
 * Description:								*
 *	Detect end of file or end of media. Here's the presumptions:	*
 *									*
 *  For Writes, we expect a errno (count == -1) and (errno == ENOSPC).	*
 *   For Reads, a (count == 0) indicates end of file, while a		*
 *	(count == -1) and (errno == ENOSPC) indicates end of medium.	*
 *									*
 *	Actually, two file marks normally indicates the end of logical	*
 * tape, while (errno == ENOSPC) normally indicates reading past all of	*
 * the recorded data.  Note, some tapes (QIC) only have one file mark.	*
 *									*
 *	Is this confusing or what?  I'm doing the best I can here :-)	*
 *									*
 * Inputs:	dip = The device information pointer.			*
 *      	count = The count from the last I/O request.            * 
 *      	size = The original request size (for error reporting)	*
 *		status = Optional pointer to return status.		*
 *									*
 * Return Value:							*
 *		Returns True / False = End of file / Not Eof.		*
 *									*
 ************************************************************************/
int
is_Eof(struct dinfo *dip, ssize_t count, size_t size, int *status)
{
    hbool_t detected_eof = False;
    hbool_t read_mode = (dip->di_mode == READ_MODE);
    os_error_t error = os_get_error();

    if ( (dip->di_scsi_io_flag == True) && (count == 0) ) {
	set_Eof(dip);
	return(dip->di_end_of_file);
    }
    /*
     * Note: This is overloaded! It needs broken into read/write EOF
     * handling, as well as disk vs. tape EOM handling (assuming we keep
     * tape support). It's messy and too complicated to follow! -Robin
     */
    /*
     * NOTE: Apparently EOF is not defined by POSIX, since each OS differs!
     *
     * Also NOTE: If this occurs, all the new "no space" logic is screwed!
     */
    if ( (dip->di_mode == WRITE_MODE) && (count == 0) ) {
#if defined(BrokenEOF)
	/* Abnormal, but added for Solaris! Is this still true? */
	/* Note: This was added a long time ago, needs revisited! */
	set_Eof(dip);
	return(dip->di_end_of_file);
#else /* !defined(BrokenEOF) */
	return(False);
#endif /* defined(BrokenEOF) */
    }

    /* Note: File system full is treated like EOF! (historic) */
    detected_eof = os_isEof(count, error);
    dip->di_file_system_full = os_isDiskFull(error);
    if (detected_eof == True) {
	large_t data_bytes;
	if (read_mode) {
	    data_bytes = dip->di_dbytes_read;
	    if (count == 0) {		/* Read EOF condition. */
		errno = 0;		/* Avoid reporting bogus error message. */
	    }
	} else {
	    data_bytes = dip->di_dbytes_written;
	}
	if ( dip->di_file_system_full && dip->di_multiple_files &&
	     (dip->di_maxdata_read || dip->di_maxdata_written) ) {
	    dip->di_no_space_left = True; /* Historic logic requires this! */
	}
	if (dip->di_file_system_full) {
	    (void)isFsFullOk(dip, OS_WRITE_FILE_OP, dip->di_dname);
	    if (dip->di_verbose_flag && dip->di_fsfull_restart) {
		/* Report extended error information for file system full. */
		/* Note: This error could be from async I/O (AIO)! */
		INIT_ERROR_INFO(eip, dip->di_dname, OS_WRITE_FILE_OP, WRITE_OP, &dip->di_fd,
				dip->di_oflags, dip->di_offset, size, error,
				logLevelWarn, PRT_NOFLAGS, (RPT_WARNING|RPT_NOHISTORY));
		(void)ReportRetryableError(dip, eip, "Failed writing %s", dip->di_dname);
	    }
	} else {
	    report_EofMsg(dip, count, error);
	}
#if defined(TAPE)
	if (dip->di_dtype->dt_dtype == DT_TAPE) {
	    if (count == 0) {
		/* Two file mark's == end of logical tape. */
		if (dip->di_end_of_file) dip->di_end_of_logical = True;
		if (dip->di_end_of_logical) dip->di_end_of_media = True;
	    } else { /* ENOSPC */
		/* Remember, QIC tapes only have one file mark! */
		dip->di_end_of_logical = True;
		dip->di_end_of_media = True;
	    }
	}
#endif /* defined(TAPE) */
	return(dip->di_end_of_file = True);
    }
    return(False);
}

void
report_EofMsg(dinfo_t *dip, ssize_t count, os_error_t error)
{
    hbool_t read_mode = (dip->di_mode == READ_MODE);

    if (dip->di_debug_flag || dip->di_eDebugFlag || dip->di_file_system_full) {
	char *endofmsg;
	if (count == 0) {
	    endofmsg = "End of file";
	} else if ( dip->di_file_system_full ) {
	    endofmsg = os_getDiskFullSMsg(error);
	} else {
	    endofmsg = (count == 0) ? "End of file" : "End of media";
	}
	Printf(dip, "File name: %s\n", dip->di_dname);
	Printf(dip, "%s detected, count = "SDF", error = %d [file #%lu, record #%lu, %s " FUF " file bytes, " FUF " total bytes]\n",
	       endofmsg, count, error,
	       (read_mode) ? (dip->di_files_read + 1) : (dip->di_files_written + 1),
	       (read_mode) ? (dip->di_records_read + 1) : (dip->di_records_written + 1),
	       (read_mode) ? "read" : "wrote", 
	       (read_mode) ? dip->di_fbytes_read : dip->di_fbytes_written,
	       (read_mode) ? dip->di_dbytes_read : dip->di_dbytes_written);
    }
    return;
}

/*
 * Used to mimic EOF @ BOM when direction is reverse.
 */
void
set_Eof(struct dinfo *dip)
{
    if (dip->di_debug_flag || dip->di_eDebugFlag) {
	hbool_t read_mode = (dip->di_mode == READ_MODE);
	long files, records;
	large_t data_bytes, file_bytes;
	char *iotype;
	char *endofmsg = (dip->di_fsfile_flag == True) ? "file" : "media";
	if ( dip->di_read_percentage ) {
	    iotype = "read/wrote";
	    files = (dip->di_files_read + dip->di_files_written) + 1;
	    records = (dip->di_records_read + dip->di_records_written) + 1;
	    data_bytes = (dip->di_dbytes_read + dip->di_dbytes_written);
	    file_bytes = (dip->di_fbytes_read + dip->di_fbytes_written);
	} else {
	    iotype = (read_mode) ? "read" : "wrote",
	    files = (read_mode) ? (dip->di_files_read + 1) : (dip->di_files_written + 1);
	    records = (read_mode) ? (dip->di_records_read + 1) : (dip->di_records_written + 1);
	    data_bytes = (read_mode) ? dip->di_dbytes_read : dip->di_dbytes_written;
	    file_bytes = (read_mode) ? dip->di_fbytes_read : dip->di_fbytes_written;
	}
	Printf(dip, "File name: %s\n", dip->di_dname);
	Printf(dip, "%s of %s detected [file #%lu, record #%lu, %s " FUF " file bytes, " FUF " total bytes]\n",
	       (dip->di_io_dir == REVERSE) ? "Beginning" : "End", endofmsg,
	       files, records, iotype, file_bytes, data_bytes);
    }
    /* Clone of what's in is_Eof(), but also needs cleaned up! */
    if (exit_status != FAILURE)	exit_status = END_OF_FILE;
    /* Set a "fake" EOF condition! */
    dip->di_end_of_file = True;
    return;
}

/*
 * Check for all hex characters in string.
 */
int
IS_HexString (char *s)
{
    if ( (*s == '0') &&
	 ((*(s+1) == 'x') || (*(s+1) == 'X')) ) {
	s += 2;	/* Skip over "0x" or "0X" */
    }
    while (*s) {
	if ( !isxdigit((int)*s++) ) return(False);
    }
    return(True);
}

/*
 * String copy with special mapping.
 */
int
StrCopy(void *to_buffer, void *from_buffer, size_t length)
{
    u_char *to = (u_char *) to_buffer;
    u_char *from = (u_char *) from_buffer;
    u_char c, key;
    int count = 0;

    while ( length ) {
	c = *from++; length--;
	if ( (c != '^') && (c != '\\') ) {
	    *to++ = c; count++;
	    continue;
	}
	if (length == 0) {
	    *to++ = c; count++;
	    continue;
	}
	if (c == '^') {		/* control/X */
	    c = *from++; length--;
	    *to++ = (c & 037); count++;
	    continue;
	}
	c = *from++; length--;
	if (c == 'a')  key = '\007';   /* alert (bell) */
	else if (c == 'b')  key = '\b';	/* backspace */
	else if ( (c == 'e') || (c == 'E') )
	    key = '\033';   /* escape */
	else if (c == 'f')  key = '\f';	/* formfeed */
	else if (c == 'n')  key = '\n';	/* newline */
	else if (c == 'r')  key = '\r';	/* return */
	else if (c == 't')  key = '\t';	/* tab */
	else if (c == 'v')  key = '\v';	/* vertical tab */
	else if ( (length >= 2) &&
		  ((c == 'x') || (c == 'X')) ) { /* hex */
	    int cnt;
	    u_char val = 0;
	    for (cnt = 0, key = 0; cnt < 2; cnt++) {
		c = *from++; count--;
		if ( isxdigit(c) ) {
		    if ( ('c' >= '0') && (c <= '9') )
			val = (c - '0');
		    else if ( ('c' >= 'a') && (c <= 'f') )
			val = 10 + (c - 'a');
		    else if ( ('c' >= 'A') && (c <= 'F') )
			val = 10 + (c - 'A');
		} else {
		    key = c;
		    break;
		}
		key = (key << 4) | val;
	    }
	} else if ( (length >= 3) &&
		    ((c >= '0') && (c <= '7')) ) { /* octal */
	    int cnt;
	    for (cnt = 0, key = 0; cnt < 3; cnt++) {
		key = (key << 3) | (c - '0');
		c = *from++; length--;
		if (c < '0' || c > '7')	break;
	    }
	} else {
	    key = c;	/* Nothing special here... */
	}
	*to++ = key; count++;
    }
    return(count);
}

/************************************************************************
 *									*
 * stoh() - Convert SCSI bytes to Host short/int/long format.		*
 *									*
 * Description:								*
 *	This function converts SCSI (big-endian) byte ordering to the	*
 * format necessary by this host.					*
 *									*
 * Inputs:	bp = Pointer to SCSI data bytes.			*
 *		size = The conversion size in bytes.			*
 *									*
 * Return Value:							*
 *		Returns a long value with the proper byte ordering.	*
 *									*
 ************************************************************************/
large_t
stoh(unsigned char *bp, size_t size)
{
    large_t value = 0L;

    switch (size) {
	
	case sizeof(uint8_t):
	    value = (large_t) bp[0];
	    break;

	case sizeof(uint16_t):
	    value = ( ((large_t)bp[0] << 8) | (large_t)bp[1] );
	    break;

	case 0x03:
	    value = ( ((large_t)bp[0] << 16) | ((large_t)bp[1] << 8) |
		      (large_t) bp[2] );
	    break;

	case sizeof(uint32_t):
	    value = ( ((large_t)bp[0] << 24) | ((large_t)bp[1] << 16) |
		      ((large_t)bp[2] << 8) | (large_t)bp[3]);
	    break;

	case 0x05:
	    value = ( ((large_t)bp[0] << 32L) | ((large_t)bp[1] << 24) |
		      ((large_t)bp[2] << 16) | ((large_t)bp[3] << 8) |
		      (large_t)bp[4] );
	    break;

	case 0x06:
	    value = ( ((large_t)bp[0] << 40L) | ((large_t)bp[1] << 32L) |
		      ((large_t)bp[2] << 24) | ((large_t)bp[3] << 16) |
		      ((large_t)bp[4] << 8) | (large_t)bp[5] );
	    break;

	case 0x07:
	    value = ( ((large_t)bp[0] << 48L) | ((large_t)bp[1] << 40L) |
		      ((large_t)bp[2] << 32L) | ((large_t)bp[3] << 24) |
		      ((large_t)bp[4] << 16) | ((large_t)bp[5] << 8) |
		      (large_t)bp[6] );
	    break;

	case sizeof(uint64_t):
	    value = ( ((large_t)bp[0] << 56L) | ((large_t)bp[1] << 48L) |
		      ((large_t)bp[2] << 40L) | ((large_t)bp[3] << 32L) |
		      ((large_t)bp[4] << 24) | ((large_t)bp[5] << 16) |
		      ((large_t)bp[6] << 8) | (large_t)bp[7] );
	    break;

	default:
	    Fprintf(NULL, bad_conversion_str, size);
	    break;

    }
    return(value);
}

/************************************************************************
 *									*
 * htos() - Convert Host short/int/long format to SCSI bytes.		*
 *									*
 * Description:								*
 *	This function converts host values to SCSI (big-endian) byte	*
 * ordering.								*
 *									*
 * Inputs:	bp = Pointer to SCSI data bytes.			*
 *		value = The numeric value to convert.			*
 *		size = The conversion size in bytes.			*
 *									*
 * Return Value:							*
 *		Void.							*
 *									*
 ************************************************************************/
void
htos(unsigned char *bp, large_t value, size_t size)
{
    switch (size) {
	
	case sizeof(uint8_t):
	    bp[0] = (uint8_t)value;
	    break;

	case sizeof(uint16_t):
	    bp[0] = (uint8_t)(value >> 8);
	    bp[1] = (uint8_t)value;
	    break;

	case 0x03:
	    bp[0] = (uint8_t)(value >> 16);
	    bp[1] = (uint8_t)(value >> 8);
	    bp[2] = (uint8_t)value;
	    break;

	case sizeof(uint32_t):
	    bp[0] = (uint8_t)(value >> 24);
	    bp[1] = (uint8_t)(value >> 16);
	    bp[2] = (uint8_t)(value >> 8);
	    bp[3] = (uint8_t)value;
	    break;

	case 0x05:
	    bp[0] = (uint8_t)(value >> 32L);
	    bp[1] = (uint8_t)(value >> 24);
	    bp[2] = (uint8_t)(value >> 16);
	    bp[3] = (uint8_t)(value >> 8);
	    bp[4] = (uint8_t)value;
	    break;

	case 0x06:
	    bp[0] = (uint8_t)(value >> 40L);
	    bp[1] = (uint8_t)(value >> 32L);
	    bp[2] = (uint8_t)(value >> 24);
	    bp[3] = (uint8_t)(value >> 16);
	    bp[4] = (uint8_t)(value >> 8);
	    bp[5] = (uint8_t)value;
	    break;

	case 0x07:
	    bp[0] = (uint8_t)(value >> 48L);
	    bp[1] = (uint8_t)(value >> 40L);
	    bp[2] = (uint8_t)(value >> 32L);
	    bp[3] = (uint8_t)(value >> 24);
	    bp[4] = (uint8_t)(value >> 16);
	    bp[5] = (uint8_t)(value >> 8);
	    bp[6] = (uint8_t)value;
	    break;

	case sizeof(uint64_t):
	    bp[0] = (uint8_t)(value >> 56L);
	    bp[1] = (uint8_t)(value >> 48L);
	    bp[2] = (uint8_t)(value >> 40L);
	    bp[3] = (uint8_t)(value >> 32L);
	    bp[4] = (uint8_t)(value >> 24);
	    bp[5] = (uint8_t)(value >> 16);
	    bp[6] = (uint8_t)(value >> 8);
	    bp[7] = (uint8_t)value;
	    break;

	default:
	    Fprintf(NULL, bad_conversion_str, size);
	    break;
    }
    return;
}

trigger_control_t
parse_trigger_control(dinfo_t *dip, char *control)
{
    trigger_control_t trigger_control;

    if (strcmp(control, "all") == 0) {
	trigger_control = TRIGGER_ON_ALL;
    } else if (strcmp(control, "errors") == 0) {
	trigger_control = TRIGGER_ON_ERRORS;
    } else if ( (strcmp(control, "miscompare") == 0) || strcmp(control, "corruption") == 0) {
	trigger_control = TRIGGER_ON_MISCOMPARE;
    } else if (strcmp(control, "noprogs") == 0) {
	trigger_control = TRIGGER_ON_NOPROGS;
    } else {
	Eprintf(dip, "Valid trigger controls: all, errors, miscompare, or noprogs\n");
	trigger_control = TRIGGER_ON_INVALID;
    }
    return(trigger_control);
}

int
add_trigger_type(dinfo_t *dip, char *trigger)
{
    trigger_data_t *tdp = &dip->di_triggers[dip->di_num_triggers];

    if (dip->di_num_triggers == NUM_TRIGGERS) {
	Eprintf(dip, "Maximum number of triggers is %d.\n", NUM_TRIGGERS);
	return (FAILURE);
    }
    if ((tdp->td_trigger = parse_trigger_type(dip, trigger)) == TRIGGER_INVALID) {
	return (FAILURE);
    }
    dip->di_num_triggers++;
    return (SUCCESS);
}

int
add_default_triggers(dinfo_t *dip)
{
    int status = SUCCESS;
    return (status);
}

void
remove_triggers(dinfo_t *dip)
{
    int i;
    /*
     * Trigger scripts and arguments:
     */ 
    for (i = 0; (i < dip->di_num_triggers); i++) {
	dip->di_triggers[i].td_trigger = TRIGGER_NONE;
	if (dip->di_triggers[i].td_trigger_cmd) {
	    FreeStr(dip, dip->di_triggers[i].td_trigger_cmd);
	    dip->di_triggers[i].td_trigger_cmd = NULL;
	}
	if (dip->di_triggers[i].td_trigger_args) {
	    FreeStr(dip, dip->di_triggers[i].td_trigger_args);
	    dip->di_triggers[i].td_trigger_args = NULL;
	}
    }
    dip->di_num_triggers = 0;
    dip->di_trigger_control = TRIGGER_ON_ALL;
    return;
}

hbool_t
trigger_type_exist(dinfo_t *dip, trigger_type_t trigger_type)
{
    trigger_data_t *tdp = &dip->di_triggers[dip->di_num_triggers];
    int triggers;

    for (triggers = 0; (triggers < dip->di_num_triggers); tdp++, triggers++) {
	if (tdp->td_trigger == trigger_type) {
	    return (True);
	}
    }
    return (False);
}

trigger_type_t
parse_trigger_type(dinfo_t *dip, char *trigger)
{
    trigger_data_t *tdp = &dip->di_triggers[dip->di_num_triggers];
    trigger_type_t trigger_type = TRIGGER_INVALID;

    if (strcmp(trigger, "br") == 0) {
	trigger_type = TRIGGER_BR;
    } else if (strcmp(trigger, "bdr") == 0) {
	trigger_type = TRIGGER_BDR;
    } else if (strcmp(trigger, "lr") == 0) {
	trigger_type = TRIGGER_LR;
    } else if (strcmp(trigger, "seek") == 0) {
	trigger_type = TRIGGER_SEEK;
#if defined(SCSI)
    } else if (strncmp(trigger, "cdb:", 4) == 0) {
	uint32_t value;
	char *token, *saveptr;
	char *sep = " ";
	char *cdbp = strdup(&trigger[4]);

	trigger_type = TRIGGER_CDB;
	if (strchr(cdbp, ',')) {
	    sep = ",";
	}
	dip->di_cdb_size = 0;
	token = strtok_r(cdbp, sep, &saveptr);
	while (token != NULL) {
	    int status;
	    value = number(dip, token, HEX_RADIX, &status, True);
	    if (status == FAILURE) {
		trigger_type = TRIGGER_INVALID;
		break;
	    }
	    if (value > 0xFF) {
		Eprintf(dip, "CDB byte value %#x is too large!\n", value);
		trigger_type = TRIGGER_INVALID;
		break;
	    }
	    dip->di_cdb[dip->di_cdb_size++] = (uint8_t)value;
	    token = strtok_r(NULL, sep, &saveptr);
	    if (dip->di_cdb_size >= MAX_CDB) {
		Eprintf(dip, "Maximum CDB size is %d bytes!\n", MAX_CDB);
		trigger_type = TRIGGER_INVALID;
		break;
	    }
	}
	Free(dip, cdbp);
#endif /* defined(SCSI) */
    } else if (strncmp(trigger, "cmd:", 4) == 0) {
	char *strp;
	tdp->td_trigger_cmd = strdup(&trigger[4]);
	strp = strstr(tdp->td_trigger_cmd, " ");
	/*
	 * Extra args get appended after our trigger arguments.
	 */
	if (strp) {
	    *strp++ = '\0';
	    tdp->td_trigger_args = strdup(strp);
	}
	trigger_type = TRIGGER_CMD;
    } else if (strcmp(trigger, "triage") == 0) {
#if defined(SCSI)
	trigger_type = TRIGGER_TRIAGE;
#else /* !defined(SCSI) */
	Wprintf(dip, "The triage trigger is *only* supported for SCSI right now!\n");
	return(TRIGGER_INVALID);
#endif /* defined(SCSI) */
    } else {
	Eprintf(dip, "Valid trigger types are: br, bdr, lr, seek, triage, cdb:bytes, cmd:string\n");
	trigger_type = TRIGGER_INVALID;
    }
    return( trigger_type );
}

int
DoSystemCommand(dinfo_t *dip, char *cmdline)
{
    if ( (cmdline == NULL) || (strlen(cmdline) == 0) ) return(WARNING);
    return( ExecuteCommand(dip, cmdline, LogPrefixDisable, dip->di_debug_flag) );
}

int
StartupShell(dinfo_t *dip, char *shell)
{
    static char *Shell;
#if defined(WIN32)
    Shell = "cmd.exe";
    /* The above should be in the users' path! */
    //Shell = "c:\\windows\\system32\\cmd.exe";
#endif /* !defined(WIN32) */

    if (shell) Shell = shell;
#if !defined(WIN32)
    if (Shell == NULL) {
	if ( (Shell = getenv("SHELL")) == NULL) {
	    if (access("/bin/ksh", X_OK) == SUCCESS) {
		Shell = "/bin/ksh";	/* My preference... */
	    } else {
		Shell = "/bin/sh";	/* System shell. */
	    }
	}
    }
#endif /* !defined(WIN32) */
    /*
     * Startup the shell.
     */
    return( system(Shell) );
}

/*
 * ExecuteCommand() - Execute an external OS Command.
 * 
 * Inputs:
 *	dip = The device information pointer.
 *      cmd = The command line to execute.
 *      prefix = The log prefix control flag.
 * 	verbose = Verbose control flag.
 * 
 * Return Value:
 * 	The exit status of the executed command line.
 */ 
int
ExecuteCommand(dinfo_t *dip, char *cmd, hbool_t prefix, hbool_t verbose)
{
    FILE *pipef;
    char cmd_line[STRING_BUFFER_SIZE];
    char cmd_output[STRING_BUFFER_SIZE];
    int status;

    /* To easily acquire stderr, merge it with stdout! */
    /* Note: Thankfully this works with Unix shell and DOS shell. */
    (void)sprintf(cmd_line, "%s 2>&1", cmd);
    if (verbose == True) {
	Printf(dip, "Executing: %s\n", cmd_line);
    }
    pipef = popen(cmd_line, "r");
    if (pipef == NULL) {
	Perror(dip, "popen() failed");
	return(FAILURE);
    }
    memset(cmd_output, '\0', sizeof(cmd_output));
    /*
     * Read and log output from the script.
     */
    while (fgets (cmd_output, sizeof(cmd_output), pipef) == cmd_output) {
	if (prefix) {
	    Printf(dip, "%s", cmd_output);
	} else {
	    Print(dip, "%s", cmd_output);
	}
    }
    status = pclose(pipef);
#if 0
    if (status || dip->di_debug_flag) {
	Printf(dip, "pclose() returned status %d (0x%x), errno = %d\n", status, status, errno);
    }
#endif /* 0 */
    status = WEXITSTATUS(status);
    return(status);
}

/*
 * ExecuteBuffered() - Execute Cmd and Return Data.
 * 
 * Inputs:
 *	dip = The device information pointer.
 *	cmd = The command line to execute.
 *	buffer = Buffer for returned data.
 *	bufsize = The buffer size.
 * 
 * Return Value:
 *	The exit status of the execute command line.
 */ 
int
ExecuteBuffered(dinfo_t *dip, char *cmd, char *buffer, int bufsize)
{
    char cmd_line[STRING_BUFFER_SIZE];
    FILE *pipef;
    int status;
    char *bp = buffer;

    (void)sprintf(cmd_line, "%s 2>&1", cmd);
    if (dip->di_pDebugFlag) {
	Printf(dip, "Executing: %s\n", cmd_line);
    }
    pipef = popen(cmd_line, "r");
    if (pipef == NULL) {
	if (dip->di_pDebugFlag) {
	    Perror(dip, "popen() failed");
	}
	return(FAILURE);
    }
    /*
     * Read and log output from the script.
     */
    while (fgets (bp, bufsize, pipef) == bp) {
	int len = (int)strlen(bp);
	if (dip->di_pDebugFlag) {
	    Fputs(bp, dip->di_ofp);
	}
	bp += len;
	bufsize -= len;
	if (len <= 0) break;
    }
    status = pclose(pipef);
    status = WEXITSTATUS(status);
    return(status);
}

int
ExecutePassCmd(dinfo_t *dip)
{
    char cmd[STRING_BUFFER_SIZE];
    large_t data_bytes = 0;
    int status;

    if (dip->di_random_io && dip->di_rdata_limit) {
	data_bytes = (dip->di_rdata_limit - dip->di_file_position);
    } else if (dip->di_data_limit && (dip->di_data_limit != INFINITY)) {
	data_bytes = dip->di_data_limit;
    }
    
    /* 
     * Format:
     * 	 pass_cmd device_name device_size starting_offset data_limit pass_count thread_number
     */ 
    (void)sprintf(cmd, "%s %s %u "FUF" "LUF" %lu %d",
		  dip->di_pass_cmd,
		  dip->di_dname, dip->di_dsize,
		  dip->di_file_position, data_bytes,
		  dip->di_pass_count, dip->di_thread_number);

    status = ExecuteCommand(dip, cmd, LogPrefixEnable, True);

    if (status || dip->di_debug_flag) {
	Printf(dip, "pass cmd exited with status %d...\n", status);
    }
    return (status);
}

/*
 * ExecuteTrigger() - Execute the User Requested Trigger.
 * 
 * Inputs:
 *	dip = The device information pointer.
 *	plus variable argument list for some triggers.
 *	For trigger script, the operation type is passed!
 * 
 * Return Value:
 *	Returns the exit status of the executed trigger.
 */ 
int
ExecuteTrigger(struct dinfo *dip, ...)
{
    char cmd[STRING_BUFFER_SIZE];
    int status = TRIGACT_CONTINUE;
    int i;

    if (dip->di_num_triggers == 0) return(status);

    for (i = 0; (i < dip->di_num_triggers); i++) {
	trigger_data_t *tdp = &dip->di_triggers[i];

	switch (tdp->td_trigger) {
	    case TRIGGER_NONE:
		return(status);
		/*NOTREACHED*/
		break;

	    case TRIGGER_BR:
#if defined(SCSI)
		if (dip->di_scsi_flag) {
		    Printf(dip, "Executing SCSI Bus Reset...\n");
		    status = os_reset_bus(dip->di_sgp);
		} else
#endif /* defined(SCSI) */
		{
		    (void)sprintf(cmd, "%s dsf=%s op=bus_rest",
				  (dip->di_spt_path) ? dip->di_spt_path : spt_path,
				  dip->di_dname);
		    status = ExecuteCommand(dip, cmd, LogPrefixEnable, True);
		}
		break;

	    case TRIGGER_BDR:
#if defined(SCSI)
		if (dip->di_scsi_flag) {
		    Printf(dip, "Executing SCSI Bus Device Reset...\n");
		    status = os_reset_device(dip->di_sgp);
		} else
#endif /* defined(SCSI) */
		{
		    (void)sprintf(cmd, "%s dsf=%s op=target_rest",
				  (dip->di_spt_path) ? dip->di_spt_path : spt_path,
				  dip->di_dname);
		    status = ExecuteCommand(dip, cmd, LogPrefixEnable, True);
		}
		break;

	    case TRIGGER_LR:
#if defined(SCSI)
		if (dip->di_scsi_flag) {
		    Printf(dip, "Executing SCSI LUN Reset...\n");
		    status = os_reset_lun(dip->di_sgp);
		} else
#endif /* defined(SCSI) */
		{
		    (void)sprintf(cmd, "%s dsf=%s op=lun_reset",
				  (dip->di_spt_path) ? dip->di_spt_path : spt_path,
				  dip->di_dname);
		    status = ExecuteCommand(dip, cmd, LogPrefixEnable, True);
		}
		break;

	    case TRIGGER_SEEK: {
		large_t lba =  makeLBA(dip, getFileOffset(dip));
#if defined(SCSI)
		if (dip->di_scsi_flag) {
		    scsi_generic_t *sgp = dip->di_sgp;
		    Printf(dip, "Executing Seek(10) to lba %u...\n", lba);
		    status = Seek10(sgp->fd, sgp->dsf, dip->di_sDebugFlag, dip->di_scsi_errors,
				    NULL, &sgp, (unsigned int)lba, sgp->timeout);
		} else
#endif /* defined(SCSI) */
		{
		    /* Note: Need to implement seek in spt! */
		    (void)sprintf(cmd, "scu -f %s seek lba " LUF,
				  dip->di_dname, lba);
		    status = ExecuteCommand(dip, cmd, LogPrefixEnable, True);
		}
		break;
	    }

	    case TRIGGER_TRIAGE:
#if defined(SCSI)
		status = do_scsi_triage(dip);
#endif /* defined(SCSI) */
		break;

	    case TRIGGER_CDB: {
#if defined(SCSI)
		scsi_generic_t *sgp = dip->di_sgp;
		if (dip->di_scsi_flag) {
		    Printf(dip, "Executing User Defined Trigger CDB...\n");
		    status = SendAnyCdb(sgp->fd, sgp->dsf, dip->di_sDebugFlag, True/*dip->di_scsi_errors*/,
					NULL, &sgp, sgp->timeout, dip->di_cdb, dip->di_cdb_size);
		} else {
		    Wprintf(dip, "SCSI device was NOT detected, so cannot send SCSI CDB!\n");
		}
#endif /* defined(SCSI) */
		break;
	    }

	    case TRIGGER_CMD: {
		va_list ap;
		char *op;
		char *cmdp = cmd;
		va_start(ap, dip);
		op = va_arg(ap, char *);
		va_end(ap);
		/*
		 * The tester has the option of disabling standard trigger args, since 
		 * the external command/script may not desire our default arguments. 
		 */
		if (dip->di_trigargs_flag) {
		    Offset_t offset = getFileOffset(dip);
		    large_t lba =  makeLBA(dip, offset);
		    /*
		     * Format: cmd dname op dsize offset bindex lba error noprogt
		     */
		    cmdp += sprintf(cmdp, "%s %s %s %u " FUF " %u " LUF " %d %u",
				    tdp->td_trigger_cmd,
				    dip->di_dname, op, dip->di_dsize,
				    offset, dip->di_block_index,
				    lba, dip->di_error,
				    ( (dip->di_initiated_time)
				      ? (unsigned)(dip->di_last_alarm_time - dip->di_initiated_time)
				      : 0 ) );
		} else {
		    cmdp += sprintf(cmdp, "%s", tdp->td_trigger_cmd);
		}
		/*
		 * Append extra trigger arguments, if requested by the user.
		 */
		if (tdp->td_trigger_args) {
        	    char *trigger_args = FmtString(dip, tdp->td_trigger_args, False);
		    cmdp += sprintf(cmdp, " %s", trigger_args);
        	    Free(dip, trigger_args);
		}
		status = ExecuteCommand(dip, cmd, LogPrefixEnable, True);
		break;
	    }

	    default:
		Eprintf(dip, "Invalid trigger type detected, type = %d\n", tdp->td_trigger);
		return(FAILURE);
		/*NOTREACHED*/
		break;
	}
    } /* end for loop */

    /* Allow user specified trigger status, to control noprog action. */
    if (dip->di_trigger_action) status = dip->di_trigger_action;

    if (status || dip->di_debug_flag) {
	Printf(dip, "Trigger exited with status %d...\n", status);
    }
    return(status);
}

void
DisplayScriptInformation(dinfo_t *dip)
{
    if (dip->script_level) {
	int level = (dip->script_level - 1);
	Fprintf(dip, "Script '%s', line number %d\n",
		dip->script_name[level], dip->script_lineno[level]);
    }
    return;
}

void
CloseScriptFile(dinfo_t *dip)
{
    int level = --dip->script_level;
    (void)fclose(dip->sfp[level]);
    dip->sfp[level] = (FILE *)0;
    Free(dip, dip->script_name[level]);
    dip->script_name[level] = NULL;
    return;
}

void
CloseScriptFiles(dinfo_t *dip)
{
    while (dip->script_level) {
	CloseScriptFile(dip);
    }
    return;
}

int
OpenScriptFile(dinfo_t *dip, char *file)
{
    char filename_buffer[PATH_BUFFER_SIZE];
    struct stat statbuf;
    struct stat *sb = &statbuf;
    char *fnp = filename_buffer;
    char *scriptfile = file;
    char *mode = "r";
    int level = dip->script_level;
    FILE **fp = &dip->sfp[level];
    int status;

    if ( strlen(file) == 0) {
	Fprintf(dip, "Please specify a script file name!\n");
	return(FAILURE);
    }
    if ( (level + 1) > ScriptLevels) {
	Fprintf(dip, "The maximum script level is %d!\n", ScriptLevels);
	return(FAILURE);
    }
    /*
     * Logic:
     *   o If default extension was specified, then attempt to locate
     *     the specified script file.
     *   o If default extension was not specified, then attempt to
     *     locate the file with default extension first, and if that
     *     fails, attempt to locate the file without default extension.
     */
    if ( strstr (scriptfile, ScriptExtension) ) {
	(void) strcpy (fnp, scriptfile);
	status = stat (fnp, sb);
    } else {
	(void) sprintf (fnp, "%s%s", scriptfile, ScriptExtension);
	if ( (status = stat (fnp, sb)) == FAILURE) {
	    (void) strcpy (fnp, scriptfile);
	    status = stat (fnp, sb);
	}
    }

    if (status == FAILURE) {
	Perror (dip, "Unable to access script file '%s'", fnp);
	return(status);
    }

    if ( (*fp = fopen (fnp, mode)) == (FILE *) 0) {
	Perror (dip, "Unable to open script file '%s', mode '%s'", file, mode);
	return(FAILURE);
    }
    dip->script_name[level] = strdup(fnp);
    dip->script_lineno[level] = 0;
    dip->script_level++;
    return(SUCCESS);
}

/*
 * CloseFile() - Close an Input/Output File.
 *
 * Inputs:
 * 	fp = Pointer to file pointer.
 *
 * Return Value:
 *	Returns 0 / -1 = SUCCESS / FAILURE
 */
void
CloseFile(dinfo_t *dip, FILE **fp)
{
    if (*fp != (FILE *) 0) {
	(void)fclose(*fp);
	*fp = (FILE *)0;
    }
}

/*
 * OpenInputFile() - Open an Input File.
 *
 * Inputs:
 * 	fp = Pointer to file pointer.
 *	file = The input file name.
 * 	mode = The open mode ("r" read, "rb" read binary).
 * 	errors = Boolean flag to control reporting errors.
 *
 * Return Value:
 *	Returns 0 / -1 = SUCCESS / FAILURE
 */
int
OpenInputFile(dinfo_t *dip, FILE **fp, char *file, char *mode, hbool_t errors)
{
    if (*fp != (FILE *) 0) {
	CloseFile(dip, fp);
    }
    if ( (*fp = fopen (file, mode)) == (FILE *) 0) {
	if (errors == True) {
	    Perror(dip, "Unable to open input file '%s', mode '%s'", file, mode);
	}
	return(FAILURE);
    }
    return(SUCCESS);
}

/*
 * OpenOutputFile() - Open an Output File.
 *
 * Inputs:
 * 	fp = Pointer to file pointer.
 *	file = Pointer to output file name.
 *	mode = The open mode ("a" append, "w" write, etc).
 * 	errors = Boolean flag to control reporting errors.
 *
 * Return Value:
 *	Returns 0 / -1 = SUCCESS / FAILURE
 */
int
OpenOutputFile(dinfo_t *dip, FILE **fp, char *file, char *mode, hbool_t errors)
{
    if (*fp != (FILE *) 0) {
	CloseFile(dip, fp);
    }
    if ( (*fp = fopen(file, mode)) == (FILE *) 0) {
	if (errors == True) {
	    Perror(dip, "Unable to open output file '%s', mode '%s'", file, mode);
	}
	return(FAILURE);
    }
    return (SUCCESS);
}
