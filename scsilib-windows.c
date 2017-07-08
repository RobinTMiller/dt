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
 * Module:	scsilib-windows.c
 * Author:	Robin T. Miller
 * Date:	March 29th, 2005
 *
 * Description:
 *  This module contains the OS SCSI specific functions for Windows.
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
#include <stddef.h>
#include <fcntl.h>
#include <sys/types.h>

#include "dt.h"
//#include "libscsi.h"

/*
 * Local Definitions:
 */

#if !defined(SCSILIB_H)
#define SCSILIB_H

/*
 * Note: The best I can recall, this stuff is inline to avoid requiring the DDK include files!
 * 	 These definitions come from: NTDDK/inc/basetsd.h (only place I find them!)
 */
#if defined(_WIN64)
    typedef __int64 INT_PTR, *PINT_PTR;
    typedef unsigned __int64 UINT_PTR, *PUINT_PTR;

    typedef __int64 LONG_PTR, *PLONG_PTR;
    typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;

    #define __int3264   __int64

#else /* !defined(_WIN64) */
    typedef _W64 int INT_PTR, *PINT_PTR;
    typedef _W64 unsigned int UINT_PTR, *PUINT_PTR;

    typedef _W64 long LONG_PTR, *PLONG_PTR;
    typedef _W64 unsigned long ULONG_PTR, *PULONG_PTR;

    #define __int3264   __int32

#endif /* defined(_WIN64) */

#include <winioctl.h>
#include <ntddscsi.h>

typedef struct _SPTDWB {
    SCSI_PASS_THROUGH_DIRECT spt;
    ULONG filler;
    UCHAR senseBuf[256]; 
} SPTWB, *PSPTWB;  

#endif /* !defined(SCSILIB_H) */

/*
 * Forward Declarations:
 */

static void DumpScsiCmd(scsi_generic_t *sgp, SPTWB sptwb);

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
    char *wdsf = sgp->dsf;
    hbool_t wdsf_allocated = False;

    /*
     * Automatically add the hidden device directory (for ease of use).
     */ 
    if (strncmp(sgp->dsf, DEV_DIR_PREFIX, DEV_DIR_LEN) != 0) {
	wdsf = Malloc(sgp->opaque, DEV_DEVICE_LEN);
	if (wdsf) {
	    wdsf_allocated = True;
	    (void)sprintf(wdsf, "%s%s", DEV_DIR_PREFIX, sgp->dsf);
	} else {
	    wdsf = sgp->dsf;
	}
    }
    if (sgp->debug == True) {
	Printf(sgp->opaque, "Opening device %s...\n", wdsf);
    }
    sgp->fd = CreateFile(wdsf, (GENERIC_READ | GENERIC_WRITE),
			 (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL,
			 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    /*
     * If write protected, try open'ing read-only. 
     */
    if ( (sgp->fd == INVALID_HANDLE_VALUE) &&
	 (GetLastError() == ERROR_WRITE_PROTECT) ) {
	if (sgp->debug == True) {
	    Printf(sgp->opaque, "Opening device %s read-only...\n", wdsf);
	}
	sgp->fd = CreateFile(sgp->dsf, GENERIC_READ, FILE_SHARE_READ, NULL,
			     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (sgp->fd == INVALID_HANDLE_VALUE) {
	if (sgp->errlog == True) {
	    os_perror(sgp->opaque, "CreateFile() failed on %s", wdsf);
	}
	status = FAILURE;
    }
    if ( (sgp->debug == True) && (sgp->fd != INVALID_HANDLE_VALUE) ) {
	Printf(sgp->opaque, "Device %s successfully opened, handle = %d\n", wdsf, sgp->fd);
    }
    if (wdsf_allocated) free(wdsf);
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
	Printf(sgp->opaque, "Closing device %s, handle = %d...\n", sgp->dsf, sgp->fd);
    }
    if ( (error = CloseHandle(sgp->fd)) == 0) {
	os_perror(sgp->opaque, "CloseHandle() failed on %s", sgp->dsf);
	error = -1;
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
    int error = SUCCESS;
    STORAGE_BUS_RESET_REQUEST sbr;
    DWORD bc; 

    sbr.PathId = sgp->scsi_addr.scsi_bus;
    if (DeviceIoControl(sgp->fd,
			IOCTL_STORAGE_RESET_BUS,
			&sbr,
			sizeof(sbr),
			NULL,
			0,
			&bc,
			NULL) == 0) {
	error = FAILURE; 
	sgp->os_error = GetLastError();
	if (sgp->errlog == True) {
	    os_perror(sgp->opaque, "SCSI reset bus (IOCTL_STORAGE_RESET_BUS) failed on %s", sgp->dsf);
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

    if (sgp->errlog == True) {
	Printf(sgp->opaque, "SCSI reset device is not implemented!\n");
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
 *    This function takes a high level SCSI command, converts it
 * into the format necessary for this OS, then executes it and
 * returns an OS independent format to the caller.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the SCSI request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_spt(scsi_generic_t *sgp)
{
    int error = SUCCESS;
    SPTWB sptwb;
    PSCSI_PASS_THROUGH_DIRECT pspt = &sptwb.spt;
    DWORD bc;
    int i;

    memset(&sptwb, 0, sizeof(sptwb));
    pspt->Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    pspt->PathId = sgp->scsi_addr.scsi_bus;
    pspt->TargetId = sgp->scsi_addr.scsi_target;
    pspt->Lun = sgp->scsi_addr.scsi_lun;

    /*
     * Timeout passed by the calling function is always in milliseconds.
     */
    pspt->TimeOutValue = (sgp->timeout / MSECS);

    /*
     * Set up the direction:
     */
    if (sgp->data_dir == scsi_data_none)
	pspt->DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;  /* Direction of data transfer unspecified */
    else if (sgp->data_dir == scsi_data_read)
	pspt->DataIn = SCSI_IOCTL_DATA_IN;	     /* Read data from the device */
    else if (sgp->data_dir == scsi_data_write)
	pspt->DataIn = SCSI_IOCTL_DATA_OUT;	     /* Write data to the device */

    pspt->DataTransferLength = sgp->data_length;
    pspt->DataBuffer = sgp->data_buffer;
    pspt->SenseInfoLength = sgp->sense_length;
    pspt->SenseInfoOffset = offsetof(SPTWB, senseBuf);

    pspt->CdbLength = sgp->cdb_size;
    for (i=0; i<sgp->cdb_size; i++) {
	pspt->Cdb[i] = sgp->cdb[i];
    }

    /*
     * Finally execute the SCSI command:
     */
    if (DeviceIoControl(sgp->fd,
			IOCTL_SCSI_PASS_THROUGH_DIRECT,
			&sptwb,
			sizeof(SPTWB),
			&sptwb,
			sizeof(SPTWB),
			&bc,
			NULL) == 0) {
	error = FAILURE;
    }

    /*
     * Handle errors, and send pertinent data back to caller.
     */
    if (error < 0) {
	sgp->os_error = GetLastError();
	if (sgp->errlog == True) {
	    os_perror(sgp->opaque, "Scsi Request IOCTL_SCSI_PASS_THROUGH_DIRECT failed on %s", sgp->dsf);
	}
	sgp->error = True;
	goto error;
    }

    if (pspt->ScsiStatus == SCSI_GOOD) {
	sgp->error = False;	  /* Show SCSI command was successful. */
    } else {
	sgp->error = True;	  /* Tell caller we've had some sort of error */ 
	if ( (sgp->errlog == True) && (pspt->ScsiStatus != SCSI_CHECK_CONDITION)) {
	    Fprintf(sgp->opaque, "%s failed, SCSI Status = %d (%s)\n",sgp->cdb_name, 
		    pspt->ScsiStatus, ScsiStatus(pspt->ScsiStatus));
	}
    }

    if ( pspt->ScsiStatus == SCSI_CHECK_CONDITION ) {
	sgp->sense_valid = True;
	(void)memcpy(sgp->sense_data, &sptwb.senseBuf, pspt->SenseInfoLength);
    }

    sgp->scsi_status = pspt->ScsiStatus;
    sgp->data_resid = (sgp->data_length - pspt->DataTransferLength);     
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
error:
    if (sgp->debug == True) {
	DumpScsiCmd(sgp, sptwb);
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
    
    if (sgp->os_error == ERROR_DEV_NOT_EXIST) {
	//
	// MessageId: ERROR_DEV_NOT_EXIST
	//
	// MessageText:
	//
	//  The specified network resource or device is no longer available.
	//
	// #define ERROR_DEV_NOT_EXIST              55L    // dderror
	//
	if (sgp->debug == True) {
	    Printf(sgp->opaque, "DEBUG: ERROR_DEV_NOT_EXIST detected on %s...\n", sgp->cdb_name);
	}
	is_retriable = True;
    } else if (sgp->os_error == ERROR_IO_DEVICE) {
	//
	// MessageId: ERROR_IO_DEVICE
	//
	// MessageText:
	//
	//  The request could not be performed because of an I/O device error.
	//
	// #define ERROR_IO_DEVICE                  1117L
	//
	if (sgp->debug == True) {
	    Printf(sgp->opaque, "DEBUG: ERROR_IO_DEVICE detected on %s...\n", sgp->cdb_name);
	}
	is_retriable = True;
    } else if (sgp->os_error == ERROR_DEVICE_NOT_CONNECTED) {
	//
	// MessageId: ERROR_DEVICE_NOT_CONNECTED
	//
	// MessageText:
	//
	//  The device is not connected.
	//
	// #define ERROR_DEVICE_NOT_CONNECTED       1167L
	//
	if (sgp->debug == True) {
	    Printf(sgp->opaque, "DEBUG: ERROR_DEVICE_NOT_CONNECTED detected on %s...\n", sgp->cdb_name);
	}
	is_retriable = True;
	
    } else if (sgp->os_error == ERROR_NO_SYSTEM_RESOURCES) {
	//
	// This error is occurring intermittently, so we will retry since we
	// believe this is a transient error. (resources should become available)
	// 
	// MessageId: ERROR_NO_SYSTEM_RESOURCES
	//
	// MessageText:
	//
	//  Insufficient system resources exist to complete the requested service.
	//
	// #define ERROR_NO_SYSTEM_RESOURCES        1450L
	//
	if (sgp->debug == True) {
	    Printf(sgp->opaque, "DEBUG: ERROR_NO_SYSTEM_RESOURCES detected on %s...\n", sgp->cdb_name);
	}
	is_retriable = True;
    }
    return (is_retriable);
}

static void
DumpScsiCmd(scsi_generic_t *sgp, SPTWB sptwb)
{
    int i;
    char buf[128];
    char *bp = buf;
    char *msgp = NULL;
    PSCSI_PASS_THROUGH_DIRECT pspt = &sptwb.spt; 

    Printf(sgp->opaque, "SCSI I/O Structure\n");

    Printf(sgp->opaque, "    Device Special File .............................: %s\n", sgp->dsf);
    Printf(sgp->opaque, "    File Descriptor .............................. fd: %d\n", sgp->fd);

    if (pspt->DataIn == SCSI_IOCTL_DATA_IN)
	msgp = "SCSI_IOCTL_DATA_IN";
    else if (pspt->DataIn == SCSI_IOCTL_DATA_OUT)
	msgp = "SCSI_IOCTL_DATA_OUT";
    else if (pspt->DataIn == SCSI_IOCTL_DATA_UNSPECIFIED)
	msgp  = "SCSI_IOCTL_DATA_UNSPECIFIED";

    Printf(sgp->opaque, "    Data Direction ........................... DataIn: %#x (%s)\n", pspt->DataIn,
	   msgp);
    Printf(sgp->opaque, "    SCSI CDB Status ...................... ScsiStatus: %#x (%s)\n", pspt->ScsiStatus,
	   ScsiStatus(pspt->ScsiStatus));  
    Printf(sgp->opaque, "    Command Timeout .................... TimeOutValue: %lu\n", pspt->TimeOutValue);

    for (i = 0; (i < pspt->CdbLength); i++) {
	bp += sprintf(bp, "%x ", pspt->Cdb[i]);
    }
    Printf(sgp->opaque, "    Command Descriptor Block .................... Cdb: %s (%s)\n",buf, sgp->cdb_name);
    Printf(sgp->opaque, "    I/O Buffer .............................. dataBuf: 0x%p\n", sgp->data_buffer);
    Printf(sgp->opaque, "    I/O Buffer Length ................... data_length: %u\n", pspt->DataTransferLength);     
    Printf(sgp->opaque, "    Request Sense Buffer ................... senseBuf: 0x%p\n", sptwb.senseBuf);
    Printf(sgp->opaque, "    Request Sense Length ............... sense_length: %u\n", pspt->SenseInfoLength);
    /* Note: Windows SPT alters DataTransferLength to be the bytes actually transferred. */
    Printf(sgp->opaque, "    Requested Data Length .......... sgp->data_length: %u\n", sgp->data_length);
    Printf(sgp->opaque, "    Residual Data Length ............ sgp->data_resid: %u\n", sgp->data_resid);
    Printf(sgp->opaque, "    Data Bytes Transferred .... sgp->data_transferred: %u\n", sgp->data_transferred);
    Printf(sgp->opaque, "\n");
    return;
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
