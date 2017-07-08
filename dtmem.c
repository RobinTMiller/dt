/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2013			    *
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
 * Module:	dtmem.c
 * Author:	Robin T. Miller
 * Date:	August 17th, 2013
 *
 * Description:
 *	Memory allocation functions.
 *
 * Modification History:
 *
 * August 17th, 2013 by Robin T Miller
 * 	Moving memory allocation functions here.
 */
#include "dt.h"

void
report_nomem(dinfo_t *dip, size_t bytes)
{
    Eprintf(dip, "Failed to allocated %u bytes!\n", bytes);
    return;
}

#if !defined(INLINE_FUNCS)

/*
 * Note: To trace more memory issues, we need our own Strdup() as well!
 */
void
Free(dinfo_t *dip, void *ptr)
{
    if (mDebugFlag) {
	Printf(dip, "Free: Deallocating buffer at address "LLPXFMT"...\n", ptr);
    }
    free(ptr);
    return;
}

void
FreeMem(dinfo_t *dip, void *ptr, size_t size)
{
    if (mDebugFlag) {
	Printf(dip, "Free: Deallocating buffer at address "LLPXFMT", "SUF" bytes...\n",
	       ptr, size);
    }
    memset(ptr, 0xdd, size);
    free(ptr);
    return;
}

void
FreeStr(dinfo_t *dip, void *ptr)
{
    FreeMem(dip, ptr, (strlen(ptr) + 1));
    return;
}

/*
 * Malloc() - Allocate memory with appropriate error checking.
 *
 * Description:
 *      This function allocates the requested memory, performs the
 * necessary error checking/reporting, and then zeros the allocated
 * memory space.
 *
 * Inputs:
 *	bytes = The number of bytes to allocate.
 * 
 * Return Value:
 *	Returns pointer to buffer allocated, NULL on failure.
 *	Note: We terminate on memory failures, until callers handle!
 */
void *
Malloc(dinfo_t *dip, size_t bytes)
{
    void *bp;

    if (!bytes) {
        LogMsg(dip, efp, logLevelDiag, 0,
               "Malloc: FIXME -> Trying to allocate %u bytes.\n", bytes);
	return(NULL);
    }
    if ( (bp = malloc(bytes)) == NULL) {
        Perror(dip, "malloc() failed allocating %u bytes.\n", bytes);
	terminate(dip, FAILURE);
	//return(NULL);
    } else if (mDebugFlag) {
	Printf(dip, "Malloc: Allocated buffer at address "LLPXFMT" of %u bytes, end "LLPXFMT"...\n",
	       bp, bytes, ((char *)bp + bytes) );
    }
    memset(bp, '\0', bytes);
    return(bp);
}

void *
Realloc(dinfo_t *dip, void *bp, size_t bytes)
{
    if ( (bp = realloc(bp, bytes)) == NULL) {
        Perror(dip, "realloc() failed allocating %u bytes.\n", bytes);
	terminate(dip, FAILURE);
	//return(NULL);
    } else if (mDebugFlag) {
	Printf(dip, "Realloc: Allocated buffer at address "LLPXFMT" of %u bytes...\n", bp, bytes);
    }
    memset(bp, '\0', bytes);
    return(bp);
}

#endif /* defined(INLINE_FUNCS) */

/* ========================================================================= */

//#define NEED_PAGESIZE	1	/* Define to obtain page size. */

/*
 * This structure is used by the page align malloc/free support code. These
 * "working sets" will contain the malloc-ed address and the page aligned
 * address for the free*() call.
 */
typedef struct mpa_ws {
    struct mpa_ws *next;
    void	*palign_addr;
    void	*malloc_addr;
    size_t	malloc_size;
} MPA_WS;

/* Initialized and uninitialized data. */

static pthread_mutex_t malloc_mutex;
static hbool_t malloc_mutex_inited = False;

static MPA_WS mpa_qhead;	/* local Q head for the malloc stuctures */

/*
 * This is a local allocation routine to alloc and return to the caller a
 * system page aligned buffer.  Enough space will be added, one more page, to
 * allow the pointers to be adjusted to the next page boundry.  A local linked
 * list will keep copies of the original and adjusted addresses.  This list 
 * will be used by free_palign() to free the correct buffer.
 *
 * Inputs:
 * 	bytes = The number of bytes to allocate.
 * 	offset = The offset to misalign from page aligned memory.
 *
 * Return Value:
 *	Returns page aligned buffer or NULL if no memory available.
 */
void *
malloc_palign(dinfo_t *dip, size_t bytes, int offset)
{
    int error;
    MPA_WS *ws;		/* pointer for the working set */
#if defined(NEED_PAGESIZE)
    int page_size;	/* for holding the system's page size */
#endif /* defined(NEED_PAGESIZE) */

    if (bytes == (size_t)0) {
        LogMsg(dip, efp, logLevelDiag, 0,
               "malloc_palign: FIXME -> Trying to allocate %u bytes.\n", bytes);
	return(NULL);
    }
    if ( malloc_mutex_inited == False ) {
	error = pthread_mutex_init(&malloc_mutex, NULL);
	if ( error ) {
	    tPerror(dip, error, "pthread_mutex_init() failed!");
	}
	malloc_mutex_inited = True;
    }
    /*
     * The space for the working set structure that will go on the queue
     * is allocated first.
     */
    ws = (MPA_WS *)Malloc(dip, sizeof(MPA_WS) );
    if ( ws == (MPA_WS *)NULL ) {
	return( NULL );
    }

#if defined(NEED_PAGESIZE)
    page_size = getpagesize();
#endif /* defined(NEED_PAGESIZE) */

    ws->malloc_size = (bytes + page_size + offset);
    /*
     * Using the requested size, from the argument list, and the page size
     * from the system allocate enough space to page align the requested 
     * buffer.  The original request will have the space of one system page
     * added to it.  The pointer will be adjusted.
     */
    ws->malloc_addr = Malloc(dip, ws->malloc_size);
    if ( ws->malloc_addr == NULL ) {
	Free(dip, ws);
	return( NULL );
    } else {
	; // Malloc() does this: (void)memset(ws->malloc_addr, 0, ws->malloc_size);
    }

    error = pthread_mutex_lock(&malloc_mutex);
    if ( error ) {
	tPerror(dip, error, "pthread_mutex_lock() failed!");
	return (NULL);
    }
    /*
     * Now align the allocated address to a page alignment and offset.
     */
    ws->palign_addr = (void *)( ((ptr_t)ws->malloc_addr + page_size) & ~(page_size-1) );
    ws->palign_addr = (void *)((ptr_t)ws->palign_addr + offset);

    /*
     * Put the working set onto the linked list so that the original malloc-ed
     * buffer can be freeed when the user program is done with it.
     */
    ws->next = mpa_qhead.next;	    /* just put it at the head */
    mpa_qhead.next = ws;

    error = pthread_mutex_unlock(&malloc_mutex);
    if ( error ) {
	tPerror(dip, error, "pthread_mutex_unlock() failed!");
    }

    if (mDebugFlag) {
	Printf(dip, "malloc_palign: Aligned buffer at address "LLPXFMT" of %u bytes...\n",
	       ws->palign_addr, (bytes + offset));
    }
    return( ws->palign_addr );
}

/*
 * This is a local free routine to return to the system a previously alloc-ed
 * buffer.  A local linked list keeps copies of the original and adjusted
 * addresses.  This list is used by this routine to free the correct buffer.
 *
 * Inputs:
 *	pa_addr = The page aligned buffer to free.
 *
 * Return Value:
 *	void
 */
void
free_palign(dinfo_t *dip, void *pa_addr)
{
    int error;
    MPA_WS *ws, *wsq;

    if (mDebugFlag) {
	Printf(dip, "free_palign: Freeing aligned buffer at address "LLPXFMT"...\n", pa_addr);
    }
    error = pthread_mutex_lock(&malloc_mutex);
    if (error) {
	tPerror(dip, error, "pthread_mutex_lock() failed!");
	/* race noted... */
    }
    /*
     * Walk along the malloc-ed memory linked list, watch for a match on
     * the page aligned address.
     */
    ws = mpa_qhead.next;
    wsq = NULL;

    while ( ws ) {
	if ( ws->palign_addr == pa_addr ) {
	    break;
	}
	wsq = ws;
	ws = ws->next;
    }

    /*
     * After falling out of the loop the pointers are at the place where
     * some work has to be done, (this could also be at the beginning).
     */
    if ( ws ) {
	if ( wsq == NULL ) {
	    mpa_qhead.next = ws->next;
	} else {
	    wsq->next = ws->next;
	}
	if (mDebugFlag) {
	    Printf(dip, "  -> Freeing buffer at address "LLPXFMT", size "SUF" bytes...\n",
		   ws->malloc_addr, ws->malloc_size);
	}
	FreeMem(dip, ws->malloc_addr, ws->malloc_size);
	FreeMem(dip, ws, sizeof(*ws));
    } else {
        Eprintf(dip, "free_palign: BUG: Did not find buffer at address "LLPXFMT"...\n", pa_addr);
    }
    error = pthread_mutex_unlock(&malloc_mutex);
    if ( error ) {
	tPerror(dip, error, "pthread_mutex_unlock() failed!");
    }
    return;
}
