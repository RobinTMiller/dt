#if !defined(AIX_WORKAROUND_H)

#define AIX_WORKAROUND_H	1

#if defined(AIX)
/*
 * These should come from sys/types.h, but I can't seem to find the
 * magic compile options to get these defined properly, so cludge it!
 *
 * Failure to define these results in numerous errors here using cc_r:
 * /usr/include/pthread.h:666: error: parse error before '*' token
 *
 * FWIW: I'm fairly certain the wrong include files are getting picked up!
 * A Google search shows many folks encountering this, and it appears to be
 * caused by misconfigured GCC include files. I'm no sure of correct fix.
 *
 * Note: This is only occurring on AIX 5.3 compiles!
 */
typedef struct {
#ifdef __64BIT__
        long    __sp_word[3];
#else
        int     __sp_word[6];
#endif /* __64BIT__ */
} pthread_spinlock_t;
typedef struct {
#ifdef __64BIT__
        long    __br_word[5];
#else
        int     __br_word[8];
#endif /* __64BIT__ */
} pthread_barrier_t;
typedef void *pthread_barrierattr_t;

#endif /* defined(AIX) */

#endif /* !defined(AIX_WORKAROUND_H) */
