/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 3 of the License, or     *
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

/*
 * Expands a layout by 2x in each dimension.
 * (Header file)
 * H. S. Teoh
 * --------------------------------------------------------------------------
 * $Id: expand2x.h,v 1.1.1.1 2003/10/16 00:17:06 michtoen Exp $
 */

#ifndef EXPAND2X_H
#define EXPAND2X_H

/* Expands a layout by 2x in each dimension.
 * Resulting layout is actually (2*xsize-1)x(2*ysize-1). (Because of the cheesy
 * algorithm, but hey, it works).
 *
 * Don't forget to free the old layout after this is called (it does not
 * presume to do so itself).
 */
char **expand2x(char **layout, int xsize, int ysize);

#endif /* EXPAND2X_H */
