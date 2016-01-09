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
 * Material related code.
 *
 * @author Alex Tokar
 */

#include <global.h>

/**
 * Material types.
 */
material_t materials[NROFMATERIALS] = {
    {"paper"},
    {"metal"},
    {"crystal"},
    {"leather"},
    {"wood"},
    {"organics"},
    {"stone"},
    {"cloth"},
    {"magic material"},
    {"liquid"},
    {"soft metal"},
    {"bone"},
    {"ice"},
};

/**
 * Real material types. This array is initialized by material_init().
 */
material_real_t materials_real[NUM_MATERIALS_REAL];

/**
 * Initialize materials from file.
 */
void
material_init (void)
{
    /* First initialize default values to the array */
    for (int i = 0; i < NUM_MATERIALS_REAL; i++) {
        materials_real[i].name[0] = '\0';
        materials_real[i].quality = 100;
        materials_real[i].type = M_NONE;
        materials_real[i].def_race = RACE_TYPE_NONE;
    }

    char filename[MAX_BUF];
    snprintf(VS(filename), "%s/materials", settings.libpath);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        LOG(ERROR,
            "Could not open materials file %s: %s",
            filename, strerror(errno));
        exit(1);
    }

    char buf[MAX_BUF];
    while (fgets(VS(buf), fp)) {
        if (*buf == '#' || *buf == '\n') {
            continue;
        }

        int i;
        if (sscanf(buf, "material_real %d\n", &i) != 1) {
            LOG(ERROR, "Bogus line in materials file: %s", buf);
            exit(1);
        }

        if (i < 0 || i >= NUM_MATERIALS_REAL) {
            LOG(ERROR,
                "Materials file contains declaration for material #%d but it "
                "doesn't exist.", i);
            exit(1);
        }

        int def_race = RACE_TYPE_NONE, type = M_NONE, quality = 100;
        char name[MAX_BUF] = {'\0'};

        while (fgets(VS(buf), fp)) {
            if (strcmp(buf, "end\n") == 0) {
                break;
            }

            if (sscanf(buf, "quality %d\n", &quality) != 1 &&
                sscanf(buf, "type %d\n", &type) != 1 &&
                sscanf(buf, "def_race %d\n", &def_race) != 1 &&
                sscanf(buf, "name %[^\n]", name) != 1) {
                LOG(ERROR, "Bogus line in materials file: %s", buf);
                exit(1);
            }
        }

        if (*name != '\0') {
            snprintf(VS(materials_real[i].name), "%s ", name);
        }

        materials_real[i].quality = quality;
        materials_real[i].type = type;
        materials_real[i].def_race = def_race;
    }

    fclose(fp);
}
