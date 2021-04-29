/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2021			    *
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
 * Module:	scsilib-sx.c
 * Author:	Robin T. Miller
 * Date:	March 29th, 2005
 *
 * Description:
 *  This module contains the OS SCSI specific functions for Solaris.
 *
 * Modification History:
 *
 * August 26th, 2010 by Robin T. Miller
 * 	When opening device, on EROFS errno, try opening read-only.
 * 
 * April 8th, 2008 by Robin T. Miller.
 *	Add USCSI_DIAGNOSE to default flags for USCSI SPT request.
 *
 * August 14th, 2007 by Robin T. Miller.
 *	Added support for Solaris queue message type control flags.
 *
 * August 6th, 2007 by Robin T. Miller.
 *	Added OS open and close functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/scsi/impl/uscsi.h>

#include "dt.h"
//#include "libscsi.h"

/*
 * Local Definitions:
 */

/*
 * Forward Declarations:
 */
static void DumpScsiCmd(scsi_generic_t *sgp, struct uscsi_cmd *siop);

/*
 * os_open_device() = Open Device.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the open() which is:
 *        0 = Success or -1 = Failure
 */
int
os_open_device(scsi_generic_t *sgp)
{
    int status = SUCCESS;
    int oflags = (O_RDWR|O_NONBLOCK);

    if (sgp->debug == True) {
	Printf(sgp->opaque, "Opening device %s, open flags = %#o (%#x)...\n",
	       sgp->dsf, oflags, oflags);
    }
    if ( (sgp->fd = open(sgp->dsf, oflags)) < 0) {
	if (errno == EROFS) {
	    int oflags = (O_RDONLY|O_NONBLOCK);
	    if (sgp->debug == True) {
		Printf(sgp->opaque, "Opening device %s read-only, open flags = %#o (%#x)...\n",
		       sgp->dsf, oflags, oflags);
	    }
	    sgp->fd = open(sgp->dsf, oflags);
	}
	if (sgp->fd == INVALID_HANDLE_VALUE) {
	    if (sgp->errlog == True) {
		os_perror(sgp->opaque, "open() of %s failed!", sgp->dsf);
	    }
	    status = FAILURE;
	}
    }
    if ( (sgp->debug == True) && (sgp->fd != INVALID_HANDLE_VALUE) ) {
	Printf(sgp->opaque, "Device %s successfully opened, fd = %d\n", sgp->dsf, sgp->fd);
    }
    return(status);
}

/*
 * os_close_device() = Close Device.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the close() which is:
 *        0 = Success or -1 = Failure
 */
int
os_close_device(scsi_generic_t *sgp)
{
    int error;

    if (sgp->debug == True) {
	Printf(sgp->opaque, "Closing device %s, fd %d...\n", sgp->dsf, sgp->fd);
    }
    if ( (error = close(sgp->fd)) < 0) {
	os_perror(sgp->opaque, "close() of %s failed", sgp->dsf);
    }
    sgp->fd = INVALID_HANDLE_VALUE;
    return(error);
}

/*
 * os_abort_task_set() - Send Abort Task Set.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_abort_task_set(scsi_generic_t *sgp)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Abort Task Set is not supported!\n");
    }
    return(error);
}

/*
 * os_clear_task_set() - Send Clear Task Set.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_clear_task_set(scsi_generic_t *sgp)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Clear Task Set is not supported!\n");
    }
    return(error);
}

/*
 * os_cold_target_reset() - Send a Cold Target Reset.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning (not supported), -1 = Failure
 */
int
os_cold_target_reset(scsi_generic_t *sgp)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Cold Target Reset is not implemented!\n");
    }
    return(error);
}

/*
 * os_warm_target_reset() - Send a Warm Target Reset.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_warm_target_reset(scsi_generic_t *sgp)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Warm Target Reset is not implemented!\n");
    }
    return(error);
}

/*
 * os_reset_bus() - Reset the SCSI Bus (All targets and LUNs).
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning (not supported), -1 = Failure
 */
int
os_reset_bus(scsi_generic_t *sgp)
{
    int error = WARNING;
    unsigned char inquiry_data[36];

    error = Inquiry(sgp->fd, sgp->dsf, sgp->debug, sgp->errlog, NULL, NULL,
		    inquiry_data, sizeof(inquiry_data), 0,
		    USCSI_RESET_ALL, sgp->timeout);
    return(error);
}

/*
 * os_reset_ctlr() - Reset the Controller.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning (not supported), -1 = Failure
 */
int
os_reset_ctlr(scsi_generic_t *sgp)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "SCSI reset controller is not supported!\n");
    }
    return(error);
}

/*
 * os_reset_device() - Reset the SCSI Device (including all LUNs).
 *
 * Note:  A device reset is also known as a Bus Device Reset (BDR).
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning (not supported), -1 = Failure
 */
int
os_reset_device(scsi_generic_t *sgp)
{
    int error = WARNING;
    unsigned char inquiry_data[36];

    error = Inquiry(sgp->fd, sgp->dsf, sgp->debug, sgp->errlog, NULL, NULL,
		    inquiry_data, sizeof(inquiry_data), 0,
		    USCSI_RESET, sgp->timeout);
    return(error);
}

/*
 * os_reset_lun() - Reset the SCSI LUN (Logical Unit only).
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_reset_lun(scsi_generic_t *sgp)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "SCSI reset lun is not supported!\n");
    }
    return(error);
}

/*
 * os_scan() - Scan For Devices.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_scan(scsi_generic_t *sgp)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Scan for devices is not implemented!\n");
    }
    return(error);
}

/*
 * os_resumeio() - Resume I/O to a Suspended Disk.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_resumeio(scsi_generic_t *sgp)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Resume I/O is not implemented!\n");
    }
    return(error);
}

/*
 * os_suspendio() - Suspend I/O to This Disk.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_suspendio(scsi_generic_t *sgp)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Suspend I/O is not implemented!\n");
    }
    return(error);
}

/*
 * os_get_timeout() - Get Device Timeout.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *      timeout= Pointer to store the timeout value.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 *    0 = Success, 1 = Warning, -1 = Failure
 */
int
os_get_timeout(scsi_generic_t *sgp, unsigned int *timeout)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Get timeout is not implemented!\n");
    }
    return(error);
}

/*
 * os_set_timeout() - Set the Device Timeout.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *      timeout= The timeout value to set.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 *    0 = Success, 1 = Warning, -1 = Failure
 */
int
os_set_timeout(scsi_generic_t *sgp, unsigned timeout)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Set timeout is not implemented!\n");
    }
    return(error);
}

/*
 * os_get_qdepth() - Get the Device Queue Depth.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *      qdepth = Pointer to store the queue depth value.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_get_qdepth(scsi_generic_t *sgp, unsigned int *qdepth)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Get queue depth is not implemented!\n");
    }
    return(error);
}

/*
 * os_set_qdepth() - Set the Device Queue Depth.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *      qdepth = The queue depth value to set.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_set_qdepth(scsi_generic_t *sgp, unsigned int qdepth)
{
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "Set queue depth is not implemented!\n");
    }
    return(error);
}

/*
 * os_spt() - OS Specific SCSI Pass-Through (spt).
 *
 * Description:
 *  This function takes a high level SCSI command, converts it
 * into the format necessary for this OS, then executes it and
 * returns an OS independent format to the caller.
 *
 * Inputs:
 *  sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *  Returns the status from the SCSI request which is:
 *    0 = Success, -1 = Failure
 */
int
os_spt(scsi_generic_t *sgp)
{
    struct uscsi_cmd  scsicmd;
    struct uscsi_cmd  *siop = &scsicmd;
    int               error;

    memset(siop, 0, sizeof(*siop));

    siop->uscsi_flags   = (sgp->sflags | USCSI_DIAGNOSE | USCSI_RQENABLE);
    siop->uscsi_cdb     = sgp->cdb;
    siop->uscsi_cdblen  = sgp->cdb_size;
    siop->uscsi_bufaddr = sgp->data_buffer;
    siop->uscsi_buflen  = sgp->data_length;
    siop->uscsi_rqbuf   = sgp->sense_data;
    siop->uscsi_rqlen   = sgp->sense_length;
    /*
     * Setup the data direction:
     */
    if (sgp->data_dir == scsi_data_none) {
	;				     /* No data to be transferred.    */
    } else if (sgp->data_dir == scsi_data_read) {
	siop->uscsi_flags |= USCSI_READ;     /* Reading data from the device. */
    } else { /* (sgp->data_dir == scsi_data_write) */
	siop->uscsi_flags |= USCSI_WRITE;    /* Writing data to the device.   */
    }
    siop->uscsi_timeout = (sgp->timeout / MSECS); /* Timeout in seconds.    */
    if (siop->uscsi_timeout == 0) {
	siop->uscsi_timeout++;
    }
    /*
     * Setup (optional) transfer modes, etc.
     */
    if (sgp->flags & SG_INIT_ASYNC) {
	siop->uscsi_flags |= USCSI_ASYNC;   /* Return bus to async mode.      */
    }
    if (sgp->flags & SG_INIT_SYNC) {
	siop->uscsi_flags |= USCSI_SYNC;    /* Negotiate sync data transfers. */
    }
#if defined(USCSI_RENEGOT)
    if (sgp->flags & SG_INIT_WIDE) {
	siop->uscsi_flags |= USCSI_RENEGOT; /* Renegotiate wide/sync data.    */
    }
#endif /* defined(USCSI_RENEGOT) */

    if (sgp->qtag_type == SG_NO_Q) {
	siop->uscsi_flags |= USCSI_NOTAG;     /* Disable tagged queuing. */
    } else if (sgp->qtag_type == SG_HEAD_OF_Q) {
	siop->uscsi_flags |= USCSI_HTAG;      /* Head of queue. */
    } else if (sgp->qtag_type == SG_ORDERED_Q) {
	siop->uscsi_flags |= USCSI_OTAG;      /* Ordered queue. */
    } else if (sgp->qtag_type == SG_HEAD_HA_Q) {
	siop->uscsi_flags |= USCSI_HEAD;      /* Head of HA queue. */
    } else {
	; /* Simple queuing must be the default. */
    }

    /*
     * Finally, execute the SCSI command:
     */
    error = ioctl(sgp->fd, USCSICMD, siop);

    /*
     * Handle errors, and send pertinent data back to the caller.
     */
    if (error < 0) {
	sgp->os_error = errno;
	/*
	 * This OS returns failure on the IOCTL, even though the SPT data was
	 * valid, and the actual error is from the adapter or SCSI CDB, so we
	 * handle that difference here.  Basically, we don't wish to log an
	 * IOCTL error, when it might be just a SCSI Check Condition.
	 */
	if (siop->uscsi_status != 0) { /* Assume a SCSI CDB failure! */
	    error = 0;
	} else {
	    if (sgp->errlog == True) {
		os_perror(sgp->opaque, "SCSI request (USCSICMD) failed on %s!", sgp->dsf);
	    }
	    sgp->error = True;
	    goto error;
	}
    }
    if (siop->uscsi_status == SCSI_GOOD) {
	sgp->error = False;	/* Show SCSI command was successful. */
    } else {
	sgp->error = True;	/* Tell caller we've had some sort of error. */
	if ( (sgp->errlog == True) && (siop->uscsi_status != SCSI_CHECK_CONDITION) ) {
	    Fprintf(sgp->opaque, "%s failed, SCSI status = %d (%s)\n", sgp->cdb_name,
		    siop->uscsi_status, ScsiStatus(siop->uscsi_status));
	}
    }
    if ( (siop->uscsi_status == SCSI_CHECK_CONDITION) &&
	 (siop->uscsi_rqstatus == SCSI_GOOD) ) {
	sgp->sense_valid = True;
	sgp->sense_resid = siop->uscsi_rqresid;
    }
    sgp->data_resid   = siop->uscsi_resid;

    /*
     * Interesting, our resid can be greater than our data length if the CDB
     * length is larger than the specified data length (at least on Linux).
     * Note: This length mismatch caused an ABORT, but data is transferred!
     */
    if (sgp->data_resid > sgp->data_length) {
	sgp->data_transferred = sgp->data_length;
    } else {
	sgp->data_transferred = (sgp->data_length - sgp->data_resid);
    }
    sgp->scsi_status  = siop->uscsi_status;
    sgp->sense_status = siop->uscsi_rqstatus;
error:
    if (sgp->debug == True) {
	DumpScsiCmd(sgp, siop);
    }
    return(error);
}

/*
 * os_is_retriable() - OS Specific Checks for Retriable Errors.
 *
 * Description:
 *  This OS specific function determines if the last SCSI request is a
 * retriable error. Note: The checks performed here are those that are
 * OS specific, such as looking a host, driver, or syscall errors that can
 * be retried automatically, and/or to perform any OS specific recovery.
 * 
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the SCSI request which is:
 *        0 = False, 1 = True
 */
hbool_t
os_is_retriable(scsi_generic_t *sgp)
{
    hbool_t is_retriable = False;
    
    return (is_retriable);
}

static void
DumpScsiCmd(scsi_generic_t *sgp, struct uscsi_cmd *siop)
{
    int i;
    char buf[256];
    char *bp = buf;

    Printf(sgp->opaque, "SCSI I/O Structure:\n");

    Printf(sgp->opaque, "    Device Special File .............................: %s\n", sgp->dsf);
    Printf(sgp->opaque, "    File Descriptor .............................. fd: %d\n", sgp->fd);

    /*
     * Decode the SCSI flags.
     */
    if (siop->uscsi_flags & USCSI_READ) {
	bp += sprintf(bp, "USCSI_READ(%x)", USCSI_READ);
    } else {
	bp += sprintf(bp, "USCSI_WRITE(0)");
    }
    if (siop->uscsi_flags & USCSI_SILENT) {
	bp += sprintf(bp, "|USCSI_SILENT(%x)", USCSI_SILENT);
    }
    if (siop->uscsi_flags & USCSI_DIAGNOSE) {
	bp += sprintf(bp, "|USCSI_DIAGNOSE(%x)", USCSI_DIAGNOSE);
    }
    if (siop->uscsi_flags & USCSI_ISOLATE) {
	bp += sprintf(bp, "|USCSI_ISOLATE(%x)", USCSI_ISOLATE);
    }
    if (siop->uscsi_flags & USCSI_RESET) {
	bp += sprintf(bp, "|USCSI_RESET(%x)", USCSI_RESET);
    }
    if (siop->uscsi_flags & USCSI_RESET_ALL) {
	bp += sprintf(bp, "|USCSI_RESET_ALL(%x)", USCSI_RESET_ALL);
    }
    if (siop->uscsi_flags & USCSI_RQENABLE) {
	bp += sprintf(bp, "|USCSI_RQENABLE(%x)", USCSI_RQENABLE);
    }
#if defined(USCSI_RENEGOT)
    if (siop->uscsi_flags & USCSI_RENEGOT) {
	bp += sprintf(bp, "|USCSI_RENEGOT(%x)", USCSI_RENEGOT);
    }
#endif /* defined(USCSI_RENEGOT) */
    if (siop->uscsi_flags & USCSI_NOTAG) {
	bp += sprintf(bp, "|USCSI_NOTAG(%x)", USCSI_NOTAG);
    }
    if (siop->uscsi_flags & USCSI_OTAG) {
	bp += sprintf(bp, "|USCSI_OTAG(%x)", USCSI_OTAG);
    }
    if (siop->uscsi_flags & USCSI_HTAG) {
	bp += sprintf(bp, "|USCSI_HTAG(%x)", USCSI_HTAG);
    }
    if (siop->uscsi_flags & USCSI_NOTAG) {
	bp += sprintf(bp, "|USCSI_HEAD(%x)", USCSI_HEAD);
    }

    Printf(sgp->opaque, "    Control Flags ....................... uscsi_flags: %#x = %s\n",
	   siop->uscsi_flags, buf);
    Printf(sgp->opaque, "    SCSI Result Status ................. uscsi_status: %#x (%s)\n",
	   siop->uscsi_status, ScsiStatus(siop->uscsi_status));
    Printf(sgp->opaque, "    Command Timeout ................... uscsi_timeout: %d seconds\n", siop->uscsi_timeout);
    for (bp = buf, i = 0; (i < siop->uscsi_cdblen); i++) {
	bp += sprintf(bp, "%x ", (unsigned char)siop->uscsi_cdb[i]);
    }
    Printf(sgp->opaque, "    Command Descriptor Block .............. uscsi_cdb: %s(%s)\n", buf, sgp->cdb_name);
    Printf(sgp->opaque, "    CDB Length ......................... uscsi_cdblen: %u\n", siop->uscsi_cdblen);
    Printf(sgp->opaque, "    I/O Buffer Address ................ uscsi_bufaddr: %#lx\n", siop->uscsi_bufaddr);
    Printf(sgp->opaque, "    I/O Buffer Length .................. uscsi_buflen: %u (%#x)\n",
	   siop->uscsi_buflen, siop->uscsi_buflen);
    Printf(sgp->opaque, "    I/O Residual Count .................. uscsi_resid: %u (%#x)\n",
	   siop->uscsi_resid, siop->uscsi_resid);
    Printf(sgp->opaque, "    Request Sense Buffer ................ uscsi_rqbuf: %#lx\n", siop->uscsi_rqbuf);
    Printf(sgp->opaque, "    Request Sense Length ................ uscsi_rqlen: %u (%#x)\n",
	   siop->uscsi_rqlen, siop->uscsi_rqlen);
    Printf(sgp->opaque, "    Request Sense Status ............. uscsi_rqstatus: %#x (%s)\n",
	   siop->uscsi_rqstatus, ScsiStatus(siop->uscsi_rqstatus));
    Printf(sgp->opaque, "\n");
}

/*
 * os_host_status_msg() - Get the Host Status Message.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns host status message or NULL if not found or unsupported.
 */
char *
os_host_status_msg(scsi_generic_t *sgp)
{
    return(NULL);
}

/*
 * os_driver_status_msg() - Get the Driver Status Message.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns driver status message or NULL if not found or unsupported.
 */
char *
os_driver_status_msg(scsi_generic_t *sgp)
{
    return(NULL);
}
