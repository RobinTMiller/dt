/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2021			    *
 *			   This Software Provided			    *
 *				     By					    *
 *			  Robin's Nest Software Inc.			    *
 *									    *
 * Permission to use, copy, modify, distribute and sell this software and   *
 * its documentation for any purpose and without fee is hereby granted,	    *
 * provided that the above copyright notice appear in all copies and that   *
 * both that copyright notice and this permissikn notice appear in the	    *
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
 * Modification History:
 * 
 * July 30th, 2021 by Robin T. Miller
 *      Adding initial support for NVMe disks.
 * 
 * May 25th, 2021 by Robin T. Miller
 *      Resetting RETRYDC_LIMIT back to 1, since the delay and extra re-read,
 * is generally undesirable when sending triggers quickly is more important!
 * Note: The extra re-read was added to help determine persistent vs. transient
 * type data corruptions, prevalent in caching products.
 * 
 * April 9th, 2020 by Robin T. Miller (Happy birthday to my son Tom!)
 *      Increase the data corruption retries from 1 to 2, so we can gather
 * yet another data point. If our first (immediate) re-read is still bad,
 * delay (5 second deault), and retry again to see if it's a permanent DC.
 *
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */

/* Note: This is *required* for AIX, but defining for all OS's! */
#define _THREAD_SAFE 1

/* Vendor Control Flags: */
#define HGST	1
//#define Nimble	1
//#define TPD     1

#if defined(AIX_WORKAROUND)
# include "aix_workaround.h"
#endif /* defined(AIX_WORKAROUND) */

/* I've seen inline, __inline, and __inline__ used! */
#define INLINE		static __inline
#define INLINE_FUNCS	1

#if defined(WIN32)
# include <winsock2.h>
# include <ws2tcpip.h>
# define _CRT_RAND_S
#endif /* defined(WIN32) */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(WIN32)
#  include<io.h>
#  include<direct.h>
#  include<process.h>
#else /* !defined(WIN32) */
#  if defined(AIO)
#    include <aio.h>
#  endif /* defined(AIO) */
#  include <pthread.h>
#  include <unistd.h>
#  include <termios.h>
#  include <sys/param.h>
#  include <sys/time.h>
#  include <sys/times.h>
#  if !defined(NOSYSLOG)
#    define SYSLOG 1
#    include <syslog.h>
#  endif /* defined(NOSYSLOG) */
#endif /* defined(WIN32) */

/* Note: dtwin.h or dtunix.h included in common.h! */
#include "common.h"
#include "dtmtrand64.h"

#include "dtbtag.h"

/*
 * Definition to control timestamps (may affect performance).
 */
#define TIMESTAMP       1

/* TODO: Cleanup this junk! Really still needed? */
#if !defined(HZ)
/* now included above! */
//#  if defined(sun) || defined(SCO) || defined(HP_UX)
//#    include <sys/param.h>
//#  endif /* defined(sun) */
#  if defined(CLK_TCK) && !defined(HZ)
#    define HZ		CLK_TCK
#  else
#    if !defined(HZ)
#      define HZ		256
#    endif /* !defined(HZ) */
#    if !defined(CLK_TCK)
#      define CLK_TCK	HZ
#    endif /* !defined(CLK_TCK) */
#  endif /* defined(CLK_TCK) */
#endif /* !defined(HZ) */

#define STARTUP_SCRIPT		".datatestrc"	/* The startup script name. */
#define STARTUP_ENVNAME		"DT_SCRIPT"	/* The startup script var.  */
#define MAXFILES_ENVNAME	"DT_MAXFILES"	/* Set the max file limit.  */
#define DEFAULT_SCRIPT_VERIFY	False		/* Script verify echo flag. */

/* Separator between file name and file postfix. */
#define DEFAULT_FILE_SEP	"-"
/* Note: Used for a directory postfix and file postfix. */
#define DEFAULT_FILE_POSTFIX	"j%jobt%thread"
#define DEFAULT_JOBLOG_POSTFIX	"Job%job"
#define DIR_PREFIX		"d"
#define DEFAULT_IOTUNE_FILE	TEMP_DIR_NAME"dtiotune.txt"
/* When directory given without a file name, use this default. */
#define DEFAULT_DATA_FILE_NAME	"dt-%user-%uuid.data"

/*
 * Default Log Prefixes:
 * 
 * Robin's Favorite: logprefix="%seq %et %prog %device (j:%job t:%thread): "
 */
#define DEFAULT_LOG_PREFIX		"%prog (j:%job t:%thread): "
#define DEFAULT_DEBUG_LOG_PREFIX	"%et %prog (j:%job t:%thread): "

#define DEFAULT_GTOD_LOG_PREFIX		"%tod (%etod) %prog (j:%job t:%thread): " 

/* Note: These must be defined per company to their own web pages! */
#  //define DATA_CORRUPTION_URL	"";
#  //define DATA_CORRUPTION_URL1"";
#  //define NO_PROGRESS_URL		"";

#if !defined(MAXHOSTNAMELEN)
#    define MAXHOSTNAMELEN	256
#endif
#if !defined(MAXBADBLOCKS)
#    define MAXBADBLOCKS	25
#endif

#define LogPrefixEnable 	True
#define LogPrefixDisable	False

#define MismatchedData		True
#define NotMismatchedData	False

#define DEFAULT_COMPARE_FLAG	True
#define DEFAULT_XCOMPARE_FLAG	False
#define DEFAULT_COREDUMP_FLAG	False
#define DEFAULT_FILEPERTHREAD   True
#define DEFAULT_LBDATA_FLAG	False
#if defined(Nimble)
#  define DEFAULT_POISON_FLAG	True
#  define DEFAULT_PREFILL_FLAG	True
#else /* defined(Nimble */
#  define DEFAULT_POISON_FLAG	False
#  define DEFAULT_PREFILL_FLAG	UNINITIALIZED
#endif /* defined(Nimble) */
#define DEFAULT_MOUNT_LOOKUP	True
#define DEFAULT_NATE_FLAG	False
#define DEFAULT_TIMESTAMP_FLAG	False
#define DEFAULT_UNIQUE_PATTERN	True
#define DEFAULT_USER_PATTERN	False
#define DEFAULT_HEALTH_CHECK	True
#define DEFAULT_HEALTH_ERRORS	False
#define DEFAULT_HEALTH_LENGTH	4096
#define DEFAULT_HEALTH_RETRIES	15
#define DEFAULT_HEALTH_TIMEOUT	(15 * MSECS)
#define DEFAULT_JOB_STATS_FLAG	False
#define DEFAULT_PASS_STATS_FLAG	True
#define DEFAULT_TOTAL_STATS_FLAG True
#define DEFAULT_SCSI_FLAG	True
#define DEFAULT_SCSI_INFO_FLAG	True
#define DEFAULT_SCSI_IO_FLAG	False
#define DEFAULT_SCSI_ERRORS	False
#define DEFAULT_SCSI_SENSE	False
//#define DEFAULT_SCSI_RECOVERY	True
#define DEFAULT_VERBOSE_FLAG	True
#define DEFAULT_VERIFY_FLAG	True

#define DEFAULT_DUMP_LIMIT	BLOCK_SIZE	/* Default dump limit.		*/
#define DEFAULT_ERROR_LIMIT	1		/* Default error limit.		*/
#define DEFAULT_FILE_LIMIT	0		/* Default file limit.		*/
#define DEFAULT_PASS_LIMIT	1		/* Default pass limit.		*/
/*
 * This controls the default frequency of checking the IO tune file.
 * This also controls the IO tuning thread run interval (see below).
 *
 * Note: Currently the IO tune file is serviced from the monitoring thread,
 * and this threads' interval is affected by noprog or keepalive intervals.
 */
#define DEFAULT_IOTUNE_FREQ	3		/* Default tuning frequency.	*/

#define DEFAULT_CANCEL_DELAY	3		/* Time to delay before cancel.	*/
#define DEFAULT_KILL_DELAY	3		/* Time to delay before kill.	*/
#define DEFAULT_TERM_DELAY	0		/* Thread terminate delay.	*/
#define DEFAULT_OPEN_DELAY	0		/* Delay before opening file.	*/
#define DEFAULT_CLOSE_DELAY	0		/* Delay before closing file.	*/
#define DEFAULT_DELETE_DELAY	0		/* Delay after deleting files.	*/
#define DEFAULT_END_DELAY	0		/* Delay between multiple passes*/
#define DEFAULT_READ_DELAY	0		/* Delay before reading record.	*/
#define DEFAULT_START_DELAY	0		/* Delay before starting test.	*/
#define DEFAULT_VERIFY_DELAY	0		/* Delay before verifying data. */
#define DEFAULT_WRITE_DELAY	0		/* Delay before writing record.	*/

#define DEFAULT_FSFREE_DELAY	3		/* File system free delay.	*/
#define DEFAULT_FSFREE_RETRIES	10		/* File system free retries.	*/

#define DEFAULT_IOTUNE_DELAY DEFAULT_IOTUNE_FREQ/* The I/O tuning interval.	*/
#define DEFAULT_IOTUNE_FLAG	True		/* The I/O tuning control flag.	*/
#define DEFAULT_IOTUNE_ADJUST	1000		/* The I/O tuning adjustment.	*/
#define DEFAULT_IOTUNE_DIVISOR	3		/* The I/O tuning scale divisor.*/
#define DEFAULT_IOTUNE_MIN_CPU	40		/* The mimimum CPU busy value.	*/
#define DEFAULT_IOTUNE_MAX_CPU	60		/* The maximum CPU busy value.	*/
#define DEFAULT_MAX_OPEN_FILES  32768           /* Linux default is only 1024! */

#define JOB_WAIT_DELAY		1		/* The job wait delay (in secs)	*/
#define THREAD_MAX_TERM_TIME	180		/* Thread termination wait time.*/
#define THREAD_TERM_WAIT_FREQ	30		/* Term wait message frequecy.  */

#define ScriptLevels		5
#define ScriptExtension          ".dt"

#define AIO_BUFS	8			/* Default number AIO buffers.	*/
#define IOT_SEED	0x01010101		/* Default seed for IOT pattern	*/
#define RETRY_DELAY	5			/* Default retry delay (secs).	*/
#define RETRY_ENTRIES	25			/* The number of retry errors.	*/
/* Note: (5 * 60) = 300 seconds or 5 minutes! */
#define RETRY_LIMIT	60			/* Default retry limit.		*/
#define RETRYDC_DELAY	5			/* Default retry DC delay (secs).*/
#define RETRYDC_LIMIT	1			/* Retry data corruption limit.	*/
#define SAVE_CORRUPTED	True			/* Default save corrupted data.	*/

typedef enum corruption_type {
    CTYPE_EXPECTED = 0,
    CTYPE_CORRUPTED = 1,
    CTYPE_REREAD = 2
} corruption_type_t;

/*
 * Default Random Block Sizes:
 */
#define MIN_RANDOM_SIZE		512		/* The minimum random size. */
#define MAX_RANDOM_SIZE		MBYTE_SIZE      /* The maximum random size. */

/*
 * Default Random File Limits:
 */
#define MIN_DATA_LIMIT     (10 * MBYTE_SIZE)    /* The minimum file limit. */
#define MAX_DATA_LIMIT     (2 * GBYTE_SIZE)     /* The maximum file limit. */

/*
 * IOPS Measurement Types:
 */
typedef enum {
    IOPS_MEASURE_EXACT = 0,	/* Done in I/O loops via gettimeofday(). */
    IOPS_MEASURE_IOMON = 1	/* Done in I/O monitor thread via delays.*/
} iops_measure_type_t;

/*
 * File Lock Definitions:
 */
#define DEFAULT_LOCK_MODE	lock_mixed
#define DEFAULT_LOCK_MODE_NAME	"mixed"
#define DEFAULT_LOCK_TEST	False
#define DEFAULT_UNLOCK_CHANCE	100

/*
 * Definitions Shared by Block Display Functions:
 */
#define BITS_PER_BYTE		8
#define BYTES_PER_LINE		16
#define BYTE_EXPECTED_WIDTH	55
#define WORD_EXPECTED_WIDTH	43

typedef enum {
    LOCK_RANGE_FULL = 0,
    LOCK_RANGE_PARTIAL = 1
} lock_range_t;

typedef struct {
    int lower, upper;		/* The lower and upper percentages. */
} lock_mode_t;

/* Lock Flags: */
typedef enum {
    LOCK_TYPE_READ = 0,
    LOCK_TYPE_WRITE = 1,
    LOCK_TYPE_UNLOCK = 2,
    NUM_LOCK_TYPES = 3
} lock_type_t;

/*
 * Note: Now that long file paths are created, this string buffer must
 *	 be large enough for message text *and* the long file names!
 */
#undef STRING_BUFFER_SIZE
#define STRING_BUFFER_SIZE (PATH_BUFFER_SIZE+256)/* String buffer size.	*/

#define isDiskDevice(dip)	( dip->di_dtype && \
				  ((dip->di_dtype->dt_dtype == DT_DISK)  || \
				   (dip->di_dtype->dt_dtype == DT_BLOCK) || \
				   (dip->di_dtype->dt_dtype == DT_CHARACTER)) )

#define isFileSystemFile(dip)	( dip->di_dtype && \
				  ((dip->di_dtype->dt_dtype == DT_REGULAR) || \
				   (dip->di_dtype->dt_dtype == DT_MMAP)) )
				   
#define isRandomIO(dip)		( (dip->di_io_type == RANDOM_IO) || \
				  (dip->di_variable_flag == True) )

/* TODO: Too many reasons to enable random generator, need to simplify! */
#define UseRandomSeed(dip)	\
  ( isRandomIO(dip) || (dip->di_lock_files == True) || \
    (dip->di_read_percentage || dip->di_random_percentage) || \
    (dip->di_random_rpercentage || dip->di_random_wpercentage) || \
    dip->di_vary_iodir || dip->di_vary_iotype || (dip->di_unmap_type == UNMAP_TYPE_RANDOM) || \
    (dip->di_iobehavior == DTAPP_IO) || (dip->di_iobehavior == THUMPER_IO) || (dip->di_variable_limit == True) )

#if defined(AIO)
# define getFileOffset(dip) \
	(dip->di_aio_flag && dip->di_current_acb) \
	    ? dip->di_current_acb->aio_offset : dip->di_offset
#else /* !defined(AIO) */
# define getFileOffset(dip) dip->di_offset
#endif /* defined(AIO) */

#define makeOffset(lba, bsize) (Offset_t)(lba * bsize)

/* Make LBA based on the device size (block/sector size). */
#define makeLBA(dip, offset) \
	(large_t)( (dip->di_dsize == 0) || (offset == (Offset_t)0) \
		   ? 0 : WhichBlock(offset, dip->di_dsize))

#define HANDLE_THREAD_EXIT(dip) handle_thread_exit(dip)
#define COMMAND_INTERRUPTED	( (InteractiveFlag == False) && (CmdInterruptedFlag == True) )
#define PROGRAM_TERMINATING	(terminating_flag)   
#define THREAD_FINISHED(dip) \
    ( PROGRAM_TERMINATING || \
      (dip->di_thread_state == TS_CANCELLED) || (dip->di_thread_state == TS_FINISHED) || \
      (dip->di_thread_state == TS_JOINED) || (dip->di_thread_state == TS_TERMINATING) )
#define THREAD_TERMINATING(dip) \
    ( (dip->di_thread_state == TS_CANCELLED) || (dip->di_thread_state == TS_TERMINATING) )
#define PAUSE_THREAD(dip) while (dip->di_thread_state == TS_PAUSED) os_sleep(1);

#if defined(WIN32)
#  define MAX_PROCS       MAXIMUM_WAIT_OBJECTS	/* Maximum processes.	*/
#  define MAX_SLICES      MAXIMUM_WAIT_OBJECTS	/* Maximum processes.	*/
#  define TERM_WAIT_TIMEOUT	10		/* Wait timeout (secs).	*/
#  define TERM_WAIT_RETRIES	6		/* Time to retry wait.	*/
#  define TERM_WAIT_ARETRIES	1		/* Abnormal wait retry.	*/
#else /* !defined(WIN32) */
#  define MAX_PROCS	256			/* Maximum processes.	*/
#  define MAX_SLICES	256			/* Maximum slices.	*/
#endif /* defined(WIN32) */
#define USE_PATTERN_BUFFER			/* Use pattern buffer.	*/

#define RANDOM_DELAY_VALUE 0xFFFFFFFF		/* Randomized delay.	*/

#define DEFAULT_PATTERN	 0x39c39c39U	/* Default data pattern.	*/
#define ASCII_PATTERN	 0x41424344U	/* Default ASCII data pattern.	*/

#define CORRUPTION_PATTERN   0xfeedface /* Default corruption pattern.  */
#define CORRUPT_READ_RECORDS  13        /* Corrupt at read records.     */
#define CORRUPT_WRITE_RECORDS 0         /* Corrupt at write records.    */

#define DEF_LOG_BUFSIZE	(PATH_BUFFER_SIZE * 2) /* File name + stats!	*/

#if defined(BUFSIZ) && (BUFSIZ > DEF_LOG_BUFSIZE)
#  define LOG_BUFSIZE	BUFSIZ		/* The log file buffer size.	*/
#else /* !defined(BUFSIZ) */
#  define LOG_BUFSIZE	DEF_LOG_BUFSIZE	/* The log file buffer size.	*/
#endif /* defined(BUFSIZ) */

#if defined(SCSI)
#  include "libscsi.h"
#  include "inquiry.h"

#  define ScsiReadTypeDefault	scsi_read16_cdb
#  define ScsiWriteTypeDefault	scsi_write16_cdb

#endif /* defined(SCSI) */

/*
 * The buffer pad bytes are allocated at the end of all data buffers,
 * initialized with the inverted data pattern, and then checked after
 * each read operation to ensure extra bytes did not get transferred
 * at the end of buffer.  This test is necessary, since quite often
 * we've seen data corruption (extra bytes) due to improper flushing
 * of DMA FIFO's, or other coding errors in our SCSI/CAM sub-system.
 */
#define PADBUFR_SIZE	sizeof(u_int32)	/* The data buffer pad size.	*/
					/* MUST match pattern length!!!	*/

/*
 * The buffer rotate size are used to force unaligned buffer access
 * by rotating the starting buffer address through the sizeof(ptr).
 * This feature has been very useful in forcing drivers through special
 * code to handle unaligned addresses & buffers crossing page boundaries.
 */
#define ROTATE_SIZE	sizeof(char *)	/* Forces through all ptr bytes	*/

/*
 * 'dt' specific exit status codes:
 */
#define END_OF_FILE	254			/* End of file code.	*/
#define FATAL_ERROR	-1			/* Fatal error code.	*/

#define get_lbn(bp)	( ((u_int32)bp[3] << 24) | ((u_int32)bp[2] << 16) | \
			  ((u_int32)bp[1] << 8) | (u_int32)bp[0])

typedef unsigned int	mytime_t;
typedef volatile time_t	vtime_t;
typedef uint32_t	lbdata_t;

typedef enum bfmt {DEC_FMT, HEX_FMT} bfmt_t;
typedef enum dfmt {NONE_FMT, BYTE_FMT, SHORT_FMT, WORD_FMT, QUAD_FMT} dfmt_t;

typedef enum opt {OFF, ON, OPT_NONE} opt_t;
typedef enum flow {FLOW_NONE, CTS_RTS, XON_XOFF} flow_t;
typedef enum stats {COPY_STATS, READ_STATS, RAW_STATS, WRITE_STATS, TOTAL_STATS, MIRROR_STATS, VERIFY_STATS, JOB_STATS} stats_t;
typedef enum dispose {DELETE_FILE, KEEP_FILE, KEEP_ON_ERROR} dispose_t;
typedef enum file_type {INPUT_FILE, OUTPUT_FILE} file_type_t;
typedef enum test_mode {READ_MODE, WRITE_MODE} test_mode_t;
typedef enum onerrors {ONERR_ABORT, ONERR_CONTINUE, ONERR_PAUSE} onerrors_t;
typedef enum iobehavior { DT_IO, DTAPP_IO, HAMMER_IO, SIO_IO, THUMPER_IO } iobehavior_t;
typedef enum iodir {FORWARD, REVERSE, NUM_IODIRS = 2} iodir_t;
typedef enum iomode {COPY_MODE, MIRROR_MODE, TEST_MODE, VERIFY_MODE} iomode_t;
typedef enum iotype {SEQUENTIAL_IO, RANDOM_IO, NUM_IOTYPES = 2} iotype_t;
typedef enum initial_state {IS_RUNNING, IS_PAUSED} istate_t;
typedef enum job_state {JS_STOPPED, JS_RUNNING, JS_FINISHED, JS_PAUSED, JS_TERMINATING, JS_CANCELLED} jstate_t;
typedef enum thread_state {TS_STOPPED, TS_STARTING, TS_RUNNING, TS_FINISHED, TS_JOINED, TS_PAUSED, TS_TERMINATING, TS_CANCELLED} tstate_t;
typedef volatile jstate_t vjstate_t;
typedef volatile tstate_t vtstate_t;
/* 
 * Buffering Modes: 
 * buffered = normal FS buffering, unbuffered = Direct I/O
 * readcache = disabled write caching, writecache = disable read caching 
 */
typedef enum bufmodes {
    NONE_SPECIFIED = 0,		/* No buffering mode specified.		*/
    BUFFERED_IO = 1,		/* Normal FS buffering (buffer cache).	*/
    UNBUFFERED_IO = 2,		/* No buffering (full direct I/O).	*/
    CACHE_READS = 3,		/* Cache reads (write cache disabled).	*/
    CACHE_WRITES = 4,		/* Cache writes (reads cache disabled).	*/
    NUM_BUFMODES = 4
} bufmodes_t;

/* File System Map Types: */
typedef enum fsmap_type {
    FSMAP_TYPE_NONE = 0,
    FSMAP_TYPE_LBA_RANGE = 1,
    FSMAP_TYPE_MAP_EXTENTS
} fsmap_type_t;

/*
 * History Information:
 */
typedef struct history {
    test_mode_t	hist_test_mode;		/* The I/O mode.       		*/
    u_int	hist_file_number;	/* The file number.		*/
    u_long	hist_record_number;	/* The record number.		*/
    Offset_t	hist_file_offset;	/* The file offset.		*/
    size_t	hist_request_size;	/* Size of the request.		*/
    ssize_t	hist_transfer_size;	/* Size of the transfer.	*/
    uint8_t	**hist_request_data;	/* First 'N' bytes of data.	*/
    struct timeval hist_timer_info;	/* Timer info in sec/usecs.	*/
} history_t;

#define DEFAULT_HISTORY_BUFFERS		1
#define DEFAULT_HISTORY_DATA_SIZE	32

/*
 * The operation type is used with no-progress option to report operation.
 */
typedef enum optype {
    NONE_OP, OPEN_OP, CLOSE_OP, READ_OP, WRITE_OP, IOCTL_OP, FSYNC_OP, MSYNC_OP, AIOWAIT_OP,
    MKDIR_OP, RMDIR_OP, DELETE_OP, TRUNCATE_OP, RENAME_OP, LOCK_OP, UNLOCK_OP, GETATTR_OP,
    SEEK_OP, SPARSE_OP, TRIM_OP, VINFO_OP, VPATH_OP, MMAP_OP, MUNMAP_OP, CANCEL_OP,
    RESUME_OP, SUSPEND_OP, TERMINATE_OP, OTHER_OP, NUM_OPS
} optype_t;
typedef struct optiming {
    optype_t	opt_optype;		/* The operation type.      */
    hbool_t	opt_timing_flag;	/* The timing control flag. */
    char	*opt_name;		/* The operation name.      */
} optiming_t;
/* Note: This table *must* be kept in sync with the above definitions! */
extern optiming_t optiming_table[];

extern char *miscompare_op;

/*
 * Macros to enable or disable tracking no-progress.
 */
#define ENABLE_NOPROG(dip, optype) \
        if (dip->di_noprog_flag && optiming_table[optype].opt_timing_flag) {    \
            dip->di_optype = optype;                                            \
            dip->di_initiated_time = time((time_t *)0);                         \
	    if (dip->di_forced_delay && (dip->di_thread_number % dip->di_forced_delay) ) \
		SleepSecs(dip, (rand() % dip->di_forced_delay) );		\
        }
#define DISABLE_NOPROG(dip) \
        if (dip->di_noprog_flag) {                                              \
            dip->di_optype = NONE_OP;                                           \
            dip->di_initiated_time = dip->di_next_noprog_time = (time_t)0;	\
        } 

typedef enum sleep_resolution {SLEEP_DEFAULT, SLEEP_SECS, SLEEP_MSECS, SLEEP_USECS} sleepres_t;
typedef enum statslevel {STATS_BRIEF, STATS_FULL, STATS_NONE} statslevel_t;
typedef enum stats_value {ST_BYTES, ST_BLOCKS, ST_FILES, ST_RECORDS, ST_OFFSET} stats_value_t;

typedef enum trigger_control {
    TRIGGER_ON_ALL, TRIGGER_ON_ERRORS, TRIGGER_ON_MISCOMPARE, TRIGGER_ON_NOPROGS, TRIGGER_ON_INVALID=-1
} trigger_control_t;

typedef enum trigger_type {
    TRIGGER_NONE, TRIGGER_BR, TRIGGER_BDR, TRIGGER_LR, TRIGGER_SEEK, TRIGGER_CMD,
    TRIGGER_TRIAGE, TRIGGER_CDB,
    TRIGGER_INVALID=-1
} trigger_type_t;

typedef enum trigger_action {
    TRIGACT_CONTINUE = 0, TRIGACT_TERMINATE = 1, TRIGACT_SLEEP = 2, TRIGACT_ABORT = 3
} trigger_action_t;

typedef enum unmap_type {
    UNMAP_TYPE_NONE = -1,
    UNMAP_TYPE_UNMAP = 0,
    UNMAP_TYPE_WRITE_SAME = 1,
    UNMAP_TYPE_ZEROROD = 2,
    UNMAP_TYPE_RANDOM = 3,
    NUM_UNMAP_TYPES = 3
} unmap_type_t;

#define NUM_TRIGGERS	5

typedef struct trigger_data {
    enum trigger_type td_trigger;	/* Trigger for corruptions.   */
    char	*td_trigger_cmd;	/* The users' trigger command.  */
    char	*td_trigger_args;	/* Extra user trigger arguments	*/
} trigger_data_t;

/* Note: The log levels are those used by syslog/event logger. */
typedef enum logLevel {
	logLevelCrit  = LOG_CRIT,
	logLevelError = LOG_ERR,
	logLevelInfo  = LOG_INFO,
        logLevelDiag  = LOG_INFO,
    	logLevelLog   = LOG_INFO,
	logLevelWarn  = LOG_WARNING
} logLevel_t;

/*
 * Print Control Flags:
 */
#define PRT_NOFLAGS	0x00
#define PRT_NOFLUSH	0x01
#define PRT_NOIDENT	0x02
#define PRT_NOLEVEL	0x04
#define PRT_NOLOG	0x08
#define PRT_SYSLOG	0x10

/* 
 * Extend Print Flags to Allow Message Types.
 * Note: This is done to use existing LogMsg() API!
 */ 
#define PRT_MSGTYPE_SHIFT	8
#define PRT_MSGTYPE_COMMAND	(1 << PRT_MSGTYPE_SHIFT)
#define PRT_MSGTYPE_PROMPT	(2 << PRT_MSGTYPE_SHIFT)
#define PRT_MSGTYPE_OUTPUT	(3 << PRT_MSGTYPE_SHIFT)
#define PRT_MSGTYPE_STATUS	(4 << PRT_MSGTYPE_SHIFT)
#define PRT_MSGTYPE_FINISHED	(5 << PRT_MSGTYPE_SHIFT)
#define PRT_MSGTYPE_NOPROG	(6 << PRT_MSGTYPE_SHIFT)
#define PRT_MSGTYPE_ERROR	(7 << PRT_MSGTYPE_SHIFT)
#define PRT_MSGTYPE_TRIGGER	(8 << PRT_MSGTYPE_SHIFT)
#define PRT_MSGTYPE_TYPES	8

/* Note: The above map to these message type strings. */
#define MSGTYPE_NONE_STR	"none"
#define MSGTYPE_COMMAND_STR	"command"
#define MSGTYPE_PROMPT_STR	"prompt"
#define MSGTYPE_OUTPUT_STR	"output"
#define MSGTYPE_STATUS_STR	"status"
#define MSGTYPE_FINISHED_STR	"finished"
#define MSGTYPE_NOPROG_STR	"noprog"
#define MSGTYPE_ERROR_STR	"error"
#define MSGTYPE_TRIGGER_STR	"trigger"

/*
 * Reporting Control Flags:
 */
#define RPT_NOFLAGS	0x00
#define RPT_NODEVINFO	0x01		/* Inhibit device information.	*/
#define RPT_NOERRORNUM	0x02		/* Inhibit error time/number.	*/
#define RPT_NOHISTORY	0x04		/* Inhibit dumping history.	*/
#define RPT_NOXERRORS	0x08		/* Inhibit extended error info.	*/
#define RPT_NOERRORMSG	0x10		/* Inhibit error code/message.	*/
#define RPT_NONEWLINE	0x20		/* Inhibit final newline.	*/
#define RPT_NOERRORS	0x40		/* Inhibit error reporting.	*/
#define RPT_NORETRYS	0x80		/* Inhibit retriable errors.	*/
#define RPT_WARNING	0x100		/* Reporting a warning message.	*/

typedef struct {
    char	*ei_file;		/* The file or device name.	*/
    char	*ei_op;			/* The operation that failed.	*/
    optype_t	ei_optype;		/* The operation type.		*/
    HANDLE	*ei_fd;			/* The file handle (if open).	*/
    int		ei_oflags;		/* The last file open flags.	*/
    Offset_t	ei_offset;		/* The current file offset.	*/
    size_t	ei_bytes;		/* The requested size (bytes).	*/
    os_error_t	ei_error;		/* The OS specific error.	*/
    logLevel_t	ei_log_level;		/* The logging level.		*/
    int		ei_prt_flags;		/* The print control flags.	*/
    int		ei_rpt_flags;		/* The reporting control flags.	*/
} error_info_t;

#define INIT_ERROR_INFO(eip, file, op, optype, fd, oflags, offset, bytes, error, log_level, prt_flags, rpt_flags) \
    error_info_t error_information;		\
    error_info_t *eip = &error_information;	\
    eip->ei_file = file;			\
    eip->ei_op = op;				\
    eip->ei_optype = optype;			\
    eip->ei_fd = fd;				\
    eip->ei_oflags = oflags;			\
    eip->ei_offset = offset;			\
    eip->ei_bytes = bytes;			\
    eip->ei_error = error;			\
    eip->ei_log_level = log_level;		\
    eip->ei_prt_flags = prt_flags;		\
    eip->ei_rpt_flags = rpt_flags;

/*
 * Descriptive Names to enable/disable errors and/or retries:
 */
#define EnableErrors	True
#define EnableRetries	True
#define DisableErrors	False
#define DisableRetries	False

/*
 * Declare External Test Functions:
 */
#define NOFUNC		(int (*)()) 0	/* No test function exists yet. */
#define NoFd		INVALID_HANDLE_VALUE

extern int nofunc();			/* Stub return (no test func).	*/
extern struct dtfuncs generic_funcs;	/* Generic test functions.	*/
#if defined(AIO)
extern struct dtfuncs aio_funcs;	/* POSIX AIO test functions.	*/
#endif /* defined(AIO) */
#if defined(MMAP)
extern struct dtfuncs mmap_funcs;	/* Memory map test functions.	*/
#endif /* defined(MMAP) */

typedef uint32_t job_id_t;	/* In case we wish to change later. */

/*
 * Define Device Types:
 */
typedef enum devtype {
    DT_BLOCK, DT_CHARACTER, DT_COMM, DT_DISK,
    DT_GRAPHICS, DT_MEMORY, DT_MMAP, DT_NETWORK,
    DT_PIPE, DT_PROCESSOR, DT_REGULAR,
    DT_SOCKET, DT_SPECIAL, DT_STREAMS, DT_TAPE,
    DT_DIRECTORY, DT_UNKNOWN
} devtype_t;

typedef struct dtype {
    char *dt_type;
    enum devtype dt_dtype;
} dtype_t;

#define PROC_ALLOC (sizeof(pid_t) * 3)	/* Extra allocation for PID.	*/

/*
 * Definitions for Multiple Processes:
 */
typedef struct dt_procs {
	pid_t	dt_pid;			/* The child process ID.	*/
	int	dt_status;		/* The child exit status.	*/
	hbool_t	dt_active;		/* The process active flag.	*/
	char	*dt_device;		/* The process target device.	*/
#if defined(WIN32)
	PROCESS_INFORMATION *dt_pip;	/* Process information.		*/
#endif /* defined(WIN32) */
} dt_procs_t;

/*
 * Slice Range Definition:
 */
typedef struct slice_info {
	int	slice;			/* Slice number.		*/
	large_t	slice_position;		/* Starting slice position.	*/
	large_t	slice_length;		/* The slice data length.	*/
} slice_info_t;

/*
 * Define File Control Flags:
 */
#define DCF_ACTIVE	1		/* File test is active.		*/

/*
 * Define Device Information:
 */
typedef struct dinfo {
	/*
	 * Parsing Information:
	 */ 
	int	argc;			/* Argument count.              */
	char	**argv;			/* Argument pointer array.      */
	char	*cmdbufptr;		/* The command buffer pointer.	*/
	size_t	cmdbufsiz;		/* The command buffer size.	*/
	/*
	 * Script File Information. 
	 */
	int	script_level;		/* The script nesting level.	*/
	char	*script_name[ScriptLevels]; /* The script names.	*/
	FILE	*sfp[ScriptLevels];	/* The script file pointers.	*/
	int	script_lineno[ScriptLevels];
					/* The script file line number.	*/
	char	*di_workload_name;	/* The workload name.		*/
	FILE	*di_efp;		/* Default error data stream.	*/
	FILE	*di_ofp;		/* Default output data stream.	*/
	HANDLE	di_fd;			/* The file descriptor.		*/
	hbool_t	di_shared_file;		/* File shared amongst threads.	*/
	//int	di_flags;		/* The file control flags.	*/
	int	di_oflags;		/* The last file open flags.	*/
        char    *di_array;              /* The array name for scripts.  */
	char	*di_bname;		/* The base file name.		*/
	char	*di_dname;		/* The /dev device name.	*/
	char	*di_device;		/* The real device name.	*/
	uint32_t di_device_size;	/* Default device block size.	*/
	int	di_device_number;	/* Multiple devices number.	*/
	os_ino_t di_inode;		/* The file system i-node number*/
	char	*di_error_file;		/* File name to write errors.	*/
	hbool_t	di_log_opened;		/* Flag to tell log file open.	*/
	hbool_t	di_script_verify;	/* Flag to control script echo. */
	hbool_t	di_stdin_flag;		/* Flag reading from stdin.	*/
	hbool_t	di_stdout_flag;		/* Flag writing to stdout.	*/
	vbool_t	di_terminating;		/* Flag to control terminating.	*/
	int     di_exit_status;         /* The thread exit status.      */
	int	di_priority;		/* The process/thread priority.	*/
	/*
	 * Test Information:
	 */
	test_mode_t di_mode;		/* The current read/write mode.	*/
	file_type_t di_ftype;		/* The file access type.	*/
	struct dtfuncs *di_funcs;	/* The test functions to use.	*/
	dtype_t *di_dtype;		/* The device type information.	*/
	/* For parsing and both input/output devices (copy/verify). */
	dtype_t *di_input_dtype;	/* The input device type info.	*/
	dtype_t *di_output_dtype;	/* The output device type info.	*/
	iodir_t  di_io_dir;		/* Default is forward I/O.	*/
	iomode_t di_io_mode;		/* Default to testing mode.	*/
	iotype_t di_io_type;		/* Default to sequential I/O.	*/
	dispose_t di_dispose_mode;	/* Output file dispose mode.	*/
	onerrors_t di_oncerr_action;	/* The child error action.	*/
					/* Error limit controls tests.	*/
	sleepres_t di_sleep_res;	/* The sleep resolution.	*/
	statslevel_t di_stats_level;	/* Type of statistics to report.*/
	/*
	 * Test Control Information:
	 */
	size_t	di_block_size;		/* Default block size to use.	*/
	size_t	di_iblock_size;		/* Default read block size.	*/
	size_t	di_oblock_size;		/* Default write block size.	*/
	size_t	di_incr_count;		/* Record increment byte count.	*/
	size_t	di_min_size;		/* The minimum record size.	*/
	size_t	di_max_size;		/* The maximum record size.	*/
	bufmodes_t di_buffer_mode;	/* The current buffering mode.	*/
	char	*di_bufmode_type;	/* The buffering mode type.	*/
	hbool_t	di_closing;		/* The device is being closed.	*/
	hbool_t	di_compare_flag;	/* Controls data comparisons.	*/
	hbool_t	di_xcompare_flag;	/* Controls extra comparison.	*/
	hbool_t	di_deleting_flag;	/* Deleting directories/files.	*/
	hbool_t	di_force_core_dump;	/* The force core dump flag.	*/
	hbool_t	di_eof_status_flag;	/* Map EOF to Success flag.	*/
	hbool_t	di_existing_file;	/* The file already exists.	*/
	vu_long	di_error_count;		/* Number of errors detected.	*/
	u_long	di_error_limit;		/* Number of errors tolerated.	*/
	hbool_t	di_extended_errors;	/* Report extended error info.	*/
        hbool_t di_fileperthread;       /* Create a file per thread.    */
	vbool_t	di_file_system_full;	/* The file system is full.	*/
	vbool_t	di_fsfull_restart;	/* Restart writes on FS full.	*/
	hbool_t	di_flushing;		/* The file is being flushed.	*/
	hbool_t di_iolock;		/* The I/O lock control flag.	*/
	u_int32	di_dsize;		/* The device block size.	*/
	u_int32	di_rdsize;		/* The real device block size.	*/
        u_int   di_qdepth;              /* The device queue depth.      */
	large_t	di_capacity;		/* The device capacity (blocks)	*/
	int	di_capacity_percentage;	/* The capacity percentage.	*/
	vbool_t	di_end_of_file;		/* End of file was detected.	*/
	vbool_t	di_end_of_logical;	/* End of logical tape detected	*/
	vbool_t	di_end_of_media;	/* End of media was detected.	*/
	vbool_t	di_beginning_of_file;	/* Beginning of file/media flag	*/
	vbool_t	di_no_space_left;	/* The "no space left" flag.	*/
	hbool_t	di_eof_processing;	/* End of file proessing.	*/
	hbool_t	di_eom_processing;	/* End of media processing.	*/
	hbool_t	di_random_io;		/* Random I/O selected flag.	*/
	hbool_t	di_random_access;	/* Random access device flag.	*/
	u_long	di_pass_count;		/* Number of passes completed.	*/
	u_long	di_pass_limit;		/* Default number of passes.	*/
	hbool_t	di_stop_immediate;	/* Stop immediately w/stop file.*/
	hbool_t	di_timestamp_flag;	/* Timestamp each data block.   */
	hbool_t	di_verbose_flag;	/* Verbose messages output.	*/
	hbool_t	di_verify_flag;		/* Verify the read/write data.	*/
	hbool_t	di_verify_only;		/* Verify of copied data flag.	*/
        /*
         * Forced Corruption Parameters:
         */
        hbool_t di_force_corruption;    /* Force a FALSE corruption.    */
        int32_t di_corrupt_index;       /* Corrupt at buffer index.     */
        uint32_t di_corrupt_length;     /* The corruption length.       */
        uint32_t di_corrupt_pattern;    /* The corruption pattern.      */
        uint32_t di_corrupt_step;       /* Corrupt buffer step value.   */
        uint64_t di_corrupt_reads;      /* Corruption read records.     */
        uint64_t di_corrupt_writes;     /* Corruption write records.    */
	/*
	 * Per Pass Statistics: 
	 */
	v_large	di_dbytes_read;		/* Number of data bytes read.	*/
	v_large	di_dbytes_written;	/* Number of data bytes written.*/
	v_large	di_fbytes_read;		/* Number of file bytes read.	*/
	v_large	di_fbytes_written;	/* Number of file bytes written.*/
	v_large	di_lbytes_read;		/* Number of loop bytes read.	*/
	v_large	di_lbytes_written;	/* Number of loop bytes wrote.	*/
	v_large	di_vbytes_read;		/* Number of volume bytes read.	*/
	v_large	di_vbytes_written;	/* Number of volume bytes wrote.*/
        /* Saved for reread command line. */
	large_t	di_pass_dbytes_read;	/* The pass data bytes read.	*/
	large_t	di_pass_dbytes_written;	/* The pass data bytes written.	*/
	u_long	di_pass_records_read;	/* The pass records read.	*/
	u_long	di_pass_records_written;/* The pass records written.	*/

	/*
	 * Information to Handle "File System Full":
	 */
	size_t	di_last_write_attempted;/* The last write attempted.	*/
	size_t	di_last_write_size;	/* The last write transferred.	*/
        Offset_t di_last_write_offset;	/* The last write offset used.	*/
	large_t	di_last_dbytes_written;	/* The last data bytes written.	*/
	large_t	di_last_fbytes_written;	/* The last file bytes written.	*/
	large_t	di_last_vbytes_written;	/* The last volume bytes wrote.	*/
	large_t	di_discarded_write_data;/* Discarded write data bytes.	*/
	/*
	 * Multiple Directory Data:
	 */
	char	*di_dir;		/* The base directory path.	*/
	char	*di_dirpath;		/* The directory path.		*/
	char	*di_topdirpath;		/* The top directory path.	*/
	char	*di_dirprefix;		/* The directory prefix.	*/
	char	*di_subdir;		/* The sub-directory path.	*/
	char	di_dir_sep;		/* The directory separator.	*/
	hbool_t	di_topdir_created;	/* We created top directory.	*/
	hbool_t	di_dir_created;		/* We created directory flag.	*/
	u_int	di_dir_number;		/* The directory number.	*/
	u_int	di_subdir_number;	/* The subdirectory number.	*/
	u_int	di_subdir_depth;	/* The subdirectory depth.	*/
	u_int	di_last_dir_number;	/* The last directory number.	*/
	u_int	di_last_subdir_number;	/* The subdirectory number.	*/
	u_int	di_last_subdir_depth;	/* The subdirectory depth.	*/
	u_int	di_max_dir_number;	/* The maximum directory.	*/
	u_int	di_max_subdir_number;	/* The maximum subdirectory.	*/
	u_int	di_max_subdir_depth;	/* The maximum subdir depth.	*/
	u_int	di_user_dir_limit;	/* The number of directories.	*/
	u_int	di_user_subdir_depth;	/* The subdirectory depth.	*/
	u_int	di_user_subdir_limit;	/* Number of subdirectories.	*/
	/*
	 * Multiple Files Data:
	 */
	char	*di_file_sep;		/* The inter-file separator.	*/
	char	*di_file_postfix;	/* The file name postfix.	*/
	u_int	di_file_limit;		/* The maximum files per pass.	*/
	u_int	di_file_number;		/* The current file number.	*/
	u_long	di_last_files_read;	/* The last files read.		*/
	u_long	di_last_files_written;	/* The last files written.	*/
	u_long	di_max_files_read;	/* The maximum files read.	*/
	u_long	di_max_files_written;	/* The maximum files written.	*/
	/*
	 * Max data/files Information:
	 */
	large_t di_max_data;		/* Max data limit (all files).	*/
	int	di_max_data_percentage;	/* The max data percentage.	*/
					/*   (percentage of free space)	*/
	u_int	di_max_files;		/* Max file limit (all dirs).	*/
	v_large	di_maxdata_read;	/* Max data read (all files).	*/
	v_large	di_maxdata_written;	/* Max data written (all files)	*/
	hbool_t	di_maxdata_reached;	/* The max data reached flag.	*/
	/*
	 * Per Pass Information:
	 */
	vu_long	di_files_read;		/* Number of files read.	*/
	vu_long	di_files_written;	/* Number of files written.	*/
        u_long  di_full_reads;          /* The # of full records read.  */
        u_long  di_full_writes;         /* The # of full records written*/
        u_long  di_partial_reads;       /* Partial # of records read.   */
        u_long  di_partial_writes;      /* Partial # of records written */
	vu_long	di_records_read;	/* Number of records read.	*/
	vu_long	di_records_written;	/* Number of records written.	*/
	u_long  di_pass_total_records;	/* Pass total (full) records.	*/
	u_long	di_pass_total_partial;	/* Pass total partial records.	*/
	vu_long di_read_errors;		/* Number of read errors.	*/
	vu_long di_write_errors;	/* Number of write errors.	*/
	large_t	di_data_limit;		/* The data limit (in bytes).	*/
	large_t	di_incr_limit;		/* The data limit increment.	*/
	large_t	di_min_limit;		/* The minimum data limit.	*/
	large_t	di_max_limit;		/* The maximum data limit.	*/
	large_t	di_rdata_limit;		/* The random I/O data limit.	*/
	large_t	di_record_limit;	/* Max # of records to process.	*/
	large_t	di_storage_size;	/* The device/file data size.	*/
	large_t	di_user_limit;		/* The user specific data limit.*/
	large_t	di_volume_bytes;	/* Accumulated volume bytes.	*/
	int	di_volume_limit;	/* Number of volumes to process.*/
	vu_long	di_volume_records;	/* The last volume record limit.*/
	/*
	 * Monitoring Information:
	 */
	vtime_t	di_initiated_time;	/* Time the I/O was initiated.	*/
	time_t	di_last_alarm_time;	/* The last alarm time (secs).	*/
	time_t	di_last_keepalive;	/* The last keepalive (secs).	*/
	/*
	 * Monitor Information:
	 */
	unsigned int di_monitor_interval; /* Monitor thread interval.	*/
	pthread_t di_monitor_thread;	/* The monitoring thread.	*/
	/*
	 * Program Run Time Information:
	 */
	time_t	di_alarmtime;		/* The alarm interval time.     */
	time_t	di_keepalive_time;	/* The keepalive time.		*/
	time_t	di_runtime;		/* The program run time.	*/
	time_t	di_runtime_end;		/* The ending future runtime.   */
	time_t	di_program_start;	/* The program start time.	*/
	time_t	di_program_end;		/* The program end time.	*/
	time_t	di_error_time;		/* Time last error occurred.	*/
	char	di_time_buffer[TIME_BUFFER_SIZE]; /* For ASCII time.	*/
	hbool_t	di_TimerActive;		/* Set after timer activated.	*/
	hbool_t	di_TimerExpired;	/* Set after timer has expired.	*/
        char    *di_date_sep;           /* The date field separator.    */
        char    *di_time_sep;           /* The time field separator.    */
        /*
	 * Information for Error Reporting / Triggers:
	 * Note: These are set when reporting device information.
         */
        Offset_t di_offset;		/* Device/file offset (@error).	*/
        size_t di_xfer_size;            /* The transfer size (@error).  */
        large_t di_start_lba;		/* The starting (mapped) LBA.   */
        Offset_t di_error_offset;	/* Corruption error offset.	*/
        large_t di_error_lba;		/* The error (mapped) LBA.      */
	uint32_t di_buffer_index;	/* Corruption buffer index.	*/
        uint32_t di_block_index;	/* Corruption block byte index.	*/
	v_int	di_error;		/* The last error encountered.	*/
	optype_t di_optype;		/* The current operation type.	*/
					/* Note: Only used for noprogs!	*/
	/*
	 * Retry Parameters:
	 */
	hbool_t	di_ignore_errors;	/* Ignore all errors flag.	*/
	vbool_t	di_retrying;		/* The recovery retrying flag.	*/
	u_char	*di_saved_pattern_ptr;	/* Saved pattern buf pointer.	*/
	hbool_t	di_retryDC_flag;	/* Retry data corruptions.	*/
	u_int	di_retryDC_delay;	/* The default retry delay.	*/
	u_int	di_retryDC_limit;	/* Retry data corruption limit.	*/
	u_int	di_retry_delay;		/* The default retry delay.	*/
	int	di_retry_errors[RETRY_ENTRIES]; /* Errors to retry.	*/
	int	di_retry_entries;	/* The number of error codes.	*/
	u_int	di_retry_count;		/* Current number of retries.	*/
	u_int	di_retry_limit;		/* The retryable error limit.	*/
	hbool_t	di_retry_disconnects;	/* Retry session disconnects.	*/
	hbool_t di_retry_warning;	/* Retries logged as warning,	*/
					/* until retry limit reached.	*/
	hbool_t	di_save_corrupted;	/* Save corrupted file data.	*/
	/*
	 * History Information:
	 */
	hbool_t	di_history_dump;	/* Dump history when exiting.	*/
	hbool_t	di_history_dumped;	/* History was already dumped.	*/
	hbool_t	di_history_dumping;	/* History in being dumped.	*/
	hbool_t	di_history_timing;	/* History timing control flag.	*/
	int	di_history_bufs;	/* History buffers per request.	*/
	int	di_history_bsize;	/* The history data block size.	*/
	int	di_history_size;	/* Size of the history array.	*/
	int	di_history_entries;	/* Number of history entries.	*/
	int	di_history_index;	/* Index to next history entry.	*/
	int	di_history_data_size;	/* Request data size to save.	*/
	history_t *di_history;		/* Array of history entries.	*/
	/*
	 * Data Pattern Related Information:
	 */
	hbool_t	di_lbdata_flag;		/* Logical block data flag.	*/
	hbool_t	di_unique_pattern;	/* Unique pattern per process.	*/
	hbool_t	di_iot_pattern;		/* IOT test pattern selected.	*/
	lbdata_t di_lbdata_addr;	/* Starting logical block addr.	*/
	lbdata_t di_lbdata_size;	/* Logical block data size.	*/
	lbdata_t di_iot_seed;		/* The default IOT seed value.	*/
	lbdata_t di_iot_seed_per_pass;	/* The per pass IOT seed value.	*/
	/*
	 * No-progress (noprog) Information:
	 */
	time_t	di_noprogtime;		/* The no progress time (secs).	*/
	time_t	di_noprogttime;		/* The no prog trigger time.	*/
	time_t	di_next_noprog_time;	/* The next no progress time.	*/
	uint32_t di_noprogs;		/* The number of noprogs.	*/
	optype_t di_last_noprog_optype;	/* The last noprog operation.	*/
	time_t	 di_last_noprog_time;	/* The last no proegress time.	*/
	time_t	 di_max_noprogt;	/* The maximum noprogt value.	*/
	optype_t di_max_noprog_optype;	/* The max noprogt operation.	*/
	time_t	 di_max_noprog_time;	/* The time of max no progress.	*/
	time_t	 di_cur_max_noprogt;	/* The current maximum noprogt.	*/
	/* Track total max noprogs for average. */
	uint32_t di_total_max_noprogs;	/* The total max noprogt count.	*/
	uint64_t di_total_max_noprogt;	/* The total max noprogt time.	*/
	/*
	 * Pattern Buffer Information: 
	 */
	uint32_t di_pattern;		/* The data pattern (not IOT).	*/
	uint8_t *di_pattern_buffer;	/* Pointer to pattern buffer.	*/
	uint8_t *di_pattern_bufptr;	/* Pointer into pattern buffer.	*/
	uint8_t *di_pattern_bufend;	/* Pointer to end of pat buffer	*/
	size_t	di_pattern_bufsize;	/* The pattern buffer size.	*/
	char	*di_pattern_string;	/* The pattern string.		*/
	int	di_pattern_strsize;	/* The pattern string size.	*/
	int	di_pattern_index;	/* The pass pattern index.	*/
	hbool_t	di_pattern_in_buffer;	/* Full pattern is in buffer.	*/
	/*
	 * Prefix String Data (initial and formatted): 
	 */
	char	*di_prefix_string;	/* The prefix string (if any).	*/
	int	di_prefix_size;		/* The prefix string size.	*/
	char	*di_fprefix_string;	/* The formatted prefix string.	*/
	int	di_fprefix_size;	/* The formatted prefix size.	*/
	hbool_t	di_uuid_dashes;		/* Flag to control UUID dashes.	*/
	char	*di_uuid_string;	/* The UUID string (if OS has).	*/

	/*
	 * Data Buffers: 
	 */
	size_t	di_data_size;		/* Data buffer size + pad bytes	*/
	size_t	di_data_alloc_size;	/* Data buffer allocation size.	*/
        size_t  di_verify_buffer_size;  /* The verify buffer size.      */
	btag_t	*di_btag;		/* The block tag (btag) buffer.	*/
	uint32_t di_btag_vflags;	/* The btag verify flags.	*/
	uint32_t di_initial_vflags;	/* The initial verify flags.	*/
	u_char	*di_base_buffer;	/* Base address of data buffer.	*/
	u_char	*di_data_buffer;	/* Pointer to data buffer.	*/
	u_char	*di_mmap_buffer;	/* Pointer to mmapped buffer.	*/
	u_char	*di_mmap_bufptr;	/* Pointer into mmapped buffer.	*/
	u_char	*di_verify_buffer;	/* The data verification buffer.*/
	/*
	 * I/O Delays:
	 * Note: volatile, since can be overriden via a iotune file!
	 */
	vu_int	di_open_delay;		/* Delay before opening file.	*/
	vu_int	di_close_delay;		/* Delay before closing file.	*/
	vu_int	di_delete_delay;	/* Delay after deleting files.	*/
	vu_int	di_fsfree_delay;	/* FS free space sleep delay.	*/
	vu_int	di_fsfree_retries;	/* FS free space wait retries.	*/
	vu_int	di_end_delay;		/* Delay between multiple passes*/
	vu_int	di_forced_delay;	/* Force random I/O delay.	*/
	vu_int	di_read_delay;		/* Delay before reading record.	*/
	vu_int	di_start_delay;		/* Delay before starting test.	*/
        vu_int  di_verify_delay;        /* Delay before verifying data. */
	vu_int	di_write_delay;		/* Delay before writing record.	*/
	vu_int	di_term_delay;		/* Child terminate delay count.	*/
	/*
	 * I/O's per Second (IOPS):
	 */
	iops_measure_type_t di_iops_type; /* The IOPS measurment type.	*/
	volatile double	di_iops;	/* The target I/O's per seccond.*/
	int	di_iops_adjust;		/* The adjusted IOPS usec value.*/
	vu_int	di_iops_usecs;		/* The desired IOPS usec delay.	*/
	uint64_t di_actual_total_usecs;	/* The total actual IOPS usecs.	*/
	uint64_t di_target_total_usecs;	/* The total target IOPS usecs.	*/
	/*
	 * Sleep Times:
	 */ 
	uint32_t di_sleep_value;	/* Sleep value (in seconds).	*/
	uint32_t di_msleep_value;	/* Millesecond (ms) sleep value	*/
	uint32_t di_usleep_value;	/* Microsecond (us) sleep value	*/
	/*
	 * Test Times: 
	 */
	struct timeval di_start_timer;	/* The test start time. (hires)	*/
	struct timeval di_end_timer;	/* The test end time.	(hires)	*/
	struct timeval di_pass_timer;	/* The per pass elapsed time.	*/
	/* Note: These must remain for user/system time on Unix! */
	clock_t di_start_time;		/* Test start time (in clicks). */
	clock_t di_end_time;		/* Test end time (in clicks).   */
	clock_t di_pass_time;		/* Per pass elapsed time.       */
	/* Note: Need this for Windows, since my gettimeofday() does   */
	/*       returns seconds since the Epoch (like standard Unix). */
	time_t	di_read_pass_start;	/* Read pass start time.(secs)	*/
	time_t	di_write_pass_start;	/* Write pass start time.(secs)	*/
	struct tms di_stimes;		/* The start time.		*/
	struct tms di_ptimes;		/* The pass time.		*/
	struct tms di_etimes;		/* The end time.		*/
	struct timeval di_gtod;		/* Current GTOD information.	*/
	struct timeval di_ptod;		/* Previous GTOD information.	*/
	pid_t	di_child_pid;		/* For the child process ID.	*/
	int	di_child_status;	/* For child exit status.	*/

#if defined(AIO)
	/*
	 * Asynchronous I/O: 
	 */
	int	di_aio_bufs;		/* The number of AIO buffers.	*/
	int	di_aio_index;		/* Index to AIO control block.	*/
	volatile Offset_t di_aio_offset; /* AIO offset (we maintain).	*/
	v_large	di_aio_data_bytes;	/* Total data bytes per pass.	*/
	v_large	di_aio_file_bytes;	/* # of file bytes processed.	*/
	vu_long	di_aio_record_count;	/* # of records to processed.	*/
	u_int32	di_aio_lba;		/* AIO logical block address.	*/
	
	/*
	 * The following variables are meant to be used with tape devices to
	 * backup unprocessed files and/or records due to read-ahead, to be
	 * repositioned prior to the next test or before closing the tape.
	 */
	ssize_t	di_aio_data_adjust;	/* # of data bytes to adjust.	*/
	u_long	di_aio_file_adjust;	/* # of tape files to adjust.	*/
	u_long	di_aio_record_adjust;	/* # of tape record to adjust.	*/
	
	struct aiocb	*di_acbs;	/* Pointer to AIO control blocks. */
	void		**di_aiobufs;	/* Pointer to base buffer addrs.  */
	struct aiocb	*di_current_acb;/* Current acb for error reports. */
	
#else /* !defined(AIO) */
	int	di_aio_bufs;		/* The number of AIO buffers.	*/
#endif /* !defined(AIO) */

	hbool_t	di_aio_flag;		/* Asynchronous I/O (AIO) flag.	*/
	vbool_t	di_dio_flag;		/* Solaris/Win32 Direct I/O.    */
	int	di_align_offset;	/* Align buffer at this offset.	*/
	
	hbool_t	di_dumpall_flag;	/* Controls dumping all blocks.	*/
	hbool_t	di_dump_context_flag;	/* Dump good block for context.	*/
	u_int	di_max_bad_blocks;	/* Maximum bad blocks to dump.	*/
	bfmt_t	di_boff_format;		/* Buffer offset data format.	*/
	dfmt_t	di_data_format;		/* Data display format.		*/
	
	bufmodes_t di_buffer_modes[NUM_BUFMODES]; /* I/O buffer modes.	*/
	int	di_bufmode_index;	/* Current buffer mode index.	*/
	int	di_bufmode_count;	/* The user specified bufmodes.	*/
	
	size_t	di_dump_limit;		/* The dump buffer data limit.	*/
	hbool_t	di_bypass_flag;		/* Bypass (some) sanity checks.	*/
	hbool_t	di_cerrors_flag;	/* Report device close errors.	*/
	hbool_t	di_child_flag;		/* This is a child process.	*/
	/*
	 * Debug Flags: 
	 */
	hbool_t	di_debug_flag;		/* Enable debug output flag.	*/
	hbool_t	di_Debug_flag;		/* Verbose debug output flag.	*/
	hbool_t	di_btag_debugFlag;	/* Block tag (btag) debug flag.	*/
	hbool_t	di_eDebugFlag;		/* End of file debug flag.	*/
	hbool_t	di_fDebugFlag;		/* File operations debug flag.	*/
	hbool_t	di_jDebugFlag;		/* Job control debug flag.	*/
	hbool_t	di_lDebugFlag;		/* File locking debug flag.	*/
	hbool_t	di_mDebugFlag;		/* Memory related debug flag.	*/
	hbool_t	di_mntDebugFlag;	/* Mount device lookup debug.	*/
	hbool_t	di_pDebugFlag;		/* Process related debug flag.	*/
	hbool_t	di_rDebugFlag;		/* Random (seek) debug flag.	*/
	hbool_t	di_sDebugFlag;		/* SCSI debug output flag.	*/
	hbool_t	di_tDebugFlag;		/* The thread debug flag.	*/
	hbool_t	di_timerDebugFlag;	/* Timer (alarm) debug flag.	*/
	hbool_t	di_delete_per_pass;	/* Delete files per pass flag.	*/
	/* File system caching is enabled by default, these control each. */
	vbool_t	di_read_cache_flag;	/* FS Read cache control flag.	*/
	vbool_t	di_write_cache_flag;	/* FS Write cache control flag.	*/
	hbool_t	di_btag_flag;		/* The block tag control flag.	*/
	hbool_t	di_dump_btags;		/* The dump btags control flag.	*/
	hbool_t	di_dump_flag;		/* Dump data buffer on errors.	*/
	hbool_t	di_errors_flag;		/* The report errors flag.	*/
	hbool_t	di_forked_flag;		/* Forked child process flag.	*/
	hbool_t	di_fsincr_flag;		/* File size increment flag.	*/
	hbool_t	di_fsync_flag;		/* fsync() after writes flag.	*/
	u_int	di_fsync_frequency;	/* The file flush frequency.	*/
	hbool_t	di_mount_lookup;	/* The mount point lookup flag.	*/
	hbool_t	di_multiple_devs;	/* Multiple devices flag.	*/
	hbool_t	di_multiple_dirs;	/* Multiple directories flag.	*/
	hbool_t	di_multiple_files;	/* Multiple files flag.		*/
	Offset_t di_end_position;	/* End position for stepping.	*/
	Offset_t di_file_position;	/* The starting file position.	*/
	Offset_t di_ofile_position;	/* The output file position.	*/
	Offset_t di_last_position;	/* Last position lseeked to.	*/
	Offset_t di_step_offset;	/* Step offset for disk seeks.	*/
	hbool_t	di_keep_existing;	/* Don't delete existing files.	*/
	hbool_t	di_noprog_flag;		/* Check for no I/O progress.	*/
	hbool_t	di_poison_buffer;	/* Poison read buffers. 	*/
	hbool_t	di_prefill_buffer;	/* Prefill read buffers.	*/
	hbool_t	di_unique_log;		/* Make the log file unique.	*/
	hbool_t	di_unique_file;		/* Make output file unqiue.	*/
        hbool_t di_user_errors;         /* User specified error limiet. */
	hbool_t	di_user_incr;		/* User specified incr count.	*/
	hbool_t	di_user_min;		/* User specified min size.	*/
	hbool_t	di_user_max;		/* User specified max size.	*/
	hbool_t	di_user_ralign;		/* User specified random align. */
	hbool_t	di_user_rseed;		/* Flags user specified rseed.	*/
	hbool_t	di_user_lbdata;		/* User specified starting lba.	*/
	hbool_t	di_user_lbsize;		/* User specified lbdata size.	*/
	hbool_t	di_user_pattern;	/* Flags user specified pattern	*/
	hbool_t	di_user_position;	/* User specified file position.*/
        hbool_t di_user_oposition;      /* The output offset specified. */
	hbool_t	di_incr_pattern;	/* Incrementing data pattern.	*/
	hbool_t	di_logheader_flag;	/* The log file header flag.	*/
	hbool_t	di_logtrailer_flag;	/* The log file trailer flag.	*/
	hbool_t	di_logappend_flag;	/* Append to log file flag.	*/
	hbool_t	di_logdiag_flag;	/* Log diagnostic messages.	*/
	hbool_t	di_logpid_flag;		/* Log process ID w/messages.	*/
	hbool_t	di_joblog_inhibit;	/* Inhibit logging to job log.	*/
	hbool_t	di_syslog_flag;		/* Log errors to syslog.	*/
	hbool_t	di_loop_on_error;	/* The loop on error flag.	*/
	hbool_t	di_mmap_flag;		/* Do memory mapped file I/O.	*/
	hbool_t	di_media_changed;	/* Shows when media changed.	*/
	int	di_last_flags;		/* The last open flags used.	*/
	int	di_initial_flags;	/* The initial open flags.	*/
	int	di_open_flags;		/* Common file open flags.	*/
	int	di_write_flags;		/* Additional write open flags.	*/
	int	di_read_mode;		/* The read open mode to use.	*/
	int	di_write_mode;		/* The write open mode to use.	*/
	int	di_rwopen_mode;		/* The read/write open mode.	*/
#if defined(WIN32)
	/* Windows specific: */
	DWORD	di_DesiredAccess;
	DWORD	di_CreationDisposition;
	DWORD	di_FlagsAndAttributes;
	DWORD	di_ShareMode;
#endif /* defined(WIN32) */
	int 	di_log_level;		/* The logging level.		*/
	int     di_sequence;		/* The sequence number.		*/
	hbool_t	di_pad_check;		/* Check data buffer pad bytes.	*/
	hbool_t	di_spad_check;		/* Check short record pad bytes.*/
	u_long	di_skip_count;		/* # of input records to skip.	*/
	u_long	di_seek_count;		/* # of output records to seek.	*/
	Offset_t di_random_align;	/* Random I/O offset alignment.	*/
	large_t	di_total_bytes;		/* Total bytes transferred.	*/
	large_t di_total_bytes_read;	/* Total bytes read.		*/
	large_t di_total_bytes_written;	/* Total bytes written.		*/
	//vu_long di_total_errors;	/* Total errors (all passes).	*/
	large_t	di_total_files;		/* Total files (all passes).	*/
	large_t di_total_files_read;	/* Total files read.		*/
	large_t di_total_files_written;	/* Total files written.		*/
	large_t	di_total_records;	/* Total records (all passes).	*/
	large_t di_total_records_read;	/* Total records read (test).   */
	large_t di_total_records_written; /* Total records written (test) */
	u_long	di_total_partial;	/* Total partial records.	*/
	u_long	di_total_partial_reads; /* Total partial record reads.  */
	u_long	di_total_partial_writes;/* Total partial record writes. */
	u_long	di_warning_errors;	/* Total non-fatal error count.	*/
	hbool_t di_job_stats_flag;	/* Display the job statistics.	*/
	hbool_t	di_pstats_flag;		/* Display per pass statistics.	*/
	hbool_t	di_total_stats_flag;	/* Display total statistics.	*/
	hbool_t	di_raw_flag;		/* The read after write flag.	*/
	hbool_t	di_reread_flag;		/* Force a re-read after raw.	*/
	hbool_t	di_rotate_flag;		/* Force data buffer rotating.	*/
	int	di_rotate_offset;	/* Current rotate buffer offset	*/
	hbool_t	di_prealloc_flag;	/* Preallocate file blocks (win)*/
	hbool_t	di_sparse_flag;		/* Sparse file attribute (win).	*/
	hbool_t	di_stats_flag;		/* Display total statistics.	*/
	char	*di_cmd_line;		/* Copy of our command line.	*/
	char	*di_job_log;		/* The job log file name.	*/
	char	*di_log_dir;		/* The default log directory.	*/
	char	*di_log_file;		/* The log file name.		*/
	char	*di_log_format;		/* The log file format string.	*/
	char	*di_log_buffer;		/* Pointer to log file buffer.	*/
	char	*di_log_bufptr;		/* Pointer into log buffer.	*/
	char	*di_log_prefix;		/* The per line log prefix.	*/
	ssize_t	di_log_bufsize;		/* The log buffer size.		*/
	/* Note: Previously used to format message for syslog(). */
	//char	*di_msg_buffer;		/* Diagnostic message buffer.	*/
	char	*di_stderr_buffer;	/* The standard error buffer.	*/
	/*
	 * I/O Tuning Definitions:
	 */
        time_t	di_iotune_mtime;	/* The last modification time.	*/
	char	*di_iotune_file;	/* Pointer to IO tune file.	*/
	char	*di_input_file;		/* Pointer to input file name.	*/
	char	*di_output_file;	/* Pointer to output file name.	*/
	char	*di_pass_cmd;		/* The per pass command.	*/
	char	*di_pattern_file;	/* Pointer to pattern file name	*/
	char	*di_stop_on_file;	/* Stop on file existance.	*/
	hbool_t	di_image_copy;		/* Sanity check image copies.	*/
	hbool_t	di_max_capacity;	/* Use max capacity from IOCTL.	*/
	large_t	di_user_capacity;	/* The user set drive capacity.	*/
	/* 
	 * Multiple Volumes: 
	 */
	hbool_t	di_multi_flag;		/* Multi-volume media flag.	*/
	v_int	di_multi_volume;	/* Multi-volume media count.	*/
	hbool_t	di_volumes_flag;	/* Flags the volumes option.	*/
        /* Random/Variable Parameters: */
	uint64_t di_random_seed;	/* Seed for random # generator.	*/
	hbool_t	di_variable_flag;	/* Variable block size flag.	*/
	hbool_t	di_variable_limit;	/* Variable data limit flag.	*/
	hbool_t	di_vary_iodir;		/* Vary sequential direction.	*/
	hbool_t	di_vary_iotype;		/* Vary the I/O type flag.	*/
	/*
	 * Fill Pattern/File Definitions: 
	 */
	hbool_t di_fill_always;		/* Always fill the files.	*/
	hbool_t di_fill_once;		/* Fill the file once flag.	*/
	hbool_t	di_user_fpattern;	/* User speciifed fill pattern.	*/
	uint32_t di_fill_pattern;	/* Write fill data pattern.	*/
	uint32_t di_prefill_pattern;	/* Read prefill data pattern.	*/
	/*
	 * I/O Percentages:
	 */
	int	di_read_percentage;	/* The read/write perecentage.	*/
	int	di_random_percentage;	/* The random/sequential perc.	*/
	int	di_random_rpercentage;	/* The read random percentage.	*/
	int	di_random_wpercentage;	/* The write random percentage.	*/
	/*
	 * Trigger Definitions: 
	 */
	hbool_t	di_trigargs_flag;	/* Trigger arguments flag.	*/
        hbool_t di_trigdefaults_flag;   /* Automatic trigger defaults.  */
	hbool_t	di_trigdelay_flag;	/* Delay mismatch triggers.	*/
	vbool_t	di_trigger_active;	/* The trigger active flag.	*/
	int	di_num_triggers;	/* The number of triggers.	*/
	int	di_trigger_action;	/* The trigger action (user).	*/
	pthread_t di_trigger_thread;	/* The trigger command thread.	*/
	trigger_control_t di_trigger_control; /* The trigger control.	*/
	trigger_data_t di_triggers[NUM_TRIGGERS];
	/*
	 * Keepalive Definitions: 
	 */
	char	*di_keepalive;		/* The keepalive string.	*/
	char    *di_pkeepalive;		/* The pass keepalive string.	*/
	char    *di_tkeepalive;		/* The total keepalive string.	*/
	hbool_t	di_user_keepalive;	/* User specified keepalive flag*/
	hbool_t	di_user_pkeepalive;	/*        ... ditto ...         */
	hbool_t	di_user_tkeepalive;	/*        ... ditto ...         */
	/*
	 * Multiple Process Information: 
	 */
	hbool_t	di_aborted_processes;	/* Processes have been aborted.	*/
	char	*di_dtcmd;		/* Command line for subprocs.	*/
#if defined(WIN32)
	HANDLE	di_proc_handles[MAXIMUM_WAIT_OBJECTS]; /* for subprocs.	*/
#endif /* defined(WIN32) */
	pid_t	di_process_id;		/* The current process ID.	*/
	struct dt_procs *di_ptable;	/* Multiple process table.	*/
	int	di_num_devs;		/* Number of devices specified.	*/
	int	di_num_procs;		/* Number of procs to create.	*/
	int	di_cur_proc;		/* The current process number.	*/
	int	di_max_procs;		/* Maximum processes started.	*/
	int	di_procs_active;	/* Number of active processes.	*/
	int	di_slices;		/* Number of slices to create.	*/
	int	di_slice_number;	/* Slice number to operate on.	*/
        Offset_t di_slice_offset;       /* The starting slice offset.   */
	/*
	 * Thread Related Information:
	 */ 
	hbool_t	di_async_job;		/* Execute job asynchronously.	*/
	struct job_info *di_job;	/* Pointer to job information.	*/
	char	*di_job_tag;		/* The user defined job tag.	*/
	time_t	di_term_wait_time;	/* The termination wait time.	*/
	int	di_threads;		/* Number of threads to create.	*/
	int	di_threads_active;	/* The number of active threads.*/
	pthread_t di_thread_id;		/* The thread ID.		*/
	time_t	di_thread_stopped;	/* The thread stopped time.	*/
	int	di_thread_number;	/* The current thread number.	*/
	istate_t di_initial_state;	/* Initial state after create.	*/
	vtstate_t di_thread_state;	/* Our maintained thread state.	*/
	void	*(*di_thread_func)(void *arg); /* Thread function.	*/

	hbool_t	di_iotuning_flag;	/* The iotuning control flag.	*/
	hbool_t	di_iotuning_active;	/* The iotuning active flag.	*/
	unsigned int di_iotune_delay;	/* The iotuning retry delay.	*/
#if defined(NVME)
        /*
         * NVMe Information:
         */
        uint32_t di_namespace_id;       /* The NVMe namespace ID.       */
        uint32_t di_nvme_sector_size;   /* The sector size.             */
        uint64_t di_namespace_size;     /* The namespave size (bytes).  */
        uint64_t di_namespace_capacity; /* The namespace capacity.      */
        uint64_t di_namespace_utilization; /* Namespace utilization.    */
        char    *di_namespace_nguid;    /* Globally Unique Identifier.  */
        char    *di_namespace_eui64;    /* IEEE Extended Unique Identifier. */
        char    *di_nvm_subsystem_nqn;  /* Subsystem NVMe Qualified Name. */
        long double di_total_nvm_capacity; /* Total NVM capacity (bytes). */
        long double di_unalloc_nvm_capacity; /* Total NVM capacity (bytes). */
#endif /* defined(NVME) */
#if defined(SCSI)
	/*
	 * SCSI Specific Information:
	 */
	hbool_t	di_scsi_errors;		/* The SCSI error logging flag.	*/
	hbool_t di_scsi_info_flag;	/* The SCSI information flag.	*/
	hbool_t	di_scsi_recovery;	/* The SCSI recovery flag.	*/
	hbool_t di_scsi_sense;		/* Display full sense flag.     */
	char	*di_scsi_dsf;		/* The SCSI device special file.*/
	char	*di_tscsi_dsf;		/* The SCSI trigger device file.*/
	scsi_generic_t *di_sgp;		/* The SCSI generic data.       */
	scsi_generic_t *di_sgpio;	/* The SCSI I/O generic data.   */
	scsi_generic_t *di_tsgp;	/* The trigger SCSI generic.    */
	unsigned int di_scsi_timeout;	/* The SCSI CDB timeout value.	*/
	uint32_t di_scsi_recovery_delay; /* The SCSI recovery delay.	*/
	uint32_t di_scsi_recovery_limit; /* The SCSI recovery limit.	*/
	inquiry_t *di_inquiry;		/* The device Inquiry data.	*/
	idt_t	di_idt;			/* The Inquiry data type.	*/
	char	*di_vendor_id;		/* The Inquiry vendor ID.	*/
	char	*di_product_id;		/* The Inquiry product ID.	*/
	char	*di_revision_level;	/* The Inquiry revision level.	*/
	char	*di_device_id;		/* The device identifier.	*/
	char	*di_serial_number;	/* The device serial number.	*/
	char	*di_mgmt_address;	/* The mgmt network address.	*/
	/* Read Capacity Information */
	large_t	di_device_capacity;	/* The device capacity (blocks).*/
	u_int32	di_block_length;	/* The device block length.	*/

	/*
	 * SCSI Unmap/Punch Hole/Write Same w/unmap Information:
	 */ 
	hbool_t	di_get_lba_status_flag;	/* Get LBA status control flag.	*/
	hbool_t	di_unmap_flag;		/* Unmap blocks control flag.	*/
	unmap_type_t di_unmap_type;	/* The Unmap type to execute.	*/
	u_int	di_unmap_frequency;	/* The unmap frequency.		*/
	char	*di_spt_path;		/* Until spt is integrated.	*/
	char	*di_spt_options;	/* Additional spt options.	*/
	/* Logical Block Provisioning Parameters: */
	/* These originate from Read Capacity(16) data. */
	hbool_t	di_lbpmgmt_valid;	/* Provisioning mgmt is valid.	*/
	hbool_t	di_lbpme_flag;		/* Provisioning mgmt enabled.	*/
					/*   True = Thin Provisioned	*/
					/*   False = Full Provisioned	*/
	hbool_t di_fua;			/* Force unit access for I/O.	*/
	hbool_t di_dpo;			/* Disable page out for I/O.	*/
	/*
	 * SCSI I/O Definitions:
	 */
	scsi_io_type_t di_scsi_read_type;  /* The default read opcode.	*/
	scsi_io_type_t di_scsi_write_type; /* The default write opcode.	*/
        /*
	 * SCSI Trigger Definitions:
	 */
	unsigned char di_cdb[MAX_CDB];	/* Command descriptor block.	*/
	unsigned char di_cdb_size;	/* The SCSI CDB size.		*/
#endif /* defined(SCSI) */
	/* Always define these SCSI flags to reduce conditionalization. */
        hbool_t di_nvme_flag;           /* The NVMe control flag.       */
	hbool_t	di_scsi_flag;		/* The SCSI control flag.	*/
	hbool_t	di_scsi_io_flag;	/* Flag to control SCSI I/O.	*/
	hbool_t	di_nvme_io_flag;	/* Flag to control NVMe I/O.	*/
	/*
	 * Mounted File System Information:
	 */
	char	*di_mounted_from_device;/* The mount from device name.	*/
	char	*di_mounted_on_dir;	/* The mounted on directory.	*/
	char	*di_filesystem_type;	/* The type of file system.	*/
	char	*di_filesystem_options;	/* The file system options.	*/
	/* Note: These are only valid for Windows at this time! */
	uint32_t di_file_system_flags;	/* The file system flags.	*/
	char	*di_protocol_version;	/* The protocol version.	*/
	char	*di_universal_name;	/* The universal name.		*/
	char	*di_volume_name;	/* The volume name.		*/
	char	*di_volume_path_name;	/* The volume path name.	*/
	uint32_t di_volume_serial_number; /* The volume serial number.	*/
	/*
	 * File System Information:
	 */
	hbool_t	di_fsalign_flag;	/* Align FS offsets and sizes.  */
	hbool_t	di_fsfile_flag;		/* The file system file flag.	*/
	hbool_t	di_fsmap_flag;		/* Flag to control FS file map.	*/
	uint32_t di_fs_block_size;	/* The file system block size.	*/
	large_t	di_fs_space_free;	/* The file system free space.	*/
	large_t di_fs_total_space;	/* The total file system space.	*/
	void	*di_fsmap;	    	/* The file system map info.	*/
        fsmap_type_t di_fsmap_type;     /* Show file system map type.   */
	/*
	 * File system trim Parameters:
	 */
	hbool_t	di_fstrim_flag;		/* The fstrim control flag.	*/
	uint32_t di_fstrim_frequency;	/* The fstrim frequency.	*/

	/*
	 * File Lock Parameters:
	 */
	hbool_t di_lock_files;		/* The lock files flag.		*/
	int	di_unlock_chance;	/* The unlock chance (%).	*/
	uint32_t di_lock_errors;	/* The number of lock errors.	*/
	lock_mode_t *di_lock_mode;	/* The locking mode.		*/
	char	*di_lock_mode_name;	/* The lock mode name.		*/
	uint64_t di_lock_stats[NUM_LOCK_TYPES]; /* The lock statistics.	*/

	/*
	 * Definitions for per thread random number generator.
	 */ 
	mtrand64_t *di_mtrand;		/* Random number information.	*/
	/* 
	 * For Copy/Verify, the output device is here (for now)! 
	 */
	struct dinfo *di_output_dinfo;	/* Output device information.	*/
					/* This is used for copy/verify	*/
	/*
	 * I/O Behavior Definitions:
	 */
	iobehavior_t di_iobehavior;	/* The I/O behavior.		*/
	struct iobehavior_funcs *di_iobf; /* I/O behavior functions.	*/
	void	*di_opaque;		/* Tool specific information.	*/

	/*
	 * Networking Definitions:
	 */
	int	di_inet_family;		/* The INET family for lookups.	*/
} dinfo_t;

typedef struct threads_info {
    int		ti_threads;		/* The number of active threads.	*/
    int		ti_finished;		/* The number of finished threads.	*/
    dinfo_t	**ti_dts;		/* Array of device info pointers.	*/
    int		ti_status;		/* Status from joined threads.		*/
} threads_info_t;

typedef struct job_info {
    struct job_info *ji_flink;		/* Forward link to next entry.  */
    struct job_info *ji_blink;		/* Backward link to prev entry. */
    pthread_mutex_t ji_job_lock;	/* Per job lock.		*/
    job_id_t	ji_job_id;		/* The job identifier.		*/
    vjstate_t	ji_job_state;		/* The job state.		*/
    int		ji_job_status;		/* The job status.		*/
    char	*ji_job_tag;		/* The users job tag.		*/
    char	*ji_job_logfile;	/* The job log file name.	*/
    FILE	*ji_job_logfp;		/* The job log file pointer.	*/
    time_t	ji_job_start;		/* The job start time.		*/
    time_t	ji_job_end;		/* The job end time.		*/
    time_t	ji_job_stopped;		/* The job stopped time.	*/
    time_t	ji_threads_started;	/* The threads start time.	*/
    pthread_mutex_t ji_print_lock;	/* The job print lock.		*/
    pthread_mutex_t ji_thread_lock;	/* The thread wait lock.	*/
    threads_info_t *ji_tinfo;		/* The thread(s) information.	*/
    void        *ji_opaque;     	/* Test specific opaque data.   */
} job_info_t;

#define DT_IOLOCK 1

/*
 * Shared data for jobs with multiple threads (not slices). 
 */
typedef struct io_global_data {
    pthread_mutex_t io_lock;
    vbool_t	    io_waiting_active;
    vbool_t         io_initialized;
    vbool_t         io_end_of_file;
    v_int	    io_threads_done;
    v_int	    io_threads_waiting;
    v_large         io_bytes_read;
    v_large         io_bytes_written;
    vu_long         io_error_count;
    vu_long         io_records_read;
    vu_long         io_records_written;
    Offset_t        io_starting_offset;
    volatile Offset_t io_sequential_offset;
} io_global_data_t;

/*
 * Modify Parameters:
 */ 
typedef struct modify_params {
    job_id_t	job_id;			/* The job identifier.		*/
    char	*job_tag;		/* The job tag (optional).	*/
    hbool_t	cdelay_parsed;
    uint32_t	close_delay;
    hbool_t	ddelay_parsed;
    uint32_t	delete_delay;
    hbool_t	edelay_parsed;
    uint32_t	end_delay;
    hbool_t	odelay_parsed;
    uint32_t	open_delay;
    hbool_t	rdelay_parsed;
    uint32_t	read_delay;
    hbool_t	sdelay_parsed;
    uint32_t	start_delay;
    hbool_t	wdelay_parsed;
    uint32_t	write_delay;
    hbool_t	debug_parsed;
    uint32_t	debug_flag;
    hbool_t	Debug_parsed;
    uint32_t	Debug_flag;
    hbool_t	eDebug_parsed;
    uint32_t	eDebug_flag;
    hbool_t	fDebug_parsed;
    uint32_t	fDebug_flag;
    hbool_t	jDebug_parsed;
    uint32_t	jDebug_flag;
    hbool_t	rDebug_parsed;
    uint32_t	rDebug_flag;
    hbool_t	sDebug_parsed;
    uint32_t	sDebug_flag;
    hbool_t	tDebug_parsed;
    uint32_t	tDebug_flag;
    hbool_t	pstats_flag_parsed;
    uint32_t	pstats_flag;
    hbool_t	stats_flag_parsed;
    uint32_t	stats_flag;
} modify_params_t;

/*
 * Define test function dispatch structure:
 *
 * [ NOTE:  These functions are not all used at this time.  The intent
 *   is to cleanup the code later by grouping functions appropriately. ]
 */
typedef struct dtfuncs {
						/* Open device or file.	   */
    int	(*tf_open)(struct dinfo	*dip, int oflags);
						/* Close device or file.   */
    int	(*tf_close)(struct dinfo *dip);
						/* Special initilization.  */
    int	(*tf_initialize)(struct dinfo *dip);
						/* Start test processing.  */
    int	(*tf_start_test)(struct dinfo *dip);
						/* End of test processing. */
    int	(*tf_end_test)(struct dinfo *dip);
						/* Read file data.	   */
    int	(*tf_read_file)(struct dinfo *dip);
						/* Processes read data.	   */
    int	(*tf_read_data)(struct dinfo *dip);
						/* Cancel read requests.   */
    int	(*tf_cancel_reads)(struct dinfo *dip);
						/* Write file data.	   */
    int	(*tf_write_file)(struct dinfo *dip);
						/* Processes write data.   */
    int	(*tf_write_data)(struct dinfo *dip);
						/* Cancel write requests.  */
    int	(*tf_cancel_writes)(struct dinfo *dip);
						/* Flush data to media.	   */
    int	(*tf_flush_data)(struct dinfo *dip);
						/* Verify data read.	   */
    int	(*tf_verify_data)(	struct dinfo	*dip,
				uint8_t		*buffer,
				size_t		count,
				uint32_t	pattern,
				uint32_t	*lba,
				hbool_t		raw_flag );
						/* Reopen device or file.  */
    int	(*tf_reopen_file)(struct dinfo *dip, int oflags);
						/* Test startup handling.  */
    int	(*tf_startup)(struct dinfo *dip);
						/* Test cleanup handling.  */
    int	(*tf_cleanup)(struct dinfo *dip);
						/* Validate test options.  */
    int	(*tf_validate_opts)(struct dinfo *dip);
						/* Report block tag.	   */
    int	(*tf_report_btag)(struct dinfo *dip, btag_t *ebtag, btag_t *rbtag, hbool_t raw_flag);
						/* Update block tag.	   */
    int	(*tf_update_btag)(struct dinfo *dip, btag_t *btag, Offset_t offset,
			  uint32_t record_index, size_t record_size, uint32_t record_number);
						/* Verify block tag.	   */
    int	(*tf_verify_btag)(struct dinfo *dip, btag_t *ebtag, btag_t *rbtag,
			  uint32_t *eindex, hbool_t raw_flag);
} dtfuncs_t;

typedef struct iobehavior_funcs {
    char *iob_name;
    iobehavior_t iob_iobehavior;
    int (*iob_map_options)(struct dinfo *dip, int argc, char **argv);
    char *iob_maptodt_name;
    int (*iob_dtmap_options)(struct dinfo *dip, int argc, char **argv);
    int (*iob_initialize)(struct dinfo *dip);
    int (*iob_initiate_job)(struct dinfo *dip);
    int (*iob_parser)(struct dinfo *dip, char *option);
    void (*iob_cleanup)(struct dinfo *dip);
    int (*iob_clone)(struct dinfo *dip, struct dinfo *cdip, hbool_t new_thread);
    void *(*iob_thread)(void *arg);
    void *(*iob_thread1)(void *arg);
    int (*iob_job_init)(struct dinfo *dip, struct job_info *job);
    int (*iob_job_cleanup)(struct dinfo *dip, struct job_info *job);
    int (*iob_job_finish)(struct dinfo *dip, struct job_info *job);
    int (*iob_job_modify)(struct dinfo *dip, struct job_info *job);
    int (*iob_job_query)(struct dinfo *dip, struct job_info *job);
    int (*iob_job_keepalive)(struct dinfo *dip, struct job_info *job);
    int (*iob_thread_keepalive)(struct dinfo *dip);
    void (*iob_show_parameters)(struct dinfo *dip);
    int (*iob_validate_parameters)(struct dinfo *dip);
} iobehavior_funcs_t;

/*
 * Workload Definitions:
 */ 
typedef struct workload_entry {
    char	*workload_name;
    char	*workload_desc;
    char	*workload_options;
} workload_entry_t;

/*
 * Macros to Improve Performance:
 */
#if defined(INLINE_FUNCS)

/* Note: Macros work better for old compilers! */

#define make_lba(dip, pos)	\
	(u_int32)((pos == (Offset_t)0) ? (u_int32) 0 : (pos / dip->di_lbdata_size))

#define make_offset(dip, lba)	((Offset_t)(lba * dip->di_lbdata_size))

#define make_lbdata(dip, pos)	\
	(uint32_t)((pos == (Offset_t)0) ? (uint32_t) 0 : (pos / dip->di_lbdata_size))

#define make_position(dip, lba)	((Offset_t)(lba * dip->di_lbdata_size))

#endif /* defined(INLINE_FUNCS) */

extern FILE *efp, *ofp;
extern char *error_log;
extern FILE *error_logfp;
extern char *master_log;
extern FILE *master_logfp;
extern dinfo_t *master_dinfo;

extern clock_t hertz;
extern int exit_status;
extern u_int32 data_patterns[];
extern int npatterns;

extern hbool_t core_dump;
/* TODO: These debug flags... */
extern volatile hbool_t CmdInterruptedFlag;
extern hbool_t InteractiveFlag, DeleteErrorLogFlag, ExitFlag, PipeModeFlag;
extern hbool_t debug_flag, mDebugFlag, pDebugFlag, tDebugFlag;

extern vbool_t terminating_flag;
extern hbool_t terminate_on_signals;
extern hbool_t sighup_flag;

extern char *cmdname, *dtpath;
extern int page_size;

extern unsigned int cancel_delay;
extern unsigned int kill_delay;

/*
 * Volatile Storage:
 */
extern hbool_t StdinIsAtty, StdoutIsAtty, StderrIsAtty;
extern char *keepalive0, *keepalive1;

/*
 * Function Prototypes:
 */

/* dt.c */
extern int SetupCommandBuffers(dinfo_t *dip);
extern int dtGetCommandLine(dinfo_t *dip);
extern int ExpandEnvironmentVariables(dinfo_t *dip, char *bufptr, size_t bufsiz);
extern int MakeArgList(char **argv, char *s);

extern pthread_attr_t *tdattrp, *tjattrp;
extern pthread_mutex_t print_lock;
extern int create_thread_log(dinfo_t *dip);
extern int create_master_log(dinfo_t *dip, char *log_name);
extern int create_detached_thread(dinfo_t *dip, void *(*func)(void *));
extern int do_monitor_processing(dinfo_t *mdip, dinfo_t *dip);
extern void do_setup_keepalive_msgs(dinfo_t *dip);
extern int do_prejob_start_processing(dinfo_t *mdip, dinfo_t *dip);
extern int do_common_thread_startup(dinfo_t *dip);
extern void do_common_thread_exit(dinfo_t *dip, int status);
extern void do_common_startup_logging(dinfo_t *dip);
extern int do_common_device_setup(dinfo_t *dip);
extern int do_common_file_system_setup(dinfo_t *dip);
extern int dt_post_open_setup(dinfo_t *dip);
extern int do_datatest_initialize(dinfo_t *dip);
extern int do_filesystem_setup(dinfo_t *dip);
extern large_t do_free_space_wait(dinfo_t *dip, int retries);

extern int initialize_prefix(dinfo_t *dip);
extern void initialize_pattern(dinfo_t *dip);
extern void setup_random_seeds(dinfo_t *dip);
extern void do_prepass_processing(dinfo_t *dip);
extern int do_postwrite_processing(dinfo_t *dip);
extern int do_deleteperpass(dinfo_t *dip);
extern int do_delete_files(dinfo_t *dip);
extern int reopen_output_file(dinfo_t *dip);

extern int format_device_name(dinfo_t *dip, char *format);
extern char *make_options_string(dinfo_t *dip, int argc, char **argv, hbool_t quoting);
extern int setup_log_directory(dinfo_t *dip, char *path, char *log);
extern int setup_thread_names(dinfo_t *dip);
extern void handle_file_dispose(dinfo_t *dip);
extern int handle_file_system_full(dinfo_t *dip, hbool_t delete_flag);
extern int HandleExit(dinfo_t *dip, int status);
extern void log_header(dinfo_t *dip, hbool_t error_flag);
extern void keepalive_alarm(dinfo_t *dip);
extern void handle_thread_exit(dinfo_t *dip);
extern void finish_test_common(dinfo_t *dip, int thread_status);

extern void terminate(dinfo_t *dip, int exit_code);
extern int nofunc(struct dinfo *dip);
extern int HandleMultiVolume(struct dinfo *dip);
extern int RequestFirstVolume(struct dinfo *dip, int oflags);
extern int RequestMultiVolume(struct dinfo *dip, hbool_t reopen, int open_flags);
extern hbool_t match(char **sptr, char *s);
extern unsigned long number(dinfo_t *dip, char *str, int base, int *status, hbool_t report_error);
extern large_t large_number(dinfo_t *dip, char *str, int base, int *status, hbool_t report_error);
extern time_t time_value(dinfo_t *dip, char *str);
extern void cleanup_device(dinfo_t *dip, hbool_t master);
extern dinfo_t *clone_device(dinfo_t *cdip, hbool_t master, hbool_t new_context);

/* dtaio.c */

#if defined(AIO)

extern int dtaio_close_file(struct dinfo *dip);
extern int dtaio_initialize(struct dinfo *dip);
extern int dtaio_cancel(struct dinfo *dip);
extern int dtaio_cancel_reads(struct dinfo *dip);
extern int dtaio_read_data(struct dinfo *dip);
extern int dtaio_write_data(struct dinfo *dip);
extern void dtaio_free_buffers(dinfo_t *dip);

#endif /* defined(AIO) */

/* dtbtag.c */
extern btag_t *initialize_btag(dinfo_t *dip, uint8_t opaque_type);
extern void report_btag(dinfo_t *dip, btag_t *ebtag, btag_t *rbtag, hbool_t raw_flag);
extern void update_btag(dinfo_t *dip, btag_t *btag, Offset_t offset, 
			uint32_t record_index, size_t record_size, uint32_t record_number);
extern void update_buffer_btags(dinfo_t *dip, btag_t *btag, Offset_t offset,
				void *buffer, size_t record_size, uint32_t record_number);
extern void update_record_btag(dinfo_t *dip, btag_t *btag, Offset_t offset,
			       uint32_t record_index, size_t record_size, uint32_t record_number);
extern int verify_btags(dinfo_t *dip, btag_t *ebtag, btag_t *rbtag, uint32_t *eindex, hbool_t raw_flag);
extern uint32_t	calculate_btag_crc(dinfo_t *dip, btag_t *btag);
extern int verify_btag_crc(dinfo_t *dip, btag_t *btag, uint32_t *rcrc, hbool_t errors);
extern int verify_buffer_btags(dinfo_t *dip, void *buffer, size_t record_size, btag_t **error_btag);

extern void btag_internal_test(dinfo_t *dip);
extern uint32_t	crc32(uint32_t crc, void *buffer, unsigned int length);
extern int parse_btag_verify_flags(dinfo_t *dip, char *string);
extern void show_btag_verify_flags(dinfo_t *dip);
extern void show_btag_verify_flags_set(dinfo_t *dip, uint32_t verify_flags);
extern int verify_btag_options(dinfo_t *dip);

/* dtfs.c */
extern hbool_t isFsFullOk(struct dinfo *dip, char *op, char *path);
extern char *make_dir_filename(struct dinfo *dip, char *dirpath);
extern char *make_file_name(struct dinfo *dip);
extern int end_file_processing(struct dinfo *dip);
extern int do_post_eof_processing(dinfo_t *dip);
extern int process_next_dir(struct dinfo *dip);
extern int process_next_file(struct dinfo *dip);
extern int process_next_subdir(struct dinfo *dip);
extern int create_directory(struct dinfo *dip, char *dir);
extern int remove_current_directory(struct dinfo *dip);
extern int remove_directory(struct dinfo *dip, char *dir);
extern int setup_directory_info(struct dinfo *dip);
extern int dt_delete_file(struct dinfo *dip, char *file, hbool_t errors);
extern int delete_files(struct dinfo *dip, hbool_t delete_topdir);
extern int delete_subdir_files(struct dinfo *dip, char *spath);
extern hbool_t dt_file_exists(dinfo_t *dip, char *file);
extern large_t dt_get_file_size(struct dinfo *dip, char *file, HANDLE *fd, hbool_t errors);
extern hbool_t dt_isdir(struct dinfo *dip, char *path, hbool_t errors);
extern hbool_t dt_isfile(struct dinfo *dip, char *path, hbool_t errors);
extern int dt_close_file(dinfo_t *dip, char *file, HANDLE *handle,
			 hbool_t *isDiskFull, hbool_t errors, hbool_t retrys);
extern int dt_create_directory(dinfo_t *dip, char *dir,	hbool_t *isDiskFull, hbool_t *isFileExists, hbool_t errors);
extern int dt_extend_file(dinfo_t *dip, char *file, HANDLE handle,
			  void *buffer, size_t write_size, large_t data_limit, hbool_t errors);
extern int dt_flush_file(dinfo_t *dip, char *file, HANDLE *handle, hbool_t *isDiskFull, hbool_t errors);
extern int dt_lock_file(dinfo_t *dip, char *file, HANDLE *handle,
			Offset_t start, Offset_t length, int type,
			hbool_t exclusive, hbool_t immediate, hbool_t errors);
extern int dt_unlock_file(dinfo_t *dip, char *file, HANDLE *handle,
			  Offset_t start, Offset_t length, hbool_t errors);
extern int dt_lock_unlock(dinfo_t *dip, char *file, HANDLE *fd,
			  lock_type_t lock_type, Offset_t offset, Offset_t length);

extern int dt_rename_file(dinfo_t *dip, char *oldpath, char *newpath, hbool_t *isDiskFull, hbool_t errors);
extern ssize_t dt_read_file(dinfo_t *dip, char *file, HANDLE *handle,
			    void *buffer, size_t bytes,	hbool_t errors, hbool_t retrys);
extern ssize_t dt_write_file(dinfo_t *dip, char *file, HANDLE *handle,
			     void *buffer, size_t bytes,
			     hbool_t *isDiskFull, hbool_t errors, hbool_t retrys);
extern int dt_truncate_file(struct dinfo *dip, char *file, Offset_t offset,
			    hbool_t *isDiskFull, hbool_t errors);
extern int dt_ftruncate_file(struct dinfo *dip, char *file, HANDLE fd, Offset_t offset,
			     hbool_t *isDiskFull, hbool_t errors);
extern int reopen_after_disconnect(dinfo_t *dip, error_info_t *eip);
extern char *make_unique_file(dinfo_t *dip, char *file);
extern void make_unique_log(dinfo_t *dip);
extern char *skip_device_prefix(char *device);
extern int do_file_trim(dinfo_t *dip);
extern int get_transfer_limits(dinfo_t *dip, uint64_t *data_bytes, Offset_t *offset);
extern large_t calculate_max_data(dinfo_t *dip);
extern large_t calculate_max_files(dinfo_t *dip);
extern hbool_t report_filesystem_free_space(dinfo_t *dip);
extern int verify_filesystem_space(dinfo_t *dip, hbool_t all_threads_flag);
extern hbool_t is_fsfull_restartable(dinfo_t *dip);
extern hbool_t restart_on_file_system_full(dinfo_t *dip);
extern hbool_t	is_modulo_device_size_io(dinfo_t *dip);
extern hbool_t is_unbuffered_mode(dinfo_t *dip);
extern void set_unbuffered_mode(dinfo_t *dip);

/* dtfmt.c */
extern size_t FmtKeepAlive(struct dinfo *dip, char *keepalivefmt, char *buffer);
extern int FmtPrefix(struct dinfo *dip, char *prefix, int psize);
extern char *FmtString(dinfo_t *dip, char *format, hbool_t filepath_flag);
/* Consolidated into one function now. */
#define FmtLogFile	FmtString
#define FmtFilePath	FmtString
#define FmtLogPrefix	FmtString

/* dtgen.c */
extern void init_open_defaults(dinfo_t *dip);
extern int open_file(struct dinfo *dip, int mode);
extern int close_file(struct dinfo *dip);
extern int reopen_file(struct dinfo *dip, int mode);
extern int initialize(struct dinfo *dip);
extern int init_file(struct dinfo *dip);
extern int flush_file(struct dinfo *dip);
extern int read_file(struct dinfo *dip);
extern int write_file(struct dinfo *dip);
extern int validate_opts(struct dinfo *dip);
extern void SetupBufferingMode(dinfo_t *dip, int *oflags);
extern hbool_t isDirectIO(dinfo_t *dip);

/* dthist.c */
extern void FreeHistoryData(dinfo_t *dip);
extern void SetupHistoryData(dinfo_t *dip);
extern void dump_history_data(struct dinfo *dip);
extern void save_history_data(	struct dinfo	*dip,
				u_long		file_number,
				u_long		record_number,
				test_mode_t	test_mode,
				Offset_t	offset,
				void		*buffer,
				size_t		rsize,
				ssize_t		tsize);

/* dtinfo.c */
extern struct dtype *setup_device_type(char *str);
extern int setup_device_info(struct dinfo *dip, char *dname, struct dtype *dtp);
//extern void system_device_info(struct dinfo *dip);
#if defined(__linux__)
extern void os_get_block_size(dinfo_t *dip, int fd, char *device_name);
#endif /* defined(__linux__) */

/* dtiot.c */
extern u_int32 init_iotdata(	dinfo_t		*dip,
                                u_char		*buffer,
                                size_t		bcount,
                                u_int32		lba,
                                u_int32		lbsize );
extern void process_iot_data(dinfo_t *dip, u_char *pbuffer, u_char *vbuffer, size_t bcount, hbool_t raw_flag);
extern void analyze_iot_data(dinfo_t *dip, u_char *pbuffer, u_char *vbuffer, size_t bcount, hbool_t raw_flag);
extern void display_iot_data(dinfo_t *dip, u_char *pbuffer, u_char *vbuffer, size_t bcount, hbool_t raw_flag);
extern void display_iot_block(dinfo_t *dip, int block, Offset_t record_index, u_char *pptr, u_char *vptr,
			      uint32_t vindex, size_t bsize, hbool_t good_data, hbool_t raw_flag);
extern void report_bad_sequence(dinfo_t *dip, int start, int length, Offset_t offset);
extern void report_good_sequence(dinfo_t *dip, int start, int length, Offset_t offset);

/* dtjobs.c */
extern job_info_t *jobs;
extern int initialize_jobs_data(dinfo_t *dip);
extern int acquire_jobs_lock(dinfo_t *dip);
extern int release_jobs_lock(dinfo_t *dip);
extern int acquire_job_lock(dinfo_t *dip, job_info_t *job);
extern int release_job_lock(dinfo_t *dip, job_info_t *job);
extern int acquire_job_print_lock(dinfo_t *dip, job_info_t *job);
extern int release_job_print_lock(dinfo_t *dip, job_info_t *job);
extern int acquire_job_thread_lock(dinfo_t *dip, job_info_t *job);
extern int release_job_thread_lock(dinfo_t *dip, job_info_t *job);
extern int dt_acquire_iolock(dinfo_t *dip, io_global_data_t *iogp);
extern int dt_release_iolock(dinfo_t *dip, io_global_data_t *iogp);
extern job_info_t *find_job_by_id(dinfo_t *dip, uint32_t job_id, hbool_t lock_jobs);
extern job_info_t *find_job_by_tag(dinfo_t *dip, char *tag, hbool_t lock_jobs);
extern job_info_t *find_jobs_by_tag(dinfo_t *dip, char *tag, job_info_t *pjob, hbool_t lock_jobs);
extern job_info_t *create_job(dinfo_t *dip);
extern int insert_job(dinfo_t *dip, job_info_t *job);
extern int remove_job(dinfo_t *mdip, job_info_t *job, hbool_t lock_jobs);
extern int remove_job_by_id(dinfo_t *mdip, job_id_t job_id);
extern int set_job_state(dinfo_t *dip, job_info_t *job, jstate_t job_state, hbool_t lock_jobs);
extern int get_threads_state_count(threads_info_t *tip, tstate_t thread_state);
#if defined(INLINE_FUNCS)
INLINE int
job_threads_starting(job_info_t *job)
{
    return ( get_threads_state_count(job->ji_tinfo, TS_STARTING) );
}
#else /* !defined(INLINE_FUNCS) */
extern int job_threads_starting(job_info_t *job);
#endif /* defined(INLINE_FUNCS) */
extern int set_threads_state(threads_info_t *tip, tstate_t thread_state);
extern int jobs_active(dinfo_t *dip);
extern int jobs_paused(dinfo_t *dip);
extern int jobs_finished(dinfo_t *dip);
extern int threads_starting(dinfo_t *dip);

extern int pause_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag);
extern int pause_job_by_id(dinfo_t *dip, job_id_t job_id);
extern int pause_job_by_tag(dinfo_t *dip, char *job_tag);
extern int pause_jobs_by_tag(dinfo_t *dip, char *job_tag);
extern int resume_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag);
extern int resume_job_by_id(dinfo_t *dip, job_id_t job_id);
extern int resume_job_by_tag(dinfo_t *dip, char *job_tag);
extern int resume_jobs_by_tag(dinfo_t *dip, char *job_tag);
extern int resume_job_thread(dinfo_t *dip, job_info_t *job);
extern int show_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag, hbool_t verbose);
extern int show_job_by_id(dinfo_t *dip, job_id_t job_id);
extern int show_job_by_tag(dinfo_t *dip, char *job_tag);
extern int show_jobs_by_tag(dinfo_t *dip, char *job_tag);
extern void show_job_info(dinfo_t *dip, job_info_t *job, hbool_t show_threads_flag);
extern void show_threads_info(dinfo_t *mdip, threads_info_t *tip);
extern int cancel_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag);
extern int cancel_job_by_id(dinfo_t *dip, job_id_t job_id);
extern int cancel_job_by_tag(dinfo_t *dip, char *job_tag);
extern int cancel_jobs_by_tag(dinfo_t *dip, char *job_tag);
extern int cancel_job_threads(dinfo_t *mdip, threads_info_t *tip);
extern int cancel_thread_threads(dinfo_t *mdip, dinfo_t *dip);
extern int modify_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag, char *modify_string);
extern int modify_job_by_id(dinfo_t *dip, job_id_t job_id, modify_params_t *modp);
extern int modify_job_by_tag(dinfo_t *dip, char *job_tag, modify_params_t *modp);
extern int modify_jobs_by_tag(dinfo_t *dip, char *job_tag, modify_params_t *modp);
extern int parse_modify_parameters(dinfo_t *dip, char *buffer, modify_params_t *modp);
extern int parse_enable_disable(dinfo_t *dip, char *token, hbool_t bool_value, modify_params_t *modp);
extern void set_thread_parameters(threads_info_t *tip, modify_params_t *modp);
extern void set_modify_parameters(dinfo_t *dip, modify_params_t *modp);
extern int query_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag, char *query_string);
extern int query_job_by_id(dinfo_t *dip, job_id_t job_id, char *query_string);
extern int query_job_by_tag(dinfo_t *dip, char *job_tag, char *query_string);
extern int query_jobs_by_tag(dinfo_t *dip, char *job_tag, char *query_string);
extern void query_threads_info(dinfo_t *mdip, threads_info_t *tip, char *query_string);
extern int stop_job(dinfo_t *dip, job_info_t *job);
extern int stop_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag);
extern int stop_job_by_id(dinfo_t *dip, job_id_t job_id);
extern int stop_job_by_tag(dinfo_t *dip, char *job_tag);
extern int stop_jobs_by_tag(dinfo_t *dip, char *job_tag);
extern int wait_for_job(dinfo_t *dip, job_info_t *job);
extern int wait_for_job_by_id(dinfo_t *dip, job_id_t job_id);
extern int wait_for_job_by_tag(dinfo_t *dip, char *tag);
extern int wait_for_jobs_by_tag(dinfo_t *dip, char *job_tag);
extern int wait_for_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag);
extern int wait_for_threads(dinfo_t *mdip, threads_info_t *tip);
extern void wait_for_threads_done(dinfo_t *dip);
extern void *a_job(void *arg);
extern int execute_threads(dinfo_t *mdip, dinfo_t **initial_dip, job_id_t *job_id);

/* dtmem.c */
extern void report_nomem(dinfo_t *dip, size_t bytes);

/* For memory tracing, need these inline! */
/* But that said, I'm leaving them for better performance! */

#if defined(INLINE_FUNCS)

#define Free(dip,ptr)		free(ptr)
#define FreeMem(dip,ptr,size)	\
  if (ptr) { memset(ptr, 0xdd, size); free(ptr); }
#define FreeStr(dip,ptr)	\
  if (ptr) { memset(ptr, 0xdd, strlen(ptr)); free(ptr); }

INLINE void* Malloc(dinfo_t *dip, size_t bytes)
{
    void *ptr = malloc(bytes);
    if (ptr) {
	memset(ptr, '\0', bytes);
    } else {
	report_nomem(dip, bytes);
    }
    return(ptr);
}

INLINE void *
Realloc(dinfo_t *dip, void *ptr, size_t bytes)
{
    ptr = realloc(ptr, bytes);
    if (ptr) {
	memset(ptr, '\0', bytes);
    } else {
	report_nomem(dip, bytes);
    }
    return(ptr);
}
/* Older compilers require code inline as above! */
//extern __inline void *Malloc(dinfo_t *dip, size_t bytes);
//extern __inline void *Realloc(dinfo_t *dip, void *ptr, size_t bytes);

#else /* !defined(INLINE_FUNCS) */

extern void Free(dinfo_t *dip, void *ptr);
extern void FreeMem(dinfo_t *dip, void *ptr, size_t size);
extern void FreeStr(dinfo_t *dip, void *ptr);
extern void *Malloc(dinfo_t *dip, size_t bytes);
extern void *Realloc(dinfo_t *dip, void *bp, size_t bytes);

#endif /* defined(INLINE_FUNCS) */

extern void *malloc_palign(dinfo_t *dip, size_t bytes, int offset);
extern void free_palign(dinfo_t *dip, void *pa_addr);

/* dtmmap.c */

#if defined(MMAP)

extern int mmap_file(struct dinfo *dip);
extern int mmap_flush(struct dinfo *dip);
extern int mmap_reopen_file(struct dinfo *dip, int mode);
extern int mmap_validate_opts(struct dinfo *dip);
extern int mmap_read_data(struct dinfo *dip);
extern int mmap_write_data(struct dinfo *dip);

#endif /* defined(MMAP) */

/* dtmtrand64.c */
extern void init_genrand64(dinfo_t *dip, unsigned long long seed);
extern void init_by_array64(dinfo_t *dip, unsigned long long init_key[], unsigned long long key_length);
extern unsigned long long genrand64_int64(dinfo_t *dip);
extern long long genrand64_int63(dinfo_t *dip);
extern double genrand64_real1(dinfo_t *dip);
extern double genrand64_real2(dinfo_t *dip);
extern double genrand64_real3(dinfo_t *dip);

/* dtprocs.c */
extern void abort_procs(dinfo_t *dip);
extern void await_procs(dinfo_t *dip);
extern pid_t fork_process(dinfo_t *dip);
extern pid_t start_devs(dinfo_t *dip);
extern pid_t start_procs(dinfo_t *dip);
extern pid_t start_slices(dinfo_t *dip);
extern int init_slice(struct dinfo *dip, int slice);

/* dtread.c */
extern int check_last_write_info(dinfo_t *dip, Offset_t offset, size_t bsize, size_t dsize);
extern int read_data(struct dinfo *dip);
extern int check_read(struct dinfo *dip, ssize_t count, size_t size);
extern int read_eof(struct dinfo *dip);
extern int read_eom(struct dinfo *dip);
extern ssize_t read_record(	struct dinfo	*dip,
				u_char		*buffer,
				size_t		bsize,
				size_t		dsize,
				Offset_t	offset,
				int		*status );
extern ssize_t verify_record(	struct dinfo	*dip,
				u_char		*buffer,
				size_t		bsize,
				Offset_t	offset,
				int		*status );
extern int FindCapacity(struct dinfo *dip);
extern void SetupTransferLimits(dinfo_t *dip, large_t bytes);

/* dtscsi.c */
#if defined(SCSI)

extern void clone_scsi_info(dinfo_t *dip, dinfo_t *cdip);
extern void free_scsi_info(dinfo_t *dip, scsi_generic_t **sgpp, scsi_generic_t **sgpiop);
extern int init_sg_info(dinfo_t *dip, char *scsi_dsf, scsi_generic_t **sgpp, scsi_generic_t **sgpiop);
extern int init_scsi_info(dinfo_t *dip, char *scsi_dsf, scsi_generic_t **sgpp, scsi_generic_t **sgpiop);
extern int init_scsi_trigger(dinfo_t *dip, char *scsi_dsf, scsi_generic_t **sgpp);
extern void report_scsi_information(dinfo_t *dip);
extern void report_standard_scsi_information(dinfo_t *dip);
extern int get_lba_status(dinfo_t *dip, Offset_t starting_offset, large_t data_bytes);
extern int do_unmap_blocks(dinfo_t *dip);
extern int unmap_blocks(dinfo_t *dip, Offset_t starting_offset, large_t data_bytes);
extern int write_same_unmap(dinfo_t *dip, Offset_t starting_offset, large_t data_bytes);
extern int xcopy_zerorod(dinfo_t *dip, Offset_t starting_offset, large_t data_bytes);
extern int do_scsi_triage(dinfo_t *dip);
extern ssize_t scsiReadData(dinfo_t *dip, void *buffer, size_t bytes, Offset_t offset);
extern ssize_t scsiWriteData(dinfo_t *dip, void *buffer, size_t bytes, Offset_t offset);
extern void dtReportScsiError(dinfo_t *dip, scsi_generic_t *sgp);
extern int get_standard_scsi_information(dinfo_t *dip, scsi_generic_t *sgp);
extern void strip_trailing_spaces(char *bp);

/* Note: Without NVME support, stubs exist for these functions. */
extern void report_standard_nvme_information(dinfo_t *dip);
extern ssize_t nvmeReadData(dinfo_t *dip, void *buffer, size_t bytes, Offset_t offset);
extern ssize_t nvmeWriteData(dinfo_t *dip, void *buffer, size_t bytes, Offset_t offset);

/* dtnvme.c */
#if defined(NVME) && defined(__linux__)
extern int init_nvme_info(dinfo_t *dip, char *dsf);
extern int get_nvme_id_ctrl(dinfo_t *dip, int fd);
extern int get_nvme_namespace(dinfo_t *dip, int fd);
extern int do_nvme_write_zeroes(dinfo_t *dip);
extern void dt_nvme_show_status(dinfo_t *dip, char *op, int status);
#endif /* defined(NVME) && defined(__linux__) */

#else /* !defined(SCSI) */

#define scsiReadData(dip, buffer, bytes, offset)	0
#define scsiWriteData(dip, buffer, bytes, offset)	0

#endif /* defined(SCSI) */

/* dtstats.c */
extern void accumulate_stats(dinfo_t *dip);
extern void gather_stats(struct dinfo *dip);
extern void gather_totals(struct dinfo *dip);
extern void init_stats(struct dinfo *dip);
extern void report_pass(struct dinfo *dip, enum stats stats_type);
extern void report_stats(struct dinfo *dip, enum stats stats_type);
extern void report_file_system_information(dinfo_t *dip, hbool_t print_header, hbool_t acquire_free_space);
extern void report_os_information(dinfo_t *dip, hbool_t print_header);
extern void report_scsi_summary(dinfo_t *dip, hbool_t print_header);
extern void dt_job_finish(dinfo_t *dip, job_info_t *job);
extern void gather_thread_stats(dinfo_t *dip, dinfo_t *tdip);
extern void display_extra_sizes(dinfo_t *dip, char *text, uint64_t blocks, uint32_t block_size);
extern void display_long_double(dinfo_t *dip, char *text, long double bytes);

/* dtwrite.c */
extern int prefill_file(dinfo_t *dip, size_t block_size, large_t data_limit, Offset_t starting_offset);
extern int write_data(struct dinfo *dip);
extern int check_write(struct dinfo *dip, ssize_t count, size_t size, Offset_t offset);
extern ssize_t copy_record(	struct dinfo	*dip,
				u_char		*buffer,
				size_t		bsize,
				Offset_t	offset,
				int		*status );
extern ssize_t write_record(	struct dinfo	*dip,
				u_char		*buffer,
				size_t		bsize,
				size_t		dsize,
				Offset_t	offset,
				int		*status );
extern int write_verify(	struct dinfo	*dip,
				u_char		*buffer,
				size_t		bsize,
				size_t		dsize,
				Offset_t	offset );

/* dtutil.c */
extern int acquire_print_lock(void);
extern int release_print_lock(void);
extern void mySleep(dinfo_t *dip, unsigned int sleep_time);
extern void SleepSecs(dinfo_t *dip, unsigned int sleep_time);
extern uint64_t	timer_diff(struct timeval *start, struct timeval *end);
extern uint64_t	timer_diffsecs(struct timeval *start, struct timeval *end);
extern uint64_t	timer_now(struct timeval *timer);

extern size_t copy_prefix( dinfo_t *dip, u_char *buffer, size_t bcount );
extern void fill_buffer(	dinfo_t		*dip,
				void		*buffer,
				size_t		byte_count,
				u_int32		pattern);
extern void init_buffer(	dinfo_t		*dip,
				void		*buffer,
				size_t		count,
				u_int32		pattern );
extern void corrupt_buffer( dinfo_t *dip, void *buffer, int32_t length, uint64_t record);
extern void poison_buffer(      dinfo_t     *dip,
                                void        *buffer,
                                size_t      count,
                                u_int32     pattern );
#if _BIG_ENDIAN_
extern void init_swapped (	dinfo_t		*dip,
				void		*buffer,
		                size_t		count,
		                u_int32		pattern );
#endif /* _BIG_ENDIAN_ */

extern u_int32 init_lbdata(	struct dinfo	*dip,
				void		*buffer,
				size_t		count,
				u_int32		lba,
				u_int32		lbsize );
#if defined(TIMESTAMP)
extern void init_timestamp (	dinfo_t		*dip,
				void		*buffer,
                        	size_t		count,
                        	u_int32		dsize );
extern void display_timestamp(dinfo_t *dip, u_char *buffer);
#endif /* defined(TIMESTAMP) */

#if !defined(INLINE_FUNCS)

extern u_int32 make_lba( struct dinfo *dip, Offset_t pos );
extern Offset_t make_offset( struct dinfo *dip, u_int32 lba);
extern u_int32 make_lbdata( struct dinfo *dip, Offset_t pos );

#endif /* defined(INLINE_FUNCS) */

extern Offset_t	get_current_offset(dinfo_t *dip, ssize_t count);
extern Offset_t	get_updated_offset(dinfo_t *dip, ssize_t count);
extern hbool_t is_correct_offset(dinfo_t *dip, Offset_t record_offset, ssize_t count, Offset_t *updated_offset);

extern Offset_t get_position(struct dinfo *dip);
extern Offset_t	dt_get_position(dinfo_t *dip, char *file, HANDLE *fdp, hbool_t errors, hbool_t retrys);

extern u_int32 winit_lbdata(	struct dinfo	*dip,
				Offset_t	pos,
				u_char		*buffer,
				size_t		count,
				u_int32		lba,
				u_int32		lbsize );
extern void init_padbytes(	u_char		*buffer,
				size_t		offset,
				u_int32		pattern);

extern char *bformat_time(char *bp, clock_t ticks);
extern void print_time(dinfo_t *dip, FILE *fp, clock_t ticks);
extern void format_time(dinfo_t *dip, clock_t ticks);
extern int FormatElapstedTime(char *buffer, clock_t ticks);

extern int process_pfile(dinfo_t *dip, char *file);
extern void process_iotune(dinfo_t *dip, char *file);
extern void copy_pattern(u_int32 pattern, u_char *buffer);
extern void reset_pattern(dinfo_t *dip);
extern void setup_pattern(dinfo_t *dip, u_char *buffer, size_t size, hbool_t init_pattern);
extern Offset_t seek_file(dinfo_t *dip, HANDLE fd, u_long records, Offset_t size, int whence);
extern Offset_t seek_position(struct dinfo *dip, Offset_t size, int whence, hbool_t expect_error);
extern Offset_t	dt_seek_position(dinfo_t *dip, char *file, HANDLE *fdp, Offset_t offset,
				 int whence, hbool_t errors, hbool_t retrys);
extern u_int32 get_lba(struct dinfo *dip);
extern Offset_t incr_position(struct dinfo *dip, Offset_t offset, hbool_t expect_error);
extern Offset_t set_position(struct dinfo *dip, Offset_t offset, hbool_t expect_error);
extern Offset_t	dt_set_position(struct dinfo *dip, char *file, HANDLE *fdp,
				Offset_t offset, hbool_t errors, hbool_t retrys);
#if !defined(INLINE_FUNCS)
extern Offset_t make_position(struct dinfo *dip, u_int32 lba);
#endif /* defined(INLINE_FUNCS) */
extern void show_position(struct dinfo *dip, Offset_t pos);

#if defined(INLINE_FUNCS)

INLINE uint32_t get_random(dinfo_t *dip)
{
    return ( (uint32_t)genrand64_int64(dip) );
}

INLINE large_t
get_random64(dinfo_t *dip)
{
    return ( genrand64_int64(dip) );
}

INLINE void
set_rseed(dinfo_t *dip, uint64_t seed)
{
    init_genrand64(dip, seed);
}

/* lower <= rnd(lower,upper) <= upper */
INLINE int32_t
rnd(dinfo_t *dip, int32_t lower, int32_t upper)
{
    return( lower + (int32_t)( ((double)(upper - lower + 1) * get_random(dip)) / (UINT32_MAX + 1.0)) );
}

/* lower <= rnd(lower,upper) <= upper */
INLINE int64_t
rnd64(dinfo_t *dip, int64_t lower, int64_t upper)
{
    return( lower + (int64_t)( ((double)(upper - lower + 1) * genrand64_int64(dip)) / (UINT64_MAX + 1.0)) );
}

INLINE hbool_t
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

INLINE hbool_t
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

#else /* !defined(INLINE_FUNCS) */

extern uint32_t get_random(dinfo_t *dip);
extern large_t get_random64(dinfo_t *dip);
extern void set_rseed(dinfo_t *dip, uint64_t seed);
extern int32_t rnd(dinfo_t *dip, int32_t lower, int32_t upper);
extern int64_t rnd64(dinfo_t *dip, int64_t lower, int64_t upper);
extern hbool_t dt_test_lock_mode(dinfo_t *dip, int lock_mode);
extern hbool_t dt_unlock_file_chance(dinfo_t *dip);

#endif /* defined(INLINE_FUNCS) */

extern size_t get_data_size(dinfo_t *dip, optype_t optype);
extern size_t get_variable(struct dinfo *dip);
extern large_t get_data_limit(dinfo_t *dip);
extern large_t get_variable_limit(dinfo_t *dip);
extern Offset_t do_random(struct dinfo *dip, hbool_t doseek, size_t xfer_size);
extern int skip_records(struct dinfo *dip, u_long records, u_char *buffer, size_t size);
extern unsigned long CvtStrtoValue(dinfo_t *dip, char *nstr, char **eptr, int base);
extern large_t CvtStrtoLarge(dinfo_t *dip, char *nstr, char **eptr, int base);
extern time_t CvtTimetoValue(char *nstr, char **eptr);
extern void Ctime(dinfo_t *dip, time_t timer);
extern int Fputs(char *str, FILE *stream);
extern int is_Eof(struct dinfo *dip, ssize_t count, size_t size, int *status);
extern void set_Eof(struct dinfo *dip);

extern int IS_HexString(char *s);
extern int StrCopy(void *to_buffer, void *from_buffer, size_t length);
extern large_t stoh(u_char *bp, size_t size);
extern void htos(u_char *bp, large_t value, size_t size);
extern void LogDiagMsg(dinfo_t *dip, char *msg);
extern trigger_control_t parse_trigger_control(dinfo_t *dip, char *control);
extern int add_trigger_type(dinfo_t *dip, char *trigger);
extern int add_default_triggers(dinfo_t *dip);
extern void remove_triggers(dinfo_t *dip);
extern hbool_t trigger_type_exist(dinfo_t *dip, trigger_type_t trigger_type);
extern trigger_type_t parse_trigger_type(dinfo_t *dip, char *trigger);
extern int DoSystemCommand(dinfo_t *dip, char *cmdline);
extern int StartupShell(dinfo_t *dip, char *shell);
extern int ExecuteCommand(dinfo_t *dip, char *cmd, hbool_t prefix, hbool_t verbose);
extern int ExecuteBuffered(dinfo_t *dip, char *cmd, char *buffer, int bufsize);
extern int ExecutePassCmd(dinfo_t *dip);
extern int ExecuteTrigger(struct dinfo *dip, ...);
extern int get_cpu_utilization(dinfo_t *dip);
extern large_t GetStatsValue(struct dinfo *dip, stats_value_t stv, hbool_t pass_stats, int *secs);

/*
 * Script Functions:
 */ 
extern void DisplayScriptInformation(dinfo_t *dip);
extern void CloseScriptFile(dinfo_t *dip);
extern void CloseScriptFiles(dinfo_t *dip);
extern int OpenScriptFile(dinfo_t *dip, char *file);

/*
 * Generic Open/Close Functions:
 */
extern void CloseFile(dinfo_t *dip, FILE **fp);
extern int OpenInputFile(dinfo_t *dip, FILE **fp, char *file, char *mode, hbool_t errors);
extern int OpenOutputFile(dinfo_t *dip, FILE **fp, char *file, char *mode, hbool_t errors);

/*
 * Strings used by Common Printing Functions (PrintAscii(), etc).
 * 
 * Note: The field width is shorter than scu/spt, so may need to adjust.
 */
#define MSG_FIELD_WIDTH	"%30.30s: "
#define ASCII_FIELD	MSG_FIELD_WIDTH"%s"
#define EMPTY_FIELD	"%32.32s%s"
#define NUMERIC_FIELD	MSG_FIELD_WIDTH"%u"
#define DEC_HEX_FIELD	MSG_FIELD_WIDTH"%u (%#lx)"
#define HEX_FIELD	MSG_FIELD_WIDTH"%#x"
#define HEX_DEC_FIELD	MSG_FIELD_WIDTH"%#x (%u)"
#define FIELD_WIDTH	32		/* The field width (see above).	*/
#define DEFAULT_WIDTH   132		/* tty display width.   */

#define LNUMERIC_FIELD	MSG_FIELD_WIDTH LUF
#define LDEC_HEX_FIELD	MSG_FIELD_WIDTH LUF " (" LXF ")"
#define LHEX_FIELD	MSG_FIELD_WIDTH LXF
#define LHEX_DEC_FIELD	MSG_FIELD_WIDTH LXF " (" LUF ")"

/* The rest of dt uses this for its' field width. */
#define DT_FIELD_WIDTH	"%30.30s: "
#define DT_BTAG_FIELD	"%24.24s (%3u): "

#define DNL		0			/* Disable newline.	*/
#define PNL		1			/* Print newline.	*/

/* dtprint.c */
extern int DisplayWidth;
extern char *fmtmsg_prefix(dinfo_t *dip, char *bp, int flags, logLevel_t level);
extern int AcquirePrintLock(dinfo_t *dip);
extern int ReleasePrintLock(dinfo_t *dip);
extern int PrintLogs(dinfo_t *dip, logLevel_t level, int flags, FILE *fp, char *buffer);
extern char *get_message_type(int flags);
extern void LogMsg(dinfo_t *dip, FILE *fp, enum logLevel level, int flags, char *fmtstr, ...);
extern void SystemLog(dinfo_t *dip, int priority, char *format, ...);

extern void ReportError(dinfo_t *dip, error_info_t *eip);
extern int ReportRetryableError(dinfo_t *dip, error_info_t *eip, char *format, ...);
extern int ReportErrorInfoX(dinfo_t *dip, error_info_t *eip, char *format, ...);
extern int ReportExtendedErrorInfo(dinfo_t *dip, error_info_t *eip, char *format, ...);
/* Old version, until all references are updated! */
extern void ReportErrorInfo( dinfo_t   *dip,
			     char      *file,
			     int       error,
			     char      *error_info,
			     optype_t  optype,
			     hbool_t   record_error);
extern void ReportErrorNumber(dinfo_t *dip);
extern void report_io(dinfo_t *dip, test_mode_t io_mode, void *buffer, size_t bytes, Offset_t offset);
extern void report_record(struct dinfo		*dip,
			  u_long		files,
			  u_long		records,
			  large_t		lba,
			  Offset_t         	offset,
			  enum test_mode	mode,
			  void			*buffer,
			  size_t		bytes );
extern void RecordErrorTimes(struct dinfo *dip, hbool_t record_error);
extern void Eprintf(dinfo_t *dip, char *format, ...);
extern void Fprintf(dinfo_t *dip, char *format, ...);
extern void Fprint(dinfo_t *dip, char *format, ...);
extern void Fprintnl(dinfo_t *dip);
extern void Lprintf(dinfo_t *dip, char *format, ...);
extern void mPrintf(dinfo_t *dip, char *format, ...);
extern void mPrint(dinfo_t *dip, char *format, ...);
extern void mPrintnl(dinfo_t *dip);
extern void Printf(dinfo_t *dip, char *format, ...);
extern void Print(dinfo_t *dip, char *format, ...);
extern void Printnl(dinfo_t *dip);
extern void Wprintf(dinfo_t *dip, char *format, ...);
extern void Perror(dinfo_t *dip, char *format, ...);
extern void Lflush(dinfo_t *dip);
extern void eLflush(dinfo_t *dip);
extern int Sprintf(char *bufptr, char *msg, ...);
extern int vSprintf(char *bufptr, const char *msg, va_list ap);

extern void DumpFieldsOffset(dinfo_t *dip, uint8_t *bptr, int length);
extern void PrintFields(dinfo_t *dip, u_char *bptr, int length);
extern void PrintHAFields(dinfo_t *dip, unsigned char *bptr, int length);

extern void PrintLines(dinfo_t *dip, hbool_t error_flag, char *buffer);
extern void PrintHeader(dinfo_t *dip, char *header);
extern void PrintNumeric(dinfo_t *dip, char *field_str, uint32_t numeric_value, int nl_flag);
#define PrintDec PrintDecimal
extern void PrintDecimal(dinfo_t *dip, char *field_str, uint32_t numeric_value, int nl_flag);
extern void PrintDecHex(dinfo_t *dip, char *field_str, uint32_t numeric_value, int nl_flag);
extern void PrintHex(dinfo_t *dip, char *field_str, uint32_t numeric_value, int nl_flag);
extern void PrintHexDec(dinfo_t *dip, char *field_str, uint32_t numeric_value, int nl_flag);
extern void PrintAscii(dinfo_t *dip, char *field_str, char *ascii_str, int nl_flag);
extern void PrintLongLong(dinfo_t *dip, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongDec(dinfo_t *dip, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongDecHex(dinfo_t *dip, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongHex(dinfo_t *dip, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongHexDec(dinfo_t *dip, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintBoolean( dinfo_t	*dip,
			  hbool_t	numeric,
			  char		*field_str,
			  hbool_t	boolean_flag,
			  hbool_t	nl_flag);
extern void PrintEnDis( dinfo_t	*dip,
			hbool_t	numeric,
			char	*field_str,
			hbool_t	boolean_flag,
			hbool_t	nl_flag);
extern void PrintOnOff( dinfo_t	*dip,
			hbool_t	numeric,
			char	*field_str,
			hbool_t	boolean_flag,
			hbool_t	nl_flag);
extern void PrintYesNo( dinfo_t	*dip,
			hbool_t	numeric,
			char	*field_str,
			hbool_t	boolean_flag,
			hbool_t	nl_flag);

/*
 * POSIX does *not* define a special device interface, and since no
 * Magtape API exists, these functions are operating system dependent.
 */

/* dttape.c */

#if defined(TAPE)
# if defined(_QNX_SOURCE) || defined(SCO)
extern int DoIoctl(dinfo_t *dip, int cmd, int argp, caddr_t msgp);
# else /* !defined(_QNX_SOURCE) && !defined(SCO) */
extern int DoIoctl(dinfo_t *dip, int cmd, caddr_t argp, caddr_t msgp);
# endif /* defined(_QNX_SOURCE) || defined(SCO) */
extern int DoMtOp(dinfo_t *dip, short cmd, daddr_t count, caddr_t msgp);
extern int DoWriteFileMark(dinfo_t *dip, daddr_t count);
extern int DoForwardSpaceFile(dinfo_t *dip, daddr_t count);
extern int DoBackwardSpaceFile(dinfo_t *dip, daddr_t count);
extern int DoForwardSpaceRecord(dinfo_t *dip, daddr_t count);
extern int DoBackwardSpaceRecord(dinfo_t *dip, daddr_t count);
extern int DoRewindTape(dinfo_t *dip);
extern int DoTapeOffline(dinfo_t *dip);
extern int DoRetensionTape(dinfo_t *dip);

# if defined(__osf__)			/* Really DEC specific. */

extern int DoSpaceEndOfData(dinfo_t *dip);
extern int DoEraseTape(dinfo_t *dip);
extern int DoTapeOnline(dinfo_t *dip);
extern int DoLoadTape(dinfo_t *dip);
extern int DoUnloadTape(dinfo_t *dip);

# endif /* defined(__osf__) */

#endif /* defined(TAPE) */

/* dtusage.c */
extern char *version_str;
extern void dthelp(dinfo_t *dip);
extern void dtusage(dinfo_t *dip);
extern void dtversion(dinfo_t *dip);

#if defined(FIFO)

extern int fifo_open(struct dinfo *dip, int mode);

#endif /* defined(FIFO) */

/* dtverify.c */
extern void dump_buffer(	dinfo_t		*dip,
				char		*name,
				u_char		*base,
				u_char		*cptr,
				size_t		dump_size,
				size_t		bufr_size,
				hbool_t		expected);
extern void dump_buffer_legacy(	dinfo_t		*dip,
				char		*name,
				u_char		*base,
				u_char		*cptr,
				size_t		dump_size,
				size_t		bufr_size,
				hbool_t		expected);
extern void dump_expected_buffer(dinfo_t	*dip,
				char		*name,
				u_char		*base,
				u_char		*ptr,
				size_t		dump_size,
				size_t		bufr_size );
extern void dump_received_buffer(dinfo_t	*dip,
				char		*name,
				u_char		*base,
				u_char		*ptr,
				size_t		dump_size,
				size_t		bufr_size );
extern void dump_file_buffer(	dinfo_t		*dip,
                                char		*name,
                                uint8_t		*base,
                                uint8_t		*cptr,
                                size_t		dump_size,
                                size_t		bufr_size );
extern int verify_buffers(	struct dinfo	*dip,
				u_char		*dbuffer,
				u_char		*vbuffer,
				size_t		count );
extern int verify_lbdata(	struct dinfo	*dip,
				u_char		*dbuffer,
				u_char		*vbuffer,
				size_t		count,
				u_int32		*lba );
extern int verify_data(		struct dinfo	*dip,
				uint8_t		*buffer,
				size_t		byte_count,
				uint32_t	pattern,
				uint32_t	*lba,
				hbool_t		raw_flag );
extern int verify_reread(	struct dinfo	*cdip,
				u_char		*buffer,
				size_t		bcount,
				u_int32		pattern,
				u_int32		*lba );
extern int verify_padbytes(	struct dinfo	*dip,
				u_char		*buffer,
				size_t		count,
				u_int32		pattern,
				size_t		offset );
extern int verify_btag_prefix(dinfo_t *dip, btag_t *ebtag, btag_t *rbtag, uint32_t *eindex);
extern hbool_t is_retryable(dinfo_t *dip, int error_code);
extern hbool_t retry_operation(dinfo_t *dip, error_info_t *eip);
extern void ReportCompareError(	struct dinfo	*dip,
				size_t		byte_count,
				u_int		byte_position,
				u_int		expected_data,
				u_int		data_found );
extern void report_device_information(dinfo_t *dip);
extern void ReportDeviceInfo(	struct dinfo	*dip,
				size_t		byte_count,
				u_int		byte_position,
				hbool_t		eio_error,
				hbool_t		mismatch_flag );
extern void report_device_informationX(dinfo_t *dip);
extern void ReportDeviceInfoX(struct dinfo *dip, error_info_t *eip);
extern uint64_t MapOffsetToLBA(dinfo_t *dip, HANDLE fd, uint32_t dsize,
			       Offset_t offset, hbool_t mismatch_flag);
extern void set_device_info(dinfo_t *dip, size_t iosize, uint32_t buffer_index,
			    hbool_t eio_error, hbool_t mismatch_flag);
extern void ReportLbdataError(	struct dinfo	*dip,
			        u_int32		lba,
				u_int32		byte_count,
				u_int32		byte_position,
				u_int32		expected_data,
				u_int32		data_found );
extern void report_reread_data(dinfo_t *dip, hbool_t corruption_flag, char *reread_file);
extern int save_corrupted_data(dinfo_t *dip, char *filepath, void *buffer, size_t bufsize, corruption_type_t ctype);

/*
 * OS Specific Functions: (dtunix.c and dtwin.c)
 * Note: These must be here since they reference the dinfo structure!
 */
extern hbool_t FindMountDevice(dinfo_t *dip, char *path, hbool_t debug);
extern hbool_t isDeviceMounted(dinfo_t *dip, char *path, hbool_t debug);
extern char *os_getcwd(void);
extern hbool_t os_isdir(char *dirpath);
extern hbool_t os_isdisk(HANDLE handle);
extern os_ino_t os_get_fileID(char *path, HANDLE handle);
extern large_t os_get_file_size(char *path, HANDLE handle);
extern hbool_t os_file_exists(char *file);
extern int os_file_information(char *file, large_t *filesize, hbool_t *is_dir, hbool_t *is_file);
extern int os_get_fs_information(dinfo_t *dip, char *dir);
extern char *os_get_universal_name(char *drive_letter);
extern char *os_get_volume_path_name(char *path);
extern int os_get_volume_information(dinfo_t *dip);
extern char *os_ctime(time_t *timep, char *time_buffer, int timebuf_size);
extern int os_set_priority(dinfo_t *dip, HANDLE hThread, int priority);
extern void tPerror(dinfo_t *dip, int error, char *format, ...);
extern void os_perror(dinfo_t *dip, char *format, ...);
extern uint64_t	os_create_random_seed(void);
extern int os_file_trim(HANDLE handle, Offset_t offset, uint64_t length);
extern hbool_t os_is_session_disconnected(int error);
extern void os_set_disconnect_errors(dinfo_t *dip);
extern int os_set_thread_cancel_type(dinfo_t *dip, int cancel_type);
extern HANDLE dt_open_file(dinfo_t *dip, char *file, int flags, int perm,
			   hbool_t *isDiskFull, hbool_t *isDirectory,
			   hbool_t errors, hbool_t retrys);
extern char *os_get_uuid(hbool_t want_dashes);
extern char *os_getaddrinfo(dinfo_t *dip, char *host, int family, void **sa, socklen_t *salen);
extern char *os_getnameinfo(dinfo_t *dip, struct sockaddr *sa, socklen_t salen);
extern int os_set_lock_flags(lock_type_t lock_type, int *lock_type_flag,
			     hbool_t *exclusive, hbool_t *immediate, hbool_t *unlock_flag);

#if defined(MacDarwin) || defined(SOLARIS)
extern int os_DirectIO(struct dinfo *dip, char *file, hbool_t flag);
#endif /* defined(MacDarwin) || defined(SOLARIS) */
extern int os_VeritasDirectIO(struct dinfo *dip, char *file, hbool_t flag);

/* File Map API's */
extern void *os_get_file_map(dinfo_t *dip, HANDLE fd);
extern void os_free_file_map(dinfo_t *dip);
extern int os_report_file_map(dinfo_t *dip, HANDLE fd, uint32_t dsize, Offset_t offset, int64_t length);
extern uint64_t os_map_offset_to_lba(dinfo_t *dip, HANDLE fd, uint32_t dsize, Offset_t offset);

/* dtunix.c and dtwin.c */
extern void ReportOpenInformation(dinfo_t *dip, char *FileName, char *Operation,
				  uint32_t DesiredAccess,
				  uint32_t CreationDisposition, uint32_t FileAttributes,
				  uint32_t ShareMode, hbool_t error_flag);
#if defined(WIN32)

extern int dt_get_file_attributes(dinfo_t *dip, char *file, DWORD *FileAttributes);
extern void SetupWindowsFlags(struct dinfo *dip, char *file, int oflags, DWORD *CreationDisposition, DWORD *FileAttributes);
extern int HandleSparseFile(dinfo_t *dip, DWORD FileAttributes);
extern int SetSparseFile(dinfo_t *dip, HANDLE hDevice, BOOL is_overlapped);
extern int os_set_sparse_file(dinfo_t *dip, char *file, HANDLE hDevice, BOOL is_overlapped);
extern int PreAllocateFile(dinfo_t *dip, DWORD FileAttributes);

#endif /* defined(WIN32) */

/* dtworkloads.c */
extern void initialize_workloads_data(void);
extern void add_workload_entry(char *workload_name, char *workload_desc, char *workload_options);
extern workload_entry_t *find_workload(char *workload_name);
extern void show_workloads(dinfo_t *dip, char *workload_name);

/*
 * Other I/O Behaviors:
 */
extern void dtapp_set_iobehavior_funcs(dinfo_t *dip);
extern void hammer_set_iobehavior_funcs(dinfo_t *dip);
extern int hammer_map_options(dinfo_t *dip, int argc, char **argv);
extern void sio_set_iobehavior_funcs(dinfo_t *dip);
