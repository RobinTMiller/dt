/*
 * dtTOOL.c - I/O Behavior for TOOL tool.
 * 
 * Author: Robin T. Miller
 * Date Created: November 5th, 2013
 * 
 * Modification History:
 */

#include "dt.h"

/*
 * Definitions:
 */
#define DEFAULT_THREAD_COUNT    1
#define DEFAULT_RUNTIME         0

/*
 * TOOL Specific Parameters (options): 
 */
typedef struct TOOL_parameters {
    /* Add TOOL specific parameters... */
    uint64_t param_iterations;
    hbool_t  param_locking;
    hbool_t  param_verbose;
} TOOL_parameters_t;

/*
 * TOOL Thread Specific Information: 
 */
typedef struct TOOL_thread_info {
    dinfo_t     *dip;
    void        *TOOL_thread_specific_data;
} TOOL_thread_info_t;

typedef struct TOOL_information {
    TOOL_parameters_t    TOOL_parameters;
    TOOL_thread_info_t   TOOL_thread_info;
} TOOL_information_t;

/*
 * Forward References: 
 */
void TOOL_help(dinfo_t *dip);
int TOOL_thread_setup(dinfo_t *dip);

/* I/O Behavior Support Functions */
int TOOL_initialize(dinfo_t *dip);
int TOOL_parser(dinfo_t *dip, char *option);
void TOOL_cleanup_information(dinfo_t *dip);
int TOOL_clone_information(dinfo_t *dip, dinfo_t *cdip, hbool_t new_thread);
int TOOL_job_finish(dinfo_t *mdip, job_info_t *job);
void TOOL_show_parameters(dinfo_t *dip);
void *TOOL_thread(void *arg);
int TOOL_validate_parameters(dinfo_t *dip);

/*
 * Declare the I/O behavior functions:
 */
iobehavior_funcs_t TOOL_iobehavior_funcs = {
    "TOOL",			/* iob_name */
    &TOOL_initialize,		/* iob_initialize */
    &TOOL_parser,		/* iob_parser */
    &TOOL_cleanup_information,	/* iob_cleanup */
    &TOOL_clone_information,	/* iob_clone */
    &TOOL_thread,		/* iob_thread */
    NULL,			/* iob_thread1 */
    NULL,			/* iob_job_init */
    NULL,			/* iob_job_cleanup */
    &TOOL_job_finish,		/* iob_job_finish */
    NULL,			/* iob_job_modify */
    &TOOL_job_finish,		/* iob_job_query */
    NULL,			/* iob_job_keepalive */
    NULL,			/* iob_thread_keepalive */
    &TOOL_show_parameters,	/* iob_show_parameters */
    &TOOL_validate_parameters	/* iob_validate_parameters */
};     
 
void     
TOOL_set_iobehavior_funcs(dinfo_t *dip)
{
    dip->di_iobf = &TOOL_iobehavior_funcs;
    return;
}
     
/* ---------------------------------------------------------------------- */

int
TOOL_parser(dinfo_t *dip, char *option)
{
    TOOL_information_t *sip = dip->di_opaque;
    TOOL_parameters_t *TOOLp = &sip->TOOL_parameters;
    int status = PARSE_MATCH;

    if (match(&option, "-")) {         /* Optional "-" to match TOOL options! */
        ;
    }
    if (match(&option, "help")) {
        TOOL_help(dip);
        return(STOP_PARSING);
    }
    if (match(&option, "verbose")) {
        dip->di_errors_flag = True;
        return(status);
    }
    if (match(&option, "locking")) {
        TOOLp->param_locking = True;
        return(status);
    }
    if (match(&option, "min_hl=")) {
        //TOOLp->param_min_hardlinks = (int)number(dip, option, ANY_RADIX, &status, True);
        return(status);
    }
    /* Add TOOL specific parsing here... */
    return(PARSE_NOMATCH);
}

/* ---------------------------------------------------------------------- */

int
TOOL_job_finish(dinfo_t *dip, job_info_t *job)
{
    TOOL_information_t *sip;
    TOOL_thread_info_t *thread_info;
    threads_info_t *tip = job->ji_tinfo;
    dinfo_t *tdip;
    int	thread;

    /*
     * Accumulate the total statistics.
     */
    for (thread = 0; (thread < tip->ti_threads); thread++) {
        tdip = tip->ti_dts[thread];
        sip = tdip->di_opaque;
        thread_info = &sip->TOOL_thread_info;
        /* Accumulate thread statistics here...*/
    }
    //TOOL_report_stats(dip, total_info, "Total", sip->TOOL_style);
    return(SUCCESS);
}

void *
TOOL_thread(void *arg)
{
    dinfo_t *dip = arg;
    TOOL_information_t *sip = dip->di_opaque;
    TOOL_thread_info_t *tip = &sip->TOOL_thread_info;
    TOOL_parameters_t *TOOLp = &sip->TOOL_parameters;
    uint64_t iterations = 0;
    int status = SUCCESS;

    status = do_common_thread_startup(dip);
    if (status == FAILURE) goto thread_exit;

    status = TOOL_thread_setup(dip);
    if (status == FAILURE) goto thread_exit;

    if (dip->di_debug_flag || dip->di_tDebugFlag) {
        Printf(dip, "Starting TOOL, Job %u, Thread %u, Thread ID "OS_TID_FMT"\n",
               dip->di_job->ji_job_id, dip->di_thread_number, (os_tid_t)pthread_self());
    }

    dip->di_start_time = times(&dip->di_stimes);
    if (dip->di_runtime) {
        dip->di_runtime_end = time((time_t *)NULL) + dip->di_runtime;
    }

    while (True) {

        PAUSE_THREAD(dip);
        if ( THREAD_TERMINATING(dip) ) break;
        if (dip->di_terminating) break;

        /* Do some I/O here... */
        break;

        iterations++;
        if (TOOLp->param_iterations && (iterations >= TOOLp->param_iterations)) {
            break;
        }
    } /* end while(True) */

    if (dip->di_tDebugFlag == True) {
        ; //TOOL_report_stats(dip, tip, "Thread", sip->TOOL_style);
    }

thread_exit:
    do_common_thread_exit(dip, status);
    /*NOT REACHED*/
    return(NULL);
}

void
TOOL_cleanup_information(dinfo_t *dip)
{
    TOOL_information_t *sip;
    TOOL_thread_info_t *tip;

    if ( (sip = dip->di_opaque) == NULL) {
        return;
    }
    tip = &sip->TOOL_thread_info;

    /* Do TOOL thread specific cleanup here... */

    Free(dip, sip);
    dip->di_opaque = NULL;
    return;
}

int
TOOL_clone_information(dinfo_t *dip, dinfo_t *cdip, hbool_t new_thread)
{
    TOOL_information_t *sip = dip->di_opaque;
    TOOL_parameters_t *TOOLp = &sip->TOOL_parameters;
    TOOL_information_t *csip; /* clone */

    csip = Malloc(dip, sizeof(*csip));
    if (csip == NULL) return(FAILURE);
    cdip->di_opaque = csip;
    *csip = *sip;           /* Copy the original information. */
    
    /* Do TOOL thread specific cloning (if any) here... */

    return(SUCCESS);
}

int
TOOL_initialize(dinfo_t *dip)
{
    TOOL_information_t *sip;
    TOOL_parameters_t *TOOLp;

    sip = Malloc(dip, sizeof(*sip));
    if (sip == NULL) return(FAILURE);
    dip->di_opaque = sip;

    TOOLp = &sip->TOOL_parameters;

    dip->di_threads = DEFAULT_THREAD_COUNT;
    dip->di_runtime = DEFAULT_RUNTIME;

    /* Note: This is necessary to bypass dt sanity checks! */
    dip->di_data_limit = 512;
    return(SUCCESS);
}

int
TOOL_thread_setup(dinfo_t *dip)
{
    TOOL_information_t *sip = dip->di_opaque;
    TOOL_thread_info_t *tip = &sip->TOOL_thread_info;
    TOOL_parameters_t *TOOLp = &sip->TOOL_parameters;
    char *filename = dip->di_output_file;
    int thread_number = dip->di_thread_number;

    tip->dip = dip;

    return(SUCCESS);
}

int
TOOL_validate_parameters(dinfo_t *dip)
{
    TOOL_information_t *sip = dip->di_opaque;
    TOOL_parameters_t *TOOLp = &sip->TOOL_parameters;
    char *style = NULL;

    if (dip->di_output_file == NULL) {
        Eprintf(dip, "You must specify an output file.\n");
        return(FAILURE);
    }

    return(SUCCESS);
}

void
TOOL_show_parameters(dinfo_t *dip)
{
    TOOL_information_t *sip = dip->di_opaque;
    TOOL_parameters_t *TOOLp = &sip->TOOL_parameters;

    Lprintf(dip, "TOOL Parameters:\n");
#if 0
    Lprintf(dip, "    style................%s\n", sip->TOOL_style          );
    Lprintf(dip, "    interval.............%d\n", TOOLp->param_interval    );
    Lprintf(dip, "    iterations...........%d\n", TOOLp->param_iterations  );
    Lprintf(dip, "    verbose..............%d\n", dip->di_errors_flag      );
    Lprintf(dip, "    locking..............%d\n", TOOLp->param_locking     );
    Lprintf(dip, "    seed................." LUF "\n", dip->di_random_seed );
    Lprintf(dip, "    filename.............%s\n", dip->di_output_file      );
    Lprintf(dip, "    thread count.........%u\n", dip->di_threads          );
    Lprintf(dip, "    runtime..............%u\n", dip->di_runtime          );
        ...
    Lprintf(dip, "\n");
#endif /* 0 */
    /* Add TOOL parameter display here... */
    Lflush(dip);
    return;
}

#define P	Print

void
TOOL_help(dinfo_t *dip)
{
    TOOL_information_t *sip = dip->di_opaque;
    TOOL_parameters_t *TOOLp = &sip->TOOL_parameters;

    P(dip, "Usage: %s iobehavior=TOOL [options...]\n", cmdname);
    P(dip, "\nOptions:\n");
    P(dip, "\t-help                    Show this help text, then exit.\n");
    P(dip, "\t-verbose                 Show errors as they occur.\n");
    P(dip, "\t-seed=value              Set random seed to use.\n");
    P(dip, "\t-noflock                 Disable file locking/unlocking.\n");
    P(dip, "\t-version                 Print the version, then exit.\n");
    /* Add TOOL specific help here! */
    P(dip, "\n");
    return;
}
