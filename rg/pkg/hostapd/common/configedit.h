/*
 * configedit -- programmatically edit a text file
 * Copyright (c) 2008, Atheros Communications
 * Author: Ted Merrill
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

/* Description:
 * A file or portions thereof may be buffered, edited and written out
 * using this module.
 * realloc() is used to make this work.
 * An important features is that configedit automatically updates
 * any user-defined bounds as text is added and removed.
 * Bounds are described in a hierarchical fashion.
 * At the top is the "root bound" describing the entire file as
 * stored in memory.
 * Beneath any bound may be child bounds.
 * Child bounds are always contrained to fit within parent bounds.
 * Insertion and deletion of text is done in the context of a bound,
 * with bounds (including parents, siblings, and descendents of those)
 * being adjusted in the process.
 *
 * The text is always kept in a single contiguous buffer, allowing
 * use of direct access methods.
 * Beware that the text is not necessarily null or even newline terminated.
 * Beware that the location of the text can change (using realloc)
 * on any operation that increases the size of the text.
 *
 * BEWARE:
 * No two sibling bounds should overlap... failure to follow this rule
 * will result in incorrect adjustments on insertions or cuts.
 * Zero-size bounds suffer from aliasing problems, and may result
 * in unanticipated ordering when they are inserted into.
 */

#ifndef configedit__h
#define configedit__h   /* once only */

struct configedit;      /* opaque declaration */


/*=========================================================================*/
/*              Bound allocation functions                                 */
/*=========================================================================*/


/* configedit_create begins an editing session by creating a root bound.
 * If buf_size is passed > 0 and buf is non-NULL then it is used
 * as the initial content of the edit buffer (don't continue to use
 * buf for anything else)... buf must have been obtained with a call
 * to malloc(buf_size)... content_size is the amount actually used.
 * Otherwise the root bound is empty.
 *
 * The root bound is returned.
 */
struct configedit *configedit_create(
        char *buf,      /* memory obtained from malloc only */
        int buf_size,   /* allocation size, > 0 only */
        int content_size    /* 0 <= content_size <= buf_size */
        );


/* configedit_begin returns a newly created bound of zero size at the
 * beginning of a given parent bound, which will be parent of new bound.
 * Pass NULL to create a new root bound (which will be it's own parent).
 *
 * No text is changed.
 *
 * When done, you can free it with configedit_free or rely
 * upon freeing of the root to free this... you must always at 
 * least free the root to avoid memory leak.
 */
struct configedit *configedit_begin(
        struct configedit *bound  /* parent bound; or NULL to create new root*/
        );

/* configedit_dup returns a newly created bound that is a duplicate
 * of the given bound (same parent, same bounds) but does not have
 * any children.
 * The argument MUST NOT be the root node.
 * No text is changed.
 *
 * If what you wanted was a zero-size bound at same start point as
 * the old bound, then next use configedit_bound_resize(new_bound, 0).
 */
struct configedit *configedit_dup(
        struct configedit *bound
        );

/* configedit_free frees a bound and all child bounds.
 * Be sure that no child bound pointers are used hereafter!!!
 * If the root bound is freed, the entire session is cleaned up.
 * Does nothing safely if bound is NULL.
 */
void configedit_free(
        struct configedit *bound
        );

/*=========================================================================*/
/*              Informational functions                                    */
/*=========================================================================*/

/* configedit_parent returns the parent bound of the given bound.
 * For the root bound, this returns itself.
 */
struct configedit *configedit_parent(
        struct configedit *bound
        );

/* configedit_offset returns the offset of the bound within parent.
 * For the root bound, this returns 0.
 */
int configedit_offset(
        struct configedit *bound
        );

/* configedit_size returns the size of the bound.
 * For the root bound, this returns entire text size.
 */
int configedit_size(
        struct configedit *bound
        );

/* configedit_ptr returns a pointer to the text for a bound.
 * NOTE that this is very volatile, and must not be used after
 * other configedit operations.
 * Also note that there is no null character termination.
 */
char *configedit_ptr(
        struct configedit *bound
        );

/* configedit_memcpy copies out the requested part of text from the bound
 * (less than requested if bound is smaller)
 * No text is changed.
 * Returns no. of bytes copied.
 * No bytes are copied if offset is out of bounds.
 */
int configedit_memcpy(
        struct configedit *bound,
        int offset,     /* offset within bound */
        int copy_size,  /* max amount to copy */
        char *out_buf         /* output here. must be at least "size" big */
        );

/* configedit_strcpy copies out the requested part of text from the bound
 * (less than requested if bound is smaller)
 * No text is changed.
 * Returns no. of bytes copied.
 * Gauranteed to be null-terminated; return value will thus always be
 * less than and never equal to size.
 */
int configedit_strcpy(
        struct configedit *bound,
        int offset,
        int buf_size,   /* size of buffer, max to copy inc. null char */
        char *out_buf
        );


/*=========================================================================*/
/*              Bound change functions (do not change text!)               */
/*=========================================================================*/

/* configedit_bound_resize modifies the size of the bound, but not outside
 * of the parent bound.
 * BEWARE not to leave any overlapping bounds when doing a cut or insertion!
 *
 * The argument MUST NOT be the root node.
 * The actual new size is returned.
 * Any children are freed.
 *
 * No text is changed.
 */
int configedit_bound_resize(
        struct configedit *bound,      /* bound to resize */
        int new_size
        );


/* configedit_advance rewrites a bound to a position nbytes relative
 * to the current character location, but not outside of the parent bound.
 * The size of the bound is set to zero.
 * The argument MUST NOT be the root node.
 * No text is changed.
 * Any children are freed.
 *
 * The actual delta is returned; zero if we're stuck at end of 
 * parent bound.
 */
int configedit_advance(
        struct configedit *bound,      /* bound to advance */
        int nbytes              /* negative to go backwards */
        );


/* configedit_next rewrites a bound to the current end of bound.
 * The size of the bound is set to zero.
 * The argument MUST NOT be the root node.
 * No text is changed.
 * Any children are freed.
 *
 * The actual delta is returned; zero if bound had no size.
 */
int configedit_next(
        struct configedit *bound       /* bound to advance */
        );


/* configedit_next_line rewrites the bound to hold 
 * the next line following within the parent bound.
 * No text is changed.
 * If there is no further text, the bound is situated at the end
 * of the parent bound and has zero size.
 * The argument MUST NOT be the root node.
 *
 * Note: any children are freed!!!
 *
 * Returns the number of bytes in the bound (0 if at end).
 */
int configedit_next_line(
        struct configedit *bound       /* bound to advance */
        );


/* configedit_next_word rewrites the bound to hold 
 * the next word following within the parent bound.
 * No text is changed.
 * Words are delimited by non-graphical characters, and
 * if template is passed non-NULL, by any characters in template as well.
 * Leading non-words are skipped first.
 * If there is no further text, the bound is situated at the end
 * of the parent bound and has zero size.
 * The argument MUST NOT be the root node.
 *
 * Note: any children are freed!!!
 *
 * Returns the number of bytes in the bound (0 if at end).
 */
int configedit_next_word(
        struct configedit *bound,      /* bound to advance */
        const char *template    /* NULL or null-term string of separators */
        );

/* configedit_rewind rewrites the bound to be at the beginning of the
 * parent and of zero size.
 * No text is changed.
 * Any children are freed.
 * The argument MUST NOT be the root node.
 */
void configedit_rewind(
        struct configedit *bound       /* bound to rewind */
        );


/* configedit_seekend rewrites the bound to be at the end of the
 * parent and of zero size.
 * Any children are freed.
 * The argument MUST NOT be the root node.
 */
void configedit_seekend(
        struct configedit *bound       /* bound to move to end of parent */
        );

/*=========================================================================*/
/*              Text changing functions                                    */
/*=========================================================================*/


/* configedit_text_resize does insertion (of newline fill) or deletion
 * at end of a bound in order to adjust bound to a new size.
 * Parent and sibling bounds (including bounds of parents) increase in size
 * or move as need to accomodate the new (or missing) content.
 * 
 * Siblings are NOT supposed to overlap each other,
 * things go to hell if this rule are not followed.
 * Also unexpected ordering can occur if zero size bounds are kept
 * around, even though it might seem to be nice to keep them around
 * for later insertion.
 *
 * Returns nonzero on error (realloc failure)
 */
int configedit_text_resize(
        struct configedit *bound,
        int new_size
        );


/* configedit_text_replace replaces content of bound with given text.
 * All bounds are adjusted as need be.
 * Returns nonzero on error.
 */
int configedit_text_replace(
        struct configedit *bound,
        const char *text,
        int text_size
        );

/* configedit_text_replace_string replaces content of bound with given string.
 * All bounds are adjusted as need be.
 * Returns nonzero on error.
 */
int configedit_text_replace_string(
        struct configedit *bound,
        const char *string
        );

/* configedit_text_append adds given text into file at end of current bound.
 * All bounds are adjusted as need be.
 * Returns nonzero on error.
 */
int configedit_text_append(
        struct configedit *bound,
        const char *text,
        int text_size
        );

/* configedit_text_append_string adds given string into file 
 * at end of current bound.
 * All bounds are adjusted as need be.
 * Returns nonzero on error.
 */
int configedit_text_append_string(
        struct configedit *bound,
        const char *string
        );

/*=========================================================================*/
/*              End                                                        */
/*=========================================================================*/

#endif  /* configedit__h */

