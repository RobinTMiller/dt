/****************************************************************************
 *      								    *
 *      		  COPYRIGHT (c) 1988 - 2020     		    *
 *      		   This Software Provided       		    *
 *      			     By 				    *
 *      		  Robin's Nest Software Inc.    		    *
 *      								    *
 * Permission to use, copy, modify, distribute and sell this software and   *
 * its documentation for any purpose and without fee is hereby granted,     *
 * provided that the above copyright notice appear in all copies and that   *
 * both that copyright notice and this permission notice appear in the      *
 * supporting documentation, and that the name of the author not be used    *
 * in advertising or publicity pertaining to distribution of the software   *
 * without specific, written prior permission.  			    *
 *      								    *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,        *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN      *
 * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
 * THIS SOFTWARE.       						    *
 *      								    *
 ****************************************************************************/
/*
 * Module:      dthist.c
 * Author:      Robin T. Miller
 * Date:	July 15th, 2013
 *
 * Description:
 *      dt's history functions.
 * 
 * Modification History:
 * 
 * May 5th, 2020 by Robin T. Miller
 *      Use high resolution timer for more accurate history timing. This is
 * implemented on Windows, but Unix systems still use gettimeofday() API.
 * 
 * February 2nd, 2016 by Robin T. Miller
 * 	Display device name when dumping history, needed for multiple devices.
 */
#include "dt.h"

void
FreeHistoryData(dinfo_t *dip)
{
    if (dip->di_history_size && dip->di_history) {
	history_t *hp;
	int buf, entries = 0;
	for (hp = dip->di_history;
	     (dip->di_history_data_size && (entries < dip->di_history_size));
	     entries++, hp++) {
	    for (buf = 0; (buf < dip->di_history_bufs); buf++) {
		if (hp->hist_request_data[buf]) {
		    Free(dip, hp->hist_request_data[buf]);
		    hp->hist_request_data[buf] = NULL;
		}
	    }
	    Free(dip, hp->hist_request_data);
	    hp->hist_request_data = NULL;
	}
	Free(dip, dip->di_history);
	dip->di_history = NULL;
    }
    return;
}

void
SetupHistoryData(dinfo_t *dip)
{
    /*
     * Allocate history buffers (if requested).
     */
    if (dip->di_history_size) {
	history_t *hp;
	int buf, entries = 0;
	dip->di_history = Malloc(dip, (dip->di_history_size * sizeof(history_t)) );
	if (dip->di_history == NULL) return;
	for (hp = dip->di_history;
	     (dip->di_history_data_size && (entries < dip->di_history_size));
	     entries++, hp++) {
	    /* Allocate an array of pointers for the history request data buffers. */
	    hp->hist_request_data = Malloc(dip, (sizeof(hp->hist_request_data) * dip->di_history_bufs));
	    if (hp->hist_request_data == NULL) break;
	    for (buf = 0; (buf < dip->di_history_bufs); buf++) {
		hp->hist_request_data[buf] = Malloc(dip, dip->di_history_data_size);
		if (hp->hist_request_data[buf] == NULL) break;
	    }
	}
    }
    return;
}

/*
 * dump_history_data() - Dump (saved) History Data.
 *
 * Description:
 *	This function displays previously saved history data.
 *
 * Inputs:
 *	dip = The device information pointer.
 *
 * Return Value:
 *	void
 */
void
dump_history_data(struct dinfo *dip)
{
    history_t *hp = dip->di_history;
    int entries = dip->di_history_entries;
    int i, field_width = 16;
    unsigned char *hbp;
    uint32_t bsize;
    int lock_status;

    if (entries == 0) {
	Printf(dip, "No history entries to report!\n");
	return;
    }
    
    dip->di_history_dumping = True;
    lock_status = AcquirePrintLock(dip);

    if (dip->di_history_bsize) {
	bsize = dip->di_history_bsize;
    } else {
	bsize = (dip->di_random_access) ? dip->di_dsize : dip->di_lbdata_size;
    }
    hp += dip->di_history_index;

    Printf(dip, "\n");
    if (dip->di_history_bufs == 1) {
	Printf(dip, "Dumping History Data for %s (%d entries):\n",
	       dip->di_dname, entries);
    } else {
	Printf(dip, "Dumping History Data for %s (%d entries, blocking %u bytes):\n",
	       dip->di_dname, entries, bsize);
    }
    Printf(dip, "\n");
    /*
     * Report history in reverse order (lastest request first).
     */
    while (entries--) {
	ssize_t data_size;
	size_t request_size;
	Offset_t offset;
	history_t *php = NULL;
	large_t iolba = NO_LBA;

	hp--;
	if (hp < dip->di_history) {
	    hp += dip->di_history_entries; /* set to end */
	}
	if (dip->di_history_timing && entries) {
	    php = (hp - 1); /* Previous history entry. */
	    if (php < dip->di_history) {
		php += dip->di_history_entries;
	    }
	}
	/*
	 * On error or end of file, display data upto requested size.
	 * This is to handle partial transfers prior to I/O errors.
	 */
	offset = hp->hist_file_offset;
	if (hp->hist_transfer_size <= (ssize_t) 0) {
	    request_size = hp->hist_request_size;
	} else {
	    request_size = hp->hist_transfer_size;
	}
	data_size = MIN((size_t)dip->di_history_data_size, request_size);
	if (dip->di_random_access) {
	    iolba = (offset / dip->di_dsize);
	} else if (dip->di_lbdata_flag || dip->di_iot_pattern) {
	    iolba = make_lbdata(dip, offset);
	}

	/*
	 * For errors or end of file, report the requested size.
	 */
	if (hp->hist_transfer_size <= 0) {
	    Printf(dip, "Record #%lu - Transfer completed with %ld, reporting attempted request size\n",
		   hp->hist_record_number, hp->hist_transfer_size);
	}
	if (dip->di_history_timing) {
	    /*
	     * Report the time stamp and time between requests.
	     */
	    Printf(dip, "%d.%06d ", hp->hist_timer_info.tv_sec,
				    hp->hist_timer_info.tv_usec);
	    if (php) {
		int secs = hp->hist_timer_info.tv_sec;
		int usecs = hp->hist_timer_info.tv_usec;
		if (usecs < php->hist_timer_info.tv_usec) {
		    secs--;
		    usecs += uSECS_PER_SEC;
		}
		Print(dip, "(%d.%06d) ", (secs - php->hist_timer_info.tv_sec),
					 (usecs - php->hist_timer_info.tv_usec));
	    }
	} else {
	    Printf(dip, "");	/* Prefix before record number below. */
	}
	/* Note: Without a buffer, a log prefix is not displayed. */
	if (hp->hist_transfer_size <= 0) {
	    /* For errors or end of file, report the requested size. */
	    report_record(dip, hp->hist_file_number, hp->hist_record_number,
			  iolba, offset, hp->hist_test_mode,
			  NULL, (size_t)hp->hist_request_size);
	} else {
	    report_record(dip, hp->hist_file_number, hp->hist_record_number,
			  iolba, offset, hp->hist_test_mode,
			  NULL, (size_t)hp->hist_transfer_size);
	}

	/*
	 * Display the requested block data bytes (if any).
	 */
	if (data_size > 0) {
	    size_t data_index = 0;
	    int buf;
	    /* Loop through each history data buffers. */
	    for (buf = 0; (buf < dip->di_history_bufs) && (data_index < request_size); buf++) {
		if (dip->di_history_bufs > 1) {
		    Printf(dip, "  Buffer %d: (lba %u, offset "FUF")\n", buf, iolba, offset);
		    Printf(dip, "    Offset\n");
		} else {
		    Printf(dip, "Offset\n");
		}
		hbp = hp->hist_request_data[buf];
		/* Display data for each block buffer. */
		for (i = 0; (i < data_size); ) {
		    if ((i % field_width) == (size_t) 0) {
			if (i) Print(dip, "\n");
			if (dip->di_history_bufs > 1) {
			    Printf(dip, "    %06d  ", i);
			} else {
			    Printf(dip, "%06d  ", i);
			}
		    }
		    if (dip->di_iot_pattern) {
			Print(dip, "%08x ", get_lbn(hbp));
			i += sizeof(u_int32);
			hbp += sizeof(u_int32);
		    } else {
			Print(dip, "%02x ", *hbp);
			i++; hbp++;
		    }
		}
		data_index += bsize;
		offset += bsize;
		iolba = makeLBA(dip, offset);
		if (i) Print(dip, "\n");
	    }
	}
	if (data_size) Printf(dip, "\n");
    }
    if (lock_status == SUCCESS) {
        lock_status = ReleasePrintLock(dip);
    }
    dip->di_history_dumped = True;
    dip->di_history_dumping = False;
    return;
}

/*
 * save_history_data() - Save History Data.
 *
 * Description:
 *	This function saves the history data for the last request.
 * The data is saved in a circular buffer who's size is determined
 * by the tester via the history=value option.  The amount of data
 * saved, can also be specified via the hdsize=value option, otherwise
 * a default is used (16 at the time of this writing).
 *
 * Inputs:
 *	dip = The device pointer.
 *	test_mode = The test mode. (READ_MODE or WRITE_MODE)
 *	offset = The file offset. (prior to transfer)
 *	buffer = The data buffer.
 *	rsize = The request size. (the request size = full count)
 *	tsize = The transfer size. (the transfer size = returned)
 *
 * Note: Beware, as tsize may be -1 on errors or 0 for end of file,
 *	 but we still wish to record that information in the history.
 *
 * Return Value:
 *	void
 */
void
save_history_data(
    struct dinfo	*dip,
    u_long		file_number,
    u_long		record_number,
    test_mode_t		test_mode,
    Offset_t		offset,
    void		*buffer,
    size_t		rsize,
    ssize_t		tsize)
{
    history_t *hp = dip->di_history;
    unsigned char *bp;
    unsigned char *hbp;
    ssize_t data_size;
    size_t data_index;
    uint32_t bsize;
    int buf, i;
    
    if (dip->di_history_bsize) {
	bsize = dip->di_history_bsize;
    } else {
	bsize = (dip->di_random_access) ? dip->di_dsize : dip->di_lbdata_size;
    }
    hp += dip->di_history_index;
    hp->hist_test_mode = test_mode;
    hp->hist_file_number = file_number;
    hp->hist_record_number = record_number;
    hp->hist_file_offset = offset;
    hp->hist_request_size = rsize;
    hp->hist_transfer_size = tsize;
    if (dip->di_history_timing) { /* expensive syscall! */
	(void)highresolutiontime(&hp->hist_timer_info, NULL);
    }
    /*DEBUG*/
    //if (dip->di_history_index%2 == 0) hp->hist_transfer_size = FAILURE;
    /*DEBUG*/
     /*
      * On error or end of file, save data upto the requested data size.
      * The reason for this is, some OS's transfer partial data prior to
      * reporting the I/O error.  This data may be on the analyzer.
      */
    if (hp->hist_transfer_size <= (ssize_t) 0) {
	data_size = (int)MIN((size_t)dip->di_history_data_size, hp->hist_request_size);
    } else {
	data_size = MIN((ssize_t)dip->di_history_data_size, hp->hist_transfer_size);
    }
    data_index = 0;
    /* Loop through I/O request blocks saving data (as instructed). */
    for (buf = 0; hp->hist_request_data && (buf < dip->di_history_bufs) && (data_index < rsize); buf++) {
	hbp = hp->hist_request_data[buf];
	bp = ((unsigned char *)buffer + (buf * bsize));
	/* Save data bytes for this block. */
	for (i = 0; (i < data_size); i++) {
	    hbp[i] = bp[i];
	}
	data_index += bsize;
    }
    if (dip->di_history_entries < dip->di_history_size) {
	dip->di_history_entries++; /* Count up to max entries. */
    }
    if (++dip->di_history_index == dip->di_history_size) {
	dip->di_history_index = 0;
    }
    return;
}
