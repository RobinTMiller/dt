/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2017			    *
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
 * Module:	dtprint.c
 * Author:	Robin T. Miller
 * Date:	July 24th, 2013
 *
 * Description:
 *	Various printing functions.
 *
 * Modification History:
 * 
 * April 23rd, 2015 by Robin T. Miller
 * 	Updated report_record() to report proper partial block percentage.
 * 	The partial number of blocks is also reported properly.
 *
 * April 21st, 2015 by Robin T. Miller
 * 	Added support for writing errors to a single error file.
 * 	Dynamically increase the log buffer, if we're about to exceed it!
 *
 * April 30th, 2014 by Robin T. Miller
 * 	For file systems, report the Direct I/O setting so it's clearly shown
 * whether the buffer cache is being used or bypassed (useful for triage).
 *
 * April 24th, 2014 by Robin T. Miller
 * 	Adding further support for extended error reporting.
 * 	Note: This is just the next stage, alot more work to do!
 *
 * December 2nd, 2013 by Robin T. Miller
 *  Modified field printing functions to call Lprintf() instead of Printf()/Print(),
 * so output can be synchronized to the terminal and/or log files. Note well, this
 * means all callers of these functions *must* invoke Lflush() to flush log buffer.
 */
#include "dt.h"

#include <ctype.h>
#include <string.h>
#include <stdarg.h>

/*
 * Local Definitions:
 */ 
int DisplayWidth = DEFAULT_WIDTH;

/*
 * Enabled/Disabled Table.
 */
char *endis_table[] = {
	"disabled",
	"enabled"
};

/*
 * On/Off Table.
 */
char *onoff_table[] = {
	"off",
	"on"
};

/*
 * True/False Table:
 */
char *boolean_table[] = {
	"false",
	"true"
};

/*
 * Yes/No Table.
 */
char *yesno_table[] = {
	"no",
	"yes"
};

void
ReportError(dinfo_t *dip, error_info_t *eip)
{
    /* Note: Assumes os_get_error() saved error here! */
    int error = dip->di_error;
    char time_buffer[TIME_BUFFER_SIZE];

    (void)time(&dip->di_error_time);
    os_ctime(&dip->di_error_time, time_buffer, sizeof(time_buffer));
    
    if ( !(eip->ei_rpt_flags & RPT_WARNING) ) {
	PrintDecimal(dip, "Error Number", dip->di_error_count, PNL);
	PrintAscii(dip, "Time of Error", time_buffer, PNL);
    } else {
	PrintAscii(dip, "Time of Warning", time_buffer, PNL);
    }
    if ( (dip->di_mode == READ_MODE) && dip->di_read_pass_start) {
	os_ctime(&dip->di_read_pass_start, time_buffer, sizeof(time_buffer));
	PrintAscii(dip, "Read Start Time", time_buffer, PNL);
	if ( (dip->di_ftype == OUTPUT_FILE) && dip->di_write_pass_start) {
	    os_ctime(&dip->di_write_pass_start, time_buffer, sizeof(time_buffer));
	    PrintAscii(dip, "Write Start Time", time_buffer, PNL);
	}
    } else if (dip->di_write_pass_start) {
	os_ctime(&dip->di_write_pass_start, time_buffer, sizeof(time_buffer));
	PrintAscii(dip, "Write Start Time", time_buffer, PNL);
    }
    if ( !(eip->ei_rpt_flags & RPT_NOERRORMSG) ) {
	char *emsg = os_get_error_msg(error);
	PrintDecimal(dip, "Error Code/Message", error, DNL);
	Lprintf(dip, " = %s\n", emsg);
	os_free_error_msg(emsg);
    }
    return;
}

/*
 * ReportRetryableError() - Report and Retry Errors.
 * 
 * Description:
 *    This function will determine if an error is retryable, and if the
 * the error should be counted, based on the retry session disconnects flag.
 * An ignore errors flag exists, but has *not* been fully implemented.
 * 
 * Inputs:
 *  dip = The device information pointer.
 *  op = The name of the operation.
 *  file = The file name (if any).
 *  bytes = The number of bytes in the request.
 *  error = The last error encountered.
 * 
 * Return Value:
 *      FAILURE or RETRYABLE if error is deemed retyable.
 */
int
ReportRetryableError(dinfo_t *dip, error_info_t *eip, char *format, ...)
{
    char buffer[STRING_BUFFER_SIZE];
    hbool_t retryable = False;
    int status = FAILURE;
    char *emsg = buffer;
    va_list ap;
    
    dip->di_error = eip->ei_error;
    /* Clean this up after all callers format a useful error message! */
    if (format) {
        va_start(ap, format);
        (void)vsprintf(emsg, format, ap);
        va_end(ap);
    } else {
        emsg = NULL;
    }
    
    if ( eip->ei_fd && (*eip->ei_fd == NoFd) ) {
	eip->ei_rpt_flags |= RPT_NODEVINFO;
    }
    if ( !(eip->ei_rpt_flags & RPT_NORETRYS) &&
	 dip->di_retry_entries && is_retryable(dip, eip->ei_error) ) {
        retryable = True;
        /* Special handling for session disconnects! */
        //if ( (dip->di_retry_disconnects == True) &&
	/* Do for ALL retryable errors! */
        if ( (dip->di_retry_disconnects == True) &&
	     (dip->di_retry_count < dip->di_retry_limit) ) {
            eip->ei_rpt_flags |= (RPT_NODEVINFO|RPT_NOERRORNUM|RPT_NOHISTORY|RPT_NOXERRORS);
	    eip->ei_prt_flags = PRT_NOFLAGS;
            eip->ei_log_level = logLevelWarn;
        } else if (dip->di_retry_count < dip->di_retry_limit) {
	    /* Allow errors below limit to be logged as a warning for negative testing. */
	    if (dip->di_retry_warning) {
		eip->ei_rpt_flags |= (RPT_NOERRORNUM|RPT_NOHISTORY|RPT_WARNING);
		if (dip->di_debug_flag == False) {
		    eip->ei_rpt_flags |= (RPT_NODEVINFO|RPT_NOXERRORS);
		}
		eip->ei_prt_flags = PRT_NOFLAGS;
		eip->ei_log_level = logLevelWarn;
	    }
	    /* Let's be less noisy when retrying errors. */
	    if (dip->di_debug_flag == False) {
		eip->ei_rpt_flags |= RPT_NOHISTORY;
	    }
	}
        (void)ReportErrorInfoX(dip, eip, emsg);
    } else if ( !(eip->ei_rpt_flags & RPT_NOERRORS) ) {
	if (dip->di_ignore_errors == False) {
	    (void)ReportErrorInfoX(dip, eip, emsg);
	}
    }
    if ( (retryable == True) && retry_operation(dip, eip) ) {
        if ( PROGRAM_TERMINATING ) {
            Fprintf(dip, "Program is terminating, so NOT retrying after %u retries!\n",
                    dip->di_retry_count);
        } else if ( THREAD_TERMINATING(dip) || COMMAND_INTERRUPTED ) {
            Fprintf(dip, "Thread is being terminated, so NOT retrying after %u retries!\n",
                    dip->di_retry_count);
        } else {
            if (dip->di_retry_disconnects == True) {
                /* Try to reestablish a new session! */
		if ( eip->ei_fd && (*eip->ei_fd != NoFd) ) {
		    (void)reopen_after_disconnect(dip, eip);
		}
            }
            status = RETRYABLE;
        }
    }
    return(status);
}

/*
 * ReportErrorInfo() - Report Error Informaion.
 *
 * Inputs:
 * 	dip = The device information pointer.
 *      file = The file name (optional, NULL if none)
 *      error = The operating system error number.
 *      error_info = Additional error info for perror.
 *	record_error = Controls reporting error/time info.
 * 
 * Outputs:
 *      dip->di_error gets the last error number. (don't use errno!)
 */
void
ReportErrorInfo( dinfo_t   *dip,
                 char      *file,
                 int       error,
                 char      *error_info,
		 optype_t  optype,
                 hbool_t   record_error)
{
    INIT_ERROR_INFO(eip, file, error_info, optype, &dip->di_fd, dip->di_oflags, dip->di_offset,
		    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
    
    if (record_error == False) {
        eip->ei_rpt_flags = (RPT_NOERRORNUM|RPT_NOHISTORY);
    }
    (void)ReportErrorInfoX(dip, eip, NULL);
    return;
}

/*
 * ReportErrorInfoX() - Report Error Information Extended.
 *
 * Inputs:
 *      dip = The device information pointer.
 *      eip = The error information pointer.
 *      format = The variable argument message.
 * 
 * Outputs:
 *      dip->di_error gets the last error number.
 *      Return FAILURE indicating an error occurred.
 */
int
ReportErrorInfoX(dinfo_t *dip, error_info_t *eip, char *format, ...)
{
    char buffer[STRING_BUFFER_SIZE];
    char *emsg = buffer;
    va_list ap;
    FILE *fp;
    char *os_emsg;

    /* If the file is not open, assume no valid device info to report. */
    if ( eip->ei_fd && (*eip->ei_fd == NoFd) ) {
	eip->ei_rpt_flags |= RPT_NODEVINFO;
    }

    if ( (eip->ei_log_level == logLevelCrit) ||
	 (eip->ei_log_level == logLevelError) ) {
	fp = dip->di_efp;
    } else {
	fp = dip->di_ofp;
    }
    dip->di_error = eip->ei_error;
    
    if (format) {
        va_start(ap, format);
        vsprintf(emsg, format, ap);
        va_end(ap);
    } else {
        emsg = NULL;
    }

    if ( !(eip->ei_rpt_flags & RPT_NOERRORNUM) ) {
	if ( !(eip->ei_rpt_flags & RPT_WARNING) ) {
	    //exit_status = FAILURE;      /* <- Note: This needs to go! */
	    ReportErrorNumber(dip);
	}
	if (dip->di_extended_errors == False) {
	    RecordErrorTimes(dip, False);
	}
    }

    if (eip->ei_error) {
	/*
	 * A couple formats allowed, until callers are all cleaned up!
	 */
	os_emsg = os_get_error_msg(eip->ei_error);
	if (emsg) {
	    LogMsg(dip, fp, eip->ei_log_level, eip->ei_prt_flags,
		   "%s, error = %d - %s\n", emsg, eip->ei_error, os_emsg);
	} else if (eip->ei_file) {
	    LogMsg(dip, fp, eip->ei_log_level, eip->ei_prt_flags,
		   "File: %s, %s, error = %d - %s\n",
		   eip->ei_file, eip->ei_op, eip->ei_error, os_emsg);
	} else {
	    LogMsg(dip, fp, eip->ei_log_level, eip->ei_prt_flags,
		   "%s, error = %d - %s\n", eip->ei_op, eip->ei_error, os_emsg);
	}
	os_free_error_msg(os_emsg);
    }

    /* Note: We *must* report device information to set trigger values! */
    if ( !(eip->ei_rpt_flags & RPT_NODEVINFO) ) {
	hbool_t eio_flag = os_isIoError(eip->ei_error);
	ReportDeviceInfo(dip, eip->ei_bytes, 0, eio_flag);
    } else if (dip->di_extended_errors == False) {
	report_device_information(dip);
    }

    if ( (dip->di_extended_errors == True) && !(eip->ei_rpt_flags & RPT_NOXERRORS) ) {
	(void)ReportExtendedErrorInfo(dip, eip, emsg);
    }
    if (dip->di_extended_errors == False) {
	/*
	 * History is dumped here to display history on ALL errors.
	 */
	if ( !(eip->ei_rpt_flags & RPT_NOHISTORY) && dip->di_history_size ) {
	    dump_history_data(dip);
	}
    }
    return (FAILURE);
}

/*
 * ReportExtendedErrorInfo() - Report Error Informaion.
 *
 * Note: This is used in conjunction with ReportErrorInfoX() right now!
 * 
 * Inputs:
 *      dip = The device information pointer.
 *      eip = The error information pointer.
 *      format = The variable argument message. (not currently used)
 * 
 * Outputs:
 *      dip->di_error gets the last error number.
 *      Return FAILURE indicating an error occurred.
 */
int
ReportExtendedErrorInfo(dinfo_t *dip, error_info_t *eip, char *format, ...)
{
    char time_buffer[TIME_BUFFER_SIZE];
    hbool_t error_flag;
    
    if ( (eip->ei_log_level == logLevelCrit) ||
	 (eip->ei_log_level == logLevelError) ) {
	error_flag = True;
    } else {
	error_flag = False;
    }

    /* Note: If we switch to this completely, handle syslog messages! */

    /* Start of new error information. */

    Lprintf(dip, "\n");

    /* Note: We may wish to report information, without reporting errors. */
    if ( !(eip->ei_rpt_flags & RPT_NOERRORNUM) ) {
	ReportError(dip, eip);
	dip->di_end_time = times(&dip->di_etimes);
	if (dip->di_pass_time) {
	    PrintLongLong(dip, "Pass Number", (uint64_t)(dip->di_pass_count + 1), PNL);
	    (void)FormatElapstedTime(time_buffer, (dip->di_end_time - dip->di_pass_time));
	    PrintAscii(dip, "Pass Elapsed Time", time_buffer, PNL);
	}
    }
    if (dip->di_start_time && dip->di_end_time) {
	(void)FormatElapstedTime(time_buffer, (dip->di_end_time - dip->di_start_time));
	PrintAscii(dip, "Test Elapsed Time", time_buffer, PNL);
    }

    PrintAscii(dip, "File Name", eip->ei_file, PNL);
    if ( isFileSystemFile(dip) ) {
	os_ino_t fileID;
	large_t filesize;
	HANDLE fd = NoFd;
	char *p;
	if (eip->ei_fd) fd = *eip->ei_fd;
	fileID = os_get_fileID(eip->ei_file, fd);
	if (fileID != (os_ino_t)FAILURE) {
	    char fileID_str[MEDIUM_BUFFER_SIZE];
	    (void)sprintf(fileID_str, "File %s", OS_FILE_ID);
	    PrintLongDecHex(dip, fileID_str, fileID, PNL);
	}
	/* Isolate the directory and report its' ID too! */
	if (p = strrchr(eip->ei_file, dip->di_dir_sep)) {
	    os_ino_t dirID;
	    *p = '\0';	/* Separate the directory from the file name. */
	    dirID = os_get_fileID(eip->ei_file, NoFd);
	    *p++ = dip->di_dir_sep;
	    if (dirID != (os_ino_t)FAILURE) {
		char dirID_str[MEDIUM_BUFFER_SIZE];
		(void)sprintf(dirID_str, "Directory %s", OS_FILE_ID);
		PrintLongDecHex(dip, dirID_str, dirID, PNL);
	    }
	}
	filesize = os_get_file_size(eip->ei_file, fd);
	if (filesize != (large_t)FAILURE) {
	    PrintLongDecHex(dip, "File Size", filesize, PNL);
	}
    }
    PrintAscii(dip, "Operation", eip->ei_op, PNL);
    if (eip->ei_bytes) {
	if (dip->di_iobehavior == DT_IO) {
	    uint64_t record;
	    record = (eip->ei_optype == READ_OP) ? dip->di_records_read : dip->di_records_written;
	    record++; /* zero based */
	    PrintLongLong(dip, "Record Number", record, PNL);
	}
	/* Note: The request size is actually size_t (could be 64-bit unsigned long)! */
	/* TODO: Create print function for size_t and ssize_t (current all unsigned). */
	PrintDecHex(dip, "Request Size", (unsigned)eip->ei_bytes, PNL);
	PrintDecHex(dip, "Block Length", ((unsigned)eip->ei_bytes / dip->di_dsize), PNL);
    }
    PrintAscii(dip, "I/O Mode", (dip->di_mode == READ_MODE) ? "read" : "write", PNL);
    PrintAscii(dip, "I/O Type", (dip->di_io_type == SEQUENTIAL_IO) ? "sequential" : "random", PNL);
    PrintAscii(dip, "File Type", (dip->di_ftype == INPUT_FILE) ? "input" : "output", PNL);

    /* Note: Normally only used for file systems, but block device can use (Linux). */
    if (dip->di_bufmode_type) {
	PrintAscii(dip, "Buffer Mode", dip->di_bufmode_type, PNL);
    }
    if ( isFileSystemFile(dip) ) {
	hbool_t dio_flag = isDirectIO(dip);	/* Handles buffer modes. */
	/* Since this is very important for triage, let's make it known! */
	PrintEnDis(dip, False, "Direct I/O", dio_flag, DNL);
	Lprintf(dip, " (%s)\n",	(dio_flag) ? "bypassing cache" : "caching data");
    }
    report_device_informationX(dip);
    if ( !(eip->ei_rpt_flags & RPT_NODEVINFO) ) {
	ReportDeviceInfoX(dip, eip);
    }
#if defined(SCSI)
    if ( (dip->di_scsi_io_flag == True) && (dip->di_sgp && dip->di_sgp->error == True) ) {
	dtReportScsiError(dip, dip->di_sgp);
    }
#endif /* defined(SCSI) */
    if (error_flag == True) {
	eLflush(dip);
    } else {
	Lflush(dip);
    }

    /*
     * History is dumped here to display history for ALL errors.
     */
    if ( !(eip->ei_rpt_flags & RPT_NOHISTORY) && dip->di_history_size ) {
	dump_history_data(dip);
    } else {
	if ( !(eip->ei_rpt_flags & RPT_NONEWLINE) ) {
	    if (error_flag == True) {
		Fprintf(dip, "\n");
	    } else {
		Printf(dip, "\n");
	    }
	}
    }
    return (FAILURE);
}

void
ReportErrorNumber(dinfo_t *dip)
{
    dip->di_error_count++;
    //exit_status = FAILURE;
    (void)time(&dip->di_error_time);
    LogMsg(dip, dip->di_efp, logLevelError, (PRT_SYSLOG|PRT_MSGTYPE_ERROR),
	   "Error number %lu occurred on %s\n", dip->di_error_count,
	   os_ctime(&dip->di_error_time, dip->di_time_buffer, sizeof(dip->di_time_buffer)));
    return;
}

/*
 * Record current time of when error occurred.
 */
void
RecordErrorTimes(struct dinfo *dip, hbool_t record_error)
{
    char buffer[STRING_BUFFER_SIZE];
    char *bp = buffer;
    int flags = PRT_SYSLOG;

    if (record_error == True) {
	ReportErrorNumber(dip);
    }

    dip->di_end_time = times(&dip->di_etimes);

    if (dip->di_pass_time) {
	bp += sprintf(bp, "Elapsed time since beginning of pass: ");
	bp += FormatElapstedTime(bp, (dip->di_end_time - dip->di_pass_time));
	Fprintf(dip, "%s\n", buffer);
    }
    if (dip->di_start_time) {
	bp = buffer;
	bp += sprintf(bp, "Elapsed time since beginning of test: ");
	bp += FormatElapstedTime(bp, (dip->di_end_time - dip->di_start_time));
	Fprintf(dip, "%s\n", buffer);
    }
    return;
}

/*
 * Note: This is only used with verbose Debug for reporting record information.
 */
void
report_io(dinfo_t *dip, test_mode_t io_mode, void *buffer, size_t bytes, Offset_t offset)
{
    large_t iolba = NO_LBA;
    Offset_t iopos = (Offset_t) 0;
    hbool_t read_mode = (io_mode == READ_MODE);
    long files, records;

    if (dip->di_random_access) {
	iopos = offset;
	iolba = (iopos / dip->di_dsize);
    } else if (dip->di_lbdata_flag || dip->di_iot_pattern) {
	iopos = (Offset_t)(dip->di_volume_bytes + offset);
	iolba = make_lbdata(dip, iopos);
    }
    /* 
     * Note: We cannot report read/write records with percentage, otherwise 
     * the record numbers will NOT match extended error reporting and btags! 
     */
    if ( False /*dip->di_read_percentage*/ ) {
	files = (dip->di_files_read + dip->di_files_written) + 1;
	records = (dip->di_records_read + dip->di_records_written) + 1;
    } else {
	files = (read_mode) ? (dip->di_files_read + 1) : (dip->di_files_written + 1);
	records = (read_mode) ? (dip->di_records_read + 1) : (dip->di_records_written + 1);
    }
    report_record(dip, files, records, iolba, iopos, io_mode, buffer, bytes);
    return;
}

/*
 * report_record() - Report Record Information.
 *
 * Description:
 *	This function is used when Debug is enabled, and is also
 * used to dump a history data entry.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	files = The file number (limited to tapes at present).
 *	records = The record number.
 *	lba = The logical block number (NO_LBA if none).
 *	offset = The file offset.
 *	mode = The test mode (READ_MODE or WRITE_MODE).
 *	buffer = The data buffer.
 *	bytes = The data byte count.
 *
 * Return Value:
 *	void
 */
void
report_record(
	struct dinfo	*dip,
	u_long		files,
	u_long		records,
	large_t		lba,
        Offset_t	offset,
	enum test_mode	mode,
	void		*buffer,
	size_t		bytes )
{
    char msg[STRING_BUFFER_SIZE];
    char *bp = msg;
    double start = 0, end = 0;
    large_t elba = 0;

    /*
     * Depending on data supplied, calculate ending block,
     * and block offsets (for file system testing).
     */
    if ( (lba != NO_LBA) && dip->di_dsize) {
        elba = ((lba + howmany(bytes, dip->di_dsize)) - 1);
	/* Note: For SAN (whole disk blocks), we report whole numbers! */
	if (offset % dip->di_dsize) {
	    start = ((double)offset / (double)dip->di_dsize);
	}
	if ((offset + bytes) % dip->di_dsize) {
	    end = ((double)(offset + bytes) / (double)dip->di_dsize);
	}
    }
    if ( dip->di_multiple_files || (dip->di_dtype->dt_dtype == DT_TAPE) ) {
	bp += sprintf(bp, "File #%lu, ", files);
    }
    bp += sprintf(bp, "Record #%lu - ", records);
    /*
     * A buffer indicates runtime debug, else assume dumping history.
     */
    if (buffer) {
	bp += sprintf(bp, "%s %u byte%s ",
		      (mode == READ_MODE) ? "Reading" : "Writing",
		      (unsigned int)bytes, (bytes > 1) ? "s" : "");
    } else {
	bp += sprintf(bp, "%s %u byte%s ",
		      (mode == READ_MODE) ? "Read" : "Wrote",
		      (unsigned int)bytes, (bytes > 1) ? "s" : "");
    }
    if ( (lba != NO_LBA) && dip->di_dsize) {
	if (bytes % dip->di_dsize) {
	    float blocks = (float)bytes / (float)dip->di_dsize;
	    bp += sprintf(bp, "(%.2f block%s) ", blocks, (blocks > 1) ? "s" : "");
	} else {
	    uint32_t blocks = (uint32_t)(bytes / dip->di_dsize);
	    bp += sprintf(bp, "(%u block%s) ", blocks, (blocks > 1) ? "s" : "");
	}
    }
    if (buffer) {
	bp += Sprintf(bp, "%s buffer "LLPXFMT", ",
		 (mode == READ_MODE) ? "into" : "from",	(uint8_t *)buffer);
    }
    if (lba != NO_LBA) {
        bp += sprintf(bp, "lba%s ", (elba > lba) ? "'s" : "");
	/* Block offsets only exist for file systems. */
        if (start && end) {
            bp += sprintf(bp, "%.2f - %.2f", start, end);
        } else if (start) {
            bp += sprintf(bp, "%.2f - " LUF, start, elba);
        } else if (end) {
            bp += sprintf(bp, LUF " - %.2f", lba, end);
        } else if (lba != elba) {
	    /* For SAN, there are no partial blocks. */
	    bp += sprintf(bp, LUF " - " LUF, lba, elba);
        } else {
            bp += sprintf(bp, LUF, lba);
        }
    }
    bp += sprintf(bp, " (offset " FUF ")", offset);
    bp += sprintf(bp, "\n");
    if (buffer) {
	Printf(dip, msg);
    } else {
	Print(dip, msg);
    }
}

int
acquire_print_lock(void)
{
    int status = pthread_mutex_lock(&print_lock);
    if (status != SUCCESS) {
	tPerror(NULL, status, "Failed to acquire print mutex!\n");
    }
    return(status);
}

int
release_print_lock(void)
{
    int status = pthread_mutex_unlock(&print_lock);
    if (status != SUCCESS) {
	tPerror(NULL, status, "Failed to unlock print mutex!\n");
    }
    return(status);
}

/*
 * fmtmsg_prefix() - Common function to format the prefix for messages.
 *
 * Inputs:
 *	dip = The device information pointer.
 * 	bp = Pointer to the message buffer.
 * 	flags = The format control flags.
 * 	level = The logging level.
 *
 * Return Value:
 *	The updated buffer pointer.
 */
char *
fmtmsg_prefix(dinfo_t *dip, char *bp, int flags, logLevel_t level)
{
    char *log_prefix;

    if (dip == NULL) dip = master_dinfo;
    /*
     * The logging prefix can be user defined, NATE, or standard.
     */ 
    if (dip->di_log_prefix) {
        log_prefix = FmtLogPrefix(dip, dip->di_log_prefix, False);
    } else {
        if (dip->di_debug_flag || dip->di_tDebugFlag) {
            log_prefix = FmtLogPrefix(dip, DEFAULT_DEBUG_LOG_PREFIX, False);
        } else {
            log_prefix = FmtLogPrefix(dip, DEFAULT_LOG_PREFIX, False);
        }
    }
    bp += sprintf(bp, "%s", log_prefix);
    free(log_prefix);

    /*
     * Add an ERROR: prefix to clearly indicate error/critical issues.
     */
    if ( !(flags & PRT_NOLEVEL) ) {
	if ( (level == logLevelCrit) || (level == logLevelError) ) {
	    bp += sprintf(bp, "ERROR: ");
	} else if (level == logLevelWarn) {
	    bp += sprintf(bp, "Warning: ");
        }
    }
    dip->di_sequence++;
    return (bp);
}

/*
 * Display failure message to file pointer and flush output.
 */
/*VARARGS*/
void
LogMsg(dinfo_t *dip, FILE *fp, enum logLevel level, int flags, char *fmtstr, ...)
{
    va_list ap;
    char buffer[STRING_BUFFER_SIZE];
    char *bp = buffer;

    if (dip == NULL) dip = master_dinfo;
    /* Note: The user controls this with "%level" during formatting! */
    dip->di_log_level = level;
    if ( !(flags & PRT_NOIDENT) ) {
	bp = fmtmsg_prefix(dip, bp, flags, level);
    }
    va_start(ap, fmtstr);
    bp += vsprintf(bp, fmtstr, ap);
    va_end(ap);
    /* Note: Not currently used, but allows syslog only. */
    if ( !(flags & PRT_NOLOG) ) {
        (void)PrintLogs(dip, level, flags, fp, buffer);
    }
    if ( !(flags & PRT_NOFLUSH) ) {
	(void)fflush(fp);
    }
    if ( dip->di_syslog_flag && (flags & PRT_SYSLOG) ) {
	syslog(level, "%s", buffer);
    }
    return;
}

void
SystemLog(dinfo_t *dip, int priority, char *format, ...)
{
    va_list ap;
    char buffer[STRING_BUFFER_SIZE];
    char *bp = buffer;
    int flags = PRT_NOLEVEL;
    logLevel_t level = logLevelInfo;

    if (dip == NULL) dip = master_dinfo;
    bp = fmtmsg_prefix(dip, bp, flags, level);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    syslog(priority, "%s", buffer);
    return;
}

int
AcquirePrintLock(dinfo_t *dip)
{
    hbool_t job_log_flag;
    int status = WARNING;

    if (error_logfp || master_logfp) {
        status = acquire_print_lock();
	return(status);
    }
    job_log_flag = (dip->di_job && dip->di_job->ji_job_logfp);
    /*
     * Locking logic:
     *  o if job log, acquire the job lock
     *    Note: This syncs all thread output to job log.
     *  o if no thread log, acquire global print lock
     *    Note: This syncs all thread output to the terminal.
     *  o otherwise, thread log we don't take any locks.
     *    Note: No locks necessary since only thread writing.
     */
    if (job_log_flag) {
        status = acquire_job_print_lock(dip, dip->di_job);
    } else if (dip->di_log_file == NULL) {
        status = acquire_print_lock();
    }
    return(status);
}

int
ReleasePrintLock(dinfo_t *dip)
{
    hbool_t job_log_flag;
    int status = WARNING;

    if (error_logfp || master_logfp) {
        status = release_print_lock();
	return(status);
    }
    job_log_flag = (dip->di_job && dip->di_job->ji_job_logfp);
    /*
     * Locking logic:
     *  o if job log is open, acquire the job lock
     *    Note: This syncs output from all thread to job log.
     *  o if no thread log, acquire global print lock
     *    Note: This syncs all thread output to the terminal.
     *  o otherwise, thread logging we don't take any locks.
     *    Note: No locks necessary since only thread writing.
     */
    if (job_log_flag) {
        status = release_job_print_lock(dip, dip->di_job);
    } else if (dip->di_log_file == NULL) {
        status = release_print_lock();
    }
    return(status);
}

/*
 * Note: This is the common function used to print all messages!
 */
int
PrintLogs(dinfo_t *dip, logLevel_t level, int flags, FILE *fp, char *buffer)
{
    hbool_t job_log_flag;
    int status;

    job_log_flag = (dip->di_job && dip->di_job->ji_job_logfp);
    /*
     * Here is our printing logic:
     *  o if job and thread log is open, write to both.
     *  o if job log is open, write to this.
     *  o otherwise, write to the specified fp.
     *    Note: This will be the thread log or stdout/stderr.
     *  o write to error file if open and error severity.
     *  o if master log is open, also write to this.
     *  o finally, write to the server (if no job/thread log)
     */
    if ( dip->di_log_opened && job_log_flag) {		/* job & thread logs */
        status = Fputs(buffer, fp);
        if (dip->di_joblog_inhibit == False) {
            status = Fputs(buffer, dip->di_job->ji_job_logfp);
	    (void)fflush(dip->di_job->ji_job_logfp);
        }
    } else if (job_log_flag) {				/* per job log */
        status = Fputs(buffer, dip->di_job->ji_job_logfp);
	(void)fflush(dip->di_job->ji_job_logfp);
    } else {
        status = Fputs(buffer, fp);
	(void)fflush(fp);
    }
    if ( error_log && ((level == logLevelCrit) || (level == logLevelError)) ) {
	/* Create the file in append mode, leaving it open thereafter! */
	if (error_logfp == NULL) {
	    status = OpenOutputFile(dip, &error_logfp, error_log, "a", DisableErrors);
	} else {
	    /* Accomodate multiple dt processes! */
	    (void)fseek(error_logfp, 0, SEEK_END);
	}
	if (error_logfp) {
	    status = Fputs(buffer, error_logfp);	/* error log file. */
	    (void)fflush(error_logfp);
	}
    }
    if (master_logfp) {
	status = Fputs(buffer, master_logfp);		/* master log file. */
	(void)fflush(master_logfp);
    }
    return(status);
}

/*
 * Function to print error message (with ERROR: prefix).
 */
void
Eprintf(dinfo_t *dip, char *format, ...)
{
    char buffer[STRING_BUFFER_SIZE];
    va_list ap;
    char *bp = buffer;
    FILE *fp;

    if (dip == NULL) dip = master_dinfo;
    (void)fflush(dip->di_ofp);
    ReportErrorNumber(dip);
    fp = dip->di_efp;
    bp = fmtmsg_prefix(dip, bp, 0, logLevelError);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    (void)PrintLogs(dip, logLevelError, PRT_MSGTYPE_ERROR, fp, buffer);
    (void)fflush(fp);
    return;
}

void
Fprintf(dinfo_t *dip, char *format, ...)
{
    char buffer[STRING_BUFFER_SIZE];
    va_list ap;
    char *bp = buffer;
    FILE *fp;

    if (dip == NULL) dip = master_dinfo;
    (void)fflush(dip->di_ofp);
    fp = dip->di_efp;
    bp = fmtmsg_prefix(dip, bp, 0, logLevelInfo);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    (void)PrintLogs(dip, logLevelError, PRT_MSGTYPE_OUTPUT, fp, buffer);
    (void)fflush(fp);
    return;
}

/*
 * Same as Fprintf except no identifier or log prefix.
 */
/*VARARGS*/
void
Fprint(dinfo_t *dip, char *format, ...)
{
    char buffer[STRING_BUFFER_SIZE];
    va_list ap;
    FILE *fp;

    if (dip == NULL) dip = master_dinfo;
    fp = dip->di_efp;
    va_start(ap, format);
    (void)vsprintf(buffer, format, ap);
    va_end(ap);
    (void)PrintLogs(dip, logLevelError, PRT_MSGTYPE_OUTPUT, fp, buffer);
    return;
}

void
Fprintnl(dinfo_t *dip)
{
    if (dip == NULL) dip = master_dinfo;
    Fprint(dip, "\n");
    (void)fflush(dip->di_efp);
}

/*
 * Format & append string to log file buffer.
 */
/*VARARGS*/
void
Lprintf(dinfo_t *dip, char *format, ...)
{
    va_list ap;
    char *bp = dip->di_log_bufptr;
    ssize_t size;

    va_start(ap, format);
    vsprintf(bp, format, ap);
    va_end(ap);
    bp += strlen(bp);
    dip->di_log_bufptr = bp;
    /*
     * If we're close to exceeding the log buffer, allocate more space!
     */
    if ((size = (dip->di_log_bufptr - dip->di_log_buffer)) > (dip->di_log_bufsize - 256)) {
	ssize_t new_log_bufsize = (dip->di_log_bufsize * 2); /* Let's try doubling it! */
	bp = malloc(new_log_bufsize);
	if (bp) {
	    strcpy(bp, dip->di_log_buffer);
	    free(dip->di_log_buffer);
	    dip->di_log_buffer = dip->di_log_bufptr = bp;
	    dip->di_log_bufptr += strlen(bp);
	    dip->di_log_bufsize = new_log_bufsize;
	} else {
	    Fprintf(dip, "Oops, we've exceeded the log buffer size, "SDF" > "SDF"!\n", 
		    size, (dip->di_log_bufsize - 256));
	    Fprintf(dip, "AND could not allocate "SDF" bytes for a new log buffer!\n",
		    dip->di_log_bufsize);
	    abort();
	}
    }
    return;
}

/*
 * Flush the log buffer and reset the buffer pointer.
 */
void
Lflush(dinfo_t *dip)
{
    PrintLines(dip, False, dip->di_log_buffer);
    dip->di_log_bufptr = dip->di_log_buffer;
    *dip->di_log_bufptr = '\0';
    return;
}

/* Same as above, but print to stderr instead of stdout. */
void
eLflush(dinfo_t *dip)
{
    PrintLines(dip, True, dip->di_log_buffer);
    dip->di_log_bufptr = dip->di_log_buffer;
    *dip->di_log_bufptr = '\0';
    return;
}

/*
 * Display message for master thread only.
 */
/*VARARGS*/
void
mPrintf(dinfo_t *dip, char *format, ...)
{
    char buffer[STRING_BUFFER_SIZE];
    va_list ap;
    FILE *fp;
    char *bp = buffer;
    int status;

    if (dip == NULL) dip = master_dinfo;
    fp = dip->di_ofp;
    bp = fmtmsg_prefix(dip, bp, 0, logLevelInfo);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    if ( (InteractiveFlag == True) || (PipeModeFlag == True) ||
	 (dip->script_level && dip->di_script_verify) ) {
	status =  Fputs(buffer, fp);
	(void)fflush(fp);
    }
    if (master_logfp) {
	status = Fputs(buffer, master_logfp);	/* master log file. */
	(void)fflush(master_logfp);
    }
    return;
}

/*
 * Same as mPrintf() except no program name identifier.
 */
/*VARARGS*/
void
mPrint(dinfo_t *dip, char *format, ...)
{
    char buffer[STRING_BUFFER_SIZE];
    va_list ap;
    FILE *fp;
    int status;

    if (dip == NULL) dip = master_dinfo;
    fp = dip->di_ofp;
    va_start(ap, format);
    (void)vsprintf(buffer, format, ap);
    va_end(ap);
    if ( (InteractiveFlag == True) || (PipeModeFlag == True) ||
	 (dip->script_level && dip->di_script_verify) ) {
	status =  Fputs(buffer, fp);
    }
    if (master_logfp) {
	status = Fputs(buffer, master_logfp);	/* master log file. */
    }
    return;
}

void
mPrintnl(dinfo_t *dip)
{
    if (dip == NULL) dip = master_dinfo;
    mPrint(dip, "\n");
    (void)fflush(dip->di_ofp);
    if (master_logfp) (void)fflush(master_logfp);
}

/*
 * Display message to stdout & flush output.
 */
/*VARARGS*/
void
Printf(dinfo_t *dip, char *format, ...)
{
    char buffer[STRING_BUFFER_SIZE];
    va_list ap;
    FILE *fp;
    char *bp = buffer;

    if (dip == NULL) dip = master_dinfo;
    fp = dip->di_ofp;
    bp = fmtmsg_prefix(dip, bp, 0, logLevelInfo);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    (void)PrintLogs(dip, logLevelInfo, PRT_MSGTYPE_OUTPUT, fp, buffer);
    (void)fflush(fp);
    return;
}

/*
 * Same as Printf except no program name identifier.
 */
/*VARARGS*/
void
Print(dinfo_t *dip, char *format, ...)
{
    char buffer[STRING_BUFFER_SIZE];
    va_list ap;
    FILE *fp;

    if (dip == NULL) dip = master_dinfo;
    fp = dip->di_ofp;
    va_start(ap, format);
    (void)vsprintf(buffer, format, ap);
    va_end(ap);
    (void)PrintLogs(dip, logLevelInfo, PRT_MSGTYPE_OUTPUT, fp, buffer);
    return;
}

void
Printnl(dinfo_t *dip)
{
    if (dip == NULL) dip = master_dinfo;
    Print(dip, "\n");
    (void)fflush(dip->di_ofp);
}

/*
 * Simple function to print warning messages (prefixed with "Warning: ")
 */
void
Wprintf(dinfo_t *dip, char *format, ...)
{
    char buffer[STRING_BUFFER_SIZE];
    va_list ap;
    FILE *fp;
    char *bp = buffer;

    if (dip == NULL) dip = master_dinfo;
    fp = dip->di_ofp;
    bp = fmtmsg_prefix(dip, bp, 0, logLevelWarn);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    (void)PrintLogs(dip, logLevelWarn, PRT_MSGTYPE_OUTPUT, fp, buffer);
    (void)fflush(fp);
    return;
}

/*
 * Perror() - Common Function to Print Error Messages.
 *
 * Description:
 *	This reports POSIX style errors only.
 *
 * Implicit Inputs:
 *      format = Pointer to format control string.
 *      ... = Variable argument list.
 *
 * Return Value:
 *      void
 */
/*VARARGS*/
void
Perror(dinfo_t *dip, char *format, ...)
{
    char msgbuf[STRING_BUFFER_SIZE];
    va_list ap;
    FILE *fp;

    if (dip == NULL) dip = master_dinfo;
    fp = (dip) ? dip->di_efp : efp;
    va_start(ap, format);
    (void)vsprintf(msgbuf, format, ap);
    va_end(ap);
    LogMsg(dip, fp, logLevelError, 0,
	   "%s, errno = %d - %s\n", msgbuf, errno, strerror(errno));
    return;
}

/*VARARGS*/
int
Sprintf(char *bufptr, char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    (void)vsprintf(bufptr, msg, ap);
    va_end(ap);
    return( (int)strlen(bufptr) );
}

int	
vSprintf(char *bufptr, const char *msg, va_list ap)
{
    (void)vsprintf(bufptr, msg, ap);
    return( (int)strlen(bufptr) );
}

void
PrintLines(dinfo_t *dip, hbool_t error_flag, char *buffer)
{
    char *bp = buffer, *p;
    size_t buffer_length = strlen(buffer);
    char *end = (buffer + buffer_length);
    int lock_status;
    
    if (buffer_length == 0) return;

    lock_status = AcquirePrintLock(dip);

    do {
        if (p = strchr(bp, '\n')) {
            p++;
	    if (error_flag == True) {
		Fprintf(dip, "%.*s", (p - bp), bp);
	    } else {
		Printf(dip, "%.*s", (p - bp), bp);
	    }
            bp = p;
        } else {
	    if (error_flag == True) {
		Fprintf(dip, "%s", bp);
	    } else {
		Printf(dip, "%s", bp);
	    }
            break;
        }
    } while (bp < end);

    if (lock_status == SUCCESS) {
        lock_status = ReleasePrintLock(dip);
    }
    return;
}

/*
 * PrintHeader() - Function to Displays Header Message.
 *
 * Inputs:
 *	header = The header strings to display.
 *
 * Return Value:
 *	void
 */
void
PrintHeader(dinfo_t *dip, char *header)
{
    Lprintf(dip, "\n%s:\n\n", header);
}

/*
 * PrintAscii() - Function to Print an ASCII Field.
 * PrintNumeric() - Function to Print a Numeric Field.
 * PrintDecimal() - Function to Print a Numeric Field in Decimal.
 * PrintHex() - Function to Print a Numeric Field in Hexadecimal.	*
 *
 * Inputs:
 *	field_str = The field name string.
 *	ascii_str = The ASCII string to print.
 *	nl_flag = Flag to control printing newline.
 *
 * Return Value:
 *	void
 */
void
PrintNumeric(dinfo_t *dip, char *field_str, uint32_t numeric_value, int nl_flag)
{
    char *printf_str = NUMERIC_FIELD;

    Lprintf(dip, printf_str, field_str, numeric_value);
    if (nl_flag) Lprintf(dip, "\n");
}

void
PrintDecimal(dinfo_t *dip, char *field_str, uint32_t numeric_value, int nl_flag)
{
    char *printf_str = NUMERIC_FIELD;

    Lprintf(dip, printf_str, field_str, numeric_value);
    if (nl_flag) Lprintf(dip, "\n");
}

void
PrintDecHex(dinfo_t *dip, char *field_str, uint32_t numeric_value, int nl_flag)
{
    char *printf_str = DEC_HEX_FIELD;

    Lprintf(dip, printf_str, field_str, numeric_value, numeric_value);
    if (nl_flag) Lprintf(dip, "\n");
}

void
PrintHex(dinfo_t *dip, char *field_str, uint32_t numeric_value, int nl_flag)
{
    char *printf_str = HEX_FIELD;

    Lprintf(dip, printf_str, field_str, numeric_value);
    if (nl_flag) Lprintf(dip, "\n");
}

void
PrintHexDec(dinfo_t *dip, char *field_str, uint32_t numeric_value, int nl_flag)
{
    char *printf_str = HEX_DEC_FIELD;

    Lprintf(dip, printf_str, field_str, numeric_value, numeric_value);
    if (nl_flag) Lprintf(dip, "\n");
}

void
PrintAscii(dinfo_t *dip, char *field_str, char *ascii_str, int nl_flag)
{
    size_t length = strlen(field_str);
    char *printf_str = ((length) ? ASCII_FIELD : EMPTY_FIELD);

    Lprintf(dip, printf_str, field_str, ascii_str);
    if (nl_flag) Lprintf(dip, "\n");
}

/*
 * Functions to display quad (64-bit) values.
 */
void
PrintLongLong(dinfo_t *dip, char *field_str, uint64_t numeric_value, int nl_flag)
{
    char *printf_str = LNUMERIC_FIELD;

    Lprintf(dip, printf_str, field_str, numeric_value);
    if (nl_flag) Lprintf(dip, "\n");
}

void
PrintLongDec(dinfo_t *dip, char *field_str, uint64_t numeric_value, int nl_flag)
{
    char *printf_str = LNUMERIC_FIELD;

    Lprintf(dip, printf_str, field_str, numeric_value);
    if (nl_flag) Lprintf(dip, "\n");
}

void
PrintLongDecHex(dinfo_t *dip, char *field_str, uint64_t numeric_value, int nl_flag)
{
    char *printf_str = LDEC_HEX_FIELD;

    Lprintf(dip, printf_str, field_str, numeric_value, numeric_value);
    if (nl_flag) Lprintf(dip, "\n");
}

void
PrintLongHex(dinfo_t *dip, char *field_str, uint64_t numeric_value, int nl_flag)
{
    char *printf_str = LHEX_FIELD;

    Lprintf(dip, printf_str, field_str, numeric_value);
    if (nl_flag) Lprintf(dip, "\n");
}

void
PrintLongHexDec(dinfo_t *dip, char *field_str, uint64_t numeric_value, int nl_flag)
{
    char *printf_str = LHEX_DEC_FIELD;

    Lprintf(dip, printf_str, field_str, numeric_value, numeric_value);
    if (nl_flag) Lprintf(dip, "\n");
}

/*
 * Printxxx() - Functions to Print Fields with Context.
 *
 * Inputs:
 *	numeric = Print field as numeric.
 *	field_str = The field name string.
 *	ascii_str = The ASCII string to print.
 *	nl_flag = Flag to control printing newline.
 *
 * Return Value:
 *	void
 */
void
PrintBoolean(
    dinfo_t *dip,
    hbool_t	numeric,
    char	*field_str,
    hbool_t	boolean_flag,
    hbool_t	nl_flag)
{
    if (numeric) {
	PrintNumeric(dip, field_str, boolean_flag, nl_flag);
    } else {
	PrintAscii(dip, field_str, boolean_table[(int)boolean_flag], nl_flag);
    }
}

void
PrintEnDis(
    dinfo_t	*dip,
    hbool_t	numeric,
    char	*field_str,
    hbool_t	boolean_flag,
    hbool_t	nl_flag)
{
    if (numeric) {
	PrintNumeric(dip, field_str, boolean_flag, nl_flag);
    } else {
	PrintAscii(dip, field_str, endis_table[(int)boolean_flag], nl_flag);
    }
}

void
PrintOnOff(
    dinfo_t	*dip,
    hbool_t	numeric,
    char	*field_str,
    hbool_t	boolean_flag,
    hbool_t	nl_flag)
{
    if (numeric) {
	PrintNumeric(dip, field_str, boolean_flag, nl_flag);
    } else {
	PrintAscii(dip, field_str, onoff_table[(int)boolean_flag], nl_flag);
    }
}

void
PrintYesNo(
    dinfo_t	*dip,
    hbool_t	numeric,
    char	*field_str,
    hbool_t	boolean_flag,
    hbool_t	nl_flag)
{
    if (numeric) {
	PrintNumeric(dip, field_str, boolean_flag, nl_flag);
    } else {
	PrintAscii(dip, field_str, yesno_table[(int)boolean_flag], nl_flag);
    }
}

void
PrintFields(dinfo_t *dip, uint8_t *bptr, int length)
{
    int field_entrys = ((DisplayWidth - FIELD_WIDTH) / 3) - 1;
    int count = 0;

    while (count < length) {
	if (CmdInterruptedFlag == True)	break;
	if ((++count % field_entrys) == 0) {
	    Lprintf(dip, "%02x\n", *bptr++);
	    if (count < length)	PrintAscii(dip, "", "", DNL);
	} else {
	    Lprintf(dip, "%02x ", *bptr++);
	}               
    }                       
    if (count % field_entrys) Lprintf(dip, "\n");
}                                     

void
PrintHAFields(dinfo_t *dip, uint8_t *bptr, int length)
{
    int field_entrys;
    int count = 0;
    u_char data;
    u_char *abufp, *abp;

    field_entrys = ((DisplayWidth - FIELD_WIDTH) / 3) - 1;
    field_entrys -= (field_entrys / 3);	/* For ASCII */
    abufp = abp = (u_char *)Malloc(dip, (field_entrys + 1));
    if (abufp == NULL) return;
    while (count < length) {
	if (CmdInterruptedFlag == True)	break;
	data = *bptr++;
	Lprintf(dip, "%02x ", data);
	abp += Sprintf((char *)abp, "%c", isprint((int)data) ? data : ' ');
	*abp = '\0';
	if ((++count % field_entrys) == 0) {
	    Lprintf(dip, "\"%s\"\n", abufp); abp = abufp;
	    if (count < length)	PrintAscii(dip, "", "", DNL);
	}
    }
    if (abp != abufp) {
	while (count++ % field_entrys) Print(dip, "   ");
	Print(dip, "\"%s\"\n", abufp);
    }
    Free(dip, abufp);
    return;
}

void
DumpFieldsOffset(dinfo_t *dip, uint8_t *bptr, int length)
{
    int field_entrys = 16;
    int count = 0;
    uint8_t data;
    uint8_t *abufp, *abp;
    hbool_t first = True;

    if (length == 0) return;
    abufp = abp = (uint8_t *)Malloc(dip, field_entrys + 1);
    if (abufp == NULL) return;
    /*
     * Print offset followed by 'n' hex bytes and ASCII text.
     */
    Lprintf(dip, "Offset  Data\n");
    while (count < length) {
        if (first) {
            Lprintf(dip, "%06d  ", count);
            first = False;
        }
        data = *bptr++;
        Lprintf(dip, "%02x ", data);
        abp += sprintf((char *)abp, "%c", isprint((int)data) ? data : ' ');
        *abp = '\0';
        if ((++count % field_entrys) == 0) {
            Lprintf(dip, "\"%s\"\n", abufp);
            first = True;
            abp = abufp;
        }
    }
    if (abp != abufp) {
        while (count++ % field_entrys) Lprintf(dip, "   ");
        Lprintf(dip, "\"%s\"\n", abufp);
    }
    Free(dip, abufp);
    return;
}
