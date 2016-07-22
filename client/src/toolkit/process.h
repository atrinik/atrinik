/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Process API header file.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <toolkit.h>

typedef struct process process_t;

/**
 * Callback definition used for acquiring data read from a process.
 *
 * @param process
 * Process.
 * @param data
 * Data that was read.
 * @param len
 * Length of the data.
 */
typedef void (*process_data_callback_t)(process_t *process,
                                        uint8_t   *data,
                                        size_t     len);

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(process);

process_t *
process_create(const char *executable);
void
process_free(process_t *process);
void
process_add_arg(process_t *process, const char *arg);
const char *
process_get_str(process_t *process);
void
process_set_restart(process_t *process, bool val);
void
process_set_data_out_cb(process_t *process, process_data_callback_t cb);
void
process_set_data_err_cb(process_t *process, process_data_callback_t cb);
bool
process_is_running(process_t *process);
bool
process_start(process_t *process);
void
process_stop(process_t *process);
void
process_check(process_t *process);
void
process_check_all(void);

#endif
