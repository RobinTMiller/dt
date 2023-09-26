/****************************************************************************
 *      								    *
 *      		  COPYRIGHT (c) 1988 - 2023     		    *
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
 * Module:      dtinfo.c
 * Author:      Robin T. Miller
 * Date:	September 8, 1993
 *
 * Description:
 *      Setup device and/or system information for 'dt' program.
 * 
 * Modification History:
 * 
 * September 20th, 2023 by Robin T. Miller
 *      When setting up device information, beware of overwriting the user
 * specified device type.
 * 
 * February 4th, 2020 by Robin T. Miller
 *      For Windows and existing file, set the filesize like we do for Unix!
 * 
 * December 28th, 2019 by Robin T. Miller
 *      For regular files, do NOT override the user specified block sizes
 * for regular files unless Direct I/O is specified, otherwise we cannot
 * perform non-block aligned I/O. In my haste to fix (override) incorrect
 * min/max/incr values for user specified device sizes (dsize=value), I
 * accidently broke this previous (desirable) file system behavior.
 * 
 * December 20th, 2019 by Robin T. Miller
 *      With changing dispose mode to KEEP_ON_ERROR, ensure existing files
 * do *not* get deleted by setting dispose to KEEP_FILE.
 * 
 * July 19th, 2019 by Robin T. Miller
 *      When user specifies an alternate device size (dsize=4k), ensure all
 * device size and I/O parameters are updated accordingly.
 * Note: This mismatch caused a false corruption due to I/O sizes NOT being
 * modulo the user device size, and did not enforce aligned I/O on the same.
 * When using block tags, the CRC generated on short reads was incorrect!
 * 
 * July 8th, 2019 by Robin T. Miller
 *      Update setup_device_info() to always call os_system_device_info() so
 * disk specific information from the OS via IOCTL gets setup properly. This
 * is only an issue when dtype=disk is specified, but if the IOCTL information
 * is less than the actual disk capacity, EINVAL occurs to unreachable offsets.
 * It's not invalid to return less disk capacity, since some may be reserved.
 * Note: Without refactoring code, the user device type may get overwritten!
 * 
 * April 12th, 2018 by Robin T. Miller
 *      For Linux, add function to set device block size. This is required
 * for file systems when direct I/O is enabled, where I/O sizes muct be
 * modulo the device size, otherwise EINVAL errors occur (misleading).
 * 
 * June 15th, 2014 by Robin T. Miller
 * 	On Linux for direct disks, ensure the DIO flag is set true, since
 * new logic for handling Direct I/O and buffering mode, requires this!
 *
 * June 7th, 2014 by Robin T. Miller
 * 	For regular files that exist, use consistent logic for setting the
 * user capacity for both reads and writes. This is consistent with dt v18,
 * and required so random reads will match random writes. Failure to do this
 * results in the wrong read offsets and thus false data corruptions! :-(
 * 
 * Auguest 17th, 2013 by Robin T. Miller
 *	Always setup the disk capacity, if we can acquire it, since new tests
 * and sanity checks like copy/verify need this information. Previosuly, the
 * capacity was only set for random I/O or slices, so sequentail I/O could
 * test EOF/EOM conditions. This latter testing proved problematic for some OS's,
 * such as Windows and Solaris, since apparently POSIX does NOT define proper end
 * of media errors for direct (raw) disk access.
 *
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */
#include "dt.h"
#if !defined(_QNX_SOURCE)
#  if !defined(sun) && !defined(WIN32)
#    include <sys/ioctl.h>
#  endif /* !defined(sun) */
#endif /* !defined(_QNX_SOURCE) */
#include <sys/stat.h>

/*
 * Forward References:
 */
void setup_device_defaults(struct dinfo *dip);
void SetupRegularFile(struct dinfo *dip, large_t file_size);
#if defined(DEC)
int SetupDiskAttributes(struct dinfo *dip, int fd);
#endif /* defined(DEC) */
void os_system_device_info(struct dinfo *dip);
large_t	GetMaxUserCapacity(dinfo_t *dip, hbool_t use_records);

/************************************************************************
 *      								*
 * setup_device_type() - Setup the device type. 			*
 *      								*
 * Description: 							*
 *      This function sets up the specific device type to be tested.    *
 * Since each operating system has a different method of identifying    *
 * devices, so this table lookup allows the user to specify the device  *
 * type.  Of course, this could be used to force errors.		*
 *      								*
 * Inputs:      str = Pointer to device type string.    		*
 *      								*
 * Return Value:							*
 *      	Returns pointer to device type table entry or NULL.     *
 *      								*
 ************************************************************************/
struct dtype dtype_table[] = {
	{ "block",      DT_BLOCK	},
	{ "character",  DT_CHARACTER    },
	{ "comm",       DT_COMM 	},
	{ "disk",       DT_DISK 	},
	{ "directory",  DT_DIRECTORY    },
	{ "graphics",   DT_GRAPHICS     },
	{ "memory",     DT_MEMORY       },
	{ "mmap",       DT_MMAP 	},
	{ "network",    DT_NETWORK      },
	{ "pipe",       DT_PIPE 	},
	{ "processor",  DT_PROCESSOR    },
	{ "regular",    DT_REGULAR      },
	{ "socket",     DT_SOCKET       },
	{ "special",    DT_SPECIAL      },
	{ "streams",    DT_STREAMS      },
	{ "tape",       DT_TAPE 	},
	{ "unknown",    DT_UNKNOWN      }
};
int num_dtypes = sizeof(dtype_table) / sizeof(dtype_table[0]);

struct dtype *
setup_device_type(char *str)
{
    int i;
    struct dtype *dtp;

    for (dtp = dtype_table, i = 0; i < num_dtypes; i++, dtp++) {
	if (strcmp(str, dtp->dt_type) == 0) {
	    return (dtp);
	}
    }
    Fprint(NULL, "Device type '%s' is invalid, valid entrys are:\n", str);
    for (dtp = dtype_table, i = 0; i < num_dtypes; i++, dtp++) {
	if ( (i % 4) == 0) Fprint(NULL, "\n");
	Fprint(NULL, "    %-12s", dtp->dt_type);
    }
    Fprint(NULL, "\n");
    return ((struct dtype *) 0);
}

/************************************************************************
 *      								*
 * setup_device_defaults() - Setup the device defaults. 		*
 *      								*
 * Description: 							*
 *      This function sets up the specific device type defaults, for    *
 * test parameters which were not specified.    			*
 *      								*
 * Inputs:      dip = The device information pointer.   		*
 *      								*
 * Return Value:							*
 *      	Returns pointer to device type table entry or NULL.     *
 *      								*
 * Note: This function may get called twice!  On Tru64 Unix, after we   *
 *       open the device, the initial device type may get overridden.   *
 *      								*
 ************************************************************************/
void
setup_device_defaults(struct dinfo *dip)
{
    struct dtype *dtp = dip->di_dtype;

    if ( (dtp->dt_dtype == DT_BLOCK)   ||
	 (dtp->dt_dtype == DT_DISK)    ||
	 (dtp->dt_dtype == DT_MMAP)    ||
	 (dtp->dt_dtype == DT_REGULAR) || dip->di_random_io ) {
	/*
	 * Note: For regular files without DIO, the device size should
	 * be set to one (1), since that's the smallest I/O transfer size.
	 * But that said, changing this now may break other sanity checks!
	 * Why bother? We can't do modulo dsize/bs sanity checks as it is!
	 */
	if (dip->di_debug_flag) {
	    Printf(dip, "Device size: %u, Real Device Size: %u, User Device Size: %u\n",
		   dip->di_dsize, dip->di_rdsize, dip->di_device_size);
	}
	if (dip->di_device_size) {
	    dip->di_dsize = dip->di_device_size;	/* Override, with user dsize! */
	}
	if ((dip->di_device_size == 0) && dip->di_rdsize) {
	    dip->di_device_size = dip->di_rdsize;	/* Use real device block size. */
	}
	if (dip->di_device_size == 0) {
	    dip->di_device_size = BLOCK_SIZE;		/* Set our default block size. */
	}
	if (dip->di_lbdata_size == 0) {
	    dip->di_lbdata_size = dip->di_device_size;	/* Set lbdata size also for IOT. */
	}
	if (dip->di_max_size && (dip->di_user_min == False)) {
	    dip->di_min_size = dip->di_device_size;	/* Set a min value, if none specified. */
	}
	if (dip->di_min_size && (dip->di_user_incr == False)) {
	    dip->di_incr_count = dip->di_device_size;	/* Set increment value, if none specified. */
	}
	/* Ensure min and incr values are non-zero, if user specified ranges. */
	if (dip->di_max_size && (dip->di_min_size == 0)) {
	    dip->di_min_size = dip->di_device_size;	/* Set a min value, required with max. */
	}
	if (dip->di_min_size && (dip->di_incr_count == 0)) {
	    dip->di_incr_count = dip->di_device_size;	/* Set an incr value, required with min. */
	}
	/* Ensure variable sizes are in line with the device size (user or OS block size). */
	/* When using direct I/O or specifying a device size, we override to make correct. */
	/* Note to self: If user values are *not* correct, then options need updated! */
	if ( isDiskDevice(dip) || (dip->di_dio_flag == True) ) {
	    if (dip->di_block_size < dip->di_device_size) {
		if (dip->di_debug_flag) {
		    Wprintf(dip, "Block size %u, overridden with device size %u.\n",
			    dip->di_block_size, dip->di_device_size);
		}
		dip->di_block_size = dip->di_device_size;
	    }
	    if (dip->di_min_size && (dip->di_min_size < dip->di_device_size)) {
		if (dip->di_debug_flag) {
		    Wprintf(dip, "Minimum size %u, overridden with device size %u.\n",
			    dip->di_min_size, dip->di_device_size);
		}
		dip->di_min_size = dip->di_device_size;
	    }
	    if (dip->di_max_size && (dip->di_max_size < dip->di_device_size)) {
		if (dip->di_debug_flag) {
		    Wprintf(dip, "Maximum size %u, overridden with device size %u.\n",
			    dip->di_max_size, dip->di_device_size);
		}
		dip->di_max_size = dip->di_device_size;
	    }
	    if (dip->di_incr_count && (dip->di_incr_count < dip->di_device_size)) {
		if (dip->di_debug_flag) {
		    Wprintf(dip, "Increment count %u, overridden with device size %u.\n",
			    dip->di_incr_count, dip->di_device_size);
		}
		dip->di_incr_count = dip->di_device_size;
	    }
	}
	/* End of device size sanity checks! */
	if (dip->di_fsalign_flag && dip->di_random_io) {
	    if (dip->di_random_align == 0) {
		dip->di_random_align = dip->di_device_size; /* Align to device size. */
	    }
	}
	if (dip->di_sleep_res == SLEEP_DEFAULT) {
	    dip->di_sleep_res = SLEEP_USECS;   		/* Disks get microsecond delays! */
	}
	if (dip->di_fsync_flag == UNINITIALIZED) {
	    if ( (dtp->dt_dtype == DT_BLOCK)   ||
		 (dtp->dt_dtype == DT_REGULAR) ) {
		dip->di_fsync_flag = True;
	    } else if (dtp->dt_dtype == DT_DISK) {
		/*
		 * Devices identified as DT_DISK should be the raw (character) device. 
		 * Since some OS's, such as AIX don't like fsync() to disks, we'll disable 
		 * it since it really only has meaning to block or regular (FS) files.
		 */
		dip->di_fsync_flag = False;
	    }
	}
	/*
	 * Additional setup for direct disk testing.
	 */
	if (dtp->dt_dtype == DT_DISK) {
	    if (dip->di_block_size < dip->di_device_size) {
		dip->di_block_size = dip->di_device_size;
	    }
#if defined(__linux__)
	    dip->di_dio_flag = True;
	    dip->di_open_flags |= O_DIRECT;
#endif /* defined(__linux__) */
	}
    } else { /* Tapes, pipes, serial lines, etc. */
	if (!dip->di_device_size) dip->di_device_size = 1;
	if (!dip->di_lbdata_size) dip->di_lbdata_size = dip->di_device_size;
	if (dip->di_max_size && !dip->di_user_min) dip->di_min_size = 1;
	if (dip->di_min_size && !dip->di_user_incr) dip->di_incr_count = 1;
	/* Ensure min and incr values are non-zero! */
	if (dip->di_max_size && !dip->di_min_size) dip->di_min_size = 1;
	if (dip->di_min_size && !dip->di_incr_count) dip->di_incr_count = 1;
	dip->di_fsync_flag = False;
    }
    return;
}

/************************************************************************
 *      								*
 * os_system_device_info() - Get OS System Device Information.  	*
 *      								*
 * Description: 							*
 *      This function attempts to obtain device information necessary   *
 * for device specific testing, by using system dependent syscalls.     *
 * Note: This function is called _after_ the device/file is opened.     *
 *      								*
 * Inputs:      dip = The device information pointer.   		*
 *      								*
 * Return Value:							*
 *      	None.   						*
 *      								*
 ************************************************************************/
#if defined(AIX)

#include <sys/devinfo.h>

void
os_system_device_info(struct dinfo *dip)
{
    struct devinfo devinfo;
    struct devinfo *devinfop = &devinfo;
    int fd = dip->di_fd;
    hbool_t temp_fd = False;
    short category;
    int i;
	
    if (fd == NoFd) {
	temp_fd = True;
	if ( (fd = open(dip->di_dname, O_RDONLY)) < 0) {
	    return;
	}
    }

    (void)memset(devinfop, '\0', sizeof(*devinfop));
    if (ioctl (fd, IOCINFO, devinfop) == SUCCESS) {

	switch (devinfop->devtype) {

	    case DD_DISK: { /* Includes LV's! */
		if (devinfop->flags & DF_LGDSK) {
		    dip->di_rdsize = devinfop->un.dk64.bytpsec;
		    if (!dip->di_dsize) dip->di_dsize = dip->di_rdsize;
		    dip->di_capacity = (large_t)
				       ((u_int64_t)devinfop->un.dk64.hi_numblks << 32L) |
					(uint32_t)devinfop->un.dk64.lo_numblks;
		} else {
		    dip->di_rdsize = devinfop->un.dk.bytpsec;
		    if (!dip->di_dsize) dip->di_dsize = dip->di_rdsize;
		    dip->di_capacity = (large_t)devinfop->un.dk.numblks;
		}
		break;
	    }
	    case DD_SCDISK: {
		if (devinfop->flags & DF_LGDSK) {
		    dip->di_rdsize = devinfop->un.scdk64.blksize;
		    if (!dip->di_dsize) dip->di_dsize = dip->di_rdsize;
		    dip->di_capacity = (large_t)
				       ((u_int64_t)devinfop->un.scdk64.hi_numblks << 32L) |
					(uint32_t)devinfop->un.scdk64.lo_numblks;
		} else {
		    dip->di_rdsize = devinfop->un.scdk.blksize;
		    if (!dip->di_dsize) dip->di_dsize = dip->di_rdsize;
		    dip->di_capacity = (large_t)devinfop->un.scdk.numblks;
		}
		break;
	    }
	    case DD_TAPE:
	    case DD_SCTAPE:
		dip->di_dtype = setup_device_type("tape");
		break;

	    default:
		break;
	}

	/*
	 * Common Disk Setup:
	 */
	if ( (devinfop->devtype == DD_DISK) || (devinfop->devtype == DD_SCDISK) ) {
	    if ( (dip->di_max_capacity == False) && !dip->di_user_capacity ) {
		dip->di_max_capacity = True;
		dip->di_user_capacity = (dip->di_capacity * (large_t)dip->di_rdsize);
	    }
	    if (dip->di_debug_flag) {
		Printf(dip, "IOCINFO Capacity: " LUF " blocks, device size %u bytes.\n",
		       dip->di_capacity, dip->di_dsize);
	    }
	    if (dip->di_dsize && !dip->di_user_lbsize && !dip->di_lbdata_size) {
		dip->di_lbdata_size = dip->di_dsize;
	    }
	    dip->di_dtype = setup_device_type("disk");
	}
    }

    if (temp_fd) (void)close(fd);
    return;
}

#endif /* defined(AIX) */

#if defined(DEC)

#include <sys/devio.h>
#include <sys/ioctl.h>
#if defined(DEC)
#  include <sys/ioctl_compat.h>
#  include <io/common/devgetinfo.h>
#endif /* defined(DEC) */

void
os_system_device_info(struct dinfo *dip)
{
    struct devget devget, *devgp = NULL;
    device_info_t devinfo, *devip = NULL;
    int fd = dip->di_fd;
    hbool_t temp_fd = False;
    short category;
    int i, status;

    if (fd == NoFd) {
	temp_fd = True;
	if ( (fd = open (dip->di_dname, (O_RDONLY | O_NDELAY))) < 0) {
	    return;
	}
    }

    /*
     * Attempt to obtain the device information.
     */
    memset(&devinfo, '\0', sizeof(devinfo));
    if (ioctl (fd, DEVGETINFO, (char *) &devinfo) == SUCCESS) {
	devip = &devinfo;
	category = devip->v1.category;
	if ( NEL (devip->v1.device, DEV_UNKNOWN, DEV_STRING_SIZE) ) {
	    dip->di_device = Malloc(dip, (DEV_STRING_SIZE + 1) );
	    (void) strncpy (dip->di_device, devip->v1.device, DEV_STRING_SIZE);
	} else if ( NEL (devip->v1.dev_name, DEV_UNKNOWN, DEV_STRING_SIZE) ) {
	    dip->di_device = Malloc(dip, (DEV_STRING_SIZE + 1) );
	    (void) strncpy (dip->di_device, devip->v1.dev_name, DEV_STRING_SIZE);
	}
	if (dip->di_device) {
	    /*
	     * In Steel, device names have trailing spaces. grrr!
	     */
	    for (i = (DEV_STRING_SIZE); i--; ) {
		if ( isspace(dip->di_device[i]) ) {
		    dip->di_device[i] = '\0';
		} else {
		    break;
		}
	    }
	}
    } else { /* Try the old DEVIOCGET IOCTL... */

	memset(&devget, '\0', sizeof(devget));
	if (ioctl (fd, DEVIOCGET, (char *) &devget) < 0) {
	    if (temp_fd) (void)close(fd);
	    return;
	}
	devgp = &devget;
	category = devgp->category;
	if ( NEL (devgp->device, DEV_UNKNOWN, DEV_SIZE) ) {
	    dip->di_device = Malloc(dip, (DEV_SIZE + 1) );
	    (void) strncpy (dip->di_device, devgp->device, DEV_SIZE);
	} else if ( NEL (devgp->dev_name, DEV_UNKNOWN, DEV_SIZE) ) {
	    dip->di_device = Malloc(dip, (DEV_SIZE + 1) );
	    (void) strncpy (dip->di_device, devgp->dev_name, DEV_SIZE);
	}
	if (dip->di_device) {
	    /*
	     * In Steel, device names have trailing spaces. grrr!
	     */
	    for (i = (DEV_SIZE); i--; ) {
		if ( isspace(dip->di_device[i]) ) {
		    dip->di_device[i] = '\0';
		} else {
		    break;
		}
	    }
	}
    }

    /*
     * Setup the device type based on the category.
     */
    switch (category) {

	case DEV_TAPE:  		/* Tape category. */
	    dip->di_dtype = setup_device_type("tape");
	    break;

	case DEV_DISK: {		/* Disk category. */
	    /*
	     * If using partition 'c', setup to use the whole capacity.
	     */
	    if (dip->di_dname[strlen(dip->di_dname)-1] == 'c') {
		if ( (dip->di_max_capacity == False) && !dip->di_user_capacity ) {
		    dip->di_max_capacity = True;
		}
	    }
	    /****************************************************************
	     * Attempt to get disk attributes using DEVGETINFO first, since *
	     * for SCSI disks we get more information, which we plan to use *
	     * one day, and we also get the real block (sector) size.       *
	     ****************************************************************/
	    if (devip && (devip->version == VERSION_1) ) {
		v1_disk_dev_info_t *diskinfo;
		diskinfo = &devip->v1.devinfo.disk;
		dip->di_rdsize = diskinfo->blocksz;
		if (!dip->di_dsize) dip->di_dsize = dip->di_rdsize;
		/*
		 * NOTE: capacity is whole disk, not the open partition,
		 *       so we don't use it unless selected by the user.
		 */
		if ( (dip->di_max_capacity == False) && !dip->di_user_capacity ) {
		    dip->di_capacity = diskinfo->capacity;
		    dip->di_user_capacity = (dip->di_capacity * (large_t)dip->di_rdsize);
		    if (dip->di_debug_flag) {
			Printf(dip, "DEVGETINFO Capacity: " LUF " blocks.\n", dip->di_capacity);
		    }
		}
		if (dip->di_dsize && !dip->di_user_lbsize && !dip->di_lbdata_size) {
		    dip->di_lbdata_size = dip->di_dsize;
		}
	    } else {
		(void)SetupDiskAttributes(dip, fd);
	    }
	    dip->di_dtype = setup_device_type("disk");
	    /*
	     * TODO: Need to read disklabel to pickup partition sizes,
	     *       and to check for mounted file systems. More work!
	     */
	    break;
	}

	case DEV_SPECIAL:       	/* Special category. */
	    /*
	     * On Tru64 Unix, LSM volumes are really disks!
	     */
	    if (SetupDiskAttributes(dip, fd) != SUCCESS) {
		dip->di_dtype = setup_device_type("special");
	    }
	    break;

	default:
	    break;
    }
    if (temp_fd) (void)close(fd);
    return;
}

/*
 * SetupDiskAttributes() - Setup Disk Attributes using DEVGETGEOM.
 *
 * Description:
 *      This function is used for disk devices which don't support
 * the newer DEVGETINFO IOCTL, like LSM devices.
 *
 * Inputs:
 *      dip = The device information pointer.
 *      fd = The file descriptor (NoFd == Not open).
 *
 * Outputs:
 *      Returns 0 or -1 for Success/Failure.
 */
int
SetupDiskAttributes (struct dinfo *dip, int fd)
{
    int status;
    hbool_t temp_fd = False;
    DEVGEOMST devgeom;

    if (fd == NoFd) {
	temp_fd = True;
	if ( (fd = open (dip->di_dname, O_RDONLY)) < 0) {
	    return (FAILURE);
	}
    }

    /*
     * If using partition 'c', setup to use the whole capacity.
     *
     * Note: Only setup maximum capacity for random I/O, or else
     * we will inhibit End of Media (EOM) testing.
     */
    if (dip->di_random_io || dip->di_slices) {
	if ( (dip->di_device && EQ(dip->di_device,"LSM")) ||
	     (dip->di_dname[strlen(dip->di_dname)-1] == 'c') ) {
	    if ( (dip->di_max_capacity == False) && !dip->di_user_capacity ) {
		dip->di_max_capacity = True;
	    }
	}
    }

    /*
     * Attempt to obtain the disk geometry.  Works for LSM, etc.
     *
     * NOTE: DEVGETGEOM *fails* on read-only devices (shit!).
     */
    memset(&devgeom, '\0', sizeof(devgeom));
    if ((status = ioctl (fd, DEVGETGEOM, (char *) &devgeom)) == SUCCESS) {
	dip->di_rdsize = devgeom.geom_info.sector_size;
	if (!dip->di_dsize) dip->di_dsize = dip->di_rdsize;
	/*
	 * NOTE: dev_size is whole disk, not the open partition,
	 *       so we don't use it unless selected by the user.
	 */
	if ( (dip->di_max_capacity == False) && !dip->di_user_capacity ) {
	    dip->di_capacity = devgeom.geom_info.dev_size;
	    dip->di_user_capacity = (dip->di_capacity * (large_t)dip->di_rdsize);
	    if (dip->di_debug_flag) {
		Printf(dip, "DEVGETGEOM Capacity: " LUF " blocks.\n", dip->di_capacity);
	    }
	}
	if (dip->di_dsize && !dip->di_user_lbsize && !dip->di_lbdata_size) {
	    dip->di_lbdata_size = dip->di_dsize;
	}
	dip->di_dtype = setup_device_type("disk");
    }
    /*
     * TODO: Need to read disklabel to pickup partition sizes,
     *       and to check for mounted file systems. More work!
     */
    if (temp_fd) (void)close(fd);
    return (status);
}

#endif /* defined(DEC) */

#if defined(HP_UX)

#include <sys/diskio.h>
#include <sys/scsi.h>

static int get_queue_depth(dinfo_t *dip, int fd, unsigned int *qdepth);
static int set_queue_depth(dinfo_t *dip, int fd, unsigned int qdepth);

void
os_system_device_info(struct dinfo *dip)
{
    disk_describe_type disk_type, *disktp = &disk_type;
    union inquiry_data inquiry;
    int fd = dip->di_fd;
    hbool_t temp_fd = False;
    short category;
    int i;

    if (fd == NoFd) {
	temp_fd = True;
	if ( (fd = open (dip->di_dname, (O_RDONLY | O_NDELAY))) < 0) {
	    return;
	}
    }

    /*
     * Attempt to obtain the device information.
     */
    memset(disktp, '\0', sizeof(*disktp));
    if (ioctl (fd, DIOC_DESCRIBE, disktp) == SUCCESS) {
	if (disktp->dev_type != UNKNOWN_DEV_TYPE) {
	    size_t size = sizeof(disktp->model_num);
	    dip->di_device = Malloc(dip, (size + 1) );
	    (void) strncpy (dip->di_device, disktp->model_num, size);
	    /*
	     * Strip trailing spaces from the device name.
	     */
	    for (i = size; i--; ) {
		if ( isspace(dip->di_device[i]) ) {
		    dip->di_device[i] = '\0';
		} else {
		    break;
		}
	    }
	    dip->di_rdsize = disktp->lgblksz;
	    if (!dip->di_dsize) dip->di_dsize = dip->di_rdsize;
	    if ( (dip->di_max_capacity == False) && !dip->di_user_capacity ) {
		dip->di_max_capacity = True;
		dip->di_capacity = (disktp->maxsva + 1);
		dip->di_user_capacity = (dip->di_capacity * (large_t)dip->di_rdsize);
		if (dip->di_debug_flag) {
		    Printf(dip, "DIOC_DESCRIBE Capacity: " LUF " blocks (%u byte blocks).\n",
				    dip->di_capacity, dip->di_rdsize);
		}
	    }
	}

	switch (disktp->dev_type) {

	    case CDROM_DEV_TYPE:	/* CDROM device */
	    case DISK_DEV_TYPE: 	/* Disk device */
	    case WORM_DEV_TYPE: 	/* Write once read many optical device */
	    case MO_DEV_TYPE:   	/* Magneto Optical device */
		dip->di_dtype = setup_device_type("disk");
		if (dip->di_qdepth != 0xFFFFFFFF) {
		    (void)set_queue_depth(dip, fd, dip->di_qdepth);
		}       						
		break;

	    case CTD_DEV_TYPE:  	/* Cartridge tape device */
		dip->di_dtype = setup_device_type("tape");
		break;

	    default:
		break;
	}
    } else if (ioctl (fd, SIOC_INQUIRY, &inquiry) == SUCCESS) {
	struct inquiry_2 *inq = (struct inquiry_2 *)&inquiry;
	size_t size = sizeof(inq->product_id);

	if (dip->di_debug_flag) {
	    Printf(dip, "SIOC_INQUIRY device type %u\n", inq->dev_type);
	}
	dip->di_device = Malloc(dip, (size + 1) );
	(void) strncpy (dip->di_device, inq->product_id, size);
	for (i = size; i--; ) {
	    if ( isspace(dip->di_device[i]) ) {
		dip->di_device[i] = '\0';
	    } else {
		break;
	    }
	}

	switch (inq->dev_type) {

	    case SCSI_DIRECT_ACCESS:
	    case SCSI_WORM:
	    case SCSI_CDROM:
	    case SCSI_MO:
		dip->di_dtype = setup_device_type("disk");
		if (dip->di_qdepth != 0xFFFFFFFF) {
		    (void)set_queue_depth(dip, fd, dip->di_qdepth);
		}       						
		break;

	    case SCSI_SEQUENTIAL_ACCESS:
		dip->di_dtype = setup_device_type("tape");
		break;

	    default:
		break;
	}
    }
    if (temp_fd) (void)close(fd);
    return;
}

static int
get_queue_depth(dinfo_t *dip, int fd, unsigned int *qdepth)
{
    struct sioc_lun_limits lun_limits;
    int status;

    (void)memset(&lun_limits, '\0', sizeof(lun_limits));
    if ( (status = ioctl(fd, SIOC_GET_LUN_LIMITS, &lun_limits)) < 0) {
	if (dip->di_debug_flag) {
	    perror("SIOC_SET_LUN_LIMITS failed");
	}
    } else {
	*qdepth = lun_limits.max_q_depth;
    }
    return (status);
}

static int
set_queue_depth(dinfo_t *dip, int fd, unsigned int qdepth)
{
    struct sioc_lun_limits lun_limits;
    int status;

    if (dip->di_debug_flag) {
	unsigned int qd;
	if (get_queue_depth (dip, fd, &qd) == 0) {
	    Printf(dip, "Current queue depth is %u\n", qd);
	}
    }
    (void)memset(&lun_limits, '\0', sizeof(lun_limits));
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
    if ( (status = ioctl(fd, SIOC_SET_LUN_LIMITS, &lun_limits)) < 0) {
	if (dip->di_debug_flag) {
	    perror("SIOC_SET_LUN_LIMITS failed");
	}
    } else if (dip->di_debug_flag) {
	Printf(dip, "Queue depth set to %u\n", qdepth);
    }
    return (status);
}

#endif /* defined(HP_UX) */

#if defined(__linux__)

/* Ugly stuff to avoid conflict with Linux BLOCK_SIZE definition. */
#undef BLOCK_SIZE
#include <linux/fs.h>
#undef BLOCK_SIZE
#define BLOCK_SIZE 512

void
os_system_device_info(struct dinfo *dip)
{
    int fd = dip->di_fd;
    hbool_t temp_fd = False;
    unsigned long nr_sects;
    int sect_size;

    if (fd == NoFd) {
	temp_fd = True;
	if ( (fd = open (dip->di_dname, (O_RDONLY | O_NDELAY))) < 0) {
	    return;
	}
    }

    /*
     * Try to obtain the sector size.
     */
    if (ioctl (fd, BLKSSZGET, &sect_size) == SUCCESS) {
	dip->di_rdsize = sect_size;
	if (!dip->di_dsize) dip->di_dsize = dip->di_rdsize;
	if (dip->di_debug_flag) {
	    Printf(dip, "BLKSSZGET Sector Size: %u bytes\n", dip->di_rdsize);
	}
	dip->di_dtype = setup_device_type("disk");
    }

    /*
     * If this IOCTL succeeds, we will assume it's a disk device.
     *
     * Note: The size returned is for the partition (thank-you!).
     */
    if (ioctl (fd, BLKGETSIZE, &nr_sects) == SUCCESS) {
	if (!dip->di_rdsize) dip->di_rdsize = BLOCK_SIZE;
	if ( (dip->di_max_capacity == False) && !dip->di_user_capacity ) {
	    dip->di_max_capacity = True;
	    dip->di_capacity = nr_sects;
	    dip->di_user_capacity = (dip->di_capacity * (large_t)BLOCK_SIZE);
	    if (dip->di_debug_flag || (dip->di_capacity == 0) ) {
		Printf(dip, "BLKGETSIZE Capacity: " LUF " blocks (%u byte blocks).\n",
		       dip->di_capacity, BLOCK_SIZE);
	    }
	}
    }

    if (temp_fd) (void)close(fd);
    return;
}

/* TODO: Move OS functions to OS file and decouple from dt. */
/* Note: This is a duplication of above, but does not set device type. */
/*       This is being called for mounted file systems to get device size. */

void
os_get_block_size(dinfo_t *dip, int fd, char *device_name)
{
    hbool_t temp_fd = False;
    int sect_size;

    if (fd == NoFd) {
	temp_fd = True;
	/* Note: Only works when running as root, of course! */
	if ( (fd = open(device_name, (O_RDONLY | O_NDELAY))) < 0) {
	    if (dip->di_debug_flag) {
		os_error_t error = os_get_error();
		INIT_ERROR_INFO(eip, device_name, OS_OPEN_FILE_OP, OPEN_OP,
				NULL, 0, (Offset_t)0, (size_t)0,
				error, logLevelWarn, PRT_NOFLAGS,
				(RPT_NORETRYS|RPT_NODEVINFO|RPT_NOERRORNUM|RPT_NOHISTORY|RPT_NOXERRORS));
		(void)ReportRetryableError(dip, eip, "Failed to open file %s", device_name);
	    }
	    return;
	}
    }

    /*
     * Try to obtain the sector size. (actually works with some file systems)
     */
    if (ioctl (fd, BLKSSZGET, &sect_size) == SUCCESS) {
	dip->di_rdsize = sect_size;
	/* Note: The device size may have been set by user device size! */
	if (!dip->di_dsize) dip->di_dsize = dip->di_rdsize;
	//dip->di_dsize = dip->di_rdsize; /* override previous dsize! */
	if (dip->di_debug_flag) {
	    Printf(dip, "BLKSSZGET Sector Size: %u bytes\n", dip->di_rdsize);
	}
    }
    if (temp_fd) (void)close(fd);
    return;
}

#endif /* defined(__linux__) */

#if defined(WIN32)

void
os_system_device_info(struct dinfo *dip)
{
    HANDLE      	  fd;
    STORAGE_DEVICE_NUMBER sdn;
    PSTORAGE_DEVICE_NUMBER psdn = &sdn;
    DWORD       	  count;

    if ( (fd = dip->di_fd) == INVALID_HANDLE_VALUE) {
	fd = CreateFile(dip->di_dname, GENERIC_READ, (FILE_SHARE_READ|FILE_SHARE_WRITE), NULL, OPEN_EXISTING, 0, NULL);
	if (fd == INVALID_HANDLE_VALUE) return;
    }

    if (!DeviceIoControl(fd, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, psdn, sizeof(*psdn), &count, NULL)) {
	if (dip->di_fd == INVALID_HANDLE_VALUE) {
	    (void)CloseHandle(fd);
	}
	return;
    }
    switch (psdn->DeviceType) {
	case FILE_DEVICE_DISK: {
	    DISK_GEOMETRY_EX  dg;
	    PDISK_GEOMETRY_EX pdg = &dg;
	    if (dip->di_debug_flag) {
		Printf(dip, "Device type is %d (FILE_DEVICE_DISK)\n",  psdn->DeviceType);
	    }
	    if (DeviceIoControl(fd, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, pdg, sizeof(*pdg), &count, NULL)) {
		dip->di_rdsize = pdg->Geometry.BytesPerSector;
		if (!dip->di_dsize) dip->di_dsize = dip->di_rdsize;
		if ( (dip->di_max_capacity == False) && !dip->di_user_capacity ) {
		    dip->di_max_capacity = True;
		    dip->di_capacity = (large_t)pdg->DiskSize.QuadPart;
		    dip->di_capacity /= dip->di_rdsize;        /* Capacity in blocks. */
		    dip->di_user_capacity = (large_t)pdg->DiskSize.QuadPart; /* In bytes! */
		    if (dip->di_debug_flag) {
			Printf(dip, "DISK_GEOMETRY_EX Capacity: " LUF " blocks (%u byte blocks).\n",
			       dip->di_capacity, dip->di_rdsize);
		    }
		}
	    }
	    dip->di_random_access = True;
	    dip->di_dtype = setup_device_type("disk");
	    setup_device_defaults(dip);
	    break;
	}
	default:
	    if (dip->di_debug_flag) {
		Printf(dip, "Device type is %d, no special setup performed!\n",  psdn->DeviceType);
	    }
	    break;
    }
    if (dip->di_fd == INVALID_HANDLE_VALUE) {
	(void)CloseHandle(fd);
    }
    return;
}

#endif /* defined(WIN32) */

/*
 * setup_device_info() - Setup Initial Device Information.
 *
 * Description:
 *      This function allocates a device information entry, and does
 * the initial setup of certain information based on known options.
 * This function is meant to be called prior to opening the device so
 * test specific functions are known for initial processing.
 * 
 * Inputs:
 *	dip = The device information pointer.
 *	dname = The device name.
 *	dtp = The device type pointer.
 *
 * Outputs:
 *	Various device information is initialized, including the device type.
 *
 * Return Value:
 * 	Returns SUCCESS/FAILURE
 */
int
setup_device_info(struct dinfo *dip, char *dname, struct dtype *dtp)
{
#if !defined(WIN32)
    struct stat sb;
#endif

    /*
     * Don't reset the functions if already set by another I/O behavior. 
     */
    if (dip->di_funcs == NULL) {
	dip->di_funcs = &generic_funcs;
#if defined(AIO)
	if (dip->di_aio_flag) {
	    dip->di_funcs = &aio_funcs;
	}
#endif /* defined(AIO) */
#if defined(MMAP)
	if (dip->di_mmap_flag) {
	    dip->di_funcs = &mmap_funcs;
	    dtp = setup_device_type("mmap");
	}
#endif /* defined(MMAP) */
    }

    /*
     * Setup the user specified device size (if any). 
     * By setting here, OS device setup leaves alone!
     */
    if (dip->di_device_size) {
	dip->di_dsize = dip->di_device_size;
    }

#if defined(DEC) || defined(HP_UX) || defined(__linux__) || defined(AIX) || defined(WIN32)
    /*
     * Must do this early on, to set device type and size.
     *
     * TODO: Create stub and remove ugly conditionalization!
     */
    if (True /*dtp == NULL*/) {     /* Note: Why always do this OS system setup? (CRS) */
	os_system_device_info(dip);
        if ( dip->di_dtype ) {
            dtp = dip->di_dtype;
        }
    }
#endif /* defined(DEC) || defined(HP_UX) || defined(__linux__) || defined(AIX) || defined(WIN32) */

    /*
     * If user specified a device type, don't override it.
     */
    if (dtp == NULL) {
	/*
	 * Determine test functions based on device name.
	 */
#if defined(WIN32)
	/* Parse both \\.\ and //./ for device prefix! */
	if ( (EQL (dname, DEV_PREFIX, DEV_LEN)) ||
	     (EQL (dname, ADEV_PREFIX, ADEV_LEN)) ) {
	    char *dentry;
	    dentry = (dname + DEV_LEN);
	    /* Note: We no longer do this mapping, it's unnecessary! */
	    /* FWIW: Windows has supported forward slashs for years! */
# if 0
	    if (EQL (dname, ADEV_PREFIX, ADEV_LEN)) {
# if (DEV_LEN != ADEV_LEN)
#  error "DEV_LEN != ADEV_LEN"
# endif
		/* Map //./ to \\.\ to ease scripting! */
		strncpy(dname, DEV_PREFIX, DEV_LEN);
		Free(dip, dip->di_dname);
		dip->di_dname = strdup(dname);
	    }
# endif /* 0 */
#else /* !defined(WIN32) */
	if ( (EQL (dname, DEV_PREFIX, DEV_LEN))   ||
	     (EQL (dname, ADEV_PREFIX, ADEV_LEN)) ||
	     (EQL (dname, NDEV_PREFIX, NDEV_LEN)) ) {
	    char *dentry;
	    if (EQL (dname, DEV_PREFIX, DEV_LEN)) {
		dentry = (dname + DEV_LEN);
	    } else if (EQL (dname, ADEV_PREFIX, ADEV_LEN)) {
		dentry = (dname + ADEV_LEN);
	    } else {
		dentry = (dname + NDEV_LEN);
	    }
#endif /* defined(WIN32) */
	    if ( (EQL (dentry, TAPE_NAME, sizeof(TAPE_NAME)-1)) ||
		 (EQL (dentry, NTAPE_NAME, sizeof(NTAPE_NAME)-1)) ) {
		dtp = setup_device_type("tape");
#if defined(WIN32)
	    } else if ( (EQLC (dentry, DISK_NAME, sizeof(DISK_NAME)-1)) ||
			(EQLC (dentry, RDISK_NAME, sizeof(RDISK_NAME)-1)) ) {
#else /* !defined(WIN32) */
	    } else if ( (EQL (dentry, DISK_NAME, sizeof(DISK_NAME)-1)) ||
			(EQL (dentry, RDISK_NAME, sizeof(RDISK_NAME)-1)) ) {
#endif /* defined(WIN32) */
		dtp = setup_device_type("disk");
#if defined(ADISK_NAME)
	    } else if ( (EQL (dentry, ADISK_NAME, sizeof(ADISK_NAME)-1)) ||
			(EQL (dentry, ARDISK_NAME, sizeof(ARDISK_NAME)-1)) ) {
		dtp = setup_device_type("disk");
#endif /* defined(ADISK_NAME) */
	    } else if ( (EQL (dentry, CDROM_NAME, sizeof(CDROM_NAME)-1)) ||
			(EQL (dentry, RCDROM_NAME, sizeof(RCDROM_NAME)-1)) ) {
		dtp = setup_device_type("disk");
	    }
#if defined(_NT_SOURCE) || defined(WIN32)
	    if ( (dtp == NULL) && (IsDriveLetter (dentry)) ) {
		dtp = setup_device_type("block");
	    }
#endif /* defined(_NT_SOURCE) || defined(WIN32) */
	}

	if ( (dtp == NULL) &&
	     (strlen(dname) == 1) && (*dname == '-') ) {
	    if (!dip->di_lbdata_size) dip->di_lbdata_size = BLOCK_SIZE;
	    dtp = setup_device_type("pipe");
	}
#if !defined(WIN32)
	if ( (dtp == NULL) &&
	     (stat (dname, &sb) == SUCCESS)) {
	    if ( S_ISBLK(sb.st_mode)) {
		dtp = setup_device_type("block");
	    } else if ( S_ISCHR(sb.st_mode) ) {
		/*
		 * Character devices are NOT treated as disks!
		 */
#if defined(DEC)
		if (SetupDiskAttributes(dip, dip->di_fd) != SUCCESS)
#endif /* defined(DEC) */
		    dtp = setup_device_type("character");
	    } 
	}
#endif /* !defined(WIN32) */
    } /* if (dtp == NULL) */

    /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /* End of device type setup.  Special setup follows. */
    /* - - - - - - - - - - - - - - - - - - - - - - - - - */

    /*
     * Do special setup for certain device types.
     */
    if ( (dip->di_dtype = dtp) ) {
	if ( (dtp->dt_dtype == DT_BLOCK) ||
	     (dtp->dt_dtype == DT_DISK)  || dip->di_random_io ) {
	    dip->di_random_access = True;
	}
	setup_device_defaults(dip);
    }
    /*
     * If the device size isn't set, then set it to our default.
     * With normal disks, this is setup by os_system_device_info()
     *
     * Note: This size is used for finding disk capacity, random I/O,
     *	     variable requests, and reporting failing relative block.
     */
    if ( !dip->di_dsize ) {
	if (!dip->di_device_size) {
	    dip->di_device_size = BLOCK_SIZE;
	}
	dip->di_dsize = dip->di_device_size;
    }
    if (!dip->di_rdsize) dip->di_rdsize = dip->di_dsize;

    /*
     * Note: This handles *existing* input/output files.
     */
#if defined(WIN32)
    if (dtp == NULL) {
	large_t filesize = 0;
	WIN32_FILE_ATTRIBUTE_DATA fad, *fadp = &fad;
	/*
	 * See if the file exists, and what its' size is.
	 */
	if ( GetFileAttributesEx(dname, GetFileExInfoStandard, fadp) ) {
	    dip->di_existing_file = True;
	    if (fadp->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		dip->di_dtype = dtp = setup_device_type("directory");
	    } else { /* Assuming a regular file (for now). */
		/* Note: Match Unix code below! */
		if (dip->di_multiple_files) {
		    SetupRegularFile(dip, (large_t)0);
		} else {
		    filesize = (large_t)(((large_t)fadp->nFileSizeHigh << 32L) + fadp->nFileSizeLow);
		    SetupRegularFile(dip, (large_t)filesize);
		}
		dip->di_dispose_mode = KEEP_FILE;   /* Keep existing files! */
	    }
	} else { /* GetFileAttributesEx() failed, could be file does not exist! */
	    if (dip->di_debug_flag || dip->di_fDebugFlag) {
		int error = os_get_error();
		char *emsg = os_get_error_msg(error);
		char *op = OS_GET_FILE_ATTR_OP;
		Printf(dip, "File name: %s\n", dname);
		Printf(dip, "DEBUG: %s failed, error %d - %s\n", op, error, emsg);
		os_free_error_msg(emsg);
	    }
	}
	/* Setup for new or existing regular files! */
	if (dtp == NULL) {
	    SetupRegularFile(dip, filesize);
	}
    } /* end of (dtp == NULL) */
#else /* !defined(WIN32) */
    if (dtp == NULL) {
	large_t filesize = 0;
	if (stat(dname, &sb) == SUCCESS) {
	    dip->di_existing_file = True;
	    if ( S_ISDIR(sb.st_mode) ) {
		dip->di_dtype = dtp = setup_device_type("directory");
	    } else if ( S_ISREG(sb.st_mode) ) {
		if (dip->di_multiple_files) {
		    SetupRegularFile(dip, (large_t)0);
		} else {
		    SetupRegularFile(dip, (large_t)sb.st_size);
		}
		dip->di_dispose_mode = KEEP_FILE;   /* Keep existing files! */
	    }
# if defined(_QNX_SOURCE)
	    else if ( S_ISBLK(sb.st_mode) ) {
		filesize = dip->di_user_capacity = ((large_t)sb.st_size * (large_t)dip->di_dsize);
	    }
# endif /* defined(_QNX_SOURCE) */
	} else { /* stat() failed, could be file does not exist! */
	    if (dip->di_debug_flag || dip->di_fDebugFlag) {
		int error = os_get_error();
		char *emsg = os_get_error_msg(error);
		char *op = OS_GET_FILE_ATTR_OP;
		Printf(dip, "File name: %s\n", dname);
		Printf(dip, "DEBUG: %s failed, error %d - %s\n", op, error, emsg);
		os_free_error_msg(emsg);
	    }
	}
	/*
	 * File doesn't exist, assume a regular file will be created,
	 */
	if (dtp == NULL) {
	    SetupRegularFile(dip, filesize);
	}
    } /* end of (dtp == NULL) */
#endif /* defined(WIN32) */
    
    if (!dip->di_dtype && dtp) dip->di_dtype = dtp;
    if (!dip->di_dtype && !dtp) {
	/* Setup a device type to avoid dereferencing a null pointer! */
	dip->di_dtype = dtp = setup_device_type("unknown"); /* Avoid seg faults! */
    }

    /* Note: Hammer is the only I/O behavior requiring a directory path today! */
    if ( (dip->di_iobehavior != HAMMER_IO) && (dip->di_dtype->dt_dtype == DT_DIRECTORY) ) {
	Eprintf(dip, "Sorry, directories are not supported at this time!\n");
	return(FAILURE);
    }

    SetupHistoryData(dip);

    dip->di_fsfile_flag = isFileSystemFile(dip);

    return (SUCCESS);
}

large_t
GetMaxUserCapacity(dinfo_t *dip, hbool_t use_records)
{
    /* Note: This capacity is from the user or the OS! */
    large_t user_data_capacity = dip->di_user_capacity;
    
    if (user_data_capacity == (large_t) 0) {
	large_t user_data_limit = 0;
	large_t user_record_data = 0;
	if (dip->di_data_limit != INFINITY) {
	    user_data_limit = dip->di_data_limit;
	}
	/* Note: The block size is small for random block sizes. */
	if ( (use_records == True) && (dip->di_record_limit != INFINITY) ) {
	    user_record_data = (dip->di_record_limit * dip->di_block_size);
	}
	user_data_capacity = max(user_data_limit, user_record_data);
    }
    return(user_data_capacity);
}
    
void
SetupRegularFile(struct dinfo *dip, large_t file_size)
{
    large_t user_data_limit;

    dip->di_random_access = True;
    dip->di_dtype = setup_device_type("regular");
    user_data_limit = GetMaxUserCapacity(dip, True);

    /*
     * If random I/O was selected, and a data or record limit was
     * not specified (i.e. runtime=n), then setup the file size.
     * This is necessary to limit random I/O within file size, or
     * for newly created files setup capacity based on data limit.
     */
    if ( dip->di_random_io || dip->di_slices ) {
	if (file_size) {
	    /*
             * If a data limit was specified, then do the following:
             *  - if reading, set to current file size
             *    ( very important for reverse or random I/O )
             *  - if writing, set to max of existing or user size
             *    ( exceeding the current size allows expansion )
             */   
            if (dip->di_data_limit == INFINITY) {
                dip->di_user_capacity = file_size;
            } else {
                /*
                 * This MAX is done, so random I/O to a file can be
                 * duplicated when specifying the same random seed.
                 * If file size is used, and it's less than limit,
                 * then random limit gets set too low so random
                 * offsets are not repeated, thus miscompares!
                 */
		dip->di_user_capacity = max(user_data_limit, file_size);
            }
	} else {
	    dip->di_user_capacity = user_data_limit;
	}
    } else { /* Sequential I/O */
	if ( (dip->di_ftype == INPUT_FILE) ) {
	    /* When reading, we cannot go beyond the end of file. */
	    dip->di_user_capacity = min(file_size, dip->di_data_limit);
	} else {
	    /* When writing, the file can be expanded based on options. */
	    dip->di_user_capacity = max(user_data_limit, file_size);
	}
    }
    if (dip->di_user_capacity) {
	SetupTransferLimits(dip, dip->di_user_capacity);
    }
    setup_device_defaults(dip);
    return;
}
