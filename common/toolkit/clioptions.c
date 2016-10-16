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
 * Command-line options API.
 *
 * @author Alex Tokar
 */

#include "string.h"
#include "clioptions.h"
#include "path.h"

/**
 * Iterate over all the CLI options.
 *
 * @param[out] _var Will contain the currently iterated CLI option.
 */
#define FOR_CLIOPTIONS_BEGIN(_var)                              \
do {                                                            \
    size_t _var##_i;                                            \
    clioption_t *_var;                                         \
    for (_var##_i = 0; _var##_i < clioptions_num; _var##_i++) { \
        _var = &clioptions[_var##_i];

/**
 * End iterating the CLI options.
 */
#define FOR_CLIOPTIONS_END()                            \
    }                                                   \
} while (0)

/**
 * A single command line option.
 */
struct clioption {
    /**
     * Option name, eg, 'verbose'.
     */
    const char *name;

    /**
     * Short option name, eg, 'v'.
     */
    const char *short_name;

    /**
     * Handler function for the option.
     */
    clioptions_handler_func handler_func;

    /**
     * Whether this option accepts an argument.
     */
    bool argument:1;

    /**
     * Whether the option is changeable at run-time.
     */
    bool changeable:1;

    /**
     * Brief description.
     */
    const char *desc_brief;

    /**
     * More detailed description.
     */
    const char *desc;

    /**
     * Last value configured.
     */
    char *value;
};

/**
 * All of the available command line options.
 */
static clioption_t *clioptions;
/**
 * Number of ::clioptions.
 */
static size_t clioptions_num;
/**
 * If true, process initialization has finished.
 */
static bool clioptions_runtime;

TOOLKIT_API(DEPENDS(logger), IMPORTS(string), IMPORTS(stringbuffer));

/**
 * Description of the --config command.
 */
static const char *clioptions_option_config_desc =
"Instead of specifying your options on the command line each time "
"you run the server, you can create a text file containing the "
"options, in the format of:\n\n"
"option = argument\n"
"help = True\n\n"
"Each option must be on its own line. Empty lines and lines "
"beginning with '#' are ignored. '\\n' strings in the argument "
"will be converted into literal newline characters.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_config (const char *arg,
                          char      **errmsg)
{
    if (!clioptions_load(arg, NULL)) {
        string_fmt(*errmsg, "Could not open configuration file: %s", arg);
        return false;
    }

    return true;
}

/**
 * Description of the --logfile command.
 */
static const char *clioptions_option_help_desc =
"Displays the help, listing available options, etc.\n\n"
"'--help=argument' can be used to get more detailed help about "
"specified option.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_help (const char *arg,
                        char      **errmsg)
{
    /* If we got an argument, try to look for the option and
     * display detailed info about it. */
    if (arg != NULL) {
        FOR_CLIOPTIONS_BEGIN(cli) {
            if (cli->desc == NULL) {
                continue;
            }

            if (strcmp(cli->name, arg) != 0) {
                continue;
            }

            LOG(INFO, "##### Option: --%s #####", cli->name);
            LOG(INFO, " ");

            for (const char *curr = cli->desc, *next;
                 (curr != NULL && (next = strchr(curr, '\n'))) || curr != NULL;
                 curr = next != NULL ? next + 1 : NULL) {
                char *cp = estrndup(curr, next - curr);
                LOG(INFO, "%s", cp);
                efree(cp);
            }

            exit(0);
        } FOR_CLIOPTIONS_END();

        LOG(INFO, "No such option '--%s'.", arg);
        exit(0);
    }

     /* Otherwise show brief information about all available options. */
    LOG(INFO, "List of available options:");
    LOG(INFO, " ");

    FOR_CLIOPTIONS_BEGIN(cli) {
        StringBuffer *sb = stringbuffer_new();

        if (cli->desc_brief == NULL) {
            continue;
        }

        if (cli->short_name != NULL) {
            stringbuffer_append_printf(sb, "-%s%s",
                                       cli->short_name,
                                       cli->argument ? " arg" : "");
        }

        if (cli->name != NULL) {
            if (stringbuffer_length(sb) > 0) {
                stringbuffer_append_string(sb, ", ");
            }

            stringbuffer_append_printf(sb, "--%s%s",
                                       cli->name,
                                       cli->argument ? "=arg" : "");
        }

        char *desc = stringbuffer_finish(sb);
        LOG(INFO, "    %s: %s", desc, cli->desc_brief);
        efree(desc);
    } FOR_CLIOPTIONS_END();

    LOG(INFO, " ");
    LOG(INFO, "Use '--help=option' to learn more about the specified option.");
    exit(0);

    /* Not reached */
    return true;
}

/**
 * Description of the --logfile command.
 */
static const char *clioptions_option_logfile_desc =
"All of the output that is normally written to stdout will also be "
"written to the specified file.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_logfile (const char *arg,
                           char      **errmsg)
{
    logger_open_log(arg);
    return true;
}

/**
 * Description of the --logger_filter_stdout command.
 */
static const char *clioptions_option_logger_filter_stdout_desc =
"All of the output that is normally written to stdout will also be "
"written to the specified file.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_logger_filter_stdout (const char *arg,
                                        char      **errmsg)
{
    logger_set_filter_stdout(arg);
    return true;
}

/**
 * Description of the --logger_filter_logfile command.
 */
static const char *clioptions_option_logger_filter_logfile_desc =
"All of the output that is normally written to stdout will also be "
"written to the specified file.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_logger_filter_logfile (const char *arg,
                                         char      **errmsg)
{
    logger_set_filter_logfile(arg);
    return true;
}

TOOLKIT_INIT_FUNC(clioptions)
{
    clioptions = NULL;
    clioptions_num = 0;
    clioptions_runtime = false;

    clioption_t *cli;
    CLIOPTIONS_CREATE_ARGUMENT(cli, config, "Read configuration from file");
    clioptions_enable_changeable(cli);

    CLIOPTIONS_CREATE(cli, help, "Displays this help");
    clioptions_set_short_name(cli, "h");

    CLIOPTIONS_CREATE_ARGUMENT(cli, logfile, "Sets the file to write log to.");
    clioptions_enable_changeable(cli);

    CLIOPTIONS_CREATE_ARGUMENT(cli,
                               logger_filter_stdout,
                               "Specify log levels filtering for stdout.");
    clioptions_enable_changeable(cli);

    CLIOPTIONS_CREATE_ARGUMENT(cli,
                               logger_filter_logfile,
                               "Specify log levels filtering for the logfile.");
    clioptions_enable_changeable(cli);
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(clioptions)
{
    FOR_CLIOPTIONS_BEGIN(cli) {
        if (cli->value != NULL) {
            efree(cli->value);
        }
    } FOR_CLIOPTIONS_END();
    if (clioptions != NULL) {
        efree(clioptions);
        clioptions = NULL;
    }

    clioptions_num = 0;
}
TOOLKIT_DEINIT_FUNC_FINISH

clioption_t *
clioptions_create (const char             *name,
                   clioptions_handler_func handler_func)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(name != NULL);

    /* Ensure that no option with the same long/short name exists. */
    FOR_CLIOPTIONS_BEGIN(cli) {
        if (strcmp(cli->name, name) == 0) {
            LOG(ERROR, "Attempting to add duplicate CLI option: %s",
                name);
            exit(1);
        }
    } FOR_CLIOPTIONS_END();

    clioptions = ereallocz(clioptions,
                           sizeof(*clioptions) * clioptions_num,
                           sizeof(*clioptions) * (clioptions_num + 1));
    clioption_t *cli = &clioptions[clioptions_num++];
    cli->name = name;
    cli->handler_func = handler_func;
    return cli;
}

const char *
clioptions_get (const char *name)
{
    HARD_ASSERT(name != NULL);

    FOR_CLIOPTIONS_BEGIN(cli) {
        if (strcmp(cli->name, name) == 0) {
            return cli->value;
        }
    } FOR_CLIOPTIONS_END();

    return NULL;
}

void
clioptions_set_short_name (clioption_t *cli,
                           const char  *short_name)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(cli != NULL);
    HARD_ASSERT(short_name != NULL);

    /* Ensure the CLI doesn't have a short name yet */
    HARD_ASSERT(cli->short_name == NULL);

    cli->short_name = short_name;
}

void
clioptions_set_description (clioption_t *cli,
                            const char  *desc_brief,
                            const char  *desc)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(cli != NULL);
    HARD_ASSERT(desc_brief != NULL);
    HARD_ASSERT(desc != NULL);

    /* Ensure the CLI doesn't have descriptions yet */
    HARD_ASSERT(cli->desc_brief == NULL);
    HARD_ASSERT(cli->desc == NULL);

    cli->desc_brief = desc_brief;
    cli->desc = desc;
}

void
clioptions_enable_argument (clioption_t *cli)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(cli != NULL);
    HARD_ASSERT(!cli->argument);
    cli->argument = true;
}

void
clioptions_enable_changeable (clioption_t *cli)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(cli != NULL);
    HARD_ASSERT(!cli->changeable);
    cli->changeable = true;
}

/**
 * Looks up a CLI option for the purposes of CLI arguments parsing.
 *
 * @param argc
 * Number of elements in argv.
 * @param argv
 * Variable length array of character pointers with the option/argument
 * combinations.
 * @param[out] idx
 * Current index inside argv[]. May be modified.
 * @param[out] cli_arg
 * Will contain argument if supplied and supported by the CLI option.
 * @return
 * CLI if found, NULL otherwise.
 */
static clioption_t *
clioptions_parse_find (int argc, char *argv[], int *idx, const char **cli_arg)
{
    *cli_arg = NULL;

    bool is_short_opt = strncmp(argv[*idx], "--", 2) != 0;
    FOR_CLIOPTIONS_BEGIN(cli) {
        char *arg = argv[*idx];

        if (is_short_opt) {
            if (cli->short_name == NULL) {
                continue;
            }

            arg += 1;
            if (strcmp(cli->short_name, arg) != 0) {
                continue;
            }

            if (cli->argument && (*idx) + 1 < argc) {
                *cli_arg = argv[++(*idx)];
            }
        } else {
            arg += 2;
            char *equals_loc = strchr(arg, '=');

            size_t end;
            if (equals_loc != NULL) {
                end = equals_loc - arg;
                *cli_arg = equals_loc + 1;
            } else {
                end = strlen(arg);
            }

            if (strncmp(cli->name, arg, end) != 0) {
                continue;
            }
        }

        if (cli->argument && (*cli_arg == NULL || **cli_arg == '\0')) {
            LOG(ERROR, "Option %s requires an argument.", argv[*idx]);
            exit(1);
        }

        return cli;
    } FOR_CLIOPTIONS_END();

    return NULL;
}

/**
 * Call the CLI handler.
 *
 * @param cli
 * CLI to call the handler for.
 * @param cli_arg
 * CLI argument.
 * @param[out] errmsg
 * Where to store the error message on failure.
 * @return
 * True on success, false on failure.
 */
static bool
clioptions_call_handler (clioption_t *cli, const char *cli_arg, char **errmsg)
{
    HARD_ASSERT(cli != NULL);
    HARD_ASSERT(errmsg != NULL);

    char *contents = NULL;
    if (cli_arg != NULL && *cli_arg == '<') {
        contents = path_file_contents(cli_arg + 1);
        if (contents == NULL) {
            string_fmt(*errmsg, "Cannot open %s", cli_arg + 1);
            return false;
        }

        cli_arg = contents;
    }

    if (!cli->handler_func(cli_arg, errmsg)) {
        if (contents != NULL) {
            efree(contents);
        }

        return false;
    }

    if (cli->value != NULL) {
        efree(cli->value);
    }

    if (contents != NULL) {
        cli->value = contents;
    } else if (cli_arg != NULL) {
        cli->value = estrdup(cli_arg);
    }

    return true;
}

void
clioptions_parse (int argc, char *argv[])
{
    TOOLKIT_PROTECT();

    /* Start at 1, as 0 is the program's name. */
    for (int i = 1; i < argc; i++) {
        if (*argv[i] == '\0') {
            continue;
        }

        const char *cli_arg;
        int old_i = i;
        clioption_t *cli = clioptions_parse_find(argc, argv, &i, &cli_arg);
        if (cli == NULL) {
            LOG(ERROR, "Unknown option: %s", argv[i]);
            exit(1);
        }

        if (cli->handler_func != NULL) {
            char *errmsg = NULL;

            if (!clioptions_call_handler(cli, cli_arg, &errmsg)) {
                LOG(ERROR, "%s: %s %s",
                    errmsg != NULL ? errmsg : "Failed to parse option",
                    argv[old_i],
                    i != old_i ? argv[i] : "");

                if (errmsg != NULL) {
                    efree(errmsg);
                }
            }
        }
    }

    clioptions_runtime = true;
}

bool
clioptions_load (const char *path, const char *category)
{
    HARD_ASSERT(path != NULL);

    TOOLKIT_PROTECT();

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return false;
    }

    LOG(INFO, "Loading configuration from %s", path);

    char category_cur[MAX_BUF];
    category_cur[0] = '\0';

    char buf[HUGE_BUF];
    while (fgets(buf, sizeof(buf), fp)) {
        char *cp = buf;
        string_skip_whitespace(cp);
        string_strip_newline(cp);

        /* Comment or blank line, skip. */
        if (*cp == '#' || *cp == '\0') {
            continue;
        }

        if (string_startswith(cp, "[") && string_endswith(cp, "]")) {
            snprintf(VS(category_cur), "%s", cp);
        } else if (category == NULL ||
                   strcasecmp(category, category_cur) == 0) {
            char *errmsg = NULL;
            if (!clioptions_load_str(cp, &errmsg)) {
                LOG(ERROR, "%s, file %s: %s",
                    errmsg != NULL ? errmsg : "Failed to load option",
                    path,
                    cp);

                if (errmsg != NULL) {
                    efree(errmsg);
                }
            }
        }
    }

    fclose(fp);

    return true;
}

bool
clioptions_load_str (const char *str, char **errmsg)
{
    HARD_ASSERT(str != NULL);
    HARD_ASSERT(errmsg != NULL);

    char *cp = estrdup(str);
    char **argv = NULL;
    bool ret = false;
    *errmsg = NULL;

    char *cps[2];
    if (string_split(cp, cps, arraysize(cps), '=') != arraysize(cps)) {
        *errmsg = estrdup("Option must be in the form of '<name> = <value>'");
        goto out;
    }

    string_whitespace_trim(cps[0]);
    string_whitespace_trim(cps[1]);
    string_newline_to_literal(cps[1]);

    char buf[HUGE_BUF];
    snprintf(buf, sizeof(buf), "--%s=%s", cps[0], cps[1]);

    argv = emalloc(sizeof(*argv) * 2);
    argv[1] = estrdup(buf);

    const char *cli_arg;
    int idx = 1;
    clioption_t *cli = clioptions_parse_find(2, argv, &idx, &cli_arg);
    if (cli == NULL) {
        *errmsg = estrdup("No such option");
        goto out;
    }

    if (cli->handler_func == NULL) {
        if (!clioptions_runtime) {
            ret = true;
        } else {
            *errmsg = estrdup("Option doesn't have a handler function");
        }

        goto out;
    }

    if (clioptions_runtime && !cli->changeable) {
        *errmsg = estrdup("Option is not changeable at runtime");
        goto out;
    }

    ret = clioptions_call_handler(cli, cli_arg, errmsg);

out:
    efree(cp);

    if (argv != NULL) {
        efree(argv[1]);
        efree(argv);
    }

    return ret;
}
