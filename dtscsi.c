#if defined(SCSI)
/****************************************************************************
 *      								    *
 *      		  COPYRIGHT (c) 1988 - 2021     		    *
 *      		   This Software Provided       		    *
 *      			     By 				    *
 *      		  Robin's Nest Software Inc.    		    *
 *      								    *
 * Permission to use, copy, modify, distribute and sell this software and   *
 * its documentation for any purpose and without fee is hereby granted,     *
 * provided that the above copyright notice appear in all copies and that   *
 * both that copyright notice and this permission notice appear in the      *
 * supporting documentation, and that the name of the author not be used    *
 * in advertising or publicity pertaining to distribution of the software   *
 * without specific, written prior permission.  			    *
 *      								    *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,        *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN      *
 * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
 * THIS SOFTWARE.       						    *
 *      								    *
 ****************************************************************************/
/*
 * Module:      dtscsi.c
 * Author:      Robin T. Miller
 * Date:	July 15th, 2013
 *
 * Description:
 *      dt's SCSI functions.
 * 
 * Modification History:
 * 
 * October 28th, 2020 by Robin T. Miller
 *      When setting up SCSI information, only update user capacity for disks.
 *      The user capacity must only be setup for disk devices, not files!
 * 
 * May 19th, 2020 by Robin T. Miller
 *      For spt commands add "logprefix=" to disable the spt log prefix.
 *      The ExecuteCommand() API was updated to apply dt's log prefix.
 * 
 * January 25th, 2019 by Robin T. Miller
 *      Add support for unmapping blocks via token based xcopy zero ROD method.
 * 
 * January 23rd, 2019 by Robin T. Miller
 *      Change default path for spt to be /usr/local/bin, also drop ".last".
 * 
 * April 26th, 2018 by Robin T. Miller
 *      If the serial number contains spaces, copy serial number without spaces.
 *      Note: This is limited to HGST compile right now, where this is prevalent.
 * 
 * July 14th, 2014 by Robin T. Miller
 * 	Add support for SCSI I/O.
 *
 * June 5th, 2014 by Robin T. Miller
 * 	Add detection of MARS LUNs, PID == "LUN FlashRay", and set the
 * cluster mode flag accordingly. This flag is required when requesting
 * the GVA Volume Name, since the data returned differs for 7-mode/c-mode.
 */
#include "dt.h"

/*
 * Forward Reference:
 */ 
void add_common_spt_options(dinfo_t *dip, char *cmd);
void strip_trailing_spaces(char *bp);
ssize_t	scsiRequestSetup(dinfo_t *dip, scsi_generic_t *sgp, void *buffer,
			 size_t bytes, Offset_t offset, uint64_t *lba, uint32_t *blocks);

void
strip_trailing_spaces(char *bp)
{
    size_t i = strlen(bp);

    while (i) {
	if (bp[--i] == ' ') {
	    bp[i] = '\0';
	    continue;
	}
	break;
    }
    return;
}

void
clone_scsi_info(dinfo_t *dip, dinfo_t *cdip)
{
    /*
     * Note: We will only clone the information dt currently uses.
     *	     The assumption is the SCSI information already exists!
     */
    if (cdip->di_sgp) {
	int error = init_sg_info(cdip);
	/* Ignore any errors! */
    }
    if (dip->di_inquiry) {
	/* Note: We don't have a thread number, so clone for all! */
	cdip->di_inquiry = Malloc(dip, sizeof(*dip->di_inquiry));
	*cdip->di_inquiry = *dip->di_inquiry;
    }
    if (dip->di_vendor_id) {
	cdip->di_vendor_id = strdup(dip->di_vendor_id);
    }
    if (dip->di_product_id) {
	cdip->di_product_id = strdup(dip->di_product_id);
    }
    if (dip->di_revision_level) {
	cdip->di_revision_level = strdup(dip->di_revision_level);
    }
    if (dip->di_device_id) {
	cdip->di_device_id = strdup(dip->di_device_id);
    }
    if (dip->di_serial_number) {
	cdip->di_serial_number = strdup(dip->di_serial_number);
    }
    if (dip->di_scsi_dsf) {
	cdip->di_scsi_dsf = strdup(dip->di_scsi_dsf);
    }
    if (dip->di_spt_path) {
	cdip->di_spt_path = strdup(dip->di_spt_path);
    }
    if (dip->di_spt_options) {
	cdip->di_spt_options = strdup(dip->di_spt_options);
    }
    return;
}

void
free_scsi_info(dinfo_t *dip)
{
    if (dip->di_sgp) {
	scsi_generic_t *sgp = dip->di_sgp;
	if (sgp->fd != INVALID_HANDLE_VALUE) {
	    (void)os_close_device(sgp);
	}
	if (sgp->sense_data) {
	    free_palign(dip, sgp->sense_data);
	    sgp->sense_data = NULL;
	}
	Free(dip, sgp->dsf);
	Free(dip, dip->di_sgp);
	dip->di_sgp = NULL;
    }
    if (dip->di_inquiry) {
	Free(dip, dip->di_inquiry);
	dip->di_inquiry = NULL;
    }
    if (dip->di_vendor_id) {
	Free(dip, dip->di_vendor_id);
	dip->di_vendor_id = NULL;
    }
    if (dip->di_product_id) {
	Free(dip, dip->di_product_id);
	dip->di_product_id = NULL;
    }
    if (dip->di_revision_level) {
	Free(dip, dip->di_revision_level);
	dip->di_revision_level = NULL;
    }
    if (dip->di_device_id) {
	Free(dip, dip->di_device_id);
	dip->di_device_id = NULL;
    }
    if (dip->di_serial_number) {
	Free(dip, dip->di_serial_number);
	dip->di_serial_number = NULL;
    }
    if (dip->di_scsi_dsf) {
	Free(dip, dip->di_scsi_dsf);
	dip->di_scsi_dsf = NULL;
    }
    if (dip->di_spt_path) {
	Free(dip, dip->di_spt_path);
	dip->di_spt_path = NULL;
    }
    if (dip->di_spt_options) {
	Free(dip, dip->di_spt_options);
	dip->di_spt_options = NULL;
    }
    /* Free SCSI I/O data (if any). */
    if (dip->di_sgpio) {
	if (dip->di_sgpio->sense_data) {
	    free_palign(dip, dip->di_sgpio->sense_data);
	    Free(dip, dip->di_sgpio);
	    dip->di_sgpio = NULL;
	}
    }
    return;
}

int
init_sg_info(dinfo_t *dip)
{
    scsi_generic_t *sgp;
    scsi_addr_t *sap;
    int error;

    sgp = Malloc(dip, sizeof(*sgp) );
    if (sgp == NULL) return(FAILURE);

    sgp->opaque = dip;
    sgp->debug = dip->di_sDebugFlag;
    sgp->errlog = dip->di_scsi_errors;
    sap = &sgp->scsi_addr;
    /* Note: Only AIX uses this, but must be -1 for any path! */
    sap->scsi_path  = -1;	/* Indicates no path specified. */
    /*
     * Recovery (retry) Parameters:
     */ 
    if (dip->di_scsi_recovery == True) {
	sgp->recovery_flag  = dip->di_scsi_recovery;       // ScsiRecoveryFlagDefault;
	sgp->recovery_delay = dip->di_scsi_recovery_delay; // ScsiRecoveryDelayDefault;
	sgp->recovery_limit = dip->di_scsi_recovery_limit; // ScsiRecoveryRetriesDefault;
    }

    if (dip->di_scsi_dsf) {
	sgp->dsf = strdup(dip->di_scsi_dsf);
    } else {
	sgp->dsf = strdup(dip->di_dname);
    }
    error = os_open_device(sgp);
    if (error) {
	dip->di_scsi_flag = False;
	Free(dip, sgp->dsf);
	Free(dip, sgp);
	return(error);
    }
    sgp->qtag_type = SG_SIMPLE_Q;
    sgp->sense_length = RequestSenseDataLength;
    sgp->sense_data = malloc_palign(dip, sgp->sense_length, 0);
    if (sgp->sense_data == NULL) {
	dip->di_scsi_flag = False;
	Free(dip, sgp->dsf);
	Free(dip, sgp);
	return(FAILURE);
    }
    sgp->sense_flag = dip->di_scsi_sense;
    if (dip->di_scsi_io_flag == True) {
	/* Create a separate sgp for SCSI I/O. */
	scsi_generic_t *sgpio = Malloc(dip, sizeof(*sgp));
	if (sgpio == NULL) {
	    dip->di_scsi_io_flag = False;
	} else {
	    dip->di_sgpio = sgpio;
	    *sgpio = *sgp;
	    sgpio->errlog = True;
	    sgpio->sense_data = malloc_palign(dip, sgpio->sense_length, 0);
	    sgpio->warn_on_error = False;
	}
    } else {
	sgp->warn_on_error = True;
    }
    dip->di_sgp = sgp;
    return(error);
}

int
init_scsi_info(dinfo_t *dip)
{
    scsi_generic_t *sgp;
    int status;
    
    if ( (sgp = dip->di_sgp) == NULL) {
	status = init_sg_info(dip);
	if (status == FAILURE) {
	    return(status);
	}
	sgp = dip->di_sgp;
    }
    status = get_standard_scsi_information(dip, sgp);

    /* Note: We leave the SCSI device open on purpose, for faster triggers, etc. */
    //(void)os_close_device(sgp);
    /*
     * If we're doing SCSI I/O, we want to report errors.
     */
    if (dip->di_scsi_io_flag == True) {
	/* Note: SCSI I/O uses sgpio now!*/
	//sgp->errlog = True;
	//sgp->warn_on_error = False;
	dip->di_scsi_errors = True;
    }
    return(status);
}

int
get_standard_scsi_information(dinfo_t *dip, scsi_generic_t *sgp)
{
    inquiry_t *inquiry;
    int error;

    if ( (inquiry = dip->di_inquiry) == NULL) {
	dip->di_inquiry = inquiry = Malloc(dip, sizeof(*inquiry));
	if (dip->di_inquiry == NULL) {
	    (void)os_close_device(sgp);
	    dip->di_scsi_flag = False;
	    return(FAILURE);
	}
    }

    error = Inquiry(sgp->fd, sgp->dsf, dip->di_sDebugFlag, dip->di_scsi_errors,
		    NULL, &sgp, inquiry, sizeof(*inquiry), 0, 0, dip->di_scsi_timeout);
    if (error) {
	(void)os_close_device(sgp);
	dip->di_scsi_flag = False;	/* Assume *not* a SCSI device! */
	return(error);
    }
    dip->di_vendor_id = Malloc(dip, INQ_VID_LEN + 1);
    dip->di_product_id = Malloc(dip, INQ_PID_LEN + 1);
    dip->di_revision_level = Malloc(dip, INQ_REV_LEN + 1);
    (void)strncpy(dip->di_vendor_id, (char *)inquiry->inq_vid, INQ_VID_LEN);
    (void)strncpy(dip->di_product_id, (char *)inquiry->inq_pid, INQ_PID_LEN);
    (void)strncpy(dip->di_revision_level, (char *)inquiry->inq_revlevel, INQ_REV_LEN);
    /* Remove trailing spaces. */
    strip_trailing_spaces(dip->di_vendor_id);
    strip_trailing_spaces(dip->di_product_id);
    strip_trailing_spaces(dip->di_revision_level);

    error = GetCapacity(sgp, &dip->di_block_length, &dip->di_device_capacity,
			&dip->di_lbpme_flag, NULL, &dip->di_lbpmgmt_valid);
    if ( (error == SUCCESS) && isDiskDevice(dip) && (dip->di_user_capacity == 0) ) {
	/* Note: This is for FindCapacity() for slices and random I/O! */
	dip->di_user_capacity = (large_t)(dip->di_block_length * dip->di_device_capacity);
    }
    if (dip->di_idt == IDT_DEVICEID) {
	dip->di_device_id  = GetDeviceIdentifier(sgp->fd, sgp->dsf, dip->di_sDebugFlag, dip->di_scsi_errors,
						 NULL, &sgp, inquiry, dip->di_scsi_timeout);
    } else if (dip->di_idt == IDT_SERIALID) {
	dip->di_serial_number = GetSerialNumber(sgp->fd, sgp->dsf, dip->di_sDebugFlag, dip->di_scsi_errors,
						NULL, &sgp, inquiry, dip->di_scsi_timeout);
    } else if (dip->di_idt == IDT_BOTHIDS) {
	dip->di_device_id  = GetDeviceIdentifier(sgp->fd, sgp->dsf, dip->di_sDebugFlag, dip->di_scsi_errors,
						 NULL, &sgp, inquiry, dip->di_scsi_timeout);
	dip->di_serial_number = GetSerialNumber(sgp->fd, sgp->dsf, dip->di_sDebugFlag, dip->di_scsi_errors,
						NULL, &sgp, inquiry, dip->di_scsi_timeout);
    /* TODO: Move this to HGST specific function! */
#if defined(HGST)
	/*
	 * Some vendors like HGST and Sandisk right justify their serial number and pad with spaces. 
	 * Since this causes the btag serial numbers to be truncated, I'm stripping spaces here!
	 * Note: This is because I don't wish to expand btag serial number just for extra spaces.
	 */
	if ( dip->di_serial_number && strchr(dip->di_serial_number, ' ') ) {
	    char serial[MEDIUM_BUFFER_SIZE];
	    char *sbp = serial;
	    char *snp = dip->di_serial_number;
	    memset(serial, '\0', sizeof(serial));
	    /* Copy the serial number without the goofy spaces! */
	    while (*snp) {
		if (*snp != ' ') {
		    *sbp = *snp;
		    sbp++;
		}
		snp++;
	    }
	    Free(dip, dip->di_serial_number);
	    dip->di_serial_number = strdup(serial);
	}
#endif /* defined(HGST) */
    }
    if ( NEL(dip->di_vendor_id, "HGST", 4) ) {
	dip->di_mgmt_address = GetMgmtNetworkAddress(sgp->fd, sgp->dsf, dip->di_sDebugFlag, dip->di_scsi_errors,
						     NULL, &sgp, inquiry, dip->di_scsi_timeout);
    }
    return(error);
}

void
report_scsi_information(dinfo_t *dip)
{
    if (dip->di_scsi_info_flag == False) return;

    report_standard_scsi_information(dip);
    return;
}

void
report_standard_scsi_information(dinfo_t *dip)
{
    Lprintf(dip, "\nSCSI Information:\n");

    Lprintf (dip, DT_FIELD_WIDTH "%s\n",
	     "SCSI Device Name", dip->di_sgp->dsf);
    if (dip->di_vendor_id) {
	Lprintf (dip, DT_FIELD_WIDTH "%s\n", "Vendor Identification", dip->di_vendor_id);
    }
    if (dip->di_product_id) {
	Lprintf (dip, DT_FIELD_WIDTH "%s\n", "Product Identification", dip->di_product_id);
    }
    if (dip->di_revision_level) {
	Lprintf (dip, DT_FIELD_WIDTH "%s\n", "Firmware Revision Level", dip->di_revision_level);
    }
    /* Also report the ALUA state, imperative for proper MPIO operation! */
    if (dip->di_inquiry) {
	char *alua_str;
	switch (dip->di_inquiry->inq_tpgs) {
	    case 0:
		alua_str = "ALUA not supported";
		break;
	    case 1:
		alua_str = "implicit ALUA";
		break;
	    case 2:
		alua_str = "explicit ALUA";
		break;
	    case 3:
		alua_str = "explicit & implicit ALUA";
		break;
	}
	Lprintf (dip, DT_FIELD_WIDTH "%u (%s)\n",
		 "Target Port Group Support",
		 dip->di_inquiry->inq_tpgs, alua_str);
    }
    if (dip->di_device_capacity) {
        large_t data_bytes = (dip->di_device_capacity * dip->di_block_length);
	double Mbytes = (double) ( (double)data_bytes / (double)MBYTE_SIZE);
	double Gbytes = (double) ( (double)data_bytes / (double)GBYTE_SIZE);
	Lprintf (dip, DT_FIELD_WIDTH "%u\n", "Block Length", dip->di_block_length);
	Lprintf (dip, DT_FIELD_WIDTH LUF " (%.3f Mbytes, %.3f Gbytes)\n",
		 "Maximum Capacity", dip->di_device_capacity, Mbytes, Gbytes);
	/* Note: This information comes from Read Capacity(16). */
	if (dip->di_lbpmgmt_valid == True) {
	    Lprintf (dip, DT_FIELD_WIDTH "%s Provisioned\n",
		   "Provisioning Management", (dip->di_lbpme_flag) ? "Thin" : "Full");
	}
    }
    if (dip->di_device_id) {
	Lprintf (dip, DT_FIELD_WIDTH "%s\n", "Device Identifier", dip->di_device_id);
    }
    if (dip->di_serial_number) {
	Lprintf (dip, DT_FIELD_WIDTH "%s\n", "Device Serial Number", dip->di_serial_number);
    }
    if (dip->di_mgmt_address) {
	Lprintf (dip, DT_FIELD_WIDTH "%s\n", "Management Network Address", dip->di_mgmt_address);
    }
    /* Note: We flush here incase we have multiple devices, thus different log buffers! */
    Lprintf (dip, "\n");
    Lflush(dip);
    return;
}

/*
 * NOTE: This is temporary, until we merge spt functions into dt!
 */ 
#if defined(WIN32)

char *spt_path = "C:\\tools\\spt.exe";

#else /* !defined(WIN32) */

char *spt_path = "/usr/local/bin/spt";

#endif /* defined(WIN32) */

int
get_lba_status(dinfo_t *dip, Offset_t starting_offset, large_t data_bytes)
{
    char cmd[STRING_BUFFER_SIZE];
    uint32_t block_length = dip->di_block_length;
    int status;
    
    if (block_length == 0) block_length = BLOCK_SIZE;
    /* Note: The starting block and limit is converted to SCSI sized blocks! */
    /* Also Note: Beware of using single quotes ('), Windows shell does NOT strip them! ;( */
    (void)sprintf(cmd, "%s dsf=%s cdb=\"9e 12\" starting="FUF" limit="FUF"b enable=sense,recovery",
		  (dip->di_spt_path) ? dip->di_spt_path : spt_path,
		  dip->di_sgp->dsf,
		  (starting_offset / block_length),
		  (data_bytes / block_length) );

    add_common_spt_options(dip, cmd);

    status = ExecuteCommand(dip, cmd, LogPrefixEnable, True);

    if (status || dip->di_debug_flag) {
	Printf(dip, "spt exited with status %d...\n", status);
    }
    return (status);
}

void
add_common_spt_options(dinfo_t *dip, char *cmd)
{
    if (dip->di_spt_options) {
	strcat(cmd, " ");
	strcat(cmd, dip->di_spt_options);
    }
    strcat(cmd, " logprefix=");
    if (dip->di_sDebugFlag) {
	strcat(cmd, " enable=Debug,xdebug,debug");
    }
    return;
}

int
do_unmap_blocks(dinfo_t *dip)
{
    large_t data_bytes;
    Offset_t offset;
    unmap_type_t unmap_type = dip->di_unmap_type;
    int status = SUCCESS;

    (void)get_transfer_limits(dip, &data_bytes, &offset);

    /* We display the LBA status both before and after Unmap operation! */
    if (dip->di_get_lba_status_flag == True) {
	status = get_lba_status(dip, offset, data_bytes);
    }
    if (unmap_type == UNMAP_TYPE_NONE) {
	dip->di_unmap_type = UNMAP_TYPE_UNMAP;
    } else if (unmap_type == UNMAP_TYPE_RANDOM) {
	unmap_type = ( rand() % NUM_UNMAP_TYPES );
    }

    switch (unmap_type) {
	case UNMAP_TYPE_UNMAP:
	    status = unmap_blocks(dip, offset, data_bytes);
	    break;
	case UNMAP_TYPE_WRITE_SAME:
	    status = write_same_unmap(dip, offset, data_bytes);
	    break;
	case UNMAP_TYPE_ZEROROD:
	    status = xcopy_zerorod(dip, offset, data_bytes);
	    break;
    }
    if ( (dip->di_get_lba_status_flag == True) && (status == SUCCESS) ) {
	status = get_lba_status(dip, offset, data_bytes);
    }
    /* SCSI commands are via spt so it must be, map failure accordingly. */
    if (status == 255) status = FAILURE;
    return(status);
}

int
unmap_blocks(dinfo_t *dip, Offset_t starting_offset, large_t data_bytes)
{
    char cmd[STRING_BUFFER_SIZE];
    uint32_t block_length = dip->di_block_length;
    int status;
    
    if (block_length == 0) block_length = BLOCK_SIZE;
    /* Note: The starting block and limit is converted to SCSI sized blocks! */
    (void)sprintf(cmd, "%s dsf=%s cdb=0x42 starting="FUF" limit="FUF"b enable=sense,recovery",
		  (dip->di_spt_path) ? dip->di_spt_path : spt_path,
		  dip->di_sgp->dsf,
		  (starting_offset / block_length),
		  (data_bytes / block_length) );

    add_common_spt_options(dip, cmd);

    status = ExecuteCommand(dip, cmd, LogPrefixEnable, True);

    if (status || dip->di_debug_flag) {
	Printf(dip, "spt exited with status %d...\n", status);
    }
    return (status);
}

#define WRITE_SAME_MAX_BYTES	(4 * MBYTE_SIZE)

int
write_same_unmap(dinfo_t *dip, Offset_t starting_offset, large_t data_bytes)
{
    char cmd[STRING_BUFFER_SIZE];
    uint32_t block_length = dip->di_block_length;
    int status;
    
    if (block_length == 0) block_length = BLOCK_SIZE;
    /* Note: The starting block and limit is converted to SCSI sized blocks! */
    (void)sprintf(cmd, "%s dsf=%s cdb=\"93 08\" starting="FUF" blocks=%u limit="FUF"b enable=sense,recovery",
		  (dip->di_spt_path) ? dip->di_spt_path : spt_path,
		  dip->di_sgp->dsf,
		  (starting_offset / block_length),
		  (uint32_t)(WRITE_SAME_MAX_BYTES / block_length),
		  (data_bytes / block_length) );

    add_common_spt_options(dip, cmd);

    status = ExecuteCommand(dip, cmd, LogPrefixEnable, True);

    if (status || dip->di_debug_flag) {
	Printf(dip, "spt exited with status %d...\n", status);
    }
    return (status);
}

int
xcopy_zerorod(dinfo_t *dip, Offset_t starting_offset, large_t data_bytes)
{
    char cmd[STRING_BUFFER_SIZE];
    uint32_t block_length = dip->di_block_length;
    int status;
    
    if (block_length == 0) block_length = BLOCK_SIZE;
    /* Note: The starting block and limit is converted to SCSI sized blocks! */
    (void)sprintf(cmd, "%s dsf=%s cdb=\"83 11\" starting="FUF" limit="FUF"b enable=sense,recovery,zerorod",
		  (dip->di_spt_path) ? dip->di_spt_path : spt_path,
		  dip->di_sgp->dsf,
		  (starting_offset / block_length),
		  (data_bytes / block_length) );

    add_common_spt_options(dip, cmd);

    status = ExecuteCommand(dip, cmd, LogPrefixEnable, True);

    if (status || dip->di_debug_flag) {
	Printf(dip, "spt exited with status %d...\n", status);
    }
    return (status);
}

int
do_scsi_triage(dinfo_t *dip)
{
    scsi_generic_t *sgp = dip->di_sgp;
    inquiry_t *inquiry;
    int status = SUCCESS, retries = 0;

    if (dip->di_scsi_flag == False) return(FAILURE);
    /* Note: We assume the SCSI device is already open! */
    if ( (sgp == NULL) || (sgp->fd == NoFd) ) {
	Printf(dip, "The SCSI device does *not* exist or is not open, so no triage!\n");
	return(FAILURE);
    }
    Printf(dip, "Performing SCSI triage on device %s...\n", sgp->dsf);

    sgp->errlog = True;
    sgp->warn_on_error = False;
    dip->di_scsi_errors = True;

    /* Note: SCSI recovery is enabled by default, so requests will be retried! */
 
    /* Note: We don't have an Inquiry buffer per thread. */
    inquiry = Malloc(dip, sizeof(*inquiry));
    if (inquiry == NULL) return(FAILURE);

    status = Inquiry(sgp->fd, sgp->dsf, dip->di_sDebugFlag, dip->di_scsi_errors,
		     NULL, &sgp, inquiry, sizeof(*inquiry), 0, 0, dip->di_scsi_timeout);
    Free(dip, inquiry);
    if (status == SUCCESS) {
	Printf(dip, "SCSI Inquiry succeeded on %s...\n", sgp->dsf);
    } else {
	return(status);
    }

    status = TestUnitReady(sgp->fd, sgp->dsf, dip->di_sDebugFlag, dip->di_scsi_errors,
			   NULL, &sgp, dip->di_scsi_timeout);
    if (status == SUCCESS) {
	Printf(dip, "SCSI Test Unit Ready succeeded on %s...\n", sgp->dsf);
    }
    /* TODO: Is there anything else we should do here? */
    return(status);
}

ssize_t
scsiRequestSetup(dinfo_t *dip, scsi_generic_t *sgp, void *buffer,
		 size_t bytes, Offset_t offset, uint64_t *lba, uint32_t *blocks)
{
    uint32_t block_length;
    ssize_t iosize = bytes;

    /* Note: This should be set by Read Caoacity when init'ing SCSI! */
    block_length = dip->di_block_length;
    if (block_length == 0) block_length = BLOCK_SIZE;
    /* 
     * Note: This should *never* happen if sanity checks are correct!
     * We cannot set a OS error easily, so for now report errors here.
     */
    if (bytes % block_length) {
	if (sgp->errlog == True) {
	    Eprintf(dip, "The SCSI I/O size of "SDF" bytes, is NOT modulo the block length of %u bytes!\n",
		    bytes, block_length);
	}
	os_set_error(OS_ERROR_INVALID);
	return( (ssize_t)FAILURE );
    } else if (offset % block_length) {
	if (sgp->errlog == True) {
	    Eprintf(dip, "The SCSI I/O offset "FUF", is NOT modulo the block length of %u bytes!\n",
		    offset, block_length);
	}
	os_set_error(OS_ERROR_INVALID);
	return( (ssize_t)FAILURE );
    }
    *lba = (offset / block_length);
    *blocks = (uint32_t)(bytes / block_length);
    sgp->data_buffer = buffer;
    /*
     * Adjust the length, as required, to avoid errors.
     */
    if ( (*lba + *blocks) > dip->di_device_capacity) {
	*blocks = (uint32_t)(dip->di_device_capacity - *lba);
	if (*blocks < dip->di_device_capacity) {
	    iosize = (*blocks * block_length);
	} else {
	    iosize = 0;	/* Too far, nothing transferred! */
	}
    }
    /* Pass these via sgp to opcodes supported this. */
    sgp->fua = dip->di_fua;
    sgp->dpo = dip->di_dpo;
    return(iosize);
}

/* 
 * Note: These API parameters match the Unix pread/pwrite API's! (except for dip)
 */
ssize_t
scsiReadData(dinfo_t *dip, void *buffer, size_t bytes, Offset_t offset)
{
    scsi_generic_t *sgp = dip->di_sgpio;
    uint64_t lba;
    uint32_t blocks;
    ssize_t iosize;
    int status;
    
    iosize = scsiRequestSetup(dip, sgp, buffer, bytes, offset, &lba, &blocks);
    if (iosize <= 0) return(iosize);

    status = ReadData(dip->di_scsi_read_type, sgp, lba, blocks, (uint32_t)iosize);
    if (status == SUCCESS) {
	return( (ssize_t)iosize);
    } else {
	return( (ssize_t)status );
    }
}

ssize_t
scsiWriteData(dinfo_t *dip, void *buffer, size_t bytes, Offset_t offset)
{
    scsi_generic_t *sgp = dip->di_sgpio;
    uint64_t lba;
    uint32_t blocks;
    ssize_t iosize;
    int status;

    iosize = scsiRequestSetup(dip, sgp, buffer, bytes, offset, &lba, &blocks);
    if (iosize == 0) os_set_error(OS_ERROR_DISK_FULL);
    if (iosize <= 0) return(iosize);

    status = WriteData(dip->di_scsi_write_type, sgp, lba, blocks, (uint32_t)iosize);
    if (status == SUCCESS) {
	return( (ssize_t)iosize);
    } else {
	return( (ssize_t)status );
    }
}

/*
 * Note: This code scarf'ed from libscsi.c libReportScsiError().
 * This format is required for the new extended error reporting.
 */
void
dtReportScsiError(dinfo_t *dip, scsi_generic_t *sgp)
{
    char *host_msg = os_host_status_msg(sgp);
    char *driver_msg = os_driver_status_msg(sgp);
    scsi_sense_t *ssp = sgp->sense_data;
    char *ascq_msg = ScsiAscqMsg(ssp->asc, ssp->asq);
    char buf[LARGE_BUFFER_SIZE];
    char *bp = buf;
    int i;

    PrintHeader(dip, "SCSI Error Information");
    PrintAscii(dip, "Device Name", sgp->dsf, PNL);
    *bp = '\0';
    for (i = 0; (i < sgp->cdb_size); i++) {
	bp += Sprintf(bp, "%02x ", sgp->cdb[i]);
    }
    PrintAscii(dip, "SCSI Operation", sgp->cdb_name, DNL);
    Lprintf(dip, " = %s\n", buf);
    PrintHex(dip, "SCSI Status", sgp->scsi_status, DNL);
    Lprintf(dip, " = %s\n", ScsiStatus(sgp->scsi_status));
    if (host_msg) {
	PrintHex(dip, "Host Status", sgp->host_status, DNL);
	Lprintf(dip, " = %s\n", host_msg);
    } else if (sgp->host_status) {
	PrintHex(dip, "Host Status", sgp->host_status, PNL);
    }
    if (driver_msg) {
	PrintHex(dip, "Driver Status", sgp->driver_status, DNL);
	Lprintf(dip, " = %s\n", driver_msg);
    } else if (sgp->driver_status) {
	PrintHex(dip, "Driver Status", sgp->driver_status, PNL);
    }
    PrintDecimal(dip, "Sense Key", ssp->sense_key, DNL);
    Lprintf(dip, " = %s\n", SenseKeyMsg(ssp->sense_key));
    PrintAscii(dip, "Sense Code/Qualifier", "", DNL);
    Lprintf(dip, "(%#x, %#x)", ssp->asc, ssp->asq);
    if (ascq_msg) {
      Lprintf(dip, " = %s", ascq_msg);
    }
    Lprintf(dip, "\n");
    return;
}

#endif /* defined(SCSI) */
