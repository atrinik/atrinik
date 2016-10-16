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
 * Everything concerning treasures.
 */

#include <global.h>
#include <loader.h>
#include <arch.h>
#include <artifact.h>
#include "object_methods.h"
#include <toolkit/string.h>

/** All the coin arches. */
const char *const coins[NUM_COINS + 1] = {
    "ambercoin",
    "mithrilcoin",
    "jadecoin",
    "goldcoin",
    "silvercoin",
    "coppercoin",
    NULL
};

/**
 * Difficulty to magic chance list.
 */
static const int difftomagic_list[DIFFLEVELS][MAXMAGIC + 1] = {
    /* Chance of magic  Difficulty */
    /* +0 +1 +2 +3 +4 */
    {94, 3, 2, 1, 0},   /*1*/
    {94, 3, 2, 1, 0},   /*2*/
    {94, 3, 2, 1, 0},   /*3*/
    {94, 3, 2, 1, 0},   /*4*/
    {94, 3, 2, 1, 0},   /*5*/
    {90, 4, 3, 2, 1},   /*6*/
    {90, 4, 3, 2, 1},   /*7*/
    {90, 4, 3, 2, 1},   /*8*/
    {90, 4, 3, 2, 1},   /*9*/
    {90, 4, 3, 2, 1},   /*10*/
    {85, 6, 4, 3, 2},   /*11*/
    {85, 6, 4, 3, 2},   /*12*/
    {85, 6, 4, 3, 2},   /*13*/
    {85, 6, 4, 3, 2},   /*14*/
    {85, 6, 4, 3, 2},   /*15*/
    {80, 8, 5, 4, 3},   /*16*/
    {80, 8, 5, 4, 3},   /*17*/
    {80, 8, 5, 4, 3},   /*18*/
    {80, 8, 5, 4, 3},   /*19*/
    {80, 8, 5, 4, 3},   /*20*/
    {75, 10, 6, 5, 4},  /*21*/
    {75, 10, 6, 5, 4},  /*22*/
    {75, 10, 6, 5, 4},  /*23*/
    {75, 10, 6, 5, 4},  /*24*/
    {75, 10, 6, 5, 4},  /*25*/
    {70, 12, 7, 6, 5},  /*26*/
    {70, 12, 7, 6, 5},  /*27*/
    {70, 12, 7, 6, 5},  /*28*/
    {70, 12, 7, 6, 5},  /*29*/
    {70, 12, 7, 6, 5},  /*30*/
    {70, 9, 8, 7, 6},   /*31*/
    {70, 9, 8, 7, 6},   /*32*/
    {70, 9, 8, 7, 6},   /*33*/
    {70, 9, 8, 7, 6},   /*34*/
    {70, 9, 8, 7, 6},   /*35*/
    {70, 6, 9, 8, 7},   /*36*/
    {70, 6, 9, 8, 7},   /*37*/
    {70, 6, 9, 8, 7},   /*38*/
    {70, 6, 9, 8, 7},   /*39*/
    {70, 6, 9, 8, 7},   /*40*/
    {70, 3, 10, 9, 8},  /*41*/
    {70, 3, 10, 9, 8},  /*42*/
    {70, 3, 10, 9, 8},  /*43*/
    {70, 3, 10, 9, 8},  /*44*/
    {70, 3, 10, 9, 8},  /*45*/
    {70, 2, 9, 10, 9},  /*46*/
    {70, 2, 9, 10, 9},  /*47*/
    {70, 2, 9, 10, 9},  /*48*/
    {70, 2, 9, 10, 9},  /*49*/
    {70, 2, 9, 10, 9},  /*50*/
    {70, 2, 7, 11, 10}, /*51*/
    {70, 2, 7, 11, 10}, /*52*/
    {70, 2, 7, 11, 10}, /*53*/
    {70, 2, 7, 11, 10}, /*54*/
    {70, 2, 7, 11, 10}, /*55*/
    {70, 2, 5, 12, 11}, /*56*/
    {70, 2, 5, 12, 11}, /*57*/
    {70, 2, 5, 12, 11}, /*58*/
    {70, 2, 5, 12, 11}, /*59*/
    {70, 2, 5, 12, 11}, /*60*/
    {70, 2, 3, 13, 12}, /*61*/
    {70, 2, 3, 13, 12}, /*62*/
    {70, 2, 3, 13, 12}, /*63*/
    {70, 2, 3, 13, 12}, /*64*/
    {70, 2, 3, 13, 12}, /*65*/
    {70, 2, 3, 12, 13}, /*66*/
    {70, 2, 3, 12, 13}, /*67*/
    {70, 2, 3, 12, 13}, /*68*/
    {70, 2, 3, 12, 13}, /*69*/
    {70, 2, 3, 12, 13}, /*70*/
    {70, 2, 3, 11, 14}, /*71*/
    {70, 2, 3, 11, 14}, /*72*/
    {70, 2, 3, 11, 14}, /*73*/
    {70, 2, 3, 11, 14}, /*74*/
    {70, 2, 3, 11, 14}, /*75*/
    {70, 2, 3, 10, 15}, /*76*/
    {70, 2, 3, 10, 15}, /*77*/
    {70, 2, 3, 10, 15}, /*78*/
    {70, 2, 3, 10, 15}, /*79*/
    {70, 2, 3, 10, 15}, /*80*/
    {70, 2, 3, 9, 16},  /*81*/
    {70, 2, 3, 9, 16},  /*82*/
    {70, 2, 3, 9, 16},  /*83*/
    {70, 2, 3, 9, 16},  /*84*/
    {70, 2, 3, 9, 16},  /*85*/
    {70, 2, 3, 8, 17},  /*86*/
    {70, 2, 3, 8, 17},  /*87*/
    {70, 2, 3, 8, 17},  /*88*/
    {70, 2, 3, 8, 17},  /*89*/
    {70, 2, 3, 8, 17},  /*90*/
    {70, 2, 3, 7, 18},  /*91*/
    {70, 2, 3, 7, 18},  /*92*/
    {70, 2, 3, 7, 18},  /*93*/
    {70, 2, 3, 7, 18},  /*94*/
    {70, 2, 3, 7, 18},  /*95*/
    {70, 2, 3, 6, 19},  /*96*/
    {70, 2, 3, 6, 19},  /*97*/
    {70, 2, 3, 6, 19},  /*98*/
    {70, 2, 3, 6, 19},  /*99*/
    {70, 2, 3, 6, 19},  /*100*/
    {70, 2, 3, 6, 19},  /*101*/
    {70, 2, 3, 6, 19},  /*101*/
    {70, 2, 3, 6, 19},  /*102*/
    {70, 2, 3, 6, 19},  /*103*/
    {70, 2, 3, 6, 19},  /*104*/
    {70, 2, 3, 6, 19},  /*105*/
    {70, 2, 3, 6, 19},  /*106*/
    {70, 2, 3, 6, 19},  /*107*/
    {70, 2, 3, 6, 19},  /*108*/
    {70, 2, 3, 6, 19},  /*109*/
    {70, 2, 3, 6, 19},  /*110*/
    {70, 2, 3, 6, 19},  /*111*/
    {70, 2, 3, 6, 19},  /*112*/
    {70, 2, 3, 6, 19},  /*113*/
    {70, 2, 3, 6, 19},  /*114*/
    {70, 2, 3, 6, 19},  /*115*/
    {70, 2, 3, 6, 19},  /*116*/
    {70, 2, 3, 6, 19},  /*117*/
    {70, 2, 3, 6, 19},  /*118*/
    {70, 2, 3, 6, 19},  /*119*/
    {70, 2, 3, 6, 19},  /*120*/
    {70, 2, 3, 6, 19},  /*121*/
    {70, 2, 3, 6, 19},  /*122*/
    {70, 2, 3, 6, 19},  /*123*/
    {70, 2, 3, 6, 19},  /*124*/
    {70, 2, 3, 6, 19},  /*125*/
    {70, 2, 3, 6, 19},  /*126*/
    {70, 2, 3, 6, 19},  /*127*/
    {70, 2, 3, 6, 19},  /*128*/
    {70, 2, 3, 6, 19},  /*129*/
    {70, 2, 3, 6, 19},  /*130*/
    {70, 2, 3, 6, 19},  /*131*/
    {70, 2, 3, 6, 19},  /*132*/
    {70, 2, 3, 6, 19},  /*133*/
    {70, 2, 3, 6, 19},  /*134*/
    {70, 2, 3, 6, 19},  /*135*/
    {70, 2, 3, 6, 19},  /*136*/
    {70, 2, 3, 6, 19},  /*137*/
    {70, 2, 3, 6, 19},  /*138*/
    {70, 2, 3, 6, 19},  /*139*/
    {70, 2, 3, 6, 19},  /*140*/
    {70, 2, 3, 6, 19},  /*141*/
    {70, 2, 3, 6, 19},  /*142*/
    {70, 2, 3, 6, 19},  /*143*/
    {70, 2, 3, 6, 19},  /*144*/
    {70, 2, 3, 6, 19},  /*145*/
    {70, 2, 3, 6, 19},  /*146*/
    {70, 2, 3, 6, 19},  /*147*/
    {70, 2, 3, 6, 19},  /*148*/
    {70, 2, 3, 6, 19},  /*149*/
    {70, 2, 3, 6, 19},  /*150*/
    {70, 2, 3, 6, 19},  /*151*/
    {70, 2, 3, 6, 19},  /*152*/
    {70, 2, 3, 6, 19},  /*153*/
    {70, 2, 3, 6, 19},  /*154*/
    {70, 2, 3, 6, 19},  /*155*/
    {70, 2, 3, 6, 19},  /*156*/
    {70, 2, 3, 6, 19},  /*157*/
    {70, 2, 3, 6, 19},  /*158*/
    {70, 2, 3, 6, 19},  /*159*/
    {70, 2, 3, 6, 19},  /*160*/
    {70, 2, 3, 6, 19},  /*161*/
    {70, 2, 3, 6, 19},  /*162*/
    {70, 2, 3, 6, 19},  /*163*/
    {70, 2, 3, 6, 19},  /*164*/
    {70, 2, 3, 6, 19},  /*165*/
    {70, 2, 3, 6, 19},  /*166*/
    {70, 2, 3, 6, 19},  /*167*/
    {70, 2, 3, 6, 19},  /*168*/
    {70, 2, 3, 6, 19},  /*169*/
    {70, 2, 3, 6, 19},  /*170*/
    {70, 2, 3, 6, 19},  /*171*/
    {70, 2, 3, 6, 19},  /*172*/
    {70, 2, 3, 6, 19},  /*173*/
    {70, 2, 3, 6, 19},  /*174*/
    {70, 2, 3, 6, 19},  /*175*/
    {70, 2, 3, 6, 19},  /*176*/
    {70, 2, 3, 6, 19},  /*177*/
    {70, 2, 3, 6, 19},  /*178*/
    {70, 2, 3, 6, 19},  /*179*/
    {70, 2, 3, 6, 19},  /*180*/
    {70, 2, 3, 6, 19},  /*181*/
    {70, 2, 3, 6, 19},  /*182*/
    {70, 2, 3, 6, 19},  /*183*/
    {70, 2, 3, 6, 19},  /*184*/
    {70, 2, 3, 6, 19},  /*185*/
    {70, 2, 3, 6, 19},  /*186*/
    {70, 2, 3, 6, 19},  /*187*/
    {70, 2, 3, 6, 19},  /*188*/
    {70, 2, 3, 6, 19},  /*189*/
    {70, 2, 3, 6, 19},  /*190*/
    {70, 2, 3, 6, 19},  /*191*/
    {70, 2, 3, 6, 19},  /*192*/
    {70, 2, 3, 6, 19},  /*193*/
    {70, 2, 3, 6, 19},  /*194*/
    {70, 2, 3, 6, 19},  /*195*/
    {70, 2, 3, 6, 19},  /*196*/
    {70, 2, 3, 6, 19},  /*197*/
    {70, 2, 3, 6, 19},  /*198*/
    {70, 2, 3, 6, 19},  /*199*/
    {70, 2, 3, 6, 19},  /*200*/
};

/** Pointers to coin archetypes. */
struct archetype *coins_arch[NUM_COINS];

/** Chance fix. */
#define CHANCE_FIX (-1)

static void free_treasurestruct(treasure_t *t);

/* Function prototypes */
static void
treasure_load_more(treasure_t **treasure,
                   FILE        *fp,
                   const char  *filename,
                   uint64_t    *linenum);
static void
treasure_generate_internal(treasure_list_t     *treasure_list,
                           object              *op,
                           int                  difficulty,
                           int                  flags,
                           treasure_affinity_t *affinity,
                           int                  artifact_chance,
                           int                  tries,
                           treasure_attrs_t    *attrs);

/**
 * Allocate and return a pointer to an empty treasure list structure.
 *
 * @return
 * New structure, never NULL.
 */
static treasure_list_t *
treasure_list_create (void)
{
    treasure_list_t *treasure_list = ecalloc(1, sizeof(*treasure_list));

    treasure_list->artifact_chance = TREASURE_ARTIFACT_CHANCE;
    treasure_list->chance_fix = CHANCE_FIX;
    treasure_list->total_chance = 0;

    return treasure_list;
}

/**
 * Allocate and return a pointer to an empty treasure structure.
 *
 * @return
 * New structure, never NULL.
 */
static treasure_t *
treasure_create (void)
{
    treasure_t *treasure = ecalloc(1, sizeof(*treasure));

    treasure->magic_chance = 3;
    treasure->artifact_chance = TREASURE_ARTIFACT_CHANCE;
    treasure->chance_fix = CHANCE_FIX;
    treasure->chance = 100;

    treasure->attrs.item_race = -1;
    treasure->attrs.material = -1;
    treasure->attrs.material_quality = -1;
    treasure->attrs.material_range = -1;
    treasure->attrs.quality = -1;
    treasure->attrs.quality_range = -1;

    return treasure;
}

/**
 * Reads one treasure from the file, including the 'yes', 'no' and 'more'
 * options.
 *
 * @param fp
 * File to read from.
 * @param filename
 * Filename.
 * @param[out] linenum
 * Line number.
 * @param[out] affinity
 * Treasure affinity.
 * @param[out] artifact_chance
 * Artifact chance.
 * @return
 * Read structure, never NULL.
 */
static treasure_t *
treasure_load_one (FILE                 *fp,
                   const char           *filename,
                   uint64_t             *linenum,
                   treasure_affinity_t **affinity,
                   int                  *artifact_chance)
{
    HARD_ASSERT(fp != NULL);
    HARD_ASSERT(filename != NULL);
    HARD_ASSERT(linenum != NULL);
    HARD_ASSERT(affinity != NULL);
    HARD_ASSERT(artifact_chance != NULL);

    bool start_marker = false;
    treasure_t *treasure = treasure_create();

    char buf[MAX_BUF];
    while (fgets(VS(buf), fp) != NULL) {
        (*linenum)++;

        char *cp = buf;
        string_skip_whitespace(cp);
        string_strip_newline(cp);

        char *cps[2];
        if (string_split(cp, cps, arraysize(cps), ' ') < 1) {
            continue;
        }

        const char *key = cps[0], *value = cps[1], *error_str = NULL;

        if (value == NULL) {
            if (strcmp(key, "yes") == 0) {
                treasure_load_more(&treasure->next_yes, fp, filename, linenum);
            } else if (strcmp(key, "no") == 0) {
                treasure_load_more(&treasure->next_no, fp, filename, linenum);
            } else if (strcmp(key, "more") == 0) {
                treasure_load_more(&treasure->next, fp, filename, linenum);
                return treasure;
            } else if (strcmp(key, "end") == 0) {
                return treasure;
            } else {
                error_str = "unrecognized attribute";
                goto error;
            }
        } else if (strcmp(key, "affinity") == 0) {
            if (start_marker) {
                FREE_AND_COPY_HASH(treasure->affinity, value);
            } else {
                FREE_AND_COPY_HASH(*affinity, value);
            }
        } else if (strcmp(key, "artifact_chance") == 0) {
            if (!string_isdigit(value)) {
                error_str = "artifact_chance attribute expects a number";
                goto error;
            }

            if (start_marker) {
                treasure->artifact_chance = atoi(value);
            } else {
                *artifact_chance = atoi(value);
            }
        } else if (strcmp(key, "arch") == 0) {
            treasure->item = arch_find(value);
            if (treasure->item == NULL) {
                LOG(ERROR, "Treasure lacks archetype: %s", value);
                exit(1);
            }

            start_marker = true;
        } else if (strcmp(key, "list") == 0) {
            start_marker = true;
            FREE_AND_COPY_HASH(treasure->name, value);
        } else if (strcmp(key, "name") == 0) {
            FREE_AND_COPY_HASH(treasure->attrs.name, value);
        } else if (strcmp(key, "title") == 0) {
            FREE_AND_COPY_HASH(treasure->attrs.title, value);
        } else if (strcmp(key, "slaying") == 0) {
            FREE_AND_COPY_HASH(treasure->attrs.slaying, value);
        } else if (strcmp(key, "item_race") == 0) {
            if (!string_isdigit(value)) {
                error_str = "item_race attribute expects a number";
                goto error;
            }

            treasure->attrs.item_race = atoi(value);
        } else if (strcmp(key, "quality") == 0) {
            if (!string_isdigit(value)) {
                error_str = "quality attribute expects a number";
                goto error;
            }

            treasure->attrs.quality = atoi(value);
        } else if (strcmp(key, "quality_range") == 0) {
            if (!string_isdigit(value)) {
                error_str = "quality_range attribute expects a number";
                goto error;
            }

            treasure->attrs.quality_range = atoi(value);
        } else if (strcmp(key, "material") == 0) {
            if (!string_isdigit(value)) {
                error_str = "material attribute expects a number";
                goto error;
            }

            treasure->attrs.material = atoi(value);
        } else if (strcmp(key, "material_quality") == 0) {
            if (!string_isdigit(value)) {
                error_str = "material_quality attribute expects a number";
                goto error;
            }

            treasure->attrs.material_quality = atoi(value);
        } else if (strcmp(key, "material_range") == 0) {
            if (!string_isdigit(value)) {
                error_str = "material_range attribute expects a number";
                goto error;
            }

            treasure->attrs.material_range = atoi(value);
        } else if (strcmp(key, "chance_fix") == 0) {
            int val = atoi(value);
            if (val < 0 || val > INT16_MAX) {
                LOG(ERROR, "Value out of range: %s %s", key, value);
                exit(1);
            }

            treasure->chance_fix = val;
            treasure->chance = 0;
        } else if (strcmp(key, "chance") == 0) {
            if (!string_isdigit(value)) {
                error_str = "chance attribute expects a number";
                goto error;
            }

            int val = atoi(value);
            if (val < 0 || val > 100) {
                LOG(ERROR, "Value out of range: %s %s", key, value);
                exit(1);
            }

            treasure->chance = val;
        } else if (strcmp(key, "nrof") == 0) {
            if (!string_isdigit(value)) {
                error_str = "nrof attribute expects a number";
                goto error;
            }

            treasure->nrof = atoi(value);
        } else if (strcmp(key, "magic") == 0) {
            if (!string_isdigit(value)) {
                error_str = "magic attribute expects a number";
                goto error;
            }

            treasure->magic = atoi(value);
        } else if (strcmp(key, "magic_fix") == 0) {
            if (!string_isdigit(value)) {
                error_str = "magic_fix attribute expects a number";
                goto error;
            }

            treasure->magic_fix = atoi(value);
        } else if (strcmp(key, "magic_chance") == 0) {
            if (!string_isdigit(value)) {
                error_str = "magic_chance attribute expects a number";
                goto error;
            }

            treasure->magic_chance = atoi(value);
        } else if (strcmp(key, "difficulty") == 0) {
            if (!string_isdigit(value)) {
                error_str = "difficulty attribute expects a number";
                goto error;
            }

            treasure->difficulty = atoi(value);
        } else {
            error_str = "unrecognized attribute";
            goto error;
        }

        continue;
error:
        LOG(ERROR,
            "Error parsing %s, line %" PRIu64 ", %s: %s %s",
            filename,
            *linenum,
            error_str != NULL ? error_str : "",
            key != NULL ? key : "",
            value != NULL ? value : "");
        exit(1);
    }

    LOG(ERROR, "Last treasure list lacks 'end'.");
    exit(1);

    /* Unreachable. */
    return NULL;
}

/**
 * Load more/yes/no section of a treasure list.
 *
 * @param[out] treasure
 * Where to load teasure into.
 * @param filename
 * Filename.
 * @param fp
 * File to read from.
 * @param[out] linenum
 * Line number.
 */
static void
treasure_load_more (treasure_t **treasure,
                    FILE        *fp,
                    const char  *filename,
                    uint64_t    *linenum)
{
    HARD_ASSERT(treasure != NULL);
    HARD_ASSERT(fp != NULL);
    HARD_ASSERT(filename != NULL);
    HARD_ASSERT(linenum != NULL);

    treasure_affinity_t *affinity = NULL;
    int artifact_chance = TREASURE_ARTIFACT_CHANCE;

    *treasure = treasure_load_one(fp,
                                  filename,
                                  linenum,
                                  &affinity,
                                  &artifact_chance);

    if ((*treasure)->artifact_chance == TREASURE_ARTIFACT_CHANCE) {
        (*treasure)->artifact_chance = artifact_chance;
    }

    if ((*treasure)->affinity == NULL) {
        (*treasure)->affinity = affinity;
    } else if (affinity != NULL) {
        free_string_shared(affinity);
    }
}

/**
 * Opens the collected treasures and reads all treasure declarations from it.
 *
 * Each treasure is parsed with the help of treasure_load_one().
 */
static void
treasure_load (void)
{
    char filename[HUGE_BUF];
    snprintf(VS(filename), "%s/treasures", settings.libpath);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        LOG(ERROR, "Can't open treasures file: %s", filename);
        exit(1);
    }

    uint64_t linenum = 0;
    char buf[MAX_BUF];
    while (fgets(buf, MAX_BUF, fp) != NULL) {
        linenum++;

        char *cp = buf;
        string_skip_whitespace(cp);
        string_strip_newline(cp);

        char *cps[2];
        if (string_split(cp, cps, arraysize(cps), ' ') < 1) {
            continue;
        }

        const char *key = cps[0], *value = cps[1], *error_str = NULL;

        if (strcmp(key, "treasureone") == 0 ||
            strcmp(key, "treasure") == 0) {
            treasure_list_t *treasure_list = treasure_list_create();
            FREE_AND_COPY_HASH(treasure_list->name, value);

            /* Add it to the linked list of treasures. */
            treasure_list->next = first_treasurelist;
            first_treasurelist = treasure_list;

            treasure_affinity_t *affinity = NULL;
            int artifact_chance = TREASURE_ARTIFACT_CHANCE;
            treasure_list->items = treasure_load_one(fp,
                                                     filename,
                                                     &linenum,
                                                     &affinity,
                                                     &artifact_chance);

            if (treasure_list->artifact_chance == TREASURE_ARTIFACT_CHANCE) {
                treasure_list->artifact_chance = artifact_chance;
            }

            if (treasure_list->affinity == NULL) {
                treasure_list->affinity = affinity;
            } else if (affinity != NULL) {
                free_string_shared(affinity);
            }

           if (strcmp(key, "treasureone") == 0) {
                for (treasure_t *treasure = treasure_list->items;
                     treasure != NULL;
                     treasure = treasure->next) {
                    if (treasure->next_yes || treasure->next_no) {
                        LOG(ERROR,
                            "Treasure %s is one item, but on treasure %s "
                            "the next_yes or next_no field is set",
                            treasure_list->name,
                            treasure->item != NULL ? treasure->item->name :
                                treasure->name);
                        exit(1);
                    }

                    treasure_list->total_chance += treasure->chance;
                }
            }
        } else {
            error_str = "unrecognized attribute";
            goto error;
        }

        continue;
error:
        LOG(ERROR,
            "Error parsing %s, line %" PRIu64 ", %s: %s %s",
            filename,
            linenum,
            error_str != NULL ? error_str : "",
            key != NULL ? key : "",
            value != NULL ? value : "");
        exit(1);
    }

    fclose(fp);
}

/**
 * Create money table, setting up pointers to the archetypes.
 *
 * This is done for faster access to the coin archetypes.
 */
static void
treasure_init_coins (void)
{
    for (int i = 0; coins[i] != NULL; i++) {
        coins_arch[i] = arch_find(coins[i]);
        if (coins_arch[i] == NULL) {
            LOG(ERROR, "Can't find %s.", coins[i]);
            exit(1);
        }
    }
}

/**
 * Checks if a treasure if valid. Will also check its yes and no options.
 *
 * @param t
 * Treasure to check.
 * @param tl
 * Needed only so that the treasure name can be printed out.
 */
static void
treasure_list_check (treasure_t *t, treasure_list_t *tl)
{
    HARD_ASSERT(t != NULL);
    HARD_ASSERT(tl != NULL);

    if (t->item == NULL && t->name == NULL) {
        LOG(ERROR,
            "Treasurelist %s has element with no name or archetype",
            tl->name);
        exit(1);
    }

    if (t->chance >= 100 &&
        t->next_yes != NULL &&
        (t->next != NULL || t->next_no != NULL)) {
        LOG(ERROR,
            "Treasurelist %s has element that has 100%% generation, "
            "next_yes field as well as next or next_no",
            tl->name);
    }

    if (t->name != NULL && t->name != shstr_cons.NONE) {
        /* treasure_list_find will drop an error message if the treasure list
         * cannot be found. */
        (void) treasure_list_find(t->name);
    }

    if (t->next != NULL) {
        treasure_list_check(t->next, tl);
    }

    if (t->next_yes != NULL) {
        treasure_list_check(t->next_yes, tl);
    }

    if (t->next_no != NULL) {
        treasure_list_check(t->next_no, tl);
    }
}

/**
 * Initialize the treasure sub-system.
 */
void
treasure_init (void)
{
    treasure_load();
    treasure_init_coins();

    /* Perform some checks on how valid the treasure data actually is.
     * Verify that list transitions work (ie, the list that it is
     * supposed to transition to exists). Also, verify that at least the
     * name or archetype is set for each treasure element. */
    for (treasure_list_t *treasure = first_treasurelist;
         treasure != NULL;
         treasure = treasure->next) {
        treasure_list_check(treasure->items, treasure);
    }

    for (int i = 0; i < DIFFLEVELS; i++) {
        int sum = 0;
        for (int j = 0; j < MAXMAGIC + 1; j++) {
            sum += difftomagic_list[i][j];
        }

        if (sum != 100) {
            log_error("Incorrect sum: %d", i);
        }
    }
}

/**
 * Searches for the given treasurelist in the globally linked list of
 * treasure lists which has been built by treasure_load().
 *
 * @param name
 * Treasure list name to search for.
 */
treasure_list_t *
treasure_list_find (const char *name)
{
    SOFT_ASSERT_RC(name != NULL, NULL, "name is NULL");

    /* Still initializing the treasure lists, so return NULL for now. */
    if (first_treasurelist == NULL) {
        return NULL;
    }

    shstr *name_sh = find_string(name);
    if (name_sh == NULL) {
        goto out;
    }

    if (name_sh == shstr_cons.none) {
        return NULL;
    }

    for (treasure_list_t *treasure_list = first_treasurelist;
         treasure_list != NULL;
         treasure_list = treasure_list->next) {
        if (name_sh == treasure_list->name) {
            return treasure_list;
        }
    }

out:
    LOG(ERROR, "Couldn't find treasure list: %s", name);
    return NULL;
}

/**
 * Calculate a magic value from the specified difficulty.
 *
 * @param difficulty
 *
 * @return
 * Magic value.
 */
static int
magic_from_difficulty (int difficulty)
{
    difficulty--;

    if (difficulty < 0) {
        difficulty = 0;
    } else if (difficulty >= DIFFLEVELS) {
        difficulty = DIFFLEVELS - 1;
    }

    int roll = rndm(0, 99);

    int magic;
    for (magic = 0; magic < MAXMAGIC + 1; magic++) {
        roll -= difftomagic_list[difficulty][magic];
        if (roll < 0) {
            break;
        }
    }

    if (magic == MAXMAGIC + 1) {
        log_error("Table for difficulty %d bad", difficulty);
        magic = 0;
    }

    return rndm_chance(20) ? -magic : magic;
}

/**
 * Sets magical bonus in an object, and recalculates the effect on the
 * armour variable, and the effect on speed of armour.
 *
 * This function doesn't work properly, should add use of archetypes to
 * make it truly absolute.
 * @param op
 * Object we're modifying.
 * @param magic
 * Magic modifier.
 */
static void
treasure_set_magical_bonus (object *op, int magic)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(magic != 0);

    SET_FLAG(op, FLAG_IS_MAGICAL);
    op->magic = magic;

    if (magic < 0) {
        SET_FLAG(op, FLAG_CURSED);
    }

    if (op->arch != NULL) {
        if (magic == 1) {
            op->value += 5300;
        } else if (magic == 2) {
            op->value += 12300;
        } else if (magic == 3) {
            op->value += 24300;
        } else if (magic == 4) {
            op->value += 52300;
        } else {
            op->value += 88300;
        }

        if (op->type == ARMOUR) {
            ARMOUR_SPEED(op) = (ARMOUR_SPEED(&op->arch->clone) *
                                (100 + magic * 10)) / 100;
        }

        if (magic < 0 && rndm_chance(3)) {
            magic = -magic;
        }

        op->weight = (op->arch->clone.weight * (100 - magic * 10)) / 100;
    } else {
        if (op->type == ARMOUR) {
            ARMOUR_SPEED(op) = (ARMOUR_SPEED(op) * (100 + magic * 10)) / 100;
        }

        if (magic < 0 && rndm_chance(3)) {
            magic = -magic;
        }

        op->weight = (op->weight * (100 - magic * 10)) / 100;
    }
}

/**
 * Sets a random magical bonus in the given object based upon the given
 * difficulty, and the given max possible bonus.
 *
 * Item will be cursed if magic is negative.
 *
 * @param op
 * The object.
 * @param difficulty
 * Difficulty we want the item to be.
 * @param max_magic
 * What should be the maximum magic of the item.
 * @param fixed_magic
 * Fixed value of magic for the object.
 * @param chance_magic
 * Chance to get a magic bonus.
 * @param flags
 * Combination of @ref GT_xxx flags.
 */
static void
treasure_set_magic (object *op,
                    int     difficulty,
                    int     max_magic,
                    int     fixed_magic,
                    int     chance_magic,
                    int     flags)
{
    int magic;

    HARD_ASSERT(op != NULL);
    HARD_ASSERT(difficulty > 0);

    /* If we have a fixed value, force it */
    if (fixed_magic) {
        magic = fixed_magic;
    } else {
        magic = magic_from_difficulty(difficulty);
        if (magic > max_magic) {
            magic = max_magic;
        }
    }

    if ((flags & GT_ONLY_GOOD) && magic < 0) {
        return;
    }

    if (magic != 0) {
        treasure_set_magical_bonus(op, magic);
    }
}

/**
 * Inserts generated treasure where it should go.
 *
 * @param op
 * Treasure just generated.
 * @param creator
 * For which object the treasure is being generated.
 * @param flags
 * Combination of @ref GT_xxx values.
 */
static void
treasure_insert (object *op, object *creator, int flags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(creator != NULL);

    if (flags & GT_ENVIRONMENT) {
        op->x = creator->x;
        op->y = creator->y;
        object_insert_map(op, creator->map, op, INS_NO_MERGE | INS_NO_WALK_ON);
    } else {
        object_insert_into(op, creator, 0);
    }
}

/**
 * Apply treasure attributes to the specified object.
 *
 * @param op
 * Generated treasure object.
 * @param attrs
 * Attributes to apply.
 */
static void
treasure_apply_attrs (object *op, treasure_attrs_t *attrs)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(attrs != NULL);

    if (attrs->name != NULL) {
        FREE_AND_COPY_HASH(op->name, attrs->name);
    }

    if (attrs->title != NULL) {
        FREE_AND_COPY_HASH(op->title, attrs->title);
    }

    if (attrs->slaying != NULL) {
        FREE_AND_COPY_HASH(op->slaying, attrs->slaying);
    }
}

/**
 * Set treasure object's object::item_quality.
 *
 * @param op
 * Object to set item_quality for.
 * @param attrs
 * Treasure attributes.
 */
static void
treasure_set_quality (object *op, treasure_attrs_t *attrs)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(attrs != NULL);

    if (attrs->quality != -1) {
        op->item_quality = attrs->quality;
    } else {
        op->item_quality = materials_real[op->material_real].quality;
    }

    if (attrs->quality_range > 0) {
        op->item_quality += rndm(0, attrs->quality_range);

        if (op->item_quality > 100) {
            op->item_quality = 100;
        }
    }

    op->item_condition = op->item_quality;
}

/**
 * Set treasure object's object::material_real.
 *
 * @param op
 * Object to set material_real for.
 * @param attrs
 * Treasure attributes.
 */
static void
treasure_set_material (object *op, treasure_attrs_t *attrs)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(attrs != NULL);

    if (op->type == MONEY) {
        return;
    }

    if (attrs->item_race != -1) {
        op->item_race = (uint8_t) attrs->item_race;
    }

    if (op->material_real == -1) {
        op->material_real = 0;
        return;
    }

    if (attrs->material != -1) {
        op->material_real = attrs->material;

        if (attrs->material_range > 0 && attrs->material != 0) {
            op->material_real += rndm(0, attrs->material_range);
        }
    } else if (op->material_real == 0 && op->material != M_NONE) {
        if (op->material & M_PAPER) {
            op->material_real = M_START_PAPER;
        } else if (op->material & M_IRON) {
            op->material_real = M_START_IRON;
        } else if (op->material & M_GLASS) {
            op->material_real = M_START_GLASS;
        } else if (op->material & M_LEATHER) {
            op->material_real = M_START_LEATHER;
        } else if (op->material & M_WOOD) {
            op->material_real = M_START_WOOD;
        } else if (op->material & M_ORGANIC) {
            op->material_real = M_START_ORGANIC;
        } else if (op->material & M_STONE) {
            op->material_real = M_START_STONE;
        } else if (op->material & M_CLOTH) {
            op->material_real = M_START_CLOTH;
        } else if (op->material & M_ADAMANT) {
            op->material_real = M_START_ADAMANT;
        } else if (op->material & M_LIQUID) {
            op->material_real = M_START_LIQUID;
        } else if (op->material & M_SOFT_METAL) {
            op->material_real = M_START_SOFT_METAL;
        } else if (op->material & M_BONE) {
            op->material_real = M_START_BONE;
        } else if (op->material & M_ICE) {
            op->material_real = M_START_ICE;
        }
    }

    if (attrs->material_quality != -1) {
        int best_material = -1;
        int material_quality = attrs->material_quality;

        /* Increase the material quality if there's a range. */
        if (attrs->material_range > 0) {
            material_quality += rndm(0, attrs->material_range);
        }

        if (op->material_real != 0) {
            int material_tmp = op->material_real / NROFMATERIALS_REAL;

            /* The first entry of the material_real of material table */
            material_tmp = material_tmp * 64 + 1;

            for (int i = 0; i < NROFMATERIALS_REAL; i++) {
                if (materials_real[material_tmp + i].quality ==
                        material_quality) {
                    op->material_real = material_tmp + i;
                    treasure_set_quality(op, attrs);
                    return;
                }

                if (materials_real[material_tmp + i].quality >=
                        attrs->material_quality &&
                    materials_real[material_tmp + i].quality <=
                        material_quality &&
                    materials_real[material_tmp + i].quality >
                        best_material) {
                    best_material = material_tmp + i;
                }
            }

            if (best_material == -1) {
                op->material_real = material_tmp;
                op->item_quality = attrs->material_quality;
                op->item_condition = op->item_quality;
                return;
            }

            /* That's now our best match! */
            op->material_real = best_material;
        } else {
            op->item_quality = material_quality;
            op->item_condition = op->item_quality;
            return;
        }
    }

    treasure_set_quality(op, attrs);
}

/**
 * This is called after an item is generated, in order to set it up right.
 *
 * This produces magical bonuses, puts spells into scrolls/books/wands,
 * makes it unidentified, generates artifacts, etc.
 *
 * @param op
 * Generated treasure object.
 * @param creator
 * For who op was created. Can be NULL.
 * @param difficulty
 * Difficulty level.
 * @param flags
 * Combination of @ref GT_xxx.
 * @param artifact_chance
 * Artifact chance.
 * @param affinity
 * Treasure affinity.
 * @param max_magic
 * Maximum magic for the item.
 * @param fixed_magic
 * Fixed magic value.
 * @param chance_magic
 * Chance of magic.
 * @param attrs
 * Treasure attributes to apply.
 */
static void
treasure_process_generated (object              *op,
                            object              *creator,
                            int                  difficulty,
                            int                  flags,
                            int                  artifact_chance,
                            treasure_affinity_t *affinity,
                            int                  max_magic,
                            int                  fixed_magic,
                            int                  chance_magic,
                            treasure_attrs_t    *attrs)
{
    HARD_ASSERT(op != NULL);

    /* Safety and to prevent polymorphed objects giving attributes. */
    if (creator == NULL || creator->type == op->type) {
        creator = op;
    }

    if (difficulty < 1) {
        difficulty = 1;
    }

    bool was_magical = op->magic != 0;
    bool generated_artifact = false;

    treasure_set_material(op, attrs);

    if (!OBJECT_METHODS(op->type)->override_treasure_processing) {
        if (creator->type == 0) {
            max_magic /= 2;
        }

        if ((op->magic == 0 && max_magic != 0) || fixed_magic != 0) {
            treasure_set_magic(op,
                               difficulty,
                               max_magic,
                               fixed_magic,
                               chance_magic,
                               flags);
        }

        if (artifact_chance != 0) {
            if ((!was_magical && rndm_chance(CHANCE_FOR_ARTIFACT)) ||
                difficulty >= 999 ||
                rndm(1, 100) <= artifact_chance) {
                generated_artifact = artifact_generate(op,
                                                       difficulty,
                                                       affinity);
            }
        }
    }

    object *new_obj;
    int res = object_process_treasure(op,
                                      &new_obj,
                                      difficulty,
                                      affinity,
                                      flags);
    if (res == OBJECT_METHOD_ERROR) {
        return;
    } else if (res == OBJECT_METHOD_OK) {
        op = new_obj;
    } else if (res == OBJECT_METHOD_UNHANDLED && !generated_artifact) {
        treasure_apply_attrs(op, attrs);
    }

    SOFT_ASSERT(op != NULL, "Object is NULL");

    if ((flags & GT_NO_VALUE) && op->type != MONEY) {
        op->value = 0;
    }

    if (flags & GT_STARTEQUIP) {
        if (op->nrof < 2 &&
            op->type != CONTAINER &&
            op->type != MONEY &&
            !QUERY_FLAG(op, FLAG_IS_THROWN)) {
            SET_FLAG(op, FLAG_STARTEQUIP);
        } else if (op->type != MONEY) {
            op->value = 0;
        }
    }

    if (creator->type == TREASURE) {
        /* If treasure is "identified", created items are too. */
        if (QUERY_FLAG(creator, FLAG_IDENTIFIED)) {
            SET_FLAG(op, FLAG_IDENTIFIED);
        }

        /* Same with the no-pick attribute. */
        if (QUERY_FLAG(creator, FLAG_NO_PICK)) {
            SET_FLAG(op, FLAG_NO_PICK);
        }
    }

    treasure_insert(op, creator, flags);
}

/**
 * Creates all the treasures.
 *
 * @param treasure
 * What to generate.
 * @param op
 * For whom to generate the treasure.
 * @param difficulty
 * Level difficulty.
 * @param flags
 * Combination of @ref GT_xxx values.
 * @param affinity
 * Treasure affinity.
 * @param artifact_chance
 * Artifact chance.
 * @param tries
 * Used to avoid infinite recursion.
 * @param attrs
 * Treasure attributes.
 */
static void
treasure_create_all (treasure_t          *treasure,
                     object              *op,
                     int                  difficulty,
                     int                  flags,
                     treasure_affinity_t *affinity,
                     int                  artifact_chance,
                     int                  tries,
                     treasure_attrs_t    *attrs)
{
    HARD_ASSERT(treasure != NULL);
    HARD_ASSERT(op != NULL);

    object *tmp;

    if (treasure->affinity != NULL) {
        affinity = treasure->affinity;
    }

    if (treasure->artifact_chance != TREASURE_ARTIFACT_CHANCE) {
        artifact_chance = treasure->artifact_chance;
    }

    if ((treasure->chance_fix != CHANCE_FIX && rndm_chance(treasure->chance_fix)) || (int) treasure->chance >= 100 || (rndm(1, 100) < (int) treasure->chance)) {
        if (treasure->name) {
            if (treasure->name != shstr_cons.NONE && difficulty >= treasure->difficulty) {
                treasure_generate_internal(treasure_list_find(treasure->name), op, difficulty, flags, affinity, artifact_chance, tries, attrs ? attrs : &treasure->attrs);
            }
        } else if (difficulty >= treasure->difficulty) {
            if (treasure->item->clone.type != WEALTH) {
                tmp = arch_to_object(treasure->item);

                if (treasure->nrof && tmp->nrof <= 1) {
                    tmp->nrof = rndm(1, treasure->nrof);
                }

                treasure_process_generated(tmp,
                                           op,
                                           difficulty,
                                           flags,
                                           artifact_chance,
                                           affinity,
                                           treasure->magic,
                                           treasure->magic_fix,
                                           treasure->magic_chance,
                                           attrs != NULL ? attrs : &treasure->attrs);
            } else {
                /* We have a wealth object - expand it to real money */

                /* If t->magic is != 0, that's our value - if not use
                 * default setting */
                int i, value = treasure->magic ? treasure->magic : treasure->item->clone.value;

                value *= (difficulty / 2) + 1;

                /* So we have 80% to 120% of the fixed value */
                value = (int) ((float) value * 0.8f + (float) value * (rndm(1, 40) / 100.0f));

                for (i = 0; i < NUM_COINS; i++) {
                    if (value / coins_arch[i]->clone.value > 0) {
                        tmp = object_get();
                        object_copy(tmp, &coins_arch[i]->clone, false);
                        tmp->nrof = value / tmp->value;
                        value -= tmp->nrof * tmp->value;
                        treasure_insert(tmp, op, flags);
                    }
                }
            }
        }

        if (treasure->next_yes != NULL) {
            treasure_create_all(treasure->next_yes, op, difficulty, flags, (treasure->next_yes->affinity == NULL) ? affinity : treasure->next_yes->affinity, artifact_chance, tries, attrs);
        }
    } else if (treasure->next_no != NULL) {
        treasure_create_all(treasure->next_no, op, difficulty, flags, (treasure->next_no->affinity == NULL) ? affinity : treasure->next_no->affinity, artifact_chance, tries, attrs);
    }

    if (treasure->next != NULL) {
        treasure_create_all(treasure->next, op, difficulty, flags, (treasure->next->affinity == NULL) ? affinity : treasure->next->affinity, artifact_chance, tries, attrs);
    }
}

/**
 * Creates one treasure from the list.
 *
 * @param treasure_list
 * What to generate.
 * @param op
 * For whom to generate the treasure.
 * @param difficulty
 * Level difficulty.
 * @param flags
 * Combination of @ref GT_xxx values.
 * @param affinity
 * Treasure affinity.
 * @param artifact_chance
 * Artifact chance.
 * @param tries
 * Used to avoid infinite recursion.
 * @param attrs
 * Treasure attributes.
 * @todo Get rid of the goto.
 */
static void
treasure_create_one (treasure_list_t     *treasure_list,
                     object              *op,
                     int                  difficulty,
                     int                  flags,
                     treasure_affinity_t *affinity,
                     int                  artifact_chance,
                     int                  tries,
                     treasure_attrs_t    *attrs)
{
    HARD_ASSERT(treasure_list != NULL);
    HARD_ASSERT(op != NULL);

    int value, diff_tries = 0;
    treasure_t *t;
    object *tmp;

    if (tries++ > 100) {
        return;
    }

    /* Well, at some point we should rework this whole system... */
create_one_treasure_again_jmp:

    if (diff_tries > 10) {
        return;
    }

    value = rndm(1, treasure_list->total_chance) - 1;

    for (t = treasure_list->items; t != NULL; t = t->next) {
        /* chance_fix will overrule the normal chance stuff!. */
        if (t->chance_fix != CHANCE_FIX) {
            if (rndm_chance(t->chance_fix)) {
                /* Only when allowed, we go on! */
                if (difficulty >= t->difficulty) {
                    value = 0;
                    break;
                }

                /* Ok, difficulty is bad let's try again or break! */
                if (tries++ > 100) {
                    return;
                }

                diff_tries++;
                goto create_one_treasure_again_jmp;
            }

            if (!t->chance) {
                continue;
            }
        }

        value -= t->chance;

        /* We got one! */
        if (value <= 0) {
            /* Only when allowed, we go on! */
            if (difficulty >= t->difficulty) {
                break;
            }

            /* Ok, difficulty is bad let's try again or break! */
            if (tries++ > 100) {
                return;
            }

            diff_tries++;
            goto create_one_treasure_again_jmp;
        }
    }

    if (!t || value > 0) {
        LOG(BUG, "create_one_treasure: got null object or not able to find treasure - tl:%s op:%s", treasure_list ? treasure_list->name : "(null)", op ? op->name : "(null)");
        return;
    }

    if (t->affinity != NULL) {
        affinity = t->affinity;
    }

    if (t->artifact_chance != TREASURE_ARTIFACT_CHANCE) {
        artifact_chance = t->artifact_chance;
    }

    if (t->name) {
        if (t->name == shstr_cons.NONE) {
            return;
        }

        if (difficulty >= t->difficulty) {
            treasure_generate_internal(treasure_list_find(t->name), op, difficulty, flags, affinity, artifact_chance, tries, attrs);
        } else if (t->nrof) {
            treasure_create_one(treasure_list, op, difficulty, flags, affinity, artifact_chance, tries, attrs);
        }

        return;
    }

    if (t->item->clone.type != WEALTH) {
        tmp = arch_to_object(t->item);

        if (t->nrof && tmp->nrof <= 1) {
            tmp->nrof = rndm(1, t->nrof);
        }

        treasure_process_generated(tmp,
                                   op,
                                   difficulty,
                                   flags,
                                   artifact_chance,
                                   (t->affinity == NULL) ? affinity : t->affinity,
                                   t->magic,
                                   t->magic_fix,
                                   t->magic_chance,
                                   attrs != NULL ? attrs : &t->attrs);
    } else {
        /* We have a wealth object - expand it to real money */

        /* If t->magic is != 0, that's our value - if not use default
         * setting */
        int i;

        value = t->magic ? t->magic : t->item->clone.value;
        value *= difficulty;

        /* So we have 80% to 120% of the fixed value */
        value = (int) ((float) value * 0.8f + (float) value * (rndm(1, 40) / 100.0f));

        for (i = 0; i < NUM_COINS; i++) {
            if (value / coins_arch[i]->clone.value > 0) {
                tmp = object_get();
                object_copy(tmp, &coins_arch[i]->clone, false);
                tmp->nrof = value / tmp->value;
                value -= tmp->nrof * tmp->value;
                treasure_insert(tmp, op, flags);
            }
        }
    }
}

/**
 * Generate treasure inside the specified object based on the given
 * treasure list.
 *
 * @param treasure_list
 * What to generate.
 * @param op
 * For whom to generate the treasure.
 * @param difficulty
 * Level difficulty.
 * @param flags
 * Combination of @ref GT_xxx values.
 * @param affinity
 * Treasure affinity.
 * @param artifact_chance
 * Artifact chance.
 * @param tries
 * To avoid infinite recursion.
 * @param attrs
 * Treasure attributes.
 */
static void
treasure_generate_internal (treasure_list_t     *treasure_list,
                            object              *op,
                            int                  difficulty,
                            int                  flags,
                            treasure_affinity_t *affinity,
                            int                  artifact_chance,
                            int                  tries,
                            treasure_attrs_t    *attrs)
{
    HARD_ASSERT(treasure_list != NULL);
    HARD_ASSERT(op != NULL);

    if (tries++ > 100) {
        LOG(ERROR,
            "Tries reached maximum for treasure list %s.",
            treasure_list->name);
        return;
    }

    if (treasure_list->affinity != NULL) {
        affinity = treasure_list->affinity;
    }

    if (treasure_list->artifact_chance != TREASURE_ARTIFACT_CHANCE) {
        artifact_chance = treasure_list->artifact_chance;
    }

    if (treasure_list->total_chance != 0) {
        treasure_create_one(treasure_list,
                            op,
                            difficulty,
                            flags,
                            affinity,
                            artifact_chance,
                            tries,
                            attrs);
    } else {
        treasure_create_all(treasure_list->items,
                            op,
                            difficulty,
                            flags,
                            affinity,
                            artifact_chance,
                            tries,
                            attrs);
    }
}

/**
 * Generate treasure inside the specified object based on the given
 * treasure list.
 *
 * @param treasure_list
 * What to generate.
 * @param op
 * For whom to generate the treasure.
 * @param difficulty
 * Level difficulty.
 * @param flags
 * Combination of @ref GT_xxx values.
 */
void
treasure_generate (treasure_list_t *treasure_list,
                   object          *op,
                   int              difficulty,
                   int              flags)
{
    SOFT_ASSERT(treasure_list != NULL, "NULL treasure list");
    SOFT_ASSERT(op != NULL, "NULL object");

    treasure_generate_internal(treasure_list,
                               op,
                               difficulty,
                               flags,
                               NULL,
                               TREASURE_ARTIFACT_CHANCE,
                               0,
                               NULL);
}

/**
 * Generate a single object from the specified treasure list.
 *
 * @param treasure_list
 * Treasure list to generate from.
 * @param difficulty
 * Treasure difficulty.
 * @param artifact_chance
 * Chance to generate an artifact.
 * @return
 * Generated treasure. Can be NULL if no suitable treasure was found.
 */
object *
treasure_generate_single (treasure_list_t *treasure_list,
                          int              difficulty,
                          int              artifact_chance)
{
    SOFT_ASSERT_RC(treasure_list != NULL,
                   NULL,
                   "NULL treasure list");

    object *op = object_get();
    treasure_generate_internal(treasure_list,
                               op,
                               difficulty,
                               0,
                               treasure_list->affinity,
                               artifact_chance,
                               0,
                               NULL);

    object *tmp = op->inv;
    if (tmp != NULL) {
        object_remove(tmp, 0);
    }

    if (op->inv != NULL) {
        LOG(ERROR,
            "Created multiple objects, treasure list: %s",
            treasure_list->name);
    }

    object_destroy(op);

    return tmp;
}

/**
 * Frees a treasure, including its yes, no and next items.
 * @param t
 * Treasure to free. Pointer is efree()d too, so becomes
 * invalid.
 */
static void free_treasurestruct(treasure_t *t)
{
    if (t->next) {
        free_treasurestruct(t->next);
    }

    if (t->next_yes) {
        free_treasurestruct(t->next_yes);
    }

    if (t->next_no) {
        free_treasurestruct(t->next_no);
    }

    FREE_AND_CLEAR_HASH2(t->name);
    FREE_AND_CLEAR_HASH2(t->affinity);
    FREE_AND_CLEAR_HASH2(t->attrs.name);
    FREE_AND_CLEAR_HASH2(t->attrs.slaying);
    FREE_AND_CLEAR_HASH2(t->attrs.title);
    efree(t);
}

/**
 * Free all treasure related memory.
 */
void free_all_treasures(void)
{
    treasure_list_t *tl, *next;

    for (tl = first_treasurelist; tl; tl = next) {
        next = tl->next;
        FREE_AND_CLEAR_HASH2(tl->name);
        FREE_AND_CLEAR_HASH2(tl->affinity);

        if (tl->items) {
            free_treasurestruct(tl->items);
        }

        efree(tl);
    }
}

/**
 * Gets the environment level for treasure generation for the given
 * object.
 * @param op
 * Object to get environment level of.
 * @return
 * The environment level, always at least 1.
 */
int get_environment_level(object *op)
{
    object *env;

    if (!op) {
        return 1;
    }

    /* Return object level or map level... */
    if (op->level) {
        return op->level;
    }

    if (op->map) {
        return op->map->difficulty ? op->map->difficulty : 1;
    }

    /* Let's check for env */
    env = op->env;

    while (env) {
        if (env->level) {
            return env->level;
        }

        if (env->map) {
            return env->map->difficulty ? env->map->difficulty : 1;
        }

        env = env->env;
    }

    return 1;
}
