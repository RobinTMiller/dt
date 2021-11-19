/*
 * Copyright 2021 NetApp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * dtsio.c - I/O Behavior for NetApp's sio tool.
 * 
 * Author: Robin T. Miller
 * Date Created: November 5th, 2013
 * 
 * Modification History:
 * 
 * November 9, 2021 by Chris Nelson (nelc@netapp.com)
 *  Add MIT license, in order to distribute to FOSS community so it can
 *  be used and maintained by a larger audience, particularly for the
 *  public dt at https://github.com/RobinTMiller/dt
 
 *
 * April 7th, 2016 by Robin T. Miller
 *      Fix regression in sio_thread() where the prefill flag was left uninitialized
 * which kept the file size check from being performed, which sets prefill as required.
 * 
 * December 12th, 2015 by Robin T. Miller
 *      When file does not exist or is smaller than the requested data limit
 * when there's a read percentage, prefill the file so reads have data to read.
 * Previously, files were being extended to the data limit, but read would never
 * reach the underlying storage, since metadata indicated nothing was written!
 * 
 * December 10th, 2015 by Robin T. Miller
 *      Updating help with examples.
 * 
 * June 9th, 2015 by Robin T. Miller
 * 	Fix "floating exception" fault, due to improperly setting the ending
 * block when the starting block was non-zero. Also added a sanity check on the
 * starting and ending blocks to ensure the ending block is greater than start.
 * 
 * June 4th, 2015 by Robin T. Miller
 * 	With multiple threads, when a starting offset is specified, ensure
 * the global current block gets initialized properly. Since this is done
 * in the job init section, the thread current block cannot get used since
 * this is setup after each threads starts up. Without this fix, the first
 * I/O request was going to the first block, not the intended block offset!
 *
 * May 13th, 2015 by Robin T. Miller
 * 	When reporting extended errors, use the current block size,
 * which is in bytes, rather than the I/O size (blocks and always 1).
 * 
 * March 31st, 2015 by Robin T. Miller
 * 	Added parsing for runtime= and threads= options, since sio
 * options can (optionally) start with '-', while dt options do not!
 * This can (and did) break NACL automation, which included the '-',
 * so adding this parsing to match the documentation and help text.
 *
 * March 25th, 2015 by Robin T. Miller
 * 	Fix issue not reporting computed runtime correctly.
 * 	Track global fill once state required for -prefill option.
 * 	After fill once, added file flushing to detect write failures.
 * 	Set the runtime end before -prefill fills, to honor the runtime.
 *
 * March 14th, 2015 by Robin T. Miller
 * 	Do not set sio's defaults for runtime, or threads, if they are
 * already set prior to iobehavior=sio, when initializing.
 *
 * January 31st, 2015 by Robin T. Miller
 * 	When a stop= file is specified, do *not* set the alarm time.
 * Setting an alarm implicitly enables keepalives, which will occur every
 * second, since this is the interval of the I/O monitoring thread! This
 * alarm time previously enabled I/O monitoring, but is no longer required
 * since the I/O monitoring thread is always enabled by default.
 *
 * January 28th, 2015 by Robin T. Miller
 * 	Adding separate read/write sizes via ibs= and obs= options.
 * 	Added records variable so reporting record number is correct.
 *
 * September 4th, 2014 by Robin T. Miller
 * 	Disable timer_resolution, since gettimeofday() is now accurate!
 *
 * June 4th, 2014 by Robin T. Miller
 * 	When the max latency is exceeded, execute triggers with the
 * operation type set to "latency", for trigger scripts.
 *
 * April 28th, 2014 by Robin T. Miller
 * 	For total statistics, use job threads started time, which is the
 * time recorded *after* all threads have started running (sync'ed by lock).
 * When starting many threads, esp. with active I/O, we don't wish to use
 * the job start time, since this is recorded *before* threads are started.
 * 
 * April 27th, 2014 by Robin T. Miller
 * 	Added extended error reporting for I/O and miscompare errors.
 *
 * April 22, 2014 by Robin T. Miller
 * 	When reporting total statistics, the thread begin/end times cannot
 * be used since with many threads (esp. with slices), this time is only for
 * a single thread rather than all threads. Therefore, the global start/end
 * times kept for the job are used, rather than the timeb values (old API).
 * Note: This makes the run time seconds and measurement seconds the same!
 * FYI: Normal dt uses clock ticks from times() API, more accurate (IMHO).
 */

#include "dt.h"
#include <sys/timeb.h>

/*
 * Definitions:
 */
#define SIO_DEFAULT_TIMER_RESOLUTION 0
#define SIO_DEFAULT_MIN_LATENCY     10000

#define SIO_DEFAULT_FILE_PER_THREAD False
#define SIO_DEFAULT_FIXED_FILL      -1
#define SIO_DEFAULT_IOMUTEX         False
#define SIO_DEFAULT_LOCKALL         False
#define SIO_DEFAULT_NOFLOCK         False
#define SIO_DEFAULT_INSTRUMENTATION False
#define SIO_DEFAULT_MAX_BLOCKS      512
#define SIO_DEFAULT_MAX_BLKSIZE     (SIO_DEFAULT_MAX_BLOCKS * BLOCK_SIZE)
//#define SIO_DEFAULT_MAX_BLKSIZE     (1024 * 256)  // That's 256k folks!
#define SIO_DEFAULT_PASS_LIMIT      0
#define SIO_DEFAULT_PRETTY_PRINT    False
#define SIO_DEFAULT_RANDOM_ALIGN    512
#define SIO_DEFAULT_RUNTIME         INFINITY
#define SIO_DEFAULT_THREAD_COUNT    1
#define SIO_DEFAULT_VERIFY_FLAG     False

#define BlockNum_t Offset_t

/*
 * Pattern for block filling:
 *
 * Note: This assumes a 32-bit word, it will 
 *       This pattern will wrap around and be repeated after
 *       a long time when dealing with large (>2GB partitions).
 */
#define PATTERN_A(blk_nbr, word_nbr, dev_nbr) \
    ((int)((dev_nbr << 12) | ((blk_nbr) << 16) | ((word_nbr) & 0x0000FFFF)))

/* get_sio_opt_bool("instrumentation") pattern: */
#define PATTERN_B(blk_nbr, word_nbr, no_word) \
    ((blk_nbr*no_word)+word_nbr)
    
#define RAND(dip)   get_random(dip)
#define RAND64(dip) get_random64(dip)

/*
 * sio Specific Parameters (options): 
 */
typedef struct sio_parameters {
    hbool_t     blockno;
    hbool_t     break_on_dc;
    hbool_t     detailed_logging;
    hbool_t     iofailok;
    hbool_t     fileperthread;
    hbool_t     fillonce;
    hbool_t     instrumentation;
    hbool_t     lockall;
    hbool_t     noflock;
    hbool_t     noheader;
    hbool_t     no_dsync;
    hbool_t     iomutex;
    hbool_t     niceoutput;
    hbool_t     no_performance;
    hbool_t     partition_among_threads;
    hbool_t     prefill;
    hbool_t     prettyprint;
    hbool_t     truncate;
    hbool_t     verify;
    int         fixedfill;
    int         read_percentage;
    int         random_percentage;
    size_t      random_alignment;
    int         target_iops;
    uint32_t    think_time;
    int         timer_resolution;
    int         verify_retry;
    size_t      max_blocks;
    size_t      max_blksize;
    int         max_latency;
    uint64_t    numops;
    /* Saved starting and ending block numbers. */
    BlockNum_t  initial_begin_blk;
    BlockNum_t  initial_end_blk;
} sio_parameters_t;

/*
 * sio Thread Specific Information: 
 */
typedef struct sio_thread_info {
    dinfo_t          *dip;
    struct timeb     begin_time, end_time;
    BlockNum_t      begin_blk;
    BlockNum_t      end_blk;
    BlockNum_t      per_thread_curblk;
    uint64_t        reads;
    uint64_t        bytes_read;
    uint64_t        writes;
    uint64_t        records;
    uint64_t        bytes_written;
    uint64_t        latency;
    uint64_t        read_latency;
    uint64_t        write_latency;
    uint64_t        interval_latency;
    uint64_t        io_completes;
    uint64_t        interval_io_completes;
    uint32_t        max_latency;
    uint32_t        min_latency;
    uint32_t        interval_max_latency;
    uint32_t        interval_min_latency;
    uint64_t        sumofsquares_latency;
    uint64_t        interval_sumofsquares_latency;
} sio_thread_info_t;

typedef struct sio_global_data {
    pthread_mutex_t global_lock;
    BlockNum_t      global_curblk;
    unsigned long   pass_count;
    hbool_t         fillonce_done;
} sio_global_data_t;

typedef struct sio_information {
    sio_parameters_t    sio_parameters;
    sio_thread_info_t   sio_thread_info;
} sio_information_t;

typedef struct sio_total_stats {
    time_t      global_time_start;
    time_t      global_time_end;
    time_t	global_compute_time_start;
    time_t	global_compute_time_end;
    uint32_t    global_max_latency;
    uint32_t    global_min_latency;
    uint64_t    combined_sumofsquares_latency;
    uint64_t    global_reads;
    uint64_t    global_bytes_read;
    uint64_t    global_writes;
    uint64_t    global_bytes_written;
    uint64_t    total_ios;
    uint64_t    total_latency;
} sio_total_stats_t;

/*
 * Forward References: 
 */
void sio_help(dinfo_t *dip);
int sio_extend_file(dinfo_t *dip);
int sio_post_open_setup(dinfo_t *dip);
int sio_read_sanity_checks(dinfo_t *dip);
void sio_report_global_time(dinfo_t *dip, sio_total_stats_t *stp);
void sio_report_statistics(dinfo_t *dip, sio_total_stats_t *stp);
void sio_report_thread_stats(dinfo_t *dip);
void sio_pretty_thread_stats(dinfo_t *dip);
void sio_report_total_stats(dinfo_t *dip, sio_total_stats_t *stp);
void sio_pretty_total_stats(dinfo_t *dip, sio_total_stats_t *stp);
void sio_initial_niceoutput(dinfo_t *dip, hbool_t flush_flag);
void sio_init_data_buffer(dinfo_t *dip, sio_parameters_t *siop);
int sio_thread_setup(dinfo_t *dip);
int sio_doio(dinfo_t *dip);
int sio_dofillonce(dinfo_t *dip);
void sio_reset_stats(dinfo_t *dip, sio_thread_info_t *stip);
unsigned long int sio_get_usecs(struct timeval time2, struct timeval time1);
int sio_check_pattern_buffer(dinfo_t *dip,
                             int target_device,
                             char *bufP, BlockNum_t offset, size_t iosize, 
                             size_t blocksize, int break_on_error);
int sio_check_fixed_val_buffer(dinfo_t *dip,
                               int target_device, char *bufP, u_char value,
                               size_t iosize, size_t blocksize, int break_on_error,
                               Offset_t offset);
void sio_report_record(dinfo_t *dip, hbool_t reading, uint64_t record, BlockNum_t curblk,
                       void *buffer, Offset_t offset, size_t bytes);
void sio_report_io_information(dinfo_t *dip, hbool_t reading, BlockNum_t block,
                               void *buffer, Offset_t offset, size_t expected, ssize_t received);

void sio_report_size_mismatch(dinfo_t *dip, hbool_t reading, BlockNum_t block,
                              void *buffer, Offset_t offset, size_t expected, ssize_t received);
void sio_report_data_compare_error(dinfo_t *dip,
                                   int target_device, int expected, int actual,
                                   int recheck, Offset_t blk_nbr, int offset);
void sio_report_miscompare_information(dinfo_t *dip, size_t blocksize, int buffer_index);
void sio_fill_pattern_buffer(dinfo_t *dip, char *bufP, BlockNum_t offset,
                             size_t iosize, size_t blocksize, int dev_nbr);
hbool_t sio_random_block(dinfo_t *dip, BlockNum_t *target, sio_thread_info_t *stip);
hbool_t sio_sequential_block(dinfo_t *dip,
                             BlockNum_t *target,
                             sio_thread_info_t *stip);
hbool_t sio_global_sequential_block(dinfo_t *dip,
                                    BlockNum_t *target,
                                    sio_parameters_t *siop,
                                    sio_thread_info_t *stip,
                                    sio_global_data_t *sgdp);
int sio_verify_write(dinfo_t *dip, HANDLE fd, int target_device, uint64_t record,
                     BlockNum_t curblk, char *buffer, size_t blocksize, Offset_t offset);

int sio_acquire_global_lock(dinfo_t *dip, sio_global_data_t *sgdp);
int sio_release_global_lock(dinfo_t *dip, sio_global_data_t *sgdp);

/* I/O Behavior Support Functions */
int sio_initialize(dinfo_t *dip);
int sio_parser(dinfo_t *dip, char *option);
void sio_cleanup_information(dinfo_t *dip);
int sio_clone_information(dinfo_t *dip, dinfo_t *cdip, hbool_t new_context);
int sio_job_init(dinfo_t *mdip, job_info_t *job);
int sio_job_cleanup(dinfo_t *mdip, job_info_t *job);
int sio_job_finish(dinfo_t *mdip, job_info_t *job);
int sio_job_keepalive(dinfo_t *mdip, job_info_t *job);
void sio_show_parameters(dinfo_t *dip);
void *sio_thread(void *arg);
int sio_validate_parameters(dinfo_t *dip);

/*
 * Declare the I/O behavior functions:
 */
iobehavior_funcs_t sio_iobehavior_funcs = {
    "sio",                      /* iob_name */
    SIO_IO,     		/* iob_iobehavior */
    NULL,                    	/* iob_map_options */
    NULL,			/* iob_maptodt_name */
    NULL,			/* iob_dtmap_options */
    &sio_initialize,            /* iob_initialize */
    NULL,			/* iob_initiate_job */
    &sio_parser,                /* iob_parser */
    &sio_cleanup_information,   /* iob_cleanup */
    &sio_clone_information,     /* iob_clone */
    &sio_thread,                /* iob_thread */
    NULL,                       /* iob_thread1 */
    &sio_job_init,              /* iob_job_init */
    &sio_job_cleanup,           /* iob_job_cleanup */
    &sio_job_finish,            /* iob_job_finish */
    NULL,                       /* iob_job_modify */
    &sio_job_finish,            /* iob_job_query */
    &sio_job_keepalive,         /* iob_job_keepalive */
    NULL,                       /* iob_thread_keepalive */
    &sio_show_parameters,       /* iob_show_parameters */
    &sio_validate_parameters    /* iob_validate_parameters */
};
 
void
sio_set_iobehavior_funcs(dinfo_t *dip)
{
    dip->di_iobf = &sio_iobehavior_funcs;
    return;
}
     
/* ---------------------------------------------------------------------- */

int
sio_parser(dinfo_t *dip, char *option)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    int status = PARSE_MATCH;

    if (match(&option, "-")) {         /* Optional "-" to match sio options! */
        ;
    }
    if (match(&option, "help")) {
        sio_help(dip);
        return(STOP_PARSING);
    }
    if ( match(&option, "dev=") || match(&option, "devs=") ||
         match(&option, "file=") || match(&option, "filename=") ) {
        if (dip->di_output_file) free(dip->di_output_file);
        dip->di_output_file = strdup(option);
        return(status);
    }
    if (match(&option, "readp=")) {
	if (match(&option, "random")) {
            siop->read_percentage = -1;
            return(status);
        }
        siop->read_percentage = (int)number(dip, option, ANY_RADIX, &status, True);
        if (siop->read_percentage > 100) {
            Eprintf(dip, "The read percentage must be in the range of 0-100!\n");
            status = FAILURE;
        }
        return(status);
    }
    if (match(&option, "randp=")) {
        siop->random_percentage = (int)number(dip, option, ANY_RADIX, &status, True);
        if (siop->random_percentage > 100) {
            Eprintf(dip, "The random percentage must be in the range of 0-100!\n");
            status = FAILURE;
        }
        return(status);
    }
    if (match(&option, "runtime=")) {
	dip->di_runtime = time_value(dip, option);
	return(status);
    }
    if (match(&option, "threads=")) {
        dip->di_threads = (int)number(dip, option, ANY_RADIX, &status, True);
        return(status);
    }
    if (match(&option, "start=")) {
        dip->di_file_position = (Offset_t)large_number(dip, option, ANY_RADIX, &status, True);
        if (status == SUCCESS) {
            dip->di_user_position = True;
        }
        return(status);
    }
    if (match (&option, "stop=")) {
        dip->di_stop_on_file = strdup(option);
	/* Beware: Enabling the alarm also enables keepalives! (backwards compatibility) */
	/* But setting this alarm is no longer required, I/O monitoring is always enabled! */
        //if (dip->di_alarmtime == 0) dip->di_alarmtime++;
        return(status);
    }
    if (match(&option, "end=")) {
        dip->di_data_limit = (Offset_t)large_number(dip, option, ANY_RADIX, &status, True);
        if (status == SUCCESS) {
            dip->di_user_limit = dip->di_data_limit;
            if (!dip->di_record_limit) {
                dip->di_record_limit = INFINITY;
            }
        }
        return(status);
    }
    if (match(&option, "align=")) {
        siop->random_alignment = (size_t)number(dip, option, ANY_RADIX, &status, True);
        return(status);
    }
    if (match(&option, "fixedfill=")) {
        siop->fixedfill = (int)number(dip, option, ANY_RADIX, &status, True);
        if (siop->fixedfill > 255) {
            Eprintf(dip, "The fixed fill character must be between 0 and 255!\n");
            status = FAILURE;
        }
        return(status);
    }
    if (match (&option, "iops=")) {
        siop->target_iops = number(dip, option, ANY_RADIX, &status, True);
        siop->think_time = (uSECS_PER_SEC / siop->target_iops);
        return(status);
    }
    if (match(&option, "max_blksize=")) {
        siop->max_blksize = (size_t)number(dip, option, ANY_RADIX, &status, True);
        if (status == SUCCESS) {
            size_t block_size = (dip->di_device_size) ? dip->di_device_size : BLOCK_SIZE;
            siop->max_blocks = (siop->max_blksize / block_size);
        }
        return(status);
    }
    if (match(&option, "max_latency=")) {
        siop->max_latency = number(dip, option, ANY_RADIX, &status, True);
        return(status);
    }
    if (match (&option, "timer_resolution=")) {
        siop->timer_resolution = number(dip, option, ANY_RADIX, &status, True);
        return(status);
    }
    if (match (&option, "think=")) {
        if (match (&option, "random")) {
            siop->think_time = RANDOM_DELAY_VALUE;
        } else {
            siop->think_time = number(dip, option, ANY_RADIX, &status, True);
        }
        return(status);
    }
    if (match(&option, "verify_retry=")) {
        siop->verify_retry = number(dip, option, ANY_RADIX, &status, True);
        return(status);
    }
    /* Various boolean flags: */
    if (match(&option, "blockno")) {
        siop->blockno = True;
        return(status);
    }
    if (match(&option, "break_on_dc")) {
        siop->break_on_dc = True;
        return(status);
    }
    if (match(&option, "debug")) {
        dip->di_debug_flag = debug_flag = True;
        return(status);
    }
    if (match(&option, "Debug")) {
        dip->di_Debug_flag = True;
        return(status);
    }
    if (match(&option, "detailed_logging")) {
        siop->detailed_logging = True;
        return(status);
    }
    if (match(&option, "direct")) {
        dip->di_open_flags |= O_DIRECT;
        dip->di_dio_flag = True;
        return(status);
    }
    if (match(&option, "fileperthread")) {
        siop->fileperthread = True;
        dip->di_fileperthread = siop->fileperthread;
        return(status);
    }
    if (match(&option, "iofailok")) {
        siop->iofailok = True;
        return(status);
    }
    if (match(&option, "fillonce")) {
        siop->fillonce = True;
        siop->no_dsync = True;
        siop->no_performance = True;
        return(status);
    }
    if (match(&option, "noheader")) {
        siop->noheader = True;
        return(status);
    }
    if (match(&option, "instrumentation")) {
        siop->instrumentation = True;
        return(status);
    }
    if (match(&option, "iomutex")) {
        siop->iomutex = True;
        return(status);
    }
    if (match(&option, "nomutex")) {
        siop->iomutex = False;
        return(status);
    }
    if (match(&option, "lockall")) {
        siop->lockall = True;
        return(status);
    }
    if (match(&option, "niceoutput")) {
        siop->niceoutput = True;
        return(status);
    }
    if (match(&option, "numops=")) {
        siop->numops = (int)number(dip, option, ANY_RADIX, &status, True);
        return(status);
    }
    if ( match(&option, "no_dsync") || match(&option, "nodsync") ) {
        siop->no_dsync = True;
        return(status);
    }
    if (match(&option, "noflock")) {
        siop->noflock = True;
        return(status);
    }
    if (match(&option, "noperf")) {
        siop->no_performance = True;
        return(status);
    }
    if (match(&option, "partition_among_threads")) {
        siop->partition_among_threads = True;
        return(status);
    }
    if (match(&option, "prefill")) {
        siop->prefill = True;
        return(status);
    }
    if (match(&option, "noprefill")) {
        siop->prefill = False;
        return(status);
    }
    if (match(&option, "prettyprint")) {
        siop->prettyprint = True;
        return(status);
    }
    if (match(&option, "truncate")) {
        dip->di_write_flags |= O_TRUNC; /* Truncate output file(s). */
        siop->truncate = True;
        return(status);
    }
    if (match(&option, "verify")) {
        siop->verify = True;
        return(status);
    }

    return(PARSE_NOMATCH);
}

/* ---------------------------------------------------------------------- */

int
sio_acquire_global_lock(dinfo_t *dip, sio_global_data_t *sgdp)
{
    int status = pthread_mutex_lock(&sgdp->global_lock);
    if (status != SUCCESS) {
        tPerror(dip, status, "Failed to acquire sio global mutex!");
    }
    return(status);
}

int
sio_release_global_lock(dinfo_t *dip, sio_global_data_t *sgdp)
{
    int status = pthread_mutex_unlock(&sgdp->global_lock);
    if (status != SUCCESS) {
        tPerror(dip, status, "Failed to unlock sio global mutex!");
    }
    return(status);
}

/* 
 * Note: This is invoked after the job is created, but *before* threads are created! 
 */
int
sio_job_init(dinfo_t *dip, job_info_t *job)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_global_data_t *sgdp = NULL;
    int status = SUCCESS;

    if (siop->timer_resolution) {
        (void)os_set_timer_resolution(siop->timer_resolution);
    }

    /*
     * Note: This global shared area is only required for multiple threads
     * accessing the same file. Otherwise each thread has its' own state.
     * Also, this shared area is only for sequential I/O, not random I/O,
     * therefore 100% random disables. Performance reduced with globals.
     */
    if ( ( (dip->di_slices == 0) && (dip->di_threads > 1) ) &&
         ( (siop->fillonce == True) ||
           ((siop->fileperthread == False) && (siop->random_percentage != 100)) ) ) {
        sgdp = Malloc(dip, sizeof(*sgdp));
        if (sgdp == NULL) return(FAILURE);
        if (siop->iomutex == True) {
            if ( (status = pthread_mutex_init(&sgdp->global_lock, NULL)) != SUCCESS) {
                tPerror(dip, status, "pthread_mutex_init() of sio global lock failed!");
                Free(dip, sgdp);
                return(FAILURE);
            }
        }
        job->ji_opaque = sgdp;
	if (dip->di_file_position) {
	    sgdp->global_curblk = (dip->di_file_position / dip->di_block_size);
	}
        if (dip->di_debug_flag) {
            Printf(dip, "Global data space has been allocated, expect slower performance!\n");
        }
    }
    /* Share this file descriptor across multiple threads to the same file. */
    if (sgdp || dip->di_slices) {
	/* Note: Expand format control strings such as "%uuid". */
	if ( strchr(dip->di_dname, '%') ) {
	    if ( strstr(dip->di_dname, "%uuid") ) {
		dip->di_uuid_string = os_get_uuid(dip->di_uuid_dashes);
	    }
	    status = format_device_name(dip, dip->di_dname);
	    if (status == FAILURE) return(status);
	}

        /* Note: Linux will not apply lock, if the file is *not* read/write! */
        /* This also handles mixed reads and writes and/or verify operations. */
        dip->di_initial_flags &= ~OS_WRITEONLY_MODE;
        dip->di_initial_flags |= OS_READWRITE_MODE;
        /* Note: This file will get cloned and shared across all threads! */
        status = (*dip->di_funcs->tf_open)(dip, dip->di_initial_flags);
        if (status == SUCCESS) {
            dip->di_shared_file = True;
            dip->di_open_flags &= ~O_CREAT; /* Only create on first open. */
        }
    }
    return(status);
}

/* 
 * sio_cleanup_job() - Do cleanup after a job comepletes.
 *                                                         .
 * This cleanup is invoked after all threads have completed.
 * The device information pointer is for the frist thread.
 *
 * Inputs:
 *  dip = The device information pointer (of the first thread).
 * 
 * Return Value:
 *  Returns Success / Failure.
 */
int
sio_job_cleanup(dinfo_t *dip, job_info_t *job)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_global_data_t *sgdp = job->ji_opaque;
    int status = SUCCESS;

    if (siop->timer_resolution) {
        (void)os_reset_timer_resolution(siop->timer_resolution);
    }

    if (sgdp) {
        if (siop->iomutex == True) {
            if ( (status = pthread_mutex_destroy(&sgdp->global_lock)) != SUCCESS) {
                tPerror(dip, status, "pthread_mutex_destroy() of sio global lock failed!");
                status = FAILURE;
            }
        }
        Free(dip, sgdp);
        job->ji_opaque = NULL;
        if ( (dip->di_shared_file) && (dip->di_fd != NoFd) ) {
            status = (*dip->di_funcs->tf_close)(dip);
        }
    }
    return(status);
}

int
sio_job_finish(dinfo_t *dip, job_info_t *job)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_thread_info_t *stip = &sip->sio_thread_info;
    threads_info_t *tip = job->ji_tinfo;
    sio_total_stats_t total_stats;
    sio_total_stats_t *stp = &total_stats;
    dinfo_t *tdip;
    int thread;

    if ( (dip->di_stats_flag == False) || (dip->di_stats_level == STATS_NONE) ) {
        return(SUCCESS);
    }
    memset(stp, '\0', sizeof(*stp));
    stp->global_time_start = job->ji_job_start;
    stp->global_time_end = job->ji_job_end;
    stp->global_compute_time_start = job->ji_threads_started;
    stp->global_compute_time_end = job->ji_job_end;
    stp->global_min_latency = SIO_DEFAULT_MIN_LATENCY;

    /*
     * Accumulate the total statistics.
     */
    for (thread = 0; (thread < tip->ti_threads); thread++) {
        tdip = tip->ti_dts[thread];
        sip = tdip->di_opaque;
        stip = &sip->sio_thread_info;
        /* Accumulate thread statistics here...*/
        stp->global_reads += stip->reads;
        stp->global_bytes_read += stip->bytes_read;
        stp->global_writes += stip->writes;
        stp->global_bytes_written += stip->bytes_written;
        stp->total_ios += stip->io_completes;
        stp->total_latency += stip->latency;
        if (stp->global_max_latency < stip->max_latency) {
            stp->global_max_latency = stip->max_latency;
        }
        if (stp->global_min_latency > stip->min_latency) {
            stp->global_min_latency = stip->min_latency;
        }
        stp->combined_sumofsquares_latency += stip->sumofsquares_latency;

        sio_report_thread_stats(tdip);
    }
    
    /* TODO: Query operation, master dip does *not* have sio pointers! */
    if ( (siop == NULL) || (stip == NULL) ) return(SUCCESS);

    if (siop->no_performance == True) {
        Lprintf(dip, "\nNote: No performance statistics are printed for fillonce or random runs.\n");
        sio_report_statistics(dip, stp);
        sio_initial_niceoutput(dip, True);
    } else {
        sio_report_total_stats(dip, stp);
    }
    if (dip->di_history_size && dip->di_history_dump) {
        dump_history_data(dip);
    }
    return(SUCCESS);
}

int
sio_job_keepalive(dinfo_t *mdip, job_info_t *job)
{
    threads_info_t *tip = job->ji_tinfo;
    int status = SUCCESS;
    dinfo_t *dip;

    dip = tip->ti_dts[0];
    /*
     * Only using job keepalive to process stop on file (if any).
     */
    if (dip->di_stop_on_file) {
        if (os_file_exists(dip->di_stop_on_file) == True) {
            if (dip->di_verbose_flag) {
                Printf(mdip, "Detected stop on file %s, so stopping job...\n",  dip->di_stop_on_file);
            }
            status = stop_job(mdip, dip->di_job);
        }
    }
    return(status);
}

void
sio_report_global_time(dinfo_t *dip, sio_total_stats_t *stp)
{
    Lprintf(dip, DT_FIELD_WIDTH, "Starting time");
    Ctime(dip, stp->global_time_start);
    Lprintf(dip, "\n");
    Lprintf(dip, DT_FIELD_WIDTH, "Ending time");
    Ctime(dip, stp->global_time_end);
    Lprintf(dip, "\n");
    return;
}

void
sio_report_statistics(dinfo_t *dip, sio_total_stats_t *stp)
{
    sio_information_t *sip = dip->di_opaque;
    sio_thread_info_t *stip = &sip->sio_thread_info;
    double Kbytes, Mbytes;

    /* Report extra information to help with triage, etc. */
    report_os_information(dip, True);
    report_file_system_information(dip, True, True);
    report_scsi_summary(dip, True);

    Lprintf(dip, "\nTotal Statistics:\n");

    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "Total I/O's", stp->total_ios);

    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "Total reads", stp->global_reads);
    Kbytes = (double)( (double)stp->global_bytes_read / (double)KBYTE_SIZE);
    Mbytes = (double)( (double)stp->global_bytes_read / (double)MBYTE_SIZE);
    Lprintf(dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes)\n", 
            "Total bytes read", stp->global_bytes_read, Kbytes, Mbytes);
    
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "Total writes", stp->global_writes);
    Kbytes = (double)( (double)stp->global_bytes_written / (double)KBYTE_SIZE);
    Mbytes = (double)( (double)stp->global_bytes_written / (double)MBYTE_SIZE);
    Lprintf(dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes)\n", 
            "Total bytes written", stp->global_bytes_written, Kbytes, Mbytes);

    Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "Computed IOPS",
            (double)(stp->global_reads + stp->global_writes) /
            (double)(stip->end_time.time - stip->begin_time.time));

    Lprintf(dip, DT_FIELD_WIDTH TMF " seconds\n", "Computed run time",
            stp->global_time_end - stp->global_time_start);
    sio_report_global_time(dip, stp);

    Lprintf(dip, "\n");
    Lflush(dip);
    return;
}

void
sio_report_thread_stats(dinfo_t *dip)
{
    sio_information_t *sip = dip->di_opaque;
    sio_thread_info_t *stip = &sip->sio_thread_info;
    sio_parameters_t *siop = &sip->sio_parameters;
    double avg, stddev;

    if (siop->prettyprint == True) {
        sio_pretty_thread_stats(dip);
        return;
    }

    if (siop->no_performance == True) return;

    Lprintf(dip, "\n");
    if (dip->di_thread_number == 1) {
        Lprintf(dip, "Thread Latency Stats:\n");
    }

    if ( (stip->io_completes == 0) && (siop->no_performance == True) ) {
        stddev = 0.0;
        avg = 0.0;
    } else {
        /* sumofsquares is already in ms */
        stddev = sqrt((double)stip->sumofsquares_latency / (double)stip->io_completes);
        avg = ((double)stip->latency / (double)stip->io_completes) / 1000.0;
    }

    Lprintf(dip, " Thread:        %10d\n", dip->di_thread_number);
    Lprintf(dip, "  ios:          %10llu\n", stip->io_completes);
    Lprintf(dip, "  latency(us):  %10llu\n", stip->latency);
    Lprintf(dip, "  sumofsquares: %10llu\n", stip->sumofsquares_latency);
    
    Lprintf(dip, "  min(ms):      %10.2f\n", (stip->min_latency / 1000.0));
    Lprintf(dip, "  max(ms):      %10.2f\n", (stip->max_latency / 1000.0));
    Lprintf(dip, "  avg(ms):      %10.2f\n", avg);
    Lprintf(dip, "  stddev(ms):   %10.2lf\n", stddev);
    Lflush(dip);
    return;
}

void
sio_pretty_thread_stats(dinfo_t *dip)
{
    sio_information_t *sip = dip->di_opaque;
    sio_thread_info_t *stip = &sip->sio_thread_info;
    sio_parameters_t *siop = &sip->sio_parameters;
    double avg, stddev;
    double Kbytes, Mbytes;

    Lprintf(dip, "\n");
    if (siop->no_performance == True) {
        Lprintf(dip, "Thread Statistics:\n");
    } else {
        Lprintf(dip, "Thread Latency Statistics:\n");
    }

    if ( (stip->io_completes == 0) && (siop->no_performance == True) ) {
        stddev = 0.0;
        avg = 0.0;
    } else {
        /* sumofsquares is already in ms */
        stddev = sqrt((double)stip->sumofsquares_latency / (double)stip->io_completes);
        avg = ((double)stip->latency / (double)stip->io_completes) / 1000.0;
    }

    Lprintf(dip, DT_FIELD_WIDTH "%d\n", "Thread", dip->di_thread_number);
    Lprintf(dip, DT_FIELD_WIDTH "%s\n", "File name", dip->di_dname);
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "I/O's complete", stip->io_completes);
    
    if (siop->no_performance == False) {
        Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "latency(us)", stip->latency);
        Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "sumofsquares", stip->sumofsquares_latency);
        Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "min(ms)", (stip->min_latency / 1000.0));
        Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "max(ms)", (stip->max_latency / 1000.0));
        Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "avg(ms)", avg);
        Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "stddev(ms)", stddev);
    }

    //Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "Total I/O's", stip->io_completes);

    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "Begin block", stip->begin_blk);
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "End block", stip->end_blk);
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "Total blocks", (stip->end_blk - stip->begin_blk));

    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "Total reads", stip->reads);
    Kbytes = (double)( (double)stip->bytes_read / (double)KBYTE_SIZE );
    Mbytes = (double)( (double)stip->bytes_read / (double)MBYTE_SIZE );
    Lprintf(dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes)\n", 
            "Total bytes read", stip->bytes_read, Kbytes, Mbytes);

    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "Total writes", stip->writes);
    Kbytes = (double)( (double)stip->bytes_written / (double)KBYTE_SIZE );
    Mbytes = (double)( (double)stip->bytes_written / (double)MBYTE_SIZE );
    Lprintf(dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes)\n", 
            "Total bytes written", stip->bytes_written, Kbytes, Mbytes);
    
    Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "Computed IOPS",
            (double)(stip->reads + stip->writes) /
            (double)(stip->end_time.time - stip->begin_time.time));
    
    Lprintf (dip, DT_FIELD_WIDTH, "Total elapsed time");
    format_time(dip, (dip->di_end_time - dip->di_start_time));

    Lflush(dip);
    return;
}

void
sio_report_total_stats(dinfo_t *dip, sio_total_stats_t *stp)
{
    sio_information_t *sip = dip->di_opaque;
    //sio_thread_info_t *stip = &sip->sio_thread_info;
    sio_parameters_t *siop = &sip->sio_parameters;
    double ios_per_sec, total_time, resp_time, throughput;
    long int secs, msecs;
    double latency_per_io;
    double combined_stddev_latency;

    if (siop->prettyprint == True) {
        sio_pretty_total_stats(dip, stp);
        return;
    }

    //secs = (long int)(stip->end_time.time - stip->begin_time.time);
    //msecs = (stip->end_time.millitm - stip->begin_time.millitm);
    //if (msecs < 0) {
    //    secs--; msecs += 1000;
    //}
    secs = (long int)(stp->global_compute_time_end - stp->global_compute_time_start);
    msecs = 0;

    if (stp->total_ios == 0) {
        latency_per_io = 0.0;
    } else {
        /* convert from usec to msec */
        latency_per_io = ((double)stp->total_latency / (double)stp->total_ios) / 1000.0;
    }

    total_time = secs + (msecs / 1000.0);    // number of seconds during run
    ios_per_sec = (double)stp->total_ios / total_time;
    resp_time = (double)dip->di_threads / ios_per_sec;
    resp_time *= 1000.0;    // convert to msecs

    throughput = ios_per_sec * (double)(dip->di_block_size / 1024.0);

    Lprintf(dip, "\n");
    if (siop->noheader == False) {
        Lprintf(dip,
                "IOPS,TPUT(KB/s),LAT(ms)Calc'd,LAT(ms)Actual,READ,RAND,IOS,SEC,THDS,BLKSZ\n");
    }
    // data output
    Lprintf(dip, "%.0f,%.0f,%.3f,[%.3f],%d,%d,%llu,%.0lf,%d,"SUF"\n",
            ios_per_sec, throughput, resp_time, latency_per_io, siop->read_percentage,
            siop->random_percentage, stp->total_ios, total_time, dip->di_threads,
            dip->di_block_size);

    if (stp->total_ios == 0) {
        combined_stddev_latency = 0.0;
    } else {
        combined_stddev_latency = sqrt((double)stp->combined_sumofsquares_latency / (double)stp->total_ios);
    }

    Lprintf(dip, "\nGlobal Latency Stats:\n");
    Lprintf(dip, " ios:           %10llu\n", stp->total_ios);
    Lprintf(dip, " latency(us):   %10llu\n", stp->total_latency);
    Lprintf(dip, " sumofsquares:  %10llu\n", stp->combined_sumofsquares_latency);
    Lprintf(dip, " min(ms):       %10.2f\n", (stp->global_min_latency / 1000.0));
    Lprintf(dip, " max(ms):       %10.2f\n", (stp->global_max_latency / 1000.0));
    Lprintf(dip, " avg(ms):       %10.2f\n", ((stp->total_latency / (double)stp->total_ios) / 1000.0));
    Lprintf(dip, " stddev:        %10.2lf\n", combined_stddev_latency);
    Lprintf(dip, "\n");

    Lprintf(dip, "global_reads = %llu; global_bytes_read = %llu (%llu KB)\n",
            stp->global_reads, stp->global_bytes_read, stp->global_bytes_read/1024);
    Lprintf(dip, "global_writes = %llu; global_bytes_written = %llu (%llu KB)\n",
            stp->global_writes, stp->global_bytes_written,
            stp->global_bytes_written / 1024);
    
    Lprintf(dip, "global_time_start = " TMF ", global_stop_time = " TMF "\n",
            stp->global_time_start, stp->global_time_end);
    Lprintf(dip, "measurement start = " TMF ", measurement stop = " TMF "\n",
            stp->global_compute_time_start, stp->global_compute_time_end);
            //stip->begin_time.time, stip->end_time.time);
    Lprintf(dip, "Computed run time seconds = " TMF "\n",
            stp->global_time_end - stp->global_time_start);
    Lprintf(dip, "Computed measurement seconds = %lu\n", secs);
            //stip->end_time.time - stip->begin_time.time);
    Lprintf(dip, "Computed IOPS = %.2f\n",
            (double)(stp->global_reads + stp->global_writes) / (double)secs);
            //(double)(stip->end_time.time - stip->begin_time.time));
    Lprintf(dip, "Computed KB/s = %.2f\n",
            (double)((stp->global_bytes_read + stp->global_bytes_written) / 1024.0) / (double)secs);
            // / (double)(stip->end_time.time - stip->begin_time.time));
    if (stp->global_reads || stp->global_writes) {
        Lprintf(dip, "Computed bytes/IO = %llu\n",
               (stp->global_bytes_read + stp->global_bytes_written) /
               (stp->global_reads + stp->global_writes));
    } else {
        Lprintf(dip, "Computed bytes/IO = 0\n");
    }

    if (siop->niceoutput == True) {
        Lprintf(dip, "\n");
        sio_initial_niceoutput(dip, False);
        /* Now, display performance information. */
        Lprintf(dip, "IOPS:           %.0f\n", ios_per_sec);
        Lprintf(dip, "TPUT(KB/s):     %.0f\n", throughput);
        Lprintf(dip, "LAT(ms):        %.3f\n", resp_time);
        Lprintf(dip, "READ:           %d\n", siop->read_percentage);
        Lprintf(dip, "RAND:           %d\n", siop->random_percentage);
        Lprintf(dip, "IOS:            %llu\n", stp->total_ios);
        Lprintf(dip, "SEC:            %.0f\n", total_time);
        Lprintf(dip, "Thds:           %d\n", dip->di_threads);
        Lprintf(dip, "BLKSZ:          %d\n", 
                (dip->di_variable_flag) ? -1 : (int)dip->di_block_size);
    }
    Lflush(dip);
    return;
}

void
sio_initial_niceoutput(dinfo_t *dip, hbool_t flush_flag)
{
    sio_information_t *sip = dip->di_opaque;
    //sio_thread_info_t *stip = &sip->sio_thread_info;
    sio_parameters_t *siop = &sip->sio_parameters;

    if (siop->niceoutput == False) return;

    Lprintf(dip, "Read:           %d\n", siop->read_percentage);
    Lprintf(dip, "Rand:           %d\n", siop->random_percentage);
    Lprintf(dip, "BlkSz:          %d\n",
            (dip->di_variable_flag) ? -1 : (int)dip->di_block_size);
    /* Note: The begin and end blocks are *not* accurate with slices! */
    Lprintf(dip, "BegnBlk:        "FUF"\n", siop->initial_begin_blk);
    Lprintf(dip, "EndBlk:         "LUF"\n", siop->initial_end_blk);
    Lprintf(dip, "Secs:           "TMF"\n", dip->di_runtime);
    Lprintf(dip, "Threads:        %d\n", dip->di_threads);
    Lprintf(dip, "Devs:           1  %s\n", dip->di_dname);
    if (flush_flag == True) Lflush(dip);
    return;
}

void
sio_pretty_total_stats(dinfo_t *dip, sio_total_stats_t *stp)
{
    sio_information_t *sip = dip->di_opaque;
    //sio_thread_info_t *stip = &sip->sio_thread_info;
    //sio_parameters_t *siop = &sip->sio_parameters;
    double ios_per_sec, total_time, resp_time, throughput;
    long int secs, msecs;
    double latency_per_io;
    double combined_stddev_latency;
    double Kbytes, Mbytes;

    /* Report extra information to help with triage, etc. */
    report_os_information(dip, True);
    report_file_system_information(dip, True, True);
    report_scsi_summary(dip, True);

    //secs = (long int)(stip->end_time.time - stip->begin_time.time);
    //msecs = (stip->end_time.millitm - stip->begin_time.millitm);
    //if (msecs < 0) {
    //    secs--; msecs += 1000;
    //}
    secs = (long int)(stp->global_compute_time_end - stp->global_compute_time_start);
    msecs = 0;

    if (stp->total_ios == 0) {
        latency_per_io = 0.0;
    } else {
        /* convert from usec to msec */
        latency_per_io = ((double)stp->total_latency / (double)stp->total_ios) / 1000.0;
    }

    total_time = secs + (msecs / 1000.0);    // number of seconds during run
    ios_per_sec = (double)stp->total_ios / total_time;
    resp_time = (double)dip->di_threads / ios_per_sec;
    resp_time *= 1000.0;    // convert to msecs

    throughput = ios_per_sec * (double)(dip->di_block_size / 1024.0);

    if (stp->total_ios == 0) {
        combined_stddev_latency = 0.0;
    } else {
        combined_stddev_latency = sqrt((double)stp->combined_sumofsquares_latency / (double)stp->total_ios);
    }

    Lprintf(dip, "\nGlobal Latency Statistics:\n");
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "ios", stp->total_ios);
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "latency(us)", stp->total_latency);
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "sumofsquares", stp->combined_sumofsquares_latency);
    Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "min(ms)", (stp->global_min_latency / 1000.0));
    Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "max(ms)", (stp->global_max_latency / 1000.0));
    Lprintf(dip, DT_FIELD_WIDTH "%.2f\n",
            "avg(ms)", ((stp->total_latency / (double)stp->total_ios) / 1000.0));
    Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "stddev", combined_stddev_latency);
    Lprintf(dip, "\n");
    
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "global reads", stp->global_reads);
    Kbytes = (double)( (double)stp->global_bytes_read / (double)KBYTE_SIZE );
    Mbytes = (double)( (double)stp->global_bytes_read / (double)MBYTE_SIZE );
    Lprintf(dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes)\n", 
            "global bytes read", stp->global_bytes_read, Kbytes, Mbytes);

    Lprintf(dip, DT_FIELD_WIDTH LUF "\n", "global writes", stp->global_writes);
    Kbytes = (double)( (double)stp->global_bytes_written / (double)KBYTE_SIZE );
    Mbytes = (double)( (double)stp->global_bytes_written / (double)MBYTE_SIZE );
    Lprintf(dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes)\n", 
            "global bytes written", stp->global_bytes_written, Kbytes, Mbytes);

    Lprintf(dip, DT_FIELD_WIDTH TMF, "global start/stop time", stp->global_time_start);
    Lprintf(dip, " / " TMF "\n", stp->global_time_end);
    Lprintf(dip, DT_FIELD_WIDTH TMF "\n", "Computed run time seconds",
            stp->global_time_end - stp->global_time_end);

    Lprintf(dip, DT_FIELD_WIDTH TMF, "measurement start/stop", stp->global_compute_time_start); // stip->begin_time.time);
    Lprintf(dip, " / " TMF "\n", stp->global_compute_time_end); // stip->end_time.time);
    Lprintf(dip, DT_FIELD_WIDTH TMF "\n", "Computed measurement seconds", secs);
            //stip->end_time.time - stip->begin_time.time);

    Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "Computed IOPS",
            (double)(stp->global_reads + stp->global_writes) / (double)secs);
            //(double)(stip->end_time.time - stip->begin_time.time));
    Lprintf(dip, DT_FIELD_WIDTH "%.2f\n", "Computed KB/s",
            (double)((stp->global_bytes_read + stp->global_bytes_written) / 1024.0) / (double)secs);
            // / (double)(stip->end_time.time - stip->begin_time.time));
    Lprintf(dip, DT_FIELD_WIDTH "", "Computed bytes/IO");
    if (stp->global_reads || stp->global_writes) {
        Lprintf(dip, LUF "\n",
               (stp->global_bytes_read + stp->global_bytes_written) /
               (stp->global_reads + stp->global_writes));
    } else {
        Lprintf(dip, "0\n");
    }

    sio_report_global_time(dip, stp);
    Lprintf(dip, "\n");
    Lflush(dip);
    return;
}

/*
 * Name:    sio_doio
 *
 * Arguments:
 *  threadnum   thread number
 *  fd          file descriptor array- copy of globalfds (-globalfds) or a 
 *              per thread copy
 *  buf         buffer allocated by work_thread for doing i/o
 *
 * Description:
 *  For every i/o,
 *  a. determines device to do i/o, read or write, blocknumber and blocksize
 *  b. does i/o
 *    *READS:
 *    * In case of 100% reads and verify, reads from the file and verifies
 *      against the fixedfill pattern.
 *    * For other reads, issues pread
 *    *WRITES:
 *    * In case of partial write test, ???
 *    * verify option - fills the buffer with a fixed pattern and does a 
 *      pwrite and does a pread to verify the write/pwrite.
 *    * all other cases issues a pwrite
 */
int
sio_doio(dinfo_t *dip)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_thread_info_t *stip = &sip->sio_thread_info;
    sio_global_data_t *sgdp = dip->di_job->ji_opaque;
    int threadnum = dip->di_thread_number;
    HANDLE fd = dip->di_fd;
    uint64_t numops = 0;
    void *buffer = dip->di_data_buffer;
    int probread, probrand;
    int target_device = (dip->di_device_number - 1);
    BlockNum_t curblk;
    struct timeval issue_time, complete_time;
    ssize_t byte_count;
    register Offset_t offset;
    hbool_t reading = False;
    register unsigned long int latency;
    int cur_p_read;
    register size_t cur_blk_sz;
    uint32_t cur_think_time;
    struct timeval loop_start_time, loop_end_time;
    uint32_t loop_usecs;
    uint64_t target_total_usecs = 0;
    uint64_t actual_total_usecs = 0;
    hbool_t first_pass = True;
    int status = SUCCESS;

    if (sgdp) {
        curblk = sgdp->global_curblk;
    } else {
        curblk = stip->begin_blk;
    }
    stip->per_thread_curblk = curblk;
    stip->latency = 0;

    while (True) {
        
        PAUSE_THREAD(dip);
        if ( THREAD_TERMINATING(dip) ) break;
        if (dip->di_terminating) break;

        gettimeofday(&loop_start_time, NULL);

        /*
         * If not the first time through, get proper accounting of the 
         * time since last loop measurement
         */
        if (first_pass == False) {
            actual_total_usecs += sio_get_usecs(loop_start_time, loop_end_time);
        }
        first_pass = False;

        probread = (int)(RAND(dip) % 100);
        probrand = (int)(RAND(dip) % 100);

        /* Note: In this implementation, each target is its' own job. */
        //target_device = pick_target(threadnum);
    
        if (siop->read_percentage == -1) {
            cur_p_read = (int)(RAND(dip) % 100);
        } else {
            cur_p_read = siop->read_percentage;
        }
	if (probread < cur_p_read) {
	    reading = True;
	} else {
	    reading = False;
	}

        if (dip->di_variable_flag == True) {
            /* 
             * blk_sz between device size bytes (512 by default) and max blocks
             * (256k by default) at device size byte alignment.
             */ 
            cur_blk_sz = (((int)(RAND(dip) % siop->max_blocks)) + 1) * BLOCK_SIZE;
            if (cur_blk_sz < siop->random_alignment) {
                cur_blk_sz = siop->random_alignment;
            } else {
                cur_blk_sz += (cur_blk_sz % siop->random_alignment);
            }
        } else {
            cur_blk_sz = dip->di_block_size;
	    /* Allow independent read/write sizes for fixed I/O. */
	    if ( (reading == True) && dip->di_iblock_size ) {
		cur_blk_sz = dip->di_iblock_size;
	    } else if ( (reading == False) && dip->di_oblock_size ) {
		cur_blk_sz = dip->di_oblock_size;
	    }
        }    
        if (probrand < siop->random_percentage) {
            (void)sio_random_block(dip, &curblk, stip);
        } else {
            hbool_t wrapped = False;
            if (sgdp) {
                //wrapped = sio_global_sequential_block(dip, &curblk, siop, stip, sgdp);
#if 1
                /*
                 * Get next global sequential block number to use.
                 */
                if (siop->iomutex == True) {
                    status = sio_acquire_global_lock(dip, sgdp);
                }
                curblk = sgdp->global_curblk++;
                if (curblk >= stip->end_blk) {
                    curblk = stip->begin_blk;
                    dip->di_pass_count++;
                    sgdp->pass_count++;
                    sgdp->global_curblk = curblk;
                    wrapped = True;
                }
                /* 
                 * If another thread wrapped, adjust this thread too! 
                 */
                if (sgdp->pass_count > dip->di_pass_count) {
                    wrapped = True;
                    dip->di_pass_count++;
                }
                if ( (siop->iomutex == True) && (status == SUCCESS) ) {
                    status = sio_release_global_lock(dip, sgdp);
                }
#endif /* 1 */
            } else { /* Single thread or partition_among_threads. */
                //wrapped = sio_sequential_block(dip, &curblk, stip);
                /* Note: Doing this inline improves performance! (~23% in my testing!) */
#if 1
                curblk = stip->per_thread_curblk++;
                if (curblk >= stip->end_blk) {
                    curblk = stip->begin_blk;
                    stip->per_thread_curblk = curblk;
                    dip->di_pass_count++;
                    wrapped = True;
                }
#endif
            }

            /*
             * Truncate after wrapping, if writing and not doing *any* random I/O.
             */
            if ( (wrapped == True) &&
                 (siop->random_percentage == 0) &&
                 (cur_p_read == 0) && (siop->truncate == True) ) {
                if (os_ftruncate_file(fd, (Offset_t) 0) == FAILURE) {
                    ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_TRUNCATE_FILE_OP, TRUNCATE_OP, True);
                }
           }
        }

        /* Note: The pass count is only adjusted for sequential I/O. */
        if ( dip->di_pass_limit && (dip->di_pass_count >= dip->di_pass_limit) ) {
            break;
        }

        /*
         * Calculate the size of the IO, plus the offset within the
         * user-specified block, for this loop.
         */
        /* Note: With a random block size, the I/O is *not* sequential! */
        /* Also Note: The offset must be available for noprog's and triggers! */
        dip->di_offset = offset = ( (Offset_t)curblk * (Offset_t)cur_blk_sz );
        if (reading == True) {
            gettimeofday(&issue_time, NULL);
            if ( (siop->read_percentage == 100) &&
                 (siop->fixedfill != -1) && siop->verify) {
                status = sio_verify_write(dip, fd, target_device, (stip->records + 1),
					  curblk, buffer, cur_blk_sz, offset);
		if (status == SUCCESS) stip->records++;
                if ( (status == FAILURE) && (siop->break_on_dc == True) ) break;
                byte_count = cur_blk_sz;
            } else {
                if (siop->blockno == True) {
                    sio_report_record(dip, reading, (stip->records + 1), curblk, buffer, offset, cur_blk_sz);
                }
                ENABLE_NOPROG(dip, READ_OP);
                byte_count = pread(fd, buffer, cur_blk_sz, offset);
                DISABLE_NOPROG(dip);
                if (dip->di_history_size) {
                    unsigned long file_number = 1;
                    save_history_data(dip,
                                      file_number, (unsigned long)(stip->records + 1), READ_MODE,
                                      offset, buffer, cur_blk_sz, byte_count);
                }
                if (byte_count == FAILURE) {
                    ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_PREAD_FILE_OP, READ_OP, True);
                    sio_report_io_information(dip, reading, curblk, buffer, offset, cur_blk_sz, byte_count);
		    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
			 (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
			(void)ExecuteTrigger(dip, "read");
		    }
                    status = FAILURE;
                    break;
                } else {
                    stip->bytes_read += byte_count;
                    stip->reads++;
		    stip->records++;
                }
            }
        } else {
            if (siop->fixedfill != -1) {
                memset(buffer, siop->fixedfill, cur_blk_sz);
            } else {
                if (siop->verify == True) {
                    sio_fill_pattern_buffer(dip, buffer, curblk, 1, cur_blk_sz, target_device);
                }
            }
            if (siop->blockno == True) {
		sio_report_record(dip, reading, (stip->records + 1), curblk, buffer, offset, cur_blk_sz);
            }
            gettimeofday(&issue_time, NULL);
            ENABLE_NOPROG(dip, WRITE_OP);
            byte_count = pwrite(fd, buffer, cur_blk_sz, offset);
            DISABLE_NOPROG(dip);
            if (dip->di_history_size) {
                unsigned long file_number = 1;
                save_history_data(dip,
                                  file_number, (unsigned long)(stip->records + 1), WRITE_MODE,
                                  offset, buffer, cur_blk_sz, byte_count);
            }
            if (byte_count == FAILURE) {
                ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_PWRITE_FILE_OP, WRITE_OP, True);
                sio_report_io_information(dip, reading, curblk, buffer, offset, cur_blk_sz, byte_count);
		if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		     (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
		    (void)ExecuteTrigger(dip, "write");
		}
            } else {
                stip->bytes_written += byte_count;
                stip->writes++;
		stip->records++;
            }
        }

        gettimeofday(&complete_time, NULL);
        latency = sio_get_usecs(complete_time, issue_time);
        if ( siop->max_latency && ((latency / 1000.0) > siop->max_latency) )  {
            Fprintf(dip, "Maximum latency %ld/%d exceeded\n",
                    (long)(latency / 1000.0), siop->max_latency);
            Fprintf(dip, "  FD: %d, op: %s, file: %s\n",
                    fd, (reading) ? "read" : "write", dip->di_dname);
	    (void)ExecuteTrigger(dip, "latency");
            status = FAILURE;
            break;
        }
        if (latency > stip->max_latency) {
            stip->max_latency = latency;
        }
        if (latency < stip->min_latency) {
            stip->min_latency = latency;
        }
        stip->latency += latency;

        if (stip->io_completes) {
            /* 
             * Sum of the squares of the difference between average and current latency.
             * We keep track of this so that we can calculate stddev at any time.
             */
            stip->sumofsquares_latency += (uint64_t)pow(((latency / 1000.0) - 
                  (((double)stip->latency / (double)stip->io_completes) / 1000.0)), 2);
        }

        /* Note: Used for both reads and writes! */
        if ((size_t)byte_count != cur_blk_sz) {
            /* Note: I/O errors and triggers are handled above. */
            if (byte_count != FAILURE) {
                sio_report_size_mismatch(dip, reading, curblk, buffer, offset, cur_blk_sz, byte_count);
            }
            if (siop->iofailok == True) {
                Printf(dip, "Ignoring error. Stopping I/O to that file.\n");
                status = SUCCESS;
                break;
            } else {
                status = FAILURE;
                break;
            }
        } else {
            if ( (reading == False) && (siop->verify == True) ) {
                status = sio_verify_write(dip, fd, target_device, stip->records,
					  curblk, buffer, cur_blk_sz, offset);
                if ( (status == FAILURE) && (siop->break_on_dc == True) ) break;
            }
            stip->io_completes++;
        }

        gettimeofday(&loop_end_time, NULL);

        /* Inject a delay to attain I/O's per second, or user specified. */
        if (siop->target_iops > 0) {
            loop_usecs = sio_get_usecs(loop_end_time, loop_start_time);
            target_total_usecs += (unsigned long int)siop->think_time; 
            actual_total_usecs += loop_usecs;
            if (target_total_usecs > actual_total_usecs)  {
                usleep( (unsigned int)(target_total_usecs - actual_total_usecs) );
            }
        } else if (siop->think_time) {
            if (siop->think_time == RANDOM_DELAY_VALUE) {
                cur_think_time = (RAND(dip) % 10000);
            } else {
                cur_think_time = siop->think_time; 
            }
            if (cur_think_time) os_msleep(cur_think_time);
        }

        numops++;
        if (siop->numops && (numops >= siop->numops)) {
            break;
        }
    } /* end while(True) */
    return(status);
}

/*
 * Name:    do_fillonce() - Fill a file. (sequential writes to limit specified)
 *
 * Arguments:
 *  dip = The device information pointer.
 *
 * Description:
 *  Called from main if -fillonce option is specified. time and threadnumber 
 *  are ignored when -fillonce is specified. One thread is created per device
 *  specified and it takes care of writing to the device from start to end once.
 */
int
sio_dofillonce(dinfo_t *dip)
{
    sio_information_t *sip = dip->di_opaque;
    sio_thread_info_t *stip = &sip->sio_thread_info;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_global_data_t *sgdp = dip->di_job->ji_opaque;
    int threadnum = (dip->di_thread_number - 1);
    HANDLE fd = dip->di_fd;
    void *buffer = dip->di_data_buffer;
    Offset_t curblk;
    ssize_t byte_count;
    size_t cur_blk_sz;
    register Offset_t offset;
    hbool_t reading = False;
    hbool_t wrapped = False;
    int pattern;
    int status = SUCCESS;

    if (sgdp) {
        curblk = sgdp->global_curblk;
    } else {
        curblk = stip->begin_blk;
    }
    stip->per_thread_curblk = curblk;

    if (dip->di_variable_flag == True) {
        cur_blk_sz = siop->max_blksize;
    } else {
        cur_blk_sz = dip->di_block_size;
    }
    if (siop->fixedfill != -1) {
        pattern = siop->fixedfill;
    } else {
        pattern = threadnum;
    }
    /* Note: Single byte pattern. */
    memset(buffer, pattern, cur_blk_sz);

    if ( (siop->blockno == True) || (dip->di_debug_flag == True) ) {
        Printf(dip, "Filling file %s, blocks "LUF" - "LUF" with byte %d...\n",
               dip->di_dname, curblk, stip->end_blk, pattern);
    }

    while (True) {

        PAUSE_THREAD(dip);
        if ( THREAD_TERMINATING(dip) ) break;
        if (dip->di_terminating) break;

        if (sgdp) {
	    /* This state is required for -prefill option! */
	    if (sgdp->fillonce_done == True) break;
            //wrapped = sio_global_sequential_block(dip, &curblk, siop, stip, sgdp);
            curblk = sgdp->global_curblk++;
            if (curblk >= stip->end_blk) {
                curblk = stip->begin_blk;
                dip->di_pass_count++;
                sgdp->pass_count++;
                sgdp->global_curblk = curblk;
                wrapped = True;
            }
            if (sgdp->pass_count > dip->di_pass_count) {
                dip->di_pass_count++;
                wrapped = True;
            }
        } else {
            //wrapped = sio_sequential_block(dip, &curblk, stip);
            curblk = stip->per_thread_curblk++;
            if (curblk >= stip->end_blk) {
                dip->di_pass_count++;
                curblk = stip->begin_blk;
                stip->per_thread_curblk = curblk;
                wrapped = True;
            }
        }
        if (wrapped == True) break;
        dip->di_offset = offset = (curblk * cur_blk_sz);
        if (siop->blockno == True) {
	    sio_report_record(dip, reading, (stip->records + 1), curblk, buffer, offset, cur_blk_sz);
        }
        ENABLE_NOPROG(dip, WRITE_OP);
        byte_count = pwrite(fd, buffer, cur_blk_sz, offset);
        DISABLE_NOPROG(dip);
        if (dip->di_history_size) {
            unsigned long file_number = 1;
            save_history_data(dip,
                              file_number, (unsigned long)(stip->records + 1), WRITE_MODE,
                              offset, buffer, cur_blk_sz, byte_count);
        }
        if (byte_count == FAILURE) {
            ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_PWRITE_FILE_OP, WRITE_OP, True);
            sio_report_io_information(dip, reading, curblk, buffer, offset, cur_blk_sz, byte_count);
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
		(void)ExecuteTrigger(dip, "write");
	    }
        } else {
            stip->io_completes++;
            stip->bytes_written += byte_count;
            stip->writes++;
	    stip->records++;
        }
        if ((size_t)byte_count != cur_blk_sz) {
            if (byte_count != FAILURE) {
                sio_report_size_mismatch(dip, reading, curblk, buffer, offset, cur_blk_sz, byte_count);
            }
            status = FAILURE;
            break;
        }
    }
    if (sgdp) {
	/* Let other threads know we are done! (for -prefill) */
	sgdp->fillonce_done = True;
    }
    /* Flush the file system data to detect write failures! */
    if (dip->di_fsync_flag == True) {
	int rc = dt_flush_file(dip, dip->di_dname, &dip->di_fd, NULL, True);
	if (rc == FAILURE) status = rc;
    }
    if ( (siop->blockno == True) || (dip->di_debug_flag == True) ) {
	Printf(dip, "Filling of file %s is complete!\n", dip->di_dname);
    }
    return(status);
}

void
sio_reset_stats(dinfo_t *dip, sio_thread_info_t *stip)
{
    dip->di_pass_count = 0;

    stip->reads = 0;
    stip->bytes_read = 0;
    stip->writes = 0;
    stip->bytes_written = 0;
    stip->records = 0;
    stip->latency = 0;
    stip->interval_latency = 0;
    stip->io_completes = 0;
    stip->interval_io_completes = 0;
    stip->max_latency = 0;
    stip->min_latency = 0;
    stip->interval_max_latency = 0;
    stip->interval_min_latency = 0;
    stip->sumofsquares_latency = 0;
    stip->interval_sumofsquares_latency = 0;
    return;
}

/*
 * Name:    sio_get_usecs
 *
 * Arguments:
 * time2
 * time1
 *
 * Description:
 *  Return the difference in usecs between time2 and time1
 */
unsigned long int
sio_get_usecs(struct timeval time2, struct timeval time1)
{
    unsigned long int temp = 0;

    if (time2.tv_sec == time1.tv_sec) {
        return (time2.tv_usec - time1.tv_usec);
    }
    if (time2.tv_sec == (time1.tv_sec + 1)) {
        temp = (uSECS_PER_SEC - time1.tv_usec);
        temp += time2.tv_usec;
    } else {
        temp = ((time2.tv_sec - 1) - time1.tv_sec) * uSECS_PER_SEC;
        temp += (uSECS_PER_SEC - time1.tv_usec);
    }
    return(temp);
}

/*
 * Name:    sio_check_pattern_buffer
 *
 * Arguments:
 *  target_device    Target device on which the i/o was performed  .
 *  bufP             Buffer read back which is to be verified.
 *  offset           block number - used in calculating the pattern.
 *  iosize           Number of blocks (usually one).
 *  blocksize        The current block size (in bytes).
 *  break_on_error   Exit the function on encountering the first error.
 *
 * Description:
 *  Verifies the buffer contents read back.
 */
int
sio_check_pattern_buffer(dinfo_t *dip,
                         int target_device, char *bufP,
                         BlockNum_t offset, size_t iosize, size_t blocksize,
                         int break_on_error)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    int word_nbr = 0;
    int *tmpP = (int *) bufP;
    BlockNum_t blk_nbr;
    int actual, pattern;
    int *PREtmpP = (int *) bufP;
    int firsterror, isfirst = 0;
    hbool_t instrumentation = siop->instrumentation;
    char *pattbuf;
    int errors = 0;

    pattbuf = Malloc(dip, iosize * blocksize);
    if (pattbuf == NULL) {
        return(FAILURE);
    }
    /* Allow for more than one pattern eventually */
    for (blk_nbr = offset; blk_nbr < (offset + (Offset_t)iosize); blk_nbr++) {
        if ( THREAD_TERMINATING(dip) ) break;
        for (word_nbr = 0; word_nbr < (int)(blocksize / sizeof(int)); word_nbr++) {
            if ( THREAD_TERMINATING(dip) ) break;
            actual = *tmpP;
            if ( (instrumentation == True) || (dip->di_variable_flag == True) ) {
                pattern = (int)PATTERN_B(blk_nbr, word_nbr, (blocksize) / (sizeof(int)));
            } else {
                pattern = (int)PATTERN_A(blk_nbr, word_nbr, target_device);
            }
            if ( (actual != pattern) && 
                 ( (instrumentation == False) || ( (instrumentation == True) && (word_nbr != 1) ) ) ) {
                if (isfirst == 0) { 
                    firsterror = word_nbr; 
                    isfirst = 1;
                }
		if (errors == 0) {
		    sio_report_miscompare_information(dip, blocksize, (firsterror * sizeof(int)) );
		}
                sio_report_data_compare_error(dip, target_device, pattern, actual,
                                              (int)*tmpP, blk_nbr, (int)word_nbr);
                errors++;
                if (break_on_error) {
                    break;
                }
            }
            PREtmpP = tmpP;
            tmpP++;
        }    
    }
    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
	 (dip->di_trigger_control == TRIGGER_ON_MISCOMPARE) ) {
	(void)ExecuteTrigger(dip, miscompare_op);
    }
    Free(dip, pattbuf);
    return(errors);
}

/*
 * Name:    sio_check_fixed_val_buffer
 *
 * Arguments:
 *  target_device    Target device on which the i/o was performed  .
 *  bufP             Buffer read back which is to be verified.
 *  value            8 bit value that the block readback should contain.
 *  iosize           Number of blocks (usually one).
 *  blocksize        The current block size (in bytes).
 *  break_on_error   Exit the function on encountering the first error.
 *  offset           block number - used in calculating the pattern.
 *
 * Description:
 *  Verifies the buffer contents read back.
 */
int
sio_check_fixed_val_buffer(dinfo_t *dip,
                           int target_device, char *bufP, u_char value,
                           size_t iosize, size_t blocksize, int break_on_error,
                           Offset_t offset)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    u_char *tmpP = (u_char *)bufP;
    u_char actual;
    int firsterror, isfirst = 0;
    int fixedfill_value = siop->fixedfill;
    char *pattern;
    int errors = 0;
    int i = 0;

    pattern = Malloc(dip, (iosize * blocksize));
    if (pattern == NULL) {
        return(FAILURE);
    }

    /* Allow for more than one pattern eventually */
    for (i = 0; i < (int)(iosize * blocksize); i++) {
        if ( THREAD_TERMINATING(dip) ) break;
        actual = *tmpP;
        if (actual != (unsigned)value) {
            if (isfirst == 0) { 
                firsterror = (i / 4); 
                isfirst = 1;
            }
	    if (errors == 0) {
		sio_report_miscompare_information(dip, blocksize, (firsterror * sizeof(int)) );
	    }
            sio_report_data_compare_error(dip, target_device, fixedfill_value,
                                          actual, (int)*tmpP, offset, i);
            if (break_on_error) {
                errors = -1;
                break;
            }
            errors++;
        }
        tmpP++;
    }
    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
	 (dip->di_trigger_control == TRIGGER_ON_MISCOMPARE) ) {
	(void)ExecuteTrigger(dip, miscompare_op);
    }
    Free(dip, pattern);
    return(errors);
}

void
sio_report_record(dinfo_t *dip, hbool_t reading, uint64_t record,
		  BlockNum_t curblk, void *buffer, Offset_t offset, size_t bytes)
{
    sio_information_t *sip = dip->di_opaque;
    sio_thread_info_t *stip = &sip->sio_thread_info;
    
    /* Note: Omitting buffer address, since it's always the same! */
    // (reading == True) ? (stip->reads + 1) : (stip->writes + 1),
    Printf(dip, "Record #" LUF " - %s " SUF " bytes, block " LUF ", offset " LUF "\n",
           record, (reading == True) ? "Reading" : "Writing",
           bytes, curblk, offset);
    return;
}

void
sio_report_io_information(dinfo_t *dip, hbool_t reading, BlockNum_t block,
                          void *buffer, Offset_t offset, size_t expected, ssize_t received)
{
    int flags = (PRT_NOLEVEL|PRT_SYSLOG);
    
    /* Report common device information. */
    if (dip->di_extended_errors == False) {
	report_device_information(dip);
    }
    ReportErrorNumber(dip);
    LogMsg(dip, dip->di_efp, logLevelError, flags,
           "  op: %s, block: " FUF ", offset: " FUF ", expected: " SUF ", received: " SDF "\n",
           (reading) ? "read" : "write", block, offset, expected, received);
    return;
}

void
sio_report_size_mismatch(dinfo_t *dip, hbool_t reading, BlockNum_t block,
                         void *buffer, Offset_t offset, size_t expected, ssize_t received)
{
    int flags = (PRT_NOLEVEL|PRT_SYSLOG);
    
    ReportErrorNumber(dip);
    LogMsg(dip, dip->di_efp, logLevelError, flags,
           "Actual I/O size doesn't match the requested I/O size!\n");
    sio_report_io_information(dip, reading, block, buffer, offset, expected, received);
    return;
}

/*
 * Name:    sio_report_data_compare_error
 *
 * Arguments:
 *  dip            The device information pointer.
 *  target_device  Target device on which the i/o was performed  .
 *  expected       expected value.
 *  actual         value read back.
 *  recheck        value read back.
 *  blk_nbr        block number on which error occured.
 *  offset         offset into the block.
 *
 * Description:
 *  Prints data comparision error.
 */
void
sio_report_data_compare_error(dinfo_t *dip,
                              int target_device,
                              int expected, int actual, int recheck,
                              Offset_t blk_nbr, int offset)
{
    struct timeval timeval;
    gettimeofday(&timeval, NULL);
    Fprintf(dip, 
             "Time %ld: DATA COMPARE ERROR device: %s block nbr: " FUF " offset: %d expected: %08x  actual: %08x  recheck: %d\n",
             timeval.tv_sec, dip->di_dname,  
             blk_nbr, offset, expected, actual, recheck);
}

void
sio_report_miscompare_information(dinfo_t *dip, size_t blocksize, int buffer_index)
{
    if (dip->di_extended_errors == True) {
	INIT_ERROR_INFO(eip, dip->di_dname, miscompare_op, READ_OP, &dip->di_fd,
			dip->di_oflags,	dip->di_offset, blocksize, (os_error_t)0,
			logLevelError, PRT_SYSLOG, RPT_NOERRORMSG);
	ReportErrorNumber(dip);
	dip->di_mode = READ_MODE;
	dip->di_buffer_index = (uint32_t)buffer_index;
	ReportExtendedErrorInfo(dip, eip, NULL);
    } else {
	RecordErrorTimes(dip, True);
	ReportDeviceInfo(dip, blocksize, (uint32_t)buffer_index, False, MismatchedData);
	if (dip->di_history_size) dump_history_data(dip);
    }
    return;
}

/*
 * Name:    sio_fill_pattern_buffer
 *
 * Arguments:
 * bufP            Buffer read back which is to be verified.
 * offset        block number - used in calculating the pattern.
 * iosize        Number of blocks (usually one).
 * blocksize        sizeof the block.
 * dev_nbr        Target device on which the i/o was performed  .
 *
 * Description:
 *  Fills a buffer with a pattern.
 */
void
sio_fill_pattern_buffer(dinfo_t *dip,
                        char *bufP, BlockNum_t offset, size_t iosize,
                        size_t blocksize, int dev_nbr)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    int word_nbr = 0;
    int *tmpP = (int *)bufP;
    BlockNum_t blk_nbr;
    hbool_t instrumentation = siop->instrumentation;

    /* Allow for more than one pattern eventually */
    for (blk_nbr = offset; blk_nbr < (offset + (Offset_t)iosize); blk_nbr++) {
        for (word_nbr = 0; word_nbr < (int)(blocksize / sizeof(int)); word_nbr++) {
            if ( (instrumentation == True) || (dip->di_variable_flag == True) ) {
                *tmpP = (int)PATTERN_B(blk_nbr, word_nbr, (blocksize / sizeof(int)) );
            } else {
                *tmpP = PATTERN_A(blk_nbr, word_nbr, dev_nbr);
            }
            tmpP++;
        }
    }
    return;
}

/*
 * Name:    sio_random_block
 *
 * Description:
 *    Selects a random block for doing I/O.
 *
 * Inputs:
 *    dip = The device information pointer.
 *    threadnum = The thread number.
 *    target = The target block number pointer.
 * 
 * Outputs:
 *    target contains the random block number.
 */
hbool_t
sio_random_block(dinfo_t *dip,
                 BlockNum_t *target,
                 sio_thread_info_t *stip)
{
    BlockNum_t blk_range_size;

    /*
     * Please Note: The block range is intentionally one block short
     * of the actual block range, to ensure we don't return a random
     * block number starting at the end of the range, and thus either
     * write past the size specified and/or read an EOF (which fails)!
     */ 
    blk_range_size = (stip->end_blk - stip->begin_blk);
    if (blk_range_size) {
	(*target) = (BlockNum_t)( RAND64(dip) % blk_range_size );
	(*target) += stip->begin_blk;
    } else {
	(*target) = stip->begin_blk;
    }
    return(True);
}

hbool_t
sio_sequential_block(dinfo_t *dip,
                     BlockNum_t *target,
                     sio_thread_info_t *stip)
{
    hbool_t wrapped = False;
    register BlockNum_t curblk;
    int status = SUCCESS;

    /*
     * Get next sequential block number to use.
     */
    curblk = stip->per_thread_curblk++;
    if (curblk >= stip->end_blk) {
        curblk = stip->begin_blk;
        dip->di_pass_count++;
        stip->per_thread_curblk = curblk;
        wrapped = True;
        //Printf(dip, "wrapped...\n");
    }
    *target = curblk;
    return(wrapped);
}

hbool_t
sio_global_sequential_block(dinfo_t *dip,
                            BlockNum_t *target,
                            sio_parameters_t *siop,
                            sio_thread_info_t *stip,
                            sio_global_data_t *sgdp)
{
    register BlockNum_t curblk;
    hbool_t wrapped = False;
    int status = SUCCESS;

    /* Note: Without the lock, the global data is racey! */

    /*
     * Get next global sequential block number to use.
     */
    if (siop->iomutex == True) {
        status = sio_acquire_global_lock(dip, sgdp);
    }
    curblk = sgdp->global_curblk++;
    if (curblk >= stip->end_blk) {
        curblk = stip->begin_blk;
        dip->di_pass_count++;
        sgdp->pass_count++;
        sgdp->global_curblk = curblk;
        wrapped = True;
    }
    *target = curblk;

    /* 
     * If another thread wrapped, adjust this thread too!
     */
    if (sgdp->pass_count > dip->di_pass_count) {
        wrapped = True;
        dip->di_pass_count++;
    }
    if ( (siop->iomutex == True) && (status == SUCCESS) ) {
        status = sio_release_global_lock(dip, sgdp);
    }
    return(wrapped);
}

/*
 * Name:    sio_verify_write
 *
 * Arguments:
 *  dip              The device information pointer.
 *  fd               The file descriptor.
 *  target_device    Target device on which the I/O was performed.
 *  curblk           The current block number.
 *  buffer           Buffer to be used to verify I/O.
 *  blocksize        The current block size (in bytes).
 *  offset           The offset - used in calculating the pattern.
 *
 * Description:
 *  Reads back a block and verifies the data on it.
 * 
 * Return Value:
 *  Returns SUCCESS / FAILURE
 */
int
sio_verify_write(dinfo_t *dip,
                 HANDLE fd, int target_device, uint64_t record,	BlockNum_t curblk,
		 char *buffer, size_t blocksize, Offset_t offset)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_thread_info_t *stip = &sip->sio_thread_info;
    int retries = siop->verify_retry;
    ssize_t byte_count;
    hbool_t reading = True;
    int errors = 0;

again:
    if (siop->blockno == True) {
        sio_report_record(dip, reading, record, curblk, buffer, offset, blocksize);
    }
    ENABLE_NOPROG(dip, READ_OP);
    byte_count = pread(fd, buffer, blocksize, offset);
    DISABLE_NOPROG(dip);
    if (dip->di_history_size) {
        unsigned long file_number = 1;
        save_history_data(dip,
                          file_number, (unsigned long)record, READ_MODE,
                          offset, buffer, blocksize, byte_count);
    }
    if (byte_count == FAILURE) {
        ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_PREAD_FILE_OP, READ_OP, True);
        sio_report_io_information(dip, reading, curblk, buffer, offset, blocksize, byte_count);
	if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
	     (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
	    (void)ExecuteTrigger(dip, "read");
	}
    } else {
        stip->bytes_read += byte_count;
        stip->reads++;
    }

    if ((size_t)byte_count != blocksize) {
        Fprintf(dip,
                "Unable to read block at " LUF " to verify, expected " SUF " bytes, received " SDF "\n",
                offset, blocksize, byte_count);
        if (retries > 0) {
            Fprintf(dip, "Verify error, retrying read.\n");
            retries--;
            goto again;
        }
        return(FAILURE);
    }

    if (siop->fixedfill != -1) {
        errors = sio_check_fixed_val_buffer(dip, target_device, buffer,
                                            (u_char)siop->fixedfill, 1, blocksize,
                                            siop->break_on_dc, offset);
    } else {
        errors = sio_check_pattern_buffer(dip, target_device, buffer,
                                          (size_t)(offset / blocksize), 1,
                                          blocksize, siop->break_on_dc);
    }

    if (errors) {
        if (retries > 0) {
            Fprintf(dip, "Verify error, retrying\n");
            retries--;
            goto again;
        }
        if (siop->iofailok == True) {
            Printf(dip, "Ignoring verify error. Stopping I/O to that file.\n");
            /* Return success below! */
        } else {
            Fprintf(dip, "Verify failed!\n");
            return(FAILURE);
        }
    }
    return(SUCCESS);
}

void *
sio_thread(void *arg)
{
    dinfo_t *dip = arg;
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_thread_info_t *stip = &sip->sio_thread_info;
    sio_global_data_t *sgdp = dip->di_job->ji_opaque;
    hbool_t file_exists = False;
    int status = SUCCESS;

    status = do_common_thread_startup(dip);
    if (status == FAILURE) goto thread_exit;

    if (dip->di_debug_flag || dip->di_tDebugFlag) {
        Printf(dip, "Starting Sio, Job %u, Thread %u, Thread ID "OS_TID_FMT"\n",
               dip->di_job->ji_job_id, dip->di_thread_number, (os_tid_t)pthread_self());
    }

    /*
     * Handle setup for multiple slices.
     */ 
    if (dip->di_slice_number) {
        status = init_slice(dip, dip->di_slice_number);
    } else if (dip->di_slices) {
        status = init_slice(dip, dip->di_thread_number);
    }
    if (status == FAILURE) goto thread_exit;

    status = setup_thread_names(dip);
    if (status == FAILURE) goto thread_exit;
    handle_file_dispose(dip);
    
    if (dip->di_fsfile_flag == True) {
        /*
         * The file must exist for all reads!
         */
        file_exists = os_file_exists(dip->di_dname);
        if (siop->read_percentage == 100) {
            if (file_exists == False) {
                if (siop->prefill == UNINITIALIZED) {
                    siop->prefill = True;
                }
                if ( (siop->prefill == True) &&
                     (dip->di_verbose_flag == True) && (dip->di_thread_number == 1) ) {
                    Wprintf(dip, "File %s does *not* exist, so will be prefilled for reading.\n",
                            dip->di_dname);
                }
            }
        }
        if (siop->prefill == UNINITIALIZED) {
            siop->prefill = False;  /* Set to its' original default! */
        }
    }

    status = sio_thread_setup(dip);
    if (status == FAILURE) goto thread_exit;

    PAUSE_THREAD(dip);
    if ( THREAD_TERMINATING(dip) ) goto thread_exit;

    if (dip->di_fd == NoFd) {
        /* Allow enable read/write mode, otherwise Linux locks fail! */
        dip->di_initial_flags &= ~OS_WRITEONLY_MODE;
        dip->di_initial_flags |= OS_READWRITE_MODE;
        status = (*dip->di_funcs->tf_open)(dip, dip->di_initial_flags);
        if (status == FAILURE) goto thread_exit;
        dip->di_open_flags &= ~O_CREAT; /* Only create on first open. */
    }
    /* 
     * Right now, we are creating a file per thread, so ensure the file
     * is extended to the requested size to avoid read failures (EOF).
     */ 
    if ( (siop->read_percentage != 0) &&
         (siop->fillonce == False) && (siop->prefill == False) ) {
        status = sio_read_sanity_checks(dip);
        if (status == FAILURE) goto thread_exit;
    }

    status = sio_post_open_setup(dip);
    if (status == FAILURE) goto thread_exit;

    /* 
     * Set an end runtime in case fill once takes a long time! 
     */
    if (dip->di_runtime > 0) {
	dip->di_runtime_end = time((time_t *)NULL) + dip->di_runtime;
    }

    /*
     * We do prefill here, since we do *not* wish to time this!
     */
    if (siop->prefill == True) {
        status = sio_dofillonce(dip);
	if ( THREAD_TERMINATING(dip) ) goto thread_exit;
        sio_reset_stats(dip, stip);
    }
    if (siop->fillonce == False) {
        sio_init_data_buffer(dip, siop);
    }

    dip->di_start_time = times(&dip->di_stimes);
    ftime(&stip->begin_time);

    if (siop->fillonce) {
        status = sio_dofillonce(dip);
    } else {
        status = sio_doio(dip);
    }

    if (dip->di_shared_file == False) {
        status = (*dip->di_funcs->tf_close)(dip);
    }
    ftime(&stip->end_time);
    dip->di_end_time = times(&dip->di_etimes);

thread_exit:
    do_common_thread_exit(dip, status);
    /*NOT REACHED*/
    return(NULL);
}

int
sio_extend_file(dinfo_t *dip)
{
    HANDLE fd = dip->di_fd;
    void *buffer = dip->di_data_buffer;
    size_t request_size = dip->di_block_size;
    ssize_t bytes_written;
    Offset_t offset = (dip->di_data_limit - request_size);
    int status = SUCCESS;

    if (dip->di_data_limit < request_size) return (status);
    if (dip->di_slices) {
        offset += dip->di_file_position;
    }
    if (dip->di_debug_flag) {
        Printf(dip, "Extending file to " LUF " bytes, by writing "SUF" bytes at offset " FUF "...\n",
               dip->di_data_limit, request_size, offset);
    }
    bytes_written = pwrite(fd, buffer, request_size, offset);
    if ((size_t)bytes_written != request_size) {
        BlockNum_t curblk = (BlockNum_t)(offset / dip->di_block_size);
        hbool_t reading = False;
        dip->di_offset = offset;
        ReportErrorInfo(dip, dip->di_dname, os_get_error(), OS_PWRITE_FILE_OP, WRITE_OP, True);
        sio_report_io_information(dip, reading, curblk, buffer, offset, request_size, bytes_written);
	if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
	     (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
	    (void)ExecuteTrigger(dip, "write");
	}
        status = FAILURE;
    } else {
        Offset_t npos = set_position(dip, (Offset_t)0, False);
        if (npos == (Offset_t)-1) status = FAILURE;
    }
    return (status);
}

void
sio_init_data_buffer(dinfo_t *dip, sio_parameters_t *siop)
{
    uint32_t pattern;
    char *pp = (char *)&pattern;

    *pp = 'r'; pp++;
    *pp = 'b'; pp++;
    *pp = 'c'; pp++;
    *pp = 'w'; pp++;
    /* Note: The block size is already set to the max size we will use! */
    init_buffer(dip, dip->di_data_buffer, dip->di_block_size, pattern);
    return;
}

int
sio_thread_setup(dinfo_t *dip)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_thread_info_t *stip = &sip->sio_thread_info;

    stip->dip = dip;

    if (dip->di_random_seed == 0) {
        dip->di_random_seed = os_create_random_seed();
    }
    set_rseed(dip, dip->di_random_seed);

    /*
     * Note: We delay this setup so the file or disk capacity can be queried
     * after being open'ed, to set the data limit when not specified by the user.
     */
    if (dip->di_file_position) {
        stip->begin_blk = (dip->di_file_position / dip->di_block_size);
    }
    stip->end_blk = (dip->di_data_limit / dip->di_block_size);
    if (dip->di_slices) {
        stip->end_blk += stip->begin_blk;
    }
    if (stip->end_blk < stip->begin_blk) {
	Eprintf(dip, "The begin block "FUF" is greater than the ending block "FUF"\n",
		stip->begin_blk, stip->end_blk);
	return(FAILURE);
    }
    
    /* Divide the number of operations across the threads. (closer to sio's way) */
    if (siop->numops && (dip->di_threads > 1) ) {
        uint64_t resid = (siop->numops % dip->di_threads);
        siop->numops /= dip->di_threads;
        if (resid && (dip->di_thread_number == 1) ) siop->numops += resid;
        /* It's up to user to ensure sufficient threads exist for the ops specified. */
        if (siop->numops == 0) siop->numops++;  /* Note: sio is not exact about this either! */
    }
        
    /* Show block range per thread, if multiple slices and debug is enabled. */
    if ( (siop->noheader == False) && dip->di_slices && dip->di_debug_flag) {
        Printf(dip, "Read: %d Rand: %d BlkSz: "SUF" BegnBlk: "LUF" EndBlk: "LUF" Secs: "TMF" Thread: %d Dev#: %d  %s\n",
               siop->read_percentage, siop->random_percentage,
               dip->di_block_size,
               stip->begin_blk, stip->end_blk, dip->di_runtime,
               dip->di_thread_number, dip->di_device_number, dip->di_dname);
    }
    return(SUCCESS);
}

int
sio_post_open_setup(dinfo_t *dip)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    int status = SUCCESS;
    
    if ( dip->di_fsfile_flag &&
         (siop->noflock == False) &&
         (dip->di_thread_number == 1) ) {
        int lock_type = F_RDLCK;
        Offset_t start, length;
        /* Lock to force cache off. */
        if (siop->lockall == True) {
            start = 0;
            length = 0;
        } else {
            start = 1;
            length = 1;
        }
        status = os_lock_file(dip->di_fd, start, length, lock_type);
        if (status == FAILURE) {
            os_perror(dip, "%s(%s) failed, Already locked by another process",
                      OS_LOCK_FILE_OP, dip->di_dname);
            }
    }
    return(status);
}

int
sio_read_sanity_checks(dinfo_t *dip)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_thread_info_t *stip = &sip->sio_thread_info;
    int status = SUCCESS;

    /*
     * Sanity checks when read operations are selected.
     */
    if ( dip->di_fsfile_flag &&
         (siop->read_percentage != 0) &&
         (siop->fillonce == False) && (siop->prefill == False) ) {
        if (os_file_exists(dip->di_dname) == False) {
            Eprintf(dip, "File %s does *not* exist, but is required for read operations.\n", dip->di_dname);
            return(FAILURE);
        } else {
            large_t data_limit = dip->di_data_limit;
            large_t filesize = os_get_file_size(dip->di_dname, dip->di_fd);
            /* Note: The data limit was adjusted for each slice! */
            if (dip->di_slices) data_limit += dip->di_file_position;
            if (filesize < data_limit) {
                if ( (dip->di_verbose_flag == True) && (dip->di_thread_number == 1) ) {
                    Printf(dip, "File size of "LUF" bytes, is less than your requested limit of "LUF" bytes.\n",
                           filesize, data_limit);
                }
                /* Note: We *must* prefill or reads never reach the underlying storage! */
                siop->prefill = True;
                if ( (dip->di_verbose_flag == True) && (dip->di_thread_number == 1) ) {
                    Wprintf(dip, "File will be filled once to populate with data for reading.\n");
                }
                /* Extend the file ac necessary for each thread. */
                //status = sio_extend_file(dip);
            }
        }
    }
    return(status);
}

void
sio_cleanup_information(dinfo_t *dip)
{
    sio_information_t *sip;
    sio_thread_info_t *stip;

    if ( (sip = dip->di_opaque) == NULL) {
        return;
    }
    stip = &sip->sio_thread_info;

    /* Do sio thread specific cleanup here... */

    Free(dip, sip);
    dip->di_opaque = NULL;
    return;
}

int
sio_clone_information(dinfo_t *dip, dinfo_t *cdip, hbool_t new_context)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_information_t *csip; /* clone */

    csip = Malloc(dip, sizeof(*csip));
    if (csip == NULL) return(FAILURE);
    cdip->di_opaque = csip;
    *csip = *sip;           /* Copy the original information. */
    
    /* Do sio thread specific cloning (if any) here... */

    return(SUCCESS);
}

int
sio_initialize(dinfo_t *dip)
{
    sio_information_t *sip;
    sio_parameters_t *siop;
    sio_thread_info_t *stip;

    sip = Malloc(dip, sizeof(*sip));
    if (sip == NULL) return(FAILURE);
    if (dip->di_opaque) {
        Free(dip, dip->di_opaque);
    }
    dip->di_opaque = sip;

    siop = &sip->sio_parameters;
    stip = &sip->sio_thread_info;

    siop->fileperthread     = SIO_DEFAULT_FILE_PER_THREAD;
    siop->fixedfill         = SIO_DEFAULT_FIXED_FILL;
    siop->instrumentation   = SIO_DEFAULT_INSTRUMENTATION;
    siop->lockall           = SIO_DEFAULT_LOCKALL;
    siop->noflock           = SIO_DEFAULT_NOFLOCK;
    siop->iomutex           = SIO_DEFAULT_IOMUTEX;
    siop->prefill           = UNINITIALIZED;
    siop->max_blocks        = SIO_DEFAULT_MAX_BLOCKS;
    siop->max_blksize       = SIO_DEFAULT_MAX_BLKSIZE;
    siop->prettyprint       = SIO_DEFAULT_PRETTY_PRINT;
    siop->random_alignment  = SIO_DEFAULT_RANDOM_ALIGN;
    siop->timer_resolution  = SIO_DEFAULT_TIMER_RESOLUTION;
    siop->verify            = SIO_DEFAULT_VERIFY_FLAG;

    stip->min_latency       = SIO_DEFAULT_MIN_LATENCY;
    
    dip->di_dispose_mode    = KEEP_FILE;
    dip->di_fileperthread   = siop->fileperthread;
    dip->di_pass_limit      = SIO_DEFAULT_PASS_LIMIT;
    /* 
     * Note: Don't set defaults, if options specified already! 
     */
    if (dip->di_runtime == 0) {
	dip->di_runtime = (time_t)SIO_DEFAULT_RUNTIME;
    }
    /* Note: dt's default (1 thread), matches our default! */
    if (dip->di_threads < SIO_DEFAULT_THREAD_COUNT) {
	dip->di_threads = SIO_DEFAULT_THREAD_COUNT;
    }
    /* Note: Not using dt's verify flag to avoid pattern buffer allocation. */
    dip->di_verify_flag = False;

    return(SUCCESS);
}

int
sio_validate_parameters(dinfo_t *dip)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;
    sio_thread_info_t *stip = &sip->sio_thread_info;

    if (dip->di_input_file) {
        siop->read_percentage = 100;
    }
    if  ( (siop->read_percentage == 100) && (siop->prefill == True) ) {
        Eprintf(dip, "Prefilling file(s) is *not* permitted when 100%% reads is chosen!\n");
        return(FAILURE);
    }
#if 0
    if ( dip->di_output_file || (siop->read_percentage < 100) ) {
        dip->di_raw_flag = True;        /* Open files for read/write! */
        /* TODO: Has side effect of allocating a verify buffer, so beware! */
        /* Note: This is important if/when we are using large data sizes! */
    }
#endif /* 0 */
    if (dip->di_record_limit && (dip->di_record_limit != INFINITY)) {
        siop->numops = dip->di_record_limit;
    } else if (siop->numops) {
        dip->di_record_limit = siop->numops;
    }
    if (siop->random_percentage) {
        dip->di_io_type = RANDOM_IO;
    }
#if 0
    if ( siop->random_percentage ||
         ((dip->di_data_limit == INFINITY) && (dip->di_record_limit == 0)) ) {
        dip->di_random_io = True; /* Forces existing file size to be used! */
        /* Note: This sends us down the FindCapacity() path, with no limit! */
    }
#endif /* 0 */
    if ( (siop->read_percentage == 0) || (siop->fillonce == True) ) {
        dip->di_dispose_mode = KEEP_FILE;
    }
    /* Enable sio's default of O_DSYNC, unless Direct I/O is enabled!    */
    /* Note Well: On Linux, O_DSYNC is equivalent to O_SYNC and O_RSYNC! */
    /*   One could argue that O_DSYNC should be disabled for 100% reads! */
    if ( (siop->no_dsync == False) && !(dip->di_open_flags & O_DIRECT) ) {
        dip->di_write_flags |= O_DSYNC;
    }
    if ( (siop->truncate == True) &&
         (siop->random_percentage == 0) && (siop->read_percentage == 0) ) {
	dip->di_write_flags |= O_TRUNC;	/* Truncate the output file. */
    }
    if ( (siop->fillonce == True) || (dip->di_variable_flag == True) ) {
        siop->no_performance = True;
    } else if ( (dip->di_iblock_size || dip->di_oblock_size) &&
		((dip->di_block_size != dip->di_iblock_size) &&
		 (dip->di_block_size != dip->di_iblock_size)) ) {
        siop->no_performance = True;
    }
    if ( (dip->di_verbose_flag == True) &&
         (siop->fillonce == False) && (siop->numops == 0) ) {
        if ( dip->di_pass_limit && (siop->random_percentage == 100) ) {
            Printf(dip, "Warning: The pass limit is *not* implemented with 100% random I/O.\n");
        } else if ( (dip->di_runtime <= 0) && 
                    ( (siop->random_percentage == 100) || (dip->di_pass_limit == 0) ) ) {
            Printf(dip, "Warning: No runtime, numops, or pass limit specified, so user must stop this run...\n");
        }
    }
    if (dip->di_slices) {
        dip->di_threads = dip->di_slices;
    } else if (siop->partition_among_threads) {
        dip->di_slices = dip->di_threads;
    }
    /* dt's multiple files enables sio's fileperthread option. */
    if (dip->di_file_limit) {
        if (dip->di_threads < (int)dip->di_file_limit) {
            if ( (dip->di_threads > 1) && dip->di_verbose_flag) {
                /* Note: This mislead me (the author), so I'm adding a warning! */
                Printf(dip, "Warning: Setting number of threads to the file limit %u!\n",
                       dip->di_file_limit);
            }
            dip->di_threads = dip->di_file_limit;
        }
        dip->di_file_limit = 0;
        siop->fileperthread = True;
        dip->di_fileperthread = siop->fileperthread;
    }
    /*
     * Check the variable record size parameters.
     */
    if (dip->di_variable_flag) {
        dip->di_max_size = siop->max_blksize;
    }
    /* Note: The proper (max) block size is used to allocate data buffers! */
    if (dip->di_min_size && !dip->di_max_size) dip->di_max_size = dip->di_block_size;
    if (dip->di_block_size < dip->di_max_size) dip->di_block_size = dip->di_max_size;
    /* Set the max block size, since used for block ranges. */
    if (dip->di_iblock_size && !dip->di_oblock_size) dip->di_oblock_size = dip->di_block_size;
    if (dip->di_oblock_size && !dip->di_iblock_size) dip->di_iblock_size = dip->di_block_size;
    dip->di_block_size = max(dip->di_block_size, dip->di_iblock_size);
    dip->di_block_size = max(dip->di_block_size, dip->di_oblock_size);

    if ( dip->di_file_position && (dip->di_file_position < (Offset_t)dip->di_block_size) ) {
        Eprintf(dip, "The start position "LUF" is less than the max block size "SUF"!\n",
               dip->di_file_position, dip->di_block_size);
        return(FAILURE);
    }
    if (dip->di_slices) {
        BlockNum_t blocks;
        blocks = ( ( (dip->di_data_limit - dip->di_file_position) / dip->di_block_size) );
        if ( blocks < (BlockNum_t)dip->di_slices ) {
            Eprintf(dip, "The number of slices %d is less than the number of calculated blocks "FUF"!\n",
                   dip->di_slices, blocks);
            Fprintf(dip, "Calculated via: ( (end offset "LUF" - start offset "FUF") / block size "SUF") = "SUF" blocks.\n",
                    dip->di_data_limit, dip->di_file_position, dip->di_block_size, blocks);
            return(FAILURE);
        }
    }
    return(SUCCESS);
}

void
sio_show_parameters(dinfo_t *dip)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;

    if (dip->di_debug_flag) {
        Lprintf(dip, "\nsio Parameters:\n");
        Lprintf(dip, "    filename...............: %s\n", dip->di_dname             );
        if (siop->read_percentage == -1) {
            Lprintf(dip, "    read percentage........: random\n"                    );
        } else {
            Lprintf(dip, "    read percentage........: %d\n", siop->read_percentage );
        }
        Lprintf(dip, "    random percentage......: %d\n", siop->random_percentage   );
        if (dip->di_variable_flag) {
            Lprintf(dip, "    block size.............: random\n"                    );
        } else {
            Lprintf(dip, "    block size.............: "SUF"\n", dip->di_block_size );
	    if (dip->di_iblock_size) {
		Lprintf(dip, "    read block size........: "SUF"\n", dip->di_iblock_size );
	    }
	    if (dip->di_oblock_size) {
		Lprintf(dip, "    write block size.......: "SUF"\n", dip->di_oblock_size );
	    }
        }
        Lprintf(dip, "    start position.........: "LUF"\n", dip->di_file_position  );
        Lprintf(dip, "    end position...........: "LUF"\n", dip->di_data_limit     );
        Lprintf(dip, "    break_on_dc............: %d\n", siop->break_on_dc         );
        Lprintf(dip, "    detailed logging.......: %d\n", siop->detailed_logging    );
        Lprintf(dip, "    fileperthread..........: %d\n", siop->fileperthread       );
        Lprintf(dip, "    fillonce...............: %d\n", siop->fillonce            );
        Lprintf(dip, "    fixedfill..............: %d\n", siop->fixedfill           );
        Lprintf(dip, "    iomutex................: %d\n", siop->iomutex             );
        Lprintf(dip, "    lockall................: %d\n", siop->lockall             );
        Lprintf(dip, "    max_blocks.............: " SUF "\n", siop->max_blocks     );
        Lprintf(dip, "    max_blksize............: " SUF "\n", siop->max_blksize    );
        Lprintf(dip, "    max_latency............: %d\n", siop->max_latency         );
        Lprintf(dip, "    no_dsync...............: %d\n", siop->no_dsync            );
        Lprintf(dip, "    noflock................: %d\n", siop->noflock             );
        Lprintf(dip, "    niceoutput.............: %d\n", siop->niceoutput          );
        Lprintf(dip, "    numops.................: "LUF"\n", siop->numops           );
        Lprintf(dip, "    partition among threads: %d\n", siop->partition_among_threads );
        Lprintf(dip, "    prefill................: %d\n", siop->prefill             );
        Lprintf(dip, "    prettyprint............: %d\n", siop->prettyprint         );
        Lprintf(dip, "    random align...........: "SUF"\n", siop->random_alignment );
        //Lprintf(dip, "    timer resolution.......: %dms\n", siop->timer_resolution  );
        if (siop->think_time == RANDOM_DELAY_VALUE) {
            Lprintf(dip, "    think time.............: random\n"                    );
        } else {
            Lprintf(dip, "    think time.............: %u\n", siop->think_time      );
        }
        Lprintf(dip, "    threads................: %d\n", dip->di_threads           );
        Lprintf(dip, "    truncate...............: %d\n", siop->truncate            );
        Lprintf(dip, "    runtime................: "TMF"\n", dip->di_runtime        );
        Lprintf(dip, "    verify.................: %d\n", siop->verify              );
        Lprintf(dip, "\n");
        Lflush(dip);
    }

    /* We save the block information here, since slices overwrites dip values. */
    siop->initial_begin_blk = (dip->di_file_position / dip->di_block_size);
    siop->initial_end_blk = (dip->di_data_limit / dip->di_block_size);

    Lprintf(dip, "Read: %d Rand: %d ", siop->read_percentage, siop->random_percentage);
    if (dip->di_iblock_size && dip->di_oblock_size) {
	Lprintf(dip, "iBlkSz: "SUF" oBlkSz: "SUF, dip->di_iblock_size, dip->di_oblock_size);
    } else if (dip->di_iblock_size) {
	Lprintf(dip, "BlkSz: "SUF" iBlkSz: "SUF, dip->di_block_size, dip->di_iblock_size);
    } else if (dip->di_oblock_size) {
	Lprintf(dip, "BlkSz: "SUF" oBlkSz: "SUF, dip->di_block_size, dip->di_oblock_size);
    } else {
	Lprintf(dip, "BlkSz: "SUF, dip->di_block_size);
    }
    Lprintf(dip, " BegnBlk: "LUF" EndBlk: "LUF" Secs: "TMF" Threads: %d Devs: %d  %s\n",
           siop->initial_begin_blk, siop->initial_end_blk,
           dip->di_runtime, dip->di_threads,
           dip->di_num_devs, dip->di_dname);
    Lflush(dip);
    
    if (dip->di_slices) {
        /* Use sio's notion of a block size for slice ranges! */
        /* FYI: Usually dt uses the device block size (for SAN). */
        /* Note: Cannot set this too early, or device size is set to this! */
        dip->di_dsize = (uint32_t)dip->di_block_size;
    }
    return;
}

#define P       Print

void
sio_help(dinfo_t *dip)
{
    sio_information_t *sip = dip->di_opaque;
    sio_parameters_t *siop = &sip->sio_parameters;

    P(dip, "Usage: %s iobehavior=sio [options...]\n\n", cmdname);
    P(dip, "sio (Simple I/O Load Generator) - NetApp\n");
    P(dip, "A tool to generate artificial I/O workloads against any device\n");
    P(dip, "Supports numerous configuration variables (reads vs writes, etc)\n");
    P(dip, "Supports multiple devices and multiple threads. Collects a wide\n");
    P(dip, "variety of statistics on I/O client machines and/or I/O servers.\n");
    P(dip, "\n");
    P(dip, "Basic Usage: \n");
    P(dip, "dt iobehavior=sio readp=<read%%> randp=<rand%%> bs=<blksz> starting=<start> \\\n");
    P(dip, "       end=<end> runtime=<secs> threads=<threads> devs=<dev>,[devs,...]\n");
    P(dip, "\n");
    P(dip, "readp=<read>          Percentage of accesses that are reads. Range [0,100].\n");
    P(dip, "                      'random' keyword makes the read/write percentage random.\n");
    P(dip, "                      BEWARE, writing to a file is unchecked and will trash files.\n");
    P(dip, "randp=<rand>          Percentage of acceses that are random. Range [0,100].\n");
    P(dip, "                      Sequential accesses = 0%%, else random percentage\n");
    P(dip, "bs=<blksz>            Size of I/O's. Example: 2k, 4k, 1m\n");
    P(dip, "                      'random' keyword makes the I/O size random 512 bytes to 262144 bytes.\n");
    P(dip, "ibs=<blksz>           Size of read requests. (overrides bs= option)\n");
    P(dip, "obs=<blksz>           Size of write requests.\n");
    P(dip, "start=<strt_byte>     Lower bound for access location in each file.\n");
    P(dip, "end=<file_size>       Total bytes accessed in each file (e.g. 100m, 2g, 1000k).\n");
    P(dip, "runtime=<seconds>     Runtime for test. Counting starts AFTER all threads have started.\n");
    P(dip, "threads=<numthreads>  Concurrent I/O generators. Uses real individual threads.\n");
    P(dip, "dev=<dev>             Device to access. May be file (foo.out) or device (/dev/dsk/etc).\n");
    P(dip, " or devs=<dev>[,...]  Multiple devices and/or files can be specified, comma separated.\n");
    P(dip, " or file=<paths>      One or more paths to files to access (synonym for 'devs' option).\n");
    //P(dip, "After <devs> a list of options may be given.\n");
    P(dip, "\n");
    P(dip, "Examples:\n");
    P(dip, " 1) Random 4k I/O with 25%% reads/75%% writes, 75%% random/sequential for 10 minutes.\n");
    P(dip, "    Accessing a total of 250 megabytes in each file, after prefilling the file.\n");
    P(dip, "\n");
    P(dip, "    %% dt iobehavior=sio file=a.file,b.file bs=4k readp=25 randp=75 end=250m -prefill runtime=10m\n");
    P(dip, "\n");
    P(dip, " 2) Random reads and writes with random block sizes via 10 threads to the same file.\n");
    P(dip, "    This test will run infinitely without -numops or runtime options.\n");
    P(dip, "\n");
    P(dip, "    %% dt iobehavior=sio file=a.file bs=random readp=random end=1g -direct -verify threads=100\n");
    P(dip, "\n");
    //P(dip, "Notes:\n");
    //P(dip, "This program supports these OS's: Windows, Solaris, Linux, HPUX, AIX\n");
    //P(dip, "\n");
    P(dip, "Options:\n");
    //P(dip, "    Options are divided into six categories:\n");
    P(dip, "    Options are divided into four categories:\n");
    P(dip, "        Basic Features, Advanced Features,\n");
    //P(dip, "        Host montioring, Filer monitoring.\n");
    P(dip, "        Q/A Features, and Esoteric Stuff.\n");
    P(dip, "\n");
    P(dip, "Basic Features:\n");
    P(dip, "    -help                 List this sio help, then exit.\n");
    P(dip, "    -version              Display this detailed version log.\n");
    //P(dip, "-nopread      do not use preads and pwrites, instead use reads and writes.\n");
    P(dip, "    -noflock              Do NOT lock files.  Locking affects caching on some OS's.\n");
    P(dip, "    -noheader             Suppress single line header output. (Good for multiple runs).\n");
    P(dip, "    -debug                Output detailed debug info. Be prepared for a lot of info.\n");
    P(dip, "    -Debug                Very verbose debug information. Be prepared for a lot of info!\n");
    P(dip, "    -niceoutput           Print output in single column, human-readable format.\n");
    P(dip, "    -prettyprint          Pretty print the output (this is dt's format).\n");
    P(dip, "    -no_dsync             Do NOT open files with O_DSYNC. Allows async writes.\n");
    P(dip, "    -noperf               Do NOT display performance statistics.\n");
    P(dip, "\n");
    P(dip, "Advanced Features:\n");
    P(dip, "    -stop=<fname>         Watch for existance of file 'fname' and terminate.\n");
    //P(dip, "-ramp -<sec>:         Allow sio to run 'sec' seconds before beginning stats.\n");
    P(dip, "    -think=<msec>         Each thread waits 'ms' MS before issuing each I/O.\n");
    P(dip, "    -iops=<rate>          Target IOPS for each thread.\n");
    P(dip, "    -lockall              Lock the complete file as opposed to a single byte.\n");
    //P(dip, "-create:              IFF pure sequential writes,\n");
    //P(dip, "                          create the initial file, if it does already exist.\n");
    P(dip, "    -truncate:            IFF pure sequential writes, then when I/O wraps to\n");
    P(dip, "                          beginning of file, the file is truncated.\n");
    //P(dip, "-globalfds:           All threads use same fd for a file. Allows more files.\n");
    P(dip, "    -max_blksize=<bytes>  Set maximum block size to 'bytes'.\n");
    P(dip, "    -max_latency=(ms)     Maximum allowed latency (in milliseconds) of an IO.\n");
    //P(dip, "-rep_latency -<secs>: Report latency every 'secs'.\n");
    P(dip, "    -direct               Disable filesystem caching.\n");
    //P(dip, "-mmap                 Use memory mapped I/O.\n");
    P(dip, "    -align=(size)         Alignment to be used with random block size.\n");
    P(dip, "    -break_on_dc          Exit upon detecting data corruption ASAP.\n");
    //P(dip, "-metadata             Run metadata test (open,write,close,etc) only.\n");
    //P(dip, "                       Expects filename to be target directory.\n");
    //P(dip, "                       Ignores r/w percentage.\n");
    //P(dip, "-iscsi                Collect iSCSI statistics from filer.\n");
    //P(dip, "-stats_reset -<fname>  Watch for existance of file 'fname' and reset global stats.\n");
    //P(dip, "                       This will affect the printed run time, but\n");
    //P(dip, "                       not the actual run time.\n");
    P(dip, "\n");
#if 0
    P(dip, "Host Monitoring\n");
    P(dip, "-nosysinfo            Disable all host side monitoring\n");
    P(dip, "\n");
    P(dip, "Filer Monitoring\n");
    P(dip, "-filer -<filer>       Collect stats on filer.\n");
    P(dip, "-mfiler -<filer>      Collect stats on filer with multiple cpus.\n");
    P(dip, "-rshuser -<name>      Use 'name' as username for rsh.\n");
    P(dip, "-rshhostfirst         On rsh command, specify host first.\n");
    P(dip, "-rshuserfirst         On rsh command, specify user first.\n");
    P(dip, "-no_statit            do not collect statit information from filer.\n");
    P(dip, "\n");
#endif /* 0 */
    P(dip, "Q/A Features:\n");
    P(dip, "    -verify               Read back written data and verify content.\n");
    //P(dip, "-hard                 Fill I/O buffers with valid Oracle 9iR2 block headers.\n");
    P(dip, "    -verify_retry=<n>     Retry failed verifies 'n' times.\n");
    P(dip, "    -instrumentation      Special pattern insertion technique.\n");
    //P(dip, "-detailed_logging     Save detailed error log files.\n");
    P(dip, "    -fixedfill=<value>    Fill the file with 8 bit value.\n");
    P(dip, "\n");
    P(dip, "Esoteric Stuff:\n");
    //P(dip, "-magic_trigger -<device>: Special trigger technique.\n");
    P(dip, "    -numops=<num_ops>     Run for 'num_ops' I/O's and stop. Beware stats.\n");
    P(dip, "    -fileperthread        Open one file per thread. Special names.\n");
    P(dip, "    -blockno              Prints out the I/O block numbers.\n");
    //P(dip, "-blockno                  Prints out the randomly selected block numbers.\n");
    //P(dip, "-openfailok               Ignore open failures (unless all opens fail).\n");
    P(dip, "    -iofailok             Allow I/O failures (do not access file again).\n");
    P(dip, "    -iomutex              Use mutex to synchronize multiple threads.\n");
    //P(dip, "-nolseek                  Remove lseek on sequential reads.\n");
    P(dip, "    -fillonce             Write all files once, then stop.\n");
    P(dip, "    -prefill              Write all files prior to test I/O.\n");
    //P(dip, "-nomutex                  Turn off mutex locking for systems w/o pread.\n");
    //P(dip, "-stream_per_thread        Independent sequential streams per-thread.\n");
    P(dip, "    -partition_among_threads Partition the file among threads.\n");
    //P(dip, "-partial_write_test       no help\n");
    //P(dip, "    -timer_resolution=(ms) Set system timer resolution to milliseconds (Windows).\n");
    P(dip, "\n");
    return;
}
