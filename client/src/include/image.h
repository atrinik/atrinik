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
 * Image related structures.
 */

#ifndef IMAGE_H
#define IMAGE_H

/**
 * Structure for bmap data.
 */
typedef struct bmap {
    /**
     * The name.
     */
    char *name;

    /**
     * Size in bytes.
     */
    size_t len;

    /**
     * Position in atrinik.p0 file, if -1, it doesn't exist in the file.
     */
    long pos;

    /**
     * Checksum.
     */
    unsigned long crc32;
} bmap_t;

/**
 * Container structure for bmaps.
 */
typedef struct bmap_hash {
    /**
     * Actual bmap data.
     */
    bmap_t bmap;

    /**
     * Hash handle.
     */
    UT_hash_handle hh;
} bmap_hash_t;

/* Prototypes */
void image_init(void);
void image_deinit(void);
void image_bmaps_init(void);
void image_bmaps_deinit(void);
void finish_face_cmd(int facenum, uint32_t checksum, const char *face);
void image_request_face(int pnum);
int image_get_id(const char *name);

#endif
