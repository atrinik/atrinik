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

#include <global.h>

/**
 * Name of the API.
 */
#define API_NAME clioptions

/**
 * If 1, the API has been initialized.
 */
static uint8 did_init = 0;

/**
 * All of the available command line options.
 */
static clioptions_struct *clioptions;

/**
 * Number of ::clioptions.
 */
static size_t clioptions_num;

/**
 * The --config command-line option.
 * @param arg The file to open for reading.
 */
static void clioptions_option_config(const char *arg)
{
    if (!clioptions_load_config(arg, "[General]")) {
        logger_print(LOG(ERROR), "Could not open file for reading: %s", arg);
        exit(1);
    }
}

/**
 * The --help command-line option.
 * @param arg Optional argument.
 */
static void clioptions_option_help(const char *arg)
{
    size_t i;

    if (arg) {
        char *curr, *next, *cp;
        
        for (i = 0; i < clioptions_num; i++) {
            if (strcmp(clioptions[i].longname, arg) != 0) {
                continue;
            }

            logger_print(LOG(INFO), "##### Option: --%s #####",
                         clioptions[i].longname);
            logger_print(LOG(INFO), " ");

            for (curr = clioptions[i].desc;
                 (curr && (next = strchr(curr, '\n'))) || curr;
                 curr = next ? next + 1 : NULL) {
                cp = estrndup(curr, next - curr);
                logger_print(LOG(INFO), "%s", cp);
                efree(cp);
            }

            break;
        }

        /* Didn't find the option. */
        if (i == clioptions_num) {
            logger_print(LOG(INFO), "No such option '--%s'.", arg);
        }
    }
    /* Otherwise brief information about all available options. */
    else {
        StringBuffer *sb;
        char *desc;

        logger_print(LOG(INFO), "List of available options:");
        logger_print(LOG(INFO), " ");

        for (i = 0; i < clioptions_num; i++) {
            sb = stringbuffer_new();

            if (clioptions[i].shortname) {
                stringbuffer_append_printf(
                    sb, "-%s%s", clioptions[i].shortname,
                    clioptions[i].argument ? " arg" : ""
                );
            }

            if (clioptions[i].longname) {
                if (stringbuffer_length(sb) > 0) {
                    stringbuffer_append_string(sb, ", ");
                }

                stringbuffer_append_printf(
                    sb, "--%s%s", clioptions[i].longname,
                    clioptions[i].argument ? "=arg" : ""
                );
            }

            desc = stringbuffer_finish(sb);
            logger_print(LOG(INFO), "    %s: %s",
                         desc, clioptions[i].desc_brief);
            efree(desc);
        }

        logger_print(LOG(INFO), " ");
        logger_print(LOG(INFO), "Use '--help=option' to learn more about the "
                                "specified option.");
    }

    exit(0);
}

/**
 * Initialize the command-line options API.
 * @internal
 */
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
            "Instead of specifying your options on the command line each time"
            " you run the server, you can create a text file containing the "
            "options, in the format of:\n\n"
            "option = argument\n"
            "help = True\n\n"
            "Each option must be on its own line. Empty lines and lines "
            "beginning with '#' are ignored. '\\n' strings in the argument "
            "will be converted into literal newline characters."
            );

        clioptions_add(
            "help",
            "h",
            clioptions_option_help,
            0,
            "Displays this help.",
            "Displays the help, listing available options, etc.\n\n"
            "'--help=argument' can be used to get more detailed help about "
            "specified option."
            );
    }
    TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the command-line options API.
 * @internal
 */
void toolkit_clioptions_deinit(void)
{
    TOOLKIT_DEINIT_FUNC_START(clioptions)
    {
        size_t i;

        for (i = 0; i < clioptions_num; i++) {
            if (clioptions[i].longname) {
                efree(clioptions[i].longname);
            }

            if (clioptions[i].shortname) {
                efree(clioptions[i].shortname);
            }

            efree(clioptions[i].desc_brief);
            efree(clioptions[i].desc);
        }

        if (clioptions) {
            efree(clioptions);
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
 * @param argument Whether the option accepts an argument or not. Can be NULL.
 * @param desc_brief Brief description of the option.
 * @param desc More detailed description of the option.
 */
void clioptions_add(const char *longname, const char *shortname,
                    clioptions_handler_func handle_func, uint8 argument,
                    const char *desc_brief, const char *desc)
{
    size_t i;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (longname == NULL && shortname == NULL) {
        logger_print(LOG(BUG), "Tried adding an option with neither longname "
                               "nor shortname.");
        return;
    }

    if (desc_brief == NULL) {
        logger_print(LOG(BUG), "Brief description must be set.");
        return;
    }

    if (desc == NULL) {
        logger_print(LOG(BUG), "Description must be set.");
        return;
    }

    /* Ensure that no option with the same long/short name exists. */
    for (i = 0; i < clioptions_num; i++) {
        if ((clioptions[i].longname != NULL && longname != NULL &&
                strcmp(clioptions[i].longname, longname) == 0) ||
            (clioptions[i].shortname != NULL && shortname != NULL &&
                strcmp(clioptions[i].shortname, shortname) == 0)) {
            logger_print(LOG(BUG), "Option already exists (longname: %s, "
                                   "shortname: %s).",
                         longname  != NULL ? longname  : "none",
                         shortname != NULL ? shortname : "none");
            return;
        }
    }

    clioptions = erealloc(
        clioptions, sizeof(*clioptions) * (clioptions_num + 1));
    clioptions[clioptions_num].longname =
        longname ? estrdup(longname) : NULL;
    clioptions[clioptions_num].shortname =
        shortname ? estrdup(shortname) : NULL;
    clioptions[clioptions_num].handle_func = handle_func;
    clioptions[clioptions_num].argument = argument;
    clioptions[clioptions_num].desc_brief = estrdup(desc_brief);
    clioptions[clioptions_num].desc = estrdup(desc);
    clioptions_num++;
}

/**
 * Parse CLI options from argv array.
 * @param argc Number of elements in argv.
 * @param argv Variable length array of character pointers with the
 * option/argument combinations.
 */
void clioptions_parse(int argc, char *argv[])
{
    int i;
    size_t opt;
    char *arg;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    /* Start at 1, as 0 is the program's name. */
    for (i = 1; i < argc; i++) {
        if (*argv[i] == '\0') {
            continue;
        }

        for (opt = 0; opt < clioptions_num; opt++) {
            /* Try to match long name CLI first. */
            if (strncmp(argv[i], "--", 2) == 0) {
                char *equals_loc;

                if (clioptions[opt].longname == NULL) {
                    continue;
                }

                equals_loc = strchr(argv[i] + 2, '=');

                if (strncmp(argv[i] + 2, clioptions[opt].longname,
                        equals_loc == NULL ? strlen(argv[1] + 2) : (size_t)
                        (equals_loc - (argv[i] + 2))) == 0) {
                    arg = equals_loc ? equals_loc + 1 : NULL;

                    if (clioptions[opt].argument && (!arg || *arg == '\0')) {
                        logger_print(
                            LOG(ERROR), "Option --%s requires an argument.",
                            clioptions[opt].longname);
                        exit(1);
                    }

                    if (clioptions[opt].handle_func != NULL) {
                        clioptions[opt].handle_func(arg);
                    }

                    break;
                }
            }
            /* Then short name CLI. */
            else if (strncmp(argv[i], "-", 1) == 0) {
                if (clioptions[opt].shortname == NULL) {
                    continue;
                }
                
                if (strcmp(argv[i] + 1, clioptions[opt].shortname) == 0) {
                    arg = clioptions[opt].argument &&
                          i + 1 < argc ? argv[++i] : NULL;

                    if (clioptions[opt].argument && (!arg || *arg == '\0')) {
                        logger_print(
                            LOG(ERROR), "Option -%s requires an argument.",
                            clioptions[opt].shortname);
                        exit(1);
                    }

                    if (clioptions[opt].handle_func != NULL) {
                        clioptions[opt].handle_func(arg);
                    }

                    break;
                }
            }
        }

        if (opt == clioptions_num) {
            logger_print(LOG(ERROR), "Unknown option %s.", argv[i]);
            exit(1);
        }
    }
}

/**
 * Load command-line options from config file.
 * @param path File to load from.
 * @param category Category of options to read; NULL for all.
 * @return 1 on success, 0 on failure.
 */
int clioptions_load_config(const char *path, const char *category)
{
    FILE *fp;
    char buf[HUGE_BUF], *end, category_cur[MAX_BUF], **argv;
    int argc, i;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    fp = fopen(path, "r");

    if (fp == NULL) {
        return 0;
    }

    argv = NULL;
    argc = 0;

    argv = erealloc(argv, sizeof(*argv) * (argc + 1));
    argv[argc] = estrdup("");
    argc++;

    category_cur[0] = '\0';

    while (fgets(buf, sizeof(buf) - 1, fp)) {
        end = strchr(buf, '\n');

        if (end) {
            *end = '\0';
        }

        /* Comment or blank line, skip. */
        if (*buf == '#' || *buf == '\0') {
            continue;
        }

        if (string_startswith(buf, "[") && string_endswith(buf, "]")) {
            strncpy(category_cur, buf, sizeof(category_cur) - 1);
            category_cur[sizeof(category_cur) - 1] = '\0';
        }
        else if (!category || strcasecmp(category, category_cur) == 0) {
            char *cps[2], cp[HUGE_BUF];

            if (string_split(buf, cps, arraysize(cps), '=') !=
                arraysize(cps)) {
                logger_print(LOG(BUG), "Invalid line: %s", buf);
                continue;
            }

            string_whitespace_trim(cps[0]);
            string_whitespace_trim(cps[1]);
            string_newline_to_literal(cps[1]);

            snprintf(cp, sizeof(cp), "--%s=%s", cps[0], cps[1]);
            argv = erealloc(argv, sizeof(*argv) * (argc + 1));
            argv[argc] = estrdup(cp);
            argc++;
        }
    }

    fclose(fp);

    clioptions_parse(argc, argv);

    for (i = 0; i < argc; i++) {
        efree(argv[i]);
    }

    efree(argv);

    return 1;
}
