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
 * File with common definitions/macros/calculations/algorithms for both the
 * server and the client.
 */

#ifndef TOOLKIT_COMMON_H
#define TOOLKIT_COMMON_H

/** Multiplier for spell points cost based on the attenuation. */
#define PATH_SP_MULT(op, spell) ((((op)->path_attuned & (spell)->path) ? 0.8 : 1) * (((op)->path_repelled & (spell)->path) ? 1.25 : 1))
/** Multiplier for spell damage based on the attenuation. */
#define PATH_DMG_MULT(op, spell) ((((op)->path_attuned & (spell)->path) ? 1.25 : 1) * (((op)->path_repelled & (spell)->path) ? 0.7 : 1))

#endif
