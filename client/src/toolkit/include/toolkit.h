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
 * Toolkit system header file.
 *
 * @author Alex Tokar */

#ifndef TOOLKIT_H
#define TOOLKIT_H

/* Porting API header file has extra priority. */
#include <porting.h>

/* Now all the other header files that are part of the toolkit. */
#include <binreloc.h>
#include <clioptions.h>
#include <common.h>
#include <console.h>
#include <logger.h>
#include <mempool.h>
#include <packet.h>
#include <sha1.h>
#include <shstr.h>
#include <socket.h>
#include <stringbuffer.h>
#include <utarray.h>
#include <uthash.h>
#include <utlist.h>

/**
 * Toolkit (de)initialization function. */
typedef void (*toolkit_func)(void);

/**
 * Check if the specified API has been imported yet. */
#define toolkit_imported(__api_name) toolkit_check_imported(toolkit_ ## __api_name ## _deinit)
/**
 * Import the specified API (if it has not been imported yet). */
#define toolkit_import(__api_name) toolkit_ ## __api_name ## _init()

/**
 * Start toolkit API initialization function. */
#define TOOLKIT_INIT_FUNC_START(__api_name) \
    { \
        toolkit_func __deinit_func = toolkit_ ## __api_name ## _deinit; \
        if (toolkit_imported(__api_name)) \
        { \
            return; \
        } \
        did_init = 1;

/**
 * End toolkit API initialization function. */
#define TOOLKIT_INIT_FUNC_END() \
    toolkit_import_register(__deinit_func); \
    }

/**
 * Start toolkit API deinitialization function. */
#define TOOLKIT_DEINIT_FUNC_START(__api_name) \
    {

/**
 * End toolkit API deinitialization function. */
#define TOOLKIT_DEINIT_FUNC_END() \
    did_init = 0; \
    }


#ifndef PRODUCTION
#   define TOOLKIT_FUNC_PROTECTOR(__api_name) \
    { \
        if (!did_init) \
        { \
            static uint8 did_warn = 0; \
            if (!did_warn) \
            { \
                toolkit_import(logger); \
                logger_print(LOG(WARNING), "Toolkit API function used, but the API was not initialized - this could result in undefined behavior."); \
                did_warn = 1; \
            } \
        } \
    }
#else
#   define TOOLKIT_FUNC_PROTECTOR
#endif


#endif
