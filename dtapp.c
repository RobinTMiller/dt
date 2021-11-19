/*
 * Copyright 2021 NetApp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * dtapp.c - I/O Behavior for dtapp functionality.
 * 
 * Author: Robin T. Miller
 * Date Created: September 12th, 2015
 * 
 * Modification History: 
 *  
 * November 9, 2021 by Chris Nelson (nelc@netapp.com)
 *    Add MIT license, in order to distribute to FOSS community so it can
 *    be used and maintained by a larger audience, particularly for the
 *    public dt at https://github.com/RobinTMiller/dt
 *
 * May 7th, 2020 by Robin T. Miller
 *      When using step option, honor the end offset even without slices.
 * 
 * May 5th, 2020 by Robin T. Miller
 *      Use high resolution timer for more accurate I/O timing. This is
 * implemented on Windows, but Unix systems still use gettimeofday() API.
 * 
 * December 21st, 2016 by Robin T. Miller 
 *      Disable random I/O operations to avoid false data corruptions.
 *  
 * February 10th, 2016 by Robin T. Miller 
 *      Update dtapp_read_record() to use common read_record() now that
 * we've switched to pread() in the latter function.
 *      When cleaning up devices, catch the case where dips is NULL, which
 * happens when the job log cannot be opened, yet we still do cleanup. 
 *  
 * October 8th, 2015 by Robin T. Miller 
 *      Added btag CRC to write ordering extension to increase verification.
 */

#include "dt.h"

/*
 * Definitions:
 */
#define DEFAULT_THREAD_COUNT    1
#define DEFAULT_RUNTIME         0

#define DTAPP_DEFAULT_LOG_PREFIX	"%prog (j:%job t:%thread d:%devnum): "
#if defined(NETAPP)
#  define DTAPP_DEFAULT_NATE_LOG_PREFIX	"%nate (%prog j:%job t:%thread d:%devnum): "
#endif /* defined(NETAPP */

#define BTAG_NO_DEVICE_INDEX    0xFF

/*
 * dtapp Specific Information: 
 */
typedef struct dtapp_information {
    dinfo_t *dta_primary_dip;       /* The primary device information.          */
    file_type_t dta_primary_type;   /* The primary device type (input or output)*/
    int dta_current_index;          /* The current device index.                */
    int dta_input_count;	    /* The number of input devices.	   	*/
    char **dta_input_devices;       /* Pointers to input device names.  	*/
    dinfo_t **dta_input_dips;	    /* Pointers to input device information.    */
    int dta_output_count;	    /* The number of output devices.    	*/
    char **dta_output_devices;	    /* Pointers to output device names.	        */
    dinfo_t **dta_output_dips;	    /* Pointers to output device information.   */
    int dta_write_order_entries;    /* The number of write order entries.	*/
    int dta_write_order_index;      /* The current write order index.	        */
    btag_write_order_t *dta_write_orders; /* Write order table (array).         */
    btag_write_order_t *dta_last_write_order; /* Pointer to last entry.         */
} dtapp_information_t;

/*
 * Forward References: 
 */
void dtapp_help(dinfo_t *dip);

/* 
 * I/O Behavior Functions:
 */
int dtapp_initialize(dinfo_t *dip);
int dtapp_initiate_job(dinfo_t *dip);
int dtapp_initialize_devices(dinfo_t *mdip, dtapp_information_t *dtap);
dinfo_t *dtapp_initialize_input_device(dinfo_t *mdip, char *device, hbool_t master);
int dtapp_initialize_input_devices(dinfo_t *mdip, dtapp_information_t *dtap);
dinfo_t *dtapp_initialize_output_device(dinfo_t *mdip, char *device, hbool_t master);
int dtapp_initialize_output_devices(dinfo_t *mdip, dtapp_information_t *dtap);
int dtapp_parser(dinfo_t *dip, char *option);
void dtapp_cleanup_information(dinfo_t *dip);
int dtapp_clone_information(dinfo_t *dip, dinfo_t *cdip, hbool_t new_context);
int dtapp_job_finish(dinfo_t *mdip, job_info_t *job);
void *dtapp_thread(void *arg);
int dtapp_validate_parameters(dinfo_t *dip);

char *dtapp_make_device_list(char **devices, int device_count);
void dtapp_report_all_statistics(dinfo_t *tdip, dtapp_information_t *dtap, char **devices, int device_count);

int dtapp_doio(dinfo_t *dip);
int dtapp_doreadpass(dinfo_t *dip);
int dtapp_dowritepass(dinfo_t *dip);
void dtapp_report_write_stats(dinfo_t *dip);
void dtapp_do_prepass_processing(dinfo_t *dip);
int dtapp_report_pass_statistics(dinfo_t *dip, dinfo_t **dips, int device_count, stats_t stats_type, hbool_t end_of_pass);
int dtapp_finish_test(dinfo_t *dip, int exit_code, hbool_t do_cleanup);
void dtapp_finish_test_common(dinfo_t *dip, int thread_status);

int dtapp_count_devices(char *devices);
char **dtapp_clone_devices(dinfo_t *dip, char **device_list, int num_devices);
dinfo_t **dtapp_clone_dips(dinfo_t *dip, dinfo_t **dips, int num_devices);
void dtapp_free_devices(dinfo_t *dip, char **device_list, int num_devices);
void dtapp_free_dips(dinfo_t *dip, dinfo_t **dips, int num_devices);

int dtapp_close_devices(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_close_input_devices(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_close_output_devices(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_open_devices(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_open_input_devices(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_open_output_devices(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_flush_output_devices(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_lock_unlock(dinfo_t **dips, int device_count, lock_type_t lock_type, large_t data_limit);
large_t dtapp_get_data_limit(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_set_device_offset(dinfo_t *dip);
int dtapp_set_device_offsets(dinfo_t *dip, dtapp_information_t *dtap);

#define IterateInputDevices(dtap, func, arg) \
    dtapp_iterate_devices(dtap->dta_input_dips, dtap->dta_input_count, &func, arg);
#define IterateOutputDevices(dtap, func, arg) \
    dtapp_iterate_devices(dtap->dta_output_dips, dtap->dta_output_count, &func, arg);
#define IterateAllDevices(dtap, func, arg, rc) \
    if ( (rc = dtapp_iterate_devices(dtap->dta_input_dips, dtap->dta_input_count, &func, arg)) == SUCCESS) { \
        rc = dtapp_iterate_devices(dtap->dta_output_dips, dtap->dta_output_count, &func, arg); \
    }

int dtapp_iterate_devices(dinfo_t **dips, int device_count, int (*func)(dinfo_t *dip, void *arg), void *arg);
int dtapp_set_open_flags(dinfo_t *dip, void *arg);
int dtapp_end_pass(dinfo_t *dip, void *arg);
int dtapp_gather_stats(dinfo_t *dip, void *arg);
int dtapp_error_count(dinfo_t *dip, void *arg);
int dtapp_pass_count(dinfo_t *dip, void *arg);
int dtapp_report_history(dinfo_t *dip, void *arg);
int dtapp_report_pass(dinfo_t *dip, void *arg);
int dtapp_prepass_processing(dinfo_t *dip, void *arg);
int dtapp_postwrite_processing(dinfo_t *dip, void *arg);
int dtapp_start_read_pass(dinfo_t *dip, void *arg);
int dtapp_start_write_pass(dinfo_t *dip, void *arg);
int dtapp_test_startup(dinfo_t *dip, void *arg);
int dtapp_test_complete(dinfo_t *dip, void *arg);

int dtapp_parse_devices(dinfo_t *dip, dtapp_information_t *dtap);
char **dtapp_parse_device_list(dinfo_t *dip, char *devices, int num_devices);
int dtapp_setup_devices(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_setup_input_devices(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_setup_output_devices(dinfo_t *dip, dtapp_information_t *dtap);
int dtapp_setup_write_orders(dinfo_t *dip, dtapp_information_t *dtap, int entries);
void dtapp_set_write_order_entry(dinfo_t *dip, btag_t *btag);

int dtapp_common_device_setup(dinfo_t *dip);

int dtapp_read_data(dinfo_t *dip);
int dtapp_write_data(dinfo_t *dip);

int dtapp_report_btag(dinfo_t *dip, btag_t *ebtag, btag_t *rbtag, hbool_t raw_flag);
int dtapp_update_btag(dinfo_t *dip, btag_t *btag, Offset_t offset,
		      uint32_t record_index, size_t record_size, uint32_t record_number);
int dtapp_verify_btag(dinfo_t *dip, btag_t *ebtag, btag_t *rbtag,
		      uint32_t *eindex, hbool_t raw_flag);
int dtapp_verify_btag_opaque_data(dinfo_t *dip, btag_t *btag);
int verify_btag_write_order(dinfo_t *dip, btag_t *btag, size_t transfer_count);
int verify_ordered_btags(dinfo_t *dip, btag_t *btag, btag_write_order_t *wrop, void *buffer, size_t record_size, btag_t **error_btag);
void dtapp_report_ordered_btags_error(dinfo_t *dip, dinfo_t *pdip, btag_t *btag, btag_t *pbtag, btag_t *error_btag);
ssize_t dtapp_read_record(dinfo_t *dip, uint8_t *buffer, size_t bsize, size_t dsize, Offset_t offset, int *status);

static char *empty_str = "";
static char *incorrect_str = "incorrect";
static char *expected_str = "Expected";
static char *received_str = "Received";

/*
 * Declare the I/O behavior functions:
 */
iobehavior_funcs_t dtapp_iobehavior_funcs = {
    "dtapp",			/* iob_name */
    DTAPP_IO,			/* iob_iobehavior */
    NULL,               	/* iob_map_options */
    NULL,			/* iob_maptodt_name */
    NULL,			/* iob_dtmap_options */
    &dtapp_initialize,		/* iob_initialize */
    &dtapp_initiate_job,	/* iob_initiate_job */
    &dtapp_parser,		/* iob_parser */
    &dtapp_cleanup_information,	/* iob_cleanup */
    &dtapp_clone_information,	/* iob_clone */
    &dtapp_thread,		/* iob_thread */
    NULL,			/* iob_thread1 */
    NULL,			/* iob_job_init */
    NULL,			/* iob_job_cleanup */
    &dtapp_job_finish,		/* iob_job_finish */
    NULL,			/* iob_job_modify */
    NULL,       		/* iob_job_query */
    NULL,			/* iob_job_keepalive */
    NULL,			/* iob_thread_keepalive */
    NULL,                	/* iob_show_parameters */
    &dtapp_validate_parameters	/* iob_validate_parameters */
};     

/*
 * Declare the generic (default) test functions.
 */
dtfuncs_t dtapp_funcs = {
    /*	tf_open,		tf_close,		tf_initialize,	  */
	open_file,		close_file,		initialize,
    /*  tf_start_test,		tf_end_test,				  */
	init_file,		nofunc,
    /*	tf_read_file,		tf_read_data,		tf_cancel_reads,  */
	nofunc, 		dtapp_read_data,	nofunc,
    /*	tf_write_file,		tf_write_data,		tf_cancel_writes, */
	nofunc, 		dtapp_write_data,	nofunc,
    /*	tf_flush_data,		tf_verify_data,		tf_reopen_file,   */
	flush_file,		verify_data,		reopen_file,
    /*	tf_startup,		tf_cleanup,		tf_validate_opts  */
	nofunc,			nofunc,			validate_opts,
    /*	tf_report_btag,		tf_update_btag		tf_verify_btag	  */
	&dtapp_report_btag,	&dtapp_update_btag,	&dtapp_verify_btag,
};

int
dtapp_count_devices(char *devices)
{
    char *p = devices;
    int num_devices = 1;

    /* Count the devices specified. */
    while (p = strchr(p, ',')) {
	num_devices++; p++;
    }
    return(num_devices);
}

char **
dtapp_parse_device_list(dinfo_t *dip, char *devices, int num_devices)
{
    char *token, *saveptr;
    char **device_list;
    int device;

    device_list = Malloc(dip, sizeof(char **) * num_devices);
    if (device_list == NULL) return(NULL);
    if (num_devices == 1) {
        device_list[0] = strdup(devices);
        return(device_list);
    }
    /* Parse the device list. */
    /* Note: strtok_r() replaces "," with '\0'! */
    token = strtok_r(devices, ",", &saveptr);
    for (device = 0; (device < num_devices); device++) {
        device_list[device] = strdup(token);
        token = strtok_r(NULL, ",", &saveptr); /* Next device please! */
    }
    return(device_list);
}

void     
dtapp_set_iobehavior_funcs(dinfo_t *dip)
{
    dip->di_iobf = &dtapp_iobehavior_funcs;
    return;
}

/* ---------------------------------------------------------------------- */

int
dtapp_parser(dinfo_t *dip, char *option)
{
    dtapp_information_t *dtap = dip->di_opaque;
    int status = PARSE_MATCH;

    if (match(&option, "-")) {         /* Optional "-" to match dtapp options! */
        ;
    }
    if (match(&option, "help")) {
        dtapp_help(dip);
        return(STOP_PARSING);
    }
    /* Add dtapp specific parsing here... */
    return(PARSE_NOMATCH);
}

/* ---------------------------------------------------------------------- */

char *
dtapp_make_device_list(char **devices, int device_count)
{
    char buffer[STRING_BUFFER_SIZE];
    char *strp = buffer;
    int device;

    for (device = 0; (device < device_count); device++) {
        strp += sprintf(strp, "%s,", devices[device]);
    }
    strp--; *strp = '\0';
    return( strdup(buffer) );
}

void
dtapp_report_all_statistics(dinfo_t *tdip, dtapp_information_t *dtap, char **devices, int device_count)
{
    char *device_list = dtapp_make_device_list(devices, device_count);
    char *dname = tdip->di_dname;
    tdip->di_dname = device_list;
    report_stats(tdip, TOTAL_STATS);
    tdip->di_dname = dname;
    return;
}

int
dtapp_job_finish(dinfo_t *dip, job_info_t *job)
{
    dtapp_information_t *dtap;
    threads_info_t *tip = job->ji_tinfo;
    dinfo_t *tdip;
    int	device, thread;

    /*
     * Accumulate the total statistics for each thread.
     */
    for (thread = 0; (thread < tip->ti_threads); thread++) {
        tdip = tip->ti_dts[thread];
        dtap = tdip->di_opaque;
        /* Accumulate thread statistics here...*/
        device = 0;
        if ( (dtap->dta_primary_type == INPUT_FILE) && (tdip == dtap->dta_primary_dip) ) {
            device++;
            report_stats(tdip, TOTAL_STATS);
        }
        for ( ; device < dtap->dta_input_count; device++) {
            dinfo_t *idip = dtap->dta_input_dips[device];
            tdip->di_total_bytes += (idip->di_total_bytes_read + idip->di_total_bytes_written);
            tdip->di_total_files += (idip->di_total_files_read + idip->di_total_files_written);
            tdip->di_total_records += idip->di_pass_total_records;
            tdip->di_total_partial += idip->di_pass_total_partial;
            report_stats(idip, TOTAL_STATS);
        }
        if (device > 1) {
            dtapp_report_all_statistics(tdip, dtap, dtap->dta_input_devices, dtap->dta_input_count);
        }
        device = 0;
        if ( (dtap->dta_primary_type == OUTPUT_FILE) && (tdip == dtap->dta_primary_dip) ) {
            device++;
            report_stats(tdip, TOTAL_STATS);
        }
        for ( ; device < dtap->dta_output_count; device++) {
            dinfo_t *odip = dtap->dta_output_dips[device];
            tdip->di_total_bytes += (odip->di_total_bytes_read + odip->di_total_bytes_written);
            tdip->di_total_files += (odip->di_total_files_read + odip->di_total_files_written);
            tdip->di_total_records += odip->di_pass_total_records;
            tdip->di_total_partial += odip->di_pass_total_partial;
            report_stats(odip, TOTAL_STATS);
        }
        if (device > 1) {
            dtapp_report_all_statistics(tdip, dtap, dtap->dta_output_devices, dtap->dta_output_count);
        }
    }
    return(SUCCESS);
}

void *
dtapp_thread(void *arg)
{
    dinfo_t *dip = arg;
    dtapp_information_t *dtap = dip->di_opaque;
    uint64_t iterations = 0;
    hbool_t do_cleanup = False;
    int status = SUCCESS, rc;

    /* Do first, to propogate log file to all devices. */
    /* TODO: This function wants to know if we're a FS! */
    /* The FS is *not* known until devices are setup below! */
    /* Sets up file/directory names and creates top directory. */
    status = do_common_thread_startup(dip);
    if (status == FAILURE) goto thread_exit;

    /*
     * Only the 1st device is initialized, now do any others.
     */
    status = dtapp_initialize_devices(dip, dtap);
    if (status == FAILURE) goto thread_exit;

    if (dip->di_debug_flag || dip->di_tDebugFlag) {
        Printf(dip, "Starting dtapp, Job %u, Thread %u, Thread ID "OS_TID_FMT"\n",
               dip->di_job->ji_job_id, dip->di_thread_number, (os_tid_t)pthread_self());
    }

    /*
     * This does all device setup and then opens each device.
     */
    status = dtapp_setup_devices(dip, dtap);
    if (status == FAILURE) goto thread_exit;
    
    /* This is delayed to here since it needs the device type! */
    (void)do_monitor_processing(master_dinfo, dip);

    IterateAllDevices(dtap, dtapp_test_startup, NULL, rc);

    while (True) {

        PAUSE_THREAD(dip);
        if ( THREAD_TERMINATING(dip) ) break;
        if (dip->di_terminating) break;

        /* Do some I/O here... */
	status = dtapp_doio(dip);
        break;

    } /* end while(True) */

    status = dtapp_close_devices(dip, dtap);
    /*
     * Triggers may bump the error count, but the status won't be failure.
     */
    if (dip->di_error_count && (status != FAILURE) ) {
	status = FAILURE;
    }

    if (dip->di_debug_flag || dip->di_tDebugFlag) {
	Printf(dip, "I/O has completed, thread exiting with status %d...\n", status);
    }
    do_cleanup = True;

thread_exit:
    status = dtapp_finish_test(dip, status, do_cleanup);
    do_common_thread_exit(dip, status);
    /*NOT REACHED*/
    return(NULL);
}

/* Temp stuff to get us compiling! */
#define HANDLE_LOOP_ERROR(dip, error)				\
    if (error == FAILURE) {					\
	status = error;						\
	if ( THREAD_TERMINATING(dip) ||				\
	    (dip->di_error_count >= dip->di_error_limit) ) {	\
	    break;						\
	}							\
    } else if (error == WARNING) { /* No more files! */		\
	break;							\
    }
    
int
dtapp_doio(dinfo_t *dip)
{
    dtapp_information_t *dtap = dip->di_opaque;
    int rc, status = SUCCESS;
    unsigned long error_count = 0;

    while ( !THREAD_TERMINATING(dip)			&&
	    (error_count < dip->di_error_limit)         &&
	    ((dip->di_pass_count < dip->di_pass_limit) || dip->di_runtime) ) {

        IterateAllDevices(dtap, dtapp_prepass_processing, NULL, rc);

	if (dip->di_output_file) {			/* Write/read the file.	*/
            rc = dtapp_dowritepass(dip);
            HANDLE_LOOP_ERROR(dip, rc);
	} else { /* Reading only. */
            rc = dtapp_doreadpass(dip);
            HANDLE_LOOP_ERROR(dip, rc);
	} /* End of a pass! */

        error_count = 0;
        IterateAllDevices(dtap, dtapp_error_count, &error_count, rc);
    }
    return(status);
}

void
dtapp_do_prepass_processing(dinfo_t *dip)
{
    /*
     * This sets a pattern and/or the pattern buffer.
     */
    initialize_pattern(dip);

    if ( UseRandomSeed(dip) ) {
        setup_random_seeds(dip);
    }
    /* 
     * Vary the I/O Type (if requested)
     */
    if (dip->di_vary_iotype) {

	switch (rand() % NUM_IOTYPES) {

	    case RANDOM_IO:
		dip->di_io_type = RANDOM_IO;
		dip->di_random_io = True;
		break;

	    case SEQUENTIAL_IO:
		dip->di_io_type = SEQUENTIAL_IO;
		dip->di_random_io = False;
		break;
	}
    }
    if (dip->di_vary_iodir && (dip->di_io_type == SEQUENTIAL_IO) ) {

	switch (rand() % NUM_IODIRS) {

	    case FORWARD:
		dip->di_io_dir = FORWARD;
		dip->di_random_io = False;
		dip->di_io_type = SEQUENTIAL_IO;
		break;

	    case REVERSE:
		dip->di_io_dir = REVERSE;
		dip->di_random_io = True;
		dip->di_io_type = SEQUENTIAL_IO;
		break;
	}
    }
    return;
}

#define HANDLE_LOOP_ERROR_RETURN(dip, error)			\
    if (error == FAILURE) {					\
	status = error;						\
	if ( THREAD_TERMINATING(dip) ||				\
	    (dip->di_error_count >= dip->di_error_limit) ) {	\
	    return(status);					\
	}							\
    } else if (error == WARNING) { /* No more files! */		\
	return(status);						\
    }

int
dtapp_doreadpass(dinfo_t *dip)
{
    dtapp_information_t *dtap = dip->di_opaque;
    dtfuncs_t *dtf = dip->di_funcs;
    int rc, status = SUCCESS;
    unsigned long error_count = 0;

    (void)IterateInputDevices(dtap, dtapp_start_read_pass, NULL);
    /*
     * Note: User must specify random seed to repeat previous write sequence!
     */
    if ( dip->di_user_rseed && UseRandomSeed(dip) ) {
        set_rseed(dip, dip->di_random_seed);
    }
    rc = (*dtf->tf_read_data)(dip);
    if (rc == FAILURE) status = rc;
    /*
     * Prevent pass unless looping, since terminate reports
     * the total statistics when called (prevents dup stats).
     */
    if ( (dip->di_pass_limit > 1) || dip->di_runtime) {
        stats_t stats_type = READ_STATS;
        (void)IterateInputDevices(dtap, dtapp_pass_count, NULL);
        (void)IterateInputDevices(dtap, dtapp_report_pass, &stats_type);
    } else {
        IterateAllDevices(dtap, dtapp_pass_count, NULL, rc);
    }
    if (dip->di_pass_cmd) {
        rc = ExecutePassCmd(dip);
        if (rc == FAILURE) {
            status = rc;
            dip->di_error_count++;
        }
    }

    (void)IterateInputDevices(dtap, dtapp_error_count, &error_count);

    /*
     * End of a read pass, prepare for the next pass (if any).
     */
    if ( (error_count < dip->di_error_limit) &&
         ((dip->di_pass_count < dip->di_pass_limit) || dip->di_runtime) ) {

        rc = dtapp_close_devices(dip, dtap);
        HANDLE_LOOP_ERROR_RETURN(dip, rc);
        if (dip->di_bufmode_count) {
            SetupBufferingMode(dip, &dip->di_initial_flags);
            (void)IterateInputDevices(dtap, dtapp_set_open_flags, &dip->di_initial_flags);
        }
        rc = dtapp_open_devices(dip, dtap);
        HANDLE_LOOP_ERROR_RETURN(dip, rc);
    }
    (void)IterateInputDevices(dtap, dtapp_end_pass, NULL);
    return(status);
}

int
dtapp_dowritepass(dinfo_t *dip)
{
    dtapp_information_t *dtap = dip->di_opaque;
    dtfuncs_t *dtf = dip->di_funcs;
    int rc, status = SUCCESS;
    unsigned long error_count = 0;
    hbool_t do_read_pass;

restart:
    IterateAllDevices(dtap, dtapp_start_write_pass, NULL, rc);

    rc = (*dtf->tf_write_data)(dip);
    if (rc == FAILURE) status = rc;
    rc = dtapp_flush_output_devices(dip, dtap);
    if (rc == FAILURE) status = rc;

    /* Note: This may go with multiple devices! */
    /*
     * Special handling of "file system full" conditions.
     */
    if ( dip->di_fsfile_flag && dip->di_file_system_full ) {
        rc = handle_file_system_full(dip, True);
        if (rc == SUCCESS) {
            init_stats(dip);
            Wprintf(dip, "Restarting write pass after file system full detected!\n");
            goto restart;
        } else if (rc == FAILURE) {
            status = rc;
        }
        /* Note: WARNING indicates we proceed with the read pass! */
    }

    IterateAllDevices(dtap, dtapp_error_count, &error_count, rc);
    /* 
     * We finished the write pass, see if we should continue with read pass.
     */
    if ( THREAD_TERMINATING(dip) || (error_count >= dip->di_error_limit) ) {
        dtapp_report_write_stats(dip);
        return(status);
    }

    /* Note: This may not be accurate, other devices may have been written. */
    /* FYI: This was originally added to avoid reading nothing, and false errors! */
    /* REVISIT this! */
    do_read_pass = True;
    //do_read_pass = (dip->di_dbytes_written != (large_t) 0);

    /*
     * Now verify (read and compare) the data just written. 
     *  
     * Note: This is *only* executed when doing a separate read pass! 
     */
    if ( dip->di_verify_flag && do_read_pass && 
         ( (dip->di_raw_flag == False) || (dip->di_raw_flag && dip->di_reread_flag)) ) {
        stats_t stats_type;

        dtapp_report_write_stats(dip);

        rc = dtapp_close_devices(dip, dtap);
        HANDLE_LOOP_ERROR_RETURN(dip, rc);

        IterateAllDevices(dtap, dtapp_end_pass, NULL, rc);

        dip->di_initial_flags = (dip->di_read_mode | dip->di_open_flags);
        (void)IterateOutputDevices(dtap, dtapp_set_open_flags, &dip->di_initial_flags);

        rc = dtapp_open_devices(dip, dtap);
        HANDLE_LOOP_ERROR_RETURN(dip, rc);

        /*
         * Reset the random seed, so reads mimic what we wrote!
         */
        if ( UseRandomSeed(dip) ) {
            set_rseed(dip, dip->di_random_seed);
        }
        (void)IterateOutputDevices(dtap, dtapp_start_read_pass, NULL);

        rc = (*dtf->tf_read_data)(dip);
        if (rc == FAILURE) status = rc;

        stats_type = READ_STATS;
        IterateAllDevices(dtap, dtapp_pass_count, NULL, rc);
        (void)IterateOutputDevices(dtap, dtapp_report_pass, &stats_type);

        /* Nothing done with mirror devices on the read pass right now! */

    } else { /* Writing only or read-after-write enabled! */
        if ( (dip->di_pass_limit > 1) || dip->di_runtime) {
            IterateAllDevices(dtap, dtapp_pass_count, NULL, rc);
            dtapp_report_write_stats(dip);
        } else {
            IterateAllDevices(dtap, dtapp_pass_count, NULL, rc);
        }
    }

    error_count = 0;
    IterateAllDevices(dtap, dtapp_error_count, &error_count, rc);
    /* 
     * Remember, a full pass is both the write/read cycle (legacy dt way).
     */
    IterateAllDevices(dtap, dtapp_end_pass, NULL, rc);
    if ( THREAD_TERMINATING(dip) || (error_count >= dip->di_error_limit) ) {
        return(status);
    }

    /* 
     * Do post write processing to allow unmap or pass script.
     */
    rc = IterateOutputDevices(dtap, dtapp_postwrite_processing, NULL);
    if (rc == FAILURE) status = rc;

    /*
     * Don't reopen if we've reached the error limit or the pass count, since we'll 
     * be terminating shortly. Otherwise, prepare for the next write pass. (messy!)
     */
    if ( ((dip->di_pass_count < dip->di_pass_limit) || dip->di_runtime) ) {
        rc = dtapp_close_devices(dip, dtap);
        HANDLE_LOOP_ERROR_RETURN(dip, rc);
        if (dip->di_raw_flag == True) {
            dip->di_initial_flags = (dip->di_rwopen_mode | dip->di_write_flags | dip->di_open_flags);
        } else {
            dip->di_initial_flags = (dip->di_write_mode | dip->di_write_flags | dip->di_open_flags);
        }
        SetupBufferingMode(dip, &dip->di_initial_flags);
        rc = IterateOutputDevices(dtap, dtapp_set_open_flags, &dip->di_initial_flags);
        rc = dtapp_open_devices(dip, dtap);
        HANDLE_LOOP_ERROR_RETURN(dip, rc);
    }
    return(status);
}

void
dtapp_report_write_stats(dinfo_t *dip)
{
    dtapp_information_t *dtap = dip->di_opaque;
    stats_t stats_type;

    if (dip->di_raw_flag) {
        stats_type = RAW_STATS;
    } else {
        stats_type = WRITE_STATS;
    }
    (void)IterateOutputDevices(dtap, dtapp_report_pass, &stats_type);

    if (dtap->dta_input_count) {
        stats_type = MIRROR_STATS;
        (void)IterateInputDevices(dtap, dtapp_report_pass, &stats_type);
    }
    return;
}

/* 
 * TODO: Loop over all devices for per device operations!
 */
int
dtapp_finish_test(dinfo_t *dip, int exit_code, hbool_t do_cleanup)
{
    dtapp_information_t *dtap = dip->di_opaque;
    int rc, status = SUCCESS;

    /*
     * Close file, which for AIO waits for outstanding I/O's,
     * before reporting statistics so they'll be correct.
     */
    if (dip && do_cleanup && (dip->di_fd != NoFd) ) {
        dtapp_information_t *dtap = dip->di_opaque;
        status = dtapp_close_devices(dip, dtap);
	//status = (*dip->di_funcs->tf_close)(dip);
	if (status == FAILURE) exit_code = status;
    }
    IterateAllDevices(dtap, dtapp_gather_stats, NULL, rc);

    /*
     * If keep on error, do the appropriate thing!
     */
    if (dip->di_dispose_mode == KEEP_ON_ERROR) {
	/* Note: Signals cause files to be kept! */
	if ( (exit_code != SUCCESS) && (exit_code != END_OF_FILE) ) {
	    dip->di_dispose_mode = KEEP_FILE;
	} else if (dip && (dip->di_existing_file == False)) {
	    dip->di_dispose_mode = DELETE_FILE;
	}
    }
    IterateAllDevices(dtap, dtapp_test_complete, &do_cleanup, rc);
    if (rc == FAILURE) exit_code = rc;

    if ( (dip->di_eof_status_flag == False) && (exit_code == END_OF_FILE) ) {
	exit_code = SUCCESS;		/* Map end-of-file status to Success! */
    }

    dtapp_finish_test_common(dip, exit_code);

    if (exit_code == WARNING) {
	exit_code = SUCCESS;		/* Map warning errors to Success! */
    }
    /*
     * Map signal numbers and/or other errno's to FAILURE. (cleanup)
     * ( easier for scripts to handle! )
     */
    if ( (exit_code != FAILURE) && (exit_code != SUCCESS) && (exit_code != END_OF_FILE) ) {
	exit_code = FAILURE;			/* Usually a signal number. */
    }
    return(exit_code);
}

void
dtapp_finish_test_common(dinfo_t *dip, int thread_status)
{
    if (dip->di_syslog_flag) {
	SystemLog(dip, LOG_INFO, "Finished: %s", dip->di_cmd_line);
    }
    /*
     * If thread status is FAILURE, log the command line.
     * Also log to thread log when log trailer flag enabled.
     */
    if ( (thread_status == FAILURE) || dip->di_logtrailer_flag ) {
	log_header(dip, True);
    }
#if defined(NETAPP) 
    if (dip->di_debug_flag || dip->di_pDebugFlag || dip->di_tDebugFlag || dip->di_nate_flag) {
	Printf (dip, "Thread exiting with status %d...\n", thread_status);
    }
    if (dip->di_nate_flag) {
	report_nate_results(dip, thread_status);
    }
#else /* !defined(NETAPP) */
    if (dip->di_debug_flag || dip->di_pDebugFlag || dip->di_tDebugFlag) {
	Printf (dip, "Thread exiting with status %d...\n", thread_status);
    }
#endif /* defined(NETAPP) */
    return;
}

/*
 * Note: We only get called for the first device entry for each thread. 
 * Therefore, this function must cleanup the other list of devices setup. 
 * But that said, we do not free the primary device, since the common cleanup 
 * logic will do this, and we must avoid duplicate freeing. 
 */
void
dtapp_cleanup_information(dinfo_t *dip)
{
    dtapp_information_t *dtap;

    if ( (dtap = dip->di_opaque) == NULL) {
        return;
    }
    if (dtap->dta_input_count) {
	dtapp_free_devices(dip, dtap->dta_input_devices, dtap->dta_input_count);
	dtap->dta_input_devices = NULL;
	if ( (dtap->dta_primary_type == INPUT_FILE) && (dip == dtap->dta_primary_dip) ) {
            if (dtap->dta_input_dips) {
                dtap->dta_input_dips[0] = NULL; /* Caller will free this dip! */
            }
	}
	dtapp_free_dips(dip, dtap->dta_input_dips, dtap->dta_input_count);
	dtap->dta_input_dips = NULL;
	dtap->dta_input_count = 0;
    }
    if (dtap->dta_output_count) {
	dtapp_free_devices(dip, dtap->dta_output_devices, dtap->dta_output_count);
	dtap->dta_output_devices = NULL;
	if ( (dtap->dta_primary_type == OUTPUT_FILE) && (dip == dtap->dta_primary_dip) ) {
            if (dtap->dta_output_dips) {
                dtap->dta_output_dips[0] = NULL; /* Caller will free this dip! */
            }
	}
	dtapp_free_dips(dip, dtap->dta_output_dips, dtap->dta_output_count);
	dtap->dta_output_dips = NULL;
	dtap->dta_output_count = 0;
    }
    if (dtap->dta_write_orders) {
        Free(dip, dtap->dta_write_orders);
        dtap->dta_write_orders = NULL;
        dtap->dta_last_write_order = NULL;
    }
    FreeMem(dip, dtap, sizeof(*dtap));
    dip->di_opaque = NULL;
    return;
}

void
dtapp_free_devices(dinfo_t *dip, char **device_list, int num_devices)
{
    int device;

    if (num_devices == 0) return;
    for (device = 0; (device < num_devices); device++) {
        FreeStr(dip, device_list[device]);
        device_list[device] = NULL;
    }
    Free(dip, device_list);
    return;
}

void
dtapp_free_dips(dinfo_t *dip, dinfo_t **dips, int num_devices)
{
    int device;

    if (num_devices == 0) return;
    if (dips == NULL) return;
    for (device = 0; (device < num_devices); device++) {
	dinfo_t *cdip;
	/* Free the device information we created. */
	if (cdip = dips[device]) {
	    cdip->di_opaque = NULL;         /* Avoid recursion. */
            cdip->di_log_opened = False;    /* Master will close log file! */
	    cleanup_device(cdip, False);
	    FreeMem(dip, cdip, sizeof(*cdip));
	    dips[device] = NULL;
	}
    }
    Free(dip, dips);
    return;
}

/*
 * We are called each time a device is cloned, to duplicate per device/thread 
 * information. The new contect flag is set when new threads are executed, so 
 * for this I/O behavior, it lets us know we need a new device list context. 
*/
int
dtapp_clone_information(dinfo_t *dip, dinfo_t *cdip, hbool_t new_context)
{
    /*
     * Each thread needs its' own copy of the per thread information. 
     * Each device within a thread share the primary device information! 
     */
    if (new_context == True) {
        dtapp_information_t *dtap = dip->di_opaque;
        dtapp_information_t *cdtap; /* clone */

        cdtap = Malloc(dip, sizeof(*cdtap));
        if (cdtap == NULL) return(FAILURE);

        cdip->di_opaque = cdtap;
        *cdtap = *dtap;

        cdtap->dta_primary_dip = cdip;
        if (dtap->dta_input_count) {
            cdtap->dta_input_devices = dtapp_clone_devices(dip, dtap->dta_input_devices, dtap->dta_input_count);
        }
        if (dtap->dta_output_count) {
            cdtap->dta_output_devices = dtapp_clone_devices(dip, dtap->dta_output_devices, dtap->dta_output_count);
        }
        /* TODO: Clone all per thread allocated data! */
    }
    return(SUCCESS);
}

char **
dtapp_clone_devices(dinfo_t *dip, char **device_list, int num_devices)
{
    int device;
    char **cdevice_list;

    if (num_devices == 0) return(NULL);
    cdevice_list = Malloc(dip, sizeof(char **) * num_devices);
    if (cdevice_list == NULL) return(NULL);
    for (device = 0; (device < num_devices); device++) {
        cdevice_list[device] = strdup(device_list[device]);
    }
    return(cdevice_list);
}

dinfo_t **
dtapp_clone_dips(dinfo_t *dip, dinfo_t **dips, int num_devices)
{
    int device;
    dinfo_t **cdips;

    if (num_devices == 0) return(NULL);
    cdips = Malloc(dip, sizeof(dinfo_t **) * num_devices);
    if (cdips == NULL) return(NULL);
    for (device = 0; (device < num_devices); device++) {
        cdips[device] = dips[device];
    }
    return(cdips);
}

int
dtapp_initialize(dinfo_t *dip)
{
    dtapp_information_t *dtap;

    dtap = Malloc(dip, sizeof(*dtap));
    if (dtap == NULL) return(FAILURE);
    dip->di_opaque = dtap;

    dip->di_btag_flag = True;
    dip->di_fsalign_flag = True;
    dip->di_dispose_mode = KEEP_FILE;
    dip->di_open_flags |= O_DIRECT;
    dip->di_raw_flag = True;
    dip->di_dio_flag = True;
    dip->di_threads = DEFAULT_THREAD_COUNT;
    dip->di_runtime = DEFAULT_RUNTIME;

    /*
     * Set the functions this I/O behavior will use.
     */
    dip->di_funcs = &dtapp_funcs;
    return(SUCCESS);
}

/*
 * This is called after parsing to start a job with threads, so we are 
 * in the master threads' context. 
 *  
 * Inputs: 
 *      mdip = The master device information.
 *  
 * Return Value:
 *      Returns SUCCESS or FAILURE. 
 */
int
dtapp_initiate_job(dinfo_t *mdip)
{
    dtapp_information_t *dtap = mdip->di_opaque;
    dinfo_t *dip = NULL;
    int device = 0, status = SUCCESS;
    
#if defined(NETAPP)
    if (mdip->di_log_prefix == NULL) {
        if (mdip->di_nate_flag == False) {
            mdip->di_log_prefix = strdup(DTAPP_DEFAULT_LOG_PREFIX);
        } else {
            mdip->di_log_prefix = strdup(DTAPP_DEFAULT_NATE_LOG_PREFIX);
        }
    }
#else /* defined(NETAPP) */
    if (mdip->di_log_prefix == NULL) {
        mdip->di_log_prefix = strdup(DTAPP_DEFAULT_LOG_PREFIX);
    }
#endif /* defined(NETAPP) */
    /*
     * Note: This order is important, since we want the output device to 
     * be the thread device information, since we wish to do writes first. 
     */
    if (dtap->dta_output_count) {
        dip = dtapp_initialize_output_device(mdip, dtap->dta_output_devices[device], True);
        if (dip == NULL) return(FAILURE);
        dtap = dip->di_opaque;
        dtap->dta_primary_dip = dip;
        dtap->dta_primary_type = OUTPUT_FILE;
    } else if (dtap->dta_input_count) {
        mdip->di_output_file = NULL;
        dip = dtapp_initialize_input_device(mdip, dtap->dta_input_devices[device], True);
        if (dip == NULL) return(FAILURE);
        dtap = dip->di_opaque;
        dtap->dta_primary_dip = dip;
        dtap->dta_primary_type = INPUT_FILE;
    }

    if (mdip->di_syslog_flag) {
	SystemLog(mdip, LOG_INFO, "Starting: %s", dip->di_cmd_line);
    }

    status = execute_threads(mdip, &dip, NULL);

    if (dip) {
        cleanup_device(dip, False);
        FreeMem(mdip, dip, sizeof(*dip));
    }
    mdip->di_opaque = NULL;
    return(status);
}

int
dtapp_initialize_devices(dinfo_t *mdip, dtapp_information_t *dtap)
{
    int status = SUCCESS;

    /*
     * Order is important, since we wish to progate output information 
     * to the input (mirror) devices, such as the UUID (if any). 
     */
    if ( (status == SUCCESS) && dtap->dta_output_count) {
	status = dtapp_initialize_output_devices(mdip, dtap);
    }
    if (dtap->dta_input_count) {
	status = dtapp_initialize_input_devices(mdip, dtap);
    }
    return(status);
}

dinfo_t *
dtapp_initialize_input_device(dinfo_t *mdip, char *device, hbool_t master)
{
    dinfo_t *idip;

    idip = clone_device(mdip, master, False);
    if (idip == NULL) return(NULL);
    FreeStr(mdip, idip->di_input_file);
    idip->di_input_file = strdup(device);
    FreeStr(mdip, idip->di_dname);
    idip->di_dname = strdup(idip->di_input_file);
    if (idip->di_output_file) {
        FreeStr(idip, idip->di_output_file);
        idip->di_output_file = NULL;
    }
    idip->di_mode = READ_MODE;
    idip->di_ftype = INPUT_FILE;
    idip->di_device_number = 0;
    return(idip);
}

int
dtapp_initialize_input_devices(dinfo_t *mdip, dtapp_information_t *dtap)
{
    dinfo_t *idip, *odip = NULL;
    int device = 0;
    
    dtap->dta_input_dips = Malloc(mdip, sizeof(dinfo_t) * dtap->dta_input_count);
    if (dtap->dta_input_dips == NULL) return(FAILURE);

    if ( (dtap->dta_primary_type == INPUT_FILE) && (mdip == dtap->dta_primary_dip) ) {
        dtap->dta_input_dips[device++] = mdip;
    }

    for ( ; (device < dtap->dta_input_count); device++) {
        idip = dtapp_initialize_input_device(mdip, dtap->dta_input_devices[device], False);
        if (idip == NULL) return(FAILURE);
	dtap->dta_input_dips[device] = idip;
        idip->di_device_number = device;
        if (dtap->dta_output_count) {
            odip = dtap->dta_output_dips[device];
            idip->di_uuid_string = strdup(odip->di_uuid_string);
        }
    }
    return(SUCCESS);
}

dinfo_t *
dtapp_initialize_output_device(dinfo_t *mdip, char *device, hbool_t master)
{
    dinfo_t *odip;

    odip = clone_device(mdip, master, False);
    if (odip == NULL) return(NULL);
    FreeStr(mdip, odip->di_output_file);
    odip->di_output_file = strdup(device);
    FreeStr(mdip, odip->di_dname);
    odip->di_dname = strdup(odip->di_output_file);
    if (odip->di_input_file) {
        FreeStr(odip, odip->di_input_file);
        odip->di_input_file = NULL;
    }
    odip->di_mode = WRITE_MODE;
    odip->di_ftype = OUTPUT_FILE;
    odip->di_device_number = 0;
    if (odip->di_uuid_string == NULL) {
        odip->di_uuid_string = os_get_uuid(odip->di_uuid_dashes);
    }
    return(odip);
}

int
dtapp_initialize_output_devices(dinfo_t *mdip, dtapp_information_t *dtap)
{
    dinfo_t *odip;
    int device = 0;

    dtap->dta_output_dips = Malloc(mdip, sizeof(dinfo_t) * dtap->dta_output_count);
    if (dtap->dta_output_dips == NULL) return(FAILURE);

    if ( (dtap->dta_primary_type == OUTPUT_FILE) && (mdip == dtap->dta_primary_dip) ) {
        dtap->dta_output_dips[device++] = mdip;
    }

    for ( ; (device < dtap->dta_output_count); device++) {
        odip = dtapp_initialize_output_device(mdip, dtap->dta_output_devices[device], False);
        if (odip == NULL) return(FAILURE);
	dtap->dta_output_dips[device] = odip;
        odip->di_device_number = device;
    }
    return(SUCCESS);
}

int
dtapp_close_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int status = SUCCESS, rc;

    if (dtap->dta_input_count) {
	rc = dtapp_close_input_devices(dip, dtap);
	if (rc == FAILURE) status = rc;
    }
    if (dtap->dta_output_count) {
	rc = dtapp_close_output_devices(dip, dtap);
	if (rc == FAILURE) status = rc;
    }
    return(status);
}

int
dtapp_close_input_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int device, status = SUCCESS, rc;

    for (device = 0; (device < dtap->dta_input_count); device++) {
	dinfo_t *idip = dtap->dta_input_dips[device];
        if (idip->di_fd == NoFd) continue;
	rc = (*idip->di_funcs->tf_close)(idip);
	if (rc == FAILURE) status = rc;
    }
    return(status);
}

int
dtapp_close_output_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int device, status = SUCCESS, rc;

    for (device = 0; (device < dtap->dta_output_count); device++) {
	dinfo_t *odip = dtap->dta_output_dips[device];
        if (odip->di_fd == NoFd) continue;
	rc = (*odip->di_funcs->tf_close)(odip);
	if (rc == FAILURE) status = rc;
    }
    return(status);
}

int
dtapp_lock_unlock(dinfo_t **dips, int device_count, lock_type_t lock_type, large_t data_limit)
{
    int device, status = SUCCESS;

    for (device = 0; (device < device_count); device++) {
        dinfo_t *dip = dips[device];
        status = dt_lock_unlock(dip, dip->di_dname, &dip->di_fd,
                                lock_type, dip->di_file_position, (Offset_t)data_limit);
        if (status == FAILURE) break;
    }
    return(status);
}

int
dtapp_iterate_devices(dinfo_t **dips, int device_count, int (*func)(dinfo_t *dip, void *arg), void *arg)
{
    int device, status = SUCCESS;

    for (device = 0; (device < device_count); device++) {
        dinfo_t *dip = dips[device];
        status = (*func)(dip, arg);
        if (status == FAILURE) break;
    }
    return(status);
}
int
dtapp_test_startup(dinfo_t *dip, void *arg)
{
    do_common_startup_logging(dip);
    do_setup_keepalive_msgs(dip);
    dip->di_program_start = time((time_t) 0);
    dip->di_start_time = times(&dip->di_stimes);
    gettimeofday(&dip->di_start_timer, NULL);
    /* Prime the keepalive time, if enabled. */
    if (dip->di_keepalive_time) {
	dip->di_last_keepalive = time((time_t *)0);
    }
    if (dip->di_runtime) {
        dip->di_runtime_end = time((time_t *)NULL) + dip->di_runtime;
    }
    return(SUCCESS);
}

int
dtapp_set_open_flags(dinfo_t *dip, void *arg)
{
    int *open_flags = arg;
    dip->di_initial_flags = *open_flags;
    return(SUCCESS);
}

int
dtapp_end_pass(dinfo_t *dip, void *arg)
{
    dip->di_end_time = times(&dip->di_etimes);
    gettimeofday(&dip->di_end_timer, NULL);
    return(SUCCESS);
}

int
dtapp_gather_stats(dinfo_t *dip, void *arg)
{
    gather_stats(dip);      /* Gather the device statistics.    */
    gather_totals(dip);     /* Update the total statistics.	*/
    return(SUCCESS);
}

int
dtapp_error_count(dinfo_t *dip, void *arg)
{
    unsigned long *error_count = arg;
    *error_count += dip->di_error_count;
    return(SUCCESS);
}

int
dtapp_pass_count(dinfo_t *dip, void *arg)
{
    dip->di_pass_count++;
    return(SUCCESS);
}

int
dtapp_report_pass(dinfo_t *dip, void *arg)
{
    stats_t *stats_type = arg;

    report_pass(dip, *stats_type);
    return(SUCCESS);
}

int
dtapp_report_history(dinfo_t *dip, void *arg)
{
    if (dip->di_history_size) {
        dump_history_data(dip);
    }
    return(SUCCESS);
}

int
dtapp_prepass_processing(dinfo_t *dip, void *arg)
{
    dtapp_do_prepass_processing(dip);
    return(SUCCESS);
}

int
dtapp_postwrite_processing(dinfo_t *dip, void *arg)
{
    return( do_postwrite_processing(dip) );
}

int
dtapp_start_read_pass(dinfo_t *dip, void *arg)
{
    dip->di_mode = READ_MODE;
    dip->di_pass_time = times(&dip->di_ptimes);
    gettimeofday(&dip->di_pass_timer, NULL);
    dip->di_read_pass_start = time(NULL);
    dip->di_pattern_bufptr = dip->di_pattern_buffer;
    return(SUCCESS);
}

int
dtapp_start_write_pass(dinfo_t *dip, void *arg)
{
    dip->di_mode = WRITE_MODE;
    dip->di_pass_time = times(&dip->di_ptimes);
    gettimeofday(&dip->di_pass_timer, NULL);
    dip->di_write_pass_start = time(NULL);
    if (dip->di_raw_flag == True) {
        dip->di_read_pass_start = dip->di_write_pass_start;
    }
    return(SUCCESS);
}

int
dtapp_test_complete(dinfo_t *dip, void *arg)
{
    hbool_t *do_cleanup = arg;
    int status = SUCCESS;

    if ( *do_cleanup && dip->di_output_file && dip->di_fsfile_flag &&
         (dip->di_io_mode == TEST_MODE) && (dip->di_dispose_mode == DELETE_FILE) ) {
        status = delete_files(dip, True);
    }

    if (dip->di_history_size &&
        (dip->di_history_dump == True) && (dip->di_history_dumped == False) ) {
        dump_history_data(dip);
    }
    return(status);
}

int
dtapp_set_device_offsets(dinfo_t *dip, dtapp_information_t *dtap)
{
    int device;
    int device_count;
    dinfo_t **dips;
    int status = SUCCESS;

    if (device_count = dtap->dta_output_count) {
        dips = dtap->dta_output_dips;
        for (device = 0; (device < device_count); device++) {
            dinfo_t *odip = dips[device];
            status = dtapp_set_device_offset(odip);
            if (odip->di_io_mode == MIRROR_MODE) {
                dinfo_t *idip = dtap->dta_input_dips[device];
                status = dtapp_set_device_offset(idip);
                if (idip->di_offset != odip->di_offset) {
                    /* Set input device to same offset as output device. */
                    idip->di_offset = set_position(idip, odip->di_offset, False);
                }
            }
        }
    } else if (device_count = dtap->dta_input_count) {
        dips = dtap->dta_input_dips;
        for (device = 0; (device < device_count); device++) {
            dinfo_t *idip = dips[device];
            status = dtapp_set_device_offset(idip);
            /* Prime the common btag data, except for IOT pattern. */
            if ( (idip->di_btag_flag == True) && (idip->di_iot_pattern == False) ) {
                update_btag(idip, idip->di_btag, idip->di_offset,
                            (uint32_t)0, (size_t)0, (idip->di_records_read + 1));
            }
        }
    }
    return(SUCCESS);
}

large_t
dtapp_get_data_limit(dinfo_t *dip, dtapp_information_t *dtap)
{
    int device;
    int device_count;
    dinfo_t **dips;
    large_t data_limit = 0;
    int status = SUCCESS;

    if (dip->di_user_limit) {
        return(dip->di_user_limit);
    }
    if (device_count = dtap->dta_output_count) {
        dips = dtap->dta_output_dips;
        for (device = 0; (device < device_count); device++) {
            dinfo_t *odip = dips[device];
            data_limit += get_data_limit(odip);
        }
    } else if (device_count = dtap->dta_input_count) {
        dips = dtap->dta_input_dips;
        for (device = 0; (device < device_count); device++) {
            dinfo_t *idip = dips[device];
            data_limit += get_data_limit(idip);
        }
    }
    return(data_limit);
}

int
dtapp_set_device_offset(dinfo_t *dip)
{
    int status = SUCCESS;

    dip->di_maxdata_reached = False;
    if ( dip->di_lbdata_addr && !dip->di_user_position && isDiskDevice(dip) ) {
        dip->di_file_position = make_position(dip, dip->di_lbdata_addr);
        if ( (dip->di_io_type == RANDOM_IO) && (dip->di_rdata_limit <= (large_t)dip->di_file_position) ) {
            Eprintf(dip, "Please specify a random data limit > lba file position!\n");
            return(FAILURE);
        }
    }
    if ( (dip->di_io_type == SEQUENTIAL_IO) && (dip->di_io_dir == REVERSE) ) {
        dip->di_offset = set_position(dip, (Offset_t)dip->di_rdata_limit, False);
    } else if (dip->di_file_position) {
        /* File position set by slices or via user request (lba= or offset=). */
        dip->di_offset = set_position(dip, dip->di_file_position, False);
    } else {
        dip->di_offset = get_position(dip);
    }
    if (dip->di_offset == (Offset_t)FAILURE) {
        status = FAILURE;
    }
    return(status);
}

int
dtapp_open_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int status = SUCCESS, rc;

    if (dtap->dta_input_count) {
	rc = dtapp_open_input_devices(dip, dtap);
	if (rc == FAILURE) status = rc;
    }
    if (dtap->dta_output_count) {
	rc = dtapp_open_output_devices(dip, dtap);
	if (rc == FAILURE) status = rc;
    }
    return(status);
}

int
dtapp_open_input_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int device, status = SUCCESS, rc;

    for (device = 0; (device < dtap->dta_input_count); device++) {
	dinfo_t *idip;
	idip = dtap->dta_input_dips[device];
        if (idip->di_fd != NoFd) {
            rc = (*idip->di_funcs->tf_close)(idip);
            if (rc == FAILURE) status = rc;
        }
        rc = (*idip->di_funcs->tf_open)(idip, idip->di_initial_flags);
	if (rc == FAILURE) status = rc;
    }
    return(status);
}

int
dtapp_open_output_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int device, status = SUCCESS, rc;

    for (device = 0; (device < dtap->dta_output_count); device++) {
	dinfo_t *odip;
	odip = dtap->dta_output_dips[device];
        if (odip->di_fd != NoFd) {
            rc = (*odip->di_funcs->tf_close)(odip);
            if (rc == FAILURE) status = rc;
        }
        rc = (*odip->di_funcs->tf_open)(odip, odip->di_initial_flags);
	if (rc == FAILURE) status = rc;
    }
    return(status);
}

int
dtapp_flush_output_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int device, status = SUCCESS, rc;

    for (device = 0; (device < dtap->dta_output_count); device++) {
	dinfo_t *odip;
	odip = dtap->dta_output_dips[device];
        if (odip->di_fd == NoFd) continue;
        rc = (*odip->di_funcs->tf_flush_data)(odip);
	if (rc == FAILURE) status = rc;
    }
    return(status);
}

int
dtapp_parse_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int status = SUCCESS;

    if (dip->di_input_file) {
        dtap->dta_input_count = dtapp_count_devices(dip->di_input_file);
    }
    if (dip->di_output_file) {
        dtap->dta_output_count = dtapp_count_devices(dip->di_output_file);
    }
    /* Both input & output devices permitted for mirror mode. */
    if (dtap->dta_input_count && dtap->dta_output_count) {
        if (dtap->dta_input_count != dtap->dta_output_count) {
            Eprintf(dip, "The number of input devices (%d) must match the output devices (%d)!\n",
                    dtap->dta_input_count, dtap->dta_output_count);
            return(FAILURE);
        }
        dip->di_io_mode = MIRROR_MODE;
        dip->di_multiple_devs = True;
    }
    if (dtap->dta_input_count) {
        dtap->dta_input_devices = dtapp_parse_device_list(dip, dip->di_input_file, dtap->dta_input_count);
    }
    if (dtap->dta_output_count) {
        dtap->dta_output_devices = dtapp_parse_device_list(dip, dip->di_output_file, dtap->dta_output_count);
    }
    if (dtap->dta_input_count) {
        ; // dip->di_output_file = NULL; /* Avoid cloning, we'll setup the output files ourselves! */
    }
    return(status);
}

int
dtapp_setup_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int status = SUCCESS;

    /* Output first, to create mirror files (as required). */
    if (dtap->dta_output_count) {
	status = dtapp_setup_output_devices(dip, dtap);
        if (status == FAILURE) return(status);
    }
    if (dtap->dta_input_count) {
	status = dtapp_setup_input_devices(dip, dtap);
    }
    return(status);
}

int
dtapp_setup_input_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int device, status = SUCCESS;

    for (device = 0; (device < dtap->dta_input_count); device++) {
	dinfo_t *idip;
	idip = dtap->dta_input_dips[device];
        status = do_datatest_initialize(idip);
        if (status == FAILURE) break;
	status = setup_device_info(idip, idip->di_input_file, idip->di_input_dtype);
	if (status == FAILURE) break;
        status = dtapp_common_device_setup(idip);
	if (status == FAILURE) break;
        status = (*idip->di_funcs->tf_open)(idip, idip->di_initial_flags);
        if (status == FAILURE) break;
    }
    return(status);
}

int
dtapp_setup_output_devices(dinfo_t *dip, dtapp_information_t *dtap)
{
    int device, status = SUCCESS;

    for (device = 0; (device < dtap->dta_output_count); device++) {
	dinfo_t *odip;
	odip = dtap->dta_output_dips[device];
        status = do_datatest_initialize(odip);
        if (status == FAILURE) break;
	status = setup_device_info(odip, odip->di_output_file, odip->di_output_dtype);
	if (status == FAILURE) break;
        status = dtapp_common_device_setup(odip);
	if (status == FAILURE) break;
        status = (*odip->di_funcs->tf_open)(odip, odip->di_initial_flags);
        if (status == FAILURE) break;
        odip->di_open_flags &= ~O_CREAT;
    }
    return(status);
}

int
dtapp_common_device_setup(dinfo_t *dip)
{
    int status = SUCCESS;

    if (dip->di_fsfile_flag == True) {
        status = do_filesystem_setup(dip);
        if (status == FAILURE) return(status);
    }

    /* 
     * Note: This initializes File System & SCSI information.
     */
    status = do_common_device_setup(dip);
    if (status == FAILURE) return(status);

    status = do_common_file_system_setup(dip);
    if (status == FAILURE) return(status);

    status = (*dip->di_funcs->tf_validate_opts)(dip);
    if (status == FAILURE) return(status);

    status = (*dip->di_funcs->tf_initialize)(dip);
    if (status == FAILURE) return(status);

    if (dip->di_slice_number) {
        status = init_slice(dip, dip->di_slice_number);
    } else if (dip->di_slices) {
        status = init_slice(dip, dip->di_thread_number);
    }
    if (status == FAILURE) return(status);

    status = setup_thread_names(dip);
    if (status == FAILURE) return(status);

    if (dip->di_btag_flag == True) {
        dip->di_btag = initialize_btag(dip, OPAQUE_WRITE_ORDER_TYPE);
        if (dip->di_btag == NULL) dip->di_btag_flag = False;
    }

    status = initialize_prefix(dip);
    if (status == FAILURE) return(status);

    if (dip->di_fsfile_flag == True) {
        dip->di_protocol_version = os_get_protocol_version(dip->di_fd);
    }
    return(status);
}

int
dtapp_setup_write_orders(dinfo_t *dip, dtapp_information_t *dtap, int entries)
{
    int status = SUCCESS;

    dtap->dta_write_order_index = 0;
    dtap->dta_write_order_entries = entries;
    if (dtap->dta_write_orders) {
        Free(dip, dtap->dta_write_orders);
    }
    dtap->dta_write_orders = Malloc(dip, sizeof(btag_write_order_t) * entries);
    if (dtap->dta_write_orders == NULL) {
        status = FAILURE;
    } else {
        btag_write_order_t *wrop = &dtap->dta_write_orders[0];
        wrop->wro_device_index = BTAG_NO_DEVICE_INDEX;
        dtap->dta_last_write_order = wrop;
    }
    return(status);
}

void
dtapp_set_write_order_entry(dinfo_t *dip, btag_t *btag)
{
    dtapp_information_t *dtap = dip->di_opaque;
    btag_write_order_t *wrop;

    if (dtap->dta_write_orders == NULL) return;
    wrop = &dtap->dta_write_orders[dtap->dta_write_order_index];
    /* Note: btag is already in correct machine endian format! */
    wrop->wro_device_index = dip->di_device_number;
    if ( isDiskDevice(dip) ) {
        uint64_t lba = LtoH64(btag->btag_lba);
        wrop->wro_write_offset = HtoL64(lba * dip->di_dsize);
    } else {
        wrop->wro_write_offset = btag->btag_offset;
    }
    wrop->wro_write_size = btag->btag_record_size;
    wrop->wro_write_secs = btag->btag_write_secs;
    wrop->wro_write_usecs = btag->btag_write_usecs;
    wrop->wro_crc32 = btag->btag_crc32;
    dtap->dta_last_write_order = wrop;
    dtap->dta_write_order_index++;
    if (dtap->dta_write_order_index == dtap->dta_write_order_entries) {
        dtap->dta_write_order_index = 0;
    }
    return;
}

/*
 * This function is called to validate user options.
 */
int
dtapp_validate_parameters(dinfo_t *dip)
{
    dtapp_information_t *dtap = dip->di_opaque;
    int status;

    status = dtapp_parse_devices(dip, dtap);
    if (status == FAILURE) return(status);

    /* 
     * Set/reset options we are not supporting!
     */
    dip->di_aio_flag = False;
    dip->di_aio_bufs = 0;
    dip->di_delete_per_pass = False;
    dip->di_file_limit = 0;
    dip->di_user_subdir_limit = 0;
    dip->di_user_subdir_depth = 0;

    /*
     * Since we do *not* track random I/O's, we cannot allow random overwrites,
     * otherwise we'll report false data corruptions! 
     */
    if ( (dip->di_bypass_flag == False) &&
	 (dip->di_vary_iotype || (dip->di_io_type == RANDOM_IO)) ) {
	Wprintf(dip, "Disabling random I/O operations, since not supported with dtapp!\n");
	dip->di_io_type = SEQUENTIAL_IO;
	dip->di_random_io = False;
	dip->di_vary_iotype = False;
    }
    return(status);
}

#define P	Print

void
dtapp_help(dinfo_t *dip)
{
    dtapp_information_t *dtap = dip->di_opaque;

    P(dip, "Usage: %s iobehavior=dtapp [options...]\n", cmdname);
    P(dip, "\nOptions:\n");
    P(dip, "\thelp                    Show this help text, then exit.\n");
    P(dip, "\tversion                 Print the version, then exit.\n");
    /* Add dtapp specific help here! */
    P(dip, "\n");
    return;
}

/* ============================================================================= */

int
dtapp_read_data(dinfo_t *dip)
{
    dtapp_information_t *dtap = dip->di_opaque;
    dinfo_t *idip = NULL;
    dinfo_t **dips;
    int device_count;
    register ssize_t count;
    register size_t bsize, dsize;
    Offset_t lock_offset = 0;
    hbool_t lock_full_range = False;
    lbdata_t lba = 0;
    uint32_t loop_usecs;
    large_t data_limit;
    large_t fbytes_read = 0, records_read = 0;
    struct timeval loop_start_time, loop_end_time;
    unsigned long error_count = 0;
    int status = SUCCESS;

    if (dip->di_ftype == INPUT_FILE) {
        dips = dtap->dta_input_dips;
        device_count = dtap->dta_input_count;
    } else { /* OUTPUT_FILE */
        dips = dtap->dta_output_dips;
        device_count = dtap->dta_output_count;
    }

    dsize = get_data_size(dip, READ_OP);
    data_limit = dtapp_get_data_limit(dip, dtap);

    status = dtapp_set_device_offsets(dip, dtap);
    if (status == FAILURE) return(status);
    
    if ( (dip->di_lock_files == True) && dt_test_lock_mode(dip, LOCK_RANGE_FULL) ) {
	lock_full_range = True;
        status = dtapp_lock_unlock(dips, device_count, LOCK_TYPE_READ, data_limit);
	if (status == FAILURE) return(status);
    }
    if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	dip->di_actual_total_usecs = 0;
	dip->di_target_total_usecs = 0;
    }

    /*
     * Now read and optionally verify the input records.
     */
    while ( (error_count < dip->di_error_limit) &&
	    (fbytes_read < data_limit) &&
	    (records_read < dip->di_record_limit) ) {

	PAUSE_THREAD(dip);
	if ( THREAD_TERMINATING(dip) ) break;
	if (dip->di_terminating) break;

        dtap->dta_current_index = rnd(dip, 0, (device_count - 1));
        idip = dips[dtap->dta_current_index];

	if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	    highresolutiontime(&loop_start_time, NULL);
	    if (dip->di_records_read) {
		/* Adjust the actual usecs to adjust for possible usleep below! */
		dip->di_actual_total_usecs += timer_diff(&loop_end_time, &loop_start_time);
	    }
	}

	if ( idip->di_max_data && (idip->di_maxdata_read >= idip->di_max_data) ) {
	    idip->di_maxdata_reached = True;
	    break;
	}

	if ( idip->di_volumes_flag &&
             (idip->di_multi_volume >= idip->di_volume_limit) &&
             (idip->di_volume_records >= idip->di_volume_records) ) {
	    break;
	}

	if (idip->di_read_delay) {			/* Optional read delay.	*/
	    mySleep(dip, idip->di_read_delay);
	}

	/*
	 * If data limit was specified, ensure we don't exceed it.
	 */
        /* Note: With slices, we MUST honor the per device limits! */
        if ( (idip->di_lbytes_read + dsize) > idip->di_data_limit) {
            bsize = (size_t)(idip->di_data_limit - idip->di_lbytes_read);
            assert(bsize < idip->di_data_alloc_size);
            if (bsize == 0) {
                set_Eof(idip);
                break;
            }
        } else {
            bsize = dsize;
        }

	if ( (idip->di_io_type == SEQUENTIAL_IO) && (idip->di_io_dir == REVERSE) ) {
	    bsize = (size_t)MIN((idip->di_offset - idip->di_file_position), (Offset_t)bsize);
	    idip->di_offset = set_position(idip, (Offset_t)(idip->di_offset - bsize), False);
	} else if (idip->di_io_type == RANDOM_IO) {
	    /*
	     * BEWARE: The size *must* match the write size, or you'll get
	     * a different offset, since the size is used in calculations.
	     */
	    idip->di_offset = do_random(idip, True, bsize);
	}

        if (idip->di_debug_flag && (bsize != dsize) && !idip->di_variable_flag) {
            Printf (idip, "Record #%lu, Reading a partial record of %lu bytes...\n",
                                    (idip->di_records_read + 1), bsize);
        }

	if (idip->di_iot_pattern || idip->di_lbdata_flag) {
	    lba = make_lbdata(idip, (Offset_t)(idip->di_volume_bytes + idip->di_offset));
	} else {
            lba = make_lbdata(idip, idip->di_offset);
	}

	/*
	 * If requested, rotate the data buffer through ROTATE_SIZE bytes
	 * to force various unaligned buffer accesses.
	 */
	if (idip->di_rotate_flag) {
	    idip->di_data_buffer = (idip->di_base_buffer + (idip->di_rotate_offset++ % ROTATE_SIZE));
	}

	/*
	 * If we'll be doing a data compare after the read, then
	 * fill the data buffer with the inverted pattern to ensure
	 * the buffer actually gets written into (driver debug mostly).
	 */
	if ( (idip->di_io_mode == TEST_MODE) && (idip->di_compare_flag == True) ) {
	    init_padbytes(idip->di_data_buffer, bsize, ~idip->di_pattern);
	    if (idip->di_iot_pattern) {
		if (idip->di_btag) {
		    update_buffer_btags(idip, idip->di_btag, idip->di_offset,
					idip->di_pattern_buffer, bsize, (idip->di_records_read + 1));
		}
		lba = init_iotdata(idip, idip->di_pattern_buffer, bsize, lba, idip->di_lbdata_size);
	    }
	}

	if (dip->di_Debug_flag) {
            if (device_count > 1) {
                Printf(idip, "Index: %d, Device: %s\n", dtap->dta_current_index, idip->di_dname);
            }
	    report_io(idip, READ_MODE, idip->di_data_buffer, bsize, idip->di_offset);
	}

	if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
	    lock_offset = idip->di_offset;
	    /* Lock a partial byte range! */
	    status = dt_lock_unlock(idip, idip->di_dname, &idip->di_fd,
				    LOCK_TYPE_READ, lock_offset, (Offset_t)bsize);
	    if (status == FAILURE) break;
	}

	idip->di_retry_count = 0;
	do {
	    count = read_record(idip, idip->di_data_buffer, bsize, dsize, idip->di_offset, &status);
	} while (status == RETRYABLE);
	if (idip->di_end_of_file) break;		/* Stop reading at end of file. */

	if (status == FAILURE) {
	    if (dip->di_error_count >= dip->di_error_limit) break;
	}

	/*
	 * Verify the data (unless disabled).
	 */
	if ( (status != FAILURE) && dip->di_compare_flag && (dip->di_io_mode == TEST_MODE) ) {
	    ssize_t vsize = count;
	    status = (*idip->di_funcs->tf_verify_data)(idip, idip->di_data_buffer, vsize, idip->di_pattern, &lba, False);
	    /*
	     * Verify the pad bytes (if enabled).
	     */
	    if ( (status == SUCCESS) && dip->di_pad_check) {
		(void)verify_padbytes(idip, idip->di_data_buffer, vsize, ~idip->di_pattern, bsize);
	    }
	}
        if (status == SUCCESS) {
            /* TODO: We *must* maintain the offset correctly reads! */
            idip->di_offset += count;
            status = verify_btag_write_order(idip, (btag_t *)idip->di_data_buffer, (size_t)count);
            idip->di_offset -= count;
        }

	/*
	 * If we had a partial transfer, perhaps due to an error, adjust
	 * the logical block address in preparation for the next request.
	 */
	if (dip->di_iot_pattern && ((size_t)count < bsize)) {
	    size_t resid = (bsize - count);
	    lba -= (lbdata_t)howmany((lbdata_t)resid, idip->di_lbdata_size);
	}

	/*
	 * For variable length records, adjust the next record size.
	 */
	if (dip->di_min_size) {
	    if (idip->di_variable_flag) {
		dsize = get_variable(dip);
	    } else {
		dsize += idip->di_incr_count;
	        if (dsize > idip->di_max_size) dsize = idip->di_min_size;
	    }
	}

        fbytes_read += count;
        records_read++;
        idip->di_lbytes_read += count;
	idip->di_records_read++;
	idip->di_volume_records++;

	if (idip->di_io_dir == FORWARD) {
	    idip->di_offset += count;	/* Maintain our own position too! */
	} else if ( (idip->di_io_type == SEQUENTIAL_IO) &&
		    (idip->di_offset == (Offset_t)idip->di_file_position) ) {
	    set_Eof(idip);
	    break;
	}

	if (dip->di_step_offset) {
	    if (dip->di_io_dir == FORWARD) {
		idip->di_offset = set_position(dip, (idip->di_offset + idip->di_step_offset), True);
		/* Linux returns EINVAL when seeking too far! */
		if (idip->di_offset == (Offset_t)-1) {
		    set_Eof(idip);
		    break;
		}
		/* 
		 * This prevents us from going past the end of a slice/data limit.
		 */ 
		if ( (idip->di_offset + (Offset_t)dsize) >= idip->di_end_position ) {
		    set_Eof(idip);
		    break;
		}
	    } else {
		idip->di_offset -= idip->di_step_offset;
		if (idip->di_offset <= (Offset_t)idip->di_file_position) {
		    set_Eof(idip);
		    break;
		}
	    }
	}

	if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
	    /* Unlock a partial byte range! */
	    status = dt_lock_unlock(idip, idip->di_dname, &idip->di_fd,
				    LOCK_TYPE_UNLOCK, lock_offset, (Offset_t)bsize);
	    if (status == FAILURE) break;
	}
	/* For IOPS, track usecs and delay as necessary. */
	if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	    highresolutiontime(&loop_end_time, NULL);
	    loop_usecs = (uint32_t)timer_diff(&loop_start_time, &loop_end_time);
            dip->di_target_total_usecs += dip->di_iops_usecs; 
            dip->di_actual_total_usecs += loop_usecs;
            if (dip->di_target_total_usecs > dip->di_actual_total_usecs) {
		unsigned int usecs = (unsigned int)(dip->di_target_total_usecs - dip->di_actual_total_usecs);
		mySleep(idip, usecs);
            }
	}
        error_count = 0;
        IterateInputDevices(dtap, dtapp_error_count, &error_count);
    }
    if (lock_full_range == True) {
        int rc = dtapp_lock_unlock(dips, device_count, LOCK_TYPE_UNLOCK, data_limit);
	if (rc == FAILURE) status = rc;
    }
    return(status);
}

/*
 * Note: The device information (dip) is for the 1st device.
 */
int
dtapp_write_data(dinfo_t *dip)
{
    dtapp_information_t *dtap = dip->di_opaque;
    dinfo_t *idip = NULL, *odip = NULL;
    register ssize_t count;
    register size_t bsize, dsize;
    large_t data_limit;
    Offset_t lock_offset = 0;
    hbool_t lock_full_range = False;
    hbool_t partial = False;
    lbdata_t lba = 0;
    uint32_t loop_usecs;
    large_t fbytes_written = 0, records_written = 0;
    struct timeval loop_start_time, loop_end_time;
    unsigned long error_count = 0;
    int rc, status = SUCCESS;

    dsize = get_data_size(dip, WRITE_OP);
    data_limit = dtapp_get_data_limit(dip, dtap);

    dtapp_set_device_offsets(dip, dtap);
    /* 
     * Setup new write orders for each pass to avoid overwrite issues! 
     *  
     * Each pass starts with a new random seed and possibly varying direction, 
     * so don't wish to write stale entries and subsequently false failures. 
     * That's my thinking today, I belief it's valid, so being safe than sorry!
     */
    status = dtapp_setup_write_orders(dip, dtap, dtap->dta_output_count);
    if (status == FAILURE) return(status);

    if ( (dip->di_lock_files == True) && dt_test_lock_mode(dip, LOCK_RANGE_FULL) ) {
	lock_full_range = True;
        status = dtapp_lock_unlock(dtap->dta_output_dips, dtap->dta_output_count,
                                   LOCK_TYPE_WRITE, data_limit);
	if (status == FAILURE) return(status);
    }
    if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	dip->di_actual_total_usecs = 0;
	dip->di_target_total_usecs = 0;
    }

    /*
     * Now write the specifed number of records.
     */
    while ( (error_count < dip->di_error_limit) &&
	    (fbytes_written < data_limit) &&
	    (records_written < dip->di_record_limit) ) {

	PAUSE_THREAD(dip);
	if ( THREAD_TERMINATING(dip) ) break;
	if (dip->di_terminating) break;

        dtap->dta_current_index = rnd(dip, 0, (dtap->dta_output_count - 1));
        odip = dtap->dta_output_dips[dtap->dta_current_index];
        /* Note: We only support mirroring with both input and output devices! */
        if (dtap->dta_input_dips) {
            idip = dtap->dta_input_dips[dtap->dta_current_index];
        }

	if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	    highresolutiontime(&loop_start_time, NULL);
	    if (dip->di_records_written) {
		/* Adjust the actual usecs to adjust for possible usleep below! */
		dip->di_actual_total_usecs += timer_diff(&loop_end_time, &loop_start_time);
	    }
	}

	if ( odip->di_max_data && (odip->di_maxdata_written >= odip->di_max_data) ) {
	    odip->di_maxdata_reached = True;
	    break;
	}

	if ( odip->di_volumes_flag &&
             (odip->di_multi_volume >= odip->di_volume_limit) &&
             (odip->di_volume_records >= odip->di_volume_records) ) {
	    break;
	}

	if (odip->di_write_delay) {			/* Optional write delay	*/
	    mySleep(odip, odip->di_write_delay);
	}

	/*
	 * If data limit was specified, ensure we don't exceed it.
	 */
        /* Note: With slices, we MUST honor the per device limits! */
        if ( (odip->di_lbytes_written + dsize) > odip->di_data_limit) {
            bsize = (size_t)(odip->di_data_limit - odip->di_lbytes_written);
            assert(bsize < odip->di_data_alloc_size);
            if (bsize == 0) {
                set_Eof(odip);
                break;
            }
        } else {
            bsize = dsize;
        }

	if ( (odip->di_io_type == SEQUENTIAL_IO) && (odip->di_io_dir == REVERSE) ) {
	    bsize = MIN((size_t)(odip->di_offset - odip->di_file_position), bsize);
	    odip->di_offset = set_position(odip, (Offset_t)(odip->di_offset - bsize), False);
	    if (idip) {
		idip->di_offset = set_position(idip, (Offset_t)(idip->di_offset - bsize), False);
	    }
	} else if (odip->di_io_type == RANDOM_IO) {
	    odip->di_offset = do_random(odip, True, bsize);
	    if (idip) {
		idip->di_offset = odip->di_offset;
		set_position(idip, idip->di_offset, False);
	    }
	}

        if (dip->di_debug_flag && (bsize != dsize) && !dip->di_variable_flag) {
            Printf (odip, "Record #%lu, Writing a partial record of %lu bytes...\n",
                                    (odip->di_records_written + 1), bsize);
        }

	if (dip->di_iot_pattern || dip->di_lbdata_flag) {
	    lba = make_lbdata(odip, (Offset_t)(odip->di_volume_bytes + odip->di_offset));
	} else {
            lba = make_lbdata(odip, odip->di_offset);
        }

	/*
	 * If requested, rotate the data buffer through ROTATE_SIZE
	 * bytes to force various unaligned buffer accesses.
	 */
	if (dip->di_rotate_flag) {
	    odip->di_data_buffer = (odip->di_base_buffer + (odip->di_rotate_offset++ % ROTATE_SIZE));
	}

	/*
	 * Initialize the data buffer with a pattern.
	 */
	if ( (dip->di_compare_flag == True) &&
	     ( (dip->di_io_mode == MIRROR_MODE) || (dip->di_io_mode == TEST_MODE) ) ) {
	    if (dip->di_iot_pattern) {
		lba = init_iotdata(odip, odip->di_data_buffer, bsize, lba, odip->di_lbdata_size);
	    } else {
		fill_buffer(odip, odip->di_data_buffer, bsize, odip->di_pattern);
	    }
	    /*
	     * Initialize the logical block data (if enabled).
	     */
	    if ( dip->di_lbdata_flag && dip->di_lbdata_size && (dip->di_iot_pattern == False) ) {
		lba = init_lbdata(odip, odip->di_data_buffer, bsize, lba, odip->di_lbdata_size);
	    }
#if defined(TIMESTAMP)
	    /*
	     * If timestamps are enabled, initialize buffer accordingly.
	     */
	    if (dip->di_timestamp_flag) {
		init_timestamp(odip, odip->di_data_buffer, bsize, odip->di_lbdata_size);
	    }
#endif /* defined(TIMESTAMP) */
	    if (odip->di_btag) {
		update_buffer_btags(odip, odip->di_btag, odip->di_offset,
				    odip->di_data_buffer, bsize, (odip->di_records_written + 1));
	    }
	}

	if (dip->di_Debug_flag) {
            if (dtap->dta_output_count > 1) {
                Printf(odip, "Index: %d, Device: %s\n", dtap->dta_current_index, odip->di_dname);
            }
	    report_io(odip, WRITE_MODE, odip->di_data_buffer, bsize, odip->di_offset);
	}

	if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
	    lock_offset = odip->di_offset;
	    /* Lock a partial byte range! */
	    status = dt_lock_unlock(odip, odip->di_dname, &odip->di_fd,
				    LOCK_TYPE_WRITE, lock_offset, (Offset_t)bsize);
	    if (status == FAILURE) break;
	}

	odip->di_retry_count = 0;
	do {
	    count = write_record(odip, odip->di_data_buffer, bsize, dsize, odip->di_offset, &status);
	} while (status == RETRYABLE);
	if (odip->di_end_of_file) break;	/* Stop writing at end of file. */

	if (status == FAILURE) {
	    if (dip->di_error_count >= dip->di_error_limit) break;
	} else {
	    partial = (count < (ssize_t)bsize) ? True : False;
	}
	if ( (status == SUCCESS) && (dip->di_io_mode == MIRROR_MODE) ) {
	    ssize_t rcount;
            rcount = verify_record(idip, odip->di_data_buffer, count, odip->di_offset, &status);
	    /* TODO: Need to cleanup multiple device support! */
	    /* For now, propagate certain information to writer. */
	    if (idip->di_end_of_file) {
		odip->di_end_of_file = idip->di_end_of_file;
		break;
	    }
	    if (status == FAILURE) { /* Read or verify failed! */
		dip->di_error_count++;
		if (dip->di_error_count >= dip->di_error_limit) break;
	    }
            /* 
             * Verify the btag write order, unless read-after-write is enabled,
             * in which case this btag verification is done below.
             */
            if ( (odip->di_raw_flag == False) && (dip->di_dump_btags == False) ) {
                /* TODO: Cleanup this offset handling! */
                idip->di_offset += count;
                status = verify_btag_write_order(idip, (btag_t *)idip->di_data_buffer, (size_t)count);
                idip->di_offset -= count;
            }
	}

	/*
	 * If we had a partial transfer, perhaps due to an error, adjust
	 * the logical block address in preparation for the next request.
	 */
	if (dip->di_iot_pattern && ((size_t)count < bsize)) {
	    size_t resid = (bsize - count);
	    lba -= (lbdata_t)howmany(resid, (size_t)dip->di_lbdata_size);
	}

        fbytes_written += count;
        records_written++;
        odip->di_lbytes_written += count;
	odip->di_records_written++;
	odip->di_volume_records++;

	/*
	 * Flush data *before* verify (required for buffered mode to catch ENOSPC).
	 */ 
	if ( odip->di_fsync_frequency && ((odip->di_records_written % odip->di_fsync_frequency) == 0) ) {
	    status = (*odip->di_funcs->tf_flush_data)(odip);
	    if (status == FAILURE) {
                if (dip->di_error_count >= dip->di_error_limit) break;
            }
	}

	if ( (count > (ssize_t) 0) && odip->di_raw_flag) {
	    /* Release write lock and apply a read lock (as required). */
	    if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
		/* Unlock a partial byte range! */
		status = dt_lock_unlock(odip, odip->di_dname, &odip->di_fd,
					LOCK_TYPE_UNLOCK, lock_offset, (Offset_t)bsize);
		if (status == FAILURE) break;
		/* Lock a partial byte range! */
		status = dt_lock_unlock(odip, odip->di_dname, &odip->di_fd,
					LOCK_TYPE_READ, lock_offset, (Offset_t)bsize);
		if (status == FAILURE) break;
	    }
	    status = write_verify(odip, odip->di_data_buffer, count, dsize, odip->di_offset);
	    if (status == FAILURE) {
                if (dip->di_error_count >= dip->di_error_limit) break;
            }
            if (status == SUCCESS) {
                /* Previous may be for this device, so need updated offset here! */
                odip->di_offset += count;
                status = verify_btag_write_order(odip, (btag_t *)odip->di_data_buffer, (size_t)count);
                odip->di_offset -= count; /* TODO: Cleanup offset handling! */
            }
	}

        if ( (status == SUCCESS) && odip->di_btag ) {
            dtapp_set_write_order_entry(odip, odip->di_btag);
        }

	/*
	 * After the first partial write to a regular file, we set a
	 * premature EOF, to avoid any further writes. This logic is
	 * necessary, since subsequent writes may succeed, but our
	 * read pass will try to read an entire record, and will report
	 * a false data corruption, depending on the data pattern and
	 * I/O type, so we cannot read past this point to be safe.
	 * Note: A subsequent write may return ENOSPC, but not always!
	 */
	if ( partial && (odip->di_dtype->dt_dtype == DT_REGULAR) ) {
	    odip->di_last_write_size = count;
	    odip->di_last_write_attempted = dsize;
	    odip->di_last_write_offset = odip->di_offset;
	    odip->di_no_space_left = True;
	    odip->di_file_system_full = True;
	    set_Eof(odip);
	    break;
	}

	/*
	 * For variable length records, adjust the next record size.
	 */
	if (dip->di_min_size) {
	    if (odip->di_variable_flag) {
		dsize = get_variable(dip);
	    } else {
		dsize += odip->di_incr_count;
		if (dsize > odip->di_max_size) dsize = odip->di_min_size;
	    }
	}

	if (odip->di_io_dir == FORWARD) {
	    odip->di_offset += count;	/* Maintain our own position too! */
	    if (idip) idip->di_offset += count;
	} else if ( (odip->di_io_type == SEQUENTIAL_IO) &&
		    (odip->di_offset == (Offset_t)odip->di_file_position) ) {
	    set_Eof(odip);
	    dip->di_beginning_of_file = True;
	    break;
	}

	if (dip->di_step_offset) {
	    if (odip->di_io_dir == FORWARD) {
		odip->di_offset = set_position(dip, (odip->di_offset + odip->di_step_offset), True);
		if (idip) idip->di_offset = set_position(dip, (idip->di_offset + idip->di_step_offset), True);
		/* Linux returns EINVAL when seeking too far! */
		if (odip->di_offset == (Offset_t)-1) {
		    set_Eof(odip);
		    break;
		}
		/* 
		 * This prevents us from going past the end of a slice/data limit.
		 */ 
		if ( (odip->di_offset + (Offset_t)dsize) >= odip->di_end_position ) {
		    set_Eof(dip);
    		    break;
		}
	    } else {
		odip->di_offset -= odip->di_step_offset;
		if (odip->di_offset <= (Offset_t)odip->di_file_position) {
		    set_Eof(odip);
		    odip->di_beginning_of_file = True;
		    break;
		}
		if (idip) {
		    idip->di_offset -= idip->di_step_offset;
		    if (idip->di_offset <= (Offset_t)idip->di_file_position) {
			set_Eof(odip);
			odip->di_beginning_of_file = True;
			break;
		    }
		}
	    }
	}
	if ( (dip->di_lock_files == True) && (lock_full_range == False) ) {
	    /* Unlock a partial byte range! */
	    status = dt_lock_unlock(odip, odip->di_dname, &odip->di_fd,
				    LOCK_TYPE_UNLOCK, lock_offset, (Offset_t)bsize);
	    if (status == FAILURE) break;
	}
	/* For IOPS, track usecs and delay as necessary. */
	if (dip->di_iops && (dip->di_iops_type == IOPS_MEASURE_EXACT) ) {
	    highresolutiontime(&loop_end_time, NULL);
	    loop_usecs = (uint32_t)timer_diff(&loop_start_time, &loop_end_time);
            odip->di_target_total_usecs += dip->di_iops_usecs;
	    if (odip->di_raw_flag == True) {
		odip->di_target_total_usecs += dip->di_iops_usecs; /* Two I/O's! */
	    }
            odip->di_actual_total_usecs += loop_usecs;
            if (odip->di_target_total_usecs > odip->di_actual_total_usecs) {
		unsigned int usecs = (unsigned int)(odip->di_target_total_usecs - odip->di_actual_total_usecs);
		mySleep(odip, usecs);
            }
	}
        error_count = 0;
        IterateAllDevices(dtap, dtapp_error_count, &error_count, rc);
    }
    if (lock_full_range == True) {
        int rc = dtapp_lock_unlock(dtap->dta_output_dips, dtap->dta_output_count,
                                   LOCK_TYPE_UNLOCK, data_limit);
	if (rc == FAILURE) status = rc;
    }
    return(status);
}

int
dtapp_report_btag(dinfo_t *dip, btag_t *ebtag, btag_t *rbtag, hbool_t raw_flag)
{
    dtapp_information_t *dtap = dip->di_opaque;
    char str[LARGE_BUFFER_SIZE];
    dinfo_t *pdip = NULL;
    char *strp = str;
    int btag_errors = 0;
    size_t btag_size = sizeof(*ebtag);
    btag_write_order_t *ewrop = NULL;
    btag_write_order_t *rwrop = NULL;
    uint32_t btag_index = 0;

    rwrop = (btag_write_order_t *)((char *)rbtag + btag_size);

    Fprintf(dip, "\n");
    Fprintf(dip, "Write Order Tag @ "LLPX0FMT" ("SDF" bytes):\n", rwrop, sizeof(*rwrop));
    Fprintf(dip, "\n");
    
    if (ebtag) {
	ewrop = (btag_write_order_t *)((char *)ebtag + btag_size);
    }
    /*
     * This condition occurs when the primary btag does *not* verify, and 
     * we are called as the result of dumping the btag with errors. Mostly,
     * cosmetic, but seeing the invalid device index is misleading, so...
     */
    if ( (dtap->dta_primary_type == INPUT_FILE) ||
         (rwrop->wro_device_index == BTAG_NO_DEVICE_INDEX) ) {
        ewrop = NULL; /* Force reporting the received btag only, no comparisons! */
    }

    btag_index = offsetof(btag_write_order_t, wro_device_index);
    if ( (ewrop && rwrop) &&
	 (ewrop->wro_device_index != rwrop->wro_device_index) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Device Index", (btag_size + btag_index), incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%u\n", expected_str, ewrop->wro_device_index );
        if ( (dip->di_ftype == INPUT_FILE) && (ewrop->wro_device_index < dtap->dta_input_count) ) {
            pdip = dtap->dta_input_dips[ewrop->wro_device_index];
        } else if (ewrop->wro_device_index < dtap->dta_output_count) {
            pdip = dtap->dta_output_dips[ewrop->wro_device_index];
        }
        if (pdip) {
            Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Expected Device", pdip->di_dname );
        }
        Fprintf(dip, DT_FIELD_WIDTH "%d\n", received_str, rwrop->wro_device_index );
        if ( (dip->di_ftype == INPUT_FILE) && (rwrop->wro_device_index < dtap->dta_input_count) ) {
            pdip = dtap->dta_input_dips[rwrop->wro_device_index];
        } else if (rwrop->wro_device_index < dtap->dta_output_count) {
            pdip = dtap->dta_output_dips[rwrop->wro_device_index];
        } else {
            pdip = NULL;
        }
        if (pdip) {
            Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Received Device", pdip->di_dname );
        }
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%u\n",
		"Device Index", (btag_size + btag_index), rwrop->wro_device_index );
        if ( (dip->di_ftype == INPUT_FILE) && (rwrop->wro_device_index < dtap->dta_input_count) ) {
            pdip = dtap->dta_input_dips[rwrop->wro_device_index];
        } else if (rwrop->wro_device_index < dtap->dta_output_count) {
            pdip = dtap->dta_output_dips[rwrop->wro_device_index];
        }
        if (pdip) {
            Fprintf(dip, DT_FIELD_WIDTH "%s\n", "Received Device", pdip->di_dname );
        }
    }
    
    btag_index = offsetof(btag_write_order_t, wro_write_size);
    if ( (ewrop && rwrop) &&
	 (ewrop->wro_write_size != rwrop->wro_write_size) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Write Size", (btag_size + btag_index), incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "%d\n",
		expected_str, LtoH32(ewrop->wro_write_size) );
	Fprintf(dip, DT_FIELD_WIDTH "%d\n",
		received_str, LtoH32(rwrop->wro_write_size) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "%d\n",
		"Write Size", (btag_size + btag_index), LtoH32(rwrop->wro_write_size) );
    }
    
    btag_index = offsetof(btag_write_order_t, wro_write_offset);
    if ( (ewrop && rwrop) &&
	 (ewrop->wro_write_offset != rwrop->wro_write_offset) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Write Offset", (btag_size + btag_index), incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH FUF" ("LXF")\n",
		expected_str, LtoH64(ewrop->wro_write_offset), LtoH64(ewrop->wro_write_offset) );
	Fprintf(dip, DT_FIELD_WIDTH FUF" ("LXF")\n",
		received_str, LtoH64(rwrop->wro_write_offset), LtoH64(rwrop->wro_write_offset) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD FUF" ("LXF")\n",
		"Write Offset", (btag_size + btag_index),
		LtoH32(rwrop->wro_write_offset), LtoH64(rwrop->wro_write_offset) );
    }

    btag_index = offsetof(btag_write_order_t, wro_write_secs);
    if ( (ewrop && rwrop) &&
	 (ewrop->wro_write_secs != rwrop->wro_write_secs) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Write Time (secs)", (btag_size + btag_index), incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		expected_str, LtoH32(ewrop->wro_write_secs) );
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		received_str, LtoH32(rwrop->wro_write_secs) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "0x%08x\n",
		"Write Time (secs)", (btag_size + btag_index), LtoH32(rwrop->wro_write_secs) );
    }

    btag_index = offsetof(btag_write_order_t, wro_write_usecs);
    if ( (ewrop && rwrop) &&
	 (ewrop->wro_write_usecs != rwrop->wro_write_usecs) ) {
	Fprintf(dip, DT_BTAG_FIELD "%s\n",
		"Write Time (usecs)", (btag_size + btag_index), incorrect_str);
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		expected_str, LtoH32(ewrop->wro_write_usecs) );
	Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
		received_str, LtoH32(rwrop->wro_write_usecs) );
	btag_errors++;
    } else {
	Fprintf(dip, DT_BTAG_FIELD "0x%08x\n",
		"Write Time (usecs)", (btag_size + btag_index), LtoH32(rwrop->wro_write_usecs) );
    }

    btag_index = offsetof(btag_write_order_t, wro_crc32);
    if ( (ewrop && rwrop) &&
         (ewrop->wro_crc32 != rwrop->wro_crc32) ) {
        Fprintf(dip, DT_BTAG_FIELD "%s\n",
                "Write CRC-32", (btag_size + btag_index), incorrect_str);
        Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
                expected_str, LtoH32(ewrop->wro_crc32) );
        Fprintf(dip, DT_FIELD_WIDTH "0x%08x\n",
                received_str, LtoH32(rwrop->wro_crc32) );
        btag_errors++;
    } else {
        Fprintf(dip, DT_BTAG_FIELD "0x%08x\n",
                "Write CRC-32", (btag_size + btag_index), LtoH32(rwrop->wro_crc32) );
    }

    return(btag_errors);
}

int
dtapp_update_btag(dinfo_t *dip, btag_t *btag, Offset_t offset,
		  uint32_t record_index, size_t record_size, uint32_t record_number)
{
    dtapp_information_t *dtap = dip->di_opaque;
    int32_t btag_size = sizeof(*btag);
    btag_write_order_t *wrop, *pwrop = dtap->dta_last_write_order;
    int status = SUCCESS;
    
    if (pwrop == NULL) return(SUCCESS);

    status = dtapp_verify_btag_opaque_data(dip, btag);
    if (status == FAILURE) return(status);

    wrop = (btag_write_order_t *)((char *)btag + btag_size);
    /* Copy the last write order information. */
    *wrop = *pwrop;

    return(status);
}

int
dtapp_verify_btag(dinfo_t *dip, btag_t *ebtag, btag_t *rbtag,
		  uint32_t *eindex, hbool_t raw_flag)
{
    dtapp_information_t *dtap = dip->di_opaque;
    int btag_errors = 0;
    uint32_t btag_size = sizeof(*ebtag);
    btag_write_order_t *ewrop = NULL;
    btag_write_order_t *rwrop = NULL;
    uint32_t btag_index = 0;

    if (dtap->dta_primary_type == INPUT_FILE) {
        /* We can't verify this part, since we don't have the expected! */
        return(btag_errors);
    } else if ( (dip->di_ftype == OUTPUT_FILE) && (dip->di_mode == READ_MODE) ) {
        /* Expected btag has stale write order entries from write pass. */
        /* Note to Self: This occurs when doing a separate read pass, NOT raw! */
        return(btag_errors);
    }
    ewrop = (btag_write_order_t *)((char *)ebtag + btag_size);
    rwrop = (btag_write_order_t *)((char *)rbtag + btag_size);

    if (rwrop->wro_device_index == BTAG_NO_DEVICE_INDEX) return(SUCCESS);

    if (ewrop->wro_device_index != rwrop->wro_device_index) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Device index incorrect, expected %u, received %u\n",
		    ewrop->wro_device_index, rwrop->wro_device_index );
	}
	btag_index = btag_size + offsetof(btag_write_order_t, wro_device_index);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }
    
    if (ewrop->wro_write_size != rwrop->wro_write_size) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Write size incorrect, expected %u, received %u\n",
		    LtoH32(ewrop->wro_write_size), LtoH32(rwrop->wro_write_size) );
	}
	btag_index = btag_size + offsetof(btag_write_order_t, wro_write_size);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if (ewrop->wro_write_offset != rwrop->wro_write_offset) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Write offset incorrect, expected "FUF", received "FUF"\n",
		    LtoH64(ewrop->wro_write_offset), LtoH64(rwrop->wro_write_offset) );
	}
	btag_index = btag_size + offsetof(btag_write_order_t, wro_write_offset);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if (ewrop->wro_write_secs != rwrop->wro_write_secs) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Write secs incorrect, expected %u, received %u\n",
		    LtoH32(ewrop->wro_write_secs), LtoH32(rwrop->wro_write_secs) );
	}
	btag_index = btag_size + offsetof(btag_write_order_t, wro_write_secs);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }

    if (ewrop->wro_write_usecs != rwrop->wro_write_usecs) {
	if (dip->di_btag_debugFlag) {
	    Fprintf(dip, "BTAG: Write usecs incorrect, expected %u, received %u\n",
		    LtoH32(ewrop->wro_write_usecs), LtoH32(rwrop->wro_write_usecs) );
	}
	btag_index = btag_size + offsetof(btag_write_order_t, wro_write_usecs);
	if (eindex && (btag_index < *eindex)) *eindex = btag_index;
	btag_errors++;
    }
    if (ewrop->wro_crc32 != rwrop->wro_crc32) {
	if (dip->di_btag_debugFlag) {
            Fprintf(dip, "BTAG: Write CRC-32 incorrect, expected 0x%08x, received 0x%08x\n",
                    LtoH32(ewrop->wro_crc32), LtoH32(rwrop->wro_crc32) );
        }
        btag_index = btag_size + offsetof(btag_write_order_t, wro_crc32);
        if (eindex && (btag_index < *eindex)) *eindex = btag_index;
        btag_errors++;
    }
    return(btag_errors);
}

int
dtapp_verify_btag_opaque_data(dinfo_t *dip, btag_t *btag)
{
    if (btag->btag_opaque_data_type != OPAQUE_WRITE_ORDER_TYPE) {
	Fprintf(dip, "The opaque data type (%u) is incorrect!\n", btag->btag_opaque_data_type);
	return(FAILURE);
    }
    if ( LtoH16(btag->btag_opaque_data_size) != sizeof(btag_write_order_t) ) {
	Fprintf(dip, "The opaque data size (%u), is incorrect\n", LtoH16(btag->btag_opaque_data_size));
	return(FAILURE);
    }
    return(SUCCESS);
}

int
verify_btag_write_order(dinfo_t *dip, btag_t *btag, size_t transfer_count)
{
    dtapp_information_t *dtap = dip->di_opaque;
    uint32_t btag_size = sizeof(*btag);
    btag_write_order_t *wrop = NULL;
    //btag_write_order_t *pwrop = NULL;
    btag_t *pbtag = NULL, *error_btag = NULL;
    dinfo_t *pdip = NULL;
    //Offset_t pbtag_offset;
    int status = SUCCESS;
    int btag_errors = 0;
    size_t bsize;
    ssize_t count;

    wrop = (btag_write_order_t *)((char *)btag + btag_size);
    if (wrop->wro_device_index == BTAG_NO_DEVICE_INDEX) return(SUCCESS);

    if (dip->di_ftype == INPUT_FILE) {
        if (wrop->wro_device_index < dtap->dta_input_count) {
            pdip = dtap->dta_input_dips[wrop->wro_device_index];
        }
    } else { /* OUTPUT_FILE */
        if (wrop->wro_device_index < dtap->dta_output_count) {
            pdip = dtap->dta_output_dips[wrop->wro_device_index];
        }
    }

    if (pdip == NULL) {
	if (dip->di_btag_debugFlag) {
            Printf(dip, "Failed to locate previous device for index %u!\n", wrop->wro_device_index);
        }
        return(WARNING);
    }
    if (dip->di_Debug_flag) {
        Printf(dip, "Order Index: %d, Device: %s\n", wrop->wro_device_index, pdip->di_dname);
    }
    /*
     * Read the record for the previous write and verify it is correct.
     */
    bsize = (size_t)LtoH32(wrop->wro_write_size);
    /* When reading a single block, we wish to verify the entire record written! */
    if (pdip->di_verify_buffer_size < bsize) {
	free_palign(dip, pdip->di_verify_buffer);
        pdip->di_verify_buffer_size = (bsize + PADBUFR_SIZE); /* No alignment offset! */
	pdip->di_verify_buffer = malloc_palign(pdip, pdip->di_verify_buffer_size, 0);
        if (dip->di_verify_buffer == NULL) return(FAILURE);
    }
    count = dtapp_read_record(pdip, pdip->di_verify_buffer, bsize, bsize,
                              LtoH64(wrop->wro_write_offset), &status);
    if (status == FAILURE) return(status);
    /* Reposition, until we switch everything to pread/pwrite! */
    /* Note: The proper offset *must* be maintained for devices! */
    //pdip->di_offset = set_position(pdip, pdip->di_offset, False);

    pbtag = (btag_t *)pdip->di_verify_buffer;

    if (dip->di_dump_btags == True) {
        uint32_t dsize = pdip->di_device_size;
        uint8_t *bp = (uint8_t *)pbtag;
        size_t buffer_index;
        /*
         * Dump as many blocks as were transferred to limit ordered btags reported.
         */
        for (buffer_index = 0; (buffer_index < transfer_count); buffer_index += dsize) {
            report_btag(pdip, NULL, (btag_t *)bp, False);
            bp += dsize;
        }
        return(status);
    }

    status = verify_buffer_btags(pdip, pbtag, bsize, &error_btag);
    if (status == FAILURE) {
        Fprintf(dip, "One or more btags have a CRC error for device %s!\n", pdip->di_dname);
        dtapp_report_ordered_btags_error(dip, pdip, btag, pbtag, error_btag);
        return(status);
    }
    /* 
     * Note: We do *not* care about the previous write order of the previous btag! 
     * Well, at least not for this test. We may wish to traverse all ordered later! 
     */
    //pwrop = (btag_write_order_t *)((char *)pbtag + btag_size);
    //if (pwrop->wro_device_index == BTAG_NO_DEVICE_INDEX) return(SUCCESS);

    status = verify_ordered_btags(dip, btag, wrop, pbtag, bsize, &error_btag);

    if (status == FAILURE) {
       dtapp_report_ordered_btags_error(dip, pdip, btag, pbtag, error_btag);
    }
    return(status);
}

void
dtapp_report_ordered_btags_error(dinfo_t *dip, dinfo_t *pdip, btag_t *btag, btag_t *pbtag, btag_t *error_btag)
{
    dtapp_information_t *dtap = dip->di_opaque;
    int rc;

    ReportErrorNumber(dip);

    Fprintf(dip, "\n");
    Fprintf(dip, "Current Block Tag @ "LLPX0FMT"\n", btag);
    report_btag(dip, NULL, btag, False);

    Fprintf(dip, "\n");
    Fprintf(dip, "Previous Block Tag @ "LLPX0FMT"\n", pbtag);
    Fprintf(dip, "   Error Block Tag @ "LLPX0FMT"\n", error_btag);
    report_btag(dip, NULL, error_btag, False);

    IterateAllDevices(dtap, dtapp_report_history, NULL, rc);

    return;
}
int
verify_ordered_btags(dinfo_t *dip, btag_t *btag, btag_write_order_t *wrop, void *buffer, size_t record_size, btag_t **error_btag)
{
    uint8_t *bp = buffer;
    btag_t *pbtag = (btag_t *)bp;
    uint32_t dsize = LtoH32(pbtag->btag_device_size);
    size_t buffer_index;
    int btag_errors = 0;
    Offset_t pbtag_offset;
    uint32_t write_size = LtoH32(wrop->wro_write_size);
    Offset_t write_offset = LtoH64(wrop->wro_write_offset);
    int status = SUCCESS;

    if (error_btag) *error_btag = NULL;

    for (buffer_index = 0; buffer_index < record_size; buffer_index += dsize) {
	pbtag = (btag_t *)bp;
        /* 
         * Verify write data and timestamps.
         */
        if ( isDiskDevice(dip) ) {
            pbtag_offset = (LtoH64(pbtag->btag_lba) * dsize);
        } else {
            pbtag_offset = LtoH64(pbtag->btag_offset);
        }
        if (pbtag_offset != write_offset) {
            Fprintf(dip, "Write offset incorrect, expected "FUF", received "FUF"\n",
                    write_offset, LtoH64(pbtag_offset) );
            btag_errors++;
        }
        if ( LtoH32(pbtag->btag_record_size) != write_size ) {
            Fprintf(dip, "Write size incorrect, expected %u, received %u\n",
                    write_size, LtoH32(pbtag->btag_record_size) );
            btag_errors++;
        }
        /*
         * Only check for exact write timestamp and CRC on the first btag.
         */
        if ( pbtag == (btag_t *)buffer ) {
            if ( LtoH32(pbtag->btag_write_secs) != LtoH32(wrop->wro_write_secs) ) {
                Fprintf(dip, "Write secs incorrect, expected 0x%08x, received 0x%08x\n",
                        LtoH32(wrop->wro_write_secs), LtoH32(pbtag->btag_write_secs) );
                btag_errors++;
            }
            if ( LtoH32(pbtag->btag_write_usecs) != LtoH32(wrop->wro_write_usecs) ) {
                Fprintf(dip, "Write usecs incorrect, expected 0x%08x, received 0x%08x\n",
                        LtoH32(wrop->wro_write_usecs), LtoH32(pbtag->btag_write_usecs) );
                btag_errors++;
            }
            if ( LtoH32(pbtag->btag_crc32) != LtoH32(wrop->wro_crc32) ) {
                Fprintf(dip, "Write CRC-32 incorrect, expected 0x%08x, received 0x%08x\n",
                        LtoH32(wrop->wro_crc32), LtoH32(pbtag->btag_crc32) );
                btag_errors++;
            }
        }
        /*
         * Now, ensure the write timestamp is less than the current record btag.
        */
        if ( LtoH32(pbtag->btag_write_secs) > LtoH32(btag->btag_write_secs) ) {
            Fprintf(dip, "Previous write secs 0x%08x greater than current btag usecs 0x%08x\n",
                    LtoH32(wrop->wro_write_secs), LtoH32(pbtag->btag_write_secs) );
            btag_errors++;
        } else if ( ( LtoH32(pbtag->btag_write_secs) == LtoH32(btag->btag_write_secs) ) &&
                    ( LtoH32(pbtag->btag_write_usecs) > LtoH32(btag->btag_write_usecs) ) ) {
            Fprintf(dip, "Previous write usecs 0x%08x greater than current btag usecs 0x%08x\n",
                    LtoH32(wrop->wro_write_usecs), LtoH32(pbtag->btag_write_usecs) );
            btag_errors++;
        }
        if (btag_errors) {
            if (error_btag) {
                *error_btag = pbtag;
            }
            status = FAILURE;
            break;
        }
	bp += dsize;
        write_size -= dsize;
        write_offset += dsize;
    }
    return(status);
}

/*
 * Note: Wrapper to common read_record() for logging the record number. 
 * Previously this was a clone, but we have switched to pread/pwrite! 
 */
ssize_t
dtapp_read_record(dinfo_t *dip, uint8_t *buffer, size_t bsize, size_t dsize, Offset_t offset, int *status)
{
    ssize_t count;

    if (dip->di_Debug_flag) {
        large_t lba = (offset / dip->di_dsize);
        unsigned long files = 0, records = 0;
        report_record(dip, files, records, lba, offset, READ_MODE, buffer, bsize);
        // FIXME: report_io(dip, READ_MODE, buffer, bsize, offset);
    }
    count = read_record(dip, buffer, bsize, dsize, offset, status);
    return(count);
}
