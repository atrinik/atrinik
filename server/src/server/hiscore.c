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
 * Includes high score related functions.
 */

#include <global.h>
#include <toolkit_string.h>
#include <player.h>
#include <object.h>

/**
 * The score structure is used when treating new high-scores
 */
typedef struct scr {
    /** Name. */
    char name[BIG_NAME];

    /** Title. */
    char title[BIG_NAME];

    /** Name (+ title) or "left". */
    char killer[BIG_NAME];

    /** Killed on what level. */
    char maplevel[MAX_BUF];

    /** Experience. */
    uint64_t exp;

    /** Max hp, sp when killed. */
    int maxhp, maxsp;

    /** Position in the highscore list. */
    int position;
} score;

/**
 * The highscore table.
 */
typedef struct {
    /** Filename of the backing file. */
    char fname[MAX_BUF];

    /** The entries in decreasing exp order. */
    score entry[HIGHSCORE_LENGTH];
} score_table;

/**
 * The highscore table. Unused entries are set to zero (except for position).
 */
static score_table hiscore_table;

/**
 * Writes the given score structure to the given buffer.
 * @param sc
 * Score.
 * @param buf
 * The buffer.
 * @param size
 * Size of the buffer.
 */
static void put_score(const score *sc, char *buf, int size)
{
    snprintf(buf, size, "%s:%s:%"PRIu64 ":%s:%s:%d:%d", sc->name, sc->title, sc->exp, sc->killer, sc->maplevel, sc->maxhp, sc->maxsp);
}

/**
 * Saves the highscore_table into the highscore file.
 * @param table
 * The highscore table to save.
 */
static void hiscore_save(const score_table *table)
{
    FILE *fp;
    size_t i;
    char buf[MAX_BUF];

    fp = fopen(table->fname, "w");

    if (!fp) {
        LOG(BUG, "Cannot create highscore file %s: %s", table->fname, strerror(errno));
        return;
    }

    for (i = 0; i < HIGHSCORE_LENGTH; i++) {
        if (table->entry[i].name[0] == '\0') {
            break;
        }

        put_score(&table->entry[i], buf, sizeof(buf));
        fprintf(fp, "%s\n", buf);
    }

    if (ferror(fp)) {
        LOG(BUG, "Cannot write to highscore file %s: %s", table->fname, strerror(errno));
        fclose(fp);
    } else if (fclose(fp) != 0) {
        LOG(BUG, "Cannot write to highscore file %s: %s", table->fname, strerror(errno));
    }
}

/**
 * The opposite of put_score(), get_score reads from the given buffer into
 * a given score structure.
 * @param bp
 * String to parse.
 * @param sc
 * Includes the parsed score.
 * @return
 * Whether parsing was successful.
 */
static int get_score(char *bp, score *sc)
{
    char *cp, *tmp[7];

    cp = strchr(bp, '\n');

    if (cp) {
        *cp = '\0';
    }

    if (string_split(bp, tmp, arraysize(tmp), ':') != arraysize(tmp)) {
        return 0;
    }

    strncpy(sc->name, tmp[0], sizeof(sc->name));
    sc->name[sizeof(sc->name) - 1] = '\0';

    strncpy(sc->title, tmp[1], sizeof(sc->title));
    sc->title[sizeof(sc->title) - 1] = '\0';

    sscanf(tmp[2], "%"PRIu64, &sc->exp);

    strncpy(sc->killer, tmp[3], sizeof(sc->killer));
    sc->killer[sizeof(sc->killer) - 1] = '\0';

    strncpy(sc->maplevel, tmp[4], sizeof(sc->maplevel));
    sc->maplevel[sizeof(sc->maplevel) - 1] = '\0';

    sscanf(tmp[5], "%d", &sc->maxhp);
    sscanf(tmp[6], "%d", &sc->maxsp);
    return 1;
}

/**
 * Formats one score to display to a player.
 * @param sc
 * Score to format.
 * @param buf
 * Buffer to write to. Will contain suitably formatted score.
 * @param size
 * Length of buf.
 * @return
 * buf.
 */
static char *draw_one_high_score(const score *sc, char *buf, size_t size)
{
    if (sc->killer[0] == '\0') {
        snprintf(buf, size, "[green]%3d[/green] %s [green]%s[/green] the %s (%s) <%d><%d>.", sc->position, string_format_number_comma(sc->exp), sc->name, sc->title, sc->maplevel, sc->maxhp, sc->maxsp);
    } else {
        const char *s1, *s2;

        if (!strcmp(sc->killer, "left")) {
            s1 = sc->killer;
            s2 = "the game";
        } else {
            s1 = "was killed by";
            s2 = sc->killer;
        }

        snprintf(buf, size, "[green]%3d[/green] %s [green]%s[/green] the %s %s %s on map %s <%d><%d>.", sc->position, string_format_number_comma(sc->exp), sc->name, sc->title, s1, s2, sc->maplevel, sc->maxhp, sc->maxsp);
    }

    return buf;
}

/**
 * Adds the given score-structure to the high-score list, but only if it
 * was good enough to deserve a place.
 * @param table
 * The highscore table to add to.
 * @param new_score
 * Score to add.
 * @param old_score
 * Returns the old player score.
 */
static void add_score(score_table *table, score *new_score, score *old_score)
{
    size_t i;

    new_score->position = HIGHSCORE_LENGTH + 1;
    memset(old_score, 0, sizeof(*old_score));
    old_score->position = -1;

    /* Find existing entry by name */
    for (i = 0; i < HIGHSCORE_LENGTH; i++) {
        if (table->entry[i].name[0] == '\0') {
            table->entry[i] = *new_score;
            table->entry[i].position = i + 1;
            break;
        }

        if (strcmp(new_score->name, table->entry[i].name) == 0) {
            *old_score = table->entry[i];

            if (table->entry[i].exp <= new_score->exp) {
                table->entry[i] = *new_score;
                table->entry[i].position = i + 1;
            }

            break;
        }
    }

    /* Entry for unknown name */
    if (i >= HIGHSCORE_LENGTH) {
        /* New exp is less than lowest hiscore entry => drop */
        if (new_score->exp < table->entry[i - 1].exp) {
            return;
        }

        /* New exp is not less than lowest hiscore entry => add */
        i--;
        table->entry[i] = *new_score;
        table->entry[i].position = i + 1;
    }

    /* Move entry to correct position */
    while (i > 0 && new_score->exp >= table->entry[i - 1].exp) {
        score tmp;

        tmp = table->entry[i - 1];
        table->entry[i - 1] = table->entry[i];
        table->entry[i] = tmp;

        table->entry[i - 1].position = i;
        table->entry[i].position = i + 1;

        i--;
    }

    new_score->position = table->entry[i].position;
    hiscore_save(table);
}

/**
 * Loads the hiscore_table from the highscore file.
 * @param table
 * The highscore table to load.
 */
static void hiscore_load(score_table *table)
{
    FILE *fp;
    size_t i = 0;

    fp = fopen(table->fname, "r");

    if (fp == NULL) {
        if (errno != ENOENT) {
            LOG(ERROR, "Cannot open highscore file %s: %s", table->fname, strerror(errno));
        }
    } else {
        while (i < HIGHSCORE_LENGTH) {
            char buf[MAX_BUF];

            if (!fgets(buf, sizeof(buf), fp)) {
                break;
            }

            if (!get_score(buf, &table->entry[i])) {
                break;
            }

            table->entry[i].position = i + 1;
            i++;
        }

        fclose(fp);
    }

    while (i < HIGHSCORE_LENGTH) {
        memset(&table->entry[i], 0, sizeof(table->entry[i]));
        table->entry[i].position = i + 1;
        i++;
    }
}

/**
 * Initializes the module.
 */
void hiscore_init(void)
{
    snprintf(hiscore_table.fname, sizeof(hiscore_table.fname), "%s/highscore", settings.datapath);
    hiscore_load(&hiscore_table);
}

/**
 * Checks if player should enter the hiscore, and if so writes them into the
 * list.
 * @param op
 * Player to check.
 * @param quiet
 * If set, don't print anything out - used for periodic updates
 * during
 * game play or when player unexpectedly quits - don't need to print anything in
 * those cases.
 */
void hiscore_check(object *op, int quiet)
{
    score new_score, old_score;
    char bufscore[MAX_BUF];
    const char *message;

    if (!op->stats.exp) {
        return;
    }

    strncpy(new_score.name, op->name, sizeof(new_score.name));
    new_score.name[sizeof(new_score.name) - 1] = '\0';

    strncpy(new_score.title, op->race, sizeof(new_score.title));
    new_score.title[sizeof(new_score.title) - 1] = '\0';

    strncpy(new_score.killer, CONTR(op)->killer, sizeof(new_score.killer));
    new_score.killer[sizeof(new_score.killer) - 1] = '\0';

    new_score.exp = op->stats.exp;

    if (op->map == NULL) {
        *new_score.maplevel = '\0';
    } else {
        size_t i;

        strncpy(new_score.maplevel, op->map->name ? op->map->name : op->map->path, sizeof(new_score.maplevel) - 1);
        new_score.maplevel[sizeof(new_score.maplevel) - 1] = '\0';

        /* Replace ':' in the map name with ' ' so it doesn't break the loading.
         * */
        for (i = 0; new_score.maplevel[i]; i++) {
            if (new_score.maplevel[i] == ':') {
                new_score.maplevel[i] = ' ';
            }
        }
    }

    new_score.maxhp = (int) op->stats.maxhp;
    new_score.maxsp = (int) op->stats.maxsp;
    add_score(&hiscore_table, &new_score, &old_score);

    /* Everything below here is just related to print messages
     * to the player.  If quiet is set, we can just return
     * now. */
    if (quiet) {
        return;
    }

    if (old_score.position == -1) {
        if (new_score.position > HIGHSCORE_LENGTH) {
            message = "You didn't enter the highscore list:";
        } else {
            message = "You entered the highscore list:";
        }
    } else {
        if (new_score.position > HIGHSCORE_LENGTH) {
            message = "You left the highscore list:";
        } else if (new_score.exp  > old_score.exp) {
            message = "You beat your last score:";
        } else {
            message = "You didn't beat your last score:";
        }
    }

    draw_info(COLOR_WHITE, op, message);

    if (old_score.position != -1) {
        draw_info(COLOR_WHITE, op, draw_one_high_score(&old_score, bufscore, sizeof(bufscore)));
    }

    draw_info(COLOR_WHITE, op, draw_one_high_score(&new_score, bufscore, sizeof(bufscore)));
}

/**
 * Displays the high score file.
 * @param op
 * Player asking for the score file.
 * @param max
 * Maximum number of scores to display.
 * @param match
 * If non-empty, will only print players with name or title
 * containing the string (non case-sensitive).
 */
void hiscore_display(object *op, int max, const char *match)
{
    int printed_entries = 0;
    size_t j;

    draw_info(COLOR_WHITE, op, "Nr    Score   Who <max hp><max sp>");

    for (j = 0; j < HIGHSCORE_LENGTH && hiscore_table.entry[j].name[0] != '\0' && printed_entries < max; j++) {
        char scorebuf[MAX_BUF];

        if (match && !strcasestr(hiscore_table.entry[j].name, match) && !strcasestr(hiscore_table.entry[j].title, match)) {
            continue;
        }

        draw_one_high_score(&hiscore_table.entry[j], scorebuf, sizeof(scorebuf));
        printed_entries++;

        draw_info(COLOR_WHITE, op, scorebuf);
    }
}
