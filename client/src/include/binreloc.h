/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * BinReloc - a library for creating relocatable executables
 * Written by: Hongli Lai <h.lai@chello.nl>
 * http://autopackage.org/
 *
 * This source code is public domain. You can relicense this code
 * under whatever license you want.
 *
 * See http://autopackage.org/docs/binreloc/ for
 * more information and how to use this.
 */

#ifndef BINRELOC_H
#define BINRELOC_H

/** These error codes can be returned by br_init(), br_init_lib(), gbr_init() or gbr_init_lib(). */
typedef enum {
	/** Cannot allocate memory. */
	BR_INIT_ERROR_NOMEM,
	/** Unable to open /proc/self/maps; see errno for details. */
	BR_INIT_ERROR_OPEN_MAPS,
	/** Unable to read from /proc/self/maps; see errno for details. */
	BR_INIT_ERROR_READ_MAPS,
	/** The file format of /proc/self/maps is invalid; kernel bug? */
	BR_INIT_ERROR_INVALID_MAPS,
	/** BinReloc is disabled (the ENABLE_BINRELOC macro is not defined). */
	BR_INIT_ERROR_DISABLED
} BrInitError;


#ifndef DOXYGEN
/* Mangle symbol names to avoid symbol collisions with other ELF objects. */
	#define br_init             PTeH3518859728963_br_init
	#define br_init_lib         PTeH3518859728963_br_init_lib
	#define br_find_exe         PTeH3518859728963_br_find_exe
	#define br_find_exe_dir     PTeH3518859728963_br_find_exe_dir
	#define br_find_prefix      PTeH3518859728963_br_find_prefix
	#define br_find_bin_dir     PTeH3518859728963_br_find_bin_dir
	#define br_find_sbin_dir    PTeH3518859728963_br_find_sbin_dir
	#define br_find_data_dir    PTeH3518859728963_br_find_data_dir
	#define br_find_locale_dir  PTeH3518859728963_br_find_locale_dir
	#define br_find_lib_dir     PTeH3518859728963_br_find_lib_dir
	#define br_find_libexec_dir PTeH3518859728963_br_find_libexec_dir
	#define br_find_etc_dir     PTeH3518859728963_br_find_etc_dir
	#define br_strcat           PTeH3518859728963_br_strcat
	#define br_build_path       PTeH3518859728963_br_build_path
	#define br_dirname          PTeH3518859728963_br_dirname
#endif

#endif
