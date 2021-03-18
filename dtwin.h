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

#if !defined(_DT_WIN32_H_)
#define _DT_WIN32_H_ 1

#include <winsock2.h>
#include <ws2tcpip.h>

#include <process.h>
#include <time.h>
#include <windows.h>
#include <io.h>                 
#include <direct.h>

/*
 * Define POSIX Mode for Creating Files:
 * Note: These are *not* actually used for Windows.
*/
/* Note: This is 0777 for Read/Write/Execute for user, group, and other! */
//#define DIR_CREATE_MODE	 (S_IRWXU | S_IRWXG | S_IRWXO)
#define DIR_CREATE_MODE 0777
/* Note: This is 0666 or Read/Write for user, group, and other! */
//#define FILE_CREATE_MODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#define FILE_CREATE_MODE 0666

/*
 * Define unused POSIX open flags for Windows specific equivalent.
 * Note: These are *only* used to map to Windows file attributes!
 * 
 * Warning: Ensure these don't conflict with flags in WINNT.H!
 * dt uses a combination of Windows and POSIX flags! (at present)
 */
#if !defined(O_DIRECT)
/* To enable the equivalent of direct I/O. (unbuffered I/O) */
/* This enables: (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH) */
# define O_DIRECT	0x100000    /* Direct disk access.  */
#endif /* !defined(O_DIRECT) */

#if !defined(O_ASYNC)
/* To enable the equivalent of async I/O. (overlapped I/O) */
/* This enables: FILE_FLAG_OVERLAPPED attribute. */
# define O_ASYNC	0x200000    /* Asynchronous I/O.  */
#endif /* !defined(O_ASYNC) */

#if !defined(O_SYNC)
/* This enables: FILE_FLAG_WRITE_THROUGH attribute. */
# define O_SYNC	0x400000    	/* Synchronize data written.  */
#endif /* !defined(O_SYNC) */

#if !defined(O_DSYNC)
# define O_DSYNC	O_SYNC
#endif /* !defined(O_DSYNC) */

#define ASSERT _ASSERT

#define uint8 ULONGLONG
#define int8 LONGLONG

#if defined(_WIN64)
typedef __int64    ssize_t;
#else /* !defined(_WIN64) */
typedef signed int ssize_t;
#endif /* defined(_WIN64) */

typedef __int64 Offset_t;
typedef DWORD	os_error_t;
/* This is the 64-bit file index. */
typedef __int64 os_ino_t;
#define OS_FILE_ID	"ID"
typedef DWORD	os_dev_t;
#define os_get_devID(path, handle)	(os_dev_t)0

/*
 * OS Specific Functions:
 */ 
#define os_sleep(value)		Sleep(value*MSECS)
#define os_msleep(value)	Sleep(value)
#define os_usleep(value)	Sleep(value/MSECS)

/* URL: http://msdn.microsoft.com/en-us/library/windows/desktop/dd757624(v=vs.85).aspx */
#define os_set_timer_resolution(value) \
    ( (timeBeginPeriod(value) == TIMERR_NOERROR) ? True : False)
#define os_reset_timer_resolution(value) \
    ( (timeEndPeriod(value) == TIMERR_NOERROR) ? True : False)

/*
 * POSIX Thread Definitions:
 */
typedef HANDLE pthread_t;
/* 
 * CreateThread() returns a HANDLE, which is a PVOID, so we cannot
 * safely use DWORD for a 64-bit machine/pointer or we'll truncate!
 */
//typedef DWORD pthread_t;
typedef unsigned long pthread_attr_t;
typedef HANDLE pthread_mutex_t;
/* For mutex attributes. */
typedef unsigned long pthread_mutexattr_t;

/* 
 * Windows CreateThread() returns a HANDLE (PVOID), and also supports
 * a thread ID via GetCurrentThreadId() which returns DWORD thread ID.
 * This format control is for displaying the OS thread ID.
 */
typedef DWORD		os_tid_t;
#define OS_TID_FMT	"0x%x"
/* Note: Windows "%p" does *not* prefix with "0x" like Unix! */
#define OS_THREAD_FMT	"0x%p"

typedef struct {
#define SIGNAL          0
#define BROADCAST       1
#define MAX_EVENTS      2
        HANDLE events_[MAX_EVENTS];  /* Signal and broadcast event HANDLEs */
} pthread_cond_t;

/* Note: Not real, just required for compiling! */
#define PTHREAD_CREATE_JOINABLE		0
#define PTHREAD_CREATE_DETACHED		1

/* Note: Dummy flags for OS specific functions. */
#define PTHREAD_CANCEL_ASYNCHRONOUS	0
#define PTHREAD_CANCEL_DEFERRED		1

extern int pthread_attr_init(pthread_attr_t *attr);
extern int pthread_attr_setscope(pthread_attr_t *attr, unsigned type);
extern int pthread_attr_setdetachstate(pthread_attr_t *attr, int type);
extern int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);
extern int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);

extern int pthread_mutexattr_init(pthread_mutexattr_t *attr);
extern int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);

/* Mutex Types:*/
#define PTHREAD_MUTEX_NORMAL            0x0
#define PTHREAD_MUTEX_ERRORCHECK        0x2
#define PTHREAD_MUTEX_RECURSIVE         0x4
#define PTHREAD_MUTEX_DEFAULT           PTHREAD_MUTEX_NORMAL
extern int pthread_mutexattr_gettype(pthread_mutexattr_t *attr, int *type);
extern int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);

extern int pthread_cancel(pthread_t thread);
extern int pthread_detach(pthread_t thread);
extern int pthread_create(pthread_t *tid, pthread_attr_t *attr, 
			  void *(*func)(void *), void *arg);
extern void pthread_exit(void *exit_code);
extern void pthread_kill(pthread_t tid, int sig);
extern int pthread_mutex_init(pthread_mutex_t *lock, void *attr);
extern int pthread_mutex_destroy(pthread_mutex_t *mutex);
extern int pthread_mutex_trylock(pthread_mutex_t *lock);
extern int pthread_mutex_lock(pthread_mutex_t *lock);
extern int pthread_mutex_unlock(pthread_mutex_t *lock);
extern int pthread_cond_init(pthread_cond_t *cv, const void *dummy);
extern int pthread_cond_broadcast(pthread_cond_t *cv);
extern int pthread_cond_signal(pthread_cond_t *cv);
extern int pthread_cond_wait(pthread_cond_t *cv, pthread_mutex_t *lock);
extern int pthread_join(pthread_t thread, void **exit_value);
extern os_tid_t pthread_self(void);

#define PTHREAD_STACK_MIN	16384

#define PTHREAD_SCOPE_PROCESS	0 
#define PTHREAD_SCOPE_SYSTEM	1
#define PTHREAD_NORMAL_EXIT	0

/*
 * I/O Functions:
 */
extern HANDLE os_open_file(char *name, int oflags, int perm);
extern Offset_t os_seek_file(HANDLE handle, Offset_t offset, int whence);
extern ssize_t os_pread_file(HANDLE handle, void *buffer, size_t size, Offset_t offset);
extern ssize_t os_pwrite_file(HANDLE handle, void *buffer, size_t size, Offset_t offset);

#define OS_API_TYPE			"win32"
#define OS_AIO_READ			"ReadFile"
#define OS_AIO_WRITE			"WriteFile"
#define OS_OPEN_FILE_OP			"CreateFile"
#define OS_CLOSE_FILE_OP		"CloseHandle"
#define OS_DELETE_FILE_OP		"DeleteFile"
#define OS_FLUSH_FILE_OP		"FlushFileBuffers"
#define OS_READ_FILE_OP			"ReadFile"
#define OS_WRITE_FILE_OP		"WriteFile"
#define OS_PREAD_FILE_OP		"ReadFile"
#define OS_PWRITE_FILE_OP		"WriteFile"
#define OS_RENAME_FILE_OP		"MoveFile"
#define OS_SEEK_FILE_OP			"SetFilePointerEx"
#define OS_TRUNCATE_FILE_OP		"SetEndOfFile"
#define OS_FTRUNCATE_FILE_OP		"SetEndOfFile"
#define OS_CREATE_DIRECTORY_OP		"CreateDirectory"
#define OS_REMOVE_DIRECTORY_OP		"RemoveDirectory"
#define OS_GET_FILE_ATTR_OP		"GetFileAttributesEx"
#define OS_GET_VOLUME_INFO_OP		"GetVolumeInformation"
#define OS_GET_VOLUME_PATH_OP		"GetVolumePathName"
#define OS_GET_FILE_SIZE_OP		"GetFileSizeEx"
#define OS_LINK_FILE_OP			"CreateHardLink"
#define OS_UNLINK_FILE_OP		OS_DELETE_FILE_OP
#define OS_SYMLINK_FILE_OP		"CreateSymbolicLink"
#define OS_LOCK_FILE_OP			"LockFile"
#define OS_UNLOCK_FILE_OP		"UnlockFile"
#define OS_SET_END_OF_FILE_OP		"SetEndOfFile"
#define OS_SET_SPARSE_FILE_OP		"FSCTL_SET_SPARSE"
#define OS_TRIM_FILE_OP			"FSCTL_FILE_LEVEL_TRIM"

/* Note: Not used for Windows locking. */
#define F_RDLCK		0
#define F_WRLCK		1

#define getuid()	1
#define sleep(a)	Sleep(a*1000)
#define msleep(a)	Sleep(a)
#define usleep(a)	Sleep(a/1000)

extern __inline clock_t times(struct tms *buffer);

#define WEXITSTATUS(status)	status

#define dup		_dup
#define getcwd		_getcwd
#define getpid		_getpid
//#define unlink		_unlink
#define stat		_stat
#define strdup		_strdup
#define fileno		_fileno
#define isatty		_isatty
#if defined(_USE_32BIT_TIME_T)
#  define localtime_s	_localtime32_s
#else
#  define localtime_s	_localtime64_s
#endif
/* Unix Equivalent API: */
extern struct tm *localtime_r(const time_t *timep, struct tm *tm);

#define mkdir		_mkdir
#define popen(cmd, mode) _popen(cmd, mode "b")
#define pclose		_pclose
#define strncasecmp	_strnicmp
#define strtoll		_strtoi64
#define strtoull	_strtoui64
#define strtok_r	strtok_s
#define SIGALRM		14

#define os_getpid		GetCurrentProcessId
#define os_getppid		GetCurrentProcessId
#define os_set_random_seed	srand

#define os_msleep(msecs)	Sleep(msecs)

#define os_chdir(path)		(SetCurrentDirectory(path) ? SUCCESS : FAILURE)

extern __inline int os_close_file(HANDLE handle);
extern __inline int os_delete_file(char *file);
extern __inline int os_flush_file(HANDLE *handle);
extern __inline int os_create_directory(char *dir_path, int permissions);
extern __inline int os_remove_directory(char *dir_path);
extern __inline int os_lock_file(HANDLE fh, Offset_t start, Offset_t length, int type);
extern __inline int os_unlock_file(HANDLE fh, Offset_t start, Offset_t length);
extern __inline int os_xlock_file(HANDLE fh, Offset_t start, Offset_t length, int type, hbool_t exclusive, hbool_t immediate);
extern __inline int os_xunlock_file(HANDLE fh, Offset_t start, Offset_t length);

extern __inline int os_move_file(char *oldpath, char *newpath);
extern __inline int os_rename_file(char *oldpath, char *newpath);
extern __inline int os_link_file(char *oldpath, char *newpath);
extern __inline int os_symlink_file(char *oldpath, char *newpath);
extern hbool_t os_symlink_supported(void);

#define os_unlink_file		os_delete_file

extern HANDLE os_open_file(char *name, int oflags, int perm);

extern ssize_t os_pread_file(HANDLE handle, void *buffer, size_t size, Offset_t offset);
extern ssize_t os_pwrite_file(HANDLE handle, void *buffer, size_t size, Offset_t offset);

extern __inline ssize_t os_read_file(HANDLE handle, void *buffer, size_t size);
extern __inline ssize_t os_write_file(HANDLE handle, void *buffer, size_t size);
extern Offset_t os_seek_file(HANDLE handle, Offset_t offset, int whence);

extern int os_truncate_file(HANDLE handle, Offset_t offset);
extern int os_ftruncate_file(HANDLE handle, Offset_t offset);

extern char *os_get_protocol_version(HANDLE handle);

typedef int		pid_t;
typedef unsigned int	speed_t;
//typedef int		ssize_t; 

#define SYSLOG		1
/*
 * See winnt.h:
 *
 * The types of events that can be logged.
 *
 * #define EVENTLOG_SUCCESS                0X0000
 * #define EVENTLOG_ERROR_TYPE             0x0001
 * #define EVENTLOG_WARNING_TYPE           0x0002
 * #define EVENTLOG_INFORMATION_TYPE       0x0004
 * #define EVENTLOG_AUDIT_SUCCESS          0x0008
 * #define EVENTLOG_AUDIT_FAILURE          0x0010
 */
/* Note: These get used internally to define dt's log levels! */
#define LOG_CRIT	EVENTLOG_ERROR_TYPE
#define LOG_ERR		EVENTLOG_ERROR_TYPE
#define LOG_INFO	EVENTLOG_INFORMATION_TYPE
#define LOG_WARNING	EVENTLOG_WARNING_TYPE

extern void syslog(int priority, char *format, ...);
extern Offset_t SetFilePtr(HANDLE hf, Offset_t distance, DWORD MoveMethod);
struct timezone {
    int	tz_minuteswest;	/* minutes W of Greenwich */
    int	tz_dsttime;	/* type of dst correction */
};
extern int gettimeofday(struct timeval *tv, struct timezone *tz);
extern int highresolutiontime(struct timeval *tv, struct timezone *tz);

#if !defined(INVALID_SET_FILE_POINTER)
#   define INVALID_SET_FILE_POINTER -1
#endif

typedef int             pid_t;
// typedef unsigned int size_t; -- ISO says not necessary
//typedef int             ssize_t;        // signed size_t

/*
 * We are defining aiocb structure similer to POSIX
 * aiocb structure, to emulate AIO via overlapped I/O.
 */
struct aiocb {			/* aiocb structure for windows */
    OVERLAPPED	overlap;	/* Overlapped structure */
    char	*aio_buf;	/* buffer pointer */
    HANDLE	aio_fildes;	/* file descriptro */
    Offset_t	aio_offset;	/* file offset */
    size_t	aio_nbytes;	/* Length of transfer */
    unsigned	bytes_rw;	/* bytes read/write at time of checking status by GetOverlappedResult */
    DWORD	last_error;	/* The GetLastError() value */
};

struct tms {                
     clock_t  tms_utime;     
     clock_t  tms_stime;     
     clock_t  tms_cutime;    
     clock_t  tms_cstime;    
};
 
/* Note: We may wish to make default DIRSEP the same as POSIX (Unix). */
#define DIRSEP		'\\'
#define DIRSEP_STR	"\\"
#define DEV_PREFIX	"\\\\.\\"	/* Native Windows device dir.	*/
#define DEV_LEN		4		/* That's for "\\.\" prefix.	*/
#define ADEV_PREFIX	"//./"		/* Windows hidden device dir.	*/
#define ADEV_LEN	4		/* Length of device name prefix.*/

#define DEV_DIR_PREFIX		"\\\\.\\"
#define DEV_DIR_LEN		(sizeof(DEV_DIR_PREFIX) - 1)
#define DEV_DEVICE_LEN		64

#define TEMP_DIR		"C:\\temp"
#define TEMP_DIR_NAME		TEMP_DIR
#define TEMP_DIR_LEN		(sizeof(TEMP_DIR_NAME) - 1)

#define TOOLS_DIR		"C:\\tools"
#define PATTERN_DIR	        "x:\\noarch\\dtdata"
#define DEDUP_PATTERN_FILE	PATTERN_DIR"\\pattern_dedup"
#define TRIGGER_SCRIPT		TOOLS_DIR"\\dt_noprog_script.bat"
#define STOPON_FILE		TEMP_DIR"\\stopit"

#define OS_READONLY_MODE	GENERIC_READ
#define OS_WRITEONLY_MODE	GENERIC_WRITE
#define OS_READWRITE_MODE	(GENERIC_READ | GENERIC_WRITE)

/* ------------------------------------------------------------------------ */

/* Note: These should go... all code should use os_xxx() API's! */
#define open	os_open_file
#define close	os_close_file
#define lseek	os_seek_file
#define	read	os_read_file
#define write	os_write_file
#define pread	os_pread_file
#define pwrite	os_pwrite_file
#define unlink	os_unlink_file
               
typedef void *caddr_t;

#define SIGKILL SIGINT

/* This does token pasting, so ensure the "mode" is constant */
#define popen(cmd, mode) _popen(cmd, mode "b")

extern HANDLE exitMutex;
//extern DWORD ParentThreadId;

extern HANDLE win32_dup(HANDLE handle);
extern int getpagesize(void);
extern int setenv(const char *name, const char *value, int overwrite);
extern int IsDriveLetter(char *device);
extern char *FindScsiDevice(char *device);

#define OS_ERROR_INVALID	ERROR_INVALID_PARAMETER
#define OS_ERROR_DISK_FULL	ERROR_DISK_FULL

#define os_isAccessDenied(error) ( (error == ERROR_ACCESS_DENIED) ? True : False)
#define os_isADirectory(error)	( (error == ERROR_ACCESS_DENIED) ? True : False)
#define os_isCancelled(error)	( (error == ERROR_CANCELLED) ? True : False)
#define os_isIoError(error)	( (error == ERROR_IO_DEVICE) ? True : False)
#define os_isFileExists(error)	( (error == ERROR_ALREADY_EXISTS) ? True : False)
#define os_isFileNotFound(error) ( (error == ERROR_FILE_NOT_FOUND) ? True : False)
#define os_isDirectoryNotFound(error) ( (error == ERROR_PATH_NOT_FOUND) ? True : False)
#define os_isDiskFull(error)	( ((error == ERROR_DISK_FULL) || (error == ERROR_HANDLE_DISK_FULL)) ? True : False)
#define os_isLocked(error)	( ((error == ERROR_LOCKED) || (error == ERROR_LOCK_VIOLATION) || (error == ERROR_LOCK_FAILED)) ? True : False)
#define os_getDiskFullMsg(error) (error == ERROR_DISK_FULL) ? "Disk full (ERROR_DISK_FULL)" : "The disk is full (ERROR_HANDLE_DISK_FULL)"
#define os_getDiskFullSMsg(error) (error == ERROR_HANDLE_DISK_FULL) ? "ERROR_HANDLE_DISK_FULL" : "ERROR_DISK_FULL"
#define os_getDiskFullError()	ERROR_DISK_FULL
#define os_mapDiskFullError(error) ENOSPC
#define os_isStreamsUnsupported(error) ( ((error == ERROR_INVALID_NAME) || (error == ERROR_FILE_NOT_FOUND)) ? True : False)

#define os_tperror		tPerror
#define os_get_error()		GetLastError()
#define os_set_error(error)	SetLastError(error)
#define os_free_error_msg(msg)	(void)LocalFree((HLOCAL)msg)
extern char *os_get_error_msg(int error);

extern hbool_t os_isEof(ssize_t count, int error);

extern char *os_gethostname(void);
extern char *os_getosinfo(void);
extern char *os_getusername(void);

extern os_error_t win32_getuncpath(char *path, char **uncpathp);

/* Windows does *not* need this device conversion function! */
#define ConvertDeviceToScsiDevice(device)	strdup(device)

#endif /* _DT_WIN32_H_ */
