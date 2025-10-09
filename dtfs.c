/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2025			    *
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
 * Module:	dtfs.c
 * Author:	Robin T. Miller
 * Date:	July 15th, 2013
 *
 * Description:
 *	File system operations.
 *
 * Modification History:
 * 
 * October 6th, 2025 by Robin T. Miller
 *      Remove Windows drive letter check in dt_create_directory(), which
 * keeps multiple directories from being created.
 * 
 * May 5th, 2015 by Robin T. Miller
 * 	When checking for file/path existance, on Windows expect an
 * additional error (ERROR_PATH_NOT_FOUND) when a portion of the path
 * does NOT exist. POSIX uses the same error (ENOENT) for both.
 * 
 * March 17th, 2015 by Robin T. Miller
 * 	Add a deleting files flag, so I/O monitoring does NOT cancel our
 * thread, if this cleanup takes longer than the term wait time.
 *
 * February 5th, 2015 by Robin T. Miller
 * 	Added dt_lock_unlock() for file locking support.
 *
 * January 27th, 2015 by Robin T. Miller
 * 	Fixed a bug in process_next_subdir() where the status variable was
 * NOT initialized, which prevented us from processing all subdirectories!
 * My regression tests missed this, since it only occurred when optimized.
 * 
 * May 7th, 2014 by Robin T. Miller
 * 	Enhanced reopen_after_disconnect() to use new parameters added to
 * error information structure.
 * 
 * July 15th, 2013 by Robin T Miller
 * 	Moving file system related functions here.
 */
#include "dt.h"
#if !defined(WIN32)
#  if defined(_QNX_SOURCE) 
#    include <fcntl.h>
#  else /* !defined(_QNX_SOURCE) */
#    include <sys/file.h>
#  endif /* defined(_QNX_SOURCE) */
#endif /* !defined(WIN32) */

/*
 * Forward References:
 */

/*
 * isFsFullOk() - Checks for File System Full Condition.
 *
 * Inputs:
 *	dip = The device information pointer.
 *      op = The operation message.
 *      path = The file path (directory or file name).
 *
 * Return Value:
 * 	Returns True/False = File System Full / Not FS Full Condition.
 * 	Actually only returns True when processing multiple files!
 */
hbool_t
isFsFullOk(struct dinfo *dip, char *op, char *path)
{
    /*
     * The limit checks control whether file system full is acceptable.
     *
     * Note: Disk full conditions should only occur on write operations.
     */
    int error = os_get_error();
    /* Note: We don't set FS full flag here, since we are used by non-write operations! */
    if ( os_isDiskFull(error) ) {
	if (dip->di_verbose_flag) {
	    char *disk_full_msg = os_getDiskFullMsg(error);
	    Wprintf(dip, " File path: %s\n", path);
	    Wprintf(dip, " Operation: %s failed, error %d - %s\n", op, error, disk_full_msg);
	    Wprintf(dip, "Statistics: file #%lu, record #%lu, %s " FUF " file bytes, " FUF " total bytes\n",
		   (dip->di_files_written + 1),	(dip->di_records_written + 1), "wrote",
		   dip->di_fbytes_written, dip->di_dbytes_written);
	} else if (dip->di_debug_flag || dip->di_fDebugFlag) {
	    char *emsg = os_get_error_msg(error);
	    Printf(dip, "DEBUG: File path: %s\n", path);
	    Printf(dip, "DEBUG: %s failed, error %d - %s\n", op, error, emsg);
	    os_free_error_msg(emsg);
	}
	/* Note: This should not be required anymore, but leaving for now! */
	errno = os_mapDiskFullError(error);    /* Map to Unix equivalent for caller. */
	/* We assume file system full is Ok for multiple files and data written! */
	if ( dip->di_multiple_files && dip->di_maxdata_written ) {
	    dip->di_no_space_left = True;
	    return(True);
	}
    }
    return (False);
}

/*
 * make_dir_filename() - Generate a Directory with File Name.
 *
 * Description:
 *      This function is called when a directory is specified without a file
 * name, and since some folks like tools that generate their own file names,
 * we will append out default file name to suite their desires. ;)
 *
 * Inputs:
 *      dip = The device information pointer.
 *      dirpath = The directory name. (caller must check!)
 *
 * Return Value:
 *      Returns a pointer to the new directory with filename.
 */
char *
make_dir_filename(struct dinfo *dip, char *dirpath)
{
    char path[PATH_BUFFER_SIZE];

    (void)sprintf(path, "%s%c%s", dirpath, dip->di_dir_sep, DEFAULT_DATA_FILE_NAME);
    return( strdup(path) );
}

/*
 * make_file_name() - Generate a New File Name.
 *
 * Description:
 *	A new file anme is created using the directory path (if any),
 * and the file number.
 *
 * Inputs:
 *	dip = The device information pointer.
 *
 * Outputs:
 * 	dip->di_dname points to the new file name.
 * 	The prefix is updated if it contains the device name.
 *
 * Return Value:
 *	Returns a pointer to the new device name.
 */
char *
make_file_name(struct dinfo *dip)
{
    char path[PATH_BUFFER_SIZE];
    char *bp = path;

    /*
     * Construct new file name with directory path and file number appended.
     */
    if (dip->di_dirpath || dip->di_subdir) {
	/* Start with the top level directory. */
	if (dip->di_dirpath) {
	    bp += sprintf(bp, "%s%c", dip->di_dirpath, dip->di_dir_sep);
	}
	/* Append subdirectory (if any). */
	if (dip->di_subdir) {
	    bp += sprintf(bp, "%s%c", dip->di_subdir, dip->di_dir_sep);
	}
	if (dip->di_file_limit) {
	    (void)sprintf(bp, "%s-%08u",
			  dip->di_bname, (dip->di_file_number + 1) );
	} else {
	    (void)sprintf(bp, "%s", dip->di_bname);
	}
    } else {
	if (dip->di_file_limit) {
	    (void)sprintf(path, "%s-%08u", dip->di_bname, (dip->di_file_number + 1) );
	} else {
	    (void)sprintf(path, "%s", dip->di_bname);
	}
    }
    Free(dip, dip->di_dname);
    dip->di_dname = strdup(path);
    /*
     * Update the prefix string, *if* it contains the device name.
     */
    if ( (dip->di_fsfile_flag == True) &&
	 dip->di_prefix_string && strstr(dip->di_prefix_string, "%d") ) {
	/* Recreate prefix with new file path and/or name. */
	(void)FmtPrefix(dip, dip->di_prefix_string, dip->di_prefix_size);
    }
    return(dip->di_dname);
}

/*
 * end_file_processing() - End of File Processing.
 *
 * Inputs:
 *	dip = The device information pointer.
 *
 * Return Value:
 *	Returns SUCCESS / FAILURE
 */
int
end_file_processing(struct dinfo *dip)
{
    struct dtfuncs *dtf = dip->di_funcs;
    int rc, status = SUCCESS;

    /*
     * Remember: AIO close cancels outstanding IO's!
     */
    if (dip->di_mode == WRITE_MODE) {
        rc = (*dtf->tf_flush_data)(dip);
	if (rc == FAILURE) status = rc;
    }
    rc = (*dip->di_funcs->tf_close)(dip);
    if (rc == FAILURE) status = rc;
    rc = do_post_eof_processing(dip);
    if (rc == FAILURE) status = rc;
    (void)(*dip->di_funcs->tf_end_test)(dip);
    return(status);
}

int
do_post_eof_processing(dinfo_t *dip)
{
    int status = SUCCESS;

    /*
     * If file trim is enabled, do during writing if read-after-write was
     * enabled, or do after reading *if* we previously wrote this file.
     */
    if ( (dip->di_io_mode == TEST_MODE) &&
	 dip->di_fsfile_flag && dip->di_fstrim_flag &&
	 ( ( (dip->di_mode == WRITE_MODE) && (dip->di_raw_flag == True) ) ||
	   ( dip->di_output_file && (dip->di_mode == READ_MODE) ) ) ) {
	unsigned long files;
	files = (dip->di_mode == READ_MODE) ? dip->di_files_read : dip->di_files_written;
	if (dip->di_fstrim_frequency && files) {
	    if ((files % dip->di_fstrim_frequency) == 0) {
		status = do_file_trim(dip);
	    }
	} else {
	    status = do_file_trim(dip);
	}
    }
    return(status);
}

/*
 * process_next_dir() - Process The Next Directory.
 *
 * Note: Not used yet, but added for future enhancements.
 *
 * Inputs:
 *	dip = The device information pointer.
 *
 * Return Value:
 *	Returns SUCCESS/FAILURE/WARNING =
 *		Next Directory Ready / Preparation Failed / No More Dirs.
 */
int
process_next_dir(struct dinfo *dip)
{
    char dirpath[PATH_BUFFER_SIZE];
    int status;

    if (dip->di_user_dir_limit && (++dip->di_dir_number < dip->di_user_dir_limit) ) {
	;
    } else {
	return (WARNING);
    }

    /*
     * Track the max levels reached for cleanup purposes.
     */
    if (dip->di_dir_number > dip->di_max_dir_number) {
	dip->di_max_dir_number = dip->di_dir_number;
    }

    /*
     * Create the next top level directory.
     */
    (void)sprintf(dirpath, "%s-%05u", dip->di_dir, dip->di_dir_number);

    if (dip->di_mode == WRITE_MODE) {
	hbool_t isDiskFull, isFileExists;
	status = dt_create_directory(dip, dirpath, &isDiskFull, &isFileExists, EnableErrors);
	if (status == FAILURE) {
	    if (isFileExists == True) {
		status = SUCCESS;
	    } else {
		--dip->di_dir_number;
		if (isDiskFull == True) {
		   status = WARNING;
		}
	    }
	}
    }
    if (status == SUCCESS) {
	if (dip->di_dirpath) Free(dip, dip->di_dirpath);
	dip->di_dirpath = strdup(dirpath);
    }
    return(status);
}

/*
 * process_next_subdir() - Process The Next Sub-Directory.
 *
 * Inputs:
 *	dip = The device information pointer.
 *
 * Return Value:
 *	Returns SUCCESS/FAILURE/WARNING =
 *		Next Directory Ready / Preparation Failed / No More Dirs.
 */
int
process_next_subdir(struct dinfo *dip)
{
    char path[PATH_BUFFER_SIZE];
    char dirpath[PATH_BUFFER_SIZE];
    u_int subdir_number;
    hbool_t processing_subdir_flag = False;
    int status = SUCCESS;

    /*
     * Handle both top level directories and subdirectories.
     */
    if ( (dip->di_subdir_depth + 1) > dip->di_user_subdir_depth) {
	if (dip->di_user_subdir_limit && (dip->di_subdir_number < dip->di_user_subdir_limit) ) {
	    if (dip->di_subdir)	Free(dip, dip->di_subdir);
	    dip->di_subdir = NULL;	/* Starting new subdirectory. */
	    dip->di_subdir_number++;
	    if (dip->di_user_subdir_depth) dip->di_subdir_depth = 1;
	    subdir_number = dip->di_subdir_number;
	    processing_subdir_flag = True;
	} else {
	    return (WARNING);
	}
    } else {
	if (dip->di_subdir_depth == 0) {
	    dip->di_subdir_number++;
	}
	subdir_number = ++dip->di_subdir_depth;
    }

    /*
     * Track the max levels reached for cleanup purposes.
     */
    if (dip->di_subdir_number > dip->di_max_subdir_number) {
	dip->di_max_subdir_number = dip->di_subdir_number;
    }
    if (dip->di_subdir_depth > dip->di_max_subdir_depth) {
	dip->di_max_subdir_depth = dip->di_subdir_depth;
    }
    dip->di_file_number = 0;

    /*
     * Create directory or next subdirectory.
     *
     * Note: Using short directory names, so we can nest deeper!
     */
    if (dip->di_subdir) {
	size_t sdir_size = (strlen(dip->di_subdir) + strlen(dip->di_dirprefix) + 10);
	if (dip->di_dirpath) sdir_size += strlen(dip->di_dirpath);
	sdir_size += strlen(dip->di_bname);
	/* Sanity check to avoid buffer overuns! */
	if ( sdir_size > PATH_BUFFER_SIZE) {
	    Printf(dip, "Subdirectory name (%u) is too long for our path buffer (%u)!\n",
		   sdir_size, PATH_BUFFER_SIZE);
	    return (FAILURE);
	}
	(void)sprintf(path, "%s%c%s%u",
		      dip->di_subdir, dip->di_dir_sep, dip->di_dirprefix, subdir_number);
    } else {
	(void)sprintf(path, "%s%u", dip->di_dirprefix, subdir_number);
    }
    if (dip->di_subdir)	Free(dip, dip->di_subdir);
    dip->di_subdir = strdup(path);
    if (dip->di_dirpath) {
	(void)sprintf(dirpath, "%s%c%s", dip->di_dirpath, dip->di_dir_sep, dip->di_subdir);
    } else {
	(void)sprintf(dirpath, "%s", dip->di_subdir);
    }
    if (dip->di_mode == WRITE_MODE) {
	hbool_t isDiskFull, isFileExists;
	status = dt_create_directory(dip, dirpath, &isDiskFull, &isFileExists, EnableErrors);
	if (status == FAILURE) {
	    if (isFileExists == True) {
		status = SUCCESS;
	    } else {
		if (processing_subdir_flag) {
		    --dip->di_subdir_number;
		} else {
		    --dip->di_subdir_depth;
		}
		if (isDiskFull == True) {
		    char *op = OS_CREATE_DIRECTORY_OP;
		    /* Report the file system full condition. */
		    (void)isFsFullOk(dip, op, dirpath);
		    status = WARNING; /* Caller must handle this error! */
		}
	    }
	}
    }
    return(status);
}

/*
 * process_next_file() - Process the next file (with files= option).
 *
 * Inputs:
 *	dip = The device information pointer.
 *
 * Return Value:
 * 	Returns SUCCESS/FAILURE = Next File Ready / Preparation Failed.
 * 	WARNING indicates no more files available to read (multiple files).
 */
int
process_next_file(struct dinfo *dip)
{
    int status;

    if ( dip->di_output_file && (dip->di_mode == READ_MODE) ) {
	/* Reopen a file written during the write pass. */
	status = (*dip->di_funcs->tf_reopen_file)(dip, dip->di_oflags);
    } else {
	status = (*dip->di_funcs->tf_open)(dip, dip->di_oflags);
	if (status == WARNING) {
	    /* Handle writing with multiple files and file system full. */
	    if ( (dip->di_no_space_left == False) && (dip->di_verbose_flag) ) {
		Printf(dip, "Warning: File %s does NOT exist, reading stopped after %u files!\n",
		       dip->di_dname, dip->di_file_number);
	    }
	    return(status);
	}
#if !defined(WIN32)
	/*
	 * Note: Special handling here, to avoid excessive errors during cleanup!
	 *	 Assumes this is the first file being created in the directory.
	 */
	if ( (status == FAILURE) && (errno == ENAMETOOLONG) && dip->di_subdir_depth) {
	    dip->di_subdir_depth--;	/* Assume we've gone too deep! */
	    (void)remove_current_directory(dip);
	}
#endif /* !defined(WIN32) */
    }
    if (status == FAILURE) return (status);

    /* Note: This is now done as part of common open initialization. */
#if 0
    /*
     * Reset counters in preparation for the next file.
     */
    if (dip->di_mode == READ_MODE) {
	dip->di_fbytes_read = (large_t) 0;
	dip->di_records_read = (large_t) 0;
    } else {
	dip->di_fbytes_written = (large_t) 0;
	dip->di_records_written = (large_t) 0;
    }
#endif /* 0 */

    if (dip->di_user_pattern == False) {
	/* Use a different pattern for each file. */
        dip->di_pattern = data_patterns[(dip->di_pattern_index + dip->di_file_number) % npatterns];
        if (dip->di_pattern_buffer) copy_pattern (dip->di_pattern, dip->di_pattern_buffer);
        if (dip->di_debug_flag) {
            Printf (dip, "Using data pattern 0x%08x for file number %u\n",
                    dip->di_pattern, (dip->di_file_number + 1));
        }
    }
    /* This is very important for memory mapped files! */
    (void)(*dip->di_funcs->tf_start_test)(dip);
    return(status);
}

/*
 * create_directory() - Create a Directory.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	dir = Pointer to directory to be created.
 *
 * Return Value:
 *	Returns SUCCESS/FAILURE/WARNING
 *	  SUCCESS = Directory created.
 *	  FAILURE = Not a directory or creation failed.
 *	  WARNING = Directory already exists.
 */
int
create_directory(struct dinfo *dip, char *dir)
{
    hbool_t is_dir;
    int status = WARNING;

    /*
     * Ensure user specified a directory, or create as necessary.
     */
    if ( os_file_information(dir, NULL, &is_dir, NULL) == SUCCESS ) {
	if ( is_dir == False ) {
	    Fprintf(dip, "%s is not a directory!\n", dir);
	    status = FAILURE;
	}
    } else {
	int rc = SUCCESS;
	if (dip->di_debug_flag || dip->di_fDebugFlag) {
	    Printf(dip, "Creating directory %s...\n", dir);
	}
	dip->di_retry_count = 0;
	do {
	    ENABLE_NOPROG(dip, MKDIR_OP);
	    status = os_create_directory(dir, DIR_CREATE_MODE);
	    DISABLE_NOPROG(dip);
	    if (status == FAILURE) {
		char *op = OS_CREATE_DIRECTORY_OP;
		os_error_t error = os_get_error();
		INIT_ERROR_INFO(eip, dir, op, MKDIR_OP, NULL, 0, (Offset_t)0, (size_t)0,
				error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
		/* Handle file system full conditions (once). */
		if ( os_isDiskFull(error) && (dip->di_retry_count == 0) ) {
		    (void)isFsFullOk(dip, op, dir);	/* Report file system full. */
		    /* If space becomes available, then retry. */
		    if ( do_free_space_wait(dip, dip->di_fsfree_retries) ) {
			dip->di_retry_count++;
			rc = RETRYABLE;
			continue;
		    }
		} else {
		    /* Note: This keeps file system full from being retried! */
		    if ( isFsFullOk(dip, op, dir) ) return(status);
		}
		if ( os_isFileExists(error) ) return(WARNING);
		rc = ReportRetryableError(dip, eip, "Failed to create directory %s", dir);
	    }
	} while ( (status == FAILURE) && (rc == RETRYABLE) );
    }
    return(status);
}

/*
 * remove_current_directory() - Remove The Current Directory.
 *
 * Inputs:
 *	dip = The device information pointer.
 *
 * Return Value:
 *	Returns SUCCESS/FAILURE = Directory Removed/Not Removed.
 */
int
remove_current_directory(struct dinfo *dip)
{
    char dirpath[PATH_BUFFER_SIZE];
    char *bp = dirpath;

    *bp = '\0';
    if (dip->di_dirpath) {
	bp += sprintf(bp, "%s%c", dip->di_dirpath, dip->di_dir_sep);
    }
    /* Append subdirectory (if any). */
    if (dip->di_subdir) {
	bp += sprintf(bp, "%s", dip->di_subdir);
    }
    return( remove_directory(dip, dirpath) );
}

/*
 * remove_directory() - Remove a Directory (assumed to be empty)
 *
 * Inputs:
 *	dip = The device information pointer.
 *	dir = Pointer to directory to be removed.
 *
 * Return Value:
 * 	Returns SUCCESS/FAILURE = Directory Removed / Not Removed.
 * 	Note: Returns success for "file not found" error w/disconnects.
 */
int
remove_directory(struct dinfo *dip, char *dir)
{
    int status;
    int rc = SUCCESS;

    if (dip->di_debug_flag || dip->di_fDebugFlag) {
	Printf(dip, "Removing directory %s...\n", dir);
    }
    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, RMDIR_OP);
	status = os_remove_directory(dir);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, dir, OS_REMOVE_DIRECTORY_OP, RMDIR_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if ( (dip->di_retry_disconnects == True) &&
		 (dip->di_retry_count > 0) &&
		 os_isFileNotFound(error) ) {
		status = SUCCESS;
		break;
	    }
	    rc = ReportRetryableError(dip, eip, "Failed to remove directory %s", dir);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return (status);
}

int
setup_directory_info(struct dinfo *dip)
{
    int status = SUCCESS;

    if (dip->di_dir == NULL) return(status);

    /* Format the directory based on user control strings. */
    if ( strchr(dip->di_dir, '%') ) {
	char *dir = FmtFilePath(dip, dip->di_dir, True);
	if (dir) {
	    Free(dip, dip->di_dir);
	    dip->di_dir = dir;
	}
    }
    /*
     * Setup for multiple directories and/or make the directory unique.
     *
     * When writing files with multiple processes, make the directory
     * name unique, just as we do for regular files.
     */
    /* Note: Don't reset, since we may be called twice (e.g. %uuid). */
    //dip->di_dir_created = False;
    if (dip->di_mode == WRITE_MODE) {
	dip->di_dirpath = strdup(dip->di_dir);
	dip->di_existing_file = False;      /* Assume files don't exist. */
	/* Note: Need to add check before creating output files. */

	status = create_directory(dip, dip->di_dirpath);
	if (status == SUCCESS) {
	    dip->di_dir_created = True;     /* Show we created directory. */
	} else if (status == WARNING) {
	    /* Note (status == WARNING) if the directory already exist!*/
	    status = SUCCESS;
	}
    } else {
	hbool_t is_dir;
	/*
	 * For input files, the directory better exist!
	 */
	dip->di_dirpath = strdup(dip->di_dir);
	if ( os_file_information(dip->di_dirpath, NULL, &is_dir, NULL) == FAILURE) {
	    os_perror(dip, "Can't access directory %s", dip->di_dirpath);
	} else if (is_dir == False) {
	    Fprintf(dip, "%s is NOT a directory!\n", dip->di_dirpath);
	    status = FAILURE;
	}
    }
    return (status);
}

/*
 * dt_delete_file() - Delete the specified file with retries.
 *
 * Inputs:
 * 	dip = The device information pointer.
 *	file = Pointer to the file to delete.
 * 	errors = The error control flag.
 *
 * Outputs:
 * 	Returns SUCCESS/FAILURE = File Delete/Not Deleted.
 */
int
dt_delete_file(struct dinfo *dip, char *file, hbool_t errors)
{
    int status;
    int rc = SUCCESS;

    if (dip->di_debug_flag || dip->di_fDebugFlag) {
	Printf(dip, "Deleting file %s...\n", file);
    }
    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, DELETE_OP);
	status = os_delete_file(file);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_DELETE_FILE_OP, DELETE_OP, NULL, 0, (Offset_t)0, 0,
			    error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if ( (dip->di_retry_disconnects == True) &&
		 (dip->di_retry_count > 0) &&
		 os_isFileNotFound(error) ) {
		status = SUCCESS;
		break;
	    }
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed to delete file %s", file);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return (status);
}

/*
 * delete_files() - Delete All Test Files.
 *
 * Description:
 *	This function deletes one or more files, and one or more
 * directories, assuming we created them.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	delete_topdir = Boolean to control deleting top level directory.
 *
 * Return Value:
 *	Returns SUCCESS/FAILURE = File Deleted/Deletion Failed
 */
int
delete_files(struct dinfo *dip, hbool_t delete_topdir)
{
    int error, status = SUCCESS;

    dip->di_deleting_flag = True;
    if (!dip->di_file_limit && !dip->di_user_dir_limit && !dip->di_user_subdir_limit && !dip->di_user_subdir_depth) {
	;
	/* Maybe a directory specified! */
	//status = dt_delete_file(dip, dip->di_dname);
	//return (status); /* Single file! */
    } else if (dip->di_file_limit && !dip->di_user_dir_limit && !dip->di_user_subdir_limit && !dip->di_user_subdir_depth) {
	if (dip->di_debug_flag || dip->di_fDebugFlag) {
	    Printf(dip, "Removing up to %u files...\n", dip->di_file_limit);
	}
	/* Fall through to delete top level files/directory. */
    } else if (dip->di_user_subdir_limit && !dip->di_user_subdir_depth) {
	char spath[PATH_BUFFER_SIZE];
	u_int max_subdir = dip->di_max_subdir_number;
	u_int subdir;

	if (dip->di_debug_flag || dip->di_fDebugFlag) {
	    Printf(dip, "Removing %u subdirs...\n", max_subdir);
	}
	/*
	 * Delete subdirectory files and directories.
	 */
	for (subdir = 0; (subdir < max_subdir); ) {
	    subdir++;
	    (void)sprintf(spath, "%s%u", dip->di_dirprefix, subdir);
	    error = delete_subdir_files(dip, spath);
	    if (error && (status == SUCCESS)) {
		status = error;
	    }
	}
	/* Fall through to delete top level files/directory. */
    } else if (!dip->di_user_subdir_limit && dip->di_user_subdir_depth) {
	char spath[PATH_BUFFER_SIZE];
	char *bp = spath;
	u_int depth;
	u_int max_depth = dip->di_max_subdir_depth;

	if (dip->di_debug_flag || dip->di_fDebugFlag) {
	    Printf(dip, "Removing subdirs with depth of %u...\n", max_depth);
	}
	while (max_depth) {
	    bp = spath;
	    /*
	     * Start at bottom of tree.
	     */
	    for (depth = 0; (depth < max_depth); ) {
		depth++;
		if (bp == spath) {
		    bp += sprintf(bp, "%s%u", dip->di_dirprefix, depth);
		} else {
		    bp += sprintf(bp, "%c%s%u", dip->di_dir_sep, dip->di_dirprefix, depth);
		}
	    }
	    error = delete_subdir_files(dip, spath);
	    if (error && (status == SUCCESS)) {
		status = error;
	    }
	    max_depth--;
	}
	/* Fall through to delete top level files/directory. */
    } else {
	char spath[PATH_BUFFER_SIZE];
	char *bp;
	u_int subdir, depth;
	u_int max_subdir, max_depth;
	max_subdir = dip->di_max_subdir_number;
	max_depth = dip->di_max_subdir_depth;
	/*
	 * For each subdirectory, remove all files and directories.
	 */
	if (dip->di_debug_flag || dip->di_fDebugFlag) {
	    Printf(dip, "Removing %u subdirs with depth of %u...\n", max_subdir, max_depth);
	}
	while (max_subdir && max_depth) {
	    while (max_depth) {
		for (subdir=0; (subdir < max_subdir); ) {
		    bp = spath;
		    subdir++;
		    bp += sprintf(bp, "%s%u", dip->di_dirprefix, subdir);
		    for (depth = 1; (depth < max_depth); ) {
			depth++;
			bp += sprintf(bp, "%c%s%u", dip->di_dir_sep, dip->di_dirprefix, depth);
		    }
		    error = delete_subdir_files(dip, spath);
		    if (error && (status == SUCCESS)) {
			status = error;
		    }
		}
		max_depth--;
	    }
	    max_subdir--;
	}
	/* Fall through to delete top level files/directory. */
    }
 
    /* Delete the top level files. */
    error = delete_subdir_files(dip, NULL);
    if (error && (status == SUCCESS)) {
	status = error;
    }

    /* Ok, now delete the directory (if we created it). */
    if ( (status == SUCCESS) &&
	 (dip->di_dir_created && dip->di_dirpath) ) {
	error = remove_directory(dip, dip->di_dirpath);
	if (error) status = error;
    }

    /* Ok, now delete the top level directory (if we created it). */
    if ( (status == SUCCESS) && (delete_topdir == True) &&
	 (dip->di_topdir_created && dip->di_topdirpath) ) {
	if ( dt_file_exists(dip, dip->di_topdirpath) == True ) {
	    error = remove_directory(dip, dip->di_topdirpath);
	    if (error) status = error;
	}
    }
    dip->di_deleting_flag = False;
    return (status);
}

/*
 * delete_subdir_files() - Delete Subdirectory Files.
 *
 * Description:
 *	This function sets up the subdirectory path, then deletes
 * one or more files, then the subdirectory itself.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	spath = The subdirectory to remove.
 *
 * Return Value:
 *	Returns SUCCESS/FAILURE = File/Directory Deleted/Deletion Failed
 */
int
delete_subdir_files(struct dinfo *dip, char *spath)
{
    char dirpath[PATH_BUFFER_SIZE];
    char *file;
    int status = SUCCESS;

    *dirpath = '\0';
    if (spath) {
	if (dip->di_dirpath) {
	    (void)sprintf(dirpath, "%s%c%s", dip->di_dirpath, dip->di_dir_sep, spath);
	} else {
	    (void)sprintf(dirpath, "%s", spath);
	}
	/*
	 * When terminating prematurely, we may be interrupting the creation
	 * of the directory or file, so we must check for existance, otherwise
	 * we'll encounter false failures.
	 */
	if ( dt_file_exists(dip, dirpath) == False ) {
	    return (SUCCESS);
	}
    }
    if (dip->di_subdir)	{
	Free(dip, dip->di_subdir);
	dip->di_subdir = NULL;
    }
    /*
     * Allow no subdirectory to setup for top level files.
     */
    if (spath) {
	dip->di_subdir = strdup(spath);
    }

    if (dip->di_file_limit == 0) {
	file = make_file_name(dip);
	if ( dt_file_exists(dip, file) == True ) {
	    status = dt_delete_file(dip, file, EnableErrors);
	}
    } else {
	int error;
	dip->di_file_number = 0;
	do {
	    file = make_file_name(dip);
	    /*
	     * All files may not exist if the file system is full,
	     * or we terminated prematurely (runtime, signal, etc).
	     */
	    if ( dt_file_exists(dip, file) == True ) {
		error = dt_delete_file(dip, file, EnableErrors);
		if (error && (status == SUCCESS)) {
		    status = error;
		}
	    } else {
		break; /* Try to remove directory below. */
	    }
	} while ( ++dip->di_file_number < dip->di_file_limit );
    }
    /*
     * If all files were removed, delete the subdirectory too!
     */
    if ( (status == SUCCESS) && spath) {
	status = remove_directory(dip, dirpath);
    }
    return (status);
}

/*
 * dt_file_exists() - Check for file existance.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	file = File name to check for existance.
 *
 * Return Value:
 *	Returns True/False = File exists / Does not exist.
 */
hbool_t
dt_file_exists(struct dinfo *dip, char *file)
{
    hbool_t exists;
    int rc = SUCCESS;

    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, GETATTR_OP);
	exists = os_file_exists(file);
	DISABLE_NOPROG(dip);
	if (exists == False) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_GET_FILE_ATTR_OP, GETATTR_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    /* Note: We only expect "file/path not found" errors! */
	    /*       These occur with multiple dirs/files and file system full. */
	    if (os_isFileNotFound(error) == True) break;
	    /* Note: We run a small chance of callers not expecting this error. */
	    if (os_isDirectoryNotFound(error) == True) break;
	    rc = ReportRetryableError(dip, eip, "Failed get attributes for %s", file);
	}
    } while ( (exists == False) && (rc == RETRYABLE) );

    return(exists);
}

/*
 * dt_get_file_size() - Get The Current File Size.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	file = Pointer to the file name.
 * 	fd = Pointer to the file handle.
 * 	errors = The error control flag.
 *
 * Outputs:
 * 	Returns the file size or FAILURE.
 */
large_t
dt_get_file_size(struct dinfo *dip, char *file, HANDLE *fd, hbool_t errors)
{
    int rc = SUCCESS;
    large_t filesize;

    if (dip->di_debug_flag || dip->di_fDebugFlag) {
	Printf(dip, "Getting file size for file %s...\n", file);
    }
    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, GETATTR_OP);
	filesize = os_get_file_size(file, *fd);
	DISABLE_NOPROG(dip);
	if (filesize == (large_t)FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_GET_FILE_ATTR_OP, GETATTR_OP, fd, 0, (Offset_t)0, 0,
			    error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed to get file size for %s", file);
	}
    } while ( (filesize == (large_t)FAILURE) && (rc == RETRYABLE) );

    return(filesize);
}


/*
 * dt_isdir() - Check path for existance and a directory.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	path = File name to check for existance.
 *	errors = The error control flag.
 *
 * Return Value:
 *	Returns True/False = File exists and is a directory / Does not exist.
 */
hbool_t
dt_isdir(struct dinfo *dip, char *path, hbool_t errors)
{
    hbool_t isdir;
    int rc = SUCCESS;
    int status;

    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, GETATTR_OP);
	status = os_file_information(path, NULL, &isdir, NULL);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, path, OS_GET_FILE_ATTR_OP, GETATTR_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    /* Note: We only expect "file not found" errors! */
	    if (os_isFileNotFound(error) == True) break;
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed get attributes for %s", path);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return(isdir);
}

/*
 * dt_isfile() - Check path for existance and a file.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	path = File name to check for existance.
 *	errors = The error control flag.
 *
 * Return Value:
 *	Returns True/False = File exists and is a file / Does not exist.
 */
hbool_t
dt_isfile(struct dinfo *dip, char *path, hbool_t errors)
{
    hbool_t isfile;
    int rc = SUCCESS;
    int status;

    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, GETATTR_OP);
	status = os_file_information(path, NULL, NULL, &isfile);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, path, OS_GET_FILE_ATTR_OP, GETATTR_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    /* Note: We only expect "file not found" errors! */
	    if (os_isFileNotFound(error) == True) break;
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed get attributes for %s", path);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return(isfile);
}

/*
 * dt_close_file() - Close a file with retries.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	file = The file name being closed.
 * 	handle = Pointer to file handle to close.
 *	isDiskFull = Pointer to disk full flag.
 * 	errors = The error control flag.
 * 	retrys = The retry control flag.
 *
 * Return Value:
 *	handle set to NoFd to indicate its' closed!
 *	Returns Success / Failure.
 */
int
dt_close_file(dinfo_t *dip, char *file, HANDLE *handle,
	      hbool_t *isDiskFull, hbool_t errors, hbool_t retrys)
{
    int status;
    int rc = SUCCESS;

    if (retrys == True) dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, CLOSE_OP);
	status = os_close_file(*handle);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_CLOSE_FILE_OP, CLOSE_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (isDiskFull) {
		*isDiskFull = os_isDiskFull(error);
		if (*isDiskFull == True) break;
	    }
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    if (retrys == False) eip->ei_rpt_flags |= RPT_NORETRYS;
	    rc = ReportRetryableError(dip, eip, "Failed to close file %s", file);
	} else {
	    *handle = NoFd;
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return(status);
}

/*
 * dt_create_directory() - Create a Directory with retries.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	dir = Pointer to directory to be created.
 * 	isDiskFull = Pointer to disk full flag.
 * 	isFileExists = Pointer to file exists flag.
 * 	errors = The error control flag.
 *
 * Return Value:
 *	Returns SUCCESS/FAILURE/WARNING:
 *	  SUCCESS = Directory created.
 *        FAILURE = Directory create failed.
 *        WARNING = Windows drive letter.
 */
int
dt_create_directory(dinfo_t *dip, char *dir,
		    hbool_t *isDiskFull, hbool_t *isFileExists, hbool_t errors)
{
    hbool_t is_dir;
    int rc = SUCCESS;
    int status = WARNING;

    if (isDiskFull) *isDiskFull = False;
    if (isFileExists) *isFileExists = False;

    /*
     * Ensure user specified a directory, or create as necessary.
     */
    if ( os_file_information(dir, NULL, &is_dir, NULL) == SUCCESS ) {
	/* File exists, ensure it's a directory. */
	if ( is_dir == False ) {
	    Fprintf(dip, "%s is not a directory!\n", dir);
	    return(FAILURE);
	}
    } else if (dip->di_debug_flag || dip->di_fDebugFlag) {
	Printf(dip, "Creating directory %s...\n", dir);
    }
    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, MKDIR_OP);
	status = os_create_directory(dir, DIR_CREATE_MODE);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    char *op = OS_CREATE_DIRECTORY_OP;
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, dir, op, MKDIR_OP, NULL, 0, (Offset_t)0, (size_t)0,
			    error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    /* Note: The next step is a function callout for expected errors! */
	    if (isDiskFull) {
		*isDiskFull = os_isDiskFull(error);
		if (*isDiskFull == True) break;
	    }
	    /* Note: This is returned for directories and files. */
	    if (isFileExists) {
		*isFileExists = os_isFileExists(error);
		if (*isFileExists == True) break;
	    }
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed to create directory %s", dir);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );
    return(status);
}

/*
 * dt_extend_file() - Extend a File to Specific Size.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	file = The file name being closed.
 * 	handle = Pointer to file handle to close.
 * 	buffer = The buffer address.
 * 	write_size = The number of bytes.
 *	isDiskFull = Pointer to disk full flag.
 * 	errors = The error control flag.
 *
 * Return Value:
 *	Returns Success / Failure.
 */
int
dt_extend_file(dinfo_t *dip, char *file, HANDLE handle,
	       void *buffer, size_t write_size, large_t data_limit, hbool_t errors)
{
    ssize_t bytes_written;
    Offset_t offset = (data_limit - write_size);
    int status = SUCCESS;

    if (data_limit < write_size) return(status);
    if (dip->di_debug_flag) {
        Printf(dip, "Extending file to " LUF " bytes, by writing "SUF" bytes at offset " FUF "...\n",
               data_limit, write_size, offset);
    }
    bytes_written = pwrite(handle, buffer, write_size, offset);
    if ((size_t)bytes_written != write_size) {
	os_error_t error = os_get_error();
	INIT_ERROR_INFO(eip, file, OS_WRITE_FILE_OP, WRITE_OP, &handle, 0, (Offset_t)offset,
			(size_t)write_size, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	status = ReportRetryableError(dip, eip, "Failed to write file %s", file);
    } else {
	Offset_t npos = set_position(dip, (Offset_t)0, False);
	if (npos == (Offset_t)-1) status = FAILURE;
    }
    return(status);
}

/*
 * dt_flush_file() - Flush a file with retries.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	file = The file name being closed.
 * 	handle = Pointer to file handle to close.
 *	isDiskFull = Pointer to disk full flag.
 * 	errors = The error control flag.
 *
 * Return Value:
 *	Returns Success / Failure.
 */
int
dt_flush_file(dinfo_t *dip, char *file, HANDLE *handle,
	      hbool_t *isDiskFull, hbool_t errors)
{
    int status;
    int rc = SUCCESS;

    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, FSYNC_OP);
	status = os_flush_file(*handle);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_FLUSH_FILE_OP, FSYNC_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (isDiskFull) {
		*isDiskFull = os_isDiskFull(error);
		if (*isDiskFull == True) break;
	    }
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed to flush file %s", file);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return(status);
}

/*
 * dt_lock_file() - Lock a file with retries.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	file = The file name being locked.
 * 	handle = Pointer to file handle to close.
 * 	start = Starting offset of byte range to lock.
 * 	length = The length of bytes to lock.
 * 	type = The lock type (POSIX style).
 * 	exclusive = Exclusive access flag. (Windows) (False = Shared lock)
 * 	immediate = Immediate lock flag. (Windows) (False = wait for lock)
 * 	errors = The error control flag.
 *
 * Return Value:
 *	Returns Success / Failure.
 */
int
dt_lock_file(dinfo_t *dip, char *file, HANDLE *handle,
	     Offset_t start, Offset_t length, int type,
	     hbool_t exclusive, hbool_t immediate, hbool_t errors)
{
    int status;
    int rc = SUCCESS;

    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, LOCK_OP);
	status = os_xlock_file(*handle, start, length, type, exclusive, immediate);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_LOCK_FILE_OP, LOCK_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed to lock file %s", file);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return(status);
}

/*
 * dt_unlock_file() - Unlock a file with retries.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	file = The file name being locked.
 * 	handle = Pointer to file handle to close.
 * 	start = Starting offset of byte range to lock.
 * 	length = The length of bytes to lock.
 * 	errors = The error control flag.
 *
 * Return Value:
 *	Returns Success / Failure.
 */
int
dt_unlock_file(dinfo_t *dip, char *file, HANDLE *handle,
	       Offset_t start, Offset_t length, hbool_t errors)
{
    int status;
    int rc = SUCCESS;

    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, UNLOCK_OP);
	status = os_unlock_file(*handle, start, length);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_UNLOCK_FILE_OP, UNLOCK_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed to unlock file %s", file);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return(status);
}

static char *lock_type_table[] = {
    "read",	/* LOCK_TYPE_READ = 0 */
    "write",	/* LOCK_TYPE_WRITE = 1 */
    "unlock"	/* LOCK_TYPE_UNLOCK = 2 */
};

int
dt_lock_unlock(dinfo_t *dip, char *file, HANDLE *fd,
	       lock_type_t lock_type, Offset_t offset, Offset_t length)
{
    int lock_type_flag;
    hbool_t exclusive = True, immediate = True, unlock = False;
    int status = SUCCESS;

    if ( (lock_type == LOCK_TYPE_UNLOCK) && (dt_unlock_file_chance(dip) == False) ) {
	if (dip->di_lDebugFlag == True) {
	    Printf(dip, "File: %s, randomly skipping unlock, offset "FUF", length "FUF"\n",
		   file, offset, length);
	}
	return(status);
    }
    if (dip->di_lDebugFlag == True) {
	Printf(dip, "File: %s, lock type = %s, offset "FUF", length "FUF"\n",
	       file, lock_type_table[lock_type], offset, length);
    }
    status = os_set_lock_flags(lock_type, &lock_type_flag, &exclusive, &immediate, &unlock);
    if (status == FAILURE) {
	Eprintf(dip, "Unknown lock type %d\n", lock_type);
	return(FAILURE);
    }
    dip->di_lock_stats[lock_type]++;	/* Adjust the lock statistics. */

    if (unlock == False) {
	/* Note: exclusive and immediate flags are *not* used on Unix! */
	status = dt_lock_file(dip, file, fd, offset, length, lock_type_flag, exclusive, immediate, EnableErrors);
    } else {
	status = dt_unlock_file(dip, file, fd, offset, length, EnableErrors);
    }
    /* Note: May count errors by lock type later. */
    if (status == FAILURE) dip->di_lock_errors++;
    return(status);
}

/*
 * dt_rename_file() - Rename a file with retries.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	oldpath = The old path name.
 * 	newpath = The new path name.
 *	isDiskFull = Pointer to disk full flag.
 * 	errors = The error control flag.
 *
 * Return Value:
 *	Returns SUCCESS / FAILURE = File renamed / rename failed.
 * 	Note: Returns success for "file not found" error w/disconnects.
 */
int
dt_rename_file(dinfo_t *dip, char *oldpath, char *newpath,
	       hbool_t *isDiskFull, hbool_t errors)
{
    int status;
    int rc = SUCCESS;

    if (isDiskFull) *isDiskFull = False;
    if (dip->di_debug_flag || dip->di_fDebugFlag) {
	Printf(dip, "Renaming %s to %s...\n", oldpath, newpath);
    }
    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, RENAME_OP);
	status = os_rename_file(oldpath, newpath);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, oldpath, OS_RENAME_FILE_OP, RENAME_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (isDiskFull) {
		*isDiskFull = os_isDiskFull(error);
		if (*isDiskFull == True) break;
	    }
	    if ( (dip->di_retry_disconnects == True) &&
		 (dip->di_retry_count > 0) &&
		 os_isFileNotFound(error) ) {
		status = SUCCESS;
		break;
	    }
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed to rename %s to %s", oldpath, newpath);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return(status);
}

/*
 * dt_read_file() - Write a file with retries.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	file = The file name being closed.
 * 	handle = Pointer to file handle.
 * 	buffer = The buffer address.
 * 	bytes = The number of bytes.
 * 	errors = The error control flag.
 * 	retrys = The retry control flag.
 *
 * Return Value:
 *	Returns the number of bytes written, or -1 for FAILURE.
 */
ssize_t
dt_read_file(dinfo_t *dip, char *file, HANDLE *handle,
	     void *buffer, size_t bytes, hbool_t errors, hbool_t retrys)
{
    int rc = SUCCESS;
    ssize_t bytes_read;

    dip->di_retry_count = 0;
    dip->di_mode = READ_MODE;
    do {
	ENABLE_NOPROG(dip, READ_OP);
	bytes_read = os_read_file(*handle, buffer, bytes);
	DISABLE_NOPROG(dip);
	if (bytes_read == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_READ_FILE_OP, READ_OP, handle, dip->di_oflags, dip->di_offset,
			    (size_t)bytes, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    /* TODO: Update callers to properly maintain the file offset! */
	    //dip->di_offset = eip->ei_offset = dt_get_position(dip, file, handle, EnableErrors, EnableRetries);
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    if (retrys == False) eip->ei_rpt_flags |= RPT_NORETRYS;
	    rc = ReportRetryableError(dip, eip, "Failed to read file %s", file);
	}
    } while ( (bytes_read == FAILURE) && (rc == RETRYABLE) );

    return(bytes_read);
}

/*
 * dt_write_file() - Write a file with retries.
 *
 * Inputs:
 * 	dip = The device information pointer.
 * 	file = The file name being closed.
 * 	handle = Pointer to file handle.
 * 	buffer = The buffer address.
 * 	bytes = The number of bytes.
 *	isDiskFull = Pointer to disk full flag.
 * 	errors = The error control flag.
 * 	retrys = The retry control flag.
 *
 * Return Value:
 *	Returns the number of bytes written, or -1 for FAILURE.
 */
ssize_t
dt_write_file(dinfo_t *dip, char *file, HANDLE *handle,
	      void *buffer, size_t bytes,
	      hbool_t *isDiskFull, hbool_t errors, hbool_t retrys)
{
    int rc = SUCCESS;
    ssize_t bytes_written;

    dip->di_retry_count = 0;
    dip->di_mode = WRITE_MODE;
    do {
	ENABLE_NOPROG(dip, WRITE_OP);
	bytes_written = os_write_file(*handle, buffer, bytes);
	DISABLE_NOPROG(dip);
	if (bytes_written == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_WRITE_FILE_OP, WRITE_OP, handle, dip->di_oflags, dip->di_offset,
			    (size_t)bytes, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (isDiskFull) {
		*isDiskFull = os_isDiskFull(error);
		if (*isDiskFull == True) break;
	    }
	    /* TODO: Update callers to properly maintain the file offset! */
	    //dip->di_offset = eip->ei_offset = dt_get_position(dip, file, handle, EnableErrors, EnableRetries);
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    if (retrys == False) eip->ei_rpt_flags |= RPT_NORETRYS;
	    rc = ReportRetryableError(dip, eip, "Failed to write file %s", file);
	}
    } while ( (bytes_written == FAILURE) && (rc == RETRYABLE) );

    return(bytes_written);
}

/*
 * dt_truncate_file() - Truncate a file using the file name.
 *
 * Description:
 *	This function truncates the file at the specified offset.
 *
 * Inputs
 *	dip = The device information pointer.
 * 	file = The file name to truncate.
 *	offset = The offset to truncate file at.
 *	isDiskFull = Pointer to disk full flag.
 * 	errors = The error control flag.
 *
 * Return Value:
 *	Returns SUCCESS/FAILURE = File Truncated/Truncate failed.
 */
int
dt_truncate_file(struct dinfo *dip, char *file,	Offset_t offset,
		 hbool_t *isDiskFull, hbool_t errors)
{
    int status;
    int rc = SUCCESS;

    if (isDiskFull) *isDiskFull = False;
    if (dip->di_debug_flag || dip->di_fDebugFlag) {
	Printf(dip, "Truncating file %s at offset " FUF "...\n", file, offset);
    }
    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, TRUNCATE_OP);
	status = os_truncate_file(file, offset);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_TRUNCATE_FILE_OP, TRUNCATE_OP, NULL, 0, (Offset_t)offset,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (isDiskFull) {
		*isDiskFull = os_isDiskFull(error);
		if (*isDiskFull == True) break;
	    }
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed to truncate file %s", file);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return (status);
}

/*
 * dt_ftruncate_file() - Truncate a file with an open file descriptor.
 *
 * Description:
 *	This function truncates the file at the specified offset.
 *
 * Inputs
 *	dip = The device information pointer.
 * 	file = The file name to truncate.
 * 	fd = The open file descriptor.
 *	offset = The offset to truncate file at.
 *	isDiskFull = Pointer to disk full flag.
 * 	errors = The error control flag.
 *
 * Return Value:
 *	Returns SUCCESS/FAILURE = File Truncated/Truncate failed.
 */
int
dt_ftruncate_file(struct dinfo *dip, char *file, HANDLE fd, Offset_t offset,
		  hbool_t *isDiskFull, hbool_t errors)
{
    int status;
    int rc = SUCCESS;

    if (dip->di_debug_flag || dip->di_fDebugFlag) {
	Printf(dip, "Truncating file %s at offset " FUF "...\n", file, offset);
    }
    dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, TRUNCATE_OP);
	status = os_ftruncate_file(fd, offset);
	DISABLE_NOPROG(dip);
	if (status == FAILURE) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_FTRUNCATE_FILE_OP, TRUNCATE_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, error, logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    if (isDiskFull) {
		*isDiskFull = os_isDiskFull(error);
		if (*isDiskFull == True) break;
	    }
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    rc = ReportRetryableError(dip, eip, "Failed to truncate file %s", file);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return (status);
}

int
reopen_after_disconnect(dinfo_t *dip, error_info_t *eip)
{
    int oflags = eip->ei_oflags;
    char *file = eip->ei_file;
    HANDLE *fdp = eip->ei_fd;
    Offset_t offset = eip->ei_offset;
    HANDLE fd;
    int status = SUCCESS;

    if ( (fdp == NULL) || (*fdp == NoFd) ) {
	return(status);
    }

#if defined(WIN32)
    /* Note: For hammer, POSIX open flags are being used and mapped. */
    if (dip->di_iobehavior != HAMMER_IO) {
	/* Note: For Windows, these flags are not POSIX, */
	/*       so, map a few to POSIX flags for now! */
	if (oflags & OS_READWRITE_MODE) {
	    oflags = O_RDWR;
	} else if (oflags & OS_READONLY_MODE) {
	    oflags = O_RDONLY;
	} else if (oflags & OS_WRITEONLY_MODE) {
	    oflags = O_WRONLY;
	}
    } else {
	oflags &= ~(O_APPEND|O_CREAT|O_TRUNC);
    }
#else /* defined(WIN32) */
    oflags &= ~(O_APPEND|O_CREAT|O_TRUNC);
#endif /* defined(WIN32) */
    /* Note: These are psuedo flags mainly for Solaris & Windows! */
    if (dip->di_aio_flag) oflags |= O_ASYNC;
    if (dip->di_dio_flag) oflags |= O_DIRECT;

    /*
     * If a file is open, we *must* reopen it to reestablish a new session!
     */
    /* Note: Switch to standard reopen function after cleanup! */
    //status = (*dip->di_funcs->tf_reopen_file)(dip, dip->di_oflags);
    fd = dt_open_file(dip, file, oflags, 0, NULL, NULL, False, False);
    if (fd == NoFd) {
	status = FAILURE;
    } else {
	(void)dt_close_file(dip, file, fdp, NULL, True, False);
	*fdp = fd; /* Save the new file handle! */
	Printf(dip, "Re-open'ed file %s after session disconnect!\n", file);
	if (offset) {
	    Offset_t noffset = dt_set_position(dip, file, fdp, offset, EnableErrors, DisableRetries);
	    /* Note: We must handle the error, to avoid recursion! */
	    if ( (noffset == (Offset_t)-1) || (noffset != offset) ) {
		Fprintf(dip, "Failed to reset offset to "FUF", offset set to "FUF"\n",
			offset, noffset);
	    } else {
		Printf(dip, "Seeked to offset "FUF", after reopen.\n", noffset);
	    }
	    if (dip->di_fsfile_flag && dip->di_debug_flag) {
		large_t filesize = os_get_file_size(file, *fdp);
		if (filesize != (large_t)FAILURE) {
		    Printf(dip, "After reopen, the file size is " LUF " bytes.\n", filesize);
		}
	    }
	}
    }
    return(status);
}

/*
 * This function creates a unique log file name.
 *
 * Inputs:
 *      dip = The device information pointer.
 *
 * Return Value:
 *      None (void)
 */
void
make_unique_log(dinfo_t *dip)
{
    /*
     * Create a unique log file (if requested).
     */
    if (dip->di_log_file && (dip->di_unique_log || strstr (dip->di_log_file, "%")) ) {
        char logfmt[STRING_BUFFER_SIZE];
	char *path;
        strcpy(logfmt, dip->di_log_file);
        if ( strstr(dip->di_log_file, "%pid") == (char *) 0 ) {
            strcat(logfmt, "-%pid");
        }
        path = FmtLogFile(dip, logfmt, True);
	if (path) {
	    Free(dip, dip->di_log_file);
	    dip->di_log_file = path;
	}
	if (path) {    
	    if (freopen (dip->di_log_file, (dip->di_logappend_flag) ? "a" : "w", efp) == NULL) {
		Perror (dip, "freopen() of %s failed, exiting...\n", dip->di_log_file);
	    }
        }
        if (dip->di_logheader_flag) log_header(dip, False);
    }
}

/*
 * skip_device_prefix() - Simple Utility Function to skip device prefixes.
 * 
 * Inputs:
 *      device = The device path (may be raw or file system)
 * 
 * Return Value:
 *      Returns the updated device path past device directory prefix.
 */ 
char *
skip_device_prefix(char *device)
{
    if ( EQL(device, DEV_PREFIX, DEV_LEN) ) {
        device += DEV_LEN;
    } else if (EQL(device, ADEV_PREFIX, ADEV_LEN) ) {
        device += ADEV_LEN;
#if defined(NDEV_PREFIX)
    } else if ( EQL(device, NDEV_PREFIX, NDEV_LEN) ) {
        device += NDEV_LEN;
#endif /* defined(NDEV_PREFIX) */
    }
    return (device);
}

int
do_file_trim(dinfo_t *dip)
{
    uint64_t data_bytes;
    Offset_t offset;
    HANDLE handle;
    int rc = SUCCESS;
    int status = SUCCESS;

    (void)get_transfer_limits(dip, &data_bytes, &offset);
    if (dip->di_debug_flag || dip->di_fDebugFlag) {
	Printf(dip, "Trimming file %s, starting offset: "FUF", length: "LUF" bytes\n",
	       dip->di_dname, offset, data_bytes);
    }
    handle = dt_open_file(dip, dip->di_dname, O_RDWR, 0, NULL, NULL, True, True);
    if (handle == NoFd) {
	return(FAILURE);
    }

    dip->di_retry_count = 0;
    do {
	status = os_file_trim(handle, offset, data_bytes);
	if (status == FAILURE) {
	    INIT_ERROR_INFO(eip, dip->di_dname, OS_TRIM_FILE_OP, TRIM_OP, NULL, 0, (Offset_t)0,
			    (size_t)0, os_get_error(), logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    rc = ReportRetryableError(dip, eip, "Failed to trim file %s", dip->di_dname);
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    if (status == WARNING) {
	Wprintf(dip, "This OS or FS does NOT support file trim operations, disabling!\n");
	dip->di_fstrim_flag = False;
	status = SUCCESS;
    }
    (void)dt_close_file(dip, dip->di_dname, &handle, NULL, True, True);
    return (status);
}

int
get_transfer_limits(dinfo_t *dip, uint64_t *data_bytes, Offset_t *offset)
{
    if (dip->di_random_io && dip->di_rdata_limit) {
	*data_bytes = (dip->di_rdata_limit - dip->di_file_position);
    } else if (dip->di_data_limit && (dip->di_data_limit != INFINITY)) {
	*data_bytes = dip->di_data_limit;
    } else {
	*data_bytes = 0;
    }
    *offset = dip->di_file_position;
    return(SUCCESS);
}

large_t
calculate_max_data(dinfo_t *dip)
{
    large_t max_files = calculate_max_files(dip);
    large_t max_data = (dip->di_max_data) ? dip->di_max_data : dip->di_data_limit;
    max_data *= max_files;
    return(max_data);
}

large_t
calculate_max_files(dinfo_t *dip)
{
    large_t max_files;
    u_long files = dip->di_file_limit;
    u_long dir_files, sdepth_files, subdir_files;

    if (dip->di_max_files) {
	max_files = dip->di_max_files;
    } else {
	if (files == 0) files++; /* one file please! */
	max_files = files;
	sdepth_files = (dip->di_user_subdir_depth * files);
	if (sdepth_files) {
	    subdir_files = (dip->di_user_subdir_limit * sdepth_files);
	} else {
	    subdir_files = (dip->di_user_subdir_limit * files);
	}
	if (subdir_files) {
	    dir_files = (dip->di_user_dir_limit * subdir_files);
	} else {
	    dir_files = (dip->di_user_dir_limit * files);
	}
	if (dir_files) {
	    max_files += dir_files;
	} else if (subdir_files) {
	    max_files += subdir_files;
	} else if (sdepth_files) {
	    max_files += sdepth_files;
	}
    }
    return(max_files);
}

/* Returns True if free space is available. */
hbool_t
report_filesystem_free_space(dinfo_t *dip)
{
    (void)os_get_fs_information(dip, dip->di_dir);
    if (dip->di_fs_space_free) {
	Printf(dip, "Free space is "LUF" bytes of total "LUF", for directory %s...\n",
		dip->di_fs_space_free, dip->di_fs_total_space, dip->di_dir);
    } else { /* No free space left! */
	/* Note: Left caller decide if this is an error or not! */
	Wprintf(dip, "Free space is "LUF" bytes of total "LUF", for directory %s...\n",
		dip->di_fs_space_free, dip->di_fs_total_space, dip->di_dir);
    }
    return( (dip->di_fs_space_free == 0) ? False : True );
}
    
int
verify_filesystem_space(dinfo_t *dip, hbool_t all_threads_flag)
{
    /*
     * Verify the file system has sufficient space for the data limit specified.
     */
    if ( (dip->di_iobehavior == DT_IO) && (dip->di_bypass_flag == False) &&
	 dip->di_fsfile_flag && dip->di_output_file && dip->di_fs_space_free ) {
	large_t max_files = calculate_max_files(dip);
	large_t max_data = (dip->di_max_data) ? dip->di_max_data : dip->di_data_limit;
	if (all_threads_flag == True) {
	    /* Threads create separate file, slices is to the same file. */
	    if (dip->di_slices == 0) {
		max_files *= dip->di_threads; /* Directory per thread. */
	    }
	    max_data *= max_files;
	} else { /* Single thread. */
	    if (dip->di_slices) {
		max_data *= dip->di_slices; /* Expected data for all slices. */
	    }
	}
	if ( (max_data != INFINITY) && (dip->di_fs_space_free < max_data) ) {
	    if ( (dip->di_threads <= 1) || dip->di_log_file ) {
		Wprintf(dip, "The free space of "LUF" bytes, is less than the data limit "LUF" required!\n",
			dip->di_fs_space_free, max_data);
	    }
	}
    }    
    return(SUCCESS);
}

hbool_t
is_fsfull_restartable(dinfo_t *dip)
{
    /* If the file system is in buffered I/O, restart writes without buffering. */
    if ( dip->di_fsfile_flag && dip->di_fsfull_restart && dip->di_file_system_full ) {
	if (is_unbuffered_mode(dip) == True) {
	    ; /* Already buffered, no restart required! */
	} else {
	    return(True);
	}
    }
    return(False);
}

hbool_t
restart_on_file_system_full(dinfo_t *dip)
{
    hbool_t restart_flag = False;

    if ( restart_flag = is_fsfull_restartable(dip) ) {
	set_unbuffered_mode(dip);
    }
    return(restart_flag);
}

hbool_t
is_modulo_device_size_io(dinfo_t *dip)
{
    if (dip->di_block_size % dip->di_dsize) {
	return(False);
    }
    if ( dip->di_min_size && (dip->di_min_size % dip->di_dsize) ) {
	return(False);
    }
    if ( dip->di_max_size && (dip->di_max_size % dip->di_dsize) ) {
	return(False);
    }
    if ( ( (dip->di_variable_flag == False) && dip->di_incr_count ) &&
	 (dip->di_incr_count % dip->di_dsize) ) {
	return(False);
    }
    return(True);
}

hbool_t
is_unbuffered_mode(dinfo_t *dip)
{
    if ( (dip->di_dio_flag == True) || (dip->di_oflags & (O_DIRECT|O_DSYNC|O_SYNC)) ) {
	return(True);
    } else {
	return(False);
    }
}

void
set_unbuffered_mode(dinfo_t *dip)
{
    if ( is_modulo_device_size_io(dip) == False ) {
	dip->di_write_flags |= O_DSYNC;	/* Synchronize writes. */
    } else if (dip->di_bufmode_count) {
	int bindex;
	/* Change buffering modes to ensure no write caching! */
	for (bindex = 0; (bindex < dip->di_bufmode_count); bindex++) {
	    if (dip->di_buffer_modes[bindex] == BUFFERED_IO) {
		dip->di_buffer_modes[bindex] = UNBUFFERED_IO;
	    } else if (dip->di_buffer_modes[bindex] == CACHE_WRITES) {
		dip->di_buffer_modes[bindex] = CACHE_READS;
	    }
	}
	dip->di_fsalign_flag = True;
    } else {
	dip->di_fsalign_flag = True;
	dip->di_bufmode_count = 0;
	dip->di_buffer_modes[dip->di_bufmode_count++] = CACHE_READS;
	dip->di_buffer_modes[dip->di_bufmode_count++] = UNBUFFERED_IO;
    }
    return;
}
