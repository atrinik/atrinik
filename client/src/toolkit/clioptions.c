/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Command-line options API.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Name of the API. */
#define API_NAME clioptions

/**
 * If 1, the API has been initialized. */
static uint8 did_init = 0;

/**
 * All of the available command line options. */
static clioptions_struct *clioptions;

/**
 * Number of ::clioptions. */
static size_t clioptions_num;

/**
 * The --config command-line option.
 * @param arg The file to open for reading. */
void clioptions_option_config(const char *arg)
{
	FILE *fp;
	char buf[HUGE_BUF], longname[HUGE_BUF], argument[HUGE_BUF], **argv;
	int argc, i;

	TOOLKIT_FUNC_PROTECTOR(API_NAME);

	fp = fopen(arg, "r");

	if (!fp)
	{
		logger_print(LOG(ERROR), "Could not open '%s' for reading.", arg);
		exit(1);
	}

	argv = NULL;
	argc = 0;

	argv = realloc(argv, sizeof(*argv) * (argc + 1));
	argv[argc] = strdup("");
	argc++;

	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		/* Comment or blank line, skip. */
		if (*buf == '#' || *buf == '\n')
		{
			continue;
		}

		if (sscanf(buf, "%s = %[^\n]\n", longname, argument) == 2)
		{
			char cp[HUGE_BUF];

			string_newline_to_literal(argument);
			snprintf(cp, sizeof(cp), "--%s=%s", longname, argument);
			argv = realloc(argv, sizeof(*argv) * (argc + 1));
			argv[argc] = strdup(cp);
			argc++;
		}
	}

	clioptions_parse(argc, argv);

	for (i = 0; i < argc; i++)
	{
		free(argv[i]);
	}

	free(argv);

	fclose(fp);
}

/**
 * The --help command-line option.
 * @param arg Optional argument. */
static void clioptions_option_help(const char *arg)
{
	size_t i;

	if (arg)
	{
		for (i = 0; i < clioptions_num; i++)
		{
			if (strcmp(clioptions[i].longname, arg) == 0)
			{
				char *curr, *next, *cp;

				logger_print(LOG(INFO), "##### Option: --%s #####", clioptions[i].longname);
				logger_print(LOG(INFO), " ");

				for (curr = clioptions[i].desc; (curr && (next = strchr(curr, '\n'))) || curr; curr = next ? next + 1 : NULL)
				{
					cp = strndup(curr, next - curr);
					logger_print(LOG(INFO), "%s", cp);
					free(cp);
				}

				break;
			}
		}

		if (i == clioptions_num)
		{
			logger_print(LOG(INFO), "No such option '--%s'.", arg);
		}
	}
	/* Otherwise brief information about all available options. */
	else
	{
		StringBuffer *sb;
		char *desc;

		logger_print(LOG(INFO), "List of available options:");
		logger_print(LOG(INFO), " ");

		for (i = 0; i < clioptions_num; i++)
		{
			sb = stringbuffer_new();

			if (clioptions[i].shortname)
			{
				stringbuffer_append_printf(sb, "-%s%s", clioptions[i].shortname, clioptions[i].argument ? " arg" : "");
			}

			if (clioptions[i].longname)
			{
				if (stringbuffer_length(sb) > 0)
				{
					stringbuffer_append_string(sb, ", ");
				}

				stringbuffer_append_printf(sb, "--%s%s", clioptions[i].longname, clioptions[i].argument ? "=arg" : "");
			}

			desc = stringbuffer_finish(sb);
			logger_print(LOG(INFO), "    %s: %s", desc, clioptions[i].desc_brief);
			free(desc);
		}

		logger_print(LOG(INFO), " ");
		logger_print(LOG(INFO), "Use '--help=option' to learn more about the specified option.");
	}

	exit(0);
}

/**
 * Initialize the command-line options API.
 * @internal */
void toolkit_clioptions_init(void)
{
	TOOLKIT_INIT_FUNC_START(clioptions)
	{
		toolkit_import(logger);
		toolkit_import(string);
		toolkit_import(stringbuffer);

		clioptions = NULL;
		clioptions_num = 0;

		clioptions_add(
			"config",
			NULL,
			clioptions_option_config,
			1,
			"Reads configuration from a text file.",
			"Instead of specifying your options on the command line each time you run"
			"the server, you can create a text file containing the options, in the format"
			"of:\n\n"
			"option = argument\n"
			"help = True\n\n"
			"Each option must be on its own line. Empty lines and lines beginning with '#'"
			"are ignored. '\\n' strings in the argument will be converted into literal newline"
			"characters."
		);

		clioptions_add(
			"help",
			"h",
			clioptions_option_help,
			0,
			"Displays this help.",
			"Displays the help, listing available options, etc.\n\n"
			"'--help=argument' can be used to get more detailed help about the specified option."
		);
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the command-line options API.
 * @internal */
void toolkit_clioptions_deinit(void)
{
	TOOLKIT_DEINIT_FUNC_START(clioptions)
	{
		size_t i;

		for (i = 0; i < clioptions_num; i++)
		{
			if (clioptions[i].longname)
			{
				free(clioptions[i].longname);
			}

			if (clioptions[i].shortname)
			{
				free(clioptions[i].shortname);
			}

			free(clioptions[i].desc_brief);
			free(clioptions[i].desc);
		}

		if (clioptions)
		{
			free(clioptions);
			clioptions = NULL;
		}

		clioptions_num = 0;
	}
	TOOLKIT_DEINIT_FUNC_END()
}

/**
 * Add a command line option.
 * @param longname Long name of the option, can be NULL.
 * @param shortname Short name of the option, can be NULL.
 * @param handle_func The handler function for the option.
 * @param argument Whether the option accepts an argument or not.
 * @param desc_brief Brief description of the option.
 * @param desc More detailed description of the option. */
void clioptions_add(const char *longname, const char *shortname, clioptions_handler_func handle_func, uint8 argument, const char *desc_brief, const char *desc)
{
	size_t i;

	TOOLKIT_FUNC_PROTECTOR(API_NAME);

	if (!longname && !shortname)
	{
		logger_print(LOG(BUG), "Tried adding an option with neither longname nor shortname.");
		return;
	}

	for (i = 0; i < clioptions_num; i++)
	{
		if ((clioptions[i].longname && longname && strcmp(clioptions[i].longname, longname) == 0) || (clioptions[i].shortname && shortname && strcmp(clioptions[i].shortname, shortname) == 0))
		{
			logger_print(LOG(BUG), "Option already exists (longname: %s, shortname: %s).", longname ? longname : "none", shortname ? shortname : "none");
			return;
		}
	}

	clioptions = realloc(clioptions, sizeof(*clioptions) * (clioptions_num + 1));
	clioptions[clioptions_num].longname = longname ? strdup(longname) : NULL;
	clioptions[clioptions_num].shortname = shortname ? strdup(shortname) : NULL;
	clioptions[clioptions_num].handle_func = handle_func;
	clioptions[clioptions_num].argument = argument;
	clioptions[clioptions_num].desc_brief = strdup(desc_brief);
	clioptions[clioptions_num].desc = strdup(desc);
	clioptions_num++;
}

void clioptions_parse(int argc, char *argv[])
{
	int i;
	size_t opt;

	TOOLKIT_FUNC_PROTECTOR(API_NAME);

	for (i = 1; i < argc; i++)
	{
		if (*argv[i] == '\0')
		{
			continue;
		}

		for (opt = 0; opt < clioptions_num; opt++)
		{
			if (strncmp(argv[i], "--", 2) == 0)
			{
				char *equals_loc;

				if (clioptions[opt].longname && strncmp(argv[i] + 2, clioptions[opt].longname, (equals_loc = strchr(argv[i] + 2, '=')) == NULL ? strlen(argv[1] + 2) : (size_t) (equals_loc - (argv[i] + 2))) == 0)
				{
					char *arg;

					arg = equals_loc ? equals_loc + 1 : NULL;

					if (clioptions[opt].argument && (!arg || *arg == '\0'))
					{
						logger_print(LOG(ERROR), "Option --%s requires an argument.", clioptions[opt].longname);
						exit(1);
					}

					clioptions[opt].handle_func(arg);
					break;
				}
			}
			else if (strncmp(argv[i], "-", 1) == 0)
			{
				if (clioptions[opt].shortname && strcmp(argv[i] + 1, clioptions[opt].shortname) == 0)
				{
					char *arg;

					arg = clioptions[opt].argument && i + 1 < argc ? argv[++i] : NULL;

					if (clioptions[opt].argument && (!arg || *arg == '\0'))
					{
						logger_print(LOG(ERROR), "Option -%s requires an argument.", clioptions[opt].shortname);
						exit(1);
					}

					clioptions[opt].handle_func(arg);
					break;
				}
			}
		}

		if (opt == clioptions_num)
		{
			logger_print(LOG(ERROR), "Unknown option %s.", argv[i]);
			exit(1);
		}
	}
}
