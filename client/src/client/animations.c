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
 *  */

#include <global.h>

/**
 * Load animations. */
void read_anims(void)
{
    size_t anim_len = 0;
    uint8 new_anim = 1;
    uint8 faces = 0;
    FILE *fp;
    char buf[HUGE_BUF];
    uint8 anim_cmd[2048];
    size_t count = 0;

    if (animations_num) {
        size_t i;

        /* Clear both animation tables. */
        for (i = 0; i < animations_num; i++) {
            if (animations[i].faces) {
                free(animations[i].faces);
            }

            if (anim_table[i].anim_cmd) {
                free(anim_table[i].anim_cmd);
            }
        }

        free(animations);
        animations = NULL;
        free(anim_table);
        anim_table = NULL;
        animations_num = 0;
    }

    anim_table = malloc(sizeof(_anim_table));

    /* Animation #0 is like face id #0. */
    anim_cmd[0] = (uint8) ((count >> 8) & 0xff);
    anim_cmd[1] = (uint8) (count & 0xff);
    anim_cmd[2] = 0;
    anim_cmd[3] = 1;
    anim_cmd[4] = 0;
    anim_cmd[5] = 0;

    anim_table[count].anim_cmd = malloc(6);
    memcpy(anim_table[count].anim_cmd, anim_cmd, 6);
    anim_table[count].len = 6;
    count++;

    fp = server_file_open(SERVER_FILE_ANIMS);

    if (!fp) {
        logger_print(LOG(ERROR), "Could not open anims server file.");
    }

    while (fgets(buf, sizeof(buf) - 1, fp)) {
        /* Are we outside an anim body? */
        if (new_anim == 1) {
            if (!strncmp(buf, "anim ", 5)) {
                new_anim = 0;
                faces = 0;
                anim_cmd[0] = (uint8)((count >> 8) & 0xff);
                anim_cmd[1] = (uint8)(count & 0xff);
                faces = 1;
                anim_len = 4;
            }
            /* we should never hit this point */
            else {
                logger_print(LOG(BUG), "Error parsing anims.tmp - unknown cmd: >%s<!", buf);
            }
        }
        /* No, we are inside! */
        else {
            if (!strncmp(buf, "facings ", 8)) {
                faces = atoi(buf + 8);
            }
            else if (!strncmp(buf, "mina", 4)) {
                anim_table = realloc(anim_table, sizeof(_anim_table) * (count + 1));
                anim_cmd[2] = 0;
                anim_cmd[3] = faces;
                anim_table[count].len = anim_len;
                anim_table[count].anim_cmd = malloc(anim_len);
                memcpy(anim_table[count].anim_cmd, anim_cmd, anim_len);
                count++;
                new_anim = 1;
            }
            else {
                uint16 face_id = atoi(buf);

                anim_cmd[anim_len++] = (uint8) ((face_id >> 8) & 0xff);
                anim_cmd[anim_len++] = (uint8) (face_id & 0xff);
            }
        }
    }

    animations_num = count;
    animations = calloc(animations_num, sizeof(Animations));
    fclose(fp);
}

/**
 * Reset the necessary values in animations table instead of reloading
 * them from file. */
void anims_reset(void)
{
    size_t i;

    for (i = 0; i < animations_num; i++) {
        animations[i].loaded = 0;
    }
}
