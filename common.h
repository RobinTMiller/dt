/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2017			    *
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
  * Moficiation History:
  *
  * May 30th, 2014 by Robin T. Miller
  *	Modify FUF to LDF for file offsets, since most OS's the offset is signed.
  */
#if !defined(_COMMON_H_)
#define _COMMON_H_ 1

#if (defined(__unix__) || defined(_AIX) || defined(MacDarwin)) && !defined(__unix)
# define __unix
#endif

#if !defined(HAVE_UUID)
/* Note: FreeBSD has UUID API's, but different than Linux and Solaris! */
# if defined(__linux) || defined(__sun) || defined(_WIN32) || defined(MacDarwin) || defined(FreeBSD)
#  define HAVE_UUID 1
# else
#  define HAVE_UUID 0
# endif
#endif /* !defined(HAVE_UUID) */

#if defined(WIN32)
# include <windows.h>
#endif

/* Various defines used by different OS's to enable 64-bit executables and native 64-bit long values. */
#if defined(__alpha) || defined(__64BIT__) || defined(_LP64) || defined(__LP64__) || defined(__arch64__) || defined(_WIN64)
#  define QuadIsLong
#  define MACHINE_64BITS
#else /* assume !64bit compile! */
#  define QuadIsLongLong
#endif /* 64bit check */

#if defined(MacDarwin)
/*
 * Ot appears my dated Mac Server prefers 32-bit definitions, even though
 * my OS and system is 64-bits. Otherwise too many of the printf() control
 * strings complains with: (sigh)
 * warning: format '%lu' expects type 'long unsigned int', but argument 5 has type 'uint64_t'
 * 
 * Here's what "man 3 printf" says: (Note: No mention of "l' printing 64-bits!)
 *
 *  Modifier          d, i           o, u, x, X            n
 *  hh                signed char    unsigned char         signed char *
 *  h                 short          unsigned short        short *
 *  l (ell)           long           unsigned long         long *
 *  ll (ell ell)      long long      unsigned long long    long long *
 *  j                 intmax_t       uintmax_t             intmax_t *
 *  t                 ptrdiff_t      (see note)            ptrdiff_t *
 *  z                 (see note)     size_t                (see note)
 *  q (deprecated)    quad_t         u_quad_t              quad_t *
 * 
 * Note; I'm from the DEC Alpha world, which is a true 64-bit environment!
 */
# undef QuadIsLong
# undef MACHINE_64BITS
# define QuadIsLongLong
#endif /* defined(MacDarwin) */

#define SECS_PER_MIN	60
#define MINS_PER_HOUR	60
#define HOURS_PER_DAY	24
#define SECS_PER_HOUR	(SECS_PER_MIN * MINS_PER_HOUR)
#define SECS_PER_DAY	(SECS_PER_HOUR * HOURS_PER_DAY)

#define MSECS           1000
#define MSECS_PER_HOUR	(SECS_PER_HOUR * MSECS)
#define MSECS_PER_DAY	(SECS_PER_DAY * MSECS)
#define MSECS_PER_MIN   (SECS_PER_MIN * MSECS)

#define MSECS_PER_SEC   MSECS
#define mSECS_PER_SEC	1000
#define uSECS_PERmSEC	1000
#define uSECS_PER_SEC	1000000

/* Note: This should be defined in <stdint.h> but we build on older OS's yet! */
#include <inttypes.h>
#if !defined(INT8_MAX)
#  define INT8_MAX        (127)
#  define INT16_MAX       (32767)
#  define INT32_MAX       (2147483647)
#  define UINT8_MAX       (255U)
#  define UINT16_MAX      (65535U)
#  define UINT32_MAX      (4294967295U)
/* Note: 64-bit definitions are below. */
#endif /* !defined(INT8_MAX) */

#undef INFINITY
#if defined(MACHINE_64BITS)
#  define MAX_SEEK	0x8000000000000000UL /* Maximum lseek() value.	*/
#  define MAX_LONG	0x7fffffffffffffffL  /* Maximum positive long.	*/
#  define MAX_ULONG	0xffffffffffffffffUL /* Maximum u_long value.	*/
# if !defined(INT64_MAX)
#  define INT64_MAX       (9223372036854775807L)
#  define UINT64_MAX      (18446744073709551615UL)
# endif /* !defined(INT64_MAX) */
#  define INFINITY	MAX_ULONG	/* Maximum possible long value.	*/
#  define TBYTE_SIZE	1099511627776L		/* Terabytes value.	*/
#else /* !defined(MACHINE_64BITS */
#  define MAX_LONG	0x7fffffffL	/* Maximum positive long value.	*/
#  define MAX_ULONG	0xffffffffUL	/* Maximum unsigned long value.	*/
# if !defined(INT64_MAX)
#  define INT64_MAX       (9223372036854775807LL)
#  define UINT64_MAX      (18446744073709551615ULL)
# endif /* !defined(INT64_MAX) */
#  if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)
#    define MAX_SEEK	0x8000000000000000ULL /* Maximum lseek() value. */
#  else /* if !defined(_FILE_OFFSET_BITS) || (_FILE_OFFSET_BITS != 64) */
#    define MAX_SEEK	0x80000000UL	/* Maximum lseek() value.	*/
#  endif /* defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64) */
#  if defined(QuadIsLongLong)
#    if defined(WIN32)
#      define MAX_ULONG_LONG  ~((ULONGLONG) 0)
#      define INFINITY  MAX_ULONG_LONG
#      define TBYTE_SIZE (large_t)1095511627776
#    else 
#      define MAX_LONG_LONG 0x7fffffffffffffffULL
#      define MAX_ULONG_LONG 0xffffffffffffffffULL
#      define INFINITY	 MAX_ULONG_LONG	/* Maximum possible large value */
#      define TBYTE_SIZE 1099511627776LL	/* Terabytes value.	*/
#    endif /* defined(WIN32) */
#  else /* assume QuadIsDouble */
#    if defined(__WIN32__)
       /* HUGE_VAL seems to be defined as 0.0, which is NFG! */
#      define INFINITY  (double)0x10000000000000L /* Max large value???	*/
#    else
#      define INFINITY	HUGE_VAL	/* Maximum possible large value */
#    endif
#    define TBYTE_SIZE	((double)GBYTE_SIZE*1024) /* Terabytes value.	*/
#  endif /* defined(QuadIsLongLong) */
#endif /* defined(MACHINE_64BITS) */

/*
 * Large value for 32 or 64 bit systems.
 */
#if defined(MACHINE_64BITS)
/*
 * 64-bit Definitions:
 */
# if defined(_WIN64)

#define QuadIsLong

typedef ULONG64 		 large_t;
typedef LONG64 	        	 slarge_t;
typedef volatile large_t         v_large;
typedef volatile slarge_t 	 v_slarge; 
# define LUF		"%I64u"
# define LDF		"%I64d"
# define LXF		"0x%I64x"
# define FUF		LUF 
# define FXF		LXF 

/* Note: Formats for leading zeros, half or full 64-bits. */
# define LLHXFMT	"%08I64X"
# define LLFXFMT	"%016I64X"

/* Note: Replacement for "%p" which differs between OS's, esp. Windows! */
# define LLPXFMT "0x%I64x"
# define LLPX0FMT "0x%016I64x"

# else /* !defined(_WIN64), so Unix! */

/* 64-bit Unix Definitions: */

#define QuadIsLong

typedef unsigned long           large_t;
typedef signed long             slarge_t;
typedef volatile large_t        v_large;
typedef volatile slarge_t       v_slarge;

/* Note: We assume a long 'l' format displays 64-bits! */

# define LUF		"%lu"
# define LDF		"%ld"
# define LXF		"%#lx"
# define LLHXFMT	"%08lX"
# define LLFXFMT	"%016lX"

# define LLPXFMT	"0x%lx"
# define LLPX0FMT	"0x%016lx"

/* For displaying file offsets. */
# define FUF     LDF
# define FXF     LXF

# endif /* defined(_WIN64) */
/*
 * 32-bit Definitions: (yea, messy... historic, needs cleaned up!)
 */
# elif defined(WIN32)

# define QuadIsLongLong

typedef ULONGLONG		 large_t;
typedef LONGLONG 		 slarge_t;
typedef volatile large_t         v_large;
typedef volatile slarge_t 	 v_slarge; 

# define LUF		"%I64u"
# define LDF		"%I64d"
# define LXF		"0x%I64x"
# define FUF		LUF 
# define FXF		LXF 
# define LLHXFMT	"%08I64X"
# define LLFXFMT	"%016I64X"

# define LLPXFMT	"0x%lx"
# define LLPX0FMT	"0x%08lx"

# else /* assume *nix 32-bit! */

/* 32-bit Unix Definitions: */

# define QuadIsLongLong

typedef unsigned long long int	large_t;
typedef signed long long int	slarge_t;
typedef volatile large_t	v_large;
typedef volatile slarge_t	v_slarge;

/* Note: This can probably be cleaned up, mostly (hopefully) historic! */
#  if defined(SCO)
#    define LUF		"%Lu"
#    define LDF		"%Ld"
#    define LXF		"%#Lx"
#    define LLHXFMT	"%08Lx"
#    define LLFXFMT	"%016Lx"
#  elif defined(Unixware)	/* is this right? can't remember which OS required this! */
#    define LUF		"%qu"
#    define LDF		"%qd"
#    define LXF		"%#qx"
#    define LLHXFMT	"%08qx"
#    define LLFXFMT	"%016qx"
#  else /* all other POSIX compliant Unix OS's! */
#    define LUF		"%llu"
#    define LDF		"%lld"
#    define LXF		"%#llx"
#    define LLHXFMT	"%08llX"
#    define LLFXFMT	"%016llX"
#  endif /* if defined(SCO) */

# define LLPXFMT	"0x%x"
# define LLPX0FMT	"0x%08x"

#  if ( defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64) ) || defined(_LARGEFILE64_SOURCE)
#    define FUF	LDF
#    define FXF	LXF
#  else /* !defined(_FILE_OFFSET_BITS) || (_FILE_OFFSET_BITS != 64) */
#    define FUF	"%ld"
#    define FXF	"%#lx"
#  endif /* defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64) */

#endif /* defined(MACHINE_64BITS) */

/*
 * Time Format:
 */ 
#if defined(WIN32)
# if defined(_USE_32BIT_TIME_T)
#  define TMF	"%d"
# else /* !defined(_USE_32BIT_TIME_T) */
#  define TMF	"%I64d"
# endif
#else
# if defined(MACHINE_64BITS)
#  define TMF	LDF
# else /* !defined(MACHINE_64BITS) */
#  define TMF	"%d"
# endif
#endif /* defined(WIN32) */

/*
 * Definitions for displaying size_t and ssize_t values.
 * Note: Each OS defines size_t differently! (sigh)
 */
#if defined(QuadIsLong)
/* Assume size_t is 64 bits. */
# define SDF	"%ld"
# define SUF	"%lu"
# define SXF	"%lx"
typedef long int mysize_t;
#else /* !defined(QuadIsLong) */
/* Assume size_t is 32 bits. */
# define SDF	"%d"
# define SUF	"%u"
# define SXF	"%x"
typedef int mysize_t;
#endif /* defined(QuadIsLong) */

/*
 * Create some shorthands for volatile storage classes:
 */
typedef	volatile char	v_char;
typedef	volatile short	v_short;
typedef	volatile int	v_int;
typedef volatile long	v_long;
#if !defined(DEC)
/* These are defined in sys/types.h on DEC Alpha systems. */
typedef	volatile unsigned char	vu_char;
typedef	volatile unsigned short	vu_short;
typedef	volatile unsigned int	vu_int;
typedef volatile unsigned long	vu_long;
#endif /* !defined(__alpha) */

/* Newer *nix support these, Windows does not! */
#if defined(_WIN32)
typedef signed char             int8_t;
typedef short int               int16_t;
typedef signed int              int32_t;
typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;
#  if defined(_WIN64)
/* Note: Unlike most Unix OS's, 64-bit Windows does *not* make a long 64-bits! */
//typedef long int                int64_t;
//typedef unsigned long int       uint64_t;
typedef LONG64			int64_t;
typedef ULONG64			uint64_t;
#  else /* !defined(_WIN64) */
typedef __int64			int64_t;
typedef unsigned __int64        uint64_t;
#  endif
#endif /* defined(_WIN32) */

/*
 * Define some Architecture dependent types:
 */
#if defined(__alpha) || defined(__LP64__) || defined(_WIN64)
# if defined(_WIN64)
typedef ULONG64			ptr_t;
# else /* defined(_WIN64) */
typedef unsigned long		ptr_t;
# endif /* defined(_WIN64) */
typedef int			int32;
typedef unsigned int		u_int32;
#if !defined(bool)
typedef unsigned int		bool;
#endif /* !defined(bool) */
typedef volatile unsigned int	v_bool;

#else /* !defined(__alpha) && !defined(__LP64__) && !defined(_WIN64) */

typedef unsigned int		ptr_t;
# if !defined(AIX)
#  if !defined(OSFMK)
typedef int			int32;
#  endif /* !defined(OSFMK) */
typedef unsigned int		u_int32;
# endif /* !defined(AIX) */
#if !defined(bool)
typedef unsigned char		bool;
#endif /* !defined(bool) */
typedef volatile unsigned char	v_bool;

#endif /* defined(__alpha) || defined(__LP64__) || defined(_WIN64) */

/*
 * Some systems don't have these definitions:
 */
#if defined(WIN32)

typedef	unsigned char		u_char;
typedef	unsigned short		u_short;
typedef	unsigned int		u_int;
typedef unsigned long		u_long;
typedef unsigned long		daddr_t;

#endif /* defined(WIN32) */

/* TODO: Cleanup! */
#if 0
#if defined(sun)

# if !defined(SOLARIS) && !defined(AIX)
typedef int			ssize_t;
# endif /* !defined(SOLARIS) && !defined(AIX) */

#endif /* defined(sun) */
#endif /* 0 */

#define ulong64		large_t
#define long64		slarge_t

/*
 * For IOT block numbers, use this definition.
 * TODO: Extend this to 64-bits in the future!
 */ 
typedef unsigned int iotlba_t;

#define SUCCESS		0			/* Success status code. */
#define FAILURE		-1			/* Failure status code. */
#define WARNING		1			/* Warning status code.	*/
#define PARSE_MATCH	SUCCESS			/* Parsing macthed.	*/
#define PARSE_NOMATCH	WARNING			/* No parsing match.	*/
#define STOP_PARSING	2			/* Stop parsing options */
#define RETRYABLE	-2			/* Retryable error.	*/
#define TRUE		1			/* Boolean TRUE value.	*/
#define FALSE		0			/* Boolean FALSE value.	*/
#define UNINITIALIZED	255			/* Uninitialized flag.	*/
#define NO_LBA		0xFFFFFFFFFFFFFFFFULL	/* No LBA vlaue.	*/

#define BLOCK_SIZE		512		/* Bytes in block.	*/
#define KBYTE_SIZE		1024		/* Kilobytes value.	*/
#define MBYTE_SIZE		1048576L	/* Megabytes value.	*/
#define GBYTE_SIZE		1073741824L	/* Gigabytes value.	*/

#define ANY_RADIX	0		/* Any radix string conversion.	*/
#define DEC_RADIX	10		/* Base for decimal conversion.	*/
#define HEX_RADIX	16		/* Base for hex str conversion.	*/

/*
 * These buffer sizes are mainly for allocating stack space
 * for pre-formatting strings to display.  Different sizes
 * to prevent wasting stack space (my preference).
 */
#define SMALL_BUFFER_SIZE       32              /* Small buffer size.   */
#define MEDIUM_BUFFER_SIZE      64              /* Medium buffer size.  */
#define LARGE_BUFFER_SIZE       128             /* Large buffer size.   */
/* Note: This string buffer size gets redefined in dt.h for long paths! */
#define STRING_BUFFER_SIZE	256		/* String buffer size.	*/
#define TIME_BUFFER_SIZE	32		/* ctime() buffer size.	*/

#define ARGS_BUFFER_SIZE	32768		/* Input buffer size.	*/
#define ARGV_BUFFER_SIZE	4096		/* Maximum arguments.	*/

/*
 * Note: This should be the largest file path allowed for any Host OS.
 *	 On Linux, "errno = 36 - File name too long" occurs at 4096.
 *	 This will usually be used for short term stack allocated space.
 */
#define PATH_BUFFER_SIZE	8192		/* Max path size.	*/

#define TRUE		1			/* Boolean TRUE value.	*/
#define FALSE		0			/* Boolean FALSE value.	*/

/*
 * Declare a boolean data type.
 *
 * Note: Prefixed with 'h', to avoid redefinitions with OS includes.
 * [ Most (not all) OS's have a bool_t or boolean_t (rats!). ]
 */
typedef enum hbool {
    False = 0,
    True  = 1
} hbool_t;

typedef volatile hbool_t vbool_t;

typedef enum open_mode {
    OPEN_FOR_READING = 0,
    OPEN_FOR_WRITING = 1
} open_mode_t;

/*
 * Shorthand macros for string compare functions:
 */
#define EQ(x,y)		(strcmp(x,y)==0)	/* String EQ compare.	*/
#define EQL(x,y,n)	(strncmp(x,y,n)==0)	/* Compare w/length.	*/
#define EQC(x,y)        (strcasecmp(x,y) == 0)  /* Case insensitive.    */
#define EQLC(x,y,n)     (strncasecmp(x,y,n) == 0) /* Compare w/length.  */

#define NE(x,y)		(strcmp(x,y)!=0)	/* String NE compare.	*/
#define NEL(x,y,n)	(strncmp(x,y,n)!=0)	/* Compare w/length.	*/
#define NEC(x,y)        (strcasecmp(x,y) != 0)  /* Case insensitive.    */
#define NELC(x,y,n)     (strncasecmp(x,y,n) != 0) /* Compare w/length.  */

#define EQS(x,y)	(strstr(x,y) != NULL)	/* Sub-string equal.	*/
#define EQSC(x,y)	(strcasestr(x,y) != NULL) /* Case insensitive.	*/

#define NES(x,y)	(strstr(x,y) == NULL)	/* Sub-string not equal	*/
#define NESC(x,y)	(strcasestr(x,y) == NULL) /* Case insensitive.	*/

#define POSIX_DIRSEP   '/'

/*
 * Alternate Device Directory:
 */
#if defined(WIN32)
#  define ADEV_PREFIX	"//./"		/* Windows hidden device dir.	*/
#  define ADEV_LEN	4		/* Length of device name prefix.*/
#elif defined(_NT_SOURCE)
#  define ADEV_PREFIX	"//./"		/* Windows hidden device dir.	*/
#  define ADEV_LEN	4		/* Length of device name prefix.*/
#  define NDEV_PREFIX	"\\\\.\\"	/* Native Windows device dir.	*/
#  define NDEV_LEN	4		/* That's for "\\.\" prefix.	*/
#else /* !defined(_NT_SOURCE) */
#  define ADEV_PREFIX	"/devices/"	/* Physical device name prefix.	*/
#  define ADEV_LEN	9		/* Length of device name prefix.*/
#  define NDEV_PREFIX	"/dev/"		/* Native device prefix (dup).	*/
#  define NDEV_LEN	5		/* Native device prefix length.	*/
#endif /* defined(_NT_SOURCE) */

#define CONSOLE_NAME	"console"	/* The console device name.	*/
#define CONSOLE_LEN	7		/* Length of console name.	*/

#if defined(_QNX_SOURCE)
#  define CDROM_NAME	"cd"		/* Start of CD-ROM device name.	*/
#  define RCDROM_NAME	"cd"		/* Start of raw CD-ROM names.	*/
#elif defined(sun) || defined(__linux__)
/* Note: Don't know about Sun systems. */
#  define CDROM_NAME	"cd"		/* Start of CD-ROM device name.	*/
#  define RCDROM_NAME	"scd"		/* Start of raw CD-ROM names.	*/
/* NOTE: Linux (RedHat V6.0) currently has no raw CD-ROM names! */
#elif defined(SCO)
#  define CDROM_NAME	"cdrom"		/* Start of CD-ROM device name.	*/
#  define RCDROM_NAME	"rcdrom"	/* Start of raw CD-ROM names.	*/
#elif defined(_NT_SOURCE)
#  define CDROM_NAME	"Cdrom"		/* Start of CD-ROM device name.	*/
#  define RCDROM_NAME	"cdrom"		/* Allow both upper/lowercase.	*/
#else
#  define CDROM_NAME	"rz"		/* Start of CD-ROM device name.	*/
#  define RCDROM_NAME	"rrz"		/* Start of raw CD-ROM names.	*/
#endif /* !defined(_QNX_SOURCE) */

#if defined(AIX)
#  define DISK_NAME	"hd"		/* Start of disk device names.	*/
#  define RDISK_NAME	"rhd"		/* Start of raw disk names.	*/
#elif defined(_QNX_SOURCE)
#  define DISK_NAME	"hd"		/* Start of disk device names.	*/
#  define RDISK_NAME	"hd"		/* Start of raw disk names.	*/
#elif defined(__linux__)
#  define DISK_NAME	"sd"		/* Start of disk device names.	*/
#  define RDISK_NAME	"raw"		/* Start of raw disk names.	*/
#  define ADISK_NAME	"dm"		/* Multi-path device name.	*/
#  define ARDISK_NAME	"mapper"	/* Long MPIO devicei name.	*/
#elif defined(sun)
#  define DISK_NAME	"sd"		/* Start of disk device names.	*/
#  define RDISK_NAME	"rsd"		/* Start of raw disk names.	*/
#  define ADISK_NAME	"dsk"		/* Start of disk device names.	*/
#  define ARDISK_NAME	"rdsk"		/* Start of raw disk names.	*/
/* NOTE: Linux (RedHat V6.0) currently has no raw disk names! */
#elif defined(SCO) || defined(HP_UX)
#  define DISK_NAME	"dsk"		/* Start of disk device names.	*/
#  define RDISK_NAME	"rdsk"		/* Start of raw disk names.	*/
#elif defined(_NT_SOURCE) || defined(WIN32)
/* Note: Windows device names are NOT case sensitive like *nix systems! */
#  define DISK_NAME	"PhysicalDrive"	/* Start of disk device names.	*/
#  define RDISK_NAME	"physicaldrive"	/* Allow both upper/lowercase.	*/
/* Note: Windows lacks an strcasestr() API, so this is just a bandaide! */
#  define ADISK_NAME	"PHYSICALDRIVE"	/* Start of disk device names.	*/
#  define ARDISK_NAME	ADISK_NAME
#else
#  define DISK_NAME	"rz"		/* Start of disk device names.	*/
#  define RDISK_NAME	"rrz"		/* Start of raw disk names.	*/
#endif /* !defined(_QNX_SOURCE) */

#if defined(_QNX_SOURCE)
#  define TTY_NAME	"ser"		/* Start of terminal names.	*/
#else /* !defined(_QNX_SOURCE) */
#  define TTY_NAME	"tty"		/* Start of terminal names.	*/
#endif /* defined(_QNX_SOURCE) */
#define TTY_LEN		3		/* Length of terminal name.	*/

#if defined(_QNX_SOURCE)
#  define TAPE_NAME	"tpr"		/* Start of tape device names.	*/
#  define NTAPE_NAME	"tp"		/* No-rewind tape device name.	*/
#elif defined(sun)
#  define TAPE_NAME	"rst"		/* Start of tape device names.	*/
#  define NTAPE_NAME	"nrst"		/* No-rewind tape device name.	*/
#elif defined(__linux__)
#  define TAPE_NAME	"st"		/* Start of tape device names.	*/
#  define NTAPE_NAME	"nst"		/* No-rewind tape device name.	*/
#elif defined(SCO)
#  define TAPE_NAME	"ctape"		/* Start of tape device names.	*/
#  define NTAPE_NAME	"ntape"		/* No-rewind tape device name.	*/
#elif defined(_NT_SOURCE)
#  define TAPE_NAME	"rmt"		/* Start of tape device names.	*/
#  define NTAPE_NAME	"nrmt"		/* No-rewind tape device name.	*/
#else
#  define TAPE_NAME	"rmt"		/* Start of tape device names.	*/
#  define NTAPE_NAME	"nrmt"		/* No-rewind tape device name.	*/
#endif /* !defined(_QNX_SOURCE) */

/*********************************** macros **********************************/

/* Historic  or BSD? */
#if !defined(MIN)
#  define MIN(a,b) (((a)<(b))?(a):(b))
#endif /* !defined(MIN) */
#if !defined(MAX)
#  define MAX(a,b) (((a)>(b))?(a):(b))
#endif /* if !defined(MAX) */

/*
 * Macro to calculate the starting block of where error occured.
 *    ( 'x' = byte count or file offset, 'y' = block size )
 */
#define WhichBlock(x,y)		((x)/(y))

#ifndef max
# define max(a,b)       ((a) > (b)? (a): (b))
#endif
#ifndef min
# define min(a,b)       ((a) < (b)? (a): (b))
#endif
#ifndef howmany
# define howmany(x, y)  (((x)+((y)-1))/(y))
#endif
#ifndef roundup
# define roundup(x, y)  ((((x)+((y)-1))/(y))*(y))
#endif
#ifndef rounddown
# define rounddown(x,y) (((x)/(y))*(y))
#endif
#ifndef ispowerof2
# define ispowerof2(x)  ((((x)-1)&(x))==0)
#endif

#if !defined(offsetof)
# define offsetof(type, field)   ((long) &((type *)0)->field)
#endif /* !defined(offsetof) */

/*
 * This Big-Endian vs. Little-Endian byte ordering gives me a headache!
 * SCSI and network ordering is Big-Endian, so values are stored left to right,
 * while Little-Endian values are stored right to left, important for data exchange!
 */
/* Big-Endian Swap Macros: */
#define BigSwap16(p) \
	( ((uint16_t)p[0] << 8) | (uint16_t)p[1] );
#define BigSwap32(p) \
	( ((uin32_t)p[0] << 24) | ((uin32_t)p[1] << 16) | \
	  ((uin32_t)p[2] << 8) | (uin32_t)p[3]);

#define BigSwap64(p) \
	( ((uin64_t)p[0] << 56ULL) | ((uin64_t)p[1] << 48ULL) | \
	  ((uin64_t)p[2] << 40ULL) | ((uin64_t)p[3] << 32ULL) | \
	  ((uin64_t)p[4] << 24) | ((uin64_t)p[5] << 16) |	\
	  ((uin64_t)p[6] << 8) | (uin64_t)p[7] );

/* Byte Swap Macros: */
#define ByteSwap16(v) \
     ((uint16_t) ((((v) >> 8) & 0xFF) | (((v) & 0xFF) << 8)))
#define ByteSwap32(v) \
     ((((v) & 0xFF000000) >> 24) | (((v) & 0x00FF0000) >>  8) | 	\
      (((v) & 0x0000FF00) <<  8) | (((v) & 0x000000FF) << 24))
#define ByteSwap64(v) \
     (	(((v) & 0xFF00000000000000ULL) >> 56) | 			\
	(((v) & 0x00FF000000000000ULL) >> 40) | 			\
	(((v) & 0x0000FF0000000000ULL) >> 24) | 			\
	(((v) & 0x000000FF00000000ULL) >> 8)  | 			\
	(((v) & 0x00000000FF000000ULL) << 8)  | 			\
	(((v) & 0x0000000000FF0000ULL) << 24) | 			\
	(((v) & 0x000000000000FF00ULL) << 40) | 			\
	(((v) & 0x00000000000000FFULL) << 56))


/* Byte Ordering Conversion Macros: */
#if _BIG_ENDIAN_
# define LtoH16(value)		ByteSwap16(value)
# define LtoH32(value)		ByteSwap32(value)
# define LtoH64(value)		ByteSwap64(value)
# define HtoL16(value)		ByteSwap16(value)
# define HtoL32(value)		ByteSwap32(value)
# define HtoL64(value)		ByteSwap64(value)
#else /* _LITTLE_ENDIAN_ */
# define LtoH16(value)		value
# define LtoH32(value)		value
# define LtoH64(value)		value
# define HtoL16(value)		value
# define HtoL32(value)		value
# define HtoL64(value)		value
#endif /* _BIG_ENDIAN_ */

#define StoH(fptr)		stoh(fptr, sizeof(fptr))
#define StoHL(fptr,size)	stoh((uint8_t *)fptr, size)
#define HtoS(fptr,val)		htos(fptr, (large_t)val, sizeof(fptr))
#define HtoSL(fptr,val,size)	htos((uint8_t *)fptr, (large_t)val, size)

extern large_t stoh(unsigned char *bp, size_t size);
extern void htos(unsigned char *bp, large_t value, size_t size);
extern int GetCdbLength(unsigned char opcode);

#if defined(_WIN32)
# include "dtwin.h"
#else /* !defined(_WIN32) */
# include "dtunix.h"
#endif /* defined(_WIN32) */

#endif /* !defined(_COMMON_H_) */
