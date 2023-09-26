/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2023			    *
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
 * Module:	dt.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	Main line code for generic data test program 'dt'.
 *
 * Modification History:
 *
 * September 21th, 2023 by Robin T. Miller
 *      When specifying retryable errors, honor user specified error limit.
 * 
 * August 18th, 2023 by Robin T. Miller
 *      When a thread is hung, report total statistics before cancelling
 * the thread. These hangs often occur when all storage connections are lost
 * and the outstanding I/O is *not* completing (NFS mounts or iSCSI LUN, etc).
 *
 * November 15th, 2021 by Robin T. Miller
 *      Add support for dtapp, hammer, and sio I/O behaviors.
 * 
 * June 16th, 2021 by Robin T. Miller
 *      Add support for separate SCSI trigger device.
 * 
 * June 14th, 2021 by Robin T. Miller
 *      Add support for "limit=random" to automatically setup random limits.
 * 
 * May 24th, 2021 by Robin T. Miller
 *      When reporting the no-progress record number with read-after-write
 * enabled, do not include both reads and writes, so the record is accurate.
 * 
 * March 21st, 2021 by Robin T. Miller
 *      Add support for forcing FALSE data corruptions for debugging.
 * 
 * March 8th, 2021 by Robin T. Miller
 *      When creating log directories in setup_log_directory(), create the
 * last subdirectory (as required). For user specified log directory, expand
 * format control strings prior to creating the directory (logdir= option).
 * 
 * February 11th, 2021 by Robin T. Miller
 *      Create master log creation function for use by other tool parsers.
 * 
 * October 28th, 2020 by Robin T. Miller
 *      Add support for comma separated workload[s]= option.
 * 
 * October 27th, 2020 by Robin T. Miller
 *      When "trigger=" is specified, no parameters, then remove triggers.
 *      This empty check is consistent with other options, like prefix=, etc.
 * 
 * October 21st, 2020 by Robin T. Miller
 * 	Add array=string option, esp. for external trigger scripts.
 *      Add --trigger= and --workload= parsing for I/O behavior options.
 * 
 * September 24th, 2020 by Robin T. Miller
 *      Updated signal handler to avoid exiting with FAILURE on SIGINT (2) or
 * SIGTERM (15), esp. for the latter since automation often sends this signal
 * to kill tools. SIGINT and SIGTERM will now exit with SUCCESS.
 * 
 * August 5th, 2020 by Robin T. Miller
 *      Add support for starting slice offset.
 * 
 * July 8th, 2020 by Robin T. Miller
 *      When looking up alternate tool I/O behavior names, do string compare
 * with length, to avoid issues with Windows ".exe" file extension.
 * 
 * May 11th, 2020 by Robin T. MIller
 *      Add options for date and time field separator used when formatting
 * the log prefix format strings (e.g. "%ymd", "%hms").
 * 
 * May 7th, 2020 by Robin T. Miller
 *      Apply special step option handling to both disks and files, by setting
 * up and ending offset so I/O loops break reaching this offset.
 * 
 * April 8th, 2020 by Robin T. Miller
 *      Add flag for output position, so zero offset is handled properly.
 *      Note: This allows copying to a different offset from input file!
 * 
 * March 31st, 2020 by Robin T. Miller
 *      Add "showvflags=hex" option to show block tag verify flags set.
 * 
 * March 19th, 2020 by Robin T. Miller
 *      When selecting 100% reads (readp=100), switch output file to input file.
 *      This helps with automation desiring to do read verify only w/of= option!
 * 
 * March 7th, 2020 by Robin T. Miller
 *      Remove a file position sanity check no longer valid with updates made
 * in FindCapacity() with new logic associated with the file position option.
 * 
 * February 12th, 2020 by Robin T. Miller
 *      For Unix systems, increase the open file resource limit, now that low
 * limits are imposed on newer Linux distros, to avoid false failures!
 * 
 * February 11th, 2020 by Robin T. Miller
 *      Added "showfslba" and "showfsmap" commands to show the file system map,
 * for the file specified, assuming we can acquire FS extents to map physical LBAs.
 * 
 * Decamber 6th, 2019 by Robin T. Miller
 *      When re-enabling stats via "enable=stats", ensure all the stat flags
 * reset when disabled, get enabled once again, esp. since these are sticky.
 * FYI: Found script issue where stats were disabled, but not easily re-enabled!
 * 
 * November 21st, 2019 by Robin T. Miller
 *      Add a retry data corruption limit option (retryDC_limit=value).
 *      The normal retry limit is used for retrying errors (60 by default).
 *      Added separate retry data corruption delay option (retryDC_delay).
 *      Fix "dir=/var/tmp of=dt-%user-%uuid.data" expansion by updating the
 * base name properly in format_device_name() function. (see comments there).
 * 
 * November 20th, 2019 by Robin T. Miller
 *      In setup_thread_names(), with multiple threads create a unique directory
 * for each thread under the top level directory. Previously, when the top level
 * directory was a mount point, a new top level directory was created with "-jNtN"
 * so subsequent files and subdirectories were not created under the mount point.
 * So, now instead of say /var/tmp/dtdir-j1t1, it's now /var/tmp/dtdir/j1t1.
 * 
 * October 6th, 2019 by Robin T. Miller
 *      Change the default dispose mode setting to keep on error.
 * 
 * July 27th, 2019 by Robin T. Miller
 *	Change the delete error log file default to be true.
 *	For those running multiple processes, beware of this change!
 *
 * June 14th, 2019 by Robin T. Miller
 *      Fix bug in do_datatest_validate() where the pattern buffer was reset
 * causing pattern=incr and pattern=string not work (only 1st 4 bytes used).
 * 
 * June 6th, 2019 by Robin T. Miller
 *      Add logdir= option to prepend to job/log file paths.
 * 
 * May 27th, 2019 by Robin T. Miller
 *      Add support for capacity percentage for thin provisioned LUNs,
 * to help avoid exceeding backend storage for over-provisioned volumes.
 * 
 * May 20th, 2019 by Robin T. Miller
 *      Add support for appending a default file name to a directory.
 * 
 * May 21st, 2018 by Robin T. Miller
 *      Fix bug introduced with mounted files system check in copy mode.
 * Only the input file has an output file dinfo pointer, so add this check.
 * Failure to have this check results in a "Segmentation fault (core dumped)".
 * 
 * April 12th, 2018 by Robin T. Miller
 *      For Linux file systems with direct I/O, get the disk block size.
 * This disk block size is required for later sanity checks for direct I/O.
 * Failure to enforce DIO I/O sizes leads to EINVAL errors, misleading for
 * everyone (including this author). This occurs on disks with 4k sectors!
 * 
 * March 1st, 2018 by Robin T. Miller
 *      Add mounted file system check when writing to /dev/ disk devices.
 * Note: This is *not* full proof, since LVM devices are not handled, and
 * will not detect /dev/sd* when DM-MP /dev/mapper partitions are mounted.
 * But that said, this is better than what we did before.
 * FYI: This is Linux only right now, since I no longer have other OS's.
 * 
 * January 21st, 2017 by Robin T. Miller
 *	For Windows, parse <cr><lf> properly for continuation lines.
 *      Allows comments on continuation lines, including leading spaces.
 * 
 * December 21st, 2016 by Robin T. Miller
 *      Fix bug in MakeArgList() while parsing quoted strings, due to how
 * strcpy() copies overlapping strings. Must use our own copy loop!
 * 
 * December 13th, 2016 by Robin T. Miller
 *      Ensure the trailing log header and exit status go to the correct
 * output stream, stdout vs. stderr, for scripts separating these two.
 * 
 * November 4th, 2016 by Robin T. Miller
 *      Add enable/disable options for controlling job statistics.
 *      Added support to control the total statistics separately.
 *      Note: Pass/Total/Job statistics are controlled seperately,
 * making it easier to customize the type of statistics desired.
 * 
 * September 17th, 2016 by Robin T. Miller
 *      Fix parsing of quoted text in MakeArgList().
 *      Bug was terminaing with a NULL, rather than processing more
 * text in the string. What was I thinking? ;(
 * 
 * February 2nd, 2016 by Robin T. Miller
 * 	When dumping history, do so for multiple devices.
 *
 * January 7th, 2016 by Robin T. Miller
 * 	Implement the log append flag, we were always overwriting logs.
 * December 18th, 2015 by Robin T. Miller
 * 	Remove interactive check in main loop, which previously automatically
 * waited for jobs. Robin thought this was a convenience, but can/is misleading.
 *
 * December 16th, 2015 by Robin T. Miller
 * 	Expand variables in conditional part of ${var:${VAR}} parsing.
 * 	Add support for ${var:?Error text} when variable is not defined.
 * 	Save the workload name, so we can use %workload format string.
 * 
 * December 12th, 2015 by Robin T. Miller
 * 	When using read percentage, initialize the file once as required.
 * 	Added enable/disable-{fill_always,fill_once} to initialize files
 * prior to writing. This is useful for read percentage and debugging.
 * 
 * December 11th, 2015 by Robin T. Miller
 *	When direct I/O is enabled, automatically enable FS align flag.
 *	Most file systems require aligned file offsets with direct I/O.
 *
 * November 21th, 2015 by Robin T. Miller
 * 	Added support for read/write and sequential/random percentages.
 *
 * November 19th, 2015 by Robin T. Miller
 * 	Added higher resolution timer (gettimeofday) for start/pass/end times.
 * 
 * November 17th, 2015 by Robin T. Miller
 * 	When emitting prompt status, use the master thread exit status rather than
 * the global exit status. This avoids incorrect status when threads fail, rather
 * than the last command status and/or jobs waited upon.
 * 	Note: These changes are specifically for pipe and client modes.
 *
 * October 6th, 2015 by Robin T. Miller
 * 	Add support for variable data limit options:
 * 	min_limit=value max_limit=value incr_limit=value
 * 	Similar to variable block sizes, except without an increment,
 * the data limit will default to variable.
 * 
 * June 14th, 2015 by Robin T. Miller
 * 	Fix bug in cleanup_device() preventing freeing data buffers.
 * 	Move dt pattern initialize to do_datatest_initialize(), and perform
 * this *after* cloning the master to avoid wasted memory which is significant
 * with IOT pattern with large block sizes and/or large pattern files.
 *
 * June 13th, 2015 by Robin T. Miller
 * 	Update docopy() and domirror() to avoid pattern buffer setup.
 *
 * June 9th, 2015 by Robin T. Miller
 * 	Added support for block tags (btags).
 *
 * April 21st, 2015 by Robin T. Miller
 * 	Add support for an optional error file via errfile=filename option.
 * When specified, this file is open'ed in append mode for writing all errors.
 * 
 * March 9th, 2015 by Robin T. Miller
 * 	Add additional check when checking for directory exists, to ensure
 * the root failure (e.g. "permission denied") gets reported, and also to
 * avoid trying to create the directory since this is misleading to folks.
 *
 * March 7th, 2015 by Robin T. Miller
 * 	Add checks for iodir=vary and iotype=vary, when parsing additional
 * iodir/iotype parameters. Do not overwrite random I/O flag, if vary enabled.
 * Previously, iodir=vary with iotype=sequential, reset the random I/O flag,
 * which in the case of slices, prevented the random limit from being setup!
 * 
 * January 29th, 2015 by Robin T. Miller
 * 	Allow empty pattern options to undue thier affect, to allow this
 * method to overide previous pattern options. Reverts to their defaults.
 * 
 * December 19th, 2014 by Robin T. Miller
 * 	Fixed a problem in the dt I/O loop where a runtime=-1 was setting
 * the end runtime (incorrectly), causing dt threads to exit after 1 second.
 * The other I/O behaviors are correct, as are the other dt threads.
 * 
 * November 19th, 2014 by Robin T. Miller
 * 	Update make_options_string() to do a better job quoting options
 * containing whitespace, now used for defining workloads and dt command
 * line for logging.
 * 
 * November 4th, 2014 by Robin T. Miller
 * 	Made debug flags persistent ("sticky") for client/server mode.
 * 
 * October 18th, 2014 by Robin T. Miller
 * 	Add support for maxdatap= option for direct disks.
 *
 * October 10th, 2014 by Robin T. Miller
 * 	Added support for iomode=mirror.
 * 	Created common functions for doio() and domirror().
 *
 * October 1st, 2014 by Robin T. Miller
 * 	Modify I/O loops to honor the error limit regardless of the operation.
 * Previously, the error limit was not honored for open or delete failures.
 *
 * September 25th, 2014 by Robin T. Miller
 * 	Note; For NFS, unbuffered I/O is enabled to avoid false corruptions.
 * 	Added warning for file systems, when the free space is less than the
 * data limit required for dir(s) and file(s) specified.
 * 
 * September 7th, 2014 by Robin T. Miller
 * 	Invoke pthread_detach() for threads we wish to detach. This is required
 * for Windows, since to mimic POSIX detached threaads, the thread handle must
 * be closed. Previously on Windows, detached threads were leaking handles.
 *
 * July 14th, 2014 by Robin T. Miller
 * 	Add support for SCSI I/O.
 *
 * June 2nd, 2014 by Robin T. Miller
 * 	When alarm is specified *without* a keepalive time, then enable
 * keepalives, which restores compatablity with older dt's.
 * 
 * May 29th, 2014 by Robin T. Miller
 * 	Fix bug when the runtime is -1 (infinite), the alarm value was set
 * to this value which caused the monitoring thread to wait forever, thus
 * things serviced by this thread never executed (noprog's, keepalives, etc).
 * 
 * April 9th, 2014 by Robin T. Miller
 * 	Converted Printf/Fprint's to Eprintf/Wprintf for errors and warning.
 *
 * April 2nd, 2014 by Robin T. Miller
 * 	When specifying procs=value, retain backwards compatability by creating
 * unique file names, otherwise we run the risk of breaking someone's scripts!
 * FYI: procs=1 is equivalent to "threads=1 enable=funique".
 *
 * March 31st, 2014 by Robin T. Miller
 * 	Allow ${VAR:string} format, when expanding environment variables.
 * 	Where "string" gets used when ${VAR} is *not* defined!
 *
 * January 27th, 2014 by Robin T. Miller
 * 	Moved validation checks *after* common device setup, required so the
 * file system information is setup (required for validation checks like XFS).
 * 
 * January 22nd, 2014 by Robin T. Miller
 * 	If the error limit was set by a noprog exit status, ensure the thread
 * status is set to FAILURE, so the onerr=abort option will stop other threads.
 *
 * January 14th, 2014 by Robin T. Miller
 * 	If spt commands fail, execute triggers specified (if any).
 * In particular, dumping ras trace buffers could be very helpful!
 *
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */
#include "dt.h"
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#if !defined(WIN32)
#  if !defined(_QNX_SOURCE) 
#    if !defined(sun)
#      include <sys/ioctl.h>
#    endif /* !defined(sun) */
#    include <sys/file.h>
#    include <sys/param.h>
#    include <sys/resource.h>
#    if defined(sun) || defined(_OSF_SOURCE)
#      include <sys/mman.h>
#    endif /* defined(sun) || defined(_OSF_SOURCE) */
#  endif /* !defined(_QNX_SOURCE) */
#include <sys/wait.h>
#endif /* !defined(WIN32) */
#if defined(DEC)
#  include <sys/utsname.h>
#endif /* defined(DEC) */

#if defined(HP_UX)
#  include <sys/scsi.h>
#  if !defined(SCSI_MAX_Q_DEPTH)
#    define SCSI_MAX_Q_DEPTH 255
#  endif
#endif /* defined(HP_UX) */

//#define MEMORY_DEBUG 1
/* To enabled: % export MALLOC_TRACE=mcheck.data */
#if defined(MEMORY_DEBUG)
# if defined(__linux__)
#  include <mcheck.h>
# endif /* defined(__linux__) */
#endif /* defined(MEMORY_DEBUG) */

pthread_attr_t detached_thread_attrs;
pthread_attr_t *tdattrp = &detached_thread_attrs;
pthread_attr_t joinable_thread_attrs;
pthread_attr_t *tjattrp = &joinable_thread_attrs;
pthread_mutex_t print_lock;		/* Printing lock (sync output). */
pthread_t ParentThread;			/* The parents' thread.		*/
pthread_t iotuneThread;			/* The IO tuning thread.	*/
pthread_t MonitorThread;		/* The monitoring thread.	*/
#if defined(WIN32)
os_tid_t ParentThreadId;		/* The parents' thread ID.	*/
os_tid_t iotuneThreadId;		/* The IO tuning thread ID.	*/
os_tid_t MonitorThreadId;		/* The monitoring thread ID.	*/
#else /* !defined(WIN32) */
# define ParentThreadId		ParentThread
# define iotuneThreadId		iotuneThread
# define MonitorThreadId	MonitorThread       
#endif /* defined(WIN32) */

extern iobehavior_funcs_t dtapp_iobehavior_funcs;
extern iobehavior_funcs_t hammer_iobehavior_funcs;
extern iobehavior_funcs_t sio_iobehavior_funcs;

iobehavior_funcs_t *iobehavior_funcs_table[] = {
    &dtapp_iobehavior_funcs,
    &hammer_iobehavior_funcs,
    &sio_iobehavior_funcs,
    NULL
};

iobehavior_funcs_t *find_iobehavior(dinfo_t *dip, char *name);

/*
 * File Lock Modes:
 */
static lock_mode_t lock_full[] = {
	{  1,  80 },	/* FULL    LOCK  80% */
	{ 81, 100 },	/* PARTIAL LOCK  20% */
};

static lock_mode_t lock_mixed[] = {
	{  1,  50 },	/* FULL    LOCK  50% */
	{ 51, 100 },	/* PARTIAL LOCK  50% */
};

static lock_mode_t lock_partial[] = {
	{  1,  20 },	/* FULL    LOCK  20% */
	{ 21, 100 },	/* PARTIAL LOCK  80% */
};

volatile unsigned int monitor_interval;

/*
 * Forward References: 
 */
int ProcessStartupFile(dinfo_t *dip);
int ProcessStartupScripts(dinfo_t *dip);

int initiate_devs(dinfo_t *dip);
int initiate_job(dinfo_t *dip, job_id_t *job_id);
void catch_signals(dinfo_t *dip);
void ignore_signals(dinfo_t *dip);
void *docopy(void *arg);
void *domirror(void *arg);
void *doio(void *arg);

/* 
 * Utility Functions:
 */
void do_sleeps(dinfo_t *dip);
hbool_t	is_stop_on_file(dinfo_t *dip);
int stop_job_on_stop_file(dinfo_t *mdip, job_info_t *job);
int make_stderr_buffered(dinfo_t *dip);
int create_unique_thread_log(dinfo_t *dip);
void report_pass_statistics(dinfo_t *dip);
int setup_base_name(dinfo_t *dip, char *file);
int finish_test(dinfo_t *dip, int exit_code, hbool_t do_cleanup);
double do_iops(dinfo_t *dip);
void *do_monitoring(void *arg);
int do_monitor_processing(dinfo_t *mdip, dinfo_t *dip);

int SetThreadCancelType(dinfo_t *dip, int cancel_type);
char *decodeCancelType(int cancel_type);

int init_pthread_attributes(dinfo_t *dip);
int setup_thread_attributes(dinfo_t *dip, pthread_attr_t *tattrp, hbool_t joinable_flag);
int StartMonitorThread(dinfo_t *dip, unsigned int interval);

char *expand_word(dinfo_t *dip, char **from, size_t bufsiz, int *status);
int parse_args(dinfo_t *dip, int argc, char **argv);
char *concatenate_args(dinfo_t *dip, int argc, char **argv, int arg_index);
void show_expression(dinfo_t *dip, large_t value);
int parse_job_args(dinfo_t *dip, char *string, job_id_t *job_id, char **job_tag, hbool_t errors);
int parse_connection_args(dinfo_t *dip, char **string, char **host, uint32_t *port, hbool_t errors);
void SignalHandler(int signal_number);
void finish_exiting(dinfo_t *dip, int status);

int create_detached_thread(dinfo_t *dip, void *(*func)(void *));
void *do_stop_all_job_threads(void *arg);

int start_iotuning(dinfo_t *dip);
void *do_iotune_file(void *arg);
void *do_triggers(void *arg);
void terminate_job(dinfo_t *dip);
void report_times(dinfo_t *dip, time_t initiated_time, time_t current_time);

void save_cmdline(dinfo_t *dip);
void EmitStatus(dinfo_t *dip);
void cleanup_EOL(char *string);
void display_command(dinfo_t *dip, char *command, hbool_t prompt);

int ParseWorkload(dinfo_t *dip, char *workload);

static dinfo_t *init_device_information(void);
void init_device_defaults(dinfo_t *dip);

int do_show_fsmap(dinfo_t *dip);
int do_precopy_setup(dinfo_t *idip, dinfo_t *odip);
int do_common_copy_setup(dinfo_t *idip, dinfo_t *odip);
int do_common_validate(dinfo_t *dip);
int do_datatest_validate(dinfo_t *dip);
large_t	do_maxdata_percentage(dinfo_t *dip, large_t data_bytes, int data_percentage);

/*
 * Variable Declarations:
 */
vbool_t CmdInterruptedFlag;		/* User interrupted command.	*/ 
hbool_t	debug_flag = False;		/* Enable debug output flag.	*/
hbool_t	mDebugFlag = False;		/* Memory related debug flag.	*/
hbool_t	pDebugFlag = False;		/* Process related debug flag.	*/
hbool_t	tDebugFlag = False;		/* Thread related debug flag.	*/
int	exit_status = SUCCESS;		/* Normal success exit status.	*/

#define UNIX_INIT_PROCESS	1	/* The process ID of init!(Unix)*/
pid_t	parent_process_id;		/* Our parent process ID. (Unix)*/

/* No longer used, can probably go! (left for parser) */
int	term_wait_retries = 0;		/* Termination wait retries.	*/

hbool_t	sighup_flag = True;		/* Hangup signal control.	*/
vbool_t	terminating_flag = False;	/* Program terminating flag.	*/
hbool_t terminate_on_signals = False;	/* Terminate on signals.	*/
int	page_size = 0;			/* Define number of bytes/page.	*/

/* Note: No longer used, but must retain backwards compatibility! */ 
unsigned int cancel_delay = DEFAULT_CANCEL_DELAY;
					/* Time to delay before cancel.	*/
unsigned int kill_delay = DEFAULT_KILL_DELAY;
					/* Delay after threads stopped.	*/

clock_t hertz;

char	*cmdname;			/* Pointer to our program name.	*/
char	*dtpath;			/* Path to our dt executable.	*/
FILE	*efp;				/* Default error data stream.	*/
FILE	*ofp;				/* Default output data stream.	*/
char	*error_log;			/* The error log file name.	*/
FILE	*error_logfp;			/* The error log file pointer.	*/
char	*master_log;			/* The master log file name.	*/
FILE	*master_logfp;			/* The master log file pointer.	*/
dinfo_t	*master_dinfo;			/* The parents' information.	*/
dinfo_t *iotune_dinfo;			/* The I/O device information.	*/
char	*reread_file;			/* Optional re-read file name.	*/
char	*tools_directory;		/* The default tools directory.	*/

hbool_t DeleteErrorLogFlag = True;	/* Controls error log deleting.	*/
hbool_t	ExitFlag = False;		/* In pipe mode, exit flag.	*/
hbool_t	InteractiveFlag = False;	/* Stay in interactive mode.	*/
hbool_t	StdinIsAtty;			/* Standard input isatty flag.  */
hbool_t	StdoutIsAtty;			/* Standard output isatty flag. */
hbool_t	StderrIsAtty;			/* Standard error isatty flag.  */
hbool_t	PipeModeFlag = False;		/* The pipe mode control flag.	*/
uint32_t PipeDelay = 250;		/* Pipe mode delay value.	*/

int max_open_files = 0;			/* The maximum open files.	*/

/*
 * Default alarm message is per pass statistics, user can override. 
 */
/* Note: To remain backwards compatable, I am not changing the default. */
#if defined(Nimble)

char    *keepalive0 = "%d Stats: mode %i, blocks %l, %m Mbytes,"
		      " MB/sec: %mbps, IO/sec: %iops, pass %p/%P,"
                      " elapsed %t";
char    *keepalive1 = "%d Stats: mode %i, blocks %L, %M Mbytes,"
		      " MB/sec: %mbps, IO/sec: %iops, pass %p/%P,"
                      " elapsed %T";
                                        /* Default keepalive messages.  */
#else /* !defined(Nimble) */
char    *keepalive0 = "%d Stats: mode %i, blocks %l, %m Mbytes, pass %p/%P,"
                      " elapsed %t";
char    *keepalive1 = "%d Stats: mode %i, blocks %L, %M Mbytes, pass %p/%P,"
                      " elapsed %T";
                                        /* Default keepalive messages.  */
#endif /* defined(Nimble) */

/*
 * When stats is set to brief, these message strings get used:
 * Remember: The stats type is automatically prepended: "End of TYPE"
 */
char    *pass_msg = "pass %p/%P, %l blocks, %m Mbytes, %c records,"
                    " errors %e/%E, elapsed %t";
char    *pass_dir_msg = 
		    "pass %p/%P, %l blocks, %m Mbytes, %c records,"
                    " errors %e/%E, iodir=%iodir, elapsed %t";
char    *pass_type_msg = 
		    "pass %p/%P, %l blocks, %m Mbytes, %c records,"
                    " errors %e/%E, iotype=%iotype, elapsed %t";
                                        /* Per pass keepalive message.  */
char    *totals_msg = "%d Totals: %L blocks, %M Mbytes,"
                      " errors %e/%E, passes %p/%P, elapsed %T";
                                        /* Totals keepalive message.    */ 

/*
 * Data patterns used for multiple passes.
 */
u_int32 data_patterns[] = {
	DEFAULT_PATTERN,
	0x00ff00ffU,
	0x0f0f0f0fU,
	0xc6dec6deU,
	0x6db6db6dU,
	0x55555555U,
	0xaaaaaaaaU,	/* Complement of previous data pattern.		 */
	0x33333333U,	/* Continuous worst case pattern (media defects) */
	0x26673333U,	/* Frequency burst worst case pattern #1.	 */
	0x66673326U,	/* Frequency burst worst case pattern #2.	 */
	0x71c7c71cU,	/* Dibit worst case data pattern.		 */
	0x00000000U,
	0xffffffffU,
};
int npatterns = sizeof(data_patterns) / sizeof(u_int32);

/*
 * This table is indexed by the operation type (enum optype):
 */
optiming_t optiming_table[] =
{
  /*  Operation Type  Control Flag      Name       */
    { NONE_OP,		False,		NULL       },
    { OPEN_OP,		True,		"open"     },
    { CLOSE_OP,		True,		"close"    },
    { READ_OP,		True,		"read"     },
    { WRITE_OP,		True,		"write"    },
    { IOCTL_OP,		True,		"ioctl"    },
    { FSYNC_OP,		True,		"fsync"    },
    { MSYNC_OP,		True,		"msync"    },
    { AIOWAIT_OP,	True,		"aiowait"  },
    { MKDIR_OP,		True,		"mkdir"    },
    { RMDIR_OP,		True,		"rmdir"    },
    { DELETE_OP,	True,		"unlink"   },
    { TRUNCATE_OP,	True,		"truncate" },
    { RENAME_OP,	True,		"rename"   },
    { LOCK_OP,		True,		"lock"     },
    { UNLOCK_OP,	True,		"unlock"   },
    { GETATTR_OP,	True,		"stat"     },
    { SEEK_OP,		True,		"seek"     },
    { SPARSE_OP,	True,		"sparse"   },
    { TRIM_OP,		True,		"trim"     },
    { VINFO_OP,		True,		"vinfo"    },
    { VPATH_OP,		True,		"vpath"    },
    { MMAP_OP,		True,		"mmap"     },
    { MUNMAP_OP,	True,		"munmap"   },
    { CANCEL_OP,	True,		"cancel"   },
    { RESUME_OP,	True,		"resume"   },
    { SUSPEND_OP,	True,		"suspend"  },
    { TERMINATE_OP,	True,		"terminate"},
    { OTHER_OP,		True,		"other"    }
};
char *miscompare_op = "miscompare";

int
HandleExit(dinfo_t *dip, int status)
{
    /* Note: This may change, but don't wish to miss any errors! */
    if (status == FAILURE) exit_status = status;
    /*
     * Commands like "help" or "version" will cause scripts to exit,
     * but we don't wish to continue on fatal errors, so...
     */
    if (InteractiveFlag || PipeModeFlag || dip->script_level) {
	if (dip->script_level && (status == FAILURE)) {
	    finish_exiting(dip,  status);
	}
    } else {
	finish_exiting(dip,  status);
    }
    return(status);
}

/*
 * The mainline sets this up, need common for logging anywhere!
 */
void
log_header(dinfo_t *dip, hbool_t error_flag)
{
    /*
     * Write the command line for future reference.
     */
    Lprintf(dip, "Command Line:\n\n    %c ", getuid() ? '%' : '#');
    Lprintf(dip, "%s\n", dip->di_cmd_line);
    Lprintf(dip, "\n        --> %s <--\n\n", version_str);
    if (error_flag == True) {
	eLflush(dip);
    } else {
	Lflush(dip);
    }
    return;
}

void
save_cmdline(dinfo_t *dip)
{
    char buffer[LOG_BUFSIZE];
    char *options;

    if (dip->di_cmd_line) {
	FreeStr(dip, dip->di_cmd_line);
	dip->di_cmd_line = NULL;
    }
    options = make_options_string(dip, dip->argc, dip->argv, True);
    if (options == NULL) return;
    /* Include the dt path with the options. */
    (void)sprintf(buffer, "%s %s", dtpath, options);
    dip->di_cmd_line = strdup(buffer);
    FreeStr(dip, options);
    return;
}

int
ProcessStartupFile(dinfo_t *dip)
{
    int status;

    do {
      terminating_flag = False;
      CmdInterruptedFlag = False;

      if ( (status = dtGetCommandLine(dip)) != SUCCESS) {
	  continue;	/* EOF or FAILURE! */
      }
      if (dip->argc <= 0) continue;
    
      /*
       * Parse the arguments.
       */
      if ( (status = parse_args(dip, dip->argc, dip->argv)) != SUCCESS) {
	  continue;
      }
      /* Note: The startup file should set flags or define workloads only. */
      /* Note: We may expand and lift this restriction later (as required). */
    } while ( !CmdInterruptedFlag && !terminating_flag && (status == SUCCESS) );

    /* Reprime for parsing command line arguments. */
    if (dip->cmdbufptr) {
	FreeStr(dip, dip->cmdbufptr);
	dip->cmdbufptr = NULL;
    }
    if (dip->argv) {
	Free(dip, dip->argv);
	dip->argv = NULL;
    }
    if (status == END_OF_FILE) status = SUCCESS;
    return(status);
}

int
ProcessStartupScripts(dinfo_t *dip)
{
    char filename[PATH_BUFFER_SIZE];
    char *script_name;
    char *home_dir;
    int status = WARNING;
    
    /*
     * Script Order: (both are optional)
     * 	1) user defined script
     * 	2) normal startip script
     * This allows #1 to override #2!
     */ 
    if ((script_name = getenv(STARTUP_ENVNAME)) == NULL) {
	script_name = STARTUP_SCRIPT;
	if ((home_dir = getenv("HOME")) == NULL) {
	    return(status);
	}
	(void)sprintf(filename, "%s%c%s", home_dir, dip->di_dir_sep, script_name);
	script_name = filename;
    }

    if ( os_file_exists(script_name) == False ) {
	return(status);
    }
    status = OpenScriptFile(dip, script_name);
    if (status == SUCCESS) {
	status = ProcessStartupFile(dip);
    }
    return(status);
}

iobehavior_funcs_t *
find_iobehavior(dinfo_t *dip, char *name)
{
    iobehavior_funcs_t **iobtp = &iobehavior_funcs_table[0];
    iobehavior_funcs_t *iobf;

    while ( iobf = *iobtp++ ) {
        /* Switched to compare with length due to Windows .exe! */
	if ( EQL(iobf->iob_name, name, strlen(iobf->iob_name)) ) {
	    return(iobf);
	}
	/* We now support a tool to dt mapping function. */
	if ( iobf->iob_maptodt_name && EQL(iobf->iob_maptodt_name, name, strlen(iobf->iob_maptodt_name)) ) {
	    return(iobf);
	}
    }
    return(NULL);
}

/*
 * main() - Start of data transfer program.
 */
int
main(int argc, char **argv)
{
    char *tmp;
    dinfo_t *dip = NULL;
    int pstatus, status;
    hbool_t FirstTime = True;
    hbool_t maptodt = False;
    iobehavior_funcs_t *iobf = NULL;
#if defined(__unix)
    struct rlimit rlim, *prlim = &rlim;
#endif /* defined(__unix) */

    efp = stderr;
    ofp = stdout;
    
    /* Note: For Windows we need to check both path separators! (Cygwin) */
    /* Also Note: Newer Windows appear to accept both separaters! really? */
    tmp = strrchr(argv[0], POSIX_DIRSEP);
#if defined(WIN32)
    if (tmp == NULL) {
	tmp = strrchr(argv[0], DIRSEP);	/* Native directory separator. */
    }
#endif /* defined(WIN32) */
    cmdname = tmp ? &(tmp[1]) : argv[0];

    dip = master_dinfo = init_device_information();

    dip->di_stdin_flag = False;
    dip->di_stdout_flag = False;

    dtpath = argv[0];
    dip->di_process_id = os_getpid();
    parent_process_id = os_getppid();

    argc--; argv++; /* Skip our program name. */
    page_size = getpagesize();
#if defined(__unix)
    hertz = sysconf(_SC_CLK_TCK);
    //max_open_files = sysconf(_SC_OPEN_MAX);
    status = getrlimit(RLIMIT_NOFILE, prlim);
    if (status == SUCCESS) {
	max_open_files = prlim->rlim_cur;
    }
    if ((max_open_files < DEFAULT_MAX_OPEN_FILES) || getenv(MAXFILES_ENVNAME)) {
        char *p;
	if (p = getenv(MAXFILES_ENVNAME)) {
	    int maxfiles = number(dip, p, ANY_RADIX, &status, False);
	    if (status == SUCCESS) {
		max_open_files = maxfiles;
	    }
	} else {
	    max_open_files = DEFAULT_MAX_OPEN_FILES;
	}
	if (max_open_files > prlim->rlim_cur) {
	    if ( getuid() ) {
		prlim->rlim_cur = prlim->rlim_max; /* non-root to hard limit! */
		max_open_files = prlim->rlim_max;
	    } else {
		prlim->rlim_cur = max_open_files; /* Try to set to higher limit. */
	    }
            /* Note: This may fail, esp. for non-root users! */
	    status = setrlimit(RLIMIT_NOFILE, prlim);
	}
    }
#elif defined(WIN32x)
    /* TODO: Don't think this is necessary now! */
    /*
     * Convert the pseudo handle into a real handle by duplicating!
     */
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
		 GetCurrentProcess(), &ParentThread, 0, True, DUPLICATE_SAME_ACCESS)) {
	wPerror(dip, "DuplicateHandle() failed");
    }
#endif /* defined(WIN32) */

    /* TODO: More cleanup! */
#if defined(OSFMK) || defined(__QNXNTO__) || defined(WIN32)
    hertz = CLK_TCK;			/* Actually a libc function. */
#endif /* defined(OSFMK) || defined(__QNX_NTO__) defined(WIN32) */

    CmdInterruptedFlag = False;
    StdinIsAtty = isatty(fileno(stdin));
    StdoutIsAtty = isatty(fileno(stdout));
    StderrIsAtty = isatty(fileno(stderr));
    if (StdoutIsAtty == True) {
	dip->di_logheader_flag = False;
	dip->di_logtrailer_flag = False;
    } else {
	dip->di_logtrailer_flag = True;
    }

    (void)make_stderr_buffered(dip);
    (void)init_pthread_attributes(dip);
    (void)initialize_jobs_data(dip);
    initialize_workloads_data();

    status = ProcessStartupScripts(dip);

    //if (tools_directory = NULL) {
    //	tools_directory = strdup(TOOLS_DIR);
    //}

    if (argc == 0) {
	/* This must be done *after* processing startup files. */
	InteractiveFlag = True;
    }
    catch_signals(dip);

    if (dip->di_debug_flag || dip->di_pDebugFlag || dip->di_tDebugFlag) {
	Printf(dip, "Parent process ID is %d, Thread ID is "OS_TID_FMT"\n",
	       dip->di_process_id, ParentThreadId);
    }
    
    dip->argc = argc;
    dip->argv = argv;

    /*
     * Try to find the I/O behavior based on the program name. 
     * Note: The purpose of this is to link tool to dt and map! 
     */
    iobf = find_iobehavior(dip, cmdname);
    /* Note: This check is necessary until we have dt I/O functions! */
    if ( (iobf == NULL) && NEL("dt", cmdname, 2) ) {
	Printf(dip, "Sorry, we don't know any I/O behavior named '%s'!\n", cmdname);
	exit(FAILURE);
    }
    /* Handle special I/O tool mapping (if supported). */
    if (iobf) {
	int status = SUCCESS;
	/* Special name to map tool options to dt options. */
	if (iobf->iob_maptodt_name) {
	    maptodt = (EQS(cmdname, iobf->iob_maptodt_name)) ? True : False;
	}
	/* Map the tool options, if mapping is supported. */
	if ( (maptodt == True) && iobf->iob_dtmap_options ) {
	    status = (*iobf->iob_dtmap_options)(dip, argc, argv);
	    exit(status);	/* Only display the mapped options! */
	} else if (iobf->iob_map_options) {
	    status = (*iobf->iob_map_options)(dip, argc, argv);
	}
	if (status == FAILURE) exit(status);
	InteractiveFlag = False;
    }

    do {
	dip = master_dinfo;

	if (FirstTime) {
	    /* Parse command line options first! */
	    FirstTime = False;
#if defined(MEMORY_DEBUG) && defined(__linux__)
	    mtrace();
#endif /* defined(MEMORY_DEBUG) && defined(__linux__) */
	} else {
	    terminating_flag = False;
	    CmdInterruptedFlag = False;
	    cleanup_device(dip, True);
	    init_device_defaults(dip);

	    if ( (pstatus = dtGetCommandLine(dip)) != SUCCESS) {
		if (pstatus == END_OF_FILE) {
		    ExitFlag = True;
		} else if (pstatus == FAILURE) {
		    dip->di_exit_status = exit_status = status = pstatus;
		    dip->di_exit_status = HandleExit(dip, pstatus);
		}
		continue;
	    }
	}
	if (dip->argc <= 0) continue;

	/*
	 * Parse the arguments.
	 */
	if ( (pstatus = parse_args(dip, dip->argc, dip->argv)) != SUCCESS) {
	    dip->di_exit_status = HandleExit(dip, pstatus);
	    continue;
	}
	/* For elapsed time, initialze the start time. */
	dip->di_start_time = times(&dip->di_stimes);
	gettimeofday(&dip->di_start_timer, NULL);

#if 0
	/* 
	 * In interactive mode, check for any background jobs finishing.
	 * In non-interactive mode, we expect folks to wait for jobs! 
	 * Note: This can be misleading to interactive users, remove? 
	 */
	if ( (InteractiveFlag == True) && (dip->script_level == 0) ) {
	    status = jobs_finished(dip);
	    if (status == FAILURE) {
		dip->di_exit_status = exit_status = status;
	    }
	}
#endif /* 0 */
	if (ExitFlag == True) break;

	do_sleeps(dip);
	
	/*
	 * Interactive or pipe mode, prompt for more options if device
	 * or operation type is not specified. Allows "dt enable=pipes"
	 */
	if ( ( (dip->di_input_file == NULL) && (dip->di_output_file == NULL) ) && 
	     (InteractiveFlag || PipeModeFlag || dip->script_level) ) {
	    continue; /* reprompt! */
	}
	
	save_cmdline(dip);

	status = do_common_validate(dip);
	if (status == FAILURE) {
	    dip->di_exit_status = HandleExit(dip, status);
	    continue;
	}

	if (dip->di_fsmap_type != FSMAP_TYPE_NONE) {
            status = do_show_fsmap(dip);
	    if (status == FAILURE) {
		dip->di_exit_status = HandleExit(dip, status);
	    }
	    continue;
	}

	if (dip->di_iobehavior == DT_IO) {
	    status = do_datatest_validate(dip);
	    if (status == FAILURE) {
		dip->di_exit_status = HandleExit(dip, status);
		continue;
	    }
	    if (dip->di_io_mode == TEST_MODE) {
		dip->di_thread_func = doio;
	    } else if (dip->di_io_mode == MIRROR_MODE) {
		dip->di_thread_func = domirror;
	    } else {
		dip->di_thread_func = docopy; /* Copy/Verify modes. */
	    }
	} else if (dip->di_iobehavior == DTAPP_IO) {
	    status = (*dip->di_iobf->iob_validate_parameters)(dip);
	    if (status == FAILURE) {
		dip->di_exit_status = HandleExit(dip, status);
		continue;
	    }
	    status = do_datatest_validate(dip);
	    if (status == FAILURE) {
		dip->di_exit_status = HandleExit(dip, status);
		continue;
	    }
	    dip->di_thread_func = dip->di_iobf->iob_thread;
	    /* The stopon file is immediate for all behaviors except dt! */
	    if (dip->di_stop_on_file) {
		dip->di_stop_immediate = True;
	    }
	} else { /* All other I/O behaviors! */
	    status = (*dip->di_iobf->iob_validate_parameters)(dip);
	    if (status == FAILURE) {
		dip->di_exit_status = HandleExit(dip, status);
		continue;
	    }
	    dip->di_thread_func = dip->di_iobf->iob_thread;
	    /* The stopon file is immediate for all behaviors except dt! */
	    if (dip->di_stop_on_file) {
		dip->di_stop_immediate = True;
	    }
	}

	/*
	 * Ok, start a job with thread(s).
	 */
	if (dip->di_iobf && dip->di_iobf->iob_initiate_job) {
	    status = (*dip->di_iobf->iob_initiate_job)(dip);
	} else if ( dip->di_multiple_devs ) {
	    status = initiate_devs(dip);
	} else {
	    status = initiate_job(dip, NULL);
	}
	if (status == FAILURE) {
	    exit_status = status;
	    dip->di_exit_status = HandleExit(dip, status);
	}
	dip = master_dinfo;

    } while ( (InteractiveFlag || PipeModeFlag || dip->script_level) && (ExitFlag == False) );

    /*
     * Jobs may be active if run async (background) and not waited on!
     */ 
    if ( jobs_active(dip) ) {
	/* Wait for threads to start. */
	while ( threads_starting(dip) ) {
	    os_sleep(1);
	}
	if ( jobs_paused(dip) ) {
	    /* Resume paused jobs, or we'll wait forever! */
	    (void)resume_jobs(dip, (job_id_t) 0, NULL);
	}
        /* Wait for any active jobs. */
	status = wait_for_jobs(dip, (job_id_t) 0, NULL);
	/* Some thread had a failure! */
	if (status == FAILURE) exit_status = FAILURE;
    }
    /* Terminate will stop any active jobs. */
    terminate(master_dinfo, exit_status); /* Terminate with the exit status.	*/
    /*NOTREACHED*/
    return(exit_status);		/* Quiet certain compilers!	*/
}

int
initiate_devs(dinfo_t *dip)
{
    char *devp, *devices, *token, *p;
    int device, devices_started = 0;
    job_id_t *job_ids;
    hbool_t async_job = dip->di_async_job;
    char *saveptr;
    char *devs_tag = NULL;
    int jstatus, status = SUCCESS;
    
    dip->di_async_job = True;
    if (dip->di_input_file) {
	devices = dip->di_input_file;
    } else {
	devices = dip->di_output_file;
    }
    devp = p = strdup(devices);
    dip->di_num_devs = 1;
    /* Count the devices specified. */
    while (p = strchr(p, ',')) {
	dip->di_num_devs++; p++;
    }
    job_ids = Malloc(dip, sizeof(job_id_t) * dip->di_num_devs );

    /* Create a job tag, required for onerr= option. */
    if (dip->di_job_tag == NULL) {
	uint64_t unique_value = os_create_random_seed();
	devs_tag = Malloc(dip, SMALL_BUFFER_SIZE);
	(void)sprintf(devs_tag, "devs-"LUF, unique_value);
	dip->di_job_tag = strdup(devs_tag);
	FreeStr(dip, devs_tag);
	devs_tag = NULL;
    }

    /*
     * Each device will be its' own job with its' own thread(s).
     */ 
    /* Remember: strtok() replaces "," with NULL! */
    token = strtok_r(devp, ",", &saveptr);
    
    for (device = 0; (device < dip->di_num_devs); device++) {
	job_id_t job_id = 0;
	
	if (dip->di_input_file) {
	    dip->di_input_file = strdup(token);
	} else {
	    dip->di_output_file = strdup(token);
	}
	
	status = initiate_job(dip, &job_id);
	if (status == FAILURE) break;
	
	devices_started++;
	job_ids[device] = job_id;
	token = strtok_r(NULL, ",", &saveptr); /* Next device please! */
	if (token == NULL) break; /* "," without a device name! */
    }
    
    if (async_job == False) {
	/*
	 * Now, wait for each job started.
	 */ 
	for (device = 0; (device < devices_started); device++) {
    
	    jstatus = wait_for_job_by_id(dip, job_ids[device] );
	    if (jstatus != SUCCESS) status = jstatus;
    
	}
    }
    Free(dip, devp);
    Free(dip, job_ids);
    Free(dip, devices);
    return(status);
}

int
initiate_job(dinfo_t *mdip, job_id_t *job_id)
{
    dinfo_t *dip = NULL;	/* Remove after full cleanup! */
    /* TODO: Restructure dinfo_t for multiple devices! */
    dinfo_t *idip = NULL;	/* Input device information.	*/
    dinfo_t *odip = NULL;	/* Output device information.	*/
    int status;

    mdip->di_device_number++;	/* Count multiple devices. */
    /*
     * Setup the initial device information & validate options.
     * 
     * Note: The original dip is used for parsing. It gets copied to
     * the input and/or output dinfo then updated accordingly.
     */
    if (mdip->di_input_file) {
	idip = clone_device(mdip, True, False);
	if (mdip->di_iobehavior == DT_IO) {
	    /* Delayed until here, to save memory in master! */
	    /* Note: Can't move to thread setup due to sanity checks. */
	    status = do_datatest_initialize(idip);
	    if (status == FAILURE) goto cleanup_exit;
	}
    }
    if (mdip->di_output_file) {
	odip = clone_device(mdip, True, False);
	if (mdip->di_iobehavior == DT_IO) {
	    status = do_datatest_initialize(odip);
	    if (status == FAILURE) goto cleanup_exit;
	}
	if (idip) {
	    /* Save pointer to output device information (until rewrite) */
	    idip->di_output_dinfo = odip;
	}
	/* HACK, until multiple device support is cleaned up! */
	if (mdip->di_io_mode != TEST_MODE) {
	    if (mdip->di_record_limit == 0) {
		odip->di_record_limit = idip->di_record_limit;
	    }
	    odip->di_aio_flag = False; /* Writes are synchronous (right now). */
	    odip->di_aio_bufs = 0;
	}
    }

    if (idip) {
	/* This is for input device info only (for now)! */
	if (idip->di_output_file) {
	    FreeStr(idip, idip->di_output_file);
	    idip->di_output_file = NULL;
	}
	idip->di_mode = READ_MODE;
	idip->di_ftype = INPUT_FILE;
	if ( (idip->di_iobehavior == DT_IO) && os_isdir(idip->di_input_file)) {
            char *dirpath = idip->di_input_file;
	    idip->di_input_file = make_dir_filename(idip, dirpath);
	    FreeStr(idip, dirpath);
	}
	idip->di_dname = strdup(idip->di_input_file);
	/* Setup the device type and various defaults. */
	status = setup_device_info(idip, idip->di_input_file, idip->di_input_dtype);
	if (status == FAILURE) goto cleanup_exit;
	if (idip->di_fsfile_flag == True) {
	    status = do_filesystem_setup(idip);
	}
	if (status == FAILURE) goto cleanup_exit;
	idip->di_fsync_flag = False;
    }
    if (odip) {
	/* This one for output info only! */
	if (odip->di_input_file) {
	    FreeStr(odip, odip->di_input_file);
	    odip->di_input_file = NULL;
	}
	odip->di_mode = WRITE_MODE;
	odip->di_ftype = OUTPUT_FILE;
        if ( (odip->di_iobehavior == DT_IO) && os_isdir(odip->di_output_file) ) {
            char *dirpath = odip->di_output_file;
	    odip->di_output_file = make_dir_filename(odip, dirpath);
	    FreeStr(odip, dirpath);
	}
	odip->di_dname = strdup(odip->di_output_file);
	/* Setup the device type and various defaults. */
	status = setup_device_info(odip, odip->di_output_file, odip->di_output_dtype);
	if (status == FAILURE) goto cleanup_exit;
	if (odip->di_fsfile_flag == True) {
	    status = do_filesystem_setup(odip);
	}
	if (status == FAILURE) goto cleanup_exit;
    }

    if (idip) {
	status = do_common_device_setup(idip);
	if (status == FAILURE) goto cleanup_exit;
	/* Note: This *must* be done last! */
	status = (*idip->di_funcs->tf_validate_opts)(idip);
	if (status == FAILURE) goto cleanup_exit;
    }
    if (odip) {
	if ( idip && odip && (mdip->di_io_mode != TEST_MODE) ) {
	    (void)do_precopy_setup(idip, odip);
	}
	status = do_common_device_setup(odip);
	if (status == FAILURE) goto cleanup_exit;
	/* Note: This *must* be done last! */
	status = (*odip->di_funcs->tf_validate_opts)(odip);
	if (status == FAILURE) goto cleanup_exit;
    }

    if ( idip && odip && (mdip->di_io_mode != TEST_MODE) &&
	 ( (idip->di_dtype->dt_dtype == DT_DISK) && (odip->di_dtype->dt_dtype == DT_DISK) ) ) {
	status = do_common_copy_setup(idip, odip);
	if (status == FAILURE) goto cleanup_exit;
    }
 
    /*
     * Do the device / test specific initialization.
     *
     * This function is responsible for allocating the necessary
     * data buffers and performing special device setup/checking.
     */
    if (idip) {
	status = (*idip->di_funcs->tf_initialize)(idip);
	if (status == FAILURE) goto cleanup_exit;
    }
    if (odip) {
	status = (*odip->di_funcs->tf_initialize)(odip);
	if (status == FAILURE) goto cleanup_exit;
    }

    /*
     * If both input and output devices are specified, we operate off
     * the input device information. I/O functions must switch for output.
     * 
     * Note: This HACK can be removed once dinfo supports both devices.
     */ 
    if (idip) {
	dip = idip;
    } else {
	dip = odip;
    }

    if (dip->di_iobehavior != DTAPP_IO) {
	(void)do_prejob_start_processing(mdip, dip);
    }

    /*
     * Finally create a job and execute the threads!
     */ 
    status = execute_threads(mdip, &dip, job_id);
    if (dip == NULL) {
	idip = odip = NULL;	/* We no longer own these! */
    }

cleanup_exit:
    if (idip) {
	cleanup_device(idip, False);
	FreeMem(mdip, idip, sizeof(*idip));
	idip = NULL;
    }
    if (odip) {
	cleanup_device(odip, False);
	FreeMem(mdip, odip, sizeof(*odip));
	odip = NULL;
    }
    return(status);
}

int
do_prejob_start_processing(dinfo_t *mdip, dinfo_t *dip)
{
    /*
     * Start a monitoring thread, if not running already.
     */
    (void)do_monitor_processing(mdip, dip);
    do_setup_keepalive_msgs(dip);

    /*
     * Start of main test loop.
     */
    if (dip->di_syslog_flag) {
	SystemLog(dip, LOG_INFO, "Starting: %s", dip->di_cmd_line);
    }

    /* Note: Only one I/O Tuning thread (right now). */
    if (dip->di_iotuning_flag) {
	(void)start_iotuning(mdip);
    }
    return(SUCCESS);
}

/*
 * catch_signals() = Enable Signals we wish to Catch.
 */ 
void
catch_signals(dinfo_t *dip)
{
    /*
     * Catch a couple signals to do elegant cleanup.
     */
    (void)signal(SIGTERM, SignalHandler);
    /*
     * Windows Note: SIGINT is not supported for any Win32 application,
     * including Windows 98/Me and Windows NT/2000/XP. When a CTRL+C
     * interrupt occurs, Win32 operating systems generate a new thread
     * to specifically handle that interrupt.  This can cause a
     * single-thread application such as UNIX, to become multithreaded,
     * resulting in unexpected behavior. (and indeed it does!)
     */
    (void)signal(SIGINT, SignalHandler);
#if !defined(WIN32)
    (void)signal(SIGHUP, (sighup_flag) ? SignalHandler : SIG_IGN);
    (void)signal(SIGPIPE, SignalHandler);
#endif /* !defined(WIN32) */
#if !defined(WIN32)
    (void)signal(SIGCHLD, SIG_DFL);
#endif
    return;
}

/*
 * ignore_signals() = Ignore Signals in Threads.
 *
 * Description:
 *  We don't want threads handling signals, just the mainline.
 */
void
ignore_signals(dinfo_t *dip)
{
#if !defined(WIN32)
    sigset_t sigs;

    /*
     * int pthread_sigmask(int how, const sigset_t  *set,  sigset_t *oset);
     */
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGTERM);
    sigaddset(&sigs, SIGHUP);
    sigaddset(&sigs, SIGCHLD);
    if (pthread_sigmask(SIG_BLOCK, &sigs, NULL) == FAILURE) {
        Perror(dip, "pthread_sigmask() failed\n");
    }
#endif /* !defined(WIN32) */
    return;
}

void
do_common_thread_exit(dinfo_t *dip, int status)
{
    dip->di_exit_status = status;
    dip->di_thread_state = TS_FINISHED;

    if (status == FAILURE) {
	if (dip->di_oncerr_action == ONERR_ABORT) {
	    /* Stopping tells threads to terminate! */
	    if (dip->di_job->ji_job_tag) {
		Printf(dip, "onerr=abort, so stopping all threads with tag %s...\n",
		       dip->di_job->ji_job_tag);
		(void)stop_jobs(dip, (job_id_t)0, dip->di_job->ji_job_tag);
	    } else {
		Printf(dip, "onerr=abort, so stopping all threads for job %u...\n",
		       dip->di_job->ji_job_id);
		(void)stop_jobs(dip, dip->di_job->ji_job_id, NULL);
	    }
	} else if (dip->di_oncerr_action == ONERR_PAUSE) {
	    if (dip->di_job->ji_job_tag) {
		Printf(dip, "onerr=pause, so pausing all threads with tag %s...\n",
		       dip->di_job->ji_job_tag);
		(void)pause_jobs(dip, (job_id_t)0, dip->di_job->ji_job_tag);
	    } else {
		Printf(dip, "onerr=pause, so pausing all threads for job %u...\n",
		       dip->di_job->ji_job_id);
		(void)pause_jobs(dip, dip->di_job->ji_job_id, NULL);
	    }
	    PAUSE_THREAD(dip)
	}
    }
    HANDLE_THREAD_EXIT(dip);
}

int
do_common_thread_startup(dinfo_t *dip)
{
    int status = SUCCESS;
 
    /* Note: Now done as part of thread startup to avoid races! */
    if (dip->di_async_job && (dip->di_initial_state == IS_PAUSED) ) {
	dip->di_thread_state = TS_PAUSED;
    } else {
	dip->di_thread_state = TS_RUNNING;
    }
    ignore_signals(dip);
    status = acquire_job_lock(dip, dip->di_job);
    if (status == SUCCESS) {
	status = release_job_lock(dip, dip->di_job);
    }
    dip->di_program_start = time((time_t) 0);
    /* Prime the keepalive time, if enabled. */
    if (dip->di_keepalive_time) {
	dip->di_last_keepalive = time((time_t *)0);
    }

    if (dip->di_uuid_string == NULL) {
	/* The UUID can be used in the prefix and/or dir/file paths. */
	dip->di_uuid_string = os_get_uuid(dip->di_uuid_dashes);
    }

    status = do_common_file_system_setup(dip);
    if (status == FAILURE) return(status);
#if 0    
    /* Format the file & directory path based on user control strings. */
    if (dip->di_fsfile_flag == True) {
	if ( strchr(dip->di_dname, '%') ) {
	    status = format_device_name(dip, dip->di_dname);
	}
	if (dip->di_dir && strchr(dip->di_dir, '%') ) {
	    /* Format the directory based on user control strings. */
	    status = setup_directory_info(dip);
	}
	if (status == FAILURE) return(status);
    }
#endif /* 0 */

    if (dip->di_log_file) {
	if ( (status = create_unique_thread_log(dip)) == FAILURE) {
	    return(status);
	}
    } else if ( (dip->di_job->ji_job_logfile == NULL) &&
		(dip->di_logheader_flag && (dip->di_thread_number == 1)) ) {
	/* Log the header for the first thread.*/
	log_header(dip, False);
    }

    /*
     * Note: The SCSI flag must be enabled by users, plus this flag should 
     * be disabled automatically when the SCSI device cannot be open'ed.
     */
#if defined(SCSIx)
    /* Note: Some I/O behaviors do *not* do SCSI setup! */
    if (dip->di_sgp == NULL) dip->di_scsi_flag = False;
    if (dip->di_output_dinfo && (dip->di_output_dinfo->di_sgp == NULL)) {
	dip->di_output_dinfo->di_scsi_flag = False;
    }
#endif /* defined(SCSI) */

    /* Note: We should support btags for all I/O behaviors! */
    if ( (dip->di_iobehavior == DT_IO) && (dip->di_btag_flag == True) ) {
	dip->di_btag = initialize_btag(dip, OPAQUE_NO_DATA_TYPE);
	if (dip->di_btag == NULL) dip->di_btag_flag = False;
	if (dip->di_output_dinfo) {
	    dinfo_t *odip = dip->di_output_dinfo;
	    odip->di_btag = initialize_btag(odip, OPAQUE_NO_DATA_TYPE);
	    if (odip->di_btag == NULL) odip->di_btag_flag = False;
	}
    }
    return(status);
}

void
do_common_startup_logging(dinfo_t *dip)
{
    dinfo_t *odip = dip->di_output_dinfo;

    /* Display OS and/or SCSI information. */
    
    /* Report for 1st thread or all threads with a log file. */
    if ( (dip->di_thread_number == 1) || dip->di_log_file ) {
	if (dip->di_logheader_flag) {
	    if ( (dip->di_iobehavior != DTAPP_IO) ||
		 (dip->di_iobehavior == DTAPP_IO) && (dip->di_device_number == 0) ) {
		report_os_information(dip, True);
	    }
	    report_file_system_information(dip, True, False);
	}
	if (odip) {
	    report_file_system_information(odip, True, False);
	}
#if defined(SCSI)
	/* Display SCSI information. */
	if ( (dip->di_nvme_flag == True) || (dip->di_scsi_flag == True) ) {
	    /* Report for 1st thread or all threads with a log file. */
	    if ( (dip->di_thread_number == 1) || dip->di_log_file ) {
		report_scsi_information(dip);
		if (odip && ((odip->di_nvme_flag == True) || (odip->di_scsi_flag == True)) ) {
		    report_scsi_information(odip);
		}
	    }
	}
#endif /* defined(SCSI) */
	Lflush(dip);
    }
    return;
}
	
/*************************/
/* Start of Test Threads */
/*************************/

/*
 * All functions invoked by the I/O loops are expected to bump the
 * error count whenever errors occur. This will happen automatically
 * assuming the proper error reporting function is called. This macro
 * is used outside I/O functions, to bump the total errors which each
 * I/O loop used as its' maximum error limit.
 * 
 * FWIW: I'm thinking both a pass and total error limit is required.
 * TODO: This is currently *very* unclean, sop needs cleaned up soon!
 * 
 * Note: The total errors are updated when reporting pass statistics!
 */
#define HANDLE_LOOP_ERROR(dip, error)				\
    if (error == FAILURE) {					\
	status = error;						\
	if ( THREAD_TERMINATING(dip) ||				\
	    (dip->di_error_count >= dip->di_error_limit) ) {	\
	    break;						\
	}							\
    } else if (error == WARNING) { /* No more files! */		\
	break;							\
    }

/*
 * FYI: For anybody looking at this multiple file/device support, I
 * apologize for this mess! Like spt, I/O specific information needs
 * moved to their own data structure to keep this separate, and so
 * the key information can be shared (UUID, logs, error counters, etc).
 * Doing this is a rather large effort, so I've HACK'ed things to work!
 * 
 * Because of this data is being copied between two dinfo_t structs
 * and errors must be propagated to the input device, very ugly indeed!
 */

void *
docopy(void *arg)
{
    dinfo_t *dip = arg;
    dinfo_t *odip = dip->di_output_dinfo;
    struct dtfuncs *dtf = dip->di_funcs;
    hbool_t do_cleanup = False;
    int rc, status = SUCCESS;

    status = do_common_thread_startup(dip);
    if (status == FAILURE) goto thread_exit;

    if (dip->di_debug_flag || dip->di_tDebugFlag) {
	Printf(dip, "Starting Copy/Verify, Job %u, Thread %u, Thread ID "OS_TID_FMT"\n",
	       dip->di_job->ji_job_id, dip->di_thread_number, pthread_self());
    }

    /* Note: We are not handling file system full at this time. */
    dip->di_fsfull_restart = odip->di_fsfull_restart = False;

    /*
     * Handle setup for multiple slices.
     */ 
    if (dip->di_slice_number) {
	status = init_slice(dip, dip->di_slice_number);
	if (status == SUCCESS) {
	    status = init_slice(odip, odip->di_slice_number);
	}
    } else if (dip->di_slices) {
	status = init_slice(dip, dip->di_thread_number);
	if (status == SUCCESS) {
	    status = init_slice(odip, odip->di_thread_number);
	}
    } else if (odip->di_user_position || odip->di_ofile_position) {
	/* Copy the output offset for common processing. */
	odip->di_file_position = odip->di_ofile_position;
    }
    if (status == FAILURE) goto thread_exit;

    status = setup_thread_names(dip);
    if (status == FAILURE) goto thread_exit;
    status = setup_thread_names(odip);
    if (status == FAILURE) goto thread_exit;

    PAUSE_THREAD(dip);
    if ( THREAD_TERMINATING(dip) ) goto thread_exit;

    if (dip->di_io_mode == COPY_MODE) {
	(void)verify_filesystem_space(odip, False);
    }

    if (dip->di_fd == NoFd) {
	if ( (status = (*dip->di_funcs->tf_open)(dip, dip->di_initial_flags)) == FAILURE) {
	    goto thread_exit;
	}
    }
    if (odip->di_fd == NoFd) {
	if ( (status = (*odip->di_funcs->tf_open)(odip, odip->di_initial_flags)) == FAILURE) {
	    goto thread_exit;
	}
    }

    if (dip->di_fsfile_flag == True) {
	dip->di_protocol_version = os_get_protocol_version(dip->di_fd);
    }
    do_common_startup_logging(dip);

    dip->di_start_time = times(&dip->di_stimes);
    odip->di_start_time = dip->di_start_time;
    gettimeofday(&dip->di_start_timer, NULL);
    gettimeofday(&odip->di_start_timer, NULL);

    if (dip->di_runtime > 0) {
	dip->di_runtime_end = time((time_t *)NULL) + dip->di_runtime;
    }

    /*
     * Note: Don't need this for output device, so free memory.
     */
    if (odip->di_pattern_buffer) {
	reset_pattern(odip);
    }

    while ( !THREAD_TERMINATING(dip)			&&
	    (dip->di_error_count < dip->di_error_limit) &&
	    ((dip->di_pass_count < dip->di_pass_limit) || dip->di_runtime) ) {
	
	do_prepass_processing(dip);

	/*
	 * Copy or Verify the input and output devices.
	 */
	dip->di_pass_time = times(&dip->di_ptimes);	/* Start the pass timer. */
	gettimeofday(&dip->di_pass_timer, NULL);
	dip->di_write_pass_start = time((time_t) 0);	/* Pass time in seconds. */
	odip->di_pass_time = dip->di_pass_time;			/* HACK! */
	odip->di_write_pass_start = dip->di_write_pass_start;	/* HACK! */

	rc = (*dtf->tf_start_test)(dip);
	if (rc == SUCCESS) {
	    rc = (*dtf->tf_start_test)(odip);
	}
	if (rc == FAILURE) status = rc;
	if (rc == SUCCESS) {
	    rc = (*dtf->tf_read_file)(dip);
	    if (rc == FAILURE) status = rc;
	}
	if (dip->di_io_mode == COPY_MODE) {
	    rc = (*dtf->tf_flush_data)(odip);
	    if (rc == FAILURE) status = rc;
	}
	rc = (*dtf->tf_end_test)(dip);
	if (rc == FAILURE) status = rc;
	rc = (*dtf->tf_end_test)(odip);
	if (rc == FAILURE) status = rc;

	gather_stats(odip);
	accumulate_stats(dip);

	/*
	 * Now verify the data copied, if copying (if requested).
	 */
	if ( (dip->di_io_mode == COPY_MODE) && dip->di_verify_flag &&
	     !dip->di_stdin_flag && (dip->di_error_count < dip->di_error_limit) ) {
	    int open_mode = (dip->di_read_mode | dip->di_open_flags);
	    report_pass(dip, COPY_STATS);	/* Report copy statistics. */
	    /*
	     * Verify Pass.
	     */
	    rc = (*dtf->tf_reopen_file)(dip, open_mode);
	    HANDLE_LOOP_ERROR(dip, rc);

	    /* Reopen the output file to reading. */
	    odip->di_mode = READ_MODE;		/* Switch to read mode. */
	    rc = (*odip->di_funcs->tf_reopen_file)(odip, open_mode);
	    HANDLE_LOOP_ERROR(dip, rc);

	    if ( UseRandomSeed(dip) ) {
		set_rseed(dip, dip->di_random_seed);
	    }
	    dip->di_pass_time = times(&dip->di_ptimes); /* Time the verify. */
	    gettimeofday(&dip->di_pass_timer, NULL);
	    dip->di_read_pass_start = time((time_t) 0);	/* Pass time in seconds. */
	    dip->di_io_mode = VERIFY_MODE;
	    rc = (*dtf->tf_start_test)(dip);
	    if (rc == SUCCESS) {
		rc = (*dtf->tf_start_test)(odip);
	    }
	    if (rc == FAILURE) status = rc;
	    if (rc == SUCCESS) {
		rc = (*dtf->tf_read_file)(dip);
		if (rc == FAILURE) status = rc;
	    }
	    rc = (*dtf->tf_end_test)(dip);
	    if (rc == FAILURE) status = rc;
	    rc = (*dtf->tf_end_test)(odip);
	    if (rc == FAILURE) status = rc;
	    dip->di_pass_count++;		/* End of copy/verify pass. */
	    gather_stats(odip);			/* Gather device statistics. */
	    accumulate_stats(dip);
	    report_pass(dip, VERIFY_STATS);	/* Report the verify stats. */
	    if ( (dip->di_pass_limit > 1) || dip->di_runtime) {
		if (dip->di_end_delay) {	/* Optional end delay. 	*/
		    SleepSecs(dip, dip->di_end_delay);
		}
	    }
	    dip->di_io_mode = COPY_MODE;	/* Reset original mode (unclean). */
	} else {
	    dip->di_pass_count++;		/* End of copy pass. */
	    if ( (dip->di_pass_limit > 1) || dip->di_runtime) {
		report_pass (dip, VERIFY_STATS);/* Report verify stats.	*/
		if (dip->di_end_delay) {	/* Optional end delay. 	*/
		    mySleep(dip, dip->di_end_delay);
		}
	    }
	}

	/*
	 * Allow folks to loop on copy/verify operations.
	 */
	if ( !THREAD_TERMINATING(dip)			&&
	    (dip->di_error_count < dip->di_error_limit) &&
	    ((dip->di_pass_count < dip->di_pass_limit) || dip->di_runtime) ) {

	    int open_mode = (dip->di_read_mode | dip->di_open_flags);

	    /* Reopen the input file. */
	    rc = (*dtf->tf_reopen_file)(dip, open_mode);
	    HANDLE_LOOP_ERROR(dip, rc);

	    /* Reopen the output file. */
	    if (odip->di_io_mode == COPY_MODE) {
		open_mode = (odip->di_write_mode | odip->di_write_flags | odip->di_open_flags);
		if (odip->di_delete_per_pass) {
		    rc = do_deleteperpass(odip);
		    HANDLE_LOOP_ERROR(dip, rc);
		    open_mode |= O_CREAT;
		    /* Note: Only open expects O_CREAT and sets permissions! */
		    rc = (*dtf->tf_open)(odip, open_mode);
		} else {
		    /* Reopen *always* clears O_CREAT, file should exist! */
		    rc = (*odip->di_funcs->tf_reopen_file)(odip, open_mode);
		}
		odip->di_mode = WRITE_MODE;	/* Switch back to write mode. */
	    } else { /* Verify Mode */
		rc = (*odip->di_funcs->tf_reopen_file)(odip, open_mode);
	    }
	    HANDLE_LOOP_ERROR(dip, rc);

	    if (is_stop_on_file(dip) == True) break;
	}
    }
    /*
     * Triggers may bump the error count, but the status won't be failure.
    */
    if (dip->di_error_count && (status != FAILURE) ) {
	status = FAILURE;
    }

    if (dip->di_debug_flag || dip->di_tDebugFlag) {
	Printf(dip, "Copy/Verify has completed, thread exiting with status %d...\n", status);
    }
    do_cleanup = True;

thread_exit:
    status = finish_test(dip, status, do_cleanup);
    do_common_thread_exit(dip, status);
    /*NOT REACHED*/
    return(NULL);
}

/*
 * Mirror - Write to output device, read/verify from input device (mirror).
 */
void *
domirror(void *arg)
{
    dinfo_t *idip = arg;
    dinfo_t *odip = idip->di_output_dinfo;
    struct dtfuncs *dtf = odip->di_funcs;
    hbool_t do_cleanup = False;
    int open_mode = 0;
    int rc, status = SUCCESS;

    odip->di_output_dinfo = idip;
    status = do_common_thread_startup(idip);
    if (status == FAILURE) goto thread_exit;

    if (idip->di_debug_flag || idip->di_tDebugFlag) {
	Printf(idip, "Starting Mirror, Job %u, Thread %u, Thread ID "OS_TID_FMT"\n",
	       idip->di_job->ji_job_id, idip->di_thread_number, pthread_self());
    }

    /* Note: We are not handling file system full at this time. */
    odip->di_fsfull_restart = False;

    /*
     * Handle setup for multiple slices.
     */ 
    if (idip->di_slice_number) {
	status = init_slice(idip, idip->di_slice_number);
	if (status == SUCCESS) {
	    status = init_slice(odip, odip->di_slice_number);
	}
    } else if (idip->di_slices) {
	status = init_slice(idip, idip->di_thread_number);
	if (status == SUCCESS) {
	    status = init_slice(odip, odip->di_thread_number);
	}
    } else if (odip->di_ofile_position) {
	/* Copy the output offset for common processing. */
	odip->di_file_position = odip->di_ofile_position;
    }
    if (status == FAILURE) goto thread_exit;

    status = setup_thread_names(idip);
    if (status == FAILURE) goto thread_exit;
    status = setup_thread_names(odip);
    if (status == FAILURE) goto thread_exit;

    PAUSE_THREAD(idip);
    if ( THREAD_TERMINATING(idip) ) goto thread_exit;

    (void)verify_filesystem_space(odip, False);

    if (idip->di_fd == NoFd) {
	if ( (status = (*idip->di_funcs->tf_open)(idip, idip->di_initial_flags)) == FAILURE) {
	    goto thread_exit;
	}
    }
    if (odip->di_fd == NoFd) {
	if ( (status = (*odip->di_funcs->tf_open)(odip, odip->di_initial_flags)) == FAILURE) {
	    goto thread_exit;
	}
    }

     /*
     * Format the prefix string (if any), after the device name is
     * setup, so we can create unique strings using pid, tid, etc.
     */
    if (odip->di_prefix_string) {
	if (idip->di_uuid_string) { /* HACK! */
	    if (odip->di_uuid_string) FreeStr(odip, odip->di_uuid_string);
	    odip->di_uuid_string = strdup(idip->di_uuid_string);
	}
	status = initialize_prefix(odip);
	if (status == FAILURE) goto thread_exit;
    }

    if (idip->di_fsfile_flag == True) {
	idip->di_protocol_version = os_get_protocol_version(idip->di_fd);
    }
    do_common_startup_logging(idip);

    idip->di_start_time = times(&idip->di_stimes);
    odip->di_start_time = idip->di_start_time;
    gettimeofday(&idip->di_start_timer, NULL);
    gettimeofday(&odip->di_start_timer, NULL);

    if (idip->di_runtime > 0) {
	idip->di_runtime_end = time((time_t *)NULL) + idip->di_runtime;
    }

    /*
     * Note: Not used, so free the memory. (could be IOT so large)
     */
    if (idip->di_pattern_buffer) {
	reset_pattern(idip);
    }

    while ( !THREAD_TERMINATING(idip)			  &&
	    (idip->di_error_count < idip->di_error_limit) &&
	    ((idip->di_pass_count < idip->di_pass_limit) || idip->di_runtime) ) {
	/*
	 * Write to output device and read from input device (mirror).
	 */
	odip->di_pass_time = times(&odip->di_ptimes);	/* Start the pass timer. */
	gettimeofday(&odip->di_pass_timer, NULL);
	odip->di_write_pass_start = time((time_t) 0);	/* Pass time in seconds. */
	/* Propagate times to the mirror device too! */
	idip->di_pass_time = odip->di_pass_time;
	idip->di_read_pass_start = odip->di_write_pass_start;
	idip->di_write_pass_start = odip->di_write_pass_start;

	do_prepass_processing(odip);
	/* Propagate these for statistics. */
	/* Mickey Mouse, until multiple devices properly implemented! */
	idip->di_pattern = odip->di_pattern;
	idip->di_iot_seed_per_pass = odip->di_iot_seed_per_pass;
	if (odip->di_fprefix_string) {
	    if (idip->di_fprefix_string) {
		FreeStr(idip, idip->di_fprefix_string);
	    }
	    idip->di_pattern_in_buffer = True; /* for verifing */
	    idip->di_fprefix_size = odip->di_fprefix_size;
	    idip->di_fprefix_string = Malloc(idip, idip->di_fprefix_size);
	    /* Note: Cannot clone with strdup(), we have pad bytes! */
	    (void)memcpy(idip->di_fprefix_string, odip->di_fprefix_string, idip->di_fprefix_size);
	}

	rc = (*dtf->tf_start_test)(idip);
	if (rc == SUCCESS) {
	    rc = (*dtf->tf_start_test)(odip);
	}
	if (rc == FAILURE) status = rc;
	if (rc == SUCCESS) {
	    rc = (*dtf->tf_write_file)(odip);
	    if (rc == FAILURE) status = rc;
	}
	rc = (*dtf->tf_flush_data)(odip);
	if (rc == FAILURE) status = rc;
	rc = (*dtf->tf_end_test)(idip);
	if (rc == FAILURE) status = rc;
	rc = (*dtf->tf_end_test)(odip);
	if (rc == FAILURE) status = rc;

	idip->di_pass_count++;		/* End of mirror pass. */
	odip->di_pass_count++;		/* Bump for next pattern. */

	gather_stats(odip);
	accumulate_stats(idip);

	/*
	 * Stop now for single pass or error limit reached.
	 */
	if ( THREAD_TERMINATING(idip) ||
	     ((idip->di_error_count + idip->di_error_count) >= idip->di_error_limit) ||
	     ((idip->di_pass_count >= idip->di_pass_limit) && (idip->di_runtime == 0)) ) {
	    break;
	}
	
	report_pass(idip, MIRROR_STATS);/* Report mirror stats. */
	if (idip->di_end_delay) {	/* Optional end delay. 	*/
	    mySleep(idip, idip->di_end_delay);
	}
	rc = do_postwrite_processing(odip);
	if (rc == FAILURE) {
	    status = rc;
	    /* report_pass() accumulates total errors, via gather_stats() */
	    /* report_pass() resets error count to 0, via init_stats() */
	    /* Note: Total statistics are reported for input device. */
	    idip->di_error_count += odip->di_error_count;
	    if (idip->di_error_count >= idip->di_error_limit) break;
	}

	/*
	 * Allow folks to loop on mirror operations.
	 */
	open_mode = (idip->di_read_mode | idip->di_open_flags);

	/* Reopen the input file. */
	rc = (*dtf->tf_reopen_file)(idip, open_mode);
	HANDLE_LOOP_ERROR(idip, rc);

	/* Reopen the output file. */
	open_mode = (odip->di_write_mode | odip->di_write_flags | odip->di_open_flags);
	rc = (*odip->di_funcs->tf_reopen_file)(odip, open_mode);
	if (rc == FAILURE) {
	    idip->di_error_count += odip->di_error_count;
	    HANDLE_LOOP_ERROR(odip, rc);
	}
	if (is_stop_on_file(idip) == True) break;

    } /* end while(...) */

    /*
     * Triggers may bump the error count, but the status won't be failure.
     */
    if ( (idip->di_error_count || odip->di_error_count) && (status != FAILURE) ) {
	status = FAILURE;
    }

    if (idip->di_debug_flag || idip->di_tDebugFlag) {
	Printf(idip, "Mirror I/O has completed, thread exiting with status %d...\n", status);
    }
    do_cleanup = True;

thread_exit:
    status = finish_test(idip, status, do_cleanup);
    do_common_thread_exit(idip, status);
    /*NOT REACHED*/
    return(NULL);
}

void *  
doio(void *arg)
{           
    dinfo_t *dip = arg;
    struct dtfuncs *dtf;
    int rc, status = SUCCESS;
    hbool_t do_cleanup = False;

    status = do_common_thread_startup(dip);
    if (status == FAILURE) goto thread_exit;

    if (dip->di_debug_flag || dip->di_tDebugFlag) {
	Printf(dip, "Starting I/O, Job %u, Thread %u, Thread ID "OS_TID_FMT"\n",
	       dip->di_job->ji_job_id, dip->di_thread_number, pthread_self());
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

    PAUSE_THREAD(dip);
    if ( THREAD_TERMINATING(dip) ) goto thread_exit;

    (void)verify_filesystem_space(dip, False);

    /*
     * Some drivers require the input device to open before we start
     * writing. For example, terminal devices must have speed, parity,
     * and flow control setup before we start writing.  The parallel
     * input device must open before we send the "readya" interrupt.
     */
    if (dip->di_start_delay) {
        mySleep(dip, dip->di_start_delay);
    }
    if (dip->di_fd == NoFd) {
	status = (*dip->di_funcs->tf_open)(dip, dip->di_initial_flags);
	if (status == FAILURE) {
	    goto thread_exit;
	}
	dip->di_open_flags &= ~O_CREAT;	/* Only create on first open. */
    }

    if (status == FAILURE) goto thread_exit;

    /*
     * Format the prefix string (if any), after the device name is
     * setup, so we can create unique strings using pid, tid, etc.
     */
    status = initialize_prefix(dip);
    if (status == FAILURE) goto thread_exit;

    if (dip->di_fsfile_flag == True) {
	dip->di_protocol_version = os_get_protocol_version(dip->di_fd);
    }
    do_common_startup_logging(dip);

    dip->di_start_time = times(&dip->di_stimes);
    gettimeofday(&dip->di_start_timer, NULL);

    if (dip->di_runtime > 0) {
	dip->di_runtime_end = time((time_t *)NULL) + dip->di_runtime;
    }

    while ( !THREAD_TERMINATING(dip)			&&
	    (dip->di_error_count < dip->di_error_limit) &&
	    ((dip->di_pass_count < dip->di_pass_limit) || dip->di_runtime) ) {

	do_prepass_processing(dip);

	dip->di_pass_time = times(&dip->di_ptimes);	/* Start the pass timer	*/
	gettimeofday(&dip->di_pass_timer, NULL);
	if (dip->di_output_file) {			/* Write/read the file.	*/
	    hbool_t do_read_pass;
	    dtf = dip->di_funcs;
	    dip->di_mode = WRITE_MODE;
	    dip->di_write_pass_start = time((time_t) 0);/* Pass time in seconds. */
	    if (dip->di_raw_flag == True) {
		dip->di_read_pass_start = dip->di_write_pass_start;
	    }
	    rc = (*dtf->tf_start_test)(dip);
	    if (rc == FAILURE) status = rc;
	    if (rc == SUCCESS) {
		rc = (*dtf->tf_write_file)(dip);
		if (rc == FAILURE) status = rc;
	    }
	    rc = (*dtf->tf_flush_data)(dip);
	    if (rc == FAILURE) status = rc;
	    rc = (*dtf->tf_end_test)(dip);
	    if (rc == FAILURE) status = rc;

	    /*
	     * Special handling of "file system full" conditions.
	     */
	    if ( dip->di_fsfile_flag && dip->di_file_system_full ) {
		rc = handle_file_system_full(dip, True);
		if (rc == SUCCESS) {
		    init_stats(dip);
		    Wprintf(dip, "Restarting write pass after file system full detected!\n");
		    continue;
		} else if (rc == FAILURE) {
		    status = rc;
		}
		/* Note: WARNING indicates we proceed with the read pass! */
	    }

	    if ( THREAD_TERMINATING(dip) || (dip->di_error_count >= dip->di_error_limit) ) {
		report_pass_statistics(dip);
		break;
	    }

	    do_read_pass = (dip->di_dbytes_written != (large_t) 0);
	    if (dip->di_iolock) do_read_pass = True;

	    /*
	     * Now verify (read and compare) the data just written.
	     */
	    if ( dip->di_verify_flag && do_read_pass && 
		 (!dip->di_raw_flag || (dip->di_raw_flag && dip->di_reread_flag)) ) {
		int open_mode = (dip->di_read_mode | dip->di_open_flags);

		if (dip->di_raw_flag) {
		    report_pass(dip, RAW_STATS);	/* Report read-after-write. */
		} else {
		    report_pass(dip, WRITE_STATS);	/* Report write stats.	*/
		}
		if (dip->di_iolock) {
		    wait_for_threads_done(dip);
		}
		/*
		 * For multiple files, reset the pattern/IOT seed for read pass!
		 */
		if (dip->di_file_limit) {
		    if (dip->di_user_pattern == False) {
			dip->di_pattern = data_patterns[dip->di_pattern_index % npatterns];
			if (dip->di_pattern_buffer) copy_pattern(dip->di_pattern, dip->di_pattern_buffer);
		    } else if (dip->di_iot_pattern) {
			dip->di_iot_seed_per_pass = dip->di_iot_seed;
			if (dip->di_unique_pattern) {
			    dip->di_iot_seed_per_pass *= (dip->di_pass_count + 1);
			}
		    }
		}
		dip->di_mode = READ_MODE;
		if (dip->di_multi_flag && dip->di_media_changed) {
		    rc = RequestFirstVolume(dip, dip->di_open_flags);
		} else {
		    rc = (*dtf->tf_reopen_file)(dip, open_mode);
		}
		HANDLE_LOOP_ERROR(dip, rc);

		/*
		 * Reset the random seed, so reads mimic what we wrote!
		 */
		if ( UseRandomSeed(dip) ) {
		    set_rseed(dip, dip->di_random_seed);
		}
		dip->di_pass_time = times(&dip->di_ptimes); /* Time just the reads. */
		gettimeofday(&dip->di_pass_timer, NULL);
		dip->di_read_pass_start = time((time_t) 0); /* Pass time in seconds. */
		/*dip->di_rotate_offset = 0;*/
		if (dip->di_pattern_buffer) {
		    dip->di_pattern_bufptr = dip->di_pattern_buffer;
		}
		rc = (*dtf->tf_start_test)(dip);
		if (rc == FAILURE) status = rc;
		if (rc == SUCCESS) {
		    rc = (*dtf->tf_read_file)(dip);
		    if (rc == FAILURE) status = rc;
		}
		rc = (*dtf->tf_end_test)(dip);
		if (rc == FAILURE) status = rc;
		dip->di_pass_count++;			/* End read/write pass. */
		report_pass(dip, READ_STATS);		/* Report read stats.	*/
		if (dip->di_end_delay) {		/* Optional end delay. 	*/
		    mySleep(dip, dip->di_end_delay);
		}
		if ( (dip->di_pass_limit > 1) || dip->di_runtime) {
		    if (dip->di_iolock) {
			wait_for_threads_done(dip);
		    }
		}
	    } else {
		dip->di_pass_count++;			/* End of write pass.	*/
		if ( (dip->di_pass_limit > 1) || dip->di_runtime) {
		    /* Report write stats. */
		    if (dip->di_raw_flag) {
			report_pass(dip, RAW_STATS);
		    } else {
			report_pass(dip, WRITE_STATS);
		    }
		    if (dip->di_end_delay) {		/* Optional end delay. 	*/
			mySleep(dip, dip->di_end_delay);
		    }
		    if (dip->di_iolock) {
			wait_for_threads_done(dip);
		    }
		}
	    }
	    if ( THREAD_TERMINATING(dip) || (dip->di_error_count >= dip->di_error_limit) ) {
		break;
	    }
	    rc = do_postwrite_processing(dip);
	    HANDLE_LOOP_ERROR(dip, rc);

	    /*
	     * Don't reopen if we've reached the error limit or the
	     * pass count, since we'll be terminating shortly.
	     */
	    if ( ((dip->di_pass_count < dip->di_pass_limit) || dip->di_runtime) ) {
		int open_mode;
		SetupBufferingMode(dip, &dip->di_open_flags);
		if (dip->di_skip_count || dip->di_raw_flag) {
		    open_mode = (dip->di_rwopen_mode | dip->di_write_flags | dip->di_open_flags);
		} else {
		    open_mode = (dip->di_write_mode | dip->di_write_flags | dip->di_open_flags);
		}
		dip->di_mode = WRITE_MODE;
		if (dip->di_delete_per_pass) {
		    open_mode |= O_CREAT;
		    rc = (*dtf->tf_open)(dip, open_mode);
		} else {
		    rc = (*dtf->tf_reopen_file)(dip, open_mode);
		}
		HANDLE_LOOP_ERROR(dip, rc);
	    }
	} else { /* Reading only. */
	    dtf = dip->di_funcs;
	    dip->di_mode = READ_MODE;
	    dip->di_read_pass_start = time((time_t) 0); /* Pass time in seconds. */
	    /*
	     * Note: User must set random seed to repeat previous write sequence!
	     */
	    if ( dip->di_user_rseed && UseRandomSeed(dip) ) {
		set_rseed(dip, dip->di_random_seed);
	    }
	    rc = (*dtf->tf_start_test)(dip);
	    if (rc == FAILURE) status = rc;
	    if (rc == SUCCESS) {
		rc = (*dtf->tf_read_file)(dip);
		if (rc == FAILURE) status = rc;
	    }
	    rc = (*dtf->tf_end_test)(dip);
	    if (rc == FAILURE) status = rc;
	    dip->di_pass_count++;			/* End of read pass.	*/
	    /*
	     * Prevent pass unless looping, since terminate reports
	     * the total statistics when called (prevents dup stats).
	     */
	    if ( (dip->di_pass_limit > 1) || dip->di_runtime) {
		report_pass(dip, READ_STATS);	/* Report read stats.	*/
		if (dip->di_end_delay) {	/* Optional end delay. 	*/
		    mySleep(dip, dip->di_end_delay);
		}
		if (dip->di_iolock) {
		    wait_for_threads_done(dip);
		}
	    }
	    if (dip->di_pass_cmd) {
		rc = ExecutePassCmd(dip);
		if (rc == FAILURE) {
		    status = rc;
		    dip->di_error_count++;
		}
	    }
	    if (THREAD_TERMINATING(dip) || (dip->di_error_count >= dip->di_error_limit) ) {
		break;
	    }
	    if ( (dip->di_error_count < dip->di_error_limit) &&
		 ((dip->di_pass_count < dip->di_pass_limit) || dip->di_runtime) ) {
		int open_mode;
		SetupBufferingMode(dip, &dip->di_open_flags);
		open_mode = (dip->di_read_mode | dip->di_open_flags);
		rc = (*dtf->tf_reopen_file)(dip, open_mode);
		HANDLE_LOOP_ERROR(dip, rc);
	    }
	} /* End of a pass! */

	if (is_stop_on_file(dip) == True) break;
    }
    /*
     * Triggers may bump the error count, but the status won't be failure.
     */
    if (dip->di_error_count && (status != FAILURE) ) {
	status = FAILURE;
    }
    
    if (dip->di_debug_flag || dip->di_tDebugFlag) {
	Printf(dip, "I/O has completed, thread exiting with status %d...\n", status);
    }
    do_cleanup = True;

thread_exit:
    status = finish_test(dip, status, do_cleanup);
    do_common_thread_exit(dip, status);
    /*NOT REACHED*/
    return(NULL);
}

/*
 * Utility Functions:
 */

int
initialize_prefix(dinfo_t *dip)
{
    int status = SUCCESS;
     /*
      * Format the prefix string (if any), after the device name is
      * setup, so we can create unique strings using pid, tid, etc.
      */
    if (dip->di_prefix_string) {
	status = FmtPrefix(dip, dip->di_prefix_string, dip->di_prefix_size);
	if ( (status == SUCCESS) &&
	     ((size_t)dip->di_fprefix_size > dip->di_lbdata_size) ) {
	    Eprintf(dip, "The prefix size (%d) is larger than lbdata size (%d)!\n",
		    dip->di_fprefix_size, dip->di_lbdata_size);
	    status = FAILURE;
	}
    }
    return(status);
}

void
initialize_pattern(dinfo_t *dip)
{
    if (dip->di_pattern_buffer) {
	dip->di_pattern_bufptr = dip->di_pattern_buffer;
    }
    /*
     * Use a different data pattern for each pass. 
     */
    if ( (dip->di_user_pattern == False)	    &&
	 (dip->di_output_file || dip->di_stdin_flag ||
	  (dip->di_input_file && 
	   (dip->di_slices || (dip->di_threads > 1) || dip->di_multiple_files)) ) ) {
	/*
	 * Logic:
	 * - Unique patterns are enabled by default (but, can be disabled) 
	 * - When writing files, factor the slice/thread/pass into pattern selected 
	 * - When reading files, ensure the same pattern is chosen for each pass. 
	 *  
	 * Please Note: When multiple files are selected, we rotate through 
	 * the data patterns for each file. Therefore, when starting the next 
	 * read pass (for input files), we must reset the starting pattern. 
	 * Note: Please see dtfs.c for pattern selection per file. 
	 */
	if (dip->di_unique_pattern == True) {
	    /* When reading multiple files, reset to starting pattern. */
	    if (dip->di_input_file) {
		if (dip->di_slices) {
		    dip->di_pattern_index = (dip->di_slice_number - 1);
		} else {
		    dip->di_pattern_index = (dip->di_thread_number - 1);
		}
	    } else { /* Writing files, factor in the pass count too! */
		if (dip->di_slices) {
		    dip->di_pattern_index = ((dip->di_slice_number - 1) + dip->di_pass_count);
		} else {
		    dip->di_pattern_index = ((dip->di_thread_number - 1) + dip->di_pass_count);
		}
	    }
	}
	if (dip->di_iot_pattern == False) {
	    dip->di_pattern = data_patterns[dip->di_pattern_index % npatterns];
	}
	if (dip->di_pattern_buffer) {
	    copy_pattern(dip->di_pattern, dip->di_pattern_buffer);
	}
	if (dip->di_debug_flag) {
	    Printf(dip, "Using data pattern 0x%08x for pass %u\n",
		   dip->di_pattern, (dip->di_pass_count + 1));
	}
    } else if (dip->di_iot_pattern) {
	dip->di_iot_seed_per_pass = dip->di_iot_seed;
	if (dip->di_unique_pattern) {
	    /* For uniqueness, factor the pass count into the seed. */
	    dip->di_iot_seed_per_pass *= (dip->di_pass_count + 1);
	}
    }
    return;
}

void
setup_random_seeds(dinfo_t *dip)
{
    /* 
     * Set a random seed, for random I/O or variable block sizes. 
     */
    if (dip->di_user_rseed == False) {
	dip->di_random_seed = os_create_random_seed();
    }
    set_rseed(dip, dip->di_random_seed);
    /* Note: rand() is used for these, to keep write/read random sequences the same! */
    if (dip->di_vary_iodir || dip->di_vary_iotype || (dip->di_unmap_type == UNMAP_TYPE_RANDOM)) {
	srand((unsigned int)dip->di_random_seed);
    }
    return;
}

void
do_prepass_processing(dinfo_t *dip)
{
    /*
     * This sets a pattern and/or the pattern buffer.
     */
    initialize_pattern(dip);

    if ( UseRandomSeed(dip) ) {
	setup_random_seeds(dip);
    }
    /* 
     * Vary the I/O Type (if requested)
     */
    if (dip->di_vary_iotype) {

	switch (rand() % NUM_IOTYPES) {

	    case RANDOM_IO:
		dip->di_io_type = RANDOM_IO;
		dip->di_random_io = True;
		break;

	    case SEQUENTIAL_IO:
		dip->di_io_type = SEQUENTIAL_IO;
		dip->di_random_io = False;
		break;
	}
    }
    if (dip->di_vary_iodir && (dip->di_io_type == SEQUENTIAL_IO) ) {

	switch (rand() % NUM_IODIRS) {

	    case FORWARD:
		dip->di_io_dir = FORWARD;
		dip->di_random_io = False;
		dip->di_io_type = SEQUENTIAL_IO;
		break;

	    case REVERSE:
		dip->di_io_dir = REVERSE;
		dip->di_random_io = True;
		dip->di_io_type = SEQUENTIAL_IO;
		break;
	}
    }
    return;
}

int
do_postwrite_processing(dinfo_t *dip)
{
    int status = SUCCESS;

#if defined(SCSI)
    if ( (dip->di_dtype->dt_dtype == DT_DISK) && dip->di_unmap_flag &&
	 ((dip->di_scsi_flag && dip->di_sgp) || (dip->di_nvme_flag == True)) ) {
	int rc = SUCCESS;
	if (dip->di_unmap_frequency) {
	    if ((dip->di_pass_count % dip->di_unmap_frequency) == 0) {
		rc = do_unmap_blocks(dip);
	    }
	} else {
	    rc = do_unmap_blocks(dip);
	}
	if (rc == FAILURE) {
	    status = rc;
	    /* Note: Revisit this when spt pulled into dt! */
	    /* Expect spt to report the real error! */
	    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_ERRORS) ) {
		(void)ExecuteTrigger(dip, "scsi");
	    }
	    dip->di_error_count++;
	    if (dip->di_error_count >= dip->di_error_limit) {
		return(status);
	    }
	}
    }
#endif /* defined(SCSI) */
    if (dip->di_pass_cmd) {
	int rc = ExecutePassCmd(dip);
	if (rc == FAILURE) {
	    status = rc;
	    dip->di_error_count++;
	    if (dip->di_error_count >= dip->di_error_limit) {
		return(status);
	    }
	}
    }
    if (dip->di_delete_per_pass) {
	int rc = do_deleteperpass(dip);
	if (rc == FAILURE) {
	    status = rc;
	    dip->di_error_count++;
	    if (dip->di_error_count >= dip->di_error_limit) {
		return(status);
	    }
	}
    }
    return(status);
}

int
do_deleteperpass(dinfo_t *dip)
{
    int status = SUCCESS;
    
    if ( dip->di_delete_per_pass && dip->di_fsfile_flag	    		&&
	 (dip->di_error_count < dip->di_error_limit) 	          	&&
	 ((dip->di_pass_count < dip->di_pass_limit) || dip->di_runtime) ) {
	status = do_delete_files(dip);

    }
    return(status);
}

int
do_delete_files(dinfo_t *dip)
{
    int rc, status = SUCCESS;

    if (dip->di_fd != NoFd) {
	rc = (*dip->di_funcs->tf_close)(dip);
	if (rc == FAILURE) status = rc;
    }
    /* Note: The top level directory is not deleted, need for free space wait! */
    /* Also Note: Recreating the directory may also get a "file system full". */
    rc = delete_files(dip, False);
    if (rc == FAILURE) status = rc;
    if (status == SUCCESS) {
	/* Note: Freeing space may not happen immediately! */
	if (dip->di_delete_delay) {
	    SleepSecs(dip, dip->di_delete_delay);
	}
	(void)do_free_space_wait(dip, dip->di_fsfree_retries);
	if (dip->di_dir) {
	    dip->di_mode = WRITE_MODE;
	    status = setup_directory_info(dip);
	}
    }
    dip->di_file_number = 0;
    dip->di_subdir_number = 0;
    dip->di_subdir_depth = 0;
    dip->di_open_flags |= O_CREAT;
    return(status);
}

/*
 * create_master_log() - Create the master log file.
 *  
 * Inputs: 
 *      dip = The device information pointer.
 *	log_name = The log file name.
 *  
 * Description: 
 *      If the file name contains a format control string '%', then the
 * log file will be expanded via those control strings before open'ing.
 * 
 * Return: 
 * 	SUCCESS / FAILURE - log file open'ed or open failed 
 */
int
create_master_log(dinfo_t *dip, char *log_name)
{
    char logfmt[STRING_BUFFER_SIZE];
    char *path = logfmt;
    char *logpath = NULL;
    int status = SUCCESS;

    if (master_log) {
	(void)CloseFile(dip, &master_logfp);
	FreeStr(dip, master_log);
	master_log = NULL;
    }
    /* Note to self: The log file path is returned in "path". */
    /* TODO: I'm not sure why I wrote it this way, unclear! ;( */
    status = setup_log_directory(dip, path, log_name);
    if (status == FAILURE) {
        return(status);
    }

    /* Handle log prefix strings. */
    if ( strstr(path, "%") ) {
	logpath = FmtLogFile(dip, path, True);
    } else {
	logpath = strdup(log_name);
    }
    if (dip->di_debug_flag || dip->di_fDebugFlag) {
	Printf(dip, "Open'ing master log %s...\n", logpath);
    }
    status = OpenOutputFile(dip, &master_logfp, logpath, "w", EnableErrors);
    if (status == SUCCESS) {
	master_log = logpath;
    } else {
	FreeStr(dip, logpath);
    }
    return(status);
}

/*
 * create_thread_log() - Create the thread log file.
 *  
 * Inputs: 
 *      dip = The device information pointer.
 *  
 * Description: 
 *      If the file name contains a format control string '%', then the
 * log file will be expanded via those control strings before open'ing.
 * 
 * Return: 
 * 	SUCCESS / FAILURE - log file open'ed or open failed 
 */
int
create_thread_log(dinfo_t *dip)
{
    int status = SUCCESS;
    char *mode = (dip->di_logappend_flag) ? "a" : "w";
    FILE *fp;

    if ( strchr(dip->di_log_file, '%') ) {
	char *path = FmtLogFile(dip, dip->di_log_file, True);
	if (path) {
	    FreeStr(dip, dip->di_log_file);
	    dip->di_log_file = path;
	}
    }
    fp = fopen(dip->di_log_file, mode);
    if (fp == NULL) {
	Perror(dip, "fopen() of %s failed", dip->di_log_file);
	status = FAILURE;
    } else {
	dip->di_log_opened = True;
	dip->di_ofp = dip->di_efp = fp;
	if (dip->di_logheader_flag) {
	    /* Messy, but don't want this logged to job log too! */
	    dip->di_joblog_inhibit = True;
	    log_header(dip, False);
	    dip->di_joblog_inhibit = False;
	}
	/* Propogate to the output device too, if there is one! */
	/* Note: This too is messy, and also needs cleanup up! */
	if (dip->di_output_dinfo) {
	    dinfo_t *odip = dip->di_output_dinfo;
	    odip->di_ofp = odip->di_efp = fp;
	}
    }
    return(status);
}

/* Beware: We are called from the main thread, without normal device setup! */
int
do_show_fsmap(dinfo_t *dip)
{
    int status = SUCCESS;

    if (dip->di_input_file == NULL) {
	Eprintf(dip, "You must specify an input file to show the file system map!\n");
	return(FAILURE);
    }
    dip->di_dname = strdup(dip->di_input_file);
    /* Setup the device type and various defaults. */
    status = setup_device_info(dip, dip->di_input_file, dip->di_input_dtype);
    if (status == FAILURE) return(status);
    if (dip->di_fsfile_flag == False) {
        Eprintf(dip, "This device is NOT a file system file: %s\n", dip->di_dname);
        return(FAILURE);
    }

    if ( (status = (*dip->di_funcs->tf_open)(dip, dip->di_initial_flags)) == FAILURE) {
        return(status);
    }
    if (dip->di_fsmap_type == FSMAP_TYPE_MAP_EXTENTS) {
        Offset_t offset = (dip->di_user_position) ? dip->di_file_position : NO_OFFSET;
	status = os_report_file_map(dip, dip->di_fd, dip->di_dsize, offset, dip->di_data_limit);
    } else if (dip->di_fsmap_type == FSMAP_TYPE_LBA_RANGE) {
        Offset_t offset = dip->di_file_position;
        large_t data_limit = dip->di_data_limit;
	uint64_t lba = NO_LBA;
        hbool_t firstTime = True;

	if (dip->di_record_limit != INFINITY) {
	    data_limit = (dip->di_record_limit * dip->di_block_size);
	}
	if (data_limit < (large_t)offset) {
            data_limit += offset;
	}
	for (; ((large_t)offset < data_limit) ;) {
	    lba = os_map_offset_to_lba(dip, dip->di_fd, dip->di_dsize, offset);
	    if (dip->di_fsmap == NULL) {
        	break;
	    }
	    if (firstTime) {
		firstTime = False;
		Printf(dip, "%14s %14s\n", "File Offset", "Physical LBA");
	    }
	    if (lba == NO_LBA) {
		Printf(dip, "%14llu %14s\n", offset, "<not mapped>");
	    } else {
		Printf(dip, "%14llu %14llu\n", offset, lba);
	    }
            offset += dip->di_block_size;
	}
    }
    status = (*dip->di_funcs->tf_close)(dip);
    return(SUCCESS);
}

void
do_sleeps(dinfo_t *dip)
{
    if (dip->di_sleep_value) (void)os_sleep(dip->di_sleep_value);
    if (dip->di_msleep_value) (void)os_msleep(dip->di_msleep_value);
    if (dip->di_usleep_value) (void)os_usleep(dip->di_usleep_value);
    return;
}

hbool_t
is_stop_on_file(dinfo_t *dip)
{
    if (dip->di_stop_on_file) {
	if (os_file_exists(dip->di_stop_on_file) == True) {
	    if (dip->di_verbose_flag) {
		Printf(dip, "Detected stop on file %s, so stopping test...\n",  dip->di_stop_on_file);
	    }
	    return(True);
	}
    }
    return(False);
}

int
stop_job_on_stop_file(dinfo_t *mdip, job_info_t *job)
{
    threads_info_t *tip = job->ji_tinfo;
    int status = SUCCESS;
    dinfo_t *dip = tip->ti_dts[0];
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

/*
 * Note Well: The setting of the file disposition is for the 1st file only!
 * Remember the original dt worked on a single file, not multiples. This
 * file disposition is then applied to all files being tested, whether dt
 * created them or they already existed. Needless to say, this can (and does)
 * lead to incorrect assumptions and results. :-(
 */ 
void
handle_file_dispose(dinfo_t *dip)
{
    if ( (dip->di_io_mode == TEST_MODE)			&&
	 (dip->di_ftype == OUTPUT_FILE)			&&
	 dip->di_fsfile_flag				&&
	 (dip->di_dispose_mode != KEEP_ON_ERROR) ) {
	/* Set the file disposition based on file existance. */
	if ( os_file_exists(dip->di_dname) ) {
	    dip->di_dispose_mode = KEEP_FILE;
	    dip->di_existing_file = True;
	}
    }
    return;
}

int
reopen_output_file(dinfo_t *dip)
{
    int open_mode;
    int status;

    /* Set the read/write flags appropriately. */
    if (dip->di_skip_count || dip->di_raw_flag) {
	open_mode = (dip->di_rwopen_mode | dip->di_write_flags | dip->di_open_flags);
    } else {
	open_mode = (dip->di_write_mode | dip->di_write_flags | dip->di_open_flags);
    }
    status = (*dip->di_funcs->tf_open)(dip, open_mode);
    return(status);
}

/*
 * Note: This should only be called for file systems and file system full!
 */
int
handle_file_system_full(dinfo_t *dip, hbool_t delete_flag)
{
    int status = WARNING;	/* Used to continue to read pass! */

    /* Old sanity check so we don't loop on doing no I/O! */
    if (dip->di_dbytes_written == 0) {
	hbool_t isDiskFull;
	large_t free_space;
	/* We may be overwriting an existing file with random I/O. */
	/* Therefore, try to free space by truncating the file. */
	/* Note: Oddly enough, truncate may encounter FS full! */
	int rc = dt_truncate_file(dip, dip->di_dname, (Offset_t)0, &isDiskFull, EnableErrors);
	/* Maybe space is being freed after delete or truncate? */
	if ( free_space = do_free_space_wait(dip, dip->di_fsfree_retries) ) {
	    /* Note: The main I/O loops expect the file to be open already. */
	    if (dip->di_file_number) dip->di_file_number--; /* Same file please! */
	    status = reopen_output_file(dip);
	    /* SUCCESS status will indicate restarting is Ok. */
	    if (status == SUCCESS) return(status);
	}
	Eprintf(dip, "File system full and no data transferred! [file #%lu, record #%lu]\n",
		dip->di_files_written, (dip->di_records_written + 1));
	ReportErrorInfo(dip, dip->di_dname, OS_ERROR_DISK_FULL, OS_WRITE_FILE_OP, WRITE_OP, True);
	(void)ExecuteTrigger(dip, "write");
	return(FAILURE);
    }

    /* If this pass is already unbuffered, we can trust the data written! */
    
    if ( is_unbuffered_mode(dip) == True ) return(status);

    /* For multiple files, we'll simply discard the last file written. */
    /* If we're running buffered, we cannot trust the last file data! */
    if ( (dip->di_files_written > 1) ) {
	if ( dt_file_exists(dip, dip->di_dname) == True ) {
	    int rc = dt_delete_file(dip, dip->di_dname, EnableErrors);
	    if (rc == SUCCESS) {
		Printf(dip, "Deleted file %s after file system full, continuing...\n", dip->di_dname);
		(void)do_free_space_wait(dip, dip->di_fsfree_retries);
	    } else if (rc == FAILURE) {
		status = rc;
	    }
	}
	/* Adjust counters for this last file being discarded. */
	dip->di_files_written--;
	dip->di_dbytes_written -= dip->di_fbytes_written;
	dip->di_vbytes_written -= dip->di_fbytes_written;
	dip->di_max_data -= dip->di_fbytes_written;
	dip->di_maxdata_written = dip->di_fbytes_written;
	/* Prevent last_fbytes_written from causing read checks! */
	/* We will stop reading when we reach the files written. */
	dip->di_fbytes_written = (large_t)0;
	dip->di_open_flags |= O_CREAT; /* Create files on multiple passes! */
	return(status);
    }
    /* When restarting on file system full, delete files, and set unbuffered. */
    if ( dip->di_fsfull_restart ) {
	if ( restart_on_file_system_full(dip) ) {
	    /* We do not expect file system full *after* we're unbuffered! */
	    dip->di_fsfull_restart = False;
	    /* Set for unbuffered writes (DIO or sync flags). */
	    SetupBufferingMode(dip, &dip->di_open_flags);
	    /* Deleting assumes we are restarting the write pass! */
	    /* Note: Deleting single file during pass is not supported. */
	    if ( (delete_flag == True) && (dip->di_slices == 0) ) {
		/* Note: Also resets flags required for 1st open! */
		if (dip->di_verbose_flag && dip->di_files_written) {
		    Printf(dip, "Deleting %d files after file system full...\n",
			   dip->di_files_written);
		}
		status = do_delete_files(dip);
	    }
	    /* Note: The main I/O loops expect the file to be open already. */
	    status = reopen_output_file(dip);
	    /* SUCCESS status will indicate restarting is Ok. */
	}
    }
    return(status);
}

large_t
do_free_space_wait(dinfo_t *dip, int retries)
{
    large_t data_limit = dip->di_data_limit;
    large_t free_space = 0;
    char *dir = (dip->di_topdirpath) ? dip->di_topdirpath : dip->di_dir;
    int retry = 0;
    int status;
    
    if (dir == NULL) return(dip->di_fs_space_free);
    if (retries == 0) {
	(void)os_get_fs_information(dip, dir);
	return(dip->di_fs_space_free);
    }
    if (dip->di_verbose_flag == True) {
	Printf(dip, "Waiting for free space on directory %s, data limit "LUF" bytes...\n",
	       dir, data_limit);
    }
    /* Prime the initial free space. */
    status = os_get_fs_information(dip, dir);
    if (status == FAILURE) return(free_space);

    /* Loop until retry limit, while free space is changing! */
    do {
	free_space = dip->di_fs_space_free;
	/* Stop when we've reached sufficient free space. */
	if (free_space >= data_limit) break;

	if ( THREAD_TERMINATING(dip) ) break;

	if (dip->di_debug_flag || dip->di_fDebugFlag) {
	    Printf(dip, "Waiting for free space, current "LUF" bytes, (retry %d)\n",
		   free_space, (retry + 1) );
	}
	SleepSecs(dip, dip->di_fsfree_delay);
	status = os_get_fs_information(dip, dir);
	if (status == FAILURE) break;

	/* Note: Free space may increase or decrease depending on threads active. */
	/* Our goal here is to wait for free space or wait while it is increasing. */

    } while ( (++retry < retries) &&
	      ( (dip->di_fs_space_free == 0) || (free_space < dip->di_fs_space_free) ) );

    if (dip->di_verbose_flag) {
	Printf(dip, "Free space is "LUF" bytes, after %d wait retries (%d secs).\n",
	       dip->di_fs_space_free, retry, (dip->di_fsfree_delay * retry));
    }
    return(dip->di_fs_space_free);
}

/*
 * Make stderr buffered, so timing is not affected by output.
 */
int
make_stderr_buffered(dinfo_t *dip)
{
    int status = SUCCESS;

    /*
     * The concept here is simple, set stderr buffered so multiple processes
     * don't have their output intermixed.  This piece of code has been very
     * problematic, so if you have problems with garbled output, remove it.
     */
    /*
     * Since stderr is normally unbuffered, we make it buffered here.
     * The reason for this is to keep text from getting fragnmented, esp.
     * with multiple processes or threads all writing to the terminal.
     * 
     * Note: Pretty sure this does *not* need to be device specific.
     * In fact, each IO thread will eventually write to it's own log file.
     */
    if ( StderrIsAtty && (dip->di_stderr_buffer == NULL) ) {
	dip->di_stderr_buffer = Malloc(dip, dip->di_log_bufsize);
	/*
	 * Can't use log buffer, or we get undesirable results.
	 */
	if (setvbuf(efp, dip->di_stderr_buffer, _IOFBF, dip->di_log_bufsize) < 0) {
	    Perror (dip, "setvbuf() failed, exiting...");
	    status = FAILURE;
	}
    }
    return(status);
}

int
setup_log_directory(dinfo_t *dip, char *path, char *log)
{
    char *bp = path;
    char *dir = dip->di_log_dir;
    int status = SUCCESS;

    /*
     * Allow a log file directory to redirect logs easier. 
     * Note: We don't create the full directory path, just last!
     */
    if (dir) {
	if ( strstr(dir, "%") ) {
	    dir = FmtLogFile(dip, dir, True);
	    FreeStr(dip, dip->di_log_dir);
	    dip->di_log_dir = dir;
	}
	if (os_file_exists(dir) == False) {
	    if (dip->di_debug_flag || dip->di_fDebugFlag) {
		Printf(dip, "Creating directory %s...\n", dir);
	    }
	    status = os_create_directory(dir, DIR_CREATE_MODE);
	}
	bp += sprintf(bp, "%s%c", dir, dip->di_dir_sep);
    } else if ( NEL(log, CONSOLE_NAME, CONSOLE_LEN) ) {
	/* Note: We do not expand the log directory here! */
        char *dsp = strrchr(log, dip->di_dir_sep);
	if (dsp) {
            char *dir = log;
            *dsp = '\0';
            /* create the log directory if it does not exist! */
	    if (os_file_exists(log) == False) {
		if (dip->di_debug_flag || dip->di_fDebugFlag) {
		    Printf(dip, "Creating directory %s...\n", log);
		}
		status = os_create_directory(log, DIR_CREATE_MODE);
	    }
            *dsp = dip->di_dir_sep;
	}
    }
    /* Add log name to directory, as required. */
    bp += sprintf(bp, "%s", log);
    return(status);
}

/*
 * This function creates a unique log file name.
 * 
 * Inputs:
 * 	dip = The device information pointer.
 * 
 * Return Value:
 * 	Returns SUCCESS / FAILURE if log open failed.
 */
int
create_unique_thread_log(dinfo_t *dip)
{
    hbool_t make_unique_log_file = True;
    char logfmt[STRING_BUFFER_SIZE];
    char *logpath = dip->di_log_file;
    char *path = logfmt;
    int status = SUCCESS;

    status = setup_log_directory(dip, path, dip->di_log_file);
    if (status == FAILURE) {
        return(status);
    }
    /*
     * For a single thread use the log file name, unless told to be unique.
     * Note: If multiple devices specified, also create unique log files!
     */
    if ( (dip->di_multiple_devs == False) &&
	 (dip->di_threads <= 1) && (dip->di_unique_log == False) ) {
	make_unique_log_file = False;
    }
    if (make_unique_log_file) {
	/*
	 * Create a unique log file per thread.
	 */
	/* Add default postfix, unless user specified their own via "%". */
	if ( strstr(dip->di_log_file, "%") == (char *) 0 ) {
	    strcat(path, dip->di_file_sep);
	    strcat(path, dip->di_file_postfix);
	}
    }
    /* Format special control strings or log directory + log file name. */
    logpath = FmtLogFile(dip, path, True);
    FreeStr(dip, dip->di_log_file);
    dip->di_log_file = logpath;
    if (dip->di_debug_flag || dip->di_fDebugFlag) {
	Printf(dip, "Job %u, Thread %d, thread log file is %s...\n",
	       dip->di_job->ji_job_id, dip->di_thread_number, logpath);
    }
    status = create_thread_log(dip);
    return(status);
}

void
report_pass_statistics(dinfo_t *dip)
{
    if (dip->di_raw_flag) {
	report_pass(dip, RAW_STATS);
    } else if (dip->di_mode == READ_MODE) {
	report_pass(dip, READ_STATS);
    } else {
	report_pass(dip, WRITE_STATS);
    }
    return;
}

/*
 * Note: This setup of directories, base name, and device names,
 * needs to be cleaned up. It's a left over hack from the original
 * port, but is messy as heck, and misleading even to the author!
 */
int
format_device_name(dinfo_t *dip, char *format)
{
    int status = SUCCESS;
    char *path = FmtFilePath(dip, format, True);
    if (path) {
	FreeStr(dip, dip->di_dname);
	dip->di_dname = path;
	/* Update the base name too! */
	if (dip->di_bname) {
	    /* If newly created path has a directory, update the base name w/directory. */
	    /* Note: If the directory is not added yet, we need to update the base name. */
	    /* This allows dir=/var/tmp of=dt-%user-%uuid.data expansion to work properly! */
	    if (dip->di_dir && strrchr(path, dip->di_dir_sep)) {
		status = setup_base_name(dip, path);
	    } else {
		/* make_file_name() uses the base name, so ensure it's updated! */
		FreeStr(dip, dip->di_bname);
		dip->di_bname = strdup(path);
	    }
	}
    }
    return(status);
}

int
setup_base_name(dinfo_t *dip, char *file)
{
    int status = SUCCESS;
    char *p;
    if (p = strrchr(file, dip->di_dir_sep)) {
	*p = '\0';	/* Separate the directory from the file name. */
	dip->di_dir = strdup(file);
	*p++ = dip->di_dir_sep;
	FreeStr(dip, dip->di_bname);
	dip->di_bname = strdup(p);
	if (dip->di_debug_flag || dip->di_fDebugFlag) {
	    Printf(dip, "Directory: %s, File: %s, Base Name: %s\n",
		   dip->di_dir, file, dip->di_bname);
	}
	status = setup_directory_info(dip);
    }
    return(status);
}

int
setup_thread_names(dinfo_t *dip)
{
    char filefmt[PATH_BUFFER_SIZE];
    char *bp = filefmt;
    int status = SUCCESS;

    if ( dip->di_fsfile_flag == False ) return(status);
    *bp = '\0';

    /*
     * Special handling for a single thread or same file (slices).
     */
    if ( (dip->di_fileperthread == False) || dip->di_slices ||
         ( (dip->di_threads <= 1) && (dip->di_unique_file == False) ) ) {
	if ( dip->di_dir ) {
            /* Note: When writing, this creates the directory! */
            status = setup_directory_info(dip);
        }
	if ( (status == SUCCESS) && (dip->di_unique_file == False) ) {
	    /* We need to formulate path for setting file disposition. */
	    if (dip->di_dirpath || dip->di_subdir || dip->di_file_limit) {
		/* Beware: This adds directory to existing device name! */
		(void)make_file_name(dip);
	    }
	}
	return(status);
    }

    /*
     * Make the directory name or file name unique per thread.
     */ 
    if (dip->di_multiple_files) {
	char *path;
	/* Create a unique directory for each thread. */
	if (dip->di_dir) {
	    /* Note: The top level directory was already added, usually "d0". */
	    /* This naming matches the current dt, other than older dt appends PID. */
#if 1
	    /* Create a unique subdirectory for each thread. */
	    /* Note: Especially important when top level directory is a moount point! */
	    (void)sprintf(filefmt, "%s%c%s", dip->di_dir, dip->di_dir_sep, dip->di_file_postfix);
#else
	    if (dip->di_multiple_dirs) {
		(void)sprintf(filefmt, "%s%s%s", dip->di_dir, dip->di_file_sep, dip->di_file_postfix);
	    } else {
		/* Create a unique subdirectory for each thread. */
		(void)sprintf(filefmt, "%s%c%s", dip->di_dir, dip->di_dir_sep, dip->di_file_postfix);
	    }
#endif /* 1 */
	} else {
	    strcpy(filefmt, dip->di_file_postfix);
	}
	/* Format the directory path. */
	path = FmtFilePath(dip, filefmt, True);
	FreeStr(dip, dip->di_dir);
	dip->di_dir = strdup(path);
	dip->di_unique_file = False;
	status = setup_directory_info(dip);
	FreeStr(dip, path);
    } else {	/* Single file setup. */
	/* Create a unique file name for each thread. */
	(void)sprintf(filefmt, "%s%s%s", dip->di_dname, dip->di_file_sep, dip->di_file_postfix);
        status = format_device_name(dip, filefmt);
    }
    return(status);
}

void
finish_test_common(dinfo_t *dip, int thread_status)
{
    dinfo_t *odip = dip->di_output_dinfo;

    if (dip->di_syslog_flag) {
	SystemLog(dip, LOG_INFO, "Finished: %s", dip->di_cmd_line);
    }

    if (dip->di_history_size &&
	(dip->di_history_dump == True) && (dip->di_history_dumped == False) ) {
	dump_history_data(dip);
    }
    if (odip && odip->di_history_size &&
	(odip->di_history_dump == True) && (odip->di_history_dumped == False) ) {
	dump_history_data(odip);
    }

    /* If we've been writing, report command to reread the file data. */
    if (dip->di_logtrailer_flag && (dip->di_ftype == OUTPUT_FILE) ) {
	if ( (dip->di_iobehavior == DT_IO) || (dip->di_iobehavior == DTAPP_IO) ) {
	    report_reread_data(dip, False, reread_file);
	}
    }

    /*
     * If thread status is FAILURE, log the command line.
     * Also log to thread log when log trailer flag enabled.
     */
    if ( (thread_status == FAILURE) || dip->di_logtrailer_flag ) {
	log_header(dip, (thread_status == FAILURE) ? True : False);
    }
    if (dip->di_debug_flag || dip->di_pDebugFlag || dip->di_tDebugFlag) {
	Printf (dip, "Thread exiting with status %d...\n", thread_status);
    }
    return;
}

/* Note: This is only used by dt's I/O behavior! */
/* Returns an updated exit status for the caller! */
int
finish_test(dinfo_t *dip, int exit_code, hbool_t do_cleanup)
{
    int status = SUCCESS;

    /*
     * Close file, which for AIO waits for outstanding I/O's,
     * before reporting statistics so they'll be correct.
     */
    if (dip && do_cleanup && (dip->di_fd != NoFd) ) {
	status = (*dip->di_funcs->tf_close)(dip);
	if (status == FAILURE) exit_code = status;
    }
    gather_stats(dip);			/* Gather the device statistics. */
    gather_totals(dip);			/* Update the total statistics.	*/
    report_stats(dip, TOTAL_STATS);

    /*
     * If keep on error, do the appropriate thing!
     */
    if (dip->di_dispose_mode == KEEP_ON_ERROR) {
	/* Note: Signals cause files to be kept! */
	if ( (exit_code != SUCCESS) && (exit_code != END_OF_FILE) ) {
	    dip->di_dispose_mode = KEEP_FILE;
	} else if (dip && (dip->di_existing_file == False)) {
	    dip->di_dispose_mode = DELETE_FILE;
	}
    }
    /*
     * Delete the output file, if requested to do so. May become common!
     */
    if ( do_cleanup && dip->di_output_file && dip->di_fsfile_flag &&
	 (dip->di_io_mode == TEST_MODE) && (dip->di_dispose_mode == DELETE_FILE) ) {
	int status = delete_files(dip, True);
	if (status == FAILURE) exit_code = status; /* Delete failed, that's a test failure! */
    }

    if ( (dip->di_eof_status_flag == False) && (exit_code == END_OF_FILE) ) {
	exit_code = SUCCESS;		/* Map end-of-file status to Success! */
    }
    
    finish_test_common(dip, exit_code);

    if (exit_code == WARNING) {
	exit_code = SUCCESS;		/* Map warning errors to Success! */
    }
    /* Note: This should probably go too! */
    //if (dip->di_force_core_dump && (exit_code != SUCCESS) && (exit_code != END_OF_FILE)) {
    //	abort();			/* Generate a core dump for debugging. */
    //}
    /*
     * Map signal numbers and/or other errno's to FAILURE. (cleanup)
     * ( easier for scripts to handle! )
     */
    if ( (exit_code != FAILURE) && (exit_code != SUCCESS) && (exit_code != END_OF_FILE) ) {
	exit_code = FAILURE;			/* Usually a signal number. */
    }
    return(exit_code);
}

/*
 *
 * parse_args() - Parse 'dt' Program Arguments.
 *
 * Inputs:
 * 	dip = The device information pointer.
 *	argc = The number of arguments.
 *	argv = Array pointer to arguments.
 *
 * Return Value;
 *	Returns Success/Failure = Parsed Ok/Parse Error.
 */
int
parse_args(dinfo_t *dip, int argc, char **argv)
{
    int i;
    char *p, *string;
    int status = SUCCESS;

    for (i = 0; i < argc; i++) {
	string = argv[i];
	/* 
         * Note: Skip these characters, which are used by other I/O tool parsers.
	 * For example: Tool parsers map common options to: --threads=value 
	 * If this poses a problem, we can control this by tool I/O behavior. 
	 * BTW: I am not removing the existing "--" options below at this time! 
	 */ 
	if (match(&string, "--") || match(&string, "-")) {  /* Optional (skip). */
	    ;
	}
	if (dip->di_iobf && dip->di_iobf->iob_parser) {
	    status = (*dip->di_iobf->iob_parser)(dip, string);
	    if (status == STOP_PARSING) { /* Stop parsing, "help", etc */
		return( HandleExit(dip, SUCCESS) );
	    } else if (status == FAILURE) {
		return( HandleExit(dip, status) );
	    } else if (status == SUCCESS) {
		continue;
	    }
	}
	if (match (&string, "aios=")) {
	    dip->di_aio_bufs = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if (dip->di_aio_bufs) {
		dip->di_aio_flag = True;
	    } else { /* zero disables AIO too! */
		dip->di_aio_flag = False;
	    }
	    continue;
	}
	if (match (&string, "alarm=")) {
	    dip->di_alarmtime = time_value(dip, string);
	    continue;
	}
	if ( match(&string, "bg") || match(&string, "&") ) {
	    dip->di_async_job = True;
	    continue;
	}
	if (match (&string, "bufmodes=")) {
	    /* Note: Allow an empty string to override automation/workloads! */
	    if (*string == '\0') {
		dip->di_bufmode_count = 0;
		continue;
	    }
	bufmode_loop:
	    if (match(&string, ","))
		goto bufmode_loop;
	    if (*string == '\0')
		continue;
	    if (dip->di_bufmode_count == NUM_BUFMODES) {
		Eprintf(dip, "Too many buffering modes specified, max is %d\n", NUM_BUFMODES);
		return ( HandleExit(dip, FAILURE) );
	    }
	    if (match(&string, "buffered")) {
		dip->di_buffer_modes[dip->di_bufmode_count++] = BUFFERED_IO;
	    } else if (match(&string, "unbuffered")) {
		dip->di_fsalign_flag = True;
		dip->di_buffer_modes[dip->di_bufmode_count++] = UNBUFFERED_IO;
	    } else if (match(&string, "cachereads")) {
		dip->di_fsalign_flag = True;
		dip->di_buffer_modes[dip->di_bufmode_count++] = CACHE_READS;
	    } else if (match(&string, "cachewrites")) {
		dip->di_fsalign_flag = True;
		dip->di_buffer_modes[dip->di_bufmode_count++] = CACHE_WRITES;
	    } else {
		Eprintf(dip, "Invalid bufmode keyword: %s\n", string);
		return ( HandleExit(dip, FAILURE) );
	    }
	    goto bufmode_loop;
	}
	if (match (&string, "boff=")) {
	    if (match(&string, "dec")) {
		dip->di_boff_format = DEC_FMT;
	    } else if (match (&string, "hex")) {
		dip->di_boff_format = HEX_FMT;
	    } else {
		Eprintf(dip, "Valid buffer offset formats are: dec or hex\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "dfmt=")) {
	    if (match(&string, "byte")) {
		dip->di_data_format = BYTE_FMT;
	    } else if (match (&string, "word")) {
		dip->di_data_format = WORD_FMT;
	    } else {
		Eprintf(dip, "Valid data formats are: byte or word\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	/* Just in case we need to override our default! */
	if (match (&string, "family=")) {
	    if (match(&string, "ipv4")) {
		dip->di_inet_family = AF_INET;
	    } else if (match (&string, "ipv6")) {
		dip->di_inet_family = AF_INET6;
	    } else {
		Eprintf(dip, "Valid INET family is: ipv4 or ipv6\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "keepalive=")) {
	    if (dip->di_keepalive) FreeStr(dip, dip->di_keepalive);
	    dip->di_keepalive = strdup(string);
	    dip->di_user_keepalive = True;
	    continue;
	}
	if (match (&string, "keepalivet=")) {
	    dip->di_keepalive_time = time_value(dip, string);
	    continue;
	}
	if (match (&string, "pkeepalive=")) {
	    if (dip->di_pkeepalive) FreeStr(dip, dip->di_pkeepalive);
	    dip->di_pkeepalive = strdup(string);
	    dip->di_user_pkeepalive = True;
	    continue;
	}
	if (match (&string, "tkeepalive=")) {
	    if (dip->di_tkeepalive) FreeStr(dip, dip->di_tkeepalive);
	    dip->di_tkeepalive = strdup(string);
	    dip->di_user_tkeepalive = True;
	    continue;
	}
	if (match (&string, "noprogt=")) {
	    dip->di_noprogtime = time_value(dip, string);
	    if (dip->di_noprogtime) {
		dip->di_noprog_flag = True;
	    }
	    continue;
	}
	if (match (&string, "noprogtt=")) {
	    dip->di_noprogttime = time_value(dip, string);
	    if (dip->di_noprogttime && !dip->di_noprogtime) {
		dip->di_noprog_flag = True;
		dip->di_noprogtime = dip->di_noprogttime;
	    }
	    continue;
	}
	if (match (&string, "notime=")) {
	    int i;
	    optiming_t *optp;
	    /*
	     * May loop through table more than once, as we parse
	     * multiple options: notime=fsync,open,close...
	     */
	    do {
		optp = &optiming_table[OPEN_OP];
		for ( i = OPEN_OP; (i < NUM_OPS); optp++, i++ ) {
		    if ( match(&string, ",") ) break;
		    if ( match(&string, optp->opt_name) ) {
			optp->opt_timing_flag = False;
			break;
		    }
		}
		if (i == NUM_OPS) break;	/* optype not found! */
		if (*string == '\0') break; /* done parsing */
	    } while (True);
	    if ( i == NUM_OPS ) {
		Eprintf(dip, "%s is not a valid operation type to disable!\n", string);
		optp = &optiming_table[OPEN_OP];
		Fprintf(dip, "Valid operation types are: ");
		for ( i = OPEN_OP; (i < NUM_OPS); optp++, i++ ) {
		    Fprint(dip, "%s ", optp->opt_name);
		}
		Fprint(dip, "\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "totals=")) {
	    totals_msg = strdup(string);
	    continue;
	}
	if (match (&string, "align=")) {
	    if (match (&string, "rotate")) {
		dip->di_rotate_flag = True;
		continue;
	    }
	    dip->di_align_offset = (u_int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "array=")) {
	    if (dip->di_array) free(dip->di_array);
	    dip->di_array = strdup(string);
	    continue;
	}
	if (match (&string, "bs=")) {
	    if (match(&string, "random")) {
		if (dip->di_user_min == False) {
		    dip->di_min_size = MIN_RANDOM_SIZE;
		}
		if (dip->di_user_max == False) {
		    dip->di_max_size = MAX_RANDOM_SIZE;
		}
		dip->di_variable_flag = True;
		continue;
	    }
	    /* bs=value */
	    dip->di_min_size = 0;
	    dip->di_max_size = 0;
	    dip->di_variable_flag = False;
	    dip->di_block_size = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if ((ssize_t)dip->di_block_size <= (ssize_t) 0) {
		Eprintf(dip, "The block size must be positive and non-zero.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "ibs=")) {
	    dip->di_iblock_size = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if ((ssize_t)dip->di_iblock_size <= (ssize_t) 0) {
		Eprintf(dip, "The read block size must be positive and non-zero.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "obs=")) {
	    dip->di_oblock_size = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if ((ssize_t)dip->di_oblock_size <= (ssize_t) 0) {
		Eprintf(dip, "The write block size must be positive and non-zero.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "capacity=")) {
	    if (match (&string, "max")) {
		dip->di_max_capacity = True;
	    } else {
		dip->di_user_capacity = large_number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if (match (&string, "capacityp=")) {
	    dip->di_capacity_percentage = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if (dip->di_capacity_percentage > 100) {
		Eprintf(dip, "The capacity percentage range is 0-100!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
        /* Force Corruption Options */
	if (match (&string, "corrupt_index=")) {
	    dip->di_corrupt_index = (int32_t)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_force_corruption = True;
	    continue;
	}
	if (match (&string, "corrupt_length=")) {
	    dip->di_corrupt_length = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_force_corruption = True;
	    continue;
	}
	if (match (&string, "corrupt_pattern=")) {
	    dip->di_corrupt_pattern = number(dip, string, HEX_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_force_corruption = True;
	    continue;
	}
	if (match (&string, "corrupt_step=")) {
	    dip->di_corrupt_step = (int32_t)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_force_corruption = True;
	    if (dip->di_corrupt_step && (dip->di_corrupt_length == sizeof(CORRUPTION_PATTERN)) ) {
		dip->di_corrupt_length *= 2;	/* Force two corrupted sections. */
	    }
	    continue;
	}
	if (match (&string, "corrupt_reads=")) {
	    dip->di_corrupt_reads = large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_force_corruption = True;
	    dip->di_corrupt_writes = 0;
	    continue;
	}
	if (match (&string, "corrupt_writes=")) {
	    dip->di_corrupt_writes = large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_force_corruption = True;
	    dip->di_corrupt_reads = 0;
	    continue;
	}
        /* End of Corruption Options. */
	if (match (&string, "dsize=")) {
	    dip->di_device_size = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    /*
	     * Adjust the dump limit to the device size (as appropriate).
	     */
	    if ( (dip->di_dump_limit == DEFAULT_DUMP_LIMIT) &&
		 (dip->di_device_size > DEFAULT_DUMP_LIMIT) ) {
		dip->di_dump_limit = dip->di_device_size;
	    }
	    continue;
	}
	if ( match(&string, "ffreq=") || match(&string, "flush_freq=") ) {
	    dip->di_fsync_frequency = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "fstrim_freq=")) {
	    dip->di_fstrim_frequency = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_fstrim_flag = True;
	    continue;
	}
	if ( match(&string, "hbufs=") || match(&string, "history_bufs=") ) {
	    dip->di_history_bufs = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if ( match(&string, "hbsize=") || match(&string, "history_bsize=") ) {
		dip->di_history_bsize = (int)number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
		continue;
	    }
	if ( match(&string, "hdsize=") || match(&string, "history_data=") ) {
	    dip->di_history_data_size = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if ( match(&string, "history=") || match(&string, "history_size=") ) {
	    dip->di_history_size = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "lba=")) {
	    dip->di_lbdata_flag = True;
	    dip->di_lbdata_addr = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_user_lbdata = True;
	    continue;
	}
	if (match (&string, "lbs=")) {
	    dip->di_lbdata_flag = True;
	    dip->di_lbdata_size = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_user_lbsize = True;
	    if ((ssize_t)dip->di_lbdata_size <= (ssize_t) 0) {
		Eprintf(dip, "lbdata size must be positive and non-zero.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "maxbad=")) {
	    dip->di_max_bad_blocks = (u_int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if ( match(&string, "count=") || match(&string, "records=") ) {
	    if (dip->di_fsincr_flag) {
		Eprintf(dip, "Cannot use record count with file size increment option!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_record_limit = large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "cancel_delay=")) {
	    cancel_delay = (u_int) number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match(&string, "delete_delay=")) {
	    if (match (&string, "random")) {
		dip->di_delete_delay = RANDOM_DELAY_VALUE;
	    } else {
		dip->di_delete_delay = (u_int)number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if (match(&string, "fsfree_delay=")) {
	    dip->di_fsfree_delay = (u_int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match(&string, "fsfree_retries=")) {
	    dip->di_fsfree_retries = (u_int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "kill_delay=")) {
	    kill_delay = (u_int) number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if ( match(&string, "odelay=") || match(&string, "open_delay=") ) {
	    if (match (&string, "random")) {
		dip->di_open_delay = RANDOM_DELAY_VALUE;
	    } else {
		dip->di_open_delay = (u_int) number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if ( match(&string, "cdelay=") || match(&string, "close_delay=") ) {
	    if (match (&string, "random")) {
		dip->di_close_delay = RANDOM_DELAY_VALUE;
	    } else {
		dip->di_close_delay = (u_int) number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if ( match(&string, "edelay=") || match(&string, "end_delay=") ) {
	    if (match (&string, "random")) {
		dip->di_end_delay = RANDOM_DELAY_VALUE;
	    } else {
		dip->di_end_delay = (u_int) number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if ( match(&string, "fdelay=") || match(&string, "forced_delay=") ) {
	    dip->di_forced_delay = (u_int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if ( match(&string, "pdelay=") || match(&string, "pipe_delay=") ) {
	    PipeDelay = (uint32_t)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if ( match(&string, "rdelay=") || match(&string, "read_delay=") ) {
	    if (match (&string, "random")) {
		dip->di_read_delay = RANDOM_DELAY_VALUE;
	    } else {
		dip->di_read_delay = (u_int)number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if ( match(&string, "sdelay=") || match(&string, "start_delay=") ) {
	    if (match (&string, "random")) {
		dip->di_start_delay = RANDOM_DELAY_VALUE;
	    } else {
		dip->di_start_delay = (u_int)number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if ( match(&string, "tdelay=") || match(&string, "term_delay=") ) {
	    dip->di_term_delay = (u_int) number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if ( match(&string, "vdelay=") || match(&string, "verify_delay=") ) {
	    if (match (&string, "random")) {
		dip->di_verify_delay = RANDOM_DELAY_VALUE;
	    } else {
		dip->di_verify_delay = (u_int)number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if ( match(&string, "wdelay=") || match(&string, "write_delay=") ) {
	    if (match (&string, "random")) {
		dip->di_write_delay = RANDOM_DELAY_VALUE;
	    } else {
		dip->di_write_delay = (u_int) number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if ( match(&string, "iops=") ) {
	    dip->di_iops = atof(string);
	    if (dip->di_iops) {
		dip->di_iops_usecs = (int)(uSECS_PER_SEC / dip->di_iops);
		if (dip->di_iops_type == IOPS_MEASURE_IOMON) {
		    dip->di_iops_adjust = (int)dip->di_iops_usecs;
		    dip->di_read_delay = dip->di_iops_usecs;
		    dip->di_write_delay = dip->di_iops_usecs;
		}
	    } else {
		Fprintf(dip, "Please enter the number of I/O's per second value!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_sleep_res = SLEEP_USECS;
	    continue;
	}
	if ( match(&string, "iops_type=") ) {
	    if ( match(&string, "exact") ) {
		dip->di_iops_type = IOPS_MEASURE_EXACT;
	    } else if ( match(&string, "iomon") || match(&string, "lazy") ) {
		dip->di_iops_type = IOPS_MEASURE_IOMON;
		if (dip->di_iops_usecs && (dip->di_iops_adjust == 0) ) {
		    dip->di_iops_adjust = (int)dip->di_iops_usecs;
		    dip->di_read_delay = dip->di_iops_usecs;
		    dip->di_write_delay = dip->di_iops_usecs;
		}
	    } else {
		Fprintf(dip, "Valid I/O measurement types are: 'exact' or 'lazy'\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "io_delay=")) {
	    if (match (&string, "random")) {
		dip->di_read_delay = dip->di_write_delay = RANDOM_DELAY_VALUE;
	    } else {
		unsigned int value = number(dip, string, ANY_RADIX, &status, True);
		if (status == SUCCESS) {
		    dip->di_read_delay = dip->di_write_delay = value;
		} else {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if (match (&string, "retry_delay=")) {
	    dip->di_retry_delay = (u_int) number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "retry_error=")) {
	    int error_code;
	    if (dip->di_retry_entries == RETRY_ENTRIES) {
		Eprintf(dip, "Maximum retry entries is %d.\n", RETRY_ENTRIES);
		return ( HandleExit(dip, FAILURE) );
	    }
	    /* Add a few errors that can be referenced by name. */
#if defined(WIN32)
	    if (match(&string, "ERROR_BUSY")) {
		error_code = ERROR_BUSY;			// 170L
	    } else if (match(&string, "ERROR_DISK_FULL")) {
		error_code = ERROR_DISK_FULL;			// 112L
	    } else if (match(&string, "ERROR_IO_DEVICE")) {
		error_code = ERROR_IO_DEVICE;			// 1117L
	    } else if (match(&string, "ERROR_VC_DISCONNECTED")) {
		error_code = ERROR_VC_DISCONNECTED;		// 240L
	    } else if (match(&string, "ERROR_UNEXP_NET_ERR")) {
		error_code = ERROR_UNEXP_NET_ERR;		// 59L
	    } else if (match(&string, "ERROR_SEM_TIMEOUT")) {
		error_code = ERROR_SEM_TIMEOUT;			// 121L
	    } else if (match(&string, "ERROR_BAD_NETPATH")) {
		error_code = ERROR_BAD_NETPATH;			// 53L
	    } else if (match(&string, "ERROR_NETNAME_DELETED")) {
		error_code = ERROR_NETNAME_DELETED;		// 64L
	    } else if (match(&string, "ERROR_DEVICE_NOT_CONNECTED")) {
		error_code = ERROR_DEVICE_NOT_CONNECTED;	// 1167L
	    } else if (match(&string, "ERROR_NO_SYSTEM_RESOURCES")) {
		error_code = ERROR_NO_SYSTEM_RESOURCES;		// 1450L
	    } else if (match(&string, "*")) {
		error_code = -1;				// All errors!
	    } else {
		error_code = (int)number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
#else /* !defined(WIN32) */
	    /* Note: I think the value of these varies by Host OS! */
	    if (match(&string, "EBUSY")) {
		error_code = EBUSY;
	    } else if (match(&string, "EIO")) {
		error_code = EIO;
	    } else if (match(&string, "ENXIO")) {
		error_code = ENXIO;
	    } else if (match(&string, "ENODEV")) {
		error_code = ENODEV;
	    } else if (match(&string, "ENOSPC")) {
		error_code = ENODEV;
	    } else if (match(&string, "ESTALE")) {
		error_code = ESTALE;
	    } else if (match(&string, "*")) {
		error_code = -1;
	    } else {
		error_code = (int)number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
#endif /* defined(WIN32) */
	    dip->di_retry_errors[dip->di_retry_entries++] = error_code;
	    continue;
	}
	if (match (&string, "retry_limit=")) {
	    dip->di_retry_limit = (u_int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "retryDC_delay=")) {
	    dip->di_retryDC_delay = (u_int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "retryDC_limit=")) {
	    dip->di_retryDC_limit = (u_int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	/* Note: Not currently used, historic! */
	if (match (&string, "term_retries=")) {
	    term_wait_retries = (u_int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "term_wait=")) {
	    dip->di_term_wait_time = time_value(dip, string);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "errors=")) {
	    dip->di_error_limit = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
            dip->di_user_errors = True;
	    continue;
	}
	if (match (&string, "hz=")) {
	    hertz = (u_int) number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "incr=")) {
	    dip->di_user_incr = True;
	    if (match (&string, "var")) {
		dip->di_variable_flag = True;
	    } else {
		dip->di_variable_flag = False;
		dip->di_incr_count = number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    continue;
	}
	if ( match(&string, "dlimit=") || match(&string, "dump_limit=") ) {
	    dip->di_dump_limit = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if ( match(&string, "limit=") || match(&string, "data_limit=") ) {
	    if (match(&string, "random")) {
		dip->di_min_limit = MIN_DATA_LIMIT;
		dip->di_max_limit = MAX_DATA_LIMIT;
		if (dip->di_incr_limit == 0) {
		    dip->di_variable_limit = True;
		}
		dip->di_data_limit = dip->di_max_limit;
	    } else {
		/* limit=value */
		dip->di_data_limit = large_number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    }
	    dip->di_user_limit = dip->di_data_limit;
	    /* Override the max limit as well, if previously specified! */
	    if (dip->di_max_limit) {
		dip->di_max_limit = dip->di_data_limit;
	    }
	    if (dip->di_record_limit == 0) {
		dip->di_record_limit = INFINITY; /* Don't stop on record limit. */
	    }
	    continue;
	}
	if (match (&string, "incr_limit=")) {
	    if (match (&string, "var")) {
		dip->di_variable_limit = True;
	    } else {
		dip->di_incr_limit = (size_t) number(dip, string, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
		dip->di_variable_limit = False;
	    }
	    continue;
	}
	if (match (&string, "max_limit=")) {
	    dip->di_max_limit = (size_t) number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if (dip->di_incr_limit == 0) {
		dip->di_variable_limit = True;
	    }
	    dip->di_data_limit = dip->di_user_limit = dip->di_max_limit;
	    continue;
	}
	if (match (&string, "min_limit=")) {
	    dip->di_min_limit = (size_t) number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if (dip->di_incr_limit == 0) {
		dip->di_variable_limit = True;
	    }
	    continue;
	}
	if (match (&string, "maxdatap=")) {
	    dip->di_max_data_percentage = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if (dip->di_max_data_percentage > 100) {
		Eprintf(dip, "The max data percentage range is 0-100!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "maxdata=")) {
	    dip->di_max_data = large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "ralign=")) {
	    dip->di_io_type = RANDOM_IO;
	    dip->di_user_ralign = True;
	    dip->di_random_align = large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "rlimit=")) {
	    dip->di_io_type = RANDOM_IO;
	    dip->di_rdata_limit = large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "max=")) {
	    dip->di_user_max = True;
	    dip->di_max_size = (size_t)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "min=")) {
	    dip->di_user_min = True;
	    dip->di_min_size = (size_t)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "enable=")) {
	eloop:
	    if (match(&string, ","))
		goto eloop;
	    if (*string == '\0')
		continue;
	    if (match(&string, "aio")) {
		dip->di_aio_flag = True;
		goto eloop;
	    }
	    if (match(&string, "async")) {
		dip->di_async_job = True;
		goto eloop;
	    }
	    if ( match(&string, "btags") ) {
		dip->di_btag_flag = True;
		dip->di_fsalign_flag = True;
		goto eloop;
	    }
	    if (match(&string, "bypass")) {
		dip->di_bypass_flag = True;
		goto eloop;
	    }
	    if (match(&string, "cerrors")) {
		dip->di_cerrors_flag = True;
		goto eloop;
	    }
	    if (match(&string, "child")) {
		dip->di_child_flag = True;
		dip->di_logpid_flag = True;
		dip->di_unique_file = True;
		goto eloop;
	    }
	    if (match(&string, "compare")) {
		dip->di_compare_flag = True;
		goto eloop;
	    }
	    if (match(&string, "xcompare")) {
		dip->di_xcompare_flag = True;
		goto eloop;
	    }
	    if (match(&string, "coredump")) {
		dip->di_force_core_dump = True;
		goto eloop;
	    }
	    if (match(&string, "deleteerrorlog")) {
		DeleteErrorLogFlag = True;
		if (error_log) {
		    if (error_logfp) {
			(void)CloseFile(dip, &error_logfp);
		    }
		    /* Delete existing error log. */
		    (void)os_delete_file(error_log);
		}
		goto eloop;
	    }
	    if (match(&string, "deleteperpass")) {
		dip->di_delete_per_pass = True;
		goto eloop;
	    }
	    if (match(&string, "debug")) {
		dip->di_debug_flag = debug_flag = True;
		goto eloop;
	    }
	    if (match(&string, "Debug")) {
		dip->di_Debug_flag = True;
		goto eloop;
	    }
	    if ( match(&string, "bdebug") || match(&string, "btag_debug") ) {
		dip->di_btag_debugFlag = True;
		goto eloop;
	    }
	    if ( match(&string, "edebug")  || match(&string, "eof_debug") ) {
		dip->di_eDebugFlag = True;
		goto eloop;
	    }
	    if ( match(&string, "fdebug") || match(&string, "file_debug") ) {
		dip->di_fDebugFlag = True;
		goto eloop;
	    }
	    if ( match(&string, "jdebug") || match(&string, "job_debug") ) {
		dip->di_jDebugFlag = True;
		goto eloop;
	    }
	    if ( match(&string, "ldebug") || match(&string, "lock_debug") ) {
		dip->di_lDebugFlag = True;
		goto eloop;
	    }
	    if (match(&string, "force-corruption")) {
		dip->di_force_corruption = True;
		goto eloop;
	    }
	    if (match(&string, "image")) {
		dip->di_image_copy = True;
		goto eloop;
	    }
	    if (match(&string, "iolock")) {
		dip->di_iolock = True;
		dip->di_fileperthread = False;
		dip->di_unique_pattern = False;
		dip->di_keep_existing = True;
		dip->di_dispose_mode = KEEP_FILE;
		goto eloop;
	    }
	    if ( match(&string, "mdebug") || match(&string, "memory_debug") ) {
		dip->di_mDebugFlag = mDebugFlag = True;
		goto eloop;
	    }
	    if ( match(&string, "mntdebug") || match(&string, "mount_debug") ) {
		dip->di_mntDebugFlag = True;
		goto eloop;
	    }
	    if ( match(&string, "pdebug") || match(&string, "process_debug") ) {
		dip->di_pDebugFlag = pDebugFlag = True;
		goto eloop;
	    }
	    if ( match(&string, "rdebug") || match(&string, "random_debug") ) {
		dip->di_rDebugFlag = True;
		goto eloop;
	    }
	    if ( match(&string, "sdebug") || match(&string, "scsi_debug") ) {
		dip->di_sDebugFlag = True;
		goto eloop;
	    }
	    if ( match(&string, "tdebug") || match(&string, "thread_debug") ) {
		dip->di_tDebugFlag = tDebugFlag = True;
		goto eloop;
	    }
	    if ( match(&string, "timerdebug") || match(&string, "timer_debug") ) {
		dip->di_timerDebugFlag = True;
		goto eloop;
	    }
	    if (match(&string, "diag")) {
		dip->di_logdiag_flag = True;
		goto eloop;
	    }
	    if (match(&string, "dumpall")) {
		dip->di_dumpall_flag = True;
		goto eloop;
	    }
	    if (match(&string, "dump_btags")) {
		dip->di_btag_flag = True;
		dip->di_dump_btags = True;
		goto eloop;
	    }
	    if (match(&string, "dump_context")) {
		dip->di_dump_context_flag = True;
		goto eloop;
	    }
	    if (match(&string, "dump")) {
		dip->di_dump_flag = True;
		goto eloop;
	    }
	    if (match(&string, "eof")) {
		dip->di_eof_status_flag = True;
		goto eloop;
	    }
	    if (match(&string, "errors")) {
		dip->di_errors_flag = True;
		goto eloop;
	    }
	    if (match(&string, "xerrors")) {
		dip->di_extended_errors = True;
		goto eloop;
	    }
	    if (match(&string, "fileperthread")) {
		dip->di_fileperthread = True;
		goto eloop;
	    }
	    if (match(&string, "fsincr")) {
		/* Note: limit= sets the record limit too! */
		if (dip->di_record_limit && (dip->di_record_limit != INFINITY)) {
		    Eprintf(dip, "Cannot use file size increment option with a record limit!\n");
		    return ( HandleExit(dip, FAILURE) );
		}
		dip->di_fsincr_flag = True;
		goto eloop;
	    }
	    if (match(&string, "fsync")) {
		dip->di_fsync_flag = True;
		goto eloop;
	    }
	    if (match(&string, "fsalign")) {
		dip->di_fsalign_flag = True;
		goto eloop;
	    }
	    if (match(&string, "fsmap")) {
		dip->di_fsmap_flag = True;
		goto eloop;
	    }
	    if (match(&string, "fstrim")) {
		dip->di_fstrim_flag = True;
		goto eloop;
	    }
	    if (match(&string, "funique")) {
		dip->di_unique_file = True;
		goto eloop;
	    }
	    if (match(&string, "fill_always")) {
		dip->di_fill_always = True;
		goto eloop;
	    }
	    if (match(&string, "fill_once")) {
		dip->di_fill_once = True;
		goto eloop;
	    }
	    if ( match(&string, "header") || match(&string, "log_header") ) {
		dip->di_logheader_flag = True;
		goto eloop;
	    }
	    if ( match(&string, "trailer") || match(&string, "log_trailer") ) {
		dip->di_logtrailer_flag = True;
		goto eloop;
	    }
	    if ( match(&string, "hdump") || match(&string, "history_dump") ) {
		dip->di_history_dump = True;
		goto eloop;
	    }
	    if ( match(&string, "htiming") || match(&string, "history_timing") ) {
		dip->di_history_timing = True;
		goto eloop;
	    }
	    if (match(&string, "iotuning")) {
		dip->di_iotuning_flag = True;
		goto eloop;
	    }
	    if (match(&string, "lbdata")) {
		dip->di_lbdata_flag = True;
		goto eloop;
	    }
	    if (match(&string, "logpid")) {
		dip->di_logpid_flag = True;
		goto eloop;
	    }
	    if (match(&string, "lockfiles")) {
		dip->di_lock_files = True;
		goto eloop;
	    }
	    if (match(&string, "looponerror")) {
		dip->di_loop_on_error = True;
		goto eloop;
	    }
	    if (match(&string, "microdelay")) {
		dip->di_sleep_res = SLEEP_USECS;
		goto eloop;
	    }
	    if (match(&string, "msecsdelay")) {
		dip->di_sleep_res = SLEEP_MSECS;
		goto eloop;
	    }
	    if (match(&string, "secsdelay")) {
		dip->di_sleep_res = SLEEP_SECS;
		goto eloop;
	    }
#if defined(MMAP)
	    if (match(&string, "mmap")) {
		dip->di_mmap_flag = True;
		dip->di_write_mode = O_RDWR;	/* MUST open read/write. */
		goto eloop;
	    }
#endif /* defined(MMAP) */
	    if (match(&string, "mount_lookup")) {
		dip->di_mount_lookup = True;
		goto eloop;
	    }
	    if (match(&string, "multi")) {
		dip->di_multi_flag = True;
		goto eloop;
	    }
	    if (match(&string, "pipes")) {
		PipeModeFlag = True;
		InteractiveFlag = False;
		goto eloop;
	    }
	    if (match(&string, "noprog")) {
		dip->di_noprog_flag = True;
		goto eloop;
	    }
	    if (match(&string, "poison")) {
		dip->di_poison_buffer = True;
		dip->di_prefill_buffer = True;
		goto eloop;
	    }
	    if (match(&string, "prefill")) {
		dip->di_prefill_buffer = True;
		goto eloop;
	    }
	    if ( match(&string, "jstats") || match(&string, "job_stats") ) {
		dip->di_job_stats_flag = True;
		dip->di_stats_flag = True;
		if (dip->di_stats_level == STATS_NONE) {
		    dip->di_stats_level = STATS_FULL;
		}
		goto eloop;
	    }
	    if ( match(&string, "pstats") || match(&string, "pass_stats") ) {
		dip->di_pstats_flag = True;
		dip->di_stats_flag = True;
		if (dip->di_stats_level == STATS_NONE) {
		    dip->di_stats_level = STATS_FULL;
		}
		goto eloop;
	    }
	    if ( match(&string, "tstats") || match(&string, "total_stats") ) {
		dip->di_total_stats_flag = True;
		dip->di_stats_flag = True;
		if (dip->di_stats_level == STATS_NONE) {
		    dip->di_stats_level = STATS_FULL;
		}
		goto eloop;
	    }
	    if (match(&string, "stats")) {
		dip->di_stats_flag = True;
		dip->di_pstats_flag = True;
		dip->di_job_stats_flag = True;
		dip->di_total_stats_flag = True;
		if (dip->di_stats_level == STATS_NONE) {
		    dip->di_stats_level = STATS_FULL;
		}
		goto eloop;
	    }
	    if ( match(&string, "raw") || match(&string, "read_after_write") || match(&string, "read_immed") ) {
		dip->di_raw_flag = True;	/* raw = read-after-write! */
		goto eloop;
	    }
	    if (match(&string, "reread")) {
		dip->di_reread_flag = True;
		goto eloop;
	    }
	    if (match(&string, "resfsfull")) {
		dip->di_fsfull_restart = True;
		goto eloop;
	    }
	    if (match(&string, "readcache")) {
		dip->di_read_cache_flag = True;
		goto eloop;
	    }
	    if (match(&string, "writecache")) {
		dip->di_write_cache_flag = True;
		goto eloop;
	    }
	    if (match(&string, "retryDC")) {
		dip->di_retryDC_flag = True;
		goto eloop;
	    }
	    if (match(&string, "retrydisc")) {
		dip->di_retry_disconnects = True;
		os_set_disconnect_errors(dip);
		goto eloop;
	    }
	    if (match(&string, "retrywarn")) {
		dip->di_retry_warning = True;
		goto eloop;
	    }
#if defined(SCSI)
	    if (match(&string, "fua")) {
		dip->di_fua = True;
		goto eloop;
	    }
	    if (match(&string, "dpo")) {
		dip->di_dpo = True;
		goto eloop;
	    }
	    if ( match(&string, "sense") || match(&string, "scsi_sense") ) {
		dip->di_scsi_sense = True;
		goto eloop;
	    }
	    if ( match(&string, "serrors") || match(&string, "scsi_errors") ) {
		dip->di_scsi_errors = True;
		goto eloop;
	    }
	    if ( match(&string, "srecovery") ||	match(&string, "scsi_recovery") ) {
		dip->di_scsi_recovery = True;
		goto eloop;
	    }
	    if (match(&string, "scsi_info")) {
		dip->di_scsi_info_flag = True;
		goto eloop;
	    }
	    if ( match(&string, "scsi_io") || match(&string, "scsiio") ) {
		dip->di_scsi_io_flag = True;
		goto eloop;
	    }
	    if ( match(&string, "nvme_io") || match(&string, "nvmeio") ) {
		dip->di_nvme_io_flag = True;
		goto eloop;
	    }
	    if (match(&string, "scsi")) {
		dip->di_scsi_flag = True;
		goto eloop;
	    }
	    if (match(&string, "get_lba_status")) {
		dip->di_get_lba_status_flag = True;
		goto eloop;
	    }
	    if (match(&string, "unmap")) {
		dip->di_unmap_flag = True;
		dip->di_unmap_type = UNMAP_TYPE_UNMAP;
		//dip->di_get_lba_status_flag = True;
		goto eloop;
	    }
#endif /* defined(SCSI) */
	    if ( match(&string, "savecorrupted") || match(&string, "sdc") ) {
		dip->di_save_corrupted = True;
		goto eloop;
	    }
	    if (match(&string, "scriptverify")) {
		dip->di_script_verify = True;
		goto eloop;
	    }
	    if (match(&string, "spad")) {
		dip->di_spad_check = True;
		goto eloop;
	    }
	    if (match(&string, "sighup")) {
		sighup_flag = True;
		goto eloop;
	    }
	    /* Windows specific, but parse for inclusion in workloads for all OS's. */
	    if (match(&string, "prealloc")) {
		dip->di_prealloc_flag = True;
		goto eloop;
	    }
	    if (match(&string, "sparse")) {
		dip->di_sparse_flag = True;
		goto eloop;
	    }
	    if (match(&string, "stopimmed")) {
		dip->di_stop_immediate = True;
		goto eloop;
	    }
	    if (match(&string, "syslog")) {
		dip->di_syslog_flag = True;
		goto eloop;
	    }
	    if (match(&string, "terminate_on_signals")) {
		terminate_on_signals = True;
		goto eloop;
	    }
#if defined(TIMESTAMP)
	    if ( match(&string, "timestamps") || match(&string, "timestamp") ) {
		dip->di_timestamp_flag = True;
		goto eloop;
	    }
#endif /* !defined(TIMESTAMP) */
	    if (match(&string, "trigargs")) {
		dip->di_trigargs_flag = True;
		goto eloop;
	    }
	    if (match(&string, "trigdelay")) {
		dip->di_trigdelay_flag = True;
		goto eloop;
	    }
	    if (match(&string, "trigdefaults")) {
        	dip->di_trigdefaults_flag = True;
		goto eloop;
	    }
	    if (match(&string, "unique")) {
		dip->di_unique_pattern = True;
		goto eloop;
	    }
	    if (match(&string, "uuid_dashes")) {
		dip->di_uuid_dashes = True;
		goto eloop;
	    }
	    if (match(&string, "verbose")) {
		dip->di_verbose_flag = True;
		goto eloop;
	    }
	    if (match(&string, "verify")) {
		dip->di_verify_flag = True;
		goto eloop;
	    }
	    Eprintf(dip, "Invalid enable keyword: %s\n", string);
	    return ( HandleExit(dip, FAILURE) );
	}
	if (match (&string, "disable=")) {
	dloop:
	    if (match(&string, ","))
		goto dloop;
	    if (*string == '\0')
		continue;
	    if (match(&string, "aio")) {
		dip->di_aio_flag = False;
		goto dloop;
	    }
	    if (match(&string, "async")) {
		dip->di_async_job = False;
		goto dloop;
	    }
	    if ( match(&string, "btags") ) {
		dip->di_btag_flag = False;
		goto dloop;
	    }
	    if (match(&string, "bypass")) {
		dip->di_bypass_flag = False;
		goto dloop;
	    }
	    if (match(&string, "cerrors")) {
		dip->di_cerrors_flag = False;
		goto dloop;
	    }
	    if (match(&string, "xerrors")) {
		dip->di_extended_errors = False;
		goto dloop;
	    }
	    if (match(&string, "compare")) {
		dip->di_compare_flag = False;
		goto dloop;
	    }
	    if (match(&string, "xcompare")) {
		dip->di_xcompare_flag = False;
		goto dloop;
	    }
	    if (match(&string, "coredump")) {
		dip->di_force_core_dump = False;
		goto dloop;
	    }
	    if (match(&string, "deleteerrorlog")) {
		DeleteErrorLogFlag = False;
		goto dloop;
	    }
	    if (match(&string, "deleteperpass")) {
		dip->di_delete_per_pass = False;
		goto dloop;
	    }
	    if (match(&string, "debug")) {
		dip->di_debug_flag = False;
		goto dloop;
	    }
	    if (match(&string, "Debug")) {
		dip->di_Debug_flag = False;
		goto dloop;
	    }
	    if ( match(&string, "bdebug") || match(&string, "btag_debug") ) {
		dip->di_btag_debugFlag = False;
		goto dloop;
	    }
	    if ( match(&string, "edebug") || match(&string, "eof_debug") ) {
		dip->di_eDebugFlag = False;
		goto dloop;
	    }
	    if ( match(&string, "fdebug") || match(&string, "file_debug") ) {
		dip->di_fDebugFlag = False;
		goto dloop;
	    }
	    if ( match(&string, "jdebug") || match(&string, "job_debug") ) {
		dip->di_jDebugFlag = False;
		goto dloop;
	    }
	    if ( match(&string, "ldebug") || match(&string, "lock_debug") ) {
		dip->di_lDebugFlag = False;
		goto dloop;
	    }
	    if (match(&string, "force-corruption")) {
		dip->di_force_corruption = False;
		goto dloop;
	    }
	    if (match(&string, "image")) {
		dip->di_image_copy = False;
		goto dloop;
	    }
	    if (match(&string, "iolock")) {
		dip->di_iolock = False;
		dip->di_fileperthread = True;
		dip->di_unique_pattern = True;
		goto dloop;
	    }
	    if ( match(&string, "mdebug") || match(&string, "memory_debug") ) {
		dip->di_mDebugFlag = mDebugFlag = False;
		goto dloop;
	    }
	    if ( match(&string, "mntdebug") || match(&string, "mount_debug") ) {
		dip->di_mntDebugFlag = False;
		goto dloop;
	    }
	    if ( match(&string, "pdebug") || match(&string, "process_debug") ) {
		dip->di_pDebugFlag = pDebugFlag = False;
		goto dloop;
	    }
	    if ( match(&string, "rdebug") || match(&string, "random_debug") ) {
		dip->di_rDebugFlag = False;
		goto dloop;
	    }
	    if ( match(&string, "sdebug") || match(&string, "scsi_debug") ) {
		dip->di_sDebugFlag = False;
		goto dloop;
	    }
	    if ( match(&string, "tdebug") || match(&string, "thread_debug") ) {
		dip->di_tDebugFlag = tDebugFlag = False;
		goto dloop;
	    }
	    if ( match(&string, "timerdebug") || match(&string, "timer_debug") ) {
		dip->di_timerDebugFlag = False;
		goto dloop;
	    }
	    if (match(&string, "diag")) {
		dip->di_logdiag_flag = False;
		goto dloop;
	    }
	    if (match(&string, "dumpall")) {
		dip->di_dumpall_flag = False;
		goto dloop;
	    }
	    if (match(&string, "dump_btags")) {
		dip->di_dump_btags = False;
		goto dloop;
	    }
	    if (match(&string, "dump_context")) {
		dip->di_dump_context_flag = False;
		goto dloop;
	    }
	    if (match(&string, "dump")) {
		dip->di_dump_flag = True;
		goto dloop;
	    }
	    if (match(&string, "eof")) {
		dip->di_eof_status_flag = False;
		goto dloop;
	    }
	    if (match(&string, "errors")) {
		dip->di_errors_flag = False;
		goto dloop;
	    }
	    if (match(&string, "fileperthread")) {
		dip->di_fileperthread = False;
		goto dloop;
	    }
	    if (match(&string, "fsincr")) {
		dip->di_fsincr_flag = False;
		goto dloop;
	    }
	    if (match(&string, "fsync")) {
		dip->di_fsync_flag = False;
		goto dloop;
	    }
	    if (match(&string, "fsalign")) {
		dip->di_fsalign_flag = False;
		goto dloop;
	    }
	    if (match(&string, "fsmap")) {
		dip->di_fsmap_flag = False;
		goto dloop;
	    }
	    if (match(&string, "fstrim")) {
		dip->di_fstrim_flag = False;
		goto dloop;
	    }
	    if (match(&string, "funique")) {
		dip->di_unique_file = False;
		goto dloop;
	    }
	    if (match(&string, "fill_always")) {
		dip->di_fill_always = False;
		goto dloop;
	    }
	    if (match(&string, "fill_once")) {
		dip->di_fill_once = False;
		goto dloop;
	    }
	    if ( match(&string, "header") || match(&string, "log_header") ) {
		dip->di_logheader_flag = False;
		goto dloop;
	    }
	    if ( match(&string, "trailer") || match(&string, "log_trailer") ) {
		dip->di_logtrailer_flag = False;
		goto dloop;
	    }
	    if ( match(&string, "hdump") || match(&string, "history_dump") ) {
		dip->di_history_dump = False;
		goto dloop;
	    }
	    if ( match(&string, "htiming") || match(&string, "history_timing") ) {
		dip->di_history_timing = False;
		goto dloop;
	    }
	    if (match(&string, "iotuning")) {
		dip->di_iotuning_flag = False;
		goto dloop;
	    }
	    if (match(&string, "lbdata")) {
		dip->di_lbdata_flag = False;
		dip->di_user_lbdata = False;
		goto dloop;
	    }
	    if (match(&string, "logpid")) {
		dip->di_logpid_flag = False;
		goto dloop;
	    }
	    if (match(&string, "lockfiles")) {
		dip->di_lock_files = False;
		goto dloop;
	    }
	    if (match(&string, "looponerror")) {
		dip->di_loop_on_error = False;
		goto dloop;
	    }
	    if (match(&string, "microdelay")) {
		dip->di_sleep_res = SLEEP_DEFAULT;
		goto dloop;
	    }
	    if (match(&string, "msecsdelay")) {
		dip->di_sleep_res = SLEEP_DEFAULT;
		goto dloop;
	    }
	    if (match(&string, "secsdelay")) {
		dip->di_sleep_res = SLEEP_DEFAULT;
		goto dloop;
	    }
#if defined(MMAP)
	    if (match(&string, "mmap")) {
		dip->di_mmap_flag = False;
		goto dloop;
	    }
#endif /* defined(MMAP) */
	    if (match(&string, "mount_lookup")) {
		dip->di_mount_lookup = False;
		goto dloop;
	    }
	    if (match(&string, "multi")) {
		dip->di_multi_flag = False;
		goto dloop;
	    }
	    if (match(&string, "pipes")) {
		PipeModeFlag = False;
		InteractiveFlag = True;
		goto dloop;
	    }
	    if (match(&string, "noprog")) {
		dip->di_noprog_flag = False;
		goto dloop;
	    }
	    if (match(&string, "pad")) {
		dip->di_pad_check = False;
		goto dloop;
	    }
	    if (match(&string, "poison")) {
		dip->di_poison_buffer = False;
		goto dloop;
	    }
	    if (match(&string, "prefill")) {
		dip->di_prefill_buffer = False;
		goto dloop;
	    }
	    if ( match(&string, "jstats") || match(&string, "job_stats") ) {
		dip->di_job_stats_flag = False;
		goto dloop;
	    }
	    if ( match(&string, "pstats") || match(&string, "pass_stats") ) {
		dip->di_pstats_flag = False;
		goto dloop;
	    }
	    if ( match(&string, "tstats") || match(&string, "total_stats") ) {
		dip->di_total_stats_flag = False;
		goto dloop;
	    }
	    if (match(&string, "stats")) {
		/* Disable all stat flags so they can be individually enabled! */
		dip->di_job_stats_flag = False;
		dip->di_pstats_flag = False;
		dip->di_total_stats_flag = False;
		dip->di_stats_flag = False;
		dip->di_stats_level = STATS_NONE;
		goto dloop;
	    }
 
	    if ( match(&string, "raw") || match(&string, "read_after_write") || match(&string, "read_immed") ) {
		dip->di_raw_flag = False;
		goto dloop;
	    }
	    if (match(&string, "reread")) {
		dip->di_reread_flag = False;
		goto dloop;
	    }
	    if (match(&string, "resfsfull")) {
		dip->di_fsfull_restart = False;
		goto dloop;
	    }
	    if (match(&string, "readcache")) {
		dip->di_read_cache_flag = False;
		goto dloop;
	    }
	    if (match(&string, "writecache")) {
		dip->di_write_cache_flag = False;
		goto dloop;
	    }
	    if (match(&string, "retryDC")) {
		dip->di_retryDC_flag = False;
		goto dloop;
	    }
	    if (match(&string, "retrydisc")) {
		dip->di_retry_disconnects = False;
		dip->di_retry_entries = 0;
		goto dloop;
	    }
	    if (match(&string, "retrywarn")) {
		dip->di_retry_warning = False;
		goto dloop;
	    }
	    if (match(&string, "sighup")) {
		sighup_flag = False;
		goto dloop;
	    }
#if defined(SCSI)
	    if (match(&string, "fua")) {
		dip->di_fua = False;
		goto dloop;
	    }
	    if (match(&string, "dpo")) {
		dip->di_dpo = False;
		goto dloop;
	    }
	    if ( match(&string, "sense") || match(&string, "scsi_sense") ) {
		dip->di_scsi_sense = False;
		goto dloop;
	    }
	    if ( match(&string, "serrors") || match(&string, "scsi_errors") ) {
		dip->di_scsi_errors = False;
		goto dloop;
	    }
	    if ( match(&string, "srecovery") ||	match(&string, "scsi_recovery") ) {
		dip->di_scsi_recovery = False;
		goto dloop;
	    }
	    if (match(&string, "scsi_info")) {
		dip->di_scsi_info_flag = False;
		goto dloop;
	    }
	    if ( match(&string, "scsi_io") || match(&string, "scsiio") ) {
		dip->di_scsi_io_flag = False;
		goto dloop;
	    }
	    if ( match(&string, "nvme_io") || match(&string, "nvmeio") ) {
		dip->di_nvme_io_flag = False;
		goto dloop;
	    }
	    if (match(&string, "scsi")) {
		dip->di_scsi_flag = False;
		goto dloop;
	    }
	    if (match(&string, "get_lba_status")) {
		dip->di_get_lba_status_flag = False;
		goto dloop;
	    }
	    if (match(&string, "unmap")) {
		dip->di_unmap_flag = False;
		dip->di_unmap_type = UNMAP_TYPE_NONE;
		goto dloop;
	    }
#endif /* defined(SCSI) */
	    if ( match(&string, "savecorrupted") || match(&string, "sdc") ) {
		dip->di_save_corrupted = False;
		goto dloop;
	    }
	    if (match(&string, "scriptverify")) {
		dip->di_script_verify = False;
		goto dloop;
	    }
	    if (match(&string, "spad")) {
		dip->di_spad_check = False;
		goto dloop;
	    }
//#if defined(WIN32)
	    /* Windows specific, but parse for inclusion in workloads for all OS's. */
	    if (match(&string, "prealloc")) {
		dip->di_prealloc_flag = False;
		goto dloop;
	    }
	    if (match(&string, "sparse")) {
		dip->di_sparse_flag = False;
		goto dloop;
	    }
//#endif /* defined(WIN32) */
           if (match(&string, "stopimmed")) {
		dip->di_stop_immediate = False;
		goto dloop;
	    }
	    if (match(&string, "syslog")) {
		dip->di_syslog_flag = False;
		goto dloop;
	    }
	    if (match(&string, "terminate_on_signals")) {
		terminate_on_signals = False;
		goto dloop;
	    }
	    if ( match(&string, "timestamps") || match(&string, "timestamp") ) {
		dip->di_timestamp_flag = False;
		goto dloop;
	    }
	    if (match(&string, "trigargs")) {
		dip->di_trigargs_flag = False;
		goto dloop;
	    }
	    if (match(&string, "trigdelay")) {
		dip->di_trigdelay_flag = False;
		goto dloop;
	    }
	    if (match(&string, "trigdefaults")) {
        	dip->di_trigdefaults_flag = False;
		goto dloop;
	    }
	    if (match(&string, "unique")) {
		dip->di_unique_pattern = False;
		goto dloop;
	    }
	    if (match(&string, "uuid_dashes")) {
		dip->di_uuid_dashes = False;
		goto dloop;
	    }
	    if (match(&string, "verbose")) {
		dip->di_verbose_flag = False;
		goto dloop;
	    }
	    if (match(&string, "verify")) {
		dip->di_verify_flag = False;
		goto dloop;
	    }
	    Eprintf(dip, "Invalid disable keyword: %s\n", string);
	    return ( HandleExit(dip, FAILURE) );
	}
	if (match (&string, "dispose=")) {
	    if (match(&string, "delete")) {
		dip->di_keep_existing = False;
		dip->di_dispose_mode = DELETE_FILE;
	    } else if (match(&string, "keeponerror")) { /* parse first! */
		dip->di_keep_existing = True;
		dip->di_dispose_mode = KEEP_ON_ERROR;
	    } else if (match(&string, "keep")) {
		dip->di_keep_existing = True;
		dip->di_dispose_mode = KEEP_FILE;
	    } else {
		Eprintf(dip, "Dispose modes are 'delete', 'keep', or 'keeponerror'.\n", string);
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "datesep=")) {
	    if (dip->di_date_sep) free(dip->di_date_sep);
	    dip->di_date_sep = strdup(string);
	    continue;
	}
	if (match (&string, "timesep=")) {
	    if (dip->di_time_sep) free(dip->di_time_sep);
	    dip->di_time_sep = strdup(string);
	    continue;
	}
	if (match (&string, "filesep=")) {
	    if (dip->di_file_sep) free(dip->di_file_sep);
	    dip->di_file_sep = strdup(string);
	    continue;
	}
	if (match (&string, "filepostfix=")) {
	    if (dip->di_file_postfix) free(dip->di_file_postfix);
	    dip->di_file_postfix = strdup(string);
	    continue;
	}
	if (match (&string, "dir=")) {
	    int dir_len = (int)strlen(string);
	    if (dir_len) {
		if (dip->di_dir) free(dip->di_dir);
		dip->di_dir = strdup(string);
		/* We do NOT want the trailing directory separator. */
		if (dip->di_dir[dir_len-1] == dip->di_dir_sep) {
		    dip->di_dir[dir_len-1] = '\0';
		}
	    }
	    continue;
	}
	if (match (&string, "dirp=")) {
	    if (dip->di_dirprefix) free(dip->di_dirprefix);
	    dip->di_dirprefix = strdup(string);
	    continue;
	}
#if not_implemented_yet
	if (match (&string, "dirs=")) {
	    dip->di_user_dir_limit = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
#endif /* not_implemented_yet */
	if (match (&string, "files=")) {
	    dip->di_file_limit = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "maxfiles=")) {
	    dip->di_max_files = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "sdirs=")) {
	    dip->di_user_subdir_limit = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "depth=")) {
	    dip->di_user_subdir_depth = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if ( match(&string, "if=") || match(&string, "src=") ||
	     match(&string, "dsf1=")  || match(&string, "mirror=") ) {
	    if (dip->di_input_file) free(dip->di_input_file);
	    if (strlen(string) == 0) {
		Eprintf(dip, "Please specify an input file name!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_input_file = strdup(string);
	    continue;
	}
	if ( match(&string, "of=") || match(&string, "dst=") || match(&string, "dsf=") ) {
	    if (dip->di_output_file) free(dip->di_output_file);
	    if (strlen(string) == 0) {
		Eprintf(dip, "Please specify an output file name!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_output_file = strdup(string);
	    continue;
	}
	if (match(&string, "lockmode=")) {
	    if (match(&string, "full")) {
		dip->di_lock_mode = lock_full;
		dip->di_lock_mode_name = "full";
	    } else if (match(&string, "mixed")) {
		dip->di_lock_mode = lock_mixed;
		dip->di_lock_mode_name = "mixed";
	    } else if (match(&string, "partial")) {
		dip->di_lock_mode = lock_partial;
		dip->di_lock_mode_name = "partial";
	    } else { /* TODO: Consider adding "custom" values! */
		Eprintf(dip, "The valid lock modes are: full, mixed, or partial\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_lock_files = True;
	    continue;
	}
	if (match(&string, "unlockchance=")) {
	    dip->di_unlock_chance = (int)number(dip, string, ANY_RADIX, &status, True);
	    if ( (status == SUCCESS) &&
		 ( (dip->di_unlock_chance < 0) || (dip->di_unlock_chance > 100) ) ) {
		Eprintf(dip, "Invalid value [%d] for unlock percentage, valid values are: 0-100\n",
			dip->di_unlock_chance);
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_lock_files = True;
	    continue;
	}
#if defined(SCSI)
	if (match (&string, "sdsf=")) {
	    if (dip->di_scsi_dsf) free(dip->di_scsi_dsf);
	    dip->di_scsi_dsf = strdup(string);
	    continue;
	}
	if (match (&string, "tdsf=")) {
	    if (dip->di_tscsi_dsf) free(dip->di_tscsi_dsf);
	    dip->di_tscsi_dsf = strdup(string);
	    continue;
	}
	if (match (&string, "readtype=")) {
	    if (match (&string, "read6")) {
		dip->di_scsi_read_type = scsi_read6_cdb;
	    } else if (match (&string, "read10")) {
		dip->di_scsi_read_type = scsi_read10_cdb;
	    } else if (match (&string, "read16")) {
		dip->di_scsi_read_type = scsi_read16_cdb;
	    } else {
		Eprintf(dip, "The supported SCSI read types are: read6, read10, or read16.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_scsi_io_flag = True;
	    continue;
	}
	if (match (&string, "writetype=")) {
	    if (match (&string, "write6")) {
		dip->di_scsi_write_type = scsi_write6_cdb;
	    } else if (match (&string, "write10")) {
		dip->di_scsi_write_type = scsi_read10_cdb;
	    } else if (match (&string, "write16")) {
		dip->di_scsi_write_type = scsi_write16_cdb;
	    } else if (match (&string, "writev16")) {
		dip->di_scsi_write_type = scsi_writev16_cdb;
	    } else {
		Eprintf(dip, "The supported SCSI write types are: write6, write10, write16, or writev16.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_scsi_io_flag = True;
	    continue;
	}
#endif /* defined(SCSI) */
	if (match (&string, "pass_cmd=")) {
	    if (dip->di_pass_cmd) {
		FreeStr(dip, dip->di_pass_cmd);
	    }
	    if (*string) {
		dip->di_pass_cmd = strdup(string);
	    }
	    continue;
	}
	if (match (&string, "pf=")) {
	    if (dip->di_pattern_file) {
		FreeStr(dip, dip->di_pattern_file);
		dip->di_pattern_file = NULL;
	    }
	    if (*string) {
		dip->di_pattern_file = strdup(string);
		dip->di_user_pattern = True;
	    } else {
		dip->di_user_pattern = False;
	    }
	    continue;
	}
	if ( match(&string, "jlog=") || match(&string, "job_log=") ) {
	    if (dip->di_job_log) {
		FreeStr(dip, dip->di_job_log);
		dip->di_job_log = NULL;
	    }
	    if (*string) {
		dip->di_job_log = strdup(string);
		dip->di_logheader_flag = True;
	    }
	    continue;
	}
	if (match (&string, "log=")) {
	    if (dip->di_log_file) {
		FreeStr(dip, dip->di_log_file);
		dip->di_log_file = NULL;
	    }
	    if (*string) {
		dip->di_log_file = strdup(string);
		dip->di_logheader_flag = True;
	    }
	    continue;
	}
	if (match (&string, "loga=")) {
	    if (dip->di_log_file) {
		FreeStr(dip, dip->di_log_file);
		dip->di_log_file = NULL;
	    }
	    if (*string) {
		dip->di_log_file = strdup(string);
		dip->di_logappend_flag = True;
		dip->di_logheader_flag = True;
	    }
	    continue;
	}
	if (match (&string, "logt=")) {
	    if (dip->di_log_file) {
		FreeStr(dip, dip->di_log_file);
		dip->di_log_file = NULL;
	    }
	    dip->di_log_file = strdup(string);
	    dip->di_logappend_flag = False;
	    dip->di_logheader_flag = True;
	    continue;
	}
	if (match (&string, "logu=")) {
	    if (dip->di_log_file) {
		FreeStr(dip, dip->di_log_file);
		dip->di_log_file = NULL;
	    }
	    if (*string) {
		dip->di_log_file = strdup(string);
		dip->di_unique_log = True;
		dip->di_logheader_flag = True;
	    }
	    continue;
	}
	if (match (&string, "logdir=")) {
	    if (dip->di_log_dir) {
		FreeStr(dip, dip->di_log_dir);
		dip->di_log_dir = NULL;
	    }
	    if (*string) {
		dip->di_log_dir = strdup(string);
	    }
	    continue;
	}
	if ( match(&string, "elog=") || match(&string, "error_log=") ) {
	    char logfmt[STRING_BUFFER_SIZE];
	    char *path = logfmt;
            char *logpath = string;
	    /* Handle existing error log file. */
	    if (error_log) {
		if (error_logfp) {
		    (void)CloseFile(dip, &error_logfp);
		}
		FreeStr(dip, error_log);
		error_log = NULL;
	    }
	    if (strlen(logpath) == 0) continue;
            status = setup_log_directory(dip, path, logpath);
	    if (status == FAILURE) {
		return (HandleExit(dip, status));
	    }
	    if (strstr(path, "%")) {
		path = FmtLogFile(dip, path, True);
	    } else {
		path = strdup(path);
	    }
	    /* Ok, that's it! The error file is open'ed for append upon errors. */
	    error_log = path;
	    if (DeleteErrorLogFlag == True) {
		(void)os_delete_file(error_log); /* Delete existing error log file. */
	    }
	    continue;
	}
	if ( match(&string, "mlog=") || match(&string, "master_log=") ) {
            char *log_name = string;
	    if (strlen(log_name) == 0) continue;
	    status = create_master_log(dip, log_name);
	    if (status == FAILURE) {
		return (HandleExit(dip, status));
	    }
            continue;
	}
	if ( match(&string, "reread_file=") ) {
	    char logfmt[STRING_BUFFER_SIZE];
	    char *path = logfmt;
            char *reread_name = string;
	    /* Handle existing reread file. */
	    if (reread_file) {
		FreeStr(dip, reread_file);
		reread_file = NULL;
	    }
	    if (strlen(reread_name) == 0) continue;
            status = setup_log_directory(dip, path, reread_name);
	    if (status == FAILURE) {
		return (HandleExit(dip, status));
	    }
	    if (strstr(path, "%")) {
		path = FmtLogFile(dip, path, True);
	    } else {
		path = strdup(path);
	    }
	    reread_file = path;
	    (void)os_delete_file(reread_file); /* Delete existing reread file. */
            /* Enable options to properly enable this reread file option: */
	    dip->di_logtrailer_flag = True;
	    dip->di_keep_existing = True;
	    dip->di_dispose_mode = KEEP_FILE;
	    continue;
	}
	if ( match(&string, "iob=") || match(&string, "iobehavior=") ) {
	    if ( match(&string, "dtapp") ) {
		dip->di_iobehavior = DTAPP_IO;
		dtapp_set_iobehavior_funcs(dip);
	    } else if ( match(&string, "dt") ) {
		dip->di_iobehavior = DT_IO;
		continue;
	    } else if (match (&string, "hammer")) {
		dip->di_iobehavior = HAMMER_IO;
		hammer_set_iobehavior_funcs(dip);
	    } else if (match (&string, "sio")) {
		dip->di_iobehavior = SIO_IO;
		sio_set_iobehavior_funcs(dip);
	   } else {
	       Eprintf(dip, "Valid I/O behaviors are: dti, dtapp, hammer, and sio\n");
	       return ( HandleExit(dip, FAILURE) );
	   }
	   status = (*dip->di_iobf->iob_initialize)(dip);
	   if (status == FAILURE) {
	       return (HandleExit(dip, status));
	   }
	   continue;
	}
	if (match (&string, "iodir=")) {
	    /* Note: iodir={reverse|vary} are special forms of random I/O! */
	    if (match (&string, "for")) {
		dip->di_io_dir = FORWARD;
		dip->di_random_io = False;
		dip->di_vary_iodir = False;
		dip->di_io_type = SEQUENTIAL_IO;
	    } else if (match (&string, "rev")) {
		dip->di_io_dir = REVERSE;
		dip->di_random_io = True;
		dip->di_vary_iodir = False;
		dip->di_io_type = SEQUENTIAL_IO;
	    } else if (match (&string, "var")) {
		dip->di_io_dir = REVERSE;
		dip->di_random_io = True;
		dip->di_io_type = SEQUENTIAL_IO;
		dip->di_vary_iodir = True;
	    } else {
		Eprintf(dip, "Valid I/O directions are: 'forward', 'reverse', or 'vary'.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if ( match(&string, "logprefix=") || match(&string, "log_prefix=") ) {
	    if (dip->di_log_prefix) FreeStr(dip, dip->di_log_prefix);
	    if (match(&string, "gtod")) { /* Short hand! */
		dip->di_log_prefix = strdup(DEFAULT_GTOD_LOG_PREFIX);
	    } else {
		dip->di_log_prefix = strdup(string);
	    }
	    continue;
	}
	if (match (&string, "iomode=")) {
	    if (match (&string, "copy")) {
		dip->di_dispose_mode = KEEP_FILE;
		dip->di_io_mode = COPY_MODE;
	    } else if (match (&string, "mirror")) {
		dip->di_io_mode = MIRROR_MODE;
	    } else if (match (&string, "test")) {
		dip->di_io_mode = TEST_MODE;
	    } else if (match (&string, "verify")) {
		dip->di_io_mode = VERIFY_MODE;
		dip->di_verify_only = True;
	    } else {
		Eprintf(dip, "Valid I/O modes are: 'copy', 'mirror', 'test', or verify.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "iotype=")) {
	    if (match (&string, "random")) {
		dip->di_io_type = RANDOM_IO;
		dip->di_random_io = True;
		dip->di_vary_iotype = False;
	    } else if (match (&string, "sequential")) {
		dip->di_io_type = SEQUENTIAL_IO;
		/* Note: iodir={reverse|vary} are special forms of random I/O! */
		/* This issue occurs if this is specified AFTER iodir= option. */
		if (dip->di_io_dir == FORWARD) {
		    dip->di_random_io = False;
		}
		dip->di_vary_iotype = False;
	    } else if (match (&string, "var")) {
		/* Note: Special setup is done for random I/O, so set initially! */
		dip->di_io_type = RANDOM_IO;
		dip->di_random_io = True;
		dip->di_vary_iotype = True;
	    } else {
		Eprintf(dip, "Valid I/O types are: 'random', 'sequential', or 'vary'.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "istate=")) {
	    if (match (&string, "paused")) {
		dip->di_initial_state = IS_PAUSED;
	    } else if (match (&string, "running")) {
		dip->di_initial_state = IS_RUNNING;
	    } else {
		Eprintf(dip, "Valid initial states: 'paused' or 'running'.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	/*
	 * Special options to help seed IOT pattern with multiple passes.
	 */
	if (match (&string, "iotpass=")) {
	    int iot_pass = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_iot_seed *= iot_pass;
	    dip->di_iot_pattern = True;
	    continue;
	}
	if (match (&string, "iotseed=")) {
	    dip->di_iot_seed = (u_int32)number(dip, string, HEX_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_iot_pattern = True;
	    continue;
	}
	if (match (&string, "iotune=")) {
	    dip->di_iotune_file = FmtFilePath(dip, string, True);
	    continue;
	}
	/*
	 * Flags which apply to read and write of a file.
	 *
	 * NOTE: I'm not sure all of flags applying to write only!
	 */
	if (match (&string, "flags=")) {
	floop:
	    if (match(&string, ","))
		goto floop;
	    if (*string == '\0')
		continue;
	    if (match(&string, "none")) {
		dip->di_open_flags = 0;		/* Clear all flags! */
		goto floop;
	    }
#if defined(O_EXCL)
	    if (match(&string, "excl")) {
		dip->di_open_flags |= O_EXCL;	/* Exclusive open. */
		goto floop;
	    }
#endif /* defined(O_EXCL) */
	    /* Note: These don't make sense, since dt doesn't handle non-blocking I/O! */
#if defined(O_NDELAY)
	    if (match(&string, "ndelay")) {
		dip->di_open_flags |= O_NDELAY;	/* Non-delay open. */
		goto floop;
	    }
#endif /* defined(O_NDELAY) */
#if defined(O_NONBLOCK)
	    if (match(&string, "nonblock")) {
		dip->di_open_flags |= O_NONBLOCK; /* Non-blocking open. */
		goto floop;
	    }
#endif /* defined(O_NONBLOCK) */
#if defined(O_CACHE)
	    if (match(&string, "cache")) {	/* QNX specific. */
		dip->di_open_flags |= O_CACHE;	/* Keep data in cache. */
		goto floop;
	    }
#endif /* defined(O_CACHE) */
#if defined(O_DIRECT)
	    /* Note: The O_DIRECT psuedo flag is defined for all OS's! */
	    if (match(&string, "direct")) {
		dip->di_open_flags |= O_DIRECT;	/* Direct disk access. */
		dip->di_dio_flag = True;	/* For Solaris/Windows! */
		dip->di_fsalign_flag = True;
		goto floop;
	    }
	    /* Workloads can set this flag, this allows an override! */
	    if (match(&string, "nodirect")) {
		dip->di_open_flags &= ~O_DIRECT; /* Disable direct I/O. */
		dip->di_dio_flag = False;	/* For Solaris/Windows! */
		dip->di_fsalign_flag = False;
		goto floop;
	    }
#endif /* defined(O_DIRECT) */
#if defined(O_FSYNC)
	    if (match(&string, "fsync")) {	/* File integrity. */
		dip->di_open_flags |= O_FSYNC;	/* Syncronize file I/O. */
		goto floop;
	    }
#endif /* defined(O_FSYNC) */
#if defined(O_RSYNC)
	    if (match(&string, "rsync")) {
		dip->di_open_flags |= O_RSYNC;	/* Read I/O integrity. */
		goto floop;			/* Use with O_DSYNC or O_SYNC. */
	    }
#endif /* defined(O_RSYNC) */
#if defined(O_SYNC)
	    if (match(&string, "sync")) {
		dip->di_open_flags |= O_SYNC;	/* Synchronous all data access. */
		goto floop;			/* Sync data & file attributes. */
	    }
#endif /* defined(O_SYNC) */
#if defined(O_LARGEFILE)
	    if (match(&string, "large")) {
		dip->di_open_flags |= O_LARGEFILE; /* Enable large file support. */
		goto floop;			/* Same as _FILE_OFFSET_BITS=64 */
	    }
#endif /* defined(O_LARGEFILE) */
	    Eprintf(dip, "Invalid flag '%s' specified, please use 'help' for valid flags.\n", string); 
	    return ( HandleExit(dip, FAILURE) );
	} /* End if "flags=" option. */
	if (match (&string, "nice=")) { /* Deprecated, but maintain for now! */
	    dip->di_priority = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "priority=")) {
	    dip->di_priority = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	/*
	 * Flags which apply to opening a file for writes.
	 */
	if (match (&string, "oflags=")) {
	oloop:
	    if (match(&string, ","))
		goto oloop;
	    if (*string == '\0')
		continue;
	    if (match(&string, "none")) {
		dip->di_write_flags = 0;	/* Clear all flags! */
		goto oloop;
	    }
#if defined(WIN32)
	    /* Note: This will revert to O_APPEND once cleanup is done! */
	    if (match(&string, "append")) {
		dip->di_write_mode = FILE_APPEND_DATA;
		goto oloop;
	    }
#else /* !defined(WIN32) */
# if defined(O_APPEND)
	    if (match(&string, "append")) {
		dip->di_write_flags |= O_APPEND; /* Append to file. */
		goto oloop;
	    }
# endif /* defined(O_APPEND) */
#endif /* defined(WIN32) */
#if defined(O_DEFER)
	    if (match(&string, "defer")) {
		dip->di_write_flags |= O_DEFER;	/* Defer updates. */
		goto oloop;
	    }
#endif /* defined(O_DEFER) */
#if defined(O_DSYNC)
	    if (match(&string, "dsync")) {	/* Write data integrity. */
		dip->di_write_flags |= O_DSYNC;	/* Synchronize data written. */
		goto oloop;
	    }
#endif /* defined(O_DSYNC) */
#if defined(O_SYNC)
	    if (match(&string, "sync")) {
		dip->di_write_flags |= O_SYNC;	/* Synchronous all data access. */
		goto oloop;			/* Sync data & file attributes. */
	    }
#endif /* defined(O_SYNC) */
#if defined(O_TRUNC)
	    if (match(&string, "trunc")) {
		dip->di_write_flags |= O_TRUNC;	/* Truncate output file. */
		goto floop;
	    }
#endif /* defined(O_TRUNC) */
#if defined(O_TEMP)
	    if (match(&string, "temp")) {
		dip->di_write_flags |= O_TEMP;	/* Temporary file. */
		goto oloop;
	    }
#endif /* defined(O_TEMP) */
	    Eprintf(dip, "Invalid output flag '%s' specified, please use 'help' for valid flags.\n", string); 
	    return ( HandleExit(dip, FAILURE) );
	} /* End of "oflags=" option. */
	if ( match(&string, "oncerr=") || match(&string, "onerr=") ) {
	    if ( match(&string, "abort") || match(&string, "stop") ) {
		dip->di_oncerr_action = ONERR_ABORT;
	    } else if (match(&string, "continue")) {
		dip->di_oncerr_action = ONERR_CONTINUE;
	    } else if (match(&string, "pause")) {
		dip->di_oncerr_action = ONERR_PAUSE;
	    } else {
		Eprintf(dip, "The valid error actions are 'abort/stop', 'continue', or 'pause'.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "passes=")) {
	    dip->di_pass_limit = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if ( match(&string, "fpattern") || match(&string, "fill_pattern=") ) {
	    dip->di_fill_pattern = (uint32_t)number(dip, string, HEX_RADIX, &status, True);
	    dip->di_user_fpattern = True;
	    continue;
	}
	if ( match(&string, "ppattern") || match(&string, "prefill_pattern=") ) {
	    dip->di_prefill_pattern = (uint32_t)number(dip, string, HEX_RADIX, &status, True);
	    dip->di_prefill_buffer = True;
	    continue;
	}
	if (match (&string, "pattern=")) {	/* TODO: This is overloaded! */
	    int size;
	    if (*string == '\0') {
		dip->di_iot_pattern = False;
		if (dip->di_pattern_file == NULL) {
		    dip->di_user_pattern = False;
		}
		if (dip->di_pattern_string) {
		    FreeStr(dip, dip->di_pattern_string);
		    dip->di_pattern_string = NULL;
		    dip->di_pattern_strsize = 0;
		}
		if (dip->di_pattern_buffer) {
		    reset_pattern(dip);
		    dip->di_pattern = DEFAULT_PATTERN;
		}
		continue;
	    }
	    size = (int)strlen(string);
	    if (size == 0) {
		Eprintf(dip, "Please specify pattern of: { hex-pattern | incr | iot | string }\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_iot_pattern = False;
	    dip->di_user_pattern = True;
	    if (match (&string, "incr")) {	/* Incrementing pattern. */
		int v, size = 256;
		uint8_t *buffer = malloc_palign(dip, size, 0);
		uint8_t *bp = buffer;
		for (v = 0; v < size; v++) {
		    *bp++ = v;
		}
		dip->di_incr_pattern = True;
		setup_pattern(dip, buffer, size, True);
	    } else if ( (size == 3) && 
			(match(&string, "iot") || match(&string, "IOT")) ) {
		dip->di_iot_pattern = True;
		if (dip->di_data_format == NONE_FMT) {
		    dip->di_data_format = WORD_FMT;
		}
		/* Allocate pattern buffer after parsing. */
	    } else if ( IS_HexString(string) && (size <= 10) ) {
		/* valid strings: XXXXXXXX or 0xXXXXXXXX */
		dip->di_pattern = (u_int32)number(dip, string, HEX_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
	    } else { /* Presume ASCII string for data pattern. */
		uint8_t *buffer = malloc_palign(dip, size, 0);
		size = StrCopy (buffer, string, size);
		dip->di_pattern_string = strdup(string);
		dip->di_pattern_strsize = size;
		setup_pattern(dip, buffer, size, True);
	    }
	    if ( dip->di_iot_pattern == False ) dip->di_unique_pattern = False;
	    continue;
	}
	if (match (&string, "prefix=")) {
	    if (dip->di_prefix_string) { /* Free previous prefix (if any). */
		FreeStr(dip, dip->di_prefix_string);
		dip->di_prefix_string = NULL;
	    }
	    dip->di_prefix_size = (int)strlen(string);
	    if (*string == '\0') continue;
	    if (dip->di_prefix_size == 0) {
		Eprintf(dip, "Please specify a non-empty prefix string!\n");
		return ( HandleExit(dip, FAILURE) );
	    } else if (dip->di_prefix_size > BLOCK_SIZE) {
		Eprintf(dip, "Please specify a prefix string < %d bytes!\n", dip->di_prefix_size);
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_prefix_string = Malloc(dip, ++dip->di_prefix_size); /* plus NULL! */
	    (void)strcpy(dip->di_prefix_string, string);
	    continue;
	}
	if ( match(&string, "position=") || match(&string, "offset=") ) {
	    dip->di_file_position = (Offset_t)large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_user_position = True;
	    continue;
	}
	if ( match(&string, "soffset=") ) {
	    dip->di_slice_offset = (Offset_t)large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	/* Note: For copy/mirror/verify, allow an alternate output offset! */
	/* Note: This is Mickey Mouse, until full multi-device support is added. */
	if (match (&string, "oposition=") || match(&string, "ooffset=") ) {
	    dip->di_ofile_position = (Offset_t)large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_user_oposition = True;
	    continue;
	}
	if (match (&string, "procs=")) {
#if 1
	    /* Processes become threads! */
	    dip->di_threads = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_unique_file = True;	/* backwards compatibility, unique files! */
	    /* Add PID for backwards compatibility. */
	    if (dip->di_file_postfix) {
		if ( strstr(dip->di_file_postfix, "%pid") == NULL ) {
		    char postfix[STRING_BUFFER_SIZE];
		    (void)sprintf(postfix, "%s%s", dip->di_file_postfix, "p%pid");
		    FreeStr(dip, dip->di_file_postfix);
		    dip->di_file_postfix = strdup(postfix);
		}
	    } else {
		dip->di_file_postfix = strdup("%pid");
	    }
#else
	    dip->di_num_procs = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if (dip->di_num_procs > MAX_PROCS) {
		Eprintf(dip, "Please limit procs to <= %d!\n", MAX_PROCS);
		return ( HandleExit(dip, FAILURE) );
	    }
#endif
	    continue;
	}
#if defined(HP_UX)
	if (match (&string, "qdepth=")) {
	    dip->di_qdepth = (u_int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if (dip->di_qdepth > SCSI_MAX_Q_DEPTH) {
		Eprintf(dip, "Please specify a SCSI queue depth <= %d!\n", SCSI_MAX_Q_DEPTH);
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
#endif /* defined(HP_UX) */
	if (match(&string, "readp=")) {
	    if (match(&string, "random")) {
		dip->di_read_percentage = -1;
		continue;
	    }
	    dip->di_read_percentage = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (dip->di_read_percentage > 100) {
		Eprintf(dip, "The read percentage must be in the range of 0-100!\n");
		status = FAILURE;
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match(&string, "randp=")) {
	    dip->di_random_percentage = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (dip->di_random_percentage > 100) {
		Eprintf(dip, "The random percentage must be in the range of 0-100!\n");
		status = FAILURE;
		return ( HandleExit(dip, status) );
	    }
	    if (dip->di_random_percentage) {
		dip->di_random_io = True;
	    }
	    continue;
	}
	if (match(&string, "rrandp=")) {
	    dip->di_random_rpercentage = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (dip->di_random_rpercentage > 100) {
		Eprintf(dip, "The random read percentage must be in the range of 0-100!\n");
		status = FAILURE;
		return ( HandleExit(dip, status) );
	    }
	    if (dip->di_random_percentage) {
		dip->di_random_io = True;
	    }
	    continue;
	}
	if (match(&string, "wrandp=")) {
	    dip->di_random_wpercentage = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (dip->di_random_wpercentage > 100) {
		Eprintf(dip, "The random write percentage must be in the range of 0-100!\n");
		status = FAILURE;
		return ( HandleExit(dip, status) );
	    }
	    if (dip->di_random_percentage) {
		dip->di_random_io = True;
	    }
	    continue;
	}
	if (match (&string, "rseed=")) {
	    dip->di_random_seed = large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    dip->di_user_rseed = True;
	    continue;
	}
	if ( match(&string, "runtime=") ||
	     match(&string, "-runtime=") ||
	     match(&string, "--runtime=") ) {
	    dip->di_runtime = time_value(dip, string);
	    continue;
	}
	if (match (&string, "script=")) {
	    int status;
	    status = OpenScriptFile(dip, string);
	    if (status == SUCCESS) {
		continue;
	    } else {
		return ( HandleExit(dip, FAILURE) );
	    }
	}
	if (match (&string, "seek=")) {
	    dip->di_seek_count = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "skip=")) {
	    dip->di_skip_count = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "slice=")) {
	    dip->di_slice_number = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "sleep=")) {
	    dip->di_sleep_value = (uint32_t)time_value(dip, string);
	    continue;
	}
	if (match (&string, "msleep=")) {
	    dip->di_msleep_value = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "usleep=")) {
	    dip->di_usleep_value = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "slices=")) {
	    dip->di_slices = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "step=")) {
	    dip->di_step_offset = (Offset_t)large_number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "stats=")) {
	    if (match (&string, "brief")) {
		dip->di_stats_level = STATS_BRIEF;
	    } else if (match (&string, "full")) {
		dip->di_stats_level = STATS_FULL;
	    } else if (match (&string, "none")) {
		dip->di_pstats_flag = dip->di_stats_flag = dip->di_job_stats_flag = False;
		dip->di_stats_level = STATS_NONE;
		dip->di_verbose_flag = False; /* To overcome compatability check in report_pass()! */
	    } else {
		Eprintf(dip, "Valid stat levels are: 'brief', 'full', or 'none'\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if ( match(&string, "threads=") ||
	     match(&string, "-threads=") ||
	     match(&string, "--threads=") ) {
	    dip->di_threads = (int)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
#if defined(__unix)
	    if ( (max_open_files > 0) && (dip->di_threads > max_open_files) ) {
		Printf(dip, "The thread count %d, exceeds the max allowable open files %d!\n",
		       dip->di_threads, max_open_files);
		return ( HandleExit(dip, FAILURE) );
	    }
#endif /* defined(__unix) */
	    continue;
	}
	if (match (&string, "tools=")) {
	    if (tools_directory) {
        	FreeStr(dip, tools_directory);
	    }
            tools_directory = strdup(string);
            continue;
	}
	if (match(&string, "dtype=")) {
	    struct dtype *dtp;
	    if ((dtp = setup_device_type (string)) == NULL) {
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_input_dtype = dip->di_output_dtype = dtp;
	    continue;
	}
	if (match (&string, "idtype=")) {
	    if ((dip->di_input_dtype = setup_device_type (string)) == NULL) {
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "odtype=")) {
	    if ((dip->di_output_dtype = setup_device_type (string)) == NULL) {
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "fstype=")) {
	    dip->di_filesystem_type = strdup(string);
	    continue;
	}
#if defined(SCSI)
	if (match (&string, "idt=")) {
	    if (match(&string, "both")) {
		dip->di_idt = IDT_BOTHIDS;
	    } else if ( match(&string, "device") || match(&string, "did") ) {
		  dip->di_idt = IDT_DEVICEID;
	    } else if (match(&string, "serial")) {
		dip->di_idt = IDT_SERIALID;
	    } else {
		Eprintf(dip, "Invalid Inquiry device type: %s\n", string);
		LogMsg(dip, dip->di_efp, logLevelLog, 0,
		       "Valid types are: both, device, or serial\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	/* Note: These match spt's exceot for "scsi_" prefix. */
        if (match (&string, "scsi_recovery_delay=")) {
            dip->di_scsi_recovery_delay = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) return ( HandleExit(dip, status) );
            continue;
        }
        if ( match(&string, "scsi_recovery_retries=") || match(&string, "scsi_recovery_limit=") ) {
            dip->di_scsi_recovery_limit = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) return ( HandleExit(dip, status) );
            continue;
        }
	if (match (&string, "scsi_timeout=")) {
	    dip->di_scsi_timeout = (u_short)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) return ( HandleExit(dip, status) );
	    continue;
	}
	if (match (&string, "spt_path=")) {
	    dip->di_spt_path = strdup(string);
	    continue;
	}
	if (match (&string, "spt_options=")) {
	    dip->di_spt_options = strdup(string);
	    continue;
	}
	if (match (&string, "unmap_freq=")) {
	    dip->di_unmap_frequency = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) return ( HandleExit(dip, status) );
	    dip->di_unmap_flag = True;
	    if (dip->di_unmap_type == UNMAP_TYPE_NONE) {
		dip->di_unmap_type = UNMAP_TYPE_UNMAP;
	    }
	    continue;
	}
#endif /* defined(SCSI) */
	if (match (&string, "stopon=")) {
	    if (dip->di_stop_on_file) {
		FreeStr(dip, dip->di_stop_on_file);
	    }
	    dip->di_stop_on_file = strdup(string);
	    if (dip->di_stop_on_file) {
		/* Delete existing stopon file. */
		(void)os_delete_file(dip->di_stop_on_file);
	    }
	    continue;
	}
	if ( match(&string, "trigger=") || match(&string, "--trigger=") ) {
	    trigger_data_t *tdp = &dip->di_triggers[dip->di_num_triggers];
            /* No trigger specified, so cleanup existing! */
	    if (*string == '\0') {
		remove_triggers(dip);
        	continue;
	    }
	    status = add_trigger_type(dip, string);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
            /* User specified triggers overrides trigger defaults. */
	    dip->di_trigdefaults_flag = False;
	    continue;
	}
	if (match (&string, "trigger_action=")) {
	    dip->di_trigger_action = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "trigger_on=")) {
	    if ((dip->di_trigger_control = parse_trigger_control(dip, string)) == TRIGGER_ON_INVALID) {
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if ( match(&string, "vflags=") || match(&string, "verifyFlags=") ) {
	    if (*string == '\0') {
		show_btag_verify_flags(dip);
		return ( HandleExit(dip, WARNING) );
	    }
	    status = parse_btag_verify_flags(dip, string);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "vrecords=")) {
	    dip->di_volume_records = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
	if (match (&string, "volumes=")) {
	    dip->di_multi_flag = True;
	    dip->di_volumes_flag = True;
	    dip->di_volume_limit = number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    continue;
	}
#if defined(SCSI)
	if (match (&string, "unmap=")) {
	    if (match(&string, "unmap")) {
		dip->di_unmap_type = UNMAP_TYPE_UNMAP;
	    } else if (match(&string, "write_same")) {
		dip->di_unmap_type = UNMAP_TYPE_WRITE_SAME;
	    } else if (match(&string, "zerorod")) {
		dip->di_unmap_type = UNMAP_TYPE_ZEROROD;
	    } else if (match(&string, "random")) {
		dip->di_unmap_type = UNMAP_TYPE_RANDOM;
	    } else {
		Eprintf(dip, "Valid unmap types are: unmap, write_same, and random.\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    dip->di_unmap_flag = True;
	    dip->di_get_lba_status_flag = True;
	    continue;
	}
#endif /* defined(SCSI) */
	if ( match (&string, "exit") || match (&string, "quit") ) {
	    ExitFlag = True;
	    continue;
	}
	if (match (&string, "help")) {
	    dthelp(dip);
	    return ( HandleExit(dip, SUCCESS) );
	}
	/*
	 * Implement a few useful commands Scu supports. 
	 */
	if (match (&string, "eval")) {
	    char *expr = concatenate_args(dip, argc, argv, ++i);
	    if (expr) {
		large_t value;
		value = large_number(dip, expr, ANY_RADIX, &status, True);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
		show_expression(dip, value);
		FreeStr(dip, expr);
	    }
	    return ( HandleExit(dip, SUCCESS) );
	}
	if ( match(&string, "system") || match(&string, "shell") ) {
	    char *cmd = concatenate_args(dip, argc, argv, ++i);
	    if (cmd) {
		status = DoSystemCommand(dip, cmd);
		FreeStr(dip, cmd);
	    } else {
		status = StartupShell(dip, NULL);
	    }
	    /* Note: We don't terminate on failed external commands! */
	    return ( HandleExit(dip, SUCCESS) );
	}
	if (match (&string, "!")) {
	    char *cmd = concatenate_args(dip, argc, argv, i);
	    if (cmd) {
		status = DoSystemCommand(dip, (cmd + 1));
		FreeStr(dip, cmd);
	    }
	    /* Note: We don't terminate on failed external commands! */
	    return ( HandleExit(dip, SUCCESS) );
	}
	/*
	 * Job Control Options:
	 */ 
	if (match (&string, "cancelall")) {
	    job_id_t job_id = 0;
	    char *job_tag = NULL;
	    status = cancel_jobs(dip, job_id, job_tag);
	    return ( HandleExit(dip, status) );
	}
	if (match (&string, "cancel")) {
	    job_id_t job_id = 0;
	    char *job_tag = NULL;

	    if (*string != '\0') {
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    } else if (++i < argc) {
		string = argv[i++];
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    }
	    if ( (job_id == 0) && (job_tag == NULL) ) {
		Eprintf(dip, "Please specify a job ID or tag!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    status = cancel_jobs(dip, job_id, job_tag);
	    return ( HandleExit(dip, status) );
	}
	if (match (&string, "jobs")) {
	    job_id_t job_id = 0;
	    char *job_tag = NULL;
	    hbool_t verbose = False;

	    if (*string == ':') {
		string++;
		if (match (&string, "full")) {
		    verbose = True;
		}
	    }
	    if (*string != '\0') {
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    } else if (++i < argc) {
		string = argv[i++];
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
		if (status == WARNING) --i;
	    }
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    status = show_jobs(dip, job_id, job_tag, verbose);
	    return ( HandleExit(dip, SUCCESS) );
	}
	if (match (&string, "modify")) {
	    job_id_t job_id = 0;
	    char *job_tag = NULL;
	    char *modify_string = NULL;

	    if (*string != '\0') {
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    } else if (++i < argc) {
		string = argv[i++];
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
		if (status == WARNING) --i;
	    }
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    /* Anything else specified is the modify string! */
	    if ( i < argc) {
		char *bp;
		bp = modify_string = Malloc(dip, KBYTE_SIZE);
		while (i < argc) {
		    string = argv[i++];
		    bp += sprintf(bp, "%s ", string);
		}
		bp--;
		if (*bp == ' ') *bp = '\0';
	    }
	    if (modify_string == NULL) {
		Printf(dip, "Please specify parameters to modify!\n");
		status = FAILURE;
	    } else {
		status = modify_jobs(dip, job_id, job_tag, modify_string);
		FreeStr(dip, modify_string);
	    }
	    return ( HandleExit(dip, status) );
	}
	if (match (&string, "pause")) {
	    job_id_t job_id = 0;
	    char *job_tag = NULL;

	    if (*string != '\0') {
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    } else if (++i < argc) {
		string = argv[i++];
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    }
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    status = pause_jobs(dip, job_id, job_tag);
	    return ( HandleExit(dip, status) );
	}
	if (match (&string, "query")) {
	    job_id_t job_id = 0;
	    char *job_tag = NULL;
	    char *query_string = NULL;
	    
	    if (*string != '\0') {
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    } else if (++i < argc) {
		string = argv[i++];
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
		if (status == WARNING) --i;
	    }
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    /* Anything else specified is the query string! */
	    if ( i < argc) {
		char *bp;
		bp = query_string = Malloc(dip, KBYTE_SIZE);
		while (i < argc) {
		    string = argv[i++];
		    bp += sprintf(bp, "%s ", string);
		}
	    }
	    status = query_jobs(dip, job_id, job_tag, query_string);
	    if (query_string) FreeStr(dip, query_string);
	    return ( HandleExit(dip, status) );
	}
	if (match (&string, "resume")) {
	    job_id_t job_id = 0;
	    char *job_tag = NULL;

	    if (*string != '\0') {
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    } else if (++i < argc) {
		string = argv[i++];
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    }
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    status = resume_jobs(dip, job_id, job_tag);
	    return ( HandleExit(dip, status) );
	}
	if (match (&string, "stopall")) {
	    job_id_t job_id = 0;
	    char *job_tag = NULL;
	    status = stop_jobs(dip, job_id, job_tag);
	    return ( HandleExit(dip, status) );
	}
	if (match (&string, "stop")) {
	    job_id_t job_id = 0;
	    char *job_tag = NULL;

	    if (*string != '\0') {
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    } else if (++i < argc) {
		string = argv[i++];
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    }
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    if ( (job_id == 0) && (job_tag == NULL) ) {
		Eprintf(dip, "Please specify a job ID or tag!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    status = stop_jobs(dip, job_id, job_tag);
	    return ( HandleExit(dip, status) );
	}
	if (match (&string, "tag=")) {
	    if (dip->di_job_tag) {
		free(dip->di_job_tag);
	    }
	    dip->di_job_tag = strdup(string);
	    continue;
	}
	if (match (&string, "wait")) {
	    job_id_t job_id = 0;
	    char *job_tag = NULL;
	    
	    status = SUCCESS;
	    if (*string != '\0') {
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    } else if (++i < argc) {
		string = argv[i++];
		status = parse_job_args(dip, string, &job_id, &job_tag, True);
	    }
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    status = wait_for_jobs(dip, job_id, job_tag);
	    if (status == FAILURE) exit_status = status;
	    return ( HandleExit(dip, status) );
	}
	/* End of jobs options. */
	/* Start of workload options. */
	if (match (&string, "define")) {
	    dinfo_t *cdip;
	    char *workload_name;
	    char *workload_desc = NULL;
	    char *workload_options;
	    /* Format is: define workload options...*/
	    i++; argv++;	/* skip "define" */
	    if (i < argc) {
		char *p;
		workload_entry_t *workload_entry;
		workload_name = *argv;
		/* Poor parser, allow description after the workload name! */
		if (p = strrchr(workload_name, ':')) {
		    *p++ = '\0';
		    workload_desc = p;
		}
		workload_entry = find_workload(workload_name);
		if (workload_entry) {
		    Printf(dip, "Workload '%s' already exists!\n", workload_name);
		    return ( HandleExit(dip, WARNING) );
		}
	    } else {
		Eprintf(dip, "Please specify the workload name!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    i++; argv++;	/* skip workload name */
	    if (i == argc) {
		Eprintf(dip, "Please specify the workload options!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
	    /* Remaining argments are the workload options! */
	    workload_options = make_options_string(dip, (argc - i), argv, False);
	    if (!workload_options) {
		return ( HandleExit(dip, WARNING) );
	    }
	    /*
	     * Parse the options to ensure they are valid!
	     */ 
	    cdip = clone_device(dip, True, False);
	    status = ParseWorkload(cdip, workload_options);
	    cleanup_device(cdip, False);
	    FreeMem(dip, cdip, sizeof(*cdip));
	    if (status == FAILURE) {
		FreeStr(dip, workload_options);
		return ( HandleExit(dip, status) );
	    }
	    add_workload_entry(workload_name, workload_desc, workload_options);
	    FreeStr(dip, workload_options);
	    return ( HandleExit(dip, SUCCESS) );
	}
	if (match (&string, "showbtag")) {
	    /* The user *must* specify the data range. */
	    dip->di_btag_flag = True;
	    dip->di_dump_btags = True;
	    dip->di_dump_limit = sizeof(btag_t);
	    /* Disable all stats. */
	    dip->di_job_stats_flag = False;
	    dip->di_pstats_flag = False;
	    dip->di_total_stats_flag = False;
	    dip->di_stats_flag = False;
	    dip->di_stats_level = STATS_NONE;
	    /* Disable SCSI information too. */
	    dip->di_scsi_flag = False;
            continue;
	}
	if (match (&string, "showfslba")) {
	    dip->di_fsmap_type = FSMAP_TYPE_LBA_RANGE;
            dip->di_data_limit = dip->di_block_size;
            continue;
	}
	if (match (&string, "showfsmap")) {
	    dip->di_fsmap_type = FSMAP_TYPE_MAP_EXTENTS;
            continue;
	}
	if (match (&string, "showtime=")) {
	    char time_buffer[TIME_BUFFER_SIZE];
	    time_t time_value = (time_t)number(dip, string, ANY_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    Printf(dip, "The time is: " TMF " seconds => %s\n", time_value,
		   os_ctime(&time_value, time_buffer, sizeof(time_buffer)));
	    return ( HandleExit(dip, SUCCESS) );
	}
	if (match (&string, "showvflags=")) {
	    uint32_t verify_flags = (uint32_t)number(dip, string, HEX_RADIX, &status, True);
	    if (status == FAILURE) {
		return ( HandleExit(dip, status) );
	    }
	    show_btag_verify_flags_set(dip, verify_flags);
	    return ( HandleExit(dip, SUCCESS) );
	}
	if ( match(&string, "workload=") || match(&string, "workloads=") ||
	     match(&string, "--workload=") || match(&string, "--workloads=") ) {
	    workload_entry_t *workload;
            char *token, *saveptr, *workloads;
	    if (*string == '\0') {
		Eprintf(dip, "Please specify the workload name(s)!\n");
		return ( HandleExit(dip, FAILURE) );
	    }
            workloads = strdup(string);
	    token = strtok_r(workloads, ",", &saveptr);
	    while (token) {
		workload = find_workload(token);
		if (workload == NULL) {
		    Eprintf(dip, "Did not find workload '%s'!\n", token);
		    return ( HandleExit(dip, FAILURE) );
		} else if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
		status = ParseWorkload(dip, workload->workload_options);
		if (status == FAILURE) {
		    return ( HandleExit(dip, status) );
		}
		/* Use the first workload name, since workloads can be nested. */
		if (dip->di_workload_name == NULL) {
		    dip->di_workload_name = strdup(token);
		}
		token = strtok_r(NULL, ",", &saveptr); /* Next workload please! */
	    }
            Free(dip, workloads);
	    continue;	/* Parse more options to override workload! */
	}
        /* Note: This parsing *must* stay after the workload= parsing! */
	if ( match(&string, "workload") || match(&string, "workloads") ) {
	    char *workload_name = NULL;
	    if (++i < argc) {
		workload_name = argv[i];
	    }
	    show_workloads(dip, workload_name);
	    return ( HandleExit(dip, SUCCESS) );
	}
	/* End of workload options. */
	if (match (&string, "usage")) {
	    dtusage(dip);
	    return ( HandleExit(dip, SUCCESS) );
	}
	if (match (&string, "version")) {
	    dtversion(dip);
	    return ( HandleExit(dip, SUCCESS) );
	}
	/* A simple way to set some environment variables for scripts! */
	if ( match(&string, "$") && (p = strstr(string, "=")) ) {
	    *p++ = '\0';
	    // int setenv(const char *envname, const char *envval, int overwrite);
	    if (*p && setenv(string, p, True)) {
		Perror(dip, "setenv() of envname=%s, envvar=%s failed!", string, p);
		return ( HandleExit(dip, FAILURE) );
	    }
	    continue;
	}
	if (dip->script_level) {
	    int level = (dip->script_level - 1);
	    LogMsg(dip, dip->di_efp, logLevelError, 0,
		   "Parsing error in script '%s' at line number %d\n",
		    dip->script_name[level], dip->script_lineno[level]);
	}
	Eprintf(dip, "Invalid option '%s' specified, please use 'help' for valid options.\n", string);
	return ( HandleExit(dip, FAILURE) );
    }
#if !defined(AIO)
    if (dip->di_aio_flag) {
	Wprintf(dip, "POSIX AIO is NOT supported on this platform, disabling AIO!\n");
	dip->di_aio_flag = False;
    }
#endif /* !defined(AIO) */
    return (SUCCESS);
}

/*
 * Convert options array into the command string.
 */
char *
make_options_string(dinfo_t *dip, int argc, char **argv, hbool_t quoting)
{
    int arg;
    char *bp;
    char *buffer, *options;
    
    bp = buffer = Malloc(dip, LOG_BUFSIZE);
    if (bp == NULL) return(bp);

    for (arg = 0; arg < argc; arg++) {
	char *opt = argv[arg];
	char *space = strchr(opt, ' ');
	/* Embedded spaces require quoting. */
	if (space) {
	    char *dquote = strchr(opt, '"');
	    char *equals = strchr(opt, '=');
	    char quote = (dquote) ? '\'' : '"';
	    char *p = opt;
	    /* Add quoting after the option= */
	    if (equals) {
		/* Copy to and including equals sign. */
		do {
		    *bp++ = *p++;
		} while (*(p-1) != '=');
	    }
	    /* TODO: Smarter handling of quotes! */
	    bp += sprintf(bp, "%c%s%c ", quote, p, quote);
	} else {
	    bp += sprintf(bp, "%s ", opt);
	}
    }
    if (bp > buffer) {
	bp--;
	if (*bp == ' ') *bp = '\0';
    }
    options = Malloc(dip, strlen(buffer) + 1);
    strcpy(options, buffer);
    FreeStr(dip, buffer);
    return(options);
}

/*
 * Common parsing for job arguments.
 * 	job=value
 * 	tag=string
 * 	={tag|jid} - old format, deprecated!
 */ 
int
parse_job_args(dinfo_t *dip, char *string, job_id_t *job_id, char **job_tag, hbool_t errors)
{
    int status = SUCCESS;

    if (match (&string, "job=")) {
	*job_id = number(dip, string, ANY_RADIX, &status, errors);
	if (status == FAILURE) return ( HandleExit(dip, status) );
    } else if (match (&string, "tag=")) {
	*job_tag = string;
    } else if (*string == '=') {
	/* Old Format was cmd={job_id|job_tag} yea ugly, should go! */
	string++;
	if ( isalpha(*string) ) {
	    *job_tag = string;
	} else {
	    *job_id = (int)number(dip, string, ANY_RADIX, &status, errors);
	    if (status == FAILURE) return ( HandleExit(dip, status) );
	}
    } else {
	Eprintf(dip, "Unknown job argument '%s'!\n", string);
	status = FAILURE;
    }
    return(status);
}

int
parse_connection_args(dinfo_t *dip, char **string, char **host, uint32_t *port, hbool_t errors)
{
    int status = SUCCESS;

    /* Format: keyword[=host[,port]] Note: Can't use ':' with IPv6 address! */
    *host = NULL;
    *port = 0;
    if (**string == '\0') return(status);
    if (match (string, "=")) {
	char *comma = strchr(*string, ',');
	if (comma) {
	    *comma++ = '\0';
	    *port = number(dip, comma, ANY_RADIX, &status, errors);
	}
	*host = *string;
    } else {
	Eprintf(dip, "Unknown connection argument '%s'\n", *string);
	status = FAILURE;
    }
    return(status);
}
    
/*
 * match() - Match a Substring within a String.
 *
 * Inputs:
 *	sptr = Pointer to string pointer.
 *	s = The substring to match.
 *
 * Outputs:
 *	sptr = Points past substring (on match).
 *
 * Return Value:
 *	Returns True/False = Match / Not Matched
 */
hbool_t
match (char **sptr, char *s)
{
    char *cs;

    cs = *sptr;
    while (*cs++ == *s) {
	if (*s++ == '\0') {
	    goto done;
	}
    }
    if (*s != '\0') {
	return(False);
    }
done:
    cs--;
    *sptr = cs;
    return(True);
}

char *
concatenate_args(dinfo_t *dip, int argc, char **argv, int arg_index)
{
    char *buffer, *bp, *string;
    if (arg_index >= argc) return(NULL);
    buffer = bp = Malloc(dip, KBYTE_SIZE);
    if (buffer == NULL) return(NULL);
    while (arg_index < argc) {
	string = argv[arg_index++];
	bp += sprintf(bp, "%s ", string);
    }
    if ( strlen(buffer) ) bp--;
    if (*bp == ' ') *bp = '\0';
    return(buffer);
}

void
show_expression(dinfo_t *dip, large_t value)
{
    double blocks, kbytes, mbytes, gbytes, tbytes;
    char blocks_buf[32], kbyte_buf[32], mbyte_buf[32], gbyte_buf[32], tbyte_buf[32];

    blocks = ( (double)value / (double)BLOCK_SIZE);
    kbytes = ( (double)value / (double)KBYTE_SIZE);
    mbytes = ( (double)value / (double)MBYTE_SIZE);
    gbytes = ( (double)value / (double)GBYTE_SIZE);
    tbytes = ( (double)value / (double)TBYTE_SIZE);

    (void)sprintf(blocks_buf, "%f", blocks);
    (void)sprintf(kbyte_buf, "%f", kbytes);
    (void)sprintf(mbyte_buf, "%f", mbytes);
    (void)sprintf(gbyte_buf, "%f", gbytes);
    (void)sprintf(tbyte_buf, "%f", tbytes);

    if (dip->di_verbose_flag) {
	Print(dip, "Expression Values:\n");
	Print(dip, "            Decimal: " LUF " \n", value);
	Print(dip, "        Hexadecimal: " LXF " \n", value);
	Print(dip, "    512 byte Blocks: %s\n", blocks_buf);
	Print(dip, "          Kilobytes: %s\n", kbyte_buf);
	Print(dip, "          Megabytes: %s\n", mbyte_buf);
	Print(dip, "          Gigabytes: %s\n", gbyte_buf);
	Print(dip, "          Terabytes: %s\n", tbyte_buf);
    } else {
	Print(dip, "Dec: " LUF " Hex: " LXF " Blks: %s Kb: %s Mb: %s Gb: %s, Tb: %s\n",
	      value, value, blocks_buf, kbyte_buf, mbyte_buf, gbyte_buf, tbyte_buf);
    }
    return;
}

/*
 * number() - Converts ASCII string into numeric value.
 *
 * Inputs:
 *	str = The string to convert.
 * 	base = The base for numeric conversions.
 * 	status = Pointer to return parse status.
 * 	report_error = Flag to control errors.
 * 
 * Outputs:
 * 	status = SUCCESS or FAILURE
 *
 * Return Value:
 *      Returns converted number.
 */
unsigned long
number(dinfo_t *dip, char *str, int base, int *status, hbool_t report_error)
{
    char *eptr;
    unsigned long value;

    *status = SUCCESS;
    value = CvtStrtoValue(dip, str, &eptr, base);
    if (*eptr != '\0') {
	if (report_error) {
	    Eprintf(dip, "Error parsing '%s', invalid character detected in number: '%c'\n",
		    str, *eptr);
	}
        *status = FAILURE;
    }
    return(value);
}

large_t
large_number(dinfo_t *dip, char *str, int base, int *status, hbool_t report_error)
{
    char *eptr;
    large_t value;

    *status = SUCCESS;
    value = CvtStrtoLarge(dip, str, &eptr, base);
    if (*eptr != '\0') {
	if (report_error) {
	    Fprintf(dip, "Error parsing '%s', invalid character detected in number: '%c'\n",
		    str, *eptr);
	}
        *status = FAILURE;
    }
    return(value);
}

time_t
time_value(dinfo_t *dip, char *str)
{
    char *eptr;
    time_t value;

    value = CvtTimetoValue(str, &eptr);

    if (*eptr != '\0') {
	Eprintf(dip, "Invalid character detected in time string: '%c'\n", *eptr);
	return ( HandleExit(dip, FAILURE) );
    }
    return (value);
}

int
start_iotuning(dinfo_t *dip)
{
    int pstatus = WARNING;

    if ( (dip->di_iotuning_flag == True) && (dip->di_iotune_file) && !iotuneThread ) {
	/* Much more than we need, just getting this working! */
	iotune_dinfo = clone_device(dip, True, False);
	pstatus = pthread_create( &iotuneThread, tjattrp, do_iotune_file, iotune_dinfo );
	if (pstatus != SUCCESS) {
	    tPerror(dip, pstatus, "pthread_create() failed for iotuning");
	} else {
	    pstatus = pthread_detach(iotuneThread);
	    if (pstatus != SUCCESS) {
		tPerror(dip, pstatus, "pthread_detach() failed for iotuning");
	    }
	}
    }
    return (pstatus);
}

void *
do_iotune_file(void *arg)
{
    dinfo_t *dip = arg;

    dip->di_iotuning_active = True;
#if defined(WIN32)
    iotuneThreadId = (os_tid_t)pthread_self();
#endif
    if (dip->di_debug_flag || dip->di_tDebugFlag) {
	Printf(dip, "The I/O Tuning Thread ID is "OS_TID_FMT"\n", (os_tid_t)pthread_self());
    }
    do {
	SleepSecs(dip, dip->di_iotune_delay);
	if ( PROGRAM_TERMINATING ) break;
	process_iotune(dip, dip->di_iotune_file);

    } while ( True );

    dip->di_iotuning_active = False;
    HANDLE_THREAD_EXIT(dip);
    return(NULL);
}

void *
do_triggers(void *arg)
{
    dinfo_t *dip = arg;
    int trigger_action;

    dip->di_trigger_active = True;
    if (dip->di_debug_flag || dip->di_tDebugFlag) {
	Printf(dip, "The Trigger Thread ID is "OS_TID_FMT"\n", (os_tid_t)pthread_self());
    }

    trigger_action = ExecuteTrigger(dip, "noprog");

    switch ( trigger_action ) {

	case TRIGACT_CONTINUE:
	    break;

	case TRIGACT_TERMINATE:
	    dip->di_error_count++;
	    dip->di_exit_status = FAILURE;
	    Eprintf(dip, "Trigger action is TERMINATE, setting terminate state...\n");
	    dip->di_terminating = True;
	    terminate_job(dip);
	    exit_status = FAILURE;
	    break;

	case TRIGACT_SLEEP:
	    dip->di_error_count++;
	    dip->di_exit_status = FAILURE;
	    Eprintf(dip, "Trigger action is SLEEP, sleeping forever...\n");
	    exit_status = FAILURE;
	    while (True) {
		if ( PROGRAM_TERMINATING ) break;
		if ( THREAD_TERMINATING(dip) ) break;
		SleepSecs(dip, 60);
	    }
	    break;

	case TRIGACT_ABORT:
	    dip->di_error_count++;
	    dip->di_exit_status = FAILURE;
	    Eprintf(dip, "Trigger action is ABORT, so aborting...\n");
	    dip->di_force_core_dump = True;
	    /* Note: We cannot do this for service based I/O! */
	    terminate(dip, exit_status = FAILURE);
	    break;

	default:
	    Printf(dip, "Unknown trigger action %d, terminating thread...\n", trigger_action);
	    /* Per request of SAN Automation, we now exit on unexpected status! */
	    /* Note: Multiple threads, we can no longer terminate the program! */
	    dip->di_error_count++;
	    dip->di_terminating = True;
	    terminate_job(dip);
	    exit_status = FAILURE;
	    break;
    }
    dip->di_trigger_active = False;
    HANDLE_THREAD_EXIT(dip);
    return(NULL);
}

void
terminate_job(dinfo_t *dip)
{
    if (dip->di_job->ji_job_tag) {
	Printf(dip, "Stopping all threads with tag %s...\n",
	       dip->di_job->ji_job_tag);
	(void)stop_jobs(dip, (job_id_t)0, dip->di_job->ji_job_tag);
    } else {
	Printf(dip, "Stopping all threads for job %u...\n",
	       dip->di_job->ji_job_id);
	(void)stop_jobs(dip, dip->di_job->ji_job_id, NULL);
    }
    return;
}

void
report_times(dinfo_t *dip, time_t initiated_time, time_t current_time)
{
    char time_buffer[TIME_BUFFER_SIZE];

    Printf(dip, "   The current time is: " TMF " seconds => %s\n", current_time,
	   os_ctime(&current_time, time_buffer, sizeof(time_buffer)));
    Printf(dip, "The initiated time was: " TMF " seconds => %s\n", initiated_time,
	   os_ctime(&initiated_time, time_buffer, sizeof(time_buffer)));
    //Printf(dip, "Please Note: The initiated time is when this request was issued.\n");
    return;
}

/*
 * keepalive_alarm() - Format and Display the Keepalive Message.
 *
 * Description:
 *    This function serves multiple purposes:
 *    o monitoring I/O (noprog's)
 *    o process keepalive messages
 *    o terminate when runtime reached
 *
 * Inputs:
 *    dip = The device infromation pointer.
 */
void
keepalive_alarm(dinfo_t *dip)
{
    char buffer[STRING_BUFFER_SIZE];
    register time_t initiated_time;
    register time_t current_time = time((time_t *)0);
    register time_t elapsed = 0;
    hbool_t check_noprogtime = True;

    initiated_time = dip->di_initiated_time;
    dip->di_last_alarm_time = current_time;
    if (initiated_time) {
	elapsed = (current_time - initiated_time);
	/* Sanity Check! */
	if (elapsed < 0) {
	    Wprintf(dip, "The current time has gone backwards, elapsed is %d seconds!\n", elapsed);
	    report_times(dip, initiated_time, current_time);
	    return;
	}
    }

    if (dip->di_timerDebugFlag && dip->di_noprog_flag) {
	char *bp = buffer;
	bp += Sprintf(bp, "Timer expired: initiated time " TMF ", current time " TMF,
		      initiated_time, current_time);
	if (initiated_time) {
	    bp += Sprintf(bp, " (elapsed %d secs)\n", elapsed);
	} else {
	    bp += Sprintf(bp, "\n");
	}
	Printf(dip, "%s", buffer);
    }

    /*
     * Note: Since the monitoring thread is for all jobs (currently), and the interval
     * may get modified dynamically by multiple jobs to a lower value than the specified
     * alarm time, then noprog's will occur more frequently than desired.
     * 
     * Therefore, this needs to be revisited at some point! (esp. for the I/O Service)
     * But that said, most folks only create a single job (or a set of jobs via the CLI),
     * so this is not a major issue today.
     * This can be overcome via:
     * 1) A monitoring thread for each job (this may be easiest and simplest).
     * 2) Implement an interval vs. alarm check to honor the users' alarm value.
     * 	  But keep in mind, the monitoring thread may *not* run every interval secs!
     * 	  Oftentimes, due to load, scheduling, or process hangs, we will get blocked.
     *
     * Therefore, I document this issue, but have *not* implemented a resolution yet.
     */
    /* 
     * If the monitoring interval changed from our original alarm, check time.
     * FWIW: Best I can tell, this matches the original dt noprogt messages.
     */
    if (dip->di_noprog_flag && initiated_time && ((time_t)monitor_interval < dip->di_alarmtime) ) {
	if (dip->di_next_noprog_time) {
	    if (current_time < dip->di_next_noprog_time) {
		check_noprogtime = False;	/* Don't check this interval! */
	    }
	}
    }
    /*
     * Check for and report no progress, kicking off health checks and triggers (as required).
     */
    if (dip->di_noprog_flag && check_noprogtime && initiated_time && (elapsed > dip->di_noprogtime) ) {
	char *bp = buffer;
	time_t it = initiated_time;
	char time_buffer[TIME_BUFFER_SIZE];
	char *optmsg = optiming_table[dip->di_optype].opt_name;

	if ((time_t)monitor_interval < dip->di_alarmtime) {
	    dip->di_next_noprog_time = (current_time + dip->di_alarmtime);
	}
	/* Keep no-progress (noprog) statistics. */
	dip->di_noprogs++;
	if (elapsed > dip->di_max_noprogt) {
	    dip->di_max_noprogt = elapsed;
	    dip->di_max_noprog_optype = dip->di_last_noprog_optype;
	    dip->di_max_noprog_time = dip->di_last_noprog_time;
	}
	/* Detect when the next no-progress sequence starts. */
	/* Note: We detect this with elapsed time, rather than optype/record/offset! */
	if (elapsed < dip->di_cur_max_noprogt) {
	    dip->di_total_max_noprogs++;
	    dip->di_total_max_noprogt += dip->di_cur_max_noprogt;
	    dip->di_cur_max_noprogt = 0;
#if 0
	  {
	    char *optmsg = optiming_table[dip->di_last_noprog_optype].opt_name;
	    Printf(dip, "DEBUG: total max noprogs %u, total max noprog time "LUF" for %s\n",
		   dip->di_total_max_noprogs, dip->di_total_max_noprogt, optmsg);
	  }
#endif /* 0 */
	}
	if (elapsed > dip->di_cur_max_noprogt) {
	    dip->di_cur_max_noprogt = elapsed;
	}
	dip->di_last_noprog_time = current_time;
	dip->di_last_noprog_optype = dip->di_optype;

	if ( (dip->di_optype == READ_OP)  ||
	     (dip->di_optype == WRITE_OP) ||
	     (dip->di_optype == AIOWAIT_OP) ) {
	    Offset_t offset = (Offset_t)GetStatsValue(dip, ST_OFFSET, False, NULL);
	    u_int32 lba = (dip->di_dsize) ? (u_int32)(offset / dip->di_dsize) : 0;
            /* Get the current read or write record count. */
	    u_long records = (dip->di_mode == READ_MODE) ? dip->di_records_read : dip->di_records_written;
	    records++; /* Current outstanding record. */
	    if (optmsg) {
		bp += Sprintf(bp, "No progress made for record %u (lba %u, offset " FUF ") during %s() on %s for %d seconds!",
			      records, lba, offset, optmsg, dip->di_dname, elapsed);
	    } else {
		bp += Sprintf(bp, "No progress made for record %u (lba %u, offset " FUF ") on %s for %d seconds!",
			      records, lba, offset, dip->di_dname, elapsed);
	    }
	} else {
	    if (optmsg) {
		bp += Sprintf(bp, "No progress made during %s() on %s for %d seconds!",
			      optmsg, dip->di_dname, elapsed);
	    } else {
		bp += Sprintf(bp, "No progress made on %s for %d seconds!",
			      dip->di_dname, elapsed);
	    }
	}

	if (elapsed > SECS_PER_MIN) {
	    bp += Sprintf(bp, " (");
	    bp = bformat_time(bp, (clock_t)(elapsed * hertz));
	    bp += Sprintf(bp, ")");
	}
	Printf(dip, "%s Since: %s\n", buffer, os_ctime(&it, time_buffer, sizeof(time_buffer)));

	/*
	 * The user can specify a no-progress trigger time to control when
	 * this gets executed. This way one can emit no-progress messages to
	 * monitor progress, then execute the trigger at a higher threshold.
	 */
	if (dip->di_noprogttime && (elapsed > dip->di_noprogttime) ) {
	    
	    if (dip->di_trigger_active == False) {
		Printf(dip, "This requests' elapsed time of %d, has exceeded the noprogtt of %d seconds!\n", 
		       elapsed, dip->di_noprogttime);

		report_times(dip, initiated_time, current_time);
#if defined(NO_PROGRESS_URL)
		Printf(dip, "Note: For more information regarding noprog's, please visit this link:\n");
		Printf(dip, "    %s\n", NO_PROGRESS_URL);
#endif /* defined(DATA_CORRUPTION_URL) */
	    }

	    if ( dip->di_num_triggers &&
		 (dip->di_trigger_control == TRIGGER_ON_ALL) ||
		 (dip->di_trigger_control == TRIGGER_ON_NOPROGS) ) {
        	/* Start thread to execute triggers, if not active already. */
		if (dip->di_trigger_active == False) {
		    int pstatus;
		    /* Execute triggers via a thread to allow us to keep running! */
		    pstatus = pthread_create(&dip->di_trigger_thread, tjattrp, do_triggers, dip);
		    if (pstatus != SUCCESS) {
			tPerror(dip, pstatus, "pthread_create() failed for executing triggers");
		    } else {
			pstatus = pthread_detach(dip->di_trigger_thread);
			if (pstatus != SUCCESS) {
			    tPerror(dip, pstatus, "pthread_detach() failed for executing triggers");
			}
		    }
		}
	    } else {
		Eprintf(dip, "No triggers or noprog triggers are not enabled, so stopping this job and its' threads...\n");
		dip->di_error_count++;
		dip->di_terminating = True;
		(void)stop_job(dip, dip->di_job);
		exit_status = FAILURE;
	    }
	}
    }

    /*
     * Allow empty keepalive to monitor I/O progress only.
     */
    if ( dip->di_keepalive && strlen(dip->di_keepalive) &&
	 ((current_time - dip->di_last_keepalive) >= dip->di_keepalive_time) ) {
	dip->di_last_keepalive = current_time;
        (void)FmtKeepAlive(dip, dip->di_keepalive, buffer);
	LogMsg(dip, dip->di_ofp, logLevelLog, 0, "%s\n", buffer);
    }

    /*
     * If runtime specified, see if we hit our end time.
     * Note: A runtime of -1 says run forever!
     */
    if ( (dip->di_runtime > 0) && dip->di_runtime_end) {
	if (current_time >= dip->di_runtime_end) {
	    if (dip->di_verbose_flag) {
		Printf(dip, "The runtime of %d seconds has expired, terminating thread...\n", dip->di_runtime);
	    }
	    dip->di_terminating = True;
	    /* We can't assume all threads start running at the same time, so... */
	    //(void)stop_job(dip, dip->di_job);
	    dip->di_thread_state = TS_TERMINATING;
	    dip->di_thread_stopped = time((time_t) 0);
	}
    }
    if ( (THREAD_TERMINATING(dip) == False) &&
	 (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_IOMON)) ) {
	(void)do_iops(dip);
    }
    return;
}

double
do_iops(dinfo_t *dip)
{
    double iops = 0.0;
    uint64_t actual_usecs, desired_usecs, difference;
    int secs;
    large_t records;
    hbool_t pass_stats = False;
    
    records = GetStatsValue(dip, ST_RECORDS, pass_stats, &secs);
    if ( (records == 0) || (secs == 0) ) return(iops);
    iops = ((double)records / (double)secs);
    if (iops < 1) return(iops);
    actual_usecs = (uSECS_PER_SEC / (uint32_t)iops);
    desired_usecs = (dip->di_iops_usecs);
    if (actual_usecs > desired_usecs) {
	difference = (actual_usecs - desired_usecs);
    } else {
	difference = (desired_usecs - actual_usecs);
    }
    if (dip->di_tDebugFlag == True) {
	Printf(dip, "Current usecs: %d, Desired usecs: %d\n",
	       dip->di_iops_adjust, dip->di_iops_usecs);
	Printf(dip, "Records: "LUF", Actual IOPS: %.3f, Desired IOPS: %.3f\n",
	       records, iops, dip->di_iops);
	Printf(dip, "  -> actual usecs: "LUF", desired usecs: "LUF", difference: "LUF"\n",
	       actual_usecs, desired_usecs, difference);
    }
    /* 
     * Note: The goal here is to adjust the I/O delay to dynamically change IOPS.
     * FWIW: This kind of stuff is new to me, so I'm still learning and playing! ;)
     */
    if (dip->di_iops > iops) {		/* Current IOPS too low! */
	if (desired_usecs < actual_usecs) {
	    /* Adjust the I/O delays lower to achieve higher IOPS's! */
	    dip->di_iops_adjust -= (int)difference;
	    dip->di_iops_adjust = MIN((int)dip->di_iops_usecs, dip->di_iops_adjust);
	    /* We wish to scale I/O up a little faster. */
	    dip->di_iops_adjust -= (int)((double)dip->di_iops_adjust * 0.10);
	    if (dip->di_iops_adjust < 0) dip->di_iops_adjust = 0;
	    dip->di_read_delay = dip->di_write_delay = dip->di_iops_adjust;
	    if (dip->di_tDebugFlag == True) {
		Printf(dip, "IOPS TOO LOW: setting delay to %u\n", dip->di_iops_adjust);
	    }
	}
    } else if (dip->di_iops < iops) {	/* Current IOPS too high! */
	if (desired_usecs > actual_usecs) {
	    /* Adjust the I/O delays higher to achieve lower IOPS's! */
	    dip->di_iops_adjust += (int)difference;
	    if (dip->di_iops_adjust < 0) dip->di_iops_adjust = dip->di_iops_usecs;
	    dip->di_read_delay = dip->di_write_delay = dip->di_iops_adjust;
	    if (dip->di_tDebugFlag == True) {
		Printf(dip, "IOPS TOO HIGH: setting delay to %u\n", dip->di_iops_adjust);
	    }
	}
    }
    return( iops );
}

/*
 * SignalHandler() - Signal handler for all signals we care about.
 *
 * Inputs:
 *	signal_number = The signal number.
 *
 * Return Value:
 *	void
 */
void
SignalHandler(int signal_number)
{
    dinfo_t *dip = master_dinfo;
    int exit_status = signal_number;

    if (debug_flag || pDebugFlag || tDebugFlag || dip->di_verbose_flag) {
	Printf(NULL, "Caught signal %d\n", signal_number);
    }

    /*
     * Note: Since automation often used SIGTERM to kill tools, lets make our 
     * exit status conditional by signal number. We'll also allow SIGINT (Ctrl/C). 
     */
    if ( (signal_number == SIGINT) || (signal_number == SIGTERM) ) {
	exit_status = SUCCESS;
    }

    if ( (terminating_flag == True) || (terminate_on_signals == True) ) {
	/* If already terminating, then exit the process (likely hung)! */
	if (debug_flag || pDebugFlag || tDebugFlag) {
	    if (terminating_flag == True) {
		Printf(dip, "Exiting with status %d, due to already terminating!\n", exit_status);
	    } else {
		Printf(dip, "Exiting with status %d, due to terminate on signals!\n", exit_status);
	    }
	}
	finish_exiting(dip, signal_number);
    }
    /* Terminate immediately on subsequent signals! */
    if (CmdInterruptedFlag == True) {
	terminate(dip, signal_number);
    }
    if (InteractiveFlag == False) {
	exit_status = FAILURE;
    }
    /* Note: For client/server mode, we may wish to rethink this! */
    /* Stop threads via a thread, since master thread may hold locks! */
    (void)create_detached_thread(dip, &do_stop_all_job_threads);
    CmdInterruptedFlag = True;
    catch_signals(dip);			/* Reenable signal handler. */
    /* If script file(s), close them! */
    CloseScriptFiles(dip);
    return;
}

int
create_detached_thread(dinfo_t *dip, void *(*func)(void *))
{
    pthread_t thread;
    int status;

    status = pthread_create( &thread, tjattrp, func, dip );
    if (status == SUCCESS) {
	status = pthread_detach(thread);
	if (status != SUCCESS) {
	    tPerror(dip, status, "pthread_detach() failed");
	}
    } else {
	tPerror(dip, status, "pthread_create() failed");
    }
    return(status);
}

void *
do_stop_all_job_threads(void *arg)
{
    dinfo_t *dip = arg;

    (void)stop_jobs(dip, (job_id_t) 0, NULL); /* Mark threads as terminating. */
    HANDLE_THREAD_EXIT(dip);
    return(NULL);
}


/*
 * terminate() - Terminate program with specified exit code.
 *
 * Notes:
 *	This needs reworked for multiple threads!
 *	This was originally was written for a single IO process and hacked
 * in some support for multiple threads. This needs rethought and reworked!
 *
 * Inputs:
 *	dip = The device information pointer.
 *	exit_code = The exit code or signal number if kill done.
 */
void
terminate(dinfo_t *dip, int exit_code)
{
    if (dip == NULL) dip = master_dinfo;

    if (debug_flag || pDebugFlag || tDebugFlag) {
	Printf(dip, "Terminating with exit code %d...\n", exit_code);
    }

    /*
     * If we enter here more than once, just exit to avoid
     * possible recursion.  kernel should do I/O rundown!
     */
    if (terminating_flag == True) {
	if (dip->di_force_core_dump && (exit_code != SUCCESS)) {
	    Printf(dip, "Forcing core dump via abort()...\n");
	    abort();		/* Generate a core dump. */
	} else {
	    /* If already terminating, just exit this thread! */
	    if (debug_flag || pDebugFlag || tDebugFlag) {
		Printf(dip, "Exiting with exit code %d, due to already terminating!\n", exit_code);
	    }
	    /*
	     * We expect some thread to be doing termination processing, so we
	     * do not wish to exit which would kill all threads and keep us from
	     * doing cleanup and displaying total statistics.
	     */
	    finish_exiting(dip, exit_code);
	}
    } else {
	terminating_flag = True;	/* Show we're terminating. */
    }

    /*
     * If terminating via monitoring thread, use the global exit status. (for now)
     */
    if (exit_code == SIGALRM) {
	exit_code = exit_status;
    }

    /*
     * We only come here for signals when executing multiple
     * processes, so abort active procs and continue waiting.
     */
    if ( jobs_active(dip) ) {
	(void)stop_jobs(dip, (job_id_t) 0, NULL); /* Mark threads as terminating. */
	if (kill_delay) SleepSecs(dip, kill_delay);
	/* 
	 * Note: We cannot wait here, due to signal handler, locks, and state.
	 * if we were waiting for a job, the job state never reaches JS_FINISHED,
	 * because we interrupted our pthread_join()! So, the best we can do is
	 * to stop each thread, and wait awhile above. Otherwise, we hang waiting!
	 */ 
	//(void)wait_for_jobs((job_id_t) 0, NULL);
    }

    finish_exiting(dip, exit_code);
    /*NOTREACHED*/
}

/*
 * finish_exiting() - Finish Exiting the Program.
 * 
 * Inputs:
 * 	dip = The device information pointer (may be NULL).
 * 	exit_status = The exit status.
 */ 
void
finish_exiting(dinfo_t *dip, int exit_status)
{
    if (dip == NULL) dip = master_dinfo;

    if ( (dip->di_eof_status_flag == False) && (exit_status == END_OF_FILE)) {
	exit_status = SUCCESS;		/* Map end-of-file status to Success! */
    }
    if (debug_flag || pDebugFlag || tDebugFlag) {
	Printf(dip, "Exiting with status %d...\n", exit_status);
    }
    if (dip->di_force_core_dump && (exit_status != SUCCESS) && (exit_status != END_OF_FILE)) {
	Printf(dip, "Forcing core dump via abort()...\n");
	abort(); /* Generate a core dump. */
    }
    /*
     * Map signal numbers and/or other errno's to FAILURE. (cleanup)
     * ( easier for scripts to handle! )
     */
    if ( (exit_status != FAILURE) && (exit_status != SUCCESS) && (exit_status != END_OF_FILE) ) {
	exit_status = FAILURE;			/* Usually a signal number. */
	if (debug_flag || pDebugFlag || tDebugFlag) {
	    Printf(dip, "Exit status changed to %d...\n", exit_status);
	}
    }
    if (dip->di_term_delay) {
	os_sleep(dip->di_term_delay);
    }
    /* Avoid any mystery of what our exit status is! */
    if ( (exit_status != SUCCESS) || debug_flag || pDebugFlag ) {
	if (exit_status) {
	    Fprintf(dip, "Program is exiting with status %d...\n", exit_status);
	} else {
	    Printf(dip, "Program is exiting with status %d...\n", exit_status);
	}
    }
    if (dip->di_log_file && dip->di_log_opened) {
	(void)fclose(dip->di_efp);
    }
    if (error_logfp) {
	(void)CloseFile(dip, &error_logfp);
    }
    if (master_logfp) {
	(void)CloseFile(dip, &master_logfp);
    }
    exit(exit_status);
}

void
handle_thread_exit(dinfo_t *dip)
{
    if (debug_flag || tDebugFlag) {
	Printf(dip, "Thread "OS_TID_FMT" is exiting...\n", (os_tid_t)pthread_self() );
    }
    pthread_exit(dip);
    return;
}

int
nofunc(struct dinfo *dip)
{
    return(SUCCESS);
}

static char *multi_prompt = 
    "\nPlease insert volume #%d in drive %s, press ENTER when ready to proceed: \007";
static char *multi_nready =
    "The drive is NOT ready or encountered an error, Retry operation (Yes): \007";

int
HandleMultiVolume (struct dinfo *dip)
{
    int status;

    status = RequestMultiVolume (dip, False, dip->di_oflags);
    if (status == FAILURE) return (status);

    if (dip->di_mode == READ_MODE) {
	dip->di_volume_bytes = (large_t)(dip->di_dbytes_read + dip->di_total_bytes_read);
	if (dip->di_verbose_flag) {
	  if ( dip->di_multiple_files || (dip->di_dtype->dt_dtype == DT_TAPE) ) {
	    Print (NULL, "    [ Continuing in file #%lu, record #%lu, bytes read so far " LUF "... ]\n",
		(dip->di_files_read + 1), (dip->di_records_read + 1), dip->di_volume_bytes);
	  } else {
	    Print (NULL, "    [ Continuing at record #%lu, bytes read so far " LUF "... ]\n",
			(dip->di_records_read + 1), dip->di_volume_bytes);
	  }
	}
	dip->di_vbytes_read = (large_t) 0;
    } else {
	dip->di_volume_bytes = (large_t)(dip->di_dbytes_written + dip->di_total_bytes_written);
	if (dip->di_verbose_flag) {
	  if ( dip->di_multiple_files || dip->di_dtype->dt_dtype == DT_TAPE) {
	    Print (NULL, "    [ Continuing in file #%lu, record #%lu, bytes written so far " LUF "... ]\n",
		(dip->di_files_written + 1), (dip->di_records_written + 1), dip->di_volume_bytes);
	  } else {
	    Print (NULL, "    [ Continuing at record #%lu, bytes written so far " LUF "... ]\n",
			(dip->di_records_written + 1), dip->di_volume_bytes);
	  }
	}
	dip->di_vbytes_written = (large_t) 0;
    }
    (void)fflush(ofp);
    dip->di_media_changed = True;
    dip->di_volume_records = 0;
    if (exit_status == END_OF_FILE) {
	exit_status = SUCCESS;		/* Ensure END_OF_FILE status is reset! */
    }
    return (status);
}

int
RequestFirstVolume (struct dinfo *dip, int oflags)
{
    int status;

    dip->di_multi_volume = 0;

    status = RequestMultiVolume (dip, True, oflags);

    dip->di_volume_bytes = (large_t) 0;
    dip->di_volume_records = 0;

    return (status);
}

int
RequestMultiVolume (struct dinfo *dip, hbool_t reopen, int oflags)
{
    struct dtfuncs *dtf = dip->di_funcs;
    char buffer[256];
    char *bp = buffer;
    FILE *fp;
    int saved_exit_status;
    u_long saved_error_count;
    int status;

    if (terminating_flag == True) return (FAILURE);

    if ( (status = (*dtf->tf_close)(dip)) == FAILURE) {
	return (status);
    }

    if ( (fp = fopen("/dev/tty", "r+")) == NULL) {
	ReportErrorInfo(dip, "/dev/tty", os_get_error(), "fopen failed", OPEN_OP, False);
	return (FAILURE);
    }
    dip->di_multi_volume++;

    (void)sprintf(bp, multi_prompt, dip->di_multi_volume, dip->di_dname);

    (void) fputs (bp, fp); fflush(fp);
    if (fgets (bp, sizeof(buffer), fp) == NULL) {
	Print (NULL, "\n");
	status = FAILURE;	/* eof or an error */
	return (status);
    }

    saved_error_count = dip->di_error_count;
    saved_exit_status = exit_status;

    /*
     * This is an important step, so allow the user to retry on errors.
     */
    do {
	if (!reopen) {
	    status = (*dtf->tf_open)(dip, oflags);
	} else {
	    status = (*dtf->tf_reopen_file)(dip, oflags);
	}
	if (status == SUCCESS) {
#if defined(TAPE) && !defined(__QNXNTO__) && !defined(AIX) && !defined(WIN32)
	    if (dip->di_dtype->dt_dtype == DT_TAPE) {
		status = DoRewindTape (dip);
		if (status == FAILURE) {
		    (void)(*dtf->tf_close)(dip);
		}
	    }
#endif /* defined(TAPE) && !defined(__QNX_NTO__) && !defined(AIX) && !defined(WIN32) */
	}
	if (status == FAILURE) {
	    (void) fputs (multi_nready, fp); fflush(fp);
	    if (fgets (bp, sizeof(buffer), fp) == NULL) {
		Print (NULL, "\n");
		break;
	    }
	    if ( (bp[0] == 'N') || (bp[0] == 'n') ) {
		break;
	    }
	    dip->di_error_count = saved_error_count;
	    exit_status = saved_exit_status;
	} else {
	    break;		/* device is ready! */
	}
    } while (status == FAILURE);

    (void)fclose(fp);
    return (status);
}

/*
 * do_monitoring() - Monitoring Thread.
 *
 * Description:
 *      Thie thread monitors all jobs/threads right now.
 *
 * Inputs:
 *	arg = Pointer to device information.
 *
 * Return Value:
 *	NULL pointer.
 */
#define QUEUE_EMPTY(jobs)	(jobs->ji_flink == jobs)

void *
do_monitoring(void *arg)
{
    dinfo_t *mdip = arg;
    job_info_t *jhdr, *job;
    int status;

    ignore_signals(NULL);
#if defined(WIN32)
    /* TODO: Will go away! (handle vs real thread ID crap!) */
    MonitorThreadId = pthread_self();
#endif
    if (debug_flag || tDebugFlag) {
	Printf(NULL, "The Monitor Thread ID is "OS_TID_FMT"\n", (os_tid_t)pthread_self());
    }

    /*
     * Basically wait for the monitor interval, and allow the standard
     * keepalive() handling to process noprog, keepalive, & runtime.
     */
    while (True) {
	if ( PROGRAM_TERMINATING ) break;
	/* Note: Too noisy when timer debug is enabled, so... */
        //SleepSecs(mdip, monitor_interval);
	os_sleep(monitor_interval);
	if ( PROGRAM_TERMINATING ) break;

#if 0
	/* TODO: Add an option to control this behavior! */
	/* Removing for now, since this breaks existing scripts! */
    	/*
	 * This handles the case where automation killed the connection, but dt is still running! 
	 * Note: This is happening with NFS hard mounts and may or may not actually stop jobs! 
	 */
	if ( os_getppid() == UNIX_INIT_PROCESS ) {
	    Wprintf(mdip, "The parent process ID is now the INIT process, so stopping all jobs!\n");
	    stop_jobs(mdip, (job_id_t)0, NULL); /* Stop all jobs, we've been abandoned! */
	    terminating_flag = True;
	    break;
	}
#endif /* 0 */

	if ( QUEUE_EMPTY(jobs) ) continue;
	if ( (status = acquire_jobs_lock(mdip)) != SUCCESS) break;
	jhdr = jobs;
	job = jhdr;
	/* Check each job's threads! */
	/* Note: We may wish a monitoring thread for each job? */
	while ( (job = job->ji_flink) != jhdr) {
	    threads_info_t *tip = job->ji_tinfo;
	    dinfo_t *dip = tip->ti_dts[0];
	    /* Note: This will report until jobs are waited on! */
	    if ((InteractiveFlag == False) && dip->di_jDebugFlag) {
		(void)show_job_info(mdip, job, False);
	    }
	    if ( (job->ji_job_state == JS_RUNNING) && tip->ti_dts) {
		/* Allow stop on file for all I/O behaviors! */
		if (dip->di_stop_immediate == True) {
		    (void)stop_job_on_stop_file(mdip, job);
		}
	    }
	    if (job->ji_job_state == JS_TERMINATING) {
		register time_t current_time = time((time_t *)0);
		register time_t elapsed = (current_time - job->ji_job_stopped);
		/* Detect hung threads by job being stopped too long! */
		if ( dip->di_term_wait_time && (elapsed > dip->di_term_wait_time) ) {
		    Printf(mdip, "Job %u, thread has exceeded the max terminate wait time of %d seconds!\n",
			   job->ji_job_id, dip->di_term_wait_time);
		    Printf(mdip, "Threads have NOT terminated for %d seconds, perhaps too slow or hung?\n", elapsed);
		    if ( (dip->di_terminating == False) && (dip->di_trigger_active == True) ) {
			Wprintf(mdip, "Triggers are still active, so *not* cancelling threads!\n");
		    } else {
			Eprintf(mdip, "Job %u has NOT terminated, so cancelling all threads!\n", job->ji_job_id);
			(void)cancel_job_threads(mdip, tip);
		    }
		}
	    } else if ( (job->ji_job_state == JS_RUNNING) && tip->ti_dts) {
		int thread;
                if (dip->di_iobf && dip->di_iobf->iob_job_keepalive) {
                    (void)(*dip->di_iobf->iob_job_keepalive)(mdip, dip->di_job);
		    /* Continue below to examine runtimes, etc. */
                }
		for (thread = 0; (thread < tip->ti_threads); thread++) {
		    /* Check each running thread. */
		    dip = tip->ti_dts[thread];
		    if ( (dip->di_thread_state == TS_RUNNING) && dip->di_program_start) {
			(void)keepalive_alarm(dip);
		    } else if (dip->di_thread_state == TS_TERMINATING) {
			register time_t current_time = time((time_t *)0);
			register time_t elapsed = (current_time - dip->di_thread_stopped);
			if ( dip->di_term_wait_time && (elapsed > dip->di_term_wait_time) ) {
			    Printf(dip, "Job %u, thread has exceeded the max terminate wait time of %d seconds!\n",
				   job->ji_job_id, dip->di_term_wait_time);
			    if ( (dip->di_terminating == False) && (dip->di_trigger_active == True) ) {
				Wprintf(dip, "Triggers are still active, so *not* cancelling thread!\n");
			    } else if (dip->di_deleting_flag == True) {
				Wprintf(dip, "Deleting files is still active, so *not* cancelling thread!\n");
			    } else if (dip->di_history_dumping == True) {
				Wprintf(dip, "History is being dumped, so *not* cancelling thread!\n");
			    } else {
				/* Report total statistics prior to cancelling threads. */
				gather_stats(dip);			/* Gather the device statistics. */
				gather_totals(dip);			/* Update the total statistics.	*/
				report_stats(dip, TOTAL_STATS);
				Eprintf(dip, "Thread has NOT terminated for %d seconds, so cancelling thread!\n", elapsed);
				(void)cancel_thread_threads(mdip, dip);
				/* Avoid trying to cancel again! */
				dip->di_term_wait_time = 0;
			    }
			    if (dip->di_thread_state != TS_CANCELLED) {
				time_t frequency = min(dip->di_term_wait_time, THREAD_TERM_WAIT_FREQ);
				/* Avoid reporting warnings every second! */
				dip->di_thread_stopped = (current_time + frequency);
			    }
			}
		    }
		}
	    }
	}
	(void)release_jobs_lock(mdip);
    }
    HANDLE_THREAD_EXIT(mdip);
    return(NULL);
}

/*
 * WTF! Linux stack size is 10MB by default! (will get cleaned up one day)
 */
#define THREAD_STACK_ENV "DT_THREAD_STACK_SIZE" /* Thread stack size.	*/
#if !defined(PTHREAD_STACK_MIN)
#  define PTHREAD_STACK_MIN 16384
#endif
/* Note: If the stack size is too small we seg fault, too large wastes address space! */
//#define THREAD_STACK_SIZE	((PTHREAD_STACK_MIN + STRING_BUFFER_SIZE) * 4)
//				/* Reduce TLS to avoid wasting swap/memory! */
#define THREAD_STACK_SIZE	MBYTE_SIZE	/* Same default as Windows! */

int
setup_thread_attributes(dinfo_t *dip, pthread_attr_t *tattrp, hbool_t joinable_flag)
{
    size_t currentStackSize = 0;
    size_t desiredStackSize = THREAD_STACK_SIZE;
    char *p, *string;
    int status;

    if (p = getenv(THREAD_STACK_ENV)) {
	string = p;
	desiredStackSize = number(dip, string, ANY_RADIX, &status, False);
    }

    /* Remember: pthread API's return 0 for success; otherwise, an error number to indicate the error! */
    status = pthread_attr_init(tattrp);
    if (status != SUCCESS) {
	tPerror(dip, status, "pthread_attr_init() failed");
	return (status);
    }
#if !defined(WIN32)
    status = pthread_attr_setscope(tattrp, PTHREAD_SCOPE_SYSTEM);
    if ( (status != SUCCESS) && (status != ENOTSUP) ) {
	tPerror(dip, status, "pthread_attr_setscope() failed setting PTHREAD_SCOPE_SYSTEM");
	/* This is considered non-fatal! */
    }

    /*
     * Verify the thread stack size (TLS) is NOT too large! (Linux issue). 
     */
    status = pthread_attr_getstacksize(tattrp, &currentStackSize);
    if (status == SUCCESS) {
	if (dip->di_debug_flag || dip->di_tDebugFlag) {
	    Printf(dip, "Current thread stack size is %u (%.3f Kbytes)\n",
		   currentStackSize, ((float)currentStackSize / (float)KBYTE_SIZE));
	}
    } else {
	if (dip->di_debug_flag || dip->di_tDebugFlag) {
	    tPerror(dip, status, "pthread_attr_getstacksize() failed!");
	}
    }

    /*
     * The default stack size on Linux is 10M, which limits the threads created!
     * ( Note: On a 32-bit executable, 10M is stealing too much address space! )
     */
    if (currentStackSize && desiredStackSize && (currentStackSize > desiredStackSize) ) {
	/* Too small and we seg fault, too large limits our threads. */
	status = pthread_attr_setstacksize(tattrp, desiredStackSize);
	if (status == SUCCESS) {
	    if (dip->di_debug_flag || dip->di_tDebugFlag) {
		Printf(dip, "Thread stack size set to %u bytes (%.3f Kbytes)\n",
		       desiredStackSize, ((float)desiredStackSize / (float)KBYTE_SIZE));
	    }
	} else {
	    tPerror(dip, status, "pthread_attr_setstacksize() failed setting stack size %u",
		   desiredStackSize);
	}
    }
    if (joinable_flag) {
	status = pthread_attr_setdetachstate(tattrp, PTHREAD_CREATE_JOINABLE);
	if (status != SUCCESS) {
	    tPerror(dip, status, "pthread_attr_setdetachstate() failed setting PTHREAD_CREATE_JOINABLE");
	}
    } else {
	status = pthread_attr_setdetachstate(tattrp, PTHREAD_CREATE_DETACHED);
	if (status != SUCCESS) {
	    tPerror(dip, status, "pthread_attr_setdetachstate() failed setting PTHREAD_CREATE_DETACHED");
	}
    }
#endif /* !defined(WIN32) */
    return(status);
}

int
init_pthread_attributes(dinfo_t *dip)
{
    size_t currentStackSize = 0;
    size_t desiredStackSize = THREAD_STACK_SIZE;
    char *p, *string;
    int status;

    if (p = getenv(THREAD_STACK_ENV)) {
	string = p;
	desiredStackSize = number(dip, string, ANY_RADIX, &status, False);
    }
    ParentThreadId = pthread_self();

#if 1
    /*
     * Create just joinable thread attributes, and use pthread_detach()
     * whenever a detached thread is desired. This change is for Windows
     * which will close the thread handle in pthread_detach() to mimic
     * POSIX detached threads (necessary so we don't lose handles!).
     */
    status = setup_thread_attributes(dip, tjattrp, True);
    if (status != SUCCESS) {
	tPerror(dip, status, "pthread_attr_init() failed");
	return (status);
    }
#else /* 0 */
    /* Create attributes for detached and joinable threads (for Unix). */
    status = setup_thread_attributes(dip, tdattrp, False);
    if (status == SUCCESS) {
	status = setup_thread_attributes(dip, tjattrp, True);
    }
    if (status != SUCCESS) {
	tPerror(dip, status, "pthread_attr_init() failed");
	return (status);
    }
#endif /* 1 */
    /* We may wish to move this initialization elsewhere. */
    if ( (status = pthread_mutex_init(&print_lock, NULL)) != SUCCESS) {
	tPerror(NULL, status, "pthread_mutex_init() of print lock failed!");
    }
    return (status);
}

int
StartMonitorThread(dinfo_t *dip, unsigned int interval)
{
    int status = WARNING;		/* Shows already running. */

    /*
     * One monitoring thread for all jobs (right now)!
     */ 
    if (interval < monitor_interval) {
	monitor_interval = interval;	/* Dynamically adjust, as required. */
    }
    /* Beware: Multiple threads may be invoking this startup! */
    if (dip->di_TimerActive == False) {
        dip->di_TimerActive = True;
	/* Note: Monitor every second to handle new jobs with smaller intervals! */
	monitor_interval = interval;
	dip->di_monitor_interval = monitor_interval; // 1; // interval; (last)
	/* Note: The master device information (dip) is used, always exists! */
	status = pthread_create( &MonitorThread, tjattrp, do_monitoring, dip );
	if (status == SUCCESS) {
	    status = pthread_detach(MonitorThread);
	    if (status != SUCCESS) {
		tPerror(dip, status, "pthread_detach() failed");
	    }
	} else {
	    tPerror(dip, status, "pthread_create() failed");
	    dip->di_TimerActive = False;
	}
    }
    return(status);
}

int
ParseWorkload(dinfo_t *dip, char *workload)
{
    int		argc;			/* Argument count.              */
    char	**argv;			/* Argument pointer array.      */
    char	*cmdbufptr;		/* The command buffer pointer.	*/
    size_t	cmdbufsiz;		/* The command buffer size.	*/
    int		status;
    
    if ( (workload == NULL) || (strlen(workload) == 0) ) {
	Eprintf(dip, "Missing workload definition!\n");
	return(FAILURE);
    }

    /* Save the original device information. */
    argc = dip->argc;
    argv = dip->argv;
    cmdbufptr = dip->cmdbufptr;
    cmdbufsiz = dip->cmdbufsiz;
    
    /* Note: Large buffer for expanding variables (if any). */
    dip->cmdbufsiz = ARGS_BUFFER_SIZE;
    if ( !(dip->cmdbufptr = Malloc(dip, dip->cmdbufsiz)) ) {
	return(FAILURE);
    }

    /* Setup the workload options to parse. */
    strcpy(dip->cmdbufptr, workload);
    dip->cmdbufsiz = strlen(workload);
    dip->argv = (char **)Malloc(dip, (sizeof(char **) * ARGV_BUFFER_SIZE) );
    if (dip->argv == NULL) {
	return(FAILURE);
    }
    
    /* Expand variables, make an arg list, then parse options. */
    status = ExpandEnvironmentVariables(dip, dip->cmdbufptr, dip->cmdbufsiz);
    if (status == SUCCESS) {
	dip->argc = MakeArgList(dip->argv, dip->cmdbufptr);
	if (dip->argc == FAILURE) status = FAILURE;
    }
    if (status == SUCCESS) {
	status = parse_args(dip, dip->argc, dip->argv);
    }
    
    free(dip->argv);
    free(dip->cmdbufptr);
    
    /* Restore the original command information. */
    dip->argc = argc;
    dip->argv = argv;
    dip->cmdbufptr = cmdbufptr;
    dip->cmdbufsiz = cmdbufsiz;
    return(status);
}

int
SetupCommandBuffers(dinfo_t *dip)
{
    /*
     * Setup command buffers, if not already done!
     */
    if (dip->cmdbufptr == NULL) {
	dip->cmdbufsiz = ARGS_BUFFER_SIZE;
	if ( (dip->cmdbufptr = Malloc(dip, dip->cmdbufsiz)) == NULL ) {
	    return(FAILURE);
	}
	/*
	 * Allocate an array of pointers for parsing arguments.
	 * Note: This overrides what arrived via the CLI in main()!
	 */ 
	dip->argv = (char **)Malloc(dip, (sizeof(char **) * ARGV_BUFFER_SIZE) );
	if (dip->argv == NULL) {
	    return(FAILURE);
	}
    } else {
	*dip->cmdbufptr = '\0';
    }
    return(SUCCESS);
}

/*
 * dtGetCommandLine() - Get Command Line to Execute.
 *
 * Description:
 *	This function gets the next command line to execute. This can
 * come from the user or from a script file.
 *
 * Inputs:
 *	dip = The device information pointer.
 *
 * Outputs:
 *	Initializes the argument count and list.
 *	Returns SUCCESS / FAILURE.
 */
int
dtGetCommandLine(dinfo_t *dip)
{
    char *bufptr, *p;
    size_t bufsiz;
    FILE *stream;
    hbool_t continuation = False;
    int status;

    /*
     * Done here, since not required for single CLI commands. 
     */
    if (dip->cmdbufptr == NULL) {
       	dip->cmdbufsiz = ARGS_BUFFER_SIZE;
	if ( (dip->cmdbufptr = Malloc(dip, dip->cmdbufsiz)) == NULL ) {
	    return(FAILURE);
	}
	/*
	 * Allocate an array of pointers for parsing arguments.
	 * Note: This overrides what arrived via the CLI in main()!
	 */ 
	dip->argv = (char **)Malloc(dip, (sizeof(char **) * ARGV_BUFFER_SIZE) );
	if (dip->argv == NULL) {
	    return(FAILURE);
	}
    } else {
	*dip->cmdbufptr = '\0';
    }
reread:
    bufptr = dip->cmdbufptr;
    bufsiz = dip->cmdbufsiz;
    status = SUCCESS;
    if (dip->script_level) {
	stream = dip->sfp[dip->script_level-1];
    } else {
	stream = stdin;
	if (InteractiveFlag) {
	    mPrint(dip, "%s> ", cmdname);
	} else if (PipeModeFlag) {
	    /* Short delay is required for stderr/stdout processing. */
	    if (PipeDelay) os_msleep(PipeDelay);
	    /* Note: The newline is required for shell reads! */
	    mPrint(dip, "%s> ? %d\n", cmdname, dip->di_exit_status);
	    dip->di_exit_status = SUCCESS; /* Prime for next command. */
	    /* Note: Consider adding EmitStatus variable! */
	}
	(void)fflush(stdout);
    }
read_more:
    if (fgets(bufptr, (int)bufsiz, stream) == NULL) {
	if (feof (stream) != 0) {
	    status = END_OF_FILE;
	    if (stream != stdin) {
		CloseScriptFile(dip);
		if (dip->script_level || InteractiveFlag) {
		    goto reread;
		} else {
		    return(status);
		}
	    }
	} else {
	    status = FAILURE;
	}
	mPrint(dip, "\n");
	clearerr(stream);
	return (status);
    }
    if (stream != stdin) {
	dip->script_lineno[dip->script_level-1]++;
    }
    /*
     * Handle comments early so we can embed comments in continuation lines.
     */
    p = bufptr;
    while ( (*p == ' ') || (*p == '\t') ) {
	p++;	/* Skip leading whitespace. */
    }
    if (*p == '#') {
	*p = '\0';
	if (continuation == True) {
	    if (InteractiveFlag && !dip->script_level) {
		mPrint(dip, "> ");
		(void)fflush(dip->di_ofp);
	    }
	    /* Note: We discard the entire comment line! */
	    goto read_more;
	} else {
	    goto reread;
	}
    }
    /*
     * Handle continuation lines.
     */ 
    if (p = strrchr(bufptr, '\n')) {
	--p;
	/* Handle Windows <cr><lf> sequence! */
	if (*p == '\r') {
	    --p;
	}
	if ( (p >= bufptr) && (*p == '\\') ) {
	    *p = '\0';
	    bufsiz -= strlen(bufptr);
	    continuation = True;
	    if (bufsiz) {
		bufptr = p;
		if (InteractiveFlag && !dip->script_level) {
		    mPrint(dip, "> ");
		   (void)fflush(dip->di_ofp);
		}
		goto read_more;
	    }
	}
    }
    cleanup_EOL(dip->cmdbufptr);

    status = ExpandEnvironmentVariables(dip, dip->cmdbufptr, dip->cmdbufsiz);
    /*
     * Display the expanded command line, depending on our mode.
     * TODO: This is rather messy, try to cleanup as time permits!
     */ 
    {
	/* Original dt (complicated) logic for prompt/command display! */
	if ( ((InteractiveFlag || dip->di_debug_flag) && dip->script_level) ||
	     (dip->script_level && dip->di_script_verify && !PipeModeFlag) ) {
	    hbool_t prompt = (dip->script_level) ? True : False;
	    display_command(dip, dip->cmdbufptr, prompt);
	}
#if 0
	if (dip->script_level) {
	    mPrint(dip, "%s> ", cmdname);
	}
	mPrint(dip, "%s\n", dip->cmdbufptr);
	(void)fflush(dip->di_ofp);
#endif
    }
    if (status == SUCCESS) {
	dip->argc = MakeArgList(dip->argv, dip->cmdbufptr);
	if (dip->argc == FAILURE) status = FAILURE;
    }
    return(status);
}

void
cleanup_EOL(char *string)
{
    char *p = string;
    size_t length = strlen(p);
    if (length == 0) return;
    p += (length - 1);
    while( length &&
	   ( (*p == '\n') || (*p == '\r') ||
	     (*p == ' ') || (*p == '\t') ) ) {
	*p-- = '\0';
	length--;
    }
    return;
}

void
display_command(dinfo_t *dip, char *command, hbool_t prompt)
{
    if (prompt == True) {
	mPrint(dip, "%s> ", cmdname);
    }
    mPrint(dip, "%s\n", command);
    (void)fflush(dip->di_ofp);
    return;
}

char *
expand_word(dinfo_t *dip, char **from, size_t bufsiz, int *status)
{
    char *src = *from;
    char *env, *bp, *str;

    if ( (str = Malloc(dip, bufsiz)) == NULL) {
	*status = FAILURE;
	return(NULL);
    }
    *status = SUCCESS;
    bp = str;

    /* Note: Nested conditional expansion not supported! */
    while ( (*src != '}') && (*src != '\0') ) {
	/* Check for nested variable and expand them! */
	if ( (*src == '$') && (*(src+1) == '{') ) {
	    int var_len = 0;
	    char *var;
	    src += 2;
	    var = src;
	    while ( (*src != '}') && (*src != '\0') ) {
		src++; var_len++;
	    }
	    if (*src != '}') {
		var_len = (int)((src - var) + 1);
		Eprintf(dip, "Failed to find right brace expanding: %.*s\n", var_len, var);
		*status = FAILURE;
		break;
	    }
	    *src = '\0';
	    env = getenv(var);
	    *src = '}';
	    /* Note: Not defined is acceptable! */
	    if (env) {
		while (*env) {
		    *str++ = *env++;
		}
	    }
	    src++; /* skip '}' */
	} else {
	    *str++ = *src++;
	}
    }
    *from = src;
    if ( (*status == SUCCESS) && strlen(bp) ) {
	str = strdup(bp);
    } else {
	str = NULL;
    }
    FreeStr(dip, bp);
    return(str);
}

int
ExpandEnvironmentVariables(dinfo_t *dip, char *bufptr, size_t bufsiz)
{
    char   *from = bufptr;
    char   *bp, *to, *env, *p, *str = NULL;
    size_t length = strlen(from);
    int    status = SUCCESS;

    if ( *from == '#' ) return(status); /* don't expand comments! */
    if ( (p = strstr(from, "${")) == NULL) return(status);
    if ( (to = Malloc(dip, bufsiz)) == NULL) return(FAILURE);
    bp = to;

    while (length > 0) {
	/*
	 * Parse variables and do limited substitution (see below). 
	 * TODO: It would be nice to support more of shell expansion. 
	 */
	if ( (*from == '$') && (*(from+1) == '{') ) {
	    hbool_t conditional = False;
	    hbool_t error_if_not_set = False;
	    char sep, *var = (from + 2);
	    int var_len = 0;
	    env = NULL;

	    p = var;
	    while ( (*p != ':') && (*p != '}') && (*p != '\0') ) {
		p++;
	    }
	    sep = *p;
	    *p = '\0';
	    env = getenv(var);
	    *p = sep;
	    /* Allow ${VAR:string} format. */
	    /* If VAR is set, then use it, otherwise use string. */
	    if (*p == ':') {
		conditional = True;
		p++;
		/* ${VAR:?string} */
		/* If VAR is not set, print string and error! */
		if (*p == '?') {
		    error_if_not_set = True;
		    p++;
		} else if (*p == '-') {
		    p++;	/* Treat the same as ${VAR:string}, ksh style! */
		}
		/* Expand the word part. */
		str = expand_word(dip, &p, bufsiz, &status);
		if (status == FAILURE) break;
	    }
	    var_len = (int)((p - from) + 1);

	    if (*p != '}') {
		var_len = (int)((p - from) + 1);
		Eprintf(dip, "Failed to find right brace expanding: %.*s\n", var_len, from);
		return(FAILURE);
	    }
	    if ( (conditional == True) && (error_if_not_set == True) &&
		 ( (env == NULL) || (strlen(env) == 0) ) ) {
		/* Note: The string is the error message! */
		if ( (str == NULL) || (strlen(str) == 0) ) {
		    Eprintf(dip, "Not defined: %.*s\n", var_len, from);
		} else if (dip->di_debug_flag == True) {
		    Eprintf(dip, "%s: %.*s\n", str, var_len, from);
		} else {
		    Eprintf(dip, "%s\n", str);
		}
		status = FAILURE;
		break;
	    } else if ( (conditional == True) && str && (env == NULL) ) {
		size_t str_len;
		str_len = strlen(str);
		if ( (size_t)((to + str_len) - bp) < bufsiz) {
		    to += Sprintf(to, "%s", str);
		    length -= var_len;
		    from += var_len;
		    if (str) { FreeStr(dip, str); str = NULL; }
		    continue;
		}
	    } else if (env) {
		size_t env_len;
		env_len = strlen(env);
		if ( (size_t)((to + env_len) - bp) < bufsiz) {
		    to += Sprintf(to, "%s", env);
		    length -= var_len;
		    from += var_len;
		    if (str) { FreeStr(dip, str); str = NULL; }
		    continue;
		}
	    } else {
		Eprintf(dip, "Failed to expand variable: %.*s\n", var_len, from);
		status = FAILURE;
		break;
	    }
	}
	*to++ = *from++;
	length--;
    }
    if (status == SUCCESS) {
	(void)strcpy(bufptr, bp);
    }
    FreeStr(dip, bp);
    return(status);
}

/*
 * MakeArgList() - Make Argument List from String.
 *
 * Description:
 *	This function creates an argument list from the string passed
 * in.  Arguments are separated by spaces and/or tabs and single and/or
 * double quotes may used to delimit arguments.
 *
 * Inputs:
 *	argv = Pointer to argument list array.
 *	s = Pointer to string buffer to parse.
 *
 * Outputs:
 *    Initialized the argv array and returns the number of arguments parsed.
 */
int
MakeArgList(char **argv, char *s)
{
    int c, c1;
    char *cp;
    int nargs = 0;
    char *str = s;

    /* Check for a comment. */
    if (*s == '#') {
	return(nargs);
    }
    /*
     * Skip over leading tabs and spaces.
     */
    while ( ((c = *s) == ' ') ||
	    (c == '\t') ) {
	s++;
    }
    if ( (c == '\0') || (c == '\n') ) {
	return(nargs);
    }
    /*
     * Strip trailing tabs and spaces.
     */
    for (cp = s; ( ((c1 = *cp) != '\0') && (c1 != '\n') ); cp++)
	;
    do {
	*cp-- = '\0';
    } while ( ((c1 = *cp) == ' ') || (c1 == '\t') );

    *argv++ = s;
    for (c = *s++; ; c = *s++) {

	switch (c) {
	    
	    case '\t':
	    case ' ':
		*(s-1) = '\0';
		while ( ((c = *s) == ' ') ||
			(c == '\t') ) {
		    s++;
		}
		*argv++ = s;
		nargs++;
		break;

	    case '\0':
	    case '\n':
		nargs++;
		*argv = 0;
		*(s-1) = '\0';
		return(nargs);

	    case '"':
	    case '\'': {
		/* Remove the quoting. */
		char *to = (s-1);
		char *from = NULL;
		while ( (c1 = *to++ = *s++) != c) {
		    if ( (c1 == '\0') || (c1 == '\n') ) {
			Eprintf(NULL, "Missing trailing quote parsing: %s\n", str);
			return(-1);
		    }
		}
                /* Copy rest of string over last quote, in case there's more text! */
                /* Note: This string copy corrupts the string! Optimized machine code? */
                //s = strcpy((to-1), s);
                to--;           /* Point to the trailing quote. */
                from = s;       /* Copy the current string pointer. */
                s = to;         /* This becomes the updated string. */
                while (*to++ = *from++) ;
                break;
	    }
	    default:
		break;
	}
    }
}

/*
 * init_device_information() - Initialize The Device Information Structure.
 *
 * Return Value:
 *      Returns a pointer to the device information data structure.
 */
static dinfo_t *
init_device_information(void)
{
    dinfo_t *dip;
    
    dip = (struct dinfo *)malloc( sizeof(*dip) );
    if (dip == NULL) {
	printf("ERROR: We failed to allocate the initial device information of %u bytes!\n",
	       (unsigned)sizeof(*dip));
	return(NULL);
    }
    memset(dip, '\0', sizeof(*dip));
    dip->di_efp = efp; // = stderr;
    dip->di_ofp = ofp; // = stdout;
    dip->di_dir_sep = DIRSEP;
    dip->di_file_sep = strdup(DEFAULT_FILE_SEP);
    dip->di_file_postfix = strdup(DEFAULT_FILE_POSTFIX);
    dip->di_log_bufsize	= LOG_BUFSIZE;
    dip->di_log_buffer = malloc(dip->di_log_bufsize);
    dip->di_log_bufptr = dip->di_log_buffer;
    //dip->di_msg_buffer = (char *)Malloc(dip, dip->di_log_bufsize);

    init_device_defaults(dip);

    /* 
     * These flags get set only once, and are considered "sticky"!
     * That is, changing them from the CLI are retained for future.
     */
    dip->di_inet_family = AF_UNSPEC;	/* Lookup IPv4 & IPv6. */
    dip->di_extended_errors = True;
    dip->di_fsfull_restart = True;
    dip->di_job_stats_flag = DEFAULT_JOB_STATS_FLAG;
    dip->di_pstats_flag = DEFAULT_PASS_STATS_FLAG;
    dip->di_total_stats_flag = DEFAULT_TOTAL_STATS_FLAG;
    dip->di_script_verify = DEFAULT_SCRIPT_VERIFY;
    dip->di_sleep_res = SLEEP_DEFAULT;
    dip->di_uuid_dashes = True;
    /* Note: Decided to enable all vflags, since this affects btag reporting! */
    dip->di_initial_vflags = BTAGV_ALL; /* BTAGV_QV; */
    dip->di_btag_vflags = dip->di_initial_vflags;

    /* Initialize debug flags. */
#if 0
    dip->di_debug_flag = False;
    dip->di_Debug_flag = False;
    dip->di_eDebugFlag = False;
    dip->di_fDebugFlag = False;
    dip->di_pDebugFlag = False;
    dip->di_rDebugFlag = False;
    dip->di_sDebugFlag = False;
    dip->di_tDebugFlag = False;
    dip->di_timerDebugFlag = False;
#endif /* 0 */
    dip->di_start_time = times(&dip->di_stimes);
    gettimeofday(&dip->di_start_timer, NULL);

    return (dip);
}

/*
 * Note: These defaults get set for each command line. 
 *  
 * BEWARE: When running scripts, it's imperative that ALL options get 
 * reset to their original defaults! Therefore, when adding new options 
 * if you expect a particular default, such as zero (0), then that option 
 * MUST be initialized here! I (the author) have been bite by this more than 
 * once, such as the slice number and/or step offset, and the result is very 
 * difficult and timeconsuming debugging, esp. due to assumptions being made! 
 *  
 * FYI: SO, why do this at all?
 * Well, the idea as shown above, is to allow "slicky" options, so all jobs
 * and their associated threads inherit those new settings. While the idea
 * is sound, heed the warning above, or bear the resulting support requests!
 */ 
void
init_device_defaults(dinfo_t *dip)
{
    /* Setup Defaults */
    dip->di_fd = NoFd;
    dip->di_funcs = NULL;
    dip->di_shared_file = False;
    /* Note: These may be pipes/redirected. */
    dip->di_efp = efp; // = stderr;
    dip->di_ofp = ofp; // = stdout;
    dip->di_async_job = False;
    dip->di_btag_flag = False;
    dip->di_data_limit = INFINITY;
    dip->di_max_limit = 0;
    dip->di_min_limit = 0;
    dip->di_incr_limit = 0;
    dip->di_user_limit = 0;
    dip->di_variable_limit = False;
    dip->di_record_limit = 0;
    dip->di_runtime = 0;

    /* Initialize block size variables: */
    dip->di_block_size = BLOCK_SIZE;
    dip->di_iblock_size = dip->di_oblock_size = 0;
    dip->di_file_position = (Offset_t)0;
    dip->di_ofile_position = (Offset_t)0;
    dip->di_min_size = 0;
    dip->di_max_size = 0;
    dip->di_step_offset = (Offset_t)0;

    /* Initialize the file lock defaults: */
    dip->di_lock_files = DEFAULT_LOCK_TEST;
    dip->di_lock_mode = DEFAULT_LOCK_MODE;
    dip->di_lock_mode_name = DEFAULT_LOCK_MODE_NAME;
    dip->di_unlock_chance = DEFAULT_UNLOCK_CHANCE;

    dip->di_num_procs = 0;
    dip->di_num_devs = 1;
    dip->di_device_number = 0;
    dip->di_slices = 0;
    dip->di_slice_number = 0;
    dip->di_threads = 1;
    dip->di_threads_active = 0;
    dip->di_initial_state = IS_RUNNING;

    dip->di_compare_flag = DEFAULT_COMPARE_FLAG;
    dip->di_xcompare_flag = DEFAULT_XCOMPARE_FLAG;
    dip->di_force_core_dump = DEFAULT_COREDUMP_FLAG;
    dip->di_force_corruption = False;
    dip->di_corrupt_index = UNINITIALIZED;
    dip->di_corrupt_length = sizeof(CORRUPTION_PATTERN);
    dip->di_corrupt_pattern = CORRUPTION_PATTERN;
    dip->di_corrupt_step = 0;
    dip->di_corrupt_reads = CORRUPT_READ_RECORDS;
    dip->di_corrupt_writes = CORRUPT_WRITE_RECORDS;
    dip->di_fileperthread = DEFAULT_FILEPERTHREAD;
    dip->di_lbdata_flag = DEFAULT_LBDATA_FLAG;
    dip->di_timestamp_flag = DEFAULT_TIMESTAMP_FLAG;
    dip->di_user_pattern = DEFAULT_USER_PATTERN;

#if defined(SCSI)
    dip->di_fua = False;
    dip->di_dpo = False;
    dip->di_scsi_flag = DEFAULT_SCSI_FLAG;
    dip->di_scsi_info_flag = DEFAULT_SCSI_INFO_FLAG;
    dip->di_scsi_io_flag = DEFAULT_SCSI_IO_FLAG;
    dip->di_scsi_errors = DEFAULT_SCSI_ERRORS;
    dip->di_scsi_sense = DEFAULT_SCSI_SENSE;
    dip->di_scsi_recovery = ScsiRecoveryFlagDefault;
    dip->di_scsi_recovery_delay = ScsiRecoveryDelayDefault;
    dip->di_scsi_recovery_limit = ScsiRecoveryRetriesDefault;
    dip->di_scsi_read_type = ScsiReadTypeDefault;
    dip->di_scsi_write_type = ScsiWriteTypeDefault;
    dip->di_unmap_type = UNMAP_TYPE_NONE;
    dip->di_unmap_flag = False;
    dip->di_get_lba_status_flag = True;
    dip->di_idt = IDT_BOTHIDS;
#else /* !defined(SCSI) */
    dip->di_scsi_flag = False;
    dip->di_scsi_io_flag = False;
#endif /* defined(SCSI) */
    /* Always define these to ease code conditionalization! */
    dip->di_nvme_flag = False;
    dip->di_nvme_io_flag = False;

    dip->di_verbose_flag = DEFAULT_VERBOSE_FLAG;
    dip->di_verify_flag = DEFAULT_VERIFY_FLAG;
    dip->di_unique_pattern = DEFAULT_UNIQUE_PATTERN;
    dip->di_error_count = 0;
    dip->di_error_limit = DEFAULT_ERROR_LIMIT;
    dip->di_file_limit = DEFAULT_FILE_LIMIT;
    dip->di_pass_limit = DEFAULT_PASS_LIMIT;
    dip->di_user_subdir_limit = 0;
    dip->di_user_subdir_depth = 0;

    dip->di_iotuning_flag = DEFAULT_IOTUNE_FLAG;
    dip->di_iot_pattern = False;
    dip->di_iot_seed = IOT_SEED;
    dip->di_pattern = DEFAULT_PATTERN;

#if defined(AIO)
    /* Asynchronous I/O */
    dip->di_aio_bufs = AIO_BUFS;
#endif /* !defined(AIO) */
    dip->di_aio_flag = False;
    dip->di_align_offset = 0;
    
    dip->di_dumpall_flag = False;
    dip->di_dump_context_flag = True;
    dip->di_max_bad_blocks = MAXBADBLOCKS;
    dip->di_boff_format = HEX_FMT;
    dip->di_data_format = NONE_FMT;
    dip->di_bufmode_index = 0;
    dip->di_bufmode_count = 0;
    dip->di_buffer_mode = NONE_SPECIFIED;
    
    dip->di_dump_limit = DEFAULT_DUMP_LIMIT;
    dip->di_bypass_flag = False;
    dip->di_cerrors_flag = True;
    dip->di_child_flag = False;
    dip->di_debug_flag = False;
    dip->di_Debug_flag = False;
    dip->di_eDebugFlag = False;
    dip->di_fDebugFlag = False;
    dip->di_pDebugFlag = False;
    dip->di_rDebugFlag = False;
    dip->di_sDebugFlag = False;
    dip->di_tDebugFlag = False;
    dip->di_timerDebugFlag = False;
    dip->di_delete_per_pass = False;
    dip->di_dio_flag = False;
    /* File system caching is enabled by default, these control each. */
    dip->di_read_cache_flag = True;
    dip->di_write_cache_flag = True;
    dip->di_dump_flag = True;
    dip->di_errors_flag = False;
    dip->di_fill_always = False;
    dip->di_fill_once = UNINITIALIZED;
    dip->di_forked_flag = False;
    dip->di_fsincr_flag = False;
    dip->di_fsync_flag = UNINITIALIZED;
    dip->di_fsync_frequency = 0;
    dip->di_fsalign_flag = False;
    dip->di_fsfile_flag = False;
    dip->di_dir_created = False;
    dip->di_topdir_created = False;
    dip->di_multiple_dirs = False;
    dip->di_multiple_files = False;
    dip->di_keep_existing = True;
    FreeHistoryData(dip);
    dip->di_history_size = 0;
    dip->di_history_dump = False;
    dip->di_history_timing = False;
    dip->di_history_bufs = DEFAULT_HISTORY_BUFFERS;
    dip->di_history_data_size = DEFAULT_HISTORY_DATA_SIZE;
    dip->di_mount_lookup = DEFAULT_MOUNT_LOOKUP;
    dip->di_noprog_flag = False;
    dip->di_noprogtime = 0;
    dip->di_noprogttime = 0;
    dip->di_poison_buffer = DEFAULT_POISON_FLAG;
    dip->di_prefill_buffer = DEFAULT_PREFILL_FLAG;
    dip->di_unique_log = False;
    dip->di_unique_file = False;
    dip->di_user_incr = False;
    dip->di_user_min = False;
    dip->di_user_max = False;
    dip->di_user_ralign = False;
    dip->di_user_rseed = False;
    dip->di_user_lbdata = False;
    dip->di_user_lbsize = False;
    dip->di_user_position = False;
    dip->di_user_oposition = False;
    dip->di_incr_pattern = False;
    dip->di_logappend_flag = False;
    dip->di_logdiag_flag = False;
    dip->di_logpid_flag = False;
    dip->di_stop_immediate = True;
    dip->di_syslog_flag = False;
    dip->di_loop_on_error = False;
    dip->di_mmap_flag = False;
    dip->di_media_changed = False;
    /* This are user specified open flags! */
    dip->di_open_flags = 0;
    dip->di_write_flags = 0;
    dip->di_read_mode = OS_READONLY_MODE;
    dip->di_write_mode = OS_WRITEONLY_MODE;
    dip->di_rwopen_mode = OS_READWRITE_MODE;
    dip->di_pad_check = True;
    dip->di_spad_check = False;
    dip->di_raw_flag = False;
    dip->di_reread_flag = False;
    dip->di_rotate_flag = False;
    dip->di_rotate_offset = 0;
    dip->di_prealloc_flag = True;
    dip->di_sparse_flag = True;
    dip->di_stats_flag = True;
    if (dip->di_dirprefix) {
	FreeStr(dip, dip->di_dirprefix);
    }
    dip->di_dirprefix = strdup(DIR_PREFIX);
#if defined(HP_UX)
    dip->di_qdepth = 0xFFFFFFFF;
#endif
    dip->di_iops = 0;
    dip->di_retry_delay = RETRY_DELAY;
    dip->di_retry_limit = RETRY_LIMIT;
    dip->di_retry_entries = 0;
    dip->di_retry_disconnects = False;
    dip->di_retry_warning = False;
    dip->di_open_delay = DEFAULT_OPEN_DELAY;
    dip->di_close_delay = DEFAULT_CLOSE_DELAY;
    dip->di_read_delay = DEFAULT_READ_DELAY;
    dip->di_verify_delay = DEFAULT_VERIFY_DELAY;
    dip->di_write_delay = DEFAULT_WRITE_DELAY;
    dip->di_start_delay = DEFAULT_START_DELAY;
    dip->di_delete_delay = DEFAULT_DELETE_DELAY;
    dip->di_fsfree_delay = DEFAULT_FSFREE_DELAY;
    dip->di_fsfree_retries = DEFAULT_FSFREE_RETRIES;
    dip->di_end_delay = DEFAULT_END_DELAY;
    dip->di_term_delay = DEFAULT_TERM_DELAY;
    dip->di_term_wait_time = THREAD_MAX_TERM_TIME;
    dip->di_retryDC_flag = True;
    dip->di_retryDC_delay = RETRYDC_DELAY;
    dip->di_retryDC_limit = RETRYDC_LIMIT;
    dip->di_save_corrupted = SAVE_CORRUPTED;
    dip->di_max_capacity = False;
    dip->di_user_capacity = (large_t)0;
    
    /* File System Parameters */
    dip->di_fsmap_flag = True;
    dip->di_fs_block_size = 0;
    dip->di_fs_space_free = 0;
    dip->di_fs_total_space = 0;
    dip->di_fsmap_type = FSMAP_TYPE_NONE;

    dip->di_multi_flag = False;
    dip->di_multi_volume = 1;
    dip->di_volumes_flag = False;
    dip->di_volume_limit = 0;
    dip->di_volume_records = 1;
    
    dip->di_iobf = NULL;
    dip->di_iobehavior = DT_IO;
    dip->di_io_dir  = FORWARD;
    dip->di_vary_iodir = False;
    dip->di_vary_iotype = False;
    dip->di_io_mode = TEST_MODE;
    dip->di_io_type = SEQUENTIAL_IO;
    dip->di_dispose_mode = KEEP_ON_ERROR;
    dip->di_oncerr_action = ONERR_CONTINUE;
    dip->di_stats_level = STATS_FULL;

    dip->di_max_data_percentage = 0;
    dip->di_max_data = 0;
    dip->di_max_files = 0;
    
    /* Initialize random I/O variables: */
    dip->di_rdata_limit = 0;
    dip->di_random_align = 0;
    dip->di_random_io = False;
    dip->di_random_seed = 0;
    dip->di_read_percentage = 0;
    dip->di_random_percentage = 0;
    dip->di_random_rpercentage = 0;
    dip->di_random_wpercentage = 0;
    dip->di_variable_flag = False;
    dip->di_variable_limit = False;
    
    dip->di_trigargs_flag = True;
    dip->di_trigdelay_flag = True;
    dip->di_trigdefaults_flag = True;
    remove_triggers(dip);
    /* Note: May not be needed anymore. */
    if (dip->di_mtrand) {
	dip->di_mtrand->mti = (NN + 1);	/* Not initialized value. */
    }
    return;
}

/*
 * cleanup_device() - Free Space Allocated for a Device.
 * 
 * Inputs:
 * 	dip = The device information pointer.
 * 	master = The master device, so limit cleanup.
 * 
 * Returns:
 * 	void
 */ 
void
cleanup_device(dinfo_t *dip, hbool_t master)
{
    if (dip->di_debug_flag) {
	Printf(NULL, "Cleaning up device "LLPXFMT", master %d...\n",  dip, master);
    }
    if (dip->di_output_dinfo) {
	dinfo_t *odip = dip->di_output_dinfo;
	dip->di_output_dinfo = NULL;
	odip->di_job = NULL;
	odip->di_output_dinfo = NULL;	/* Avoid recursion! */
	cleanup_device(odip, False);
	odip->di_output_dinfo = odip;
#if defined(SCSI)
	free_scsi_info(odip, &odip->di_sgp, &odip->di_sgpio);
	if (odip->di_tsgp) {
	    free_scsi_info(odip, &odip->di_tsgp, NULL);
	}
#endif /* defined(SCSI) */
    }
    /*
     * Note: This should be done by each I/O thread!
     */ 
    if (dip->di_fd != NoFd) {
	if (dip->di_shared_file == False) {
	    (void)(*dip->di_funcs->tf_close)(dip);
	}
	if ( dip->di_output_file && dip->di_fsfile_flag	&&
	     (dip->di_io_mode == TEST_MODE)		&&
	     (dip->di_dispose_mode == DELETE_FILE) ) {
	    (void)delete_files(dip, True);
	}
    }
    if (dip->di_array) {
	FreeStr(dip, dip->di_array);
	dip->di_array = NULL;
    }
    if ((master == False) && dip->di_file_sep) {
	FreeStr(dip, dip->di_file_sep);
	dip->di_file_sep = NULL;
    }
    if ((master == False) && dip->di_file_postfix) {
	FreeStr(dip, dip->di_file_postfix);
	dip->di_file_postfix = NULL;
    }
    if (dip->di_dir) {
	FreeStr(dip, dip->di_dir);
	dip->di_dir = NULL;
    }
    if (dip->di_dirpath) {
	FreeStr(dip, dip->di_dirpath);
	dip->di_dirpath = NULL;
    }
    if ((master == False) && dip->di_dirprefix) {
	FreeStr(dip, dip->di_dirprefix);
	dip->di_dirprefix = NULL;
    }
    if (dip->di_topdirpath) {
	FreeStr(dip, dip->di_topdirpath);
	dip->di_topdirpath = NULL;
    }
    if (dip->di_input_file) {
	FreeStr(dip, dip->di_input_file);
	dip->di_input_file = NULL;
    }
    if (dip->di_output_file) {
	FreeStr(dip, dip->di_output_file);
	dip->di_output_file = NULL;
    }
    if (dip->di_dname) {
	FreeStr(dip, dip->di_dname);
	dip->di_dname = NULL;
    }
    if (dip->di_bname) {
	FreeStr(dip, dip->di_bname);
	dip->di_bname = NULL;
    }
    if (dip->di_job_tag) {
	FreeStr(dip, dip->di_job_tag);
	dip->di_job_tag = NULL;
    }
    if (dip->di_log_dir) {
	FreeStr(dip, dip->di_log_dir);
	dip->di_log_dir = NULL;
    }
    if (dip->di_log_format) {
	FreeStr(dip, dip->di_log_format);
	dip->di_log_format = NULL;
    }
#if 0
    if ((master == False) && dip->di_msg_buffer) {
	Free(dip, dip->di_msg_buffer);
	dip->di_msg_buffer = NULL;
    }
#endif /* 0 */
    if (dip->di_pattern_buffer) {
	reset_pattern(dip);
    }
    if ((master == False) && dip->di_stderr_buffer) {
	Free(dip, dip->di_stderr_buffer);
	dip->di_stderr_buffer = NULL;
    }
    if (dip->di_btag) {
	FreeMem(dip, dip->di_btag, sizeof(*dip->di_btag));
	dip->di_btag = NULL;
    }
    if (dip->di_data_buffer) {
#if defined(AIO)
	/* Note: 1st AIO buffer used for data buffer is freed below! */
	if (dip->di_aio_flag == False) {
	    free_palign(dip, dip->di_base_buffer);
	}
#else /* !defined(AIO) */
	free_palign(dip, dip->di_base_buffer);
#endif /* defined(AIO) */
	dip->di_base_buffer = dip->di_data_buffer = NULL;
    }
    if (dip->di_verify_buffer) {
	free_palign(dip, dip->di_verify_buffer);
	dip->di_verify_buffer = NULL;
    }
    if (dip->di_cmd_line) {
	FreeStr(dip, dip->di_cmd_line);
	dip->di_cmd_line = NULL;
    }
    if (dip->di_dtcmd) {
	FreeStr(dip, dip->di_dtcmd);
	dip->di_dtcmd = NULL;
    }
    if (dip->di_prefix_string) {
	FreeStr(dip, dip->di_prefix_string);
	dip->di_prefix_string = NULL;
    }
    if (dip->di_fprefix_string) {
	FreeStr(dip, dip->di_fprefix_string);
	dip->di_fprefix_string = NULL;
    }
    if (dip->di_uuid_string) {
	FreeStr(dip, dip->di_uuid_string);
	dip->di_uuid_string = NULL;
    }
    if (dip->di_workload_name) {
	FreeStr(dip, dip->di_workload_name);
	dip->di_workload_name = NULL;
    }
    if (dip->di_mounted_from_device) {
	FreeStr(dip, dip->di_mounted_from_device);
	dip->di_mounted_from_device = NULL;
    }
    if (dip->di_mounted_on_dir) {
	FreeStr(dip, dip->di_mounted_on_dir);
	dip->di_mounted_on_dir = NULL;
    }
    if (dip->di_filesystem_type) {
	FreeStr(dip, dip->di_filesystem_type);
	dip->di_filesystem_type = NULL;
    }
    if (dip->di_filesystem_options) {
	FreeStr(dip, dip->di_filesystem_options);
	dip->di_filesystem_options = NULL;
    }
    if (dip->di_fsmap) {
	os_free_file_map(dip);
    }
    if (dip->di_protocol_version) {
	FreeStr(dip, dip->di_protocol_version);
	dip->di_protocol_version = NULL;
    }
    if (dip->di_universal_name) {
	FreeStr(dip, dip->di_universal_name);
	dip->di_universal_name = NULL;
    }
    if (dip->di_volume_name) {
	FreeStr(dip, dip->di_volume_name);
	dip->di_volume_name = NULL;
    }
    if (dip->di_volume_path_name) {
	FreeStr(dip, dip->di_volume_path_name);
	dip->di_volume_path_name = NULL;
    }
#if defined(SCSI)
    free_scsi_info(dip, &dip->di_sgp, &dip->di_sgpio);
    if (dip->di_tsgp) {
	free_scsi_info(dip, &dip->di_tsgp, NULL);
    }
#endif /* defined(SCSI) */
    if (dip->di_pass_cmd) {
	FreeStr(dip, dip->di_pass_cmd);
	dip->di_pass_cmd = NULL;
    }
    if (dip->di_pattern_file) {
	FreeStr(dip, dip->di_pattern_file);
	dip->di_pattern_file = NULL;
    }
    if (dip->di_pattern_string) {
	FreeStr(dip, dip->di_pattern_string);
	dip->di_pattern_string = NULL;
    }
    if (dip->di_stop_on_file) {
	FreeStr(dip, dip->di_stop_on_file);
	dip->di_stop_on_file = NULL;
    }
    if (dip->di_keepalive) {
	FreeStr(dip, dip->di_keepalive);
	dip->di_keepalive = NULL;
    }
    if (dip->di_pkeepalive) {
	FreeStr(dip, dip->di_pkeepalive);
	dip->di_pkeepalive = NULL;
    }
    if (dip->di_tkeepalive) {
	FreeStr(dip, dip->di_tkeepalive);
	dip->di_tkeepalive = NULL;
    }
    FreeHistoryData(dip);
#if defined(AIO)
    dtaio_free_buffers(dip);
#endif /* defined(AIO) */
    /*
     * Trigger scripts and arguments:
     */
    remove_triggers(dip);

    if ((master == False) && dip->di_mtrand) {
	Free(dip, dip->di_mtrand);
	dip->di_mtrand = NULL;
    }

    /*
     * Do tool specific cleanup:
     */
    if (dip->di_iobf && dip->di_iobf->iob_cleanup) {
	(*dip->di_iobf->iob_cleanup)(dip);
    }

    if (dip->di_job_log) {
	FreeStr(dip, dip->di_job_log);
	dip->di_job_log = NULL;
    }
    /* 
     * Note: Done last, since may be writing to master log file. 
     */
    if (dip->di_log_file) {
	if (dip->di_log_opened == True) {
	    if ( fclose(dip->di_efp) != SUCCESS) {
		Perror(dip, "fclose() of %s failed...\n", dip->di_log_file);
	    }
	    if (master == True) {
		dip->di_ofp = stdout;
		dip->di_efp = stderr;
	    }
	}
	FreeStr(dip, dip->di_log_file);
	dip->di_log_file = NULL;
	dip->di_log_opened = False;
    }
    /* Note: Obviously, we cannot use this hereafter! */
    /* Note: This may need to move, if print functions use it! */
    if ((master == False) && dip->di_log_buffer) {
	FreeMem(dip, dip->di_log_buffer, dip->di_log_bufsize);
	dip->di_log_buffer = dip->di_log_bufptr = NULL;
    }
    /* Note: Do this last, so debug prefix is correct. */
    if ((master == False) && dip->di_log_prefix) {
	FreeStr(dip, dip->di_log_prefix);
	dip->di_log_prefix = NULL;
    }
    return;
}

dinfo_t *
clone_device(dinfo_t *dip, hbool_t master, hbool_t new_context)
{
    dinfo_t *cdip;
    int i, status;

    if (dip->di_debug_flag) {
	/* TODO: Clean this up! */
	if (dip->di_dname) {
	    Printf(NULL, "Cloning device "LLPXFMT" - %s...\n",  dip, dip->di_dname);
	} else {
	    Printf(NULL, "Cloning device "LLPXFMT"\n",  dip);
	}
    }
    cdip = (struct dinfo *)Malloc(dip, sizeof(*dip));
    if (cdip == NULL) return(NULL);
    *cdip = *dip;

    if (master == False) {
	dip->di_sequence = 0;		/* This is per thread. */
    }

    /* Clone the output device (for copy), if any. */
    if (dip->di_output_dinfo) {
	 /* Note: Clearly messy... clone output devices separately! */
	dinfo_t *odip = dip->di_output_dinfo;
	odip->di_job = dip->di_job;
	odip->di_output_dinfo = NULL;	/* Avoid recursion! */
	cdip->di_output_dinfo = NULL;
	cdip->di_output_dinfo = clone_device(odip, False, new_context);
	odip->di_output_dinfo = odip;
    }
    if (dip->di_shared_file == False) {
	cdip->di_fd = NoFd;
    }

    /* 
     * Note: If the master open'ed a log file, then reset stdout/stderr (for now).
     * Reason: We don't wish to fclose() the same file, leading to memory corruption!
     */ 
    if ( (master == True) && dip->di_log_file) {
	cdip->di_ofp = stdout;
	cdip->di_efp = stderr;
    }
    if (dip->di_array) {
	cdip->di_array = strdup(dip->di_array);
    }
    if (dip->di_file_sep) {
	cdip->di_file_sep = strdup(dip->di_file_sep);
    }
    if (dip->di_file_postfix) {
	cdip->di_file_postfix = strdup(dip->di_file_postfix);
    }
    if (dip->di_dir) {
	cdip->di_dir = strdup(dip->di_dir);
    }
    if (dip->di_dirpath) {
	cdip->di_dirpath = strdup(dip->di_dirpath);
    }
    if (dip->di_dirprefix) {
	cdip->di_dirprefix = strdup(dip->di_dirprefix);
    }
    if (dip->di_topdirpath) {
	cdip->di_topdirpath = strdup(dip->di_topdirpath);
    }
    if (dip->di_input_file) {
	cdip->di_input_file = strdup(dip->di_input_file);
    }
    if (dip->di_output_file) {
	cdip->di_output_file = strdup(dip->di_output_file);
    }
    if (dip->di_dname) {
	cdip->di_dname = strdup(dip->di_dname);
    }
    if (dip->di_bname) {
	cdip->di_bname = strdup(dip->di_bname);
    }
    if (dip->di_job_tag) {
	cdip->di_job_tag = strdup(dip->di_job_tag);
    }
    if (dip->di_job_log) {
	cdip->di_job_log = strdup(dip->di_job_log);
    }
    if (dip->di_log_dir) {
	cdip->di_log_dir = strdup(dip->di_log_dir);
    }
    if (dip->di_log_file) {
	cdip->di_log_file = strdup(dip->di_log_file);
    }
    if (dip->di_log_format) {
	cdip->di_log_format = strdup(dip->di_log_format);
    }
    if (dip->di_log_prefix) {
	cdip->di_log_prefix = strdup(dip->di_log_prefix);
    }
    if (dip->di_log_buffer) {
	cdip->di_log_buffer = Malloc(dip, dip->di_log_bufsize);
	cdip->di_log_bufptr = cdip->di_log_buffer;
    }
#if 0
    if (dip->di_msg_buffer) {
	cdip->di_msg_buffer = (char *)Malloc(dip, dip->di_log_bufsize);
    }
#endif /* 0 */
    if (dip->di_pattern_buffer) {
	uint8_t *buffer = malloc_palign(dip, dip->di_pattern_bufsize, 0);
	memcpy(buffer, dip->di_pattern_buffer, dip->di_pattern_bufsize);
	setup_pattern(cdip, buffer, dip->di_pattern_bufsize, True);
    }
    if (dip->di_stderr_buffer) {
	/* Note: Only master needs to set stderr buffered! */
	//cdip->di_stderr_buffer = Malloc(dip, dip->di_log_bufsize);
	cdip->di_stderr_buffer = NULL;
    }
    if (dip->di_btag) {
	/* Note: This gets allocated after threads start! */
	cdip->di_btag = NULL;
    }
    if (dip->di_base_buffer) {
	/* These will get allocated during initialization. */
	cdip->di_base_buffer = cdip->di_data_buffer = NULL;
	if (master == False) {
#if defined(AIO)
	    /* New AIO buffers please! */
	    cdip->di_acbs = NULL;
#endif /* defined(AIO) */
	    /* Note: For AIO, this allocates data buffers! */
	    status = (*cdip->di_funcs->tf_initialize)(cdip);
	}
    }
    if (dip->di_verify_buffer) {
	cdip->di_verify_buffer = malloc_palign(dip, dip->di_verify_buffer_size, dip->di_align_offset);
    }
    if (dip->di_cmd_line) {
	cdip->di_cmd_line = strdup(dip->di_cmd_line);
    }
    if (dip->di_dtcmd) {
	cdip->di_dtcmd = strdup(dip->di_dtcmd);
    }
    if (dip->di_prefix_string) {
	cdip->di_prefix_string = strdup(dip->di_prefix_string);
    }
    /* Note: The formatted prefix string is setup in each thread. */
    if (dip->di_fprefix_string) {
	cdip->di_fprefix_size = 0;
	cdip->di_fprefix_string = NULL;
    }
    if (dip->di_uuid_string) {
	/* Note: This will be unique per thread! */
	cdip->di_uuid_string = NULL;
    }
    if (dip->di_workload_name) {
	cdip->di_workload_name = strdup(dip->di_workload_name);
    }
    if (dip->di_mounted_from_device) {
	cdip->di_mounted_from_device = strdup(dip->di_mounted_from_device);
    }
    if (dip->di_mounted_on_dir) {
	cdip->di_mounted_on_dir = strdup(dip->di_mounted_on_dir);
    }
    if (dip->di_filesystem_type) {
	cdip->di_filesystem_type = strdup(dip->di_filesystem_type);
    }
    if (dip->di_filesystem_options) {
	cdip->di_filesystem_options = strdup(dip->di_filesystem_options);
    }
    if (dip->di_protocol_version) {
	cdip->di_protocol_version = strdup(dip->di_protocol_version);
    }
    if (dip->di_universal_name) {
	cdip->di_universal_name = strdup(dip->di_universal_name);
    }
    if (dip->di_volume_name) {
	cdip->di_volume_name = strdup(dip->di_volume_name);
    }
    if (dip->di_volume_path_name) {
	cdip->di_volume_path_name = strdup(dip->di_volume_path_name);
    }
#if defined(SCSI)
    clone_scsi_info(dip, cdip);
#endif /* defined(SCSI) */
    if (dip->di_pass_cmd) {
	cdip->di_pass_cmd = strdup(dip->di_pass_cmd);
    }
    if (dip->di_pattern_file) {
	cdip->di_pattern_file = strdup(dip->di_pattern_file);
    }
    if (dip->di_pattern_string) {
	cdip->di_pattern_string = strdup(dip->di_pattern_string);
    }
    if (dip->di_stop_on_file) {
	cdip->di_stop_on_file = strdup(dip->di_stop_on_file);
    }
    if (dip->di_keepalive) {
	cdip->di_keepalive = strdup(dip->di_keepalive);
    }
    if (dip->di_pkeepalive) {
	cdip->di_pkeepalive = strdup(dip->di_pkeepalive);
    }
    if (dip->di_tkeepalive) {
	cdip->di_tkeepalive = strdup(dip->di_tkeepalive);
    }
    /*
     * History Data: 
     */ 
    if (dip->di_history_size) {
	SetupHistoryData(cdip);
    }
    /*
     * Trigger scripts and arguments:
     */ 
    for (i = 0; (i < dip->di_num_triggers); i++) {
	cdip->di_triggers[i].td_trigger = dip->di_triggers[i].td_trigger;
	if (dip->di_triggers[i].td_trigger_cmd) {
	    cdip->di_triggers[i].td_trigger_cmd = strdup(dip->di_triggers[i].td_trigger_cmd);
	}
	if (dip->di_triggers[i].td_trigger_args) {
	    cdip->di_triggers[i].td_trigger_args = strdup(dip->di_triggers[i].td_trigger_args);
	}
    }

    if ((master == False) && dip->di_mtrand) {
	cdip->di_mtrand = NULL;		/* Force realloc/reinit if used. */
    }

    /*
     * Do tool specific cloning:
     */
    if (dip->di_iobf && dip->di_iobf->iob_clone) {
	(void)(*dip->di_iobf->iob_clone)(dip, cdip, new_context);
    }
    /* Note: This allows elapsed time to be reported early properly! */
    cdip->di_start_time = times(&cdip->di_stimes);
    gettimeofday(&cdip->di_start_timer, NULL);

    return(cdip);
}

/*
 * This function does validation of options for all I/O behaviours.
 * The expectation is each I/O behavior will have its' own validation.
 */ 
int
do_common_validate(dinfo_t *dip)
{
    int status = SUCCESS;
    char *devs;

    if ( (dip->di_input_file == NULL) && (dip->di_output_file == NULL) ) {
	Eprintf(dip, "You must specify an input file, an output file, or both.\n");
	return(FAILURE);
    }

    /*
     * Disallow both seek type options, to simplify test loops.
     */
    if ( (dip->di_vary_iodir == False) && (dip->di_vary_iotype == False) &&
	 (dip->di_io_dir == REVERSE) && (dip->di_io_type == RANDOM_IO) ) {
	Eprintf(dip, "Please specify one of iodir=reverse or iotype=random, not both!\n");
	return(FAILURE);
    }

    devs = (dip->di_input_file) ? dip->di_input_file : dip->di_output_file;
    if ( EQS(devs, ",") ) {
	dip->di_multiple_devs = True;
    } else {
	dip->di_multiple_devs = False;
    }

    /*
     * Special retry handling moved here from parser to avoid option ordering!
     */
    if (dip->di_retry_entries && (dip->di_user_errors == False)) {
	/*
	 * Retries are normally logged as errors, so in order to keep I/O
         * loops from terminating on retries, we adjust the error limit. 
         * Note: Retaining this for historic reasons, backwards compatible.
	 */
	if ( dip->di_error_limit < dip->di_retry_limit ) {
	    /* Note Windows session disconnects are logged as warnings. */
	    if ( (dip->di_retry_warning == False) &&
		 (dip->di_retry_disconnects == False) ) {
                if (dip->di_verbose_flag) {
                    Wprintf(dip, "Setting the error limit to the retry limit of %u.\n",
                            dip->di_retry_limit);
                }
		dip->di_error_limit = dip->di_retry_limit;
	    }
	}
    }
    return (status);
}

int
do_datatest_initialize(dinfo_t *dip)
{
    int status = SUCCESS;

    /*
     * Process the pattern file (if one was specified).
     */
    if (dip->di_pattern_file) {
	status = process_pfile(dip, dip->di_pattern_file);
	if (status != SUCCESS) {
	    return(status);
	}
    }

    /*
     * Do special handling of IOT data pattern.
     */
    if (dip->di_iot_pattern == True) {
	size_t size = dip->di_block_size;
	uint8_t *buffer = malloc_palign(dip, size, 0);
	if (buffer == NULL) return(FAILURE);
	setup_pattern(dip, buffer, size, True);
	dip->di_pattern_string = strdup("IOT Pattern");
	/*
	 * When variable I/O sizes are requested, align sizes to device size
	 * instead of pattern buffer size else max transfer size is used!
	 */
	dip->di_fsalign_flag = True;
	/* IOT takes precedence! */
	dip->di_lbdata_flag = False;
	dip->di_user_lbdata = False;
    }

    /*
     * Setup the pattern as a pattern string, so non-modulo
     * sizeof(uint32_t) read counts will compare data properly.
     * Note: This assumes the default 32-bit data pattern.
     */
    if (dip->di_pattern_buffer == NULL) {
	size_t size = sizeof(uint32_t);
	/* Note: Kind of a waste, but common to above. */
	uint8_t *buffer = malloc_palign(dip, size, 0);
	if (buffer == NULL) return(FAILURE);
	copy_pattern (dip->di_pattern, buffer);
	setup_pattern(dip, buffer, size, True);
    }
    return(status);
}

/*
 * Note: This is invoked with the master device, so don't allocate buffers!
 */
int
do_datatest_validate(dinfo_t *dip)
{
    int status = SUCCESS;

    if (dip->di_iobehavior == DT_IO) {
        /* Switch output file to input file for 100% reads! (for automation) */
	if ( (dip->di_read_percentage == 100) &&
	     dip->di_output_file && (dip->di_input_file == NULL) ) {
	    dip->di_input_file = dip->di_output_file;
	    dip->di_output_file = NULL;
	    dip->di_read_percentage = 0;
	}
	if ( ((dip->di_aio_flag == True) || (dip->di_mmap_flag == True)) &&
	      (dip->di_read_percentage || dip->di_random_percentage ||
	       dip->di_random_rpercentage || dip->di_random_wpercentage) ) {
	    Wprintf(dip, "Percentage options are NOT support with AIO/MMAP I/O, so disabling!\n");
	    dip->di_read_percentage = 0;
	    dip->di_random_percentage = 0;
	    dip->di_random_rpercentage = 0;
	    dip->di_random_wpercentage = 0;
	}
	if (dip->di_read_percentage || dip->di_random_wpercentage) {
	    dip->di_raw_flag = True; /* Force read/write access! */
	}
	if (dip->di_read_percentage || dip->di_random_percentage ||
	    dip->di_random_rpercentage || dip->di_random_wpercentage) {
	    dip->di_reread_flag = False;
	}
    }
    /* Don't reset the pattern buffer else lose pattern=incr, etc. */
    /* TODO: Is there a valid time to reset the pattern buffer? */
    /* do_datatest_initialize() will setup a 4 byte pattern! */
    //if (dip->di_pattern_buffer) reset_pattern(dip);
    /*
     * Set the default display data format, if not specified by the user.
     */
    if (dip->di_data_format == NONE_FMT) {
	dip->di_data_format = BYTE_FMT;
    }

    /*
     * Catch some of the pattern option conflicts, misleading users!
     */
    if ((dip->di_iot_pattern || dip->di_incr_pattern || dip->di_pattern_strsize) && dip->di_pattern_file) {
	Eprintf(dip, "Multiple pattern options selected, please choose only one!\n");
	return(FAILURE);
    }

    /*
     * Multiple Directory Sanity Checks:
     */
    dip->di_multiple_dirs = (dip->di_user_dir_limit || dip->di_user_subdir_limit || dip->di_user_subdir_depth);
    dip->di_multiple_files = (dip->di_multiple_dirs || dip->di_file_limit);

#if defined(WIN32)
    /* URL: http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx */
    /* Note: Windows supports both forward and backward slash in paths. */
    /* Switch to POSIX style path separator if user specified forward slash! */
    if ( (dip->di_input_file && strchr(dip->di_input_file, POSIX_DIRSEP)) ||
	 (dip->di_output_file && strchr(dip->di_output_file, POSIX_DIRSEP)) ) {
	dip->di_dir_sep = POSIX_DIRSEP;
    }
    if ( dip->di_dir && strchr(dip->di_dir, POSIX_DIRSEP) ) {
	dip->di_dir_sep = POSIX_DIRSEP;
    }
#endif /* defined(WIN32) */

    if ( dip->di_dir &&
	 ((dip->di_input_file && strrchr(dip->di_input_file, dip->di_dir_sep)) ||
	  (dip->di_output_file && strrchr(dip->di_output_file, dip->di_dir_sep))) ) {
	Eprintf(dip, "Please do not specify an if/of= directory path with dir= option!\n");
	return(FAILURE);
    }

    if (dip->di_slice_number) {
	if (!dip->di_slices) {
	    Eprintf(dip, "Please specify number of slices with slices=value option!\n");
	    return(FAILURE);
	} else if (dip->di_slice_number > dip->di_slices) {
	    Eprintf(dip, "Please specify slice (%d) <= max slices (%d)\n",
		    dip->di_slice_number, dip->di_slices);
	    return(FAILURE);
	}
    }
    /*
     * Slices becomes threads, unless a single slice was specified (one thread).
     */ 
    if (dip->di_slices && !dip->di_slice_number) {
	if (dip->di_threads > 1) {
	    Wprintf(dip, "The slices option (%d) overrides the threads (%d) specified!\n",
		    dip->di_slices, dip->di_threads);
	}
	dip->di_iolock = False;
	dip->di_threads = dip->di_slices;
    }
    /*
     * Multiple threads to the same device cannot use random I/O to avoid false corruptions. 
     * Random I/O with a write/read pass is only possible when the same random seed is used, 
     * but basically this doesn't make sense since each thread uses the same offset. ;( 
     * Lots of flags being checked, but I'm trying not to force improper behavior. ;) 
     */
    if ( (dip->di_iobehavior == DT_IO) && (dip->di_bypass_flag == False) &&
	 dip->di_iolock && (dip->di_threads > 1) && dip->di_output_file &&
	 dip->di_verify_flag && dip->di_compare_flag &&
	 (dip->di_io_type == RANDOM_IO) && (dip->di_user_rseed == False) ) {
	dip->di_raw_flag = True;
	dip->di_reread_flag = False;
	dip->di_read_percentage = 50;
	dip->di_random_percentage = 100;
    }

    if ( (dip->di_io_mode != TEST_MODE) &&
	 ( (dip->di_input_file == NULL) || (dip->di_output_file == NULL) ) ) {
	Eprintf(dip, "Copy/Mirror/Verify modes require both input and output devices.\n");
	return(FAILURE);
    }

    /*
     * When reading multiple files, don't require data or record limits (these will vary).
     * But when writing multiple tape files, we need to know how many records or bytes to be written.
     */
    if (dip->di_input_file && !dip->di_output_file && dip->di_file_limit && !dip->di_record_limit) {
	dip->di_record_limit = INFINITY;
    }

    /*
     * Check the variable record size parameters.
     */
    if (dip->di_min_size && !dip->di_max_size) dip->di_max_size = dip->di_block_size;
    if (dip->di_block_size < dip->di_max_size) dip->di_block_size = dip->di_max_size;
    if (dip->di_max_size) {
	if (dip->di_block_size > dip->di_max_size) dip->di_block_size = dip->di_max_size;
    }
    /* Set the block size accordingly if seperate read/write block sizes are specified. */
    dip->di_block_size = max(dip->di_block_size, dip->di_iblock_size);
    dip->di_block_size = max(dip->di_block_size, dip->di_oblock_size);

    /*
     * If specified, verify the variable data limits.
     */
    if ( dip->di_min_limit && dip->di_max_limit && (dip->di_max_limit < dip->di_min_limit) ) {
	Eprintf(dip, "The max limit "LUF", must be greater than the minimum limit "LUF"!\n",
		dip->di_max_limit, dip->di_min_limit);
	return(FAILURE);
    }
    /* NOTE: Other checks are done below now... */

    /*
     * Calculate the data limit if it wasn't specified by the user.
     *
     * Note: For random I/O options, don't set data limit (FindCapacity() will set).
     */
    if ( ((dip->di_random_io == False) || dip->di_slices) &&
	 (dip->di_data_limit == INFINITY) &&
	 ( (dip->di_record_limit != 0L) && (dip->di_record_limit != INFINITY) ) ) {
	dip->di_data_limit = (dip->di_block_size * dip->di_record_limit);
    }

    if (dip->di_min_size && (dip->di_max_size <= dip->di_min_size)) {
	Eprintf(dip, "Please specify max count > min count for record sizes.\n");
	return(FAILURE);
    }

    /*
     * If the pattern is unique, disable prefilling of buffer.
     */
    if (dip->di_btag_flag || dip->di_iot_pattern || dip->di_lbdata_flag) {
	if (dip->di_prefill_buffer == UNINITIALIZED) {
	    dip->di_prefill_buffer = False;
	}
    } else if (dip->di_prefill_buffer == UNINITIALIZED) {
	dip->di_prefill_buffer = True;
    }
    if ( (dip->di_io_mode != MIRROR_MODE) && (dip->di_io_mode != TEST_MODE) &&
	 (dip->di_btag_flag || dip->di_iot_pattern || dip->di_lbdata_flag || dip->di_prefix_string || dip->di_timestamp_flag) ) {
	Eprintf(dip, "btag, IOT, lbdata, prefix, & timestamp options disallowed with Copy/Verify modes!\n");
	return(FAILURE);
    }

    /* 
     * Verify Block Tag (btag) options (if enabled).
     */
    if (dip->di_btag_flag == True) {
	status = verify_btag_options(dip);
	if (status == FAILURE) return(status);
    }

    /*
     * We don't test loopback devices anymore, so automatically setup copy mode!
     */
    if ( (dip->di_input_file && dip->di_output_file) && (dip->di_io_mode == TEST_MODE) ) {
	dip->di_io_mode = COPY_MODE;
	dip->di_dispose_mode = KEEP_FILE;
    }

    /*
     * Ok, sanity check options that are allowing folks to shoot themselves
     * in the foot (sometimes due to hardcoded automation) when adding options
     * via a pass-through parameter.
     * 
     * Note: Done here to avoid multiple messages from each process.
     */ 
#if defined(O_TRUNC)
    if ( (dip->di_slices && (dip->di_write_flags & O_TRUNC)) ) {
	if (dip->di_verbose_flag) {
	    Wprintf(dip, "Disabling file truncate flag, not valid with multiple slices!\n");
	}
	dip->di_write_flags &= ~O_TRUNC;
    }
#endif /* defined(O_TRUNC) */
    if (dip->di_slices && dip->di_delete_per_pass) {
	if (dip->di_verbose_flag) {
	    Wprintf(dip, "Disabling delete per pass flag, not valid with multiple slices!\n");
	}
	dip->di_delete_per_pass = False;
    }
    return(status);
}

int
do_precopy_setup(dinfo_t *idip, dinfo_t *odip)
{
    /*
     * For file system copies, FindCapacity() fails if file doesn't exist,
     * but we need a capacity for multiple slices, so use source file size.
     */
    if (odip->di_user_capacity == 0) {
	odip->di_user_capacity = idip->di_user_capacity;
    }
    return(SUCCESS);
}

int
do_common_copy_setup(dinfo_t *idip, dinfo_t *odip)
{
    int status = SUCCESS;
    large_t src_data_limit;
    large_t dst_data_limit;

    if (idip->di_bypass_flag == True) return(status);
    if (idip->di_device_size != odip->di_device_size) return(status);
    /* Use the record limit if specified. */
    if ( (idip->di_record_limit != INFINITY) ||
	 (odip->di_record_limit != INFINITY) ) {
	return(status);
    }

    if ( (idip->di_data_limit == INFINITY) && (odip->di_data_limit == INFINITY) ) {
	src_data_limit = (idip->di_capacity * idip->di_device_size);
	dst_data_limit = (odip->di_capacity * odip->di_device_size);
    } else {
	/* Note: With current structure, these are always both the same! */
	src_data_limit = idip->di_data_limit;
	dst_data_limit = odip->di_data_limit;
    }
    if ( (idip->di_image_copy == True) &&
	 (src_data_limit > dst_data_limit) ) {
        Eprintf(idip, "The source device capacity is larger than the destination device!\n");
	Printf(idip, "     Source Device: %s, Capacity: " LUF " blocks\n", idip->di_input_file, idip->di_capacity);
	Printf(idip, "Destination Device: %s, Capacity: " LUF " blocks\n", odip->di_output_file, odip->di_capacity);
        return(FAILURE);
    }

    if ( src_data_limit > dst_data_limit ) {
	Printf(idip, "WARNING: The capacity is different between the selected devices!\n");
	Printf(idip, "     Source Device: %s, Capacity: " LUF " blocks\n", idip->di_input_file, idip->di_capacity);
	Printf(idip, "Destination Device: %s, Capacity: " LUF " blocks\n", odip->di_output_file, odip->di_capacity);
	if (True /*idip->di_slices*/) {
	    Printf(idip, "Setting both devices to the smallest capacity to ensure the same block ranges!\n");
	    idip->di_data_limit = min(src_data_limit, dst_data_limit);
	    odip->di_data_limit = min(src_data_limit, dst_data_limit);
	}
    }
    return(status);
}

/*
 * Please Beware: This function is invoked for ALL I/O behaviors right now.
 * Therefore, to bypass certain dt sanity checks, a data limit must be setup.
 * 
 * Note: Clearly, this function is overloaded, and replaced with an I/O behavior
 * specific function. Still a fair amount of cleanup required, but when? (sigh) 
 *  
 * Note: This function is invoked before I/O Behavior validate function. 
 */
int
do_common_device_setup(dinfo_t *dip)
{
    char *device = (dip->di_input_file) ? dip->di_input_file : dip->di_output_file;
    int status;

    /*
     * Please Note: I/O Behavior specific checks belong in their validate function!
     */

    /*
     * When doing random I/O, enable file system alignments, to help
     * prevent false corruptions.  This only affects regular files,
     * and only when read-after-write (raw) is disabled.  When raw
     * is enabled, we don't have to worry about data overwrites,
     * unless POSIX Async I/O (AIO) is enabled.
     */
    /*
     * Special file system handling.
     */
    if ( (dip->di_dtype && (dip->di_dtype->dt_dtype == DT_REGULAR)) &&
	 (dip->di_random_io == True) && (dip->di_bypass_flag == False) &&
	 ( (dip->di_raw_flag == False) ||
	   (dip->di_raw_flag && dip->di_reread_flag) || dip->di_aio_flag) ) {
	/* Note: Direct I/O & FS align is handled in dtgen.c validate_opts()! */
	if (dip->di_debug_flag || dip->di_Debug_flag) {
	    LogMsg(dip, dip->di_efp, logLevelWarn, 0,
		   "Enabling FS alignment for sizes and random offsets!\n");
	}
	dip->di_fsalign_flag = True;    /* Align FS sizes & random offsets. */
	/*
	 * Sanity check the pattern size is modulo the device size,
	 * otherwise this too will cause false failures w/random I/O.
	 */
	if (dip->di_pattern_bufsize && dip->di_dsize) {
	    if ( ((dip->di_pattern_bufsize > dip->di_dsize) && (dip->di_pattern_bufsize % dip->di_dsize)) ||
		 ((dip->di_pattern_bufsize < dip->di_dsize) && (dip->di_dsize % dip->di_pattern_bufsize)) ) {
		Eprintf(dip, "Please specify a pattern size (%u) modulo the device size (%u)!\n",
			dip->di_pattern_bufsize, dip->di_dsize);
		return(FAILURE);
	    }
	}
    }

    /*
     * Calculate the maxdata (for output), if percentage was specified.
     */
    if (dip->di_fsfile_flag && dip->di_output_file &&
	dip->di_fs_space_free && dip->di_max_data_percentage) {
	dip->di_max_data = (large_t)( (double)dip->di_fs_space_free *
				      ((double)dip->di_max_data_percentage / 100.0) );
	if (dip->di_threads > 1) {
	    dip->di_max_data /= dip->di_threads;    /* Divide space across threads. */
	}
	if (dip->di_max_data) {
	    /* This is important for direct I/O and IOT data pattern. */
	    dip->di_max_data = rounddown(dip->di_max_data, dip->di_device_size);
	    if (dip->di_fDebugFlag || dip->di_debug_flag) {
		Printf(dip, "Free space is "LUF", setting max data to "LUF" bytes.\n",
		       dip->di_fs_space_free, dip->di_max_data);
	    }
	}
    }

    /*
     * For file systems, lookup mount information and setup SCSI device (if any).
     */
    if (dip->di_fsfile_flag) {
	if (dip->di_mount_lookup) {
	    if (dip->di_dir) {
		(void)FindMountDevice(dip, dip->di_dir, dip->di_mntDebugFlag);
	    } else {
		(void)FindMountDevice(dip, device, dip->di_mntDebugFlag);
	    }
	    if (dip->di_mounted_from_device) {
		if (dip->di_debug_flag) {
		    Printf(dip, "Mounted from device: %s\n", dip->di_mounted_from_device);
		}
#if defined(__linux__)
		/* Note: Eventually, this may be required for other OS's! */
		if ( dip->di_dio_flag || (dip->di_bufmode_count != 0) ) {
		    /* Get device block size for DIO sanity checks later. */
		    os_get_block_size(dip, dip->di_fd, dip->di_mounted_from_device);
		}
#endif /* defined(__linux__) */
	    }
	    /* Note: For Copy/Verify, may wish to do both devices? */
	}
#if defined(SCSI)
	if (dip->di_scsi_flag && dip->di_mounted_from_device) {
	    /* Note: May need to massage DSF's for some OS's! */
	    if (strncmp(dip->di_mounted_from_device, DEV_DIR_PREFIX, DEV_DIR_LEN) == 0) {
		if (dip->di_scsi_dsf == NULL) {
#if defined(DEV_BDIR_LEN)
		    dip->di_scsi_dsf = ConvertBlockToRawDevice(dip->di_mounted_from_device);
#else /* !defined(DEV_BDIR_LEN) */
		    dip->di_scsi_dsf = ConvertDeviceToScsiDevice(dip->di_mounted_from_device);
#endif /* defined(DEV_BDIR_LEN) */
		}
	    }
	}
	/* Note: If not a SCSI device, disable SCSI write operations! */
	if (dip->di_scsi_dsf == NULL) dip->di_scsi_flag = False;
	if (dip->di_scsi_io_flag && dip->di_mounted_from_device && (dip->di_mode == WRITE_MODE) ) {
	    Eprintf(dip, "SCSI I/O is NOT permitted to a mounted file system!\n");
	    return(FAILURE);
	}
#endif /* defined(SCSI) */
    } else if (strncmp(device, DEV_PREFIX, sizeof(DEV_PREFIX)-1) == 0) { 
	if (dip->di_mount_lookup) {
	    if ( isDeviceMounted(dip, device, dip->di_mntDebugFlag) == True ) {
		if (dip->di_output_file) {
		    Eprintf(dip, "Device %s is mounted on %s, writing disallowd!\n",
			    device, dip->di_mounted_on_dir);
		    return(FAILURE);
		}
	    }
	}
	if (dip->di_debug_flag && dip->di_mounted_from_device) {
	    Printf(dip, "Device %s is mounted on %s\n", device, dip->di_mounted_on_dir);
	}
	if ( (dip->di_io_mode == COPY_MODE) && (dip->di_ftype == INPUT_FILE) ) {
	    dinfo_t *odip = dip->di_output_dinfo;
	    char *odevice = (odip->di_input_file) ? odip->di_input_file : odip->di_output_file;
	    if (dip->di_mount_lookup) {
		if ( isDeviceMounted(odip, odevice, dip->di_mntDebugFlag) == True ) {
		    if (odip->di_output_file) {
			Eprintf(dip, "Device %s is mounted on %s, writing disallowd!\n",
				odevice, odip->di_mounted_on_dir);
			return(FAILURE);
		    }
		}
	    }
	    if (dip->di_debug_flag && odip->di_mounted_from_device) {
		Printf(dip, "Device %s is mounted on %s\n", odevice, odip->di_mounted_on_dir);
	    }
	}
    } /* end if (dip->di_fsfile_flag) */

    /*
     * Note: Order is important, initialize SCSI *before* FindCapacity()!
     */
#if defined(SCSI)
    if ( (dip->di_scsi_flag == True) &&
	 ((dip->di_dtype->dt_dtype == DT_DISK) || dip->di_scsi_dsf) ) {
	if (dip->di_scsi_dsf == NULL) {
	    dip->di_scsi_dsf = strdup(dip->di_dname);
	}
	if ((status = init_scsi_info(dip, dip->di_scsi_dsf, &dip->di_sgp, &dip->di_sgpio)) == FAILURE) {
	    dip->di_scsi_flag = False;
	}
    } else {
	dip->di_scsi_flag = False;	/* Ok, not doing SCSI operations! */
    }
    if ( (dip->di_scsi_io_flag == True) &&
	 ( (dip->di_nvme_flag == False) && (dip->di_scsi_flag == False) ) ) {
	Eprintf(dip, "NVMe/SCSI operations are disabled, so pass-thru I/O is NOT possible!\n");
	return(FAILURE);
    }
    if ( (dip->di_aio_flag == True) &&
	 ( (dip->di_nvme_io_flag == True) || (dip->di_scsi_io_flag == True) ) ) {
	Eprintf(dip, "NVMe/SCSI I/O and Asynchronous I/O (AIO) is NOT supported!\n");
	return(FAILURE);
    }
    if (dip->di_tscsi_dsf) {
	(void)init_scsi_trigger(dip, dip->di_tscsi_dsf, &dip->di_tsgp);
    }
#endif /* defined(SCSI) */

    /*
     * Do multiple slices processing.
     */
    if (dip->di_slices) {
	/*
	 * Create multiple slices (if requested).
	 */
	if ( (dip->di_random_access == False) && (dip->di_bypass_flag == False) ) {
	    Eprintf(dip, "Multiple slices is only supported on random access devices!\n");
	    return(FAILURE);
	}
	/*
	 * If the the file does *not* exist, and we cannot setup the capacity
	 * or we'll report this error below:
	 *   ERROR: You must specify a data limit, a record count, or both.
	 * 
	 * Note: If the file exists, or a user capacity/data limit was specified,
	 * then SetupTransferLimits() has already been called from dtinfo.c!
	 * But that said, please don't forget direct access disks! -Robin
	 * 
	 * Yea, major league messy, but making due for now, until more cleanup!
	 */
	if ( dip->di_user_capacity || isDiskDevice(dip) || os_file_exists(dip->di_dname) ) {
	     status = FindCapacity(dip);
	     if (status == FAILURE) return(status);
	}
    } /* End of multiple slices processing. */

    /*
     * Open device / Setup system / device specific test information.
     */
    if (dip->di_ftype == INPUT_FILE) {
	int open_mode = (dip->di_read_mode | dip->di_open_flags);
	SetupBufferingMode(dip, &open_mode);
	dip->di_initial_flags = open_mode;
	dip->di_input_dtype = dip->di_dtype;

	/*
	 * The new logic for the IOT pattern is to make it unique during
	 * multiple passes. However, during reads it must remain constant.
	 */
	if (dip->di_iot_pattern) {
	    dip->di_unique_pattern = False;	/* Re-reads during all pass use same seed! */
	}

	/*
	 * If disk device and random I/O selected, attempt to get
	 * device / partition capacity to limit random I/O seeks.
	 */
	if ( !dip->di_slices &&
	     (dip->di_user_capacity ||
	      (dip->di_random_io && dip->di_random_access)) ) {
	    if ( (status = FindCapacity(dip)) == FAILURE ) {
		return(status);
	    }
	}
	if (dip->di_record_limit == 0) {
	    dip->di_record_limit = INFINITY;	/* Read until EOF on reads. */
	}

    } else { /* Process the output device/file. */
	int open_mode;

	/*
	 * If a skip count was specified, then open the output file for R/W,
	 * since skips are accomplished via read()'s. (for pelle)
	 */
	if (dip->di_skip_count || dip->di_raw_flag) {
	    /* Note: For Windows, open flags and write flags may be defined! */
	    open_mode = (dip->di_rwopen_mode | dip->di_write_flags | dip->di_open_flags);
	} else {
	    open_mode = (dip->di_write_mode | dip->di_write_flags | dip->di_open_flags);
	}
	/*
	 * Don't create files in the /dev directory (presume the file
	 * should exist instead of creating file & misleading user).
	 */
	if ( dip->di_output_file &&
	     (NEL (dip->di_output_file, DEV_PREFIX, DEV_LEN)) &&
	     (NEL (dip->di_output_file, ADEV_PREFIX, ADEV_LEN)) ) {
	    open_mode |= O_CREAT;	/* For creating test files.	*/
	}
	/*
	 * If verify mode, the output device is open for reads.
	 */
	if (dip->di_io_mode == VERIFY_MODE) {
	    open_mode = (dip->di_read_mode | dip->di_open_flags);
	    dip->di_mode = READ_MODE;
	}
	SetupBufferingMode(dip, &open_mode);
	dip->di_initial_flags = open_mode;
	dip->di_output_dtype = dip->di_dtype;

	/*
	 * If disk device and random I/O selected, attempt to get
	 * device / partition capacity to limit random I/O seeks.
	 */
	if ( !dip->di_slices &&
	     (dip->di_user_capacity ||
	      (dip->di_random_io && dip->di_random_access)) ) {
	    if ((status = FindCapacity(dip)) == FAILURE) {
		return(status);
	    }
	}

	/*
	 * For disks and tapes, default writing until EOF is reached.
	 */
	if ( (dip->di_record_limit == 0) &&
	     ((dip->di_dtype->dt_dtype == DT_DISK)  ||
	      (dip->di_dtype->dt_dtype == DT_BLOCK) ||
	      (dip->di_dtype->dt_dtype == DT_TAPE)) ) {
	    dip->di_record_limit = INFINITY;
	}
    } /* if (dip->di_ftype == INPUT_FILE) */

    /*
     * Set the default lbdata size, if not setup by the system
     * dependent functions above.  Delaying this check to this
     * point allows the device sector size to be setup, instead
     * of forcing it to 512 byte blocks.
     */
    if (!dip->di_lbdata_size) dip->di_lbdata_size = BLOCK_SIZE;
    /*
     * Verify counts are large enough, to avoid false compare errors.
     */
    if ( (dip->di_btag_flag || dip->di_iot_pattern || dip->di_lbdata_flag || dip->di_timestamp_flag) &&
	 (dip->di_block_size < dip->di_lbdata_size) ) {
	Eprintf(dip, "Please specify a block size >= %u (lbdata size) for btag, iot, lbdata, or timestamp options!\n",
		dip->di_lbdata_size);
	return(FAILURE);
    }

    if ( (dip->di_iobehavior == DT_IO) &&
	 (dip->di_rdata_limit || dip->di_random_align) &&
	 (dip->di_random_percentage == 0) &&
	 ((dip->di_io_dir != REVERSE) && (dip->di_io_type != RANDOM_IO)) ) {
	Wprintf(dip, "random options have no effect without iotype=random!\n");
    }

    /*
     * If random I/O was selected, and a data limit isn't available,
     * inform the user we need one, and don't allow testing.
     */
    if (dip->di_rdata_limit == 0UL) dip->di_rdata_limit = dip->di_data_limit;
    if ( (dip->di_rdata_limit == 0) && (dip->di_io_type == RANDOM_IO) ) {
	Eprintf(dip, "Please specify a record or data limit for random I/O.\n");
	return(FAILURE);
    }

#if 0
    /* This is no longer a valid sanity check due to changes in FindCapacity() for file position! */
    /*
     * Sanity check the random I/O data limits.
     */
    if ( (dip->di_io_type == RANDOM_IO) &&
	 ((large_t)(dip->di_file_position + dip->di_block_size + dip->di_random_align) > dip->di_rdata_limit)) {
	Eprintf(dip, "The max block size is too large for random data limits!\n");
	if (dip->di_Debug_flag) {
	    Printf (dip, "file position " FUF ", bs=%ld, ralign=" FUF ", rlimit=" LUF "\n",
		    dip->di_file_position, dip->di_block_size, dip->di_random_align, dip->di_rdata_limit);
	}
	return(FAILURE);
    }
#endif /* 0 */

    /*
     * Special handling for step option:
     *
     * With a step option and a data limit, we need to set the end offset.
     * The I/O loops will stop when the offset reaches this end offset.
     * 
     * For regular files, we must setup an end position (offset), since
     * doing I/O to a file system will NOT encounter an EOF like raw disks.
     * Slices also requires an ending position when step option is used.
     *
     * Note: setup_slice() sets this for slices, called after this function!
     */
    if (dip->di_step_offset &&
	((dip->di_dtype->dt_dtype == DT_REGULAR) || dip->di_slices) ) {
	if (dip->di_data_limit && (dip->di_data_limit != INFINITY)) {
	    dip->di_end_position = (dip->di_file_position + dip->di_data_limit);
	} else {
	    dip->di_end_position = (dip->di_file_position + (dip->di_record_limit * dip->di_block_size));
	}
    }
    
    /*
     * Ensure either a data limit and/or a record count was specified.
     *
     * Remember: The record limit is set to INFINITY when a data limit is specified.
     * 
     * TODO: More cleanup, add I/O behavior specific function! (too messy)
     */
    if ( (dip->di_io_mode == TEST_MODE) &&
	 ( ( (dip->di_record_limit == 0) &&
	 ((dip->di_data_limit == 0) || (dip->di_data_limit == INFINITY)) ) ||
	   ( (dip->di_iobehavior == SIO_IO) &&
	     ((dip->di_data_limit == INFINITY) && (dip->di_record_limit == INFINITY)) ) ) ) {
	Eprintf(dip, "You must specify a data limit, a record count, or both.\n");
	return(FAILURE);
    }
    
    /*
     * Reset the data limit, if specified by the user, as we overwrote it earlier.
     * The data limit gets set to the max data limit in ReadCapacity (needs cleanup!).
     * Note: Actually, ReadCapacity() is for SCSI disks, which sets the user capacity.
     * Note: This is a workaround for random I/O or slices so user limit is not ignored!
     */
    if (dip->di_user_limit && (dip->di_user_limit < dip->di_data_limit)) {
	dip->di_data_limit = dip->di_user_limit;
    }

    /*
     * Calculate size necessary for the data buffer & the pad bytes.
     */
    dip->di_data_size = (dip->di_block_size + PADBUFR_SIZE);
    dip->di_data_alloc_size = dip->di_data_size;
    if (dip->di_rotate_flag) {
	dip->di_data_alloc_size += ROTATE_SIZE;
    }
    /* Tracked separately, since this can dynamically grow! */
    dip->di_verify_buffer_size = dip->di_data_alloc_size;

    /*
     * Extra verify buffer required for read-after-write operations.
     */
    if (dip->di_raw_flag || (dip->di_iobehavior == DTAPP_IO) || (dip->di_iobehavior == THUMPER_IO) ) {
	dip->di_verify_buffer = malloc_palign(dip, dip->di_verify_buffer_size, dip->di_align_offset);
    }

    if ( dip->di_max_data_percentage && isDiskDevice(dip) &&
	 dip->di_data_limit && (dip->di_data_limit != INFINITY) ) {
	dip->di_max_data = do_maxdata_percentage(dip, dip->di_data_limit,
						 dip->di_max_data_percentage);
    }

    return(SUCCESS);
}

int
do_common_file_system_setup(dinfo_t *dip)
{
    int status = SUCCESS;

    /* Format the file & directory path based on user control strings. */
    if (dip->di_fsfile_flag == True) {
	if ( strchr(dip->di_dname, '%') ) {
	    status = format_device_name(dip, dip->di_dname);
	}
	if (dip->di_dir && strchr(dip->di_dir, '%') ) {
	    /* Format the directory based on user control strings. */
	    status = setup_directory_info(dip);
	}
        /*
         * The file must exist for all reads!
         */
        if ( (dip->di_read_percentage == 100) &&
	     (dip->di_fill_always == False) && (dip->di_fill_once == False) ) {
            hbool_t file_exists = os_file_exists(dip->di_dname);
            if (file_exists == False) {
                Eprintf(dip, "You've requested 100%% reads, but file %s does *not* exist!\n", dip->di_dname);
                status = FAILURE;
            }
        }
    }
    return(status);
}

int
dt_post_open_setup(dinfo_t *dip)
{
    int status = SUCCESS;

    /* Be careful *not* to break other I/O types or behaviors! */
    if ( (dip->di_iobehavior != DT_IO) ||
	 (dip->di_io_mode != TEST_MODE) ||
	 (dip->di_mode != WRITE_MODE) ) {
	return(status);
    }
    if (dip->di_fsfile_flag == True) {
	if (dip->di_read_percentage) {
	    large_t data_limit = dip->di_data_limit;
	    large_t filesize = os_get_file_size(dip->di_dname, dip->di_fd);
	    if (dip->di_slices) {
		data_limit += dip->di_file_position;
	    }
	    if ( filesize < data_limit ) {
		if (dip->di_fill_once == UNINITIALIZED) {
		    dip->di_fill_once = True;
		}
		if (dip->di_debug_flag || dip->di_Debug_flag) {
		    Printf(dip, "File size of "LUF" bytes, is less than your requested limit of "LUF" bytes.\n",
			   filesize, data_limit);
		}
		if (dip->di_fill_once == True) {
		    if ( (dip->di_verbose_flag == True) &&
			 (dip->di_thread_number == 1) &&
			 (dip->di_file_number == 0) && (dip->di_subdir_number == 0) ) {
			/* Note: Make thie debug if this continues to be too noisy! */
			Wprintf(dip, "Files will be filled once to populate with data for reading.\n");
		    }
		} else {
		    /* Extend the file. */
		    status = dt_extend_file(dip, dip->di_dname, dip->di_fd,
					    dip->di_data_buffer, dip->di_block_size,
					    data_limit, EnableErrors);
		}
	    }
	}
    }
    return(status);
}

large_t
do_maxdata_percentage(dinfo_t *dip, large_t data_bytes, int data_percentage)
{
    large_t max_data;

    max_data = (large_t)( (double)data_bytes * ((double)data_percentage / 100.0) );
    if (dip->di_threads > 1) {
	max_data /= dip->di_threads; /* Divide across threads. */
    }
    if (max_data) {
	/* This is important for direct disks, direct I/O, and IOT data pattern. */
	if (max_data < dip->di_device_size) {
	    max_data = roundup(max_data, dip->di_device_size);
	} else {
	    max_data = rounddown(max_data, dip->di_device_size);
	}
	if (dip->di_fDebugFlag || dip->di_debug_flag) {
	    double Kbytes = (double)( (double)max_data / (double)KBYTE_SIZE);
	    double Mbytes = (double)( (double)max_data / (double)MBYTE_SIZE);
	    /* Convey some useful debug information. */
	    Printf(dip, "Setting max data to %d%% of "LUF", or "LUF" bytes (%.3f Kbytes, %.3f Mbytes).\n",
		   data_percentage, data_bytes, max_data, Kbytes, Mbytes);
	}
    }
    return(max_data);
}
    
/*
 * do_filesystem_setup() - Do File System Setup.
 *
 * Description:
 *	The input or output file path is parsed into the directory part
 * (di_dir), the device name (di_dname), and the base name (di_bname).
 * This breakup is mainly done for multiple directories and multiple
 * files with threads, as a full path is constructed using this info.
 * This function also creates the top level directory, if it does not
 * already exist, and sets flags for deleting the directory later.
 *
 * Input:
 * 	dip = The device information pointer.
 * 	Note: Assumes dip->di_dname is already setup!
 *
 * Return Value:
 *	Returns Success/Failure.
 *
 */
int
do_filesystem_setup(dinfo_t *dip)
{
    char *file = dip->di_dname;
    int status = SUCCESS;

    /*
     * Note: Do NOT setup the directory name (di_dir) for non-file system 
     *       devices, without updating all places where this is referenced. 
     */

    /*
     * Setup the directory name, and create the top directory (as required).
     */
    if (dip->di_dir == NULL) {
	char *p;
	if (p = strrchr(file, dip->di_dir_sep)) {
	    *p = '\0';	/* Separate the directory from the file name. */
	    dip->di_dir = strdup(file);
	    *p++ = dip->di_dir_sep;
	    dip->di_bname = strdup(p);
	    if (dip->di_debug_flag || dip->di_fDebugFlag) {
		Printf(dip, "Directory: %s, File: %s, Base Name: %s\n",
		       dip->di_dir, file, dip->di_bname);
	    }
	} else {
	    /* Note: Don't wish long file paths, if user did not specify! */
	    //dip->di_dir = os_getcwd();
	    dip->di_bname = strdup(file);
	}
    } else {
	/* The user probably specified the directory name! */
	dip->di_bname = strdup(file);
    }

    /* 
     * Create the top directory, if it does not exist.
     */
    if (dip->di_dir && dip->di_output_file) {
	dip->di_topdir_created = False;
	if ( dt_file_exists(dip, dip->di_dir) == False ) {
	    os_error_t error = os_get_error();
	    /* Note: We only expect "file/path not found" errors! */
	    if ( (os_isFileNotFound(error) == False) &&
		 (os_isDirectoryNotFound(error) == False) ) {
		 return(FAILURE);
	    }
	    /* Don't create the directory if format strings exist! */
	    if ( strchr(dip->di_dir, '%') == NULL ) {
		/* We must create the directory to obtain FS info below. */
		status = create_directory(dip, dip->di_dir);
		if (status == SUCCESS) {
		    dip->di_topdir_created = True;
		} else if (status == FAILURE) {
		    return(status);
		} else if(status == WARNING) {
		    status = SUCCESS;	/* Directory already exists. */
		}
		/* Must Keep top directory if multiple threads are using it! */
		if ( (dip->di_topdir_created == True) && (dip->di_threads > 1) ) {
		    dip->di_topdir_created = False;
		    if (dip->di_verbose_flag) {
			Wprintf(dip, "Top level directory %s, will *not* be deleted!\n",
				dip->di_dir);
		    }
		}
	    }
	}
	dip->di_topdirpath = strdup(dip->di_dir);
    }

    /*
     * Note: This must be done *after* the top directory is created!
     */
    if (dip->di_fDebugFlag == True) {
	Printf(dip, "Requesting file system information...\n");
    }
    /*
     * Note: This is Mickey Mouse, but we cannot expand the directory
     * format strings *until* we are in per thread context (e.g. "%uuid").
     */
    if (dip->di_dir) {
	if ( strchr(dip->di_dir, '%') == NULL ) {
	    (void)os_get_fs_information(dip, dip->di_dir);
	} else {
	    char *p;
	    /* Format strings must be the last part of path! */
	    if (p = strrchr(dip->di_dir, dip->di_dir_sep)) {
		*p = '\0';	/* Remove last part of directory name. */
		(void)os_get_fs_information(dip, dip->di_dir);
		*p++ = dip->di_dir_sep;
	    }
	}
    } else { /* No directory, current directory will be used. */
	(void)os_get_fs_information(dip, dip->di_dir);
    }

    /*
     * With multiple files, we'll create a top level directory "d0",
     * by default, to place files. This allows multiple threads with
     * just a mount point to work properly (helps out automation).
     */
    if (dip->di_multiple_dirs) {
	char topdir[PATH_BUFFER_SIZE];
	if (dip->di_dir) {
	    (void)sprintf(topdir, "%s%c%s0", dip->di_dir, dip->di_dir_sep, dip->di_dirprefix);
	} else {
	    (void)sprintf(topdir, "%s0", dip->di_dirprefix);
	}
	free(dip->di_dir);
	dip->di_dir = strdup(topdir);
    }
    return(status);
}

/*
 * do_monitor_processing() - Handle Monitor Related Options/Setup.
 */
int
do_monitor_processing(dinfo_t *mdip, dinfo_t *dip)
{
    /*
     * Note: Each of these options implicitly enable a default alarm time.
     *       (moved here from individal option parsing to consolidate )
     */
    /* Note: Backwards compatability, alarm enables keepalives! */
    if ( (dip->di_iobehavior == DT_IO) &&
	 dip->di_alarmtime && (dip->di_keepalive_time == (time_t)0) ) {
	dip->di_keepalive_time = dip->di_alarmtime;
    }
    if (dip->di_alarmtime == (time_t)0) {
        /*
	 * Note: We *always* start the I/O monitoring thread to detect hung threads.
	 */
    	if (dip->di_alarmtime == 0) dip->di_alarmtime++;	/* One second please! */
    }
    /*
     * Adjust the alarm time to the lowest value of the options specified.
     */
    if (dip->di_alarmtime) {
	if (dip->di_noprogtime) dip->di_alarmtime = MIN(dip->di_alarmtime,dip->di_noprogtime);
	if (dip->di_keepalive_time) dip->di_alarmtime = MIN(dip->di_alarmtime,dip->di_keepalive_time);
	if (dip->di_runtime > 0) dip->di_alarmtime = MIN(dip->di_alarmtime,dip->di_runtime);
	/* Also see below for the default alarm time for I/O Monitoring! */
    }
    /* Note: The alarm is our timer frequency. If too high, noprogt and runtimes are delayed. */

    /*
     * Handle enable=noprog, without a noprog time.
     */
    if ( (dip->di_noprog_flag == True) && (dip->di_noprogtime == (time_t)0) ) {
	dip->di_noprogtime = dip->di_alarmtime;
	if (dip->di_noprogtime == 0) dip->di_noprogtime++;
    }
    if ( dip->di_keepalive && (dip->di_keepalive_time == (time_t)0) ) {
	dip->di_keepalive_time = dip->di_alarmtime;
    }
#if 0
    if ( dip->di_keepalive_time && (dip->di_keepalive == NULL) ) {
	/* Setup the default keepalive message. */
	if (dip->di_pstats_flag) {
	    dip->di_keepalive = strdup(keepalive0);
	} else { /* disable=pstats */
	    dip->di_keepalive = strdup(keepalive1);
	}
    }
#endif /* 0 */

    /* Note: Using mdip for monitoring thread (right now)! */
    (void)StartMonitorThread(mdip, (unsigned int)dip->di_alarmtime);

#if 0
    /*
     * When stats=brief, we allow a one line format for pass and totals.
     * If the user has not specified their own format, set our defaults.
     * Note: They aren't really keepalive, just use the same logic. :-)
     */
    if ( (dip->di_pkeepalive == NULL) || (strlen(dip->di_pkeepalive) == 0) ) {
	if (dip->di_vary_iotype) {
	    dip->di_pkeepalive = strdup(pass_type_msg);
	} else if (dip->di_vary_iodir) {
	    dip->di_pkeepalive = strdup(pass_dir_msg);
	} else {
	    dip->di_pkeepalive = strdup(pass_msg);
	}
	if ( (dip->di_dtype->dt_dtype == DT_REGULAR) && dip->di_bufmode_count ) {
	    char *buffer = Malloc(dip, (strlen(dip->di_pkeepalive) + SMALL_BUFFER_SIZE));
	    if (buffer) {
		(void)sprintf(buffer, "%s (%%bufmode)", dip->di_pkeepalive);
		FreeStr(dip, dip->di_pkeepalive);
		dip->di_pkeepalive = buffer;
	    }
	}
    }
    if ( (dip->di_tkeepalive == NULL) || (strlen(dip->di_tkeepalive) == 0) ) {
	dip->di_tkeepalive = strdup(totals_msg);
    }
#endif /* 0 */
    return(SUCCESS);
}

void
do_setup_keepalive_msgs(dinfo_t *dip)
{
    if ( dip->di_keepalive_time && (dip->di_keepalive == NULL) ) {
	/* Setup the default keepalive message. */
	if (dip->di_pstats_flag) {
	    dip->di_keepalive = strdup(keepalive0);
	} else { /* disable=pstats */
	    dip->di_keepalive = strdup(keepalive1);
	}
    }
    /*
     * When stats=brief, we allow a one line format for pass and totals.
     * If the user has not specified their own format, set our defaults.
     * Note: They aren't really keepalive, just use the same logic. :-)
     */
    if ( (dip->di_pkeepalive == NULL) || (strlen(dip->di_pkeepalive) == 0) ) {
	if (dip->di_vary_iotype) {
	    dip->di_pkeepalive = strdup(pass_type_msg);
	} else if (dip->di_vary_iodir) {
	    dip->di_pkeepalive = strdup(pass_dir_msg);
	} else {
	    dip->di_pkeepalive = strdup(pass_msg);
	}
	if ( (dip->di_dtype->dt_dtype == DT_REGULAR) && dip->di_bufmode_count ) {
	    char *buffer = Malloc(dip, (strlen(dip->di_pkeepalive) + SMALL_BUFFER_SIZE));
	    if (buffer) {
		(void)sprintf(buffer, "%s (%%bufmode)", dip->di_pkeepalive);
		FreeStr(dip, dip->di_pkeepalive);
		dip->di_pkeepalive = buffer;
	    }
	}
    }
    if ( (dip->di_tkeepalive == NULL) || (strlen(dip->di_tkeepalive) == 0) ) {
	dip->di_tkeepalive = strdup(totals_msg);
    }
    return;
}
