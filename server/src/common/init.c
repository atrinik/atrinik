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

/**
 * @file
 * Basic initialization for the common library. */

#define EXTERN
#define INIT_C

#include <global.h>
#include <object.h>

/**
 * You unforunately need to looking in include/global.h to see what these
 * correspond to. */
struct Settings settings =
{
	/* Logfile */
	"",
	/* Client/server port */
	CSPORT,
	GLOBAL_LOG_LEVEL,
	/* dumpvalues, dumparg, daemonmode */
	0, NULL, 0,
	DATADIR,
	LOCALDIR,
	MAPDIR, PLAYERDIR, ARCHETYPES,TREASURES,
	UNIQUE_DIR, TMPDIR,
	STAT_LOSS_ON_DEATH,
	USE_PERMANENT_EXPERIENCE,
	BALANCED_STAT_LOSS,
	RESET_LOCATION_TIME,
	/* This and the next 3 values are metaserver values */
	0,
	"",
	"",
	"",
	0,
	0
};

/** World's darkness value. */
int world_darkness;

/** Time of day tick. */
unsigned long todtick;

/** Pointer to archetype that is used as effect when player levels up. */
archetype *level_up_arch = NULL;

/** Name of the archetype to use for the level up effect. */
#define ARCHETYPE_LEVEL_UP "level_up"

static void init_environ();
static void init_defaults();
static void init_dynamic();
static void init_clocks();

/**
 * Initialize the ::shstr_cons structure. */
static void init_strings()
{
	shstr_cons.none = add_string("none");
	shstr_cons.NONE = add_string("NONE");
	shstr_cons.home = add_string("- home -");
	shstr_cons.force = add_string("force");
	shstr_cons.portal_destination_name = add_string(PORTAL_DESTINATION_NAME);
	shstr_cons.portal_active_name = add_string(PORTAL_ACTIVE_NAME);
	shstr_cons.spell_quickslot = add_string("spell_quickslot");

	shstr_cons.GUILD_FORCE = add_string("GUILD_FORCE");
	shstr_cons.guild_force = add_string("guild_force");
	shstr_cons.RANK_FORCE = add_string("RANK_FORCE");
	shstr_cons.rank_force = add_string("rank_force");
	shstr_cons.ALIGNMENT_FORCE = add_string("ALIGNMENT_FORCE");
	shstr_cons.alignment_force = add_string("alignment_force");

	shstr_cons.grace_limit = add_string("grace limit");
	shstr_cons.restore_grace = add_string("restore grace");
	shstr_cons.restore_hitpoints = add_string("restore hitpoints");
	shstr_cons.restore_spellpoints = add_string("restore spellpoints");
	shstr_cons.heal_spell = add_string("heal spell");
	shstr_cons.remove_curse = add_string("remove curse");
	shstr_cons.remove_damnation = add_string("remove damnation");
	shstr_cons.heal_depletion = add_string("heal depletion");
	shstr_cons.message = add_string("message");
	shstr_cons.enchant_weapon = add_string("enchant weapon");

	shstr_cons.player_info = add_string("player_info");
	shstr_cons.BANK_GENERAL = add_string("BANK_GENERAL");
}

void free_strings()
{
	int nrof_strings = sizeof(shstr_cons) / sizeof(const char *);
	const char **ptr = (const char **) &shstr_cons;
	int i = 0;

	LOG(llevDebug, "Freeing all string constants\n");

	for (i = 0; i < nrof_strings; i++)
	{
		FREE_ONLY_HASH(ptr[i]);
	}
}

/**
 * It is vital that init_library() is called by any functions using this
 * library.
 *
 * If you want to lessen the size of the program using the library, you
 * can replace the call to init_library() with init_globals() and
 * init_function_pointers(). Good idea to also call init_vars() and
 * init_hash_table() if you are doing any object loading. */
void init_library()
{
	init_environ();
	init_globals();
	init_hash_table();
	/* Inits the pooling memory manager and the new object system */
	init_mempools();
	init_block();
	LOG(llevInfo, "Atrinik Server, v%s\n", VERSION);
	LOG(llevInfo, "Copyright (C) 2009 Alex Tokar.\n");
	read_bmap_names();
	init_materials();
	/* Must be after we read in the bitmaps */
	init_anim();
	/* Reads all archetypes from file */
	init_archetypes();
	init_dynamic();
	init_clocks();

	/* init some often used default archetypes */
	if (level_up_arch == NULL)
	{
		level_up_arch = find_archetype(ARCHETYPE_LEVEL_UP);
	}

	if (!level_up_arch)
	{
		LOG(llevBug, "BUG: Can't find '%s' arch\n", ARCHETYPE_LEVEL_UP);
	}
}

/**
 * Initializes values from the environmental variables.
 *
 * Needs to be called very early, since command line options should
 * overwrite these if specified. */
static void init_environ()
{
	char *cp;

	cp = getenv("ATRINIK_LIBDIR");

	if (cp)
	{
		settings.datadir = cp;
	}

	cp = getenv("ATRINIK_LOCALDIR");

	if (cp)
	{
		settings.localdir = cp;
	}

	cp = getenv("ATRINIK_MAPDIR");

	if (cp)
	{
		settings.mapdir = cp;
	}

	cp = getenv("ATRINIK_ARCHETYPES");

	if (cp)
	{
		settings.archetypes = cp;
	}

	cp = getenv("ATRINIK_TREASURES");

	if (cp)
	{
		settings.treasures = cp;
	}

	cp = getenv("ATRINIK_UNIQUEDIR");

	if (cp)
	{
		settings.uniquedir = cp;
	}

	cp = getenv("ATRINIK_TMPDIR");

	if (cp)
	{
		settings.tmpdir = cp;
	}
}

/**
 * Initialises all global variables.
 * Might use environment variables as default for some of them. */
void init_globals()
{
	if (settings.logfilename[0] == '\0')
	{
		logfile = stderr;
	}
	else if ((logfile = fopen(settings.logfilename, "w")) == NULL)
	{
		logfile = stderr;
		LOG(llevInfo, "Unable to open %s as the logfile - will use stderr instead\n", settings.logfilename);
	}

	/* Global round ticker */
	global_round_tag = 1;
	/* Global race counter */
	global_race_counter = 0;

	exiting = 0;
	first_player = NULL;
	first_friendly_object = NULL;
	first_map = NULL;
	first_treasurelist = NULL;
	first_artifactlist = NULL;
	first_archetype = NULL;
	first_map = NULL;
	nroftreasures = 0;
	nrofartifacts = 0;
	nrofallowedstr = 0;
	init_strings();
	init_object_initializers();
	num_animations = 0;
	animations = NULL;
	animations_allocated = 0;
	init_defaults();
}

/**
 * Initializes global variables which can be changed by options.
 *
 * Called by init_library(). */
static void init_defaults()
{
	nroferrors = 0;
}

/**
 * Initializes first_map_path from the archetype collection. */
static void init_dynamic()
{
	archetype *at = first_archetype;

	while (at)
	{
		if (at->clone.type == MAP && EXIT_PATH(&at->clone))
		{
			strcpy(first_map_path, EXIT_PATH(&at->clone));
			return;
		}

		at = at->next;
	}

	LOG(llevError, "init_dynamic(): You need an archetype called 'map' and it has to contain start map.\n");
}

/**
 * Write out the current time to a file so time does not reset every
 * time the server reboots. */
void write_todclock()
{
	char filename[MAX_BUF];
	FILE *fp;

	snprintf(filename, sizeof(filename), "%s/clockdata", settings.localdir);

	if ((fp = fopen(filename, "w")) == NULL)
	{
		LOG(llevBug, "BUG: Cannot open %s for writing.\n", filename);
		return;
	}

	fprintf(fp, "%lu", todtick);
	fclose(fp);
}

/**
 * Initializes the gametime and TOD counters.
 *
 * Called by init_library(). */
static void init_clocks()
{
	char filename[MAX_BUF];
	FILE *fp;
	static int has_been_done = 0;

	if (has_been_done)
	{
		return;
	}
	else
	{
		has_been_done = 1;
	}

	snprintf(filename, sizeof(filename), "%s/clockdata", settings.localdir);
	LOG(llevDebug, "Reading clockdata from %s...", filename);

	if ((fp = fopen(filename, "r")) == NULL)
	{
		LOG(llevDebug, "Can't open %s.\n", filename);
		todtick = 0;
		write_todclock();
		return;
	}

	if (fscanf(fp, "%lu", &todtick))
	{
		LOG(llevDebug, "todtick=%lu\n", todtick);
	}

	fclose(fp);
}
