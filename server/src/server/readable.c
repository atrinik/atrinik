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
 * This file contains code relevant to allow randomly occurring
 * messages in non-magical texts.
 *
 * @author b.t. thomas@astro.psu.edu
 * @date December 1995 */

#include <global.h>
#include <book.h>
#include <toolkit_string.h>
#include <arch.h>
#include <artifact.h>

/* This flag is useful for debugging archiving action */
/* #define ARCHIVE_DEBUG */

/** special structure, used only by art_name_array[] */
typedef struct namebytype {
    /** Generic name to call artifacts of this type */
    char *name;

    /** Matching type */
    int type;
} arttypename;

/**
 * Array of all the monsters. */
static object **monsters = NULL;
/**
 * Number of the monsters. */
static size_t num_monsters = 0;

/**
 * Array of the messages as read from the messages file. */
static char **msgs = NULL;
/**
 * Number of msgs. */
static size_t num_msgs = 0;

/** Spellpath information */
static uint32_t spellpathdef[NRSPELLPATHS] = {
    PATH_PROT,
    PATH_FIRE,
    PATH_FROST,
    PATH_ELEC,
    PATH_MISSILE,
    PATH_SELF,
    PATH_SUMMON,
    PATH_ABJURE,
    PATH_RESTORE,
    PATH_DETONATE,
    PATH_MIND,
    PATH_CREATE,
    PATH_TELE,
    PATH_INFO,
    PATH_TRANSMUTE,
    PATH_TRANSFER,
    PATH_TURNING,
    PATH_WOUNDING,
    PATH_DEATH,
    PATH_LIGHT
};

/** Path book information */
static char *path_book_name[] = {
    "codex",
    "compendium",
    "exposition",
    "tables",
    "treatise",
    "devotional",
    "devout notes",
    "divine text",
    "divine work",
    "holy book",
    "holy record",
    "moral text",
    "sacred guide",
    "testament",
    "transcript"
};

/** Used by spellpath texts */
static char *path_author[] = {
    "aether",
    "astral byways",
    "connections",
    "the Grey Council",
    "deep pathways",
    "knowledge",
    "magic",
    "mystic ways",
    "pathways",
    "power",
    "spells",
    "transforms",
    "the mystic veil",
    "unknown spells",
    "cults",
    "joy",
    "lasting curse",
    "madness",
    "religions",
    "the dead",
    "the gods",
    "the heirophant",
    "the poor priest",
    "the priestess",
    "pain",
    "white"
};

/**
 * Artifact/item information
 *
 * If it isn't listed here, then art_attr_msg will never generate
 * a message for this type of artifact. */
static arttypename art_name_array[] = {
    {"Helmet", HELMET},
    {"Amulet", AMULET},
    {"Shield", SHIELD},
    {"Bracers", BRACERS},
    {"Boots", BOOTS},
    {"Cloak", CLOAK},
    {"Gloves", GLOVES},
    {"Girdle", GIRDLE},
    {"Ring", RING},
    {"Missile Weapon", BOW},
    {"Missile", ARROW},
    {"Hand Weapon", WEAPON},
    {"Artifact", SKILL},
    {"Food", FOOD},
    {"Body Armour", ARMOUR},
    {"Pants", PANTS}
};

/** Artifact book information */
static char *art_book_name[] = {
    "collection",
    "file",
    "files",
    "guide",
    "handbook",
    "index",
    "inventory",
    "list",
    "listing",
    "record",
    "record book"
};

/** Used by artifact texts */
static char *art_author[] = {
    "ancient things",
    "artifacts",
    "Havlor",
    "items",
    "lost artifacts",
    "the ancients",
    "useful things"
};

/** Monster book information */
static char *mon_book_name[] = {
    "bestiary",
    "catalog",
    "compilation",
    "collection",
    "encyclopedia",
    "guide",
    "handbook",
    "list",
    "manual",
    "notes",
    "record",
    "register",
    "volume"
};

/** Used by monster bestiary texts */
static char *mon_author[] = {
    "beasts",
    "creatures",
    "dezidens",
    "dwellers",
    "evil nature",
    "life",
    "monsters",
    "nature",
    "new life",
    "residents",
    "the spawn",
    "the living",
    "things"
};

/**
 * Generic book information. */
static char *book_name[] = {
    "calendar",
    "datebook",
    "diary",
    "guidebook",
    "handbook",
    "ledger",
    "notes",
    "notebook",
    "octavo",
    "pamphlet",
    "practicum",
    "script",
    "transcript",
    "catalog",
    "compendium",
    "guide",
    "manual",
    "opus",
    "tome",
    "treatise",
    "volume",
    "work"
};

/** Used by 'generic' books */
static char *book_author[] = {
    "Abdulah",
    "Al'hezred",
    "Alywn",
    "Arundel",
    "Arvind",
    "Aerlingas",
    "Bacon",
    "Baliqendii",
    "Bosworth",
    "Beathis",
    "Bertil",
    "Cauchy",
    "Chakrabarti",
    "der Waalis",
    "Dirk",
    "Djwimii",
    "Eisenstaadt",
    "Fendris",
    "Frank",
    "Habbi",
    "Harlod",
    "Ichibod",
    "Janus",
    "June",
    "Magnuson",
    "Nandii",
    "Nitfeder",
    "Norris",
    "Parael",
    "Penhew",
    "Sophia",
    "Skilly",
    "Tahir",
    "Thockmorton",
    "Thomas",
    "van Helsing",
    "van Pelt",
    "Voormis",
    "Xavier",
    "Xeno",
    "Zardoz",
    "Zagy",
    "Albertus Magnus",
};

/**
 * We don't want to exceed the buffer size of buf1 by adding on buf2!
 * @param buf1
 * @param buf2
 * Buffers we plan on concatenating. Can be NULL.
 * @param bufsize Size of buf1. Can be 0.
 * @return 1 if overflow will occur, 0 otherwise. */
static int buf_overflow(const char *buf1, const char *buf2, size_t bufsize)
{
    size_t len1 = 0, len2 = 0;

    if (buf1) {
        len1 = strlen(buf1);
    }

    if (buf2) {
        len2 = strlen(buf2);
    }

    if ((len1 + len2) >= bufsize) {
        return 1;
    }

    return 0;
}

/**
 * Checks if book will overflow.
 * @param buf1
 * @param buf2
 * Buffers we plan on combining.
 * @param booksize Maximum book size.
 * @return 1 if it will overflow, 0 otherwise. */
int book_overflow(const char *buf1, const char *buf2, size_t booksize)
{
    /* 2 less so always room for trailing \n */
    if (buf_overflow(buf1, buf2, BOOK_BUF - 2) || buf_overflow(buf1, buf2, booksize)) {
        return 1;
    }

    return 0;
}

/**
 * Reads the messages file into the list pointed to by first_msg. */
static void init_msgfile(void)
{
    FILE *fp;
    char buf[MAX_BUF], fname[MAX_BUF], *cp;

    msgs = NULL;
    num_msgs = 0;

    snprintf(fname, sizeof(fname), "%s/messages", settings.libpath);

    fp = fopen(fname, "r");

    if (fp) {
        int lineno, error_lineno = 0, in_msg = 0;
        char msgbuf[HUGE_BUF];

        for (lineno = 1; fgets(buf, sizeof(buf), fp); lineno++) {
            if (*buf == '#' || (*buf == '\n' && !in_msg)) {
                continue;
            }

            cp = strchr(buf, '\n');

            if (cp) {
                while (cp > buf && (cp[-1] == ' ' || cp[-1] == '\t')) {
                    cp--;
                }

                *cp = '\0';
            }

            if (in_msg) {
                if (!strcmp(buf, "ENDMSG")) {
                    if (strlen(msgbuf) > BOOK_BUF) {
                        LOG(BUG, "This string exceeded max book buf size: %s", msgbuf);
                    }

                    num_msgs++;
                    msgs = erealloc(msgs, sizeof(char *) * num_msgs);
                    msgs[num_msgs - 1] = estrdup(msgbuf);
                    in_msg = 0;
                } else if (!buf_overflow(msgbuf, buf, sizeof(msgbuf) - 1)) {
                    strcat(msgbuf, buf);
                    strcat(msgbuf, "\n");
                } else if (error_lineno != 0) {
                    LOG(BUG, "Truncating book at %s, line %d", fname, error_lineno);
                    error_lineno = 0;
                }
            } else if (!strcmp(buf, "MSG")) {
                error_lineno = lineno;
                msgbuf[0] = '\0';
                in_msg = 1;
            } else {
                LOG(BUG, "Syntax error at %s, line %d", fname, lineno);
            }
        }

        fclose(fp);
    }
}

/**
 * Initialize array of ::monsters. */
static void init_mon_info(void)
{
    monsters = NULL;
    num_monsters = 0;

    archetype_t *at, *tmp;
    HASH_ITER(hh, arch_table, at, tmp) {
        if (!QUERY_FLAG(&at->clone, FLAG_MONSTER)) {
            continue;
        }

        monsters = erealloc(monsters, sizeof(*monsters) * (num_monsters + 1));
        monsters[num_monsters] = &at->clone;
        num_monsters++;
    }
}

/**
 * Initializes linked lists utilized by message functions
 * in tailor_readable_ob().
 *
 * This is the function called by the main routine to initialize
 * all the readable information. */
void init_readable(void)
{
    init_msgfile();
    init_mon_info();
}

/**
 * Only for objects of type BOOK. SPELLBOOK stuff is
 * handled directly in change_book_name(). Names are based on text
 * msgtype.
 *
 * This sets book book->name based on msgtype given. What name
 * is given is based on various criteria.
 * @param book The book object.
 * @param msgtype Message type. */
static void new_text_name(object *book, int msgtype)
{
    const char *name;

    if (book->type != BOOK) {
        return;
    }

    switch (msgtype) {
    case MSGTYPE_MONSTER:
        name = mon_book_name[rndm(1, arraysize(mon_book_name)) - 1];
        break;

    case MSGTYPE_ARTIFACT:
        name = art_book_name[rndm(1, arraysize(art_book_name)) - 1];
        break;

    case MSGTYPE_SPELLPATH:
        name = path_book_name[rndm(1, arraysize(path_book_name)) - 1];
        break;

    case MSGTYPE_MSGFILE:
    default:
        name = book_name[rndm(1, arraysize(book_name)) - 1];
        break;
    }

    FREE_AND_COPY_HASH(book->name, name);
}

/**
 * A lot like new_text_name(), but instead chooses an author and
 * sets op->title to that value.
 * @param op The book object.
 * @param msgtype Message type. */
static void add_author(object *op, int msgtype)
{
    char title[MAX_BUF];
    const char *name;

    if (msgtype < 0 || strlen(op->msg) < 5) {
        return;
    }

    switch (msgtype) {
    case MSGTYPE_MONSTER:
        name = mon_author[rndm(1, arraysize(mon_author)) - 1];
        break;

    case MSGTYPE_ARTIFACT:
        name = art_author[rndm(1, arraysize(art_author)) - 1];
        break;

    case MSGTYPE_SPELLPATH:
        name = path_author[rndm(1, arraysize(path_author)) - 1];
        break;

    case MSGTYPE_MSGFILE:
    default:
        name = book_author[rndm(1, arraysize(book_author)) - 1];
    }

    snprintf(title, sizeof(title), "of %s", name);
    FREE_AND_COPY_HASH(op->title, title);
}

/**
 * Give a new, fancier name to generated objects of type BOOK and
 * SPELLBOOK.
 *
 * Will attempt to create consistent author/title and message content for
 * BOOKs. Also, will alter books that match archive entries to the
 * archival levels and archetypes.
 * @param book The book object to alter.
 * @param msgtype Message type to make. */
static void change_book(object *book, int msgtype)
{
    if (book->type != BOOK || book->title) {
        return;
    }

    /* Random book name */
    new_text_name(book, msgtype);
    /* Random author */
    add_author(book, msgtype);
}

/**
 * Returns a random monster from all the monsters in the game.
 * @return The monster object */
object *get_random_mon(void)
{
    /* Safety. */
    if (!monsters || !num_monsters) {
        return NULL;
    }

    return monsters[rndm(1, num_monsters) - 1];
}

/**
 * Returns a description of the monster.
 * @param mon Monster to describe.
 * @param buf Buffer that will contain the description.
 * @param size Size of 'buf'.
 * @return 'buf'. */
static char *mon_desc(object *mon, char *buf, size_t size)
{
    char *desc = object_get_description_s(mon, NULL);
    snprintf(buf, size, "[title]%s[/title]\n%s", mon->name, desc);
    efree(desc);
    return buf;
}

/**
 * Generate a message detailing the properties of randomly selected
 * monsters.
 * @param buf Buffer that will contain the message.
 * @param booksize Size (in characters) of the book we want.
 * @return 'buf'. */
static char *mon_info_msg(char *buf, size_t booksize)
{
    char tmpbuf[HUGE_BUF], desc[MAX_BUF];
    object *tmp;

    /* Preamble */
    strncpy(buf, "[title]Bestiary[/title]\nHerein are detailed creatures found in the world around.\n", booksize - 1);

    /* Lets print info on as many monsters as will fit in our
     * document. */
    while ((tmp = get_random_mon())) {
        snprintf(tmpbuf, sizeof(tmpbuf), "\n%s", mon_desc(tmp, desc, sizeof(desc)));

        if (!rndm(0, 6) || book_overflow(buf, tmpbuf, booksize)) {
            break;
        }

        snprintf(buf + strlen(buf), booksize - strlen(buf), "%s", tmpbuf);
    }

    return buf;
}

/**
 * Generate a message detailing the properties of 1-6 artifacts drawn
 * sequentially from the artifact list.
 * @param level Level of the book
 * @param buf Buffer to contain the description.
 * @param booksize Length of the book.
 * @return 'buf'. */
static char *artifact_msg(int level, char *buf, size_t booksize)
{
    artifact_list_t *al;
    artifact_t *art;
    int chance, i, type, idx;
    int book_entries = level > 5 ? RANDOM () % 3 + RANDOM () % 3 + 2 : RANDOM () % level + 1;
    char *final;
    object *tmp = NULL;
    StringBuffer *desc;

    /* Values greater than 5 create msg buffers that are too big! */
    if (book_entries > 5) {
        book_entries = 5;
    }

    /* Let's determine what kind of artifact type randomly.
     * Right now legal artifacts only come from those listed
     * in art_name_array. Also, we check to be sure an artifactlist
     * for that type exists! */
    i = 0;

    do {
        idx = rndm(1, arraysize(art_name_array)) - 1;
        type = art_name_array[idx].type;
        al = artifact_list_find(type);
        i++;
    } while (al == NULL && i < 10);

    /* Unable to find a message */
    if (i == 10) {
        snprintf(buf, booksize, "None");
        return buf;
    }

    /* There is no reason to start on the artifact list at the beginning. Lets
     * take our starting position randomly... */
    art = al->items;

    for (i = rndm(1, level) + rndm(0, 1); i > 0; i--) {
        /* Out of stuff, loop back around */
        if (art == NULL) {
            art = al->items;
        }

        art = art->next;
    }

    /* Ok, let's print out the contents */
    snprintf(buf, booksize, "[title]Magical %s[/title]\nHerein %s detailed %s...\n", art_name_array[idx].name, book_entries > 1 ? "are" : "is", book_entries > 1 ? "some artifacts" : "an artifact");

    /* Artifact msg attributes loop. Let's keep adding entries to the 'book'
     * as long as we have space up to the allowed max # (book_entries) */
    while (book_entries > 0) {
        if (art == NULL) {
            art = al->items;
        }

        desc = stringbuffer_new();
        tmp = arch_to_object(art->def_at);
        SET_FLAG(tmp, FLAG_IDENTIFIED);

        stringbuffer_append_string(desc, "\n[title]");
        desc = object_get_material_name(tmp, NULL, desc);
        stringbuffer_append_string(desc, "[/title]\nIt is ");

        /* Chance of finding. */
        chance = 100 * ((float) art->chance / al->total_chance);

        if (chance >= 20) {
            stringbuffer_append_string(desc, "an uncommon");
        } else if (chance >= 10) {
            stringbuffer_append_string(desc, "an unusual");
        } else if (chance >= 5) {
            stringbuffer_append_string(desc, "a rare");
        } else {
            stringbuffer_append_string(desc, "a very rare");
        }

        /* Value of artifact. */
        stringbuffer_append_printf(desc, " item with a value of %s.", shop_get_cost_string(tmp->value));

        StringBuffer *sb = object_get_description(tmp, NULL, NULL);
        if (stringbuffer_length(sb) > 1) {
            stringbuffer_append_string(desc,
                    "\nProperties of this artifact include:\n");
            stringbuffer_append_stringbuffer(desc, sb);
        }
        stringbuffer_free(sb);

        object_destroy(tmp);
        final = stringbuffer_finish(desc);

        /* Add the buf if it will fit. */
        if (book_overflow(buf, final, booksize)) {
            efree(final);
            break;
        }

        snprintf(buf + strlen(buf), booksize - strlen(buf), "%s", final);
        efree(final);

        art = art->next;
        book_entries--;
    }

    return buf;
}

/**
 * Generate a message detailing the member incantations (and some of their
 * properties) belonging to a random spellpath.
 * @param level Level of the book.
 * @param buf Buffer to write the description into.
 * @param booksize Length of the book.
 * @return 'buf'. */
static char *spellpath_msg(int level, char *buf, size_t booksize)
{
    int path = rndm(1, NRSPELLPATHS) - 1;
    int i, did_first_sp = 0;
    uint32_t pnum = spellpathdef[path];
    StringBuffer *desc;
    char *final;

    desc = stringbuffer_new();
    buf[0] = '\0';

    /* Preamble */
    stringbuffer_append_printf(desc, "[title]Path of %s[/title]\nHerein are detailed the names of incantations belonging to the path of %s:\n\n", spellpathnames[path], spellpathnames[path]);

    /* Now go through the entire list of spells. Add appropriate spells
     * in our message buffer */
    for (i = 0; i < NROFREALSPELLS; i++) {
        if (!(pnum & spells[i].path)) {
            continue;
        }

        if (strlen(spells[i].name) + stringbuffer_length(desc) >= booksize) {
            break;
        }

        if (did_first_sp) {
            stringbuffer_append_string(desc, ",\n");
        }

        did_first_sp = 1;
        stringbuffer_append_string(desc, spells[i].name);
    }

    final = stringbuffer_finish(desc);

    /* Geez, no spells were generated. */
    if (!did_first_sp) {
        snprintf(buf + strlen(buf), booksize - strlen(buf), "%s\n - no known spells exist -\n", final);
    } else {
        snprintf(buf + strlen(buf), booksize - strlen(buf), "%s\n", final);
    }

    efree(final);
    return buf;
}

/**
 * Generate a message drawn randomly from a file in lib/.
 * @param booksize Maximum book size.
 * @return Pointer to the generated message. */
static char *msgfile_msg(size_t booksize)
{
    static char buf[BOOK_BUF];
    char *msg = NULL;

    /* Get a random message. */
    if (msgs && num_msgs) {
        msg = msgs[rndm(1, num_msgs) - 1];
    }

    if (msg && !book_overflow(buf, msg, booksize)) {
        strncpy(buf, msg, sizeof(buf) - 1);
    } else {
        strncpy(buf, "*undecipherable text*", sizeof(buf) - 1);
    }

    return buf;
}

/**
 * The main routine. This chooses a random message to put in
 * given readable object (type == BOOK) which will be
 * referred hereafter as a 'book'. We use the book level to de-
 * termine the value of the information we will insert. Higher
 * values mean the book will (generally) have better/more info.
 * See individual cases as to how this will be utilized.
 * "Book" name/content length are based on the weight of the
 * document. If the value of msg_type is negative, we will randomly
 * choose the kind of message to generate.
 * @param book The object we are creating into.
 * @param msg_type
 * If this is a positive value, we use it to determine the
 * message type - otherwise a random value is used.
 * @author b.t. thomas@astro.psu.edu */
void tailor_readable_ob(object *book, int msg_type)
{
    char msgbuf[BOOK_BUF];
    int level = book->level ? (RANDOM () % book->level) + 1 : 1;

    /* Safety. */
    if (book->type != BOOK) {
        return;
    }

    /* If no level no point in doing any more... */
    if (level <= 0) {
        return;
    }

    msg_type = msg_type > 0 ? msg_type : rndm(0, MSGTYPE_NUM);

    switch (msg_type) {
    case MSGTYPE_MONSTER:
        mon_info_msg(msgbuf, BOOK_BUF);
        break;

    case MSGTYPE_ARTIFACT:
        artifact_msg(level, msgbuf, BOOK_BUF);
        break;

    case MSGTYPE_SPELLPATH:
        spellpath_msg(level, msgbuf, BOOK_BUF);
        break;

    case MSGTYPE_MSGFILE:
    default:
        snprintf(VS(msgbuf), "%s", msgfile_msg(BOOK_BUF));
        break;
    }

    /* Safety -- we get ugly map saves/crashes without this */
    snprintfcat(VS(msgbuf), "\n");

    if (strlen(msgbuf) > 1) {
        FREE_AND_COPY_HASH(book->msg, msgbuf);
        /* Let's give the "book" a new name, which may be a compound word */
        change_book(book, msg_type);
    }
}

/**
 * Cleanup routine for readable stuff. */
void free_all_readable(void)
{
    size_t i;

    for (i = 0; i < num_msgs; i++) {
        efree(msgs[i]);
    }

    efree(msgs);
    efree(monsters);
}
