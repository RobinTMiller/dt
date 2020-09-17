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
 * Module:	dtstats.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	Display statistics information for generic data test program.
 *
 * Modification History:
 * 
 * December 3rd, 2016 by Robin T. Miller
 *      Expanding job statistics to include the total read/write bytes, and
 * adding Gigabyte since we are using very large and very fast disks nowadays!
 * Note: I have *not* added Gbytes to other statistics for fear of breaking
 * "screen scraping" automation. Added to just read/write % and job stats.
 * 
 * November 4th, 2016 by Robin T. Miller
 *      Add support for reporting job statistics.
 *      Added support to control the total statistics separately.
 *
 * November 21st, 2015 by Robin T. Miller
 * 	When read percentage is specified, report rhe read/write statistics.
 * 
 * November 19th, 2015 by Robin T. Miller
 * 	Switch to using higher timer resolution for calculating I/O rates, IOPS,
 * and secs per I/O. Timer is 10x more accurate with gettimeofday() (on Linux)!
 * Previous method used times() in clock cycles, resulting in zero rates/IOPS
 * with short runs seen with FS caches and flash storage which is very fast!
 * 
 * September 25th, 2014 by Robin T. Miller
 * 	Added warning to indicate the bytes written is less than requested.
 *
 * June 4th, 2014 by Robin T. Miller
 * 	Properly calculate the total files processed when a full pass has
 * not been completed. Previous the total files was reported as zero (0).
 * 
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */
#include "dt.h"

static char *stats_names[] = {
	"Copy",			/* COPY_STATS */
	"Read",			/* READ_STATS */
	"Read After Write",	/* RAW_STATS */
	"Write",		/* WRITE_STATS */
	"Total",		/* TOTAL_STATS */
        "Mirror",		/* MIRROR_STATS */
	"Verify",		/* VERIFY_STATS */
	"Job"			/* JOB_STATS */
};
static char *data_op_str = "Data operation performed";

/*
 * Forward References:
 */
void format_buffer_modes(dinfo_t *dip, char *buffer);
void report_file_lock_statistics(dinfo_t *dip, hbool_t print_header);

/*
 * Functions to Process Statistics:
 */
/* 
 * This function is used with multiple devices to propagate the
 * statistics from one device to the reporting device.
 */
void
accumulate_stats(dinfo_t *dip)
{
    dinfo_t *odip = dip->di_output_dinfo;

    if (odip == NULL) return;

    /*
     * Accumlate multiple device statistics (copy/mirror/verify).
     */
    dip->di_files_read += odip->di_files_read;
    dip->di_dbytes_read += odip->di_dbytes_read;
    dip->di_vbytes_read += odip->di_vbytes_read;
    dip->di_records_read += odip->di_records_read;
    dip->di_files_written += odip->di_files_written;
    dip->di_dbytes_written += odip->di_dbytes_written;
    dip->di_vbytes_written += odip->di_vbytes_written;
    dip->di_records_written += odip->di_records_written;
    dip->di_volume_records += odip->di_volume_records;

    odip->di_pass_total_records = (odip->di_full_reads + odip->di_full_writes);
    odip->di_pass_total_partial = (odip->di_partial_reads + odip->di_partial_writes);
    dip->di_full_reads += odip->di_full_reads;
    dip->di_full_writes += odip->di_full_writes;
    dip->di_partial_reads += odip->di_partial_reads;
    dip->di_partial_writes += odip->di_partial_writes;
    dip->di_pass_total_records += odip->di_pass_total_records;
    dip->di_pass_total_partial += odip->di_pass_total_partial;

    return;
}

void
gather_stats(struct dinfo *dip)
{
    /*
     * Gather per pass statistics.
     */
    dip->di_total_files_read += dip->di_files_read;
    dip->di_total_files_written += dip->di_files_written;
    dip->di_total_bytes_read += dip->di_dbytes_read;
    dip->di_total_bytes_written += dip->di_dbytes_written;
    dip->di_total_partial_reads += dip->di_partial_reads;
    dip->di_total_partial_writes += dip->di_partial_writes;
    dip->di_total_records_read += dip->di_full_reads;
    dip->di_total_records_written += dip->di_full_writes;
    dip->di_pass_total_records = (dip->di_full_reads + dip->di_full_writes);
    dip->di_pass_total_partial = (dip->di_partial_reads + dip->di_partial_writes);
    /*
     * Save the last data bytes written for handling "file system full".
     */
    dip->di_last_dbytes_written = dip->di_dbytes_written;
    dip->di_last_fbytes_written = dip->di_fbytes_written;
    dip->di_last_vbytes_written = dip->di_vbytes_written;
    if (dip->di_mode == READ_MODE) {
	dip->di_last_files_read = dip->di_files_read;
	if (dip->di_files_read > dip->di_max_files_read) {
	    dip->di_max_files_read = dip->di_files_read;
	}
    } else {
	dip->di_last_files_written = dip->di_files_written;
	if (dip->di_files_written > dip->di_max_files_written) {
	    dip->di_max_files_written = dip->di_files_written;
	}
    }
    dip->di_last_dir_number = dip->di_dir_number;
    dip->di_last_subdir_number = dip->di_subdir_number;
    dip->di_last_subdir_depth = dip->di_subdir_depth;
    return;
}

void
gather_totals(struct dinfo *dip)
{
    /*
     * Gather total (accumulated) statistics:
     */
    dip->di_total_bytes = (dip->di_total_bytes_read + dip->di_total_bytes_written);
    dip->di_total_files = (dip->di_total_files_read + dip->di_total_files_written);
    dip->di_total_records += dip->di_pass_total_records;
    dip->di_total_partial += dip->di_pass_total_partial;
    //dip->di_total_errors += dip->di_error_count;
    return;
}

void
init_stats(struct dinfo *dip)
{
    /*
     * Initialize fields in preparation for the next pass.
     */
    /* Note: Now used for the total errors for all passes! */
    //dip->di_error_count = (u_long) 0;
    dip->di_end_of_file = False;
    dip->di_end_of_media = False;
    dip->di_end_of_logical = False;
    dip->di_beginning_of_file = False;
    dip->di_file_system_full = False;
    dip->di_no_space_left = False;
    dip->di_dir_number = (u_long) 0;
    dip->di_subdir_depth = (u_long) 0;
    dip->di_subdir_number = (u_long) 0;
    dip->di_file_number = (u_long) 0;
    dip->di_files_read = (u_long) 0;
    dip->di_fbytes_read = (large_t) 0;
    dip->di_dbytes_read = (large_t) 0;
    dip->di_lbytes_read = (large_t) 0;
    dip->di_vbytes_read = (large_t) 0;
    dip->di_records_read = (u_long) 0;
    dip->di_files_written = (u_long) 0;
    dip->di_fbytes_written = (large_t) 0;
    dip->di_dbytes_written = (large_t) 0;
    dip->di_lbytes_written = (large_t) 0;
    dip->di_vbytes_written = (large_t) 0;
    dip->di_records_written = (u_long) 0;
    dip->di_volume_records = (u_long) 0;
    dip->di_full_reads = (u_long) 0;
    dip->di_full_writes = (u_long) 0;
    dip->di_partial_reads = (u_long) 0;
    dip->di_partial_writes = (u_long) 0;
    dip->di_discarded_write_data = (large_t) 0;
    dip->di_maxdata_read = (large_t) 0;
    dip->di_maxdata_written = (large_t) 0;
    dip->di_pass_total_records = (u_long) 0;
    /*
     * Must free this to force starting at top of tree.
     */
    if (dip->di_subdir) {
	Free(dip, dip->di_subdir);
	dip->di_subdir = NULL;
    }
    return;
}

/************************************************************************
 *									*
 * report_pass() - Report end of pass information.			*
 *									*
 * Inputs:								*
 *	dip = The device information pointer.				*
 *	stats_type = Type of statistics to display (read/write/total).	*
 *									*
 ************************************************************************/
void
report_pass(dinfo_t *dip, stats_t stats_type)
{
    gather_stats(dip);		/* Gather the total statistics.	*/
    gather_totals(dip);		/* Update the total statistics. */

    if (dip->di_stats_level != STATS_NONE) {
	if (dip->di_pstats_flag && (dip->di_stats_level == STATS_FULL) ) {
	    if (dip->di_stats_flag) {
		report_stats(dip, stats_type);
	    }
	} else if ( ((dip->di_pstats_flag == False) && dip->di_verbose_flag) || /* compatability */
		    ((dip->di_pstats_flag == True) && (dip->di_stats_level == STATS_BRIEF)) ) {
	    /* Overloaded, need to restructure! */
	    if (dip->di_stats_level == STATS_BRIEF) {
		/*
		 * Note: Empty keepalive setup for monitoring no I/O progress.
		 *	 Backwards compatability, messy, so may be time to go!
		 */
		if ( (dip->di_user_keepalive && strlen(dip->di_keepalive)) && !dip->di_user_pkeepalive &&
		     (time((time_t *)0) > dip->di_last_alarm_time) ) {
		    dip->di_log_bufptr += FmtKeepAlive(dip, dip->di_keepalive, dip->di_log_bufptr);
		    Lprintf(dip, "\n");
		    Lflush(dip);
		}
	    }
	    if ( dip->di_pkeepalive && strlen(dip->di_pkeepalive) ) {
		/* TODO: Make stats type available in FmtKeepAlive()! */
		Lprintf(dip, "End of %s ", stats_names[(int)stats_type]);
		dip->di_log_bufptr += FmtKeepAlive(dip, dip->di_pkeepalive, dip->di_log_bufptr);
		Lprintf(dip, "\n");
		Lflush(dip);
	    }
	}
    } /* if (dip->di_stats_level != STATS_NONE) */

    /*
     * To help with triage, report whether the requested data limit was *not* reached.
     */
    if ( (dip->di_iobehavior == DT_IO) && (dip->di_iolock == False) &&
	 (dip->di_fsfile_flag == True) && (dip->di_verbose_flag == True) &&
	 (dip->di_read_percentage == 0) && (dip->di_random_percentage == 0) &&
	 ((stats_type == RAW_STATS) || (stats_type == WRITE_STATS) ) &&
	 (dip->di_max_data == 0) && (dip->di_fsincr_flag == False) && (dip->di_verbose_flag == True) ) {
	 large_t max_files = calculate_max_files(dip);
	 large_t max_data = (dip->di_max_data) ? dip->di_max_data : dip->di_data_limit;
	 /* This check is for data written per thread. */
	 if (dip->di_dbytes_written < max_data) {
	     Wprintf(dip, "The bytes written "LUF", is less than the data limit "LUF" requested!\n",
		     dip->di_dbytes_written, max_data);
	 }
    }
    /*
     * Re-initialize the per pass counters.
     */
    init_stats(dip);
    /* For Copy/Mirror/Verify, clear the output stats too! */
    if (dip->di_output_dinfo) {
	init_stats(dip->di_output_dinfo);
    }
    return;
}

/************************************************************************
 *									*
 * report_stats() - Report statistics at end of pass or program.	*
 *									*
 * Inputs:								*
 *	dip = The device information pointer.				*
 *	stats_type = Type of statistics to display (read/write/total).	*
 *									*
 ************************************************************************/
void
report_stats(struct dinfo *dip, enum stats stats_type)
{
    double bytes_sec, kbytes_sec, ios_sec, secs_io, msecs_io;
    double Kbytes, Mbytes, Gbytes;
    large_t xfer_bytes, xfer_records;
    large_t bytes_read, bytes_written;
    large_t records_read, records_written;
    unsigned long xfer_partial;
    uint64_t usecs;
    double elapsed;
#if defined(WIN32)
    clock_t et;
#else
    clock_t at, et;
#endif
    struct tms *stms, *etms = &dip->di_etimes;
    char pbuf[MEDIUM_BUFFER_SIZE];
    char *pinfo = pbuf;

    if ( (dip->di_stats_flag == False) || (dip->di_stats_level == STATS_NONE) ) return;
    if ( (stats_type == TOTAL_STATS) && (dip->di_total_stats_flag == False) ) return;

    if ( (dip->di_stats_level == STATS_BRIEF) &&
	 ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) ) {
	/* Overloaded, need to restructure! */
	if (dip->di_stats_level == STATS_BRIEF) {
	    if ( dip->di_user_keepalive && !dip->di_user_tkeepalive &&
		 (time((time_t *)0) > dip->di_last_alarm_time) ) {
		if ( FmtKeepAlive(dip, dip->di_keepalive, dip->di_log_buffer) ) {
		    LogMsg (dip, dip->di_ofp, logLevelLog, 0, "%s\n", dip->di_log_buffer);
		}
	    }
	}
	if ( dip->di_tkeepalive && strlen(dip->di_tkeepalive) ) {
	    /* WTF? Need better explanation! */
	    init_stats(dip);	/* Init to get correct totals! */
	    if (dip->di_output_dinfo) {
		init_stats(dip->di_output_dinfo);
	    }
	    if ( FmtKeepAlive(dip, dip->di_tkeepalive, dip->di_log_buffer) ) {
		LogMsg (dip, dip->di_ofp, logLevelLog, 0, "%s\n", dip->di_log_buffer);
	    }
	}
	return;
    }
    dip->di_end_time = times(etms);
    gettimeofday(&dip->di_end_timer, NULL);

    if ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) {
	report_os_information(dip, True);
    	report_file_system_information(dip, True, False);
	report_file_lock_statistics(dip, True);
	report_scsi_summary(dip, True);
	if (dip->di_output_dinfo) {
	    report_file_system_information(dip->di_output_dinfo, True, False);
	    report_scsi_summary(dip->di_output_dinfo, True);
	}
    }

    Lprintf (dip, "\n%s Statistics:\n", stats_names[(int)stats_type]);

    if ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) {
	et = dip->di_end_time - dip->di_start_time;	/* Test elapsed time.	*/
	usecs = timer_diff(&dip->di_start_timer, &dip->di_end_timer);
	elapsed = ((double)usecs / (double)uSECS_PER_SEC);
	stms = &dip->di_stimes;				/* Test start times.	*/
	bytes_read = dip->di_total_bytes_read;		/* Total bytes read.	*/
	bytes_written = dip->di_total_bytes_written;	/* Total bytes written.	*/
	records_read = dip->di_total_records_read;	/* Total bytes read.	*/
	records_read += dip->di_total_partial_reads;	/* Accumulate partials.	*/
	records_written = dip->di_total_records_written;/* Total bytes written.	*/
	records_written += dip->di_total_partial_writes;/* Accumulate partials.	*/
	xfer_bytes = dip->di_total_bytes;		/* Total bytes xferred.	*/
	xfer_records = dip->di_total_records;		/* Total records xfered	*/
	xfer_partial = dip->di_total_partial;		/* Total partial records*/
    } else { /* Pass Statistics */
	et = dip->di_end_time - dip->di_pass_time;	/* Pass elapsed time.	*/
	usecs = timer_diff(&dip->di_pass_timer, &dip->di_end_timer);
	elapsed = ((double)usecs / (double)uSECS_PER_SEC);
	stms = &dip->di_ptimes;				/* Test pass times.	*/
	bytes_read = dip->di_dbytes_read;		/* Total bytes read.	*/
	bytes_written = dip->di_dbytes_written;		/* Total bytes written.	*/
	records_read = dip->di_full_reads;		/* Total bytes read.	*/
	records_read += dip->di_partial_reads;		/* Accumulate partials.	*/
	records_written = dip->di_full_writes;		/* Total bytes written.	*/
	records_written += dip->di_partial_writes;	/* Accumulate partials.	*/
	xfer_bytes = dip->di_dbytes_read;		/* Data bytes per pass.	*/
	xfer_bytes += dip->di_dbytes_written;		/* Data bytes written.	*/
	xfer_records = dip->di_pass_total_records;	/* Data records/pass.	*/
	xfer_partial = dip->di_pass_total_partial;	/* Partial records.	*/
    }

    if ( (dip->di_multiple_devs == True) ||
	 (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) {
	/*
	 * Display device tested & other device information.
	 */
	if (dip->di_input_file) {
	    struct dtype *dtp = dip->di_dtype;
	    Lprintf (dip, DT_FIELD_WIDTH "%s",
		     "Input device/file name", dip->di_dname);
	    if (dtp && (dtp->dt_dtype != DT_UNKNOWN) ) {
		if (dip->di_device != NULL) {
		    Lprintf (dip, " (Device: %s, type=%s)\n",
			     dip->di_device, dtp->dt_type);
		} else {
		    Lprintf (dip, " (device type=%s)\n", dtp->dt_type);
		}
	    } else {
		Lprintf (dip, "\n");
	    }
	} else {
	    struct dtype *dtp = dip->di_dtype;
	    Lprintf (dip, DT_FIELD_WIDTH "%s",
		     "Output device/file name", dip->di_dname);
	    if (dtp && (dtp->dt_dtype != DT_UNKNOWN) ) {
		if (dip->di_device != NULL) {
		    Lprintf (dip, " (Device: %s, type=%s)\n",
			     dip->di_device, dtp->dt_type);
		} else {
		    Lprintf (dip, " (device type=%s)\n", dtp->dt_type);
		}
	    } else {
		Lprintf (dip, "\n");
	    }
	}
    } /* end 'if (stats_type == TOTAL_STATS)' */
    
    /* Note: Needless to say, this is getting way to messy, time for a cleanup! */
    /*	     But that said, also trying to keep output backwards compatable! */
    if ( ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) ||
	 dip->di_vary_iodir || dip->di_vary_iotype) {
	Lprintf(dip, DT_FIELD_WIDTH, "Type of I/O's performed");
	if (dip->di_io_type == RANDOM_IO) {
	    Lprintf(dip, "random (rseed=" LXF, dip->di_random_seed);
	} else {
	    Lprintf(dip, "sequential (%s",
		     (dip->di_io_dir == FORWARD) ? "forward" : "reverse");
	    if ( UseRandomSeed(dip) ) {
		Lprintf(dip, ", rseed=" LXF, dip->di_random_seed);
	    }
	}
	if (dip->di_raw_flag) {
	    Lprintf(dip, ", read-after-write)\n");
	} else {
	    Lprintf(dip, ")\n");
	}
    } else if ( UseRandomSeed(dip) ) { /* Seed changes on each pass. */
	Lprintf(dip, DT_FIELD_WIDTH LXF "\n", "Current random seed", dip->di_random_seed);
    }
    
    if (stats_type == TOTAL_STATS) {
	if (dip->di_io_type == RANDOM_IO) {
	    Lprintf (dip, DT_FIELD_WIDTH, "Random I/O Parameters");
	    Lprintf (dip, "offset=" FUF ", ralign=" FUF ", rlimit=" LUF "\n",
		     dip->di_file_position, dip->di_random_align, dip->di_rdata_limit);
	} else if (dip->di_slices) {
	    Lprintf (dip, DT_FIELD_WIDTH, "Slice Range Parameters");
	    Lprintf (dip, "offset=" FUF " (lba " LUF "), limit=" LUF "\n",
		     dip->di_file_position, (large_t)(dip->di_file_position / dip->di_dsize), dip->di_data_limit);
	}

	if (dip->di_align_offset || dip->di_rotate_flag) {
	    Lprintf (dip, DT_FIELD_WIDTH, "Buffer alignment options");
	    if (dip->di_align_offset) {
		Lprintf (dip, "alignment offset = %d bytes\n", dip->di_align_offset);
	    } else {
		Lprintf (dip, "rotating through 1st %d bytes\n", ROTATE_SIZE);
	    }
	}
    } /* end 'if (stats_type == TOTAL_STATS)' */

    if (stats_type == JOB_STATS) {
	if (dip->di_job->ji_job_tag) {
	    Lprintf(dip, DT_FIELD_WIDTH "Job %u, Tag=%s\n",
		    "Job Information Reported", dip->di_job->ji_job_id, dip->di_job->ji_job_tag);
	} else {
	    Lprintf(dip, DT_FIELD_WIDTH "Job %u\n",
		    "Job Information Reported", dip->di_job->ji_job_id);
	}
    } else {
	Lprintf(dip, DT_FIELD_WIDTH "Job %u, Thread %u\n",
		"Job Information Reported", dip->di_job->ji_job_id, dip->di_thread_number);
    }

    if (stats_type == JOB_STATS) {
	if (dip->di_slices) {
	    Lprintf(dip, DT_FIELD_WIDTH "%d\n", "Number of slices", dip->di_slices);
	} else {
	    Lprintf(dip, DT_FIELD_WIDTH "%d\n", "Number of threads", dip->di_threads);
	}
    } else {
	if (dip->di_slices) {
	    Lprintf (dip, DT_FIELD_WIDTH "%d/%d\n",
		     "Current Slice Reported", dip->di_slice_number, dip->di_slices);
	} else if (dip->di_job->ji_tinfo->ti_threads > 1) {
	    int active_threads = dip->di_job->ji_tinfo->ti_threads;
	    Lprintf (dip, DT_FIELD_WIDTH "%d/%d\n",
		     "Current Thread Reported", dip->di_thread_number, active_threads);
	}
    }

    if ( (dip->di_io_mode == MIRROR_MODE) || (dip->di_io_mode == TEST_MODE) ) {
	if ( (dip->di_io_mode == MIRROR_MODE) && dip->di_output_dinfo ) {
	    Lprintf (dip, DT_FIELD_WIDTH "Wrote '%s' verified '%s'.\n",
		     data_op_str,
		     dip->di_output_dinfo->di_dname,
		     dip->di_dname);
	}
	/*
	 * Extra information regarding pattern verification.
	 */
	memset(pinfo, '\0', sizeof(*pinfo));
	if (dip->di_output_file && !dip->di_verify_flag) {
	    pinfo = " (read verify disabled)";
	} else if (!dip->di_compare_flag) {
	    pinfo = " (data compare disabled)";
	} else if (dip->di_incr_pattern) {
	    sprintf(pinfo, " (incrementing 0-255)");
	} else if (dip->di_iot_pattern) {
	    sprintf(pinfo, " (blocking is %u bytes)", (unsigned int)dip->di_lbdata_size);
	} else if (dip->di_pattern_file || dip->di_pattern_string) {
	    sprintf(pinfo, " (first %u bytes)", (unsigned int)sizeof(dip->di_pattern));
	} else if (dip->di_lbdata_flag) {
	    sprintf(pinfo, " (w/lbdata, lba %u, size %u bytes)",
		    dip->di_lbdata_addr, (unsigned int)dip->di_lbdata_size);
	}
	if ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) || (dip->di_pass_limit > 1L) ) {
	    if (dip->di_fprefix_string) {
		Lprintf (dip, DT_FIELD_WIDTH "%s\n",
			 "Data pattern prefix used", dip->di_fprefix_string);
	    }
	    if (dip->di_pattern_file) {
		Lprintf (dip, DT_FIELD_WIDTH "%s (%lu bytes)\n",
			 "Data pattern file used", dip->di_pattern_file, dip->di_pattern_bufsize);
	    } else if (dip->di_pattern_string) {
		Lprintf (dip, DT_FIELD_WIDTH "'%s'%s\n",
			 "Data pattern string used", dip->di_pattern_string,
			 (dip->di_iot_pattern) ? pinfo : "");
	    }
	}
	if (dip->di_iot_pattern) {
	    Lprintf (dip, DT_FIELD_WIDTH "0x%08x\n",
		     "Last IOT seed value used", dip->di_iot_seed_per_pass);
	}
        if (!dip->di_iot_pattern) {
	    if (dip->di_output_file && dip->di_verify_flag) {
		Lprintf (dip, DT_FIELD_WIDTH "0x%08x%s\n",
			 "Data pattern read/written", dip->di_pattern, pinfo);
	    } else if (dip->di_output_file && !dip->di_verify_flag) {
		Lprintf (dip, DT_FIELD_WIDTH "0x%08x%s\n",
			 "Data pattern written", dip->di_pattern, pinfo);
	    } else {
		Lprintf (dip, DT_FIELD_WIDTH "0x%08x%s\n",
			 "Data pattern read", dip->di_pattern, pinfo);
	    }
	}
	if (dip->di_btag_flag == True) {
	    Lprintf (dip, DT_FIELD_WIDTH "0x%08x\n",
		     "Block tag verify flags", dip->di_btag_vflags);
	}
	if (dip->di_buffer_mode) {
	    if ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) {
		char buffer[MEDIUM_BUFFER_SIZE];
		Lprintf(dip, DT_FIELD_WIDTH, "File system buffer modes");
		format_buffer_modes(dip, buffer);
		Lprintf(dip, "%s\n", buffer);
	    } else {
		Lprintf (dip, DT_FIELD_WIDTH "%s\n", "File system buffer mode", dip->di_bufmode_type);
	    }
	}
    } else { /* !MIRROR_MODE && !TEST_MODE */
	if ( (stats_type == COPY_STATS) ||
	     ( ((stats_type == JOB_STATS) || (stats_type == TOTAL_STATS)) && !dip->di_verify_flag) ) {
	    Lprintf (dip, DT_FIELD_WIDTH "Copied '%s' to '%s'.\n",
		     data_op_str,
		     dip->di_dname,
		     dip->di_output_dinfo->di_dname);
	} else if ( (stats_type == VERIFY_STATS) ||
		    ( ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) && dip->di_verify_only) ) {
	    Lprintf (dip, DT_FIELD_WIDTH "Verified '%s' with '%s'.\n",
		     data_op_str,
		     dip->di_dname,
		     dip->di_output_dinfo->di_dname);
	}
    } /* end 'if (dip->di_io_mode == TEST_MDOE)' */

    if (stats_type != JOB_STATS) {
	/*
	 * Report the capacity or max data percentage calculated, if specified.
	 */
	if (dip->di_capacity_percentage && dip->di_user_capacity) {
	    large_t data_bytes = dip->di_user_capacity;
	    Mbytes = (double) ( (double)data_bytes / (double)MBYTE_SIZE);
	    Gbytes = (double) ( (double)data_bytes / (double)GBYTE_SIZE);
	    Lprintf (dip, DT_FIELD_WIDTH LUF " (%.3f Mbytes, %.3f Gbytes)\n",
		     "Data capacity calculated", data_bytes, Mbytes, Gbytes);
	}
	if (dip->di_max_data_percentage && dip->di_max_data) {
	    large_t data_bytes = dip->di_max_data;
	    Mbytes = (double) ( (double)data_bytes / (double)MBYTE_SIZE);
	    Gbytes = (double) ( (double)data_bytes / (double)GBYTE_SIZE);
	    Lprintf (dip, DT_FIELD_WIDTH LUF " (%.3f Mbytes, %.3f Gbytes)\n",
		     "Maximum data calculated", data_bytes, Mbytes, Gbytes);
	}
    }

    /*
     * Report reads/writes when percentage is specified.
     */
    if (dip->di_read_percentage) {

	Lprintf (dip, DT_FIELD_WIDTH LUF " (%d%%)\n",
		 "Total records read", records_read,
		 (records_read) ? (int)( (((double)records_read / (double)xfer_records) + .005) * 100) : 0 );

	Kbytes = (double) ( (double) bytes_read / (double) KBYTE_SIZE);
	Mbytes = (double) ( (double) bytes_read / (double) MBYTE_SIZE);
	Gbytes = (double) ( (double) bytes_read / (double) GBYTE_SIZE);

	Lprintf (dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes, %.3f Gbytes)\n",
		 "Total bytes read",
		 bytes_read, Kbytes, Mbytes, Gbytes);

	Lprintf (dip, DT_FIELD_WIDTH LUF " (%d%%)\n",
		 "Total records written", records_written,
		 (records_written) ? (int)( (((double)records_written / (double)xfer_records) + .005) * 100) : 0 );

	Kbytes = (double) ( (double) bytes_written / (double) KBYTE_SIZE);
	Mbytes = (double) ( (double) bytes_written / (double) MBYTE_SIZE);
	Gbytes = (double) ( (double) bytes_written / (double) GBYTE_SIZE);

	Lprintf (dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes, %.3f Gbytes)\n",
		 "Total bytes written",
		 bytes_written, Kbytes, Mbytes, Gbytes);

    } else if ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) {

	/* Note: Mostly a duplicate above, but keeping it simple! */
	Lprintf (dip, DT_FIELD_WIDTH LUF "\n",
		 "Total records read", records_read);

	Kbytes = (double) ( (double) bytes_read / (double) KBYTE_SIZE);
	Mbytes = (double) ( (double) bytes_read / (double) MBYTE_SIZE);
	Gbytes = (double) ( (double) bytes_read / (double) GBYTE_SIZE);

	Lprintf (dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes, %.3f Gbytes)\n",
		 "Total bytes read",
		 bytes_read, Kbytes, Mbytes, Gbytes);

	Lprintf (dip, DT_FIELD_WIDTH LUF "\n",
		 "Total records written", records_written);

	Kbytes = (double) ( (double) bytes_written / (double) KBYTE_SIZE);
	Mbytes = (double) ( (double) bytes_written / (double) MBYTE_SIZE);
	Gbytes = (double) ( (double) bytes_written / (double) GBYTE_SIZE);

	Lprintf (dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes, %.3f Gbytes)\n",
		 "Total bytes written",
		 bytes_written, Kbytes, Mbytes, Gbytes);
    }

    if (dip->di_min_size) {
	Lprintf (dip, DT_FIELD_WIDTH LUF " with min=%ld, max=%ld, incr=",
		 "Total records processed",
		 (xfer_records + xfer_partial),
		 dip->di_min_size, dip->di_max_size);
	if (dip->di_variable_flag) {
	    Lprintf (dip, "variable\n");
	} else {
	    Lprintf (dip, "%ld\n", dip->di_incr_count);
	}
    } else {
	Lprintf (dip, DT_FIELD_WIDTH LUF " @ %ld bytes/record",
		 "Total records processed",
		 xfer_records, dip->di_block_size);
	if (xfer_partial) {
	    Lprintf (dip, ", %lu partial\n", xfer_partial);
	} else {
	    Lprintf (dip, " (%.3f Kbytes)\n",
		     ((double)dip->di_block_size / (double) KBYTE_SIZE));
	}
    }

    Kbytes = (double) ( (double) xfer_bytes / (double) KBYTE_SIZE);
    Mbytes = (double) ( (double) xfer_bytes / (double) MBYTE_SIZE);

    Lprintf (dip, DT_FIELD_WIDTH LUF " (%.3f Kbytes, %.3f Mbytes)\n",
	     "Total bytes transferred",
	     xfer_bytes, Kbytes, Mbytes);

    /*
     * Calculate the transfer rates.
     */
    if (elapsed) {
	bytes_sec = ((double)xfer_bytes / elapsed);
    } else {
	bytes_sec = 0.0;
    }
    kbytes_sec = (double) ( (double) bytes_sec / (double) KBYTE_SIZE);
    Lprintf (dip, DT_FIELD_WIDTH "%.0f bytes/sec, %.3f Kbytes/sec\n",
	     "Average transfer rates",
	     bytes_sec, kbytes_sec);
#if defined(AIO)
    if ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) {
	if (dip->di_aio_flag) {
	    Lprintf(dip, DT_FIELD_WIDTH "%d\n", "Asynchronous I/O's used", dip->di_aio_bufs);
	}
    }
#endif /* defined(AIO) */

    if (elapsed && xfer_records) {
	double records = (double)(xfer_records + xfer_partial);
	double secs = elapsed;
	ios_sec = (records / secs);
	secs_io = (secs / records);
	msecs_io = ( ((double)usecs / (double)MSECS) / records);
    } else {
	ios_sec = 0.0;
	secs_io = 0.0;
	msecs_io = 0.0;
    }
    /* Note: These stats are for all operations, *not* just I/O! Could be misleading!  */
    Lprintf (dip, DT_FIELD_WIDTH "%.3f\n", "Number I/O's per second", ios_sec);
    Lprintf (dip, DT_FIELD_WIDTH "%.4f (%.2fms)\n", "Number seconds per I/O", secs_io, msecs_io);
    
    if ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) {
	/* Accumlate last no-progress (if any). */
	if (dip->di_cur_max_noprogt) {
	    dip->di_total_max_noprogs++;
	    dip->di_total_max_noprogt += dip->di_cur_max_noprogt;
	    dip->di_cur_max_noprogt = 0;
	}
	if (dip->di_total_max_noprogs) {
	    double average_noprogt = ((double)dip->di_total_max_noprogt / (double)dip->di_total_max_noprogs);
	    Lprintf(dip, DT_FIELD_WIDTH "%.2f secs\n", "Average no-progress time", average_noprogt);
	}
	if (dip->di_max_noprogt) {
	    char *optmsg = optiming_table[dip->di_max_noprog_optype].opt_name;
	    Lprintf(dip, DT_FIELD_WIDTH TMF " secs\n", "Maximum no-progress time", dip->di_max_noprogt);
	    if (optmsg) {
		Lprintf(dip, DT_FIELD_WIDTH "%s", "Max no-progress operation", optmsg);
		(void)os_ctime(&dip->di_max_noprog_time, dip->di_time_buffer, sizeof(dip->di_time_buffer));
		Lprintf(dip, " @ %s\n", dip->di_time_buffer);
	    }
	}
    }
    if (dip->di_multi_flag || dip->di_volumes_flag) {
	Lprintf (dip, DT_FIELD_WIDTH, "Total volumes completed");
	if (dip->di_volumes_flag) {
	    Lprintf (dip, "%d/%d\n", dip->di_multi_volume, dip->di_volume_limit);
	} else {
	    Lprintf (dip, "%d\n", dip->di_multi_volume);
	}
    }

    Lprintf (dip, DT_FIELD_WIDTH, "Total passes completed");
    if (dip->di_runtime) {
	Lprintf (dip, "%lu\n", dip->di_pass_count);
    } else {
	Lprintf (dip, "%lu/%lu\n", dip->di_pass_count, dip->di_pass_limit);
    }

     if (dip->di_file_limit || dip->di_user_dir_limit || dip->di_user_subdir_limit || dip->di_user_subdir_depth) {
	large_t max_files = calculate_max_files(dip);

	if ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) {
	    if (dip->di_output_file && dip->di_verify_flag) {
		/*
		 * Handle multiple files for both read and write pass.
		 */
		if ( (!dip->di_raw_flag || (dip->di_raw_flag && dip->di_reread_flag)) && (dip->di_total_files > max_files) ) {
		    max_files *= 2; /* Adjust for read + write passes. */
		}
	    } else if ((dip->di_io_mode == COPY_MODE) && dip->di_verify_flag) {
		max_files *= 2;	/* Adjust for copy + verify passes. */
	    }
	}
	Lprintf (dip, DT_FIELD_WIDTH, "Total files processed");
	if ( ( (stats_type == JOB_STATS) || (stats_type == TOTAL_STATS) ) && dip->di_total_files) {
	    if (dip->di_pass_count) max_files *= dip->di_pass_count;
	    if (stats_type == JOB_STATS) max_files *= dip->di_threads;
	    Lprintf(dip, LUF "/" LUF "\n", dip->di_total_files, max_files);
	} else {
	    large_t total_files = (dip->di_files_read + dip->di_files_written);
	    Lprintf (dip, LUF "/" LUF "\n", total_files, max_files);
	}
    }

    Lprintf (dip, DT_FIELD_WIDTH "%lu/%lu",
	     "Total errors detected",
	     dip->di_error_count, dip->di_error_limit);
    Lprintf(dip, "\n");

    /*
     * Report elapsed, user, and system times.
     */
    Lprintf (dip, DT_FIELD_WIDTH, "Total elapsed time");
    format_time (dip, et);
#if !defined(WIN32)
    /*
     * More ugliness for Windows, since the system and
     * user times are always zero, don't display them.
     */
    Lprintf (dip, DT_FIELD_WIDTH, "Total system time");
    at = etms->tms_stime - stms->tms_stime;
    at += etms->tms_cstime - stms->tms_cstime;
    format_time (dip, at);

    Lprintf (dip, DT_FIELD_WIDTH, "Total user time");
    at = etms->tms_utime - stms->tms_utime;
    at += etms->tms_cutime - stms->tms_cutime;
    format_time (dip, at);
#endif /* !defined(WIN32) */

    Lprintf (dip, DT_FIELD_WIDTH, "Starting time");
    Ctime(dip, dip->di_program_start);
    Lprintf(dip, "\n");
    dip->di_program_end = time((time_t) 0);
    Lprintf (dip, DT_FIELD_WIDTH, "Ending time");
    Ctime(dip, dip->di_program_end);
    Lprintf(dip, "\n");
    if (stats_type == TOTAL_STATS) {
	Lprintf (dip, "\n");
    }
    Lflush(dip);
}

void
format_buffer_modes(dinfo_t *dip, char *buffer)
{
    char *bp = buffer;
    int bindex;
    
    *bp = '\0';
    for (bindex = 0; bindex < dip->di_bufmode_count; bindex++) {
	switch (dip->di_buffer_modes[bindex]) {
	    case BUFFERED_IO:
		bp += sprintf(bp, "buffered,");
		break;
	    case UNBUFFERED_IO:
		bp += sprintf(bp, "unbuffered,");
		break;
	    case CACHE_READS:
		bp += sprintf(bp, "cachereads,");
		break;
	    case CACHE_WRITES:
		bp += sprintf(bp, "cachewrites,");
		break;
	    default:
		break;
	}
    }
    if (bp[-1] == ',') bp[-1] = '\0';
    return;
}

void
report_os_information(dinfo_t *dip, hbool_t print_header)
{
    char *bp;

    if (print_header == True) {
	Lprintf(dip, "\nOperating System Information:\n");
    }
    if ( bp = os_gethostname() ) {
	char *host = bp;
	char *address;
	Lprintf(dip, DT_FIELD_WIDTH "%s", "Host name", host);
	if ( address = os_getaddrinfo(dip, host, dip->di_inet_family, NULL, NULL) ) {
	    Lprintf(dip, " (%s)\n", address);
	    Free(dip, address);
	} else {
	    Lprintf(dip, "\n", address);
	}
	Free(dip, host);
    }
    if ( bp = os_getusername() ) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n", "User name", bp);
	Free(dip, bp); bp = NULL;
    }
    Lprintf(dip, DT_FIELD_WIDTH "%d\n", "Process ID", os_getpid());
    if ( bp = os_getosinfo() ) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n", "OS information", bp);
	Free(dip, bp); bp = NULL;
    }
    return;
}

void
report_file_system_information(dinfo_t *dip, hbool_t print_header, hbool_t report_free_space)
{
    double Mbytes, Gbytes, Tbytes;

#if defined(WIN32)
    if ( (print_header == True) &&
	 (dip->di_volume_name || dip->di_universal_name || dip->di_filesystem_type ||
	  dip->di_fs_block_size || dip->di_volume_path_name ||
	  dip->di_volume_serial_number || dip->di_protocol_version) ) {
	Lprintf(dip, "\nFile System Information:\n");
    }
    if (dip->di_multiple_devs == True) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n",
		"File name", dip->di_dname);
    }
    if (dip->di_volume_name) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n",
		"Volume name", dip->di_volume_name);
    }
    if (dip->di_universal_name) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n",
		"Universal name", dip->di_universal_name);
    }
    if (dip->di_filesystem_type) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n",
		"Filesystem type", dip->di_filesystem_type);
    }
    if (dip->di_fs_block_size) {
	Lprintf(dip, DT_FIELD_WIDTH "%u\n",
		"Filesystem block size", dip->di_fs_block_size);
    }
    /* Note: Free space is not updated for total statistics. */
    /* Why? If we've stopped I/O on the array, we cannot get it! */
    if (report_free_space && dip->di_fs_space_free) {
        large_t data_bytes = dip->di_fs_space_free;
	Mbytes = (double) ( (double)data_bytes / (double)MBYTE_SIZE);
	Gbytes = (double) ( (double)data_bytes / (double)GBYTE_SIZE);
	Tbytes = (double) ( (double)data_bytes / (double)TBYTE_SIZE);
	Lprintf(dip, DT_FIELD_WIDTH "%u (%.3f Mbytes, %.3f Gbytes, %.3f Tbytes)\n",
		"Filesystem free space", data_bytes, Mbytes, Gbytes, Tbytes);
    }
    if (dip->di_fs_total_space) {
        large_t data_bytes = dip->di_fs_total_space;
	Mbytes = (double) ( (double)data_bytes / (double)MBYTE_SIZE);
	Gbytes = (double) ( (double)data_bytes / (double)GBYTE_SIZE);
	Tbytes = (double) ( (double)data_bytes / (double)TBYTE_SIZE);
	Lprintf(dip, DT_FIELD_WIDTH "%u (%.3f Mbytes, %.3f Gbytes, %.3f Tbytes)\n",
		"Filesystem total space", data_bytes, Mbytes, Gbytes, Tbytes);
    }
    if (dip->di_volume_path_name) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n",
		"Volume path name", dip->di_volume_path_name);
    }
    if (dip->di_volume_serial_number) {
	Lprintf(dip, DT_FIELD_WIDTH "%u\n",
		"Volume serial number", dip->di_volume_serial_number);
    }
    if (dip->di_protocol_version) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n",
		"Remote Protocol Version", dip->di_protocol_version);
    }
#else /* !defined(WIN32) */
    if ( (print_header == True) &&
	 (dip->di_mounted_from_device || dip->di_mounted_on_dir || dip->di_filesystem_type ||
	  dip->di_filesystem_options || dip->di_fs_block_size) ) {
	Lprintf(dip, "\nFile System Information:\n");
    }
    if (dip->di_mounted_from_device) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n",
		"Mounted from device", dip->di_mounted_from_device);
    }
    if (dip->di_mounted_on_dir) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n",
		"Mounted on directory", dip->di_mounted_on_dir);
    }
    if (dip->di_filesystem_type) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n",
		"Filesystem type", dip->di_filesystem_type);
    }
    if (dip->di_filesystem_options) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n",
		"Filesystem options", dip->di_filesystem_options);
    }
    if (dip->di_fs_block_size) {
	Lprintf(dip, DT_FIELD_WIDTH "%u\n",
		"Filesystem block size", dip->di_fs_block_size);
    }
    /* Note: Free space is not updated for total statistics. */
    /* Why? If we've stopped I/O on the array, we cannot get it! */
    if (report_free_space && dip->di_fs_space_free) {
	Lprintf(dip, DT_FIELD_WIDTH "%u\n",
		"Filesystem free space", dip->di_fs_space_free);
    }
    if (dip->di_fs_total_space) {
	Lprintf(dip, DT_FIELD_WIDTH "%u\n",
		"Filesystem total space", dip->di_fs_total_space);
    }
#endif /* defined(WIN32) */
    return;
}

void
report_file_lock_statistics(dinfo_t *dip, hbool_t print_header)
{
    if (dip->di_lock_files == False) return;
    if (print_header == True) {
	Lprintf(dip, "\nFile Lock Statistics:\n");
    } else {
	Lprintf(dip, "\n");
    }
    Lprintf(dip, DT_FIELD_WIDTH "%s ",
	    "Lock mode name", dip->di_lock_mode_name);
    Lprintf(dip, "(range %d-%d%%)\n",
	    dip->di_lock_mode[LOCK_RANGE_FULL].lower,
	    dip->di_lock_mode[LOCK_RANGE_FULL].upper);
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n",
	    "Number of read locks", dip->di_lock_stats[LOCK_TYPE_READ]);
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n",
	    "Number of write locks", dip->di_lock_stats[LOCK_TYPE_WRITE]);
    Lprintf(dip, DT_FIELD_WIDTH LUF "\n",
	    "Total number of unlocks", dip->di_lock_stats[LOCK_TYPE_UNLOCK]);
    Lprintf(dip, DT_FIELD_WIDTH "%u\n",
	    "Total number of lock errors", dip->di_lock_errors);
    return;
}

void
report_scsi_summary(dinfo_t *dip, hbool_t print_header)
{
#if defined(SCSI)
    if (dip->di_scsi_flag == False) return;
    if (dip->di_scsi_info_flag == False) return;
    
    if (print_header == True) {
	Lprintf(dip, "\nSCSI Information:\n");
    } else {
	Lprintf(dip, "\n");
    }
    if (dip->di_io_mode != TEST_MODE) {
	if (dip->di_input_file) {
	    Lprintf(dip, DT_FIELD_WIDTH "%s\n", "Source device name", dip->di_input_file);
	} else {
	    Lprintf(dip, DT_FIELD_WIDTH "%s\n", "Destination device name", dip->di_output_file);
	}
    } else if (dip->di_scsi_dsf) {
	/* This gets setup, or specified, for file sytems! */
	Lprintf(dip, DT_FIELD_WIDTH "%s\n", "SCSI device name", dip->di_scsi_dsf);
    }

    if (dip->di_vendor_id && dip->di_product_id && dip->di_revision_level) {
	Lprintf (dip, DT_FIELD_WIDTH, "Inquiry information");
	Lprintf (dip, "Vid=%s, Pid=%s, Rev=%s\n",
		    dip->di_vendor_id, dip->di_product_id, dip->di_revision_level);
    }
    if (dip->di_device_capacity) {
	Lprintf (dip, DT_FIELD_WIDTH, "Capacity information");
	Lprintf (dip, "Block Length=%u", dip->di_block_length);
	Lprintf (dip, ", Capacity=" LUF " (%.3f Mbytes)\n",
		    dip->di_device_capacity,
		    (double)(dip->di_device_capacity * dip->di_block_length) / (double)MBYTE_SIZE );
	/* Note: This information comes from Read Capacity(16). */
	if (dip->di_lbpmgmt_valid == True) {
	    Lprintf (dip, DT_FIELD_WIDTH "%s Provisioned\n",
		   "Provisioning management", (dip->di_lbpme_flag) ? "Thin" : "Full");
	}
    }
    if (dip->di_device_id) {
	Lprintf (dip, DT_FIELD_WIDTH "%s\n", "Device identifier", dip->di_device_id);
    }
    if (dip->di_serial_number) {
	Lprintf (dip, DT_FIELD_WIDTH "%s\n", "Device serial number", dip->di_serial_number);
    }
    if (dip->di_mgmt_address) {
	Lprintf (dip, DT_FIELD_WIDTH "%s\n", "Management Network Address", dip->di_mgmt_address);
    }
    /* Note: We flush here incase we have multiple devices, thus different log buffers! */
    Lflush(dip);
#endif /* defined(SCSI) */
    return;
}

/*
 * Note: This moves to the I/O behavior structure after this is created for dt.
 * 
 * Inputs: 
 *      dip = The first threads' device information pointer.
 *  
 * Return Value: 
 * 	Returns SUCCESS (always). 
 */
void
dt_job_finish(dinfo_t *dip, job_info_t *job)
{
    threads_info_t *tip = job->ji_tinfo;
    dinfo_t *tdip;
    int thread;

    if ( (dip->di_job_stats_flag == False) ||
	 (dip->di_stats_flag == False) || (dip->di_stats_level == STATS_NONE) ) {
        return;
    }

    /*
     * Accumulate the total statistics.
     */
    for (thread = 1; (thread < tip->ti_threads); thread++) {
        tdip = tip->ti_dts[thread];
	gather_thread_stats(dip, tdip);
    }
    report_stats(dip, JOB_STATS);
    return;
}

void
gather_thread_stats(dinfo_t *dip, dinfo_t *tdip)
{
    dip->di_total_files_read += tdip->di_total_files_read;
    dip->di_total_files_written += tdip->di_total_files_written;
    dip->di_total_bytes_read += tdip->di_total_bytes_read;
    dip->di_total_bytes_written += tdip->di_total_bytes_written;
    dip->di_total_partial_reads += tdip->di_total_partial_reads;
    dip->di_total_partial_writes += tdip->di_total_partial_writes;
    dip->di_total_records_read += tdip->di_total_records_read;
    dip->di_total_records_written += tdip->di_total_records_written;
    dip->di_pass_total_records += tdip->di_pass_total_records;
    dip->di_pass_total_partial += tdip->di_pass_total_partial;
    dip->di_total_bytes += tdip->di_total_bytes;
    dip->di_total_files += tdip->di_total_files;
    dip->di_total_records += tdip->di_total_records;
    dip->di_total_partial += tdip->di_total_partial;
    dip->di_error_count += tdip->di_error_count;
    //dip->di_total_errors += tdip->di_total_errors;
    return;
}
