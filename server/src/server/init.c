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
 * Server initialization, settings loading, command line handling and
 * such. */

#include <global.h>
#include <loader.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

void set_logfile(char *val)
{
	settings.logfilename = val;
}

void call_version()
{
	version(NULL);
	exit(0);
}

void showscores()
{
	display_high_score(NULL, 9999, NULL);
	exit(0);
}

void set_debug()
{
	settings.debug = llevDebug;
}

void unset_debug()
{
	settings.debug = llevInfo;
}

void set_mondebug()
{
	settings.debug = llevMonster;
}

void set_dumpmon1()
{
	settings.dumpvalues = DUMP_VALUE_MONSTERS;
}

void set_dumpmon2()
{
	settings.dumpvalues = DUMP_VALUE_ABILITIES;
}

void set_dumpmon3()
{
	settings.dumpvalues = DUMP_VALUE_ARTIFACTS;
}

void set_dumpmon4()
{
	settings.dumpvalues = DUMP_VALUE_SPELLS;
}

void set_dumpmon5()
{
	settings.dumpvalues = DUMP_VALUE_SKILLS;
}

void set_dumpmon6()
{
	settings.dumpvalues = DUMP_VALUE_RACES;
}

void set_dumpmon7()
{
	settings.dumpvalues = DUMP_VALUE_ALCHEMY;
}

void set_dumpmon8()
{
	settings.dumpvalues = DUMP_VALUE_GODS;
}

void set_dumpmon9()
{
	settings.dumpvalues = DUMP_VALUE_ALCHEMY_COSTS;
}

void set_dumpmon10()
{
	settings.dumpvalues = DUMP_VALUE_ARCHETYPES;
}

void set_dumpmon11(char *name)
{
	settings.dumpvalues = DUMP_VALUE_MONSTER_TREASURE;
	settings.dumparg = name;
}

void set_dumpmon12()
{
	settings.dumpvalues = DUMP_VALUE_LEVEL_COLORS;
}

void set_daemon()
{
	settings.daemonmode = 1;
}

void set_watchdog()
{
	settings.watchdog = 1;
}

void set_interactive()
{
	settings.interactive = 1;
}

void set_datadir(char *path)
{
	settings.datadir = path;
}

void set_localdir(char *path)
{
	settings.localdir = path;
}

void set_mapdir(char *path)
{
	settings.mapdir = path;
}

void set_archetypes(char *path)
{
	settings.archetypes = path;
}

void set_treasures(char *path)
{
	settings.treasures = path;
}

void set_uniquedir(char *path)
{
	settings.uniquedir = path;
}

void set_tmpdir(char *path)
{
	settings.tmpdir = path;
}

void showscoresparm(const char *data)
{
	display_high_score(NULL, 9999, data);
	exit(0);
}

void set_csport(const char *val)
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

static void use_permanent_experience_true()
{
	settings.use_permanent_experience = 1;
}

static void use_permanent_experience_false()
{
	settings.use_permanent_experience = 0;
}

static void balanced_stat_loss_true()
{
	settings.balanced_stat_loss = 1;
}

static void balanced_stat_loss_false()
{
	settings.balanced_stat_loss = 0;
}

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

	{"-s", 0, 3, showscores},
	{"-score", 1, 3, showscoresparm},
	{"-stat_loss_on_death", 0, 3, stat_loss_on_death_true},
	{"+stat_loss_on_death", 0, 3, stat_loss_on_death_false},
	{"-balanced_stat_loss", 0, 3, balanced_stat_loss_true},
	{"+balanced_stat_loss", 0, 3, balanced_stat_loss_false},
	{"-use_permanent_experience", 0, 3, use_permanent_experience_true},
	{"+use_permanent_experience", 0, 3, use_permanent_experience_false}
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
			if (!strcmp(options[i].cmd_option, argv[on_arg]))
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
		else if (!strcasecmp(buf, "metaserver_comment"))
		{
			strcpy(settings.meta_comment, cp);
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
 * @param argv[] Arguments. */
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
	init_word_darkness();
	parse_args(argc, argv, 2);

	SRANDOM(time(NULL));
	global_map_tag = (uint32) RANDOM();

	/* Write (C), check shutdown file */
	init_startup();
	/* Sets up signal interceptions */
	init_signals();
	/* Set up callback function pointers */
	setup_library();
	/* Sort command tables */
	init_commands();
	/* Load up the old temp map files */
	read_map_log();
	parse_args(argc, argv, 3);

#ifndef WIN32
	if (settings.daemonmode)
	{
		become_daemon(settings.logfilename[0] == '\0' ? "logfile" : settings.logfilename);
	}
#endif

	init_beforeplay();
	init_ericserver();
	metaserver_init();
	reset_sleep();
	init_done = 1;
}

void usage()
{
	LOG(llevInfo, "Usage: atrinik_server [-h] [-<flags>]...\n");
}

void help()
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
	LOG(llevInfo, " -use_permanent_experience - If set, player may gain permanent experience\n");
	LOG(llevInfo, " +use_permanent_experience - If set, player does not gain permanent experience\n");
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
	exit(0);
}

void init_beforeplay()
{
	/* If not called before, reads all archetypes from file */
	init_archetypes();
	/* If not called before, links archtypes used by spells */
	init_spells();
	/* overwrite race designations using entries in lib/races file */
	init_races();
	/* init linked list of gods from archs*/
	init_gods();
	/* inits useful arrays for readable texts */
	init_readable();
	/* Setup global pointers to archetypes */
	init_archetype_pointers();

	/* If not called before, reads formulae from file */
	init_formulae();

	/* If not called before, inits experience system */
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
				dump_races();
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
 * Checks if starting the server is allowed. */
void init_startup()
{
#ifdef SHUTDOWN_FILE
	char buf[MAX_BUF];
	FILE *fp;
	int comp;

	snprintf(buf, sizeof(buf), "%s/%s", settings.localdir, SHUTDOWN_FILE);

	if ((fp = open_and_uncompress(buf, 0, &comp)) != NULL)
	{
		while (fgets(buf, MAX_BUF - 1, fp) != NULL)
		{
			printf("%s", buf);
		}

		close_and_delete(fp, comp);
		exit(1);
	}
#endif
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

#ifdef SHUTDOWN_FILE
	LOG(llevInfo, "Shutdown file:\t%s/%s\n", settings.localdir, SHUTDOWN_FILE);
#endif

	LOG(llevInfo, "Save player:\t<true>\n");
	LOG(llevInfo, "Save mode:\t%4.4o\n", SAVE_MODE);
	LOG(llevInfo, "Itemsdir:\t%s/%s\n", settings.localdir, settings.uniquedir);

#ifdef USE_CHECKSUM
	LOG(llevInfo, "Use checksum:\t<true>\n");
#else
	LOG(llevInfo, "Use checksum:\t<false>\n");
#endif

	LOG(llevInfo, "Tmpdir:\t\t%s\n", settings.tmpdir);
	LOG(llevInfo, "Map timeout:\t%d\n", MAP_MAXTIMEOUT);

#ifdef MAP_RESET
	LOG(llevInfo, "Map reset:\t<true>\n");
#else
	LOG(llevInfo, "Map reset:\t<false>\n");
#endif

	LOG(llevInfo, "Max objects:\t%d (used:%d free:%d)\n", MAX_OBJECTS, mempools[POOL_OBJECT].nrof_used, mempools[POOL_OBJECT].nrof_free);

#ifdef USE_CALLOC
	LOG(llevInfo, "Use_calloc:\t<true>\n");
#else
	LOG(llevInfo, "Use_calloc:\t<false>\n");
#endif

#ifdef SHOP_LISTINGS
	LOG(llevInfo, "Shop listings:\t<true>\n");
#else
	LOG(llevInfo, "Shop listings:\t<false>\n");
#endif
	LOG(llevInfo, "Max_time:\t%d\n", MAX_TIME);

	LOG(llevInfo, "Logfilename:\t%s (llev:%d)\n", settings.logfilename, settings.debug);
	LOG(llevInfo, "ObjectSize:\t%d (living: %d)\n", sizeof(object), sizeof(living));
	LOG(llevInfo, "MapStructSize:\t%d\n", sizeof(mapstruct));
	LOG(llevInfo, "MapSpaceSize:\t%d\n", sizeof(MapSpace));
	LOG(llevInfo, "PlayerSize:\t%d\n", sizeof(player));
	LOG(llevInfo, "SocketSize:\t%d\n", sizeof(NewSocket));

	LOG(llevInfo, "Setup info: Done.\n");
}

/* Signal handlers: */

void rec_sigsegv(int i)
{
	(void) i;

	LOG(llevSystem, "\nSIGSEGV received.\n");
	fatal_signal(1);
}

void rec_sigint(int i)
{
	(void) i;

	LOG(llevSystem, "\nSIGINT received.\n");
	fatal_signal(0);
}

void rec_sighup(int i)
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

void rec_sigquit(int i)
{
	(void) i;

	LOG(llevSystem, "\nSIGQUIT received\n");
	fatal_signal(1);
}

void rec_sigpipe(int i)
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

void rec_sigbus(int i)
{
	(void) i;

#ifdef SIGBUS
	LOG(llevSystem, "\nSIGBUS received\n");
	fatal_signal(1);
#endif
}

void rec_sigterm(int i)
{
	(void) i;

	LOG(llevSystem,"\nSIGTERM received\n");
	fatal_signal(0);
}

/**
 * General signal handling. Will exit() in any case.
 *
 * @param make_core If set abort() instead of exit() to generate a core
 * dump. */
void fatal_signal(int make_core)
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
void init_signals()
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
 * Set up the function pointers which will point back from the library
 * into the server. */
void setup_library()
{
	set_emergency_save(emergency_save);
	set_clean_tmp_files(clean_tmp_files);
	set_remove_friendly_object(remove_friendly_object);
	set_update_buttons(update_buttons);
	set_draw_info(new_draw_info);
	set_container_unlink(container_unlink);
	set_move_apply(move_apply);
	set_monster_check_apply(monster_check_apply);
	set_move_teleporter(move_teleporter);
	set_move_firewall(move_firewall);
	set_move_creator(move_creator);
	set_trap_adjust(trap_adjust);
	set_esrv_send_item(esrv_send_item);
	set_esrv_del_item(esrv_del_item);
	set_esrv_update_item(esrv_update_item);
	set_info_map(new_info_map);
	set_dragon_gain_func(dragon_ability_gain);

	setup_poolfunctions(POOL_PLAYER, NULL, (chunk_destructor)free_player);
}

/**
 * Add corpse to race list.
 * @param race_name Race name
 * @param op Archetype of the corpse. */
static void add_corpse_to_racelist(const char *race_name, archetype *op)
{
	racelink *race;

	if (!op || !race_name)
	{
		return;
	}

	race = find_racelink(race_name);

	/* If we don't have this race, just skip the corpse. */
	if (race)
	{
		race->corpse = op;
	}
}

/**
 * Initialize races by looking through all the archetypes and checking if
 * the archetype is a @ref MONSTER or @ref PLAYER.
 *
 * We use object::sub_type1 as selector - every monster of race X will be
 * added to race list. */
void init_races()
{
	archetype *at, *tmp;
	racelink *list;
	static int init_done = 0;

	if (init_done)
	{
		return;
	}

	init_done = 1;

	first_race = NULL;
	LOG(llevDebug, "Init races... ");

	for (at = first_archetype; at != NULL; at = at->next)
	{
		if (at->clone.type == MONSTER || at->clone.type == PLAYER)
		{
			add_to_racelist(at->clone.race, &at->clone);
		}
	}

	/* Now search for corpses and add them to the race list */
	for (at = first_archetype; at != NULL; at = at->next)
	{
		if (at->clone.type == CONTAINER && at->clone.sub_type1 == ST1_CONTAINER_CORPSE)
		{
			add_corpse_to_racelist(at->clone.race, at);
		}
	}

	/* Last action: for all races without a special defined corpse add
	 * our corpse_default arch to it. */
	tmp = find_archetype("corpse_default");

	if (!tmp)
	{
		LOG(llevError, "ERROR: init_races: Can't find corpse_default in archetypes.\n");
	}

	for (list = first_race; list; list = list->next)
	{
		if (!list->corpse)
		{
			list->corpse = tmp;
		}
	}

#ifdef DEBUG
	dump_races();
#endif

	LOG(llevDebug, "\ndone.\n");
}

/**
 * Dumps all race information. */
void dump_races()
{
	racelink *list;
	objectlink *tmp;

	for (list = first_race; list; list = list->next)
	{
		LOG(llevInfo, "\nRACE %s (%s - %d member): ", list->name, list->corpse->name, list->nrof);

		for (tmp = list->member; tmp; tmp = tmp->next)
		{
			LOG(llevInfo, "%s(%d), ", tmp->ob->arch->name, tmp->ob->sub_type1);
		}
	}
}

/**
 * Add an object to the racelist.
 * @param race_name Race name.
 * @param op What object to add to the race. */
void add_to_racelist(const char *race_name, object *op)
{
	racelink *race;

	if (!op || !race_name)
	{
		return;
	}

	race = find_racelink(race_name);

	/* Add in a new race list */
	if (!race)
	{
		/* We need this for treasure generation (slaying race) */
		global_race_counter++;
		race = get_racelist();
		race->next = first_race;
		first_race = race;
		FREE_AND_COPY_HASH(race->name, race_name);
	}

	if (race->member->ob)
	{
		objectlink *tmp = get_objectlink();
		tmp->next=race->member;
		race->member = tmp;
	}

	race->nrof++;
	race->member->ob = op;
}

/**
 * Create a new ::racelink structure.
 * @return Empty structure. */
racelink *get_racelist()
{
	racelink *list;

	list = (racelink *) malloc(sizeof(racelink));
	list->name = NULL;
	list->corpse = NULL;
	list->nrof = 0;
	list->member = get_objectlink();
	list->next = NULL;

	return list;
}

/**
 * Frees all race related information. */
void free_racelist()
{
	racelink *list, *next;

	for (list = first_race; list;)
	{
		next = list->next;
		free(list);
		list = next;
	}
}

/**
 * Dump level colors table. */
void dump_level_colors_table()
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
