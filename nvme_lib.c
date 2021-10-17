
#include "nvme_lib.h"
#include <stdio.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>

#include <getopt.h>
#include <fcntl.h>
#include <inttypes.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

const char *nvme_status_to_string(__u16 status)
{
    switch (status & 0x7ff) {
    case NVME_SC_SUCCESS:
        return "SUCCESS: The command completed successfully";
    case NVME_SC_INVALID_OPCODE:
	return "INVALID_OPCODE: The associated command opcode field is not valid";
    case NVME_SC_INVALID_FIELD:
        return "INVALID_FIELD: A reserved coded value or an unsupported value in a defined field";
    case NVME_SC_CMDID_CONFLICT:
        return "CMDID_CONFLICT: The command identifier is already in use";
    case NVME_SC_DATA_XFER_ERROR:
        return "DATA_XFER_ERROR: Error while trying to transfer the data or metadata";
    case NVME_SC_POWER_LOSS:
        return "POWER_LOSS: Command aborted due to power loss notification";
    case NVME_SC_INTERNAL:
        return "INTERNAL: The command was not completed successfully due to an internal error";
    case NVME_SC_ABORT_REQ:
        return "ABORT_REQ: The command was aborted due to a Command Abort request";
    case NVME_SC_ABORT_QUEUE:
        return "ABORT_QUEUE: The command was aborted due to a Delete I/O Submission Queue request";
    case NVME_SC_FUSED_FAIL:
        return "FUSED_FAIL: The command was aborted due to the other command in a fused operation failing";
    case NVME_SC_FUSED_MISSING:
        return "FUSED_MISSING: The command was aborted due to a Missing Fused Command";
    case NVME_SC_INVALID_NS:
        return "INVALID_NS: The namespace or the format of that namespace is invalid";
    case NVME_SC_CMD_SEQ_ERROR:
        return "CMD_SEQ_ERROR: The command was aborted due to a protocol violation in a multicommand sequence";
    case NVME_SC_SGL_INVALID_LAST:
        return "SGL_INVALID_LAST: The command includes an invalid SGL Last Segment or SGL Segment descriptor.";
    case NVME_SC_SGL_INVALID_COUNT:
        return "SGL_INVALID_COUNT: There is an SGL Last Segment descriptor or an SGL Segment descriptor in a location other than the last descriptor of a segment based on the length indicated.";
    case NVME_SC_SGL_INVALID_DATA:
        return "SGL_INVALID_DATA: This may occur if the length of a Data SGL is too short.";
    case NVME_SC_SGL_INVALID_METADATA:
        return "SGL_INVALID_METADATA: This may occur if the length of a Metadata SGL is too short";
    case NVME_SC_SGL_INVALID_TYPE:
        return "SGL_INVALID_TYPE: The type of an SGL Descriptor is a type that is not supported by the controller.";
    case NVME_SC_CMB_INVALID_USE:
        return "CMB_INVALID_USE: The attempted use of the Controller Memory Buffer is not supported by the controller.";
    case NVME_SC_PRP_INVALID_OFFSET:
        return "PRP_INVALID_OFFSET: The Offset field for a PRP entry is invalid.";
    case NVME_SC_ATOMIC_WRITE_UNIT_EXCEEDED:
        return "ATOMIC_WRITE_UNIT_EXCEEDED: The length specified exceeds the atomic write unit size.";
    case NVME_SC_OPERATION_DENIED:
        return "OPERATION_DENIED: The command was denied due to lack of access rights.";
    case NVME_SC_SGL_INVALID_OFFSET:
        return "SGL_INVALID_OFFSET: The offset specified in a descriptor is invalid.";
    case NVME_SC_INCONSISTENT_HOST_ID:
        return "INCONSISTENT_HOST_ID: The NVM subsystem detected the simultaneous use of 64-bit and 128-bit Host Identifier values on different controllers.";
    case NVME_SC_KEEP_ALIVE_EXPIRED:
        return "KEEP_ALIVE_EXPIRED: The Keep Alive Timer expired.";
    case NVME_SC_KEEP_ALIVE_INVALID:
        return "KEEP_ALIVE_INVALID: The Keep Alive Timeout value specified is invalid.";
    case NVME_SC_PREEMPT_ABORT:
        return "PREEMPT_ABORT: The command was aborted due to a Reservation Acquire command with the Reservation Acquire Action (RACQA) set to 010b (Preempt and Abort).";
    case NVME_SC_SANITIZE_FAILED:
        return "SANITIZE_FAILED: The most recent sanitize operation failed and no recovery actions has been successfully completed";
    case NVME_SC_SANITIZE_IN_PROGRESS:
        return "SANITIZE_IN_PROGRESS: The requested function is prohibited while a sanitize operation is in progress";
    case NVME_SC_IOCS_NOT_SUPPORTED:
        return "IOCS_NOT_SUPPORTED: The I/O command set is not supported";
    case NVME_SC_IOCS_NOT_ENABLED:
        return "IOCS_NOT_ENABLED: The I/O command set is not enabled";
    case NVME_SC_IOCS_COMBINATION_REJECTED:
        return "IOCS_COMBINATION_REJECTED: The I/O command set combination is rejected";
    case NVME_SC_INVALID_IOCS:
        return "INVALID_IOCS: the I/O command set is invalid";
    case NVME_SC_LBA_RANGE:
        return "LBA_RANGE: The command references a LBA that exceeds the size of the namespace";
    case NVME_SC_NS_WRITE_PROTECTED:
        return "NS_WRITE_PROTECTED: The command is prohibited while the namespace is write protected by the host.";
    case NVME_SC_TRANSIENT_TRANSPORT:
        return "TRANSIENT_TRANSPORT: A transient transport error was detected.";
    case NVME_SC_CAP_EXCEEDED:
        return "CAP_EXCEEDED: The execution of the command has caused the capacity of the namespace to be exceeded";
    case NVME_SC_NS_NOT_READY:
        return "NS_NOT_READY: The namespace is not ready to be accessed as a result of a condition other than a condition that is reported as an Asymmetric Namespace Access condition";
    case NVME_SC_RESERVATION_CONFLICT:
        return "RESERVATION_CONFLICT: The command was aborted due to a conflict with a reservation held on the accessed namespace";
    case NVME_SC_FORMAT_IN_PROGRESS:
        return "FORMAT_IN_PROGRESS: A Format NVM command is in progress on the namespace.";
    case NVME_SC_ZONE_BOUNDARY_ERROR:
        return "ZONE_BOUNDARY_ERROR: Invalid Zone Boundary crossing";
    case NVME_SC_ZONE_IS_FULL:
        return "ZONE_IS_FULL: The accessed zone is in ZSF:Full state";
    case NVME_SC_ZONE_IS_READ_ONLY:
        return "ZONE_IS_READ_ONLY: The accessed zone is in ZSRO:Read Only state";
    case NVME_SC_ZONE_IS_OFFLINE:
        return "ZONE_IS_OFFLINE: The access zone is in ZSO:Offline state";
    case NVME_SC_ZONE_INVALID_WRITE:
        return "ZONE_INVALID_WRITE: The write to zone was not at the write pointer offset";
    case NVME_SC_TOO_MANY_ACTIVE_ZONES:
        return "TOO_MANY_ACTIVE_ZONES: The controller does not allow additional active zones";
    case NVME_SC_TOO_MANY_OPEN_ZONES:
        return "TOO_MANY_OPEN_ZONES: The controller does not allow additional open zones";
    case NVME_SC_ZONE_INVALID_STATE_TRANSITION:
        return "INVALID_ZONE_STATE_TRANSITION: The zone state change was invalid";
    case NVME_SC_CQ_INVALID:
        return "CQ_INVALID: The Completion Queue identifier specified in the command does not exist";
    case NVME_SC_QID_INVALID:
        return "QID_INVALID: The creation of the I/O Completion Queue failed due to an invalid queue identifier specified as part of the command. An invalid queue identifier is one that is currently in use or one that is outside the range supported by the controller";
    case NVME_SC_QUEUE_SIZE:
        return "QUEUE_SIZE: The host attempted to create an I/O Completion Queue with an invalid number of entries";
    case NVME_SC_ABORT_LIMIT:
        return "ABORT_LIMIT: The number of concurrently outstanding Abort commands has exceeded the limit indicated in the Identify Controller data structure";
    case NVME_SC_ABORT_MISSING:
        return "ABORT_MISSING: The abort command is missing";
    case NVME_SC_ASYNC_LIMIT:
        return "ASYNC_LIMIT: The number of concurrently outstanding Asynchronous Event Request commands has been exceeded";
    case NVME_SC_FIRMWARE_SLOT:
        return "FIRMWARE_SLOT: The firmware slot indicated is invalid or read only. This error is indicated if the firmware slot exceeds the number supported";
    case NVME_SC_FIRMWARE_IMAGE:
        return "FIRMWARE_IMAGE: The firmware image specified for activation is invalid and not loaded by the controller";
    case NVME_SC_INVALID_VECTOR:
        return "INVALID_VECTOR: The creation of the I/O Completion Queue failed due to an invalid interrupt vector specified as part of the command";
    case NVME_SC_INVALID_LOG_PAGE:
        return "INVALID_LOG_PAGE: The log page indicated is invalid. This error condition is also returned if a reserved log page is requested";
    case NVME_SC_INVALID_FORMAT:
        return "INVALID_FORMAT: The LBA Format specified is not supported. This may be due to various conditions";
    case NVME_SC_FW_NEEDS_CONV_RESET:
        return "FW_NEEDS_CONVENTIONAL_RESET: The firmware commit was successful, however, activation of the firmware image requires a conventional reset";
    case NVME_SC_INVALID_QUEUE:
        return "INVALID_QUEUE: This error indicates that it is invalid to delete the I/O Completion Queue specified. The typical reason for this error condition is that there is an associated I/O Submission Queue that has not been deleted.";
    case NVME_SC_FEATURE_NOT_SAVEABLE:
        return "FEATURE_NOT_SAVEABLE: The Feature Identifier specified does not support a saveable value";
    case NVME_SC_FEATURE_NOT_CHANGEABLE:
        return "FEATURE_NOT_CHANGEABLE: The Feature Identifier is not able to be changed";
    case NVME_SC_FEATURE_NOT_PER_NS:
        return "FEATURE_NOT_PER_NS: The Feature Identifier specified is not namespace specific. The Feature Identifier settings apply across all namespaces";
    case NVME_SC_FW_NEEDS_SUBSYS_RESET:
        return "FW_NEEDS_SUBSYSTEM_RESET: The firmware commit was successful, however, activation of the firmware image requires an NVM Subsystem";
    case NVME_SC_FW_NEEDS_RESET:
        return "FW_NEEDS_RESET: The firmware commit was successful; however, the image specified does not support being activated without a reset";
    case NVME_SC_FW_NEEDS_MAX_TIME:
        return "FW_NEEDS_MAX_TIME_VIOLATION: The image specified if activated immediately would exceed the Maximum Time for Firmware Activation (MTFA) value reported in Identify Controller. To activate the firmware, the Firmware Commit command needs to be re-issued and the image activated using a reset";
    case NVME_SC_FW_ACTIVATE_PROHIBITED:
        return "FW_ACTIVATION_PROHIBITED: The image specified is being prohibited from activation by the controller for vendor specific reasons";
    case NVME_SC_OVERLAPPING_RANGE:
        return "OVERLAPPING_RANGE: This error is indicated if the firmware image has overlapping ranges";
    case NVME_SC_NS_INSUFFICIENT_CAP:
        return "NS_INSUFFICIENT_CAPACITY: Creating the namespace requires more free space than is currently available. The Command Specific Information field of the Error Information Log specifies the total amount of NVM capacity required to create the namespace in bytes";
    case NVME_SC_NS_ID_UNAVAILABLE:
        return "NS_ID_UNAVAILABLE: The number of namespaces supported has been exceeded";
    case NVME_SC_NS_ALREADY_ATTACHED:
        return "NS_ALREADY_ATTACHED: The controller is already attached to the namespace specified";
    case NVME_SC_NS_IS_PRIVATE:
        return "NS_IS_PRIVATE: The namespace is private and is already attached to one controller";
    case NVME_SC_NS_NOT_ATTACHED:
        return "NS_NOT_ATTACHED: The request to detach the controller could not be completed because the controller is not attached to the namespace";
    case NVME_SC_THIN_PROV_NOT_SUPP:
        return "THIN_PROVISIONING_NOT_SUPPORTED: Thin provisioning is not supported by the controller";
    case NVME_SC_CTRL_LIST_INVALID:
        return "CONTROLLER_LIST_INVALID: The controller list provided is invalid";
    case NVME_SC_DEVICE_SELF_TEST_IN_PROGRESS:
        return "DEVICE_SELF_TEST_IN_PROGRESS: The controller or NVM subsystem already has a device self-test operation in process.";
    case NVME_SC_BP_WRITE_PROHIBITED:
        return "BOOT PARTITION WRITE PROHIBITED: The command is trying to modify a Boot Partition while it is locked";
    case NVME_SC_INVALID_CTRL_ID:
        return "INVALID_CTRL_ID: An invalid Controller Identifier was specified.";
    case NVME_SC_INVALID_SECONDARY_CTRL_STATE:
        return "INVALID_SECONDARY_CTRL_STATE: The action requested for the secondary controller is invalid based on the current state of the secondary controller and its primary controller.";
    case NVME_SC_INVALID_NUM_CTRL_RESOURCE:
        return "INVALID_NUM_CTRL_RESOURCE: The specified number of Flexible Resources is invalid";
    case NVME_SC_INVALID_RESOURCE_ID:
        return "INVALID_RESOURCE_ID: At least one of the specified resource identifiers was invalid";
    case NVME_SC_ANA_INVALID_GROUP_ID:
        return "ANA_INVALID_GROUP_ID: The specified ANA Group Identifier (ANAGRPID) is not supported in the submitted command.";
    case NVME_SC_ANA_ATTACH_FAIL:
        return "ANA_ATTACH_FAIL: The controller is not attached to the namespace as a result of an ANA condition";
    case NVME_SC_BAD_ATTRIBUTES:
        return "BAD_ATTRIBUTES: Bad attributes were given";
    case NVME_SC_INVALID_PI:
        return "INVALID_PROTECION_INFO: The Protection Information Field settings specified in the command are invalid";
    case NVME_SC_READ_ONLY:
        return "WRITE_ATTEMPT_READ_ONLY_RANGE: The LBA range specified contains read-only blocks";
    case NVME_SC_CMD_SIZE_LIMIT_EXCEEDED:
        return "CMD_SIZE_LIMIT_EXCEEDED: Command size limit exceeded";
    case NVME_SC_WRITE_FAULT:
        return "WRITE_FAULT: The write data could not be committed to the media";
    case NVME_SC_READ_ERROR:
        return "READ_ERROR: The read data could not be recovered from the media";
    case NVME_SC_GUARD_CHECK:
        return "GUARD_CHECK: The command was aborted due to an end-to-end guard check failure";
    case NVME_SC_APPTAG_CHECK:
        return "APPTAG_CHECK: The command was aborted due to an end-to-end application tag check failure";
    case NVME_SC_REFTAG_CHECK:
        return "REFTAG_CHECK: The command was aborted due to an end-to-end reference tag check failure";
    case NVME_SC_COMPARE_FAILED:
        return "COMPARE_FAILED: The command failed due to a miscompare during a Compare command";
    case NVME_SC_ACCESS_DENIED:
        return "ACCESS_DENIED: Access to the namespace and/or LBA range is denied due to lack of access rights";
    case NVME_SC_UNWRITTEN_BLOCK:
        return "UNWRITTEN_BLOCK: The command failed due to an attempt to read from an LBA range containing a deallocated or unwritten logical block";
    case NVME_SC_INTERNAL_PATH_ERROR:
        return "INTERNAL_PATH_ERROT: The command was not completed as the result of a controller internal error";
    case NVME_SC_ANA_PERSISTENT_LOSS:
        return "ASYMMETRIC_NAMESPACE_ACCESS_PERSISTENT_LOSS: The requested function (e.g., command) is not able to be performed as a result of the relationship between the controller and the namespace being in the ANA Persistent Loss state";
    case NVME_SC_ANA_INACCESSIBLE:
        return "ASYMMETRIC_NAMESPACE_ACCESS_INACCESSIBLE: The requested function (e.g., command) is not able to be performed as a result of the relationship between the controller and the namespace being in the ANA Inaccessible state";
    case NVME_SC_ANA_TRANSITION:
        return "ASYMMETRIC_NAMESPACE_ACCESS_TRANSITION: The requested function (e.g., command) is not able to be performed as a result of the relationship between the controller and the namespace transitioning between Asymmetric Namespace Access states";
    case NVME_SC_CTRL_PATHING_ERROR:
        return "CONTROLLER_PATHING_ERROR: A pathing error was detected by the controller";
    case NVME_SC_HOST_PATHING_ERROR:
        return "HOST_PATHING_ERROR: A pathing error was detected by the host";
    case NVME_SC_HOST_CMD_ABORT:
        return "HOST_COMMAND_ABORT: The command was aborted as a result of host action";
    case NVME_SC_CMD_INTERRUPTED:
        return "CMD_INTERRUPTED: Command processing was interrupted and the controller is unable to successfully complete the command. The host should retry the command.";
    case NVME_SC_PMR_SAN_PROHIBITED:
        return "Sanitize Prohibited While Persistent Memory Region is Enabled: A sanitize operation is prohibited while the Persistent Memory Region is enabled.";
    default:
        return "Unknown";
    }
}

void
nvme_show_status(__u16 status) {
    fprintf(stderr, "NVMe status: %s(%#x)\n", nvme_status_to_string(status), status);
}

int
nvme_submit_passthru(int fd, unsigned long ioctl_cmd,
             struct nvme_passthru_cmd *cmd) {
    return ioctl(fd, ioctl_cmd, cmd);
}

int
nvme_submit_admin_passthru(int fd, struct nvme_passthru_cmd *cmd) {
    return ioctl(fd, NVME_IOCTL_ADMIN_CMD, cmd);
}

int
nvme_submit_io_passthru(int fd, struct nvme_passthru_cmd *cmd) {
    return ioctl(fd, NVME_IOCTL_IO_CMD, cmd);
}

int
nvme_passthru(int fd, unsigned long ioctl_cmd, __u8 opcode,
          __u8 flags, __u16 rsvd,
          __u32 nsid, __u32 cdw2, __u32 cdw3, __u32 cdw10, __u32 cdw11,
          __u32 cdw12, __u32 cdw13, __u32 cdw14, __u32 cdw15,
          __u32 data_len, void *data, __u32 metadata_len,
          void *metadata, __u32 timeout_ms, __u32 *result) {
    struct nvme_passthru_cmd cmd = {
        .opcode        = opcode,
        .flags        = flags,
        .rsvd1        = rsvd,
        .nsid        = nsid,
        .cdw2        = cdw2,
        .cdw3        = cdw3,
        .metadata    = (__u64)(uintptr_t) metadata,
        .addr        = (__u64)(uintptr_t) data,
        .metadata_len    = metadata_len,
        .data_len    = data_len,
        .cdw10        = cdw10,
        .cdw11        = cdw11,
        .cdw12        = cdw12,
        .cdw13        = cdw13,
        .cdw14        = cdw14,
        .cdw15        = cdw15,
        .timeout_ms    = timeout_ms,
        .result        = 0,
    };
    int err;

    err = nvme_submit_passthru(fd, ioctl_cmd, &cmd);
    if (!err && result)
        *result = cmd.result;
    return err;
}

int nvme_io(int fd, __u8 opcode, __u64 slba, __u16 nblocks, __u16 control,
	    __u32 dsmgmt, __u32 reftag, __u16 apptag, __u16 appmask, void *data,
	    void *metadata)
{
	struct nvme_user_io io = {
		.opcode		= opcode,
		.flags		= 0,
		.control	= control,
		.nblocks	= nblocks,
		.rsvd		= 0,
		.metadata	= (__u64)(uintptr_t) metadata,
		.addr		= (__u64)(uintptr_t) data,
		.slba		= slba,
		.dsmgmt		= dsmgmt,
		.reftag		= reftag,
		.appmask	= appmask,
		.apptag		= apptag,
	};
	return ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io);
}

int
nvme_verify(int fd, __u32 nsid, __u64 slba, __u16 nblocks,
        __u16 control, __u32 reftag, __u16 apptag, __u16 appmask) {
    struct nvme_passthru_cmd cmd = {
        .opcode        = nvme_cmd_verify,
        .nsid        = nsid,
        .cdw10        = slba & 0xffffffff,
        .cdw11        = slba >> 32,
        .cdw12        = nblocks | (control << 16),
        .cdw14        = reftag,
        .cdw15        = apptag | (appmask << 16),
    };

    return nvme_submit_io_passthru(fd, &cmd);
}

int
nvme_passthru_io(int fd, __u8 opcode, __u8 flags, __u16 rsvd,
             __u32 nsid, __u32 cdw2, __u32 cdw3, __u32 cdw10,
             __u32 cdw11, __u32 cdw12, __u32 cdw13, __u32 cdw14,
             __u32 cdw15, __u32 data_len, void *data,
             __u32 metadata_len, void *metadata, __u32 timeout_ms) {
    return nvme_passthru(fd, NVME_IOCTL_IO_CMD, opcode, flags, rsvd, nsid,
                 cdw2, cdw3, cdw10, cdw11, cdw12, cdw13, cdw14,
                 cdw15, data_len, data, metadata_len, metadata,
                 timeout_ms, NULL);
}

int nvme_write_zeros(int fd, __u32 nsid, __u64 slba, __u16 nlb,
		     __u16 control, __u32 reftag, __u16 apptag, __u16 appmask)
{
	struct nvme_passthru_cmd cmd = {
		.opcode		= nvme_cmd_write_zeroes,
		.nsid		= nsid,
		.cdw10		= slba & 0xffffffff,
		.cdw11		= slba >> 32,
		.cdw12		= nlb | (control << 16),
		.cdw14		= reftag,
		.cdw15		= apptag | (appmask << 16),
	};

	return nvme_submit_io_passthru(fd, &cmd);
}

int
nvme_identify13(int fd, __u32 nsid, __u32 cdw10, __u32 cdw11, void *data) {
    struct nvme_admin_cmd cmd = {
        .opcode       = nvme_admin_identify,
        .nsid         = nsid,
        .addr         = (__u64)(uintptr_t) data,
        .data_len     = NVME_IDENTIFY_DATA_SIZE,
        .cdw10        = cdw10,
        .cdw11        = cdw11,
    };

    return nvme_submit_admin_passthru(fd, &cmd);
}

int
nvme_identify(int fd, __u32 nsid, __u32 cdw10, void *data) {
    return nvme_identify13(fd, nsid, cdw10, 0, data);
}

int
nvme_identify_ctrl(int fd, void *data) {
    memset(data, 0, sizeof(struct nvme_id_ctrl));
    return nvme_identify(fd, 0, 1, data);
}

int
nvme_identify_ns(int fd, __u32 nsid, bool present, void *data) {
    int cns = present ? NVME_ID_CNS_NS_PRESENT : NVME_ID_CNS_NS;

    return nvme_identify(fd, nsid, cns, data);
}

int
nvme_identify_ns_list_csi(int fd, __u32 nsid, __u8 csi, bool all, void *data) {
    int cns;

    if (csi) {
        cns = all ? NVME_ID_CNS_CSI_NS_PRESENT_LIST : NVME_ID_CNS_CSI_NS_ACTIVE_LIST;
    } else {
        cns = all ? NVME_ID_CNS_NS_PRESENT_LIST : NVME_ID_CNS_NS_ACTIVE_LIST;
    }

    return nvme_identify13(fd, nsid, cns, csi << 24, data);
}

int
nvme_identify_ns_list(int fd, __u32 nsid, bool all, void *data) {
    return nvme_identify_ns_list_csi(fd, nsid, 0x0, all, data);
}

int
nvme_identify_ctrl_list(int fd, __u32 nsid, __u16 cntid, void *data) {
    int cns = nsid ? NVME_ID_CNS_CTRL_NS_LIST : NVME_ID_CNS_CTRL_LIST;

    return nvme_identify(fd, nsid, (cntid << 16) | cns, data);
}

int
nvme_identify_secondary_ctrl_list(int fd, __u32 nsid, __u16 cntid, void *data) {
    return nvme_identify(fd, nsid, (cntid << 16) | NVME_ID_CNS_SCNDRY_CTRL_LIST, data);
}

int
nvme_identify_ns_descs(int fd, __u32 nsid, void *data) {

    return nvme_identify(fd, nsid, NVME_ID_CNS_NS_DESC_LIST, data);
}

int
nvme_identify_nvmset(int fd, __u16 nvmset_id, void *data) {
    return nvme_identify13(fd, 0, NVME_ID_CNS_NVMSET_LIST, nvmset_id, data);
}

int
nvme_identify_ns_granularity(int fd, void *data) {
    return nvme_identify13(fd, 0, NVME_ID_CNS_NS_GRANULARITY, 0, data);
}

int
nvme_identify_uuid(int fd, void *data) {
    return nvme_identify(fd, 0, NVME_ID_CNS_UUID_LIST, data);
}

int
nvme_identify_ctrl_nvm(int fd, void *data) {
    return nvme_identify13(fd, 0, NVME_ID_CNS_CSI_ID_CTRL, 0, data);
}

int
nvme_zns_identify_ns(int fd, __u32 nsid, void *data) {
    return nvme_identify13(fd, nsid, NVME_ID_CNS_CSI_ID_NS, 2 << 24, data);
}

int
nvme_zns_identify_ctrl(int fd, void *data) {
    return nvme_identify13(fd, 0, NVME_ID_CNS_CSI_ID_CTRL, 2 << 24, data);
}

int
nvme_identify_iocs(int fd, __u16 cntid, void *data) {
    return nvme_identify(fd, 0, (cntid << 16) | NVME_ID_CNS_CSI, data);
}

int
nvme_dsm(int fd, __u32 nsid, __u32 cdw11, struct nvme_dsm_range *dsm,
         __u16 nr_ranges) {
    struct nvme_passthru_cmd cmd = {
        .opcode        = nvme_cmd_dsm,
        .nsid        = nsid,
        .addr        = (__u64)(uintptr_t) dsm,
        .data_len    = nr_ranges * sizeof(*dsm),
        .cdw10        = nr_ranges - 1,
        .cdw11        = cdw11,
    };

    return nvme_submit_io_passthru(fd, &cmd);
}

struct nvme_dsm_range
*nvme_setup_dsm_range(int *ctx_attrs, int *llbas,
                        unsigned long long *slbas,
                        __u16 nr_ranges) {
    int i;
    struct nvme_dsm_range *dsm = malloc(nr_ranges * sizeof(*dsm));

    if (!dsm) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        return NULL;
    }
    for (i = 0; i < nr_ranges; i++) {
        dsm[i].cattr = cpu_to_le32(ctx_attrs[i]);
        dsm[i].nlb = cpu_to_le32(llbas[i]);
    }
    for (i = 0; i < nr_ranges; i++) {
        dsm[i].cattr = cpu_to_le32(ctx_attrs[i]);
        dsm[i].nlb = cpu_to_le32(llbas[i]);
        dsm[i].slba = cpu_to_le64(slbas[i]);
    }
    return dsm;
}

int nvme_get_nsid(int fd)
{
    static struct stat nvme_stat;
    int err = fstat(fd, &nvme_stat);

    if (err < 0)
	    return err;

    return ioctl(fd, NVME_IOCTL_ID);
}

long double int128_to_double(__u8 *data)
{
	int i;
	long double result = 0;

	for (i = 0; i < 16; i++) {
		result *= 256;
		result += data[15 - i];
	}
	return result;
}
