/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2018			    *
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
 * Module:	dtunix.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	This module contains *unix OS specific functions.
 * 
 * Modification History:
 * 
 * March 1st, 2018 by Robin T Miller
 *      Add isDeviceMounted() for Linux only (right now), to determine if
 * a /dev disk name is mounted to prevent overwriting mounted file systems.
 * 
 * May 29th, 2015 by Robin T. Miller
 * 	Increase the mount file system options buffer, since the previous
 * of 128 bytes was too small for recent RHEL6 mounts with options >194!
 * Also, changed strcpy() to strncpy() to avoid overwriting the stack!
 * 
 * April 29th, 2015 by Robin T. Miller
 * 	For HP-UX, Solaris, and Linux, increase the mount path buffer sizes
 * from 128 bytes to 8k, since somone has decided to use rather long mounts!
 * 
 * Date Unknown! (Robin forgot to add one)
 *	Updated AIX mount function to lookup file system type.  
 */
#include "dt.h"

#include <fcntl.h>
#include <sys/statvfs.h>

/*
 * SHIT! Another area of incompatability between *nix OS's! :-(
 */
#if defined(__linux__)
# include <mntent.h>
#  define MOUNT_FILE	_PATH_MOUNTED
#elif defined(__hpux)
# include <mntent.h>
#  define MOUNT_FILE	MNT_MNTTAB
#elif defined(SOLARIS)
# include <sys/mnttab.h>
#  define MOUNT_FILE	MNTTAB
#endif /* defined(__linux__) */

#if defined(DEV_BDIR_LEN)

/*
 * ConvertBlockToRawDevice() Convert a Block Device to a Raw Device.
 * 
 * Inputs:
 * 	block_device = Pointer to the block device.
 * 
 * Return Value:
 * 	Returns the raw device or NULL of it's not a block device.
 */ 
char *
ConvertBlockToRawDevice(char *block_device)
{
    char *raw_device = NULL;
    char *p = (block_device + DEV_BDIR_LEN);

    if (strncmp(block_device, DEV_BDIR_PREFIX, DEV_BDIR_LEN) == 0) {
	/* Note: Assumes raw device path is longer than block device! */
	raw_device = Malloc(NULL, strlen(block_device) + (DEV_RDIR_LEN - DEV_BDIR_LEN));
	if (raw_device) {
	    (void)sprintf(raw_device, "%s%s", DEV_RDIR_PREFIX, p);
	}
    }
    return (raw_device);
}

#endif /* defined(DEV_BDIR_LEN) */

#if defined(__linux__)

#include <ctype.h>

/*
 * ConvertDeviceToScsiDevice() Convert a Device to a SCSI Device.
 *  
 * Note: This is being specific added for RHEL6 since SCSI IOCTL fails 
 * if the device name has trailing numbers for the partition. Error returned:
 * 
 * SCSI request (SG_IO) failed on /dev/sdb1!, errno = 25 - Inappropriate ioctl for device
 *
 * Inputs:
 * 	device = Pointer to the device.
 * 
 * Return Value:
 * 	Returns a pointer to new memory with SCSI device name.
 */ 
char *
ConvertDeviceToScsiDevice(char *device)
{
    char *scsi_device, *p;

    if (device == NULL) return(device);
    scsi_device = strdup(device);
    p = ( scsi_device + (strlen(scsi_device) - 1) );

    while ( isdigit(*p) ) {
	*p = '\0';
	--p;
    }
    return (scsi_device);
}

#else /* !defined(__linux__) */

char *
ConvertDeviceToScsiDevice(char *device)
{
    return ( strdup(device) );
}

#endif /* defined(__linux__) */

/* ----------------------------------------------------------------------------------- */

#if defined(AIX)

#include <sys/mntctl.h>
#include <sys/vmount.h>

#define MNT_BUFFER_SIZE	(32 * KBYTE_SIZE)

char *lookup_gfstype(int gfstype);

typedef struct aix_filesystem_types {
    int	gfstype;
    char *gfsname;
} aix_filesystem_types_t;

aix_filesystem_types_t aix_gfstypes[] = {
    { MNT_J2,		"jfs2"		}, /* 0 -  AIX physical fs "jfs2"         */
    { MNT_NAMEFS,	"namefs"	}, /* 1 -  AIX pseudo fs "namefs"         */
    { MNT_NFS,		"nfs"		}, /* 2 -  SUN Network File System "nfs"  */
    { MNT_JFS,		"jfs"		}, /* 3 -  AIX R3 physical fs "jfs"       */
    { MNT_CDROM,	"cdrom"		}, /* 5 -  CDROM File System "cdrom"      */
    { MNT_PROCFS,	"proc"		}, /* 6 -  PROCFS File System "proc"      */
    { MNT_SFS,		"sfs"		}, /* 16 - AIX Special FS (STREAM mounts) */
    { MNT_CACHEFS,	"cachefs"	}, /* 17 - Cachefs file system            */
    { MNT_NFS3,		"nfs3"		}, /* 18 - NFSv3 file system              */
    { MNT_AUTOFS,	"autofs"	}, /* 19 - Automount file system          */
    { MNT_VXFS,		"vxfs"		}, /* 32 - THRPGIO File System "vxfs"     */
    { MNT_VXODM,	"vxodm"		}, /* 33 - For Veritas File System        */
    { MNT_UDF,		"udf"		}, /* 34 - UDFS file system               */
    { MNT_NFS4,		"nfs4"		}, /* 35 - NFSv4 file system              */
    { MNT_RFS4,		"rfs4"		}, /* 36 - NFSv4 Pseudo file system       */
    { MNT_CIFS,		"cifs"		}  /* 37 - AIX SMBFS (CIFS client)        */
};
int num_aix_gfstypes = sizeof(aix_gfstypes) / sizeof(aix_filesystem_types_t);

char *
lookup_gfstype(int gfstype)
{
    int entry;

    for (entry = 0; entry < num_aix_gfstypes; entry++) {
	if (aix_gfstypes[entry].gfstype == gfstype) {
	    return(aix_gfstypes[entry].gfsname);
	}
    }
    return(NULL);
}

hbool_t
FindMountDevice(dinfo_t *dip, char *path, hbool_t debug)
{
    char path_dir[PATH_BUFFER_SIZE];
    char *mounted_match = NULL;
    char *mounted_path = NULL;
    char *filesystem_type;
    struct vmount *vmp;
    char *mounted, *mounted_over, *vbp;
    hbool_t match = False;
    char *path_dirp = path;
    int entries, size;
    void *buffer;

    if (path[0] != dip->di_dir_sep) {
	memset(path_dir, '\0', sizeof(path_dir));
	path_dirp = getcwd(path_dir, sizeof(path_dir));
	if (path_dirp == NULL) return(match);
    }

    buffer = Malloc(dip, MNT_BUFFER_SIZE);
    if (buffer == NULL) return(match);
    if ( (entries = mntctl(MCTL_QUERY, MNT_BUFFER_SIZE, buffer)) < 0) {
	Perror(dip, "mntctl() failed");
	Free(dip, buffer);
	return(match);
    }
    vbp = buffer;

    while (entries--) {
	vmp = (struct vmount *)vbp;
        mounted = vmt2dataptr(vmp, VMT_OBJECT);
        mounted_over = vmt2dataptr(vmp, VMT_STUB);
	if (debug) {
	    Printf(dip, "mounted = %s, mounted_over = %s, type = %d\n", mounted, mounted_over, vmp->vmt_gfstype);
	}
	/*
	 * Loop through all mounted path entries to find the right match!
	 * Multiple matches via: /, /var, /var/tmp, /mnt, /mnt/nfs, etc
	 * So we want the match with the longest string in our path!
	 */
	if (strncmp(mounted_over, path_dirp, strlen(mounted_over)) == 0) {
	    /* Replace with entries matching more of the path. */
	    if ( (mounted_path == NULL) ||
		 (strlen(mounted_over) > strlen(mounted_path)) ) {
		if (debug) {
		    Printf(dip, "Found match! -> %s on %s\n", mounted, mounted_over);
		}
		/* In memory buffer, so just save the pointed. */
		mounted_path = mounted_over;
		mounted_match = mounted;
		filesystem_type = lookup_gfstype(vmp->vmt_gfstype);
		match = True;
	    }
	}
	vbp += vmp->vmt_length;
    }
    if (match == True) {
	dip->di_mounted_from_device = strdup(mounted_match);
	dip->di_mounted_on_dir = strdup(mounted_path);
	if (filesystem_type) {
	    dip->di_filesystem_type = strdup(filesystem_type);
	}
    }
    Free(dip, buffer);
    return(match);
}

hbool_t
isDeviceMounted(dinfo_t *dip, char *path, hbool_t debug)
{
    if (debug) {
	Printf(dip, "isDeviceMounted: This needs implmented for this OS!\n");
    }
    return(False);
}

/* ----------------------------------------------------------------------------------- */

#elif defined(SOLARIS)

hbool_t
FindMountDevice(dinfo_t *dip, char *path, hbool_t debug)
{
    char mounted_path[PATH_BUFFER_SIZE];
    char mounted_match[PATH_BUFFER_SIZE];
    char path_dir[PATH_BUFFER_SIZE];
    struct mnttab mnttab;
    struct mnttab *mnt = &mnttab;
    hbool_t match = False;
    char *path_dirp = path;
    FILE *fp;

    if (path[0] != dip->di_dir_sep) {
	memset(path_dir, '\0', sizeof(path_dir));
	path_dirp = getcwd(path_dir, sizeof(path_dir));
	if (path_dirp == NULL) return(match);
    }

    fp = fopen(MOUNT_FILE, "r");
    if (fp == NULL) return (match);

    memset(mounted_path, '\0', sizeof(mounted_path));
    memset(mounted_match, '\0', sizeof(mounted_match));

    while ( getmntent(fp, mnt) == 0 ) {
	if (debug) {
	    Printf(dip, "mount point = %s, special = %s, type = %s\n",
		   mnt->mnt_mountp, mnt->mnt_special, mnt->mnt_fstype);
	}
	/*
	 * Loop through all mounted path entries to find the right match!
	 * Multiple matches via: /, /var, /var/tmp, /mnt, /mnt/nfs, etc
	 * So we want the match with the longest string in our path!
	 */
	if (strncmp(mnt->mnt_mountp, path_dirp, strlen(mnt->mnt_mountp)) == 0) {
	    /* Replace with entries matching more of the path. */
	    if (strlen(mnt->mnt_mountp) > strlen(mounted_path)) {
		if (debug) {
		    Printf(dip, "Found match! -> %s on %s\n", mnt->mnt_special, mnt->mnt_mountp);
		}
		strncpy(mounted_path, mnt->mnt_mountp, sizeof(mounted_path)-1);
		strncpy(mounted_match, mnt->mnt_special, sizeof(mounted_match)-1);
		match = True;
	    }
	}
    }
    if (match == True) {
	dip->di_mounted_from_device = strdup(mounted_match);
	dip->di_mounted_on_dir = strdup(mounted_path);
    }
    (void)fclose(fp);
    return(match);
}

hbool_t
isDeviceMounted(dinfo_t *dip, char *path, hbool_t debug)
{
    if (debug) {
	Printf(dip, "isDeviceMounted: This needs implmented for this OS!\n");
    }
    return(False);
}

/* ----------------------------------------------------------------------------------- */

#elif defined(MOUNT_FILE) /* Linux, HP-UX */

hbool_t
FindMountDevice(dinfo_t *dip, char *path, hbool_t debug)
{
    char mounted_path[PATH_BUFFER_SIZE];
    char mounted_match[PATH_BUFFER_SIZE];
    char filesystem_type[SMALL_BUFFER_SIZE];
    char filesystem_options[PATH_BUFFER_SIZE];
    char path_dir[PATH_BUFFER_SIZE];
    struct mntent *mnt;
    hbool_t match = False;
    char *path_dirp = path;
    FILE *fp;

    if (path[0] != dip->di_dir_sep) {
	memset(path_dir, '\0', sizeof(path_dir));
	path_dirp = getcwd(path_dir, sizeof(path_dir));
	if (path_dirp == NULL) return(match);
    }

    fp = setmntent(MOUNT_FILE, "r");
    if (fp == NULL) return (match);

    memset(mounted_path, '\0', sizeof(mounted_path));
    memset(mounted_match, '\0', sizeof(mounted_match));

    while ( (mnt = getmntent(fp)) != NULL ) {
	if (debug) {
	    Printf(dip, "dir = %s, fsname = %s, type = %s\n", mnt->mnt_dir, mnt->mnt_fsname, mnt->mnt_type);
	}
	/*
	 * Loop through all mounted path entries to find the right match!
	 * Multiple matches via: /, /var, /var/tmp, /mnt, /mnt/nfs, etc
	 * So we want the match with the longest string in our path!
	 */
	if (strncmp(mnt->mnt_dir, path_dirp, strlen(mnt->mnt_dir)) == 0) {
	    /* Replace with entries matching more of the path. */
	    if (strlen(mnt->mnt_dir) > strlen(mounted_path)) {
		if (debug) {
		    Printf(dip, "Found match! -> %s on %s\n", mnt->mnt_fsname, mnt->mnt_dir);
		}
		strncpy(mounted_path, mnt->mnt_dir, sizeof(mounted_path)-1);
		strncpy(mounted_match, mnt->mnt_fsname, sizeof(mounted_match)-1);
		strncpy(filesystem_type, mnt->mnt_type, sizeof(filesystem_type)-1);
		strncpy(filesystem_options, mnt->mnt_opts, sizeof(filesystem_options)-1);
		match = True;
	    }
	}
    }
    if (match == True) {
	dip->di_mounted_from_device = strdup(mounted_match);
	dip->di_mounted_on_dir = strdup(mounted_path);
	dip->di_filesystem_type = strdup(filesystem_type);
	dip->di_filesystem_options = strdup(filesystem_options);
    }
    (void)endmntent(fp);
    return(match);
}

hbool_t
isDeviceMounted(dinfo_t *dip, char *path, hbool_t debug)
{
    char mounted_path[PATH_BUFFER_SIZE];
    char mounted_match[PATH_BUFFER_SIZE];
    char filesystem_type[SMALL_BUFFER_SIZE];
    char filesystem_options[PATH_BUFFER_SIZE];
    char path_dir[PATH_BUFFER_SIZE];
    struct mntent *mnt;
    size_t path_len = strlen(path);
    hbool_t match = False;
    FILE *fp;

    fp = setmntent(MOUNT_FILE, "r");
    if (fp == NULL) return (match);

    memset(mounted_path, '\0', sizeof(mounted_path));
    memset(mounted_match, '\0', sizeof(mounted_match));

    while ( (mnt = getmntent(fp)) != NULL ) {
	if (debug) {
	    Printf(dip, "dir = %s, fsname = %s, type = %s\n", mnt->mnt_dir, mnt->mnt_fsname, mnt->mnt_type);
	}
	/*
	 * Normally users will specific /dev/sda, for example, while file systems 
	 * are mounted from partitions such as /dev/sda1. We also must be careful 
	 * for matching /dev/sda and /dev/sdaa, etc. when we have many disk names. 
	 * DM-MP paths looks like this: /dev/mapper/35000cca2510285c8-part1 
	 *  
	 * Note: We do *not* catch this type of mount today: (sigh)
	 *  /dev/mapper/centos_cos--lab--l4--test01-root -> ../dm-0 -> /dev/sdm
	 */
	if ( (strncmp(path, mnt->mnt_fsname, path_len) == 0) && !isalpha(mnt->mnt_fsname[path_len]) ) {
	    if (debug) {
		Printf(dip, "Found match! -> %s on %s\n", mnt->mnt_fsname, mnt->mnt_dir);
	    }
	    strncpy(mounted_path, mnt->mnt_dir, sizeof(mounted_path)-1);
	    strncpy(mounted_match, mnt->mnt_fsname, sizeof(mounted_match)-1);
	    strncpy(filesystem_type, mnt->mnt_type, sizeof(filesystem_type)-1);
	    strncpy(filesystem_options, mnt->mnt_opts, sizeof(filesystem_options)-1);
	    match = True;
	    break;
	}
    }
    if (match == True) {
	dip->di_mounted_from_device = strdup(mounted_match);
	dip->di_mounted_on_dir = strdup(mounted_path);
	dip->di_filesystem_type = strdup(filesystem_type);
	dip->di_filesystem_options = strdup(filesystem_options);
    }
    (void)endmntent(fp);
    return(match);
}

#elif defined(FreeBSD) || defined(MacDarwin)

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>

hbool_t
FindMountDevice(dinfo_t *dip, char *path, hbool_t debug)
{
    char path_dir[PATH_BUFFER_SIZE];
    char *mounted_match = NULL;
    char *mounted_path = NULL;
    char *filesystem_type;
    hbool_t match = False;
    char *path_dirp = path;
    int count, entries;
    struct statfs *statfs_array;
    struct statfs *sfsp;
    long bufsize;

    if (path[0] != dip->di_dir_sep) {
	memset(path_dir, '\0', sizeof(path_dir));
	path_dirp = getcwd(path_dir, sizeof(path_dir));
	if (path_dirp == NULL) return(match);
    }

    /*
     * int
     * getfsstat(struct statfs *buf, long bufsize, int flags);
     * 
     * DESCRIPTION
     * The getfsstat() system call returns information about all mounted file
     * systems.  The buf argument is a pointer to statfs structures, as
     * described in statfs(2).
     * 
     * Fields that are undefined for a particular file system are set to -1.
     * The buffer is filled with an array of fsstat structures, one for each
     * mounted file system up to the byte count specified by bufsize.  Note, the
     * bufsize argument is the number of bytes that buf can hold, not the count
     * of statfs structures it will hold.
     * 
     * If buf is given as NULL, getfsstat() returns just the number of mounted
     * file systems.
     * 
     * Normally flags should be specified as MNT_WAIT.  If flags is set to
     * MNT_NOWAIT, getfsstat() will return the information it has available
     * without requesting an update from each file system.  Thus, some of the
     * information will be out of date, but getfsstat() will not block waiting
     * for information from a file system that is unable to respond.
     * 
     * RETURN VALUES
     * Upon successful completion, the number of fsstat structures is returned.
     * Otherwise, -1 is returned and the global variable errno is set to indi-
     * cate the error.
     */

    /* Note: If we use more statfs information, we may need to switch to MNT_WAIT. */
    entries = getfsstat(NULL, 0, MNT_NOWAIT);
    if (entries == 0) return(False);
    if (entries == FAILURE) {
	Perror(dip, "getfsstat");
    }
    bufsize = (sizeof(*sfsp) * entries);
    statfs_array = Malloc(dip, bufsize);
    if (statfs_array == NULL) return(False);
    
    entries = getfsstat(statfs_array, bufsize, MNT_NOWAIT);
    if (entries == FAILURE) {
	Perror(dip, "getfsstat");
	Free(dip, statfs_array);
	return(False);
    }

    for (count = 0; count < entries; count++) {
	sfsp = &statfs_array[count];
	if (debug) {
	    Printf(dip, "mount point = %s, mounted from = %s, type = %s\n",
		   sfsp->f_mntonname, sfsp->f_mntfromname, sfsp->f_fstypename);
	}
	/*
	 * Loop through all mounted path entries to find the right match!
	 * Multiple matches via: /, /var, /var/tmp, /mnt, /mnt/nfs, etc
	 * So we want the match with the longest string in our path!
	 */
	if (strncmp(sfsp->f_mntonname, path_dirp, strlen(sfsp->f_mntonname)) == 0) {
	    /* Replace with entries matching more of the path. */
	    if ( (mounted_path == NULL) ||
		 (strlen(sfsp->f_mntonname) > strlen(mounted_path)) ) {
		if (debug) {
		    Printf(dip, "Found match! -> %s on %s\n", sfsp->f_mntfromname, sfsp->f_mntonname);
		}
		/* In memory buffer, so just save the pointed. */
		mounted_path = sfsp->f_mntonname;
		mounted_match = sfsp->f_mntfromname;
		filesystem_type = sfsp->f_fstypename;
		match = True;
	    }
	}
    }
    if (match == True) {
	dip->di_mounted_from_device = strdup(mounted_match);
	dip->di_mounted_on_dir = strdup(mounted_path);
	dip->di_filesystem_type = strdup(filesystem_type);
    }
    Free(dip, statfs_array);
    return(match);
}

hbool_t
isDeviceMounted(dinfo_t *dip, char *path, hbool_t debug)
{
    if (debug) {
	Printf(dip, "isDeviceMounted: This needs implmented for this OS!\n");
    }
    return(False);
}

#else /* We don't have support for this OS yet! */

hbool_t
FindMountDevice(dinfo_t *dip, char *path, hbool_t debug)
{
    if (debug) {
	Printf(dip, "FindMountDevice: Don't know how to find mount device yet!\n");
    }
    return (False);
}

hbool_t
isDeviceMounted(dinfo_t *dip, char *path, hbool_t debug)
{
    if (debug) {
	Printf(dip, "isDeviceMounted: This needs implmented for this OS!\n");
    }
    return(False);
}

#endif /* defined(SOLARIS */

/*
 * dt_open_file() - Open a file with retries.
 *
 * Inputs:
 *	dip = The device information pointer.
 * 	file = File name to check for existance.
 * 	flags = The file open flags.
 * 	perm = The file permissions.
 *	isDiskFull = Pointer to disk full flag.
 *	isDirectory = Pointer to directory flag.
 * 	errors = The error control flag.
 * 	retries = The retries control flag.
 *
 * Return Value:
 *	Returns the file handle (NoFd on failures).
 */
HANDLE
dt_open_file(dinfo_t *dip, char *file, int flags, int perm,
	     hbool_t *isDiskFull, hbool_t *isDirectory,
	     hbool_t errors, hbool_t retrys)
{
    HANDLE handle = NoFd;
    int rc = SUCCESS;

    if (isDiskFull) *isDiskFull = False;
    if (isDirectory) *isDirectory = False;

    if (dip->di_debug_flag) {
	Printf(dip, "Opening file %s with POSIX open flags %#x...\n", file, flags);
	if (dip->di_extended_errors == True) {
	    ReportOpenInformation(dip, file, OS_OPEN_FILE_OP, flags, 0, 0, 0, False);
	}
    }
    if (retrys == True) dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, OPEN_OP);
	handle = os_open_file(file, flags, perm);
	DISABLE_NOPROG(dip);
	if (handle == NoFd) {
	    os_error_t error = os_get_error();
	    INIT_ERROR_INFO(eip, file, OS_OPEN_FILE_OP, OPEN_OP, NULL, 0, (Offset_t)0, (size_t)0,
			    error, logLevelError, PRT_SYSLOG, RPT_NOXERRORS);
	    if (isDiskFull) {
		*isDiskFull = os_isDiskFull(error);
		if (*isDiskFull == True) return(handle);
	    }
	    if (isDirectory) {
		*isDirectory = os_isADirectory(error);
		if (*isDirectory == True) return(handle);
	    }
	    if (errors == False) eip->ei_rpt_flags |= RPT_NOERRORS;
	    if (retrys == False) eip->ei_rpt_flags |= RPT_NORETRYS;
	    rc = ReportRetryableError(dip, eip, "Failed to open file %s", file);
	}
    } while ( (handle == NoFd) && (rc == RETRYABLE) );

    if ( (handle == NoFd) && (errors == True) ) {
	if (dip->di_extended_errors == True) {
	    ReportOpenInformation(dip, file, OS_OPEN_FILE_OP, flags, 0, 0, 0, True);
	}
    } else if ( (handle != NoFd) && (dip->di_debug_flag == True) ) {
	Printf(dip, "File %s successfully opened, fd = %d\n", file, handle);
    }
    return(handle);
}

#if defined(MacDarwin)

#define DIRECTIO_ON	1
#define DIRECTIO_OFF	0

HANDLE
os_open_file(char *name, int oflags, int perm)
{
    HANDLE fd;
    hbool_t dio_flag = (oflags & O_DIRECT) ? True : False;
    
    oflags &= ~O_DIRECT;	/* Clear the psuedo-flag. */
    
    fd = open(name, oflags, perm);
    /* If requested, try to enable Direct I/O (DIO). */
    if ( (fd != NoFd) && (dio_flag == True) ) {
	int status = fcntl(fd, F_NOCACHE, DIRECTIO_ON);
    }
    return(fd);
}

#elif defined(SOLARIS)

HANDLE
os_open_file(char *name, int oflags, int perm)
{
    HANDLE fd;
    hbool_t dio_flag = (oflags & O_DIRECT) ? True : False;
    
    oflags &= ~O_DIRECT;	/* Clear the psuedo-flag. */
    
    fd = open(name, oflags, perm);
    /* If requested, try to enable Direct I/O (DIO). */
    if ( (fd != NoFd) && (dio_flag == True) ) {
	int status;
	if ( (status = directio(fd, DIRECTIO_ON)) < 0) {
	    if (errno == ENOTTY) {
		(void)ioctl(fd, VX_SETCACHE, VX_DIRECT);
	    }
	}
    }
    return(fd);
}

#endif /* defined(MacDarwin) || defined(SOLARIS) */
    
char *
os_ctime(time_t *timep, char *time_buffer, int timebuf_size)
{
    char *bp;

    bp = ctime_r(timep, time_buffer);
    if (bp == NULL) {
	Perror(NULL, "ctime_r() failed");
	(int)sprintf(time_buffer, "<no time available>\n");
    } else {
	if (bp = strrchr(time_buffer, '\n')) {
	    *bp = '\0';
	}
    }
    return(time_buffer);
}

#if !defined(INLINE_FUNCS)

int
os_create_directory(char *dir_path, int permissions)
{
    return( mkdir(dir_path, permissions) );
}

int
os_remove_directory(char *dir_path)
{
    return( rmdir(dir_path) );
}

#endif /* !defined(INLINE_FUNCS) */

char *
os_getcwd(void)
{
    char path[PATH_BUFFER_SIZE];

    if ( getcwd(path, sizeof(path)) == NULL ) {
	return(NULL);
    } else {
	return ( strdup(path) );
    }
}

os_dev_t
os_get_devID(char *path, HANDLE handle)
{
    struct stat st;
    os_dev_t devID = (os_dev_t)FAILURE;
    int status;

    if (handle == INVALID_HANDLE_VALUE) {
	status = stat(path, &st);
    } else {
	status = fstat(handle, &st);
    }
    if (status == SUCCESS) {
	if (st.st_rdev) {
	    devID = st.st_rdev;
	} else {
	    devID = st.st_dev;
	}
    }
    return(devID);
}

os_ino_t
os_get_fileID(char *path, HANDLE handle)
{
    struct stat st;
    os_ino_t fileID = (os_ino_t)FAILURE;
    int status;

    if (handle == INVALID_HANDLE_VALUE) {
	status = stat(path, &st);
    } else {
	status = fstat(handle, &st);
    }
    if (status == SUCCESS) {
	fileID = st.st_ino;
    }
    return(fileID);
}

large_t
os_get_file_size(char *path, HANDLE handle)
{
    struct stat sb, *stp = &sb;
    slarge_t filesize = -1LL;
    int status;

    if (handle == INVALID_HANDLE_VALUE) {
	status = stat(path, stp);
    } else {
	status = fstat(handle, stp);
    }
    if (status == SUCCESS) {
	return( (large_t)stp->st_size );
    } else {
	return( (large_t)filesize );
    }
}

char *
os_gethostname(void)
{
    char hostname[MAXHOSTNAMELEN];

    if (gethostname(hostname, sizeof(hostname)) == FAILURE) {
	//os_perror(NULL, "gethostname() failed");
	return(NULL);
    }
    return ( strdup(hostname) );
}

#define IPv4_ADDRSIZE	4
#define IPv4_STRSIZE	INET_ADDRSTRLEN		// 16
#define IPv6_ADDRSIZE	16
#define IPv6_STRSIZE	INET6_ADDRSTRLEN	// 46

/*
 * Note: I'm new to network programming, thus lots of documentation!
 */
char *
os_getaddrinfo(dinfo_t *dip, char *host, int family, void **sa, socklen_t *salen)
{
    char address_str[IPv6_STRSIZE];
    char *ipv4 = NULL, *ipv6 = NULL;
    struct addrinfo hints, *addrinfop, *aip;
    int status;

    memset(&hints, 0, sizeof hints);
    memset(address_str, 0, sizeof(address_str));
    if (sa && *sa) {
	free(*sa);
	*sa = NULL;
    }
    if (family) {
	hints.ai_family = family;
    } else {
	hints.ai_family = AF_UNSPEC;	/* IPv4 and IPv6 allowed. */
    }
    hints.ai_socktype = SOCK_STREAM;

    /*
     * int getaddrinfo(const char *nodename, const char *servname,
     *	               const struct addrinfo *hints, struct addrinfo **res);
     */
    if ((status = getaddrinfo(host, NULL, &hints, &addrinfop)) != SUCCESS) {
	return(NULL);
    }

    /* Loop through all the results to find the desired address. */
    for (aip = addrinfop; (aip != NULL); aip = aip->ai_next) {
	if (aip->ai_family == AF_INET) {
	    /* 
	     * struct sockaddr_in {
	     *   short   sin_family;
	     *   u_short sin_port;
	     *   struct  in_addr sin_addr;
	     *   char    sin_zero[8];
	     * };
	     */ 
	    struct sockaddr_in *sainp = (struct sockaddr_in *)(aip->ai_addr);
	    /* 
	     * OS's NOT supporting these are in trouble! (not supported)
	     * 
	     * const char *inet_ntop(int af, const void *src,
	     * 			     char *dst, socklen_t size);
	     * int inet_pton(int af, const char *cp, void *addr);
	     */
	    const char *p = inet_ntop(AF_INET, &sainp->sin_addr, address_str, IPv4_STRSIZE);
	    if (p) {
		if (ipv4) free(ipv4);
		ipv4 = strdup(p);
	    }
	    /* Optionally save the socket address for other API's! */
	    if (sa) {
		*salen = (socklen_t)sizeof(*sainp);
		if (*sa) free(*sa);
		*sa = Malloc(dip, (size_t)*salen);
		if (*sa) {
		    memcpy(*sa, sainp, *salen);
		}
	    }
	    /* We prefer IPv4 address for now! */
	    /* We'll hope the last is an IPv4 address. */
	    //break;
	} else if (aip->ai_family == AF_INET6) {
	    /*
	     * struct sockaddr_in6 {
	     *   short   sin6_family;
	     *   u_short sin6_port;
	     *   u_long  sin6_flowinfo;
	     *   struct  in6_addr sin6_addr;
	     *   u_long  sin6_scope_id;
	     *   };
	     */
	    struct sockaddr_in6 *sain6p = (struct sockaddr_in6 *)(aip->ai_addr);
	    const char *p = inet_ntop(AF_INET6, &sain6p->sin6_addr, address_str, IPv6_STRSIZE);
	    if (p) {
		/* 
		 * Note: This is required for Windows, not sure about Unix but adding it here too!
		 * 
		 * There can be multiple IP addresses returned, and "::1" is for the IPv6 local host.
		 * The ::1 address (or, rather, in any address, that has a double colon in it) double	
		 * colon expands into the number of zero-bits, neccessary to pad the address to full
		 * length, so the expanded version looks like 0000:0000:0000:0000:0000:0000:0000:0001.
		 */
		if ( EQ(p, "::1") ) {
		    continue;
		}
		if (ipv6) free(ipv6);
		ipv6 = strdup(address_str);
	    }
	    /* Optionally save the socket address for other API's! */
	    if (sa) {
		*salen = (socklen_t)sizeof(*sain6p);
		if (*sa) free(*sa);
		*sa = Malloc(dip, (size_t)*salen);
		if (*sa) {
		    memcpy(*sa, sain6p, *salen);
		}
	    }
	    /* Don't break, instead try for IPv4 address! */
	    //break;
	}
	/* continue to the next entry... */
    }
    freeaddrinfo(addrinfop);
    /* We'll favor IPv4 vs. IPv6 (for now)! */
    if (ipv4 && ipv6) {
	free(ipv6); ipv6 = NULL;
    }
    return( (ipv4) ? ipv4 : ipv6 );
}

/*
 * Convert a network (socket) address to a host name.
 */
char *
os_getnameinfo(dinfo_t *dip, struct sockaddr *sa, socklen_t salen)
{
    char host[NI_MAXHOST], server[NI_MAXSERV];
    int status;

    memset(host, 0, sizeof(host));
    memset(server, 0, sizeof(server));
    /*
     * The socket address is a generic data structure, and will point
     * to either struct sockaddr_in (IPv4) or struct sockaddr_in6 (IPv6).
     *
     *   struct sockaddr {
     *     ushort  sa_family;
     *     char    sa_data[14];
     *   };
     */
    /*
     * int getnameinfo(const struct sockaddr *sa, socklen_t salen,
     *                 char *host, size_t hostlen,
     *                 char *serv, size_t servlen, int flags);
     */
    status = getnameinfo(sa, salen, host, sizeof(host),
			 server, sizeof(server), NI_NAMEREQD);
    if (status == FAILURE) {
	return(NULL); 
    } else {
	return( (strlen(host)) ? strdup(host) : NULL );
    }
}

#include <sys/utsname.h>

char *
os_getosinfo(void)
{
    struct utsname u;
    char osinfo[STRING_BUFFER_SIZE];

    /*
     * utsname fields:
     *     char    sysname[SYS_NMLN];
     *     char    nodename[SYS_NMLN];
     *     char    release[SYS_NMLN];
     *     char    version[SYS_NMLN];
     *     char    machine[SYS_NMLN];
     */
    if (uname(&u) == FAILURE) {
	//os_perror(NULL, "uname() failed");
	return(NULL);
    }
    sprintf(osinfo, "%s %s %s %s", u.sysname, u.release, u.version, u.machine);
    return( strdup(osinfo) );
}

char *
os_getusername(void)
{
    size_t bufsize = STRING_BUFFER_SIZE;
    char username[STRING_BUFFER_SIZE];

    if (getlogin_r(username, bufsize) != SUCCESS) {
	//os_perror(NULL, "getlogin_r() failed");
	return(NULL);
    }
    return ( strdup(username) );
}

int
os_file_information(char *file, large_t *filesize, hbool_t *is_dir, hbool_t *is_file)
{
    struct stat sb;
    os_error_t status;
    
    if (is_dir) *is_dir = False;
    if (is_file) *is_file = False;
    if (filesize) *filesize = 0;
    
    status = stat(file, &sb);
    if (status == SUCCESS) {
	if (filesize) *filesize = (large_t)sb.st_size;
	if ( is_dir && S_ISDIR(sb.st_mode) ) *is_dir = True;
	if ( is_file && S_ISREG(sb.st_mode) ) *is_file = True;
    }
    return(status);
}

hbool_t
os_isdir(char *dirpath)
{
    hbool_t isdir;
    (void)os_file_information(dirpath, NULL, &isdir, NULL);
    return(isdir);
}

hbool_t
os_file_exists(char *file)
{
    struct stat sb;
    
    if (stat(file, &sb) == SUCCESS) {
	return(True);
    } else {
	return(False);
    }
}

int
os_get_fs_information(dinfo_t *dip, char *dir)
{
    struct statvfs statfs_info;
    struct statvfs *sfsp = &statfs_info;
    int status;

    /* 
     * Get the specified or current directory information.
     */
    if (dir) {
	status = statvfs(dir, sfsp);
    } else {
	char *cdir;
	if ( (cdir = os_getcwd()) == NULL) return(FAILURE);
	status = statvfs(cdir, sfsp);
    }
	
    /*
     * struct statvfs {
     *     unsigned long  f_bsize;    file system block size
     *     unsigned long  f_frsize;   fragment size
     *     fsblkcnt_t     f_blocks;   size of fs in f_frsize units
     *     fsblkcnt_t     f_bfree;    # free blocks
     *     fsblkcnt_t     f_bavail;   # free blocks for non-root
     *     fsfilcnt_t     f_files;    # inodes
     *     fsfilcnt_t     f_ffree;    # free inodes
     *     fsfilcnt_t     f_favail;   # free inodes for non-root
     *     unsigned long  f_fsid;     file system ID
     *     unsigned long  f_flag;     mount flags
     *     unsigned long  f_namemax;  maximum filename length
     * };
     */
    if (status == SUCCESS) {
	dip->di_fs_block_size = (uint32_t)sfsp->f_bsize;
	/* Note: The file system blocks are converted to bytes! */
	if ( getuid() ) {
	    dip->di_fs_space_free = (large_t)(sfsp->f_bsize * sfsp->f_bavail);
	} else {
	    dip->di_fs_space_free = (large_t)(sfsp->f_bsize * sfsp->f_bfree);
	}
	dip->di_fs_total_space = (large_t)(sfsp->f_frsize * sfsp->f_blocks);
    }
    return(status);
}

int
os_set_priority(dinfo_t *dip, HANDLE hThread, int priority)
{
    int status;

    status = nice(priority);
    return(status);
}

#if !defined(SYSLOG)

void
os_syslog(int priority, char *format, ...)
{
    return;
}

#endif /* !defined(SYSLOG) */

void
tPerror(dinfo_t *dip, int error, char *format, ...)
{
    char msgbuf[LOG_BUFSIZE];
    va_list ap;
    FILE *fp;

    if (dip == NULL) dip = master_dinfo;
    fp = (dip) ? dip->di_efp : efp;
    va_start(ap, format);
    (void)vsprintf(msgbuf, format, ap);
    va_end(ap);
    Eprintf(dip, "%s, error = %d - %s\n", msgbuf, error, strerror(error));
    return;
}

#if defined(NEEDS_SETENV_API)

/* Note: Solaris 8 does NOT have this POSIX API! :-( */

int
setenv(const char *name, const char *value, int overwrite)
{
    /* Note: This is memory that will never be given back! */
    char *string = malloc(strlen(name) + strlen(value) + 2);
    if (string == NULL) return(-1);
    (void)sprintf(string, "%s=%s", name, value);
    return ( putenv(string) );
}

#endif /* defined(NEEDS_SETENV_API) */

#if defined(MacDarwin)

int
os_DirectIO(struct dinfo *dip, char *file, hbool_t flag)
{
    char fmt[STRING_BUFFER_SIZE];
    char *dio_msg = (flag) ? "Enabling" : "Disabling";
    int status;

    if (dip->di_debug_flag) {
        Printf(dip, "%s direct I/O via fcntl(F_NOCACHE) API...\n", dio_msg);
    }
    /*
     * https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man2/fcntl.2.html 
     * 
     * F_NOCACHE  Turns data caching off/on. A non-zero value in arg turns data caching off.
     *            A value of zero in arg turns data caching on.
     */
    status = fcntl(dip->di_fd, F_NOCACHE, (flag) ? DIRECTIO_ON : DIRECTIO_OFF);
    if (status == FAILURE) {
	Printf(dip, "Warning: Unable to enable Direct I/O (DIO), continuing...\n");
	status = SUCCESS;
    }
    return (status);
}

#elif defined(SOLARIS)
/*
 * os_DirectIO - Enable or disable Direct I/O (DIO) for Solaris FS's.
 *
 * Description:
 *      Sadly, Solaris is an odd ball when it comes to enabling Direct I/O.
 * Solaris does NOT support O_DIRECT like other Unix systems.  Instead, if
 * using UFS, DIO is enabled via the directio() API after the file is open.
 * Furthermore, if Veritas VxFS is used, the directio() API is not supported,
 * so a vendor unique IOCTL must be used to enable DIO.
 *
 * Inputs:
 *      dip = The device information pointer.
 *      file = The file name we're working on.
 *	flag = Flag to control enable/disable.
 *
 * Return Value:
 *      Returns SUCCESS (if enabled) or FAILURE (couldn't enable DIO).
 */
int
os_DirectIO(struct dinfo *dip, char *file, hbool_t flag)
{
    char fmt[STRING_BUFFER_SIZE];
    char *dio_msg = (flag) ? "Enabling" : "Disabling";
    int status;

    if (dip->di_debug_flag) {
        Printf(dip, "%s direct I/O via directio() API...\n", dio_msg);
    }
    /* Note: ZFS does *not* support Direct I/O! */
    if ( (status = directio(dip->di_fd, (flag) ? DIRECTIO_ON : DIRECTIO_OFF)) < 0) {
        if (errno == ENOTTY) {
	    if (dip->di_debug_flag || dip->di_fDebugFlag) {
                Printf(dip, "%s direct I/O via VX_SETCACHE/VX_DIRECT IOCTL...\n", dio_msg);
            }
            if ( (status = ioctl(dip->di_fd, VX_SETCACHE, (flag) ? VX_DIRECT : 0)) < 0) {
		;
		//(void)sprintf(fmt, "VX_DIRECT -> %s", file);
		//report_error (dip, os_get_error(), fmt, True);
            }
	}
#if 0
	} else {
	    (void)sprintf(fmt, "directio -> %s", file);
	    report_error(dip, os_get_error(), fmt, True);
	}
#endif /* 0 */
    }
    if (status == FAILURE) {
	Printf(dip, "Warning: Unable to enable Direct I/O (DIO), continuing...\n");
	status = SUCCESS;
    }
    return (status);
}

#endif /* defined(MacDarwin) || defined(SOLARIS) */

/*
 * os_isEof() - Determine if this is an EOF condition.
 *
 * Description:
 *	Generally, a read EOF is a count of 0, while writes are failed
 * with errno set to indicate ENOSPC. But, apparently POSIX does *not*
 * define this for direct disk and file system, thus this ugliness.
 * Note: Some of these extra errors are caused by seeks past EOM.
 */
hbool_t
os_isEof(ssize_t count, int error)
{
    /* TODO: May need to know if we're doing a read or write for accuracy! */
    if ( (count == (ssize_t) 0) ||
	 ( (count == (ssize_t) -1) &&
	   ( (error == ENOSPC) ||
	     (error == ENXIO)) ||
	     (error == EDQUOT) ) ) { /* For file systems, treat like EOF! */
	return(True);
    } else {
	return(False);
    }
}

void
ReportOpenInformation(dinfo_t *dip, char *FileName, char *Operation,
		      uint32_t DesiredAccess,
		      uint32_t CreationDisposition, uint32_t FileAttributes,
		      uint32_t ShareMode, hbool_t error_flag)
{
    INIT_ERROR_INFO(eip, FileName, Operation, OPEN_OP, NULL, 0, (Offset_t)0, (size_t)0, os_get_error(),
		    logLevelInfo, PRT_NOFLAGS, (RPT_NODEVINFO|RPT_NOHISTORY|RPT_NONEWLINE));
    char buffer[STRING_BUFFER_SIZE];
    char *bp = buffer;

    if (error_flag == True) {
	eip->ei_log_level = logLevelError;
    } else {
	eip->ei_rpt_flags |= (RPT_NOERRORMSG|RPT_NOERRORNUM);
    }
    (void)ReportExtendedErrorInfo(dip, eip, NULL);
    PrintHex(dip, "Desired Access", DesiredAccess, DNL);
    bp = buffer;
    if (DesiredAccess == 0) {
	if (O_RDONLY == 0) {
	    bp += sprintf(bp, "O_RDONLY");
	} else {
	    bp += sprintf(bp, "none specified");
	}
    } else {
	if (DesiredAccess & O_CREAT) {
	    bp += sprintf(bp, "O_CREAT|");
	}
	if (DesiredAccess & O_RDONLY) {
	    bp += sprintf(bp, "O_RDONLY|");
	}
	if (DesiredAccess & O_WRONLY) {
	    bp += sprintf(bp, "O_WRONLY|");
	}
	if (DesiredAccess & O_RDWR) {
	    bp += sprintf(bp, "O_RDWR|");
	}
	if (DesiredAccess & O_APPEND) {
	    bp += sprintf(bp, "O_APPEND|");
	}
#if defined(O_SYNC)
	if (DesiredAccess & O_SYNC) {
	    bp += sprintf(bp, "O_SYNC|");
	}
#endif /* defined(O_SYNC) */
/* 
 * Note: On Linux, O_SYNC, O_DSYNC, and O_RSYNC are all the same value!* 
 */
#if defined(O_DSYNC) && defined(O_SYNC) && (O_SYNC != O_DSYNC)
# if defined(O_DSYNC)
	if (DesiredAccess & O_DSYNC) {
	    bp += sprintf(bp, "O_DSYNC|");
	}
# endif /* defined(O_DSYNC) */
#endif /* defined(O_DSYNC) && defined(O_SYNC) && (O_SYNC != O_DSYNC) */
#if defined(O_RSYNC) && defined(O_SYNC) && (O_SYNC != O_RSYNC)
# if defined(O_RSYNC)
	if (DesiredAccess & O_RSYNC) {
	    bp += sprintf(bp, "O_RSYNC|");
	}
# endif /* defined(O_RSYNC) */
#endif /* defined(O_RSYNC) && defined(O_SYNC) && (O_SYNC != O_RSYNC) */
#if defined(O_DIRECT)
	if (DesiredAccess & O_DIRECT) {
	    bp += sprintf(bp, "O_DIRECT|");
	}
#endif /* defined(O_DIRECT) */
#if defined(O_EXCL)
	if (DesiredAccess & O_EXCL) {
	    bp += sprintf(bp, "O_EXCL|");
	}
#endif /* defined(O_EXCL) */
#if defined(O_LARGEFILE)
	if (DesiredAccess & O_LARGEFILE) {
	    bp += sprintf(bp, "O_LARGEFILE|");
	}
#endif /* defined(O_LARGEFILE) */
	if (DesiredAccess & O_TRUNC) {
	    bp += sprintf(bp, "O_TRUNC");
	}
	if (bp[-1] == '|') bp[-1] = '\0';
    }
    Lprintf(dip, " = %s\n", buffer);
    Lprintf(dip, "\n");
    if (error_flag == True) {
	eLflush(dip);
    } else {
	Lflush(dip);
    }
    return;
}

int
os_lock_file(HANDLE fd, Offset_t start, Offset_t length, int type)
{
    struct flock fl;
    struct flock *flp = &fl;
    int status;

    memset(flp, '\0', sizeof(*flp));
    flp->l_whence = SEEK_SET;
    flp->l_start  = start;
    flp->l_len    = length;	/* That's right, len is off64_t! */
    flp->l_type   = type;

    status = fcntl(fd, F_SETLK, flp);
    return(status);
}

/* Note: This wrapper is mainly for Windows extended lock API. */
int
os_xlock_file(HANDLE fd, Offset_t start, Offset_t length, int type, hbool_t exclusive, hbool_t immediate)
{
    /* Maybe extend the basic locks later. */
    return( os_lock_file(fd, start, length, type) );
}

int
os_unlock_file(HANDLE fd, Offset_t start, Offset_t length)
{
    struct flock fl;
    struct flock *flp = &fl;
    int status;

    memset(flp, '\0', sizeof(*flp));
    flp->l_whence = SEEK_SET;
    flp->l_start  = start;
    flp->l_len    = length;
    flp->l_type   = F_UNLCK;

    status = fcntl(fd, F_SETLK, flp);
    return(status);
}

int
os_set_lock_flags(lock_type_t lock_type, int *lock_type_flag,
		  hbool_t *exclusive, hbool_t *immediate, hbool_t *unlock_flag)
{
    int status = SUCCESS;

    *exclusive = True;
    *immediate = True;
    *unlock_flag = False;
    
    switch (lock_type) {
	
	case LOCK_TYPE_READ:
	    *lock_type_flag = F_RDLCK;
	    break;

	case LOCK_TYPE_WRITE:
	    *lock_type_flag = F_WRLCK;
	    break;

	case LOCK_TYPE_UNLOCK:
	    *unlock_flag = True;
	    *lock_type_flag = F_UNLCK;
	    break;

	default:
	    status = FAILURE;
	    break;
    }
    return(status);
}

uint64_t
os_create_random_seed(void)
{
    struct timeval time_val;
    struct timeval *tv = &time_val;

    if ( gettimeofday(tv, NULL) == SUCCESS ) {
	return( ((uint64_t)tv->tv_sec << 32L) + (uint64_t)tv->tv_usec );
    } else {
	return(0);
    }
}

int
os_file_trim(HANDLE handle, Offset_t offset, uint64_t length)
{
    return(WARNING);
}

static int disconnect_errors[] = { ESTALE };
static int num_disconnect_entries = sizeof(disconnect_errors) / sizeof(int);

hbool_t
os_is_session_disconnected(int error)
{
    int entry;

    for (entry = 0; (entry < num_disconnect_entries); entry++) {
	if ( error == disconnect_errors[entry] ) return(True);
    }
    return(False);
}

void
os_set_disconnect_errors(dinfo_t *dip)
{
    int entry;

    for (entry = 0; (entry < num_disconnect_entries); entry++) {
	dip->di_retry_errors[dip->di_retry_entries++] = disconnect_errors[entry];
	if (dip->di_retry_entries == RETRY_ENTRIES) break;
    }
    return;
}


#if defined(DEBUG)

char *
decodeCancelType(int cancel_type)
{
    switch (cancel_type) {
	case PTHREAD_CANCEL_ASYNCHRONOUS:
	    return ("PTHREAD_CANCEL_ASYNCHRONOUS");
	    break;
	case PTHREAD_CANCEL_DEFERRED:
	    return ("PTHREAD_CANCEL_DEFERRED");
	    break;
	default:
	    return ("cancel type unknown");
	    break;
    }
}
#endif /* defined(DEBUG) */

int
os_set_thread_cancel_type(dinfo_t *dip, int cancel_type)
{
    int status;
    int old_cancel_type = 0;

#if defined(DEBUG)
    if (dip->di_tDebugFlag) {
	Printf(dip, "Setting the thread cancel type to %s...\n", decodeCancelType(cancel_type));
    }
    if ( (status = pthread_setcanceltype(cancel_type, &old_cancel_type)) == SUCCESS) {
	if (dip->di_tDebugFlag) {
	    Printf(dip, "Previous cancel type is %d (%s)\n",
		  old_cancel_type, decodeCancelType(old_cancel_type));
	}
    } else {
	tPerror(dip, status, "pthread_setcanceltype() failed");
    }
#else /* !defined(DEBUG) */
    if ( (status = pthread_setcanceltype(cancel_type, &old_cancel_type)) != SUCCESS) {
	tPerror(dip, status, "pthread_setcanceltype() failed");
    }
#endif /* defined(DEBUG) */
    return (status);
}

#if HAVE_UUID

# if defined(FreeBSD) && !defined(MacDarwin)

#include <uuid.h>

#define UUID_BUFFER_SIZE	(sizeof(uuid_t) * 3)

char *
os_get_uuid(hbool_t want_dashes)
{
    char auuid[UUID_BUFFER_SIZE];
    char *struuid;
    uuid_t uuid;
    int uuidlen = 0;
    int idx = 0;
    int status;

    memset(auuid, '\0', sizeof(auuid));

    uuid_create(&uuid, &status);
    if (status != uuid_s_ok) return(NULL);

    /* Convert internal binary format into a 36-byte string. */
    /* UUID format is: c5b922f4-12df-4145-b8f1-6ae717ed60b3 */
    uuid_to_string(&uuid, &struuid, &status);
    if (status != uuid_s_ok) return(NULL);

    /* We may not always want dashes in path names, so... */
    if (want_dashes == True) {
	(void)strcpy(auuid, struuid);
    } else {
	/* Copy the UUID, stripping dash '-' characters. */
	for (idx = 0; idx < (int)strlen(struuid); idx++) {
	    if (struuid[idx] != '-') {
		auuid[uuidlen++] = struuid[idx];
	    }
	}
	auuid[uuidlen] = '\0';
    }
    free(struuid);
    return( strdup(auuid) );
}

# else /* All other Unix OS's! */

#include <uuid/uuid.h>

/* The UUID is 16 bytes (128 bits) long. (added extra) */
#define UUID_BUFFER_SIZE	(sizeof(uuid_t) * 3)

char *
os_get_uuid(hbool_t want_dashes)
{
    char auuid[UUID_BUFFER_SIZE];
    char struuid[UUID_BUFFER_SIZE];
    uuid_t uuid;
    int uuidlen = 0;
    int idx = 0;

    memset(auuid, '\0', sizeof(auuid));

    uuid_generate(uuid);
    /* Convert internal binary format into a 36-byte string. */
    /* UUID format is: c5b922f4-12df-4145-b8f1-6ae717ed60b3 */
    uuid_unparse(uuid, struuid);

    /* We may not always want dashes in path names, so... */
    if (want_dashes == True) {
	(void)strcpy(auuid, struuid);
    } else {
	/* Copy the UUID, stripping dash '-' characters. */
	for (idx = 0; idx < (int)strlen(struuid); idx++) {
	    if (struuid[idx] != '-') {
		auuid[uuidlen++] = struuid[idx];
	    }
	}
	auuid[uuidlen] = '\0';
    }
    return( strdup(auuid) );
}

# endif /* defined(FreeBSD) */
#else /* !defined(HAVE_UUID) */

#define UUID_BYTES		16
#define UUID_WORDS		(UUID_BYTES / 2)
#define ASCII_UUID_BUFFER_SIZE	(UUID_BYTES * 3)

char *
os_get_uuid(hbool_t want_dashes)
{
    char auuid[ASCII_UUID_BUFFER_SIZE];
    uint16_t uuid[UUID_WORDS];
    uint32_t seed = (uint32_t)pthread_self();
    int idx = 0;

    memset(auuid, '\0', sizeof(auuid));

    /* Create a fake UUID. */
    for (idx = 0; idx < UUID_WORDS; idx++) {
	uuid[idx] = (uint16_t)rand_r(&seed);
    }

    if (want_dashes == True) {
		    /* Match uuidgen format and Windows UUID strings. */
		    /* Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx */
	(void)sprintf(auuid, "%04x%04x-%04x-%04x-%04x-%04x%04x%04x",
		      uuid[7], uuid[6],
		      uuid[5], uuid[4], uuid[3],
		      uuid[2], uuid[1], uuid[0]);
    } else {
	(void)sprintf(auuid, "%04x%04x%04x%04x%04x%04x%04x%04x",
		      uuid[7], uuid[6],
		      uuid[5], uuid[4], uuid[3],
		      uuid[2], uuid[1], uuid[0]);
    }
    return( strdup(auuid) );
}

#endif /* HAVE_UUID */
