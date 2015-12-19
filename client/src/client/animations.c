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
 *
 */

#include <global.h>

/**
 * Load animations.
 */
void read_anims(void)
{
    anims_deinit();

    size_t count = 0;
    anim_table = emalloc(sizeof(*anim_table));

    /* Animation #0 is like face id #0. */
    uint8_t anim_cmd[2048];
    anim_cmd[0] = (uint8_t) ((count >> 8) & 0xff);
    anim_cmd[1] = (uint8_t) (count & 0xff);
    anim_cmd[2] = 0;
    anim_cmd[3] = 1;
    anim_cmd[4] = 0;
    anim_cmd[5] = 0;

    anim_table[count].anim_cmd = emalloc(6);
    memcpy(anim_table[count].anim_cmd, anim_cmd, 6);
    anim_table[count].len = 6;
    count++;

    FILE *fp = server_file_open_name(SERVER_FILE_ANIMS);
    if (fp == NULL) {
        return;
    }

    uint8_t faces;
    size_t anim_len;
    bool in_anim = false;

    char buf[HUGE_BUF];
    while (fgets(VS(buf), fp)) {
        if (!in_anim) {
            if (strncmp(buf, "anim ", 5) == 0) {
                in_anim = true;
                anim_cmd[0] = (uint8_t) ((count >> 8) & 0xff);
                anim_cmd[1] = (uint8_t) (count & 0xff);
                faces = 1;
                anim_len = 4;
            } else {
                LOG(ERROR, "Invalid entry in animations file: %s", buf);
                break;
            }
        } else {
            if (!strncmp(buf, "facings ", 8)) {
                faces = atoi(buf + 8);
            } else if (!strncmp(buf, "mina", 4)) {
                anim_table = erealloc(anim_table, sizeof(*anim_table) *
                        (count + 1));
                anim_cmd[2] = 0;
                anim_cmd[3] = faces;
                anim_table[count].len = anim_len;
                anim_table[count].anim_cmd = emalloc(anim_len);
                memcpy(anim_table[count].anim_cmd, anim_cmd, anim_len);
                count++;
                in_anim = false;
            } else {
                uint16_t face_id = atoi(buf);
                anim_cmd[anim_len++] = (uint8_t) ((face_id >> 8) & 0xff);
                anim_cmd[anim_len++] = (uint8_t) (face_id & 0xff);
            }
        }
    }

    animations_num = count;
    animations = ecalloc(animations_num, sizeof(*animations));
    fclose(fp);
}

/**
 * Deinitialize the animations.
 */
void anims_deinit(void)
{
    /* Clear both animation tables. */
    for (size_t i = 0; i < animations_num; i++) {
        if (animations[i].faces != NULL) {
            efree(animations[i].faces);
            animations[i].faces = NULL;
        }

        if (anim_table[i].anim_cmd != NULL) {
            efree(anim_table[i].anim_cmd);
        }
    }

    animations_num = 0;

    if (animations != NULL) {
        efree(animations);
        animations = NULL;
    }

    if (anim_table != NULL) {
        efree(anim_table);
        anim_table = NULL;
    }
}

/**
 * Reset the necessary values in animations table instead of reloading
 * them from file.
 */
void anims_reset(void)
{
    size_t i;

    for (i = 0; i < animations_num; i++) {
        animations[i].loaded = 0;
        animations[i].num_animations = 0;

        if (animations[i].faces != NULL) {
            efree(animations[i].faces);
            animations[i].faces = NULL;
        }
    }
}
