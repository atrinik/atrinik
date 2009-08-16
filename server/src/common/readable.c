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

/* This file contains code relevant to the BOOKS hack -- designed
 * to allow randomly occuring messages in non-magical texts.
 */

/* laid down initial file - dec 1995. -b.t. thomas@astro.psu.edu */

#include <global.h>
#include <book.h>
#include <living.h>
#include <spells.h>

/* Define this if you want to archive book titles by contents.
 * This option should enforce UNIQUE combinations of titles,authors and
 * msg contents during and *between* game sessions.
 * Note: a slight degeneracy exists since books are archived based on an integer
 * index value calculated from the message text (similar to alchemy formulae).
 * Sometimes two widely different messages have the same index value (rare). In
 * this case,  it is possible to occasionally generate 2 books with same title and
 * different message content. Not really a bug, but rather a feature. This action
 * should  keeps player on their toes ;).
 * Also, note that there is *finite* space available for archiving message and titles.
 * Once this space is used, books will stop being archived. Not a serious problem
 * under the current regime, since there are generally fewer possible (random)
 * messages than space available on the titlelists.
 * One exception (for sure) are the monster messages. But no worries, you should
 * see all of the monster info in some order (but not all possble combinations)
 * before the monster titlelist space is run out. You can increase titlelist
 * space by increasing the array sizes for the monster book_authours and book_names
 * (see  max_titles[] array and include/read.h). Since the unique_book algorthm is
 * kinda stupid, this program *may* slow down program execution if defined (but I don't
 * think its a significant problem, at least, I have no problems running this option
 * on a Sparc 10! Also, once archive title lists are filled and/or all possible msg
 * combinations have been generated, unique_book isnt called anymore. It takes 5-10
 * sessions for this to happen).
 * Final note: the game remembers book/title/msg combinations from reading the
 * file lib/bookarch. If you REMOVE this file, you will lose your archive. So
 * be sure to copy it over to the new lib directory when you change versions.
 * -b.t. */

/* This flag is useful to see what kind of output messages are created */
/* #define BOOK_MSG_DEBUG */

/* This flag is useful for debuging archiving action */
/* #define ARCHIVE_DEBUG */

/* Moved these structures from struct.h to this file in 0.94.3 - they
 * are not needed anyplace else, so why have them globally declared? */

/* 'title' and 'titlelist' are used by the readable code */
typedef struct titlestruct
{
	/* the name of the book */
	const char *name;
	/* the name of the book authour */
	const char *authour;
	/* the archetype name of the book */
	const char *archname;
	/* level of difficulty of this message */
	int level;
	/* size of the book message */
	int size;
	/* an index value derived from book message */
	int msg_index;
	struct titlestruct *next;
} title;

typedef struct titleliststruct
{
	/* number of items in the list */
	int number;
	/* pointer to first book in this list */
	struct titlestruct *first_book;
	/* pointer to next book list */
	struct titleliststruct *next;
} titlelist;


/* special structure, used only by art_name_array[] */

typedef struct namebytype
{
	/* generic name to call artifacts of this type */
	char *name;
	/* matching type */
	int type;
} arttypename;

/* booklist is the buffer of books read in from the bookarch file */
static titlelist *booklist = NULL;

static objectlink *first_mon_info = NULL;

/* these are needed for creation of a linked list of
 * pointers to all (hostile) monster objects */

static int nrofmon = 0, need_to_write_bookarchive = 0;


/* this is needed to keep track of status of initialization
 * of the message file */
static int nrofmsg = 0;

/* first_msg is the started of the linked list of messages as read from
 * the messages file */
static linked_char *first_msg = NULL;

/* Spellpath information */

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

static char *path_book_name[] =
{
	"codex",
	"compendium",
	"exposition",
	"tables",
	"treatise"
};

/* used by spellpath texts */
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

/* Artiface/item information */

/* if it isnt listed here, then art_attr_msg will never generate
 * a message for this type of artifact. -b.t. */
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

/* used by artifact texts */
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

/* Monster book information */

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

/* used by monster beastuary texts */
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

/* God book information */
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

/* used by gods texts */
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


/* Alchemy (formula) information */
static char *formula_book_name[] =
{
	"cookbook",
	"formulary",
	"lab book",
	"lab notes",
	"recipe book"
};

/* this isn't used except for empty books */
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

/* Generic book information */

/* used by msg file and 'generic' books */
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

/* used by 'generic' books */
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

static int max_titles[6] =
{
	((sizeof (light_book_name) / sizeof (char *)) + (sizeof (heavy_book_name) / sizeof (char *))) * (sizeof (book_author) / sizeof (char *)),
	(sizeof (mon_book_name) / sizeof (char *)) * (sizeof (mon_author) / sizeof (char *)),
	(sizeof (art_book_name) / sizeof (char *)) * (sizeof (art_author) / sizeof (char *)),
	(sizeof (path_book_name) / sizeof (char *)) * (sizeof (path_author) / sizeof (char *)),
	(sizeof (formula_book_name) / sizeof (char *)) * (sizeof (formula_author) / sizeof (char *)),
	(sizeof (gods_book_name) / sizeof (char *)) * (sizeof (gods_author) / sizeof (char *))
};

/******************************************************************************
 *
 * Start of misc. readable functions used by others functions in this file
 *
 *****************************************************************************/

static titlelist *get_empty_booklist()
{
	titlelist *bl = (titlelist *) malloc (sizeof (titlelist));

	if (bl == NULL)
		LOG(llevError, "ERROR: get_empty_booklist(): OOM.\n");

	bl->number = 0;
	bl->first_book = NULL;
	bl->next = NULL;
	return bl;
}

static title *get_empty_book()
{
	title  *t = (title *) malloc (sizeof (title));

	if (t == NULL)
		LOG(llevError, "ERROR: get_empty_book(): OOM.\n");

	t->name = NULL;
	t->archname = NULL;
	t->authour = NULL;
	t->level = 0;
	t->size = 0;
	t->msg_index = 0;
	t->next = NULL;
	return t;
}

/* get_titlelist() - returns pointer to the title list referanced by i  */
static titlelist *get_titlelist(int i)
{
	titlelist *tl = booklist;
	int number = i;

	if (number < 0)
		return tl;

	while (tl && number)
	{
		if (!tl->next)
			tl->next = get_empty_booklist();

		tl = tl->next;
		number--;
	}

	return tl;
}

/* HANDMADE STRING FUNCTIONS.., perhaps these belong in another file
 * (shstr.c ?), but the quantity BOOK_BUF will need to be defined. */

/* nstrtok() - simple routine to return the number of list
 * items in buf1 as separated by the value of buf2 */
int nstrtok(const char *buf1, const char *buf2)
{
	char *tbuf, sbuf[12], buf[MAX_BUF];
	int number = 0;

	if (!buf1 || !buf2)
		return 0;

	sprintf(buf, "%s", buf1);
	sprintf(sbuf, "%s", buf2);
	tbuf = strtok (buf, sbuf);

	while (tbuf)
	{
		number++;
		tbuf = strtok (NULL, sbuf);
	}

	return number;
}

/* strtoktolin() - takes a string in buf1 and separates it into
 * a list of strings delimited by buf2. Then returns a comma
 * separated string w/ decent punctuation. */
char *strtoktolin(const char *buf1, const char *buf2)
{
	int maxi, i = nstrtok (buf1, buf2);
	char *tbuf, buf[MAX_BUF], sbuf[12];
	static char rbuf[BOOK_BUF];

	maxi = i;
	strcpy(buf, buf1);
	strcpy(sbuf, buf2);
	strcpy(rbuf, " ");
	tbuf = strtok (buf, sbuf);

	while (tbuf && i > 0)
	{
		strcat(rbuf, tbuf);
		i--;

		if (i == 1 && maxi > 1)
			strcat(rbuf, " and ");
		else if (i > 0 && maxi > 1)
			strcat(rbuf, ", ");
		else
			strcat(rbuf, ".");

		tbuf = strtok (NULL, sbuf);
	}

	return (char *) rbuf;
}

int book_overflow(const char *buf1, const char *buf2, int booksize)
{
	/* 2 less so always room for trailing \n */
	if (buf_overflow(buf1, buf2, BOOK_BUF - 2) || buf_overflow(buf1, buf2, booksize))
		return 1;

	return 0;
}

/*****************************************************************************
 *
 * Start of initialization related functions.
 *
 ****************************************************************************/

/* init_msgfile() - if not called before, initialize the info list
 * reads the messages file into the list pointed to by first_msg */
static void init_msgfile(void)
{
	FILE *fp;
	char buf[MAX_BUF], msgbuf[HUGE_BUF], fname[MAX_BUF], *cp;
	int comp;
	static int did_init_msgfile;

	if (did_init_msgfile)
		return;

	did_init_msgfile = 1;
	sprintf(fname, "%s/messages", settings.datadir);
	LOG(llevDebug, "Reading messages from %s...", fname);

	if ((fp = open_and_uncompress(fname, 0, &comp)) != NULL)
	{
		linked_char *tmp = NULL;
		while (fgets(buf, MAX_BUF, fp) != NULL)
		{
			if (*buf == '#')
				continue;

			if ((cp = strchr(buf, '\n')) != NULL)
				*cp = '\0';

			cp = buf;
			/* Skip blanks */
			while (*cp == ' ')
				cp++;

			if (!strncmp (cp, "ENDMSG", 6))
			{
				if (strlen(msgbuf) > BOOK_BUF)
				{
					LOG(llevDebug, "Warning: this string exceeded max book buf size:");
					LOG(llevDebug, "  %s", msgbuf);
				}

				tmp->name = NULL;
				FREE_AND_COPY_HASH(tmp->name, msgbuf);
				tmp->next = first_msg;
				first_msg = tmp;
				nrofmsg++;
				continue;
			}
			else if (!strncmp(cp, "MSG", 3))
			{
				tmp = (linked_char *) malloc(sizeof (linked_char));
				/* reset msgbuf for new message */
				strcpy(msgbuf, " ");
				continue;
			}
			else if (!buf_overflow(msgbuf, cp, HUGE_BUF - 1))
			{
				strcat (msgbuf, cp);
				strcat (msgbuf, "\n");
			}
		}
		close_and_delete(fp, comp);
	}

#ifdef BOOK_MSG_DEBUG
	LOG(llevDebug, "\ninit_info_listfile() got %d messages.\n", nrofmsg);
#endif
	LOG(llevDebug, "done.\n");
}


/* init_book_archive() - if not called before, initialize the info list
 * This reads in the bookarch file into memory.  bookarch is the file
 * created and updated across multiple runs of the program. */
static void init_book_archive(void)
{
	FILE *fp;
	int comp, nroftitle = 0;
	char buf[MAX_BUF], fname[MAX_BUF], *cp;
	title *book = NULL;
	titlelist *bl = get_empty_booklist();
	static int did_init_barch;

	if (did_init_barch)
		return;

	did_init_barch = 1;

	if (!booklist)
		booklist = bl;

	sprintf(fname, "%s/bookarch", settings.localdir);
	LOG(llevDebug, " Reading bookarch from %s...\n", fname);

	if ((fp = open_and_uncompress(fname, 0, &comp)) != NULL)
	{
		int i = 0, value, type = 0;
		while (fgets(buf, MAX_BUF, fp) != NULL)
		{
			if (*buf == '#')
				continue;

			if ((cp = strchr(buf, '\n')) != NULL)
				*cp = '\0';

			cp = buf;
			/* Skip blanks */
			while (*cp == ' ')
				cp++;

			if (!strncmp(cp, "title", 4))
			{
				/* init new book entry */
				book = get_empty_book();
				FREE_AND_COPY_HASH(book->name, strchr (cp, ' ') + 1);
				type = -1;
				nroftitle++;
				continue;
			}

			if (!strncmp(cp, "authour", 7))
			{
				FREE_AND_COPY_HASH(book->authour, strchr(cp, ' ') + 1);
			}
			else if (!strncmp(cp, "arch", 4))
			{
				FREE_AND_COPY_HASH(book->archname, strchr(cp, ' ') + 1);
			}
			else if (sscanf(cp, "level %d", &value))
				book->level = (uint16) value;
			else if (sscanf(cp, "type %d", &value))
				type = (uint16) value;
			else if (sscanf(cp, "size %d", &value))
				book->size = (uint16) value;
			else if (sscanf(cp, "index %d", &value))
				book->msg_index = (uint16) value;
			else if (!strncmp(cp, "end", 3))
			{
				/* link it */
				bl = get_titlelist(type);
				book->next = bl->first_book;
				bl->first_book = book;
				bl->number++;
			}
		}

		LOG(llevDebug, " book archives(used/avail): ");
		bl = booklist;

		while (bl && max_titles[i])
		{
			LOG(llevDebug, "(%d/%d)", bl->number, max_titles[i]);
			bl = bl->next;
			i++;
		}

		LOG(llevDebug, "\n");
		close_and_delete(fp, comp);
	}

#ifdef BOOK_MSG_DEBUG
	LOG(llevDebug, "\n init_book_archive() got %d titles.\n", nroftitle);
#endif
	LOG(llevDebug, " done.\n");
}

/* init_mon_info() - creates the linked list of pointers to
 * monster archetype objects if not called previously */
static void init_mon_info(void)
{
	archetype *at;
	static int did_init_mon_info = 0;

	if (did_init_mon_info)
		return;

	did_init_mon_info = 1;

	for (at = first_archetype; at != NULL; at = at->next)
	{
		if (QUERY_FLAG(&at->clone, FLAG_MONSTER) && (!QUERY_FLAG(&at->clone, FLAG_CHANGING) || QUERY_FLAG(&at->clone, FLAG_UNAGGRESSIVE)))
		{
			objectlink *mon = (objectlink *) malloc (sizeof (objectlink));
			mon->ob = &at->clone;
			mon->id = nrofmon;
			mon->next = first_mon_info;
			first_mon_info = mon;
			nrofmon++;
		}
	}

	LOG(llevDebug, "init_mon_info() got %d monsters ...", nrofmon);
}


/* init_readable() - initialize linked lists utilized by
 * message functions in tailor_readable_ob()
 *
 * This is the function called by the main routine to initialize
 * all the readable information. */
void init_readable(void)
{
	static int did_this;

	if (did_this)
		return;

	did_this = 1;

	LOG(llevDebug, "Initializing reading data...");
	init_msgfile();
	init_book_archive();
	init_mon_info();
	LOG(llevDebug, " done.\n");

}

/*****************************************************************************
 *
 * This is the start of the administrative functions when creating
 * new books (ie, updating title and the like)
 *
 *****************************************************************************/

/* find_title() - Search the titlelist (based on msgtype) to see if
 * book matches something already there.  IF so, return that title. */

static title *find_title(object *book, int msgtype)
{
	title *t = NULL;
	titlelist *tl = get_titlelist(msgtype);
	int length = strlen(book->msg);
	int index = strtoint(book->msg);

	if (msgtype < 0)
		return (title *) NULL;

	if (tl)
		t = tl->first_book;

	while (t)
	{
		if (t->size == length && t->msg_index == index)
			break;
		else
			t = t->next;
	}

#ifdef ARCHIVE_DEBUG
	if (t)
		LOG(llevDebug, "Found title match (list %d): %s %s (%d)\n", msgtype, t->name, t->authour, t->msg_index);
#endif

	return t;
}

/* new_text_name() - Only for objects of type BOOK. SPELLBOOK stuff is
 * handled directly in change_book_name(). Names are based on text
 * msgtype
 * this sets book book->name based on msgtype given.  What name
 * is given is based on various criteria */

static void new_text_name(object *book, int msgtype)
{
	int nbr;
	char name[MAX_BUF];

	if (book->type != BOOK)
		return;

	switch (msgtype)
	{
			/* monster */
		case 1:
			nbr = sizeof(mon_book_name) / sizeof(char *);
			strcpy(name, mon_book_name[RANDOM () % nbr]);
			break;

			/* artifact */
		case 2:
			nbr = sizeof(art_book_name) / sizeof(char *);
			strcpy(name, art_book_name[RANDOM () % nbr]);
			break;

			/* spellpath */
		case 3:
			nbr = sizeof(path_book_name) / sizeof(char *);
			strcpy(name, path_book_name[RANDOM () % nbr]);
			break;

			/* alchemy */
		case 4:
			nbr = sizeof(formula_book_name) / sizeof(char *);
			strcpy(name, formula_book_name[RANDOM () % nbr]);
			break;

			/* gods */
		case 5:
			nbr = sizeof (gods_book_name) / sizeof(char *);
			strcpy(name, gods_book_name[RANDOM () % nbr]);
			break;

			/* msg file */
		case 6:
		default:
			/* based on weight */
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

/* add_book_author()
 * A lot like new_text_name above, but instead chooses an author
 * and sets op->title to that value */
static void add_author(object *op, int msgtype)
{
	char title[MAX_BUF], name[MAX_BUF];
	int nbr = sizeof(book_author) / sizeof(char *);

	if (msgtype < 0 || strlen (op->msg) < 5)
		return;

	switch (msgtype)
	{
			/* monster */
		case 1:
			nbr = sizeof(mon_author) / sizeof(char *);
			strcpy(name, mon_author[RANDOM () % nbr]);
			break;

			/* artifacts */
		case 2:
			nbr = sizeof(art_author) / sizeof(char *);
			strcpy(name, art_author[RANDOM () % nbr]);
			break;

			/* spellpath */
		case 3:
			nbr = sizeof(path_author) / sizeof(char *);
			strcpy(name, path_author[RANDOM () % nbr]);
			break;

			/* alchemy */
		case 4:
			nbr = sizeof(formula_author) / sizeof(char *);
			strcpy(name, formula_author[RANDOM () % nbr]);
			break;

			/* gods */
		case 5:
			nbr = sizeof(gods_author) / sizeof(char *);
			strcpy(name, gods_author[RANDOM () % nbr]);
			break;

			/* msg file */
		case 6:
		default:
			strcpy(name, book_author[RANDOM () % nbr]);
	}

	sprintf(title, "of %s", name);
	FREE_AND_COPY_HASH(op->title, title);
}

/* unique_book() - check to see if the book title/msg is unique. We
 * go through the entire list of possibilities each time. If we find
 * a match, then unique_book returns true (because inst unique). */
static int unique_book(object *book, int msgtype)
{
	title *test;

	/* No archival entries! Must be unique! */
	if (!booklist)
		return 1;

	/* Go through the booklist.  If the author and name match, not unique so
	 * return 0. */
	for (test = get_titlelist(msgtype)->first_book; test; test = test->next)
	{
		if (!strcmp(test->name, book->name) && !strcmp(book->title, test->authour))
			return 0;
	}
	return 1;
}

/* add_book_to_list() */
static void add_book_to_list(object *book, int msgtype)
{
	titlelist *tl = get_titlelist (msgtype);
	title *t;

	if (!tl)
	{
		LOG(llevBug, "BUG: add_book_to_list() can't get booklist!\n");
		return;
	}

	t = get_empty_book();
	FREE_AND_COPY_HASH(t->name, book->name);
	FREE_AND_COPY_HASH(t->authour, book->title);
	t->size = strlen(book->msg);
	t->msg_index = strtoint(book->msg);
	FREE_AND_COPY_HASH(t->archname, book->arch->name);
	t->level = book->level;

	t->next = tl->first_book;
	tl->first_book = t;
	tl->number++;

	/* We have stuff we need to write now */
	need_to_write_bookarchive = 1;

#ifdef ARCHIVE_DEBUG
	LOG(llevDebug, "Archiving new title: %s %s (%d)\n", book->name, book->title, msgtype);
#endif
}

/* change_book() - give a new, fancier name to generated
 * objects of type BOOK and SPELLBOOK.
 * Aug 96 I changed this so we will attempt to create consistent
 * authour/title and message content for BOOKs. Also, we will
 * alter books  that match archive entries to the archival
 * levels and architypes. -b.t. */

#define MAX_TITLE_CHECK 20

void change_book(object *book, int msgtype)
{
	int nbr = sizeof(book_descrpt) / sizeof(char *);
	char name[MAX_BUF];

	switch (book->type)
	{
		case BOOK:
		{
			titlelist *tl = get_titlelist(msgtype);
			title *t = NULL;
			int tries = 0;
			/* look to see if our msg already been archived. If so, alter
			 * the book to match the archival text. If we fail to match,
			 * then we archive the new title/name/msg combo if there is
			 * room on the titlelist. */
			if ((strlen(book->msg) > 5) && (t = find_title(book, msgtype)))
			{
				object *tmpbook;

				/* alter book properties */
				if ((tmpbook = get_archetype(t->archname)) != NULL)
				{
					FREE_AND_COPY_HASH(tmpbook->msg, book->msg);
					copy_object(tmpbook, book);
				}

				FREE_AND_COPY_HASH(book->title, t->authour);
				FREE_AND_COPY_HASH(book->name, t->name);
				book->level = t->level;
			}
			/* Don't have any default title, so lets make up a new one */
			else
			{
				int numb, maxnames = max_titles[msgtype];
				char old_title[MAX_BUF], old_name[MAX_BUF];

				if (book->title)
					strcpy(old_title, book->title);

				strcpy(old_name, book->name);

				/* some pre-generated books have title already set (from
				 * maps), also don't bother looking for unique title if
				 * we already used up all the available names! */

				if (!tl)
				{
					LOG(llevBug, "BUG: change_book_name(): can't find title list\n");
					numb = 0;
				}
				else
					numb = tl->number;

				if (numb == maxnames)
				{
#ifdef ARCHIVE_DEBUG
					LOG(llevDebug, "titles for list %d full (%d possible).\n", msgtype, maxnames);
#endif
					break;
				}
				/* shouldnt change map-maker books */
				else if (!book->title)
				{
					do
					{
						/* random book name */
						new_text_name(book, msgtype);
						/* random author */
						add_author(book, msgtype);
						tries++;
					}
					while (!unique_book(book, msgtype) && tries < MAX_TITLE_CHECK);
				}

				/* Now deal with 2 cases.
				 * 1)If no space for a new title exists lets just restore
				 * the old book properties. Remember, if the book had
				 * matchd an older entry on the titlelist, we shouldnt
				 * have called this routine in the first place!
				 * 2) If we got a unique title, we need to add it to
				 * the list. */

				/* got to check maxnames again */
				if (tries == MAX_TITLE_CHECK || numb == maxnames)
				{
#ifdef ARCHIVE_DEBUG
					LOG(llevDebug, "Failed to obtain unique title for %s %s (names:%d/%d)\n", book->name, book->title, numb, maxnames);
#endif
					/* restore old book properties here */
					FREE_AND_CLEAR_HASH(book->name);
					FREE_AND_CLEAR_HASH(book->title);

					if (old_title != NULL)
						FREE_AND_COPY_HASH(book->title, old_title);

					/* Lets give the book a description to individualize it some */
					if (RANDOM () % 4)
						sprintf(old_name, "%s %s", book_descrpt[RANDOM () % nbr], old_name);

					FREE_AND_COPY_HASH(book->name, old_name);
				}
				/* archive if long msg texts */
				else if (book->title && strlen (book->msg) > 5)
					add_book_to_list(book, msgtype);
			}
			break;
		}

		/* depends on mage/clerical */
		case SPELLBOOK:
			if (book->sub_type1 == ST1_SPELLBOOK_CLERIC)
			{
				int level;
				level = spells[book->stats.sp].level / 2;
				nbr = sizeof(priest_book_name) / sizeof(char *);

				if (level > (nbr - 1))
					level = nbr - 1;

				strcpy(name, priest_book_name[level]);
			}
			else
			{
				int level;

				level = spells[book->stats.sp].level / 2;
				nbr = sizeof(mage_book_name) / sizeof(char *);

				if (level > (nbr - 1))
					level = nbr - 1;

				strcpy(name, mage_book_name[level]);
			}

			FREE_AND_COPY_HASH(book->name, name);
			break;

		default:
			LOG(llevBug, "BUG: change_book_name() called with illegal obj type.\n");
			return;
	}
}

/*****************************************************************************
 *
 * This is the start of the area that generates the actual contents
 * of the book.
 *
 *****************************************************************************/

/*****************************************************************************
 * Monster msg generation code.
 ****************************************************************************/

/* get_random_mon() - returns a random monster selected from linked
 * list of all monsters in the current game. If level is non-zero,
 * then only monsters greater than that level will be returned.
 * Changed 971225 to be greater than equal to level passed.  Also
 * made choosing by level more random. */

/* We have a problem here, we must fix later: level are NOT fix set in arch
 * anymore for monsters. We have a level 1 template mob in the arch and we
 * simply set the level as high as we need it in the game... So we can generate
 * a level 10 dragon, but also a level 100 dragon. _MT 2003. */
object *get_random_mon(int level)
{
	objectlink *mon = first_mon_info;
	int i = 0, monnr;

	/* LEVEL FIX. MT -2003 */
	level = 0;

	/* safety check.  Problem with init_mon_info list? */
	if (!nrofmon || !mon)
		return (object *) NULL;

	if (!level)
	{
		/* lets get a random monster from the mon_info linked list */
		monnr = RANDOM () % nrofmon;

		for (mon = first_mon_info, i = 0; mon; mon = mon->next)
			if (i++ == monnr)
				break;

		if (!mon)
		{
			LOG(llevBug, "BUG: get_random_mon: Didn't find a monster when we should have\n");
			return NULL;
		}

		return mon->ob;
	}

	/* Case where we are searching by level.  Redone 971225 to be clearer
	 * and more random.  Before, it looks like it took a random monster from
	 * the list, and then returned the first monster after that which was
	 * appropriate level.  This wasn't very random because if you had a
	 * bunch of low level monsters and then a high level one, if the random
	 * determine took one of the low level ones, it would just forward to the
	 * high level one and return that.  Thus, monsters that immediatly followed
	 * a bunch of low level monsters would be more heavily returned.  It also
	 * means some of the dragons would be poorly represented, since they
	 * are a group of high level monsters all around each other. */

	/* First count number of monsters meeting level criteria */
	for (mon = first_mon_info, i = 0; mon; mon = mon->next)
		if (mon->ob->level >= level)
			i++;

	if (i == 0)
	{
		LOG(llevBug, "BUG: get_random_mon() couldnt return monster for level %d\n", level);
		return NULL;
	}

	monnr = RANDOM () % i;
	for (mon = first_mon_info; mon; mon = mon->next)
		if (mon->ob->level >= level && monnr-- == 0)
			return mon->ob;

	if (!mon)
	{
		LOG(llevBug, "BUG: get_random_mon(): didn't find a monster when we should have\n");
		return NULL;
	}

	/* Should be unreached, by keeps warnings down */
	return NULL;
}

/* Returns a description of the monster.  This really needs to be
 * redone, as describe_item really gives a pretty internal description. */
char *mon_desc(object *mon)
{
	static char retbuf[HUGE_BUF];

	sprintf(retbuf, "<t t=\"%s\">", mon->name);
	strcat(retbuf, describe_item(mon));

	return retbuf;
}

/* This function returns the next monsters after 'tmp'.  If no match is
 * found, it returns NULL (changed 0.94.3 to do this, since the
 * calling function (mon_info_msg) seems to expect that. */
object *get_next_mon(object *tmp)
{
	objectlink *mon;

	for (mon = first_mon_info; mon; mon = mon->next)
		if (mon->ob == tmp)
			break;

	/* didn't find a match */
	if (!mon)
		return NULL;

	if (mon->next)
		return mon->next->ob;
	else
		return first_mon_info->ob;
}

/* mon_info_msg() - generate a message detailing the properties
 * of a randomly selected monster. */
char *mon_info_msg(int level, int booksize)
{
	static char retbuf[BOOK_BUF];
	char    tmpbuf[HUGE_BUF];
	object *tmp;

	/* preamble */
	strcpy(retbuf, "This beastiary contains:");

	/* lets print info on as many monsters as will fit in our
	 * document.
	 * 8-96 Had to change this a bit, otherwise there would
	 * have been an impossibly large number of combinations
	 * of text! (and flood out the available number of titles
	 * in the archive in a snap!) -b.t. */
	tmp = get_random_mon(level * 3);
	do
	{
		/* monster description */
		sprintf(tmpbuf, "\n%s", mon_desc(tmp));

		if (!book_overflow(retbuf, tmpbuf, booksize))
			strcat(retbuf, tmpbuf);
		else
			break;

		/* Note that the value this returns is not based on level */
		tmp = get_next_mon(tmp);
	}
	while (tmp);

#ifdef BOOK_MSG_DEBUG
	LOG(llevDebug, "\n mon_info_msg() created strng: %d\n", strlen(retbuf));
	LOG(llevInfo , " MADE THIS:\n%s\n", retbuf);
#endif

	return retbuf;
}


/*****************************************************************************
 * Artifact msg generation code.
 ****************************************************************************/

/* artifact_msg() - generate a message detailing the properties
 * of 1-6 artifacts drawn sequentially from the artifact list.
 */

char *artifact_msg(int level, int booksize)
{
	artifactlist *al = NULL;
	artifact *art;
	int chance, i, type, index;
	int book_entries = level > 5 ? RANDOM () % 3 + RANDOM () % 3 + 2 : RANDOM () % level + 1;
	char *ch, name[MAX_BUF], buf[BOOK_BUF], sbuf[MAX_BUF];
	static char retbuf[BOOK_BUF];
	object *tmp = NULL;
	long int val;

	/* values greater than 5 create msg buffers that are too big! */
	if (book_entries > 5)
		book_entries = 5;

	/* lets determine what kind of artifact type randomly.
	 * Right now legal artifacts only come from those listed
	 * in art_name_array. Also, we check to be sure an artifactlist
	 * for that type exists!
	 */
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
		return "None";

	/* There is no reason to start on the artifact list at the begining. Lets
	 * take our starting position randomly... */
	art = al->items;
	for (i = RANDOM() % level + RANDOM() % 2 + 1; i > 0; i--)
	{
		/* hmm, out of stuff, loop back around */
		if (art == NULL)
			art = al->items;

		art = art->next;
	}

	/* the base 'generic' name for our artifact */
	strcpy(name, art_name_array[index].name);

	/* Ok, lets print out the contents */
	sprintf(retbuf, "Herein %s detailed %s...", book_entries > 1 ? "are" : "is", book_entries > 1 ? "some artifacts" : "an artifact");

	/* artifact msg attributes loop. Lets keep adding entries to the 'book'
	 * as long as we have space up to the allowed max # (book_entires) */
	if (art == NULL)
		art = al->items;

	while (book_entries > 0)
	{
		if (art == NULL)
			break;

		tmp = arch_to_object(find_archetype(art->def_at_name));
		val = tmp->value;
		give_artifact_abilities(tmp, art);

		SET_FLAG(tmp, FLAG_IDENTIFIED);

		if (is_magical(tmp))
			SET_FLAG(tmp, FLAG_KNOWN_MAGICAL);

		sprintf(buf, "\n<t t=\"%s %s\">It is ", tmp->name, tmp->title ? tmp->title : "");
		strcat(retbuf, buf);

		/* chance of finding */
		chance = (int) (100.0f * ((float) art->chance / (float) al->total_chance));
		if (chance >= 20)
			sprintf(sbuf, "an uncommon");
		else if (chance >= 10)
			sprintf(sbuf, "an unusual");
		else if (chance >= 5)
			sprintf(sbuf, "a rare");
		else
			sprintf(sbuf, "a very rare");

		/* value of artifact */
		if (val)
			sprintf(buf, "%s item with a value that is %ld times normal.\n", sbuf, tmp->value / val);
		else
			sprintf(buf, "%s item with a value of %d\n", sbuf, tmp->value);

		strcat(retbuf, buf);
		if ((ch = describe_item(tmp)) != NULL && strlen(ch) > 1)
		{
			sprintf(buf, "Properties of this artifact include: \n %s", ch);

			if (!book_overflow(retbuf, buf, booksize))
				strcat(retbuf, buf);
			else
				break;
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

/*****************************************************************************
 * Spellpath message generation
 *****************************************************************************/

/* spellpath_msg() - generate a message detailing the member
 * incantations/prayers (and some of their properties) belonging to
 * a given spellpath. */
char *spellpath_msg(int level, int booksize)
{
	static char retbuf[BOOK_BUF];
	char tmpbuf[BOOK_BUF];
	int path = RANDOM () % NRSPELLPATHS, prayers = (RANDOM () % SPELL_TYPE_NROF) + 1;
	int i = 0, did_first_sp = 0;
	uint32 pnum = (path == -1) ? PATH_NULL : spellpathdef[path];

	/* Preamble */
	sprintf(retbuf, "Herein are detailed the names of %s ", !(prayers) ? "incantations" : "prayers");

	if (path == -1)
		strcat(retbuf, "having no known spell path.\n");
	else
		sprintf(retbuf, "%sbelonging to the path of %s:\n", retbuf, spellpathnames[path]);

	/* Now go through the entire list of spells. Add appropriate spells
	 * in our message buffer */
	do
	{
		if (spells[i].type == prayers && (pnum & spells[i].path))
		{
			/* book level determines max spell level to show
			 * thus higher level books are more comprehensive */
			if (spells[i].level > (level * 8))
			{
				i++;
				continue;
			}
			sprintf(tmpbuf, "<t t=\"%s\">", spells[i].name);

			if (book_overflow(retbuf, tmpbuf, booksize))
				break;
			else
			{
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
		/* usually, lets make a recursive call... */
		if (RANDOM() % 4)
			spellpath_msg(level, booksize);
		/* give up, cause knowning no spells exist for path is info too. */
		else
			strcat(retbuf, "\n - no known spells exist -\n");
	}
	else
	{
#ifdef BOOK_MSG_DEBUG
		LOG(llevDebug, "\n spellpath_msg() created strng: %d\n", strlen (retbuf));
		LOG(llevInfo , " MADE THIS: path=%d pray=%d\n%s\n", path, prayers, retbuf);
#endif
		strcat (retbuf, "\n");
	}
	return retbuf;
}

#ifdef ALCHEMY

/* formula_msg() - generate a message detailing the properties
 * of a randomly selected alchemical formula. */
void make_formula_book(object *book, int level)
{
	char retbuf[BOOK_BUF], title[MAX_BUF];
	recipelist *fl;
	recipe *formula = NULL;
	int chance;

	/* the higher the book level, the more complex (ie number of
	 * ingredients) the formula can be. */
	fl = get_formulalist (((RANDOM() % level) / 3) + 1);

	/* safety */
	if (!fl)
		fl = get_formulalist(1);

	if (fl->total_chance == 0)
	{
		FREE_AND_COPY_HASH(book->msg, "*indescipherable text*");
		new_text_name(book, 4);
		add_author(book, 4);
		return;
	}

	/* get a random formula, weighted by its bookchance */
	chance = RANDOM () % fl->total_chance;
	for (formula = fl->items; formula != NULL; formula = formula->next)
	{
		chance -= formula->chance;
		if (chance <= 0)
			break;
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
		/* looks like a formula was found. Base the amount
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
			op_name = formula->arch_name;

		if ((at = find_archetype(op_name)) != (archetype *) NULL)
			op_name = at->clone.name;
		else
			LOG(llevBug, "BUG: formula_msg() can't find arch %s for formula.", op_name);

		/* item name */
		if (strcmp(formula->title, "NONE"))
		{
			sprintf(retbuf, "%sThe %s of %s", retbuf, op_name, formula->title);
			/* This results in things like pile of philo. sulfur.
			 * while philo. sulfur may look better, without this,
			 * you get things like 'the wise' because its missing the
			 * water of section. */
			sprintf(title, "%s: %s of %s", formula_book_name[RANDOM() % (sizeof(formula_book_name) / sizeof(char*))], op_name, formula->title);
		}
		else
		{
			sprintf (retbuf, "%sThe %s", retbuf, op_name);
			sprintf(title, "%s: %s", formula_book_name[RANDOM() % (sizeof(formula_book_name) / sizeof(char*))], op_name);
			if (at->clone.title)
			{
				strcat(retbuf, " ");
				strcat(retbuf, at->clone.title);
				strcat(title, " ");
				strcat(title, at->clone.title);
			}
		}

		/* Lets name the book something meaningful ! */
		FREE_AND_COPY_HASH(book->name, title);
		FREE_AND_CLEAR_HASH(book->title);

		/* ingredients to make it */
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
			LOG(llevBug, "BUG: formula_msg() no ingredient list for object %s of %s", op_name, formula->title);

		if (retbuf[strlen(retbuf) - 1] != '\n')
			strcat(retbuf, "\n");

		FREE_AND_COPY_HASH(book->msg, retbuf);
	}
}
#endif

/* msgfile_msg() - generate a message drawn randomly from a
 * file in lib/. */
char *msgfile_msg(int booksize)
{
	static char retbuf[BOOK_BUF];
	int i, msgnum;
	linked_char *msg = NULL;

	/* get a random message for the 'book' from linked list */
	if (nrofmsg > 1)
	{
		msg = first_msg;
		msgnum = RANDOM () % nrofmsg;

		for (i = 0; msg && i < nrofmsg && i != msgnum; i++)
			msg = msg->next;
	}

	if (msg && !book_overflow(retbuf, msg->name, booksize))
		strcpy(retbuf, msg->name);
	else
		sprintf(retbuf, "*undecipherable text*");

#ifdef BOOK_MSG_DEBUG
	LOG(llevDebug, "\n info_list_msg() created strng: %d\n", strlen(retbuf));
	LOG(llevDebug, " MADE THIS:\n%s\n", retbuf);
#endif

	return retbuf;
}


/* god_info_msg() - generate a message detailing the properties
 * of a random god. Used by the book hack. b.t. */
char *god_info_msg(int level, int booksize)
{
	static char retbuf[BOOK_BUF];
	const char *name = NULL;
	char buf[BOOK_BUF];
	int i, introlen;
	object *god = pntr_to_god_obj(get_rand_god());

	/* oops, problems... */
	if (!god)
		return (char *) NULL;

	name = god->name;

	/* preamble.. */
	sprintf(retbuf, "This document contains knowledge concerning the diety %s", name);

	/* Always have as default information the god's descriptive terms. */
	if (nstrtok(god->msg, ",") > 0)
	{
		strcat(retbuf, ", known as");
		strcat(retbuf, strtoktolin(god->msg, ","));
	}
	else
		strcat(retbuf, "...");

	strcat (retbuf, "\n ---\n");
	/* so we will know if no new info is added later */
	introlen = strlen(retbuf);

	/* Information about the god is random, and based on the level of the
	 * 'book'. Probably there is a more intellegent way to implement
	 * this ... */
	while (level > 0)
	{
		sprintf(buf, " ");

		/* enemy god */
		if (level == 2 && RANDOM () % 2)
		{
			const char *enemy = god->title;
			if (enemy)
				sprintf(buf, "The gods %s and %s are enemies.\n ---\n", name, enemy);
		}

		/* enemy race, what the god's holy word effects */
		if (level == 3 && RANDOM () % 2)
		{
			const char *enemy = god->slaying;

			if (enemy && !(god->path_denied & PATH_TURNING))
			{
				if ((i = nstrtok(enemy, ",")) > 0)
				{
					char tmpbuf[MAX_BUF];
					sprintf (buf, "The holy words of %s have the power to slay creatures belonging to the \n", name);

					if (i > 1)
						sprintf(tmpbuf, "following races:\n%s", strtoktolin(enemy, ","));
					else
						sprintf(tmpbuf, "race of%s", strtoktolin(enemy, ","));

					sprintf(buf, "%s%s\n ---\n", buf, tmpbuf);
				}
			}
		}

		/* Priest of god gets these protect,vulnerable... */
		if (level == 4 && RANDOM () % 2)
		{
			char tmpbuf[MAX_BUF], *cp;

			cp = describe_resistance(god, 1);

			/* This god does have protections */
			if (*cp)
			{
				sprintf(tmpbuf, "%s has a potent aura which is extended to faithful priests. The effects of this aura include:\n", name);
				strcat(tmpbuf, cp);
				strcat(buf, tmpbuf);
				strcat(buf, "\n ---\n");
			}
			else
				sprintf (buf, " ");
		}

		/* aligned race, summoning  */
		if (level == 5 && RANDOM () % 2)
		{
			/* aligned race */
			const char *race = god->race;

			if (race && !(god->path_denied & PATH_SUMMON))
			{
				if ((i = nstrtok(race, ",")) > 0)
				{
					char tmpbuf[MAX_BUF];
					sprintf(buf, "Creatures sacred to %s include the ", name);
					if (i > 1)
						sprintf(tmpbuf, "following races:\n%s", strtoktolin(race, ","));
					else
						sprintf(tmpbuf, "race of%s", strtoktolin (race, ","));

					sprintf(buf, "%s%s\n ---\n", buf, tmpbuf);
				}
			}
		}

		/* blessing,curse properties of the god */
		if (level == 6 && RANDOM () % 2)
		{
			char tmpbuf[MAX_BUF], *cp;

			cp = describe_resistance(god, 1);

			/* This god does have protections */
			if (*cp)
			{
				sprintf(tmpbuf, "\nThe priests of %s are known to be able to bestow a blessing which makes the recipient\n", name);
				strcat(tmpbuf, cp);
				strcat(buf, tmpbuf);
				strcat(buf, "\n ---\n");
			}
			else
				sprintf(buf, " ");
		}

		/* immunity, holy possession */
		if (level == 8 && RANDOM () % 2)
		{
			int has_effect = 0, tmpvar;
			char tmpbuf[MAX_BUF];
			sprintf(tmpbuf, "\n");
			sprintf(tmpbuf, "The priests of %s are known to make cast a mighty prayer of possession which gives the recipient\n", name);

			for (tmpvar = 0; tmpvar < NROFATTACKS; tmpvar++)
			{
				if (god->resist[tmpvar] == 100)
				{
					has_effect = 1;
					sprintf(tmpbuf + strlen(tmpbuf), "Immunity to %s", attacktype_desc[tmpvar]);
				}
			}

			if (has_effect)
			{
				strcat(buf, tmpbuf);
				strcat(buf, "\n ---\n");
			}
			else
				sprintf(buf, " ");
		}

		/* spell paths */
		if (level == 12 && RANDOM () % 2)
		{
			int has_effect = 0, tmpvar;
			char tmpbuf[MAX_BUF];
			sprintf(tmpbuf, "\n");
			sprintf(tmpbuf, "It is rarely known fact that the priests of %s are mystically transformed. Effects of this include:\n", name);

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
				sprintf(buf, " ");
		}

		/* check to be sure new buffer size dont exceed either
		 * the maximum buffer size, or the 'natural' size of the
		 * book... */
		if (book_overflow(retbuf, buf, booksize))
			break;
		else if (strlen (buf) > 1)
			strcat(retbuf, buf);

		level--;
	}

	/* we got no information beyond the preamble! */
	if ((int)strlen(retbuf) == introlen)
		strcat(retbuf, "<t t=\"Unfortunately the rest of the information is hopelessly garbled!\">");

#ifdef BOOK_MSG_DEBUG
	LOG(llevDebug, "\n god_info_msg() created strng: %d\n", strlen(retbuf));
	LOG(llevInfo, " MADE THIS:\n%s", retbuf);
#endif
	return retbuf;
}

/* tailor_readable_ob()- The main routine. This chooses a random
 * message to put in given readable object (type==BOOK) which will
 * be referred hereafter as a 'book'. We use the book level to de-
 * termine the value of the information we will insert. Higher
 * values mean the book will (generally) have better/more info.
 * See individual cases as to how this will be utilized.
 * "Book" name/content length are based on the weight of the
 * document. If the value of msg_type is negative, we will randomly
 * choose the kind of message to generate.
 * -b.t. thomas@astro.psu.edu
 *
 * book is the object we are creating into.
 * If msg_type is a positive value, we use that to determine the
 * message type - otherwise a random value is used. */
void tailor_readable_ob(object *book, int msg_type)
{
	char msgbuf[BOOK_BUF];
	int level = book->level ? (RANDOM () % book->level) + 1 : 1;
	int book_buf_size;

	/* safety */
	if (book->type != BOOK)
		return;

	/* if no level no point in doing any more... */
	if (level <= 0)
		return;

	/* Max text length this book can have. */
	book_buf_size = BOOKSIZE(book);

	/* &&& The message switch &&& */
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
			/* monster attrib */
		case 1:
			strcpy(msgbuf, mon_info_msg(level, book_buf_size));
			break;

			/* artifact attrib */
		case 2:
			strcpy(msgbuf, artifact_msg(level, book_buf_size));
			break;

			/* grouping incantations/prayers by path */
		case 3:
			strcpy(msgbuf, spellpath_msg(level, book_buf_size));
			break;

			/* describe an alchemy formula */
		case 4:
#ifdef ALCHEMY
			make_formula_book(book, level);
			/* make_formula_book already gives title */
			return;
#else
			strcpy(msgbuf, msgfile_msg(book_buf_size));
			msg_type = 0;
#endif
			break;

			/* bits of information about a god */
		case 5:
			strcpy(msgbuf, god_info_msg(level, book_buf_size));
			break;

			/* use info list in lib/ */
		case 0:
		default:
			strcpy(msgbuf, msgfile_msg(book_buf_size));
			break;
	}

	/* safety -- we get ugly map saves/crashes without this */
	strcat(msgbuf, "\n");
	if (strlen (msgbuf) > 1)
	{
		FREE_AND_COPY_HASH(book->msg,msgbuf);
		/* lets give the "book" a new name, which may be a compound word */
		change_book(book, msg_type);
	}
}


/*****************************************************************************
 *
 * Cleanup routine for readble stuff.
 *
 *****************************************************************************/

void free_all_readable()
{
	titlelist *tlist, *tnext;
	title  *title1, *titlenext;
	linked_char *lmsg, *nextmsg;
	objectlink *monlink, *nextmon;

	LOG(llevDebug, "freeing all book information\n");

	for (tlist = booklist; tlist != NULL; tlist = tnext)
	{
		tnext = tlist->next;
		for (title1 = tlist->first_book; title1; title1 = titlenext)
		{
			titlenext = title1->next;
			FREE_AND_CLEAR_HASH2(title1->name);
			FREE_AND_CLEAR_HASH2(title1->authour);
			FREE_AND_CLEAR_HASH2(title1->archname);
			free(title1);
		}

		free(tlist);
	}

	for (lmsg = first_msg; lmsg; lmsg = nextmsg)
	{
		nextmsg = lmsg->next;
		FREE_AND_CLEAR_HASH2(lmsg->name);
		free(lmsg);
	}

	for (monlink = first_mon_info; monlink; monlink = nextmon)
	{
		nextmon = monlink->next;
		free(monlink);
	}
}


/*****************************************************************************
 *
 * Writeback routine for updating the bookarchive.
 *
 ****************************************************************************/

/* write_book_archive() - write out the updated book archive */

void write_book_archive (void)
{
	FILE *fp;
	int index = 0;
	char fname[MAX_BUF];
	title *book = NULL;
	titlelist *bl = get_titlelist(0);

	/* If nothing changed, don't write anything */
	if (!need_to_write_bookarchive)
		return;

	need_to_write_bookarchive = 0;

	sprintf(fname, "%s/bookarch", settings.localdir);
	LOG(llevDebug, "Updating book archive: %s...\n", fname);

	if ((fp = fopen(fname, "w")) == NULL)
	{
		LOG(llevDebug, "Can't open book archive file %s\n", fname);
	}
	else
	{
		while (bl)
		{
			for (book = bl->first_book; book; book = book->next)
			{
				if (book && book->authour)
				{
					fprintf(fp, "title %s\n", book->name);
					fprintf(fp, "authour %s\n", book->authour);
					fprintf(fp, "arch %s\n", book->archname);
					fprintf(fp, "level %d\n", book->level);
					fprintf(fp, "type %d\n", index);
					fprintf(fp, "size %d\n", book->size);
					fprintf(fp, "index %d\n", book->msg_index);
					fprintf(fp, "end\n");
				}
			}
			bl = bl->next;
			index++;
		}
		fclose(fp);
		chmod(fname, SAVE_MODE);
	}
}
