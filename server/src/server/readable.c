/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * This file contains code relevant to allow randomly occuring
 * messages in non-magical texts.
 *
 * @author b.t. thomas@astro.psu.edu
 * @date December 1995 */

#include <global.h>
#include <book.h>

/* This flag is useful to see what kind of output messages are created */
/* #define BOOK_MSG_DEBUG */

/* This flag is useful for debuging archiving action */
/* #define ARCHIVE_DEBUG */

/** special structure, used only by art_name_array[] */
typedef struct namebytype
{
	/** Generic name to call artifacts of this type */
	char *name;

	/** Matching type */
	int type;
} arttypename;

/** First monster information */
static objectlink *first_mon_info = NULL;

/**
 * This are needed for creation of a linked list of
 * pointers to all (hostile) monster objects */
static int nrofmon = 0;

/**
 * This is needed to keep track of status of initialization
 * of the message file */
static int nrofmsg = 0;

/**
 * The start of the linked list of messages as read from
 * the messages file */
static linked_char *first_msg = NULL;

/** Spellpath information */
static uint32 spellpathdef[NRSPELLPATHS] =
{
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
static char *path_book_name[] =
{
	"codex",
	"compendium",
	"exposition",
	"tables",
	"treatise"
};

/** Used by spellpath texts */
static char *path_author[] =
{
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
	"unknown spells"
};

/**
 * Artifact/item information
 *
 * If it isn't listed here, then art_attr_msg will never generate
 * a message for this type of artifact. */
static arttypename art_name_array[] =
{
	{"Helmet", HELMET},
	{"Amulet", AMULET},
	{"Shield", SHIELD},
	{"Bracers", BRACERS},
	{"Boots", BOOTS},
	{"Cloak", CLOAK},
	{"Gloves", GLOVES},
	{"Gridle", GIRDLE},
	{"Ring", RING},
	{"Horn", HORN},
	{"Missile Weapon", BOW},
	{"Missile", ARROW},
	{"Hand Weapon", WEAPON},
	{"Artifact", SKILL},
	{"Food", FOOD},
	{"Body Armour", ARMOUR}
};

/** Artifact book information */
static char *art_book_name[] =
{
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
static char *art_author[] =
{
	"ancient things",
	"artifacts",
	/* ancient warrior scribe :) */
	"Havlor",
	"items",
	"lost artifacts",
	"the ancients",
	"useful things"
};

/** Monster book information */
static char *mon_book_name[] =
{
	"beastuary",
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

/** Used by monster beastuary texts */
static char *mon_author[] =
{
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

/** God book information */
static char *gods_book_name[] =
{
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

/** Used by gods texts */
static char *gods_author[] =
{
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

/** Alchemy (formula) information */
static char *formula_book_name[] =
{
	"cookbook",
	"formulary",
	"lab book",
	"lab notes",
	"recipe book"
};

/** This isn't used except for empty books */
static char *formula_author[] =
{
	"Albertus Magnus",
	"alchemy",
	"balms",
	"creation",
	"dusts",
	"magical manufacture",
	"making",
	"philosophical items",
	"potions",
	"powders",
	"the cauldron",
	"the lamp black",
	"transmutation",
	"waters"
};

/**
 * Generic book information
 *
 * Used by msg file and 'generic' books */
static char *light_book_name[] =
{
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
	"transcript"
};

/** Heavy book information */
static char *heavy_book_name[] =
{
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
static char *book_author[] =
{
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
	"Zagy"
};

/** Book descriptions */
static char *book_descrpt[] =
{
	"ancient",
	"cryptic",
	"cryptical",
	"dusty",
	"hiearchical",
	"grizzled",
	"gold-guilt",
	"great",
	"lost",
	"magnificent",
	"musty",
	"mythical",
	"mystical",
	"rustic",
	"stained",
	"silvered",
	"transcendental",
	"weathered"
};

/** Mage book names */
static char *mage_book_name[] =
{
	/* Level 1   */
	"grimoire",
	/* Level 2-3 */
	"grimoire",
	/* Level 4-5 */
	"manual",
	/* Level 6-7 */
	"tome",
	/* Level 8+  */
	"treatise"
};

/** Priest book names */
static char *priest_book_name[] =
{
	/* Level 1   */
	"hymnal",
	/* Level 2-3 */
	"prayerbook",
	/* Level 4-5 */
	"prayerbook",
	/* Level 6-7 */
	"sacred text",
	/* Level 8+  */
	"testament"
};

static int nstrtok(const char *buf1, const char *buf2);
static char *strtoktolin(const char *buf1, const char *buf2);
static void change_book(object *book, int msgtype);
static char *mon_desc(object *mon);
static object *get_next_mon(object *tmp);
static char *mon_info_msg(int booksize);
static char *artifact_msg(int level, int booksize);
static char *spellpath_msg(int level, int booksize);
static void make_formula_book(object *book, int level);
static char *msgfile_msg(int booksize);
static char *god_info_msg(int level, int booksize);

/**
 * Simple routine to return the number of list items in buf1 as separated
 * by the value of buf2
 * @param buf1 Items we want to split.
 * @param buf2 What to split by.
 * @return Number of elements. */
static int nstrtok(const char *buf1, const char *buf2)
{
	char *tbuf, sbuf[12], buf[MAX_BUF];
	int number = 0;

	if (!buf1 || !buf2)
	{
		return 0;
	}

	snprintf(buf, sizeof(buf), "%s", buf1);
	snprintf(sbuf, sizeof(sbuf), "%s", buf2);
	tbuf = strtok(buf, sbuf);

	while (tbuf)
	{
		number++;
		tbuf = strtok(NULL, sbuf);
	}

	return number;
}

/**
 * Takes a string in buf1 and separates it into a list of
 * strings delimited by buf2. Then returns a comma separated
 * string with decent punctuation.
 * @param buf1 Buffer to split.
 * @param buf2 What to split buf1 by.
 * @return The comma separated string */
static char *strtoktolin(const char *buf1, const char *buf2)
{
	int maxi, i = nstrtok (buf1, buf2);
	char *tbuf, buf[MAX_BUF], sbuf[12];
	static char rbuf[BOOK_BUF];

	maxi = i;
	strcpy(buf, buf1);
	strcpy(sbuf, buf2);
	strcpy(rbuf, " ");

	tbuf = strtok(buf, sbuf);

	while (tbuf && i > 0)
	{
		strcat(rbuf, tbuf);
		i--;

		if (i == 1 && maxi > 1)
		{
			strcat(rbuf, " and ");
		}
		else if (i > 0 && maxi > 1)
		{
			strcat(rbuf, ", ");
		}
		else
		{
			strcat(rbuf, ".");
		}

		tbuf = strtok(NULL, sbuf);
	}

	return rbuf;
}

/**
 * Checks if book will overflow.
 * @param buf1
 * @param buf2
 * Buffers we plan on combining.
 * @param booksize Maximum book size.
 * @return 1 if it will overflow, 0 otherwise. */
int book_overflow(const char *buf1, const char *buf2, int booksize)
{
	/* 2 less so always room for trailing \n */
	if (buf_overflow(buf1, buf2, BOOK_BUF - 2) || buf_overflow(buf1, buf2, booksize))
	{
		return 1;
	}

	return 0;
}

/**
 * If not called before, initializes the info list.
 * Reads the messages file into the list pointed to by first_msg. */
static void init_msgfile()
{
	FILE *fp;
	char buf[MAX_BUF], msgbuf[HUGE_BUF], fname[MAX_BUF], *cp;
	int comp;
	static int did_init_msgfile;

	if (did_init_msgfile)
	{
		return;
	}

	did_init_msgfile = 1;
	snprintf(fname, sizeof(fname), "%s/messages", settings.datadir);
	LOG(llevDebug, "Reading messages from %s...", fname);

	fp = open_and_uncompress(fname, 0, &comp);

	if (fp)
	{
		linked_char *tmp = NULL;
		int lineno;
		int error_lineno = 0;

		for (lineno = 1; fgets(buf, sizeof(buf), fp); lineno++)
		{
			if (*buf == '#' || *buf == '\n')
			{
				continue;
			}

			cp = strchr(buf, '\n');

			if (cp)
			{
				while (cp > buf && (cp[-1] == ' ' || cp[-1] == '\t'))
				{
					cp--;
				}

				*cp = '\0';
			}

			if (tmp)
			{
				if (!strcmp(buf, "ENDMSG"))
				{
					if (strlen(msgbuf) > BOOK_BUF)
					{
						LOG(llevDebug, "WARNING: This string exceeded max book buf size:\n");
						LOG(llevDebug, "  %s\n", msgbuf);
					}

					tmp->name = add_string(msgbuf);
					tmp->next = first_msg;
					first_msg = tmp;
					nrofmsg++;
					tmp = NULL;
				}
				else if (!buf_overflow(msgbuf, buf, sizeof(msgbuf) - 1))
				{
					strcat(msgbuf, buf);
					strcat(msgbuf, "\n");
				}
				else if (error_lineno != 0)
				{
					LOG(llevInfo, "WARNING: Truncating book at %s, line %d\n", fname, error_lineno);
					error_lineno = 0;
				}
			}
			else if (!strcmp(buf, "MSG"))
			{
				error_lineno = lineno;
				tmp = (linked_char *) malloc(sizeof(linked_char));
				/* Reset msgbuf for new message */
				strcpy(msgbuf, " ");
			}
			else
			{
				LOG(llevInfo, "WARNING: Syntax error at %s, line %d\n", fname, lineno);
			}
		}

		close_and_delete(fp, comp);
	}

#ifdef BOOK_MSG_DEBUG
	LOG(llevDebug, "\ninit_info_listfile() got %d messages.\n", nrofmsg);
#endif

	LOG(llevDebug, "done.\n");
}

/**
 * Creates the linked list of pointers to monster archetype objects if
 * not called previously. */
static void init_mon_info()
{
	archetype *at;
	static int did_init_mon_info = 0;

	if (did_init_mon_info)
	{
		return;
	}

	did_init_mon_info = 1;

	for (at = first_archetype; at; at = at->next)
	{
		if (QUERY_FLAG(&at->clone, FLAG_MONSTER) && (!QUERY_FLAG(&at->clone, FLAG_CHANGING) || QUERY_FLAG(&at->clone, FLAG_UNAGGRESSIVE)))
		{
			objectlink *mon = get_objectlink();

			mon->objlink.ob = &at->clone;
			mon->id = nrofmon;
			mon->next = first_mon_info;
			first_mon_info = mon;
			nrofmon++;
		}
	}

	LOG(llevDebug, "init_mon_info() got %d monsters...", nrofmon);
}

/**
 * Frees object links created by init_mon_info(). */
void free_mon_info()
{
	objectlink *ol, *next;

	for (ol = first_mon_info; ol; ol = next)
	{
		next = ol->next;
		free_objectlink_simple(ol);
	}
}

/**
 * Initializes linked lists utilized by message functions
 * in tailor_readable_ob().
 *
 * This is the function called by the main routine to initialize
 * all the readable information. */
void init_readable()
{
	static int did_this;

	if (did_this)
	{
		return;
	}

	did_this = 1;

	LOG(llevDebug, "Initializing reading data... ");
	init_msgfile();
	init_mon_info();
	LOG(llevDebug, " done.\n");
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
	int nbr;
	char name[MAX_BUF];

	if (book->type != BOOK)
	{
		return;
	}

	switch (msgtype)
	{
		/* Monster */
		case 1:
			nbr = sizeof(mon_book_name) / sizeof(char *);
			strcpy(name, mon_book_name[RANDOM () % nbr]);
			break;

		/* Artifact */
		case 2:
			nbr = sizeof(art_book_name) / sizeof(char *);
			strcpy(name, art_book_name[RANDOM () % nbr]);
			break;

		/* Spellpath */
		case 3:
			nbr = sizeof(path_book_name) / sizeof(char *);
			strcpy(name, path_book_name[RANDOM () % nbr]);
			break;

		/* Alchemy */
		case 4:
			nbr = sizeof(formula_book_name) / sizeof(char *);
			strcpy(name, formula_book_name[RANDOM () % nbr]);
			break;

		/* Gods */
		case 5:
			nbr = sizeof(gods_book_name) / sizeof(char *);
			strcpy(name, gods_book_name[RANDOM () % nbr]);
			break;

		/* Msg file */
		case 6:
		default:
			/* Based on weight */
			if (book->weight > 2000)
			{
				nbr = sizeof(heavy_book_name) / sizeof(char *);
				strcpy(name, heavy_book_name[RANDOM () % nbr]);
			}
			else if (book->weight < 2001)
			{
				nbr = sizeof(light_book_name) / sizeof (char *);
				strcpy(name, light_book_name[RANDOM () % nbr]);
			}

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
	char title[MAX_BUF], name[MAX_BUF];
	int nbr = sizeof(book_author) / sizeof(char *);

	if (msgtype < 0 || strlen(op->msg) < 5)
	{
		return;
	}

	switch (msgtype)
	{
		/* Monster */
		case 1:
			nbr = sizeof(mon_author) / sizeof(char *);
			strcpy(name, mon_author[RANDOM () % nbr]);
			break;

		/* Artifacts */
		case 2:
			nbr = sizeof(art_author) / sizeof(char *);
			strcpy(name, art_author[RANDOM () % nbr]);
			break;

		/* Spellpath */
		case 3:
			nbr = sizeof(path_author) / sizeof(char *);
			strcpy(name, path_author[RANDOM () % nbr]);
			break;

		/* Alchemy */
		case 4:
			nbr = sizeof(formula_author) / sizeof(char *);
			strcpy(name, formula_author[RANDOM () % nbr]);
			break;

		/* Gods */
		case 5:
			nbr = sizeof(gods_author) / sizeof(char *);
			strcpy(name, gods_author[RANDOM () % nbr]);
			break;

		/* Msg file */
		case 6:
		default:
			strcpy(name, book_author[RANDOM () % nbr]);
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
	int nbr = sizeof(book_descrpt) / sizeof(char *);
	char name[MAX_BUF];

	switch (book->type)
	{
		case BOOK:
			/* Shouldn't change books placed on map */
			if (!book->title)
			{
				/* Random book name */
				new_text_name(book, msgtype);
				/* Random author */
				add_author(book, msgtype);
			}
			break;

		/* Depends on mage/clerical */
		case SPELLBOOK:
			if (book->sub_type1 == ST1_SPELLBOOK_CLERIC)
			{
				int level = spells[book->stats.sp].level / 2;
				nbr = sizeof(priest_book_name) / sizeof(char *);

				if (level > (nbr - 1))
				{
					level = nbr - 1;
				}

				strcpy(name, priest_book_name[level]);
			}
			else
			{
				int level = spells[book->stats.sp].level / 2;
				nbr = sizeof(mage_book_name) / sizeof(char *);

				if (level > (nbr - 1))
				{
					level = nbr - 1;
				}

				strcpy(name, mage_book_name[level]);
			}

			FREE_AND_COPY_HASH(book->name, name);
			break;

		default:
			LOG(llevBug, "BUG: change_book_name() called with illegal obj type.\n");
			return;
	}
}

/**
 * Returns a random monster selected from linked list of all
 * monsters in the current game.
 * @return The monster object */
object *get_random_mon()
{
	objectlink *mon = first_mon_info;
	int i = 0, monnr;

	/* Safety check. Problem with init_mon_info list? */
	if (!nrofmon || !mon)
	{
		return NULL;
	}

	/* Let's get a random monster from the mon_info linked list */
	monnr = RANDOM () % nrofmon;

	for (mon = first_mon_info, i = 0; mon; mon = mon->next)
	{
		if (i++ == monnr)
		{
			break;
		}
	}

	if (!mon)
	{
		LOG(llevBug, "BUG: get_random_mon: Didn't find a monster when we should have\n");
		return NULL;
	}

	return mon->objlink.ob;
}

/**
 * Returns a description of the monster.
 * @param mon The monster object
 * @return Description of the monster
 * @todo
 * This really needs to be redone, as describe_item() gives
 * a pretty internal description. */
static char *mon_desc(object *mon)
{
	static char retbuf[HUGE_BUF];

	snprintf(retbuf, sizeof(retbuf), "<t t=\"%s\">", mon->name);
	strncat(retbuf, describe_item(mon), sizeof(retbuf) - strlen(retbuf) - 1);

	return retbuf;
}

/**
 * This function returns the next monster after 'tmp'.
 * @param tmp The monster object
 * @return Next monster after the monster object, NULL
 * if there is no next monster. */
static object *get_next_mon(object *tmp)
{
	objectlink *mon;

	for (mon = first_mon_info; mon; mon = mon->next)
	{
		if (mon->objlink.ob == tmp)
		{
			break;
		}
	}

	/* didn't find a match */
	if (!mon)
	{
		return NULL;
	}

	if (mon->next)
	{
		return mon->next->objlink.ob;
	}
	else
	{
		return first_mon_info->objlink.ob;
	}
}

/**
 * Generate a message detailing the properties of a randomly
 * selected monster.
 * @param booksize The maximum book size
 * @return Pointer to the generated message */
static char *mon_info_msg(int booksize)
{
	static char retbuf[BOOK_BUF];
	char tmpbuf[HUGE_BUF];
	object *tmp;

	/* Preamble */
	strcpy(retbuf, "<t t=\"Bestiary\">Herein are detailed creatures found in the world around.\n");

	/* Let's print info on as many monsters as will fit in our
	 * document. */
	tmp = get_random_mon();

	do
	{
		/* Monster description */
		snprintf(tmpbuf, sizeof(tmpbuf), "\n%s", mon_desc(tmp));

		if (!book_overflow(retbuf, tmpbuf, booksize))
		{
			strcat(retbuf, tmpbuf);
		}
		else
		{
			break;
		}

		tmp = get_next_mon(tmp);
	}
	while (tmp);

#ifdef BOOK_MSG_DEBUG
	LOG(llevDebug, "\n mon_info_msg() created strng: %d\n", strlen(retbuf));
	LOG(llevInfo , " MADE THIS:\n%s\n", retbuf);
#endif

	return retbuf;
}

/**
 * Generate a message detailing the properties of 1-6 artifacts
 * drawn sequentially from the artifact list.
 * @param level Level of the book
 * @param booksize Maximum book size
 * @return Pointer to the generated message */
static char *artifact_msg(int level, int booksize)
{
	artifactlist *al = NULL;
	artifact *art;
	int chance, i, type, index;
	int book_entries = level > 5 ? RANDOM () % 3 + RANDOM () % 3 + 2 : RANDOM () % level + 1;
	char *ch, name[MAX_BUF], buf[BOOK_BUF], sbuf[MAX_BUF];
	static char retbuf[BOOK_BUF];
	object *tmp = NULL;
	long int val;

	/* Values greater than 5 create msg buffers that are too big! */
	if (book_entries > 5)
	{
		book_entries = 5;
	}

	/* Let's determine what kind of artifact type randomly.
	 * Right now legal artifacts only come from those listed
	 * in art_name_array. Also, we check to be sure an artifactlist
	 * for that type exists! */
	i = 0;

	do
	{
		index = RANDOM() % (sizeof(art_name_array) / sizeof(arttypename));
		type = art_name_array[index].type;
		al = find_artifactlist(type);
		i++;
	}
	while ((al == NULL) && (i < 10));

	/* Unable to find a message */
	if (i == 10)
	{
		return "None";
	}

	/* There is no reason to start on the artifact list at the begining. Let's
	 * take our starting position randomly... */
	art = al->items;

	for (i = RANDOM() % level + RANDOM() % 2 + 1; i > 0; i--)
	{
		/* Out of stuff, loop back around */
		if (art == NULL)
		{
			art = al->items;
		}

		art = art->next;
	}

	/* the base 'generic' name for our artifact */
	strcpy(name, art_name_array[index].name);

	/* Ok, let's print out the contents */
	snprintf(retbuf, sizeof(retbuf), "<t t=\"Magical %s\">Herein %s detailed %s...\n\n", name, book_entries > 1 ? "are" : "is", book_entries > 1 ? "some artifacts" : "an artifact");

	/* Artifact msg attributes loop. Let's keep adding entries to the 'book'
	 * s long as we have space up to the allowed max # (book_entires) */
	if (art == NULL)
	{
		art = al->items;
	}

	while (book_entries > 0)
	{
		if (art == NULL)
		{
			break;
		}

		tmp = arch_to_object(find_archetype(art->def_at_name));
		val = tmp->value;
		give_artifact_abilities(tmp, art);

		SET_FLAG(tmp, FLAG_IDENTIFIED);

		snprintf(buf, sizeof(buf), "\n<t t=\"%s %s\">It is ", tmp->name, tmp->title ? tmp->title : "");
		strcat(retbuf, buf);

		/* Chance of finding */
		chance = 100 * ((float) art->chance / al->total_chance);

		if (chance >= 20)
		{
			snprintf(sbuf, sizeof(sbuf), "an uncommon");
		}
		else if (chance >= 10)
		{
			snprintf(sbuf, sizeof(sbuf), "an unusual");
		}
		else if (chance >= 5)
		{
			snprintf(sbuf, sizeof(sbuf), "a rare");
		}
		else
		{
			snprintf(sbuf, sizeof(sbuf), "a very rare");
		}

		/* Value of artifact */
		if (val)
		{
			snprintf(buf, sizeof(buf), "%s item with a value that is %"FMT64" times normal.\n", sbuf, tmp->value / val);
		}
		else
		{
			snprintf(buf, sizeof(buf), "%s item with a value of %"FMT64"\n", sbuf, tmp->value);
		}

		strcat(retbuf, buf);

		if ((ch = describe_item(tmp)) != NULL && strlen(ch) > 1)
		{
			snprintf(buf, sizeof(buf), "Properties of this artifact include: \n %s", ch);

			if (!book_overflow(retbuf, buf, booksize))
			{
				strcat(retbuf, buf);
			}
			else
			{
				break;
			}
		}

		art = art->next;
		book_entries--;
	}

#ifdef BOOK_MSG_DEBUG
	LOG(llevDebug, "artifact_msg() created strng: %d\n", strlen(retbuf));
	LOG(llevInfo, " MADE THIS:\n%s", retbuf);
#endif

	return retbuf;
}

/**
 * Generate a message detailing the member incantations/prayers
 * (and some of their properties) belonging to a given spellpath.
 * @param level The book level
 * @param booksize Maximum book size
 * @return Pointer to the generated message. */
static char *spellpath_msg(int level, int booksize)
{
	static char retbuf[BOOK_BUF];
	char tmpbuf[BOOK_BUF];
	int path = RANDOM() % NRSPELLPATHS, prayers = (RANDOM() % SPELL_TYPE_NROF) + 1;
	int i = 0, did_first_sp = 0;
	uint32 pnum = spellpathdef[path];

	/* Preamble */
	snprintf(retbuf, sizeof(retbuf), "<t t=\"Path of %s\">Herein are detailed the names of %s belonging to the path of %s:\n\n", spellpathnames[path], prayers ? "prayers" : "incantations", spellpathnames[path]);

	/* Now go through the entire list of spells. Add appropriate spells
	 * in our message buffer */
	do
	{
		if (spells[i].type == prayers && (pnum & spells[i].path))
		{
			/* Book level determines max spell level to show
			 * thus higher level books are more comprehensive */
			if (spells[i].level > (level * 8))
			{
				i++;
				continue;
			}

			strcpy(tmpbuf, spells[i].name);

			if (book_overflow(retbuf, tmpbuf, booksize))
			{
				break;
			}
			else
			{
                if (did_first_sp)
				{
                    strcat(retbuf, ",\n");
				}

				did_first_sp = 1;
				strcat(retbuf, tmpbuf);
			}
		}

		i++;
	}
	while (i < NROFREALSPELLS);

	/* Geez, no spells were generated. */
	if (!did_first_sp)
	{
		/* Usually, let's make a recursive call... */
		if (RANDOM() % 4)
		{
			spellpath_msg(level, booksize);
		}
		/* Give up, cause knowing no spells exist for path is info too. */
		else
		{
			strcat(retbuf, "\n - no known spells exist -\n");
		}
	}
	else
	{
#ifdef BOOK_MSG_DEBUG
		LOG(llevDebug, "\n spellpath_msg() created strng: %d\n", strlen(retbuf));
		LOG(llevInfo, " MADE THIS: path=%d pray=%d\n%s\n", path, prayers, retbuf);
#endif
		strcat(retbuf, "\n");
	}

	return retbuf;
}

/**
 * Generate a message detailing the properties of a randomly
 * selected alchemical formula.
 * @param book The book object
 * @param level Book level */
static void make_formula_book(object *book, int level)
{
	char retbuf[BOOK_BUF], title[MAX_BUF];
	recipelist *fl;
	recipe *formula = NULL;
	int chance;

	/* The higher the book level, the more complex (ie number of
	 * ingredients) the formula can be. */
	fl = get_formulalist(((RANDOM() % level) / 3) + 1);

	/* Safety */
	if (!fl)
	{
		fl = get_formulalist(1);
	}

	if (fl->total_chance == 0)
	{
		FREE_AND_COPY_HASH(book->msg, "*indescipherable text*");
		new_text_name(book, 4);
		add_author(book, 4);

		return;
	}

	/* Get a random formula, weighted by its bookchance */
	chance = RANDOM () % fl->total_chance;

	for (formula = fl->items; formula != NULL; formula = formula->next)
	{
		chance -= formula->chance;

		if (chance <= 0)
		{
			break;
		}
	}

	/* preamble */
	strcpy(retbuf, "Herein is described an alchemical proceedure: \n");

	if (!formula)
	{
		FREE_AND_COPY_HASH(book->msg, "*indescipherable text*");
		new_text_name(book, 4);
		add_author(book, 4);
	}
	else
	{
		/* Looks like a formula was found. Base the amount
		 * of information on the booklevel and the spellevel
		 * of the formula. */
		const char *op_name = NULL;
		archetype *at;
		int nindex = nstrtok(formula->arch_name, ",");

		/* construct name of object to be made */
		if (nindex > 1)
		{
			char tmpbuf[MAX_BUF];
			int rnum = RANDOM () % nindex;
			strncpy(tmpbuf, formula->arch_name, MAX_BUF - 1);
			tmpbuf[MAX_BUF - 1] = 0;
			op_name = strtok(tmpbuf, ",");

			while (rnum)
			{
				op_name = strtok(NULL, ",");
				rnum--;
			}
		}
		else
		{
			op_name = formula->arch_name;
		}

		if ((at = find_archetype(op_name)) != (archetype *) NULL)
		{
			op_name = at->clone.name;
		}
		else
		{
			LOG(llevBug, "BUG: formula_msg() can't find arch %s for formula.", op_name);
		}

		/* item name */
		if (formula->title != shstr_cons.NONE)
		{
			snprintf(retbuf, sizeof(retbuf), "%sThe %s of %s", retbuf, op_name, formula->title);

			/* This results in things like pile of philo. sulfur.
			 * while philo. sulfur may look better, without this,
			 * you get things like 'the wise' because it's missing the
			 * water of section. */
			snprintf(title, sizeof(title), "%s: %s of %s", formula_book_name[RANDOM() % (sizeof(formula_book_name) / sizeof(char*))], op_name, formula->title);
		}
		else
		{
			snprintf(retbuf, sizeof(retbuf), "%sThe %s", retbuf, op_name);
			snprintf(title, sizeof(title), "%s: %s", formula_book_name[RANDOM() % (sizeof(formula_book_name) / sizeof(char*))], op_name);

			if (at->clone.title)
			{
				strcat(retbuf, " ");
				strcat(retbuf, at->clone.title);
				strcat(title, " ");
				strcat(title, at->clone.title);
			}
		}

		/* Let's name the book something meaningful! */
		FREE_AND_COPY_HASH(book->name, title);
		FREE_AND_CLEAR_HASH(book->title);

		/* Ingredients to make it */
		if (formula->ingred != NULL)
		{
			linked_char *next;
			strcat(retbuf, " may be made using the following ingredients:\n");

			for (next = formula->ingred; next != NULL; next = next->next)
			{
				strcat(retbuf, next->name);
				strcat(retbuf, "\n");
			}
		}
		else
		{
			LOG(llevBug, "BUG: formula_msg() no ingredient list for object %s of %s", op_name, formula->title);
		}

		if (retbuf[strlen(retbuf) - 1] != '\n')
		{
			strcat(retbuf, "\n");
		}

		FREE_AND_COPY_HASH(book->msg, retbuf);
	}
}

/**
 * Generate a message drawn randomly from a file in lib/.
 * @param booksize Maximum book size
 * @return Pointer to the generated message */
static char *msgfile_msg(int booksize)
{
	static char retbuf[BOOK_BUF];
	int i, msgnum;
	linked_char *msg = NULL;

	/* Get a random message for the 'book' from linked list */
	if (nrofmsg > 1)
	{
		msg = first_msg;
		msgnum = RANDOM () % nrofmsg;

		for (i = 0; msg && i < nrofmsg && i != msgnum; i++)
		{
			msg = msg->next;
		}
	}

	if (msg && !book_overflow(retbuf, msg->name, booksize))
	{
		strcpy(retbuf, msg->name);
	}
	else
	{
		sprintf(retbuf, "*undecipherable text*");
	}

#ifdef BOOK_MSG_DEBUG
	LOG(llevDebug, "\n info_list_msg() created strng: %d\n", strlen(retbuf));
	LOG(llevDebug, " MADE THIS:\n%s\n", retbuf);
#endif

	return retbuf;
}

/**
 * Generate a message detailing the properties of a random god.
 * @param level The book level
 * @param booksize Maximum book size
 * @return Pointer to the generated message */
static char *god_info_msg(int level, int booksize)
{
	static char retbuf[BOOK_BUF];
	const char *name = NULL;
	char buf[BOOK_BUF];
	int i;
	size_t introlen;
	object *god = pntr_to_god_obj(get_rand_god());

	/* Oops, problems... */
	if (!god)
	{
		return NULL;
	}

	name = god->name;

	/* Preamble... */
	snprintf(retbuf, sizeof(retbuf), "<t t=\"%s\">This document contains knowledge concerning the deity %s", name, name);

	/* Always have as default information the god's descriptive terms. */
	if (nstrtok(god->msg, ",") > 0)
	{
		strcat(retbuf, ", known as");
		strcat(retbuf, strtoktolin(god->msg, ","));
	}
	else
	{
		strcat(retbuf, "...");
	}

	strcat(retbuf, "\n ---\n");

	/* So we will know if no new info is added later */
	introlen = strlen(retbuf);

	/* Information about the god is random, and based on the level of the
	 * book. */
	while (level > 0)
	{
		sprintf(buf, " ");

		/* Enemy god */
		if (level == 2 && RANDOM () % 2)
		{
			shstr *enemy = god->title;

			if (enemy)
			{
				snprintf(buf, sizeof(buf), "The gods %s and %s are enemies.\n ---\n", name, enemy);
			}
		}

		/* Enemy race, what the god's holy word effects */
		if (level == 3 && RANDOM () % 2)
		{
			shstr *enemy = god->slaying;

			if (enemy && !(god->path_denied & PATH_TURNING))
			{
				if ((i = nstrtok(enemy, ",")) > 0)
				{
					char tmpbuf[MAX_BUF];

					snprintf(buf, sizeof(buf), "The holy words of %s have the power to slay creatures belonging to the \n", name);

					if (i > 1)
					{
						snprintf(tmpbuf, sizeof(tmpbuf), "following races:\n%s", strtoktolin(enemy, ","));
					}
					else
					{
						snprintf(tmpbuf, sizeof(tmpbuf), "race of%s", strtoktolin(enemy, ","));
					}

					snprintf(buf, sizeof(buf), "%s%s\n ---\n", buf, tmpbuf);
				}
			}
		}

		/* Priest of god gets these protect, vulnerable... */
		if (level == 4 && RANDOM () % 2)
		{
			char tmpbuf[MAX_BUF], *cp = describe_resistance(god, 1);

			/* This god does have protections */
			if (*cp)
			{
				snprintf(tmpbuf, sizeof(tmpbuf), "%s has a potent aura which is extended to faithful priests. The effects of this aura include:\n", name);
				strcat(tmpbuf, cp);
				strcat(buf, tmpbuf);
				strcat(buf, "\n ---\n");
			}
			else
			{
				sprintf(buf, " ");
			}
		}

		/* Aligned race, summoning  */
		if (level == 5 && RANDOM () % 2)
		{
			/* Aligned race */
			const char *race = god->race;

			if (race && !(god->path_denied & PATH_SUMMON))
			{
				if ((i = nstrtok(race, ",")) > 0)
				{
					char tmpbuf[MAX_BUF];

					snprintf(buf, sizeof(buf), "Creatures sacred to %s include the ", name);

					if (i > 1)
					{
						snprintf(tmpbuf, sizeof(tmpbuf), "following races:\n%s", strtoktolin(race, ","));
					}
					else
					{
						snprintf(tmpbuf, sizeof(tmpbuf), "race of%s", strtoktolin (race, ","));
					}

					snprintf(buf, sizeof(buf), "%s%s\n ---\n", buf, tmpbuf);
				}
			}
		}

		/* Blessing, curse properties of the god */
		if (level == 6 && RANDOM () % 2)
		{
			char tmpbuf[MAX_BUF], *cp;

			cp = describe_resistance(god, 1);

			/* This god does have protections */
			if (*cp)
			{
				snprintf(tmpbuf, sizeof(tmpbuf), "\nThe priests of %s are known to be able to bestow a blessing which makes the recipient\n", name);
				strcat(tmpbuf, cp);
				strcat(buf, tmpbuf);
				strcat(buf, "\n ---\n");
			}
			else
			{
				sprintf(buf, " ");
			}
		}

		/* Immunity, holy possession */
		if (level == 8 && RANDOM () % 2)
		{
			int has_effect = 0, tmpvar;
			char tmpbuf[MAX_BUF];

			snprintf(tmpbuf, sizeof(tmpbuf), "\nThe priests of %s are known to make cast a mighty prayer of possession which gives the recipient\n", name);

			for (tmpvar = 0; tmpvar < NROFATTACKS; tmpvar++)
			{
				if (god->resist[tmpvar] == 100)
				{
					has_effect = 1;
					snprintf(tmpbuf + strlen(tmpbuf), sizeof(tmpbuf), "Immunity to %s", attacktype_desc[tmpvar]);
				}
			}

			if (has_effect)
			{
				strcat(buf, tmpbuf);
				strcat(buf, "\n ---\n");
			}
			else
			{
				sprintf(buf, " ");
			}
		}

		/* Spell paths */
		if (level == 12 && RANDOM () % 2)
		{
			int has_effect = 0, tmpvar;
			char tmpbuf[MAX_BUF];

			snprintf(tmpbuf, sizeof(tmpbuf), "\nIt is rarely known fact that the priests of %s are mystically transformed. Effects of this include:\n", name);

			if ((tmpvar = god->path_attuned))
			{
				has_effect = 1;
				DESCRIBE_PATH(tmpbuf, tmpvar, "Attuned");
			}

			if ((tmpvar = god->path_repelled))
			{
				has_effect = 1;
				DESCRIBE_PATH(tmpbuf, tmpvar, "Repelled");
			}

			if ((tmpvar = god->path_denied))
			{
				has_effect = 1;
				DESCRIBE_PATH(tmpbuf, tmpvar, "Denied");
			}

			if (has_effect)
			{
				strcat(buf, tmpbuf);
				strcat(buf, "\n ---\n");
			}
			else
			{
				sprintf(buf, " ");
			}
		}

		/* Check to be sure new buffer size doesn't exceed either
		 * the maximum buffer size, or the 'natural' size of the
		 * book... */
		if (book_overflow(retbuf, buf, booksize))
		{
			break;
		}
		else if (strlen(buf) > 1)
		{
			strcat(retbuf, buf);
		}

		level--;
	}

	/* We got no information beyond the preamble! */
	if (strlen(retbuf) == introlen)
	{
		strcat(retbuf, " [Unfortunately the rest of the information is hopelessly garbled!]\n ---\n");
	}

#ifdef BOOK_MSG_DEBUG
	LOG(llevDebug, "\n god_info_msg() created strng: %d\n", strlen(retbuf));
	LOG(llevInfo, " MADE THIS:\n%s", retbuf);
#endif

	return retbuf;
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
	int book_buf_size;

	/* Safety */
	if (book->type != BOOK)
	{
		return;
	}

	/* If no level no point in doing any more... */
	if (level <= 0)
	{
		return;
	}

	/* Max text length this book can have. */
	book_buf_size = BOOKSIZE(book);

	/* Below all of the possible types of messages in the "book"s. */

	/* IF you add a new type of book msg, you will have to do several things.
	 * 1) make sure there is an entry in the msg switch below!
	 * 2) make sure there is an entry in max_titles[] array.
	 * 3) make sure there are entries for your case in new_text_title()
	 *    and add_authour().
	 * 4) you may want separate authour/book name arrays in read.h */
	msg_type = msg_type > 0 ? msg_type : (RANDOM () % 6);

	switch (msg_type)
	{
		/* Monster attrib */
		case 1:
			strcpy(msgbuf, mon_info_msg(book_buf_size));
			break;

		/* Artifact attrib */
		case 2:
			strcpy(msgbuf, artifact_msg(level, book_buf_size));
			break;

		/* Grouping incantations/prayers by path */
		case 3:
			strcpy(msgbuf, spellpath_msg(level, book_buf_size));
			break;

		/* Describe an alchemy formula */
		case 4:
			make_formula_book(book, level);
			/* make_formula_book already gives title */
			return;

			break;

		/* Bits of information about a god */
		case 5:
			strcpy(msgbuf, god_info_msg(level, book_buf_size));
			break;

		/* Use info list in lib/ */
		case 0:
		default:
			strcpy(msgbuf, msgfile_msg(book_buf_size));
			break;
	}

	/* Safety -- we get ugly map saves/crashes without this */
	strcat(msgbuf, "\n");

	if (strlen(msgbuf) > 1)
	{
		FREE_AND_COPY_HASH(book->msg,msgbuf);

		/* Let's give the "book" a new name, which may be a compound word */
		change_book(book, msg_type);
	}
}

/**
 * Cleanup routine for readable stuff. */
void free_all_readable()
{
	linked_char *lmsg, *nextmsg;

	LOG(llevDebug, "Freeing all book information.\n");

	for (lmsg = first_msg; lmsg; lmsg = nextmsg)
	{
		nextmsg = lmsg->next;
		FREE_AND_CLEAR_HASH2(lmsg->name);
		free(lmsg);
	}
}
