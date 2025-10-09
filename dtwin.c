/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2025			    *
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
 * Module:	dtwin.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	This module contains *unix OS specific functions.
 * 
 * Modification History:
 * 
 * October 8th, 2025 by Robin T. Miller
 *      If trying to open the volume handle fails, disable file system mapping.
 * 
 * June 5th, 2020 by Robin T. Miller
 *      Update os_pread_file() to detecting overlapped I/O, then wait for
 * accordingly for it to finish. When async I/O is enable, and read after
 * write is enabled, currently reads are done synchonously. While this
 * defeats the purpose of async I/O, we don't wish these reads to fail!
 * 
 * May 5th, 2020 by Robin T. Miller
 *      Include the high resolution gettimeofday() as highresolutiontime(),
 * which will be used where more accurate timing is desired, such as history
 * entries. The Unix eqivalent gettimeofday() is only accurate to 10-15ms,
 * but is preferred (by some) in block tags (btags) for Epoch write times.
 * Note: Robin often creates large history with timing for tracing I/O's.
 * 
 * April 6th, 2015 by Robin T. Miller
 * 	In os_rename_file(), do not delete the newpath unless the oldpath
 * exists. Depending on the test, we can delete files that should remain,
 * plus this behavior is closer to POSIX rename() semantics (I believe).
 * 
 * February 7th, 2015 by Robin T. Miller
 * 	Fixed bug in POSIX flag mapping, O_RDONLY was setting the wrong
 * access (Read and Write), which failed on a read-only file! The author
 * forgot that O_RDONLY is defined with a value of 0, so cannot be checked!
 *
 * January 2012 by Robin T. Miller
 * 	pthread* API's should not report errors, but simply return the
 * error to the caller, so these changes have been made. The caller needs
 * to use OS specific functions to handle *nix vs. Windows error reporting.
 * 
 * Note: dt has its' own open/close/seek functions, so these may go!
 * 	 Still need to evaluate the AIO functions. and maybe cleanup more
 * error handling, and probably remove unused API's.
 */

#include "dt.h"
#include "dtwin.h"
#include <stdio.h>
#include <signal.h>
#include <winbase.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <io.h>

//#define WINDOWS_XP 1

#define Thread __declspec(thread)

/*
 * Local Storage:
 */

/*
 * Forward References:
 */
char *dt_get_volume_path_name(dinfo_t *dip, char *path);
void map_posix_flags(dinfo_t *dip, char *name, int posix_flags,
		     DWORD *DesiredAccess, DWORD *CreationDisposition,
		     DWORD *FlagsAndAttributes, DWORD *ShareMode);

/*
 * Fake pthread implementation using Windows threads. Windows threads
 * are generally a superset of pthreads, so there is no lost functionality.
 * 
 * Note: Lots of Windows documentation/links added by Robin, who does not
 * do sufficient Windows programming to feel confident in his theads knowledge.
 */
int
pthread_attr_init(pthread_attr_t *attr)
{
    return( PTHREAD_NORMAL_EXIT );
}

int
pthread_attr_setscope(pthread_attr_t *attr, unsigned type)
{
    return( PTHREAD_NORMAL_EXIT );
}

int
pthread_attr_setdetachstate(pthread_attr_t *attr, int type)
{
    return( PTHREAD_NORMAL_EXIT );
}

/* 
 * Reference:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms682453(v=vs.85).aspx
 *
 * Remarks:
 * The number of threads a process can create is limited by the available virtual
 * memory. By default, every thread has one megabyte of stack space. Therefore,
 * you can create at most 2,048 threads. If you reduce the default stack size, you
 * can create more threads. However, your application will have better performance
 * if you create one thread per processor and build queues of requests for which
 * the application maintains the context information. A thread would process all
 * requests in a queue before processing requests in the next queue.
 */ 
int
pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    *stacksize = MBYTE_SIZE;
    return( PTHREAD_NORMAL_EXIT );
}

int
pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    return( PTHREAD_NORMAL_EXIT );
}

int
pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    return( PTHREAD_NORMAL_EXIT );
}

int
pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    return( PTHREAD_NORMAL_EXIT );
}

int
pthread_mutexattr_gettype(pthread_mutexattr_t *attr, int *type)
{
    return( PTHREAD_NORMAL_EXIT );
}
    
int
pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
    return( PTHREAD_NORMAL_EXIT );
}

/* 
 * Windows Notes from:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms682659(v=vs.85).aspx
 * A thread in an executable that is linked to the static C run-time library  
 * (CRT) should use _beginthread and _endthread for thread management rather  
 * than CreateThread and ExitThread. Failure to do so results in small memory  
 * leaks when the thread calls ExitThread. Another work around is to link the  
 * executable to the CRT in a DLL instead of the static CRT.  
 * Note that this memory leak only occurs from a DLL if the DLL is linked to  
 * the static CRT *and* a thread calls the DisableThreadLibraryCalls function.  
 * Otherwise, it is safe to call CreateThread and ExitThread from a thread in  
 * a DLL that links to the static CRT.  
 *  
 * Robin's Note: September 2012
 * Since we are NOT calling DisableThreadLibraryCalls(), I'm assuming we  
 * do not need to worry about using these alternate thread create/exit API's!
 * Note: The code has left, but not enabled since _MT changes to _MTx.
 * Update: Reenabling _MT method for threads, otherwise hangs occur!
 * Note: The hangs occur while waiting for the parent thread to exit.
 *  
 * A different warning...
 * If you are going to call C run-time routines from a program built
 * with Libcmt.lib, you must start your threads with the _beginthread
 * or _beginthreadex function. Do not use the Win32 functions ExitThread
 * and CreateThread. Using SuspendThread can lead to a deadlock when more
 * than one thread is blocked waiting for the suspended thread to complete
 * its access to a C run-time data structure.
 * URL: http://msdn.microsoft.com/en-us/library/7t9ha0zh(VS.80).aspx
 *
 * Note: My testing proved this to be true, but hangs occurred regardless
 * of using these alternate thread API's with thread suspend/resume!
 *
 * Outputs:
 *	tid for Windows is actually the Thread handle, NOT the thread ID!
 */
int
pthread_create(pthread_t *tid, pthread_attr_t *attr,
	       void *(*func)(void *), void *arg)
{
    DWORD dwTid;
#if defined(_MT)
    /* 
     * uintptr_t _beginthreadex( 
     *   void *security,
     *   unsigned stack_size,
     *   unsigned ( *start_address )( void * ),
     *   void *arglist,
     *   unsigned initflag,
     *   unsigned *thrdaddr );
     */
#define myTHREAD_START_ROUTINE unsigned int (__stdcall *)(void *)
    *tid = (pthread_t *)_beginthreadex(NULL, 0, (myTHREAD_START_ROUTINE)func, arg, 0, &dwTid);
#else /* !defined(_MT) */
    /*
     * Use CreateThread so we have a real thread handle
     * to synchronize on
     * HANDLE WINAPI CreateThread(
     *  _In_opt_   LPSECURITY_ATTRIBUTES lpThreadAttributes,
     *  _In_       SIZE_T dwStackSize,
     *  _In_       LPTHREAD_START_ROUTINE lpStartAddress,
     *  _In_opt_   LPVOID lpParameter,
     *  _In_       DWORD dwCreationFlags,
     *  _Out_opt_  LPDWORD lpThreadId
     * );
     */
    *tid = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, &dwTid);
#endif /* defined(_MT) */
    if (*tid == NULL) {
	return (int)GetLastError();
    } else {
	return PTHREAD_NORMAL_EXIT;
    }
}

void
pthread_exit(void *status)
{
#if defined(_MT)
    /* 
     * void _endthreadex(unsigned retval);
     */
    _endthreadex( (unsigned)status );
#else /* !defined(_MT) */
    ExitThread( (DWORD)status );
#endif /* defined(_MT) */
}

int
pthread_join(pthread_t thread, void **exit_value)
{
    HANDLE handle = (HANDLE)thread;
    DWORD status = PTHREAD_NORMAL_EXIT;
    DWORD thread_status = PTHREAD_NORMAL_EXIT;
    DWORD wait_status;

    if (GetCurrentThread() == thread) {
	return -1;
    }
    wait_status = WaitForSingleObject(handle, INFINITE);
    if (wait_status == WAIT_FAILED) {
	status = GetLastError();
    } else if (GetExitCodeThread(handle, &thread_status) == False) {
	status = GetLastError();
    }
    if (CloseHandle(handle) == False) {
	status = GetLastError();
    }
    if (exit_value) {
	*(int *)exit_value = (int)thread_status;
    }
    return((int)status);
}

int
pthread_detach(pthread_t thread)
{
    if (CloseHandle((HANDLE)thread) == False) {
	return (int)GetLastError();
    } else {
	return PTHREAD_NORMAL_EXIT;
    }
}

/*
 * Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/ms686717(v=vs.85).aspx
 *
 * TerminateThread is used to cause a thread to exit. When this occurs, the target thread
 * has no chance to execute any user-mode code. DLLs attached to the thread are not notified
 * that the thread is terminating. The system frees the thread's initial stack.
 *
 * Windows Server 2003 and Windows XP:  The target thread's initial stack is not freed, causing
 * a resource leak.
 *
 * TerminateThread is a dangerous function that should only be used in the most extreme cases.
 * You should call TerminateThread only if you know exactly what the target thread is doing,
 * and you control all of the code that the target thread could possibly be running at the
 * time of the termination. For example, TerminateThread can result in the following problems:
 *
 *  •If the target thread owns a critical section, the critical section will not be released.
 *  •If the target thread is allocating memory from the heap, the heap lock will not be released.
 *  •If the target thread is executing certain kernel32 calls when it is terminated, the kernel32
 *  state for the thread's process could be inconsistent.
 *  •If the target thread is manipulating the global state of a shared DLL, the state of the DLL
 *  could be destroyed, affecting other users of the DLL.
 */
int
pthread_cancel(pthread_t thread)
{
    if (TerminateThread((HANDLE)thread, ERROR_SUCCESS) == False) {
	return (int)GetLastError();
    } else {
	return PTHREAD_NORMAL_EXIT;
    }
}
    
void
pthread_kill(pthread_t thread, int sig)
{
    if (sig == SIGKILL) {
	(void)TerminateThread((HANDLE)thread, sig);
    }
    return;
}

int
pthread_mutex_init(pthread_mutex_t *lock, void *attr)
{
    /*
     * HANDLE WINAPI CreateMutex(
     *   _In_opt_  LPSECURITY_ATTRIBUTES lpMutexAttributes,
     *   _In_      BOOL bInitialOwner,
     *   _In_opt_  LPCTSTR lpName
     * );
     */
    *lock = CreateMutex(NULL, False, NULL);

    if (*lock == NULL) {
	return (int)GetLastError();
    } else {
	return(PTHREAD_NORMAL_EXIT);
    }
}

int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    if (CloseHandle(*mutex) == False) {
	return (int)GetLastError();
    } else {
	return( PTHREAD_NORMAL_EXIT );
    }
}

/* 
 * the diff b/w this and mutex_lock is that this one returns
 * if any thread including itself has the mutex object locked
 * (man pthread_mutex_trylock on Solaris)
 */
int
pthread_mutex_trylock(pthread_mutex_t *lock)
{
    DWORD result = WaitForSingleObject(*lock, INFINITE);

    /* TODO - errors and return values? */
    return( PTHREAD_NORMAL_EXIT );
}

int
pthread_mutex_lock(pthread_mutex_t *lock)
{
    DWORD result = WaitForSingleObject(*lock, INFINITE);

    switch (result) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:  
	    break;
	case WAIT_FAILED:
	    return (int)GetLastError();
    }
    return( PTHREAD_NORMAL_EXIT );
}

int
pthread_mutex_unlock(pthread_mutex_t *lock)
{
    /*
     * BOOL WINAPI ReleaseMutex( _In_  HANDLE hMutex );
     *
     * Note: The caller *must* be the owner (thread) of the lock!
     */
    if (ReleaseMutex(*lock) == False) {
	return (int)GetLastError();
    } else {
	return(PTHREAD_NORMAL_EXIT);
    }
}

int
pthread_cond_init(pthread_cond_t * cv, const void *dummy)
{
    /* 
     * I'm not sure the broadcast thang works - untested 
     * I had to tweak this to use SetEvent when signalling
     * the SIGNAL event 
     */

    /* Create an auto-reset event */
    cv->events_[SIGNAL] = CreateEvent(NULL,	/* no security */
				      False,	/* auto-reset event */
				      False,	/* non-signaled initially */
				      NULL);	/* unnamed */

    /* Create a manual-reset event. */
    cv->events_[BROADCAST] = CreateEvent(NULL,	/* no security */
					 True,	/* manual-reset */
					 False,	/* non-signaled initially */
					 NULL);	/* unnamed */

    /* TODO - error handling */
    return( PTHREAD_NORMAL_EXIT );
}

/* Note: This should return pthread_t, but cannot due to type mismatch! */
os_tid_t
pthread_self(void)
{
    /* DWORD WINAPI GetCurrentThreadId(void); */
    return( (os_tid_t)GetCurrentThreadId() );
}

/* Release the lock and wait for the other lock
 * in one move.
 *
 * N.B.
 *        This isn't strictly pthread_cond_wait, but it works
 *        for this program without any race conditions.
 *
 */ 
int
pthread_cond_wait(pthread_cond_t *cv, pthread_mutex_t *lock)
{
    DWORD dwRes = 0;
    int ret = 0;

    dwRes = SignalObjectAndWait(*lock, cv->events_[SIGNAL], INFINITE, True);

    switch (dwRes) {
	case WAIT_ABANDONED:
	    //printf("SignalObjectAndWait - Wait Abandoned \n");
	    return -1;
    
	    /* MSDN says this is one of the ret values, but I get compile errors */
	//case WAIT_OBJECT_O:
	//	printf("SignalObjectAndWait thinks object is signalled\n");
	//	break;
    
	case WAIT_TIMEOUT:
	    //printf("SignalObjectAndWait timed out\n");
	    break;
    
	case 0xFFFFFFFF:
	    //os_perror(NULL, "SignalObjectAndWait failed");
	    return -1;
    }

    /* reacquire the lock */
    WaitForSingleObject(*lock, INFINITE);

    return ret;
}

/* 
 * Try to release one waiting thread. 
 */
int
pthread_cond_signal(pthread_cond_t *cv)
{
    if (!SetEvent (cv->events_[SIGNAL])) {
	return (int)GetLastError();
    } else {
	return( PTHREAD_NORMAL_EXIT );
    }
}

/* 
 * Try to release all waiting threads. 
 */
int
pthread_cond_broadcast(pthread_cond_t *cv)
{
    if (!PulseEvent (cv->events_[BROADCAST])) {
	return (int)GetLastError();
    } else {
	return(PTHREAD_NORMAL_EXIT);
    }
}

void
map_posix_flags(dinfo_t *dip, char *file, int posix_flags,
		DWORD *DesiredAccess, DWORD *CreationDisposition,
		DWORD *FlagsAndAttributes, DWORD *ShareMode)
{
    *DesiredAccess = 0;
    *CreationDisposition = 0;
    *FlagsAndAttributes = 0;
    *ShareMode = 0;
     
    /*
     * FILE_SHARE_DELETE = 0x00000004
     *   Enables subsequent open operations on a file or device to request
     * delete access. Otherwise, other processes cannot open the file or device
     * if they request delete access. If this flag is not specified, but the
     * file or device has been opened for delete access, the function fails.
     *    Note: Delete access allows both delete and rename operations.
     */
    if (posix_flags & O_EXCL) {
	/*
	 * Value 0 - Prevents other processes from opening a file or device
	 * if they request delete, read, or write access.
	 */
	*ShareMode = 0;
    } else {
	*ShareMode = (FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE);
    }

    /*
     * We map Unix style flags to the Windows equivalent (as best we can).
     * 
     * Note: Changed FILE_READ_DATA/FILE_WRITE_DATA to GENERIC methods, to
     * match what dt has always used and most other tools. Only cruisio and
     * sio/nassio use the FILE*DATA method. dwim, crud, & netmist use GENERIC.
     * At this point, I see no harm in switching, but add this note nonetheless!
     * 
     * References: (Amazing what one can do on Windows!)
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa365432(v=vs.85).aspx
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa379607(v=vs.85).aspx
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364399(v=vs.85).aspx
     * 
     */
    if (posix_flags & O_WRONLY) {
	*DesiredAccess = GENERIC_WRITE;
    } else if (posix_flags & O_RDWR) {
	*DesiredAccess = (GENERIC_READ | GENERIC_WRITE);
    } else { /* Assume O_RDONLY, which has a value of 0! */
	*DesiredAccess = GENERIC_READ;
    }
    if (posix_flags & O_APPEND) {
	*DesiredAccess |= FILE_APPEND_DATA;
    }

    if (posix_flags & O_CREAT) {
	/*
	 * Note: This logic is required to match Unix create file behavior!
	 */
	if (posix_flags & O_EXCL) {
	    /*
	     * CREATE_NEW = 1 - Creates a new file, only if it does not already exist.
	     *   If the specified file exists, the function fails and the last-error
	     * code is set to ERROR_FILE_EXISTS (80). If the specified file does not
	     * exist and is a valid path to a writable location, a new file is created.
	     */
	    *CreationDisposition = CREATE_NEW;
#if 0
	} else if (os_file_exists(file) == False) {
	    /*
	     * CREATE_ALWAYS = 2 - Creates a new file, always.
	     *   If the specified file exists and is writable, the function overwrites the
	     * file, the function succeeds, and last-error code is set to ERROR_ALREADY_EXISTS (183).
	     * If the specified file does not exist and is a valid path, a new file is created,
	     * the function succeeds, and the last-error code is set to zero.
	     * Note: The overwrite sets the file length to 0, effectively truncating!
	     */
	    *CreationDisposition = CREATE_ALWAYS;
#endif /* 0 */
	} else {
	    /*
	     * OPEN_ALWAYS = 4 - Opens a file, always.
	     *   If the specified file exists, the function succeeds and the last-error
	     * code is set to ERROR_ALREADY_EXISTS (183).
	     *   If the specified file does not exist and is a valid path to a writable
	     * location, the function creates a file and the last-error code is set to zero.
	     */
	    *CreationDisposition = OPEN_ALWAYS;
	}
    } else if (posix_flags & O_TRUNC) {
	hbool_t exists;
	/*
	 * TRUNCATE_EXISTING = 5
	 *   Opens a file and truncates it so that its size is zero bytes, only
	 * if it exists. If the specified file does not exist, the function fails
	 * and the last-error code is set to ERROR_FILE_NOT_FOUND (2).
	 *   The calling process must open the file with the GENERIC_WRITE bit set
	 * as part of the dwDesiredAccess parameter.
	 */
	if (dip) {
	    exists = dt_file_exists(dip, file);
	} else {
	    exists = os_file_exists(file);
	}
	if (exists == True) {
	    *CreationDisposition = TRUNCATE_EXISTING;
	} else {
	    *CreationDisposition = OPEN_ALWAYS;
	}
    } else {
	/*
	 * OPEN_EXISTING = 3 - Opens a file or device, only if it exists.
	 *   If the specified file or device does not exist, the function
	 * fails and the last-error code is set to ERROR_FILE_NOT_FOUND (2).
	 */
	*CreationDisposition = OPEN_EXISTING;
    }

    /*
     * Who would have thought?
     * https://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
     * To open a directory using CreateFile, specify the FILE_FLAG_BACKUP_SEMANTICS
     * flag as part of dwFlagsAndAttributes. You must set this flag to obtain a handle
     * to a directory. A directory handle can be passed to some functions instead of a
     * file handle. Otherwise, ERROR_ACCESS_DENIED (5) is returned!
     */
    if ( os_isdir(file) == True ) {
	*FlagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;
    }
    if (posix_flags & (O_SYNC|O_DSYNC)) {
	*FlagsAndAttributes |= FILE_FLAG_WRITE_THROUGH;
    }
    if (posix_flags & O_DIRECT) {
	*FlagsAndAttributes |= (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
    }
    if (posix_flags & O_RDONLY) {
	*FlagsAndAttributes |= FILE_ATTRIBUTE_READONLY;
    }
    if (posix_flags & O_RANDOM) {
	*FlagsAndAttributes |= FILE_FLAG_RANDOM_ACCESS;
    } else if (posix_flags & O_SEQUENTIAL) {
	*FlagsAndAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;
    }
    if (posix_flags & O_ASYNC) {
	*FlagsAndAttributes |= FILE_FLAG_OVERLAPPED;
    }
    /* Note: cruisio/sio also add FILE_FLAG_BACKUP_SEMANTICS. */
    /*       The best I can tell, this bypasses certain security! */
    if (*FlagsAndAttributes == 0) {
	*FlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
    }
    return;
}

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
    DWORD DesiredAccess;
    DWORD CreationDisposition;
    DWORD FlagsAndAttributes;
    DWORD ShareMode;
    HANDLE handle = NoFd;
    int rc = SUCCESS;

    if (isDiskFull) *isDiskFull = False;
    if (isDirectory) *isDirectory = False;

    map_posix_flags(dip, file, flags, &DesiredAccess, &CreationDisposition, &FlagsAndAttributes, &ShareMode);

    if (dip->di_debug_flag) {
	Printf(dip, "Attempting to open file %s with POSIX open flags %#x...\n", file, flags);
	if (dip->di_extended_errors == True) {
	    ReportOpenInformation(dip, file, OS_OPEN_FILE_OP, DesiredAccess,
				  CreationDisposition, FlagsAndAttributes, ShareMode, False);
	}
    }
    if (retrys == True) dip->di_retry_count = 0;
    do {
	ENABLE_NOPROG(dip, OPEN_OP);
	handle = CreateFile(file, DesiredAccess, ShareMode,
			    NULL, CreationDisposition, FlagsAndAttributes, NULL);
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
	    ReportOpenInformation(dip, file, OS_OPEN_FILE_OP, DesiredAccess,
				  CreationDisposition, FlagsAndAttributes, ShareMode, True);
	}
    } else if ( (dip->di_debug_flag == True) && (handle != NoFd) ) {
	Printf(dip, "File %s successfully opened, handle = %d\n", file, handle);
    }
    return( handle );
}

HANDLE
os_open_file(char *name, int oflags, int perm)
{
    HANDLE Handle;
    DWORD DesiredAccess;
    DWORD CreationDisposition;
    DWORD FlagsAndAttributes;
    DWORD ShareMode;

    map_posix_flags(NULL, name, oflags, &DesiredAccess, &CreationDisposition, &FlagsAndAttributes, &ShareMode);

    /*
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
     *
     * HANDLE WINAPI CreateFile(
     *  _In_      LPCTSTR lpFileName,
     *  _In_      DWORD dwDesiredAccess,
     *  _In_      DWORD dwShareMode,
     *  _In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
     *  _In_      DWORD dwCreationDisposition,
     *  _In_      DWORD dwFlagsAndAttributes,
     *  _In_opt_  HANDLE hTemplateFile
     *);
     */
    Handle = CreateFile(name, DesiredAccess, ShareMode,
			NULL, CreationDisposition, FlagsAndAttributes, NULL);
    return( Handle );
}

__inline ssize_t
os_read_file(HANDLE handle, void *buffer, size_t size)
{
    DWORD bytesRead;

    /*
     * BOOL WINAPI ReadFile(
     *	_In_         HANDLE hFile,
     *	_Out_        LPVOID lpBuffer,
     *	_In_         DWORD nNumberOfBytesToRead,
     * 	_Out_opt_    LPDWORD lpNumberOfBytesRead,
     * 	_Inout_opt_  LPOVERLAPPED lpOverlapped
     * );
     */
    if (ReadFile(handle, buffer, (DWORD)size, (LPDWORD)&bytesRead, NULL) == False) {
       bytesRead = -1;
    }
    return( (ssize_t)(int)bytesRead );
}

__inline ssize_t
os_write_file(HANDLE handle, void *buffer, size_t size)
{
    DWORD bytesWritten;

    /*
     * BOOL WINAPI WriteFile(
     *	_In_         HANDLE hFile,
     *	_In_         LPCVOID lpBuffer,
     *	_In_         DWORD nNumberOfBytesToWrite,
     *	_Out_opt_    LPDWORD lpNumberOfBytesWritten,
     *	_Inout_opt_  LPOVERLAPPED lpOverlapped
     * );
     */
    if (WriteFile(handle, buffer, (DWORD)size, (LPDWORD)&bytesWritten, NULL) == False) {
       bytesWritten = -1;
    }
    return( (ssize_t)(int)bytesWritten );
}

/*    Unix whence values: SEEK_SET(0), SEEK_CUR(1), SEEK_END(2) */
static int seek_map[] = { FILE_BEGIN, FILE_CURRENT, FILE_END };

/*
 * Note that this is a 64-bit seek, Offset_t better be 64 bits
 */
Offset_t
os_seek_file(HANDLE handle, Offset_t offset, int whence)
{
    LARGE_INTEGER liDistanceToMove;
    LARGE_INTEGER lpNewFilePointer;
	
    /*
     * BOOL WINAPI SetFilePointerEx(
     *  _In_       HANDLE hFile,
     *  _In_       LARGE_INTEGER liDistanceToMove,
     *  _Out_opt_  PLARGE_INTEGER lpNewFilePointer,
     *  _In_       DWORD dwMoveMethod
     * );
     */
    liDistanceToMove.QuadPart = offset;
    if ( SetFilePointerEx(handle, liDistanceToMove, &lpNewFilePointer, seek_map[whence]) == False) {
	lpNewFilePointer.QuadPart = -1;
    }
    return( (Offset_t)(lpNewFilePointer.QuadPart) );
}

ssize_t
os_pread_file(HANDLE handle, void *buffer, size_t size, Offset_t offset)
{
    DWORD bytesRead;
    OVERLAPPED overlap;
    BOOL result;
    LARGE_INTEGER li;

    li.QuadPart = offset;
    overlap.Offset = li.LowPart;
    overlap.OffsetHigh = li.HighPart;
    overlap.hEvent = NULL;
    /*
     * BOOL WINAPI ReadFile(
     *	_In_         HANDLE hFile,
     *	_Out_        LPVOID lpBuffer,
     *	_In_         DWORD nNumberOfBytesToRead,
     * 	_Out_opt_    LPDWORD lpNumberOfBytesRead,
     * 	_Inout_opt_  LPOVERLAPPED lpOverlapped
     * );
     */
    result = ReadFile(handle, buffer, (DWORD)size, (LPDWORD)&bytesRead, &overlap);
    if (result == False) {
        DWORD error = GetLastError();
	if (error == ERROR_IO_PENDING) {
            /* WTF? When setting wait parameter True, the wrong bytes read value is returned on disks! */
	    while ( (result = GetOverlappedResult(handle, &overlap, (LPDWORD)&bytesRead, False)) == False) {
        	error = GetLastError();
		if (error == ERROR_IO_INCOMPLETE) {
		    Sleep(10);
		} else {
        	    break;
		}
	    }
	}
	if (result == False) {
	    bytesRead = FAILURE;
	}
    }
    return( (ssize_t)(int)bytesRead );
}

ssize_t
os_pwrite_file(HANDLE handle, void *buffer, size_t size, Offset_t offset)
{
    DWORD bytesWrite;
    OVERLAPPED overlap;
    BOOL result;
    LARGE_INTEGER li;

    li.QuadPart = offset;
    overlap.Offset = li.LowPart;
    overlap.OffsetHigh = li.HighPart;
    overlap.hEvent = NULL;
    /*
     * BOOL WINAPI WriteFile(
     *	_In_         HANDLE hFile,
     *	_In_         LPCVOID lpBuffer,
     *	_In_         DWORD nNumberOfBytesToWrite,
     *	_Out_opt_    LPDWORD lpNumberOfBytesWritten,
     *	_Inout_opt_  LPOVERLAPPED lpOverlapped
     * );
     */
    result = WriteFile(handle, buffer, (DWORD)size, (LPDWORD)&bytesWrite, &overlap);
    if (result == False) {
	bytesWrite = FAILURE;
    }
    return( (ssize_t)(int)bytesWrite );
}

os_error_t
win32_getuncpath(char *path, char **uncpathp)
{
    os_error_t error = NO_ERROR;

    if (IsDriveLetter(path) == True) {
	char uncpath[PATH_BUFFER_SIZE];
	char drive[3];
	DWORD uncpathsize = sizeof(uncpath);
	strncpy(drive, path, 2);	/* Copy the drive letter. */
	drive[2] = '\0';
	/*
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa385453(v=vs.85).aspx
	 * 
	 * DWORD WNetGetConnection(
	 *  _In_     LPCTSTR lpLocalName,
	 *  _Out_    LPTSTR lpRemoteName,
	 *  _Inout_  LPDWORD lpnLength
	 * );
	 */
	if ((error = WNetGetConnection(drive, uncpath, &uncpathsize)) == NO_ERROR) {
	    strcat(uncpath, &path[2]); /* Copy everything *after* drive letter. */
	    *uncpathp = strdup(uncpath);
	}
    }
    return(error);
}

HANDLE
win32_dup(HANDLE handle)
{
    HANDLE hDup = (HANDLE)-1;

    /* http://msdn.microsoft.com/en-us/library/ms724251(VS.85).aspx */

    if ( !DuplicateHandle(GetCurrentProcess(), 
			  handle, 
			  GetCurrentProcess(),
			  &hDup, 
			  0,
			  False,
			  DUPLICATE_SAME_ACCESS) ) {
	DWORD dwErr = GetLastError();
	errno = EINVAL;
    }
    return (hDup);
}

/* ============================================================================== */

hbool_t
IsDriveLetter(char *device)
{
    /* Check for drive letters "[a-zA-Z]:" */
    if ((strlen(device) >= 2) && (device[1] == ':') &&
        ((device[0] >= 'a') && (device[0] <= 'z') ||
         (device[0] >= 'A') && (device[0] <= 'Z'))) {
        return(True);
    }
    return(False);
}

char *
setup_scsi_device(dinfo_t *dip, char *path)
{
    char *scsi_device;

    scsi_device = Malloc(dip, (DEV_DIR_LEN * 2) );
    if (scsi_device == NULL) return(scsi_device);
    /* Format: \\.\[A-Z]: */
    strcpy(scsi_device, DEV_DIR_PREFIX);
    scsi_device[DEV_DIR_LEN] = path[0];		/* The drive letter. */
    scsi_device[DEV_DIR_LEN+1] = path[1];	/* The ':' terminator. */
    return(scsi_device);
}

hbool_t
FindMountDevice(dinfo_t *dip, char *path, hbool_t debug)
{
    hbool_t match = False;
    char *sdsf = NULL;

    if ( IsDriveLetter(path) ) {
	match = True;
	sdsf = setup_scsi_device(dip, path);
    } else if ( EQL(path, "\\\\", 2) || EQL(path, "//", 2) ) {
	;	/* Skip UNC paths! */
    } else {
	char path_dir[PATH_BUFFER_SIZE];
	char *path_dirp;
	memset(path_dir, '\0', sizeof(path_dir));
	path_dirp = getcwd(path_dir, sizeof(path_dir));
	if (path_dirp == NULL) return(match);
	if ( IsDriveLetter(path_dirp) ) {
	    match = True;
	    sdsf = setup_scsi_device(dip, path_dirp);
	}
    }
    if (match == True) {
	dip->di_mounted_from_device = strdup(sdsf);
	//dip->di_mounted_on_dir = strdup(mounted_path);
    }
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

char *
os_ctime(time_t *timep, char *time_buffer, int timebuf_size)
{
    int error;

    error = ctime_s(time_buffer, timebuf_size, timep);
    if (error) {
	(int)sprintf(time_buffer, "<no time available>");
    } else {
	char *bp;
	if (bp = strrchr(time_buffer, '\n')) {
	    *bp = '\0';
	}
    }
    return(time_buffer);
}

char *
os_gethostname(void)
{
    char hostname[MAXHOSTNAMELEN];
    DWORD len = MAXHOSTNAMELEN;

    if ( (GetComputerNameEx(ComputerNameDnsFullyQualified, hostname, &len)) == False) {
	  //os_perror(NULL, "GetComputerNameEx() failed");
	  return(NULL);
    }
    return ( strdup(hostname) );
}

#undef UNICODE

//#include <winsock2.h>
//#include <ws2tcpip.h>

// link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

#define IPv4_ADDRSIZE	4
#define IPv4_STRSIZE	12
#define IPv6_ADDRSIZE	16
#define IPv6_STRSIZE	46

/* Reference URL: http://msdn.microsoft.com/en-us/library/windows/desktop/ms738520(v=vs.85).aspx */

char *
os_getaddrinfo(dinfo_t *dip, char *host, int family, void **sa, socklen_t *salen)
{
    WSADATA wsaData;
    struct addrinfo *addrinfop = NULL;
    struct addrinfo *aip = NULL;
    struct addrinfo hints;
    char address_str[IPv6_STRSIZE];
    char *ipv4 = NULL, *ipv6 = NULL;
    int i = 1, status;

    /* Initialize Winsock */
    status = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (status != 0) {
        return(NULL);
    }

    ZeroMemory( &hints, sizeof(hints) );
    ZeroMemory( address_str, sizeof(address_str) );
    if (sa && *sa) {
	free(*sa);
	*sa = NULL;
    }
    /* 
     * A value of AF_UNSPEC for ai_family indicates the caller will
     * accept only the AF_INET and AF_INET6 address families.
     * Note: May need a flag to control which family to query!
     */
    if (family) {
	hints.ai_family = family;
    } else {
	hints.ai_family = AF_UNSPEC;	/* IPv4 and IPv6 allowed. */
    }
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    /*
     * int WSAAPI getaddrinfo(
     *  _In_opt_  PCSTR pNodeName,
     *  _In_opt_  PCSTR pServiceName,
     *  _In_opt_  const ADDRINFOA *pHints,
     *  _Out_     PADDRINFOA *ppResult
     * );
     */
    status = getaddrinfo(host, NULL, &hints, &addrinfop);
    if (status != SUCCESS) {
        WSACleanup();
        return(NULL);
    }

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
	    char *p;
            p = inet_ntoa(sainp->sin_addr);
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
	    LPSOCKADDR sockaddr_ip;
	    DWORD ipbufferlength = IPv6_STRSIZE;
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

	    sockaddr_ip = (LPSOCKADDR)aip->ai_addr;
	    /*
	     * INT WSAAPI WSAAddressToString(
	     *  _In_      LPSOCKADDR lpsaAddress,
	     *  _In_      DWORD dwAddressLength,
	     *  _In_opt_  LPWSAPROTOCOL_INFO lpProtocolInfo,
	     *  _Inout_   LPTSTR lpszAddressString,
	     *  _Inout_   LPDWORD lpdwAddressStringLength
	     * );
	     */
	    status = WSAAddressToString(sockaddr_ip, (DWORD)aip->ai_addrlen, NULL, 
					address_str, &ipbufferlength);
	    if (status) {
		;
	    } else {
		/*
		 * There can be multiple IP addresses returned, and "::1" is for the IPv6 local host.
		 * The ::1 address (or, rather, in any address, that has a double colon in it) double	
		 * colon expands into the number of zero-bits, neccessary to pad the address to full
		 * length, so the expanded version looks like 0000:0000:0000:0000:0000:0000:0000:0001.
		 */
		if ( EQ(address_str, "::1") ) {
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
	/*
	 * My Windows client returns multiple IP addresses:
	 * 
	 * host IP: fe80::fc0b:aa83:2e17:4db3%15
	 *  host IP: fe80::40fe:ea75:dbdb:e7d6%13
	 *  host IP: fe80::a5ba:f5bb:cfbb:7a07%10
	 *  host IP: fd20:8b1e:b255:83fa:40fe:ea75:dbdb:e7d6
	 *  host IP: fd20:8b1e:b255:840b:0:3:ae5:888a
	 *  host IP: fd20:8b1e:b255:840b:a5ba:f5bb:cfbb:7a07
	 *  host IP: 4.9.30.102
	 *  host IP: 4.9.14.102
	 *  host IP: 10.229.136.138
	 * 
	 * How to choose? We need to filter undesirables out!
	 * 
	 * Here's what these IP's are:
	 *
	 * H:\Windows>ipconfig
	 *
	 * Windows IP Configuration
	 *
	 * Ethernet adapter Local Area Connection 4:
	 *
	 *   Connection-specific DNS Suffix  . :
	 *   IPv6 Address. . . . . . . . . . . : fd20:8b1e:b255:83fb:fc0b:aa83:2e17:4db3
	 *   Link-local IPv6 Address . . . . . : fe80::fc0b:aa83:2e17:4db3%15
	 *   IPv4 Address. . . . . . . . . . . : 4.9.30.102
	 *   Subnet Mask . . . . . . . . . . . : 255.255.240.0
	 *   Default Gateway . . . . . . . . . :
	 *
	 * Ethernet adapter Local Area Connection 3:
	 *
	 *   Connection-specific DNS Suffix  . :
	 *   IPv6 Address. . . . . . . . . . . : fd20:8b1e:b255:83fa:40fe:ea75:dbdb:e7d6
	 *   Link-local IPv6 Address . . . . . : fe80::40fe:ea75:dbdb:e7d6%13
	 *   IPv4 Address. . . . . . . . . . . : 4.9.14.102
	 *   Subnet Mask . . . . . . . . . . . : 255.255.240.0
	 *   Default Gateway . . . . . . . . . : fe80::226:98ff:fe05:5bc1%13
	 *
	 * Ethernet adapter Local Area Connection:
	 *
	 *   Connection-specific DNS Suffix  . :
	 *   IPv6 Address. . . . . . . . . . . : fd20:8b1e:b255:840b:0:3:ae5:888a
	 *   IPv6 Address. . . . . . . . . . . : fd20:8b1e:b255:840b:a5ba:f5bb:cfbb:7a07
	 *   Link-local IPv6 Address . . . . . : fe80::a5ba:f5bb:cfbb:7a07%10
	 *   IPv4 Address. . . . . . . . . . . : 10.229.136.138
	 *   Subnet Mask . . . . . . . . . . . : 255.255.248.0
	 *   Default Gateway . . . . . . . . . : fd20:8b1e:b255:840b::3
	 *				         fe80::5:73ff:fea0:3e%10
	 *				         10.229.136.1
	 *
	 * Tunnel adapter Local Area Connection* 8:
	 *
	 *   Media State . . . . . . . . . . . : Media disconnected
	 *   Connection-specific DNS Suffix  . :
	 *
	 * Tunnel adapter Local Area Connection* 9:
	 *
	 *   Media State . . . . . . . . . . . : Media disconnected
	 *   Connection-specific DNS Suffix  . :
	 *
	 * Tunnel adapter Local Area Connection* 11:
	 *
	 *   Media State . . . . . . . . . . . : Media disconnected
	 *   Connection-specific DNS Suffix  . :
	 *
	 * H:\Windows>
	 *
	 */
	/* continue to the next entry... */
    }
    freeaddrinfo(addrinfop);
    WSACleanup();
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
    WSADATA wsaData;
    char host[NI_MAXHOST], server[NI_MAXSERV];
    int status;

    /* Initialize Winsock */
    status = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (status != 0) {
       return(NULL);
    }
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
    WSACleanup();
    if (status == FAILURE) {
	return(NULL); 
    } else {
	return( (strlen(host)) ? strdup(host) : NULL );
    }
}

char *
os_getosinfo(void)
{
    OSVERSIONINFOEX osv;
    char osinfo[STRING_BUFFER_SIZE];
    char osversion[STRING_BUFFER_SIZE];

    memset(&osv, 0, sizeof osv);
    osv.dwOSVersionInfoSize = sizeof osv;

    osversion[0] = '\0';

    /* 
     * See this link for deprecated GetVersionEx() API: 
     *   warning C4996: 'GetVersionExA': was declared deprecated
     *   https://docs.microsoft.com/en-us/windows/desktop/w8cookbook/operating-system-version-changes-in-windows-8-1
     *      OR
     * Switch to this API for Windows 10:
     *   https://docs.microsoft.com/en-us/windows/desktop/DevNotes/rtlgetversion
     * Note: Required Ntddk.h include file and additional libraries.
     */

    /* URL: http://msdn.microsoft.com/en-us/library/windows/desktop/ms724451(v=vs.85).aspx */

    if (GetVersionEx((OSVERSIONINFO *)&osv) == FALSE) {
	//os_perror(NULL, "GetVersionEx() failed");
	return(NULL);
    }

    /* OS Mappings: http://msdn.microsoft.com/en-us/library/windows/desktop/ms724833(v=vs.85).aspx */

    if (osv.dwMajorVersion == 4) {
	if (osv.dwMinorVersion == 0) {
	    strcpy(osversion, "Windows NT 4");
	}
	if (osv.dwMinorVersion == 10) {
	    strcpy(osversion, "Windows 98");
	}
	if (osv.dwMinorVersion == 90) {
	    strcpy(osversion, "Windows Me");
	}
    } else if (osv.dwMajorVersion == 5) {
	if (osv.dwMinorVersion == 0) {
	    strcpy(osversion, "Windows 2000");
	}
	if (osv.dwMinorVersion == 1) {
	    strcpy(osversion, "Windows XP");
	}
	if (osv.dwMinorVersion == 2) {
	    if (GetSystemMetrics(89) /* SM_SERVERR2 */) {
		strcpy(osversion, "Windows Server 2003 R2");
	    } else if (osv.wProductType == VER_NT_WORKSTATION) {
		strcpy(osversion, "Windows XP x64");
	    } else {
		strcpy(osversion, "Windows Server 2003");
	    }
	}
    } else if (osv.dwMajorVersion == 6) {
	if (osv.dwMinorVersion == 0) {
	    if (osv.wProductType == VER_NT_WORKSTATION) {
		strcpy(osversion, "Windows Vista");
	    } else {
		strcpy(osversion, "Windows Server 2008");
	    }
	} else if (osv.dwMinorVersion == 1) {
	    if (osv.wProductType == VER_NT_WORKSTATION) {
		strcpy(osversion, "Windows 7");
	    } else {
		strcpy(osversion, "Windows Server 2008 R2");
	    }
	} else if (osv.dwMinorVersion == 2) {
	    if (osv.wProductType == VER_NT_WORKSTATION) {
		strcpy(osversion, "Windows 8");
	    } else {
		strcpy(osversion, "Windows Server 2012");
	    }
	} else if (osv.dwMinorVersion == 3) {
	    if (osv.wProductType == VER_NT_WORKSTATION) {
		strcpy(osversion, "Windows 8.1");
	    } else {
		strcpy(osversion, "Windows Server 2012 R2");
	    }
	}
    } else if (osv.dwMajorVersion == 10) {
	strcpy(osversion, "Windows 10");
    }

    if (osversion[0] == '\0') {
	strcpy(osversion, "Unknown Windows Version");
    }

    sprintf(osinfo, "%s [%d.%d.%d %s]", osversion,
	    osv.dwMajorVersion, osv.dwMinorVersion, osv.dwBuildNumber,
	    strlen(osv.szCSDVersion) ? osv.szCSDVersion : "No Service Pack");

    return( strdup(osinfo) );
}

char *
os_getusername(void)
{
    DWORD size = STRING_BUFFER_SIZE;
    TCHAR username[STRING_BUFFER_SIZE];

    if (GetUserName(username, &size) == False) {
	//os_perror(NULL, "GetUserName() failed");
	return(NULL);
    }
    return ( strdup(username) );
}

int
getpagesize(void)
{
    SYSTEM_INFO sysinfo;

    GetSystemInfo(&sysinfo);
    return ( sysinfo.dwPageSize );
}

int
setenv(const char *name, const char *value, int overwrite)
{
    return ( _putenv_s(name, value) );
}

/*
 * Windows equivalent...
 *	we need both since POSIX and Windows API's are both used! (yet)
 */
/*VARARGS*/
void
os_perror(dinfo_t *dip, char *format, ...)
{
    char msgbuf[LOG_BUFSIZE];
    DWORD error = GetLastError();
    LPVOID emsg = os_get_error_msg(error);
    va_list ap;
    FILE *fp;

    if (dip == NULL) dip = master_dinfo;
    fp = (dip) ? dip->di_efp : efp;
    va_start(ap, format);
    (void)vsprintf(msgbuf, format, ap);
    va_end(ap);
    LogMsg(dip, fp, logLevelError, 0, "%s, error = %d - %s\n", msgbuf, error, emsg);
    os_free_error_msg(emsg);
    return;
}

void
tPerror(dinfo_t *dip, int error, char *format, ...)
{
    char msgbuf[LOG_BUFSIZE];
    LPVOID emsg = os_get_error_msg(error);
    va_list ap;
    FILE *fp;

    if (dip == NULL) dip = master_dinfo;
    fp = (dip) ? dip->di_efp : efp;
    va_start(ap, format);
    (void)vsprintf(msgbuf, format, ap);
    va_end(ap);
    LogMsg(dip, fp, logLevelError, 0, "%s, error = %d - %s\n", msgbuf, error, emsg);
    os_free_error_msg(emsg);
    return;
}

/*
 * os_get_error_msg() - Get OS (Windows) Error Message.
 *
 * Inputs:
 *	error = The error code, from GetLastError()
 *
 * Return Value:
 *	A pointer to a dynamically allocated message.
 *	The buffer should be freed via LocalFree().
 */
char *
os_get_error_msg(int error)
{
    char *msgbuf;

    if ( FormatMessage (
		       FORMAT_MESSAGE_ALLOCATE_BUFFER |
		       FORMAT_MESSAGE_FROM_SYSTEM,
		       NULL,
		       error,
		       MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
		       (LPSTR) &msgbuf,
		       0, NULL) == 0) {
	Fprintf(NULL, "FormatMessage() failed with %d\n", GetLastError());
	return(NULL);
    } else {
	char *bp = strrchr(msgbuf, '\r'); /* Terminated with \r\n */
	if (bp) *bp = '\0';		  /* Just the message please! */
	return(msgbuf);
    }
}

Offset_t
SetFilePtr(HANDLE hf, Offset_t distance, DWORD MoveMethod)
{
    LARGE_INTEGER seek;

    /*
     * DWORD WINAPI SetFilePointer(
     *  _In_         HANDLE hFile,
     *  _In_         LONG lDistanceToMove,
     *  _Inout_opt_  PLONG lpDistanceToMoveHigh,
     *  _In_         DWORD dwMoveMethod
     * );
     */
    seek.QuadPart = distance;
    seek.LowPart = SetFilePointer(hf, seek.LowPart, &seek.HighPart, MoveMethod);
    if ( (seek.LowPart == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR) ) {
	seek.QuadPart = -1;
    }
    return( (Offset_t)seek.QuadPart );
}

/*
 * syslog() - Windows API to Emulate Unix Syslog API.
 *
 * Inputs:
 *	priority = The message priority.
 *	format = Format control string.
 *	(optional arguments ...)
 *
 * Return Value:
 *	void
 */
void
syslog(int priority, char *format, ...)
{
    LPCSTR sourceName = "System";	// The event source name.
    DWORD dwEventID = 999;              // The event identifier.
    WORD cInserts = 1;                  // The count of insert strings.
    HANDLE h; 
    char msgbuf[LOG_BUFSIZE];
    LPCSTR bp = msgbuf;
    va_list ap;

    va_start(ap, format);
    (void)vsprintf(msgbuf, format, ap);
    va_end(ap);

    /*
     * Get a handle to the event log.
     */
    h = RegisterEventSource(NULL,        // Use local computer. 
                            sourceName); // Event source name. 
    if (h == NULL) {
	if (debug_flag) {
	    Fprintf(NULL, "RegisterEventSource() failed, error %d\n", GetLastError());
	}
        return;
    }

    /*
     * Report the event.
     */
    if (!ReportEvent(h,           // Event log handle. 
            priority,             // Event type. 
            0,                    // Event category.  
            dwEventID,            // Event identifier. 
            (PSID) 0,             // No user security identifier. 
            cInserts,             // Number of substitution strings. 
            0,                    // No data. 
            &bp,                  // Pointer to strings. 
            NULL))                // No data. 
    {
	if (debug_flag) {
	    Fprintf(NULL, "ReportEvent() failed, error %d\n", GetLastError());
	}
    }
    DeregisterEventSource(h); 
    return;
}

/* Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/dn553408(v=vs.85).aspx */

int
highresolutiontime(struct timeval *tv, struct timezone *tz)
{
    LARGE_INTEGER CounterTime, Frequency;
    double counter;

    UNREFERENCED_PARAMETER(tz);

    if (tv == NULL) {
	errno = EINVAL;
	return(FAILURE);
    }

    QueryPerformanceFrequency(&Frequency);	/* Ticks per second. */
    QueryPerformanceCounter(&CounterTime);

    /* Convert to double so we don't lose the remainder for usecs! */
    counter = (double)CounterTime.QuadPart / (double)Frequency.QuadPart;
    tv->tv_sec = (int)counter;
    counter -= tv->tv_sec;
    tv->tv_usec = (int)(counter * uSECS_PER_SEC);

    return 0;
}

/*
 * Taken from URL: 
 *  http://social.msdn.microsoft.com/Forums/vstudio/en-US/430449b3-f6dd-4e18-84de-eebd26a8d668/gettimeofday
 */
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
# define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
# define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
 
int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    FILETIME ft;
    unsigned __int64 tmpres = 0;
    static int tzflag;

    if (NULL != tv) {
	/* Precision is 10-15ms. */
	GetSystemTimeAsFileTime(&ft);
	tmpres |= ft.dwHighDateTime;
	tmpres <<= 32;
	tmpres |= ft.dwLowDateTime;

	/* Converting file time to UNIX epoch. */
	tmpres /= 10;  /* convert to microseconds from nanoseconds */
	tmpres -= DELTA_EPOCH_IN_MICROSECS; 
	tv->tv_sec = (long)(tmpres / 1000000UL);
	tv->tv_usec = (long)(tmpres % 1000000UL);
    }

    if (NULL != tz) {
	if (!tzflag) {
	    _tzset();
	    tzflag++;
	}
	tz->tz_minuteswest = _timezone / 60;
	tz->tz_dsttime = _daylight;
    }
    return 0;
}

/*
 * localtime_r() - Get Local Time.
 * 
 * The arguments are reversed and the return value is different.
 * 
 * Unix:
 *  struct tm *localtime_r(const time_t *timep, struct tm *result);
 *
 * Windows:
 *  errno_t localtime_s(struct tm* _tm,	const time_t *time);
 * );
 *
 * Return Value:
 * 	Returns tm on Success or NULL on Failure.
 */
struct tm *
localtime_r(const time_t *timep, struct tm *tm)
{
    if ( localtime_s(tm, timep) == SUCCESS ) {
	return(tm);
    } else {
	return(NULL);
    }
}

__inline clock_t
times(struct tms *buffer)
{
    return ( (clock_t)(time(0) * hertz) );
}

uint64_t
os_create_random_seed(void)
{
    LARGE_INTEGER PerformanceCount;
    /*
     * BOOL WINAPI QueryPerformanceCounter(
     *  _Out_  LARGE_INTEGER *lpPerformanceCount
     * );
     */
    if ( QueryPerformanceCounter(&PerformanceCount) ) {
	return( (uint64_t)(PerformanceCount.QuadPart) );
    } else {
	return(0);
    }
}

__inline int
os_create_directory(char *dir_path, int permissions)
{
    /* BOOL WINAPI CreateDirectory(
     *  _In_      LPCTSTR lpPathName,
     *  _In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes
     * );
     */
    return ( ( CreateDirectory(dir_path, (LPSECURITY_ATTRIBUTES) 0) ) ? SUCCESS : FAILURE );
}


__inline int
os_remove_directory(char *dir_path)
{
    /* BOOL WINAPI RemoveDirectory( _In_ LPCTSTR lpPathName );	*/
    return ( ( RemoveDirectory(dir_path) ) ? SUCCESS : FAILURE );
}

__inline int
os_close_file(HANDLE handle)
{
    /* BOOL WINAPI CloseHandle(	_In_ HANDLE hObject );	*/
    return ( ( CloseHandle(handle) ) ? SUCCESS : FAILURE );
}

__inline int
os_delete_file(char *file)
{
    /* BOOL WINAPI DeleteFile( _In_ LPCTSTR lpFileName	); */
    return ( ( DeleteFile(file) ) ? SUCCESS : FAILURE );
}

__inline int
os_flush_file(HANDLE *handle)
{
    /* BOOL WINAPI FlushFileBuffers( _In_ HANDLE hFile	); */
    return ( (FlushFileBuffers(handle) ) ? SUCCESS : FAILURE );
}

/* Note: This mimics the POSIX truncate() API. */
int
os_truncate_file(char *file, Offset_t offset)
{
    int status = SUCCESS;
    HANDLE handle;

    handle = os_open_file(file, O_RDWR, 0);
    if (handle == NoFd) return(FAILURE);
    if (os_seek_file(handle, offset, SEEK_SET) == (Offset_t) -1) {
	status = FAILURE;
    } else {
	/* BOOL WINAPI SetEndOfFile( _In_ HANDLE hFile	); */
	if ( SetEndOfFile(handle) == False ) {
	    status = FAILURE;
	}
    }
    (void)os_close_file(handle);
    return(status);
}

int
os_ftruncate_file(HANDLE handle, Offset_t offset)
{
    int status = SUCCESS;

    if (os_seek_file(handle, offset, SEEK_SET) == (Offset_t) -1) {
	status = FAILURE;
    } else {
	/* BOOL WINAPI SetEndOfFile( _In_ HANDLE hFile	); */
	if ( SetEndOfFile(handle) == False ) {
	    status = FAILURE;
	}
    }
    return(status);
}

int
os_file_information(char *file, large_t *filesize, hbool_t *is_dir, hbool_t *is_file)
{
    WIN32_FILE_ATTRIBUTE_DATA fad, *fadp = &fad;

    if (is_dir) *is_dir = False;
    if (is_file) *is_file = False;
    if (filesize) *filesize = 0;

    /*
     * See if the file exists, and what it's size is. 
     *  
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364946(v=vs.85).aspx
     * 
     * BOOL WINAPI GetFileAttributesEx(
     *  _In_   LPCTSTR lpFileName,
     *  _In_   GET_FILEEX_INFO_LEVELS fInfoLevelId,
     *  _Out_  LPVOID lpFileInformation
     *);
     */
    if ( GetFileAttributesEx(file, GetFileExInfoStandard, fadp) == True ) {
	/* Assuming a regular file (for now). */
	if (filesize) {
	    /* Setup the 64-bit file size please! */
	    *filesize = (large_t)(((large_t)fadp->nFileSizeHigh << 32L) + fadp->nFileSizeLow);
	}
	if ( is_dir && (fadp->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) {
	    *is_dir = True;
	} 
	/* Assume regular file for now! */
	if ( is_file && !(fadp->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) {
	    *is_file = True;
	}
	return(SUCCESS);
    } else {
	return(FAILURE);
    }
}

hbool_t
os_isdir(char *dirpath)
{
    hbool_t isdir;
    (void)os_file_information(dirpath, NULL, &isdir, NULL);
    return(isdir);
}

hbool_t
os_isdisk(HANDLE handle)
{
    /* 
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364960(v=vs.85).aspx 
     * 
     * DWORD WINAPI GetFileType( _In_  HANDLE hFile );
     */
    return( (GetFileType(handle) == FILE_TYPE_DISK) ? True : False ); 
}

/*
 * Please Note: This API does NOT work on disk device paths!
 * FWIW: GetFileType() works for disks, but need a handle! ;(
 */
hbool_t
os_file_exists(char *file)
{
    WIN32_FILE_ATTRIBUTE_DATA fad, *fadp = &fad;

    /*
     * See if the file exists, and what it's size is. 
     *  
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364946(v=vs.85).aspx
     * 
     * BOOL WINAPI GetFileAttributesEx(
     *  _In_   LPCTSTR lpFileName,
     *  _In_   GET_FILEEX_INFO_LEVELS fInfoLevelId,
     *  _Out_  LPVOID lpFileInformation
     *);
     */
    return ( GetFileAttributesEx(file, GetFileExInfoStandard, fadp) );
}

int
dt_get_file_attributes(dinfo_t *dip, char *file, DWORD *FileAttributes)
{
    int status = SUCCESS;
    int	rc = SUCCESS;
    
    dip->di_retry_count = 0;
    do {
	/*
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364944(v=vs.85).aspx
	 * 
	 * DWORD WINAPI GetFileAttributes( _In_ LPCTSTR lpFileName ); 
	 * 
	 * If the function fails, the return value is INVALID_FILE_ATTRIBUTES.
	 * 
	 * Returns: http://msdn.microsoft.com/en-us/library/windows/desktop/gg258117(v=vs.85).aspx
	 */
	*FileAttributes = GetFileAttributes(file);
	if (*FileAttributes == INVALID_FILE_ATTRIBUTES) {
	    char *op = "GetFileAttributes";
	    INIT_ERROR_INFO(eip, file, op, GETATTR_OP, NULL, 0, (Offset_t)0, (size_t)0,
			    os_get_error(), logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    rc = ReportRetryableError(dip, eip, "Failed %s on file %s", op, file);
	    status = FAILURE;
	} else {
	    status = SUCCESS;
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return(status);
}

char *
os_getcwd(void)
{
    char path[PATH_BUFFER_SIZE];

    /*
     * DWORD WINAPI GetCurrentDirectory(
     *  _In_   DWORD nBufferLength,
     *  _Out_  LPTSTR lpBuffer
     * );
     */
    if ( GetCurrentDirectory(sizeof(path), path) == 0 ) {
	return(NULL);
    } else {
	return ( strdup(path) );
    }
}

os_ino_t
os_get_fileID(char *path, HANDLE handle)
{
    BY_HANDLE_FILE_INFORMATION file_info;
    PBY_HANDLE_FILE_INFORMATION fip = &file_info;
    os_ino_t fileID = (os_ino_t)FAILURE;
    hbool_t my_open = False;
    BOOL rc;

    if (handle == INVALID_HANDLE_VALUE) {
	handle = os_open_file(path, O_RDONLY, 0);
	if (handle == NoFd) return(fileID);
	my_open = True;
    }
    /*
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364952(v=vs.85).aspx
     * 
     * BOOL WINAPI GetFileInformationByHandle(
     *  _In_   HANDLE hFile,
     *  _Out_  LPBY_HANDLE_FILE_INFORMATION lpFileInformation
     * );
     */
    rc = GetFileInformationByHandle(handle, fip);
    if (rc == True) {
	fileID = ( ((os_ino_t)fip->nFileIndexHigh << 32) | fip->nFileIndexLow);
    }
    if (my_open == True) {
	(void)os_close_file(handle);
    }
    return(fileID);
}

#if defined(WINDOWS_XP)

char *
os_get_protocol_version(HANDLE handle)
{
    return(NULL);
}

#else /* !defined(WINDOWS_XP) */

char *
os_get_protocol_version(HANDLE handle)
{
    FILE_INFO_BY_HANDLE_CLASS FileInformationClass = FileRemoteProtocolInfo;
    FILE_REMOTE_PROTOCOL_INFO remote_protocol_info;
    PFILE_REMOTE_PROTOCOL_INFO rpip = &remote_protocol_info;

    /*
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364953(v=vs.85).aspx
     *
     * BOOL WINAPI GetFileInformationByHandleEx(
     *  _In_   HANDLE hFile,
     *  _In_   FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
     *  _Out_  LPVOID lpFileInformation,
     *  _In_   DWORD dwBufferSize
     * ); 
     */
    if ( GetFileInformationByHandleEx(handle, FileInformationClass, rpip, sizeof(*rpip)) == True ) {
	if (rpip->Protocol == WNNC_NET_SMB) {
	    char protocol_version[SMALL_BUFFER_SIZE];
	    (void)sprintf(protocol_version, "SMB%u.%u", rpip->ProtocolMajorVersion, rpip->ProtocolMinorVersion);
	    return( strdup(protocol_version) );
	}
    }
    return(NULL);
}

#endif /* !deifned(WINDOWS_XP) */

large_t
os_get_file_size(char *path, HANDLE handle)
{
    large_t filesize = -1LL;

    if (handle == INVALID_HANDLE_VALUE) {
	/*
	 * BOOL WINAPI GetFileAttributesEx(
	 *  _In_   LPCTSTR lpFileName,
	 *  _In_   GET_FILEEX_INFO_LEVELS fInfoLevelId,
	 *  _Out_  LPVOID lpFileInformation
	 * );
	*/
	WIN32_FILE_ATTRIBUTE_DATA fad, *fadp = &fad;
	if ( GetFileAttributesEx(path, GetFileExInfoStandard, fadp) == True ) {
	    filesize = (large_t)(((large_t)fadp->nFileSizeHigh << 32L) + fadp->nFileSizeLow);
	    return (filesize);
	}
    } else {
	/*
	 * BOOL WINAPI GetFileSizeEx(
	 *  _In_   HANDLE hFile,
	 *  _Out_  PLARGE_INTEGER lpFileSize
	 * );
	 */
	LARGE_INTEGER FileSize;
	if ( GetFileSizeEx(handle, &FileSize) ) {
	    return ( (large_t)FileSize.QuadPart );
	}
    }
    return (filesize);
}

int
os_get_fs_information(dinfo_t *dip, char *dir)
{
    DWORD SectorsPerCluster;
    DWORD BytesPerSector;
    DWORD NumberOfFreeClusters;
    DWORD TotalNumberOfClusters;
    ULARGE_INTEGER FreeBytesAvailable;
    ULARGE_INTEGER TotalNumberOfBytes;
    ULARGE_INTEGER TotalNumberOfFreeBytes;
    int status = SUCCESS;

    if (dip->di_volume_path_name == NULL) {
	/* 
	 * Get the specified or current directory information.
	 */
	if (dir) {
	    dip->di_volume_path_name = dt_get_volume_path_name(dip, dir);
	} else {
	    char *cdir;
	    if ( (cdir = os_getcwd()) == NULL) return(FAILURE);
	    dip->di_volume_path_name = dt_get_volume_path_name(dip, cdir);
	    free(cdir);
	}
    }
    dip->di_universal_name = os_get_universal_name(dip->di_volume_path_name);

    /* 
     * BOOL WINAPI GetDiskFreeSpace(
     *  _In_   LPCTSTR lpRootPathName,
     *  _Out_  LPDWORD lpSectorsPerCluster,
     *  _Out_  LPDWORD lpBytesPerSector,
     *  _Out_  LPDWORD lpNumberOfFreeClusters,
     *  _Out_  LPDWORD lpTotalNumberOfClusters
     * );
     */
    if (GetDiskFreeSpace(dip->di_volume_path_name,
			 &SectorsPerCluster,
			 &BytesPerSector,
			 &NumberOfFreeClusters,
			 &TotalNumberOfClusters) == True) {
	dip->di_fs_block_size = (uint32_t)(SectorsPerCluster * BytesPerSector);
    }

    /*
     * BOOL WINAPI GetDiskFreeSpaceEx(
     *   _In_opt_   LPCTSTR lpDirectoryName,
     *   _Out_opt_  PULARGE_INTEGER lpFreeBytesAvailable,
     *   _Out_opt_  PULARGE_INTEGER lpTotalNumberOfBytes,
     *   _Out_opt_  PULARGE_INTEGER lpTotalNumberOfFreeBytes
     * );
     */
    if (GetDiskFreeSpaceEx(dip->di_volume_path_name,
			   &FreeBytesAvailable,
			   &TotalNumberOfBytes,
			   &TotalNumberOfFreeBytes) == True) {
	dip->di_fs_space_free = (large_t)(FreeBytesAvailable.QuadPart);
	dip->di_fs_total_space = (large_t)(TotalNumberOfBytes.QuadPart);
    } else {
	status = FAILURE;
    }
    return(status);
}

#pragma comment(lib, "mpr.lib")

char *
os_get_universal_name(char *drive_letter)
{
    DWORD cbBuff = PATH_BUFFER_SIZE;
    TCHAR szBuff[PATH_BUFFER_SIZE];
    REMOTE_NAME_INFO *prni = (REMOTE_NAME_INFO *)&szBuff;
    UNIVERSAL_NAME_INFO *puni = (UNIVERSAL_NAME_INFO *)&szBuff;
    DWORD result;

    /*
     * DWORD WNetGetUniversalName(
     *  _In_     LPCTSTR lpLocalPath,
     *  _In_     DWORD dwInfoLevel,
     *  _Out_    LPVOID lpBuffer,
     *  _Inout_  LPDWORD lpBufferSize
     * );
     */
    result = WNetGetUniversalName((LPTSTR)drive_letter,
				  UNIVERSAL_NAME_INFO_LEVEL,
				  (LPVOID)&szBuff, &cbBuff);
    if (result == NO_ERROR) {
	return( strdup(puni->lpUniversalName) );
    } else {
	return(NULL);
    }
//    _tprintf(TEXT("Universal Name: \t%s\n\n"), puni->lpUniversalName); 
#if 0
    if ( (res = WNetGetUniversalName((LPTSTR)drive_letter, 
				     REMOTE_NAME_INFO_LEVEL, 
				     (LPVOID)&szBuff,
				     &cbBuff)) != NO_ERROR) {
	return(NULL);
    }
//    _tprintf(TEXT("Universal Name: \t%s\nConnection Name:\t%s\nRemaining Path: \t%s\n"),
//          prni->lpUniversalName, 
//          prni->lpConnectionName, 
//          prni->lpRemainingPath);
    return(NULL);
#endif
}

char *
dt_get_volume_path_name(dinfo_t *dip, char *path)
{
    char	*RootPathName;
    os_error_t	error = ERROR_SUCCESS;
    int		rc = SUCCESS;

    dip->di_retry_count = 0;
    do {
	RootPathName = os_get_volume_path_name(path);
	if ( (RootPathName == NULL) && ((error = os_get_error()) != ERROR_SUCCESS) ) {
	    INIT_ERROR_INFO(eip, path, OS_GET_VOLUME_PATH_OP, VPATH_OP, NULL, 0, (Offset_t)0, (size_t)0,
			    os_get_error(), logLevelError, PRT_SYSLOG, RPT_NODEVINFO);
	    rc = ReportRetryableError(dip, eip, "Failed to get volume path for %s", path);
	}
    } while ( (error != ERROR_SUCCESS) && (rc == RETRYABLE) );

    return(RootPathName);
}
    
char *
os_get_volume_path_name(char *path)
{
    BOOL	bStatus;
    CHAR	VolumePathName[PATH_BUFFER_SIZE];

    /*
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364996(v=vs.85).aspx
     * 
     * BOOL WINAPI GetVolumePathName(
     *  _In_   LPCTSTR lpszFileName,
     *  _Out_  LPTSTR lpszVolumePathName,
     *  _In_   DWORD cchBufferLength );
     */
    bStatus = GetVolumePathName(path, VolumePathName, sizeof(VolumePathName));
    if (bStatus == True) {
	return ( strdup(VolumePathName) );
    } else {
	return (NULL);
    }
}

int
os_get_volume_information(dinfo_t *dip)
{
    BOOL	bStatus;
    DWORD	FileSystemFlags;
    CHAR	FileSystemName[MAX_PATH + 1];
    PCHAR	RootPathName;
    CHAR	VolumeName[MAX_PATH + 1];
    DWORD	VolumeSerialNumber;
    int		rc = SUCCESS;

    if ( (RootPathName = dip->di_volume_path_name) == NULL) {
	RootPathName = dt_get_volume_path_name(dip, dip->di_dname);
	if (RootPathName) {
	    dip->di_volume_path_name = RootPathName;
	} else {
	    if (strrchr(dip->di_dname, dip->di_dir_sep) == NULL) {
		RootPathName = NULL; /* Use the default directory. */
	    } else {
		char *sep;
		RootPathName = strdup(dip->di_dname);
		sep = strrchr(RootPathName, dip->di_dir_sep);
		sep++; *sep = '\0';
		/* Note: I'm not sure this is right! */
		dip->di_volume_path_name = RootPathName;
	    }
	}
    }

    /*
     * BOOL WINAPI GetVolumeInformation(
     *  _In_opt_   LPCTSTR lpRootPathName,
     *  _Out_opt_  LPTSTR lpVolumeNameBuffer,
     *  _In_       DWORD nVolumeNameSize,
     *  _Out_opt_  LPDWORD lpVolumeSerialNumber,
     *  _Out_opt_  LPDWORD lpMaximumComponentLength,
     *  _Out_opt_  LPDWORD lpFileSystemFlags,
     *  _Out_opt_  LPTSTR lpFileSystemNameBuffer,
     *  _In_       DWORD nFileSystemNameSize
     * ); 
     *  
     * lpRootPathName [in, optional]
     * A pointer to a string that contains the root directory of the volume to be described.
     * If this parameter is NULL, the root of the current directory is used. 
     * A trailing backslash is required. For example, you specify 
     *   \\MyServer\MyShare as "\\MyServer\MyShare\", or the C drive as "C:\". 
     */
    dip->di_retry_count = 0;
    do {
	bStatus = GetVolumeInformation(RootPathName,
				       VolumeName, sizeof(VolumeName),
				       &VolumeSerialNumber, 0,
				       &FileSystemFlags,
				       FileSystemName, sizeof(FileSystemName));
	if (bStatus == False) {
	    INIT_ERROR_INFO(eip, RootPathName, OS_GET_VOLUME_INFO_OP, VINFO_OP, NULL, 0, (Offset_t)0, (size_t)0,
			    os_get_error(), logLevelError, PRT_SYSLOG, RPT_NODEVINFO);
	    if (RootPathName) {
		rc = ReportRetryableError(dip, eip, "Failed to get volume information for volume %s", RootPathName);
	    } else {
		eip->ei_file = dip->di_dname;
		rc = ReportRetryableError(dip, eip, "Failed to get volume information for file %s", dip->di_dname);
	    }
	}
    } while ( (bStatus == False) && (rc == RETRYABLE) );

    if (bStatus == False) return(FAILURE);

    dip->di_filesystem_type = strdup(FileSystemName);
    dip->di_file_system_flags = FileSystemFlags;
    dip->di_volume_name = strdup(VolumeName);
    dip->di_volume_serial_number = VolumeSerialNumber;
    return(SUCCESS);
}

int
os_set_priority(dinfo_t *dip, HANDLE hThread, int priority)
{
    int status = SUCCESS;

    /*
     * BOOL WINAPI SetThreadPriority(_In_ HANDLE hThread, _In_ int nPriority);
     */ 
    if ( SetThreadPriority(hThread, priority) == False ) {
	status = FAILURE;
    }
    return(status);
}

char *CreationDispositionTable[] = {
    "NONE",		/* 0 */
    "CREATE_NEW",	/* 1 */
    "CREATE_ALWAYS",	/* 2 */
    "OPEN_EXISTING",	/* 3 */
    "OPEN_ALWAYS",	/* 4 */
    "TRUNCATE_EXISTING"	/* 5 */
};

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
	//eip->ei_prt_flags = PRT_SYSLOG;
    } else {
	eip->ei_rpt_flags |= (RPT_NOERRORMSG|RPT_NOERRORNUM);
    }
    (void)ReportExtendedErrorInfo(dip, eip, NULL);
    PrintHex(dip, "Desired Access", DesiredAccess, DNL);
    bp = buffer; *bp = '\0';
    if (DesiredAccess == 0) {
	bp += sprintf(bp, "none specified");
    } else {
	if (DesiredAccess & FILE_READ_DATA) {
	    bp += sprintf(bp, "FILE_READ_DATA|");
	}
	if (DesiredAccess & FILE_WRITE_DATA) {
	    bp += sprintf(bp, "FILE_WRITE_DATA|");
	}
	if (DesiredAccess & FILE_APPEND_DATA) {
	    bp += sprintf(bp, "FILE_APPEND_DATA|");
	}
	if (DesiredAccess & GENERIC_READ) {
	    bp += sprintf(bp, "GENERIC_READ|");
	}
	if (DesiredAccess & GENERIC_WRITE) {
	    bp += sprintf(bp, "GENERIC_WRITE|");
	}
	if (DesiredAccess & GENERIC_EXECUTE) {
	    bp += sprintf(bp, "GENERIC_EXECUTE|");
	}
	if (DesiredAccess & GENERIC_ALL) {
	    bp += sprintf(bp, "GENERIC_ALL");
	}
	if (bp[-1] == '|') bp[-1] = '\0';
    }
    Lprintf(dip, " = %s\n", buffer);
    PrintHex(dip, "Creation Disposition", CreationDisposition, DNL);
    Lprintf(dip, " = %s\n", CreationDispositionTable[CreationDisposition] );
    PrintHex(dip, "File Attributes", FileAttributes, DNL);
    bp = buffer; *bp = '\0';
    if (FileAttributes == 0) {
	bp += sprintf(bp, "none specified");
    } else {
	/* Note: This is not all the attributes! */
	if (FileAttributes & FILE_ATTRIBUTE_NORMAL) {
	    bp += sprintf(bp, "FILE_ATTRIBUTE_NORMAL|");
	}
	if (FileAttributes & FILE_ATTRIBUTE_READONLY) {
	    bp += sprintf(bp, "FILE_ATTRIBUTE_READONLY|");
	}
	if (FileAttributes & FILE_FLAG_BACKUP_SEMANTICS) {
	    bp += sprintf(bp, "FILE_FLAG_BACKUP_SEMANTICS|");
	}
	if (FileAttributes & FILE_FLAG_DELETE_ON_CLOSE) {
	    bp += sprintf(bp, "FILE_FLAG_DELETE_ON_CLOSE|");
	}
	if (FileAttributes & FILE_FLAG_NO_BUFFERING) {
	    bp += sprintf(bp, "FILE_FLAG_NO_BUFFERING|");
	}
	if (FileAttributes & FILE_FLAG_OVERLAPPED) {
	    bp += sprintf(bp, "FILE_FLAG_OVERLAPPED|");
	}
	if (FileAttributes & FILE_FLAG_RANDOM_ACCESS) {
	    bp += sprintf(bp, "FILE_FLAG_RANDOM_ACCESS|");
	}
	if (FileAttributes & FILE_FLAG_SEQUENTIAL_SCAN) {
	    bp += sprintf(bp, "FILE_FLAG_SEQUENTIAL_SCAN|");
	}
	if (FileAttributes & FILE_FLAG_WRITE_THROUGH) {
	    bp += sprintf(bp, "FILE_FLAG_WRITE_THROUGH");
	}
	if (bp[-1] == '|') bp[-1] = '\0';
    }
    Lprintf(dip, " = %s\n", buffer);
    PrintHex(dip, "Share Mode", ShareMode, DNL);
    bp = buffer; *bp = '\0';
    if (ShareMode == 0) {
	bp += sprintf(bp, " none specified");
    } else {
	if (ShareMode & FILE_SHARE_DELETE) {
	    bp += sprintf(bp, "FILE_SHARE_DELETE|");
	}
	if (ShareMode & FILE_SHARE_READ) {
	    bp += sprintf(bp, "FILE_SHARE_READ|");
	}
	if (ShareMode & FILE_SHARE_WRITE) {
	    bp += sprintf(bp, "FILE_SHARE_WRITE|");
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

/*
 * SetupWindowsFlags() - Setup Native Windows Flags.
 *
 * Inputs:
 *	dip = Device information pointer.
 *	file = The file name.
 *	oflags = The POSIX open flags.
 *	CreateDisposition = Creation disposition flags pointer.
 * 	File Attributes = File attributes pointer.
 * 
 * Outputs:
 *	CreateDisposition = Creation disposition flags.
 * 	File Attributes = File attributes.
 *
 * Return Value:
 *	Void
 */
void
SetupWindowsFlags(struct dinfo *dip, char *file, int oflags,
		  DWORD *CreationDisposition, DWORD *FileAttributes)
{
    *CreationDisposition = 0;
    *FileAttributes = 0;

    if ( (dip->di_dio_flag || (oflags & O_DIRECT))		    	    ||
	((dip->di_mode == READ_MODE) && (dip->di_read_cache_flag == False)) ||
	((dip->di_mode == WRITE_MODE) && (dip->di_write_cache_flag == False)) ) {
	*FileAttributes |= (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
    } else if (oflags & O_DSYNC) {
	*FileAttributes |= FILE_FLAG_WRITE_THROUGH;
    }

    if (dip->di_mode == READ_MODE) {
	*CreationDisposition = OPEN_EXISTING;
	*FileAttributes |= FILE_ATTRIBUTE_READONLY;
    } else {
	if (dip->di_dtype && (dip->di_dtype->dt_dtype == DT_REGULAR)) {
	    //if (GetFileAttributes(file) == INVALID_FILE_ATTRIBUTES) {
	    /* Note: Using dt's API for retryable errors. */
	    if (dt_file_exists(dip, file) == False) {
		/* Avoid isssue w/multiple slices race condition! */
		if (dip->di_slices) {
		    *CreationDisposition = OPEN_ALWAYS;
		} else {
		    *CreationDisposition = CREATE_NEW;
		}
	    } else if (oflags & O_TRUNC) {
		*CreationDisposition = TRUNCATE_EXISTING;
	    } else {
		*CreationDisposition = OPEN_ALWAYS;
	    }
	} else {
	    if (oflags & O_CREAT) {
		*CreationDisposition = OPEN_ALWAYS;
	    } else {
		*CreationDisposition = OPEN_EXISTING;
	    }
	}
    }
    if ( (dip->di_io_type == RANDOM_IO) || (dip->di_io_dir == REVERSE) ) {
	*FileAttributes |= FILE_FLAG_RANDOM_ACCESS;
    } else if (dip->di_io_dir == FORWARD) {
	*FileAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;
    }
    if (dip->di_aio_flag) {
	*FileAttributes |= FILE_FLAG_OVERLAPPED;
    } else if (*FileAttributes == 0) {
	/* The file does not have other attributes set. */
	*FileAttributes |= FILE_ATTRIBUTE_NORMAL;
    }
    return;
}

int
HandleSparseFile(dinfo_t *dip, DWORD FileAttributes)
{
    int status = SUCCESS;
    BOOL is_overlapped = (FileAttributes & FILE_FLAG_OVERLAPPED) ? True : False;

    if ( (dip->di_mode == WRITE_MODE) &&
	 (dip->di_dtype->dt_dtype == DT_REGULAR) ) {
	/*
	 * Attempt to enable sparse file attribute to mimic *nix OS's.
	 * This avoids wasted space (holes) and avoids long noprog's!
	 */
	if (dip->di_sparse_flag) {
	    status = SetSparseFile(dip, dip->di_fd, is_overlapped);
	    //if (status == SUCCESS) return(status);
	    if (status == WARNING) status = SUCCESS;
	    return(status);
	}
	/*
	 * If sparse is disabled or failed, preallocate file blocks.
	 */
	if (dip->di_prealloc_flag &&
	    ((dip->di_io_dir == REVERSE) || (dip->di_io_type == RANDOM_IO)) ) {
	    status = PreAllocateFile(dip, FileAttributes);
	    if (status == FAILURE) {
		if ( isFsFullOk(dip, "WriteFile", dip->di_dname) == False ) {
		    ReportErrorInfo(dip, dip->di_dname, os_get_error(), "WriteFile failed", WRITE_OP, True);
		}
		(void)close_file(dip);
	    }
	}
	status = os_get_volume_information(dip);
    }
    return(status);
}

/*
 * SetSparseFile - Set File as Sparse (to mimic Unix behaviour).
 *
 * Description:
 *	This function sets the sparse attribute, which is necessary to
 * avoid allocating all blocks in a file when writing to large offsets
 * and or a file expected to have holes (any seek / write operation).
 * The default on Windows is to allocate all blocks, where on Unix it
 * is to allow holes so all blocks are not allocated.
 *
 * In the Windows case, allocating blocks for say 400m of data can take
 * many seconds, which causes false long no-progress indicators.  The
 * time is actually being spent allocating blocks, not the actual I/O.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	hDevice = The handle to the device.
 *	FileAttributes = The file attributes (we look at overlapped flag)
 *
 * Return Value:
 *	Success / Failure / Warning (sparse not supported).
 */
int
SetSparseFile(dinfo_t *dip, HANDLE hDevice, BOOL is_overlapped)
{
    char *file = dip->di_dname;
    DWORD FileAttributes;
    int	status;
    int	rc = SUCCESS;

    status = os_get_volume_information(dip);
    if (status == FAILURE) return(status);

    if (dip->di_file_system_flags & FILE_SUPPORTS_SPARSE_FILES) {
	; 	// File system supports sparse streams
    } else {
	if (dip->di_debug_flag) {
	    Printf(dip, "Warning: Sparse files are NOT supported!\n");
	}
	return(WARNING);
    }

    status = dt_get_file_attributes(dip, file, &FileAttributes);
    if (status == FAILURE) return(status);

    if (FileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) {
	if (dip->di_debug_flag) {
	    Printf(dip, "File %s is already a sparse file!\n", file);
	}
	return(status);
    }

    if (dip->di_debug_flag) {
        Printf(dip, "Enabling sparse file attribute via FSCTL_SET_SPARSE...\n");
    }
    
    //dip->di_retry_count = 0;		/* Assume we're being called after open! */
    do {
	status = os_set_sparse_file(dip, file, hDevice, is_overlapped);
	if (status == FAILURE) {
	    char *op = OS_SET_SPARSE_FILE_OP;
	    INIT_ERROR_INFO(eip, file, op, SPARSE_OP, &dip->di_fd, dip->di_oflags, dip->di_offset,
			    (size_t)0, os_get_error(), logLevelError, PRT_SYSLOG, RPT_NOFLAGS);
	    rc = ReportRetryableError(dip, eip, "Failed %s on file %s", op, file);
	    status = FAILURE;
	}
    } while ( (status == FAILURE) && (rc == RETRYABLE) );

    return(status);
}

int
os_set_sparse_file(dinfo_t *dip, char *file, HANDLE hDevice, BOOL is_overlapped)
{
    OVERLAPPED Overlapped;
    DWORD BytesReturned;
    BOOL bStatus;
    int status = SUCCESS;
    
    memset(&Overlapped, '\0', sizeof(Overlapped));
    bStatus = DeviceIoControl( hDevice,	FSCTL_SET_SPARSE, NULL,	0,
			       NULL, 0,	&BytesReturned,	&Overlapped);
    if (bStatus == False) {
	DWORD error = GetLastError();
	if (error == ERROR_IO_PENDING) {
	    do {
		/* Note: We could simplify, but this is an interesting exercise! */
		bStatus = GetOverlappedResult(hDevice, &Overlapped, &BytesReturned, False);
		if (bStatus == False) {
		    error = GetLastError();
		    if (error == ERROR_IO_INCOMPLETE) {
			Sleep(10);
		    } else {
			//os_perror(dip, "GetOverlappedResult failed");
			status = FAILURE;
			break;
		    }
		}
	    } while ( (bStatus == False) && (error == ERROR_IO_INCOMPLETE) );
	} else {
	    //Fprintf(dip, "Setting sparse file attribute for file %s failed!\n", dip->di_dname);
	    //os_perror(dip, "DeviceIoControl(FSCTL_SET_SPARSE) failed");
	    status = FAILURE;
	}
    } else if (is_overlapped == True) {
	bStatus = GetOverlappedResult(hDevice, &Overlapped, &BytesReturned, True);
	if (bStatus == False) {
	    //os_perror(dip, "GetOverlappedResult failed");
	    status = FAILURE;
	}
    }
    return(status);
}

int
PreAllocateFile(dinfo_t *dip, DWORD FileAttributes)
{
    unsigned char *buffer;
    DWORD nbytes = BLOCK_SIZE, written = 0;
    Offset_t pos = (dip->di_data_limit - nbytes), npos;
    BOOL result;
    int status = SUCCESS;

    if (dip->di_data_limit < nbytes) return (status);
    if (pos < 0) pos = (Offset_t) 0;
    if (FileAttributes & FILE_FLAG_NO_BUFFERING) {
	/* Must be block aligned offset! */
	pos = roundup(pos, nbytes);
    }
    /* Windows with unbuffered I/O doesn't care if we're aligned, but some FS's do! */
    buffer = malloc_palign(dip, nbytes, 0);
    if (dip->di_debug_flag) {
	Printf(dip, "Preallocating data blocks by writing %u bytes to offset " FUF "...\n",
	       nbytes, pos);
    }
    if (FileAttributes & FILE_FLAG_OVERLAPPED) {
	OVERLAPPED overlap;
	overlap.hEvent = 0;
	overlap.Offset = ((PLARGE_INTEGER)(&pos))->LowPart;
	overlap.OffsetHigh = ((PLARGE_INTEGER)(&pos))->HighPart;
	result = WriteFile(dip->di_fd, buffer, nbytes, NULL, &overlap);
	if ( (result == False) && (GetLastError() != ERROR_IO_PENDING) ) {
	    free_palign(dip, buffer);
	    return (FAILURE);
	}
	result = GetOverlappedResult(dip->di_fd, &overlap, &written, True);
    } else {
	npos = set_position(dip, pos, False);
	if (npos == (Offset_t) -1) return(FAILURE);
	if (npos != pos) {
	    Fprintf(dip, "ERROR: Wrong seek position, (npos " FUF " != pos " FUF ")!\n",
		    npos, pos);
	    free_palign(dip, buffer);
	    return (FAILURE);
	}
	result = WriteFile(dip->di_fd, buffer, nbytes, &written, NULL);
    }
    free_palign(dip, buffer);
    if (result == False) status = FAILURE;
    if (nbytes != written) status = FAILURE;
    /* Ensure we restore the file position, we expect 0 after open! */
    /* Note: With iotype=vary we may switch to sequential I/O. */
    npos = set_position(dip, (Offset_t) 0, False);
    if (npos == (Offset_t) -1) return(FAILURE);
    return (status);
}

/*
 * os_isEof() - Determine if this is an EOF condition.
 *
 * Description:
 *	Well clearly, we're checking for more than EOF errors below.
 * These additional checks are made, since the algorithm for finding
 * capacity (seek/read), and our step option, can cause one of these
 * other errors when reading past end of media (EOM).
 *
 *	Now that said, I did not do the original Windows port, so I'm
 * trusting these other error checks for won't mask *real* errors. But,
 * this may need revisited at some point.
 */
hbool_t
os_isEof(ssize_t count, int error)
{
    if ( (count == 0) ||
	 ( (count < 0) && 
	   ( (error == ERROR_DISK_FULL)		||
	     (error == ERROR_HANDLE_EOF)	||
	     (error == ERROR_SECTOR_NOT_FOUND) ) ) ) { 
	return(True);
    } else {
	return(False);
    }
}

__inline int
os_lock_file(HANDLE fh, Offset_t start, Offset_t length, int type)
{
    /*
     * BOOL WINAPI LockFile(
     *  _In_  HANDLE hFile,
     *  _In_  DWORD dwFileOffsetLow,
     *  _In_  DWORD dwFileOffsetHigh,
     *  _In_  DWORD nNumberOfBytesToLockLow,
     *  _In_  DWORD nNumberOfBytesToLockHigh
     * );
     */
    return( ( LockFile(fh, (DWORD)start, (DWORD)(start >> 32), (DWORD)length, (DWORD)(length >> 32)) ) ? SUCCESS : FAILURE);
}

__inline int
os_unlock_file(HANDLE fh, Offset_t start, Offset_t length)
{
    /*
     * BOOL WINAPI UnlockFile(
     *  _In_  HANDLE hFile,
     *  _In_  DWORD dwFileOffsetLow,
     *  _In_  DWORD dwFileOffsetHigh,
     *  _In_  DWORD nNumberOfBytesToUnlockLow,
     *  _In_  DWORD nNumberOfBytesToUnlockHigh
     * );
     */
    return( ( UnlockFile(fh, (DWORD)start, (DWORD)(start >> 32), (DWORD)length, (DWORD)(length >> 32)) ) ? SUCCESS : FAILURE);
}

__inline int
os_xlock_file(HANDLE fh, Offset_t start, Offset_t length, int type, hbool_t exclusive, hbool_t immediate)
{
    OVERLAPPED ol;
    DWORD flags = 0;
    
    memset(&ol, sizeof(ol), '\0');
    ol.Offset = ((PLARGE_INTEGER)(&start))->LowPart;
    ol.OffsetHigh = ((PLARGE_INTEGER)(&start))->HighPart;
    if (exclusive == True) flags |= LOCKFILE_EXCLUSIVE_LOCK;
    if (immediate == True) flags |= LOCKFILE_FAIL_IMMEDIATELY;

    /*
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa365203(v=vs.85).aspx
     * 
     * BOOL WINAPI LockFileEx(
     *  _In_        HANDLE hFile,
     *  _In_        DWORD dwFlags,
     *  _Reserved_  DWORD dwReserved,
     *  _In_        DWORD nNumberOfBytesToLockLow,
     *  _In_        DWORD nNumberOfBytesToLockHigh,
     *  _Inout_     LPOVERLAPPED lpOverlapped
     * );
     */
    return( ( LockFileEx(fh, flags, 0, (DWORD)length, (DWORD)(length >> 32), &ol)) ? SUCCESS : FAILURE);
}

__inline int
os_xunlock_file(HANDLE fh, Offset_t start, Offset_t length)
{
    OVERLAPPED ol;
    
    memset(&ol, sizeof(ol), '\0');
    ol.Offset = ((PLARGE_INTEGER)(&start))->LowPart;
    ol.OffsetHigh = ((PLARGE_INTEGER)(&start))->HighPart;

    /*
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa365716(v=vs.85).aspx
     * 
     * BOOL WINAPI UnlockFileEx(
     *  _In_        HANDLE hFile,
     *  _Reserved_  DWORD dwReserved,
     *  _In_        DWORD nNumberOfBytesToUnlockLow,
     *  _In_        DWORD nNumberOfBytesToUnlockHigh,
     *  _Inout_     LPOVERLAPPED lpOverlapped
     * );
     */
    return( ( UnlockFileEx(fh, 0, (DWORD)length, (DWORD)(length >> 32), &ol)) ? SUCCESS : FAILURE);
}

__inline int
os_move_file(char *oldpath, char *newpath)
{
    /* 
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa365239(v=vs.85).aspx 
     *  
     * BOOL WINAPI MoveFile(
     *  _In_  LPCTSTR lpExistingFileName,
     *  _In_  LPCTSTR lpNewFileName
     * );
     */
    return ( (MoveFile(oldpath, newpath) == True) ? SUCCESS : FAILURE);
}

__inline int
os_rename_file(char *oldpath, char *newpath)
{
    /*
     * Unix rename() behavior is different than Windows: 
     *    If newpath already exists it will be atomically replaced (subject to
     * a few conditions), so that there is no point at which another process
     * attempting to access newpath will find it missing. 
     *  
     * Therefore, for Windows we must remove the newpath first!
     */
    if ( os_file_exists(oldpath) && os_file_exists(newpath) ) {
	int status = os_delete_file(newpath);
	if (status == FAILURE) return(status);
    }

    /* 
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa365239(v=vs.85).aspx 
     *  
     * BOOL WINAPI MoveFile(
     *  _In_  LPCTSTR lpExistingFileName,
     *  _In_  LPCTSTR lpNewFileName
     * );
     */
    return ( (MoveFile(oldpath, newpath) == True) ? SUCCESS : FAILURE);
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
	    *lock_type_flag = LOCKFILE_FAIL_IMMEDIATELY;        /* default is "shared" */
	    *exclusive = False;
	    break;

	case LOCK_TYPE_WRITE:
	    *lock_type_flag = (LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY);
	    break;

	case LOCK_TYPE_UNLOCK:
	    *unlock_flag = True;
	    break;

	default:
	    status = FAILURE;
	    break;
    }
    return(status);
}

__inline int
os_link_file(char *oldpath, char *newpath)
{
    /* 
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363866(v=vs.85).aspx 
     *  
     * BOOL WINAPI CreateHardLink(
     *  _In_        LPCTSTR lpFileName,
     *  _In_        LPCTSTR lpExistingFileName,
     *  _Reserved_  LPSECURITY_ATTRIBUTES lpSecurityAttributes
     * );
     */
    return ( (CreateHardLink(newpath, oldpath, NULL) == True) ? SUCCESS : FAILURE);
}

hbool_t
os_symlink_supported(void)
{
    LUID luid;
    LPCTSTR Name = "SeCreateSymbolicLinkPrivilege";
    PTOKEN_PRIVILEGES tpp = NULL;
    TOKEN_INFORMATION_CLASS tc = TokenPrivileges;
    DWORD ReturnLength, priv_index;
    HANDLE hToken;
    BOOL result;

    /*
     * URL: http://msdn.microsoft.com/en-us/library/windows/desktop/aa379180(v=vs.85).aspx
     * 
     * BOOL WINAPI LookupPrivilegeValue(
     *  _In_opt_  LPCTSTR lpSystemName,
     *  _In_      LPCTSTR lpName,
     *  _Out_     PLUID lpLuid
     * );
     */
    result = LookupPrivilegeValue(NULL, Name, &luid);
    if (result == False) return(result);

    /*
     * BOOL WINAPI OpenProcessToken(
     *  _In_   HANDLE ProcessHandle,
     *  _In_   DWORD DesiredAccess,
     *  _Out_  PHANDLE TokenHandle
     * );
     */
    result = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken);
    if (result == False) return(result);

    /*
     * BOOL WINAPI GetTokenInformation(
     *  _In_       HANDLE TokenHandle,
     *  _In_       TOKEN_INFORMATION_CLASS TokenInformationClass,
     *  _Out_opt_  LPVOID TokenInformation,
     *  _In_       DWORD TokenInformationLength,
     *  _Out_      PDWORD ReturnLength
     * );
     */
    result = GetTokenInformation(hToken, tc, NULL, (DWORD)0, &ReturnLength);
    // Yes, the result is failure, but the ReturnLength is filled in!
    tpp = malloc(ReturnLength);
    if (tpp == NULL) goto error_return;
    result = GetTokenInformation(hToken, tc, tpp, ReturnLength, &ReturnLength);
    if (result == False) {
	//os_perror(NULL, "GetTokenInformation() failed");
	goto error_return;
    }
    /*
     * typedef struct _TOKEN_PRIVILEGES {
     *  DWORD               PrivilegeCount;
     *  LUID_AND_ATTRIBUTES Privileges[ANYSIZE_ARRAY];
     * } TOKEN_PRIVILEGES, *PTOKEN_PRIVILEGES;
     * 
     * typedef struct _LUID_AND_ATTRIBUTES {
     *  LUID  Luid;
     *  DWORD Attributes;
     * } LUID_AND_ATTRIBUTES, *PLUID_AND_ATTRIBUTES;
     * 
     * typedef struct _LUID {
     *  DWORD LowPart;
     *  LONG  HighPart;
     * } LUID, *PLUID;
     */
    result = False;
    for (priv_index = 0; priv_index < tpp->PrivilegeCount; priv_index++) {
	PLUID puidp = &tpp->Privileges[priv_index].Luid;
	if ( (puidp->LowPart == luid.LowPart) &&
	     (puidp->HighPart == luid.HighPart) ) {
	    result = True;	/* Symbolic link privilege is supported! */
	    break;
	}
    }
    free(tpp);
error_return:
    (void)CloseHandle(hToken);
    return(result);
}

#if defined(WINDOWS_XP)

__inline int
 os_symlink_file(char *oldpath, char *newpath)
 {
     return(FAILURE);
 }

#else /* !defined(WINDOWS_XP) */

__inline int
os_symlink_file(char *oldpath, char *newpath)
{
    /* 
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363860(v=vs.85).aspx 
     *  
     * BOOLEAN WINAPI CreateSymbolicLink(
     *  _In_  LPTSTR lpSymlinkFileName,
     *  _In_  LPTSTR lpTargetFileName,
     *  _In_  DWORD dwFlags
     * );
     * 
     * Note: Requires SE_CREATE_SYMBOLIC_LINK_NAME privilege.
     * This function requires the SE_CREATE_SYMBOLIC_LINK_NAME (defined as
     * "SeCreateSymbolicLinkPrivilege" in <WinNT.h>), otherwise the function
     * will fail and GetLastError will return ERROR_PRIVILEGE_NOT_HELD (1314).
     * This means the process must be run in an elevated state.
     */
    return ( (CreateSymbolicLink(newpath, oldpath, 0) == True) ? SUCCESS : FAILURE);
}

#endif /* !defined(WINDOWS_XP) */

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)

int
os_file_trim(HANDLE handle, Offset_t offset, uint64_t length)
{
    BOOL result;
    FILE_LEVEL_TRIM trim_header;
    PFILE_LEVEL_TRIM pth = &trim_header;
    PFILE_LEVEL_TRIM_RANGE ptr = &pth->Ranges[0];
    FILE_LEVEL_TRIM_OUTPUT trim_output;
    PFILE_LEVEL_TRIM_OUTPUT pto = &trim_output;
    DWORD bytesReturned = 0;
    
    pth->Key = 0;
    pth->NumRanges = 1;
    ptr->Offset = (ULONGLONG)offset;
    ptr->Length = (ULONGLONG)length;
    pto->NumRangesProcessed = (ULONG)0;
    //printf("ranges = %u, offset = %llu, length = %llu\n", pth->NumRanges, ptr->Offset, ptr->Length);

    /* 
     * URL: http://msdn.microsoft.com/en-us/library/windows/desktop/hh447306(v=vs.85).aspx
     * 
     * BOOL 
     * WINAPI 
     * DeviceIoControl( (HANDLE) hDevice,              // handle to device
     *     	        (DWORD) FSCTL_FILE_LEVEL_TRIM, // dwIoControlCode
     *     	        (LPVOID) lpInBuffer,           // input buffer
     *     	        (DWORD) nInBufferSize,         // size of input buffer
     *     	        (LPVOID) lpOutBuffer,          // output buffer
     *     	        (DWORD) nOutBufferSize,        // size of output buffer
     *     	        (LPDWORD) lpBytesReturned,     // number of bytes returned
     *     	        (LPOVERLAPPED) lpOverlapped ); // OVERLAPPED structure
     * 
     */
    result = DeviceIoControl( (HANDLE)handle,               // handle to device
			      (DWORD)FSCTL_FILE_LEVEL_TRIM, // dwIoControlCode
			      (LPVOID)pth,                  // input buffer
			      (DWORD)sizeof(*pth),          // size of input buffer
			      (LPVOID)pto,                  // output buffer
			      (DWORD)sizeof(*pto),          // size of output buffer
			      (LPDWORD)&bytesReturned,      // number of bytes returned
			      (LPOVERLAPPED)NULL );         // OVERLAPPED structure

    return( (result == True) ? SUCCESS : FAILURE );
}

#else /* !_WIN32_WINNT >= _WIN32_WINNT_WIN8 */

int
os_file_trim(HANDLE handle, Offset_t offset, uint64_t length)
{
    return(WARNING);
}

#endif /*_WIN32_WINNT >= _WIN32_WINNT_WIN8 */

static DWORD disconnect_errors[] = {
    ERROR_NETNAME_DELETED,	// 64L = The specified network name is no longer available.
    ERROR_UNEXP_NET_ERR,	// 59L = An unexpected network error occurred.
    ERROR_DEV_NOT_EXIST,	// 55L = The specified network resource or device is no longer available.
    ERROR_REM_NOT_LIST,		// 51L = The remote computer is not available.
    ERROR_BAD_NETPATH,		// 53L = The network path was not found.
    ERROR_BAD_NET_NAME,		// 67L = The network name cannot be found.
    ERROR_VC_DISCONNECTED,	// 240L = The session was canceled.
    ERROR_SEM_TIMEOUT,		// 121L = The semaphore timeout period has expired.
    ERROR_NO_LOGON_SERVERS,	// 1311L = There are currently no logon servers available to service the logon request.
    ERROR_LOGON_FAILURE,	// 1326L = Logon failure: unknown user name or bad password.
    ERROR_INVALID_HANDLE,	// 6L = The handle is invalid.
    ERROR_NOT_SUPPORTED,	// 50L = The network request is not supported.
    ERROR_NONE_MAPPED,		// 1332L = No mapping between account names and security IDs was done.
    ERROR_CONNECTION_ABORTED,	// 1236L = The network connection was aborted by the local system.
    ERROR_OPERATION_ABORTED	// 995L = The I/O operation has been aborted because of either a thread exit or an application request.
};
static int num_disconnect_entries = sizeof(disconnect_errors) / sizeof(DWORD);

hbool_t
os_is_session_disconnected(int error)
{
    int entry;

    for (entry = 0; (entry < num_disconnect_entries); entry++) {
	if ( (DWORD)error == disconnect_errors[entry] ) return(True);
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

int
os_set_thread_cancel_type(dinfo_t *dip, int cancel_type)
{
    return(SUCCESS);
}

#define UUID_BUFFER_SIZE	(sizeof(UUID) * 3)

char *
os_get_uuid(hbool_t want_dashes)
{
    char auuid[UUID_BUFFER_SIZE];
    uuid_t uuid;
    int uuidlen = 0;
    int idx = 0;
    RPC_CSTR struuid;
    RPC_STATUS status;

    memset(auuid, '\0', sizeof(auuid));
    UuidCreate(&uuid);
    status = UuidToString(&uuid, &struuid);
    if (status != RPC_S_OK) {
	return(NULL);
    }

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

    RpcStringFree(&struuid);

    return( strdup(auuid) );
}

/* --------------------------------------------------------------------------------------------------------- */
/*
 * NTFS File Offset to Physical LBA mapping functions.
 */
#include <ntddvol.h>
#include <winioctl.h>

/* Note: Data structures added for Robin's reference, I am not a Windows guy! */

typedef struct {
    /*
     * typedef struct _VOLUME_DISK_EXTENTS {
     *   DWORD       NumberOfDiskExtents;
     *   DISK_EXTENT Extents[ANYSIZE_ARRAY];
     * } VOLUME_DISK_EXTENTS, *PVOLUME_DISK_EXTENTS;
     */
    VOLUME_DISK_EXTENTS volExtents;
    /*
     * typedef struct _DISK_EXTENT {
     *   DWORD         DiskNumber;
     *   LARGE_INTEGER StartingOffset;
     *   LARGE_INTEGER ExtentLength;
     * } DISK_EXTENT, *PDISK_EXTENT;
     */
    DISK_EXTENT extent;
} MIRRORED_DISK_EXTENT;

typedef enum SupportedFileSystems {
    FAT32,
    NTFS
} SupportedFileSystems_t;

typedef struct {
    HANDLE fileHandle;				/* The file handle. */
    BOOL closeFileHandle;			/* Close file handle if True. */
    HANDLE hVolume;				/* The volume handle (e.g. C:\). */
    MIRRORED_DISK_EXTENT volumeExtents;
    /* 
     * typedef struct {
     *   LARGE_INTEGER VolumeSerialNumber;
     *   LARGE_INTEGER NumberSectors;
     *   LARGE_INTEGER TotalClusters;
     *   LARGE_INTEGER FreeClusters;
     *   LARGE_INTEGER TotalReserved;
     *   DWORD         BytesPerSector;
     *   DWORD         BytesPerCluster;
     *   DWORD         BytesPerFileRecordSegment;
     *   DWORD         ClustersPerFileRecordSegment;
     *   LARGE_INTEGER MftValidDataLength;
     *   LARGE_INTEGER MftStartLcn;
     *   LARGE_INTEGER Mft2StartLcn;
     *   LARGE_INTEGER MftZoneStart;
     *   LARGE_INTEGER MftZoneEnd;
     * } NTFS_VOLUME_DATA_BUFFER, *PNTFS_VOLUME_DATA_BUFFER;  
     */ 
    NTFS_VOLUME_DATA_BUFFER volumeData;
    LONGLONG volStartSector;
    /*
     * typedef struct {
     *   LARGE_INTEGER StartingVcn;
     * } STARTING_VCN_INPUT_BUFFER, *PSTARTING_VCN_INPUT_BUFFER; 
     */
    STARTING_VCN_INPUT_BUFFER inputVcn;
    /*
     * typedef struct RETRIEVAL_POINTERS_BUFFER {
     *   DWORD                    ExtentCount;
     *   LARGE_INTEGER            StartingVcn;
     *   struct {
     *     LARGE_INTEGER NextVcn;
     *     LARGE_INTEGER Lcn;
     *   };
     *   __unnamed_struct_087a_54 Extents[1];
     * } RETRIEVAL_POINTERS_BUFFER, *PRETRIEVAL_POINTERS_BUFFER;
     */
    RETRIEVAL_POINTERS_BUFFER rpBuf;
    BOOLEAN verify;				/* Verify the translation. */
    SupportedFileSystems_t fileSystemType;
    DWORD rootStart;
    DWORD clusterStart;
    char fullVolName[MAX_PATH];
    char fileSystemName[MAX_PATH];
} TRANSLATION;

typedef void (*IterateAction)(dinfo_t *dip, LONGLONG vcn, LONGLONG lcn, LONGLONG clusters);

/*
 * Forward References: 
 *  
 * Good Reference: http://timr.probo.com/wd3/121503/luserland.htm 
 *  
 * Please Note: None of this works on compressed files or volumes or NTFS sparse allocated files. 

 * Maybe someone more knowledge of Windows, can provide methods to overcome these limitations?
 * FYI: I have found the inability to handle sparse files the biggest issue, since folks using
 * random I/O or using slices or step options creates sparse files.
 */
TRANSLATION *initFileTranslation(dinfo_t *dip, char *filename, HANDLE fileHandle, BOOL verify);
void closeTranslation(dinfo_t *dip, TRANSLATION *translation);
void resetTranslation(TRANSLATION *translation);
DWORD getNextTranslation( dinfo_t *dip,
			  TRANSLATION *translation,
			  LONGLONG *fileOffset,
			  LONGLONG *startSector,
			  LONGLONG *nSectors);
BOOL getLBAandLengthByOffset( dinfo_t *dip,
			      TRANSLATION *translation,
			      LONGLONG fileOffset,
			      LONGLONG recordLength,
			      LONGLONG *startSectorLBA,
			      LONGLONG *runLength);
BOOL validateTranslation( dinfo_t *dip,
			  TRANSLATION *translation,
			  LONGLONG *fileOffset,
			  LONGLONG *startSector);
void printClusterMap(dinfo_t *dip, LONGLONG vcn, LONGLONG lcn, LONGLONG clusters);
BOOL printAllClusters(dinfo_t *dip, PVOID translationToken);
BOOL iterateAllClusters(dinfo_t *dip, HANDLE fileHandle, IterateAction callback);
int get_fs_info(dinfo_t *dip, char *filename, HANDLE fileHandle, DWORD64 offset, DWORD64 fsize);

/* --------------------------------------------------------------------------------------------------------- */

/* Callback function to print the cluster map entries. Do we need this? */
void
printClusterMap(dinfo_t *dip, LONGLONG vcn, LONGLONG lcn, LONGLONG clusters)
{
    Printf(dip, "VCN: %I64d LCN: %I64d Clusters: %I64d\n", vcn, lcn, clusters);
}

/* Print all clusters for the current translation object. Do we need this? */
BOOL
printAllClusters(dinfo_t *dip, PVOID translationToken)
{
    TRANSLATION *translation = (TRANSLATION *)translationToken;

    return iterateAllClusters(dip, translation->fileHandle, printClusterMap);
}

/*
 * Iterate across the VCN -> LCN mapping,
 * calling the supplied callback function for each iteration.
 */
BOOL
iterateAllClusters(dinfo_t *dip, HANDLE fileHandle, IterateAction callback)
{
    STARTING_VCN_INPUT_BUFFER inputVcn;
    RETRIEVAL_POINTERS_BUFFER rpBuf;
    DWORD dwBytesReturned;
    DWORD error = NO_ERROR;
    BOOL result = False;

    inputVcn.StartingVcn.QuadPart = 0; /* Start at the beginning. */

   do {
        result = DeviceIoControl(fileHandle,
				 FSCTL_GET_RETRIEVAL_POINTERS,
				 &inputVcn,
				 sizeof(STARTING_VCN_INPUT_BUFFER),
				 &rpBuf,
				 sizeof(RETRIEVAL_POINTERS_BUFFER),
				 &dwBytesReturned,
				 NULL);

        error = GetLastError();

        switch (error) {

            case ERROR_HANDLE_EOF:
                result = True;
                break;

            case ERROR_MORE_DATA:
                inputVcn.StartingVcn = rpBuf.Extents[0].NextVcn;
		/* Fall through... */
	    case NO_ERROR:
                callback(dip, rpBuf.StartingVcn.QuadPart, rpBuf.Extents[0].Lcn.QuadPart,
			 rpBuf.Extents[0].NextVcn.QuadPart - rpBuf.StartingVcn.QuadPart);
                result = True;
                break;

            default:
		if (dip->di_fDebugFlag) {
		    os_perror(dip, "iterateAllClusters: FSCTL_GET_RETRIEVAL_POINTERS failed");
		}
                break;
        }
    } while (error == ERROR_MORE_DATA);

    return(result);
}

/* --------------------------------------------------------------------------------------------------------- */

/*
 * This does the initial setup to acquire required information for file offset mapping!
 */
TRANSLATION *
initFileTranslation(dinfo_t *dip, char *filename, HANDLE fileHandle, BOOL verify)
{
    TRANSLATION *transaction = NULL;
    BOOL result = True;

    do {
        transaction = Malloc(dip, sizeof(TRANSLATION));
        if (transaction == NULL) {
            result = False;
            break;
        }

        transaction->verify = verify;
	transaction->fileHandle = INVALID_HANDLE_VALUE;

	/* Attributes URL: https://docs.microsoft.com/en-us/windows/win32/fileio/file-attribute-constants */
        DWORD Attributes = GetFileAttributes(filename);

        if  (Attributes == INVALID_FILE_ATTRIBUTES) {
	    if (dip->di_fDebugFlag) {
		os_perror(dip, "GetFileAttributes() failed on %s", filename);
	    }
            result = False;
            break;
        }

        if (Attributes & (FILE_ATTRIBUTE_COMPRESSED|FILE_ATTRIBUTE_ENCRYPTED)) {
	    if (dip->di_fDebugFlag) {
		Wprintf(dip, "Compressed or encrypted file detected, NOT supported!\n");
	    }
            SetLastError(ERROR_INVALID_PARAMETER);
            result = False;
            break;
        }

	if (fileHandle != INVALID_HANDLE_VALUE) {
	    transaction->fileHandle = fileHandle;	/* The file is already open. */
	    transaction->closeFileHandle = False;	/* Don't close during cleanup. */
	} else {
	    transaction->fileHandle = CreateFile(filename,
						 verify ? GENERIC_READ : FILE_READ_ATTRIBUTES,
						 FILE_SHARE_READ|FILE_SHARE_WRITE,
						 0,
						 OPEN_EXISTING,
						 FILE_FLAG_NO_BUFFERING,
						 0);

	    if (transaction->fileHandle == INVALID_HANDLE_VALUE) {
		if (dip->di_fDebugFlag) {
		    os_perror(dip, "CreateFile() failed on %s", filename);
		}
		result = False;
		break;
	    }
	    transaction->closeFileHandle = True;	/* Close file handle during cleanup. */
	}

        char volumeName[MAX_PATH];
	/*
	 * BOOL GetVolumePathName(
	 *   LPCSTR lpszFileName,
	 *   LPSTR  lpszVolumePathName,
	 *   DWORD  cchBufferLength
	 * );
	 */
        result = GetVolumePathName(filename, volumeName, sizeof(volumeName));

        if (result == False) {
           if (dip->di_fDebugFlag) {
		os_perror(dip, "GetVolumePathName() failed on %s", filename);
	    }
            break;
        }

        DWORD SectorsPerCluster;
        DWORD NumberOfFreeClusters;
        DWORD TotalNumberOfClusters;
	/*
	 * BOOL GetDiskFreeSpace(
	 *   LPCSTR  lpRootPathName,
	 *   LPDWORD lpSectorsPerCluster,
	 *   LPDWORD lpBytesPerSector,
	 *   LPDWORD lpNumberOfFreeClusters,
	 *   LPDWORD lpTotalNumberOfClusters
	 * );
	 */
        result = GetDiskFreeSpace(volumeName,
				  &SectorsPerCluster,
				  &transaction->volumeData.BytesPerSector,
				  &NumberOfFreeClusters,
				  &TotalNumberOfClusters);
        if (result == False) {
	    if (dip->di_fDebugFlag) {
		os_perror(dip, "GetDiskFreeSpace() failed on %s\n", volumeName);
	    }
            break;
        }
        transaction->volumeData.BytesPerCluster = (transaction->volumeData.BytesPerSector * SectorsPerCluster);
        transaction->volumeData.NumberSectors.QuadPart = TotalNumberOfClusters;
        transaction->volumeData.NumberSectors.QuadPart *= SectorsPerCluster;

        DWORD maxNameLength;
        DWORD fileSystemFlags;
	/*
	 * BOOL GetVolumeInformation(
	 *   LPCSTR  lpRootPathName,
	 *   LPSTR   lpVolumeNameBuffer,
	 *   DWORD   nVolumeNameSize,
	 *   LPDWORD lpVolumeSerialNumber,
	 *   LPDWORD lpMaximumComponentLength,
	 *   LPDWORD lpFileSystemFlags,
	 *   LPSTR   lpFileSystemNameBuffer,
	 *   DWORD   nFileSystemNameSize
	 * );
	 */
        result = GetVolumeInformation(volumeName,
				      NULL, 0,
				      NULL,
				      &maxNameLength,
				      &fileSystemFlags,
				      transaction->fileSystemName,
				      sizeof(transaction->fileSystemName));
        if (result == False) {
	    if (dip->di_fDebugFlag) {
		os_perror(dip, "GetVolumeInformation() failed on %s", volumeName);
	    }
	    break;
        }

        if ( EQ(transaction->fileSystemName, "FAT32") ) {
            transaction->fileSystemType = FAT32;
	    if (dip->di_fDebugFlag) {
		Wprintf(dip, "FAT32 file system detected, we do NOT support this!");
	    }
            SetLastError(ERROR_NOT_SUPPORTED);
	    break;
        } else if ( EQS(transaction->fileSystemName, "NTFS") ) {
            transaction->fileSystemType = NTFS;
        } else {
            result = False;
            SetLastError(ERROR_NOT_SUPPORTED);
            break;
        }

	int vlen = (int)strlen(volumeName);
	/* Remove trailing slash, if any. */
	if (volumeName[vlen-1] == '\\') {
	    volumeName[vlen-1] = '\0';
	}
	/* Construct the full volume name. "\\.\" is the hidden device directory! */
        _snprintf(transaction->fullVolName, sizeof(transaction->fullVolName), "\\\\.\\%s", volumeName);

	/*
	 * Please Note: This open WILL fail, if you do NOT have permissions! (administrator/system)
	 */
        transaction->hVolume = CreateFile(transaction->fullVolName,
					  GENERIC_READ,
					  (FILE_SHARE_READ | FILE_SHARE_WRITE),
					  NULL, OPEN_EXISTING,
					  FILE_ATTRIBUTE_NORMAL, NULL);

        if (transaction->hVolume == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if ( os_isAccessDenied(error) ) {
                if ( dip->di_verbose_flag ) {
                    Wprintf(dip, "Unable to open volume handle %s, disabling file system mapping!\n",
                            transaction->fullVolName);
                }
            } else if (dip->di_fDebugFlag) {
		os_perror(dip, "CreateFile()/GENERIC_READ failed on %s", transaction->fullVolName);
	    }
            dip->di_fsmap_flag = False;
	    result = False;
            break;
        }

        DWORD dwBytesReturned = 0;
        transaction->volumeData.BytesPerCluster = 0;
        result = DeviceIoControl(transaction->hVolume,
				 FSCTL_GET_NTFS_VOLUME_DATA,
				 NULL, 0,
				 &transaction->volumeData,
				 sizeof(transaction->volumeData),
				 &dwBytesReturned,
				 NULL);

	if (dip->di_fDebugFlag) {
	    Printf(dip, "Volume Serial number "LXF"\n", transaction->volumeData.VolumeSerialNumber.QuadPart);
	}

        result = DeviceIoControl(transaction->hVolume,
				 IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
				 NULL, 0,
				 &transaction->volumeExtents,
				 sizeof(transaction->volumeExtents),
				 &dwBytesReturned,
				 NULL);

        if (result == False) {
            if (GetLastError() != ERROR_MORE_DATA) {
                result = False;
                break;
            }
        }

        if (transaction->fileSystemType == FAT32) {
	    result = False;	/* Not supporting this! */
        } else { /* NTFS! */
            transaction->clusterStart = 0;
            transaction->rootStart = 0;
        }
        if (result == False) {
            break;
        }

        LONGLONG volumeLength = (transaction->volumeData.NumberSectors.QuadPart * transaction->volumeData.BytesPerSector);

	/* FM! */
        if (transaction->volumeExtents.volExtents.NumberOfDiskExtents > 1) {
            if ((transaction->volumeExtents.volExtents.NumberOfDiskExtents > 2) ||
                (transaction->volumeExtents.volExtents.Extents[0].StartingOffset.QuadPart !=
                 transaction->volumeExtents.volExtents.Extents[1].StartingOffset.QuadPart) ||
                (transaction->volumeExtents.volExtents.Extents[0].ExtentLength.QuadPart !=
                transaction->volumeExtents.volExtents.Extents[1].ExtentLength.QuadPart) ||
                (transaction->volumeExtents.volExtents.Extents[0].ExtentLength.QuadPart < volumeLength) ||
                 (transaction->volumeExtents.volExtents.Extents[1].ExtentLength.QuadPart < volumeLength)) {
		if (dip->di_fDebugFlag) {
		    Wprintf(dip, "Stripped or compressed file detected, NOT supported!\n");
		}
                SetLastError(ERROR_NOT_SUPPORTED);
                result = False;
                break;
            }
        }

        transaction->volStartSector = (transaction->volumeExtents.volExtents.Extents[0].StartingOffset.QuadPart / transaction->volumeData.BytesPerSector);

        memset(&transaction->inputVcn, 0, sizeof(STARTING_VCN_INPUT_BUFFER));
        memset(&transaction->rpBuf, 0, sizeof(RETRIEVAL_POINTERS_BUFFER));

    } while(0);

    if (result == False) {
        if (transaction) {
	    closeTranslation(dip, transaction);
	    transaction = NULL;
        }
    }
    return(transaction);
}

void
closeTranslation(dinfo_t *dip, TRANSLATION *translation)
{
    if ( translation->fileHandle && (translation->fileHandle != INVALID_HANDLE_VALUE) ) {
	if (translation->closeFileHandle) {
	    CloseHandle(translation->fileHandle);
	}
    }
    if ( translation->hVolume && (translation->hVolume != INVALID_HANDLE_VALUE) ) {
	CloseHandle(translation->hVolume);
    }
    /* BEWARE: By freeing here, the caller must NULL its' pointer to avoid reuse! */
    FreeMem(dip, translation, sizeof(*translation));
    return;
}

void
resetTranslation(TRANSLATION *translation)
{
    translation->inputVcn.StartingVcn.QuadPart = 0;
    return;
}

DWORD
getNextTranslation(
    dinfo_t *dip,
    TRANSLATION *translation,
    LONGLONG *fileOffset,
    LONGLONG *startSector,
    LONGLONG *nSectors)
{
    DWORD dwBytesReturned;
    DWORD error = NO_ERROR;

    /*
     * DeviceIoControl(
     *   (HANDLE) hDevice,              // handle to file, directory, or volume
     *   FSCTL_GET_RETRIEVAL_POINTERS,  // dwIoControlCode(LPVOID) lpInBuffer,
     *   (DWORD) nInBufferSize,         // size of input buffer
     *   (LPVOID) lpOutBuffer,          // output buffer
     *   (DWORD) nOutBufferSize,        // size of output buffer
     *   (LPDWORD) lpBytesReturned,     // number of bytes returned
     *   (LPOVERLAPPED) lpOverlapped ); // OVERLAPPED structure
     */
    BOOL result = DeviceIoControl(translation->fileHandle,
				  FSCTL_GET_RETRIEVAL_POINTERS,
				  &translation->inputVcn,
				  sizeof(STARTING_VCN_INPUT_BUFFER),
				  &translation->rpBuf,
				  sizeof(RETRIEVAL_POINTERS_BUFFER),
				  &dwBytesReturned,
				  NULL);
    error = GetLastError();

    switch (error) {

	case ERROR_HANDLE_EOF:
	    /* Indicates where are no more records, thus EOF! */
	    break;

	case ERROR_MORE_DATA:
	    translation->inputVcn.StartingVcn = translation->rpBuf.Extents[0].NextVcn;
	    /* Fall through... */
	case NO_ERROR: {
	    /*
	     * This has to be scaled by the cluster factor and offset by the volume extent 
	     * starting offset, and everything normalized to sectors.
	     */
	    LONGLONG lengthInClusters = (translation->rpBuf.Extents[0].NextVcn.QuadPart - translation->rpBuf.StartingVcn.QuadPart);
	    /*
	     * typedef struct _VOLUME_LOGICAL_OFFSET {
	     *   LONGLONG LogicalOffset;
	     * } VOLUME_LOGICAL_OFFSET, *PVOLUME_LOGICAL_OFFSET;
	     */
	    VOLUME_LOGICAL_OFFSET logicalOffset;
	    struct {
		/* 
		 * typedef struct _VOLUME_PHYSICAL_OFFSETS {
		 *   ULONG                  NumberOfPhysicalOffsets;
		 *   VOLUME_PHYSICAL_OFFSET PhysicalOffset[ANYSIZE_ARRAY];
		 * } VOLUME_PHYSICAL_OFFSETS, *PVOLUME_PHYSICAL_OFFSETS;
		 */
		VOLUME_PHYSICAL_OFFSETS physical;
		/*
		 * typedef struct _VOLUME_PHYSICAL_OFFSET {
		 *   ULONG    DiskNumber;
		 *   LONGLONG Offset;
		 * } VOLUME_PHYSICAL_OFFSET, *PVOLUME_PHYSICAL_OFFSET;
		 */
		VOLUME_PHYSICAL_OFFSET plex2;
	    } outputBuffer;

	    logicalOffset.LogicalOffset = (translation->rpBuf.Extents[0].Lcn.QuadPart * translation->volumeData.BytesPerCluster);

	    result = DeviceIoControl(translation->hVolume,
				     IOCTL_VOLUME_LOGICAL_TO_PHYSICAL,
				     &logicalOffset,
				     sizeof(VOLUME_LOGICAL_OFFSET),
				     &outputBuffer,
				     sizeof(outputBuffer),
				     &dwBytesReturned,
				     NULL);
	    if (result == False) {
		error = GetLastError();
		break;
	    }

	    *startSector = (outputBuffer.physical.PhysicalOffset[0].Offset / translation->volumeData.BytesPerSector);
	    *startSector += translation->clusterStart;

	    *nSectors = (lengthInClusters * translation->volumeData.BytesPerCluster) / translation->volumeData.BytesPerSector;
	    *fileOffset = (translation->rpBuf.StartingVcn.QuadPart * translation->volumeData.BytesPerCluster);

	    if (translation->verify) {
		BOOL result = validateTranslation(dip, translation, fileOffset, startSector);
		if (result == False) {
		    error = ERROR_INVALID_DATA;
		}
	    }
	    break;
	}
	default: {
	    /* Note: This occurs with sparse files since data is not mapped! */
	    if (dip->di_fDebugFlag) {
		os_perror(dip, "FSCTL_GET_RETRIEVAL_POINTERS failed");
	    }
	    break;
	}
    } /* end of switch() */
    return(error);
}

/*
 * Pass in a file offset (byte offset in file), and the length in bytes
 * of the data at that offset, and get back the physical LBA (sector offset)
 * and byte length of this on disk run.
 *
 * Returns true if the operation succeeded, false otherwise.
 * If runLength < recordLength, this record is split across more than one physical run.
 * runLength should be added to fileOffset, and subtracted from recordLength, and
 * getLBAandLengthByOffset() should be called again to get the next LBA.
 * RecordLength mod BytesPerSector must be zero!
 */
BOOL
getLBAandLengthByOffset(
    dinfo_t *dip,
    TRANSLATION *translation,
    LONGLONG fileOffset,
    LONGLONG recordLength,
    LONGLONG *startSectorLBA,
    LONGLONG *runLength)
{
    BOOL result = True;
    BOOL foundRun = False;
    DWORD error = NO_ERROR;
    LONGLONG currentRunOffset = 0;
    LONGLONG nextRunOffset = 0;
    LONGLONG startSector = 0;
    LONGLONG nSectors = 0;

    resetTranslation(translation);
    /* 
     * Loop until we find translation or we fail.
     */
    while (foundRun == False) {

        error = getNextTranslation(dip, translation,
				   &currentRunOffset, &startSector, &nSectors);

        switch (error) {

	    case ERROR_HANDLE_EOF:
		result = False;
		break;

	    case NO_ERROR:
	    case ERROR_MORE_DATA: {
		/*
		 * Compute the record LBA and length.
		 */
		LONGLONG newRunOffset = currentRunOffset + (nSectors * translation->volumeData.BytesPerSector);
		if (newRunOffset <= nextRunOffset) {
		    /*
		     * Assume the file offset was invalid.
		     */
		    result = False;
		    SetLastError(ERROR_INVALID_PARAMETER);
		    break;
		}
		nextRunOffset = newRunOffset;

		if ( (fileOffset >= currentRunOffset) && (fileOffset < nextRunOffset) ) {
		    /*
		     * Found the file offset in this cluster offset range.
		     */
		    foundRun = True;
		    /*
		     * Calculate the record offset within the current range.
		     */
		    LONGLONG recordOffset = (fileOffset - currentRunOffset);
		    LONGLONG sectorOffset = (recordOffset / translation->volumeData.BytesPerSector);

		    //Printf(dip, "getLBAandLengthByOffset: File Offset "LUF", Record Offset "LUF", Sector Offset "LUF", currentRunOffset "LUF"\n",
		    //	   fileOffset, recordOffset, sectorOffset, currentRunOffset);

		    *startSectorLBA = (startSector + sectorOffset);
		    nSectors -= sectorOffset;
		    *runLength = (nSectors * translation->volumeData.BytesPerSector);
		    if (*runLength > recordLength) {
			*runLength = recordLength;
		    }

		    if (translation->verify) {
			result = validateTranslation(dip, translation, &fileOffset, startSectorLBA);
			if (result == False) {
			    error = ERROR_INVALID_DATA;
			}
		    }
		}
		break;
	    }
	    default: {
		result = False;
		break;
	    }
	} /* end of switch() */
        if (result == False) {
            break;
        }
        currentRunOffset = nextRunOffset;

    } /* end of while(foundRun == False) */

    return(result);
}

/*
 * This reads one cluster of data from both the file and disk, using the file offset,
 * and the physical disk, using the startSector, and compares them.
 */
BOOL
validateTranslation(
    dinfo_t *dip,
    TRANSLATION *translation,
    LONGLONG *fileOffset,
    LONGLONG *startSector)
{
    char physicalDisk[MAX_PATH];

    _snprintf(physicalDisk, sizeof(physicalDisk), "\\\\.\\PhysicalDrive%d",
	      translation->volumeExtents.volExtents.Extents[0].DiskNumber);

    if (dip->di_fDebugFlag) {
	Printf(dip, "validateTranslation: Physical disk is %s\n", physicalDisk);
    }
    HANDLE hDisk = CreateFile(physicalDisk,
			      GENERIC_READ,
			      (FILE_SHARE_READ | FILE_SHARE_WRITE),
			      NULL, OPEN_EXISTING,
			      FILE_ATTRIBUTE_NORMAL, NULL);

    if (hDisk == INVALID_HANDLE_VALUE) {
	if (dip->di_fDebugFlag) {
	    os_perror(dip, "CreateFile()/GENERIC_READ failed on %s", physicalDisk);
	}
        return False;
    }

    LARGE_INTEGER offset;
    BOOL result = True;
    DWORD bytesRead;

    char *physicalBuffer = Malloc(dip, translation->volumeData.BytesPerCluster);
    char *fileBuffer = Malloc(dip, translation->volumeData.BytesPerCluster);

    if ( (physicalBuffer == NULL) || (fileBuffer == NULL) ) {
        result = False;
    }

    /* Note: I am NOT a fan of this style of error handling! */
    do {
        if (result == False) {
            break;
        }

        offset.QuadPart = (*startSector * translation->volumeData.BytesPerSector);
        result = SetFilePointerEx(hDisk, offset, NULL, FILE_BEGIN);
        if (result == False) {
	    if (dip->di_fDebugFlag) {
		os_perror(dip, "SetFilePointerEx() at offset "FUF" failed on %s", offset, physicalDisk);
	    }
            break;
        }

        result = ReadFile(hDisk, physicalBuffer,
			  translation->volumeData.BytesPerCluster,
			  &bytesRead, NULL);
        if (result == False) {
	    if (dip->di_fDebugFlag) {
		os_perror(dip, "ReadFile() failed on %s", physicalDisk);
	    }
            break;
        }

        offset.QuadPart = *fileOffset;
        result = SetFilePointerEx(translation->fileHandle, offset, NULL, FILE_BEGIN);
        if (result == False) {
	    if (dip->di_fDebugFlag) {
		os_perror(dip, "SetFilePointerEx() to offset "FUF" failed on %s", offset, physicalDisk);
	    }
            break;
        }

        result = ReadFile(translation->fileHandle, fileBuffer,
			  translation->volumeData.BytesPerCluster,
			  &bytesRead, NULL);
        if (result == False) {
	    if (dip->di_fDebugFlag) {
		os_perror(dip, "ReadFile() failed on %s\n", physicalDisk);
	    }
            break;
        }

	/* Verify if this is the cluster information we're looking for! */
        if (memcmp(physicalBuffer, fileBuffer, translation->volumeData.BytesPerCluster) != 0) {
            result = False;
	    break;
        }
	/* Success, the data matched! */
    } while(0);

    if (physicalBuffer) {
        Free(dip, physicalBuffer);
    }
    if (fileBuffer) {
        Free(dip, fileBuffer);
    }
    (void)CloseHandle(hDisk);
    if (dip->di_fDebugFlag) {
	Printf(dip, "validateTranslation: result %d\n", result);
    }
    return result;
}

/* --------------------------------------------------------------------------------------------------------- */

int
get_fs_info(dinfo_t *dip, char *filename, HANDLE fileHandle, DWORD64 offset, DWORD64 fsize)
{
    DWORD error = NO_ERROR;
    TRANSLATION *translation;

    translation = initFileTranslation(dip, filename, fileHandle, False);
    if (translation == NULL) {
	return(FAILURE);
    }

#if 0
    char physicalDisk[MAX_PATH];
    _snprintf(physicalDisk, sizeof(physicalDisk), "\\\\.\\PhysicalDrive%d",
	      translation->volumeExtents.volExtents.Extents[0].DiskNumber);
    if (dip->di_fDebugFlag) {
	Printf(dip, "Physical disk is %s\n", physicalDisk);
    }
#endif /* 0 */

    LONGLONG fileOffset = offset;
    LONGLONG recordLength = fsize;

    /* Loop on record length to report all LBA's in the range specified. */
    while (recordLength > 0) {
        LONGLONG startSectorLBA = 0;
        LONGLONG runLength = 0;

        BOOL result = getLBAandLengthByOffset(dip, translation,
					      fileOffset, recordLength,
					      &startSectorLBA, &runLength);
        if (result == False) {
            break;
        }

        Printf(dip, "File Offset: "FUF", Unit LBA "LDF" ("LXF"), VCN %d, LCN %d [cluster size: %d] on %s [%s]\n",
	       fileOffset,
	       startSectorLBA, startSectorLBA, translation->rpBuf.StartingVcn,
	       translation->rpBuf.Extents[0].Lcn, translation->volumeData.BytesPerCluster,
	       translation->fullVolName, translation->fileSystemName);

	/* Adjust to handle records split across more than one physical run. */
	fileOffset += runLength;
	recordLength -= runLength;
    }
    //closeTranslation(dip, translation);
    //translation = NULL;
    return(SUCCESS);
}

/* --------------------------------------------------------------------------------------------------------- */

/*
 * Note: We are using the translation record as our file system map.
 */
void *
os_get_file_map(dinfo_t *dip, HANDLE fd)
{
    /* Caller must free file map to force repopulating! */
    if (dip->di_fsmap == NULL) {
	TRANSLATION *translation;
	char *filename = dip->di_dname;
	HANDLE fileHandle = fd;

	translation = initFileTranslation(dip, filename, fileHandle, False);
	if (translation) {
	    dip->di_fsmap = translation;
	}
    }
    return(dip->di_fsmap);
}

void
os_free_file_map(dinfo_t *dip)
{
    if (dip->di_fsmap) {
	closeTranslation(dip, (TRANSLATION *)dip->di_fsmap);
	dip->di_fsmap = NULL;
    }
    return;
}

int
os_report_file_map(dinfo_t *dip, HANDLE fd, uint32_t dsize, Offset_t offset, int64_t length)
{
    TRANSLATION *translation;
    LONGLONG fileOffset = (offset == NO_OFFSET) ? 0 : offset;
    LONGLONG recordLength = length;
    LONGLONG startSectorLBA = 0;
    LONGLONG runLength = 0;
    BOOL firstTime = True;
    BOOL result;

    if ( (translation = (TRANSLATION *)os_get_file_map(dip, fd)) == NULL) {
	return(FAILURE);
    }

    do {
	result = getLBAandLengthByOffset(dip, translation,
					 fileOffset, recordLength,
					 &startSectorLBA, &runLength);
	/* TODO: Handle sparse files! */
	/* dt.exe (j:1 t:1): ERROR: FSCTL_GET_RETRIEVAL_POINTERS failed, error = 87 - The parameter is incorrect. */
	if (result == False) {
	    break;
	}
	LONGLONG startLBAOffset = (startSectorLBA * translation->volumeData.BytesPerSector);
	LONGLONG starting_lba = (startLBAOffset / dsize);
	LONGLONG ending_lba = (starting_lba + (runLength / dsize));
	LONGLONG totalBlocks = (runLength / dsize);

	if (firstTime) {
	    firstTime = False;
	    /*
	     * Example: 
	     *  
	     * File: dt.data, LBA Size: 512, Cluster Size: 4096 on \\.\C: [NTFS]
	     *     File Offset    Start LBA      End LBA     Blocks      VCN        LCN
	     *               0     27827936     27828064        128        0    3189724
	     *           65536     23595232     23595360        128       16    2660636
	     *          131072      9206144      9206400        256       32     862000
	     *          262144    520802432    520803968       1536       64   64811536
	     */
	    char physicalDisk[MAX_PATH];
	    _snprintf(physicalDisk, sizeof(physicalDisk), "\\\\.\\PhysicalDrive%d",
		      translation->volumeExtents.volExtents.Extents[0].DiskNumber);
	    Printf(dip, "File: %s, LBA Size: %u bytes\n", dip->di_dname, dsize);
	    Printf(dip, "Physical Disk: %s, Cluster Size: %d on %s [%s]\n",
		   physicalDisk,
		   translation->volumeData.BytesPerCluster,
		   translation->fullVolName, translation->fileSystemName);
	    Printf(dip, "\n");
	    Printf(dip, "%14s %12s %12s %10s %8s %10s\n",
		   "File Offset", "Start LBA", "End LBA", "Blocks", "VCN", "LCN");
	}

	Printf(dip, "%14llu %12llu %12llu %10llu %8llu %10llu\n",
	       fileOffset, starting_lba, ending_lba, totalBlocks,
	       translation->rpBuf.StartingVcn, translation->rpBuf.Extents[0].Lcn);

	fileOffset += runLength;
	recordLength -= runLength;
    } while (recordLength > 0);

    if (recordLength > 0) {
	Wprintf(dip, "File offset "FUF" was NOT found, possible sparse file!\n", fileOffset);
	Printf(dip, "Therefore, file offset maps for "LDF" bytes were NOT reported!\n", recordLength);
    }
    return ((result == True) ? SUCCESS : FAILURE);
}

uint64_t
os_map_offset_to_lba(dinfo_t *dip, HANDLE fd, uint32_t dsize, Offset_t offset)
{
    TRANSLATION *translation;
    LONGLONG fileOffset = offset;
    LONGLONG recordLength = dsize;
    LONGLONG startSectorLBA = 0;
    LONGLONG runLength = 0;
    uint64_t lba = NO_LBA;
    BOOL result = NO_ERROR;

    if ( (translation = (TRANSLATION *)os_get_file_map(dip, fd)) == NULL) {
	return(lba);
    }

    result = getLBAandLengthByOffset(dip, translation,
				     fileOffset, recordLength,
				     &startSectorLBA, &runLength);
    if (result == True) {
	LONGLONG startLBAOffset = (startSectorLBA * translation->volumeData.BytesPerSector);
	lba = (startLBAOffset / dsize);
	if (dip->di_fDebugFlag) {
	    Printf(dip, "File Offset: "FUF", Physical LBA "LDF" ("LXF"), VCN %d, LCN %d [cluster size: %d] on %s [%s]\n",
		   fileOffset,
		   startSectorLBA, startSectorLBA, translation->rpBuf.StartingVcn,
		   translation->rpBuf.Extents[0].Lcn, translation->volumeData.BytesPerCluster,
		   translation->fullVolName, translation->fileSystemName);
	}
    }
    return(lba);
}
