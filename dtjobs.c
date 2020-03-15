/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2019			    *
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
 * Module:	dtjobs.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	This file contains functions to handle dt's jobs.
 *
 * Modification History:
 * 
 * June 7th, 2019 by Robin T. Miller
 *      Add support for log directory (logdir=).
 * 
 * November 4th, 2016 by Robin T. Miller
 *      Add support for dt job statistics.
 * 
 * December 17th, 2015 by Robin T. Miller
 * 	Fix problem in remove_job(), causing previous jobs to be lost.
 * 
 * September 15th, 2015 by Robin T. Miller
 * 	Update execute_threads() (once again), to use the device information
 * passed in, since this is already a clone of the master, as saves memory!
 * Previous breakge was with async threads due to overwriting the thread ID.
 * 
 * June 25th, 2015 by Robin T. Miller
 * 	Revert code in execute_threads() where I tried to opimize the use
 * of the 1st threads to save memory. This is not safe (today), since the
 * initial dip is later used when cleaning up jobs, so we corrupt memory.
 * This initialize device is expected to require freeing when jobs complete.
 * 
 * June 14th, 2015 by Robin T. Miller
 * 	In execute_threads(), use the device information passed in for
 * the 1st thread to avoid an extra clone and extra memory resources.
 * 
 * May 19th, 2015 by Robin T. Miller
 * 	For multiple device support, ensure the thread state is set for
 * both the primary (source) device, and the output (destination) device.
 *
 * July 8th, 2014 by Robin T. Miller
 *	Fix bug in cleanup_job() where the jobs lock was *not* released!
 *
 * April 9th, 2014 by Robin T. Miller
 * 	Converted Printf/Fprint's to Eprintf/Wprintf for errors and warning.
 *
 * December 13th, 2013 by Robin T. Miller
 * 	Adding modify parsing of random debug "rdebug" and SCSI debug "sdebug".
 */
#include "dt.h"

/*
 * Globals for Tracking Job Information:
 */ 
uint16_t job_id = 0;		/* The next job ID. */
job_info_t jobsList;		/* The jobs list header. */
job_info_t *jobs = NULL;	/* List of active jobs. */
pthread_mutex_t jobs_lock;	/* Job queue lock. */
pthread_mutexattr_t jobs_lock_attr; /* The jobs lock attributes. */

#define QUEUE_EMPTY(jobs)	(jobs->ji_flink == jobs)

char *job_state_table[] = {
    "stopped", "running", "finished", "paused", "terminating", "cancelling", NULL};

char *thread_state_table[] = {
    "stopped", "starting", "running", "finished", "joined", "paused", "terminating", "cancelling", NULL};

/*
 * Forward References:
 */
int cleanup_job(dinfo_t *mdip, job_info_t *job, hbool_t lock_jobs);
int jobs_eq_state(dinfo_t *dip, jstate_t job_state);
int jobs_ne_state(dinfo_t *dip, jstate_t job_state);
int cancel_job(dinfo_t *dip, job_info_t *job);
int pause_job(dinfo_t *dip, job_info_t *job);
int resume_job(dinfo_t *dip, job_info_t *job);
int sync_threads_starting(dinfo_t *dip, job_info_t *job);
int create_job_log(dinfo_t *dip, job_info_t *job);

/*
 * Start of Job Functions:
 */ 
int
initialize_jobs_data(dinfo_t *dip)
{
    pthread_mutexattr_t *attrp = &jobs_lock_attr;
    int status;

    if ( (status = pthread_mutexattr_init(attrp)) !=SUCCESS) {
	tPerror(dip, status, "pthread_mutexattr_init() of jobs mutex attributes failed!");
	return(FAILURE);
    }
    /*
     * Snippet from Solaris manual page:
     *
     * The mutex object referenced by mutex is  locked  by  calling
     * pthread_mutex_lock().  If  the  mutex is already locked, the
     * calling thread blocks until  the  mutex  becomes  available.
     * This  operation  returns with the mutex object referenced by
     * mutex in the locked state with the  calling  thread  as  its
     * owner.
     *
     * If the mutex type is  PTHREAD_MUTEX_NORMAL, deadlock  detec-
     * tion  is not provided. Attempting to relock the mutex causes
     * deadlock. If a thread attempts to unlock a mutex that it has
     * not  locked  or a mutex that is unlocked, undefined behavior
     * results.
     *
     * If the mutex type is  PTHREAD_MUTEX_ERRORCHECK,  then  error
     * checking is provided. If a thread attempts to relock a mutex
     * that it has already locked, an error will be returned. If  a
     * thread  attempts to unlock a mutex that it has not locked or
     * a mutex which is unlocked, an error will be returned.
     * 
     * If the mutex  type  is   PTHREAD_MUTEX_RECURSIVE,  then  the
     * mutex   maintains the concept of a lock count. When a thread
     * successfully acquires a mutex for the first time,  the  lock
     * count  is  set to 1. Every time a thread relocks this mutex,
     * the lock count is  incremented by one. Each time the  thread
     * unlocks  the  mutex,  the  lock count is decremented by one.
     * When the lock count reaches  0, the mutex becomes  available
     * for  other  threads  to  acquire.  If  a thread  attempts to
     * unlock a mutex that it has not locked or  a  mutex  that  is
     * unlocked, an error will be returned.
     *
     * If the mutex type is  PTHREAD_MUTEX_DEFAULT,  attempting  to
     * recursively  lock  the  mutex results in undefined behavior.
     * Attempting to  unlock the mutex if it was not locked by  the
     * calling  thread results in undefined behavior. Attempting to
     * unlock the mutex if it is not locked  results  in  undefined
     * behavior.
     * 
     */
    /* Note: This does *not* work for Windows! Still working on equivalent! */
    //if ( (status = pthread_mutexattr_settype(attrp, PTHREAD_MUTEX_RECURSIVE)) != SUCCESS) {
    if ( (status = pthread_mutexattr_settype(attrp, PTHREAD_MUTEX_ERRORCHECK)) != SUCCESS) {
	tPerror(dip, status, "pthread_mutexattr_settype() of jobs mutex type failed!");
	return(FAILURE);
    }
    if ( (status = pthread_mutex_init(&jobs_lock, attrp)) != SUCCESS) {
	tPerror(dip, status, "pthread_mutex_init() of jobs lock failed!");
    }
    jobs = &jobsList;
    jobs->ji_flink = jobs;
    jobs->ji_blink = jobs;
    dip->di_job = jobs;
    return(status);
}

int
acquire_jobs_lock(dinfo_t *dip)
{
    int status = pthread_mutex_lock(&jobs_lock);
    if (status != SUCCESS) {
	tPerror(dip, status, "Failed to acquire jobs mutex!");
    }
    return(status);
}

int
release_jobs_lock(dinfo_t *dip)
{
    int status = pthread_mutex_unlock(&jobs_lock);
    if (status != SUCCESS) {
	tPerror(dip, status, "Failed to unlock jobs mutex!");
    }
    return(status);
}

int
acquire_job_lock(dinfo_t *dip, job_info_t *job)
{
    int status = pthread_mutex_lock(&job->ji_job_lock);
    if (status != SUCCESS) {
	tPerror(dip, status, "Failed to acquire per job mutex!");
    }
    return(status);
}

int
release_job_lock(dinfo_t *dip, job_info_t *job)
{
    int status = pthread_mutex_unlock(&job->ji_job_lock);
    if (status != SUCCESS) {
	tPerror(dip, status, "Failed to unlock per job mutex!");
    }
    return(status);
}

int
acquire_job_print_lock(dinfo_t *dip, job_info_t *job)
{
    int status = pthread_mutex_lock(&job->ji_print_lock);
    if (status != SUCCESS) {
	tPerror(dip, status, "Failed to acquire per job print mutex!");
    }
    return(status);
}

int
release_job_print_lock(dinfo_t *dip, job_info_t *job)
{
    int status = pthread_mutex_unlock(&job->ji_print_lock);
    if (status != SUCCESS) {
	tPerror(dip, status, "Failed to unlock per job print mutex!");
    }
    return(status);
}

int
acquire_thread_lock(dinfo_t *dip, job_info_t *job)
{
    int status = pthread_mutex_lock(&job->ji_thread_lock);
    if (status != SUCCESS) {
        tPerror(dip, status, "Failed to acquire job thread lock!");
    }
    return(status);
}

int
release_thread_lock(dinfo_t *dip, job_info_t *job)
{
    int status = pthread_mutex_unlock(&job->ji_thread_lock);
    if (status != SUCCESS) {
        tPerror(dip, status, "Failed to unlock job thread lock!");
    }
    return(status);
}

int
dt_acquire_iolock(dinfo_t *dip, io_global_data_t *iogp)
{
    int status = pthread_mutex_lock(&iogp->io_lock);
    if (status != SUCCESS) {
        tPerror(dip, status, "Failed to acquire dt I/O lock!");
    }
    return(status);
}

int
dt_release_iolock(dinfo_t *dip, io_global_data_t *iogp)
{
    int status = pthread_mutex_unlock(&iogp->io_lock);
    if (status != SUCCESS) {
        tPerror(dip, status, "Failed to unlock dt I/O lock!");
    }
    return(status);
}

/*
 * find_job_by_id() - Find a job by its' job ID.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	job_id = The job ID to find.
 *	lock_jobs = Flag controlling job lock.
 *	  True = acquire lock, False = do not lock.
 *
 * Outputs:
 *	Returns a job if found, else NULL if not found.
 *	When job is found, the jobs lock is still held.
 *	Therefore, the caller *must* release the lock!
 */
job_info_t *
find_job_by_id(dinfo_t *dip, job_id_t job_id, hbool_t lock_jobs)
{
    job_info_t *jhdr = jobs;
    job_info_t *jptr = jhdr->ji_flink;
    job_info_t *job = NULL;

    if ( QUEUE_EMPTY(jobs) ) return(job);
    if (lock_jobs == True) {
	if (acquire_jobs_lock(dip) != SUCCESS) {
	    return(job);
	}
    }
    do {
	/* Find job entry. */
	if (jptr->ji_job_id == job_id) {
	    job = jptr;
	    break;
	}
    } while ( (jptr = jptr->ji_flink) != jhdr );

    if ( (lock_jobs == True) && (job == NULL) ) {
	(void)release_jobs_lock(dip);
    }
    return(job);
}

/*
 * find_job_by_tag() - Find a job by its' job tag.
 *
 * Inputs:
 *	dip = The device information pointer.
 *	job_tag = The job tag to find.
 *	lock_jobs = Flag controlling job lock.
 *	  True = acquire lock, False = do not lock.
 *
 * Outputs:
 *	Returns a job if found, else NULL if not found.
 *	When job is found, the jobs lock is still held.
 *	Therefore, the caller *must* release the lock!
 */
job_info_t *
find_job_by_tag(dinfo_t *dip, char *tag, hbool_t lock_jobs)
{
    job_info_t *jhdr = jobs;
    job_info_t *jptr = jhdr->ji_flink;
    job_info_t *job = NULL;

    if ( QUEUE_EMPTY(jobs) ) return(job);
    if (lock_jobs == True) {
	if (acquire_jobs_lock(dip) != SUCCESS) {
	    return(job);
	}
    }
    do {
	/* Find job entry. */
	if ( jptr->ji_job_tag && (strcmp(jptr->ji_job_tag,tag) == 0) ) {
	    job = jptr;
	    break;
	}
    } while ( (jptr = jptr->ji_flink) != jhdr );

    if ( (lock_jobs == True) && (job == NULL) ) {
	(void)release_jobs_lock(dip);
    }
    return(job);
}

/*
 * find_job_by_tag() - Find a job by its' job tag.
 *
 * Inputs:
 *	dip = The device information pointer.
 * 	tag = The job tag to find.
 * 	pjob = The previous job (context for next job).
 *	lock_jobs = Flag controlling job lock.
 *	  True = acquire lock, False = do not lock.
 *
 * Outputs:
 *	Returns a job if found, else NULL if not found.
 *	When job is found, the jobs lock is still held.
 *	Therefore, the caller *must* release the lock!
 */
job_info_t *
find_jobs_by_tag(dinfo_t *dip, char *tag, job_info_t *pjob, hbool_t lock_jobs)
{
    job_info_t *jhdr = jobs;
    job_info_t *jptr = jhdr;
    job_info_t *job = NULL;
    int status = SUCCESS;

    if (lock_jobs == True) {
	if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	    return(job);
	}
    }
    if (pjob) {
	jptr = pjob;	/* Start at the previous job. */
    }
    if ( QUEUE_EMPTY(jptr) ) {
	if (lock_jobs == True) {
	    (void)release_jobs_lock(dip);
	}
	return(job);
    }
    while ( (jptr = jptr->ji_flink) != jhdr ) {
	/* Find job entry by tag. */
	if ( jptr->ji_job_tag && (strcmp(jptr->ji_job_tag,tag) == 0) ) {
	    job = jptr;
	    break;
	}
    }

    if ( (lock_jobs == True) && (job == NULL) ) {
	(void)release_jobs_lock(dip);
    }
    return(job);
}

job_info_t *
create_job(dinfo_t *dip)
{
    job_info_t *job = Malloc(dip, sizeof(job_info_t));

    if (job) {
	int status;
	job->ji_job_id = ++job_id;
	job->ji_job_state = JS_STOPPED;
	if ( (status = pthread_mutex_init(&job->ji_job_lock, NULL)) != SUCCESS) {
	    tPerror(dip, status, "pthread_mutex_init() of per job lock failed!");
	}
	if ( (status = pthread_mutex_init(&job->ji_print_lock, NULL)) != SUCCESS) {
	    tPerror(dip, status, "pthread_mutex_init() of per job print lock failed!");
	}
    }
    return(job);
}

int
insert_job(dinfo_t *dip, job_info_t *job)
{
    job_info_t *jhdr = jobs, *jptr;
    int status;
    
    /*
     * Note: Job threads started, so queue even if lock fails!
     *       May revert later, but recent bug was misleading! ;(
     */
    status = acquire_jobs_lock(dip);
    jptr = jhdr->ji_blink;
    jptr->ji_flink = job;
    job->ji_blink = jptr;
    job->ji_flink = jhdr;
    jhdr->ji_blink = job;
    if (status == SUCCESS) {
	status = release_jobs_lock(dip);
    }
    return(status);
}

/*
 * Note: This may be called for full or partially initialized job.
 */
int
cleanup_job(dinfo_t *mdip, job_info_t *job, hbool_t lock_jobs)
{
    threads_info_t *tip;
    int status = SUCCESS;
    int lock_status;

    if (lock_jobs == True) {
	if ( (lock_status = acquire_jobs_lock(mdip)) != SUCCESS) {
	    return(lock_status);
	}
    }
    /* Free job resources. */
    if (tip = job->ji_tinfo) {
	dinfo_t *dip = NULL;
	int thread;

	for (thread = 0; (thread < tip->ti_threads); thread++) {
	    hbool_t first_time = True;
	    dip = tip->ti_dts[thread];
	    while (dip->di_trigger_active) {
		if (first_time == True) {
		    Wprintf(mdip, "The trigger thread is still active, waiting %u seconds...\n",
			   cancel_delay);
		    os_sleep(cancel_delay);
		    first_time = False;
		} else {
		    Eprintf(mdip, "Cancelling trigger thread...\n");
		    (void)cancel_thread_threads(mdip, dip);
		}
	    }
	    cleanup_device(dip, False);
	    FreeMem(mdip, dip, sizeof(*dip));
	} /* End threads for loop. */
	Free(mdip, tip->ti_dts);

    } /* End if (tip = job->ji_tinfo) */

    if (job->ji_job_tag) {
	FreeStr(mdip, job->ji_job_tag);
	job->ji_job_tag = NULL;
    }
    if (job->ji_job_logfile) {
	if (job->ji_job_logfp) {
	    (void)fclose(job->ji_job_logfp);
	    job->ji_job_logfp = NULL;
	}
	FreeStr(mdip, job->ji_job_logfile);
	job->ji_job_logfile = NULL;
    }
    if ( (status = pthread_mutex_destroy(&job->ji_job_lock)) != SUCCESS) {
	tPerror(mdip, status, "pthread_mutex_destroy() of per job lock failed!");
    }
    if ( (status = pthread_mutex_destroy(&job->ji_print_lock)) != SUCCESS) {
	tPerror(mdip, status, "pthread_mutex_destroy() of per job print lock failed!");
    }
    FreeMem(mdip, job, sizeof(*job));
    //mdip->di_job = NULL;
    if ( (lock_jobs == True) && (lock_status == SUCCESS) ) {
	(void)release_jobs_lock(mdip);
    }
    return (status);
}

int
remove_job(dinfo_t *mdip, job_info_t *job, hbool_t lock_jobs)
{
    job_info_t *jptr;
    int status = SUCCESS;
    int lock_status;

    if (lock_jobs == True) {
	if ( (lock_status = acquire_jobs_lock(mdip)) != SUCCESS) {
	    return(lock_status);
	}
    }
    jptr = job->ji_blink;
    jptr->ji_flink = job->ji_flink;
    job->ji_flink->ji_blink = jptr;

    status = cleanup_job(mdip, job, False);

    if ( (lock_jobs == True) && (lock_status == SUCCESS) ) {
	(void)release_jobs_lock(mdip);
    }
    return(status);
}

int
remove_job_by_id(dinfo_t *dip, job_id_t job_id)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_id(dip, job_id, True)) {
	status = remove_job(dip, job, False);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
set_job_state(dinfo_t *dip, job_info_t *job, jstate_t job_state, hbool_t lock_jobs)
{
    int status = SUCCESS;

    if (lock_jobs) {
	if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	    return(status);
	}
    }
    job->ji_job_state = job_state;
    if (lock_jobs) {
	(void)release_jobs_lock(dip);
    }
    return(status);
}

int
get_threads_state_count(threads_info_t *tip, tstate_t thread_state)
{
    dinfo_t *dip;
    int thread;
    int count = 0;

    for (thread = 0; (thread < tip->ti_threads); thread++) {
	dip = tip->ti_dts[thread];
	if (dip->di_thread_state == thread_state) {
	    count++;
	}
    }
    return(count);
}

int
set_threads_state(threads_info_t *tip, tstate_t thread_state)
{
    dinfo_t *dip;
    int thread;

    for (thread = 0; (thread < tip->ti_threads); thread++) {
	dip = tip->ti_dts[thread];
	dip->di_thread_state = thread_state;
	if (thread_state == TS_TERMINATING) {
	    dip->di_thread_stopped = time((time_t) 0);
	}
	/* For copy/mirror/verify I/O modes, do output device too! */
	if (dip = dip->di_output_dinfo) {
	    dip->di_thread_state = thread_state;
	    if (thread_state == TS_TERMINATING) {
		dip->di_thread_stopped = time((time_t) 0);
	    }
	}
    }
    return(SUCCESS);
}

int
jobs_active(dinfo_t *dip)
{
    int count;

    count = jobs_ne_state(dip, JS_FINISHED);
    return(count);
}

int
jobs_paused(dinfo_t *dip)
{
    int count;

    count = jobs_eq_state(dip, JS_PAUSED);
    return(count);
}

int
jobs_ne_state(dinfo_t *dip, jstate_t job_state)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int count = 0, status;

    if ( QUEUE_EMPTY(jobs) ) return(count);
    if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	return(count);
    }
    do {
	/* This state is set prior to thread exit. */
	if (job->ji_job_state != job_state) {
	    count++;
	}
    } while ( ((job = job->ji_flink) != jhdr) );
    (void)release_jobs_lock(dip);
    return(count);
}

int
jobs_eq_state(dinfo_t *dip, jstate_t job_state)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int count = 0, status;

    if ( QUEUE_EMPTY(jobs) ) return(count);
    if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	return(count);
    }
    do {
	/* This state is set prior to thread exit. */
	if (job->ji_job_state == job_state) {
	    count++;
	}
    } while ( ((job = job->ji_flink) != jhdr) );
    (void)release_jobs_lock(dip);
    return(count);
}

int
jobs_finished(dinfo_t *dip)
{
    job_info_t *jhdr = jobs;
    job_info_t *jptr = jhdr->ji_flink;
    job_info_t *job;
    int status = SUCCESS;

    if ( QUEUE_EMPTY(jobs) ) return(status);
    if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	return(status);
    }
    do {
	job = jptr;
	jptr = job->ji_flink;
	if (job->ji_job_state == JS_FINISHED) {
	    if (job->ji_job_status == FAILURE) status = job->ji_job_status;
	    if (job->ji_job_tag) {
		Printf(dip, "Job %u (%s) completed with status %d\n",
		       job->ji_job_id, job->ji_job_tag, status);
	    } else {
		Printf(dip, "Job %u completed with status %d\n", job->ji_job_id, status);
	    }
	    remove_job(dip, job, False);
	    /* next job, please! */
	}
    } while ( jptr != jhdr );
    (void)release_jobs_lock(dip);
    return(status);
}

int
cancel_job_threads(dinfo_t *mdip, threads_info_t *tip)
{
    dinfo_t *dip;
    int thread;
    int status = SUCCESS;

    for (thread = 0; (thread < tip->ti_threads); thread++) {
	dip = tip->ti_dts[thread];
	if (dip->di_debug_flag || dip->di_tDebugFlag) {
	    Printf(mdip, "Canceling thread number %d, thread ID: "OS_TID_FMT"...\n",
		   dip->di_thread_number, dip->di_thread_id);
	}
	status = cancel_thread_threads(mdip, dip);
    }
    return(status);
}

/*
 * This function cancel's all threads associated with a job thread.
 */
int
cancel_thread_threads(dinfo_t *mdip, dinfo_t *dip)
{
    int status = SUCCESS;
    int pstatus;

    (void)os_set_thread_cancel_type(dip, PTHREAD_CANCEL_ASYNCHRONOUS);

    if (dip->di_trigger_active == True) {
	if ( (pstatus = pthread_cancel(dip->di_trigger_thread)) != SUCCESS) {
	    tPerror(mdip, pstatus, "pthread_cancel() on trigger thread ID "OS_TID_FMT" failed!\n", dip->di_trigger_thread);
	    status = FAILURE;
	}
	dip->di_trigger_active = False;
    }
    /* Finally, cancel the device thread. */
    if (dip->di_thread_state != TS_JOINED) {
	if ( (pstatus = pthread_cancel(dip->di_thread_id)) != SUCCESS) {
	    tPerror(mdip, pstatus, "pthread_cancel() on thread ID "OS_TID_FMT" failed!\n", dip->di_thread_id);
	    status = FAILURE;
	} else {
	    dip->di_thread_state = TS_CANCELLED;
	}
    }
    /* Set the thread exit status to FAILURE, when terminating! */
    dip->di_exit_status = FAILURE;
    return(status);
}

int
cancel_job(dinfo_t *dip, job_info_t *job)
{
    int status = WARNING;

    if (job->ji_job_state != JS_FINISHED) {
	threads_info_t *tip = job->ji_tinfo;
	job->ji_job_state = JS_CANCELLED;
	Printf(dip, "Job %u is being cancelled (%d thread%s)\n",
	       job->ji_job_id, tip->ti_threads,
	       (tip->ti_threads > 1) ? "s" : "");
	status = cancel_job_threads(dip, tip);
    }
    return(status);
}

int
cancel_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int status = SUCCESS;

    if ( QUEUE_EMPTY(jobs) ) {
	Wprintf(NULL, "There are no jobs active!\n");
	return(status);
    }
    if (job_id) {
	status = cancel_job_by_id(dip, job_id);
    } else if (job_tag) {
	status = cancel_jobs_by_tag(dip, job_tag);
    } else {
	if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	    return(status);
	}
	do {
	    int cstatus = cancel_job(dip, job);
	    if (cstatus == FAILURE) status = cstatus;
	    if (CmdInterruptedFlag) break;
	} while ( (job = job->ji_flink) != jhdr);
	(void)release_jobs_lock(dip);
    }
    return(status);
}

int
cancel_job_by_id(dinfo_t *dip, job_id_t job_id)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_id(dip, job_id, True)) {
	status = cancel_job(dip, job);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
cancel_job_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_tag(dip, job_tag, True)) {
	status = cancel_job(dip, job);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
cancel_jobs_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int jobs_found = 0;
    int status = SUCCESS;
    hbool_t lock_jobs = True;
    
    while (job = find_jobs_by_tag(dip, job_tag, job, lock_jobs)) {
	int cstatus;
	jobs_found++;
	cstatus = cancel_job(dip, job);
	if (cstatus == FAILURE) {
	    status = cstatus;
	}
	lock_jobs = False;
    }
    if (jobs_found == 0) {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else {
	(void)release_jobs_lock(dip);
    }
    return(status);
}

int
pause_job(dinfo_t *dip, job_info_t *job)
{
    threads_info_t *tip;
    int status;

    job->ji_job_state = JS_PAUSED;
    tip = job->ji_tinfo;
    Printf(dip, "Job %u is being paused (%d thread%s)\n",
	   job->ji_job_id, tip->ti_threads,
	   (tip->ti_threads > 1) ? "s" : "");
    status = set_threads_state(tip, TS_PAUSED);
    return(status);
}

int
pause_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int status = SUCCESS;

    if ( QUEUE_EMPTY(jobs) ) {
	Wprintf(dip, "There are no jobs active!\n");
	return(status);
    }
    if (job_id) {
	status = pause_job_by_id(dip, job_id);
    } else if (job_tag) {
	status = pause_jobs_by_tag(dip, job_tag);
    } else {
	if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	    return(status);
	}
	do {
	    if (job->ji_job_state == JS_RUNNING) {
		int sstatus = pause_job(dip, job);
		if (sstatus == FAILURE) status = sstatus;
	    }
	} while ( (job = job->ji_flink) != jhdr);
	(void)release_jobs_lock(dip);
    }
    return(status);
}

int
pause_job_by_id(dinfo_t *dip, job_id_t job_id)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_id(dip, job_id, True)) {
	if (job->ji_job_state == JS_RUNNING) {
	    status = pause_job(dip, job);
	} else if (job->ji_job_state == JS_PAUSED) {
	    Wprintf(dip, "Job %u is already paused!\n", job_id);
	} else {
	    Wprintf(dip, "Job %u is not running!\n", job_id);
	}
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
pause_job_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_tag(dip, job_tag, True)) {
	if (job->ji_job_state == JS_RUNNING) {
	    status = pause_job(dip, job);
	} else if (job->ji_job_state == JS_PAUSED) {
	    Wprintf(dip, "Job %u (%s) is already paused!\n",
		    job->ji_job_id, job->ji_job_tag);
	} else {
	    Wprintf(dip, "Job %u (%s) is not running!\n",
		    job->ji_job_id, job->ji_job_tag);
	}
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
pause_jobs_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int jobs_found = 0;
    int status = SUCCESS;
    hbool_t lock_jobs = True;
    
    while (job = find_jobs_by_tag(dip, job_tag, job, lock_jobs)) {
	jobs_found++;
	if (job->ji_job_state == JS_RUNNING) {
	    int pstatus = pause_job(dip, job);
	    if (pstatus == FAILURE) {
		status = pstatus;
	    }
	} else if (job->ji_job_state == JS_PAUSED) {
	    Wprintf(dip, "Job %u (%s) is already paused!\n",
		    job->ji_job_id, job->ji_job_tag);
	} else {
	    Wprintf(dip, "Job %u (%s) is not running!\n",
		    job->ji_job_id, job->ji_job_tag);
	}
	lock_jobs = False;
    }
    if (jobs_found == 0) {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else {
	(void)release_jobs_lock(dip);
    }
    return(status);
}

int
resume_job(dinfo_t *dip, job_info_t *job)
{
    threads_info_t *tip;
    int status;

    job->ji_job_state = JS_RUNNING;
    tip = job->ji_tinfo;
    Printf(dip, "Job %u is being resumed (%d thread%s)\n",
	   job->ji_job_id, tip->ti_threads,
	   (tip->ti_threads > 1) ? "s" : "");
    status = set_threads_state(tip, TS_RUNNING);
    return(status);
}

int
resume_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int status = SUCCESS;

    if ( QUEUE_EMPTY(jobs) ) {
	Wprintf(dip, "There are no jobs active!\n");
	return(status);
    }
    if (job_id) {
	status = resume_job_by_id(dip, job_id);
    } else if (job_tag) {
	status = resume_jobs_by_tag(dip, job_tag);
    } else {
	if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	    return(status);
	}
	do {
	    if (job->ji_job_state == JS_PAUSED) {
		int sstatus = resume_job(dip, job);
		if (sstatus == FAILURE) status = sstatus;
	    }
	} while ( (job = job->ji_flink) != jhdr);
	(void)release_jobs_lock(dip);
    }
    return(status);
}

int
resume_job_by_id(dinfo_t *dip, job_id_t job_id)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_id(dip, job_id, True)) {
	if (job->ji_job_state == JS_PAUSED) {
	    status = resume_job(dip, job);
	} else if (job->ji_job_state == JS_RUNNING) {
	    Wprintf(dip, "Job %u is already running!\n", job_id);
	} else {
	    Wprintf(dip, "Job %u is not paused!\n", job_id);
	}
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
resume_job_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_tag(dip, job_tag, True)) {
	if (job->ji_job_state == JS_PAUSED) {
	    status = resume_job(dip, job);
	} else if (job->ji_job_state == JS_RUNNING) {
	    Wprintf(dip, "Job %u (%s) is already running!\n",
		    job->ji_job_id, job->ji_job_tag);
	} else {
	    Wprintf(dip, "Job %u (%s) is not paused!\n",
		    job->ji_job_id, job->ji_job_tag);
	}
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
resume_jobs_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int jobs_found = 0;
    int status = SUCCESS;
    hbool_t lock_jobs = True;
    
    while (job = find_jobs_by_tag(dip, job_tag, job, lock_jobs)) {
	jobs_found++;
	if (job->ji_job_state == JS_PAUSED) {
	    int rstatus = resume_job(dip, job);
	    if (rstatus == FAILURE) {
		status = rstatus;
	    }
	} else if (job->ji_job_state == JS_RUNNING) {
	    Wprintf(dip, "Job %u (%s) is already running!\n",
		    job->ji_job_id, job->ji_job_tag);
	} else {
	    Wprintf(dip, "Job %u (%s) is not paused!\n",
		    job->ji_job_id, job->ji_job_tag);
	}
	lock_jobs = False;
    }
    if (jobs_found == 0) {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else {
	(void)release_jobs_lock(dip);
    }
    return(status);
}

int
modify_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag, char *modify_string)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    modify_params_t *modp = Malloc(dip, sizeof(modify_params_t) );
    int status = SUCCESS;

    if ( QUEUE_EMPTY(jobs) ) {
	Wprintf(dip, "There are no jobs active!\n");
	return(status);
    }
    status = parse_modify_parameters(dip, modify_string, modp);
    if (status == FAILURE) {
	FreeMem(dip, modp, sizeof(*modp));
	return(status);
    }
    /* User can specify these via the string specified! */
    if (modp->job_id) job_id = modp->job_id;
    if (modp->job_tag) job_tag = modp->job_tag;
    if (job_id) {
	status = modify_job_by_id(dip, job_id, modp);
    } else if (job_tag) {
	status = modify_jobs_by_tag(dip, job_tag, modp);
    } else {
	if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	    return(status);
	}
	do {
	    Printf(dip, "Job %u is being modified (%d thread%s)\n",
		   job->ji_job_id, job->ji_tinfo->ti_threads,
		   (job->ji_tinfo->ti_threads > 1) ? "s" : "");
	    set_thread_parameters(job->ji_tinfo, modp);
	} while ( (job = job->ji_flink) != jhdr);
	(void)release_jobs_lock(dip);
    }
    if (modp->job_tag) FreeStr(dip, modp->job_tag);
    return(status);
}

int
modify_job_by_id(dinfo_t *dip, job_id_t job_id, modify_params_t *modp)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_id(dip, job_id, True)) {
	Printf(dip, "Job %u is being modified (%d thread%s)\n",
	       job->ji_job_id, job->ji_tinfo->ti_threads,
	       (job->ji_tinfo->ti_threads > 1) ? "s" : "");
	set_thread_parameters(job->ji_tinfo, modp);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
modify_job_by_tag(dinfo_t *dip, char *job_tag, modify_params_t *modp)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_tag(dip, job_tag, True)) {
	Printf(dip, "Job %u (%s) is being modified (%d thread%s)\n",
	       job->ji_job_id, job->ji_job_tag, job->ji_tinfo->ti_threads,
	       (job->ji_tinfo->ti_threads > 1) ? "s" : "");
	set_thread_parameters(job->ji_tinfo, modp);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
modify_jobs_by_tag(dinfo_t *dip, char *job_tag, modify_params_t *modp)
{
    job_info_t *job = NULL;
    int jobs_found = 0;
    int status = SUCCESS;
    hbool_t lock_jobs = True;
    
    while (job = find_jobs_by_tag(dip, job_tag, job, lock_jobs)) {
	jobs_found++;
	Printf(dip, "Job %u (%s) is being modified (%d thread%s)\n",
	       job->ji_job_id, job->ji_job_tag, job->ji_tinfo->ti_threads,
	       (job->ji_tinfo->ti_threads > 1) ? "s" : "");
	set_thread_parameters(job->ji_tinfo, modp);
	lock_jobs = False;
    }
    if (jobs_found == 0) {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else {
	(void)release_jobs_lock(dip);
    }
    return(status);
}

/*
 * Note: This is not very clean, may wish to use a real parser!
 */ 
int
parse_modify_parameters(dinfo_t *dip, char *buffer, modify_params_t *modp)
{
    char *token, *string, *saveptr;
    unsigned int value;
    int status = SUCCESS;

    token = string = buffer;
    if (dip == NULL) dip = master_dinfo;
    token = strtok_r(string, " ", &saveptr);
    while (token != NULL) {
	if (match (&token, "job=")) {
	    value = number(dip, token, ANY_RADIX, &status, dip->di_tDebugFlag);
	    if (status == SUCCESS) {
		modp->job_id = (job_id_t)value;
	    }
	} else if (match (&token, "tag=")) {
	    modp->job_tag = strdup(string);
	} else if (match (&token, "io_delay=")) {
	    value = number(dip, token, ANY_RADIX, &status, dip->di_tDebugFlag);
	    if (status == SUCCESS) {
		modp->rdelay_parsed = modp->wdelay_parsed = True;
		modp->read_delay = modp->write_delay = value;
	    }
	} else if (match (&token, "open_delay=")) {
	    value = number(dip, token, ANY_RADIX, &status, dip->di_tDebugFlag);
	    if (status == SUCCESS) {
		modp->odelay_parsed = True;
		modp->open_delay = value;
	    }
	} else if (match (&token, "close_delay=")) {
	    value = number(dip, token, ANY_RADIX, &status, dip->di_tDebugFlag);
	    if (status == SUCCESS) {
		modp->cdelay_parsed = True;
		modp->close_delay = value;
	    }
	} else if (match (&token, "delete_delay=")) {
	    value = number(dip, token, ANY_RADIX, &status, dip->di_tDebugFlag);
	    if (status == SUCCESS) {
		modp->ddelay_parsed = True;
		modp->delete_delay = value;
	    }
	} else if (match (&token, "end_delay=")) {
	    value = number(dip, token, ANY_RADIX, &status, dip->di_tDebugFlag);
	    if (status == SUCCESS) {
		modp->edelay_parsed = True;
		modp->end_delay = value;
	    }
	} else if (match (&token, "read_delay=")) {
	    value = number(dip, token, ANY_RADIX, &status, dip->di_tDebugFlag);
	    if (status == SUCCESS) {
		modp->rdelay_parsed = True;
		modp->read_delay = value;
	    }
	} else if (match (&token, "start_delay=")) {
	    value = number(dip, token, ANY_RADIX, &status, dip->di_tDebugFlag);
	    if (status == SUCCESS) {
		modp->sdelay_parsed = True;
		modp->start_delay = value;
	    }
	} else if (match (&token, "write_delay=")) {
	    value = number(dip, token, ANY_RADIX, &status, dip->di_tDebugFlag);
	    if (status == SUCCESS) {
		modp->wdelay_parsed = True;
		modp->write_delay = value;
	    }
	} else if (match (&token, "enable=")) {
	    status = parse_enable_disable(dip, token, True, modp);
	    if (status == FAILURE) break;
	} else if (match (&token, "disable=")) {
	    status = parse_enable_disable(dip, token, False, modp);
	    if (status == FAILURE) break;
	} else {
	    Eprintf(dip, "Unknown modify parameter '%s'\n", token);
	    status = FAILURE;
	    break;
	}
	token = strtok_r(NULL, " ", &saveptr);	/* Next token please. */
    }
    return(status);
}

int
parse_enable_disable(dinfo_t *dip, char *token, hbool_t bool_value, modify_params_t *modp)
{
    int status = SUCCESS;

    while ( *token != '\0' ) {
	if (match(&token, ","))
	    continue;
	if (match(&token, "debug")) {
	    modp->debug_parsed = True;
	    modp->debug_flag = bool_value;
	    continue;
	}
	if (match(&token, "Debug")) {
	    modp->Debug_parsed = True;
	    modp->Debug_flag = bool_value;
	    continue;
	}
	if (match(&token, "edebug")) {
	    modp->eDebug_parsed = True;
	    modp->eDebug_flag = bool_value;
	    continue;
	}
	if (match(&token, "fdebug")) {
	    modp->fDebug_parsed = True;
	    modp->fDebug_flag = bool_value;
	    continue;
	}
	if (match(&token, "jdebug")) {
	    modp->jDebug_parsed = True;
	    modp->jDebug_flag = bool_value;
	    continue;
	}
	if (match(&token, "rdebug")) {
	    modp->rDebug_parsed = True;
	    modp->rDebug_flag = bool_value;
	    continue;
	}
	if (match(&token, "sdebug")) {
	    modp->sDebug_parsed = True;
	    modp->sDebug_flag = bool_value;
	    continue;
	}
	if (match(&token, "tdebug")) {
	    modp->tDebug_parsed = True;
	    modp->tDebug_flag = bool_value;
	    continue;
	}
	if (match(&token, "pstats")) {
	    modp->pstats_flag_parsed = True;
	    modp->pstats_flag = bool_value;
	    continue;
	}
	if (match(&token, "stats")) {
	    modp->stats_flag_parsed = True;
	    modp->stats_flag = bool_value;
	    continue;
	}
	if (*token != '\0') {
	    Eprintf(dip, "Unknown %s parameter '%s'\n",
		   (bool_value == True) ? "enable" : "disable", token);
	    status = FAILURE;
	    break;
	}
    }
    return(status);
}

void
set_thread_parameters(threads_info_t *tip, modify_params_t *modp)
{
    dinfo_t *dip;
    int thread;

    for (thread = 0; (thread < tip->ti_threads); thread++) {
	dip = tip->ti_dts[thread];
	set_modify_parameters(dip, modp);
    }
    return;
}

void
set_modify_parameters(dinfo_t *dip, modify_params_t *modp)
{
    
    if (modp->odelay_parsed) {
	dip->di_open_delay = modp->open_delay;
    }
    if (modp->cdelay_parsed) {
	dip->di_close_delay = modp->close_delay;
    }
    if (modp->edelay_parsed) {
	dip->di_end_delay = modp->end_delay;
    }
    if (modp->rdelay_parsed) {
	dip->di_read_delay = modp->read_delay;
    }
    if (modp->sdelay_parsed) {
	dip->di_start_delay = modp->start_delay;
    }
    if (modp->wdelay_parsed) {
	dip->di_write_delay = modp->write_delay;
    }
    if (modp->debug_parsed) {
	dip->di_debug_flag = modp->debug_flag;
    }
    if (modp->Debug_parsed) {
	dip->di_Debug_flag = modp->Debug_flag;
    }
    if (modp->eDebug_parsed) {
	dip->di_eDebugFlag = modp->eDebug_flag;
    }
    if (modp->fDebug_parsed) {
	dip->di_fDebugFlag = modp->fDebug_flag;
    }
    if (modp->jDebug_parsed) {
	dip->di_jDebugFlag = modp->jDebug_flag;
    }
    if (modp->rDebug_parsed) {
	dip->di_rDebugFlag = modp->rDebug_flag;
    }
    if (modp->sDebug_parsed) {
	dip->di_sDebugFlag = modp->sDebug_flag;
    }
    if (modp->tDebug_parsed) {
	dip->di_tDebugFlag = modp->tDebug_flag;
    }
    if (modp->pstats_flag_parsed) {
	dip->di_pstats_flag = modp->pstats_flag;
    }
    if (modp->stats_flag_parsed) {
	dip->di_stats_flag = modp->stats_flag;
    }
    return;
}

int
query_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag, char *query_string)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int status = SUCCESS;

    if ( QUEUE_EMPTY(jobs) ) {
	Wprintf(dip, "There are no jobs active!\n");
	return(status);
    }
    if (job_id) {
	status = query_job_by_id(dip, job_id, query_string);
    } else if (job_tag) {
	status = query_jobs_by_tag(dip, job_tag, query_string);
    } else {
	if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	    return(status);
	}
	do {
	    query_threads_info(dip, job->ji_tinfo, query_string);
	} while ( (job = job->ji_flink) != jhdr);
	(void)release_jobs_lock(dip);
    }
    return(status);
}

int
query_job_by_id(dinfo_t *dip, job_id_t job_id, char *query_string)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_id(dip, job_id, True)) {
	query_threads_info(dip, job->ji_tinfo, query_string);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
query_job_by_tag(dinfo_t *dip, char *job_tag, char *query_string)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_tag(dip, job_tag, True)) {
	query_threads_info(dip, job->ji_tinfo, query_string);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
query_jobs_by_tag(dinfo_t *dip, char *job_tag, char *query_string)
{
    job_info_t *job = NULL;
    int jobs_found = 0;
    int status = SUCCESS;
    hbool_t lock_jobs = True;
    
    while (job = find_jobs_by_tag(dip, job_tag, job, lock_jobs)) {
	jobs_found++;
	query_threads_info(dip, job->ji_tinfo, query_string);
	lock_jobs = False;
    }
    if (jobs_found == 0) {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else {
	(void)release_jobs_lock(dip);
    }
    return(status);
}

void
query_threads_info(dinfo_t *mdip, threads_info_t *tip, char *query_string)
{
    char buffer[STRING_BUFFER_SIZE];
    dinfo_t *dip;
    job_info_t *job;
    int thread;

    for (thread = 0; (thread < tip->ti_threads); thread++) {
	dip = tip->ti_dts[thread];
	job = dip->di_job;
	if (job->ji_job_tag) {
	    Printf(mdip, "Job: %u, Tag: %s, Thread: %d, State: %s, Device: %s\n",
		   job->ji_job_id, job->ji_job_tag, dip->di_thread_number,
		   thread_state_table[dip->di_thread_state], dip->di_dname);
	} else {
	    Printf(mdip, "Job: %u, Thread: %d, State: %s, Device: %s\n",
		   job->ji_job_id, dip->di_thread_number,
		   thread_state_table[dip->di_thread_state], dip->di_dname);
	}
	if (query_string) {
	    (void)FmtKeepAlive(dip, query_string, buffer);
	    LogMsg(mdip, ofp, logLevelLog, 0, "%s\n", buffer);
	} else {
	    int status = WARNING;
	    if ( (dip->di_thread_state == TS_RUNNING) && dip->di_pkeepalive) {
		(void)FmtKeepAlive(dip, dip->di_pkeepalive, buffer);
	    } else if ( (dip->di_thread_state == TS_FINISHED) && dip->di_tkeepalive) {
		(void)FmtKeepAlive(dip, dip->di_tkeepalive, buffer);
	    } else if (dip->di_keepalive) {
		(void)FmtKeepAlive(dip, dip->di_keepalive, buffer);
	    }
	    if (status == SUCCESS) {
		LogMsg(mdip, ofp, logLevelLog, 0, "%s\n", buffer);
	    }
	}
    }
    if (dip && dip->di_iobf && dip->di_iobf->iob_job_query) {
	/* Note: We don't pass in dip, since thread may be redirected to a log. */
	(void)(*dip->di_iobf->iob_job_query)(mdip, dip->di_job);
    }
    return;
}

int
show_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag, hbool_t verbose)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int status = SUCCESS;

    if ( QUEUE_EMPTY(jobs) ) {
	Wprintf(dip, "There are no jobs active!\n");
	return(status);
    }
    if (job_id) {
	status = show_job_by_id(dip, job_id);
    } else if (job_tag) {
	status = show_jobs_by_tag(dip, job_tag);
    } else {
	if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	    return(status);
	}
	do {
	    show_job_info(dip, job, verbose);
	} while ( (job = job->ji_flink) != jhdr);
	(void)release_jobs_lock(dip);
    }
    return(status);
}

int
show_job_by_id(dinfo_t *dip, job_id_t job_id)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_id(dip, job_id, True)) {
	show_job_info(dip, job, True);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
show_job_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_tag(dip, job_tag, True)) {
	show_job_info(dip, job, True);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
show_jobs_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int jobs_found = 0;
    int status = SUCCESS;
    hbool_t lock_jobs = True;
    
    while (job = find_jobs_by_tag(dip, job_tag, job, lock_jobs)) {
	jobs_found++;
	show_job_info(dip, job, True);
	lock_jobs = False;
    }
    if (jobs_found == 0) {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else {
	(void)release_jobs_lock(dip);
    }
    return(status);
}

void
show_job_info(dinfo_t *dip, job_info_t *job, hbool_t show_threads_flag)
{
    char fmt[STRING_BUFFER_SIZE];
    char *bp = fmt;

    if (job->ji_job_tag) {
	bp += sprintf(bp, "Job %u (%s) is %s (%d thread%s)",
		      job->ji_job_id, job->ji_job_tag,
		      job_state_table[job->ji_job_state],
		      job->ji_tinfo->ti_threads,
		      (job->ji_tinfo->ti_threads > 1) ? "s" : "");
    } else {
	bp += sprintf(bp, "Job %u is %s (%d thread%s)",
		      job->ji_job_id, job_state_table[job->ji_job_state],
		      job->ji_tinfo->ti_threads,
		      (job->ji_tinfo->ti_threads > 1) ? "s" : "");
    }
    if (job->ji_job_state == JS_FINISHED) {
	bp += sprintf(bp, ", with status %d\n", job->ji_job_status);
    } else {
	bp += sprintf(bp, "\n");
    }
    PrintLines(dip, False, fmt);
    if (show_threads_flag) {
	show_threads_info(dip, job->ji_tinfo);
    }
    return;
}

void
show_threads_info(dinfo_t *mdip, threads_info_t *tip)
{
    dinfo_t *dip;
    int thread;

    for (thread = 0; (thread < tip->ti_threads); thread++) {
	char fmt[PATH_BUFFER_SIZE];
	char *bp = fmt;
	dip = tip->ti_dts[thread];
	bp += sprintf(bp, "  Thread: %d, State: %s, Device: %s\n",
		      dip->di_thread_number, thread_state_table[dip->di_thread_state], dip->di_dname);
	if (dip->di_cmd_line) {
	    /* Skip the dt path. */
	    char *cmd = strchr(dip->di_cmd_line, ' ');
	    if (cmd) {
		cmd++;
	    } else {
		cmd = dip->di_cmd_line;
	    }
	    bp += sprintf(bp, "  -> %s\n", cmd);
	}
	/* Note: This was added to do visual inspection of buffer information. */
	if (dip->di_mDebugFlag) {
	    if (dip->di_prefix_string) {
		bp += sprintf(bp, "            Prefix: %p -> %s (%d)\n",
			      dip->di_prefix_string, dip->di_prefix_string, dip->di_prefix_size);
		/* Note: Formatted *after* the thread starts! */
		if (dip->di_fprefix_string) {
		    bp += sprintf(bp, "  Formatted Prefix: %p -> %s (%d)\n",
				  dip->di_fprefix_string, dip->di_fprefix_string, dip->di_fprefix_size);
		}
	    }
	    bp += sprintf(bp, "  Block Size: "SUF", Data size: "SUF", Allocation Size: "SUF"\n",
			  (mysize_t)dip->di_block_size, (mysize_t)dip->di_data_size,
			  (mysize_t)dip->di_data_alloc_size);
	    bp += sprintf(bp, "  Base Address: %p, Data Buffer: %p, Verify Buffer: %p (raw only)\n",
			  dip->di_base_buffer, dip->di_data_buffer, dip->di_verify_buffer);
	    bp += sprintf(bp, "  Pattern Buffer: %p, Length: "SUF"\n",
			  dip->di_pattern_buffer, (mysize_t)dip->di_pattern_bufsize);
	}
	PrintLines(mdip, False, fmt);
    }
    return;
}

int
stop_job(dinfo_t *dip, job_info_t *job)
{
    int status = WARNING;

    if ( (job->ji_job_state != JS_CANCELLED) &&
	 (job->ji_job_state != JS_FINISHED) &&
	 (job->ji_job_state != JS_TERMINATING) ) {
	threads_info_t *tip;
	job->ji_job_state = JS_TERMINATING;
	job->ji_job_stopped = time((time_t) 0);
	tip = job->ji_tinfo;
	Printf(NULL, "Job %u is being stopped (%d thread%s)\n",
	       job->ji_job_id, tip->ti_threads,
	       (tip->ti_threads > 1) ? "s" : "");
	status = set_threads_state(tip, TS_TERMINATING);
    }
    return(status);
}

int
stop_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int status;

    if ( QUEUE_EMPTY(jobs) ) {
	Wprintf(dip, "There are no jobs active!\n");
	return(WARNING);
    }
    if (job_id) {
	status = stop_job_by_id(dip, job_id);
    } else if (job_tag) {
	status = stop_jobs_by_tag(dip, job_tag);
    } else {
	if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	    return(status);
	}
	do {
	    int sstatus = stop_job(dip, job);
	    if (sstatus == FAILURE) status = sstatus;
	} while ( (job = job->ji_flink) != jhdr);
	(void)release_jobs_lock(dip);
    }
    return(status);
}

int
stop_job_by_id(dinfo_t *dip, job_id_t job_id)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_id(dip, job_id, True)) {
	status = stop_job(dip, job);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
stop_job_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_tag(dip, job_tag, True)) {
	status = stop_job(dip, job);
	(void)release_jobs_lock(dip);
    } else {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
stop_jobs_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int jobs_found = 0;
    int status = SUCCESS;
    hbool_t lock_jobs = True;
    
    while (job = find_jobs_by_tag(dip, job_tag, job, lock_jobs)) {
	int sstatus;
	jobs_found++;
	sstatus = stop_job(dip, job);
	if (sstatus == FAILURE) {
	    status = sstatus;
	}
	lock_jobs = False;
    }
    if (jobs_found == 0) {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else {
	(void)release_jobs_lock(dip);
    }
    return(status);
}

/*
 * wait_for_job() - Wait for job running in the foreground.
 */ 
int
wait_for_job(dinfo_t *mdip, job_info_t *job)
{
    threads_info_t *tip = job->ji_tinfo;
    int status;

    if (mdip->di_jDebugFlag) {
	Printf(mdip, "Waiting for Job %u, active threads %u...\n",
	       job->ji_job_id, tip->ti_threads);
    }
    job->ji_job_state = JS_RUNNING;
    status = wait_for_threads(mdip, tip);
    job->ji_job_state = JS_FINISHED;
    if (mdip->di_jDebugFlag) {
	Printf(mdip, "Job %u completed with status %d\n", job->ji_job_id, status);
    }
    (void)remove_job(mdip, job, True);
    mdip->di_job = NULL;
    return(status);
}

/*
 * wait_for_jobs() - Wait for all jobs.
 */ 
int
wait_for_jobs(dinfo_t *dip, job_id_t job_id, char *job_tag)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int status = SUCCESS;
    int count = 0;

    if ( QUEUE_EMPTY(jobs) ) {
	Wprintf(dip, "There are no active jobs!\n");
	return(status);
    }
    if (job_id) {
	status = wait_for_job_by_id(dip, job_id);
    } else if (job_tag) {
	status = wait_for_jobs_by_tag(dip, job_tag);
    } else {
	hbool_t first_time = True;
	while ( (count = jobs_active(dip)) ) {
	    if (CmdInterruptedFlag) break;
	    if (first_time || dip->di_jDebugFlag) {
		Printf(dip, "Waiting on %u job%s to complete...\n",
		       count, (count > 1) ? "s" : "");
		first_time = False;
	    }
	    SleepSecs(dip, JOB_WAIT_DELAY);
	}
	status = jobs_finished(dip);
    }
    return(status);
}

int
wait_for_job_by_id(dinfo_t *dip, job_id_t job_id)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    hbool_t first_time = True;
    int job_found = 0, job_finished = 0;
    
    while (job = find_job_by_id(dip, job_id, True)) {
	job_found++;
	if (job->ji_job_state != JS_FINISHED) {
	    if (first_time || dip->di_jDebugFlag) {
		Printf(dip, "Waiting for Job %u, active threads %u...\n",
		       job->ji_job_id, job->ji_tinfo->ti_threads);
		first_time = False;
	    }
	    (void)release_jobs_lock(dip);
            //if (CmdInterruptedFlag) break;
	    SleepSecs(dip, JOB_WAIT_DELAY);
	    continue;
	}
	job_finished++;
	status = job->ji_job_status;
	(void)release_jobs_lock(dip);
	(void)remove_job(dip, job, True);
	break;
    }
    if (job_found == 0) {
	Eprintf(dip, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    } else if (job_finished == 0) {
	Eprintf(dip, "Job %u did *not* finish!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
wait_for_job_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    hbool_t first_time = True;
    int job_found = 0, job_finished = 0;
    
    while (job = find_job_by_tag(dip, job_tag, True)) {
	job_found++;
	if (job->ji_job_state != JS_FINISHED) {
	    if (first_time || dip->di_jDebugFlag) {
		Printf(dip, "Waiting for Job %u (%s), active threads %u...\n",\
		       job->ji_job_id, job->ji_job_tag, job->ji_tinfo->ti_threads);
		first_time = False;
	    }
	    (void)release_jobs_lock(dip);
            //if (CmdInterruptedFlag) break;
	    SleepSecs(dip, JOB_WAIT_DELAY);
	    continue;
	}
	job_finished++;
	status = job->ji_job_status;
	(void)release_jobs_lock(dip);
	(void)remove_job(dip, job, True);
	break;
    }
    if (job_found == 0) {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else if (job_finished == 0) {
	Eprintf(dip, "Jobs with tag %s did *not* finish!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
wait_for_jobs_by_tag(dinfo_t *dip, char *job_tag)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    hbool_t first_time = True;
    int jobs_found = 0, jobs_finished = 0;
    
    /* Find first or next job. */
    while (job = find_job_by_tag(dip, job_tag, True)) {
	jobs_found++;
	if (job->ji_job_state != JS_FINISHED) {
	    if (first_time || dip->di_jDebugFlag) {
		Printf(dip, "Waiting for Job %u (%s), active threads %u...\n",
		       job->ji_job_id, job->ji_job_tag, job->ji_tinfo->ti_threads);
		first_time = False;
	    }
	    (void)release_jobs_lock(dip);
            //if (CmdInterruptedFlag) break;
	    SleepSecs(dip, JOB_WAIT_DELAY);
	    continue;
	}
	first_time = True;
	jobs_finished++;
	/* Set status and remove this job. */
	if (job->ji_job_status == FAILURE) {
	    status = job->ji_job_status;
	}
	(void)release_jobs_lock(dip);
	(void)remove_job(dip, job, True);
    }
    if (jobs_found == 0) {
	Eprintf(dip, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else if (jobs_finished == 0) {
	Eprintf(dip, "Jobs with tag %s did *not* finish!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

/*
 * do_wait_for_threads_done() - Wait for all job threads to complete. 
 *  
 * Description: 
 *      This function is used to wait for dt I/O threads when the I/O lock
 * for multiple concurrent threads to same file/device is enabled. This is 
 * required for multiple passes or runtime so thread global data can be reset. 
 * The job lock is used to synchronize the I/O threads, just like startup time! 
 *  
 * Inputs: 
 * 	arg = The device information pointer.
 */
void *
do_wait_for_threads_done(void *arg)
{
    dinfo_t *dip = arg;
    job_info_t *job = dip->di_job;
    io_global_data_t *iogp = job->ji_opaque;
    int loops = 0, max_loops = 500, ms_delay = 50;
    int status;

    status = acquire_thread_lock(dip, job);
    iogp->io_waiting_active = True;
    /* Give the caller time to detect us active! */
    os_msleep(ms_delay);

    if (dip->di_tDebugFlag) {
	Printf(dip, ">- Starting threads done loop, threads done %d...\n", iogp->io_threads_done);
    }
    while (iogp->io_threads_done) {
	/* Note: Theads are joined in order, so this calculation won't work! */
	//int threads_active = (job->ji_tinfo->ti_threads - job->ji_tinfo->ti_finished);
	int threads_active = get_threads_state_count(job->ji_tinfo, TS_RUNNING);
	if ( THREAD_TERMINATING(dip) ) break;
	loops++;
	if (iogp->io_threads_done > threads_active) {
	    Printf(dip, "-> BUG: Threads done of %d exceeds threads active %d, should NEVER happen!\n",
		   iogp->io_threads_done, dip->di_threads);
	    //break;
	}
	/* We may need to loop many times when file systems flush our file data. */
	if (loops > max_loops) {
	    Printf(dip, "-> do_wait_for_threads_done() max loops of %d reached, threads done %d\n",
		   max_loops, iogp->io_threads_done);
	    Printf(dip, "Please check for hung I/O or threads exiting abnormally!\n");
	    break;
	}
	if (dip->di_tDebugFlag) {
	    if ((loops % 100) == 0) {
		Printf(dip, "Loop #%d, threads active %d, threads done %d...\n",
		       loops, threads_active, iogp->io_threads_done);
	    }
	}
	if (iogp->io_threads_done >= threads_active) {
	    iogp->io_threads_done = 0;
	    iogp->io_end_of_file = False;
	    iogp->io_bytes_read = 0;
	    iogp->io_bytes_written = 0;
	    iogp->io_records_read = 0;
	    iogp->io_records_written = 0;
	    iogp->io_sequential_offset = iogp->io_starting_offset;
	    break;
	}
	os_msleep(ms_delay);
    }
    iogp->io_waiting_active = False;
    if (dip->di_tDebugFlag) {
	Printf(dip, ">- Ending threads done loop, threads done %d...\n", iogp->io_threads_done);
    }
    release_thread_lock(dip, job);
    pthread_exit(dip);
    return(NULL);
}

void
wait_for_threads_done(dinfo_t *dip)
{
    job_info_t *job = dip->di_job;
    io_global_data_t *iogp = job->ji_opaque;
    int loops = 0, max_loops = 500, ms_delay = 10;

    /* We must wait for all threads to continue *before* starting next wait! */
    /* This occurs with more threads than I/O, causing to reenter quickly. */
    while ((iogp->io_waiting_active == False) && iogp->io_threads_waiting) {
	loops++;
	if (loops > max_loops) {
	    Printf(dip, "-> wait_for_threads_done() max loops of %d reached, suspect hung thread!\n", max_loops);
	    break;
	}
	if (dip->di_tDebugFlag) {
	    if ((loops % 100) == 0) {
		Printf(dip, "Loop #%d, threads waiting %d...\n", loops, iogp->io_threads_waiting);
	    }
	}
	os_msleep(ms_delay);
    }
    (void)dt_acquire_iolock(dip, iogp);
    iogp->io_threads_done++;
    iogp->io_threads_waiting++;
    if (dip->di_tDebugFlag) {
	Printf(dip, "Adjusted threads done %d...\n", iogp->io_threads_done);
    }
    if (iogp->io_waiting_active == False) {
	(void)create_detached_thread(dip, &do_wait_for_threads_done);
        /* Give thread time to startup! */
        while (iogp->io_waiting_active == False) {
            os_msleep(ms_delay);
        }
	if (dip->di_tDebugFlag) {
	    Printf(dip, "Waiting thread is now active, continuing...\n");
	}
    }
    (void)dt_release_iolock(dip, iogp);
    (void)acquire_thread_lock(dip, job);
    (void)release_thread_lock(dip, job);
    (void)dt_acquire_iolock(dip, iogp);
    iogp->io_threads_waiting--;
    if (dip->di_tDebugFlag) {
	Printf(dip, "Finished, threads waiting is %d...\n", iogp->io_threads_waiting);
    }
    (void)dt_release_iolock(dip, iogp);
    return;
}

/*
 * a_job() - Wait for an async (background) job.
 */ 
void *  
a_job(void *arg)
{
    job_info_t *job = arg;
    dinfo_t *dip = master_dinfo;
    threads_info_t *tip = job->ji_tinfo;

#if 0
    Printf(dip, "Job %u started, with %u thread%s...\n",
	   job->ji_job_id, tip->ti_threads,
	   (tip->ti_threads > 1) ? "s" : "");
#endif
    if (job->ji_job_state != JS_PAUSED) {
	job->ji_job_state = JS_RUNNING;
    }
    job->ji_job_status = wait_for_threads(dip, tip);
    job->ji_job_state = JS_FINISHED;
    if (job->ji_job_tag) {
	LogMsg(dip, dip->di_ofp, LOG_INFO, PRT_MSGTYPE_FINISHED,
	       "Finished Job %u (%s), with status %d...\n",
	       job->ji_job_id, job->ji_job_tag, job->ji_job_status);
    } else {
	LogMsg(dip, dip->di_ofp, LOG_INFO, PRT_MSGTYPE_FINISHED,
	       "Finished Job %u, with status %d...\n",
	       job->ji_job_id, job->ji_job_status);
    }
    //if (job->ji_job_status == FAILURE) exit_status = job->ji_job_status; /* for now! */
    pthread_exit(NULL);
    return(NULL);
}

int
create_job_log(dinfo_t *dip, job_info_t *job)
{
    if (dip->di_job_log) {
	char logfmt[STRING_BUFFER_SIZE];
	char *path = logfmt;

	(void)setup_log_directory(dip, path, dip->di_log_dir, dip->di_job_log);
	if (dip->di_num_devs > 1) {
	    /*
	     * Create a unique log file with multiple devices.
	     * Note: Failure to do this leads to corrupted logs!
	     */
	    /* Add default postfix, unless user specified their own via "%". */
	    if ( strstr(dip->di_job_log, "%") == (char *) 0 ) {
		strcat(path, dip->di_file_sep);
		strcat(path, DEFAULT_JOBLOG_POSTFIX);
	    }
	}
	/* Format special control strings or log directory + log file name. */
	job->ji_job_logfile = FmtLogFile(dip, path, True);
	FreeStr(dip, dip->di_job_log);	/* Avoid unnecessary cloning! */
	dip->di_job_log = NULL;		/* The thread job log is gone! */
	if (dip->di_debug_flag) {
	    Printf(dip, "Job %u, job log file is %s...\n",
		   dip->di_job->ji_job_id, job->ji_job_logfile);
	}
	job->ji_job_logfp = fopen(job->ji_job_logfile, "w");
	if (job->ji_job_logfp == NULL) {
	    Perror(dip, "fopen() of %s failed", job->ji_job_logfile);
	    return(FAILURE);
	}
	if (dip->di_logheader_flag) {
	    log_header(dip, False);
	}
    }
    return(SUCCESS);
}

int
execute_threads(dinfo_t *mdip, dinfo_t **initial_dip, job_id_t *job_id)
{
    int status = SUCCESS, thread;
    dinfo_t *dip = *initial_dip;
    dinfo_t **dts, *tdip;
    threads_info_t *tip;
    job_info_t *job;

    job = create_job(dip);
    if (job == NULL) return(FAILURE);
    if (job_id) {
	*job_id = job->ji_job_id;
    }
    dip->di_threads_active = 0;
    dts = (dinfo_t **)Malloc(dip, (sizeof(dip) * dip->di_threads) );

    status = acquire_job_lock(dip, job);
    dip->di_job = job;
    if (dip->di_job_tag) {
	job->ji_job_tag = dip->di_job_tag;
	dip->di_job_tag = NULL;
    }
    /* Open the job log, if specified. */
    if (dip->di_job_log) {
	status = create_job_log(dip, job);
	if (status == FAILURE) {
	    release_job_lock(dip, job);
	    (void)cleanup_job(dip, job, True);
	    return(FAILURE);
	}
    }
    /* Do special job initialization (if any). */
    if (dip->di_iobf && dip->di_iobf->iob_job_init) {
	status = (*dip->di_iobf->iob_job_init)(dip, job);
	if (status == FAILURE) {
	    release_job_lock(dip, job);
	    (void)cleanup_job(dip, job, True);
	    return(FAILURE);
	}
    } else if ( (dip->di_iobehavior == DT_IO) && dip->di_iolock && (dip->di_slices == 0) ) {
#if defined(DT_IOLOCK)
	pthread_mutexattr_t *attrp = &jobs_lock_attr;
	/* Note: This moves to job init once I/O behavior implemented for dt. */
	io_global_data_t *iogp = Malloc(dip, sizeof(*iogp));
	if (iogp == NULL) {
	    release_job_lock(dip, job);
	    (void)cleanup_job(dip, job, True);
	    return(FAILURE);
	}
	if ( (status = pthread_mutex_init(&iogp->io_lock, attrp)) != SUCCESS) {
	    tPerror(dip, status, "pthread_mutex_init() of global I/O lock failed!");
	    release_job_lock(dip, job);
	    (void)cleanup_job(dip, job, True);
	    return(FAILURE);
	}
	job->ji_opaque = iogp;
	if ( (status = pthread_mutex_init(&job->ji_thread_lock, attrp)) != SUCCESS) {
	    tPerror(dip, status, "pthread_mutex_init() of thread wait lock failed!");
	    release_job_lock(dip, job);
	    (void)cleanup_job(dip, job, True);
	    return(FAILURE);
	}
#endif /* defined(DT_IOLOCK) */
    }
    /* Show the tool parameters once. */
    if (dip->di_iobf && dip->di_iobf->iob_show_parameters) {
	(*dip->di_iobf->iob_show_parameters)(dip);
    }
    job->ji_job_start = time((time_t) 0);

    /*
     * Now, create the requested threads for this job.
     */
    for (thread = 0; (thread < dip->di_threads); thread++) {
	dinfo_t *odip = NULL;
	/*
	 * Copy original information for each thread instance.
	 */
	/* Note: This is *only* for dtapp, until more cleanup! */
	if ( (dip->di_iobehavior == DTAPP_IO) && (thread == 0) ) {
	    tdip = dip;			/* Use the initial device. */
	    *initial_dip = NULL;	/* Let caller know we now own this! */
	} else {
	    tdip = clone_device(dip, False, True);
	}
	dts[thread] = tdip;
	tdip->di_thread_number = (thread + 1);
	tdip->di_thread_state = TS_STARTING;

	/* TODO: Cleanup output device processing! */
	if (odip = tdip->di_output_dinfo) {
	    /* Need these for printing functions. */
	    odip->di_thread_number = tdip->di_thread_number;
	    odip->di_thread_state = tdip->di_thread_state;
	}
	status = pthread_create( &tdip->di_thread_id, tjattrp, dip->di_thread_func, tdip );
	/*
	 * Expected Failure:
	 * EAGAIN Insufficient resources to create another thread, or a
	 * system-imposed limit on the number of threads was encountered.
	 * Linux: The kernel's system-wide limit on the number of threads,
	 * /proc/sys/kernel/threads-max, was reached. Mine reports 32478.
	 */
	if (status != SUCCESS) {
	    tdip->di_thread_state = TS_STOPPED;
	    tPerror(dip, status, "pthread_create() failed");
	    HandleExit(dip, FAILURE);
	    break;
	}
	if (tdip->di_thread_id == 0) {
	    tdip->di_thread_state = TS_STOPPED;
	    /* Why wasn't EAGAIN returned as described above? */
	    Wprintf(mdip, "No thread ID returned, breaking after %d threads!\n", thread);
	    break;
	}
	dip->di_threads_active++;
	if (CmdInterruptedFlag) break;

    } /* End of setting up for and creating threads! */

    tip = Malloc(dip, sizeof(threads_info_t));
    tip->ti_threads = dip->di_threads_active;
    tip->ti_dts = dts;

    job->ji_tinfo = tip;
    (void)insert_job(dip, job);
    
    /*
     * All commands are executed by thread(s).
     */
    if (dip->di_async_job == True) {
	pthread_t thread_id;
	if (dip->di_initial_state == IS_PAUSED) {
	    job->ji_job_state = JS_PAUSED;
	}
	status = pthread_create( &thread_id, tjattrp, a_job, job );
	if (status != SUCCESS) {
	    tPerror(mdip, status, "pthread_create() failed");
	    HandleExit(dip, FAILURE);
	}
	if (job->ji_job_tag) {
	    Printf(mdip, "Job %u (%s) started, with %u thread%s...\n",
		   job->ji_job_id, job->ji_job_tag, tip->ti_threads,
		   (tip->ti_threads > 1) ? "s" : "");
	} else {
	    Printf(mdip, "Job %u started, with %u thread%s...\n",
		   job->ji_job_id, tip->ti_threads,
		   (tip->ti_threads > 1) ? "s" : "");
	}

	status = pthread_detach(thread_id);
	if (status != SUCCESS) {
	    tPerror(mdip, status, "pthread_detach() failed");
	    HandleExit(mdip, FAILURE);
	}
	(void)sync_threads_starting(mdip, job);
    } else { /* (dip->di_async_job == False) */
	(void)sync_threads_starting(mdip, job);
	if (dip->di_iobehavior == DTAPP_IO) {
	    status = wait_for_job(mdip, job);
	} else {
	    status = wait_for_job(dip, job);
	}
    }
    /*
     * Ensure the job pointers are cleared, since this is used by logging!
     */
    if (*initial_dip) {
	dip->di_job = NULL;
	if (dip->di_output_dinfo) {
	    dip->di_output_dinfo->di_job = NULL;
	}
    }
    return(status);
}

#if !defined(INLINE_FUNCS)
int
job_threads_starting(job_info_t *job)
{
    return ( get_threads_state_count(job->ji_tinfo, TS_STARTING) );
}
#endif /* !defined(INLINE_FUNCS) */

int
threads_starting(dinfo_t *dip)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int status = SUCCESS;
    int count = 0;

    if ( QUEUE_EMPTY(jobs) ) {
	return(count);
    }
    if ( (status = acquire_jobs_lock(dip)) != SUCCESS) {
	return(status);
    }
    do {
	if (job->ji_job_state != JS_FINISHED) {
	    count += job_threads_starting(job);
	}
	if (CmdInterruptedFlag) break;
    } while ( (job = job->ji_flink) != jhdr);
    (void)release_jobs_lock(dip);
    return(count);
}

/*
 * The jobs lock was acquired before starting the thread, so after all
 * the threads have successfully started, we'll release the lock and let'em
 * all start running (or enter pause state). I added this since threads
 * starting their I/O slow down the thread creation too much. Of course,
 * thread startup will still be slow, if there are lots of other jobs.
 */
int
sync_threads_starting(dinfo_t *dip, job_info_t *job)
{
    int count, status;

    do {
	count = job_threads_starting(job);
	if (count) os_msleep(10);
	//if (CmdInterruptedFlag) break;
    } while (count);
    status = release_job_lock(dip, job);
    job->ji_threads_started = time((time_t) 0);
    if (CmdInterruptedFlag) {
	(void)stop_jobs(dip, job->ji_job_id, NULL);
    }
    return(status);
}

int
wait_for_threads(dinfo_t *mdip, threads_info_t *tip)
{
    dinfo_t 	*dip = NULL;
    void	*thread_status = NULL;
    int		thread, pstatus, status = SUCCESS;

    /*
     * Now, wait for each thread to complete.
     */
    for (thread = 0; (thread < tip->ti_threads); thread++) {
	dip = tip->ti_dts[thread];
	pstatus = pthread_join( dip->di_thread_id, &thread_status );
	tip->ti_finished++;
	if (pstatus != SUCCESS) {
	    dip->di_exit_status = FAILURE;
	    tPerror(mdip, pstatus, "pthread_join() failed");
	    /* Continue, waiting for other threads. */
	} else {
	    dip->di_thread_state = TS_JOINED;
#if !defined(WIN32)
	    /* Note: Thread status is unsigned int on Windows! */
	    if ( (thread_status == NULL) || (long)thread_status == -1 ) {
		dip->di_exit_status = FAILURE;   /* Assumed canceled, etc. */
	    } else {
		/* Note: This can probably go! */
		if (dip != (dinfo_t *)thread_status) {
		    Eprintf(mdip, "Sanity check of thread status failed for device %s!\n", dip->di_dname);
		    Fprintf(mdip, "Expected dip = "LLPXFMT", Received: "LLPXFMT"\n", dip, thread_status);
		}
	    }
#endif /* defined(WIN32) */
	}
	if (dip->di_exit_status == FAILURE) {
	    status = dip->di_exit_status;
	}
    }
    dip->di_job->ji_job_end = time((time_t) 0);
    if (dip->di_iobf) {
	dip = tip->ti_dts[0];
        if (dip->di_iobf->iob_job_finish) {
            /* Beware: Can't use mdip, if the log buffer is used! */
            /*	   Multiple async jobs cannot use the same buffer. */
            (void)(*dip->di_iobf->iob_job_finish)(dip, dip->di_job);
        }
        /* Do special job cleanup (if any). */
        if (dip->di_iobf->iob_job_cleanup) {
            (*dip->di_iobf->iob_job_cleanup)(dip, dip->di_job);
        }
#if defined(DT_IOLOCK)
	 else if ( (dip->di_iobehavior == DT_IO) && dip->di_job->ji_opaque) {
	    /* Note: This moves after I/O behavior defined for dt. */
	    io_global_data_t *iogp = dip->di_job->ji_opaque;
	    (void)pthread_mutex_destroy(&iogp->io_lock);
	    FreeMem(mdip, iogp, sizeof(*iogp));
	    dip->di_job->ji_opaque = NULL;
	}
#endif /* defined(DT_IOLOCK) */
    }
    /* Do common test processing, dump history, syslog, etc.*/
    /* Note: We may wish to control this, but for non-dt I/O, we need! */
    if ( (dip->di_iobehavior != DT_IO) && (dip->di_iobehavior != DTAPP_IO) ) {
	for (thread = 0; (thread < tip->ti_threads); thread++) {
	    dip = tip->ti_dts[thread];
	    finish_test_common(dip, dip->di_exit_status);
	}
    } else {
	dt_job_finish(tip->ti_dts[0], dip->di_job);
    }
    return (status);
}
