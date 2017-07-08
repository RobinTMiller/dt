/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2017			    *
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
 * Module:	scsilib-ux.c
 * Author:	Robin T. Miller
 * Date:	March 24th, 2005
 *
 * Description:
 *  This module contains the OS SCSI specific functions for HP-UX.
 *
 * Modification History:
 *
 * August 26th, 2010 by Robin T. Miller
 * 	When opening device, on EROFS errno, try opening read-only.
 * 
 * August 6th, 2007 by Robin T. Miller.
 *	Added OS open and close functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/scsi.h>

#include "dt.h"
//#include "libscsi.h"

/*
 * Local Definitions:
 */

/*
 * Forward Declarations:
 */
static void DumpScsiCmd(scsi_generic_t *sgp, struct sctl_io *siop);
static char *hpux_ScsiStatus(unsigned scsi_status);

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

    /* TODO:  Revisit this for 11.31! Implement Abort Task Set. */
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

    /* TODO:  Revisit this for 11.31! Implement Clear Task Set. */
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
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_cold_target_reset(scsi_generic_t *sgp)
{
    int error = WARNING;

    /* TODO:  Revisit this for 11.31! */
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

    /* TODO:  Revisit this for 11.31! */
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
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_reset_bus(scsi_generic_t *sgp)
{
    int error;

    if ( (error = ioctl(sgp->fd, SIOC_RESET_BUS, 0)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    os_perror(sgp->opaque, "SCSI reset bus (SIOC_RESET_BUS) failed on %s!", sgp->dsf);
	}
    }
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
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_reset_ctlr(scsi_generic_t *sgp)
{
#if 1
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "SCSI reset controller is not supported!\n");
    }
#else
    int one = 1;
    int error;

    if ( (error = ioctl(sgp->fd, DIOC_RSTCLR, &one)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    Fprintf(sgp->opaque, "SCSI reset controller (DIOC_RSTCLR) failed on %s!\n", sgp->dsf);
	}
    }
#endif
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
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_reset_device(scsi_generic_t *sgp)
{
    int error;

    if ( (error = ioctl(sgp->fd, SIOC_RESET_DEV, 0)) < 0) {
	if (sgp->errlog == True) {
	    os_perror(sgp->opaque, "SCSI reset device (SIOC_RESET_DEV) failed on %s!", sgp->dsf);
	}
    }
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

    /* TODO: Update for 11.31 with task management lun reset. */
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
 *    0 = Success, 1 = Warning, -1 = Failure
 */
int
os_get_qdepth(scsi_generic_t *sgp, unsigned int *qdepth)
{
    struct sioc_lun_limits lun_limits;
    int error;

    memset(&lun_limits, '\0', sizeof(lun_limits));
    if ( (error = ioctl(sgp->fd, SIOC_GET_LUN_LIMITS, &lun_limits)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    Fprintf(sgp->opaque, "SIOC_GET_LUN_LIMITS on %s failed!\n", sgp->dsf);
	}
    } else {
	*qdepth = lun_limits.max_q_depth;
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
    struct sioc_lun_limits lun_limits;
    int error;

    memset(&lun_limits, '\0', sizeof(lun_limits));
    lun_limits.max_q_depth = qdepth;
    /*
     * For performance testing, allow disabling tags.
     */
    if (qdepth == 0) {
#if defined(SCTL_DISABLE_TAGS)
	lun_limits.flags = SCTL_DISABLE_TAGS;
#else /* !defined(SCTL_DISABLE_TAGS) */
	lun_limits.flags = 0;
#endif /* defined(SCTL_DISABLE_TAGS) */
    } else {
	lun_limits.flags = SCTL_ENABLE_TAGS;
    }
    if ( (error = ioctl(sgp->fd, SIOC_SET_LUN_LIMITS, &lun_limits)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    Fprintf(sgp->opaque, "SIOC_SET_LUN_LIMITS failed on %s!\n", sgp->dsf);
	}
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
    struct sctl_io sctl_io;
    struct sctl_io *siop = &sctl_io;
    int error;

    memset(siop, 0, sizeof(*siop));
    /*
     * Sanity check the CDB size (just in case).
     */
    if (sgp->cdb_size > sizeof(siop->cdb)) {
	Fprintf(sgp->opaque, "CDB size of %d is too large for max OS CDB of %d!\n",
		sgp->cdb_size, sizeof(siop->cdb));
	return(-1);
    }
    (void)memcpy(siop->cdb, sgp->cdb, (size_t)sgp->cdb_size);

    siop->flags       = sgp->sflags;
    siop->cdb_length  = sgp->cdb_size;
    siop->data        = sgp->data_buffer;
    siop->data_length = sgp->data_length;
    /*
     * Setup the data direction:
     */
    if (sgp->data_dir == scsi_data_none) {
	;				/* No data to be transferred.    */
    } else if (sgp->data_dir == scsi_data_read) {
	siop->flags |= SCTL_READ;	/* Reading data from the device. */
    } else { /* (sgp->data_dir == scsi_data_write) */
	;			       /* Writing data to the device.    */
    }
    siop->max_msecs = sgp->timeout;  /* Timeout in milliseconds.       */

    if (sgp->flags & SG_INIT_SYNC) {
	siop->flags |= SCTL_INIT_SDTR; /* Negotiate sync data transfers. */
    }
    if (sgp->flags & SG_INIT_WIDE) {
	siop->flags |= SCTL_INIT_WDTR; /* Negotiate wide data transfers. */
    }

    /*
     * Finally, execute the SCSI command:
     */
    error = ioctl(sgp->fd, SIOC_IO, siop);

    /*
     * Handle errors, and send pertinent data back to the caller.
     */
    if (error < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    os_perror(sgp->opaque, "SCSI request (SIOC_IO) failed on %s!", sgp->dsf);
	}
	sgp->error = True;
	goto error;
    }
    if (siop->cdb_status == S_GOOD) {
	sgp->error = False;	/* Show SCSI command was successful. */
    } else {
	sgp->error = True;	/* Tell caller we've had some sort of error. */
	if ( (sgp->errlog == True) && (siop->cdb_status != S_CHECK_CONDITION) ) {
	    Fprintf(sgp->opaque, "%s failed, SCSI status = %d (%s)\n", sgp->cdb_name,
		    siop->cdb_status, hpux_ScsiStatus(siop->cdb_status));
	}
    }
    if ( (siop->cdb_status == S_CHECK_CONDITION) &&
	 (siop->sense_status == S_GOOD) ) {
	int sense_length = min(sgp->sense_length, sizeof(siop->sense));
	sgp->sense_valid = True;
	sgp->sense_resid = (sgp->sense_length - siop->sense_xfer);
	(void)memcpy(sgp->sense_data, &siop->sense, sense_length);
    }
    sgp->data_resid   = (sgp->data_length - siop->data_xfer);

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
    sgp->scsi_status  = siop->cdb_status;
    sgp->sense_status = siop->sense_status;

    /*
     * Please Beware: The siop->flags may get altered by the IOCTL!
     *                For example, SCTL_INIT_SDTR and SCTL_INIT_WDTR 
     */
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
DumpScsiCmd(scsi_generic_t *sgp, struct sctl_io *siop)
{
    int i;
    char buf[128];
    char *bp = buf;

    Printf(sgp->opaque, "SCSI I/O Structure:\n");

    Printf(sgp->opaque, "    Device Special File .............................: %s\n", sgp->dsf);
    Printf(sgp->opaque, "    File Descriptor .............................. fd: %d\n", sgp->fd);
    /*
     * Decode the SCSI flags.
     */
    if (siop->flags & SCTL_READ) {
	bp += sprintf(bp, "SCTL_READ(%x)", SCTL_READ);
    } else {
	bp += sprintf(bp, "SCTL_WRITE(0)");
    }
    if (siop->flags & SCTL_INIT_WDTR) {
	bp += sprintf(bp, "|SCTL_INIT_WDTR(%x)", SCTL_INIT_WDTR);
    }
    if (siop->flags & SCTL_INIT_SDTR) {
	bp += sprintf(bp, "|SCTL_INIT_SDTR(%x)", SCTL_INIT_SDTR);
    }
    if (siop->flags & SCTL_NO_DISC) {
	bp += sprintf(bp, "|SCTL_NO_DISC(%x)", SCTL_NO_DISC);
    }
    Printf(sgp->opaque, "    Control Flags ............................. flags: %#x = %s)\n",
	   siop->flags, buf);
    Printf(sgp->opaque, "    SCSI CDB Status ...................... cdb_status: %#x (%s)\n",
	   siop->cdb_status, hpux_ScsiStatus(siop->cdb_status));
    Printf(sgp->opaque, "    Command Timeout ....................... max_msecs: %u ms (%u seconds)\n",
	   siop->max_msecs, (siop->max_msecs / MSECS));
    for (bp = buf, i = 0; (i < siop->cdb_length); i++) {
	bp += sprintf(bp, "%x ", siop->cdb[i]);
    }
    Printf(sgp->opaque, "    Command Descriptor Block .................... cdb: %s(%s)\n", buf, sgp->cdb_name);
    Printf(sgp->opaque, "    CDB Length ........................... cdb_length: %d\n", siop->cdb_length);
    Printf(sgp->opaque, "    I/O Buffer Address ......................... data: %#lx\n", siop->data);
    Printf(sgp->opaque, "    I/O Buffer Length ................... data_length: %d (%#x)\n",
	   siop->data_length, siop->data_length);
    Printf(sgp->opaque, "    I/O Data Transferred .................. data_xfer: %d (%#x)\n",
	   siop->data_xfer, siop->data_xfer);
    Printf(sgp->opaque, "    Request Sense Buffer ...................... sense: %#lx\n", &siop->sense);
    Printf(sgp->opaque, "    Request Sense Length .............. sizeof(sense): %d (%#x)\n",
	   sizeof(siop->sense), sizeof(siop->sense));
    Printf(sgp->opaque, "    Request Sense Status ............... sense_status: %#x (%s)\n",
	   siop->sense_status, hpux_ScsiStatus(siop->sense_status));
    Printf(sgp->opaque, "\n");
}

/*
 * HP-UX SCSI Status Code Table.
 */
static struct hpuxSCSI_StatusTable {
    unsigned  scsi_status;  /* The SCSI status code. */
    char      *status_msg;  /* The SCSI text message. */
} hpuxscsi_StatusTable[] = {
    /* See /usr/include/sys/scsi.h for descriptions. */
    { S_GOOD,               "S_GOOD",		    /* 0x00 */},
    { S_CHECK_CONDITION,    "S_CHECK_CONDITION",    /* 0x02 */},
    { S_CONDITION_MET,      "S_CONDITION_MET",	    /* 0x04 */},
    { S_BUSY,               "S_BUSY",		    /* 0x08 */},
    { S_INTERMEDIATE,       "S_INTERMEDIATE",	    /* 0x10 */},
    { S_I_CONDITION_MET,    "S_I_CONDITION_MET",    /* 0x14 */},
    { S_RESV_CONFLICT,      "S_RESV_CONFLICT",	    /* 0x18 */},
    { S_COMMAND_TERMINATED, "S_COMMAND_TERMINATED", /* 0x22 */},
    { S_QUEUE_FULL,         "S_QUEUE_FULL",	    /* 0x28 */},
    /*
     * Additional SCSI status returned by HP-UX drivers.
     */
    { SCTL_INVALID_REQUEST, "SCTL_INVALID_REQUEST", /* 0x100 */},
    { SCTL_SELECT_TIMEOUT,  "SCTL_SELECT_TIMEOUT",  /* 0x200 */},
    { SCTL_INCOMPLETE,      "SCTL_INCOMPLETE",	    /* 0x400 */},
    { SCTL_POWERFAIL,       "SCTL_POWERFAIL",	    /* 0x800 */}
#if defined(UX1131)
    /* New for 11.31 */
    ,
    { SCTL_NO_RESOURCE,     "SCTL_NO_RESOURCE"	    /* 0x1000 */},
    { SCTL_TP_OFFLINE,      "SCTL_TP_OFFLINE"	    /* 0x2000 */},
    { SCTL_IO_TIMEOUT,      "SCTL_IO_TIMEOUT"	    /* 0x3000 */},
    { SCTL_IO_ABORTED,      "SCTL_IO_ABORTED"	    /* 0x4000 */},
    { SCTL_RESET_OCCURRED,  "SCTL_RESET_OCCURRED"   /* 0x5000 */}
#endif /* defined(UX1131) */
};
static int hpuxscsi_StatusEntrys =
sizeof(hpuxscsi_StatusTable) / sizeof(hpuxscsi_StatusTable[0]);

static char *
hpux_ScsiStatus(unsigned scsi_status)
{
    struct hpuxSCSI_StatusTable *cst = hpuxscsi_StatusTable;
    int entrys;

    for (entrys = 0; entrys < hpuxscsi_StatusEntrys; cst++, entrys++) {
	if (cst->scsi_status == scsi_status) {
	    return(cst->status_msg);
	}
    }
    return("???");
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
