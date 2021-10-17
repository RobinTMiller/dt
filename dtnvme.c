#if defined(NVME) && defined(__linux__)

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
 * Module:      dtnvme.c
 * Author:      Robin T. Miller
 * Date:	July 30th, 2021
 *
 * Description:
 *      dt's NMVe functions.
 * 
 * Modification History:
 * 
 * August 5th, 2021 by Robin T. Miller
 * 	Initial creation for Linux NVMe support.
 */
#include "dt.h"
#include "nvme_lib.h"

int
init_nvme_info(dinfo_t *dip, char *dsf)
{
    struct nvme_id_ctrl ctrl;
    struct nvme_id_ctrl *ctrlp = &ctrl;
    HANDLE fd;
    int status;

    memset(ctrlp, '\0', sizeof(*ctrlp));
    if (dsf == NULL) {
	dsf = (dip->di_scsi_dsf) ? dip->di_scsi_dsf : dip->di_dname;
    }
    fd = open(dsf, O_RDONLY);
    if (fd == INVALID_HANDLE_VALUE) {
	if (dip->di_scsi_errors) {
	    os_perror(dip, "open() of %s failed!", dsf);
	}
	dip->di_nvme_io_flag = False;
	return(FAILURE);
    }

    status = get_nvme_id_ctrl(dip, fd);
    if (status == SUCCESS) {
	dip->di_nvme_flag = True;
	dip->di_scsi_flag = False;
	status = get_nvme_namespace(dip, fd);
    } else {
	dip->di_nvme_io_flag = False;
    }
    (void)close(fd);
    return (status);
}

int
get_nvme_id_ctrl(dinfo_t *dip, int fd)
{
    struct nvme_id_ctrl ctrl;
    struct nvme_id_ctrl *ctrlp = &ctrl;
    char *op = "NVMe Identify Controller";
    int status;

    if (dip->di_debug_flag) {
	Printf(dip, "Issuing %s...\n", op);
    }
    status = nvme_identify_ctrl(fd, ctrlp);

    if (status == SUCCESS) {
	char subnqn_buf[sizeof(ctrlp->subnqn) + 1];
	char *subnqn = subnqn_buf;

	/* Reuse SCSI fields, where it makes sense! */
	dip->di_product_id = Malloc(dip, sizeof(ctrlp->mn) + 1);
	dip->di_serial_number = Malloc(dip, sizeof(ctrlp->sn) + 1);
	dip->di_revision_level = Malloc(dip, sizeof(ctrlp->fr) + 1);
	(void)strncpy(dip->di_product_id, ctrlp->mn, sizeof(ctrlp->mn));
	(void)strncpy(dip->di_serial_number, ctrlp->sn, sizeof(ctrlp->sn));
	(void)strncpy(dip->di_revision_level, ctrlp->fr, sizeof(ctrlp->fr));
	/* Remove trailing spaces. */
	strip_trailing_spaces(dip->di_product_id);
	strip_trailing_spaces(dip->di_serial_number);
	strip_trailing_spaces(dip->di_revision_level);
	/* Note: Not sure if these are relavant yet, zero for 3PAR! */
	dip->di_total_nvm_capacity = int128_to_double(ctrlp->tnvmcap);
	dip->di_unalloc_nvm_capacity = int128_to_double(ctrlp->unvmcap);

	memset(subnqn_buf, '\0', sizeof(subnqn_buf));
	(void)strncpy(subnqn_buf, ctrlp->subnqn, sizeof(ctrlp->subnqn));
	strip_trailing_spaces(subnqn_buf);
	dip->di_nvm_subsystem_nqn = strdup(subnqn_buf);
    } else {
	if (dip->di_debug_flag) {
	    if (status > 0) {
		dt_nvme_show_status(dip, op, status);
	    } else {
		Perror(dip, "%s failed", op);
	    }
	}
	status = FAILURE;
    }
    return(status);
}

int
get_nvme_namespace(dinfo_t *dip, int fd)
{
    struct nvme_id_ns id_ns;
    struct nvme_id_ns *nsp = &id_ns;
    int status;
    __u32 namespace_id;
    int force = 0;
    char *op = "Identify Namespace";

    if (dip->di_debug_flag) {
	Printf(dip, "Requesting Namespace ID...\n");
    }
    status = dip->di_namespace_id = nvme_get_nsid(fd);
    if (status == FAILURE) {
	Perror(dip, "Identify Namespace failed");
	return(status);
    }
    if (dip->di_debug_flag) {
	Printf(dip, "Namespace ID %u\n", dip->di_namespace_id);
    }

    if (dip->di_debug_flag) {
	Printf(dip, "Issuing %s...\n", op);
    }
    status = nvme_identify_ns(fd, dip->di_namespace_id, force, nsp);
    if (status == SUCCESS) {
	char nguid_buf[2 * sizeof(nsp->nguid) + 1];
	char eui64_buf[2 * sizeof(nsp->eui64) + 1];
	char *nguid = nguid_buf, *eui64 = eui64_buf;
	uint32_t lba_size;
	int i;

	lba_size = 1 << nsp->lbaf[(nsp->flbas & 0x0f)].ds;
	/* Note: These values are in logical blocks! */
	dip->di_namespace_size = le64_to_cpu(nsp->nsze);
	dip->di_namespace_capacity = le64_to_cpu(nsp->ncap);
	dip->di_namespace_utilization = le64_to_cpu(nsp->nuse);
	dip->di_nvme_sector_size = lba_size;
	/* SCSI logic uses block length for seeks, etc. */
	dip->di_block_length = dip->di_nvme_sector_size;
	/* Copy fields for namespace identification. */
	memset(eui64_buf, 0, sizeof(eui64_buf));
	for (i = 0; i < sizeof(nsp->eui64); i++) {
	    eui64 += sprintf(eui64, "%02x", nsp->eui64[i]);
	}
	memset(nguid, 0, sizeof(nguid_buf));
	for (i = 0; i < sizeof(nsp->nguid); i++) {
	    nguid += sprintf(nguid, "%02x", nsp->nguid[i]);
	}
	dip->di_namespace_nguid = strdup(nguid_buf);
	dip->di_namespace_eui64 = strdup(eui64_buf);
    } else {
	if (dip->di_debug_flag) {
	    if (status > 0) {
		dt_nvme_show_status(dip, op, status);
	    } else {
		Perror(dip, "%s failed", op);
	    }
	}
	status = FAILURE;
    }
    return(status);
}

void
report_standard_nvme_information(dinfo_t *dip)
{
    char *dsf = (dip->di_scsi_dsf) ? dip->di_scsi_dsf : dip->di_dname;

    Lprintf(dip, "\nNVMe Information:\n");

    Lprintf (dip, DT_FIELD_WIDTH "%s\n", "NVMe Device Name", dsf);
    if (dip->di_product_id) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n", "Product Number", dip->di_product_id);
    }
    if (dip->di_serial_number) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n", "Serial Number", dip->di_serial_number);
    }
    if (dip->di_revision_level) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n", "Firmware Revision", dip->di_revision_level);
    }
    if (dip->di_namespace_id) {
	Lprintf(dip, DT_FIELD_WIDTH "%u\n", "Namespace ID (NSID)", dip->di_namespace_id);
    }
    if (dip->di_namespace_size) {
	display_extra_sizes(dip, "Namespace Size (NSZE)",
			    dip->di_namespace_size, dip->di_nvme_sector_size);
    }
    if (dip->di_namespace_capacity) {
	display_extra_sizes(dip, "Namespace Capacity (NCAP)",
			    dip->di_namespace_capacity, dip->di_nvme_sector_size);
    }
    if (dip->di_namespace_utilization) {
	display_extra_sizes(dip, "Namespace Utilization (NUSE)",
			    dip->di_namespace_utilization, dip->di_nvme_sector_size);
    }
    if (dip->di_nvme_sector_size) {
	Lprintf(dip, DT_FIELD_WIDTH "%u", "Formatted LBA Size (FLBAS)", dip->di_nvme_sector_size);
	Lprintf(dip, " (logical block / sector size)\n");
    }
    if (dip->di_total_nvm_capacity) {
	display_long_double(dip, "Total NVM Capacity (TNVMCAP)", dip->di_total_nvm_capacity);
    }
    if (dip->di_unalloc_nvm_capacity) {
	display_long_double(dip, "Unallocated NVM Capacity", dip->di_unalloc_nvm_capacity);
    }
    if (dip->di_namespace_eui64) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n", "IEEE Unique Identifier", dip->di_namespace_eui64);
    }
    if (dip->di_namespace_nguid) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n", "Globally Unique Identifier", dip->di_namespace_nguid);
    }
    if (dip->di_nvm_subsystem_nqn) {
	Lprintf(dip, DT_FIELD_WIDTH "%s\n", "Subsystem NVMe Qualified Name", dip->di_nvm_subsystem_nqn);
    }
    Lprintf(dip, "\n");
    Lflush(dip);
    return;
}

int
do_nvme_write_zeroes(dinfo_t *dip)
{
    __u64 start_block;
    __u32 ref_tag = 0;
    __u16 app_tag = 0;
    __u16 app_tag_mask = 0;
    __u16 block_count;
    __u16 control = 0;
    __u8  prinfo = 0;
    large_t data_bytes;
    Offset_t offset;
    char *op = "NVMe Write Zeroes";
    int fd = dip->di_fd;
    int status = SUCCESS;

    /* Note: Currently the disk is close in write post processing. */
    if (fd == INVALID_HANDLE_VALUE) {
	char *dsf = (dip->di_scsi_dsf) ? dip->di_scsi_dsf : dip->di_dname;
	fd = open(dsf, O_RDWR);
	if (fd == INVALID_HANDLE_VALUE) {
	    os_perror(dip, "open() of %s failed!", dsf);
	    return(FAILURE);
	}
    }

    (void)get_transfer_limits(dip, &data_bytes, &offset);

    start_block = (__u64)(offset / dip->di_nvme_sector_size);
    block_count = (__u16)(data_bytes / dip->di_nvme_sector_size);

    if (dip->di_verbose_flag) {
	Printf(dip, "Issuing %s, starting LBA "LUF", blocks %u, bytes "LUF", offset "FUF"...\n",
	       op, start_block, block_count, data_bytes, offset);
    }
    /* Note: The block count is zero based, this is abnormal! */
    if (block_count) {
	block_count--;
    }
    status = nvme_write_zeros(fd, dip->di_namespace_id, start_block, block_count,
			      control, ref_tag, app_tag, app_tag_mask);
    if (status != SUCCESS) {
	if (status > 0) {
	    dt_nvme_show_status(dip, op, status);
	} else {
	    Perror(dip, "%s failed", op);
	}
	status = FAILURE;
    }
    if (dip->di_fd == INVALID_HANDLE_VALUE) {
	(void)close(fd);
    }
    return(status);
}

/* 
 * Note: These API parameters match the Unix pread/pwrite API's! (except for dip)
 */
ssize_t
nvmeReadData(dinfo_t *dip, void *buffer, size_t bytes, Offset_t offset)
{
    __u8 opcode = nvme_cmd_read;
    __u64 start_block;
    __u32 ref_tag = 0;
    __u16 app_tag = 0;
    __u16 app_tag_mask = 0;
    __u16 block_count;
    __u16 control = 0;
    __u8  prinfo = 0;
    __u16 dsmgmt = 0;
    //__u64 storage_tag = 0;
    void *mbuffer = NULL;
    char *op = "NVMe Read";
    int fd = dip->di_fd;
    int status;

    start_block = (__u64)(offset / dip->di_nvme_sector_size);
    block_count = (__u16)(bytes / dip->di_nvme_sector_size);

    if (dip->di_sDebugFlag) {
	Printf(dip, "Issuing %s, starting LBA "LUF", blocks %u, bytes "LUF", offset "FUF"...\n",
	       op, start_block, block_count, bytes, offset);
    }

    /* Note: The block count is zero based, this is abnormal! */
    if (block_count) {
	block_count--;
    }

    status = nvme_io(fd, opcode, start_block, block_count, control, dsmgmt,
		     ref_tag, app_tag, app_tag_mask, buffer, mbuffer);

    if (status != SUCCESS) {
	if (status > 0) {
	    dt_nvme_show_status(dip, op, status);
	} else {
	    Perror(dip, "%s failed", op);
	}
	status = FAILURE;
    }
    if (status == SUCCESS) {
	return( (ssize_t)bytes );
    } else {
	return( (ssize_t)status );
    }
}

ssize_t
nvmeWriteData(dinfo_t *dip, void *buffer, size_t bytes, Offset_t offset)
{
    __u8 opcode = nvme_cmd_write;
    __u64 start_block;
    __u32 ref_tag = 0;
    __u16 app_tag = 0;
    __u16 app_tag_mask = 0;
    __u16 block_count;
    __u16 control = 0;
    __u8  prinfo = 0;
    __u16 dsmgmt = 0;
    //__u64 storage_tag = 0;
    void *mbuffer = NULL;
    char *op = "NVMe Write";
    int fd = dip->di_fd;
    int status;

    start_block = (__u64)(offset / dip->di_nvme_sector_size);
    block_count = (__u16)(bytes / dip->di_nvme_sector_size);

    if (dip->di_sDebugFlag) {
	Printf(dip, "Issuing %s, starting LBA "LUF", blocks %u, bytes "LUF", offset "FUF"...\n",
	       op, start_block, block_count, bytes, offset);
    }

    /* Note: The block count is zero based, this is abnormal! */
    if (block_count) {
	block_count--;
    }

    status = nvme_io(fd, opcode, start_block, block_count, control, dsmgmt,
		     ref_tag, app_tag, app_tag_mask, buffer, mbuffer);

    if (status != SUCCESS) {
	if (status > 0) {
	    dt_nvme_show_status(dip, op, status);
	} else {
	    Perror(dip, "%s failed", op);
	}
	status = FAILURE;
    }
    if (status == SUCCESS) {
	return( (ssize_t)bytes );
    } else {
	return( (ssize_t)status );
    }
}

void
dt_nvme_show_status(dinfo_t *dip, char *op, int status)
{
    Eprintf(dip, "%s: NVMe status: %s (%#x)\n", op, nvme_status_to_string((__u16)status), status);
    return;
}

#endif /* defined(NVME) && defined(__linux__) */
