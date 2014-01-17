/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the Free Software           *
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
*                                                                       *
* The author can be reached at admin@atrinik.org                        *
************************************************************************/

/**
 * @file
 * Object link related functions. */

#include <global.h>

/**
 * Objectlink memory pool. */
mempool_struct *pool_objectlink;

/**
 * Initialize the objectlink API. */
void objectlink_init(void)
{
    pool_objectlink = mempool_create("object links", 500, sizeof(objectlink), 0, NULL, NULL, NULL, NULL);
}

/**
 * Deinitialize the objectlink API. */
void objectlink_deinit(void)
{
    mempool_free(pool_objectlink);
}

/**
 * Allocate a new objectlink structure and initialize it.
 * @return Pointer to the new objectlink */
objectlink *get_objectlink(void)
{
    objectlink *ol = (objectlink *) get_poolchunk(pool_objectlink);

    memset(ol, 0, sizeof(objectlink));
    return ol;
}

/**
 * Free an object link.
 * @param ol Object link to free. */
void free_objectlink(objectlink *ol)
{
    if (OBJECT_VALID(ol->objlink.ob, ol->id)) {
        CLEAR_FLAG(ol->objlink.ob, FLAG_IS_LINKED);
    }

    free_objectlink_simple(ol);
}

/**
 * Recursively free all objectlinks.
 * @param ol The objectlink. */
static void free_objectlink_recursive(objectlink *ol)
{
    if (ol->next) {
        free_objectlink_recursive(ol->next);
    }

    if (OBJECT_VALID(ol->objlink.ob, ol->id)) {
        CLEAR_FLAG(ol->objlink.ob, FLAG_IS_LINKED);
    }

    free_objectlink_simple(ol);
}

/**
 * Recursively free all linked lists of objectlink pointers
 * @warning Only call for lists with FLAG_IS_LINKED - friendly lists
 * and some others handle their own objectlink malloc/free.
 * @param obp The oblinkpt */
void free_objectlinkpt(objectlink *obp)
{
    if (obp->next) {
        free_objectlinkpt(obp->next);
    }

    if (obp->objlink.link) {
        free_objectlink_recursive(obp->objlink.link);
    }

    free_objectlink_simple(obp);
}

/**
 * Generic link function for object links. */
objectlink *objectlink_link(objectlink **startptr, objectlink **endptr, objectlink *afterptr, objectlink *beforeptr, objectlink *objptr)
{
    /* Link it behind afterptr */
    if (!beforeptr) {
        /* If not, we just have to update startptr and endptr */
        if (afterptr) {
            /* Link between something? */
            if (afterptr->next) {
                objptr->next = afterptr->next;
                afterptr->next->prev = objptr;
            }

            afterptr->next = objptr;
            objptr->prev = afterptr;
        }

        if (startptr && !*startptr) {
            *startptr = objptr;
        }

        if (endptr && (!*endptr || *endptr == afterptr)) {
            *endptr = objptr;
        }
    }
    /* Link it before beforeptr */
    else if (!afterptr) {
        if (beforeptr->prev) {
            objptr->prev = beforeptr->prev;
            beforeptr->prev->next = objptr;
        }

        beforeptr->prev = objptr;
        objptr->next = beforeptr;

        /* We can't be endptr but perhaps start */
        if (startptr && (!*startptr || *startptr == beforeptr)) {
            *startptr = objptr;
        }
    }
    /* Special: link together two lists/objects */
    else {
        beforeptr->prev = objptr;
        afterptr->next = objptr;
        objptr->next = beforeptr;
        objptr->prev = afterptr;
    }

    return objptr;
}

/**
 * Unlink object link from a list. */
objectlink *objectlink_unlink(objectlink **startptr, objectlink **endptr, objectlink *objptr)
{
    if (startptr && *startptr == objptr) {
        *startptr = objptr->next;
    }

    if (endptr && *endptr == objptr) {
        *endptr = objptr->prev;
    }

    if (objptr->prev) {
        objptr->prev->next = objptr->next;
    }

    if (objptr->next) {
        objptr->next->prev = objptr->prev;
    }

    return objptr;
}
