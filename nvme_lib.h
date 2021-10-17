/*
 * Definitions for the NVM Express interface
 * Copyright (c) 2011-2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef NVME_LIB_H__
#define NVME_LIB_H__

#include <linux/types.h>
#include <stdbool.h>

#define NVME_IOCTL_TIMEOUT	120000 /* in milliseconds */
#define NVME_IDENTIFY_DATA_SIZE	4096

extern int nvme_get_nsid(int fd);
extern long double int128_to_double(__u8 *data);

#include <linux/types.h>
#include <sys/ioctl.h>

/* I/O commands */

enum nvme_opcode {
    nvme_cmd_flush            = 0x00,
    nvme_cmd_write            = 0x01,
    nvme_cmd_read             = 0x02,
    nvme_cmd_write_uncor      = 0x04,
    nvme_cmd_compare          = 0x05,
    nvme_cmd_write_zeroes     = 0x08,
    nvme_cmd_dsm              = 0x09,
    nvme_cmd_verify           = 0x0c,
    nvme_cmd_resv_register    = 0x0d,
    nvme_cmd_resv_report      = 0x0e,
    nvme_cmd_resv_acquire     = 0x11,
    nvme_cmd_resv_release     = 0x15,
    nvme_cmd_copy             = 0x19,
    nvme_zns_cmd_mgmt_send    = 0x79,
    nvme_zns_cmd_mgmt_recv    = 0x7a,
    nvme_zns_cmd_append       = 0x7d,
};

enum {
    NVME_ID_CNS_NS                  = 0x00,
    NVME_ID_CNS_CTRL                = 0x01,
    NVME_ID_CNS_NS_ACTIVE_LIST      = 0x02,
    NVME_ID_CNS_NS_DESC_LIST        = 0x03,
    NVME_ID_CNS_NVMSET_LIST         = 0x04,
    NVME_ID_CNS_CSI_ID_NS           = 0x05,
    NVME_ID_CNS_CSI_ID_CTRL         = 0x06,
    NVME_ID_CNS_CSI_NS_ACTIVE_LIST  = 0x07,
    NVME_ID_CNS_NS_PRESENT_LIST     = 0x10,
    NVME_ID_CNS_NS_PRESENT          = 0x11,
    NVME_ID_CNS_CTRL_NS_LIST        = 0x12,
    NVME_ID_CNS_CTRL_LIST           = 0x13,
    NVME_ID_CNS_SCNDRY_CTRL_LIST    = 0x15,
    NVME_ID_CNS_NS_GRANULARITY      = 0x16,
    NVME_ID_CNS_UUID_LIST           = 0x17,
    NVME_ID_CNS_CSI_NS_PRESENT_LIST = 0x1a,
    NVME_ID_CNS_CSI_NS_PRESENT      = 0x1b,
    NVME_ID_CNS_CSI                 = 0x1c,
};

/* Admin commands */

enum nvme_admin_opcode {
    nvme_admin_delete_sq        = 0x00,
    nvme_admin_create_sq        = 0x01,
    nvme_admin_get_log_page     = 0x02,
    nvme_admin_delete_cq        = 0x04,
    nvme_admin_create_cq        = 0x05,
    nvme_admin_identify         = 0x06,
    nvme_admin_abort_cmd        = 0x08,
    nvme_admin_set_features     = 0x09,
    nvme_admin_get_features     = 0x0a,
    nvme_admin_async_event      = 0x0c,
    nvme_admin_ns_mgmt          = 0x0d,
    nvme_admin_activate_fw      = 0x10,
    nvme_admin_download_fw      = 0x11,
    nvme_admin_dev_self_test    = 0x14,
    nvme_admin_ns_attach        = 0x15,
    nvme_admin_keep_alive       = 0x18,
    nvme_admin_directive_send   = 0x19,
    nvme_admin_directive_recv   = 0x1a,
    nvme_admin_virtual_mgmt     = 0x1c,
    nvme_admin_nvme_mi_send     = 0x1d,
    nvme_admin_nvme_mi_recv     = 0x1e,
    nvme_admin_dbbuf            = 0x7C,
    nvme_admin_format_nvm       = 0x80,
    nvme_admin_security_send    = 0x81,
    nvme_admin_security_recv    = 0x82,
    nvme_admin_sanitize_nvm     = 0x84,
    nvme_admin_get_lba_status   = 0x86,
};

struct nvme_id_power_state {
    __le16     max_power;    /* centiwatts */
    __u8       rsvd2;
    __u8       flags;
    __le32     entry_lat;    /* microseconds */
    __le32     exit_lat;    /* microseconds */
    __u8       read_tput;
    __u8       read_lat;
    __u8       write_tput;
    __u8       write_lat;
    __le16     idle_power;
    __u8       idle_scale;
    __u8       rsvd19;
    __le16     active_power;
    __u8       active_work_scale;
    __u8       rsvd23[9];
};

enum {
    NVME_RW_LR                  = 1 << 15,
    NVME_RW_FUA                 = 1 << 14,
    NVME_RW_DEAC                = 1 << 9,
    NVME_RW_DSM_FREQ_UNSPEC     = 0,
    NVME_RW_DSM_FREQ_TYPICAL    = 1,
    NVME_RW_DSM_FREQ_RARE       = 2,
    NVME_RW_DSM_FREQ_READS      = 3,
    NVME_RW_DSM_FREQ_WRITES     = 4,
    NVME_RW_DSM_FREQ_RW         = 5,
    NVME_RW_DSM_FREQ_ONCE       = 6,
    NVME_RW_DSM_FREQ_PREFETCH   = 7,
    NVME_RW_DSM_FREQ_TEMP       = 8,
    NVME_RW_DSM_LATENCY_NONE    = 0 << 4,
    NVME_RW_DSM_LATENCY_IDLE    = 1 << 4,
    NVME_RW_DSM_LATENCY_NORM    = 2 << 4,
    NVME_RW_DSM_LATENCY_LOW     = 3 << 4,
    NVME_RW_DSM_SEQ_REQ         = 1 << 6,
    NVME_RW_DSM_COMPRESSED      = 1 << 7,
    NVME_RW_PRINFO_PRCHK_REF    = 1 << 10,
    NVME_RW_PRINFO_PRCHK_APP    = 1 << 11,
    NVME_RW_PRINFO_PRCHK_GUARD  = 1 << 12,
    NVME_RW_PRINFO_PRACT        = 1 << 13,
    NVME_RW_DTYPE_STREAMS       = 1 << 4,
};

enum {
    /*
     * Generic Command Status:
     */
    NVME_SC_SUCCESS            = 0x0,
    NVME_SC_INVALID_OPCODE     = 0x1,
    NVME_SC_INVALID_FIELD      = 0x2,
    NVME_SC_CMDID_CONFLICT     = 0x3,
    NVME_SC_DATA_XFER_ERROR    = 0x4,
    NVME_SC_POWER_LOSS         = 0x5,
    NVME_SC_INTERNAL           = 0x6,
    NVME_SC_ABORT_REQ          = 0x7,
    NVME_SC_ABORT_QUEUE        = 0x8,
    NVME_SC_FUSED_FAIL         = 0x9,
    NVME_SC_FUSED_MISSING      = 0xa,
    NVME_SC_INVALID_NS         = 0xb,
    NVME_SC_CMD_SEQ_ERROR      = 0xc,
    NVME_SC_SGL_INVALID_LAST   = 0xd,
    NVME_SC_SGL_INVALID_COUNT  = 0xe,
    NVME_SC_SGL_INVALID_DATA   = 0xf,
    NVME_SC_SGL_INVALID_METADATA = 0x10,
    NVME_SC_SGL_INVALID_TYPE   = 0x11,
    NVME_SC_CMB_INVALID_USE    = 0x12,
    NVME_SC_PRP_INVALID_OFFSET = 0x13,
    NVME_SC_ATOMIC_WRITE_UNIT_EXCEEDED = 0x14,
    NVME_SC_OPERATION_DENIED    = 0x15,
    NVME_SC_SGL_INVALID_OFFSET  = 0x16,

    NVME_SC_INCONSISTENT_HOST_ID = 0x18,
    NVME_SC_KEEP_ALIVE_EXPIRED   = 0x19,
    NVME_SC_KEEP_ALIVE_INVALID   = 0x1A,
    NVME_SC_PREEMPT_ABORT        = 0x1B,
    NVME_SC_SANITIZE_FAILED      = 0x1C,
    NVME_SC_SANITIZE_IN_PROGRESS = 0x1D,

    NVME_SC_NS_WRITE_PROTECTED   = 0x20,
    NVME_SC_CMD_INTERRUPTED      = 0x21,
    NVME_SC_TRANSIENT_TRANSPORT  = 0x22,

    NVME_SC_LBA_RANGE            = 0x80,
    NVME_SC_CAP_EXCEEDED         = 0x81,
    NVME_SC_NS_NOT_READY         = 0x82,
    NVME_SC_RESERVATION_CONFLICT = 0x83,
    NVME_SC_FORMAT_IN_PROGRESS   = 0x84,

    /*
     * Command Specific Status:
     */
    NVME_SC_CQ_INVALID        = 0x100,
    NVME_SC_QID_INVALID       = 0x101,
    NVME_SC_QUEUE_SIZE        = 0x102,
    NVME_SC_ABORT_LIMIT       = 0x103,
    NVME_SC_ABORT_MISSING     = 0x104,
    NVME_SC_ASYNC_LIMIT       = 0x105,
    NVME_SC_FIRMWARE_SLOT     = 0x106,
    NVME_SC_FIRMWARE_IMAGE    = 0x107,
    NVME_SC_INVALID_VECTOR    = 0x108,
    NVME_SC_INVALID_LOG_PAGE  = 0x109,
    NVME_SC_INVALID_FORMAT    = 0x10a,
    NVME_SC_FW_NEEDS_CONV_RESET    = 0x10b,
    NVME_SC_INVALID_QUEUE          = 0x10c,
    NVME_SC_FEATURE_NOT_SAVEABLE   = 0x10d,
    NVME_SC_FEATURE_NOT_CHANGEABLE = 0x10e,
    NVME_SC_FEATURE_NOT_PER_NS     = 0x10f,
    NVME_SC_FW_NEEDS_SUBSYS_RESET  = 0x110,
    NVME_SC_FW_NEEDS_RESET         = 0x111,
    NVME_SC_FW_NEEDS_MAX_TIME      = 0x112,
    NVME_SC_FW_ACTIVATE_PROHIBITED = 0x113,
    NVME_SC_OVERLAPPING_RANGE      = 0x114,
    NVME_SC_NS_INSUFFICIENT_CAP    = 0x115,
    NVME_SC_NS_ID_UNAVAILABLE      = 0x116,
    NVME_SC_NS_ALREADY_ATTACHED    = 0x118,
    NVME_SC_NS_IS_PRIVATE          = 0x119,
    NVME_SC_NS_NOT_ATTACHED        = 0x11a,
    NVME_SC_THIN_PROV_NOT_SUPP     = 0x11b,
    NVME_SC_CTRL_LIST_INVALID      = 0x11c,
    NVME_SC_DEVICE_SELF_TEST_IN_PROGRESS = 0x11d,
    NVME_SC_BP_WRITE_PROHIBITED          = 0x11e,
    NVME_SC_INVALID_CTRL_ID              = 0x11f,
    NVME_SC_INVALID_SECONDARY_CTRL_STATE = 0x120,
    NVME_SC_INVALID_NUM_CTRL_RESOURCE    = 0x121,
    NVME_SC_INVALID_RESOURCE_ID          = 0x122,
    NVME_SC_PMR_SAN_PROHIBITED           = 0x123,
    NVME_SC_ANA_INVALID_GROUP_ID         = 0x124,
    NVME_SC_ANA_ATTACH_FAIL              = 0x125,

    /*
     * Command Set Specific - Namespace Types commands:
     */
    NVME_SC_IOCS_NOT_SUPPORTED           = 0x129,
    NVME_SC_IOCS_NOT_ENABLED             = 0x12A,
    NVME_SC_IOCS_COMBINATION_REJECTED    = 0x12B,
    NVME_SC_INVALID_IOCS                 = 0x12C,

    /*
     * I/O Command Set Specific - NVM commands:
     */
    NVME_SC_BAD_ATTRIBUTES          = 0x180,
    NVME_SC_INVALID_PI              = 0x181,
    NVME_SC_READ_ONLY               = 0x182,
    NVME_SC_CMD_SIZE_LIMIT_EXCEEDED = 0x183,

    /*
     * I/O Command Set Specific - Fabrics commands:
     */
    NVME_SC_CONNECT_FORMAT        = 0x180,
    NVME_SC_CONNECT_CTRL_BUSY     = 0x181,
    NVME_SC_CONNECT_INVALID_PARAM = 0x182,
    NVME_SC_CONNECT_RESTART_DISC  = 0x183,
    NVME_SC_CONNECT_INVALID_HOST  = 0x184,

    NVME_SC_DISCOVERY_RESTART     = 0x190,
    NVME_SC_AUTH_REQUIRED         = 0x191,

    /*
     * I/O Command Set Specific - Zoned Namespace commands:
     */
    NVME_SC_ZONE_BOUNDARY_ERROR              = 0x1B8,
    NVME_SC_ZONE_IS_FULL                     = 0x1B9,
    NVME_SC_ZONE_IS_READ_ONLY                = 0x1BA,
    NVME_SC_ZONE_IS_OFFLINE                  = 0x1BB,
    NVME_SC_ZONE_INVALID_WRITE               = 0x1BC,
    NVME_SC_TOO_MANY_ACTIVE_ZONES            = 0x1BD,
    NVME_SC_TOO_MANY_OPEN_ZONES              = 0x1BE,
    NVME_SC_ZONE_INVALID_STATE_TRANSITION    = 0x1BF,

    /*
     * Media and Data Integrity Errors:
     */
    NVME_SC_WRITE_FAULT     = 0x280,
    NVME_SC_READ_ERROR      = 0x281,
    NVME_SC_GUARD_CHECK     = 0x282,
    NVME_SC_APPTAG_CHECK    = 0x283,
    NVME_SC_REFTAG_CHECK    = 0x284,
    NVME_SC_COMPARE_FAILED  = 0x285,
    NVME_SC_ACCESS_DENIED   = 0x286,
    NVME_SC_UNWRITTEN_BLOCK = 0x287,

    /*
     * Path-related Errors:
     */
    NVME_SC_INTERNAL_PATH_ERROR    = 0x300,
    NVME_SC_ANA_PERSISTENT_LOSS    = 0x301,
    NVME_SC_ANA_INACCESSIBLE       = 0x302,
    NVME_SC_ANA_TRANSITION         = 0x303,

    /*
     * Controller Detected Path errors
     */
    NVME_SC_CTRL_PATHING_ERROR    = 0x360,

    /*
    * Host Detected Path Errors
    */
    NVME_SC_HOST_PATHING_ERROR    = 0x370,
    NVME_SC_HOST_CMD_ABORT        = 0x371,

    NVME_SC_CRD            = 0x1800,
    NVME_SC_DNR            = 0x4000,
};

enum {
    NVME_DSMGMT_IDR        = 1 << 0,
    NVME_DSMGMT_IDW        = 1 << 1,
    NVME_DSMGMT_AD         = 1 << 2,
};


#define NVME_DSM_MAX_RANGES    256

struct nvme_dsm_range {
    __le32            cattr;
    __le32            nlb;
    __le64            slba;
};

struct nvme_id_ctrl {
    __le16          vid;
    __le16          ssvid;
    char            sn[20];
    char            mn[40];
    char            fr[8];
    __u8            rab;
    __u8            ieee[3];
    __u8            cmic;
    __u8            mdts;
    __le16          cntlid;
    __le32          ver;
    __le32          rtd3r;
    __le32          rtd3e;
    __le32          oaes;
    __le32          ctratt;
    __le16          rrls;
    __u8            rsvd102[9];
    __u8            cntrltype;
    char            fguid[16];
    __le16          crdt1;
    __le16          crdt2;
    __le16          crdt3;
    __u8            rsvd134[122];
    __le16          oacs;
    __u8            acl;
    __u8            aerl;
    __u8            frmw;
    __u8            lpa;
    __u8            elpe;
    __u8            npss;
    __u8            avscc;
    __u8            apsta;
    __le16          wctemp;
    __le16          cctemp;
    __le16          mtfa;
    __le32          hmpre;
    __le32          hmmin;
    __u8            tnvmcap[16];
    __u8            unvmcap[16];
    __le32          rpmbs;
    __le16          edstt;
    __u8            dsto;
    __u8            fwug;
    __le16          kas;
    __le16          hctma;
    __le16          mntmt;
    __le16          mxtmt;
    __le32          sanicap;
    __le32          hmminds;
    __le16          hmmaxd;
    __le16          nsetidmax;
    __le16          endgidmax;
    __u8            anatt;
    __u8            anacap;
    __le32          anagrpmax;
    __le32          nanagrpid;
    __le32          pels;
    __u8            rsvd356[156];
    __u8            sqes;
    __u8            cqes;
    __le16          maxcmd;
    __le32          nn;
    __le16          oncs;
    __le16          fuses;
    __u8            fna;
    __u8            vwc;
    __le16          awun;
    __le16          awupf;
    __u8            icsvscc;
    __u8            nwpc;
    __le16          acwu;
    __u8            rsvd534[2];
    __le32          sgls;
    __le32          mnan;
    __u8            rsvd544[224];
    char            subnqn[256];
    __u8            rsvd1024[768];
    __le32          ioccsz;
    __le32          iorcsz;
    __le16          icdoff;
    __u8            fcatt;
    __u8            msdbd;
    __le16          ofcs;
    __u8            rsvd1806[242];
    struct nvme_id_power_state    psd[32];
    __u8            vs[1024];
};

struct nvme_user_io {
    __u8    opcode;
    __u8    flags;
    __u16    control;
    __u16    nblocks;
    __u16    rsvd;
    __u64    metadata;
    __u64    addr;
    __u64    slba;
    __u32    dsmgmt;
    __u32    reftag;
    __u16    apptag;
    __u16    appmask;
};

struct nvme_passthru_cmd {
    __u8     opcode;
    __u8     flags;
    __u16    rsvd1;
    __u32    nsid;
    __u32    cdw2;
    __u32    cdw3;
    __u64    metadata;
    __u64    addr;
    __u32    metadata_len;
    __u32    data_len;
    __u32    cdw10;
    __u32    cdw11;
    __u32    cdw12;
    __u32    cdw13;
    __u32    cdw14;
    __u32    cdw15;
    __u32    timeout_ms;
    __u32    result;
};

struct nvme_passthru_cmd64 {
    __u8    opcode;
    __u8    flags;
    __u16   rsvd1;
    __u32   nsid;
    __u32   cdw2;
    __u32   cdw3;
    __u64   metadata;
    __u64   addr;
    __u32   metadata_len;
    __u32   data_len;
    __u32   cdw10;
    __u32   cdw11;
    __u32   cdw12;
    __u32   cdw13;
    __u32   cdw14;
    __u32   cdw15;
    __u32   timeout_ms;
    __u32   rsvd2;
    __u64   result;
};

#define nvme_admin_cmd nvme_passthru_cmd

#define __CHECK_ENDIAN__

#include <stdint.h>
#include <inttypes.h>
#include <linux/types.h>
#include <endian.h>

#ifdef __CHECKER__
#  define __force       __attribute__((force))
#else
#  define __force
#endif

static inline __le16 cpu_to_le16(uint16_t x)
{
	return (__force __le16)htole16(x);
}
static inline __le32 cpu_to_le32(uint32_t x)
{
	return (__force __le32)htole32(x);
}
static inline __le64 cpu_to_le64(uint64_t x)
{
	return (__force __le64)htole64(x);
}

static inline uint16_t le16_to_cpu(__le16 x)
{
	return le16toh((__force __u16)x);
}
static inline uint32_t le32_to_cpu(__le32 x)
{
	return le32toh((__force __u32)x);
}
static inline uint64_t le64_to_cpu(__le64 x)
{
	return le64toh((__force __u64)x);
}

struct nvme_lbaf {
	__le16			ms;
	__u8			ds;
	__u8			rp;
};

struct nvme_id_ns {
	__le64			nsze;
	__le64			ncap;
	__le64			nuse;
	__u8			nsfeat;
	__u8			nlbaf;
	__u8			flbas;
	__u8			mc;
	__u8			dpc;
	__u8			dps;
	__u8			nmic;
	__u8			rescap;
	__u8			fpi;
	__u8			dlfeat;
	__le16			nawun;
	__le16			nawupf;
	__le16			nacwu;
	__le16			nabsn;
	__le16			nabo;
	__le16			nabspf;
	__le16			noiob;
	__u8			nvmcap[16];
	__le16			npwg;
	__le16			npwa;
	__le16			npdg;
	__le16			npda;
	__le16			nows;
	__le16			mssrl;
	__le32			mcl;
	__u8			msrc;
	__u8			rsvd81[11];
	__le32			anagrpid;
	__u8			rsvd96[3];
	__u8			nsattr;
	__le16			nvmsetid;
	__le16			endgid;
	__u8			nguid[16];
	__u8			eui64[8];
	struct nvme_lbaf	lbaf[16];
	__u8			rsvd192[192];
	__u8			vs[3712];
};

#define NVME_IOCTL_ID           _IO('N', 0x40)
#define NVME_IOCTL_ADMIN_CMD    _IOWR('N', 0x41, struct nvme_admin_cmd)
#define NVME_IOCTL_SUBMIT_IO    _IOW('N', 0x42, struct nvme_user_io)
#define NVME_IOCTL_IO_CMD       _IOWR('N', 0x43, struct nvme_passthru_cmd)
#define NVME_IOCTL_RESET        _IO('N', 0x44)
#define NVME_IOCTL_SUBSYS_RESET _IO('N', 0x45)
#define NVME_IOCTL_RESCAN       _IO('N', 0x46)
#define NVME_IOCTL_ADMIN64_CMD  _IOWR('N', 0x47, struct nvme_passthru_cmd64)
#define NVME_IOCTL_IO64_CMD     _IOWR('N', 0x48, struct nvme_passthru_cmd64)

/* Generic passthrough */

int nvme_submit_passthru(int fd, unsigned long ioctl_cmd,
             struct nvme_passthru_cmd *cmd);
int nvme_submit_admin_passthru(int fd, struct nvme_passthru_cmd *cmd);
int nvme_submit_io_passthru(int fd, struct nvme_passthru_cmd *cmd);

int nvme_passthru(int fd, unsigned long ioctl_cmd, __u8 opcode, __u8 flags,
          __u16 rsvd, __u32 nsid, __u32 cdw2, __u32 cdw3,
          __u32 cdw10, __u32 cdw11, __u32 cdw12,
          __u32 cdw13, __u32 cdw14, __u32 cdw15,
          __u32 data_len, void *data, __u32 metadata_len,
          void *metadata, __u32 timeout_ms, __u32 *result);


 // NVME_SUBMIT_IO
int nvme_io(int fd, __u8 opcode, __u64 slba, __u16 nblocks, __u16 control,
	    __u32 dsmgmt, __u32 reftag, __u16 apptag, __u16 appmask, void *data,
	    void *metadata);

// NVME_IO_CMD
int nvme_passthru_io(int fd, __u8 opcode, __u8 flags, __u16 rsvd,
             __u32 nsid, __u32 cdw2, __u32 cdw3,
             __u32 cdw10, __u32 cdw11, __u32 cdw12,
             __u32 cdw13, __u32 cdw14, __u32 cdw15,
             __u32 data_len, void *data, __u32 metadata_len,
             void *metadata, __u32 timeout);

/* NVME_IO_CMD */
int nvme_write_zeros(int fd, __u32 nsid, __u64 slba, __u16 nlb,
		     __u16 control, __u32 reftag, __u16 apptag, __u16 appmask);

int nvme_write_uncorrectable(int fd, __u32 nsid, __u64 slba, __u16 nlb);

int nvme_verify(int fd, __u32 nsid, __u64 slba, __u16 nblocks,
        __u16 control, __u32 reftag, __u16 apptag, __u16 appmask);

int nvme_flush(int fd, __u32 nsid);

int nvme_dsm(int fd, __u32 nsid, __u32 cdw11, struct nvme_dsm_range *dsm,
         __u16 nr_ranges);
struct nvme_dsm_range *nvme_setup_dsm_range(int *ctx_attrs, int *llbas,
                        unsigned long long *slbas,

                        __u16 nr_ranges);
// NVME_ADMIN_CMD
int nvme_identify13(int fd, __u32 nsid, __u32 cdw10, __u32 cdw11, void *data);
int nvme_identify(int fd, __u32 nsid, __u32 cdw10, void *data);
int nvme_identify_ctrl(int fd, void *data);
int nvme_identify_ns(int fd, __u32 nsid, bool present, void *data);
int nvme_identify_ns_list(int fd, __u32 nsid, bool all, void *data);
int nvme_identify_ns_list_csi(int fd, __u32 nsid, __u8 csi, bool all, void *data);
int nvme_identify_ctrl_list(int fd, __u32 nsid, __u16 cntid, void *data);
int nvme_identify_ns_descs(int fd, __u32 nsid, void *data);
int nvme_identify_nvmset(int fd, __u16 nvmset_id, void *data);
int nvme_identify_uuid(int fd, void *data);
int nvme_identify_secondary_ctrl_list(int fd, __u32 nsid, __u16 cntid, void *data);
int nvme_identify_ns_granularity(int fd, void *data);
int nvme_identify_ctrl_nvm(int fd, void *data);
int nvme_zns_identify_ctrl(int fd, void *data);
int nvme_zns_identify_ns(int fd, __u32 nsid, void *data);
int nvme_identify_iocs(int fd, __u16 cntid, void *data);
const char *nvme_status_to_string(__u16 status);
void nvme_show_status(__u16 status);

#endif /* NVME_LIB_H__ */

