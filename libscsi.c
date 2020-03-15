/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2019			    *
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
 * Module:	libscsi.c
 * Author:	Robin T. Miller
 * Date:	March 24th, 2005
 *
 * Description:
 *	This module contains common SCSI functions, which are meant to be
 * a front-end to calling the underlying OS dependent SCSI functions,
 * when appropriate, to send a SCSI CDB (Command Descriptor Block).
 * 
 * Modification History:
 * 
 * July 18th, 2019 by Robin T. Miller
 *      Do not retry "target port in standby state" sense code/qualifier,
 * otherwise we loop forever, since we do not have retry limit on this error.
 * 
 * January 23rd, 2013 by Robin T. Miller
 * 	Update libExecuteCdb() to allow a user defined execute CDB function.
 * 	Added command retries, including OS specific error recovery.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>

#include "dt.h"
//#include "libscsi.h"
#include "scsi_cdbs.h"
#include "scsi_opcodes.h"

/*
 * Local Declarations:
 */

/*
 * Forward Declarations:
 */ 
hbool_t	isSenseRetryable(scsi_generic_t *sgp, int scsi_status, scsi_sense_t *ssp);
void ReportCdbScsiInformation(scsi_generic_t *sgp);

int verify_inquiry_header(inquiry_t *inquiry, inquiry_header_t *inqh, unsigned char page);

/* ======================================================================== */

scsi_generic_t *
init_scsi_generic(void)
{
    scsi_generic_t *sgp;
    sgp = Malloc(NULL, sizeof(*sgp));
    if (sgp == NULL) return (NULL);
    init_scsi_defaults(sgp);
    return (sgp);
}

void
init_scsi_defaults(scsi_generic_t *sgp)
{
    scsi_addr_t *sap;
    /*
     * Initial Defaults: 
     */
    sgp->fd             = INVALID_HANDLE_VALUE;
    sgp->sense_length   = RequestSenseDataLength;
    sgp->sense_data     = malloc_palign(NULL, sgp->sense_length, 0);

    sgp->debug          = ScsiDebugFlagDefault;
    sgp->errlog         = ScsiErrorFlagDefault;
    sgp->timeout        = ScsiDefaultTimeout;

    sgp->qtag_type      = SG_SIMPLE_Q;

    /*
     * Recovery Parameters:
     */ 
    sgp->recovery_flag  = ScsiRecoveryFlagDefault;
    sgp->recovery_delay = ScsiRecoveryDelayDefault;
    sgp->recovery_limit = ScsiRecoveryRetriesDefault;

    sap = &sgp->scsi_addr;
    /* Note: Only AIX uses this, but must be -1 for any path! */
    sap->scsi_path      = -1;	/* Indicates no path specified. */

    return;
}

hbool_t
libIsRetriable(scsi_generic_t *sgp)
{
    hbool_t retriable = False;

    /* Note: This section is dt specified, but required to avoid looping! */
    if ( PROGRAM_TERMINATING || COMMAND_INTERRUPTED ) {
	return(retriable);
    }
    if ( sgp->opaque ) {
	dinfo_t *dip = sgp->opaque;
	if ( (dip->di_trigger_active == False) && THREAD_TERMINATING(dip) ) {
	    return(retriable);
	}
    }
    /* end of dt specific! */

    if (sgp->recovery_retries++ < sgp->recovery_limit) {
	/*
	 * Try OS specific first, then check for common retriables.
	 */ 
	retriable = os_is_retriable(sgp);
	if (retriable == False) {
	    scsi_sense_t *ssp = sgp->sense_data;
	    unsigned char sense_key, asc, asq;
	    /* Get sense key and sense code/qualifiers. */
	    GetSenseErrors(ssp, &sense_key, &asc, &asq);
	    if (sgp->debug == True) {
		print_scsi_status(sgp, sgp->scsi_status, sense_key, asc, asq);
	    }
	    if ( (sgp->scsi_status == SCSI_BUSY) ||
		 (sgp->scsi_status == SCSI_QUEUE_FULL) ) {
		retriable = True;
	    } else if (sgp->scsi_status == SCSI_CHECK_CONDITION) {
		if (sense_key == SKV_UNIT_ATTENTION) {
		    if (asc != ASC_RECOVERED_DATA) {
			retriable = True;
		    }
		} else if ( (sense_key == SKV_NOT_READY) &&
			    (asc == ASC_NOT_READY) ) {
		    /* Lots of reasons, but we retry them all! */
		    /* Includes:
		     * "Logical unit is in process of becoming ready"
		     * "Logical unit not ready, space allocation in progress"
		     * Note: We'll be more selective, if this becomes an issue!
		     */
		    /* Note: We do not retry this error, or we'll loop forever! */
		    /* (0x4, 0xb) - Logical unit not accessible, target port in standby state */
		    if (asq != ASQ_STANDBY_STATE) {
			retriable = True;
		    }
		}
	    }
	}
    }
    return (retriable);
}

/* ======================================================================== */

/*
 * libExecuteCdb() = Execute a SCSI Command Descriptor Block (CDB).
 *
 * Inputs:
 *  sgp = Pointer to SCSI generic pointer.
 *
 * Return Value:
 *      Returns 0/-1 for Success/Failure.
 */
int
libExecuteCdb(scsi_generic_t *sgp)
{
    int error;
    hbool_t retriable;

    /*
     * Allow user to define their own execute CDB function.
     */
    if (sgp->execute_cdb && sgp->opaque) {
	error = (*sgp->execute_cdb)(sgp->opaque, sgp);
	return (error);
    }

    sgp->recovery_retries = 0;
    do {
	retriable = False;
	/*
	 * Ensure the sense data is cleared for emiting status.
	 */
	memset(sgp->sense_data, '\0', sgp->sense_length);
	/* Clear these too, since IOCTL may fail so never get updated! */
	sgp->os_error = 0;		/* The system call error (if any). */
	sgp->scsi_status = sgp->driver_status = sgp->host_status = sgp->data_resid = 0;

	/*
	 * Call OS dependent SCSI Pass-Through (spt) function.
	 */
	error = os_spt(sgp);
	//if ( (sgp->error == True) && (sgp->sense_valid == True) ) {
	//    MapSenseDescriptorToFixed(sgp->sense_data);
	//}
	if (((error == FAILURE) || (sgp->error == True)) && sgp->recovery_flag) {
	    if (sgp->recovery_retries == sgp->recovery_limit) {
		Fprintf(sgp->opaque, "Exceeded retry limit (%u) for this request!\n", sgp->recovery_limit);
	    } else {
		retriable = libIsRetriable(sgp);
		if (retriable == True) {
		    (void)os_sleep(sgp->recovery_delay);
		    if (sgp->errlog == True) {
			/* Folks wish to see the actual error too! */
			if (error == FAILURE) {		/* The system call failed! */
			    libReportIoctlError(sgp, True);
			} else {
			    libReportScsiError(sgp, True);
			}
			Fprintf(sgp->opaque, "Warning: Retrying %s after %u second delay, retry #%u...\n",
			       sgp->cdb_name, sgp->recovery_delay, sgp->recovery_retries);
		    }
		}
	    }
	}
    } while (retriable == True);

    if (error == FAILURE) {		/* The system call failed! */
	if (sgp->errlog) {
	    libReportIoctlError(sgp, sgp->warn_on_error);
	}
    } else if ( sgp->error && (sgp->errlog || sgp->debug) ) { /* SCSI error. */
	libReportScsiError(sgp, sgp->warn_on_error);
    }

    if (sgp->error == True) {
        error = FAILURE;      /* Tell caller we've had an error! */
    }
    return (error);
}

/* Note: This can be replaced after spt is integrated! */
void
ReportCdbScsiInformation(scsi_generic_t *sgp)
{
    char efmt_buffer[LARGE_BUFFER_SIZE];
    char *to = efmt_buffer;
    int i, slen;

    to += sprintf(to, "SCSI CDB: ");
    for (i = 0; (i < sgp->cdb_size); i++) {
	slen = Sprintf(to, "%02x ",  sgp->cdb[i]);
	to += slen;
    }
    if (i) {
	slen--; to--; *to = '\0';
    }

    to += sprintf(to, ", dir=");
    if (sgp->data_dir == scsi_data_none) {
	slen = sprintf(to, "%s", "none");
    } else if (sgp->data_dir == scsi_data_read) {
	slen = sprintf(to, "%s", "read");
    } else if (sgp->data_dir == scsi_data_write) {
	slen = sprintf(to, "%s", "write");
    } else {
	slen = 0;
    }
    to += slen;

    to += sprintf(to, ", length=");
    to += sprintf(to, "%u",  sgp->data_length);

    Fprintf(sgp->opaque, "%s\n", efmt_buffer);
    return;
}

void
libReportIoctlError(scsi_generic_t *sgp, hbool_t warn_on_error)
{
    if (sgp->errlog == True) {
	time_t error_time = time((time_t *) 0);
	Fprintf(sgp->opaque, "%s: Error occurred on %s",
		(warn_on_error == True) ? "Warning" : "ERROR",
		ctime(&error_time));
	Fprintf(sgp->opaque, "%s failed on device %s\n", sgp->cdb_name, sgp->dsf);
	ReportCdbScsiInformation(sgp);
    }
    return;
}

void
libReportScsiError(scsi_generic_t *sgp, hbool_t warn_on_error)
{
    time_t error_time = time((time_t *) 0);
    char *host_msg = os_host_status_msg(sgp);
    char *driver_msg = os_driver_status_msg(sgp);
    scsi_sense_t *ssp = sgp->sense_data;
    unsigned char sense_key, asc, asq;
    char *ascq_msg;

    /* Get sense key and sense code/qualifiers. */
    GetSenseErrors(ssp, &sense_key, &asc, &asq);
    ascq_msg = ScsiAscqMsg(asc, asq);
    Fprintf(sgp->opaque, "%s: Error occurred on %s",
	    (warn_on_error == True) ? "Warning" : "ERROR",
	    ctime(&error_time));
    Fprintf(sgp->opaque, "%s failed on device %s\n", sgp->cdb_name, sgp->dsf);
    ReportCdbScsiInformation(sgp);
    Fprintf(sgp->opaque, "SCSI Status = %#x (%s)\n", sgp->scsi_status, ScsiStatus(sgp->scsi_status));
    if (host_msg && driver_msg) {
	Fprintf(sgp->opaque, "Host Status = %#x (%s), Driver Status = %#x (%s)\n",
	       sgp->host_status, host_msg, sgp->driver_status, driver_msg);
    } else if (host_msg || driver_msg) {
	if (host_msg) {
	    Fprintf(sgp->opaque, "Host Status = %#x (%s)\n", sgp->host_status, host_msg);
	}
	if (driver_msg) {
	    Fprintf(sgp->opaque, "Driver Status = %#x (%s)\n", sgp->driver_status, driver_msg);
	}
    } else if (sgp->host_status || sgp->driver_status) {
	Fprintf(sgp->opaque, "Host Status = %#x, Driver Status = %#x\n",
		sgp->host_status, sgp->driver_status);
    }
    Fprintf(sgp->opaque, "Sense Key = %d = %s, Sense Code/Qualifier = (%#x, %#x)",
	    sense_key, SenseKeyMsg(sense_key),
	    asc, asq);
    if (ascq_msg) {
      Fprint(sgp->opaque, " - %s", ascq_msg);
    }
    Fprintnl(sgp->opaque);
    if ( ssp->error_code && (sgp->debug || sgp->sense_flag) ) {
	DumpSenseData(sgp, ssp);
    }
    return;
}

void
libReportScsiSense(scsi_generic_t *sgp, int scsi_status, scsi_sense_t *ssp)
{
    char *ascq_msg = ScsiAscqMsg(ssp->asc, ssp->asq);

    Fprintf(sgp->opaque, "SCSI Status = %#x (%s)\n", scsi_status, ScsiStatus(scsi_status));
    Fprintf(sgp->opaque, "Sense Key = %d = %s, Sense Code/Qualifier = (%#x, %#x)",
	    ssp->sense_key, SenseKeyMsg(ssp->sense_key),
	    ssp->asc, ssp->asq);
    if (ascq_msg) {
      Fprint(sgp->opaque, " - %s", ascq_msg);
    }
    Fprintnl(sgp->opaque);
    return;
}

/* ======================================================================== */

/*
 * NOTE: These API's were written a long time ago, with the ability to invoke
 * them without defining SCSI structures, for ease of use. Since then, I have
 * found it easier (and more efficient) to expose structs to external callers.
 * Removing the optional sg allocation and initialization makes this cleaner!
 */

#include "inquiry.h"

/* ======================================================================== */

/*
 * Declarations/Definitions for Inquiry Command:
 */
#define InquiryName           "Inquiry"
#define InquiryOpcode         0x12
#define InquiryCdbSize        6
#define InquiryTimeout        ScsiDefaultTimeout

/*
 * Inquiry() - Send a SCSI Inquiry Command.
 *
 * Inputs:
 *  fd     = The file descriptor to issue bus reset to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to scsi generic pointer (optional).
 *  data   = Buffer for received Inquiry data.
 *  len    = The length of the data buffer.
 *  page   = The Inquiry VPD page (if any).
 *  flags  = OS specific SCSI flags (if any).
 *  timeout = The timeout value (in ms).
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 *
 * Note:  If the caller supplies a SCSI generic pointer, then
 * it's the callers responsibility to free this structure, along
 * with the data buffer and sense buffer.  This capability is
 * provided so the caller can examine SCSI status, sense data,
 * and data transfers, to make (more) intelligent decisions.
 */
int
Inquiry(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	scsi_addr_t *sap, scsi_generic_t **sgpp,
	void *data, unsigned int len, unsigned char page,
	unsigned int sflags, unsigned int timeout)
{
    scsi_generic_t *sgp;
    struct Inquiry_CDB *cdb; 
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic();
    }
    if (sgp->fd == INVALID_HANDLE_VALUE) {
	sgp->fd     = fd;
	sgp->dsf    = dsf;
    }
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    if (data && len) memset(data, 0, len);
    cdb             = (struct Inquiry_CDB *)sgp->cdb;
    cdb->opcode     = InquiryOpcode;
    if (page) {
	cdb->pgcode = page;
	cdb->evpd   = 1;
    }
    cdb->alclen     = len;
    sgp->cdb_size   = InquiryCdbSize;
    sgp->cdb_name   = InquiryName;
    sgp->data_dir   = scsi_data_read;
    sgp->data_buffer= data;
    sgp->data_length= len;
    sgp->errlog     = errlog;
    sgp->iface      = NULL;
    sgp->sflags     = sflags;
    sgp->timeout    = (timeout) ? timeout : InquiryTimeout;
    sgp->debug	    = debug;
    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }
    
    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;	/* Return the generic data pointer. */
	}
    } else {
	free_palign(sgp->opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}

int
verify_inquiry_header(inquiry_t *inquiry, inquiry_header_t *inqh, unsigned char page)
{
    if ( (StoH(inqh->inq_page_length) == 0) ||
	 (inqh->inq_page_code != page) ||
	 (inqh->inq_dtype != inquiry->inq_dtype) ) {
	return(FAILURE);
    }
    return(SUCCESS);
}

/* ======================================================================== */

/*
 * GetDeviceIdentifier() - Gets Inquiry Device ID Page.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  inqp   = Pointer to device Inquiry data.
 *  timeout = The timeout value (in ms).
 *
 * Return Value:
 *    Returns NULL of no device ID page or it's invalid.
 *    Otherwise, returns a pointer to a malloc'd buffer w/ID.
 */
char *
GetDeviceIdentifier(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
		    scsi_addr_t *sap, scsi_generic_t **sgpp,
		    void *inqp, unsigned int timeout)
{
    inquiry_t *inquiry = inqp;
    inquiry_page_t inquiry_data;  
    inquiry_page_t *inquiry_page = &inquiry_data;
    inquiry_header_t *inqh = &inquiry_page->inquiry_hdr;
    inquiry_ident_descriptor_t *iid;
    unsigned char page = INQ_DEVICE_PAGE;
    size_t page_length;
    char *bp = NULL;
    int status;
    /* Identifiers in order of precedence: (the "Smart" way :-) */
    /* REMEMBER:  The lower values have *higher* precedence!!! */
    enum pidt {
	REGEXT, REG, EXT_V, EXT_0, EUI64, TY1_VID, BINARY, ASCII, NONE
    };
    enum pidt pid_type = NONE;	/* Precedence ID type. */

    status = Inquiry(fd, dsf, debug, errlog, NULL, NULL,
		     inquiry_page, sizeof(*inquiry_page), page, 0, timeout);

    if (status != SUCCESS) return(NULL);

    if (verify_inquiry_header(inquiry, inqh, page) == FAILURE) return(NULL);

    page_length = (size_t)StoH(inquiry_page->inquiry_hdr.inq_page_length);
    iid = (inquiry_ident_descriptor_t *)inquiry_page->inquiry_page_data;

    /*
     * Snarf'ed out of CAM's configuration logic (ccfg.c).
     *
     * Notes:
     *	- We loop through ALL descriptors, enforcing the precedence
     *	  order defined above (see enum pidt).  This is because some
     *	  devices return more than one identifier.
     *	- This logic differs from CAM ccfg code slightly, as it
     *	  accepts unknown BINARY device ID's, which I think is Ok.
     */
    while ( (ssize_t)page_length > 0 ) {
	unsigned char *fptr = (unsigned char *)iid + sizeof(*iid);

	switch (iid->iid_code_set) {
	    
	    case IID_CODE_SET_ASCII: {
		    /* Only accept Vendor ID's of Type 1. */
		    if ( (pid_type > TY1_VID) &&
			 (iid->iid_ident_type == IID_ID_TYPE_T10_VID) ) {
			int id_len = iid->iid_ident_length + sizeof(inquiry->inq_pid);
			if (bp) {
			    free(bp) ; bp = NULL;
			};
			bp = (char *)Malloc(NULL, (id_len + 1) );
			if (bp == NULL)	return(NULL);
			pid_type = TY1_VID;
			memcpy(bp, inquiry->inq_pid, sizeof(inquiry->inq_pid));
			memcpy((bp + sizeof(inquiry->inq_pid)), fptr, iid->iid_ident_length);
		    }
		    /* Continue looping looking for IEEE identifier. */
		    break;
		}
		/*
		 * This is the preferred (unique) identifier.
		 */
	    case IID_CODE_SET_BINARY: {

		    switch (iid->iid_ident_type) {
			
			case IID_ID_TYPE_NAA: {
				enum pidt npid_type;
				int i = 0;

				/*
				 * NAA is the high order 4 bits of the 1st byte.
				 */
				switch ( (*fptr >> 4) & 0xF) {
				    /* TODO: Add defines for NAA definitions! */
				    case 0x6:	  /* IEEE Registered */
					npid_type = REGEXT;
					break;
				    case 0x5:	  /* IEEE Registered Extended */
					npid_type = REG;
					break;
				    case 0x2:	  /* ???? */
					npid_type = EXT_V;
					break;
				    case 0x1:	  /* ???? */
					npid_type = EXT_0;
					break;
				    default:
					/* unrecognized */
					npid_type = BINARY;
					break;
				}
				/*
				 * If the previous precedence ID is of lower priority,
				 * that is a higher value, then make this pid the new.
				 */
				if ( (pid_type > npid_type) ) {
				    int blen = (iid->iid_ident_length * 3);
				    char *bptr;
				    pid_type = npid_type; /* Set the new precedence type */
				    if (bp) {
					free(bp) ; bp = NULL;
				    };
				    bptr = bp = (char *)Malloc(NULL, blen);
				    if (bp == NULL) return(NULL);

				    /* Format as: xxxx-xxxx... */
				    while (i < (int)iid->iid_ident_length) {
					bptr += sprintf(bptr, "%02x", fptr[i++]);
					if (( (i % 2) == 0) &&
					    (i < (int)iid->iid_ident_length) ) {
					    bptr += sprintf(bptr, "-");
					}
				    }
				}
				break;
			    }
			case IID_ID_TYPE_EUI64: {
				int blen, i = 0;
				char *bptr;

				if ( (pid_type <= EUI64) ) {
				    break;
				}
				pid_type = EUI64;
				blen = (iid->iid_ident_length * 3);
				if (bp) {
				    free(bp) ; bp = NULL;
				};
				bptr = bp = (char *)Malloc(NULL, blen);
				if (bp == NULL)	return(NULL);

				/* Format as: xxxx-xxxx... */
				while (i < (int)iid->iid_ident_length) {
				    bptr += sprintf(bptr, "%02x", fptr[i++]);
				    if (( (i % 2) == 0) &&
					(i < (int)iid->iid_ident_length) ) {
					bptr += sprintf(bptr, "-");
				    }
				}
				break;
			    }
			default: {
				if (debug) {
				    Fprintf(NULL, "Unknown identifier %#x\n", iid->iid_ident_type);
				}
				break;
			    }
		    } /* switch (iid->iid_ident_type) */
		} /* case IID_CODE_SET_BINARY */

	    default:
		break;

	} /* switch (iid->iid_code_set) */

	page_length -= iid->iid_ident_length + sizeof(*iid);
	iid = (inquiry_ident_descriptor_t *)((char *) iid + iid->iid_ident_length + sizeof(*iid));

    } /* while ( (ssize_t)page_length > 0 ) */

    /* NOTE: Caller MUST free allocated buffer! */
    return(bp);
}

/*
 * GetSerialNumber() - Gets Inquiry Serial Number Page.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  inqp   = Pointer to device Inquiry data.
 *  timeout = The timeout value (in ms).
 *
 * Return Value:
 *    Returns NULL if no serial number page or it's invalid.
 *    Otherwise, returns a pointer to a malloc'd buffer w/serial #.
 */
char *
GetSerialNumber(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
		scsi_addr_t *sap, scsi_generic_t **sgpp,
		void *inqp, unsigned int timeout)
{
    inquiry_t *inquiry = inqp;
    inquiry_page_t inquiry_data;  
    inquiry_page_t *inquiry_page = &inquiry_data;
    inquiry_header_t *inqh = &inquiry_page->inquiry_hdr;
    unsigned char page = INQ_SERIAL_PAGE;
    size_t page_length;
    char *bp;
    int status;

    status = Inquiry(fd, dsf, debug, errlog, NULL, sgpp,
		     inquiry_page, sizeof(*inquiry_page), page, 0, timeout);

    if (status != SUCCESS) return(NULL);

    if (verify_inquiry_header(inquiry, inqh, page) == FAILURE) return(NULL);

    page_length = (size_t)StoH(inquiry_page->inquiry_hdr.inq_page_length);
    bp = (char *)Malloc(NULL, (page_length + 1) );
    if (bp == NULL) return(NULL);
    strncpy (bp, (char *)inquiry_page->inquiry_page_data, page_length);
    bp[page_length] = '\0';
    /* NOTE: Caller MUST free allocated buffer! */
    return(bp);
}


/*
 * MgmtNetworkAddress() - Gets Inquiry Managment Network Address.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  inqp   = Pointer to device Inquiry data.
 *  timeout = The timeout value (in ms).
 *
 * Return Value:
 *    Returns NULL if no serial number page or it's invalid.
 *    Otherwise, returns a pointer to a malloc'd buffer w/mgmt address.
 */
char *
GetMgmtNetworkAddress(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	       scsi_addr_t *sap, scsi_generic_t **sgpp,
	       void *inqp, unsigned int timeout)
{
    inquiry_t *inquiry = inqp;
    inquiry_page_t inquiry_data;  
    inquiry_page_t *inquiry_page = &inquiry_data;
    inquiry_header_t *inqh = &inquiry_page->inquiry_hdr;
    inquiry_network_service_page_t *inap;
    unsigned char page = INQ_MGMT_NET_ADDR_PAGE;
    size_t address_length;
    char *bp;
    int status;

    status = Inquiry(fd, dsf, debug, errlog, NULL, sgpp,
		     inquiry_page, sizeof(*inquiry_page), page, 0, timeout);

    if (status != SUCCESS) return(NULL);

    if (verify_inquiry_header(inquiry, inqh, page) == FAILURE) return(NULL);

    inap = (inquiry_network_service_page_t *)&inquiry_page->inquiry_page_data;
    address_length = (size_t)StoH(inap->address_length);
    if (address_length == 0) return(NULL);
    bp = (char *)Malloc(NULL, (address_length + 1) );
    if (bp == NULL) return(NULL);
    strncpy(bp, (char *)inap->address, address_length);
    bp[address_length] = '\0';
    /* NOTE: Caller MUST free allocated buffer! */
    return(bp);
}

/*
 * GetUniqueID - Get The Devices' Unique ID.
 *
 * Description:
 *  This function returns a unique identifer for this device.
 * We attemp to obtain the Inquiry Device ID page first, then
 * if that fails we attempt to obtain the serial num,ber page.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  identifier = Pointer to buffer to return identifier.
 *  idt    = The ID type(s) to attempt (IDT_BOTHIDS, IDT_DEVICEID, IDT_SERIALID)
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  timeout = The timeout value (in ms).
 *
 * Returns:
 *  Returns IDT_NONE and set identifier ptr to NULL if an error occurs.
 *  Returns IDT_DEVICEID if identifier points to an actual device ID.
 *  Returns IDT_SERIALID if identifier points to a manufactured identifier,
 *    using Inquiry vendor/product info and serial number page.
 *
 * Note: The identifier is dynamically allocated, so the caller is
 * responsible to free that memory, when it's no longer desired.
 */
idt_t
GetUniqueID(HANDLE fd, char *dsf, hbool_t debug,
	    char **identifier, idt_t idt,
	    hbool_t errlog, unsigned int timeout)
{
    inquiry_t inquiry_data;
    inquiry_t *inquiry = &inquiry_data;
    char *serial_number;
    int status;

    *identifier = NULL;
    /*
     * Start off by requesting the standard Inquiry data.
     */
    status = Inquiry(fd, dsf, debug, errlog, NULL, NULL,
		     inquiry, sizeof(*inquiry), 0, 0, timeout);
    if (status != SUCCESS) {
	return(IDT_NONE);
    }

    if ( (idt == IDT_BOTHIDS) || (idt == IDT_DEVICEID) ) {
	/*
	 * The preferred ID, is from Inquiry Page 0x83 (Device ID).
	 */
	if (*identifier = GetDeviceIdentifier(fd, dsf, debug, errlog,
					      NULL, NULL, inquiry, timeout)) {
	    return(IDT_DEVICEID);
	}
    }

    if ( (idt == IDT_BOTHIDS) || (idt == IDT_SERIALID) ) {
	/*
	 * The less preferred WWID, is the serial number prepended with
	 * the vendor and product names to attempt uniqueness.
	 */
	if (serial_number = GetSerialNumber(fd, dsf, debug, errlog,
					    NULL, NULL, inquiry, timeout)) {
	    *identifier = Malloc(NULL, MAX_INQ_LEN + INQ_VID_LEN + INQ_PID_LEN);
	    *identifier[0] = '\0';
	    (void)strncpy(*identifier, (char *)inquiry->inq_vid, INQ_VID_LEN);
	    (void)strncat(*identifier, (char *)inquiry->inq_pid, INQ_PID_LEN);
	    (void)strcat(*identifier, serial_number);
	    (void)free(serial_number);
	    return(IDT_SERIALID);
	}
    }
    return(IDT_NONE);
}

/* ======================================================================== */

/*
 * Declarations/Definitions for Read Capacity(10) Command:
 */
#define ReadCapacity10Name	"Read Capacity(10)"
#define ReadCapacity10Opcode	0x25
#define ReadCapacity10CdbSize	10 
#define ReadCapacity10Timeout	ScsiDefaultTimeout

/*
 * ReadCapacity10() - Issue Read Capacity (10) Command.
 *
 * Inputs:
 *  fd     = The file descriptor to issue bus reset to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  data   = Buffer for received capacity data.
 *  len    = The length of the data buffer.
 *  sflags = OS specific SCSI flags (if any).
 *  timeout = The timeout value (in ms).
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 *
 * Note:  If the caller supplies a SCSI generic pointer, then
 * it's the callers responsibility to free this structure, along
 * with the data buffer and sense buffer.  This capability is
 * provided so the caller can examine SCSI status, sense data,
 * and data transfers, to make (more) intelligent decisions.
 */
int
ReadCapacity10(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	       scsi_addr_t *sap, scsi_generic_t **sgpp,
	       void *data, unsigned int len,
	       unsigned int sflags, unsigned int timeout)
{
    scsi_generic_t *sgp;
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic();
    }
    if (sgp->fd == INVALID_HANDLE_VALUE) {
	sgp->fd     = fd;
	sgp->dsf    = dsf;
    }
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    if (data && len) memset(data, 0, len);
    sgp->cdb[0]     = ReadCapacity10Opcode;
    sgp->cdb_size   = ReadCapacity10CdbSize;
    sgp->cdb_name   = ReadCapacity10Name;
    sgp->data_dir   = scsi_data_read;
    sgp->data_buffer= data;
    sgp->data_length= len;
    sgp->errlog     = errlog;
    sgp->iface      = NULL;
    sgp->timeout    = (timeout) ? timeout : ReadCapacity10Timeout;
    sgp->debug	    = debug;
    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }

    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;	/* Return the generic data pointer. */
	}
    } else {
	free_palign(sgp->opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}

/* ======================================================================== */

/*
 * Declarations/Definitions for Read Capacity(16) Command:
 */
#define ReadCapacity16Name	"Read Capacity(16)"
#define ReadCapacity16Opcode	0x9e
#define ReadCapacity16Subcode	0x10
#define ReadCapacity16CdbSize	16 
#define ReadCapacity16Timeout	ScsiDefaultTimeout

/*
 * ReadCapacity16() - Issue Read Capacity (16) Command.
 *
 * Inputs:
 *  fd     = The file descriptor to issue bus reset to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  data   = Buffer for received capacity data.
 *  len    = The length of the data buffer.
 *  sflags = OS specific SCSI flags (if any).
 *  timeout = The timeout value (in ms).
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 *
 * Note:  If the caller supplies a SCSI generic pointer, then
 * it's the callers responsibility to free this structure, along
 * with the data buffer and sense buffer.  This capability is
 * provided so the caller can examine SCSI status, sense data,
 * and data transfers, to make (more) intelligent decisions.
 */
int
ReadCapacity16(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	       scsi_addr_t *sap, scsi_generic_t **sgpp,
	       void *data, unsigned int len,
	       unsigned int sflags, unsigned int timeout)
{
    scsi_generic_t *sgp;
    ReadCapacity16_CDB_t *cdb;
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic();
    }
    if (sgp->fd == INVALID_HANDLE_VALUE) {
	sgp->fd     = fd;
	sgp->dsf    = dsf;
    }
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    if (data && len) memset(data, 0, len);
    cdb = (ReadCapacity16_CDB_t *)sgp->cdb;
    cdb->opcode     = ReadCapacity16Opcode;
    cdb->service_action = ReadCapacity16Subcode;
    HtoS(cdb->allocation_length,  len);
    sgp->cdb_size   = ReadCapacity16CdbSize;
    sgp->cdb_name   = ReadCapacity16Name;
    sgp->data_dir   = scsi_data_read;
    sgp->data_buffer= data;
    sgp->data_length= len;
    sgp->errlog     = errlog;
    sgp->iface      = NULL;
    sgp->timeout    = (timeout) ? timeout : ReadCapacity16Timeout;
    sgp->debug	    = debug;
    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }

    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;	/* Return the generic data pointer. */
	}
    } else {
	free_palign(sgp->opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}

/* ======================================================================== */

int
GetCapacity(scsi_generic_t *sgp, u_int32 *device_size, large_t *device_capacity,
	    hbool_t *lbpme_flag, hbool_t *lbprz_flag, hbool_t *lbpmgmt_valid)
{
    ReadCapacity16_data_t ReadCapacity16_data;
    ReadCapacity16_data_t *rcdp = &ReadCapacity16_data;
    int status;
    
    if (lbpme_flag) *lbpme_flag = False;
    if (lbprz_flag) *lbprz_flag = False;
    if (lbpmgmt_valid) *lbpmgmt_valid = False;
    /*
     * 16-byte CDB may fail on some disks, but 10-byte should succeed!
     */ 
    status = ReadCapacity16(sgp->fd, sgp->dsf, sgp->debug,
			    False, NULL, NULL,
			    rcdp, sizeof(*rcdp), 0, 0);
    if (status == SUCCESS) {
	*device_size = (uint32_t)StoH(rcdp->block_length);
	*device_capacity = StoH(rcdp->last_block) + 1;
	if (lbpmgmt_valid) *lbpmgmt_valid = True;
	if (lbpme_flag && rcdp->lbpme) *lbpme_flag = True;
	if (lbprz_flag && rcdp->lbprz) *lbprz_flag = True;
    } else {
	ReadCapacity10_data_t ReadCapacity10_data;
	ReadCapacity10_data_t *rcdp = &ReadCapacity10_data;
	status = ReadCapacity10(sgp->fd, sgp->dsf, sgp->debug,
				True, NULL, NULL,
				rcdp, sizeof(*rcdp), 0, 0);
	if (status == SUCCESS) {
	    *device_size = (uint32_t)StoH(rcdp->block_length);
	    *device_capacity = StoH(rcdp->last_block) + 1;
	}
    }
    if ( (status == SUCCESS) && sgp->debug) {
	Printf(sgp->opaque, "Device: %s, Device Size: %u bytes, Capacity: " LUF " blocks\n",
	       sgp->dsf, *device_size, *device_capacity);
    }
    return (status);
}

/* ======================================================================== */

#define ReadTimeout        ScsiDefaultTimeout

int
ReadData(scsi_io_type_t read_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    int status;

    switch (read_type) {
	case scsi_read6_cdb:
	    status = Read6(sgp, (uint32_t)lba, (uint8_t)blocks, bytes);
	    break;

	case scsi_read10_cdb:
	    status = Read10(sgp, (uint32_t)lba, (uint16_t)blocks, bytes);
	    break;

	case scsi_read16_cdb:
	    status = Read16(sgp, (uint64_t)lba, (uint32_t)blocks, bytes);
	    break;

	default:
	    Eprintf(sgp->opaque, "SCSI ReadData: Invalid read I/O type detected, type = %d\n", read_type);
	    status = FAILURE;
	    break;
    }
    return (status);
}

/*
 * Read6() - Send a Read(6) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Read6(scsi_generic_t *sgp, uint32_t lba, uint8_t blocks, uint32_t bytes)
{
    DirectRW6_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW6_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_READ_6;
    HtoS(cdb->lba, lba);
    cdb->length	    = blocks;
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Read(6)";
    sgp->data_dir   = scsi_data_read;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = ReadTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/*
 * Read10() - Send a Read(10) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Read10(scsi_generic_t *sgp, uint32_t lba, uint16_t blocks, uint32_t bytes)
{
    DirectRW10_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW10_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_READ_10;
    HtoS(cdb->lba, lba);
    HtoS(cdb->length, blocks);
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Read(10)";
    sgp->data_dir   = scsi_data_read;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = ReadTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/*
 * Read16() - Send a Read(16) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Read16(scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    DirectRW16_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW16_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_READ_16;
    if (sgp->dpo) {
	cdb->flags |= SCSI_DIR_RDWR_16_DPO;
    }
    if (sgp->fua) {
	cdb->flags |= SCSI_DIR_RDWR_16_FUA;
    }
    HtoS(cdb->lba, lba);
    HtoS(cdb->length, blocks);
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Read(16)";
    sgp->data_dir   = scsi_data_read;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = ReadTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/* ======================================================================== */

#define WriteTimeout        ScsiDefaultTimeout

int
WriteData(scsi_io_type_t write_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    int status;

    switch (write_type) {
	case scsi_write6_cdb:
	    status = Write6(sgp, (uint32_t)lba, (uint8_t)blocks, bytes);
	    break;

	case scsi_write10_cdb:
	    status = Write10(sgp, (uint32_t)lba, (uint16_t)blocks, bytes);
	    break;

	case scsi_write16_cdb:
	case scsi_writev16_cdb:
	    status = Write16(sgp, (uint64_t)lba, (uint32_t)blocks, bytes);
	    break;

	default:
	    Eprintf(sgp->opaque, "SCSI WriteData: Invalid write I/O type detected, type = %d\n", write_type);
	    status = FAILURE;
	    break;
    }
    return (status);
}

/*
 * Write6() - Send a Write(6) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Write6(scsi_generic_t *sgp, uint32_t lba, uint8_t blocks, uint32_t bytes)
{
    DirectRW6_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW6_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_WRITE_6;
    HtoS(cdb->lba, lba);
    cdb->length	    = blocks;
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Write(6)";
    sgp->data_dir   = scsi_data_write;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = WriteTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/*
 * Write10() - Send a Write(10) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Write10(scsi_generic_t *sgp, uint32_t lba, uint16_t blocks, uint32_t bytes)
{
    DirectRW10_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW10_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_WRITE_10;
    HtoS(cdb->lba, lba);
    HtoS(cdb->length, blocks);
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Write(10)";
    sgp->data_dir   = scsi_data_write;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = WriteTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/*
 * Write16() - Send a Write(16) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Write16(scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    DirectRW16_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW16_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_WRITE_16;
    HtoS(cdb->lba, lba);
    if (sgp->dpo) {
	cdb->flags |= SCSI_DIR_RDWR_16_DPO;
    }
    if (sgp->fua) {
	cdb->flags |= SCSI_DIR_RDWR_16_FUA;
    }
    HtoS(cdb->length, blocks);
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Write(16)";
    sgp->data_dir   = scsi_data_write;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = ReadTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/*
 * Declarations/Definitions for Test Unit Ready Command:
 */
#define TestUnitReadyName     "Test Unit Ready"
#define TestUnitReadyOpcode   0
#define TestUnitReadyCdbSize  6
#define TestUnitReadyTimeout  (30 * MSECS)

/*
 * TestUnitReady() - Send a SCSI Test Unit Teady (tur).
 *
 * Inputs:
 *  fd     = The file descriptor to issue bus reset to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to scsi generic pointer (optional).
 *  timeout = The timeout value (in ms).
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 *
 * Note:  If the caller supplies a SCSI generic pointer, then
 * it's the callers responsibility to free this structure, along
 * with the data buffer and sense buffer.  This capability is
 * provided so the caller can examine SCSI status, sense data,
 * and data transfers, to make (more) intelligent decisions.
 */
int
TestUnitReady(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	      scsi_addr_t *sap, scsi_generic_t **sgpp, unsigned int timeout)
{
    scsi_generic_t *sgp;
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic();
    }
    if (sgp->fd == INVALID_HANDLE_VALUE) {
	sgp->fd     = fd;
	sgp->dsf    = dsf;
    }
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    sgp->cdb[0]     = TestUnitReadyOpcode;
    sgp->cdb_size   = TestUnitReadyCdbSize;
    sgp->cdb_name   = TestUnitReadyName;
    sgp->data_dir   = scsi_data_none;
    sgp->data_buffer= NULL;
    sgp->data_length= 0;
    sgp->errlog     = errlog;
    sgp->iface      = NULL;
    sgp->timeout    = (timeout) ? timeout : TestUnitReadyTimeout;
    sgp->debug	    = debug;
    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }

    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;	/* Return the generic data pointer. */
	}
    } else {
	free_palign(sgp->opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}

/* ======================================================================== */

/*
 * Declarations/Definitions for Seek(10) Command:
 */
#define Seek10Name     "Seek(10)"
#define Seek10Opcode   0x2B
#define Seek10CdbSize  10
#define Seek10Timeout  (30 * MSECS)

/*
 * Seek10() - Send a Seek LBA Command.
 *
 * Inputs:
 *  fd     = The file descriptor to issue bus reset to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to scsi generic pointer (optional).
 *  lba    = The logical block address.
 *  timeout = The timeout value (in ms).
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 */
int
Seek10(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
       scsi_addr_t *sap, scsi_generic_t **sgpp,
       unsigned int lba, unsigned int timeout)
{
    scsi_generic_t *sgp;
    Seek10_CDB_t *cdb;
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic();
    }
    if (sgp->fd == INVALID_HANDLE_VALUE) {
	sgp->fd     = fd;
	sgp->dsf    = dsf;
    }
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb		    = (Seek10_CDB_t *)sgp->cdb;
    cdb->opcode	    = Seek10Opcode;
    HtoS(cdb->lba, lba);
    sgp->cdb_size   = Seek10CdbSize;
    sgp->cdb_name   = Seek10Name;
    sgp->data_dir   = scsi_data_none;
    sgp->data_buffer= NULL;
    sgp->data_length= 0;
    sgp->errlog     = errlog;
    sgp->iface      = NULL;
    sgp->timeout    = (timeout) ? timeout : Seek10Timeout;
    sgp->debug	    = debug;
    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }

    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;	/* Return the generic data pointer. */
	}
    } else {
	free_palign(sgp->opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}

/* ======================================================================== */

/*
 * SendAnyCdb() - Send a User Defined CDB (no data for now).
 * 
 * Description:
 * 	Simple function to send a user defined trigger CDB.
 *
 * Inputs:
 *  fd     = The file descriptor to issue bus reset to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to scsi generic pointer (optional).
 *  timeout = The timeout value (in ms).
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 *
 * Note:  If the caller supplies a SCSI generic pointer, then
 * it's the callers responsibility to free this structure, along
 * with the data buffer and sense buffer.  This capability is
 * provided so the caller can examine SCSI status, sense data,
 * and data transfers, to make (more) intelligent decisions.
 */
int
SendAnyCdb(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	   scsi_addr_t *sap, scsi_generic_t **sgpp, unsigned int timeout, 
	   uint8_t *cdb, uint8_t cdb_size)
{
    scsi_generic_t scsi_generic;
    scsi_generic_t *sgp = &scsi_generic;
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic();
    }
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    sgp->fd         = fd;
    sgp->dsf        = dsf;
    memcpy(sgp->cdb, cdb, cdb_size);
    sgp->cdb_size   = cdb_size;
    sgp->cdb_name   = "Any SCSI";
    sgp->data_dir   = scsi_data_none;
    sgp->debug      = debug;
    sgp->errlog     = errlog;
    sgp->timeout    = (timeout) ? timeout : ScsiDefaultTimeout;

    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }

    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;        /* Return the generic data pointer. */
	}
    } else {
	free_palign(sgp->opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}

/* ======================================================================== */
/* Utility Functions: */

/*
 * scmn_cdb_length() - Calculate the Command Descriptor Block length.
 *
 * Description:
 *	This function is used to determine the SCSI CDB length.  This
 * is done by checking the command group code.  The command specified
 * is expected to be the actual SCSI command byte, not a psuedo command
 * byte.  There should be tables for vendor specific commands, since
 * there is no way of determining the length of these commands.
 *
 * Inputs:
 *	opcode = The SCSI operation code.
 *
 * Return Value:
 *	Returns the CDB length.
 */
int
GetCdbLength(unsigned char opcode)
{
    int cdb_length = 0;

    /*
     * Calculate the size of the SCSI command.
     */
    switch (opcode & SCSI_GROUP_MASK) {
	
	case SCSI_GROUP_0:
	    cdb_length = 6;	    /* 6 byte CDB. */
	    break;

	case SCSI_GROUP_1:
	case SCSI_GROUP_2:
	    cdb_length = 10;	    /* 10 byte CDB. */
	    break;

	case SCSI_GROUP_5:
	    cdb_length = 12;	    /* 12 byte CDB. */
	    break;

	case SCSI_GROUP_3:
	    cdb_length = 0;	    /* Reserved group. */
	    break;

	case SCSI_GROUP_4:
	    cdb_length = 16;	    /* 16 byte CDB. */
	    break;

	case SCSI_GROUP_6:
	case SCSI_GROUP_7:
	    cdb_length = 10;	    /* Vendor unique. */
	    break;
    }
    return(cdb_length);
}
