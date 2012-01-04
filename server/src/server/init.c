/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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

#include <global.h>

/**
 * You unfortunately need to looking in include/global.h to see what these
 * correspond to. */
struct Settings settings =
{
	/* Logfile */
	"",
	/* Client/server port */
	CSPORT,
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
	".",
	"",
	0,
	0,
	0,
	0,
	10,
	1.0f
};

/** The shared constants. */
shstr_constants shstr_cons;

/** World's darkness value. */
int world_darkness;

/** Time of day tick. */
unsigned long todtick;

/** Pointer to archetype that is used as effect when player levels up. */
archetype *level_up_arch = NULL;

/** Ignores signals until init_done is true. */
long init_done;

/** Number of treasures. */
long nroftreasures;
/** Number of artifacts. */
long nrofartifacts;
/** Number of allowed treasure combinations. */
long nrofallowedstr;

/** The starting map. */
char first_map_path[MAX_BUF];

/** Name of the archetype to use for the level up effect. */
#define ARCHETYPE_LEVEL_UP "level_up"

static void usage();
static void help();
static void init_beforeplay();
static void init_environ();
static void init_dynamic();
static void init_clocks();

/**
 * Initialize the ::shstr_cons structure. */
static void init_strings(void)
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

	shstr_cons.restore_grace = add_string("restore grace");
	shstr_cons.restore_hitpoints = add_string("restore hitpoints");

	shstr_cons.player_info = add_string("player_info");
	shstr_cons.BANK_GENERAL = add_string("BANK_GENERAL");
	shstr_cons.of_poison = add_string("of poison");
	shstr_cons.of_hideous_poison = add_string("of hideous poison");
}

/**
 * Free the string constants. */
void free_strings(void)
{
	int nrof_strings = sizeof(shstr_cons) / sizeof(const char *);
	const char **ptr = (const char **) &shstr_cons;
	int i = 0;

	for (i = 0; i < nrof_strings; i++)
	{
		FREE_ONLY_HASH(ptr[i]);
	}
}

static void console_command_shutdown(const char *params)
{
	server_shutdown();
}

static void console_command_speed(const char *params)
{
	long int new_speed;

	if (params && sscanf(params, "%ld", &new_speed) == 1)
	{
		set_max_time(new_speed);
		reset_sleep();
		logger_print(LOG(INFO), "The speed has been changed to %ld.", max_time);
		draw_info_flags(NDI_ALL, COLOR_GRAY, NULL, "You feel a sudden and inexplicable change in the fabric of time and space...");
	}
	else
	{
		logger_print(LOG(INFO), "Current speed is: %ld, default speed is: %d.", max_time, MAX_TIME);
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
void init_library(void)
{
	toolkit_import(console);
	toolkit_import(logger);
	toolkit_import(math);
	toolkit_import(mempool);
	toolkit_import(packet);
	toolkit_import(path);
	toolkit_import(porting);
	toolkit_import(shstr);
	toolkit_import(signals);
	toolkit_import(string);
	toolkit_import(stringbuffer);

	console_command_add(
		"shutdown",
		console_command_shutdown,
		"Shuts down the server",
		"Shuts down the server, saving all the data and disconnecting all players.\n\n"
		"All of the used memory is freed, if possible."
	);

	console_command_add(
		"speed",
		console_command_speed,
		"Changes the server's speed",
		"Changes the speed of the server, which in turn affects how quickly everything is processed."
		"Without an argument, shows the current speed and the default speed."
	);

	signals_set_handler_func(cleanup);

	init_environ();
	init_globals();
	objectlink_init();
	object_init();
	player_init();
	ban_init();
	party_init();
	init_block();
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
		logger_print(LOG(BUG), "Can't find '%s' arch", ARCHETYPE_LEVEL_UP);
	}
}

/**
 * Initializes values from the environmental variables.
 *
 * Needs to be called very early, since command line options should
 * overwrite these if specified. */
static void init_environ(void)
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
 * Initializes all global variables.
 * Might use environment variables as default for some of them. */
void init_globals(void)
{
	if (settings.logfilename[0] != '\0')
	{
		logger_open_log(settings.logfilename);
	}

	/* Global round ticker */
	global_round_tag = 1;

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
	object_methods_init();
}

/**
 * Initializes first_map_path from the archetype collection. */
static void init_dynamic(void)
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

	logger_print(LOG(ERROR), "You need an archetype called 'map' and it has to contain start map.");
}

/**
 * Write out the current time to a file so time does not reset every
 * time the server reboots. */
void write_todclock(void)
{
	char filename[MAX_BUF];
	FILE *fp;

	snprintf(filename, sizeof(filename), "%s/clockdata", settings.localdir);

	if ((fp = fopen(filename, "w")) == NULL)
	{
		logger_print(LOG(BUG), "Cannot open %s for writing.", filename);
		return;
	}

	fprintf(fp, "%lu", todtick);
	fclose(fp);
}

/**
 * Initializes the gametime and TOD counters.
 *
 * Called by init_library(). */
static void init_clocks(void)
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

	if ((fp = fopen(filename, "r")) == NULL)
	{
		logger_print(LOG(DEBUG), "Can't open %s.", filename);
		todtick = 0;
		write_todclock();
		return;
	}

	if (fscanf(fp, "%lu", &todtick))
	{
	}

	fclose(fp);
}

/** @cond */

static void set_logfile(char *val)
{
	settings.logfilename = val;
}

static void call_version(void)
{
	version(NULL);
	exit(0);
}

static void showscores(void)
{
	hiscore_display(NULL, 9999, NULL);
	exit(0);
}

static void set_watchdog(void)
{
	settings.watchdog = 1;
}

static void set_interactive(void)
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
		logger_print(LOG(ERROR), "%d is an invalid csport number.", settings.csport);
		exit(1);
	}
#endif
}

static void stat_loss_on_death_true(void)
{
	settings.stat_loss_on_death = 1;
}

static void stat_loss_on_death_false(void)
{
	settings.stat_loss_on_death = 0;
}

static void balanced_stat_loss_true(void)
{
	settings.balanced_stat_loss = 1;
}

static void balanced_stat_loss_false(void)
{
	settings.balanced_stat_loss = 0;
}

static void set_unit_tests(void)
{
#if defined(HAVE_CHECK)
	settings.unit_tests = 1;
#else
	logger_print(LOG(INFO), "The server was built without the check unit testing framework. If you want to run unit tests, you must first install this framework.");
	exit(0);
#endif
}

static void set_world_maker(const char *data)
{
#if defined(HAVE_WORLD_MAKER)
	settings.world_maker = 1;

	if (data)
	{
		strncpy(settings.world_maker_dir, data, sizeof(settings.world_maker_dir) - 1);
		settings.world_maker_dir[sizeof(settings.world_maker_dir) - 1] = '\0';
	}
#else
	(void) data;
	logger_print(LOG(INFO), "The server was built without the world maker module.");
	exit(0);
#endif
}

/** @endcond */

/** One command line option definition. */
typedef struct Command_Line_Options
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
} Command_Line_Options;

/**
 * Valid command line options.
 *
 * The way this system works is pretty simple - parse_args takes the
 * options passed to the program and a pass number. If an option matches
 * both in name and in pass (and we have enough options), we call the
 * associated function. This makes writing a multi pass system very easy,
 * and it is very easy to add in new options. */
static struct Command_Line_Options options[] =
{
	/* Pass 1 functions - Stuff that can/should be called before we actually
	 * initialize any data. */
	{"-h", 0, 1, help},
	/* Honor -help also, since it is somewhat common */
	{"-help", 0, 1, help},
	{"-v", 0, 1, call_version},
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
	{"-watchdog", 0, 2, set_watchdog},
	{"-interactive", 0, 2, set_interactive},

	/* Start of pass 3 information. In theory, by pass 3, all data paths
	 * and defaults should have been set up.  */
	{"-tests", 0, 3, set_unit_tests},
	{"-world_maker", 1, 3, set_world_maker},

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
						logger_print(LOG(SYSTEM), "command line: %s requires an argument.", options[i].cmd_option);
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
			logger_print(LOG(SYSTEM), "Unknown option: %s", argv[on_arg]);
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
static void load_settings(void)
{
	char buf[MAX_BUF], *cp;
	int	has_val;
	FILE *fp;

	snprintf(buf, sizeof(buf), "%s/%s", settings.localdir, SETTINGS);
	fp = fopen(buf, "rb");

	if (!fp)
	{
		logger_print(LOG(BUG), "No %s file found", SETTINGS);
		return;
	}

	while (fgets(buf, MAX_BUF - 1, fp) != NULL)
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
				logger_print(LOG(BUG), "Unknown value for metaserver_notification: %s", cp);
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
				logger_print(LOG(BUG), "metaserver_server must have a value.");
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
				logger_print(LOG(BUG), "metaserver_host must have a value.");
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
				logger_print(LOG(BUG), "metaserver_name must have a value.");
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
				logger_print(LOG(ERROR), "item_power_factor must be a positive number (%f < 0).", tmp);
				exit(1);
			}
			else
			{
				settings.item_power_factor = tmp;
			}
		}
		else if (!strcasecmp(buf, "magic_devices_level"))
		{
			settings.magic_devices_level = atoi(cp);
		}
		else if (!strcasecmp(buf, "client_maps"))
		{
			if (has_val)
			{
				strcpy(settings.client_maps_url, cp);
			}
			else
			{
				logger_print(LOG(BUG), "client_maps must have a value.");
			}
		}
		else
		{
			logger_print(LOG(BUG), "Unknown value in %s file: %s", SETTINGS, buf);
		}
	}

	fclose(fp);
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

	/* Sort command tables */
	init_commands();
	/* Load up the old temp map files */
	read_map_log();
	parse_args(argc, argv, 3);
	init_regions();
	hiscore_init();

	init_beforeplay();
	init_ericserver();
	metaserver_init();
	load_bans_file();
	statistics_init();
	reset_sleep();
	init_done = 1;
}

/**
 * Show the usage. */
static void usage(void)
{
	logger_print(LOG(INFO), "Usage: atrinik_server [-h] [-<flags>]...");
}

/**
 * Show help about the command line options. */
static void help(void)
{
	logger_print(LOG(INFO), "Flags:");
	logger_print(LOG(INFO), " -csport <port> Specifies the port to use for the new client/server code.");
	logger_print(LOG(INFO), " -d          Turns on some debugging.");
	logger_print(LOG(INFO), " +d          Turns off debugging (useful if server compiled with debugging");
	logger_print(LOG(INFO), "             as default).");
	logger_print(LOG(INFO), " -h, -help   Display this information.");
	logger_print(LOG(INFO), " -log <file> Specifies which file to send output to.");
	logger_print(LOG(INFO), "             Only has meaning if -detach is specified.");
	logger_print(LOG(INFO), " -s          Display the high-score list.");
	logger_print(LOG(INFO), " -score <name or class> Displays all high scores with matching name/class.");
	logger_print(LOG(INFO), " -stat_loss_on_death - If set, player loses stat when they die.");
	logger_print(LOG(INFO), " +stat_loss_on_death - If set, player does not lose a stat when they die.");
	logger_print(LOG(INFO), " -balanced_stat_loss - If set, death stat depletion is balanced by level etc.");
	logger_print(LOG(INFO), " +balanced_stat_loss - If set, ordinary death stat depletion is used.");
	logger_print(LOG(INFO), " -v          Print version information.");
	logger_print(LOG(INFO), " -data       Sets the lib dir (archetypes, treasures, etc.)");
	logger_print(LOG(INFO), " -local      Read/write local data (hiscore, unique items, etc.)");
	logger_print(LOG(INFO), " -maps       Sets the directory for maps.");
	logger_print(LOG(INFO), " -arch       Sets the archetype file to use.");
	logger_print(LOG(INFO), " -treasures  Sets the treasures file to use.");
	logger_print(LOG(INFO), " -uniquedir  Sets the unique items/maps directory.");
	logger_print(LOG(INFO), " -tmpdir     Sets the directory for temporary files (mostly maps.)");

#if defined(HAVE_CHECK)
	logger_print(LOG(INFO), " -tests      Runs unit tests.");
#endif

#if defined(HAVE_WORLD_MAKER)
	logger_print(LOG(INFO), " -world_maker <path> Generates region maps and stores them in the specified path.");
#endif

	logger_print(LOG(INFO), " -watchdog   Enables sending datagrams to an external watchdog program.");
	logger_print(LOG(INFO), " -interactive Enables interactive mode. Type 'help' in console for more information.");
	logger_print(LOG(INFO), " -ts         If enabled, all log entries will be prefixed with UNIX timestamp.");

	exit(0);
}

/**
 * Initialize before playing. */
static void init_beforeplay(void)
{
	init_archetypes();
	init_spells();
	race_init();
	init_gods();
	init_readable();
	init_archetype_pointers();
	init_new_exp_system();
}
