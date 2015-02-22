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


/*=========================================================================*/
/*              Includes                                                   */
/*=========================================================================*/
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*=========================================================================*/
/*              Data structures                                            */
/*=========================================================================*/

/* struct configedit -- a node that describes a "bounds" within file.
 * Note: this is opaque outside of this file!
 */
struct configedit {
    struct configedit *root;      /* pointer to root node */
    struct configedit *parent;    /* NOTE: root is it's own parent */
    /* sibling list, includes self */
    struct configedit *next;      /* double-linked list */
    struct configedit *prev;      /* double-linked list */
    /* NULL or pointer to one of the children, who are linked 
     *  via next/prev pointers
     */
    struct configedit *child;
    /* begin/end are relative to parent! */
    int begin;          /* beginning offset */
    int end;            /* 1 past the last char in bound; == begin if empty */
    /* buf and buf_size are used only by the root node! */
    char *buf;          /* holds content being edited */
    int buf_size;        /* amount allocated */
};

/* configedit_malloc_extra -- allocate this much extra text buffer
 * whenever we need to reallocate to cut down on the number of re-allocations.
 */
#define configedit_malloc_extra 512

/*=========================================================================*/
/*              Private helper functions                                   */
/*=========================================================================*/

/* configedit_new allocates a new bound which will be
 * maintained by configedit.
 * The bound will be freed if it's parent is freed...
 * Returns NULL on malloc error.
 *
 * This is a private function because care must be taken to first properly
 * adjust the bounds of the parent, which can only be done "privately".
 */
static struct configedit *configedit_new(
        struct configedit *parent_bound,
        /* begin/end are relative to begin of parent! */
        int begin,      /* offset of first byte in bound, rel to parent */
        int end         /* 1 past last byte, or == begin if empty */
        )
{
    struct configedit *root;
    struct configedit *bound;

    if (parent_bound == NULL) {
        return NULL;    /* sanity */
    }
    root = parent_bound->root;
    if (root == NULL) {
        return NULL;    /* sanity */
    }

    bound = malloc(sizeof(*bound));
    if (bound == NULL) {
        return NULL;
    }
    memset(bound, 0, sizeof(*bound));
    bound->root = root;
    bound->parent = parent_bound;
    if (parent_bound->child == NULL) {
        /* we are first child */
        bound->prev = bound->next = bound;
        parent_bound->child = bound;
    } else {
        /* add to existing sibling list */
        /* note, we don't care about order in the list */
        bound->next = parent_bound->child;
        bound->prev = bound->next->prev;
        bound->next->prev = bound;
        bound->prev->next = bound;
    }
    bound->begin = begin;
    bound->end = end;

    return bound;
}


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
        )
{
    struct configedit *root = malloc(sizeof(*root));
    if (root == NULL) return NULL;
    memset(root, 0, sizeof(*root));
    root->root = root;
    root->parent = root;
    root->next = root->prev = root;     /* sibling list */
    if (buf && buf_size) {
        root->buf = buf;
        root->buf_size = buf_size;
        root->end = content_size;
    } else {
        /* Given an initial allocation so we never again deal with buf==NULL */
        root->buf_size = configedit_malloc_extra;
        root->buf = malloc(root->buf_size);
        if (root->buf == NULL) {
            free(root);
            return NULL;
        }
    }
    return root;
}


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
        )
{
    if (bound == NULL) return configedit_create(NULL, 0, 0);
    return configedit_new(bound, 0, 0);
}

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
        )
{
    return configedit_new(bound->parent, bound->begin, bound->end);
}


/* configedit_free frees a bound and all child bounds.
 * Be sure that no child bound pointers are used hereafter!!!
 * If the root bound is freed, the entire session is cleaned up.
 * Does nothing safely if bound is NULL.
 */
void configedit_free(
        struct configedit *bound
        )
{
    struct configedit *root;

    if (bound == NULL ) {
        return;         /* sanity */
    }
    root = bound->root;
    if (root == NULL ) {
        return;         /* sanity */
    }
    /* free children first */
    while (bound->child != NULL) {
        configedit_free(bound->child);
    }
    if (bound == root) {
        /* free entire session */
        if (root->buf) free(root->buf);
        memset(root, 0, sizeof(*root)); /* helps debugging */
        free(root);
    } else {
        /* free just this bound */
        if (bound->next == bound) {
            /* last in sibling list */
            bound->parent->child = NULL;
        } else {
            /* remove from sibling list */
            if (bound->parent->child == bound) {
                bound->parent->child = bound->next;
            }
            bound->prev->next = bound->next;
            bound->next->prev = bound->prev;
        }
        memset(bound, 0, sizeof(*bound));       /* helps debugging */
        free(bound);
    }
    return;
}

/*=========================================================================*/
/*              Informational functions                                    */
/*=========================================================================*/

/* configedit_parent returns the parent bound of the given bound.
 * For the root bound, this returns itself.
 */
struct configedit *configedit_parent(
        struct configedit *bound
        )
{
    return bound->parent;
}

/* configedit_offset returns the offset of the bound within parent.
 * For the root bound, this returns 0.
 */
int configedit_offset(
        struct configedit *bound
        )
{
    return bound->begin;
}

/* configedit_size returns the size of the bound.
 * For the root bound, this returns entire text size.
 */
int configedit_size(
        struct configedit *bound
        )
{
    return bound->end - bound->begin;
}

/* configedit_ptr returns a pointer to the text for a bound.
 * NOTE that this is very volatile, and must not be used after
 * other configedit operations.
 * Also note that there is no null character termination.
 */
char *configedit_ptr(
        struct configedit *bound
        )
{
    struct configedit *root = bound->root;
    int offset = bound->begin;
    struct configedit *parent = bound->parent;
    while (parent != root) {
        offset += parent->begin;
        parent = parent->parent;
    }
    return root->buf + offset;
}

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
        char *out_buf   /* output here. must be at least "size" big */
        )
{
    int bound_size = bound->end - bound->begin;
    int end_offset;

    /* bring within bound */
    if (offset < 0) return 0;
    if (offset >= bound_size) return 0;
    end_offset = offset + copy_size;
    if (end_offset > bound_size) end_offset = bound_size;
    copy_size = end_offset - offset;
    if (copy_size <= 0) return 0;

    /* copy the portion */
    memcpy(out_buf, configedit_ptr(bound)+offset, copy_size);
    return copy_size;
}


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
        )
{
    int ncopied = configedit_memcpy(bound, offset, buf_size-1, out_buf);
    if (buf_size > 0) out_buf[ncopied] = 0; /* null termination */
    return ncopied;
}


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
        )
{
    struct configedit *parent = bound->parent;
    int parent_size = parent->end - parent->begin;
    int new_end;
    
    /* Free any children, since they no longer apply */
    while (bound->child) configedit_free(bound->child);

    /* Adjust size */
    if (new_size < 0) new_size = 0;
    new_end = bound->begin + new_size;
    if (new_end > parent_size) new_end = parent_size;
    bound->end = new_end;

    return bound->end - bound->begin;
}


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
        )
{
    struct configedit *parent = bound->parent;
    int parent_size = parent->end - parent->begin;
    int old_begin = bound->begin;
    int new_begin = old_begin + nbytes;

    /* Free any children, since they no longer apply */
    while (bound->child) configedit_free(bound->child);

    if (new_begin < 0) new_begin = 0;
    if (new_begin > parent_size) new_begin = parent_size;
    bound->begin = new_begin;
    bound->end = bound->begin;
    return new_begin - old_begin;
}


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
        )
{
    int bound_size = bound->end - bound->begin;
    return configedit_advance(bound, bound_size);
}


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
        )
{
    char *ptr;
    int parent_size;
    int max_size;
    int size;

    /* Free any children, since they no longer apply */
    while (bound->child) configedit_free(bound->child);

    bound->begin = bound->end;  /* throw away previous bound */
    parent_size = bound->parent->end - bound->parent->begin;
    max_size = parent_size - bound->begin;
    ptr = configedit_ptr(bound);
    for (size = 0; size < max_size; size++) {
        if (ptr[size] == '\n') {
            size++;
            break;
        }
    }
    bound->end += size;
    return size;
}


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
        )
{
    char *ptr;
    int parent_size;
    int max_size;
    int size;

    /* Free any children, since they no longer apply */
    while (bound->child) configedit_free(bound->child);

    bound->begin = bound->end;  /* throw away previous bound */
    parent_size = bound->parent->end - bound->parent->begin;
    max_size = parent_size - bound->begin;
    ptr = configedit_ptr(bound);
    /* skip leading delimiters */
    for (size = 0; size < max_size; size++) {
        if (! isgraph(ptr[size])) {
            continue;
        }
        if (template && strchr(template,ptr[size])) {
            continue;
        }
        break;
    }
    max_size -= size;
    bound->begin += size;
    bound->end = bound->begin;
    /* Now count the word */
    for (size = 0; size < max_size; size++) {
        if (! isgraph(ptr[size])) {
            size++;
            break;
        }
        if (template && strchr(template,ptr[size])) {
            size++;
            break;
        }
    }
    bound->end += size;
    return size;
}


/* configedit_rewind rewrites the bound to be at the beginning of the
 * parent and of zero size.
 * No text is changed.
 * Any children are freed.
 * The argument MUST NOT be the root node.
 */
void configedit_rewind(
        struct configedit *bound       /* bound to rewind */
        )
{
    /* Free any children, since they no longer apply */
    while (bound->child) configedit_free(bound->child);

    bound->begin = bound->end = 0;
    return;
}


/* configedit_seekend rewrites the bound to be at the end of the
 * parent and of zero size.
 * Any children are freed.
 * The argument MUST NOT be the root node.
 */
void configedit_seekend(
        struct configedit *bound       /* bound to move to end of parent */
        )
{
    struct configedit *parent = bound->parent;

    /* Free any children, since they no longer apply */
    while (bound->child) configedit_free(bound->child);

    bound->begin = bound->end = (parent->end - parent->begin);
    return;
}


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
        )
{
    struct configedit *root = bound->root;
    struct configedit *parent = bound->parent;
    int old_size = bound->end - bound->begin;
    int add_size;       /* how much we are adding (negative to cut) */
    int offset;
    int end_offset;
    int new_root_size;
    struct configedit *node;

    if (new_size == old_size) goto done;

    add_size = new_size - old_size;     /* negative to cut */
    /* Note that we are not bound by parent bounds because they
     * will be increased if necessary! 
     */

    /* Convert to root offset */
    offset = bound->end;
    for (parent = bound->parent; parent != root; parent = parent->parent) {
        offset += parent->begin;
    }
    end_offset = offset + add_size;

    /* allocate more memory if need be */
    new_root_size = root->end + add_size;
    if (new_root_size > root->buf_size) {
        int new_buf_size = new_root_size+configedit_malloc_extra;
        char *new_buf = realloc(root->buf, new_buf_size);
        if (new_buf == NULL) goto fail;
        root->buf = new_buf;
        root->buf_size = new_buf_size;
    }
    /* any stored text after our bound gets moved */
    if (root->end > offset) {
        memmove(root->buf + end_offset, root->buf + offset,
            root->end - offset);
    }
    /* provide default fill... just in case.
     * Newlines should be safe
     */
    if (add_size > 0) {
        memset(root->buf + offset, '\n', add_size);
    }

    /* The bound and all parents change size (move their end) by
     * the amount we've added/subtracted (add_size)... 
     * do this up to and including the root.
     * Siblings are NOT supposed to overlap the bound, 
     * and we MUST NOT keep zero size bounds around,
     * so all we do is move the siblings (move both begin and end)
     * if their begin was >= insertion point.
     *
     * Things go to hell if these rules are not followed.
     */
    offset = old_size;   /* in our space */
    node = bound;      /* begin with ourself */
    for(;;) {
        struct configedit *sibling;

        offset += node->begin;        /* in our parent's space */
        node->end += add_size;        /* change our size */
        sibling = node->next;
        while (sibling != node) {
            if (sibling->begin >= offset) {
                /* move siblings who are after us */
                sibling->begin += add_size;
                sibling->end += add_size;
            }
            sibling = sibling->next;
        }
        if (node == root) break;
        node = node->parent;
    }

    done:
    return 0;

    fail:
    return 1;
}


/* configedit_text_replace replaces content of bound with given text.
 * All bounds are adjusted as need be.
 * Returns nonzero on error.
 */
int configedit_text_replace(
        struct configedit *bound,
        const char *text,
        int text_size
        )
{
    if (configedit_text_resize(bound, text_size)) {
        return 1;
    }
    memcpy(configedit_ptr(bound), text, text_size);
    return 0;
}


/* configedit_text_replace_string replaces content of bound with given string.
 * All bounds are adjusted as need be.
 * Returns nonzero on error.
 */
int configedit_text_replace_string(
        struct configedit *bound,
        const char *string
        )
{
    return configedit_text_replace(bound, string, strlen(string));
}

/* configedit_text_append adds given text into file at end of current bound.
 * All bounds are adjusted as need be.
 * Returns nonzero on error.
 */
int configedit_text_append(
        struct configedit *bound,
        const char *text,
        int text_size
        )
{
    int old_size = bound->end - bound->begin;
    int new_size = old_size + text_size;
    if (configedit_text_resize(bound, new_size)) {
        return 1;
    }
    memcpy(configedit_ptr(bound)+old_size, text, text_size);
    return 0;
}

/* configedit_text_append_string adds given string into file 
 * at end of current bound.
 * All bounds are adjusted as need be.
 * Returns nonzero on error.
 */
int configedit_text_append_string(
        struct configedit *bound,
        const char *string
        )
{
    return configedit_text_append(bound, string, strlen(string));
}



/*=========================================================================*/
/*              End                                                        */
/*=========================================================================*/


