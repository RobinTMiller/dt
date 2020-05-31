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
 * Module:	dtworkloads.c
 * Author:	Robin T. Miller
 * Date:	July 26th, 2013
 *
 * Description:
 *	Support for dt workloads.
 *
 * Modification History:
 * 
 * August 8th, 2019 (happy birthday brother Randy!)
 *      Added keepalive workload to define a better keepalive message.
 * 
 * June 6th, 2019 by Robin T. Miller
 *      Add log_files workload template to create job/thread log files.
 * 
 * May 20th, 2019 by Robin T. Miller
 *      Add direct disk read-after-write w/reread workload.
 *      Add log prefix with timestamp workload (for options).
 * 
 * December 11th, 2015 by Robin T. Miller
 *	Adding new workloads with newly added percentages. (yay!)
 *
 * April 8th, 2014 by Robin T. Miller
 * 	Remove Flash Accel (Mercury) workloads.
 */
#include "dt.h"

typedef struct workload_info {
    struct workload_info *wi_flink;	/* Forward link to next entry.  */
    struct workload_info *wi_blink;	/* Backward link to prev entry. */
    workload_entry_t *wi_entry;		/* The workload entry.		*/
} workload_info_t;

workload_info_t workloadsList;		/* The workload list header.	*/
workload_info_t *workloads = NULL;	/* The list of workloads.	*/
pthread_mutex_t workloads_lock;		/* The workloads queue lock.	*/

#define QUEUE_EMPTY(whdr)	(whdr->wi_flink == workloads)

/* 
 * Note: The C compiler concatenates strings together, thus trailing space " ".* 
 */
workload_entry_t predefined_workloads[] =
{
    {	"dt_acid",
	"File System Acid Workload (requires ~2.37g space)",
	"limit=25m maxdatap=75 onerr=abort disable=pstats oflags=trunc "
	"incr=var min=1k max=256k dispose=keep pattern=iot prefix='%d@%h' "
	"noprogt=15s noprogtt=130s alarm=6 notime=fsync,close "
	"enable=syslog history=5 hdsize=128 enable=htiming "
	"iodir=vary iotype=vary "
	"enable=deleteperpass,fsincr,timestamp,aio "
	"dirp=DT_ sdirs=8 files=6 depth=4 "
	"bufmodes=cachereads,buffered,unbuffered,cachewrites "
	"runtime=24h stopon="TEMP_DIR"stopit.dt "
	"trigger=cmd:"TRIGGER_SCRIPT" "
	"keepalivet=300"
    },
    {	"dt_dedup",
	"Deduplication Pattern",
	"min=8k max=256k incr=4k limit=1g maxdatap=75 "
	"pf="PATTERN_FILE_DIR"pattern_dedup "
	"enable=syslog history=5 enable=htiming "
	"bufmodes=cachereads,buffered,unbuffered,cachewrites "
	"disable=pstats keepalivet=5m threads=4"
    },
    {	"dt_hammer",
	"dt Hammer File System Workload (requires ~6.20g space)",
	"bs=random min_limit=b max_limit=5m incr_limit=vary "
	"files=250 maxdatap=75 iodir=vary iotype=vary "
	"onerr=abort enable=btags,deleteperpass prefix='%d@%h' pattern=iot "
	"bufmodes=buffered,cachereads,cachewrites,unbuffered "
	"history=5 hdsize=128 enable=htiming alarm=3 noprogt=15 noprogtt=3m "
	"disable=pstats keepalivet=5m threads=10"
    },
    /* Note: If bs=random is used, rseed= is required when only reading! */
    {	"many_files",
	"Populate directory with many files (requires ~1g space)",
	"min=b max=1m incr=4k limit=1m files=100 sdirs=3 depth=3 dispose=keep "
	"prefix='%d@%h' enable=lbdata disable=pstats"
    },
    {	"incr_files",
	"Create incrementing file sizes (requires ~1.36g space)",
	"files=256 min=b max=1m limit=64m prefix=%d@%h pattern=iot enable=fsincr dispose=keep"
    },
    /* Note: Percentages is NOT implemented for AIO at this time! */
    {	"file_percentages",
	"Single file with read/write and random/sequential percentages",
	"bs=random limit=1g enable=btags flags=direct onerr=abort slices=10 readp=-1 randp=50 dispose=keep"
    },
    {	"file_performance",
	"Single file performance",
	"bs=32k limit=1g flags=direct slices=10 disable=pstats disable=compare dispose=keep"
    },
    {	"fill_once",
	"Fill a file or disk once (write only)",
	"bs=64k slices=25 disable=compare,stats,verify dispose=keep"
    },
    {	"vary_file_sizes",
	"Create files with varying sizes (requires ~2.41g space)",
	"bs=random min_limit=b max_limit=5m incr_limit=vary "
	"files=100 sdirs=3 depth=3 maxdatap=75 dispose=keeponerror "
	"prefix='%d@%h' enable=lbdata,deleteperpass disable=pstats"
    },
    {	"terabyte_lun",
	"Terabyte LUN",
	"slices=16 step=4g aios=4 bs=random align=rotate "
	"pattern=iot prefix='%d@%h' iodir=reverse enable=pstats"
    },
    {	"terabyte_file",
	"Single terabyte file",
	"limit=15t slices=15 step=1g aios=4 bs=random align=rotate iodir=reverse "
	"pattern=iot prefix='%d@%h' enable=pstats dispose=keep"
    },
    {	"sparse_files",
	"Sparse files test",
	"bs=16k step=32k disable=pstats enable=lbdata dispose=keep"
    },
    /* Define I/O profiles for simulating various workloads. */
    /* http://blogs.msdn.com/b/tvoellm/archive/2009/05/07/useful-io-profiles-for-simulating-various-workloads.aspx */
    {	"web_file_server",
	"Web File Server Workload",
	"bs=64k readp=95 randp=5 disable=verify flags=direct"
    },
    {	"dss_db",
	"Decision Support System Database Workload",
	"bs=1m readp=100 randp=100 disable=compare,verify flags=direct"
    },
    {	"media_streaming",
	"Media Streaming Workload",
	"bs=64k readp=98 randp=0 disable=verify flags=direct"
    },
    {	"sql_Server_log",
	"SQL Server Log Workload",
	"bs=64k readp=0 randp=0 disable=verify flags=direct"
    },
    {	"os_paging",
	"OS Paging Workload",
	"bs=64k readp=90 randp=0 disable=verify flags=direct"
    },
    {	"web_server_log",
	"Web Server Log",
	"bs=8k readp=0 randp=0 disable=verify flags=direct"
    },
    {	"oltp_db",
	"Online Transaction Processing (OLTP) Workload",
	"bs=8k readp=70 randp=100 disable=verify flags=direct"
    },
    {	"exchange_server",
	"Exchange Server Workload",
	"bs=4k readp=67 randp=100 disable=verify flags=direct"
    },
    {	"workstation",
	"Workstation Workload",
	"bs=8k readp=80 randp=80 disable=verify flags=direct"
    },
    {	"video_on_demand",
	"Video on Demand (VOD) Workload",
	"bs=512k readp=0 randp=100 disable=verify flags=direct"
    },
    {	"longevity_common",
	"Longevity Common Options (template)",
	"min=8k max=1m incr=vary "
	"enable=raw,reread enable=syslog "
	"history=5 history_data=128 enable=history_timing "
	"logprefix='%seq %nos %et %prog (j:%job t:%thread): ' "
	"keepalivet=5m runtime=-1 "
	"onerr=abort noprogt=30s noprogtt=5m"
    },
    {	"longevity_file_dedup",
	"Longevity File System w/Dedup Workload",
	"workload=longevity_common "
	"min_limit=1m max_limit=2g "
	"flags=direct notime=close,fsync oflags=trunc "
	"pf="PATTERN_FILE_DIR"pattern_dedup "
	"maxdatap=75 threads=4"
    },
    {	"longevity_disk_dedup",
	"Longevity Direct Disk w/Dedup Workload",
	"workload=longevity_common "
	"pf="PATTERN_FILE_DIR"pattern_dedup "
	"capacityp=75 slices=4"
    },
    {	"longevity_file_system",
	"Longevity File System Workload",
	"workload=longevity_common workload=high_validation "
	"min_limit=1m max_limit=2g incr_limit=vary "
	"flags=direct notime=close,fsync oflags=trunc "
	"maxdatap=75 threads=4"
    },
    {	"longevity_disk_unmap",
	"Longevity Direct Disk w/SCSI UNMAP Workload",
	"workload=longevity_common workload=high_validation "
	"capacityp=75 slices=4 unmap=unmap"
    },
    {	"longevity_disk",
	"Longevity Direct Disk Workload",
	"workload=longevity_common workload=high_validation "
	"capacityp=75 slices=4"
    },
    {	"san_file_system",
	"SAN File System Workload",
	"bs=random limit=2g dispose=keeponerror "
	"iodir=vary iotype=vary keepalivet=5m workload=keepalive "
	"pattern=iot prefix='%d@%h' enable=btags "
	"onerr=abort disable=pstats "
	"notime=close,fsync oflags=trunc threads=4 "
	"enable=noprog noprogt=15s noprogtt=130s alarm=3s "
	"history=5 hdsize=128 enable=htiming "
	"enable=syslog runtime=12h stopon="TEMP_DIR"stopit.dt "
	"bufmodes=buffered,cachereads,cachewrites,unbuffered"
    },
    {	"san_disk",
	"SAN Direct Disk Workload",
	"bs=random slices=4 "
	"pattern=iot prefix='%d@%h' enable=btags "
	"iodir=vary iotype=vary keepalivet=5m workload=keepalive "
	"onerr=abort disable=pstats "
	"noprogt=15s noprogtt=130s alarm=3s "
	"history=5 hdsize=128 enable=htiming "
	"enable=syslog runtime=12h "
	"enable=stopimmed stopon="TEMP_DIR"stopit.dt"
    },
    {	"keepalive",
	"Keepalive Message (template)",
	"keepalive='%d stats: Mode: %i, Blocks: %l, %m Mbytes, "
	"MB/sec: %mbps, IO/sec: %iops, Pass %p, Elapsed: %T'"
    },
    {	"disk_read_after_write",
	"Direct Disk Read-After-Write w/Rereads",
	"workload=san_disk "
	"enable=read_immed,reread slices=8"
    },
    {	"disk_aligned_io",
	"Direct Disk Aligned I/O (assumes 4k blocks)",
	"workload=san_disk dsize=4k"
    },
    {	"disk_unaligned_io",
	"Direct Disk Aligned I/O (assumes 4k blocks)",
	"workload=san_disk dsize=4k offset=4k-3b"
    },
    {	"disk_dedup",
	"Direct Disk Deduplication",
	"min=8k max=256k incr=4k "
	"pf="PATTERN_FILE_DIR"pattern_dedup "
	"enable=syslog history=5 enable=htiming "
	"disable=pstats keepalivet=5m threads=4"
    },
    {	"disk_unmaps",
	"Direct Disk with Unmaps",
	"workload=san_disk unmap=unmap"
    },
    {	"disk_write_only",
	"Direct Disk Write Only",
	"workload=san_disk disable=verify"
    },
    {	"high_validation",
	"Define Highest Data Validation Options (template)",
	"enable=btags pattern=iot prefix='%d@%h'"
    },
    {	"job_stats_only",
	"Define options to display job statistics only (template)",
	"disable=stats enable=job_stats"
    },
    /* Note: Use logdir= option to direct logs to specific directory! */
    {	"all_logs",
	"Define options for creating all logs (template)",
	"job_log='dt_job%job.log' log='dt_thread-j%jobt%thread-%dsf.log'"
    },
    {	"job_logs",
	"Define options for creating job logs (template)",
	"job_log='dt_job%job.log'"
    },
    {	"thread_logs",
	"Define options for creating thread logs (template)",
	"log='dt_thread-j%jobt%thread-%dsf.log'"
    },
    {	"log_timestamps",
	"Define options for adding log file timestamps (template)",
	"logprefix='%date %et %prog (j:%job t:%thread): '"
    }
};
int workload_entries = sizeof(predefined_workloads) / sizeof(workload_entry_t);

/*
 * Forward References:
 */ 
int acquire_workloads_lock(void);
int release_workloads_lock(void);

void add_predefined_workloads(void);
workload_info_t *create_workload(workload_entry_t *workload_entry);
void insert_workload(workload_info_t *workload);

void
initialize_workloads_data(void)
{
    int status;
    if ( (status = pthread_mutex_init(&workloads_lock, NULL)) != SUCCESS) {
	tPerror(NULL, status, "pthread_mutex_init() of workloads lock failed!");
    }
    workloads = &workloadsList;
    workloads->wi_flink = workloads;
    workloads->wi_blink = workloads;
    add_predefined_workloads();
    return;
}

void
add_predefined_workloads(void)
{
    int entry;
    workload_info_t *workload;
    workload_entry_t *workload_entry;

    for (entry = 0; entry < workload_entries; entry++) {
	workload_entry = &predefined_workloads[entry];
	workload = create_workload(workload_entry);
	if (workload == NULL) break;
	insert_workload(workload);
    }
    return;
}

void
add_workload_entry(char *workload_name, char *workload_desc, char *workload_options)
{
    workload_info_t *workload;
    workload_entry_t *workload_entry;

    workload_entry = Malloc(NULL, sizeof(*workload_entry));
    if (!workload_entry) return;
    workload_entry->workload_name = strdup(workload_name);
    if (workload_desc) {
	workload_entry->workload_desc = strdup(workload_desc);
    } else {
	workload_entry->workload_desc = strdup("User Defined");
    }
    workload_entry->workload_options = strdup(workload_options);
    workload = create_workload(workload_entry);
    if (!workload) {
	Free(NULL, workload_entry->workload_name);
	Free(NULL, workload_entry->workload_options);
	Free(NULL, workload_entry);
    } else {
	insert_workload(workload);
    }
    return;
}

int
acquire_workloads_lock(void)
{
    int status = pthread_mutex_lock(&workloads_lock);
    if (status != SUCCESS) {
	tPerror(NULL, status, "Failed to acquire workloads mutex!\n");
    }
    return(status);
}

int
release_workloads_lock(void)
{
    int status = pthread_mutex_unlock(&workloads_lock);
    if (status != SUCCESS) {
	tPerror(NULL, status, "Failed to unlock workloads mutex!\n");
    }
    return(status);
}

workload_info_t *
create_workload(workload_entry_t *workload_entry)
{
    workload_info_t *workload = Malloc(NULL, sizeof(workload_info_t));
    if (workload) {
	workload->wi_entry = workload_entry;
    }
    return(workload);
}

void
insert_workload(workload_info_t *workload)
{
    workload_info_t *whdr = workloads, *wptr;
    int status;
    
    if ( (status = acquire_workloads_lock()) != SUCCESS) {
	return;
    }
    wptr = whdr->wi_blink;
    wptr->wi_flink = workload;
    workload->wi_blink = wptr;
    workload->wi_flink = whdr;
    whdr->wi_blink = workload;
    (void)release_workloads_lock();
    return;
}

workload_entry_t *
find_workload(char *workload_name)
{
    workload_info_t *whdr = workloads;
    workload_info_t *wptr = whdr->wi_flink;
    workload_entry_t *workload = NULL;

    if ( QUEUE_EMPTY(whdr) ) return(workload);

    do {
	/* Find workload entry. */
	if ( (strlen(workload_name) == strlen(wptr->wi_entry->workload_name)) &&
	     EQ(workload_name, wptr->wi_entry->workload_name) ) {
	    workload = wptr->wi_entry;
	    break;
	}
    } while ( (wptr = wptr->wi_flink) != whdr );
    return(workload);
}

void
show_workloads(dinfo_t *dip, char *workload_name)
{
    workload_info_t *whdr = workloads;
    workload_info_t *wptr = whdr->wi_flink;
    workload_entry_t *workload_entry = NULL;

    if ( QUEUE_EMPTY(whdr) ) return;

    Print(dip, "Valid Workloads:\n\n");

    do {
	workload_entry = wptr->wi_entry;
	if (workload_name) {
	    /* Allow a substring search to selectively show workloads. */
	    if ( strstr(wptr->wi_entry->workload_name, workload_name) == NULL ) {
		continue;
	    }
	}
	Print(dip, "    %s: %s\n", workload_entry->workload_name, workload_entry->workload_desc);
	Print(dip, "\t%s\n", workload_entry->workload_options);
	Print(dip, "\n");
    } while ( (wptr = wptr->wi_flink) != whdr );
    return;
}
