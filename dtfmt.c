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
 * Module:	dtfmt.c
 * Author:	Robin T. Miller
 * Date:	August 17th, 2013
 *
 * Description:
 *	File system operations.
 *
 * Modification History:
 * 
 * November 16th, 2021 by Robin T. Miller
 *      Add %nate for NetApp NATE log prefix format string.
 * 
 * June 16th, 2021 by Robin T. Miller
 *      Add SCSI format control strings: %sdsf and %tdsf
 *      %sdsf = The SCSI device, %tdsf = the SCSI trigger device.
 * 
 * October 21st, 2020 by Robin T. Miller
 *      Add format control strings for Nimble specific SCSI information.
 * 
 * May 11th, 2020 by Robin T. MIller
 *      Add format strings for individual date and time fields for more
 * flexible formatting and add support for date/time field separators.
 * 
 * May 9th, 2020 by Robin T. Miller
 *      Use high resolution timer for more accurate I/O timing. This is
 * implemented on Windows, but Unix systems still use gettimeofday() API.
 * 
 * Decmber 15th, 2015 by Robin T. Miller
 * 	Add %workload as valid format string.
 * 
 * August 17th, 2013 by Robin T Miller
 * 	Moving formatting functions here.
 */
#include "dt.h"

/*
 * Forward References:
 */
int FmtCommon(dinfo_t *dip, char *key, char **buffer);

static char *not_available = "NA";

/*
 * FmtKeepAlive() - Format Keepalive Message.
 *
 * Special Format Characters:
 *
 *      %b = The bytes read or written.
 *      %B = The total bytes read and written.
 *      %c = The count of records this pass.
 *      %C = The total records for this test.
 *	%d = The device name.
 *	%D = The real device name.
 *      %e = The number of errors.
 *      %E = The error limit.
 *      %f = The files read or written.
 *      %F = The total files read and written.
 *	%h = The host name.
 *	%H = The full host name.
 *      %i = The I/O mode ("read" or "write").
 *      %k = The kilobytes this pass.
 *      %K = The total kilobytes this test.
 *      %l = The logical blocks read or written.
 *      %L = The total blocks read and written.
 *      %m = The megabytes this pass.
 *      %M = The total megabytes this test.
 *	%p = The pass count.
 *	%P = The pass limit.
 *      %r = The records read this pass.
 *      %R = The total records read this test.
 *      %s = The seconds this pass.
 *      %S = The total seconds this test.
 *      %t = The pass elapsed time.
 *      %T = The total elapsed time.
 *	%u = The user (login) name.
 *      %w = The records written this pass.
 * 	%W = The total records written this test.
 * 
 * Performance Keywords: (upper and lower case, see below)
 *      %bps  = The bytes per second.
 *      %lbps = The blocks per second.
 *      %kbps = The kilobytes per second.
 *      %mbps = The megabytes per second.
 *      %iops = The I/O's per second.
 *      %spio = The seconds per I/O.
 * 
 * Lowercase means per pass stats, while uppercase means total stats.
 *
 * I/O Keywords:
 *	%bufmode = The file buffer mode (file systems).
 * 	%iodir = The I/O direction (forward or reverse).
 * 	%iotype = The I/O type (random or sequential).
 * 	%lba = The current logical block address.
 *      %offset = The current file offset.
 *      %status = The thread exit status.
 * 
 * Misc Keywords:
 * 	%keepalivet = The keepalive time.
 * 
 * See FmtCommon() for common format control strings.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	keepalivefmt = Keepalive formal control string.
 *	buffer = Buffer for formatted message.
 *
 * Outputs:
 *	Returns the number of characters formatted.
 */
size_t
FmtKeepAlive(struct dinfo *dip, char *keepalivefmt, char *buffer)
{
    char    *from = keepalivefmt;
    char    *to = buffer;
    ssize_t length = (ssize_t)strlen(keepalivefmt);

    while (length-- > 0) {
	hbool_t full_info = False;
	/*
	 * Running out of single characters, use key words for performance.
	 */
	if (*from == '%') {
	    char *key = (from + 1);
	    /*
	     * Performance Keywords:
	     */
	    if (strncasecmp(key, "bps", 3) == 0) {
		int secs;
		large_t bytes;
		hbool_t pass_stats = (strncmp(key, "bps", 3) == 0);
		bytes = GetStatsValue(dip, ST_BYTES, pass_stats, &secs);
		if (secs) {
		    to += Sprintf(to, "%.3f", ((double)bytes / (double)secs));
		} else {
		    to += Sprintf(to, "0.000");
		}
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "lbps", 4) == 0) {
		int secs;
		large_t blocks;
		hbool_t pass_stats = (strncmp(key, "lbps", 4) == 0);
		blocks = GetStatsValue(dip, ST_BLOCKS, pass_stats, &secs);
		if (secs && dip->di_dsize) {
		    to += Sprintf(to, "%.3f", ((double)blocks / (double)secs));
		} else {
		    to += Sprintf(to, "0.000");
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "kbps", 4) == 0) {
		int secs;
		large_t bytes;
		hbool_t pass_stats = (strncmp(key, "kbps", 4) == 0);
		bytes = GetStatsValue(dip, ST_BYTES, pass_stats, &secs);
		if (secs) {
		    to += Sprintf(to, "%.3f", ((double)bytes / (double)KBYTE_SIZE) / secs);
		} else {
		    to += Sprintf(to, "0.000");
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "mbps", 4) == 0) {
		int secs;
		large_t bytes;
		hbool_t pass_stats = (strncmp(key, "mbps", 4) == 0);
		bytes = GetStatsValue(dip, ST_BYTES, pass_stats, &secs);
		if (secs) {
		    to += Sprintf(to, "%.3f", ((double)bytes / (double)MBYTE_SIZE) / secs);
		} else {
		    to += Sprintf(to, "0.000");
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "iops", 4) == 0) {
		int secs;
		u_long records;
		hbool_t pass_stats = (strncmp(key, "iops", 4) == 0);
		records = (u_long)GetStatsValue(dip, ST_RECORDS, pass_stats, &secs);
		if (secs) {
		    to += Sprintf(to, "%.3f", ((double)records / (double)secs));
		} else {
		    to += Sprintf(to, "0.000");
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "spio", 4) == 0) {
		int secs;
		u_long records;
		hbool_t pass_stats = (strncmp(key, "spio", 4) == 0);
		records = (u_long)GetStatsValue(dip, ST_RECORDS, pass_stats, &secs);
		if (records) {
		    to += Sprintf(to, "%.4f", ((double)secs / (double)records));
		} else {
		    to += Sprintf(to, "0.0000");
		}
		length -= 4;
		from += 5;
		continue;
	    /*
	     * I/O Keywords:
	     */
	    } else if (strncasecmp(key, "bufmode", 7) == 0) {
		if (dip->di_bufmode_type) {
		    to += sprintf(to, "%s", dip->di_bufmode_type);
		}
		length -= 7;
		from += 8;
		continue;
	    } else if (strncasecmp(key, "iodir", 5) == 0) {
		/* Note: These may vary, but will always be one of these! */
                if (dip->di_io_dir == FORWARD) {
                    to += Sprintf(to, "%s", "forward");
                } else {
                    to += Sprintf(to, "%s", "reverse");
                }
                length -= 5;
                from += 6;
                continue;
            } else if (strncasecmp(key, "iotype", 6) == 0) {
                if (dip->di_io_type == SEQUENTIAL_IO) {
                    to += Sprintf(to, "%s", "sequential");
                } else {
                    to += Sprintf(to, "%s", "random");
                }
                length -= 6;
                from += 7;
                continue;
	    } else if (strncasecmp(key, "lba", 3) == 0) {
		Offset_t offset;
		offset = (Offset_t)GetStatsValue(dip, ST_OFFSET, False, NULL);
		to += Sprintf(to, "%u", (dip->di_dsize) ? (u_int32)(offset / dip->di_dsize) : 0);
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "offset", 6) == 0) {
		Offset_t offset;
		offset = (Offset_t)GetStatsValue(dip, ST_OFFSET, False, NULL);
		to += Sprintf(to, FUF, offset);
		length -= 6;
		from += 7;
		continue;
	    } else if (strncasecmp(key, "status", 6) == 0) {
		to += Sprintf(to, "%d", dip->di_exit_status);
		length -= 6;
		from += 7;
		continue;
	    /*
	     * Misc Keywords:
	     */
	    } else if (strncasecmp(key, "keepalivet", 10) == 0) {
		to += Sprintf(to, "%d", dip->di_keepalive_time);
		length -= 10;
		from += 11;
		continue;
	    } else {
		int len = FmtCommon(dip, key, &to);
		if (len) {
		    length -= len;
		    from += (len + 1);
		    continue;
		}
	    }
	}
	/*
	 * The approach taken is: lower = pass, upper = total
	 */
	switch (*from) {
	    case '%': {
		if (length) {
		    switch (*(from + 1)) {
			case 'b': {
			    if (dip->di_raw_flag || (dip->di_io_mode == COPY_MODE)) {
				to += Sprintf(to, LUF,
					      (dip->di_dbytes_read + dip->di_dbytes_written));
			    } else if (dip->di_mode == READ_MODE) {
				to += Sprintf(to, LUF, dip->di_dbytes_read);
			    } else {
				to += Sprintf(to, LUF, dip->di_dbytes_written);
			    }
			    break;
			}
			case 'B': {
			    to += Sprintf(to, LUF,
					  (dip->di_total_bytes + dip->di_dbytes_read + dip->di_dbytes_written));
			    break;
			}
			case 'c': {
			    to += Sprintf(to, "%lu",
					  (dip->di_pass_total_records + dip->di_pass_total_partial));
			    break;
			}
			case 'C': {
			    /* Note: The record read/write counts are included for running tests. */
			    to += Sprintf(to, LUF,
					  (dip->di_total_records + dip->di_total_partial) +
					  (dip->di_records_read + dip->di_records_written));
			    break;
			}
			case 'd': {
			    to += Sprintf(to, "%s", dip->di_dname);
			    break;
			}
			case 'D': {
			    if ( dip->di_device ) { /* Only if known. */
				to += Sprintf(to, "%s", dip->di_device);
			    } else {
				struct dtype *dtp = dip->di_dtype;
				to += Sprintf(to, "%s", dtp->dt_type);
			    }
			    break;
			}
			case 'e': {
			    to += Sprintf(to, "%lu", dip->di_error_count);
			    // TODO: Need per pass vs. total error formats!
			    // compare errors not included, so inaccurate!
			    // (dip->di_read_errors + dip->di_write_errors));
			    break;
			}
			case 'E': {
			    to += Sprintf(to, "%lu", dip->di_error_limit);
			    break;
			}
			case 'f': {
			    if (dip->di_raw_flag || (dip->di_io_mode == COPY_MODE)) {
				to += Sprintf(to, "%lu",
					      (dip->di_files_read + dip->di_files_written));
			    } else if (dip->di_mode == READ_MODE) {
				to += Sprintf(to, "%lu", dip->di_files_read);
			    } else {
				to += Sprintf(to, "%lu", dip->di_files_written);
			    }
			    break;
			}
			case 'F': {
			    to += Sprintf(to, LUF,
					  (dip->di_total_files + dip->di_files_read + dip->di_files_written));
			    break;
			}
			case 'H':
			    full_info = True;
			    /* FALL THROUGH */
                        case 'h': {
                            char *p, *hostname;
                            hostname = os_gethostname();
                            if (!hostname) break;
                            if ( !full_info ) {
                                if ( (p = strchr(hostname, '.')) ) {
                                    *p = '\0';
                                }
                            }
                            to += Sprintf(to, "%s", hostname);
                            free(hostname);
                            break;
                        }
			case 'i': {
                            if (dip->di_raw_flag) {
                                to += Sprintf(to, "raw");
                            } else if (dip->di_mode == READ_MODE) {
                                to += Sprintf(to, "read");
                            } else {
                                to += Sprintf(to, "write");
                            }
                            break;
                        }
			case 'k': {
                            if (dip->di_raw_flag || (dip->di_io_mode == COPY_MODE)) {
                                to += Sprintf(to, "%.3f",
                                              (((double)dip->di_dbytes_read + (double)dip->di_dbytes_written)
                                               / (double)KBYTE_SIZE));
                            } else if (dip->di_mode == READ_MODE) {
                                to += Sprintf(to, "%.3f",
                                              ((double)dip->di_dbytes_read / (double)KBYTE_SIZE));
                            } else {
                                to += Sprintf(to, "%.3f",
                                              ((double)dip->di_dbytes_written / (double)KBYTE_SIZE));
                            }
                            break;
                        }
			case 'K': {
                            to += Sprintf(to, "%.3f",
                                          (((double)dip->di_total_bytes +
                                            (double)dip->di_dbytes_read + (double)dip->di_dbytes_written)
                                           / (double)MBYTE_SIZE));
                            break;
                        }
			case 'l': {
			    if (dip->di_dsize <= 1) { /* Device without a size, tape, etc. */
				to += Sprintf(to, "<n/a>");
			    } else if (dip->di_raw_flag || (dip->di_io_mode == COPY_MODE)) {
				to += Sprintf(to, LUF,
					      ((dip->di_dbytes_read + dip->di_dbytes_written) / dip->di_dsize));
			    } else if (dip->di_mode == READ_MODE) {
				to += Sprintf(to, LUF, (dip->di_dbytes_read / dip->di_dsize));
			    } else {
				to += Sprintf(to, LUF, (dip->di_dbytes_written / dip->di_dsize));
			    }
			    break;
			}
			case 'L': {
			    if (dip->di_dsize <= 1) { /* Device without a size, tape, etc. */
				to += Sprintf(to, "<n/a>");
			    } else {
				to += Sprintf(to, LUF,
					      ((dip->di_total_bytes + dip->di_dbytes_read + dip->di_dbytes_written) / dip->di_dsize));
			    }
			    break;
			}
			case 'm': {
			    if (dip->di_raw_flag || (dip->di_io_mode == COPY_MODE)) {
				to += Sprintf(to, "%.3f",
					      (((double)dip->di_dbytes_read + (double)dip->di_dbytes_written)
					       / (double)MBYTE_SIZE));
			    } else if (dip->di_mode == READ_MODE) {
				to += Sprintf(to, "%.3f",
					      ((double)dip->di_dbytes_read / (double)MBYTE_SIZE));
			    } else {
				to += Sprintf(to, "%.3f",
					      ((double)dip->di_dbytes_written / (double)MBYTE_SIZE));
			    }
			    break;
			}
			case 'M': {
			    to += Sprintf(to, "%.3f",
					  (((double)dip->di_total_bytes +
					    (double)dip->di_dbytes_read + (double)dip->di_dbytes_written)
					   / (double)MBYTE_SIZE));
			    break;
			}
			case 'p': {
			    to += Sprintf(to, "%lu", dip->di_pass_count);
			    break;
			}
			case 'P': {
			    to += Sprintf(to, "%lu", dip->di_pass_limit);
			    break;
			}
			case 'r': {
			    to += Sprintf(to, "%lu", dip->di_records_read);
			    break;
			}
			case 'R': {
			    to += Sprintf(to, LUF,
					  (dip->di_total_records_read + dip->di_total_partial_reads + dip->di_records_read));
			    break;
			}
			case 's': {
			    int secs;
			    dip->di_end_time = times (&dip->di_etimes);
			    secs = ((dip->di_end_time - dip->di_pass_time) / hertz);
			    to += Sprintf(to, "%d", secs);
			    break;
			}
			case 'S': {
			    int secs;
			    dip->di_end_time = times (&dip->di_etimes);
			    secs = ((dip->di_end_time - dip->di_start_time) / hertz);
			    to += Sprintf(to, "%d", secs);
			    break;
			}
			case 't': {
			    clock_t at = 0;
			    if (dip->di_pass_time) {
				dip->di_end_time = times (&dip->di_etimes);
				at = dip->di_end_time - dip->di_pass_time;
			    }
			    to = bformat_time(to, at);
			    break;
			}
			case 'T': {
			    clock_t at = 0;
			    if (dip->di_start_time) {
				dip->di_end_time = times (&dip->di_etimes);
				at = dip->di_end_time - dip->di_start_time;
			    }
			    to = bformat_time(to, at);
			    break;
			}
			case 'u': {
			    char *username = os_getusername();
			    if (username) {
				to += Sprintf(to, "%s", username);
				free(username);
			    }
			    break;
			}
			case 'w': {
			    to += Sprintf(to, "%lu", dip->di_records_written);
			    break;
			}
			case 'W': {
			    to += Sprintf(to, LUF,
					  (dip->di_total_records_written + dip->di_total_partial_writes + dip->di_records_written));
			    break;
			}
			default: {
			    *to++ = *from;
			    *to++ = *(from + 1);
			    break;
			}
		    } /* end switch */
		    length--;
		    from += 2;
		    break;
		} else { /* !length */
		    *to++ = *from++;
		} /* end if length */
		break;
	    } /* end case '%' */
	    case '\\': {
		if (length) {
		    switch (*(from + 1)) {
			case 'n': {
			    to += Sprintf(to, "\n");
			    break;
			}
			case 't': {
			    to += Sprintf(to, "\t");
			    break;
			}
			default: {
			    *to++ = *from;
			    *to++ = *(from + 1);
			    break;
			}
		    } /* end switch */
		    length--;
		    from += 2;
		    break;
		} else { /* !length */
		    *to++ = *from++;
		} /* end if length */
		break;
	    } /* end case '\' */
	    default: {
		*to++ = *from++;
		break;
	    }
	}
    }
    *to = '\0';
    return( strlen(buffer) );
}

/*
 * GetStatsValue() - Simple function to obtain stats values.
 *
 * Inputs:
 *    dip = The device information pointer.
 *    stv = The stats value to obtain.
 *    pass_stats = Boolean true if pass stats.
 *    secs = Optional pointer to store seconds.
 *
 * Return Value:
 *    Returns the stats value (64 bits).
 */
large_t
GetStatsValue(struct dinfo *dip, stats_value_t stv, hbool_t pass_stats, int *secs)
{
   large_t value;

   switch (stv) {
     case ST_BYTES: {
       if (pass_stats) {
          if (dip->di_raw_flag || (dip->di_io_mode == COPY_MODE)) {
            value = (dip->di_dbytes_read + dip->di_dbytes_written);
          } else if (dip->di_mode == READ_MODE) {
            value = dip->di_dbytes_read;
          } else {
            value = dip->di_dbytes_written;
          }
        } else {
          value = (dip->di_total_bytes + dip->di_dbytes_read + dip->di_dbytes_written);
        }
        break;
     }
     case ST_BLOCKS: {
       value = GetStatsValue(dip, ST_BYTES, pass_stats, secs);
       if (dip->di_dsize) {
         value /= dip->di_dsize; /* Convert to logical blocks. */
       }
       break;
     }
     case ST_FILES: {
       if (pass_stats) {
         if (dip->di_raw_flag || (dip->di_io_mode == COPY_MODE)) {
           value = (dip->di_files_read + dip->di_files_written);
         } else if (dip->di_mode == READ_MODE) {
           value = dip->di_files_read;
         } else {
           value = dip->di_files_written;
         }
       } else {
         value = (dip->di_total_files + dip->di_files_read + dip->di_files_written);
       }
       break;
     }
     case ST_RECORDS: {
       if (pass_stats) {
         if (dip->di_raw_flag || (dip->di_io_mode == COPY_MODE)) {
           value = (dip->di_records_read + dip->di_records_written);
         } else if (dip->di_mode == READ_MODE) {
           value = dip->di_records_read;
         } else {
           value = dip->di_records_written;
         }
       } else {
         value = (dip->di_total_records + dip->di_total_partial +
                  dip->di_records_read + dip->di_records_written);
       }
       break;
     }
     case ST_OFFSET: {
#if defined(AIO)
	if (dip->di_aio_flag && dip->di_current_acb) {
	    value = dip->di_current_acb->aio_offset;
	} else {
	    value = dip->di_offset;
	}
#else /* !defined(AIO) */
	value = dip->di_offset;
#endif /* defined(AIO) */
	break;
     }
     default:
       Fprintf(dip, "Invalid stats value request, %d\n", stv);
       value = 0;
       //abort();
   }
   if (secs) {
     dip->di_end_time = times(&dip->di_etimes);
     if (pass_stats) {
       *secs = ((dip->di_end_time - dip->di_pass_time) / hertz);
     } else {
       *secs = ((dip->di_end_time - dip->di_start_time) / hertz); 
     }
   }
   return (value);
}

/*
 * FmtPrefix() - Format the Prefix String.
 *
 * Special Format Characters:
 *
 *	%d = The device name.
 *	%D = The real device name.
 *	%h = The host name.
 * 	%H = The full host name.
 * 	%j = The job ID.
 * 	%J = The job tag.
 *	%p = The process ID.
 *	%P = The parent process ID.
 * 	%s = The device serial number.
 * 	%t = The thread number.
 * 	%T = The thread ID.
 * 	%u = The user (login) name.
 * 	%U = A UUID (if OS supports).
 * 
 *	Note: Mainly added %j, %J, %t, and %T for debugging.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	prefix = The prefix string.
 *	psize = The prefix string size.
 *
 * Outputs:
 *	dip->di_fprefix_string = The formatted prefix string.
 *	dip->di_fprefix_size = The formatted prefix string size.
 *
 * Return Value:
 *	Returns SUCCESS or FAILURE.
 */
int
FmtPrefix(struct dinfo *dip, char *prefix, int psize)
{
    char	*from = prefix;
    char	*buffer, *to;
    int		length = psize;

    if (dip->di_fprefix_string) {
	Free(dip, dip->di_fprefix_string);
	dip->di_fprefix_string = NULL;
    }
    buffer = to = Malloc(dip, PATH_BUFFER_SIZE);
    while (length--) {
	hbool_t full_info = False;
	switch (*from) {
	    case '%':
		if (length) {
		    switch (*(from + 1)) {
			case 'd': {
			    to += Sprintf(to, "%s", dip->di_dname);
			    break;
			}
			case 'D': {
			    if ( dip->di_device ) { /* Only if known. */
				to += Sprintf(to, "%s", dip->di_device);
			    } else {
				struct dtype *dtp = dip->di_dtype;
				to += Sprintf(to, "%s", dtp->dt_type);
			    }
			    break;
			}
			case 'H':
			    full_info = True;
			    /* FALL THROUGH */
			case 'h': {
			    char *p, *hostname;
			    hostname = os_gethostname();
			    if (!hostname) break;
			    if ( !full_info ) {
				if ( (p = strchr(hostname, '.')) ) {
				    *p = '\0';
				}
			    }
			    to += Sprintf(to, "%s", hostname);
			    free(hostname);
			    break;
			}
			case 'j': {
			    if (dip->di_job) {
				to += Sprintf(to, "%u", dip->di_job->ji_job_id);
			    }
			    break;
			}
			case 'J': {
			    if (dip->di_job && dip->di_job->ji_job_tag) {
				to += Sprintf(to, "%s", dip->di_job->ji_job_tag);
			    }
			    break;
			}
			case 'p': {
			    pid_t pid = os_getpid();
			    to += Sprintf(to, "%d", pid);
			    break;
			}
			case 'P': {
			    pid_t ppid = os_getppid();
			    to += Sprintf(to, "%d", ppid);
			    break;
			}
#if defined(SCSI)
			/* Note: I'd prefer %serial, but not yet! */
			case 's': {
			    if (dip->di_serial_number) {
				to += Sprintf(to, "%s", dip->di_serial_number);
			    }
			    break;
			}
#endif /* defined(SCSI) */
			case 't': {
			    to += Sprintf(to, "%u", dip->di_thread_number);
			    break;
			}
			case 'T': {
			    to += Sprintf(to, OS_TID_FMT, pthread_self());
			    break;
			}
			case 'u': {
			    char *username = os_getusername();
			    if (username) {
				to += Sprintf(to, "%s", username);
				free(username);
			    }
			    break;
			}
			case 'U': {
			    if (dip->di_uuid_string) {
				to += Sprintf(to, "%s", dip->di_uuid_string);
			    }
			    break;
			}
			default: {
			    *to++ = *from;
			    *to++ = *(from + 1);
			    break;
			}
		    }
		    length--;
		    from += 2;
		    break;
		}
		/* FALLTHROUGH */
	    default: {
		*to++ = *from++;
		break;
	    }
	}
    }
    dip->di_fprefix_size = (int)strlen(buffer) + 1;	/* Include NULL! */
    /*
     * To avoid problems with random I/O, make the prefix string
     * modulo the lba (iot or lbdata) or our 4 byte data pattern.
     * Otherwise, random I/O fails with a partial pattern.
     *
     * NOTE: *Always* roundup, since one pass may use sequential
     * while the next (read-only) pass may use random, we need the
     * prefix/data to match up in both cases!
     */
    dip->di_fprefix_size = roundup(dip->di_fprefix_size, sizeof(u_int32));
    /* Note: We depend on Malloc() zeroing the memory! */
    dip->di_fprefix_string = Malloc(dip, dip->di_fprefix_size);
    (void)strcpy(dip->di_fprefix_string, buffer);
    Free(dip, buffer);
#if 0
    if (dip->di_debug_flag) {
	Printf(dip, "Formatted prefix string @ "LLPXFMT" is: %s\n",
	       dip->di_fprefix_string, dip->di_fprefix_string);
    }
#endif /* 0 */
    return (SUCCESS);
}

/* This macro is specific to the next funtion only! */
#define GetDateTime()				\
    if (now == (time_t) 0) {			\
	(void)time(&now);			\
	if ( tmp && localtime_r(&now, tmp) ) {	\
	    tmp->tm_year += 1900;		\
	    tmp->tm_mon++;			\
	} else {				\
	    tmp = NULL;				\
	}					\
    }
 
/*
 * FmtString() - Format a String Based on Control Strings.
 *
 * Description:
 *	This function is used for formatting control strings for file paths
 * as well as the log prefix. More control strings are available using keepalive,
 * but these are deemed sufficient for file paths and the log prefix (so far).
 *
 * Special Format Control Strings:
 *
 *	%bufmode = The file buffer mode.
 *      %date = The date string.
 *	%et = The elapsed time.
 *	%tod = The time of day.
 *	%etod = Elapsed time of day.
 * 	%device = The device path. (full path)
 * 	%devnum = The device number.
 * 	%dsf = The device name only. (base name)
 * 	%dfs = The directory file separator. (Unix: '/' or Windows: '\')
 * 	%file = The file path. (base name)
 *	%iodir = The I/O direction.
 *	%iotype = The I/O type.
 *      %iotune - The default I/O tune file path.
 *	%host = The host name.
 * 	%nate = The NATE timestamp.
 *      %ymd = The year, month, day.
 *      %year = The year.
 *      %month = The month.
 *      %day = The day.
 *      %hms = The hour, minute, second.
 *      %hour = The hour.
 *      %minute = The minute.
 *      %second = The second.
 *      %nos = The Nimble OS date/time.
 *	%secs = Seconds since start.
 *	%seq = The sequence number.
 * 	%prog = Our program name.
 * 	%pid = The process ID.
 * 	%ppid = The parent process ID.
 * 	%script = The script file name.
 * 	%tmpdir = The temporary directory.
 *	%user = The user (login) name.
 * 	%uuid = The UUID string.
 * 
 * See FmtCommon() for common format control strings.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	format = Pointer to format strings.
 *	filepath_flag = Formatting a file path.
 *
 * Return Value:
 * 	Returns a pointer to the allocated formatted string.
 */
char *
FmtString(dinfo_t *dip, char *format, hbool_t filepath_flag)
{
    char	buffer[LOG_BUFSIZE];
    char	*from = format;
    char	*to = buffer;
    int		length = (int)strlen(format);
    int		ifs = dip->di_dir_sep;
    struct tm	time_data;
    struct tm	*tmp = &time_data;
    time_t	now = 0;

    *to = '\0';
    while (length--) {
        if (*from == '%') {
            char *key = (from + 1);
	    if (strncasecmp(key, "bufmode", 7) == 0) {
		if (dip->di_bufmode_type) {
		    to += sprintf(to, "%s", dip->di_bufmode_type);
		}
		length -= 7;
		from += 8;
		continue;
	    } else if (strncasecmp(key, "date", 4) == 0) {
		char time_buffer[TIME_BUFFER_SIZE];
		time_t current_time = time((time_t *) 0);
		memset(time_buffer, '\0', sizeof(time_buffer));
		os_ctime(&current_time, time_buffer, sizeof(time_buffer));
		to += Sprintf(to, "%s", time_buffer);
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "device", 6) == 0) {
		char *device = dip->di_dname;
		char *dptr, *pptr, *sptr;
		if (device) {	/* master does not have a device! */
		    if (filepath_flag) {
			dptr = strdup(device);
			pptr = sptr = skip_device_prefix(dptr);
			/* Replace path delimiter(s) with undersores. */
			while (sptr = strchr(sptr, ifs)) {
			    *sptr++ = '_';
			}
			sptr = pptr;
			/* Replace periods with an underscore too! */
			while (sptr = strchr(sptr, '.')) {
			    *sptr++ = '_';
			}
			to += Sprintf(to, "%s", pptr);
			Free(dip, dptr);
		    } else {
			to += Sprintf(to, "%s", device);
		    }
		}
                length -= 6;
                from += 7;
                continue;
	    } else if (strncasecmp(key, "devnum", 6) == 0) {
		to += Sprintf(to, "%d", dip->di_device_number);
		length -= 6;
		from += 7;
		continue;
	    } else if ( (strncasecmp(key, "dsf", 3) == 0) ||
			(strncasecmp(key, "file", 4) == 0) ) {
		char *device = dip->di_dname;
		char *dptr, *pptr, *sptr;
		if (device) {
		    if (filepath_flag) {
			dptr = strdup(device);
			pptr = sptr = skip_device_prefix(dptr);
			/* Add basename of the device path. */
			if (sptr = strrchr(pptr, ifs)) {
			    sptr++;
			} else {
			    sptr = pptr;
			}
			to += Sprintf(to, "%s", sptr);
			Free(dip, dptr);
		    } else {
			to += Sprintf(to, "%s", device);
		    }
		}
		if (strncasecmp(key, "dsf", 3) == 0) {
		    length -= 3;
		    from += 4;
		} else {
		    length -= 4;
		    from += 5;
		}
		continue;
	    } else if (strncasecmp(key, "dfs", 3) == 0) {
		to += Sprintf(to, "%c", dip->di_dir_sep);
		length -= 3;
		from += 4;
		continue;
            } else if (strncasecmp(key, "lba", 3) == 0) {
		to += Sprintf(to, LUF, (slarge_t)dip->di_start_lba);
                length -= 3;
                from += 4;
                continue;
            } else if (strncasecmp(key, "offset", 6) == 0) {
		to += Sprintf(to, FUF, dip->di_offset);
                length -= 6;
                from += 7;
                continue;
            } else if (strncasecmp(key, "elba", 4) == 0) {
		to += Sprintf(to, LUF, dip->di_error_lba);
                length -= 4;
                from += 5;
                continue;
            } else if (strncasecmp(key, "eoffset", 7) == 0) {
		to += Sprintf(to, FUF, dip->di_error_offset);
                length -= 7;
                from += 8;
                continue;
	    } else if (strncasecmp(key, "iodir", 5) == 0) {
                if (dip->di_io_dir == FORWARD) {
                    to += Sprintf(to, "%s", "forward");
                } else {
                    to += Sprintf(to, "%s", "reverse");
                }
                length -= 5;
                from += 6;
                continue;
            } else if (strncasecmp(key, "iotype", 6) == 0) {
                if (dip->di_io_type == SEQUENTIAL_IO) {
                    to += Sprintf(to, "%s", "sequential");
                } else {
                    to += Sprintf(to, "%s", "random");
                }
                length -= 6;
                from += 7;
                continue;
	    } else if (strncasecmp(key, "iotune", 6) == 0) {
		to += Sprintf(to, "%s", DEFAULT_IOTUNE_FILE);
		length -= 6;
		from += 7;
		continue;
            } else if (strncasecmp(key, "host", 4) == 0) {
                char *p, *hostname = os_gethostname();
		if (hostname) {
		    if (p = strchr(hostname, '.')) {
			*p = '\0';
		    }
		    to += Sprintf(to, "%s", hostname);
		    free(hostname);
		}
                length -= 4;
                from += 5;
                continue;
	    } else if (strncasecmp(key, "nate", 4) == 0) {
		GetDateTime()
		if (tmp) {
		    tmp->tm_year += 1900;
		    tmp->tm_mon++;
		    /* NATE Format: yyyymmdd hhmmss */
		    to += sprintf(to, "%04d%02d%02d %02d%02d%02d",
				  tmp->tm_year, tmp->tm_mon, tmp->tm_mday,
				  tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "nos", 3) == 0) {
		GetDateTime()
		if (tmp) {
		    /* Format: yyyy-mm-dd,hh:mm:ss */
        	    /* Example: 2020-05-11,14:55:01 */
		    to += sprintf(to, "%04d-%02d-%02d,%02d:%02d:%02d",
				  tmp->tm_year, tmp->tm_mon, tmp->tm_mday,
				  tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		}
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "ymd", 3) == 0) {
		GetDateTime()
		if (tmp) {
		    char *fs;
		    /* Format: yyyymmdd */
		    if (fs = dip->di_date_sep) {
			to += sprintf(to, "%04d%s%02d%s%02d",
				      tmp->tm_year, fs, tmp->tm_mon, fs, tmp->tm_mday);
		    } else {
			to += sprintf(to, "%04d%02d%02d",
				      tmp->tm_year, tmp->tm_mon, tmp->tm_mday);
		    }
		}
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "year", 4) == 0) {
		GetDateTime()
		if (tmp) {
		    to += sprintf(to, "%02d", tmp->tm_year);
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "month", 5) == 0) {
		GetDateTime()
		if (tmp) {
		    to += sprintf(to, "%02d", tmp->tm_mon);
		}
		length -= 5;
		from += 6;
		continue;
	    } else if (strncasecmp(key, "day", 3) == 0) {
		GetDateTime()
		if (tmp) {
		    to += sprintf(to, "%02d", tmp->tm_mday);
		}
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "hms", 3) == 0) {
		GetDateTime()
		if (tmp) {
        	    char *fs;
		    /* Format: hhmmss */
		    if (fs = dip->di_time_sep) {
			to += sprintf(to, "%02d%s%02d%s%02d",
				      tmp->tm_hour, fs, tmp->tm_min, fs, tmp->tm_sec);
		    } else {
			to += sprintf(to, "%02d%02d%02d",
				      tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		    }
		}
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "hour", 4) == 0) {
		GetDateTime()
		if (tmp) {
		    to += sprintf(to, "%02d", tmp->tm_hour);
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "minute", 6) == 0) {
		GetDateTime()
		if (tmp) {
		    to += sprintf(to, "%02d", tmp->tm_min);
		}
		length -= 6;
		from += 7;
		continue;
	    } else if (strncasecmp(key, "second", 6) == 0) {
		GetDateTime()
		if (tmp) {
		    to += sprintf(to, "%02d", tmp->tm_sec);
		}
		length -= 6;
		from += 7;
		continue;
	    } else if (strncasecmp(key, "level", 5) == 0) {
		to += Sprintf(to, "%d", dip->di_log_level);
		length -= 5;
		from += 6;
		continue;
	    } else if (strncasecmp(key, "secs", 4) == 0) {
		/* We may wish to change this to use ctime() */
                struct tms tms;
		clock_t end_time = times(&tms);
		clock_t et = (end_time - dip->di_start_time);
		int secs = (et / hertz);
		to += Sprintf(to, "%08d", secs);
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "seq", 3) == 0) {
		to += Sprintf(to, "%8d", dip->di_sequence);
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key, "tod", 3) == 0) {
		*&dip->di_ptod = *&dip->di_gtod;
		(void)highresolutiontime(&dip->di_gtod, NULL);
		to += Sprintf(to, "%d.%06d", dip->di_gtod.tv_sec, dip->di_gtod.tv_usec);
		if (dip->di_ptod.tv_sec == 0) {
		    *&dip->di_ptod = *&dip->di_gtod;
		}
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key, "etod", 4) == 0) {
		int secs, usecs;
		secs = dip->di_gtod.tv_sec;
		usecs = dip->di_gtod.tv_usec;
		if (usecs < dip->di_ptod.tv_usec) {
		    secs--;
		    usecs += uSECS_PER_SEC;
		}
		to += Sprintf(to, "%d.%06d", (secs - dip->di_ptod.tv_sec), (usecs - dip->di_ptod.tv_usec));
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "et", 2) == 0) {
		struct tms tms;
		clock_t end_time = times(&tms);
		clock_t et = (end_time - dip->di_start_time);
		if (!dip->di_start_time) et = 0;
		to += FormatElapstedTime(to, et);
		from += 3;
		length -= 2;
		continue;
	    } else if (strncasecmp(key, "prog", 4) == 0) {
                to += Sprintf(to, "%s", cmdname);
                length -= 4;
                from += 5;
                continue;
            } else if (strncasecmp(key, "pid", 3) == 0) {
                pid_t pid = os_getpid();
                to += Sprintf(to, "%d", pid);
                length -= 3;
                from += 4;
                continue;
            } else if (strncasecmp(key, "ppid", 4) == 0) {
                pid_t ppid = os_getppid();
                to += Sprintf(to, "%d", ppid);
                length -= 4;
                from += 5;
                continue;
	    } else if (strncasecmp(key, "script", 6) == 0) {
		int level = dip->script_level;
		if (level--) {
		    char *script_name = dip->script_name[level];
		    char *p = strrchr(script_name, dip->di_dir_sep);
		    if (p) script_name = ++p;
		    to += Sprintf(to, "%s", script_name);
		}
		length -= 6;
		from += 7;
		continue;
	    } else if (strncasecmp(key, "tmpdir", 6) == 0) {
		to += Sprintf(to, "%s%c", TEMP_DIR_NAME, dip->di_dir_sep);
		length -= 6;
		from += 7;
		continue;
            } else if (strncasecmp(key, "user", 4) == 0) {
		char *username = os_getusername();
                if (username) {
                    to += Sprintf(to, "%s", username);
		    free(username);
                }
                length -= 4;
                from += 5;
                continue;
            } else if (strncasecmp(key, "uuid", 4) == 0) {
                if (dip->di_uuid_string) {
                    to += Sprintf(to, "%s", dip->di_uuid_string);
                }
                length -= 4;
                from += 5;
                continue;
            } else {
		int len = FmtCommon(dip, key, &to);
		if (len) {
		    length -= len;
		    from += (len + 1);
		    continue;
		}
	    }
        }
        *to++ = *from++;
    }
    *to = '\0';
    /* Note: May return NULL if no memory is available! */
    return( strdup(buffer) );
}

/*
 * FmtCommon - Format Common Strings. 
 *  
 * Description: 
 *      This function is invoked from other formatting functions, since what's
 * defined here is common to log prefix, keepalive messages, and prefix strings.
 *  
 * Note: The "%" is expected to have been stripped by the caller! 
 * 
 * Common Format Controls: 
 *      %array = The array name or IP.
 *      %sdsf = The SCSI device special file.
 *      %tdsf = The Trigger device special file.
 *  
 * Job Control Keywords:
 *      %job = The job ID.
 *      %jlog = The job log.
 *      %tlog = The thread log.
 *	%tag = The job tag.
 * 	%tid = The thread ID.
 * 	%thread = The thread number.
 * 	%workload = The workload name.
 * 
 * SCSI Format Control Strings: 
 *  	%capacity = The disk capacity.
 *  	%blocklen = The disk block length.
 * 	%vendor = The Inquiry vendor ID.
 * 	%product = The Inquiry product ID.
 * 	%revision = The Inquiry revision level.
 *	%devid = The device identifier. (Inquiry page 0x83)
 *  	%serial = The disk serial number. (Inquiry page 0x80)
 *      %mgmtaddr = The management network address. (Inquiry page 0x85)
 */
int
FmtCommon(dinfo_t *dip, char *key, char **buffer)
{
    int length = 0;
    char *to = *buffer;

    if (strncasecmp(key, "array", 5) == 0) {
        /* This is a user specified option. */
	if (dip->di_array) {
	    to += Sprintf(to, "%s", dip->di_array);
        } else {
	    to += Sprintf(to, "%s", not_available);
	}
	length = 5;
    } else if (strncasecmp(key, "job", 3) == 0) {
	if (dip->di_job) {
	    to += Sprintf(to, "%u", dip->di_job->ji_job_id);
	}
	length = 3;
    } else if (strncasecmp(key, "jlog", 4) == 0) {
	if (dip->di_job_log) {
	    to += Sprintf(to, "%s", dip->di_job_log);
        } else {
	    to += Sprintf(to, "%s", not_available);
	}
	length = 4;
    } else if (strncasecmp(key, "tlog", 4) == 0) {
	if (dip->di_log_file) {
	    to += Sprintf(to, "%s", dip->di_log_file);
        } else {
	    to += Sprintf(to, "%s", not_available);
	}
	length = 4;
    } else if (strncasecmp(key, "tag", 3) == 0) {
	if (dip->di_job && dip->di_job->ji_job_tag) {
	    to += Sprintf(to, "%s", dip->di_job->ji_job_tag);
	}
	length = 3;
    } else if (strncasecmp(key, "tid", 3) == 0) {
	to += Sprintf(to, OS_TID_FMT, pthread_self());
	length = 3;
    } else if (strncasecmp(key, "thread", 6) == 0) {
	to += Sprintf(to, "%u", dip->di_thread_number);
	length = 6;
    } else if (strncasecmp(key, "workload", 8) == 0) {
	if (dip->di_workload_name) {
	    to += Sprintf(to, "%s", dip->di_workload_name);
	}
	length = 8;
#if defined(SCSI)
    } else if (strncasecmp(key, "sdsf", 4) == 0) {
	if (dip->di_scsi_dsf) {
	    to += Sprintf(to, "%s", dip->di_scsi_dsf);
	}
	length = 4;
    } else if (strncasecmp(key, "tdsf", 4) == 0) {
	if (dip->di_tscsi_dsf) {
	    to += Sprintf(to, "%s", dip->di_tscsi_dsf);
	}
	length = 4;
    } else if (strncasecmp(key, "capacity", 8) == 0) {
	if (dip->di_device_capacity) {
	    to += Sprintf(to, LUF, dip->di_device_capacity);
	}
	length = 8;
    } else if (strncasecmp(key, "blocklen", 8) == 0) {
	if (dip->di_block_length) {
	    to += Sprintf(to, LUF, dip->di_block_length);
	}
	length = 8;
    } else if (strncasecmp(key, "vendor", 6) == 0) {
	if (dip->di_vendor_id) {
	    to += Sprintf(to, "%s", dip->di_vendor_id);
	}
	length = 6;
    } else if (strncasecmp(key, "vendor", 6) == 0) {
	if (dip->di_vendor_id) {
	    to += Sprintf(to, "%s", dip->di_vendor_id);
	}
	length = 6;
    } else if (strncasecmp(key, "product", 7) == 0) {
	if (dip->di_product_id) {
	    to += Sprintf(to, "%s", dip->di_product_id);
	}
	length = 7;
    } else if (strncasecmp(key, "revision", 8) == 0) {
	if (dip->di_revision_level) {
	    to += Sprintf(to, "%s", dip->di_revision_level);
	}
	length = 8;
    } else if (strncasecmp(key, "devid", 5) == 0) {
	if (dip->di_device_id) {
	    to += Sprintf(to, "%s", dip->di_device_id);
	}
	length = 5;
    } else if (strncasecmp(key, "serial", 6) == 0) {
	if (dip->di_serial_number) {
	    to += Sprintf(to, "%s", dip->di_serial_number);
	}
	length = 6;
    } else if (strncasecmp(key, "mgmtaddr", 8) == 0) {
	if (dip->di_mgmt_address) {
	    to += Sprintf(to, "%s", dip->di_mgmt_address);
	}
	length = 8;
#endif /* defined(SCSI) */
    }
    if (length) {
	*buffer = to;
    }
    return(length);
}
