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
 * Module:	dtusage.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	Display usage information for generic data test program.
 *
 * Modification History:
 *
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded I/O, so starting with new history!
 */
#include "dt.h"
#include <fcntl.h>

/*
 * dtusage()/dthelp() - Display valid options for dt program.
 */
#define P	Print
#define D	Fprint

#if defined(WINDOWS_XP)
# define VARIANT " (XP)"
#else /* !defined(WINDOWS_XP) */
#  define VARIANT ""
#endif /* defined(WINDOWS_XP) */

char *version_str = "Date: September 21st, 2023"VARIANT", Version: 25.05, Author: Robin T. Miller";

void
dtusage(dinfo_t *dip)
{
    P (dip, "Usage: %s options...\n", cmdname);
    P (dip, " Type '%s help' for a list of valid options.\n", cmdname);
    return;
}

void
dtversion(dinfo_t *dip)
{
    P (dip, "    --> %s <--\n", version_str);
    return;
}

void
dthelp(dinfo_t *dip)
{
    static char *enabled_str = "enabled";
    static char *disabled_str = "disabled";

    P (dip, "Usage: %s options...\n", cmdname);
    P (dip, "\n    Where options are:\n");
    P (dip, "\tif=filename           The input file to read.\n");
    P (dip, "\tof=filename           The output file to write.\n");
#if defined(SCSI)
    P (dip, "\tsdsf=filename         The SCSI device special file.\n");
    P (dip, "\ttdsf=filename         The SCSI trigger device file.\n");
#endif /* defined(SCSI) */
    P (dip, "\tpf=filename           The data pattern file to use.\n");
    P (dip, "\tdir=dirpath           The directory path for files.\n");
    P (dip, "\tdirp=string           The directory prefix for subdirs.\n");
    P (dip, "\tfilepostfix=str       The file postfix. (D: %s)\n", dip->di_file_postfix);
#if not_implemented_yet
    P (dip, "\tdirs=value            The number of directories.\n");
#endif
    P (dip, "\tsdirs=value           The number of subdirectories.\n");
    P (dip, "\tdepth=value           The subdirectory depth.\n");
    P (dip, "\tbs=value              The block size to read/write.\n");
    P (dip, "    or");
    P (dip, "\tbs=random             Random sizes between "SDF" and "SDF" bytes.\n",
       MIN_RANDOM_SIZE, MAX_RANDOM_SIZE);
    P (dip, "\tibs=value             The read block size. (overrides bs=)\n");
    P (dip, "\tobs=value             The write block size. (overrides bs=)\n");
    P (dip, "\tjob_log=filename      The job log file name. (alias: jlog=)\n");
    P (dip, "\tlogdir=filename       The log directory name.\n");
    P (dip, "\tlog[atu]=filename     The thread log file name to write.\n");
    P (dip, "\t                      a=append, t=truncate, u=unique (w/tid)\n");
    P (dip, "\tlogprefix=string      The per line logging prefix.\n");
    P (dip, "\terror_log=filename    The error log file name. (alias: elog=)\n");
    P (dip, "\tmaster_log=filename   The master log file name. (alias: mlog=)\n");
    P (dip, "\treread_file=filename  The reread file name.\n");
#if defined(AIO)
    P (dip, "\taios=value            Set number of AIO's to queue.\n");
#endif /* defined(AIO) */
#if !defined(_QNX_SOURCE)
    P (dip, "\talarm=time            The keepalive alarm time.\n");
    P (dip, "\tkeepalive=string      The keepalive message string.\n");
    P (dip, "\tkeepalivet=time       The keepalive message frequency.\n");
    P (dip, "\tpkeepalive=str        The pass keepalive message string.\n");
    P (dip, "\ttkeepalive=str        The totals keepalive message string.\n");
    P (dip, "\talign=offset          Set offset within page aligned buffer.\n");
    P (dip, "    or\talign=rotate          Rotate data address through sizeof(ptr).\n");
#endif /* !defined(_QNX_SOURCE) */
    P (dip, "\tcapacity=value        Set the device capacity in bytes.\n");
    P (dip, "    or\tcapacity=max          Set maximum capacity from disk driver.\n");
    P (dip, "\tcapacityp=value       Set capacity by percentage (range: 0-100).\n");
    P (dip, "\tbufmodes={buffered,unbuffered,cachereads,cachewrites}\n");
    P (dip, "\t                      Set one or more buffering modes. (Default: none)\n");
    P (dip, "\tboff=string           Set the buffer offsets to: dec or hex. (Default: %s)\n",
					    (dip->di_boff_format == DEC_FMT) ? "dec" : "hex");
    P (dip, "\tdfmt=string           Set the data format to: byte or word. (Default: %s)\n",
    					    (dip->di_data_format == BYTE_FMT) ? "byte" : "word");
    P (dip, "\tdispose=mode          Set file dispose to: {delete, keep, or keeponerror}.\n");
    P (dip, "\tdlimit=value          Set the dump data buffer limit.\n");
    P (dip, "\tdtype=string          Set the device type being tested.\n");
    P (dip, "\tidtype=string         Set input device type being tested.\n");
    P (dip, "\todtype=string         Set output device type being tested.\n");
    P (dip, "\tdsize=value           Set the device block (sector) size.\n");
    P (dip, "\terrors=value          The number of errors to tolerate.\n");
    P (dip, "\tfiles=value           Set number of disk/tape files to process.\n");
    P (dip, "\tmaxfiles=value        The maximum files for all directories.\n");
    P (dip, "\tffreq=value           The frequency (in records) to flush buffers.\n");
    P (dip, "\tfstrim_freq=value     The file system trim frequency (in files).\n");
    P (dip, "\tfill_pattern=value    The write fill pattern (32 bit hex).\n");
    P (dip, "\tprefill_pattern=value The read prefill pattern (32 bit hex).\n");
    P (dip, "\tflow=type             Set flow to: none, cts_rts, or xon_xoff.\n");
/*  P (dip, "\thz=value              Set number of clock ticks per second.\n");	*/
    P (dip, "\tincr=value            Set number of record bytes to increment.\n");
    P (dip, "    or\tincr=variable         Enables variable I/O request sizes.\n");
    P (dip, "\tiops=value            Set I/O per second (this is per thread).\n");
    P (dip, "\tiodir=direction       Set I/O direction to: {forward, reverse, or vary}.\n");
    P (dip, "\tiomode=mode           Set I/O mode to: {copy, mirror, test, or verify}.\n");
    P (dip, "\tiotype=type           Set I/O type to: {random, sequential, or vary}.\n");
    P (dip, "\tiotpass=value         Set the IOT pattern for specified pass.\n");
    P (dip, "\tiotseed=value         Set the IOT pattern block seed value.\n");
    P (dip, "\tiotune=filename       Set I/O tune delay parameters via file.\n");
    P (dip, "\thistory=value         Set the number of history request entries.\n");
    P (dip, "\thistory_bufs=value    Set the history data buffers (per request).(or hbufs)\n");
    P (dip, "\thistory_bsize=value   Set the history block data size increment. (or hbsize)\n");
    P (dip, "\thistory_data=value    Set the history data size (bytes to save). (or hdsize)\n");
    P (dip, "\tmin=value             Set the minumum record size to transfer.\n");
    P (dip, "\tmax=value             Set the maximum record size to transfer.\n");
    P (dip, "\tlba=value             Set starting block used w/lbdata option.\n");
    P (dip, "\tlbs=value             Set logical block size for lbdata option.\n");
    P (dip, "\tlimit=value           The number of bytes to transfer (data limit).\n");
    P (dip, "    or");
    P (dip, "\tlimit=random          Random data limits between "LUF" and "LUF" bytes.\n",
       MIN_DATA_LIMIT, MAX_DATA_LIMIT);
    P (dip, "\tincr_limit=value      Set the data limit increment.\n");
    P (dip, "\tmin_limit=value       Set the minumum data limit.\n");
    P (dip, "\tmax_limit=value       Set the maximum data limit.\n");
    P (dip, "\tmaxdata=value         The maximum data limit (all files).\n");
    P (dip, "\tmaxdatap=value        The maximum data percentage (range: 0-100).\n");
    P (dip, "\tflags=flags           Set open flags:   {excl,sync,...}\n");
    P (dip, "\toflags=flags          Set output flags: {append,trunc,...}\n");
    P (dip, "\tvflags=flags          Set/clear btag verify flags. {lba,offset,...}\n");
    P (dip, "\tmaxbad=value          Set maximum bad blocks to display. (Default: %d)\n",
       dip->di_max_bad_blocks);
    P (dip, "\tonerr=action          Set error action: {abort, continue, or pause}.\n");
#if !defined(WIN32)
    P (dip, "\tnice=value            Apply the nice value to alter our priority.\n");
#endif
    P (dip, "\tnoprogt=value         Set the no progress time (in seconds).\n");
    P (dip, "\tnoprogtt=value        Set the no progress trigger time (secs).\n");
    P (dip, "\tnotime=optype         Disable timing of specified operation type.\n");
#if defined(_QNX_SOURCE)
    P (dip, "\tparity=string         Set parity to: {even, odd, mark, space, or none}.\n");
#else /* !defined(_QNX_SOURCE) */
    P (dip, "\tparity=string         Set parity to: {even, odd, or none}.\n");
#endif /* defined(_QNX_SOURCE) */
    P (dip, "\tpass_cmd=string       The per pass command to execute.\n");
    P (dip, "\tpasses=value          The number of passes to perform.\n");
    P (dip, "\tpattern=value         The 32 bit hex data pattern to use.\n");
    P (dip, "    or\tpattern=iot           Use DJ's IOT test pattern.\n");
    P (dip, "    or\tpattern=incr          Use an incrementing data pattern.\n");
    P (dip, "    or\tpattern=string        The string to use for the data pattern.\n");
    P (dip, "\tposition=offset       Position to offset before testing.\n");
    P (dip, "\toposition=offset      The output file position (copy/verify).\n");
    P (dip, "\tprefix=string         The data pattern prefix string.\n");
    P (dip, "\tprocs=value           The number of processes to create.\n");
#if defined(HP_UX)
    P (dip, "\tqdepth=value          Set the queue depth to specified value.\n");
#endif /* defined(HP_UX) */
    P (dip, "\tralign=value          The random I/O offset alignment.\n");
    P (dip, "\trlimit=value          The random I/O data byte limit.\n");
    P (dip, "\trseed=value           The random number generator seed.\n");
    P (dip, "\trecords=value         The number of records to process.\n");
    P (dip, "\treadp=value           Percentage of accesses that are reads. Range [0,100].\n");
    P (dip, "\t                      'random' keyword makes the read/write percentage random.\n");
    P (dip, "\trandp=value           Percentage of accesses that are random. Range [0,100].\n");
    P (dip, "\t                      Sequential accesses = 0%%, else random percentage\n");
    P (dip, "\trrandp=value          Percentage of read accesses that are random. Range [0,100].\n");
    P (dip, "\twrandp=value          Percentage of write accesses that are random. Range [0,100].\n");
    P (dip, "\truntime=time          The number of seconds to execute.\n");
    P (dip, "\tscript=filename       The script file name to execute.\n");
    P (dip, "\tslices=value          The number of disk slices.\n");
    P (dip, "\tslice=value           Choose a specific disk slice.\n");
    P (dip, "\tsoffset=value         The starting slice offset.\n");
    P (dip, "\tskip=value            The number of records to skip past.\n");
    P (dip, "\tseek=value            The number of records to seek past.\n");
    P (dip, "\tstep=value            The number of bytes seeked after I/O.\n");
    P (dip, "\tstats=level           The stats level: {brief, full, or none}\n");
    P (dip, "\tstopon=filename       Watch for file existence, then stop.\n");
    P (dip, "\tsleep=time            The sleep time (in seconds).\n");
    P (dip, "\tmsleep=value          The msleep time (in milliseconds).\n");
    P (dip, "\tusleep=value          The usleep time (in microseconds).\n");
    P (dip, "\tshowbtags opts...     Show block tags and btag data.\n");
    P (dip, "\tshowfslba             Show file system offset to physical LBA.\n");
    P (dip, "\tshowfsmap             Show file system map extent information.\n");
    P (dip, "\tshowtime=value        Show time value in ctime() format.\n");
    P (dip, "\tshowvflags=value      Show block tag verify flags set.\n");
    P (dip, "\tthreads=value         The number of threads to execute.\n");
    P (dip, "\ttrigger={br, bdr, lr, seek, cdb:bytes, cmd:str, and/or triage}\n");
    P (dip, "\t                      The triggers to execute on errors.\n");
    P (dip, "\ttrigger_action=value  The trigger action (for noprogs).\n");
    P (dip, "\ttrigger_on={all, errors, miscompare, or noprogs} (Default: all)\n");
    P (dip, "\t                      The trigger control (when to execute).\n");
    P (dip, "\tvolumes=value         The number of volumes to process.\n");
    P (dip, "\tvrecords=value        The record limit for the last volume.\n");
    P (dip, "\tenable=flag           Enable one or more of the flags below.\n");
    P (dip, "\tdisable=flag          Disable one or more of the flags below.\n");
    P (dip, "\thelp                  Display this help text.\n");
    P (dip, "\teval EXPR             Evaluate expression, show values.\n");
    P (dip, "\tsystem CMD            Execute a system command.\n");
    P (dip, "\t!CMD                  Same as above, short hand.\n");
    P (dip, "\tshell                 Startup a system shell.\n");
    P (dip, "\tusage                 Display the program usage.\n");
    P (dip, "\tversion               Display the version information.\n");

    P (dip, "\n    I/O Behaviors:\n");
    P (dip, "\tiobehavior=type       Specify the I/O behavior. (alias: iob=)\n");
    P (dip, "\t  Where type is:\n");
    P (dip, "\t    dt                The dt I/O behavior (default).\n");
    P (dip, "\t    dtapp             The dtapp I/O behavior.\n");
    P (dip, "\t    hammer            The hammer I/O behavior.\n");
    P (dip, "\t    sio               The simple I/O (sio) behavior.\n");
    P (dip, "\n    For help on each I/O behavior use: \"iobehavior=type help\"\n");

    P (dip, "\n    Block Tag Verify Flags: (prefix with ~ to clear flag)\n");
    P (dip, "\tlba,offset,devid,inode,serial,hostname,signature,version\n");
    P (dip, "\tpattern_type,flags,write_start,write_secs,write_usecs,\n");
    P (dip, "\tpattern,generation,process_id,thread_number,device_size\n");
    P (dip, "\trecord_index,record_size,record_number,step_offset,\n");
    P (dip, "\topaque_data_type,opaque_data_size,crc32\n");
    P (dip, "\n");
    P (dip, "\tdefault Disk: lba,devid,serial + common\n");
    P (dip, "\tdefault File: offset,inode + common flags\n");
    P (dip, "\tcommon Flags: hostname,signature,write_start,generation,\n");
    P (dip, "\t              prcoess_id,job_id,thread_number,crc32\n");
    P (dip, "\n");
    P (dip, "\tExample: verifyFlags= or vflags=~all,lba,crc32\n");
    /* Omitting, seems a bit too verbose! */
    //show_btag_verify_flags(dip);

    P (dip, "\n    Force Corruption Options:\n");
    P (dip, "\tcorrupt_index=value   The corruption index. (Default: random)\n");
    P (dip, "\tcorrupt_length=value  The corruption length. (Default: %d bytes)\n",
       dip->di_corrupt_length);
    P (dip, "\tcorrupt_pattern=value The corruption pattern. (Default: 0x%x)\n",
       dip->di_corrupt_pattern);
    P (dip, "\tcorrupt_step=value    Corruption buffer step. (Default: %d bytes)\n",
       dip->di_corrupt_step);
    P (dip, "\tcorrupt_reads=value   Corrupt at read records. (Default: %d)\n",
       dip->di_corrupt_reads);
    P (dip, "\tcorrupt_writes=value  Corrupt at write records. (Default: %d)\n",
       dip->di_corrupt_writes);

    P (dip, "\n    Job Start Options:\n");
    P (dip, "\tistate={paused,running} (Default: running)\n");
    P (dip, "\t                      Initial state after thread created.\n");
    P (dip, "\ttag=string            Specify job tag when starting tests.\n");
    P (dip, "\n    Job Control Options:\n");
    P (dip, "\tjobs[:full][={jid|tag}] | [job=value] | [tag=string]\n");
    P (dip, "\t                      Show all jobs or specified job.\n");
    P (dip, "\tcancelall             Cancel all jobs.\n");
    P (dip, "\tcancel={jid|tag} | [job=value] | [tag=string]\n");
    P (dip, "\t                      Cancel the specified job ID.\n");
    P (dip, "\tmodify[={jid|tag}] | [job=value] | [tag=string] [modify_options]\n");
    P (dip, "\t                      Modify all jobs or specified job.\n");
    P (dip, "\tpause[={jid|tag}] | [job=value] | [tag=string]\n");
    P (dip, "\t                      Pause all jobs or specified job.\n");
    P (dip, "\tquery[={jid|tag}] | [job=value] | [tag=string] [query_string]\n");
    P (dip, "\t                      Query all jobs or specified job.\n");
    P (dip, "\tresume[={jid|tag}] | [job=value] | [tag=string]\n");
    P (dip, "\t                      Resume all jobs or specified job.\n");
    P (dip, "\tstopall               Stop all jobs.\n");
    P (dip, "\tstop={jid|tag} | [job=value] | [tag=string]\n");
    P (dip, "\t                      Stop the specified job.\n");
    P (dip, "\twait[={jid|tag}] | [job=value] | [tag=string]\n");
    P (dip, "\t                      Wait for all jobs or specified job.\n");

    P (dip, "\n    File System Map Format:\n");
    P (dip, "\tshowfslba [bs=value] [count=value] [limit=value] [offset=value]\n");
    P (dip, "\t                      Show FS offset(s) mapped to physical LBA(s)\n");
    P (dip, "\t                      The default is to show LBA for specified offset.\n");
    P (dip, "\tshowfsmap [bs=value] [count=value] [limit=value] [offset=value]\n");
    P (dip, "\t                      Show the file system map extent information.\n");
    P (dip, "\t                      The default is to show the full extent map.\n");

    P (dip, "\n    File Locking Options:\n");
    P (dip, "\tenable=lockfiles      Enables file locks (locks & unlocks)\n");
    P (dip, "\tlockmode={mixed | full | partial}\n");
    P (dip, "\t                      Chance of full or partial file locks (default: mixed).\n");
    P (dip, "\tunlockchance=[0-100]  Probability of keeping locks and skipping unlocking.\n");
    P (dip, "\tExamples:\n");
    P (dip, "\t    unlockchance=100  100%% chance of unlocking, ALL files unlocked. [default]\n");
    P (dip, "\t    unlockchance=50    50%% chance of unlocking each file.\n");
    P (dip, "\t    unlockchance=0      0%% chance of unlocking, NO files are unlocked.\n");

    P (dip, "\n    Workload Options:\n");
    P (dip, "\tdefine workloadName options...\n");
    P (dip, "\t                      Define a workload with options.\n");
    P (dip, "\tworkloads [substr]\n");
    P (dip, "\t                      Display the valid workloads.\n");
    P (dip, "\tworkload=name         Select the specified workload.\n");

    P (dip, "\n    File System Full Options:\n");
    P (dip, "\tfsfree_delay=value    FS free space sleep delay.    (Def: %u secs)\n",
						dip->di_fsfree_delay);
    P (dip, "\tfsfree_retries=value  FS free space wait retries.   (Def: %u)\n",
						dip->di_fsfree_retries);
    P (dip, "\n");
    P (dip, "\tPlease consider adding the truncate flag or enable=deleteperpass,\n");
    P (dip, "\tto free space between passes or with multiple threads to same FS.\n");

    P (dip, "\n    Retry Related Options:\n");
    P (dip, "\tretry_error=value     The error code to retry.\n");
    P (dip, "\tretry_delay=value     The retry delay.              (Def: %u secs)\n",
						dip->di_retry_delay);
    P (dip, "\tretry_limit=value     The retry limit.              (Def: %u)\n",
						dip->di_retry_limit);
    P (dip, "\tretryDC_delay=value   The retry corruptions delay.  (Def: %u)\n",
						dip->di_retryDC_delay);
    P (dip, "\tretryDC_limit=value   The retry corruptions limit.  (Def: %u)\n",
						dip->di_retryDC_limit);
    P (dip, "\n    Error Strings Accepted:\n");
    /* Note: Disk full errors cannot be retried at this time! */
#if defined(WIN32)
    P (dip, "\tERROR_BUSY (%d), ERROR_DISK_FULL (%d)\n",
       ERROR_BUSY, ERROR_DISK_FULL);
    P (dip, "\tERROR_IO_DEVICE (%d), ERROR_VC_DISCONNECTED (%d)\n",
       ERROR_IO_DEVICE, ERROR_VC_DISCONNECTED);
    P (dip, "\tERROR_UNEXP_NET_ERR (%d), ERROR_SEM_TIMEOUT (%d)\n",
       ERROR_UNEXP_NET_ERR, ERROR_SEM_TIMEOUT);
    P (dip, "\tERROR_BAD_NETPATH (%d), ERROR_NETNAME_DELETED (%d)\n",
       ERROR_BAD_NETPATH, ERROR_NETNAME_DELETED);
    P (dip, "\tERROR_DEVICE_NOT_CONNECTED (%d), ERROR_NO_SYSTEM_RESOURCES (%d)\n",
       ERROR_DEVICE_NOT_CONNECTED, ERROR_NO_SYSTEM_RESOURCES);
#else /* !defined(WIN32) */
    P (dip, "\tEIO (%d), ", EIO);
    P (dip, "ENXIO (%d), ", ENXIO);
    P (dip, "EBUSY (%d), ", EBUSY);
    P (dip, "ENODEV (%d), ", ENODEV);
    P (dip, "ENOSPC (%d), ", ENOSPC);
    P (dip, "ESTALE (%d)\n", ESTALE);
#endif /* defined(WIN32) */
    P (dip, "\t    OR\n");
    P (dip, "\tretry_error='*' or -1 to retry all errors.\n");

#if defined(SCSI)
    P (dip, "\n    SCSI Specific Options:\n");
    P (dip, "\tidt=string            The Inquiry device type. (both, device, or serial)\n");
    P (dip, "\tspt_path=string       Path to SCSI (spt) tool.\n");
    P (dip, "\tspt_options=string    Additional spt options.\n");
    P (dip, "\treadtype=string       The SCSI read type (read8, read10, read16).\n");
    P (dip, "\twritetype=string      The SCSI write type (write8, write10, write16, writev16).\n");
    P (dip, "\tscsi_recovery_delay=value The SCSI recovery delay.  (Def: %u secs)\n",
       dip->di_scsi_recovery_delay);
    P (dip, "\tscsi_recovery_retries=value The SCSI recovery retries.(Def: %u)\n",
       dip->di_scsi_recovery_limit);
    P (dip, "\tscsi_timeout=value    The SCSI timeout (in ms).     (Def: %ums)\n",
       dip->di_scsi_timeout);
    P (dip, "\tunmap_freq=value      The SCSI unmap frequency.     (Def: %u)\n",
       dip->di_unmap_frequency);
    P (dip, "\tunmap=type            The SCSI unmap type.\n");
    P (dip, "\t Valid types are: random, unmap, write_same, zerorod.\n");
#endif /* defined(SCSI) */

    P (dip, "\n    Flags to enable/disable:\n");
#if defined(AIO)
    P (dip, "\taio              POSIX Asynchronous I/O.    (Default: %s)\n",
				(dip->di_aio_flag) ? enabled_str : disabled_str);
#endif /* defined(AIO) */
    P (dip, "\tasync            Asynchronous job control.  (Default: %s)\n",
				(dip->di_async_job) ? enabled_str : disabled_str);
    P (dip, "\tbtags            Block tag control flag.    (Default: %s)\n",
    				(dip->di_btag_flag) ? enabled_str : disabled_str);
    //P (dip, "\tcerrors          Report close errors.       (Default: %s)\n",
    //				(dip->di_cerrors_flag) ? enabled_str : disabled_str);
    P (dip, "\tcompare          Data comparison flag.      (Default: %s)\n",
				(dip->di_compare_flag) ? enabled_str : disabled_str);
    P (dip, "\txcompare         Extra btag prefix compare. (Default: %s)\n",
				(dip->di_xcompare_flag) ? enabled_str : disabled_str);
    P (dip, "\tcoredump         Core dump on errors.       (Default: %s)\n",
				(dip->di_force_core_dump) ? enabled_str : disabled_str);
    P (dip, "\tdeleteerrorlog   Delete error log file.     (Default: %s)\n",
			 	DeleteErrorLogFlag ? enabled_str : disabled_str);
    P (dip, "\tdeleteperpass    Delete files per pass.     (Default: %s)\n",
			 	(dip->di_delete_per_pass) ? enabled_str : disabled_str);
    P (dip, "\tdebug            Debug output.              (Default: %s)\n",
				(dip->di_debug_flag) ? enabled_str : disabled_str);
    P (dip, "\tDebug            Verbose debug output.      (Default: %s)\n",
				(dip->di_Debug_flag) ? enabled_str : disabled_str);
    P (dip, "\tbtag_debug       Block tag (btag) debug.    (Default: %s)\n",
				(dip->di_btag_debugFlag) ? enabled_str : disabled_str);
    P (dip, "\tedebug           End of file debug.         (Default: %s)\n",
				(dip->di_eDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\tfdebug           File operations debug.     (Default: %s)\n",
				(dip->di_fDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\tjdebug           Job control debug.         (Default: %s)\n",
				(dip->di_jDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\tldebug           File locking debug.        (Default: %s)\n",
				(dip->di_lDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\tmdebug           Memory related debug.      (Default: %s)\n",
				(dip->di_mDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\tmntdebug         Mount device lookup debug. (Default: %s)\n",
				(dip->di_mDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\tpdebug           Process related debug.     (Default: %s)\n",
				(dip->di_pDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\trdebug           Random debug output.       (Default: %s)\n",
				(dip->di_rDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\ttdebug           Thread debug output.       (Default: %s)\n",
				(dip->di_tDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\ttimerdebug       Timer debug output.        (Default: %s)\n",
				(dip->di_timerDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\tdump             Dump data buffer.          (Default: %s)\n",
				(dip->di_dump_flag) ? enabled_str : disabled_str);
    P (dip, "\tdumpall          Dump all blocks.           (Default: %s)\n",
				(dip->di_dumpall_flag) ? enabled_str : disabled_str);
    P (dip, "\tdump_btags       Dump block tags (btags).   (Default: %s)\n",
				(dip->di_dump_btags) ? enabled_str : disabled_str);
    P (dip, "\tdump_context     Dump good context block.   (Default: %s)\n",
				(dip->di_dump_context_flag) ? enabled_str : disabled_str);
    P (dip, "\terrors           Report errors flag.        (Default: %s)\n",
				(dip->di_errors_flag) ? enabled_str : disabled_str);
    P (dip, "\txerrors          Report extended errors.    (Default: %s)\n",
				(dip->di_extended_errors) ? enabled_str : disabled_str);
    P (dip, "\teof              EOF/EOM exit status.       (Default: %s)\n",
				(dip->di_eof_status_flag) ? enabled_str : disabled_str);
    P (dip, "\tfileperthread    File per thread.           (Default: %s)\n",
				(dip->di_fileperthread == True) ? enabled_str : disabled_str);
    P (dip, "\tfill_always      Always fill files.         (Default: %s)\n",
				(dip->di_fill_always == True) ? enabled_str : disabled_str);
    P (dip, "\tfill_once        Fill the file once.        (Default: %s)\n",
				(dip->di_fill_once == True) ? enabled_str : disabled_str);
    P (dip, "\tfsalign          File system align.         (Default: %s)\n",
				(dip->di_fsalign_flag) ? enabled_str : disabled_str);
    P (dip, "\tfsmap            File system map control.   (Default: %s)\n",
				(dip->di_fsmap_flag) ? enabled_str : disabled_str);
    P (dip, "\tfstrim           File system trim.          (Default: %s)\n",
				(dip->di_fstrim_flag) ? enabled_str : disabled_str);
    P (dip, "\tfunique          Unique output file.        (Default: %s)\n",
        			(dip->di_unique_file) ? enabled_str : disabled_str);
    P (dip, "\tfsincr           File size incrementing.    (Default: %s)\n",
				(dip->di_fsincr_flag) ? enabled_str : disabled_str);
    P (dip, "\tfsync            Controls file sync'ing.    (Default: %s)\n",
				(dip->di_fsync_flag == UNINITIALIZED) ? "runtime"
				: (dip->di_fsync_flag) ? enabled_str : disabled_str);
    P (dip, "\theader           Log file header.           (Default: %s)\n",
				(dip->di_logheader_flag) ? enabled_str : disabled_str);
    P (dip, "\ttrailer          Log file trailer.          (Default: %s)\n",
				(dip->di_logtrailer_flag) ? enabled_str : disabled_str);
    P (dip, "\tforce-corruption Force a FALSE corruption.  (Default: %s)\n",
				(dip->di_force_corruption) ? enabled_str : disabled_str);
    P (dip, "\thdump            History dump.              (Default: %s)\n",
				(dip->di_history_timing) ? enabled_str : disabled_str);
    P (dip, "\thtiming          History timing.            (Default: %s)\n",
				(dip->di_history_timing) ? enabled_str : disabled_str);
    P (dip, "\timage            Image mode copy (disks).   (Default: %s)\n",
			 	(dip->di_image_copy) ? enabled_str : disabled_str);
    P (dip, "\tiolock           I/O lock control.          (Default: %s)\n",
			 	(dip->di_iolock) ? enabled_str : disabled_str);
    P (dip, "\tlbdata           Logical block data.        (Default: %s)\n",
				(dip->di_lbdata_flag) ? enabled_str : disabled_str);
    P (dip, "\tlogpid           Log process ID.            (Default: %s)\n",
        			(dip->di_logpid_flag) ? enabled_str : disabled_str);
    P (dip, "\tlockfiles        Lock files.                (Default: %s)\n",
        			(dip->di_lock_files) ? enabled_str : disabled_str);
    P (dip, "\tlooponerror      Loop on error.             (Default: %s)\n",
        			(dip->di_loop_on_error) ? enabled_str : disabled_str);
    P (dip, "\tmicrodelay       Microsecond delays.        (Default: %s)\n",
			(dip->di_sleep_res == SLEEP_USECS) ? enabled_str : disabled_str);
    P (dip, "\tmsecsdelay       Millisecond delays.        (Default: %s)\n",
			(dip->di_sleep_res == SLEEP_MSECS) ? enabled_str : disabled_str);
    P (dip, "\tsecsdelay        Second delays.             (Default: %s)\n",
			(dip->di_sleep_res == SLEEP_SECS) ? enabled_str : disabled_str);
#if defined(MMAP)
    P (dip, "\tmmap             Memory mapped I/O.         (Default: %s)\n",
				(dip->di_mmap_flag) ? enabled_str : disabled_str);
#endif /* defined(MMAP) */
    P (dip, "\tmount_lookup     Mount device lookup.       (Default: %s)\n",
				(dip->di_mount_lookup) ? enabled_str : disabled_str);
    P (dip, "\tmulti            Multiple volumes.          (Default: %s)\n",
				(dip->di_multi_flag) ? enabled_str : disabled_str);
    P (dip, "\tnoprog           No progress check.         (Default: %s)\n",
				(dip->di_noprog_flag) ? enabled_str : disabled_str);
    P (dip, "\tpipes            Pipe mode control flag.    (Default: %s)\n",
				(PipeModeFlag) ? enabled_str : disabled_str);
    P (dip, "\tpoison           Poison read buffer flag.   (Default: %s)\n",
				(dip->di_poison_buffer) ? enabled_str : disabled_str);
    P (dip, "\tprefill          Prefill read buffer flag.  (Default: %s)\n",
				(dip->di_prefill_buffer == UNINITIALIZED) ? "runtime"
				: (dip->di_prefill_buffer) ? enabled_str : disabled_str);
    P (dip, "\tjob_stats        The job statistics flag.   (Default: %s)\n",
				(dip->di_job_stats_flag) ? enabled_str : disabled_str);
    P (dip, "\tpstats           The per pass statistics.   (Default: %s)\n",
				(dip->di_pstats_flag) ? enabled_str : disabled_str);
    P (dip, "\ttotal_stats      The total statistics.      (Default: %s)\n",
				(dip->di_total_stats_flag) ? enabled_str : disabled_str);
    P (dip, "\traw              Read after write.          (Default: %s)\n",
				(dip->di_raw_flag) ? enabled_str : disabled_str);
    P (dip, "\treread           Re-read after raw.         (Default: %s)\n",
				(dip->di_reread_flag) ? enabled_str : disabled_str);
    P (dip, "\tresfsfull        Restart file system full.  (Default: %s)\n",
				(dip->di_fsfull_restart) ? enabled_str : disabled_str);
    P (dip, "\treadcache        Read cache control.        (Default: %s)\n",
				(dip->di_read_cache_flag) ? enabled_str : disabled_str);
    P (dip, "\twritecache       Write cache control.       (Default: %s)\n",
				(dip->di_write_cache_flag) ? enabled_str : disabled_str);
    P (dip, "\tretryDC          Retry data corruptions.    (Default: %s)\n",
				(dip->di_retryDC_flag) ? enabled_str : disabled_str);
    P (dip, "\tretrydisc        Retry session disconnects. (Default: %s)\n",
				(dip->di_retry_disconnects) ? enabled_str : disabled_str);
    P (dip, "\tretrywarn        Retry logged as warning.   (Default: %s)\n",
				(dip->di_retry_warning) ? enabled_str : disabled_str);
    P (dip, "\tsavecorrupted    Save corrupted data.       (Default: %s)\n",
				(dip->di_save_corrupted) ? enabled_str : disabled_str);
    P (dip, "\tscriptverify     Script verify display.     (Default: %s)\n",
				(dip->di_script_verify) ? enabled_str : disabled_str);   
    P (dip, "\tsighup           Hangup signal control.     (Default: %s)\n",
				(sighup_flag) ? enabled_str : disabled_str);   
#if defined(WIN32)
    P (dip, "\tsparse           Sparse file attribute.     (Default: %s)\n",
				(dip->di_sparse_flag) ? enabled_str : disabled_str);   
    P (dip, "\tprealloc         Preallocate w/o sparse.    (Default: %s)\n",
				(dip->di_prealloc_flag) ? enabled_str : disabled_str);   
#endif /* defined(WIN32) */
#if defined(NVME)
    P (dip, "\tnvme_io          NVMe I/O operations.       (Default: %s)\n",
				(dip->di_nvme_io_flag) ? enabled_str : disabled_str);
#endif /* defined(NVME) */
#if defined(SCSI)
    P (dip, "\tscsi             SCSI operations.           (Default: %s)\n",
				(dip->di_scsi_flag) ? enabled_str : disabled_str);
    P (dip, "\tscsi_info        SCSI information.          (Default: %s)\n",
				(dip->di_scsi_info_flag) ? enabled_str : disabled_str);
    P (dip, "\tscsi_io          SCSI I/O operations.       (Default: %s)\n",
				(dip->di_scsi_io_flag) ? enabled_str : disabled_str);
    P (dip, "\tsdebug           SCSI debug output.         (Default: %s)\n",
				(dip->di_sDebugFlag) ? enabled_str : disabled_str);
    P (dip, "\tscsi_errors      SCSI error logging.        (Default: %s)\n",
				(dip->di_scsi_errors) ? enabled_str : disabled_str);
    P (dip, "\tscsi_recovery    SCSI recovery control.     (Default: %s)\n",
				(dip->di_scsi_recovery) ? enabled_str : disabled_str);
    P (dip, "\tunmap            SCSI unmap per pass.       (Default: %s)\n",
				(dip->di_unmap_flag) ? enabled_str : disabled_str);
    P (dip, "\tget_lba_status   SCSI Get LBA Status.       (Default: %s)\n",
				(dip->di_unmap_flag) ? enabled_str : disabled_str);
    P (dip, "\tfua              SCSI Force unit access.    (Default: %s)\n",
				(dip->di_fua) ? enabled_str : disabled_str);
    P (dip, "\tdpo              SCSI Disable page out.     (Default: %s)\n",
				(dip->di_dpo) ? enabled_str : disabled_str);
#endif /* defined(SCSI) */
    P (dip, "\tstats            Display statistics.        (Default: %s)\n",
				(dip->di_stats_flag) ? enabled_str : disabled_str);
    P (dip, "\tstopimmed        Stop immediate w/stop file.(Default: %s)\n",
				(dip->di_stop_immediate) ? enabled_str : disabled_str);
#if defined(SYSLOG)
    P (dip, "\tsyslog           Log errors to syslog.      (Default: %s)\n",
				(dip->di_syslog_flag) ? enabled_str : disabled_str);
#endif /* defined(SYSLOG) */
    P (dip, "\tterminate_on_signals Terminate on signals.  (Default: %s)\n",
				(terminate_on_signals) ? enabled_str : disabled_str);
#if defined(TIMESTAMP)
    P (dip, "\ttimestamp        Timestamp each block.      (Default: %s)\n",
				(dip->di_timestamp_flag) ? enabled_str : disabled_str);
#endif /* defined(TIMESTAMP) */
    P (dip, "\ttrigargs         Trigger cmd arguments.     (Default: %s)\n",
				(dip->di_trigargs_flag) ? enabled_str : disabled_str);
    P (dip, "\ttrigdefaults     Automatic trigger defaults.(Default: %s)\n",
				(dip->di_trigdefaults_flag) ? enabled_str : disabled_str);
    P (dip, "\ttrigdelay        Delay mismatch triggers.   (Default: %s)\n",
				(dip->di_trigdelay_flag) ? enabled_str : disabled_str);
    P (dip, "\tunique           Unique pattern.            (Default: %s)\n",
				(dip->di_unique_pattern) ? enabled_str : disabled_str);
    P (dip, "\tuuid_dashes      Dashes in UUID strings.    (Default: %s)\n",
				(dip->di_uuid_dashes) ? enabled_str : disabled_str);
    P (dip, "\tverbose          Verbose output.            (Default: %s)\n",
				(dip->di_verbose_flag) ? enabled_str : disabled_str);
    P (dip, "\tverify           Verify data written.       (Default: %s)\n",
				(dip->di_verify_flag) ? enabled_str : disabled_str);
    P (dip, "\n");
    P (dip, "\tExample: enable=debug disable=compare,pstats\n");
    P (dip, "\n    Common Open Flags:\n");
    P (dip, "\tnone                  Clear all user set flags.\n");
#if defined(O_EXCL)
    P (dip, "\texcl (O_EXCL)         Exclusive open. (don't share)\n");
#endif /* defined(O_EXCL) */
#if defined(O_NDELAY)
    P (dip, "\tndelay (O_NDELAY)     Non-delay open. (don't block)\n");
#endif /* defined(O_NDELAY) */
#if defined(O_NONBLOCK)
    P (dip, "\tnonblock (O_NONBLOCK) Non-blocking open/read/write.\n");
#endif /* defined(O_NONBLOCK) */
#if defined(O_CACHE)
    P (dip, "\tcache (O_CACHE)       Attempt to keep data in file system cache.\n");
#endif /* defined(O_CACHE) */
#if defined(O_DIRECT)
    P (dip, "\tdirect (O_DIRECT)     Direct disk access. (don't cache data).\n");
#endif /* defined(O_DIRECT) */
#if defined(SOLARIS)
    P (dip, "\tdirect (directio())   Direct disk access. (don't cache data).\n");
#endif /* defined(SOLARIS) */
#if defined(WIN32)
    P (dip, "\tdirect (NO_BUFFERING) Direct disk access. (don't cache data).\n");
#endif /* !defined(WIN32) */
    P (dip, "\tnodirect              Cache data (disables Direct I/O).\n");
#if defined(O_FSYNC)
    P (dip, "\tfsync (O_FSYNC)       Sync both read/write data with disk file.\n");
#endif /* defined(O_FSYNC) */
#if defined(O_RSYNC)
    P (dip, "\trsync (O_RSYNC)       Synchronize read operations.\n");
#endif /* defined(O_RSYNC) */
#if defined(O_SYNC)
    P (dip, "\tsync (O_SYNC)         Sync updates for data/file attributes.\n");
#endif /* defined(O_SYNC) */
#if defined(O_LARGEFILE)
    P (dip, "\tlarge (O_LARGEFILE)   Enable large (64-bit) file system support.\n");
#endif /* defined(O_LARGEFILE) */
    P (dip, "\n    Output Open Flags:\n");
    P (dip, "\tnone                  Clear all user set flags.\n");
#if defined(O_APPEND)
    P (dip, "\tappend (O_APPEND)     Append data to end of existing file.\n");
#endif /* defined(O_APPEND) */
#if defined(O_DEFER)
    P (dip, "\tdefer (O_DEFER)       Defer updates to file during writes.\n");
#endif /* defined(O_DEFER) */
#if defined(O_DSYNC)
    P (dip, "\tdsync (O_DSYNC)       Sync data to disk during write operations.\n");
#endif /* defined(O_DSYNC) */
#if defined(O_TRUNC)
    P (dip, "\ttrunc (O_TRUNC)       Truncate an existing file before writing.\n");
#endif /* defined(O_TRUNC) */
#if defined(O_TEMP)
    P (dip, "\ttemp (O_TEMP)         Temporary file, try to keep data in cache.\n");
#endif /* defined(O_TEMP) */
    P (dip, "\n    Delays (Values are seconds, unless micro/msecs delay is enabled):\n");
    P (dip, "\topen_delay=value      Delay before opening the file.    (Default: %u)\n",
							dip->di_open_delay);
    P (dip, "\tclose_delay=value     Delay before closing the file.    (Default: %u)\n",
							dip->di_close_delay);
    P (dip, "\tdelete_delay=value    Delay after deleting files.       (Default: %u secs)\n",
							dip->di_end_delay);
    P (dip, "\tend_delay=value       Delay between multiple passes.    (Default: %u secs)\n",
							dip->di_end_delay);
    P (dip, "\tforced_delay=value    Force random I/O delay (noprog).  (Default: %u secs)\n",
							dip->di_end_delay);
    P (dip, "\tstart_delay=value     Delay before starting the test.   (Default: %u secs)\n",
							dip->di_start_delay);
    P (dip, "\tread_delay=value      Delay before reading each record. (Default: %u)\n",
							dip->di_read_delay);
    P (dip, "\tverify_delay=value    Delay before verifying data.      (Default: %u)\n",
							dip->di_verify_delay);
    P (dip, "\twrite_delay=value     Delay before writing each record. (Default: %u)\n",
							dip->di_write_delay);
    P (dip, "\tterm_delay=value      Delay before terminating.         (Default: %u secs)\n",
							dip->di_term_delay);
    P (dip, "\tterm_wait=time        Thread termination wait time.     (Default: %d secs)\n",
							dip->di_term_wait_time);
    P (dip, "\n");
    P (dip, "\tThe delay options accept 'random' for random delays.\n");
    P (dip, "\tPlease Note: For disk devices, microseconds is the default!:\n");
    P (dip, "\n    Numeric Input:\n");
    P (dip, "\tFor options accepting numeric input, the string may contain any\n");
    P (dip, "\tcombination of the following characters:\n");
    P (dip, "\n\tSpecial Characters:\n");
    P (dip, "\t    w = words (%u bytes)", (unsigned int)sizeof(int));
    P (dip, "            q = quadwords (%u bytes)\n", (unsigned int)sizeof(large_t));
    P (dip, "\t    b = blocks (512 bytes)         k = kilobytes (1024 bytes)\n");
    P (dip, "\t    m = megabytes (1048576 bytes)  p = page size (%d bytes)\n", page_size);
    P (dip, "\t    g = gigabytes (%ld bytes)\n", GBYTE_SIZE);
    P (dip, "\t    t = terabytes (" LDF " bytes)\n", TBYTE_SIZE);
    P (dip, "\t    d = device size (set via dsize=value option)\n");
    P (dip, "\t    inf or INF = infinity (" LUF " bytes)\n", INFINITY);
    P (dip, "\n\tArithmetic Characters:\n");
    P (dip, "\t    + = addition                   - = subtraction\n");
    P (dip, "\t    * or x = multiplcation         / = division\n");
    P (dip, "\t    %% = remainder\n");
    P (dip, "\n\tBitwise Characters:\n");
    P (dip, "\t    ~ = complement of value       >> = shift bits right\n");
    P (dip, "\t   << = shift bits left            & = bitwise 'and' operation\n");
    P (dip, "\t    | = bitwise 'or' operation     ^ = bitwise exclusive 'or'\n\n");
    P (dip, "\tThe default base for numeric input is decimal, but you can override\n");
    P (dip, "\tthis default by specifying 0x or 0X for hexadecimal conversions, or\n");
    P (dip, "\ta leading zero '0' for octal conversions.  NOTE: Evaluation is from\n");
    P (dip, "\tright to left without precedence, and parenthesis are not permitted.\n");

    P (dip, "\n    Keepalive Format Control:\n");
    P (dip, "\t    %%b = The bytes read or written.   %%B = Total bytes read and written.\n");
    P (dip, "\t    %%c = Record count for this pass.  %%C = Total records for this test.\n");
    P (dip, "\t    %%d = The device/file name.        %%D = The real device name.\n");
    P (dip, "\t    %%e = The number of errors.        %%E = The error limit.\n");
    P (dip, "\t    %%f = The files read or written.   %%F = Total files read and written.\n");
    P (dip, "\t    %%h = The host name.               %%H = The full host name.\n");
    P (dip, "\t    %%k = The kilobytes this pass.     %%K = Total kilobytes for this test.\n");
    P (dip, "\t    %%l = Blocks read or written.      %%L = Total blocks read and written.\n");
    P (dip, "\t    %%m = The megabytes this pass.     %%M = Total megabytes for this test.\n");
    P (dip, "\t    %%p = The pass count.              %%P = The pass limit.\n");
    P (dip, "\t    %%r = Records read this pass.      %%R = Total records read this test.\n");
    P (dip, "\t    %%s = The seconds this pass.       %%S = The total seconds this test.\n");
    P (dip, "\t    %%t = The pass elapsed time.       %%T = The total elapsed time.\n");
    P (dip, "\t    %%i = The I/O mode (read/write)    %%u = The user (login) name.\n");
    P (dip, "\t    %%w = Records written this pass.   %%W = Total records written this test.\n");
    P (dip, "\n    Performance Keywords:\n");
    P (dip, "\t    %%bps  = The bytes per second.     %%lbps = Logical blocks per second.\n");
    P (dip, "\t    %%kbps = Kilobytes per second.     %%mbps = The megabytes per second.\n");
    P (dip, "\t    %%iops = The I/O's per second.     %%spio = The seconds per I/O.\n");
    P (dip, "\n");
    P (dip, "\t    Lowercase means per pass stats, while uppercase means total stats.\n");
    P (dip, "\n    I/O Keywords:\n");
    P (dip, "\t    %%iodir = The I/O direction.       %%iotype = The I/O type.\n");
    P (dip, "\t    %%lba = The current logical block. %%offset = The current file offset.\n");
    P (dip, "\t    %%elba = The error logical block.  %%eoffset = The error file offset.\n");
    P (dip, "\t    %%bufmode = The file buffer mode.  %%status = The thread exit status.\n");
    P (dip, "\n    Job Control Keywords:\n");
    P (dip, "\t    %%job  = The job ID.               %%tag    = The job tag.\n");
    P (dip, "\t    %%tid  = The thread ID.            %%thread = The thread number.\n");
    P (dip, "\n    Misc Keywords:\n");
    P (dip, "\t    %%keepalivet = The keepalive time.\n");
    P (dip, "\n    Default Keepalive:\n");
    P (dip, "\t    keepalive=\"%s\"\n", keepalive0);
    P (dip, "\n    Default Pass Keepalive: (when full pass stats are disabled via disable=pstats)\n");
    P (dip, "\t    pkeepalive=\"%s\"\n", keepalive1);

    P (dip, "\n    Common Format Control Keywords:\n");
    P (dip, "\t    %%array   = The array name or management IP.\n");
    P (dip, "\t    %%bufmode = The file buffer mode.  %%dfs    = The directory separator ('%c')\n",
						       dip->di_dir_sep);
    P (dip, "\t    %%dsf     = The device name.       %%device = The device path.\n");
    P (dip, "\t    %%sdsf    = The SCSI device name.  %%tdsf   = The trigger device name.\n");
    P (dip, "\t    %%file    = The file name.         %%devnum = The device number.\n");
    P (dip, "\t    %%host    = The host name.         %%user   = The user name.\n");
    P (dip, "\t    %%job     = The job ID.            %%tag    = The job tag.\n");
    P (dip, "\t    %%jlog    = The job log.           %%tlog   = The Thread log.\n");
    P (dip, "\t    %%tid     = The thread ID.         %%thread = The thread number.\n");
    P (dip, "\t    %%pid     = The process ID.        %%prog   = The program name.\n");
    P (dip, "\t    %%ymd     = The year,month,day.    %%hms    = The hour,day,seconds.\n");
    P (dip, "\t    %%date    = The date string.       %%et     = The elapsed time.\n");
    P (dip, "\t    %%tod     = The time of day.       %%etod   = Elapsed time of day.\n");
    P (dip, "\t    %%secs    = Seconds since start.   %%seq    = The sequence number.\n");
    P (dip, "\t    %%script  = The script file name.  %%tmpdir = The temporary directory.\n");
    P (dip, "\t    %%uuid    = The UUID string.       %%workload = The workload name.\n");
    P (dip, "\t    %%month   = The month of the year. %%day    = The day of the month.\n");
    P (dip, "\t    %%year    = The four digit year.   %%hour   = The hour of the day.\n");
    P (dip, "\t    %%minutes = The minutes of hour.   %%seconds= The seconds of minute.\n");
    P (dip, "\t    %%nate    = The NATE log prefix.   %%nos    = The Nimble log prefix.\n");
    P (dip, "\n");
    P (dip, "\t    String 'gtod' = \"%%tod (%%etod) %%prog (j:%%job t:%%thread): \"\n");
    P (dip, "\n");
    P (dip, "\tExample: log=dt_%%host_%%user_%%iodir_%%iotype-%%uuid.log\n");
    P (dip, "\t         logprefix=\"%%seq %%ymd %%hms %%et %%prog (j:%%job t:%%thread): \"\n");

#if defined(SCSI)
    P (dip, "\n    SCSI Format Keywords:\n");
    P (dip, "\t    %%capacity = The disk capacity.    %%blocklen = The disk block length.\n");
    P (dip, "\t    %%vendor = The Inquiry vendor ID.  %%product = The Inquiry product ID.\n");
    P (dip, "\t    %%revision = The Inquiry revision. %%devid = The device identifier.\n");
    P (dip, "\t    %%serial = The disk serial number. %%mgmtaddr = The management address.\n");
#endif /* defined(SCSI) */

    P (dip, "\n    I/O Tune File Format Keywords:\n");
    P (dip, "\t    %%iotune = The I/O tune path.      %%tmpdir = The temporary directory.\n");
    P (dip, "\t    %%host   = The host name.          %%user   = The user (login) name.\n");
    P (dip, "\n");
    P (dip, "\tExample: iotune=%%iotune OR %%tmpdir%%host_MyIOtune_file\n");

    P (dip, "\n    Pattern String Input:\n");
    P (dip, "\t    \\\\ = Backslash   \\a = Alert (bell)   \\b = Backspace\n");
    P (dip, "\t    \\f = Formfeed    \\n = Newline        \\r = Carriage Return\n");
    P (dip, "\t    \\t = Tab         \\v = Vertical Tab   \\e or \\E = Escape\n");
    P (dip, "\t    \\ddd = Octal Value    \\xdd or \\Xdd = Hexadecimal Value\n");
  
    P (dip, "\n    Prefix Format Control:\n");
    P (dip, "\t    %%d = The device/file name.      %%D = The real device name.\n");
    P (dip, "\t    %%h = The host name.             %%H = The full host name.\n");
    P (dip, "\t    %%p = The process ID.            %%P = The parent PID.\n");
    P (dip, "\t    %%s = The device serial number.\n");
    P (dip, "\t    %%u = The user name.             %%U = A unique UUID.\n");
    P (dip, "\t    %%j = The job ID.                %%J = The job tag.\n");
    P (dip, "\t    %%t = The thread number.         %%T = The thread ID.\n");
    P (dip, "\n");
    P (dip, "\tExample: prefix=\"%%U %%d@%%h\" OR prefix=\"%%d(%%s)@%%h\"\n");

    P (dip, "\n    Time Input:\n");
    P (dip, "\t    d = days (%d seconds),      h = hours (%d seconds)\n",
						SECS_PER_DAY, SECS_PER_HOUR);
    P (dip, "\t    m = minutes (%d seconds),      s = seconds (the default)\n\n",
								SECS_PER_MIN);
    P (dip, "\tArithmetic characters are permitted, and implicit addition is\n");
    P (dip, "\tperformed on strings of the form '1d5h10m30s'.\n");

    P (dip, "\n    Trigger Types:\n");
    P (dip, "\t    br = Execute a bus reset.\n");
    P (dip, "\t    bdr = Execute a bus device reset.\n");
    P (dip, "\t    lr = Execute a device lun reset.\n");
    P (dip, "\t    seek = Issue a seek to the failing lba.\n");
    P (dip, "\t    triage = Issue SCSI triage commands.\n");
    P (dip, "\t    cmd:string = Execute command with these args:\n");
    P (dip, "\t      string dname op dsize offset position lba errno noprogt\n");
    P (dip, "\t      args following cmd:string get appended to above args.\n");

    /*
     * Display the program defaults.
     */
    P (dip, "\n    Defaults:\n");
    P (dip, "\terrors=%u", DEFAULT_ERROR_LIMIT);
    P (dip, ", files=%u", DEFAULT_FILE_LIMIT);
    P (dip, ", passes=%u", DEFAULT_PASS_LIMIT);
    P (dip, ", records=%u", 0);
    P (dip, ", bs=%u", (unsigned int)BLOCK_SIZE);
    P (dip, ", log=stderr\n");

    P (dip, "\tpattern=%#x", DEFAULT_PATTERN);
    P (dip, ", dispose=delete");
    P (dip, ", align=%d (page aligned)\n", dip->di_align_offset);

#if defined(AIO)
    P (dip, "\taios=%d", dip->di_aio_bufs);
    P (dip, ", dlimit=%u", (unsigned int)dip->di_dump_limit);
#else /* !defined(AIO) */
    /*
     * dump_limit is defined as size_t, which is either 32 or 64 bits,
     * which poses a problem during display. Note: Should be 32 bits!
     * Workaround via a cast, since our default is a small value. :-)
     */
    P (dip, "\tdlimit=%u", (unsigned int)dip->di_dump_limit);
#endif /* defined(AIO) */
    P (dip, ", onerr=%s", (dip->di_oncerr_action == ONERR_ABORT) ? "abort" : "continue");
    P (dip, ", volumes=%d, vrecords=%lu\n", dip->di_volume_limit, dip->di_volume_records);
    P (dip, "\tiodir=%s", (dip->di_io_dir == FORWARD) ? "forward" : "reverse");
    P (dip, ", iomode=%s", (dip->di_io_mode == TEST_MODE) ? "test" :
			(dip->di_io_mode == COPY_MODE) ? "copy" : "verify");
    P (dip, ", iotype=%s", (dip->di_io_type == RANDOM_IO) ? "random" : "sequential");
    P (dip, ", stats=%s\n", (dip->di_stats_level == STATS_BRIEF) ? "brief" :
                       (dip->di_stats_level == STATS_FULL) ? "full" : "none");
    P (dip, "\tiotseed=0x%08x, hdsize=%d", dip->di_iot_seed, dip->di_history_data_size);
    P (dip, ", maxbad=%u\n", dip->di_max_bad_blocks);
    P (dip, "\n    --> %s <--\n", version_str);

    return;
}
