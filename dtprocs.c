/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2020			    *
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
 * Module:	dtprocs.c
 * Author:	Robin T. Miller
 * Date:	August 7, 1993
 *
 * Description:
 *	Functions to handle multiple processes for 'dt' program.
 *
 * Modification History:
 * 
 * September 5th, 2020 by Robin T. Miller
 *      When initializing a slice, ensure the min/max limit sizes do NOT
 * exceed the data/slice limits. Otherwise, overwrites occur and thus cause
 * false corruptions!
 * 
 * March 7th, 2015 by Robin T. Miller
 * 	When using slices, always set the random data limit, since variable
 * options such as iodir=vary and/or iotype=vary may be specified, plus it
 * does no harm to set this if random I/O options are not specified.
 *
 * July 16th, 2014 by Robin T. Miller
 * 	In setup_slice(), when the step option is specified, setup the end
 * position, which I/O functions use to avoid overwriting the slice length.
 * Since code has been rearranged, the value setup in do_common_device_setup()
 * is incorrect, since its' called before init_slice. Sadly, this means folks
 * using the step option with slices could encounter a false data corruption,
 * since we were writing past the end of the slice! ;( (oh my, dt v19 bug!)
 * 
 * June 20th, 2013 by Robin T Miller
 * 	Mostly a rewrite for multithreaded IO, so starting with new history!
 */
#include "dt.h"
#if !defined(WIN32)
#  include <signal.h>
#  include <strings.h>
#  include <sys/stat.h>
#  include <sys/wait.h>
#endif /* !defined(WIN32) */

#define PROC_ALLOC (sizeof(pid_t) * 3)	/* Extra allocation for PID.	*/

/*
 * Forward References:
 */
static int init_slice_info(struct dinfo *dip, slice_info_t *sip, large_t *data_resid);
static void setup_slice(struct dinfo *dip, slice_info_t *sip);
static void setup_iotuning(dinfo_t *dip, char *cmd);
static void setup_multiprocs(dinfo_t *dip, char *cmd);
static void update_cmd_line(dinfo_t *dip);

#if defined(WIN32)

#define HANDLE_MASK (MAXIMUM_WAIT_OBJECTS - 1)

PROCESS_INFORMATION *start_process(dinfo_t *dip, char *cmd);

/*
 * abort_procs - Abort processes started by the parent.
 */
void
abort_procs(dinfo_t *dip)
{
    int proc_num;
    struct dt_procs *dtp;
    PROCESS_INFORMATION *pip;

    if ((dip->di_ptable == NULL) || dip->di_aborted_processes) return;
    /*
     * Terminate all active processes.
     */
    for (dtp = dip->di_ptable, proc_num = 0; proc_num < dip->di_max_procs; proc_num++, dtp++) {
        pip = dtp->dt_pip;
	if ( !dtp->dt_active ) continue; /* Already reaped status! */
	if (dip->di_debug_flag || dip->di_pDebugFlag) {
	    Printf(dip, "Terminating child process %d...\n", pip->dwProcessId);
	}

	/*
	 * I'd rather send a signal like Unix, to have child report
	 * statistics, but Windows does not support a kill() API!
	 */
	if (TerminateProcess(pip->hProcess, ERROR_PROCESS_ABORTED) == 0) {
	    DWORD error = GetLastError();
	    /*
	     * Child thread may be terminating due to signal.
	     */
	    if (error != ERROR_ACCESS_DENIED) {
		Fprintf(dip, "Failed to terminate child process %d...\n", pip->dwProcessId);
		ReportErrorInfo(dip, NULL, error, "TerminateProcess() failed", OTHER_OP, True);
	   }
	}
    }
    dip->di_aborted_processes = True;
    return;
}

void
await_procs(dinfo_t *dip)
{
    DWORD status;
    struct dt_procs *dtp;
    int proc_num = 0, wait_index;
    PROCESS_INFORMATION *pip;
    bool aborted_procs = False;
    DWORD timeoutMs = INFINITE;

    if (dip->di_debug_flag || dip->di_pDebugFlag) {
	timeoutMs = (60 * 1000); /* For debug! */
	Printf(dip, "Waiting for %d child processes to complete...\n", dip->di_procs_active);
    }
    /*
     * For dt's oncerror option, we must wait on one process at a time,
     * so we can abort procs if instructed to on child errors.
     */
    while (dip->di_procs_active) {
	/*
	 * Wait for all processes to complete.
	 */
	status = WaitForMultipleObjects((DWORD)dip->di_procs_active, // Number of handles.
					dip->di_proc_handles,// Array of handles.
					False,		     // Wait for any object.
					timeoutMs);          // Timeout in ms (disable).

	if (status == WAIT_FAILED) {
	    ReportErrorInfo(dip, NULL, os_get_error(), "WaitForMultipleObjects() failed", OTHER_OP, False);
	    abort_procs(dip);
	    break;
	}
	if (status == WAIT_TIMEOUT) {
	    Printf(dip, "Still waiting on %d processes...\n", dip->di_procs_active);
	    Printf(dip, "Active PIDs:");
	    for (dtp = dip->di_ptable, proc_num = 0; proc_num < dip->di_max_procs; proc_num++, dtp++) {
		pip = dtp->dt_pip;
		if (dtp->dt_active) {
		    Print(dip, " %d", pip->dwProcessId);
		}
	    }
	    Print(dip, "\n");
	    fflush(ofp);
	    continue;
	}
	if ((status & ~HANDLE_MASK) != WAIT_OBJECT_0) {
	    Fprintf(dip, "WaitForMultipleObjects returned status %#x\n", status);
	    dip->di_child_status = GetLastError();
	    abort_procs(dip);
	    return;
	}
	wait_index = (status & HANDLE_MASK); /* Handle index - 1 */
	/*
	 * Find the process by searching for its' handle.
	 */
	for (dtp = dip->di_ptable, proc_num = 0; proc_num < dip->di_max_procs; proc_num++, dtp++) {
	    pip = dtp->dt_pip;
	    if (pip->hProcess == dip->di_proc_handles[wait_index]) {
		break;
	    }
	}
	if ( !GetExitCodeProcess( pip->hProcess, &dip->di_child_status ) ) {
	    Fprintf(dip, "GetExitCodeProcess failed (%d)\n", GetLastError() );
	    dip->di_child_status = GetLastError();
	}
	if (dip->di_debug_flag || dip->di_pDebugFlag) {
	    Printf(dip, "Child process %d, exited with status %d\n",
		    pip->dwProcessId, dip->di_child_status);
	}
	dtp->dt_active = False;
	dtp->dt_status = dip->di_child_status;
        /*
	 * Close process and thread handles.
	 */
	CloseHandle( pip->hProcess );
	CloseHandle( pip->hThread );
	/*
	 * Remove the completed handle from the array to wait upon.
	 */
	for (; (wait_index < dip->di_procs_active); wait_index++) {
	    dip->di_proc_handles[wait_index] = dip->di_proc_handles[wait_index + 1];
	}

	if ( (exit_status == SUCCESS) && (dip->di_child_status != SUCCESS) ) {
	    if ( (dip->di_oncerr_action == ONERR_ABORT) &&
		 (dip->di_child_status != WARNING) && (dip->di_child_status != END_OF_FILE) ) {
		if (!aborted_procs) {
		    abort_procs(dip);		/* Abort procs on error. */
		    aborted_procs = True;
		}
	    }
	    /*
	    * Save the most sever error for parent exit status.
	    *
	    * Severity Priorities:
	    *			WARNING		(lowest)
	    *			END_OF_FILE
	    *			Signal Number
	    *			FATAL_ERROR	(highest)
	    */
	    if ( ((exit_status == SUCCESS) || (dip->di_child_status == FATAL_ERROR)) ||
		 ((exit_status == WARNING) && (dip->di_child_status > WARNING))      ||
		 ((exit_status == END_OF_FILE) && (dip->di_child_status > WARNING)) ) {
		    exit_status = dip->di_child_status;	/* Set error code for exit. */
	    }
	}
	dip->di_procs_active--;
    } 
    return;
}

/*
 * start_process() - Start a Process.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	cmd = The command line to execute.
 *
 * Return Value:
 *	Returns pointer to process information or NULL on failure.
 */
PROCESS_INFORMATION *
start_process(dinfo_t *dip, char *cmd)
{
    STARTUPINFO si;
    PROCESS_INFORMATION *pip;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    pip = (PROCESS_INFORMATION *)Malloc(dip, sizeof(*pip));
    if (pip == NULL) return(pip);
    ZeroMemory( pip, sizeof(*pip) );

    if (dip->di_debug_flag || dip->di_pDebugFlag) {
	    Printf(dip, "Command: %s\n", cmd);
    }

    /* Start the child process. */
    if ( !CreateProcess( NULL,	     // No module name (use command line)
		         cmd,        // Command line
		         NULL,       // Process handle not inheritable
		         NULL,       // Thread handle not inheritable
		         False,      // Handle inheritance flag
		         0,          // No creation flags
		         NULL,       // Use parent's environment block
		         NULL,       // Use parent's starting directory 
		         &si,        // Pointer to STARTUPINFO structure
			 pip ) )     // Pointer to PROCESS_INFORMATION structure
    {
        ReportErrorInfo(dip, NULL, os_get_error(), "CreateProcess failed", OTHER_OP, True);
        Free(dip, pip);
        return(NULL);
    }
    return(pip);
}

static void
setup_iotuning(dinfo_t *dip, char *cmd)
{
    if (dip->di_iotuning_flag && dip->di_iotune_file ) {
	//(void)strcat(cmd, " disable=iotuning");
	;
    }
    return;
}

static void
setup_multiprocs(dinfo_t *dip, char *cmd)
{
    return;
}

pid_t
start_devs(dinfo_t *dip)
{
    struct dt_procs *dtp;
    size_t psize;
    PROCESS_INFORMATION *pip;
    char *devs, *token, *p;
    int proc_num = 0;
    char *dt_path, *dt_cmd;
    char *our_cmd, *bp, *saveptr;

    devs = p = (dip->di_input_file) ? dip->di_input_file : dip->di_output_file;
    dip->di_num_devs = 1;
    /* Count the devices specified. */
    while (p = strchr(p, ',')) {
	dip->di_num_devs++; p++;
    }
    dip->di_max_procs = dip->di_num_devs;
    psize = (dip->di_max_procs * sizeof(*dtp));

    if ((dip->di_ptable = (struct dt_procs *)Malloc(dip, psize)) == NULL) {
	ReportErrorInfo(dip, NULL, os_get_error(), "No memory for proc table", OTHER_OP, False);
	exit (FATAL_ERROR);
    }
    ZeroMemory(dip->di_ptable, psize);
    our_cmd = (char *)Malloc(dip, LOG_BUFSIZE);
    /* Setup pointers to dt path and dt command line. */
    dt_path = dip->di_dtcmd;
    p = strchr(dt_path, ' ');
    if (!p) {
	Fprintf(dip, "We're broken setting up dt's path!\n");
	abort();
    }
    *p = '\0';
    dt_cmd = ++p;

    dip->di_cur_proc = 1;
    dip->di_procs_active = 0;

    /* Remember: strtok() replaces "," with NULL! */
    token = strtok_r(devs, ",", &saveptr);
    /*
     * Start a process for each target device specified.
     */
    for (dtp = dip->di_ptable, proc_num = 0; proc_num < dip->di_max_procs; proc_num++, dtp++) {
	dtp->dt_device = token;
	bp = our_cmd;
	/* Note: Main stripped the device options already. */
	bp += Sprintf(bp, "%s ", dt_path);
	/* More work, but I like the device first on the command line! */
	if (dip->di_input_file) {
	    bp += Sprintf(bp, "if=%s", token);
	} else {
	    bp += Sprintf(bp, "of=%s", token);
	}
	/* Now, add original options. */
	bp += Sprintf(bp, " %s enable=child", dt_cmd);
	setup_iotuning(dip, bp);
	pip = start_process(dip, our_cmd);
	if (pip == NULL) return (-1);
	if (dip->di_debug_flag || dip->di_pDebugFlag) {
	    Printf(dip, "Started process %d for %s...\n", pip->dwProcessId, dtp->dt_device);
	}
	dip->di_proc_handles[proc_num] = pip->hProcess;
	dip->di_cur_proc++;
	dtp->dt_pid = dip->di_child_pid = pip->dwProcessId;
	dtp->dt_active = True;
	dtp->dt_pip = pip;
	dip->di_procs_active++;
	token = strtok_r(NULL, ",", &saveptr);	/* Next device please! */
    }
    Free(dip, our_cmd);
    return (dip->di_child_pid);
}

pid_t
start_procs(dinfo_t *dip)
{
    struct dt_procs *dtp;
    size_t psize;
    PROCESS_INFORMATION *pip;
    int proc_num = 0;

    dip->di_max_procs = dip->di_num_procs;
    psize = (dip->di_max_procs * sizeof(*dtp));

    if ((dip->di_ptable = (struct dt_procs *)Malloc(dip, psize)) == NULL) {
	ReportErrorInfo (dip, NULL, os_get_error(), "No memory for proc table", OTHER_OP, False);
	exit (FATAL_ERROR);
    }
    ZeroMemory(dip->di_ptable, psize);

    dip->di_cur_proc = 1;
    dip->di_procs_active = 0;
    (void)strcat(dip->di_dtcmd, " enable=child");
    setup_iotuning(dip, dip->di_dtcmd);
    setup_multiprocs(dip, dip->di_dtcmd);

    /*
     * Spawn specified number of processes.
     */
    for (dtp = dip->di_ptable, proc_num = 0; proc_num < dip->di_max_procs; proc_num++, dtp++) {
	pip = start_process(dip, dip->di_dtcmd);
	if (pip == NULL) return (-1);
	if (dip->di_debug_flag || dip->di_pDebugFlag) {
	    Printf(dip, "Started process %d...\n", pip->dwProcessId);
	}
	dip->di_proc_handles[proc_num] = pip->hProcess;
	dip->di_cur_proc++;
	dtp->dt_pid = dip->di_child_pid = pip->dwProcessId;
	dtp->dt_active = True;
	dtp->dt_pip = pip;
	dip->di_procs_active++;
    }
    return (dip->di_child_pid);
}

pid_t
start_slices(dinfo_t *dip)
{
    struct slice_info slice_info;
    slice_info_t *sip = &slice_info;
    struct dt_procs *dtp;
    size_t psize;
    PROCESS_INFORMATION *pip;
    int proc_num = 0;
    large_t data_resid;
    char *cmd = Malloc(dip, LOG_BUFSIZE);

    dip->di_max_procs = dip->di_slices;
    psize = (dip->di_max_procs * sizeof(*dtp));

    if ((dip->di_ptable = (struct dt_procs *)Malloc(dip, psize)) == NULL) {
	ReportErrorInfo (dip, NULL, os_get_error(), "No memory for proc table", OTHER_OP, False);
	exit (FATAL_ERROR);
    }
    memset(dip->di_ptable, '\0', psize);

    init_slice_info(dip, sip, &data_resid);

    dip->di_cur_proc = 1;
    dip->di_procs_active = 0;

    /*
     * Spawn specified number of processes.
     */
    for (dtp = dip->di_ptable, proc_num = 0; proc_num < dip->di_max_procs; proc_num++, dtp++) {
	sip->slice++;
	/* Last slice gets any residual bytes. */
	if ((proc_num + 1) == dip->di_max_procs) {
	    sip->slice_length += data_resid;
	}
	(void)sprintf(cmd, "%s enable=logpid slice=%d", dip->di_dtcmd, sip->slice);
	setup_iotuning(dip, cmd);
	setup_multiprocs(dip, cmd);
	pip = start_process(dip, cmd);
	if (pip == NULL) return (-1);
	dip->di_proc_handles[proc_num] = pip->hProcess;
	dip->di_cur_proc++;
	dtp->dt_pid = dip->di_child_pid = pip->dwProcessId;
	dtp->dt_active = True;
	dtp->dt_pip = pip;
	dip->di_procs_active++;
	if (dip->di_debug_flag || dip->di_pDebugFlag) {
	    Printf (dip, "Started Slice %d, PID %d...\n", sip->slice, dip->di_child_pid);
	}
	if (proc_num < dip->di_max_procs) {
	    sip->slice_position += sip->slice_length;
	}
    }
    return (dip->di_child_pid);
}


#else /* !defined(WIN32) */

/*
 * abort_procs - Abort processes started by the parent.
 */
void
abort_procs(dinfo_t *dip)
{
    struct dt_procs *dtp;
    int procs;
    pid_t pid;

    if ((dip->di_ptable == NULL) || dip->di_aborted_processes)  return;
    /*
     * Force all processes to terminate.
     */
    for (dtp = dip->di_ptable, procs=0; procs < dip->di_max_procs; procs++, dtp++) {
	if ((pid = dtp->dt_pid) == (pid_t) 0) continue;
	if (dip->di_debug_flag || dip->di_pDebugFlag) {
	    Printf(dip, "Aborting child process %d via a SIGINT (signal %d)...\n",
								pid, SIGINT);
	}
	if (dtp->dt_active) {
	    int status = kill (pid, SIGINT);
    	    if ( (dip->di_debug_flag || dip->di_pDebugFlag) && (status == FAILURE) ) {
		Perror(dip, "DEBUG: Failed to kill PID %d", pid);
	    }
	}
    }
    dip->di_aborted_processes = True;
    return;
}

void
await_procs(dinfo_t *dip)
{
    pid_t wpid;
    struct dt_procs *dtp;
    int procs, status;

    if (dip->di_debug_flag || dip->di_pDebugFlag) {
	Printf (dip, "Waiting for %d child processes to complete...\n", dip->di_procs_active);
    }
    while (1) {
	if ((wpid = waitpid ((pid_t) -1, &dip->di_child_status, 0)) == FAILURE) {
	    if (errno == ECHILD) {
		if (dip->di_procs_active && dip->di_pDebugFlag) {
		    Printf(dip, "Processes still active (%d) and ECHILD received!\n",
			   dip->di_procs_active);
		}
		break;				/* No more children... */
	    } else if (errno == EINTR) {
		/* Note: Expect signal handler to abort children! */
		//abort_procs(dip);
		continue;
	    } else {
		ReportErrorInfo (dip, NULL, os_get_error(), "waitpid", OTHER_OP, False);
		exit (FATAL_ERROR);
	    }
	}
	/*
	 * Examine the child process status.
	 */
	if ( WIFSTOPPED(dip->di_child_status) ) {
	    Printf (dip, "Child process %d, stopped by signal %d.\n",
					wpid, WSTOPSIG(dip->di_child_status));
	    continue; /* Maybe attached from debugger... */
	} else if ( WIFSIGNALED(dip->di_child_status) ) {
	    status = WTERMSIG(dip->di_child_status);
	    Fprintf (dip, "Child process %d, exiting because of signal %d\n",
							wpid, status);
	} else { /* Process must be exiting... */
	    status = WEXITSTATUS (dip->di_child_status);
	    if (dip->di_debug_flag || dip->di_pDebugFlag) {
		Printf (dip, "Child process %d, exited with status %d\n",
							wpid, status);
	    }
	}

	/*
	 * Update this process' status.
	 */
	for (dtp = dip->di_ptable, procs = 0; procs < dip->di_max_procs; procs++, dtp++) {
	    if (dtp->dt_pid == wpid) {
		dtp->dt_active = False;
		dtp->dt_status = status;
		dip->di_procs_active--;
		break;
	    }
	}

	if ( (exit_status == SUCCESS) && (status != SUCCESS) ) {
	    if ( (dip->di_oncerr_action == ONERR_ABORT) &&
		 (status != WARNING) && (status != END_OF_FILE) ) {
		abort_procs(dip);		/* Abort procs on error. */
	    }
	    /*
	     * Save the most sever error for parent exit status.
	     *
	     * Severity Priorities:	WARNING		(lowest)
	     *				END_OF_FILE
	     *				Signal Number
	     *				FATAL_ERROR	(highest)
	     */
	    if ( ((exit_status == SUCCESS) || (status == FATAL_ERROR)) ||
		 ((exit_status == WARNING) && (status > WARNING))      ||
		 ((exit_status == END_OF_FILE) && (status > WARNING)) ) {
		exit_status = status;	/* Set error code for exit. */
	    }
	}
    } /* End of while(1)... */
}

pid_t
fork_process(dinfo_t *dip)
{
    pid_t pid;

    if ((pid = fork()) == (pid_t) -1) {
	if (errno == EAGAIN) {
	    if (dip->di_procs_active == 0) {
		LogMsg (dip, dip->di_efp, logLevelCrit, 0,
			"could NOT start any processes, please check your system...\n");
		exit (FATAL_ERROR);
	    } else {
		Printf (dip,
	"Warning: System imposed process limit reached, only %d procs started...\n",
								dip->di_procs_active);
	    }
	} else {
	    ReportErrorInfo (dip, NULL, os_get_error(), "fork", OTHER_OP, False);
	    abort_procs(dip);
	}
    }
    return (pid);
}

static void
update_cmd_line(dinfo_t *dip)
{
    char *device, *bp, *p;
    char *dt_path, *options, *buffer;
    
    bp = buffer = (char *)Malloc(dip, LOG_BUFSIZE);
    device = (dip->di_input_file) ? dip->di_input_file : dip->di_output_file;
    /* Setup pointers to dt path and dt command line. */
    dt_path = dip->di_dtcmd;
    p = strchr(dt_path, ' ');
    *p = '\0';
    options = ++p;
    /* Note: Main stripped the device options already. */
    bp += Sprintf(bp, "%s ", dt_path);
    if (dip->di_input_file) {
	bp += Sprintf(bp, "if=%s", device);
    } else {
	bp += Sprintf(bp, "of=%s", device);
    }
    /* Now, add original options. */
    bp += Sprintf(bp, " %s", options);
    if (dip->di_debug_flag || dip->di_pDebugFlag) {
	Printf(dip, "Command: %s\n", buffer);
    }
    if (dip->di_cmd_line) Free(dip, dip->di_cmd_line);
    dip->di_cmd_line = Malloc(dip, (strlen(buffer) + 1) );
    strcpy(dip->di_cmd_line, buffer);
    Free(dip, buffer);
    return;
}

static void
setup_iotuning(dinfo_t *dip, char *cmd)
{
    return;
}

static void
setup_multiprocs(dinfo_t *dip, char *cmd)
{
    return;
}

pid_t
start_devs(dinfo_t *dip)
{
    struct dt_procs *dtp;
    size_t psize;
    char *devs, *token, *p, *saveptr;
    int procs;

    devs = p = (dip->di_input_file) ? dip->di_input_file : dip->di_output_file;
    dip->di_num_devs = 1;
    /* Count the devices specified. */
    while (p = strchr(p, ',')) {
	dip->di_num_devs++; p++;
    }
    dip->di_max_procs = dip->di_num_devs;
    psize = (dip->di_max_procs * sizeof(*dtp));

    if ((dip->di_ptable = (struct dt_procs *)Malloc(dip, psize)) == NULL) {
	ReportErrorInfo(dip, NULL, os_get_error(), "No memory for proc table", OTHER_OP, False);
	exit (FATAL_ERROR);
    }
    memset(dip->di_ptable, 0, psize);
    dip->di_cur_proc = 1;
    dip->di_procs_active = 0;

    /* Remember: strtok() replaces "," with NULL! */
    token = strtok_r(devs, ",", &saveptr);
    /*
     * Start a process for each target device specified.
     */
    for (dtp = dip->di_ptable, procs = 0; procs < dip->di_max_procs; procs++, dtp++) {
	dtp->dt_device = token;
	if ((dip->di_child_pid = fork_process(dip)) == (pid_t) -1) {
	    break;
	} else if (dip->di_child_pid) {		/* Parent process gets the PID. */
	    dip->di_cur_proc++;
	    dtp->dt_pid = dip->di_child_pid;
	    dtp->dt_active = True;
	    dip->di_procs_active++;
	    if (dip->di_debug_flag || dip->di_pDebugFlag) {
		Printf (dip, "Started Process %d for %s...\n", dip->di_child_pid, dtp->dt_device);
	    }
	    token = strtok_r(NULL, ",", &saveptr); /* Next device please! */
	} else {			/* Child process... */
	    dip->di_logpid_flag = True;
	    dip->di_multiple_devs = False;
	    if (dip->di_input_file) {
		dip->di_input_file = dtp->dt_device;
	    } else {
		dip->di_output_file = dtp->dt_device;
	    }
	    dip->di_process_id = getpid();
	    if (dip->di_log_format) {
		dip->di_log_file = dip->di_log_format;	/* Original log file w/format strings. */
	    }
	    setup_iotuning(dip, NULL);	/* Setup IO tuning options. */
	    update_cmd_line(dip);	/* Update the command line (for logging). */
	    make_unique_log(dip);	/* Check & make a unique log. */
	    break;			/* Child process, continue... */
	}
    }
    return (dip->di_child_pid);
}

pid_t
start_procs(dinfo_t *dip)
{
    struct dt_procs *dtp;
    size_t psize;
    int procs;

    dip->di_max_procs = dip->di_num_procs;
    psize = (dip->di_max_procs * sizeof(*dtp));

    if ((dip->di_ptable = (struct dt_procs *)Malloc(dip, psize)) == NULL) {
	ReportErrorInfo (dip, NULL, os_get_error(), "No memory for proc table", OTHER_OP, False);
	exit (FATAL_ERROR);
    }
    memset(dip->di_ptable, '\0', psize);

    dip->di_cur_proc = 1;
    dip->di_procs_active = 0;

    for (dtp = dip->di_ptable, procs = 0; procs < dip->di_max_procs; procs++, dtp++) {
	if ((dip->di_child_pid = fork_process(dip)) == (pid_t) -1) {
	    break;
	} else if (dip->di_child_pid) {		/* Parent process gets the PID. */
	    dip->di_cur_proc++;
	    dtp->dt_pid = dip->di_child_pid;
	    dtp->dt_active = True;
	    dip->di_procs_active++;
	    if (dip->di_debug_flag || dip->di_pDebugFlag) {
		Printf (dip, "Started Process %d...\n", dip->di_child_pid);
	    }
	} else {			/* Child process... */
	    dip->di_process_id = getpid();
	    if (dip->di_output_file) {
		dip->di_unique_file = True;	/* Create unique file name. */
	    }
	    if (dip->di_log_format) {
		dip->di_log_file = dip->di_log_format;	/* Original log file w/format strings. */
	    }
	    setup_iotuning(dip, NULL);	/* Setup IO tuning options. */
	    setup_multiprocs(dip, NULL);/* Setup multiple process options. */
	    make_unique_log(dip);	/* Check & make a unique log. */
	    break;			/* Child process, continue... */
	}
    }
    return (dip->di_child_pid);
}

pid_t
start_slices(dinfo_t *dip)
{
    struct dt_procs *dtp;
    size_t psize;
    struct slice_info slice_info;
    slice_info_t *sip = &slice_info;
    large_t data_resid;
    int procs;

    dip->di_max_procs = dip->di_slices;
    psize = (dip->di_max_procs * sizeof(*dtp));

    if ((dip->di_ptable = (struct dt_procs *)Malloc(dip, psize)) == NULL) {
	ReportErrorInfo(dip, NULL, os_get_error(), "No memory for proc table", OTHER_OP, False);
	exit (FATAL_ERROR);
    }
    memset(dip->di_ptable, '\0', psize);

    init_slice_info(dip, sip, &data_resid);

    dip->di_cur_proc = 1;
    dip->di_procs_active = 0;

    for (dtp = dip->di_ptable, procs = 0; procs < dip->di_max_procs; procs++, dtp++) {
	sip->slice++;
	/* Last slice gets any residual bytes. */
	if ((procs + 1) == dip->di_max_procs) {
	    sip->slice_length += data_resid;
	}
	if ((dip->di_child_pid = fork_process(dip)) == (pid_t) -1) {
	    break;
	} else if (dip->di_child_pid) {		/* Parent process gets the PID. */
	    dip->di_cur_proc++;
	    dtp->dt_pid = dip->di_child_pid;
	    dtp->dt_active = True;
	    dip->di_procs_active++;
	    if (dip->di_debug_flag || dip->di_pDebugFlag) {
		Printf (dip, "Started Slice %d, PID %d...\n", sip->slice, dip->di_child_pid);
	    }
	    if (procs < dip->di_max_procs) {
		sip->slice_position += sip->slice_length;
	    }
	} else {			/* Child process... */
	    /*
	     * Initialize the starting data pattern for each slice.
	     */
	    dip->di_process_id = getpid();
	    if (dip->di_unique_pattern) {
		dip->di_pattern = data_patterns[(dip->di_cur_proc - 1) % npatterns];
	    }
	    if (dip->di_log_format) {
		dip->di_log_file = dip->di_log_format;	/* Original log file w/format strings. */
	    }
	    setup_iotuning(dip, NULL);	/* Setup IO tuning options. */
	    setup_multiprocs(dip, NULL);/* Setup multiple process options. */
	    make_unique_log(dip);	/* Check & make a unique log. */
	    setup_slice(dip, sip);
	    break;			/* Child process, continue... */
	}
    }
    return (dip->di_child_pid);
}
#endif /* !defined(WIN32) */

int
init_slice(struct dinfo *dip, int slice)
{
    struct slice_info slice_info;
    slice_info_t *sip = &slice_info;
    large_t data_resid;
    int status;

    status = init_slice_info(dip, sip, &data_resid);
    if (status == FAILURE) return(status);
    sip->slice_position += (sip->slice_length * (slice - 1));
    /*
     * Any residual goes to the last slice.
     */
    if (slice == dip->di_slices) {
	sip->slice_length += data_resid;
    }
    sip->slice = slice;
    setup_slice(dip, sip);
    /*
     * Initialize the starting data pattern for each slice.
     * 
     * TODO: Verify, but this is done in main I/O loop (I think).
     */
    if (dip->di_unique_pattern) {
	dip->di_pattern = data_patterns[(slice - 1) % npatterns];
    }
    return(status);
}

static int
init_slice_info(struct dinfo *dip, slice_info_t *sip, large_t *data_resid)
{
    /* Note: The data limit was adjusted by file position by FindCapacity(). */
    large_t data_limit = dip->di_data_limit;
    //large_t data_limit = (dip->di_data_limit - dip->di_file_position);
    large_t slice_length;
    int status = SUCCESS;

    sip->slice = 0;
    sip->slice_position = dip->di_file_position;
    slice_length = (data_limit / dip->di_slices);
    sip->slice_length = rounddown(slice_length, dip->di_dsize);
    if (sip->slice_length < (large_t)dip->di_dsize) {
	LogMsg (dip, dip->di_efp, logLevelCrit, 0,
		"Slice length of " LUF " bytes is smaller than device size of %u bytes!\n",
		sip->slice_length, dip->di_dsize);
	status = FAILURE;
    } else {
	*data_resid = (data_limit - (sip->slice_length * dip->di_slices));
	*data_resid = rounddown(*data_resid, dip->di_dsize);
    }
    return(status);
}

static void
setup_slice(struct dinfo *dip, slice_info_t *sip)
{
    dip->di_file_position = sip->slice_position;
    /* Variable options may be set, so always set the random data limit! */
    /* Note: This limit is really the max random data offset, see usage! */
    /* This random data value is used for random I/O and reverse I/O. */
    dip->di_rdata_limit = (dip->di_file_position + sip->slice_length);
    /*
     * Restrict data limit to slice length or user set limit.
     */
    dip->di_data_limit = MIN(dip->di_data_limit, sip->slice_length);
    /* BEWARE: These override the data limit, when specified! */
    if (dip->di_min_limit > dip->di_data_limit) {
        dip->di_min_limit = dip->di_data_limit;
    }
    if (dip->di_max_limit > dip->di_data_limit) {
        dip->di_max_limit = dip->di_data_limit;
    }
    if (dip->di_step_offset) {
	dip->di_end_position = (dip->di_file_position + dip->di_data_limit);
    }
    if (dip->di_debug_flag || dip->di_Debug_flag || dip->di_pDebugFlag) {
	large_t blocks, lba;

	Lprintf(dip, "\nSlice %d Information:\n", sip->slice);

	lba = (large_t)(dip->di_file_position / dip->di_dsize);
	Lprintf(dip, DT_FIELD_WIDTH FUF " (lba " LUF ")\n", "Starting offset", 
		dip->di_file_position, lba);

	lba = (large_t)((dip->di_file_position + sip->slice_length) / dip->di_dsize);
	Lprintf(dip, DT_FIELD_WIDTH FUF " (lba " LUF ")\n", "Ending offset",
		(dip->di_file_position + sip->slice_length), (lba - 1));

	blocks = (large_t)(sip->slice_length / dip->di_dsize);
	Lprintf(dip, DT_FIELD_WIDTH FUF " bytes (" LUF " block%s)\n", "Slice length",
		sip->slice_length, blocks, (blocks > 1) ? "s" : "");

	blocks = (large_t)(dip->di_data_limit / dip->di_dsize);
	Lprintf(dip, DT_FIELD_WIDTH FUF " bytes (" LUF " block%s)\n", "Data limit",
		dip->di_data_limit, blocks, (blocks > 1) ? "s" : "");

	Lprintf(dip, DT_FIELD_WIDTH FUF " (lba "LUF") - "FUF" (lba "LUF")\n", "Random range",
		dip->di_file_position, (large_t)(dip->di_file_position / dip->di_dsize),
		dip->di_rdata_limit, (large_t)(dip->di_rdata_limit / dip->di_dsize));

#if defined(DEBUG)
	if (dip->di_step_offset) {
	    lba = (large_t)(dip->di_end_position / dip->di_dsize);
	    Lprintf(dip, DT_FIELD_WIDTH FUF " (lba " LUF ")\n", "End Position",
		    dip->di_end_position, lba);
	}
#endif /* defined(DEBUG) */

	Lflush(dip);
    }
    dip->di_slice_number = sip->slice;
    return;
}
