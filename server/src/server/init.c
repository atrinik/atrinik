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
 * Server initialization, settings loading, command line handling and
 * such. */

#define INIT_C
#define EXTERN

#include <global.h>

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
	BALANCED_STAT_LOSS,
	/* This and the next 3 values are metaserver values */
	0,
	"",
	"",
	"",
	"",
	0,
	0,
	0,
	1.0f
};

/** World's darkness value. */
int world_darkness;

/** Time of day tick. */
unsigned long todtick;

/** Pointer to archetype that is used as effect when player levels up. */
archetype *level_up_arch = NULL;

/** Name of the archetype to use for the level up effect. */
#define ARCHETYPE_LEVEL_UP "level_up"

static void usage();
static void help();
static void init_beforeplay();
static void fatal_signal(int make_core);
static void init_signals();
static void dump_level_colors_table();
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
	shstr_cons.of_poison = add_string("of poison");
	shstr_cons.of_hideous_poison = add_string("of hideous poison");
}

/**
 * Free the string constants. */
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
	init_hash_table();
	init_globals();
	/* Inits the pooling memory manager and the new object system */
	init_mempools();
	init_block();
	LOG(llevInfo, "Atrinik Server, v%s\n", VERSION);
	LOG(llevInfo, "Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team.\n");
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
	pticks = 1;
	/* Global race counter */
	global_race_counter = 0;

	first_player = NULL;
	last_player = NULL;
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

/** @cond */

static void set_logfile(char *val)
{
	settings.logfilename = val;
}

static void call_version()
{
	version(NULL);
	exit(0);
}

static void showscores()
{
	hiscore_display(NULL, 9999, NULL);
	exit(0);
}

static void set_debug()
{
	settings.debug = llevDebug;
}

static void unset_debug()
{
	settings.debug = llevInfo;
}

static void set_mondebug()
{
	settings.debug = llevMonster;
}

static void set_dumpmon1()
{
	settings.dumpvalues = DUMP_VALUE_MONSTERS;
}

static void set_dumpmon2()
{
	settings.dumpvalues = DUMP_VALUE_ABILITIES;
}

static void set_dumpmon3()
{
	settings.dumpvalues = DUMP_VALUE_ARTIFACTS;
}

static void set_dumpmon4()
{
	settings.dumpvalues = DUMP_VALUE_SPELLS;
}

static void set_dumpmon5()
{
	settings.dumpvalues = DUMP_VALUE_SKILLS;
}

static void set_dumpmon6()
{
	settings.dumpvalues = DUMP_VALUE_RACES;
}

static void set_dumpmon7()
{
	settings.dumpvalues = DUMP_VALUE_ALCHEMY;
}

static void set_dumpmon8()
{
	settings.dumpvalues = DUMP_VALUE_GODS;
}

static void set_dumpmon9()
{
	settings.dumpvalues = DUMP_VALUE_ALCHEMY_COSTS;
}

static void set_dumpmon10()
{
	settings.dumpvalues = DUMP_VALUE_ARCHETYPES;
}

static void set_dumpmon11(char *name)
{
	settings.dumpvalues = DUMP_VALUE_MONSTER_TREASURE;
	settings.dumparg = name;
}

static void set_dumpmon12()
{
	settings.dumpvalues = DUMP_VALUE_LEVEL_COLORS;
}

static void set_spell_dump(char *arg)
{
	settings.dumpvalues = DUMP_VALUE_SPELLS;
	settings.dumparg = arg;
}

static void set_daemon()
{
	settings.daemonmode = 1;
}

static void set_watchdog()
{
	settings.watchdog = 1;
}

static void set_interactive()
{
	settings.interactive = 1;
}

static void set_datadir(char *path)
{
	settings.datadir = path;
}

static void set_localdir(char *path)
{
	settings.localdir = path;
}

static void set_mapdir(char *path)
{
	settings.mapdir = path;
}

static void set_archetypes(char *path)
{
	settings.archetypes = path;
}

static void set_treasures(char *path)
{
	settings.treasures = path;
}

static void set_uniquedir(char *path)
{
	settings.uniquedir = path;
}

static void set_tmpdir(char *path)
{
	settings.tmpdir = path;
}

static void showscoresparm(const char *data)
{
	hiscore_display(NULL, 9999, data);
	exit(0);
}

static void set_csport(const char *val)
{
	settings.csport = atoi(val);

#ifndef WIN32
	if (settings.csport <= 0 || settings.csport > 32765 || (settings.csport < 1024 && getuid() != 0))
	{
		LOG(llevError, "ERROR: %d is an invalid csport number.\n", settings.csport);
	}
#endif
}

static void stat_loss_on_death_true()
{
	settings.stat_loss_on_death = 1;
}

static void stat_loss_on_death_false()
{
	settings.stat_loss_on_death = 0;
}

static void balanced_stat_loss_true()
{
	settings.balanced_stat_loss = 1;
}

static void balanced_stat_loss_false()
{
	settings.balanced_stat_loss = 0;
}

static void set_unit_tests()
{
#if defined(HAVE_CHECK)
	settings.unit_tests = 1;
#else
	LOG(llevInfo, "\nThe server was built without the check unit testing framework.\nIf you want to run unit tests, you must first install this framework.\n");
	exit(0);
#endif
}

/** @endcond */

/** One command line option definition. */
struct Command_Line_Options
{
	/** How it is called on the command line */
	char *cmd_option;

	/** Number or args it takes */
	uint8 num_args;

	/** What pass this should be processed on. */
	uint8 pass;

	/**
	 * Function to call when we match this.
	 *
	 * If num_args is true, then that gets passed to the function,
	 * otherwise nothing is passed. */
	void (*func)();
};

/**
 * Valid command line options.
 *
 * The way this system works is pretty simple - parse_args takes the
 * options passed to the program and a pass number. If an option matches
 * both in name and in pass (and we have enough options), we call the
 * associated function. This makes writing a multi pass system very easy,
 * and it is very easy to add in new options. */
struct Command_Line_Options options[] =
{
	/* Pass 1 functions - Stuff that can/should be called before we actually
	 * initialize any data. */
	{"-h", 0, 1, help},
	/* Honor -help also, since it is somewhat common */
	{"-help", 0, 1, help},
	{"-v", 0, 1, call_version},
	{"-d", 0, 1, set_debug},
	{"+d", 0, 1, unset_debug},
	{"-mon", 0, 1, set_mondebug},
	{"-data",1,1, set_datadir},
	{"-local",1,1, set_localdir},
	{"-maps", 1, 1, set_mapdir},
	{"-arch", 1, 1, set_archetypes},
	{"-treasures", 1, 1, set_treasures},
	{"-uniquedir", 1, 1, set_uniquedir},
	{"-tmpdir", 1, 1, set_tmpdir},
	{"-log", 1, 1, set_logfile},

	/* Pass 2 functions.  Most of these could probably be in pass 1,
	 * as they don't require much of anything to bet set up. */
	{"-csport", 1, 2, set_csport},
	{"-detach", 0, 2, set_daemon},
	{"-watchdog", 0, 2, set_watchdog},
	{"-interactive", 0, 2, set_interactive},

	/* Start of pass 3 information. In theory, by pass 3, all data paths
	 * and defaults should have been set up.  */
	{"-o", 0, 3, compile_info},

	{"-m1", 0, 3, set_dumpmon1},
	{"-m2", 0, 3, set_dumpmon2},
	{"-m3", 0, 3, set_dumpmon3},
	{"-m4", 0, 3, set_dumpmon4},
	{"-m5", 0, 3, set_dumpmon5},
	{"-m6", 0, 3, set_dumpmon6},
	{"-m7", 0, 3, set_dumpmon7},
	{"-m8", 0, 3, set_dumpmon8},
	{"-m9", 0, 3, set_dumpmon9},
	{"-m10", 0, 3, set_dumpmon10},
	{"-m11", 1, 3, set_dumpmon11},
	{"-m12", 0, 3, set_dumpmon12},
	{"-spell", 1, 3, set_spell_dump},

	{"-tests", 0, 3, set_unit_tests},

	{"-s", 0, 3, showscores},
	{"-score", 1, 3, showscoresparm},
	{"-stat_loss_on_death", 0, 3, stat_loss_on_death_true},
	{"+stat_loss_on_death", 0, 3, stat_loss_on_death_false},
	{"-balanced_stat_loss", 0, 3, balanced_stat_loss_true},
	{"+balanced_stat_loss", 0, 3, balanced_stat_loss_false}
};

/**
 * Parse command line arguments.
 *
 * Note since this may be called before the library has been set up,
 * we don't use any of crossfires built in logging functions.
 * @param argc Length of argv.
 * @param argv[] Arguments.
 * @param pass Initialization pass arguments to use. */
static void parse_args(int argc, char *argv[], int pass)
{
	size_t i;
	int on_arg = 1;

	while (on_arg < argc)
	{
		for (i = 0; i < sizeof(options) / sizeof(struct Command_Line_Options); i++)
		{
			if (!strcmp(options[i].cmd_option, argv[on_arg]) || (argv[on_arg][0] == '-' && !strcmp(options[i].cmd_option, argv[on_arg] + 1)))
			{
				/* Found a matching option, but should not be processed on
				 * this pass.  Just skip over it */
				if (options[i].pass != pass)
				{
					on_arg += options[i].num_args + 1;
					break;
				}

				if (options[i].num_args)
				{
					if ((on_arg + options[i].num_args) >= argc)
					{
						LOG(llevSystem, "command line: %s requires an argument.\n", options[i].cmd_option);
						exit(1);
					}
					else
					{
						if (options[i].num_args == 1)
						{
							options[i].func(argv[on_arg + 1]);
						}

						if (options[i].num_args == 2)
						{
							options[i].func(argv[on_arg + 1],argv[on_arg + 2]);
						}

						on_arg += options[i].num_args + 1;
					}
				}
				/* takes no args */
				else
				{
					options[i].func();
					on_arg++;
				}

				break;
			}
		}

		if (i == sizeof(options) / sizeof(struct Command_Line_Options))
		{
			LOG(llevSystem, "Unknown option: %s\n", argv[on_arg]);
			usage();
			exit(1);
		}
	}
}

/**
 * This loads the settings file.
 *
 * There could be debate whether this should be here or in the common
 * directory - but since only the server needs this information, having
 * it here probably makes more sense. */
static void load_settings()
{
	char buf[MAX_BUF], *cp;
	int	has_val, comp;
	FILE *fp;

	snprintf(buf, sizeof(buf), "%s/%s", settings.localdir, SETTINGS);

	/* We don't require a settings file at current time, but down the road,
	 * there will probably be so many values that not having a settings file
	 * will not be a good thing. */
	if ((fp = open_and_uncompress(buf, 0, &comp)) == NULL)
	{
		LOG(llevBug, "BUG: No %s file found\n", SETTINGS);
		return;
	}

	while (fgets(buf, MAX_BUF-1, fp) != NULL)
	{
		if (buf[0] == '#')
		{
			continue;
		}

		/* eliminate newline */
		if ((cp = strrchr(buf, '\n')) != NULL)
		{
			*cp = '\0';
		}

		/* Skip over empty lines */
		if (buf[0] == 0)
		{
			continue;
		}

		/* Skip all the spaces and set them to nulls.  If not space,
		 * set cp to "" to make strcpy's and the like easier down below. */
		if ((cp = strchr(buf, ' ')) != NULL)
		{
			while (*cp == ' ')
			{
				*cp++ = 0;
			}

			has_val = 1;
		}
		else
		{
			cp = "";
			has_val = 0;
		}

		if (!strcasecmp(buf, "metaserver_notification"))
		{
			if (!strcasecmp(cp, "on") || !strcasecmp(cp, "true"))
			{
				settings.meta_on = 1;
			}
			else if (!strcasecmp(cp, "off") || !strcasecmp(cp, "false"))
			{
				settings.meta_on = 0;
			}
			else
			{
				LOG(llevBug, "BUG: load_settings(): Unknown value for metaserver_notification: %s\n", cp);
			}
		}
		else if (!strcasecmp(buf, "metaserver_server"))
		{
			if (has_val)
			{
				strcpy(settings.meta_server, cp);
			}
			else
			{
				LOG(llevBug, "BUG: load_settings(): metaserver_server must have a value.\n");
			}
		}
		else if (!strcasecmp(buf, "metaserver_host"))
		{
			if (has_val)
			{
				strcpy(settings.meta_host, cp);
			}
			else
			{
				LOG(llevBug, "BUG: load_settings(): metaserver_host must have a value.\n");
			}
		}
		else if (!strcasecmp(buf, "metaserver_name"))
		{
			if (has_val)
			{
				strcpy(settings.meta_name, cp);
			}
			else
			{
				LOG(llevBug, "BUG: load_settings(): metaserver_name must have a value.\n");
			}
		}
		else if (!strcasecmp(buf, "metaserver_comment"))
		{
			strcpy(settings.meta_comment, cp);
		}
		else if (!strcasecmp(buf, "item_power_factor"))
		{
			float tmp = atof(cp);

			if (tmp < 0)
			{
				LOG(llevError, "ERROR: load_settings(): item_power_factor must be a positive number (%f < 0).\n", tmp);
			}
			else
			{
				settings.item_power_factor = tmp;
			}
		}
		else
		{
			LOG(llevBug, "BUG: Unknown value in %s file: %s\n", SETTINGS, buf);
		}
	}

	close_and_delete(fp, comp);
}

/**
 * This is the main server initialization function.
 *
 * Called only once, when starting the program.
 * @param argc Length of argv.
 * @param argv Arguments. */
void init(int argc, char **argv)
{
	/* We don't want to be affected by players' umask */
	(void) umask(0);

	/* Must be done before init_signal() */
	init_done = 0;
	logfile = stderr;

	/* First arg pass - right now it does
	 * nothing, but in future specifying the
	 * LibDir in this pass would be reasonable*/
	parse_args(argc, argv, 1);

	/* Must be called early */
	init_library();
	/* Load the settings file */
	load_settings();
	init_world_darkness();
	parse_args(argc, argv, 2);

	SRANDOM(time(NULL));

	/* Sets up signal interceptions */
	init_signals();
	/* Sort command tables */
	init_commands();
	/* Load up the old temp map files */
	read_map_log();
	parse_args(argc, argv, 3);
	cftimer_init();
	init_regions();
	hiscore_init();

#ifndef WIN32
	if (settings.daemonmode)
	{
		become_daemon(settings.logfilename[0] == '\0' ? "logfile" : settings.logfilename);
	}
#endif

	init_beforeplay();
	init_ericserver();
	metaserver_init();
	load_bans_file();
	reset_sleep();
	init_done = 1;
}

/**
 * Show the usage. */
static void usage()
{
	LOG(llevInfo, "Usage: atrinik_server [-h] [-<flags>]...\n");
}

/**
 * Show help about the command line options. */
static void help()
{
	LOG(llevInfo, "Flags:\n");
	LOG(llevInfo, " -csport <port> Specifies the port to use for the new client/server code.\n");
	LOG(llevInfo, " -d          Turns on some debugging.\n");
	LOG(llevInfo, " +d          Turns off debugging (useful if server compiled with debugging\n");
	LOG(llevInfo, "             as default).\n");
	LOG(llevInfo, " -detach     The server will go in the background, closing all\n");
	LOG(llevInfo, "             connections to the tty.\n");
	LOG(llevInfo, " -h          Display this information.\n");
	LOG(llevInfo, " -log <file> Specifies which file to send output to.\n");
	LOG(llevInfo, "             Only has meaning if -detach is specified.\n");
	LOG(llevInfo, " -mon        Turns on monster debugging.\n");
	LOG(llevInfo, " -o          Prints out info on what was defined at compile time.\n");
	LOG(llevInfo, " -s          Display the high-score list.\n");
	LOG(llevInfo, " -score <name or class> Displays all high scores with matching name/class.\n");
	LOG(llevInfo, " -stat_loss_on_death - If set, player loses stat when they die.\n");
	LOG(llevInfo, " +stat_loss_on_death - If set, player does not lose a stat when they die.\n");
	LOG(llevInfo, " -balanced_stat_loss - If set, death stat depletion is balanced by level etc.\n");
	LOG(llevInfo, " +balanced_stat_loss - If set, ordinary death stat depletion is used.\n");
	LOG(llevInfo, " -v          Print version information.\n");
	LOG(llevInfo, " -data       Sets the lib dir (archetypes, treasures, etc.)\n");
	LOG(llevInfo, " -local      Read/write local data (hiscore, unique items, etc.)\n");
	LOG(llevInfo, " -maps       Sets the directory for maps.\n");
	LOG(llevInfo, " -arch       Sets the archetype file to use.\n");
	LOG(llevInfo, " -playerdir  Sets the directory for the player files.\n");
	LOG(llevInfo, " -treasures	 Sets the treasures file to use.\n");
	LOG(llevInfo, " -uniquedir  Sets the unique items/maps directory.\n");
	LOG(llevInfo, " -tmpdir     Sets the directory for temporary files (mostly maps.)\n");
	LOG(llevInfo, " -m1         Dumps out object settings for all monsters.\n");
	LOG(llevInfo, " -m2         Dumps out abilities for all monsters.\n");
	LOG(llevInfo, " -m3         Dumps out artificat information.\n");
	LOG(llevInfo, " -m4         Dumps out spell information.\n");
	LOG(llevInfo, " -m5         Dumps out skill information.\n");
	LOG(llevInfo, " -m6         Dumps out race information.\n");
	LOG(llevInfo, " -m7         Dumps out alchemy information.\n");
	LOG(llevInfo, " -m8         Dumps out gods information.\n");
	LOG(llevInfo, " -m9         Dumps out more alchemy information (formula checking).\n");
	LOG(llevInfo, " -m10        Dumps out all arches.\n");
	LOG(llevInfo, " -m11 <arch> Dumps out list of treasures for a monster.\n");
	LOG(llevInfo, " -m12        Dumps out level colors table.\n");
	LOG(llevInfo, " -spell <name> Dumps various information about the specified spell (if 'all', information about all spells available). This is useful when debugging/balancing spells.\n");
	exit(0);
}

/**
 * Initialize before playing. */
static void init_beforeplay()
{
	init_archetypes();
	init_spells();
	race_init();
	init_gods();
	init_readable();
	init_archetype_pointers();
	init_formulae();
	init_new_exp_system();

	if (settings.dumpvalues)
	{
		switch (settings.dumpvalues)
		{
			case DUMP_VALUE_MONSTERS:
				print_monsters();
				break;

			case DUMP_VALUE_ABILITIES:
				dump_abilities();
				break;

			case DUMP_VALUE_ARTIFACTS:
				dump_artifacts();
				break;

			case DUMP_VALUE_SPELLS:
				dump_spells();
				break;

			case DUMP_VALUE_SKILLS:
				dump_skills();
				break;

			case DUMP_VALUE_RACES:
				race_dump();
				break;

			case DUMP_VALUE_ALCHEMY:
				dump_alchemy();
				break;

			case DUMP_VALUE_GODS:
				dump_gods();
				break;

			case DUMP_VALUE_ALCHEMY_COSTS:
				dump_alchemy_costs();
				break;

			case DUMP_VALUE_ARCHETYPES:
				dump_all_archetypes();
				break;

			case DUMP_VALUE_MONSTER_TREASURE:
				dump_monster_treasure(settings.dumparg);
				break;

			case DUMP_VALUE_LEVEL_COLORS:
				dump_level_colors_table();
				break;
		}

		exit(0);
	}
}

/**
 * Dump compilation information, activated with the -o flag.
 *
 * It writes out information on how Imakefile and config.h was configured
 * at compile time. */
void compile_info()
{
	int i = 0;

	LOG(llevInfo, "Setup info:\n");
	LOG(llevInfo, "Non-standard include files:\n");
#if !defined (__STRICT_ANSI__) || defined (__sun__)
#if !defined (Mips)
	LOG(llevInfo, "<stdlib.h>\n");
	i = 1;
#endif

#if !defined (MACH) && !defined (sony)
	LOG(llevInfo, "<malloc.h>\n");
	i = 1;
#endif
#endif

#ifndef __STRICT_ANSI__
#ifndef MACH
	LOG(llevInfo, "<memory.h\n");
	i = 1;
#endif
#endif

#ifndef sgi
	LOG(llevInfo, "<sys/timeb.h>\n");
	i = 1;
#endif

	if (!i)
	{
		LOG(llevInfo, "(none)\n");
	}

	LOG(llevInfo, "Datadir:\t%s\n", settings.datadir);
	LOG(llevInfo, "Localdir:\t%s\n", settings.localdir);

	LOG(llevInfo, "Save player:\t<true>\n");
	LOG(llevInfo, "Save mode:\t%4.4o\n", SAVE_MODE);
	LOG(llevInfo, "Itemsdir:\t%s/%s\n", settings.localdir, settings.uniquedir);

	LOG(llevInfo, "Tmpdir:\t\t%s\n", settings.tmpdir);
	LOG(llevInfo, "Map timeout:\t%d\n", MAP_MAXTIMEOUT);

	LOG(llevInfo, "Objects:\tAllocated: %d, free: %d\n", pool_object->nrof_allocated[0], pool_object->nrof_free[0]);

#ifdef USE_CALLOC
	LOG(llevInfo, "Use_calloc:\t<true>\n");
#else
	LOG(llevInfo, "Use_calloc:\t<false>\n");
#endif

	LOG(llevInfo, "Max_time:\t%d\n", MAX_TIME);

	LOG(llevInfo, "Logfilename:\t%s (llev:%d)\n", settings.logfilename, settings.debug);
	LOG(llevInfo, "ObjectSize:\t%"FMT64U" (living: %"FMT64U")\n", (uint64) sizeof(object), (uint64) sizeof(living));
	LOG(llevInfo, "ObjectlinkSize:\t%"FMT64U"\n", (uint64) sizeof(objectlink));
	LOG(llevInfo, "MapStructSize:\t%"FMT64U"\n", (uint64) sizeof(mapstruct));
	LOG(llevInfo, "MapSpaceSize:\t%"FMT64U"\n", (uint64) sizeof(MapSpace));
	LOG(llevInfo, "PlayerSize:\t%"FMT64U"\n", (uint64) sizeof(player));
	LOG(llevInfo, "SocketSize:\t%"FMT64U"\n", (uint64) sizeof(socket_struct));
	LOG(llevInfo, "PartylistSize:\t%"FMT64U"\n", (uint64) sizeof(party_struct));
	LOG(llevInfo, "KeyValueSize:\t%"FMT64U"\n", (uint64) sizeof(key_value));

	LOG(llevInfo, "Setup info: Done.\n");
}

/** @cond */

static void rec_sigsegv(int i)
{
	(void) i;

	LOG(llevSystem, "\nSIGSEGV received.\n");
	fatal_signal(1);
}

static void rec_sigint(int i)
{
	(void) i;

	LOG(llevSystem, "\nSIGINT received.\n");

	if (init_done)
	{
		cleanup();
	}

	fatal_signal(0);
}

static void rec_sighup(int i)
{
	(void) i;

	LOG(llevSystem, "\nSIGHUP received\n");

	if (init_done)
	{
		emergency_save(0);
		cleanup();
	}

	exit(0);
}

static void rec_sigquit(int i)
{
	(void) i;

	LOG(llevSystem, "\nSIGQUIT received\n");
	fatal_signal(1);
}

static void rec_sigpipe(int i)
{
	(void) i;

	/* Keep running if we receive a sigpipe.  Crossfire should really be able
	 * to handle this signal (at least at some point in the future if not
	 * right now).  By causing a dump right when it is received, it is not
	 * doing much good.  However, if it core dumps later on, at least it can
	 * be looked at later on, and maybe fix the problem that caused it to
	 * dump core.  There is no reason that SIGPIPES should be fatal */
#if 1 && !defined(WIN32) /* ***win32: we don't want send SIGPIPE */
	LOG(llevSystem, "\nReceived SIGPIPE, ignoring...\n");
	/* hocky-pux clears signal handlers */
	signal(SIGPIPE, rec_sigpipe);
#else
	LOG(llevSystem, "\nSIGPIPE received, not ignoring...\n");
	/* Might consider to uncomment this line */
	fatal_signal(1);
#endif
}

static void rec_sigbus(int i)
{
	(void) i;

#ifdef SIGBUS
	LOG(llevSystem, "\nSIGBUS received\n");
	fatal_signal(1);
#endif
}

static void rec_sigterm(int i)
{
	(void) i;

	LOG(llevSystem,"\nSIGTERM received\n");
	fatal_signal(0);
}

/** @endcond */

/**
 * General signal handling. Will exit() in any case.
 *
 * @param make_core If set abort() instead of exit() to generate a core
 * dump. */
static void fatal_signal(int make_core)
{
	if (init_done)
	{
		emergency_save(0);
		clean_tmp_files();
	}

	if (make_core)
	{
		abort();
	}

	exit(0);
}

/**
 * Setup the signal handlers. */
static void init_signals()
{
#ifndef WIN32
	/* init_signals() remove signals */
	signal(SIGHUP, rec_sighup);
	signal(SIGINT, rec_sigint);
	signal(SIGQUIT, rec_sigquit);
	signal(SIGSEGV, rec_sigsegv);
	signal(SIGPIPE, rec_sigpipe);
#ifdef SIGBUS
	signal(SIGBUS, rec_sigbus);
#endif
	signal(SIGTERM, rec_sigterm);
#endif
}

/**
 * Dump level colors table. */
static void dump_level_colors_table()
{
	int i, ii, range, tmp;
	uint32 vx = 0, vc = 1000000;
	float xc = 38;

	for (i = 0; i < 100; i++)
	{
		vc += 100000;
		vx += vc;
		LOG(-1, "%4.2f, ", (((float) vc) / xc) / 125.0f);
		xc += 2;
	}

	LOG(-1, "\n");

	for (i = 1; i < 201; i++)
	{
		for (ii = i; ii > 1; ii--)
		{
			if (!calc_level_difference(i, ii))
			{
				break;
			}
		}

		level_color[i].yellow = i - (i / 33);
		level_color[i].blue = level_color[i].yellow - 1;
		level_color[i].orange = i + (i / 33) + 1;

		range = level_color[i].yellow - ii - 1;

		if (range < 2)
		{
			level_color[i].green = level_color[i].blue - 1;
			level_color[i].red = level_color[i].orange + 1;
			level_color[i].purple = level_color[i].orange + 2;
		}
		else
		{
			tmp = (int) ((double) range * 0.4);

			if (!tmp)
			{
				tmp = 1;
			}
			else if (tmp == range)
			{
				tmp--;
			}

			level_color[i].green = level_color[i].blue - (range - tmp);

			range = (int) ((double) range * 0.75);

			if (!range)
			{
				range = 0;
			}

			tmp = (int) ((double) range * 0.7);

			if (!tmp)
			{
				tmp = 1;
			}
			else if (tmp == range)
			{
				tmp--;
			}

			if (tmp == range)
			{
				range++;
			}

			level_color[i].red = level_color[i].orange + (range - tmp);
			level_color[i].purple = level_color[i].red + tmp;
		}

		LOG(llevSystem, "{ %d, %d, %d, %d, %d, %d},  lvl %d \n", ii + 1, level_color[i].green + 1, level_color[i].yellow, level_color[i].orange, level_color[i].red, level_color[i].purple, i);
	}
}
