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
 * dthammer.c - I/O Behavior for hammer tool.
 * 
 * Author: Robin T. Miller
 * Date Created: April 30th, 2014
 * 
 * Modification History:
 * 
 * February 4th, 2023 by Robin T. Miller, Chris Nelson, & John Hollowell
 *      Fix segmentation fault when overwriting a file encounters a file
 * system full condition, due to writefile() freeing the file structure.
 * 
 * November 9, 2021 by Chris Nelson (nelc@netapp.com)
 *  Add MIT license, in order to distribute to FOSS community so it can
 *  be used and maintained by a larger audience, particularly for the
 *  public dt at https://github.com/RobinTMiller/dt
 *
 * March 30th, 2016 by Robin T. Miller
 * 	Add hammer mapping function. This gets called when the program name is
 * "hammer" to map hammer options to dts' hammer I/O behavior options.
 * 
 * March 27th, 2016 by Robin T. Miller
 * 	Adding support for maxdata option to avoid file system full conditions.
 * 	Also adding support for maxfiles option to provide this flexibility.
 * 
 * December 10th, 2015 by Robin T. Miller
 *      Adding help examples.
 * 
 * July 31st, 2015 by Robin T. Miller
 * 	Fix logic issue when bufmodes=unbuffered and -maxfsize is specifed.
 * Whence DIO is enabled, a later sanity check improperly set maxfsize!
 *
 * April 20th, 2015 by Robin T. Miller
 * 	Add handle to inode_lookup() to avoid extra open request.
 * 	On Windows, do not inode_add() for streams file of dup ID.
 *
 * March 31st, 2014 by Robin T. Miller
 * 	Added parsing for runtime= and threads= options, since sio
 * options can (optionally) start with '-', while dt option do not!
 * This can (and did) break NACL automation, which included the '-',
 * so adding this parsing to match the documentation and help text.
 *
 * March 17th, 2015 by Robin T. Miller
 * 	Add a deleting files flag, so I/O monitoring does NOT cancel our
 * thread, if this cleanup takes longer than the term wait time.
 *
 * June 2nd, 2014 by Robin T. Miller
 * 	Add support for Direct I/O, thus page aligned buffers and rounding
 * I/O sized to the device size (usually 512, but dsize=value sets otherwise).
 *
 * May 30th, 2014 by Robin T. Miller
 * 	Make changes to free the full file path, and recreate it when that
 * file entry is used. With many files, this will help conserve memory!
 * 
 * May 28th, 2014 by Robin T. Miller
 * 	Make changes so I/O monitoring will report the correct file name.
 * Basically setup dt's di_dname field with the file name being operated on.
 * Note: The full file path is setup, since multiple hammer threads use the
 * same file names.
 *
 * May 23rd, 2014 by Robin T. Miller  
 * 	Removed change directory API's, since this is NFS for multiple threads.
 * Sadly, these API's set the global current directory for the process, and no
 * API exists to allow this per thread (too bad). This required adding full
 * paths for files, but thankfully the impact is minimized by the change below
 * to optimize memory. But behold, we can now run multiple hammer threads! ;)
 * Note: If memory does pose an issue, we can dynamically create full paths.
 * 
 * May 22nd, 2014 by Robin T. Miller
 * 	Update allocfile() to dynamically allocate memory for the file path,
 * rather than use hardcoded 64 byte path array (saves memory), and ensure
 * this	memory is freed in freefile().
 * 
 * May 21st, 2014 by Robin T. Miller
 * 	When creating the directory, clearly indicate disk full conditions.
 * 	When removing the base streams file, honor the "hasdir" flag, since
 * this will be false on disk full errors.
 * 	Added -onlydelete option to *only* delete files during cleanup.
 * Previously file reads and locks were performed when cleaning up files.
 * Note: The original hammer behavior persists without -onlydelete option.
 */

#include "dt.h"

#include <assert.h>

#define COPYRIGHT	"Copyright (c) 2012 Network Appliance, Inc. All rights reserved."
#ifndef VERSION
#define VERSION		"$Id: hammer.c#11 $"
#endif

/*
 * Definitions:
 */
#define HAMMER_DEFAULT_THREAD_COUNT	1
#define HAMMER_DEFAULT_RUNTIME		-1
#define HAMMER_MAX_TERM_TIME		(60 * 5)

#define DEFAULT_BLOCK_SIZE_MIN	BLOCK_SIZE
#define DEFAULT_BLOCK_SIZE_MAX	(128 * KBYTE_SIZE)
#define DEFAULT_FILE_SIZE_MIN	1
#define DEFAULT_FILE_SIZE_MAX	(5 * MBYTE_SIZE)
#define DEFAULT_MODE		mixed
#define DEFAULT_RANDOM_BSIZE	True

#define DEFAULT_HALT_ON_ALL_ERRORS	True
#define DEFAULT_HALT_ON_FILE_ERRORS	True
#define DEFAULT_HALT_ON_LOCK_ERRORS	True
#define DEFAULT_HALT_ON_CORRUPTIONS	True

#define DEFAULT_KEEP_DISK_FULL	False
#define DEFAULT_NOFLUSH		False
#define DEFAULT_DELETE_ONLY	False
#define DEFAULT_FLUSH_ONLY	False
#define DEFAULT_ITERATIONS	0
#define DEFAULT_NOCLEANUP	False
#define DEFAULT_RETRY_DISC	True

#if defined(WIN32)
# define DEFAULT_NOSTREAMS	False
#else /* !defined(WIN32) */
# define DEFAULT_NOSTREAMS	True
#endif /* defined(WIN32) */

#define DEFAULT_LOCK_DEBUG	True
#define DEFAULT_LOCK_TEST	False
#define DEFAULT_UNLOCK_CHANCE	100

/*
 * Internal Hammer Errors:
 */
#define HAMMER_SUCCESS		SUCCESS
#define HAMMER_FAILURE		1
#define HAMMER_DISK_FULL	-1
#define HAMMER_NO_STREAMS	-2

#define LOCK_FULL_RANGE		0
#define LOCK_PARTIAL_RANGE	1

#define HAMMER_LOGFILE		"hamlog.txt"

/* lock flags */
#define FLAG_LOCK_READ		'r'
#define FLAG_LOCK_WRITE		'w'
#define FLAG_UNLOCK		'u'

#ifndef O_BINARY
# define O_BINARY 0
#endif

/* 
 * Note: This format *only* displays the low 8 hex digits (with leading zeros),
 * 	 of the 64-bit value, which is Ok for the way hammer uses it!
 */ 
#define LL0XFMT	"0x"LLHXFMT

#define KILOBYTE ((int64_t)1024)
#define MEGABYTE ((int64_t)1024 * KILOBYTE)
#define GIGABYTE ((int64_t)1024 * MEGABYTE)
#define TERABYTE ((int64_t)1024 * GIGABYTE)

#define freemem(mptr) if (mptr) { free(mptr); mptr = NULL; }
#define freestr(strp) if (strp) { memset(strp, 0xdd, strlen(strp)); free(strp); strp = NULL; }

#define STREQ(s1, s2)		(strcmp((s1), (s2)) == 0)

#define start_timer(dip)	(void)timer(dip, True)
#define stop_timer(dip)		timer(dip, False)

#define MINFSIZE	1
#define MINBSIZE	1
#define MAXFSIZE	INT64_MAX
#define RNDFSIZE(dip,hmrp) rnd64(dip, hmrp->minfsize, hmrp->maxfsize)
#define MAXBLOCKSIZE	(64 * MBYTE_SIZE)
#define RNDBSIZE(dip,hmrp) (int)rnd(dip, hmrp->minbsize, hmrp->filebufsize)
#define KPS(bytes,secs)	(((bytes) / 1024.0) / (secs))

/*
 * This is the data repeated in each block written.
 */
#define SIGNATURE		"HAMR"
#define SIGNATURE_LENGTH	(sizeof SIGNATURE - 1)
#define CLIENTNAME_TERMCHAR	0x7f

typedef struct datablock {
    char	signature[SIGNATURE_LENGTH];
    Offset_t	offset;			/* Offset of this datablock in the file. */
    uint32_t	fileid;			/* Unique number associated with file.   */
    uint32_t	timestamp;		/* time_t when the file was written.     */
    uint32_t	pid;			/* Process ID that wrote the file.       */
    char	clientname[1];		/* clientname + CLIENTNAME_TERMCHAR      */
} datablock_t;

#define FILESIG 0xc0edbabe

typedef struct hammer_file {
    uint32_t	sig;			/* The file signature (above).	*/
    hbool_t	hasdir;			/* Has directory flag.		*/
    hbool_t	is_disk_full;		/* Is the disk full flag.	*/
    char	*path;			/* Pointer to the file path.	*/
    char	*colon;			/* Pointer to ':' (if any).	*/
    char	*fpath;			/* Pointer to the full path.	*/
    uint32_t	fileid;			/* The file ID (random number).	*/
    uint32_t	timestamp;		/* The file timestamp.		*/
    int64_t	size;			/* The size of the file.	*/
    struct hammer_file *prev, *next;
    struct hammer_file *base;
} hammer_file_t;

/*
 * Inodes allocated to/freed from hammer. 
 * (Assume that nothing else is running on the target volume.)
 */
#define INODE_HASHTABLE_SIZE	65521
#define INODE_HASH(inode)	((inode) % INODE_HASHTABLE_SIZE)

typedef struct hammer_inode {
    os_ino_t		ino;
    struct hammer_inode	*next;
} hammer_inode_t;

typedef struct hammer_mode {
    int lower, upper;
} hammer_mode_t;

/*
 * hammer Specific Parameters (options): 
 */
typedef struct hammer_parameters {
    hbool_t	disk_filled;
    hbool_t	randombsize;
    int		filebufsize;
    int		next_action;
    hbool_t	background;
    hbool_t	keep_disk_full;
    hbool_t	noflush;
    hbool_t	onlydelete;
    hbool_t	onlyflush;
    hbool_t	nostreams;
    hbool_t	nocleanup;
    hbool_t	wantcore;
    hbool_t	nofilercore;
    hbool_t	testfilercore;
    hbool_t	lock_files;
    int		unlock_chance;
    hbool_t	halt_on_all_errors;
    hbool_t	halt_on_file_errors;
    hbool_t	halt_on_lock_errors;
    hbool_t	halt_on_data_corruption;
    hbool_t	inode_check;
    uint64_t	num_iterations;
    uint64_t	max_iterations;
    int64_t	minfsize;
    int64_t	maxfsize;
    int		minbsize;
    int		maxbsize;
    time_t	max_runtime;
    hammer_mode_t *mode;
    hammer_mode_t *lock_mode;	/* lock mode selection (randomization control) */
} hammer_parameters_t;

/*
 * hammer Thread Specific Information:
 * 
 * Note: There's only one hammer thread, but keeping this consistent with
 * other I/O behaviors (parameters vs. thread data).
 */
typedef struct hammer_thread_info {
    dinfo_t     *dip;
    pid_t	mypid;
    time_t	whenstart;
    char	*filebuf;
    char	*clientname;
    char	*clientver;
    char	*curdir;
    char	*logdir;
    char	*corrupted_file;
    Offset_t	corrupted_offset;
    int		ncopies;
    uint64_t	file_number;
    int64_t	nfiles;
    int64_t	nfiles_when_full;
    hammer_file_t *head;
    hammer_file_t *lastwrittenfile;
    datablock_t *datablock;
    uint32_t	datablocklen;
    hammer_inode_t *inode_hash_table[INODE_HASHTABLE_SIZE];
    struct timeval start;
    char	logtime_buf[TIME_BUFFER_SIZE];
    char	*uncpath;
    /* Error Counters: */
    uint32_t	file_errors;
    uint32_t	lock_errors;
    uint32_t	data_corruptions;
} hammer_thread_info_t;

typedef struct hammer_information {
    hammer_parameters_t    hammer_parameters;
    hammer_thread_info_t   hammer_thread_info;
} hammer_information_t;

/*
 * Local Definitions:
 */
static char *disk_full_str = "disk is full";

/*
 * hammer File Operations:
 */
#define INVALID_ACTION	-1
#define CREATEFILE	0
#define OWRITEFILE	1
#define  TRUNCFILE	2
#define DELETEFILE	3
#define RENAMEFILE	4
#define   READFILE	1000

/* Note: Not tuneable, so leave as static (for now). */
static struct hammer_mode creates[] = {
	{  1,  80 },	/* CREATEFILE 80% */
	{ 81,  85 },	/* RENAMEFILE  5% */
	{ 86,  90 },	/* OWRITEFILE  5% */
	{ 91,  95 },	/*  TRUNCFILE  5% */
	{ 96, 100 },	/* DELETEFILE  5% */
};

static struct hammer_mode mixed[] = {
	{  1,  40 },	/* CREATEFILE 40% */
	{ 41,  55 },	/* RENAMEFILE 15% */
	{ 56,  70 },	/* OWRITEFILE 15% */
	{ 71,  85 },	/*  TRUNCFILE 15% */
	{ 86, 100 },	/* DELETEFILE 15% */
};

static struct hammer_mode overwrites[] = {
	{  1,  20 },	/* CREATEFILE 20% */
	{ 21,  25 },	/* RENAMEFILE  5% */
	{ 26,  85 },	/* OWRITEFILE 60% */
	{ 86,  90 },	/*  TRUNCFILE  5% */
	{ 91, 100 },	/* DELETEFILE 10% */
};

static struct hammer_mode lck_full[] = {
	{  1,  80 },	/* FULL    LOCK  80% */
	{ 81, 100 },	/* PARTIAL LOCK  20% */
};

static struct hammer_mode lck_mixed[] = {
	{  1,  50 },	/* FULL    LOCK  50% */
	{ 51, 100 },	/* PARTIAL LOCK  50% */
};

static struct hammer_mode lck_partial[] = {
	{  1,  20 },	/* FULL    LOCK  20% */
	{ 21, 100 },	/* PARTIAL LOCK  80% */
};

/*
 * Hammer Specific Functions:
 */
int hammer_doio(dinfo_t *dip);
void hammer_startup(dinfo_t *dip);
char *getmodename(dinfo_t *dip);
char *makefullpath(dinfo_t *dip, char *path);
void update_dname(dinfo_t *dip, char *file);
int init_datablock(dinfo_t *dip);
void setmem(dinfo_t *dip, char *buf, size_t nbytes, Offset_t offset, uint32_t fileid, uint32_t timestamp);
unsigned char *corruption(dinfo_t *dip, unsigned char *bad, unsigned char *good, size_t nbytes);
char *mkcorruptmsg(unsigned char *bad, unsigned char *good, size_t nbytes);
void hammer_report_miscompare_information(
    dinfo_t *dip, char *file, HANDLE *fdp, char *base, size_t iosize, size_t nbytes, Offset_t buffer_index);
void dumpfilercore(dinfo_t *dip);
void *chkmem(dinfo_t *dip, char *file, HANDLE *fdp, char *buf, size_t nbytes, Offset_t offset, uint32_t fileid, uint32_t timestamp);
HANDLE start_copy(dinfo_t *dip, hammer_file_t *f, int64_t fsize);
int choose_action(dinfo_t *dip, hammer_parameters_t *hmrp);
int cleanup_files(dinfo_t *dip);
void cleanup_hammer(dinfo_t *dip);
hammer_file_t *allocfile(dinfo_t *dip, char *path);
int freefile(dinfo_t *dip, hammer_file_t *file);
int freefiles(dinfo_t *dip);
hammer_file_t *getrndfile(dinfo_t *dip);
hammer_file_t *findfile(hammer_thread_info_t *tip, char *path);
static hammer_file_t *findotherstream(dinfo_t *dip, hammer_thread_info_t *tip, hammer_file_t *f);
uint64_t newrndfilenum(hammer_thread_info_t *tip);
hammer_file_t *newrndfile(dinfo_t *dip);
int updatesize(dinfo_t *dip, hammer_file_t *f);
int writefile(dinfo_t *dip, hammer_file_t **fp);
int api_writefile(dinfo_t *dip, hammer_file_t *f, int bsize, int64_t fsize, int do_overwrite);
int readfile(dinfo_t *dip, hammer_file_t *f);
int api_readfile(dinfo_t *dip, hammer_file_t *f, int bsize);
int truncatefile(dinfo_t *dip, hammer_file_t *f);
int renamefile(dinfo_t *dip, hammer_file_t *f);
int deletefile(dinfo_t *dip, hammer_file_t *f, hbool_t cleanup_flag);
int removepath(dinfo_t *dip, char *path);
static int inode_add(dinfo_t *dip, hammer_thread_info_t *tip, os_ino_t ino);
static void inode_remove(hammer_thread_info_t *tip, os_ino_t ino);
static hbool_t inode_exists(hammer_thread_info_t *tip, os_ino_t ino);
static os_ino_t inode_lookup(char *path, HANDLE fd);
static double timer(dinfo_t *dip, hbool_t dostart);
char *mklogtime(hammer_thread_info_t *tip);
char *mktimezone(hammer_thread_info_t *tip);
static __inline void setfileid(hammer_file_t *f);
static __inline void setfiletimestamp(hammer_file_t *f);
static __inline void setdiskisfull(hammer_parameters_t *hmrp, hammer_thread_info_t *tip);
hbool_t	test_lock_mode(dinfo_t *dip, hammer_parameters_t *hmrp, int lck_mode);
hbool_t	unlock_file_chance(dinfo_t *dip, hammer_parameters_t *hmrp);

/* Temorary API's: */
int api_lockfile(dinfo_t *dip, HANDLE *fd, hammer_file_t *f, char lock_type, Offset_t offset, Offset_t length);


/* ---------------------------------------------------------------------- */

/*
 * Forward References: 
 */
void hammer_help(dinfo_t *dip);
int hammer_thread_setup(dinfo_t *dip);

/* I/O Behavior Support Functions */
int hammer_initialize(dinfo_t *dip);
int hammer_parser(dinfo_t *dip, char *option);
void hammer_cleanup_information(dinfo_t *dip);
int hammer_clone_information(dinfo_t *dip, dinfo_t *cdip, hbool_t new_context);
int hammer_job_finish(dinfo_t *mdip, job_info_t *job);
void hammer_show_parameters(dinfo_t *dip);
void *hammer_thread(void *arg);
int hammer_validate_parameters(dinfo_t *dip);

/*
 * Declare the I/O behavior functions:
 */
iobehavior_funcs_t hammer_iobehavior_funcs = {
    "hammer",			/* iob_name */
    HAMMER_IO,			/* iob_iobehavior */
    &hammer_map_options,	/* iob_map_options */
    NULL,			/* iob_maptodt_name */
    NULL,			/* iob_dtmap_options */
    &hammer_initialize,		/* iob_initialize */
    NULL,                  	/* iob_initiate_job */
    &hammer_parser,		/* iob_parser */
    &hammer_cleanup_information,/* iob_cleanup */
    &hammer_clone_information,	/* iob_clone */
    &hammer_thread,		/* iob_thread */
    NULL,			/* iob_thread1 */
    NULL,			/* iob_job_init */
    NULL,			/* iob_job_cleanup */
    &hammer_job_finish,		/* iob_job_finish */
    NULL,			/* iob_job_modify */
    &hammer_job_finish,		/* iob_job_query */
    NULL,			/* iob_job_keepalive */
    NULL,			/* iob_thread_keepalive */
    &hammer_show_parameters,	/* iob_show_parameters */
    &hammer_validate_parameters	/* iob_validate_parameters */
};     
 
void
hammer_set_iobehavior_funcs(dinfo_t *dip)
{
    dip->di_iobf = &hammer_iobehavior_funcs;
    /* Match hammers' UUID directory style! */
    dip->di_uuid_dashes = False;
    return;
}
     
/* ---------------------------------------------------------------------- */

#if _BIG_ENDIAN_

# define tobigendian2(n)	(n)
# define tobigendian4(n)	(n)
# define tobigendian8(n)	(n)

#else /* _BIG_ENDIAN_ == 0 */

/* Forward Declarations: */
uint16_t tobigendian2(uint16_t little_n);
uint32_t tobigendian4(uint32_t little_n);
uint64_t tobigendian8(uint64_t little_n);

uint16_t
tobigendian2(uint16_t little_n)
{
    unsigned char *little_p, *big_p;
    uint16_t big_n;

    little_p = (unsigned char *)&little_n;
    big_p = (unsigned char *)&big_n;

    big_p[0] = little_p[1];
    big_p[1] = little_p[0];

    return big_n;
}

uint32_t
tobigendian4(uint32_t little_n)
{
    unsigned char *little_p, *big_p;
    uint32_t big_n;

    little_p = (unsigned char *)&little_n;
    big_p = (unsigned char *)&big_n;

    big_p[0] = little_p[3];
    big_p[1] = little_p[2];
    big_p[2] = little_p[1];
    big_p[3] = little_p[0];

    return big_n;
}

uint64_t
tobigendian8(uint64_t little_n)
{
    unsigned char *little_p, *big_p;
    uint64_t big_n;

    little_p = (unsigned char *)&little_n;
    big_p = (unsigned char *)&big_n;

    big_p[0] = little_p[7];
    big_p[1] = little_p[6];
    big_p[2] = little_p[5];
    big_p[3] = little_p[4];
    big_p[4] = little_p[3];
    big_p[5] = little_p[2];
    big_p[6] = little_p[1];
    big_p[7] = little_p[0];

    return big_n;
}

#endif /* _BIG_ENDIAN_ */

/* Note: BSD random API's are NOT being ported! */

/* Note: Moved to dt.h and dtutil.c for sharing! */

#if 0
/* lower <= rnd(lower,upper) <= upper */
static __inline int64_t
rnd64(dinfo_t *dip, int64_t lower, int64_t upper)
{
    return( lower + (int64_t)( ((double)(upper - lower + 1) * genrand64_int64(dip)) / (UINT64_MAX + 1.0)) );
}

/* lower <= rnd(lower,upper) <= upper */
static __inline long
rnd(dinfo_t *dip, long lower, long upper)
{
    return( lower + (long)( ((double)(upper - lower + 1) * get_random(dip)) / (UINT32_MAX + 1.0)) );
}
#endif /* 0 */

/* ---------------------------------------------------------------------- */

int
hammer_map_options(dinfo_t *dip, int argc, char **argv)
{
    int i;
    char *cmdp, *option, *optp, *param;
    int status = SUCCESS;

    status = SetupCommandBuffers(dip);
    if (status == FAILURE) return(status);
    cmdp = dip->cmdbufptr;

    /* Set the I/O behavior firstr!. */
    cmdp += sprintf(cmdp, "iobehavior=hammer");

    for (i = 0; i < argc; i++) {
	option = optp = argv[i];
	/* Note: Allowing dt style options too! */
	if ( (*option != '-') && NES(option, "=") ) {
	    cmdp += sprintf(cmdp, " dir=%s", option);
	    continue;
	}
	/* Remember, match() updates 1st arg (string parsed)! */
	if ( match(&optp, "-api") || match(&optp, "-filercore") ||
	     match(&optp, "-iterations") || match(&optp, "-mode") ||
	     match(&optp, "-logfile") ||
	     match(&optp, "-blocksize") || match(&option, "-bsize") ||
	     match(&optp, "-minfsize") || match(&optp, "-maxfsize") ||
	     match(&optp, "-minbsize") || match(&optp, "-maxbsize") ||
	     match(&optp, "-runtime") || match(&optp, "-seed") || 
	     match(&optp, "-lockmode") || match(&optp, "-unlockchance") ||
	     match(&optp, "-trigger") || match(&optp, "-ontap_cserver") ||
	     match(&optp, "-ontap_nodes") || match(&optp, "-ontap_username") ||
	     match(&optp, "-optap_password") || match(&optp, "-ontapi_path") ) {
	    /* Get next parameter (if any). */
	    if (++i < argc) {
		param = argv[i];
		cmdp += sprintf(cmdp, " %s=%s", option, param);
	    }
	    continue;
	}
	/* Pass option through to I/O behavior parser. */
	cmdp += sprintf(cmdp, " %s", option);
    }
    if (status == SUCCESS) {
	dip->argc = MakeArgList(dip->argv, dip->cmdbufptr);
	if (dip->argc == FAILURE) status = FAILURE;
    }
    return(status);
}

int
hammer_parser(dinfo_t *dip, char *option)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    char *optp = option;
    int status = PARSE_MATCH;

    if (match(&option, "-")) {         /* Optional "-" to match hammer options! */
        ;
    }
    if (match(&option, "help")) {
        hammer_help(dip);
        return(STOP_PARSING);
    }

    /* Hammer specific options. */

    if (match(&option, "version")) {
	Printf(dip, "%s\n", COPYRIGHT);
	Printf(dip, "hammer version %s\n", VERSION);
	return(STOP_PARSING);
    }
    if (match(&option, "api=") ) {
#if !defined(WIN32)
	if ( NE(option, "posix") ) {
	    Eprintf(dip, "Unix only supports the POSIX API!\n");
	    return(FAILURE);
	}
#else /* defined(WIN32) */
	if ( NE(option, "win32") ) {
	    Eprintf(dip, "Windows only supports the WIN32 API!\n");
	    return(FAILURE);
	}
#endif /* !defined(WIN32) */
	return(status);
    }
    if (match(&option, "bg")) {
	hmrp->background = True;
	return(status);
    }
    if (match(&option, "interactive")) {
	Wprintf(dip, "Hammers' interactive mode is NOT supported!\n");
        return(status);
    }
    if (match(&option, "iterations=")) {
        hmrp->max_iterations =  large_number(dip, option, ANY_RADIX, &status, True);
        if (status == SUCCESS) dip->di_record_limit = hmrp->max_iterations;
        return(status);
    }
    if (match(&option, "mode=")) {
	if (match(&option, "creates")) {
	    hmrp->mode = creates;
	} else if (match(&option, "mixed")) {
	    hmrp->mode = mixed;
	} else if (match(&option, "overwrites")) {
	    hmrp->mode = overwrites;
	} else {
	    Eprintf(dip, "Valid modes are: creates, mixed, or overwrites\n");
	    status = FAILURE;
	}
	return(status);
    }
    if (match(&option, "blocksize=") || match(&option, "bsize=")) {
	if (match(&option, "random")) {
	    hmrp->randombsize = True;
	} else {
	    hmrp->randombsize = False;
	    hmrp->maxbsize = (int)number(dip, option, ANY_RADIX, &status, True);
	}
	return(status);
    }
    if (match(&option, "minfsize=")) {
	hmrp->minfsize = large_number(dip, option, ANY_RADIX, &status, True);
	return(status);
    }
    if (match(&option, "maxfsize=")) {
	hmrp->maxfsize = large_number(dip, option, ANY_RADIX, &status, True);
	dip->di_data_limit = hmrp->maxfsize;
	return(status);
    }
    if (match(&option, "minbsize=")) {
	hmrp->minbsize = number(dip, option, ANY_RADIX, &status, True);
	return(status);
    }
    if (match(&option, "maxbsize=")) {
	hmrp->maxbsize = number(dip, option, ANY_RADIX, &status, True);
	return(status);
    }
    if (match(&option, "logfile=")) {
	if (dip->di_log_file) {
	    FreeStr(dip, dip->di_log_file);
	    dip->di_log_file = NULL;
	}
	if (*option) {
	    dip->di_log_file = strdup(option);
	    /* Note: hammer does not display the command line. */
	    //dip->di_logheader_flag = True;
	}
	return(status);
    }
    if (match(&option, "runtime=")) {
	dip->di_runtime = time_value(dip, option);
	return(status);
    }
    if (match(&option, "threads=")) {
        dip->di_threads = (int)number(dip, option, ANY_RADIX, &status, True);
        return(status);
    }
    if (match(&option, "seed=")) {
	dip->di_random_seed = large_number(dip, option, ANY_RADIX, &status, True);
	if (status == SUCCESS) dip->di_user_rseed = True;
	return(status);
    }
    if (match(&option, "direct")) {
        dip->di_open_flags |= O_DIRECT;
        dip->di_dio_flag = True;
        return(status);
    }
    if (match(&option, "fill")) {
	hmrp->keep_disk_full = True;
	hmrp->mode = creates;
	return(status);
    }
    if (match(&option, "nocleanup")) {
	hmrp->nocleanup = True;
	return(status);
    }
    if (match(&option, "noflush")) {
	hmrp->noflush = True;
	return(status);
    }
    if (match(&option, "onlydelete")) {
	hmrp->onlydelete = True;
	return(status);
    }
    if (match(&option, "onlyflush")) {
	hmrp->onlyflush = True;
	return(status);
    }
    if (match(&option, "streams")) {
	hmrp->nostreams = False;
	return(status);
    }
#if defined(WIN32)
    if (match(&option, "nostreams")) {
	hmrp->nostreams = True;
	return(status);
    }
    if (match(&option, "noretrydisc")) {
	dip->di_retry_disconnects = False;
	dip->di_retry_entries = 0;
	return(status);
    }
    if (match(&option, "retrydisc")) {
	dip->di_retry_disconnects = True;
	os_set_disconnect_errors(dip);
	return(status);
    }
#endif /* defined(WIN32) */
    if (match(&option, "checkinodes")) {
	hmrp->inode_check = True;
	return(status);
    }
#if defined(NETAPP)
    /* Convert this to dts' equivalent for c-mode! */
    if (match(&option, "filercore=")) {
	trigger_data_t *tdp = &dip->di_triggers[dip->di_num_triggers];
	dip->di_ontap_cserver = strdup(option);
	if (dip->di_num_triggers == NUM_TRIGGERS) {
	    Eprintf(dip, "Maximum number of triggers is %d.\n", NUM_TRIGGERS);
	    status = FAILURE;
	} else {
	    if ((tdp->td_trigger = check_trigger_type(dip, "zapipanic")) == TRIGGER_INVALID) {
		status = FAILURE;
	    } else {
		dip->di_num_triggers++;
	    }
	}
	return(status);
    }
    if (match(&option, "nofilercore")) {
	return(status);
    }
#endif /* defined(NETAPP) */
    if (match(&option, "lockdebug")) {
        dip->di_lDebugFlag = True;
        return(status);
    }
    if (match(&option, "nolockdebug")) {
        dip->di_lDebugFlag = False;
        return(status);
    }
    if (match(&option, "lockfiles")) {
        hmrp->lock_files = True;
        return(status);
    }
    if (match(&option, "lockmode=")) {
	if (match(&option, "full")) {
	    hmrp->lock_mode = lck_full;
	} else if (match(&option, "mixed")) {
	    hmrp->lock_mode = lck_mixed;
	} else if (match(&option, "partial")) {
	    hmrp->lock_mode = lck_partial;
	} else {
	    Eprintf(dip, "Valid lock modes are: full, mixed, or partial\n");
	    status = FAILURE;
	}
	if (status == SUCCESS) hmrp->lock_files = True;
	return(status);
    }
    if (match(&option, "ignoreallerrors")) {
        hmrp->halt_on_all_errors = False;
        return(status);
    }
    if (match(&option, "ignorefileerrors")) {
        hmrp->halt_on_file_errors = False;
        return(status);
    }
    if (match(&option, "ignorelockerrors")) {
        hmrp->halt_on_lock_errors = False;
        return(status);
    }
    if (match(&option, "ignoredatacorruption")) {
        hmrp->halt_on_data_corruption = False;
        return(status);
    }
    if (match(&option, "unlockchance=")) {
	hmrp->unlock_chance = (int)number(dip, option, ANY_RADIX, &status, True);
	if ( (status == SUCCESS) &&
	     ( (hmrp->unlock_chance < 0) || (hmrp->unlock_chance > 100) ) ) {
	    Eprintf(dip, "invalid value [%d] for '-unlockchance' option. Valid values are in the range: 0-100\n", hmrp->unlock_chance);
	    status = FAILURE;
	}
        hmrp->lock_files = True;
        return(status);
    }
    if ( match(&optp, "-bg") ||
	 match(&optp, "-interactive") ||
	 match(&optp, "-nofilercore") ) {
	Wprintf(dip, "Option %s is NOT supported in dts' hammer, so ignored!\n", option);
        return(status);
    }
    return(PARSE_NOMATCH);
}

/* ---------------------------------------------------------------------- */

int
hammer_job_finish(dinfo_t *dip, job_info_t *job)
{
    hammer_information_t *hip;
    hammer_thread_info_t *thread_info;
    threads_info_t *tip = job->ji_tinfo;
    dinfo_t *tdip;
    int	thread;

    /*
     * Accumulate the total statistics.
     */
    for (thread = 0; (thread < tip->ti_threads); thread++) {
        tdip = tip->ti_dts[thread];
        hip = tdip->di_opaque;
        thread_info = &hip->hammer_thread_info;
        /* Accumulate thread statistics here...*/
    }
    //hammer_report_stats(dip, total_info, "Total", hip->hammer_style);
    return(SUCCESS);
}

int
hammer_doio(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    hammer_file_t *f = NULL;
    int action;
    int status = SUCCESS;

    do {
	PAUSE_THREAD(dip);

	if (hmrp->disk_filled && (tip->nfiles <= (tip->nfiles_when_full / 4)) ) {
	    /* Max data also sets the disk full flag. */
	    if (dip->di_maxdata_reached) {
		Printf(dip, "Resume filling disk, max data written is "LUF" bytes...\n",
		       dip->di_maxdata_written);
	    } else {
		Printf(dip, "disk is no longer full...\n");
	    }
	    hmrp->disk_filled = False;
	    dip->di_maxdata_reached = False;
	}
	action = choose_action(dip, hmrp);
	if (hmrp->disk_filled) {
	    /*
	     * The disk has been filled.
	     */
	    if (hmrp->keep_disk_full) {
		/*
		 * Overwrite when we would have created,
		 * and create when we would have overwritten.
		 */
		if (action == CREATEFILE) {
		    action = OWRITEFILE;
		} else if (action == OWRITEFILE) {
		    action = CREATEFILE;
		}
	    } else {
		/*
	         * Delete when we would have created,
	         * and we create when we would have
	         * deleted.
	         */
		if (action == CREATEFILE) {
		    action = DELETEFILE;
		} else if (action == DELETEFILE) {
		    if (dip->di_max_files && (tip->nfiles >= dip->di_max_files) ) {
			action = OWRITEFILE;
		    } else {
			action = CREATEFILE;
		    }
		}
	    }
	}
	/*
	 * If max files specified, don't create more files than requested!
	 */
	if ( (action == CREATEFILE) &&
	     (dip->di_max_files && (tip->nfiles >= dip->di_max_files)) ) {
	    action = OWRITEFILE;
	}

	switch (action) {

	    case CREATEFILE: {
                f = NULL;
		status = writefile(dip, &f);
		if (status == SUCCESS) hmrp->num_iterations++;
		break;
	    }
	    case OWRITEFILE: {
		if ( (f = getrndfile(dip)) != NULL) {
		    status = writefile(dip, &f);
		    if (status == SUCCESS) hmrp->num_iterations++;
		}
		break;
	    }
	    case RENAMEFILE: {
		if ( (f = getrndfile(dip)) != NULL) {
		    status = renamefile(dip, f);
		    if (status == SUCCESS) hmrp->num_iterations++;
		}
		break;
	    }
	    case TRUNCFILE: {
		if ( (f = getrndfile(dip)) != NULL) {
		    status = truncatefile(dip, f);
		    if (status == SUCCESS) hmrp->num_iterations++;
		}
		break;
	    }
	    case DELETEFILE: {
		hbool_t cleanup_flag = False;
		if ( (f = getrndfile(dip)) != NULL) {
		    status = deletefile(dip, f, cleanup_flag);
		    if (status == SUCCESS) hmrp->num_iterations++;
		    f = NULL; /* Already freed, skip below! */
		}
		break;
	    }
	    case READFILE: {
		if ( (f = getrndfile(dip)) != NULL) {
		    status = readfile(dip, f);
		    if (status == SUCCESS) hmrp->num_iterations++;
		}
		break;
	    }
	    default: {
		Eprintf(dip, "hammer: impossible action %d?", action);
		return(FAILURE);
	    }
	} /* end of switch()... */

	/* Note: To conserve memory, we free the full file path. */
	if (f && f->fpath) {
	    freestr(f->fpath);
	}
	if (hmrp->num_iterations >= hmrp->max_iterations) {
	    /*
	     * Time to stop hammering. But first, verify and then delete
	     * all the hammer data files.
	     */
	    Printf(dip, "iterations limit ("LUF" iterations) reached.\n",
		   hmrp->max_iterations);
	    break;
	}

	/*
	 * Honor the max data, if specified by the user. 
	 * Note: We use disk full logic to control this limit. 
	 */
	if ( dip->di_max_data &&
	     (dip->di_maxdata_reached == False) &&
	     (dip->di_maxdata_written > dip->di_max_data) ) {
	    Printf(dip, "Max data limit of "LUF" bytes reached, starting to remove files...\n",
		   dip->di_max_data);
	    dip->di_maxdata_reached = True;
	    setdiskisfull(hmrp, tip);
	}

	if ( (hmrp->halt_on_all_errors == True) &&
	     (dip->di_error_count >= dip->di_error_limit) ) {
	    if ( (hmrp->halt_on_file_errors == False) && tip->file_errors) continue;
	    if ( (hmrp->halt_on_lock_errors == False) && tip->lock_errors) continue;
	    if ( (hmrp->halt_on_data_corruption == False) && tip->data_corruptions) continue;
	    break;
	} else { /* Allow halting on individual errors. */
	    if ( (hmrp->halt_on_file_errors == True) && tip->file_errors) break;
	    if ( (hmrp->halt_on_lock_errors == True) && tip->lock_errors) break;
	    if ( (hmrp->halt_on_data_corruption == True) && tip->data_corruptions) break;
	}

    } while ( (THREAD_TERMINATING(dip) == False) && (dip->di_terminating == False) );

    return(status);
}

void *
hammer_thread(void *arg)
{
    dinfo_t *dip = arg;
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    uint64_t iterations = 0;
    int status = SUCCESS;

    status = do_common_thread_startup(dip);
    if (status == FAILURE) goto thread_exit;

    status = hammer_thread_setup(dip);
    if (status == FAILURE) goto thread_exit;

    if (dip->di_debug_flag || dip->di_tDebugFlag) {
        Printf(dip, "Starting hammer, Job %u, Thread %u, Thread ID "OS_TID_FMT"\n",
               dip->di_job->ji_job_id, dip->di_thread_number, (os_tid_t)pthread_self());
    }

    dip->di_start_time = times(&dip->di_stimes);
    if (dip->di_runtime > 0) {
        dip->di_runtime_end = time((time_t *)NULL) + dip->di_runtime;
    }

    status = hammer_doio(dip);

    if (status == SUCCESS) {
	int cstatus = cleanup_files(dip);
	if (cstatus == FAILURE) status = cstatus;
    } else {
	Printf(dip, "An error occurred, so NOT removing files!\n");
    }
    cleanup_hammer(dip);

thread_exit:
    do_common_thread_exit(dip, status);
    /*NOT REACHED*/
    return(NULL);
}

void
hammer_cleanup_information(dinfo_t *dip)
{
    hammer_information_t *hip;
    hammer_thread_info_t *tip;
    hammer_parameters_t *hmrp;

    if ( (hip = dip->di_opaque) == NULL) {
        return;
    }
    tip = &hip->hammer_thread_info;
    hmrp = &hip->hammer_parameters;

    /* Do hammer thread specific cleanup here... */

    if (tip->corrupted_file) {
	Free(dip, tip->corrupted_file);
	tip->corrupted_file = NULL;
    }
    if (tip->filebuf) {
	free_palign(dip, tip->filebuf);
	tip->filebuf = NULL;
    }
    if (tip->datablock) {
	Free(dip, tip->datablock);
	tip->datablock = NULL;
    }
    if (tip->uncpath) {
	Free(dip, tip->uncpath);
	tip->uncpath = NULL;
    }
    Free(dip, hip);
    dip->di_opaque = NULL;
    return;
}

int
hammer_clone_information(dinfo_t *dip, dinfo_t *cdip, hbool_t new_context)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    hammer_information_t *chip; /* clone */

    chip = Malloc(dip, sizeof(*chip));
    if (chip == NULL) return(FAILURE);
    cdip->di_opaque = chip;
    *chip = *hip;           /* Copy the original information. */
    
    /* Do hammer thread specific cloning (if any) here... */

    return(SUCCESS);
}

/* Note: This is invoked directly after setting the I/O behavior. */

int
hammer_initialize(dinfo_t *dip)
{
    hammer_information_t *hip;
    hammer_parameters_t *hmrp;

    hip = Malloc(dip, sizeof(*hip));
    if (hip == NULL) return(FAILURE);
    if (dip->di_opaque) {
	Free(dip, dip->di_opaque);
    }
    dip->di_opaque = hip;

    hmrp = &hip->hammer_parameters;

    /* Note: An output file name is required to get past common file checks! */
    if (dip->di_output_file == NULL) {
	/* To match legacy hammer, use current directory. */
	dip->di_output_file = strdup(".");
	// dip->di_output_file = strdup("hammer");
    }
    /* 
     * Note: Don't set hammer defaults, if options specified already! 
     */
    if (dip->di_runtime == 0) {
	dip->di_runtime = HAMMER_DEFAULT_RUNTIME;
    }
    /* Note: dt's default (1 thread), matches hammer's default! */
    if (dip->di_threads < HAMMER_DEFAULT_THREAD_COUNT) {
	dip->di_threads = HAMMER_DEFAULT_THREAD_COUNT;
    }
    /*
     * Many files esp. with read/locks, can take a long time to cleanup!
     */
    if (dip->di_term_wait_time == THREAD_MAX_TERM_TIME) {
	dip->di_term_wait_time = HAMMER_MAX_TERM_TIME;
    }

    /* Note: This is necessary to bypass dt sanity checks! */
    dip->di_data_limit = DEFAULT_FILE_SIZE_MAX;
    
    hmrp->wantcore = False;
    hmrp->nofilercore = False;
    hmrp->testfilercore = False;
    dip->di_lDebugFlag = DEFAULT_LOCK_DEBUG;
    hmrp->lock_files = DEFAULT_LOCK_TEST;
    hmrp->unlock_chance = DEFAULT_UNLOCK_CHANCE;
    hmrp->halt_on_all_errors = DEFAULT_HALT_ON_ALL_ERRORS;
    hmrp->halt_on_file_errors = DEFAULT_HALT_ON_FILE_ERRORS;
    hmrp->halt_on_lock_errors = DEFAULT_HALT_ON_LOCK_ERRORS;
    hmrp->halt_on_data_corruption = DEFAULT_HALT_ON_CORRUPTIONS;
    
    hmrp->keep_disk_full = DEFAULT_KEEP_DISK_FULL;
    hmrp->next_action = INVALID_ACTION;
    hmrp->disk_filled = False;
    hmrp->noflush = DEFAULT_NOFLUSH;
    hmrp->onlydelete = DEFAULT_DELETE_ONLY;
    hmrp->onlyflush = DEFAULT_FLUSH_ONLY;
    hmrp->num_iterations = DEFAULT_ITERATIONS;
    hmrp->nocleanup = DEFAULT_NOCLEANUP;
    hmrp->max_runtime = dip->di_runtime;

    /* defaults */
    hmrp->max_iterations = INFINITY;
    hmrp->mode = DEFAULT_MODE;
    hmrp->randombsize = DEFAULT_RANDOM_BSIZE;
    hmrp->minfsize = DEFAULT_FILE_SIZE_MIN;
    hmrp->maxfsize = DEFAULT_FILE_SIZE_MAX;
    hmrp->minbsize = DEFAULT_BLOCK_SIZE_MIN;
    hmrp->maxbsize = DEFAULT_BLOCK_SIZE_MAX;
    hmrp->nostreams = DEFAULT_NOSTREAMS;
    
    dip->di_retry_disconnects = DEFAULT_RETRY_DISC;
    if (dip->di_retry_disconnects == True) {
	os_set_disconnect_errors(dip);
    }
    return(SUCCESS);
}

/* Note: This is invoked from each hammer thread for per thread setup. */
int
hammer_thread_setup(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    char *filename = dip->di_output_file;
    int thread_number = dip->di_thread_number;
    int status;

    tip->dip = dip;
    
    /* Note: These may get updated for DIO based on min/max and file system (XFS). */
    if ( (dip->di_bypass_flag == False) && (dip->di_dio_flag == True) ) {
	if ((int)dip->di_min_size != hmrp->minbsize) {
	    hmrp->minbsize = (int)dip->di_min_size;
	}
	if ((int)dip->di_max_size != hmrp->maxbsize) {
	    hmrp->maxbsize = (int)dip->di_max_size;
	}
	if ((int64_t)dip->di_data_limit != hmrp->maxfsize) {
	    hmrp->maxfsize = dip->di_data_limit;
	}
    }
    if (dip->di_dir) {
	char dirpath[PATH_BUFFER_SIZE];
	(void)sprintf(dirpath, "%s%c%s", dip->di_dir, dip->di_dir_sep, dip->di_uuid_string);
	Free(dip, dip->di_dir);
	dip->di_dir = strdup(dirpath);
	status = setup_directory_info(dip);
	if (status == FAILURE) return(status);
    }
    if (hmrp->background && (dip->di_log_file == NULL) ) {
	if (dip->di_dir) {
	    char logpath[PATH_BUFFER_SIZE];
	    (void)sprintf(logpath, "%s%c%s", dip->di_dir, dip->di_dir_sep, HAMMER_LOGFILE);
	    dip->di_log_file = strdup(logpath);
	} else {
	    dip->di_log_file = strdup(HAMMER_LOGFILE);
	}
	status = create_thread_log(dip);
	if (status == FAILURE) return(status);
    }
#if defined(WIN32)
    {
	if (dip->di_dir) {
	    char buffer[PATH_BUFFER_SIZE];
	    os_error_t error;
	    /* 
	     * Run systeminfo on Windows clients. This file can then be submitted
	     * along with the hammer log so that we'll usually know everything we
	     * need to know about the client without having to ask.
	     * 
	     * Note: Beware of this stupid CMD.EXE restriction!
	     * '\\10.63.8.66\robin\hammer\b4c839439c7347238435db864766c2b3'
	     * CMD.EXE was started with the above path as the current directory.
	     * UNC paths are not supported.  Defaulting to Windows directory.
	     * Access is denied.
	     */
	    sprintf(buffer, "systeminfo > %s%csysinfo.txt", dip->di_dir, dip->di_dir_sep);
	    ExecuteCommand(dip, buffer, LogPrefixDisable, dip->di_pDebugFlag);
    
	    /* Get the UNC path, if the directory includes a drive letter.*/
	    error = win32_getuncpath(dip->di_dir, &tip->uncpath);
	    if ( (error != NO_ERROR) && (error != ERROR_NOT_CONNECTED) ) {
		Wprintf(dip, "WNetGetConnection failed on %s, error = %d", dip->di_dir, error);
	    }
	}
    }
#endif /* defined(WIN32) */

    tip->clientname = os_gethostname();
    tip->clientver = os_getosinfo();
    tip->curdir = os_getcwd();
    tip->mypid = os_getpid();

    time(&tip->whenstart);
    Printf(dip, "%s\n", COPYRIGHT);
    Printf(dip, "hammer started at %s\n",
	   os_ctime(&tip->whenstart, dip->di_time_buffer, sizeof(dip->di_time_buffer)) );

    if (dip->di_random_seed == 0) {
	dip->di_random_seed = os_create_random_seed();
    }
    set_rseed(dip, dip->di_random_seed);

    tip->filebuf = malloc_palign(dip, hmrp->filebufsize, 0);
    if (tip->filebuf == NULL) return(FAILURE);

    tip->nfiles = 0;
    tip->nfiles_when_full = 0;
    tip->head = NULL;
    tip->lastwrittenfile = NULL;

    /*
     * Comes last, because it uses information obtained above.
     */
    status = init_datablock(dip);

    hammer_startup(dip);

    return(status);
}

void
hammer_startup(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    time_t now;

    Lprintf(dip, "version=%s\n", VERSION);
    Lprintf(dip, "path=%s pid=0x%04X\n", dip->di_dir, tip->mypid);
    Lprintf(dip, "client=%s (%s)\n", tip->clientname, tip->clientver);

#if defined(WIN32)
    if (tip->uncpath) {
	Printf(dip, "uncpath=%s\n", tip->uncpath);
    }
#endif /* defined(WIN32) */

    Lprintf(dip, "minfsize="LXF" maxfsize="LXF"\n", hmrp->minfsize, hmrp->maxfsize);
    Lprintf(dip, "minbsize=0x%08X maxbsize=0x%08X", hmrp->minbsize, hmrp->maxbsize);
    if (hmrp->randombsize) {
	Lprintf(dip, " blocksize=random\n");
    } else {
	Lprintf(dip, " blocksize=0x%08X\n", hmrp->filebufsize);
    }
    Lprintf(dip, "api=%s mode=%s streams=%s flush=%s nocleanup=%s retrydisc=%s seed="LUF"\n",
	    OS_API_TYPE,
	    getmodename(dip),
	    hmrp->nostreams ? "off" : "on",
	    hmrp->onlyflush ? "only" : (hmrp->noflush ? "off" : "random"),
	    hmrp->nocleanup ? "true" : "false",
	    dip->di_retry_disconnects ? "true" : "false",
	    dip->di_random_seed);
    Lprintf(dip, "logfile=%s timezone=%s\n", 
	    (dip->di_log_file) ? dip->di_log_file : "(none)", mktimezone(tip));
    time(&now);
    Lprintf(dip, "num_iterations="LUF" max_iterations="LUF" cur_runtime=%d max_runtime=%d\n",
	    hmrp->num_iterations, hmrp->max_iterations,
	    (int)(now - tip->whenstart), (int)hmrp->max_runtime);

    Lflush(dip);
    return;
}

char *
getmodename(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;

    if (hmrp->mode == creates) return "creates";
    if (hmrp->mode == mixed) return "mixed";
    if (hmrp->mode == overwrites) return "overwrites";
    return "???";
}

/* Note: This function is only responsilble for validating parameters. */

int
hammer_validate_parameters(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    char *style = NULL;

    /* Note: This is to allow of=/dirpath for automation. */
    if ( (dip->di_dir == NULL) && dip->di_output_file) {
	dip->di_dir = strdup(dip->di_output_file);
    } else {
	/* Note: Avoid finding "hammer" directory in current directory. */
	freestr(dip->di_output_file);
	dip->di_output_file = makefullpath(dip, "hammer");
    }
    if (dip->di_dir == NULL) {
        Eprintf(dip, "You must specify a directory for files.\n");
        return(FAILURE);
    }

    /* Check lock flags. */
    if (hmrp->lock_files == True) {
	/* First, initialize some defaults. */
	if (hmrp->lock_mode == NULL) {
	    hmrp->lock_mode = lck_mixed;
	}
    }
#if 0
    /* override lock_mode with user values, if required */
    if ((hmrp->lock_mode != NULL) && (hmrp->lock_files == False)) {
	/* user did not provide the -lockfiles option along with the -lockmode option */
	Eprintf(dip, "provide '-lockfiles' option to initialize locking defaults along with '-lockmode' \n");
	return(FAILURE);
    }
#endif /* 0 */

    /* Check if the master flag to ignore "ALL" errors is set. */
    if (hmrp->halt_on_all_errors == False) {
	Wprintf(dip, "-ignoreallerrors is set, so this will ignore file/lock errors, and corruptions.\n");
	hmrp->halt_on_file_errors = False;
	hmrp->halt_on_lock_errors = False;
	hmrp->halt_on_data_corruption = False;
    }

    if (hmrp->onlyflush && hmrp->noflush) {
	Eprintf(dip, "It doesn't make sense to specify both -noflush and -onlyflush\n");
	return(FAILURE);
    }

    /*
     * Make sure specified parameters are within our limits.
     */
    if (hmrp->maxbsize < hmrp->minbsize) {
	hmrp->minbsize = hmrp->maxbsize;
    }
    if (hmrp->minbsize < MINBSIZE || hmrp->minbsize > MAXBLOCKSIZE) {
	Eprintf(dip, "minbsize must be an integer >= 0x%08X and <= 0x%08X\n",
		MINBSIZE, MAXBLOCKSIZE);
	return(FAILURE);
    }
    if (hmrp->maxbsize > MAXBLOCKSIZE) {
	Eprintf(dip, "%s must be an integer >= 0x%08X and <= 0x%08X\n",
		hmrp->randombsize ? "maxbsize" : "blocksize",
		hmrp->minbsize, MAXBLOCKSIZE);
	return(FAILURE);
    }
    if ( (hmrp->minfsize < MINFSIZE) || (hmrp->minfsize > MAXFSIZE) ) {
	Eprintf(dip, "minfsize must be an integer >= 0x%08X and <= "LXF"\n",
		MINFSIZE, MAXFSIZE);
	return(FAILURE);
    }
    if ( (hmrp->maxfsize < hmrp->minfsize) || (hmrp->maxfsize > MAXFSIZE) ) {
	Eprintf(dip, "maxfsize must be an integer >= "LXF" and <= "LXF"\n",
		hmrp->minfsize, MAXFSIZE);
	return(FAILURE);
    }

    if (hmrp->maxbsize > (int)hmrp->maxfsize) {
	hmrp->maxbsize = (int)hmrp->maxfsize;
    }
    if (hmrp->minbsize > hmrp->maxbsize) {
	hmrp->minbsize = hmrp->maxbsize;
    }
    hmrp->filebufsize = hmrp->maxbsize;
    hmrp->max_runtime = dip->di_runtime;

    /* Note: These are set for common DIO validation checks. */
    if ( (dip->di_bypass_flag == False) &&
	 (dip->di_dio_flag || dip->di_bufmode_count) ) {
	dip->di_min_size = hmrp->minbsize;
	dip->di_max_size = hmrp->maxbsize;
	dip->di_variable_flag = hmrp->randombsize;
	dip->di_data_limit = hmrp->maxfsize;
    }
    return(SUCCESS);
}

void
hammer_show_parameters(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;

    /* Note: hammer parameters are displayed per thread. */
    return;
}

#define P	Print

void
hammer_help(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;

    P(dip, "Usage: %s iobehavior=hammer [dir=DirectoryPath] [options...]\n", cmdname);
    P(dip, "\n");
    P(dip, "    Options:\n");
    P(dip, "\t-help                    Show this help text, then exit.\n");
    /* Add hammer specific help here! */
    P(dip, "\tdir=DirectoryPath        Directory path for hammer files.\n");
    P(dip, "\t                         If omitted, current directory is the default.\n");
#if !defined(WIN32)
    P(dip, "\t-api posix               The only API supported for Unix is POSIX.\n");
#else /* defined(WIN32) */
    P(dip, "\t-api win32               The only API supported for Windows is WIN32.\n");
#endif /* !defined(WIN32) */
    P(dip, "\t-bg                      Don't use stdin or stdout (output to log file).\n");
    P(dip, "\t-checkinodes             File system reusing inodes?\n");
    P(dip, "\t-filercore=FILERNAME     Coredump the filer upon corruption.\n");
    P(dip, "\t-nofilercore             Disable coredumping the filer.\n");
    P(dip, "\t-direct                  Disable filesystem caching.\n");
    P(dip, "\t-fill                    Fill disk and then keep it full.\n");
    P(dip, "\t-interactive             Use interactive mode (not supported).\n");
    P(dip, "\t-iterations NUMBER       The number of iterations to execute.\n");
    P(dip, "\t-logfile FILE            Use logfile FILE. (Default is none).\n");
    P(dip, "\t-mode={mixed | creates | overwrites} (Default: mixed)\n");
    P(dip, "\t                         Use the specified mode.\n");
    P(dip, "\t-nocleanup               Don't remove files upon completion.\n");
    P(dip, "\t-noflush                 Don't use flush buffers (write through).\n");
    P(dip, "\t-onlydelete              When exiting, only delete files.\n");
    P(dip, "\t-onlyflush               Always use async I/O and flush buffers.\n");
#if defined(WIN32)
    P(dip, "\t-nostreams               Don't use NT stream files.\n");
    P(dip, "\t-noretrydisc             Don't retry session disconnects.\n");
    P(dip, "\t-retrydisc               Retry session disconnects.\n");
#endif /* defined(WIN32) */
    P(dip, "\t-runtime=N               Stop hammering after N seconds.\n");
    P(dip, "\t-threads=value           The number of hammer threads.\n");
    P(dip, "\t-seed=value              Set the random seed to use.\n");
    P(dip, "\t-version                 Print the version, then exit.\n");

    P(dip, "\n");
    P(dip, "    Blocksize Options:\n");
    P(dip, "\t-blocksize=NBYTES        Use blocksize NBYTES.\n");
    P(dip, "\t-blocksize=random        Use a random blocksize. (Default: %s)\n",
                                    (DEFAULT_RANDOM_BSIZE) ? "True" : "False");
    P(dip, "\t-minbsize=NBYTES         Set minimum block size to NBYTES. (Default: "LDF")\n",
                                                                    DEFAULT_BLOCK_SIZE_MIN);
    P(dip, "\t-maxbsize=NBYTES         Set maximum block size to NBYTES. (Default: "LDF")\n",
                                                                    DEFAULT_BLOCK_SIZE_MAX);
    P(dip, "\t-minfsize=NBYTES         Set minimum file  size to NBYTES. (Default: "LDF")\n",
                                                                    DEFAULT_FILE_SIZE_MIN);
    P(dip, "\t-maxfsize=NBYTES         Set maximum file  size to NBYTES. (Default: "LDF")\n",
                                                                    DEFAULT_FILE_SIZE_MAX);
    P(dip, "\n");
    P(dip, "    Error Control Options:\n");
    P(dip, "\t-ignorelockerrors        Don't halt on file locking errors, continue.\n");
    P(dip, "\t-ignorefileerrors        Don't halt on file operation errors, continue.\n");
    P(dip, "\t-ignoredatacorruption    Don't halt on data corruption errors, continue.\n");
    P(dip, "\t-ignoreallerrors         Don't halt on any of the above errors, continue.\n");
    P(dip, "\t                         [NOTE: hammer will stop on other critical errors.\n");
    P(dip, "\t                          that can prevent it from functioning properly].\n");

    
    P(dip, "\n");
    P(dip, "    Lock Control Options:\n");
    P(dip, "\t-nolockdebug             Exclude file lock/unlock debug output (it's chatty).\n");
    P(dip, "\t-lockfiles               Include file locks (locks & unlocks) using defaults for the lock options below.\n");
    //P(dip, "\tNote: -lockfiles is required for the following options:\n");
    P(dip, "\t-lockmode={mixed | full | partial}\n");
    P(dip, "\t                         More chance of full or partial file locks (default: mixed).\n");
    P(dip, "\t-unlockchance=[0-100]    Probability of keeping locks and skipping unlocking, 0-100 percent.\n");
    P(dip, "\tExamples:\n");
    P(dip, "\t   if -unlockchance=100  100%% chance of unlocking, ALL files unlocked. [default]\n");
    P(dip, "\t   if -unlockchance=50    50%% chance of unlocking each file.\n");
    P(dip, "\t   if -unlockchance=0      0%% chance of unlocking, NO files are unlocked.\n");

    P(dip, "\n");
    P(dip, "    dt Options Supported:\n");
    P(dip, "\tbufmodes={buffered,unbuffered,cachereads,cachewrites}\n");
    P(dip, "\t                         Set one or more buffering modes (Default: none)\n");
    P(dip, "\tmaxdata=value            The maximum data limit (all files).\n");
    P(dip, "\tmaxdatap=value           The maximum data percentage (range: 0-100).\n");
    P(dip, "\tmaxfiles=value           The maximum files for all directories.\n");
    P(dip, "\tstopon=filename          Watch for file existance, then stop.\n");
    P(dip, "\tenable=raw               The read after write flag.\n");
    P(dip, "\n");
    P(dip, "    Also know, I/O monitoring (noprog*= options), keepalive, and trigger= options\n");
    P(dip, "    are also supported with hammer.\n");

    P(dip, "\n");
    P(dip, "Examples:\n");
    P(dip, "    %% dt iobehavior=hammer dir=/mnt/hammer maxdatap=25 runtime=1h\n");
    P(dip, "    %% dt iobehavior=hammer dir=/mnt/hammer bufmodes=buffered,unbuffered stopon=stopfile\n");
    P(dip, "    %% dt iobehavior=hammer dir=/mnt/hammer -lockfiles -onlydelete -threads=3 log=hammer.log\n");
    P(dip, "\n");
    return;
}

/* ============================================================================== */

char *
makefullpath(dinfo_t *dip, char *path)
{
    char fpath[PATH_BUFFER_SIZE];
    
    (void)sprintf(fpath, "%s%c%s", dip->di_dir, dip->di_dir_sep, path);
    return( strdup(fpath) );
}

/* Note: This dt device name is required for noprog's and triggers! */
void
update_dname(dinfo_t *dip, char *file)
{
    if (file != NULL) {
	if (dip->di_dname) Free(dip, dip->di_dname);
	dip->di_dname = strdup(file);
    }
    return;
}
	     
/*
 * Allocates a datablock and initializes it with the constant stuff
 * that we know about at hammer initialization time.
 */
int
init_datablock(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    datablock_t *datablock;
    char *trimclientname, *firstdot;
    int trimclientnamelen;

    if ((trimclientname = strdup(tip->clientname)) == NULL) {
	Eprintf(dip, "init_datablock: strdup failed");
	return(FAILURE);
    }

    /*
     * Get rid of any domain that might be in the name.
     */
    if ((firstdot = strchr(trimclientname, '.')) != NULL) {
	*firstdot = 0;
    }
    trimclientnamelen = (int)strlen(trimclientname);

    /*
     * The clientname field in struct datablock is only 1 character long,
     * because it's a variable-length field. So, the actual length of the
     * datablock is:
     *    the offset of clientname in struct datablock,
     *    plus the length of our trimclientname,
     *    plus 1 for the termination character
     */
    tip->datablocklen = offsetof(struct datablock, clientname) + trimclientnamelen + 1;

    if ((datablock = Malloc(dip, tip->datablocklen)) == NULL) {
	return(FAILURE);
    }
    tip->datablock = datablock;
    memcpy(datablock->signature, SIGNATURE, SIGNATURE_LENGTH);
    datablock->pid = tobigendian4(tip->mypid);
    memcpy(datablock->clientname, trimclientname, trimclientnamelen);
    datablock->clientname[trimclientnamelen] = CLIENTNAME_TERMCHAR;

    free(trimclientname);
    return(SUCCESS);
}

void
setmem(dinfo_t *dip, char *buf, size_t nbytes, Offset_t offset, uint32_t fileid, uint32_t timestamp)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    datablock_t *datablock = tip->datablock;
    uint32_t n, aligndelta, alignlen;
    char *p;

    aligndelta = (int)offset % tip->datablocklen;
    offset -= aligndelta;
    alignlen = tip->datablocklen - aligndelta;

    /*
     * Fill in the datablock with the variable data we were passed.
     */
    datablock->fileid = tobigendian4(fileid);
    datablock->offset = tobigendian8(offset);
    datablock->timestamp = tobigendian4(timestamp);

    p = (char *)datablock + aligndelta;
    n = (uint32_t)MIN(alignlen, nbytes);
    memcpy(buf, p, n);

    buf += n;
    nbytes -= n;
    offset += tip->datablocklen;
    datablock->offset = tobigendian8(offset);
    if (nbytes == 0) return;
    assert(nbytes > 0);

    while (nbytes >= tip->datablocklen) {
	memcpy(buf, datablock, tip->datablocklen);
	buf += tip->datablocklen;
	nbytes -= tip->datablocklen;
	offset += tip->datablocklen;
	datablock->offset = tobigendian8(offset);
    }
    if (nbytes == 0) return;
    assert(nbytes > 0);

    memcpy(buf, datablock, nbytes);
    return;
}

/* Note: Returns pointer to the start of the bad data! */
void *
chkmem(dinfo_t *dip, char *file, HANDLE *fdp, char *buf, size_t nbytes,
       Offset_t offset, uint32_t fileid, uint32_t timestamp)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    datablock_t *datablock = tip->datablock;
    uint32_t n, aligndelta, alignlen;
    char *base = buf;
    size_t iosize = nbytes;
    char *p;

    aligndelta = (int)offset % tip->datablocklen;
    offset -= aligndelta;
    alignlen = tip->datablocklen - aligndelta;

    /*
     * Fill in the datablock with the variable data we were passed.
     */
    datablock->fileid = tobigendian4(fileid);
    datablock->offset = tobigendian8(offset);
    datablock->timestamp = tobigendian4(timestamp);

    p = (char *)datablock + aligndelta;
    n = (uint32_t)MIN(alignlen, nbytes);
    if (memcmp(buf, p, n) != 0) {
	hammer_report_miscompare_information(dip, file, fdp, base, iosize, n, offset);
	return corruption(dip, (unsigned char *)buf, (unsigned char *)p, n);
    }

    buf += n;
    nbytes -= n;
    offset += tip->datablocklen;
    datablock->offset = tobigendian8(offset);
    if (nbytes == 0) return NULL;
    assert(nbytes > 0);

    while (nbytes >= tip->datablocklen) {
	if (memcmp(buf, datablock, tip->datablocklen) != 0) {
	    hammer_report_miscompare_information(dip, file, fdp, base, iosize, nbytes, offset);
	    return corruption(dip, (unsigned char *)buf, (unsigned char *)datablock, tip->datablocklen);
	}
	buf += tip->datablocklen;
	nbytes -= tip->datablocklen;
	offset += tip->datablocklen;
	datablock->offset = tobigendian8(offset);
    }
    if (nbytes == 0) return NULL;
    assert(nbytes > 0);

    if (memcmp(buf, datablock, nbytes) != 0) {
	hammer_report_miscompare_information(dip, file, fdp, base, iosize, nbytes, offset);
	return corruption(dip, (unsigned char *)buf, (unsigned char *)datablock, nbytes);
    }
    return NULL;
}

unsigned char *
corruption(dinfo_t *dip, unsigned char *bad, unsigned char *good, size_t nbytes)
{
    char *corrmsg;

    corrmsg = mkcorruptmsg(bad, good, nbytes);
    PrintLines(dip, True, corrmsg);
    free(corrmsg);
    return bad;
}

char *
mkcorruptmsg(unsigned char *bad, unsigned char *good, size_t nbytes)
{
    size_t n;
    char *msgp;
    char msgbuf[STRING_BUFFER_SIZE];

    msgp = msgbuf;

    msgp += sprintf(msgp, "CORRUPTION: (%ld bytes, '*' marks corrupted bytes)\n", (long)nbytes);
    msgp += sprintf(msgp, "  expected:");

    for (n = 0; n < nbytes; n++) {
	msgp += sprintf(msgp, " %02X", good[n]);
    }

    msgp += sprintf(msgp, "\n     found:");

    for (n = 0; n < nbytes; n++) {
	if (good[n] == bad[n]) {
	    msgp += sprintf(msgp, " %02X", bad[n]);
	} else {
	    msgp += sprintf(msgp, "*%02X", bad[n]);
	}
    }
    (void)sprintf(msgp, "\n");

    return( strdup(msgbuf) );
}

void
hammer_report_miscompare_information(
    dinfo_t *dip, char *file, HANDLE *fdp, char *base, size_t iosize, size_t nbytes, Offset_t buffer_index)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;

    Printnl(dip);
    /* We must save/restore this offset for trigger scripts. */
    /* This is because triggers are *not* executed until corrupted file is copied! */
    tip->corrupted_offset = dip->di_offset;
    if (dip->di_extended_errors == True) {
	INIT_ERROR_INFO(eip, file, miscompare_op, READ_OP, fdp, dip->di_oflags,
			dip->di_offset, iosize, (os_error_t)0,
			logLevelError, PRT_SYSLOG, RPT_NOERRORMSG);
	ReportErrorNumber(dip);
	dip->di_mode = READ_MODE;
	dip->di_buffer_index = (uint32_t)buffer_index;
	ReportExtendedErrorInfo(dip, eip, NULL);
    } else {
	RecordErrorTimes(dip, True);
	ReportDeviceInfo(dip, iosize, (uint32_t)buffer_index, False, MismatchedData);
	/* Note: history *not* implemented yet! */
	//if (dip->di_history_size) dump_history_data(dip);
    }
    return;
}

void
dumpfilercore(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;

    if ( (dip->di_trigger_control == TRIGGER_ON_ALL) ||
	 (dip->di_trigger_control == TRIGGER_ON_MISCOMPARE) ) {
	dip->di_offset = tip->corrupted_offset;
	(void)ExecuteTrigger(dip, miscompare_op);
    }
    return;
}

/*
 * This function is called whenever a data corruption has been detected.
 */
HANDLE
start_copy(dinfo_t *dip, hammer_file_t *f, int64_t fsize)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    char		corr_file[STRING_BUFFER_SIZE];
    HANDLE		fd;
    int			oflags;

    /*
     * Write bad data to file in same directory as logfile.
     * Note that this copy will begin with the 
     */
    if (f->colon != NULL) {
	*f->colon = '_';
    }
    /* Note: No hammer log file, so this can probably go! */
    if (tip->logdir == NULL) {
	sprintf(corr_file, "%s%cCORRUPT%d-%s", dip->di_dir, dip->di_dir_sep, tip->ncopies++, f->path);
    } else {
	sprintf(corr_file, "%s%cCORRUPT%d-%s", tip->logdir, dip->di_dir_sep, tip->ncopies++, f->path);
    }
    if (f->colon != NULL) {
	*f->colon = ':';
    }
    oflags = (O_WRONLY | O_BINARY | O_CREAT | O_TRUNC);
    fd = dt_open_file(dip, corr_file, oflags, FILE_CREATE_MODE, NULL, NULL, EnableErrors, EnableRetries);
    if (fd == NoFd) {
	return(NoFd);
    }

    Printf(dip, "copying from %s to %s starting at offset "LL0XFMT"\n",
	   f->fpath, corr_file, fsize);

    if (hmrp->inode_check == True) {
	os_ino_t inode = inode_lookup(corr_file, fd);
	Printf(dip, "%s INODE ADD start_copy path=%s inode="LUF"\n",
	       mklogtime(tip), corr_file, inode);
	inode_add(dip, tip, inode);
    }

    if (tip->corrupted_file) Free(dip, tip->corrupted_file);
    tip->corrupted_file = strdup(corr_file);

    return(fd);
}

int
choose_action(dinfo_t *dip, hammer_parameters_t *hmrp)
{
    long n;

    if (hmrp->next_action != INVALID_ACTION) {
	int act;
	act = hmrp->next_action;
	hmrp->next_action = INVALID_ACTION;
	return act;
    }

    n = rnd(dip, 1, 100);
    if (n >= hmrp->mode[CREATEFILE].lower && n <= hmrp->mode[CREATEFILE].upper) {
	return CREATEFILE;
    } else if (n >= hmrp->mode[RENAMEFILE].lower && n <= hmrp->mode[RENAMEFILE].upper) {
	return RENAMEFILE;
    } else if (n >= hmrp->mode[OWRITEFILE].lower && n <= hmrp->mode[OWRITEFILE].upper) {
	return OWRITEFILE;
    } else if (n >= hmrp->mode[TRUNCFILE].lower && n <= hmrp->mode[TRUNCFILE].upper) {
	return TRUNCFILE;
    } else if (n >= hmrp->mode[DELETEFILE].lower && n <= hmrp->mode[DELETEFILE].upper) {
	return DELETEFILE;
    } else {
	Eprintf(dip, "choose_action: mode couldn't handle n=%ld?", n);
    }
    return -1;
}

int
cleanup_files(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    int status = SUCCESS;

    if (hmrp->nocleanup) {
	Printf(dip, "nocleanup was specified -- skipping cleanup.\n");
	return(status);
    }
    Printf(dip, "cleaning up...\n");
    dip->di_deleting_flag = True;
    while (tip->head != NULL) {
	hammer_file_t *f = tip->head;
	if (f->fpath == NULL) {
	    f->fpath = makefullpath(dip, f->path);
	}
	update_dname(dip, f->fpath);
	status = deletefile(dip, f, hmrp->onlydelete);
	if (status == FAILURE) break;
    }
    dip->di_deleting_flag = False;
    return(status);
}

void
cleanup_hammer(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    time_t whenstop;

    (void)freefiles(dip);

    hammer_startup(dip);
    time(&whenstop);
    Printf(dip, "hammer stopped at %s\n",
	   os_ctime(&whenstop, dip->di_time_buffer, sizeof(dip->di_time_buffer)) );

    return;
}

hammer_file_t *
allocfile(dinfo_t *dip, char *path)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    hammer_file_t *newfile;

    if ( ( newfile = Malloc(dip, sizeof(*newfile)) ) == NULL) {
	Eprintf(dip, "allocfile: malloc failed, nbytes=0x%08X", (int)sizeof *newfile);
	return(NULL);
    }
    newfile->sig = FILESIG;
    newfile->path = strdup(path);
    newfile->colon = strchr(newfile->path, ':');
    newfile->fpath = makefullpath(dip, newfile->path);
    newfile->hasdir = False;
    newfile->is_disk_full = False;
    update_dname(dip, newfile->fpath);

    if (tip->head == NULL) {
	tip->head = newfile;
	newfile->prev = newfile->next = NULL;
    } else {
	tip->head->prev = newfile;
	newfile->next = tip->head;
	tip->head = newfile;
	tip->head->prev = NULL;
    }

    if (tip->nfiles < 0) {
	Eprintf(dip, "allocfile: BUG! negative nfiles="LXF"\n", tip->nfiles);
    }
    tip->nfiles++;

    return newfile;
}

int
freefile(dinfo_t *dip, hammer_file_t *file)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;

    if (dip->di_max_data) {
	dip->di_maxdata_written -= file->size;
	/* Sanity check, this should not happen! */
	if ((slarge_t)dip->di_maxdata_written < 0) {
	    if (dip->di_fDebugFlag) {
		Printf(dip, "Max data written has gone negative, bug!!!\n");
	    }
	    dip->di_maxdata_written = 0;
	}
    }
    //Printf(dip, "DEBUG: Free'ing file %s...\n", file->path);
    if (tip->lastwrittenfile == file) {
	tip->lastwrittenfile = NULL;
    }
    if (file == tip->head) {
	if ((tip->head = file->next) != NULL) {
	    tip->head->prev = NULL;
	}
    } else {
	if (file->next != NULL) {
	    file->next->prev = file->prev;
	}
	file->prev->next = file->next;
    }
    if (file->sig != FILESIG) {
	tip->head = NULL;	/* don't recurse */
	Eprintf(dip, "freefile: %s: sig=0x%08X", file->path, file->sig);
	return(FAILURE);
    }
    freestr(file->path);
    freestr(file->fpath);
    memset(file, 0xdd, sizeof(*file));
    free(file);
    if (tip->nfiles <= 0) {
	tip->head = NULL;	/* don't recurse */
	Eprintf(dip, "freefile: nfiles="LL0XFMT, tip->nfiles);
	return(FAILURE);
    }
    tip->nfiles--;
    return(SUCCESS);
}

int
freefiles(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    int status = SUCCESS;

    while (tip->head != NULL) {
	status = freefile(dip, tip->head);
	if (status == FAILURE) break;
    }
    return(status);
}


hammer_file_t *
getrndfile(dinfo_t *dip)
{  
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_file_t *f;
    uint64_t n;

    if (tip->nfiles == 0) return NULL;

    n = rnd64(dip, 0, tip->nfiles - 1);
    for (f = tip->head; f != NULL && n > 0; f = f->next) {
	n--;
    }
    if (f != NULL) {
	if (f->fpath == NULL) {
	    f->fpath = makefullpath(dip, f->path);
	}
	update_dname(dip, f->fpath);
    }

    return f;
}

hammer_file_t *
findfile(hammer_thread_info_t *tip, char *path)
{
    hammer_file_t *f;

    for (f = tip->head; f != NULL; f = f->next) {
	if (STREQ(f->path, path)) {
	    /* Note: We don't need the file path as used today! */
	    return f;
	}
    }
    return NULL;
}

static hammer_file_t *
findotherstream(dinfo_t *dip, hammer_thread_info_t *tip, hammer_file_t *f)
{
    hammer_file_t *other;
    int len;

    if (f->colon != NULL) {
	*f->colon = 0;
    }
    len = (int)strlen(f->path);

    for (other = tip->head; other != NULL; other = other->next) {
	if (other != f &&
	    (STREQ(other->path, f->path) ||
	     (other->colon != NULL && (other->colon - other->path) == len &&
	      strncmp(other->path, f->path, len) == 0))) {
	    break;
	}
    }
    if (f->colon != NULL) {
	*f->colon = ':';
    }
    if (other != NULL) {
	if (other->fpath == NULL) {
	    other->fpath = makefullpath(dip, other->path);
	}
	update_dname(dip, other->fpath);
    }
    return other;
}

uint64_t
newrndfilenum(hammer_thread_info_t *tip)
{
    tip->file_number++;
    //Printf(tip->dip, "DEBUG: File Number: "LLHXFMT"\n");
    return( tip->file_number );
}

/* hammer files use the file number for its' name! */
#define FILENAME_NOSTREAM	LLHXFMT".ham"
#define FILENAME_STREAM		LLHXFMT".ham:s"LLHXFMT
#define FILENAME_ADDSTREAM	"%s:s"LLHXFMT

hammer_file_t *
newrndfile(dinfo_t *dip)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    char path[PATH_BUFFER_SIZE];
    hammer_file_t *f, *base;
    uint64_t n;

    base = NULL;
    n = newrndfilenum(tip);

    /* Stream files are for Windows only! */
    if (hmrp->nostreams == True) {
	(void)sprintf(path, FILENAME_NOSTREAM, n);
    } else { /* OS supports streams and enabled! */
	long chance = rnd(dip, 1, 100);
	if ( (chance > 66) && (base = getrndfile(dip)) != NULL) {
	    /*
	     * Add a stream to a random existing file. If the random
	     * existing file is a stream with an empty base (i.e. a
	     * base that we have no hammer_file_t for) then just create
	     * a hammer_file_t for that base and use that instead.
	     */
	    if (base->colon != NULL) {
		*base->colon = 0;
	    }
	    if ( (base->hasdir == False) && (base->colon != NULL) && findfile(tip, base->path) == NULL) {
		strcpy(path, base->path);
		/* It had better be an existing file! */
		if (dt_isfile(dip, path, EnableErrors) == False) {
		    Eprintf(dip, "newrndfile: %s is not a file!\n", path);
		    //return(NULL); // or some error!
		}
	    } else {
		(void)sprintf(path, FILENAME_ADDSTREAM, base->path, n);
	    }
	    if (base->colon != NULL) {
		*base->colon = ':';
	    }
	} else if (chance > 33) {
	    /* new stream */
	    (void)sprintf(path, FILENAME_STREAM, n, n);
	} else {
	    /* regular file */
	    (void)sprintf(path, FILENAME_NOSTREAM, n);
	}
    }

    if ( (f = allocfile(dip, path)) != NULL ) {
	f->base = base;
	if (base != NULL) {
	    f->hasdir = base->hasdir;
	}
    }
    return f;
}

int
updatesize(dinfo_t *dip, hammer_file_t *f)
{
    HANDLE fd;
    large_t filesize;
    
    fd = dt_open_file(dip, f->fpath, O_RDONLY, 0, NULL, NULL, EnableErrors, EnableRetries);
    if (fd == NoFd) return(FAILURE);
    
    filesize = dt_get_file_size(dip, f->fpath, &fd, EnableErrors);
    if (filesize != (large_t)FAILURE) {
	f->size = filesize;
    }
    (void)dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
    return(SUCCESS);
}

int
writefile(dinfo_t *dip, hammer_file_t **fp)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    hammer_file_t *f = *fp;
    int do_overwrite = (f != NULL);
    int bsize;
    int64_t fsize;
    double time_taken;
    hbool_t isDirectory = False;
    hbool_t isFileExists = False;
    int err;
    int status = SUCCESS;

    if (hmrp->randombsize == True) {
	bsize = RNDBSIZE(dip, hmrp);
	if (dip->di_dio_flag || dip->di_bufmode_count) {
	    bsize = roundup(bsize, dip->di_dsize);
	}
    } else{
	bsize = hmrp->filebufsize;
    }
    /* Create a new file. */
    if (f == NULL) {
	f = newrndfile(dip);
        *fp = f;
	fsize = RNDFSIZE(dip, hmrp);
	if (dip->di_dio_flag || dip->di_bufmode_count) {
	    fsize = roundup(fsize, dip->di_dsize);
	}
	/* 
	 * For Windows streams, 50% of the time create streams directory/file.
	 * URL: http://msdn.microsoft.com/en-us/library/windows/desktop/aa364404(v=vs.85).aspx
	 * 
	 * Note: For Unix, -streams will enable *testing* this code path! (no actual streams)
	 */ 
	if ( f->colon && (f->base == NULL) && (rnd(dip, 1, 100) > 50) ) {
	    char *fpath;
	    *f->colon = '\0';
	    fpath = makefullpath(dip, f->path);
	    /* Note: This message was added by Robin for debug! */
	    Printf(dip, "%s MKDIR   %s", mklogtime(tip), f->path);
	    start_timer(dip);
mkdiragain:	    
	    status = dt_create_directory(dip, fpath, &f->is_disk_full, &isFileExists, EnableErrors);
	    if (status == SUCCESS) {
		f->hasdir = True;
	    } else { /* Failure... */
		if (isFileExists == True) {
		    /* Remove it, since it might be a regular file. */
		    if (removepath(dip, fpath) == FAILURE) {
			freestr(fpath);
			return(FAILURE);
		    }
		    goto mkdiragain;
		}
		if (f->is_disk_full == False) {
		    tip->file_errors++;
		    freestr(fpath);
		    return(FAILURE);
		}
		/* Otherwise just try a regular file, on disk full. */
	    }
	    freestr(fpath);
	    time_taken = stop_timer(dip);
	    Print(dip, " %gsec", time_taken);
	    /* Make sure "disk full", is well known! */
	    if (f->is_disk_full == True) {
		os_error_t error = os_get_error();
		Print(dip, " - %s! %s...\n", os_getDiskFullSMsg(error), disk_full_str);
	    } else {
		Printnl(dip);
	    }
	    *f->colon = ':';
	} else if ( f->colon && (f->base == NULL) ) {
	    /* Note: This code exists to test streams on Unix! */
	    /* FWIW: This could be expanded to test subdirectories (IMHO). */
#if !defined(WIN32)
	    /* Gotta manually create the base file. */
	    HANDLE fd, did_delete;
	    *f->colon = '\0';
	    did_delete = False;
openagain:
	    dip->di_oflags = (O_CREAT | O_WRONLY);
	    fd = dt_open_file(dip, f->fpath, dip->di_oflags, FILE_CREATE_MODE,
			      &f->is_disk_full, &isDirectory, EnableErrors, EnableRetries);
	    if (fd != NoFd) {
		status = dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
	    }
	    if ( (fd == NoFd) || (status == FAILURE) ) {
		os_error_t error = os_get_error();
		if ( (did_delete == False) && (isDirectory == True) ) {
		    if (removepath(dip, f->fpath) == FAILURE) {
			return(FAILURE);
		    }
		    did_delete = True;
		    goto openagain;
		}
		if (f->is_disk_full == True) {
		    /* Just leave the colon NULL. */
		    f->colon = NULL;
		} else {
		    *f->colon = ':';
		}
	    } else { /* Successful open! */
		*f->colon = ':';
	    }

	    if (hmrp->inode_check) {
		os_ino_t inode = inode_lookup(f->fpath, fd);
		Printf(dip, "%s INODE ADD writefile path=%s inode="LUF,
		       mklogtime(tip), f->path, inode);
		inode_add(dip, tip, inode);
	    }
#else /* defined(WIN32) */
	    /*
	     * There might already be a base -- find out if it's a directory.
	     */
	    *f->colon = '\0';
	    f->hasdir = dt_isdir(dip, f->fpath, DisableErrors);
	    *f->colon = ':';
#endif /* !defined(WIN32) */
	}
    } else {
	fsize = f->size;
    }
    start_timer(dip);
    /* Ok, finally do the actual file writing! */
    err = api_writefile(dip, f, bsize, fsize, do_overwrite);
    time_taken = stop_timer(dip);
    if (err == HAMMER_NO_STREAMS) {
	Eprintf(dip, "Couldn't create - discontinuing Windows streams testing!\n");
	hmrp->nostreams = True;
	*f->colon = 0;
	if ( (f->hasdir == True) && (removepath(dip, f->fpath) == FAILURE) ) {
	    return(FAILURE);
	}
	/* file was never created */
        *fp = NULL;
	freefile(dip, f);
	return(FAILURE);
    } else if (err == HAMMER_DISK_FULL) {
	Print(dip, " open - %s...\n", disk_full_str);
	/*
	 * If it's a stream and nothing else is using the base, then delete the base.
	 */
	if ( (do_overwrite == False) && (f->colon != NULL) ) {
	    if (findotherstream(dip, tip, f) == NULL) {
		*f->colon = 0;
		if (dt_file_exists(dip, f->fpath) == True) {
		    if (removepath(dip, f->fpath) == FAILURE) {
			return(FAILURE);
		    }
		}
		f->colon = NULL;
	    }
	}
	/* file will be freed below */
    } else if (f->size < fsize) { /* Partial write! */
	/*
	 * Oops, now we don't know how big the file really is,
	 * so we have to update the size from the file attributes.
	 */
	updatesize(dip, f);
	Print(dip, " %gK/s", KPS(f->size, time_taken));
	if (f->is_disk_full == True) {
	    os_error_t error = os_getDiskFullError();
	    Print(dip, " -- %s! %s -- ", os_getDiskFullSMsg(error), disk_full_str);
	} else {
	    Print(dip, " -- partial file -- ");
	}
	Print(dip, "(wrote "LL0XFMT")\n", f->size);
    } else {
	Print(dip, " %gK/s\n", KPS(f->size, time_taken));
    }
    if ( (err == HAMMER_DISK_FULL) || (f->size < fsize) ) {
	if ( (THREAD_TERMINATING(dip) == False) && (hmrp->disk_filled == False) ) {
	    Printf(dip, "Setting disk as full, to start removing files...\n");
	    setdiskisfull(hmrp, tip);
	}
#if defined(WIN32)
	/* 
	 * Why Windows only? Why not all OS's?
	 * 
	 * Our request failed because the disk is full. Since
	 * we might be using Write Raw, this means we shouldn't
	 * believe lastwrittenfile->size anymore, because Write
	 * Raw can hide write errors from us sometimes. So, we
	 * refresh lastwrittenfile->size by stat'ing the file.
	 */
	if (tip->lastwrittenfile) {
	    tip->lastwrittenfile->fpath = makefullpath(dip, tip->lastwrittenfile->path);
	    updatesize(dip, tip->lastwrittenfile);
	}
#endif /* defined(WIN32) */
	if (err == HAMMER_DISK_FULL) {
	    /* File may not have been created! */
            *fp = NULL;
	    freefile(dip, f);
	    return(FAILURE);
	}
    }
    tip->lastwrittenfile = f;
    return(SUCCESS);
}

/*
 * Returns:
 * 0 if success
 * -1 if open returned "disk full" error
 * -2 if streams not supported
 * 1 on all other errors (open, close, write, remove, flush failed)
 */
int
api_writefile(dinfo_t *dip, hammer_file_t *f, int bsize, int64_t fsize, int do_overwrite)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    HANDLE fd;
    int writethroughflag;
    int fopen_mode;
    size_t n;
    ssize_t wrote;
    hbool_t did_delete = False;
    int64_t nleft;
    int numdisconnects = 0;
    hbool_t isDirectory = False;
    hbool_t lock_full_range = False;
    int status = HAMMER_SUCCESS;

    dip->di_mode = WRITE_MODE;
    f->is_disk_full = False;
again:
    if (do_overwrite) {
	uint32_t oldfileid;
	/*
	 * Set a new fileid and timestamp, since we are	overwriting the file with new data.
	 */
	oldfileid = f->fileid;
	setfileid(f);
	setfiletimestamp(f);
	Printf(dip, "%s OWRITE  %s fileid=0x%08X blocksize=0x%08X filesize="LL0XFMT" timestamp=0x%08X oldfileid=0x%08X",
	       mklogtime(tip), f->path, f->fileid, bsize, fsize, f->timestamp, oldfileid);
    } else {
	setfileid(f);
	setfiletimestamp(f);
	Printf(dip, "%s CREATE  %s fileid=0x%08X blocksize=0x%08X filesize="LL0XFMT" timestamp=0x%08X",
	       mklogtime(tip), f->path, f->fileid, bsize, fsize, f->timestamp);
    }

    SetupBufferingMode(dip, &fopen_mode);
    if (dip->di_dio_flag == True) {
	writethroughflag = O_DIRECT;
	Print(dip, " (direct)");
    } else if (hmrp->onlyflush == True) {
	writethroughflag = 0;
	Print(dip, " (async then flush)");
    } else if ( (hmrp->noflush == True) || (rnd(dip, 1, 100) > 50) ) {
	writethroughflag = O_SYNC;
	Print(dip, " (sync)");
    } else {
	writethroughflag = 0;
	Print(dip, " (async then flush)");
    }

    if (hmrp->lock_files == False) {
	/* keep legacy behavior if locking is not being tested */
	fopen_mode = O_WRONLY;
    } else { /* Locking files. */
	/* select a random open mode between WR_ONLY and RDWR */
	if (rnd(dip, 1, 100) > 50) {
	    fopen_mode = O_WRONLY;
	} else {
	    fopen_mode = O_RDWR;
	}
    }

    /* open file */
    
    dip->di_oflags = ( fopen_mode | O_BINARY | writethroughflag | 
		       ( (do_overwrite) ? O_TRUNC : (O_CREAT|O_TRUNC)) );
	      
    fd = dt_open_file(dip, f->fpath, dip->di_oflags, FILE_CREATE_MODE,
		      &f->is_disk_full, &isDirectory, EnableErrors, EnableRetries);
    if ( (dip->di_retry_disconnects == True) && dip->di_retry_count) {
	numdisconnects += dip->di_retry_count;
    }
    if (fd == NoFd) {
	os_error_t error = os_get_error();
	if ( (did_delete == False) && (isDirectory == True) ) {
	    if (removepath(dip, f->fpath) == FAILURE) {
		return(HAMMER_FAILURE);
	    }
	    did_delete = True;
	    goto again;
	}
#if defined(WIN32)
	if ( (do_overwrite == False) && (strchr(f->path, ':') != NULL) ) {
	    /*
	     * Failed creating a new stream.
	     * Maybe streams aren't supported?
	     */
	    os_error_t error = os_get_error();
	    if ( os_isStreamsUnsupported(error) ) {
		/*
	         * Indicate streams is not supported.
		 */
		return(HAMMER_NO_STREAMS);
	    }
	}
#endif /* defined(WIN32) */
	if (f->is_disk_full == True) {
	    return(HAMMER_DISK_FULL);
	} else {
	    return(HAMMER_FAILURE);
	}
    }

    if ( (hmrp->inode_check == True) && (do_overwrite == False) &&
	 (strchr(f->path, ':') == NULL) ) {
	os_ino_t inode = inode_lookup(f->fpath, fd);
	Print(dip, "\n");
	Printf(dip, "%s INODE ADD api_writefile path=%s inode="LUF,
	       mklogtime(tip), f->path, inode);
	inode_add(dip, tip, inode);
    }

    /* Before writing, try to obtain a lock: fcntl() */
    if (hmrp->lock_files == True) {
	/* 
	 * Randomly decide whether to lock the full byte range across the file,
	 * or lock specific bytes as we write (code in WRITE block below).
	 */ 
	if (test_lock_mode(dip, hmrp, LOCK_FULL_RANGE)) {
	    /* LOCK_FULL_RANGE = True */
	    lock_full_range = True;    /* else, keep default = False */
	    api_lockfile(dip, &fd, f, FLAG_LOCK_WRITE, (Offset_t)0, (Offset_t)fsize);
	    if (dip->di_lDebugFlag == True) {
		Printf(dip, "%s - WRITE %s fileid=0x%08X bytes="LDF", locked full byte range",
		       mklogtime(tip), f->path, f->fileid, fsize);
	    }
	}
    }

    nleft = fsize;
    dip->di_dbytes_written = 0;
    dip->di_records_written = 0;

    while (nleft != 0) {
	
	PAUSE_THREAD(dip);
	if ( THREAD_TERMINATING(dip) ) break;

	dip->di_offset = (Offset_t)(fsize - nleft);
	n = (nleft > bsize) ? bsize : (ssize_t)nleft;
	setmem(dip, tip->filebuf, n, (Offset_t)(fsize - nleft), f->fileid, f->timestamp);

	/* Before writing, try to obtain a byte range lock on only the bytes we wish to write. */
	if ( (hmrp->lock_files == True) && (lock_full_range == False) ) {
	    /* LOCK_PARTIAL_RANGE = True */
	    api_lockfile(dip, &fd, f, FLAG_LOCK_WRITE, (Offset_t)(fsize - nleft), (Offset_t)n);
	    if (dip->di_lDebugFlag == True) {
		Printf(dip, "%s - WRITE %s fileid=0x%08X bytes="LUF, mklogtime(tip), f->path, f->fileid, n);
	    }
	}

	/* Write bytes to file */
	wrote = dt_write_file(dip, f->fpath, &fd, tip->filebuf, n, &f->is_disk_full, True, True);
	if ( (dip->di_retry_disconnects == True) && dip->di_retry_count) {
	    numdisconnects += dip->di_retry_count;
	}
	if (wrote != FAILURE) {
	    dip->di_records_written++;
	    dip->di_dbytes_written += wrote;
	}
	if (wrote < (ssize_t)n) {
	    if ( (wrote == FAILURE) && (f->is_disk_full == False) ) {
		status = dt_close_file(dip, f->fpath, &fd, &f->is_disk_full, EnableErrors, EnableRetries);
		tip->file_errors++;
		f->size = (fsize - nleft);	/* Update the file size based on written. */
		return(HAMMER_FAILURE);
	    }
	    if (wrote > 0) {
		nleft -= wrote;
	    }
	    break;
	}
	/* Note: We should *never* write more than requested! */
	if (wrote > (ssize_t)n) {
	    (void)dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
	    Eprintf(dip, "api_writefile: %s: nwritten="LXF" is greater than nbytes="LXF"\n", f->path, wrote, n);
	    return(HAMMER_DISK_FULL); /* Treat like "disk is full" */
	}

	if ( (hmrp->lock_files == True) && (lock_full_range == False) ) {
	    if (unlock_file_chance(dip, hmrp) == False) {
		/* let's leave this lock here */
		Print(dip, "\n");
		Printf(dip, "%s - UNLCK %s fileid=0x%08X ** randomly decided to skip unlocking this byte range **",
		       mklogtime(tip), f->path, f->fileid);
	    } else {
		/* unlock the partial byte range */
		api_lockfile(dip, &fd, f, FLAG_UNLOCK, (Offset_t)(fsize - nleft), (Offset_t)n);
	    }
	}
	nleft -= wrote;
    } /* end while (nleft != 0) */

    /*
     * Flush the file data out if the file wasn't open in write through mode.
     */
    if (writethroughflag == 0) {
	if (hmrp->noflush == True) {
	    status = dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
	    /* Note: This looks like a user error, message is nondescriptive! */
	    Eprintf(dip, "api_writefile: fsync'ing when noflush=True?\n");
	    return(HAMMER_DISK_FULL); /* WTF? */
	}
	status = dt_flush_file(dip, f->fpath, &fd, &f->is_disk_full, EnableErrors);
	if (status == FAILURE) {
	    if (f->is_disk_full == False) {
		status = dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
		tip->file_errors++;
		return(HAMMER_FAILURE);
	    }
	    /*
	     * One or more of the async writes must have failed,
	     * but we don't know exactly how many. So, just
	     * indicate that at least one byte didn't make it.
	     */
	    nleft = 1;
	}
    }

    /* done writing, now, release the WRITE lock. */
    if ( (hmrp->lock_files == True) && (lock_full_range == True) ) {
	if (unlock_file_chance(dip, hmrp) == False) {
	    /* let's leave this lock here */
	    if (dip->di_lDebugFlag == True) {
		Print(dip, "\n");
		Printf(dip, "%s - UNLCK %s fileid=0x%08X ** randomly decided to skip unlocking the full byte range **",
		       mklogtime(tip), f->path, f->fileid);
	    }
	} else {
	    /* unlock the full byte range */
	    api_lockfile(dip, &fd, f, FLAG_UNLOCK, (Offset_t)0, (Offset_t)fsize);
	}
    }

    status = dt_close_file(dip, f->fpath, &fd, &f->is_disk_full, EnableErrors, EnableRetries);
    if (status == FAILURE) {
	if (f->is_disk_full == False) {
	    tip->file_errors++;
	    return(HAMMER_FAILURE);
	}
	/*
	 * One or more of the async writes must have failed,
	 * but we don't know exactly how many. So, just
	 * indicate that at least one byte didn't make it.
	 */
	nleft = 1;
    }
    if ( (dip->di_retry_disconnects == True) && dip->di_retry_count) {
	numdisconnects += dip->di_retry_count;
    }

    if (dip->di_max_data) {
	int64_t bytes_written = (fsize - nleft);;
	if (do_overwrite == False) {
	    dip->di_maxdata_written += bytes_written;
	} else if (bytes_written > f->size) {
	    dip->di_maxdata_written += (bytes_written - f->size);
	}
    }
    /* Note: See special error handling in writefile()... this is messy! */
    f->size = (fsize - nleft);	/* Update the file size based on written. */

    /*
     * If we had disconnects or read after write, read the file immediately!
     */
    if ( (numdisconnects > 0) || (dip->di_raw_flag == True) ) {
	int rstatus;
	/* Avoid "unexpected file size encountered" error during reading! */
	if ( (f->is_disk_full == True) || (f->size < fsize) ) {
	    updatesize(dip, f);
	}
	Printnl(dip); /* required for current hammer logging... */
	rstatus = readfile(dip, f);
	if (rstatus == FAILURE) status = rstatus;
    }
    return( (status == SUCCESS) ? HAMMER_SUCCESS : HAMMER_FAILURE );
}

int
readfile(dinfo_t *dip, hammer_file_t *f)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    int bsize;
    double time_taken;
    int status;

    if (hmrp->randombsize) {
	bsize = RNDBSIZE(dip, hmrp);
	if (dip->di_dio_flag || dip->di_bufmode_count) {
	    bsize = roundup(bsize, dip->di_dsize);
	}
    } else {
	bsize = hmrp->filebufsize;
    }
    Printf(dip, "%s READ    %s fileid=0x%08X blocksize=0x%08X filesize="LL0XFMT,
	   mklogtime(tip), f->path, f->fileid, bsize, f->size);
    start_timer(dip);
    status = api_readfile(dip, f, bsize);
    time_taken = stop_timer(dip);
    Print(dip, " %gK/s\n", KPS(f->size, time_taken));

    return(status);
}

int
api_readfile(dinfo_t *dip, hammer_file_t *f, int bsize)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    HANDLE fd, copyfd = NoFd;
    ssize_t n;
    int lock_full_range = FALSE;
    int64_t read_lock_size = 0;
    int64_t fsize;
    int status;

    dip->di_mode = READ_MODE;
    dip->di_oflags = (O_RDONLY | O_BINARY);
    SetupBufferingMode(dip, &dip->di_oflags);
    if (dip->di_dio_flag == True) {
	dip->di_oflags |= O_DIRECT;
	Print(dip, " (direct)");
    } else if (dip->di_bufmode_count) {
	Print(dip, " (async)");
    }
    fd = dt_open_file(dip, f->fpath, dip->di_oflags, 0, NULL, NULL, EnableErrors, EnableRetries);
    if (fd == NoFd) {
	Fprintf(dip, "api_readfile: %s: open failed, expectedsize="LL0XFMT, f->path, f->size);
	return(FAILURE);
    }

    /* Before reading, try to obtain a read lock. */
    if (hmrp->lock_files == True) {
	/* 
	 * Randomly decide whether to lock the full byte range across the file,
	 * or lock specific bytes as we read.
	 */
	if (test_lock_mode(dip, hmrp, LOCK_FULL_RANGE)) {
	    /* LOCK_FULL_RANGE = TRUE */
	    lock_full_range = True;    /* else, keep default = FALSE */
	    read_lock_size = f->size;
	    api_lockfile(dip, &fd, f, FLAG_LOCK_READ, (Offset_t)0, (Offset_t)read_lock_size);
	    if (dip->di_lDebugFlag == True) {
		Printf(dip, "%s - READ  %s fileid=0x%08X bytes="LDF", locked full byte range",
		       mklogtime(tip), f->path, f->fileid, f->size);
	    }
	} else {
	    /* LOCK_PARTIAL_RANGE = TRUE */
	    /* 
	     * Before reading, try to obtain a byte range lock on the first few bytes
	     * that we wish to read.
	     */ 
	    if (bsize < f->size) {
		read_lock_size = bsize;
	    } else {
		read_lock_size = f->size;
	    }
	    api_lockfile(dip, &fd, f, FLAG_LOCK_READ, (Offset_t)0, (Offset_t)read_lock_size);
	    if (dip->di_lDebugFlag == True) {
		Printf(dip, "%s - READ  %s fileid=0x%08X bytes="LL0XFMT"",
		       mklogtime(tip), f->path, f->fileid, read_lock_size);
	    }
	}
    }
    if (f->is_disk_full == True) {
	Print(dip, " (no validation)");
    }

    /* read bytes from file */
    fsize = 0;
    dip->di_offset = (Offset_t)0;
    dip->di_dbytes_read = 0;
    dip->di_records_read = 0;

    /* Note: Expect to encounter EOF when count will be zero! */
    while ((n = dt_read_file(dip, f->fpath, &fd, tip->filebuf, bsize, EnableErrors, EnableRetries)) > 0) {
	char *badaddr;

	dip->di_records_read++;
	dip->di_dbytes_read += n;

	PAUSE_THREAD(dip);
	/* Note: We do reads when cleaning up files, so allow this behavior. */
	//if ( THREAD_TERMINATING(dip) ) break;

	if ( (copyfd == NoFd) && (f->is_disk_full == False) &&
	     (badaddr = chkmem(dip, f->fpath, &fd, tip->filebuf, n, (Offset_t)fsize, f->fileid, f->timestamp)) != NULL) {
	    Printf(dip, "CORRUPTION: %s: offset="LL0XFMT", expectedsize="LL0XFMT"\n",
		   f->path, fsize + (badaddr - tip->filebuf), f->size);
	    if ((copyfd = start_copy(dip, f, fsize)) == NoFd) {
		if (dip->di_num_triggers) {
		    dumpfilercore(dip);
		}
		(void)dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
		Printf(dip, "CORRUPTION found, stopped reading this file...\n");
		tip->data_corruptions++;
		return(FAILURE);
	    }
	    /* keep going, copying the data out */
	}
	if ( (copyfd != NoFd) && 
	     (dt_write_file(dip, tip->corrupted_file, &copyfd, tip->filebuf, n, &f->is_disk_full, True, True) != n) ) {
	    Printf(dip, "error copying corruption\n");
	    (void)dt_close_file(dip, tip->corrupted_file, &copyfd, NULL, EnableErrors, EnableRetries);
	    (void)dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
	    if (dip->di_num_triggers) {
		dumpfilercore(dip);
	    }
	    Eprintf(dip, "CORRUPTION found, stopped reading this file...");
	    tip->data_corruptions++;
	    return(FAILURE);
	}
	if (hmrp->lock_files && !lock_full_range) {
	    if (unlock_file_chance(dip, hmrp) == False) {
		/* let's leave this lock here */
		Printnl(dip);
		Printf(dip, "%s - UNLCK %s fileid=0x%08X ** randomly decided to skip unlocking this byte range **",
		       mklogtime(tip), f->path, f->fileid);
	    } else {
		/* unlock the partial byte range */
		api_lockfile(dip, &fd, f, FLAG_UNLOCK, (Offset_t)fsize, (Offset_t)read_lock_size);
	    }
	    /* now, try to obtain a byte range lock on the next few bytes that we wish to read: fcntl() */
	    if ((fsize + n) < f->size) {
		/* we have at least 1 more byte left */
		read_lock_size = bsize;
		api_lockfile(dip, &fd, f, FLAG_LOCK_READ, (uint32_t)(fsize + n), (uint32_t)read_lock_size);
		if (dip->di_lDebugFlag == True) {
		    Printf(dip, "%s - READ  %s fileid=0x%08X bytes="LUF, mklogtime(tip), f->path, f->fileid, n);
		}
	    }
	}
	fsize += n;
	dip->di_offset = (Offset_t)fsize;
    } /* end while ((n = dt_read_file(dip... */

    if (copyfd != NoFd) {
	(void)dt_close_file(dip, tip->corrupted_file, &copyfd, NULL, EnableErrors, EnableRetries);
	(void)dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
	if (dip->di_num_triggers) {
	    dumpfilercore(dip);
	}
	Printf(dip, "CORRUPTION found...");
	tip->data_corruptions++;
	return(FAILURE);
    }

    if (n == FAILURE) { /* Failure from read above! */
	Eprintf(dip, "api_readfile: %s: read failed, bsize=0x%08X, expectedsize="LL0XFMT"\n",
		f->path, bsize, f->size);
	tip->file_errors++;
	return(FAILURE);
    }
    if (fsize != f->size) {
#if !defined(WIN32)
	if (copyfd != NoFd) (void)dt_close_file(dip, tip->corrupted_file, &copyfd, NULL, EnableErrors, EnableRetries);
	(void)dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
	Eprintf(dip, "api_readfile: %s: unexpected file size encountered, expected="LL0XFMT", actual="LL0XFMT"\n",
		f->path, f->size, fsize);
	return(FAILURE);
#else /* defined(WIN32) */
	if (fsize > f->size) {
	    if (copyfd != NoFd) (void)dt_close_file(dip, tip->corrupted_file, &copyfd, NULL, EnableErrors, EnableRetries);
	    (void)dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
	    Eprintf(dip, "api_readfile: %s: unexpected file size encountered, expected="LL0XFMT", actual="LL0XFMT"\n",
		    f->path, f->size, fsize);
	    return(FAILURE);
	} else { /* Short read! */
	    /*
	     * Assume we weren't told about a failed ENOSPC write because of Write Raw. huh?
	     */
	    Printf(dip, "api_readfile: %s: file size shorter than expected; assuming %s, expected="LL0XFMT", actual="LL0XFMT"\n",
		   f->path, disk_full_str, f->size, fsize);
	    updatesize(dip, f);
	    if (f->size != fsize) {
		if (copyfd != NoFd) (void)dt_close_file(dip, tip->corrupted_file, &copyfd, NULL, EnableErrors, EnableRetries);
		(void)dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
		Eprintf(dip, "api_readfile: %s told us "LL0XFMT", but we read "LL0XFMT"?!\n",
			OS_GET_FILE_ATTR_OP, f->size, fsize);
		return(FAILURE);
	    }
	}
#endif /* defined(WIN32) */
    }

    /* Done reading, now, release the READ lock. */
    if (hmrp->lock_files && lock_full_range) {
	if (unlock_file_chance(dip, hmrp) == False) {
	    /* let's leave this lock here */
	    if (dip->di_lDebugFlag == True) {
		Print(dip, "\n");
		Printf(dip, "%s - UNLCK %s fileid=0x%08X ** randomly decided to skip unlocking the full byte range **",
		       mklogtime(tip), f->path, f->fileid);
	    }
	} else {
	    /* unlock the full byte range */
	    api_lockfile(dip, &fd, f, FLAG_UNLOCK, (Offset_t)0, (Offset_t)f->size);
	}
    }
    if (copyfd != NoFd) (void)dt_close_file(dip, tip->corrupted_file, &copyfd, NULL, EnableErrors, EnableRetries);
    status = dt_close_file(dip, f->fpath, &fd, NULL, EnableErrors, EnableRetries);
    return(status);
}

int
truncatefile(dinfo_t *dip, hammer_file_t *f)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    int64_t newsize;
    double time_taken;
    hbool_t is_disk_full;
    int status;

    /*
     * If file is too small for the truncate test, do nothing.
     */
    if (f->size < 2) {
	return(WARNING);
    }

    newsize = rnd64(dip, 1, f->size-1);
    if (dip->di_dio_flag || dip->di_bufmode_count) {
	newsize = rounddown(newsize, dip->di_dsize);
    }
    Printf(dip, "%s TRUNC   %s fileid=0x%08X oldsize="LL0XFMT" newsize="LL0XFMT,
	   mklogtime(tip), f->path, f->fileid, f->size, newsize);
    start_timer(dip);

    status = dt_truncate_file(dip, f->fpath, newsize, &is_disk_full, EnableErrors);

    time_taken = stop_timer(dip);
    Print(dip, " %gsec", time_taken);
    
    if (status == SUCCESS) {
	if (dip->di_max_data && (newsize < f->size) ) {
	    dip->di_maxdata_written -= (f->size - newsize);
	}
	f->size = newsize;	/* Set the new file size! */
	Printnl(dip);
    } else {
	if (is_disk_full == True) {
	    os_error_t error = os_get_error();
	    Print(dip, " - %s! %s...\n", os_getDiskFullSMsg(error), disk_full_str);
	    updatesize(dip, f);
	    setdiskisfull(hmrp, tip);
	} else { /* truncate failure */
	    tip->file_errors++;
	}
    }
    return(status);
}

int
renamefile(dinfo_t *dip, hammer_file_t *f)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    double time_taken;
    uint64_t n;
    char newpath[PATH_BUFFER_SIZE], *fnewpath;
    hbool_t is_disk_full = False;
    int status;

    /*
     * This should support streams someday. XXX
     * OR, maybe streams support should go away? -Robin
     */
    if (f->colon != NULL || (f->hasdir == True) || findotherstream(dip, tip, f) != NULL) {
	return(WARNING);
    }

    n = newrndfilenum(tip);
    (void)sprintf(newpath, FILENAME_NOSTREAM, n);
    fnewpath = makefullpath(dip, newpath);

    Printf(dip, "%s RENAME  %s fileid=0x%08X newpath=%s", mklogtime(tip), f->path, f->fileid, newpath);
    start_timer(dip);

    /*
     * Remove whatever is at newpath now. It might be a directory.
     */
    if (dt_file_exists(dip, fnewpath) == True) {
	if (removepath(dip, fnewpath) == FAILURE) {
	    freestr(fnewpath);
	    return(FAILURE);
	}
    }

    status = dt_rename_file(dip, f->fpath, fnewpath, &is_disk_full, EnableErrors);

    time_taken = stop_timer(dip);
    Print(dip, " %gsec", time_taken);

    if (status == SUCCESS) {
	strcpy(f->path, newpath);
	freemem(f->fpath);
	f->fpath = fnewpath;
	Printnl(dip);
    } else {
	if (is_disk_full == True) {
	    os_error_t error = os_get_error();
	    Print(dip, " - %s! %s...\n", os_getDiskFullSMsg(error), disk_full_str);
	    setdiskisfull(hmrp, tip);
	} else { /* rename failure */
	    tip->file_errors++;
	}
	freestr(fnewpath);
    }
    return(status);
}

int
deletefile(dinfo_t *dip, hammer_file_t *f, hbool_t cleanup_flag)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    double time_taken;
    int status = SUCCESS;

    /* Note: Why are we reading the file when deleting? */
    /* Answer: Apparently to verify the data, no reads done! */
    if (cleanup_flag == False) {
	status = readfile(dip, f);
	if (status == FAILURE) return(status);
    }

    if ( (hmrp->nostreams == False) && (dt_isdir(dip, f->fpath, DisableErrors) == True) ) {
	Printf(dip, "%s RMDIR   %s fileid=0x%08X", mklogtime(tip), f->path, f->fileid);
    } else {
	Printf(dip, "%s DELETE  %s fileid=0x%08X", mklogtime(tip), f->path, f->fileid);
    }
    start_timer(dip);
    /* Windows streams handling: (Unix too with -streams option!) */
    if (f->colon) {
	if (removepath(dip, f->fpath) == FAILURE) {
	    return(FAILURE);
	}
	time_taken = stop_timer(dip);
	Print(dip, " %gsec\n", time_taken);
	/* Only delete the base if this was the only stream. */
	if (findotherstream(dip, tip, f) == NULL) {
	    char *fpath;
	    *f->colon = 0;
	    fpath = makefullpath(dip, f->path);
	    /* Note: No directory exists if disk full when creating directory! */
	    if (dt_file_exists(dip, fpath) == True) {
		if (dt_isdir(dip, fpath, DisableErrors) == True) {
		    Printf(dip, "%s RMDIR   %s fileid=0x%08X", mklogtime(tip), f->path, f->fileid);
		} else {
		    Printf(dip, "%s DELETE  %s fileid=0x%08X", mklogtime(tip), f->path, f->fileid);
		}
		start_timer(dip);
		/* Note: This removes directories or files! */
		if (removepath(dip, fpath) == FAILURE) {
		    freestr(fpath);
		    return(FAILURE);
		}
		time_taken = stop_timer(dip);
		Print(dip, " %gsec\n", time_taken);
	    }
	    freestr(fpath);
	}
    } else {
	int otherfiles = 0;
	/*
	 * This regular file could have streams attached to it,	so check them before deleting
	 */
	hbool_t hadstreams = False;
	hammer_file_t *other;
	while ((other = findotherstream(dip, tip, f)) != NULL) {
	    otherfiles++;
	    if (hadstreams == False) {
		Print(dip, " checking streams first...\n");
		hadstreams = True;
	    }
	    if (cleanup_flag == False) {
		status = readfile(dip, other);
	    }
#if !defined(WIN32)
	    /*
	     * Deleting the base won't delete the "streams"
	     * on Unix (which we allow just for testing),
	     * so delete it manually.
	     */
	    Printf(dip, "%s DELETE %s fileid=0x%08X", mklogtime(tip), other->path, other->fileid);
	    if (removepath(dip, other->fpath) == FAILURE) {
		return(FAILURE);
	    }
	    Printnl(dip);
#endif /* !defined(WIN32) */
	    freefile(dip, other);
	}
	if (otherfiles) update_dname(dip, f->fpath);
	if (removepath(dip, f->fpath) == FAILURE) {
	    return(FAILURE);
	}
	if (hadstreams == False) {
	    time_taken = stop_timer(dip);
	    Print(dip, " %gsec\n", time_taken);
	} else {
	    Printf(dip, "%s DELETE %s fileid=0x%08X\n", mklogtime(tip), f->path, f->fileid);
	}
    }
    freefile(dip, f);

    return(status);
}

int
removepath(dinfo_t *dip, char *path)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    int disconnects = 0;
    int status;
    char *spath = strrchr(path, dip->di_dir_sep);

    /* Create a short name since full paths are now used. */
    if (spath) {
	spath++;	/* past the separator please. */
    } else {
	spath = path;
    }

    if (hmrp->inode_check && (strchr(path, ':') == NULL) ) {
	os_ino_t inode = inode_lookup(path, NoFd);
	if (inode != (os_ino_t)FAILURE) {
	    Printnl(dip);
	    Printf(dip, "%s INODE REMOVE path=%s inode="LUF"\n",
		   mklogtime(tip), spath, inode);
	    inode_remove(tip, inode);
	}
    }

    /* Note: We only create directories for streams (at present). */
    if ( (hmrp->nostreams == False) && (dt_isdir(dip, path, DisableErrors) == True) ) {
	double time_taken;
	status = remove_directory(dip, path);
	if (dip->di_fDebugFlag) {
	    time_taken = stop_timer(dip);
	    Print(dip, " %gsec\n", time_taken);
	}
	if (status == FAILURE) tip->file_errors++;
	return(status);
    }

    /* Note: Original hammer uses POSIX remove() API. */
    /* FYI: POSIX remove() removes directories or files! */
    status = dt_delete_file(dip, path, EnableErrors);
    if (status == FAILURE) tip->file_errors++;
    return(status);
}

static int
inode_add(dinfo_t *dip, hammer_thread_info_t *tip, os_ino_t ino)
{
    hammer_inode_t *p;

    if (ino <= 0) {
	return(WARNING);
    }
    if (inode_exists(tip, ino) == True) {
	Eprintf(dip, "inode_add: "LUF": duplicate add\n", ino);
	return(FAILURE);
    }
    if ((p = (hammer_inode_t *)Malloc(dip, sizeof(hammer_inode_t))) == 0) {
	Eprintf(dip, "inode_add: out of memory\n");
	return(FAILURE);
    }
    p->ino = ino;
    p->next = tip->inode_hash_table[INODE_HASH(ino)];
    tip->inode_hash_table[INODE_HASH(ino)] = p;
    return(SUCCESS);
}

static void
inode_remove(hammer_thread_info_t *tip, os_ino_t ino)
{
    hammer_inode_t **p;

    if (ino <= (os_ino_t)0) {
	return;
    }
    p = &tip->inode_hash_table[INODE_HASH(ino)];
    while (*p != 0 && (*p)->ino != ino) {
	p = &(*p)->next;
    }
    if (*p) {
	hammer_inode_t *q = *p;
	*p = (*p)->next;
	free(q);
    }
    return;
}

static hbool_t
inode_exists(hammer_thread_info_t *tip, os_ino_t ino)
{
    hammer_inode_t *p;

    if (ino <= (os_ino_t)0) {
	return(False);
    }
    p = tip->inode_hash_table[INODE_HASH(ino)];
    while ( (p != NULL) && (p->ino != ino) ) {
	p = p->next;
    }
    return ( (p != NULL) ? True : False );
}

static os_ino_t
inode_lookup(char *path, HANDLE fd)
{
    return( os_get_fileID(path, fd) );
}

static double
timer(dinfo_t *dip, hbool_t dostart)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    double delta;
    struct timeval now;

    if (gettimeofday(&now, NULL) == FAILURE) {
	os_perror(dip, "timer: gettimeofday");
	return(-1);
    }
    if (dostart == True) {
	tip->start = now;
	return 0.0;
    } else {
	delta = now.tv_sec - tip->start.tv_sec;
	delta += (now.tv_usec - tip->start.tv_usec) / 1000000.0;
	return MAX(delta, 0.000001);
    }
}

char *
mklogtime(hammer_thread_info_t *tip)
{
    time_t	secs;
    struct tm	*tm;
    char	*bp = tip->logtime_buf;

    time(&secs);
    tm = localtime(&secs);
    strftime(bp, sizeof(tip->logtime_buf), "%Y/%m/%d-%H:%M:%S", tm);
    return bp;
}

char *
mktimezone(hammer_thread_info_t *tip)
{
    time_t	secs;
    struct tm	*tm;
    char	*bp = tip->logtime_buf;

    time(&secs);
    tm = localtime(&secs);
    strftime(bp, sizeof(tip->logtime_buf), "%Z", tm);
    return(bp);
}

static __inline void
setfileid(hammer_file_t *f)
{
    f->fileid = rand();
    return;
}

static __inline void
setfiletimestamp(hammer_file_t *f)
{
    f->timestamp = (uint32_t)time(NULL);
    return;
}

static __inline void
setdiskisfull(hammer_parameters_t *hmrp, hammer_thread_info_t *tip)
{
    hmrp->disk_filled = True;
    tip->nfiles_when_full = tip->nfiles;
    return;
}

/*
 * Function    : test_lock_mode
 * Description : tests requested lock mode against randomization
 *               control values, set by lock_mode
 * Input       : [int] FULL_RANGE_LOCK or PARTIAL_RANGE_LOCK
 * Return      : True or False
 */
hbool_t
test_lock_mode(dinfo_t *dip, hammer_parameters_t *hmrp, int lck_mode)
{
    int32_t n;

    n = rnd(dip, 1, 100);
    if ( (n >= hmrp->lock_mode[lck_mode].lower) && (n <= hmrp->lock_mode[lck_mode].upper) ) {
	return True;
    } else {
	return False;
    }
}

/*
 * Function    : unlock_file_chance
 * Description : unlock probability calculation (randomization control)
 *               based on user provided chance of unlocking a file
 * Input       : NONE
 * Return      : True (yes) or False (no)
 */
hbool_t
unlock_file_chance(dinfo_t *dip, hammer_parameters_t *hmrp)
{
    int32_t n;

    if (hmrp->unlock_chance == 0) {
	return False;
    }
    n = rnd(dip, 1, 100);
    if (n <= hmrp->unlock_chance) {
	return True;
    } else {
	return False;
    }
}

/* Temorary, until we switch to OS specific API's! */

#if !defined(WIN32)
/*
 * Method      : api_lockfile
 * Description : use fcntl to lock/unlock files over a specific byte range
 * Inputs      : dip = The device information pointer.
 *		 fd = Pointer to file descriptor (handle)
 *               file = pointer to the 'file' struct
 *               lock_type = FLAG_LOCK_WRITE, FLAG_LOCK_READ, or FLAG_UNLOCK
 *               offset = starting value of byte range to be locked
 *               length = how many bytes to lock
 * Returns     : SUCCESS / FAILURE
 */
int
api_lockfile(dinfo_t *dip, HANDLE *fd, hammer_file_t *f, char lock_type, Offset_t offset, Offset_t length)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    char *lock_type_str, *msg_hdr = "LOCK ";
    int lock_type_flag;
    hbool_t unlock = False;
    hbool_t exclusive = True, immediate = True;
    int status = SUCCESS;

    /* pick lock type flag and display string */
    if (lock_type == FLAG_LOCK_WRITE) {
	lock_type_flag = F_WRLCK;
	lock_type_str = "F_WRLCK";
    } else if (lock_type == FLAG_LOCK_READ) {
	lock_type_flag = F_RDLCK;
	lock_type_str = "F_RDLCK";
    } else if (lock_type == FLAG_UNLOCK) {
	/* check if "-keeplocks" was requested */
	if (unlock_file_chance(dip, hmrp) == False) {
	    return(status);
	}
	unlock = True;
	lock_type_flag = F_UNLCK;
	lock_type_str = "F_UNLCK";
	msg_hdr = "UNLCK";
    } else {
	Printnl(dip);
	Eprintf(dip, "%s - ** Unknown LOCK Type provided, valid values are FLAG_LOCK_WRITE, FLAG_LOCK_READ, FLAG_UNLOCK\n",
		mklogtime(tip));
	return(FAILURE);
    }

    if (dip->di_lDebugFlag == True) {
	Printnl(dip);
	Printf(dip, "%s - %s %s fileid=0x%08X method=fcntl, cmd=F_SETLK, type=%s, whence=SEEK_SET, start="LL0XFMT", len="LL0XFMT"\n",
	       mklogtime(tip), msg_hdr, f->path, f->fileid, lock_type_str, offset, length);
    }
    if (unlock == False) {
	/* Note: exclusive and immediate flags are *not* used on Unix! */
	status = dt_lock_file(dip, f->fpath, fd, offset, length, lock_type_flag, exclusive, immediate, EnableErrors);
    } else {
	status = dt_unlock_file(dip, f->fpath, fd, offset, length, EnableErrors);
    }
    if (status == FAILURE) tip->lock_errors++;
    return(status);
}

#else /* defined(WIN32) */

/*
* Method      : api_lockfile
* Description : use LockFileEx to lock/unlock files over a specific byte range
* Inputs      : fh = file handle
*               path = file path
*               lock_type = type of lock (e.g.- "write", "read", "unlock")
*               offset = starting value of byte range to be locked
*               length = how many bytes to lock
* Returns     : NONE
*/
int
api_lockfile(dinfo_t *dip, HANDLE *fh, hammer_file_t *f, char lock_type, Offset_t offset, Offset_t length)
{
    hammer_information_t *hip = dip->di_opaque;
    hammer_thread_info_t *tip = &hip->hammer_thread_info;
    hammer_parameters_t *hmrp = &hip->hammer_parameters;
    char *lock_type_str, *msg_hdr = "LOCK ";
    int lock_type_flag;
    hbool_t unlock = False;
    hbool_t exclusive = True, immediate = True;
    int status = SUCCESS;

    /* pick lock type flag and display string */
    if (lock_type == FLAG_LOCK_WRITE) {
	lock_type_flag = LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY;
	lock_type_str = "EXCL_LK";
    } else if (lock_type == FLAG_LOCK_READ) {
	lock_type_flag = LOCKFILE_FAIL_IMMEDIATELY;        /* default is "shared" */
	exclusive = False;
	lock_type_str = "SHRD_LK";
    } else if (lock_type == FLAG_UNLOCK) {
	if (unlock_file_chance(dip, hmrp) == False) {
	    return(status);
	}
	unlock = True;
	lock_type_str = "UNLOCK ";
	msg_hdr = "UNLCK";
    } else {
	Printnl(dip);
	Eprintf(dip, "%s - ** Unknown LOCK Type provided: %s, valid values are \"write\', \"read\', \"unlock\"",
		mklogtime(tip), lock_type);
	return(FAILURE);
    }

    if (unlock == True) {
	if (dip->di_lDebugFlag == True) {
	    Printnl(dip);
	    Printf(dip, "%s - %s %s fileid=0x%08X method=UnlockFileEx, type=%s, start="LL0XFMT", len="LL0XFMT"\n",
		   mklogtime(tip), msg_hdr, f->path, f->fileid, lock_type_str, offset, length);
	}
	/* Unlocking the file was requested. */
	status = dt_unlock_file(dip, f->fpath, fh, offset, length, EnableErrors);
    } else {
	if (dip->di_lDebugFlag == True) {
	    Printnl(dip);
	    Printf(dip, "%s - %s %s fileid=0x%08X method=LockFileEx, type=%s, start="LL0XFMT", len="LL0XFMT"\n",
		   mklogtime(tip), msg_hdr, f->path, f->fileid, lock_type_str, offset, length);
	}
	/* Lock the file with the supplied parameters. */
	status = dt_lock_file(dip, f->fpath, fh, offset, length, lock_type_flag, exclusive, immediate, EnableErrors);
    }

    if (status == FAILURE) tip->lock_errors++;
    return(status);
}
#endif /* !defined(WIN32) */
