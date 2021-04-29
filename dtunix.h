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
#if !defined(_DTUNIX_H_)
#define _DTUNIX_H_ 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Note: stdint.h does NOT exist on HP-UX 11.11 (very old now).    */
/*	 Nor does Solaris 8! (sigh) Time to retire these old OS's! */
/*	 But that said, required typedef's are in sys/types.h      */
#include <sys/types.h>

#if defined(__hpux)
#  include <sys/fs/vx_ioctl.h>
#else /* !defined(__hpux) */
//#if defined(SOLARIS)
/* This is for enabling Direct I/O on VxFS! */
/* https://searchcode.com/codesearch/view/4922497/ */
#  define VX_IOCTL        (('V' << 24) | ('X' << 16) | ('F' << 8))
#  define VX_SETCACHE     (VX_IOCTL | 1)   /* set cache advice */
#  define VX_GETCACHE     (VX_IOCTL | 2)   /* get cache advice */
#  define VX_DIRECT       0x00004          /* perform direct (un-buffered) I/O */
//#endif /* defined(SOLARIS) */
#endif /* defined(__hpux) */

#if defined(__linux__) || defined(MacDarwin)
#  include <stdint.h>
#  define XFS_DIO_BLOCK_SIZE 4096
#endif /* defined(__linux__) */
#include <poll.h>

#if defined(DEC)
#  define LOG_DIAG_INFO	1
/* Tru64 Unix */
#  if defined(O_DIRECTIO)
#    define O_DIRECT	O_DIRECTIO
#  endif
#endif /* defined(DEC) */

#if !defined(O_ASYNC)
/* Psuedo flag only used for Windows to enable its' async I/O. */
# define O_ASYNC	0x0         /* Asynchronous I/O. */
#endif /* !defined(O_ASYNC) */

/*
 * Define unused POSIX open flag for enabling Direct I/O in OS open.
 * Note: This is only used on Solaris and MacOS (at present).
 * For Solaris, thie O_DIRECT flag is cleared before the OS open()!								   .
 */
#if !defined(O_DIRECT)
# if defined(__sun)
/* To enable the equivalent of direct I/O. (unbuffered I/O) */
#  define O_DIRECT	0x400000	/* Direct disk access. */
# elif defined(MacDarwin)
#  define O_DIRECT	0x800000	/* F_NOCACHE on MacOS! */
# else /* unknown OS, force new code, no assumptions! */
#  error "Please define O_DIRECT for this operating system!"
/* Note: For Linux, this is defined as: O_DIRECT 040000 */
/*	 and _GNU_SOURCE MUST be specified to define this! */
//#  define O_DIRECT	0		/* No Direct I/O. */
# endif
#endif /* !defined(O_DIRECT) */

/*
 * Note: FreeBSD does NOT defined O_DSYNC, but O_SYNC is required for POSIX!
 */
#if !defined(O_DSYNC)
#  define O_DSYNC   O_SYNC
#endif /* !defined(O_DSYNC) */

#define HANDLE			int
#define INVALID_HANDLE_VALUE	-1

#define DIRSEP		'/'
#define DIRSEP_STR	"/"
#define DEV_PREFIX	"/dev/"		/* Physical device name prefix.	*/
#define DEV_LEN		5		/* Length of device name prefix.*/

#define DEV_DIR_PREFIX		"/dev/"
#define DEV_DIR_LEN		(sizeof(DEV_DIR_PREFIX) - 1)
#define DEV_DEVICE_LEN		128

#if defined(__hpux) || defined(SOLARIS)
#  define DEV_BDIR_PREFIX	"/dev/dsk/"
#  define DEV_BDIR_LEN		(sizeof(DEV_BDIR_PREFIX) - 1)
#  define DEV_RDIR_PREFIX	"/dev/rdsk/"
#  define DEV_RDIR_LEN		(sizeof(DEV_RDIR_PREFIX) - 1)
#endif /* defined(__hpux) || defined(SOLARIS) */

#define TEMP_DIR		"/var/tmp"
#define TEMP_DIR_NAME		TEMP_DIR
#define TEMP_DIR_LEN		(sizeof(TEMP_DIR_NAME) - 1)

#if defined(Nimble)
/* Nimble Tools/Scripts/Data Files: */
#  define TOOLS_DIR             "/usr/local/bin"
#  define PATTERN_DIR		TOOLS_DIR"/data"
#  define DEDUP_PATTERN_FILE	PATTERN_DIR"/pattern_dedup"
#  define TRIGGER_SCRIPT	TOOLS_DIR"/nosmgr.py --array=%array --stop"
#  define STOPON_FILE		TEMP_DIR"/stopdt"
#else /* !defined(Nimble) */
/* Historic from NetApp */
#  define TOOLS_DIR             "/usr/software/test/noarch"
#  define PATTERN_DIR		TOOLS_DIR"/dtdata"
#  define DEDUP_PATTERN_FILE	PATTERN_DIR"/pattern_dedup"
#  define TRIGGER_SCRIPT	TOOLS_DIR"/dt_noprog_script.ksh"
#  define STOPON_FILE		TEMP_DIR"/stopit"
#endif /* defined(Nimble) */

/*
 * Define POSIX Mode for Creating Files & Directories:
 */
/* Note: This is 0777 for Read/Write/Execute for user, group, and other! */
#define DIR_CREATE_MODE	 (S_IRWXU | S_IRWXG | S_IRWXO)
/* Note: This is 0666 or Read/Write for user, group, and other! */
#define FILE_CREATE_MODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

#define OS_API_TYPE			"posix"
#define OS_AIO_READ			"aio_read"
#define OS_AIO_WRITE			"aio_write"
#define OS_OPEN_FILE_OP			"open"
#define OS_CLOSE_FILE_OP		"close"
#define OS_DELETE_FILE_OP		"unlink"
#define OS_FLUSH_FILE_OP		"fsync"
#define OS_READ_FILE_OP			"read"
#define OS_WRITE_FILE_OP		"write"
#define OS_PREAD_FILE_OP		"pread"
#define OS_PWRITE_FILE_OP		"pwrite"
#define OS_RENAME_FILE_OP		"rename"
#define OS_SEEK_FILE_OP			"lseek"
#define OS_TRUNCATE_FILE_OP		"truncate"
#define OS_FTRUNCATE_FILE_OP		"ftruncate"
#define OS_CREATE_DIRECTORY_OP		"mkdir"
#define OS_REMOVE_DIRECTORY_OP		"rmdir"
#define OS_GET_FILE_ATTR_OP		"stat"
#define OS_GET_FS_INFO_OP		"statvfs"
#define OS_GET_FILE_SIZE_OP		"fstat"
#define OS_LINK_FILE_OP			"link"
#define OS_UNLINK_FILE_OP		OS_DELETE_FILE_OP
#define OS_SYMLINK_FILE_OP		"symlink"
#define OS_LOCK_FILE_OP			"lock"
#define OS_UNLOCK_FILE_OP		"unlock"
#define OS_SET_END_OF_FILE_OP		"SetEndOfFile"
#define OS_TRIM_FILE_OP			"FITRIM"

#define OS_READONLY_MODE	O_RDONLY
#define OS_WRITEONLY_MODE	O_WRONLY
#define OS_READWRITE_MODE	O_RDWR

/* 
 * POSIX pthread_create() returns a pthread_t for the thread ID.
 * For most POSIX implementations, this is a "void *".
 * This format control is for displaying the OS thread ID.
 */
typedef pthread_t	os_tid_t;
#define OS_TID_FMT	"%p"
#define OS_THREAD_FMT	"%p"

typedef off_t Offset_t;
typedef int   os_error_t;
typedef ino_t os_ino_t;
#define OS_FILE_ID	"Inode"
typedef dev_t os_dev_t;

#define os_chdir(path)		chdir(path)

#if defined(__hpux) || defined(MacDarwin) || defined(SOLARIS)
/* Extra processing for these operation systems for Direct I/O. */
extern HANDLE os_open_file(char *name, int oflags, int perm);
#else /* !defined(__hpux) && !defined(MacDarwin) && !defined(SOLARIS) */
# define os_open_file		open
#endif /* defined(__hpux) || defined(MacDarwin) || defined(SOLARIS) */
#define os_close_file		close
#define os_seek_file		lseek
#define os_read_file		read
#define os_write_file		write
#define os_pread_file		pread
#define os_pwrite_file		pwrite
#define os_delete_file		unlink
#define os_flush_file		fsync
#define os_move_file		rename
#define os_rename_file		rename
#define os_truncate_file	truncate
#define os_ftruncate_file	ftruncate
#define os_link_file		link
#define os_unlink_file		os_delete_file
#define os_symlink_file		symlink
#define os_getpid		getpid
#define os_getppid		getppid
#define os_set_random_seed	srandom
#define os_symlink_supported()	True

#define OS_ERROR_INVALID	EINVAL
#define OS_ERROR_DISK_FULL	ENOSPC

/* TODO: Define a Unix version of high resolution time like Windows has. */
#define highresolutiontime	gettimeofday

#define os_isAccessDenied(error) ( (error == EACCES) ? True : False)
#define os_isADirectory(error)	( (error == EISDIR) ? True : False)
#define os_isCancelled(error)	( (error == ECANCELED) ? True : False)
#define os_isIoError(error)	( (error == EIO) ? True : False)
#define os_isFileExists(error)	( (error == EEXIST) ? True : False)
#define os_isFileNotFound(error) ( (error == ENOENT) ? True : False)
#define os_isDirectoryNotFound(error) ( (error == ENOENT) ? True : False)
#define os_isDiskFull(error)	( ((error == ENOSPC) || (error == EDQUOT)) ? True : False)
#define os_isLocked(error)	( ((error == EACCES) || (error == EAGAIN)) ? True : False)
#define os_getDiskFullMsg(error) (error == ENOSPC) ? "No space left on device (ENOSPC)" : "Quota exceeded (EDQUOT)"
#define os_getDiskFullSMsg(error) (error == EDQUOT) ? "EDQUOT" : "ENOSPC"
#define os_getDiskFullError()	ENOSPC
#define os_mapDiskFullError(error) error

#define os_perror		Perror
#define os_tperror		tPerror
#define os_get_error()		errno
#define os_set_error(error)	errno = error
#define os_get_error_msg(error)	strerror(error)
#define os_free_error_msg(msg)	

#if !defined(SYSLOG)

/* Note: These get used internally to define dt's log levels! */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_INFO        6       /* informational */

#define syslog	os_syslog
extern void os_syslog(int priority, char *format, ...);

#endif /* !defined(SYSLOG) */

#define os_sleep(value)		(void)poll(0, 0, (int)(value*MSECS))
#define os_msleep(value)	(void)poll(0, 0, (int)value)
//#define os_usleep(value)	(void)poll(0, 0, (int)(value/MSECS))
/* Note: Signals will terminate usleep() prematurely. */
#define os_usleep(value)	(void)usleep((useconds_t)value)
/* TODO: nanosleep() - high resolution sleep */
//#define os_nanosleep(value)

#define os_get_protocol_version(handle)	    NULL

#define os_set_timer_resolution(value)      True
#define os_reset_timer_resolution(value)    True

#if defined(INLINE_FUNCS)
#define os_create_directory(dir_path, permissions) \
			    mkdir(dir_path, permissions)

#define os_remove_directory(dir_path) \
			    rmdir(dir_path)
#else /* !defined(INLINE_FUNCS) */
extern int os_create_directory(char *dir_path, int permissions);
extern int os_remove_directory(char *dir_path);
#endif /* defined(INLINE_FUNCS) */

extern int os_lock_file(HANDLE fd, Offset_t start, Offset_t length, int lock_type);
extern int os_unlock_file(HANDLE fd, Offset_t start, Offset_t length);
extern int os_xlock_file(HANDLE fd, Offset_t start, Offset_t length, int lock_type, hbool_t exclusive, hbool_t immediate);
#define os_xunlock_file	os_unlock_file

extern os_dev_t	os_get_devID(char *path, HANDLE handle);
extern char *os_gethostname(void);
extern char *os_getosinfo(void);
extern char *os_getusername(void);
extern char *ConvertBlockToRawDevice(char *block_device);
extern char *ConvertDeviceToScsiDevice(char *device);

extern hbool_t os_isEof(ssize_t count, int error);

#if defined(NEEDS_SETENV_API)

/* Older versions of HP-UX and Solaris do NOT have this API! */
extern int setenv(const char *name, const char *value, int overwrite);

#endif /* defined(NEEDS_SETENV_API) */

#endif /* !defined(_DTUNIX_H_) */
