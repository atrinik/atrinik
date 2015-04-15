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
 * Server initialization. */

#include <global.h>
#include <packet.h>

/**
 * The server's settings. */
struct settings_struct settings;

/** The shared constants. */
shstr_constants shstr_cons;

/** World's darkness value. */
int world_darkness;

/** Time of day tick. */
unsigned long todtick;

/** Pointer to archetype that is used as effect when player levels up. */
archetype *level_up_arch = NULL;

/** The starting map. */
char first_map_path[MAX_BUF];

/** Name of the archetype to use for the level up effect. */
#define ARCHETYPE_LEVEL_UP "level_up"

static void init_beforeplay(void);
static void init_dynamic(void);
static void init_clocks(void);

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

    for (i = 0; i < nrof_strings; i++) {
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

    if (params && sscanf(params, "%ld", &new_speed) == 1) {
        set_max_time(new_speed);
        reset_sleep();
        logger_print(LOG(INFO), "The speed has been changed to %ld.", max_time);
        draw_info(COLOR_GRAY, NULL, "You feel a sudden and inexplicable change in the fabric of time and space...");
    } else {
        logger_print(LOG(INFO), "Current speed is: %ld, default speed is: %d.", max_time, MAX_TIME);
    }
}

static void console_command_speed_multiplier(const char *params)
{
    int new_speed_multiplier;

    if (params != NULL && sscanf(params, "%d", &new_speed_multiplier) == 1) {
        set_max_time_multiplier(new_speed_multiplier);
        logger_print(LOG(INFO), "The speed multiplier has been changed to %d.",
                max_time_multiplier);
        draw_info(COLOR_GRAY, NULL, "You feel a sudden and inexplicable change "
                "in the fabric of time and space...");
    } else {
        logger_print(LOG(INFO), "Current speed multiplier is: %d, default "
                "speed multiplier is: %d.", max_time_multiplier,
                MAX_TIME_MULTIPLIER);
    }
}

/**
 * Free all data before exiting. */
void cleanup(void)
{
    cache_remove_all();
    remove_plugins();
    player_deinit();
    account_deinit();
    free_all_maps();
    free_style_maps();
    free_all_archs();
    free_all_treasures();
    free_all_images();
    free_all_newserver();
    free_all_readable();
    free_all_anim();
    free_strings();
    race_free();
    regions_free();
    objectlink_deinit();
    object_deinit();
    ban_deinit();
    party_deinit();
    toolkit_deinit();
    free_object_loader();
    free_random_map_loader();
    free_map_header_loader();
}

static void clioptions_option_unit(const char *arg)
{
    settings.unit_tests = 1;
}

static void clioptions_option_worldmaker(const char *arg)
{
    settings.world_maker = 1;
}

static void clioptions_option_version(const char *arg)
{
    version(NULL);
    exit(0);
}

static void clioptions_option_port(const char *arg)
{
    int val;

    val = atoi(arg);

    if (val <= 0 || val > UINT16_MAX) {
        logger_print(LOG(ERROR), "%d is an invalid port number.", val);
        exit(1);
    }

    settings.port = val;
}

static void clioptions_option_logfile(const char *arg)
{
    logger_open_log(arg);
}

static void clioptions_option_libpath(const char *arg)
{
    strncpy(settings.libpath, arg, sizeof(settings.libpath) - 1);
    settings.libpath[sizeof(settings.libpath) - 1] = '\0';
}

static void clioptions_option_datapath(const char *arg)
{
    strncpy(settings.datapath, arg, sizeof(settings.datapath) - 1);
    settings.datapath[sizeof(settings.datapath) - 1] = '\0';
}

static void clioptions_option_mapspath(const char *arg)
{
    strncpy(settings.mapspath, arg, sizeof(settings.mapspath) - 1);
    settings.mapspath[sizeof(settings.mapspath) - 1] = '\0';
}

static void clioptions_option_httppath(const char *arg)
{
    strncpy(settings.httppath, arg, sizeof(settings.httppath) - 1);
    settings.httppath[sizeof(settings.httppath) - 1] = '\0';
}

static void clioptions_option_metaserver_url(const char *arg)
{
    strncpy(settings.metaserver_url, arg, sizeof(settings.metaserver_url) - 1);
    settings.metaserver_url[sizeof(settings.metaserver_url) - 1] = '\0';
}

static void clioptions_option_server_host(const char *arg)
{
    strncpy(settings.server_host, arg, sizeof(settings.server_host) - 1);
    settings.server_host[sizeof(settings.server_host) - 1] = '\0';
}

static void clioptions_option_server_name(const char *arg)
{
    strncpy(settings.server_name, arg, sizeof(settings.server_name) - 1);
    settings.server_name[sizeof(settings.server_name) - 1] = '\0';
}

static void clioptions_option_server_desc(const char *arg)
{
    strncpy(settings.server_desc, arg, sizeof(settings.server_desc) - 1);
    settings.server_desc[sizeof(settings.server_desc) - 1] = '\0';
}

static void clioptions_option_magic_devices_level(const char *arg)
{
    int val;

    val = atoi(arg);

    if (val < INT8_MIN || val > INT8_MAX) {
        logger_print(LOG(ERROR), "Invalid value for argument of --magic_devices_level (%d).", val);
        exit(1);
    }

    settings.magic_devices_level = val;
}

static void clioptions_option_item_power_factor(const char *arg)
{
    settings.item_power_factor = atof(arg);
}

static void clioptions_option_python_reload_modules(const char *arg)
{
    if (KEYWORD_IS_TRUE(arg)) {
        settings.python_reload_modules = 1;
    } else if (KEYWORD_IS_FALSE(arg)) {
        settings.python_reload_modules = 0;
    }
}

static void clioptions_option_default_permission_groups(const char *arg)
{
    if (strcmp(arg, "None") == 0) {
        settings.default_permission_groups[0] = '\0';
    } else {
        strncpy(settings.default_permission_groups, arg, sizeof(settings.default_permission_groups) - 1);
        settings.default_permission_groups[sizeof(settings.default_permission_groups) - 1] = '\0';
    }
}

static void clioptions_option_allowed_chars(const char *arg)
{
    char word[MAX_BUF], *cps[2], *cmd;
    size_t type, pos;
    const char *allowed_chars_names[ALLOWED_CHARS_NUM] = {"account", "charname", "password"};
    int lower, upper;

    pos = 0;

    if (!string_get_word(arg, &pos, ' ', word, sizeof(word), 0)) {
        logger_print(LOG(ERROR), "Invalid argument for allowed_chars option: %s", arg);
        return;
    }

    if (string_split(word, cps, arraysize(cps), ':') != arraysize(cps)) {
        logger_print(LOG(ERROR), "Invalid word in allowed_chars option: %s", word);
        return;
    }

    for (type = 0; type < ALLOWED_CHARS_NUM; type++) {
        if (strcmp(cps[0], allowed_chars_names[type]) == 0) {
            break;
        }
    }

    if (type == ALLOWED_CHARS_NUM) {
        logger_print(LOG(ERROR), "Invalid allowed_chars option type: %s", cps[0]);
        return;
    }

    if (sscanf(cps[1], "%d-%d", &lower, &upper) != 2) {
        logger_print(LOG(ERROR), "Lower/upper bounds for allowed_chars option in invalid format: %s", cps[1]);
        return;
    }

    settings.limits[type][0] = lower;
    settings.limits[type][1] = upper;
    settings.allowed_chars[type][0] = '\0';

    while (string_get_word(arg, &pos, ' ', word, sizeof(word), 0)) {
        if (string_startswith(word, "[") && string_endswith(word, "]")) {
            cmd = string_sub(word, 1, -1);

            if (string_startswith(cmd, ":") && string_endswith(cmd, ":")) {
                char start, end;

                if (strcmp(cmd, ":alphalower:") == 0) {
                    start = 'a';
                    end = 'z';
                } else if (strcmp(cmd, ":alphaupper:") == 0) {
                    start = 'A';
                    end = 'Z';
                } else if (strcmp(cmd, ":numeric:") == 0) {
                    start = '0';
                    end = '9';
                } else if (strcmp(cmd, ":print:") == 0) {
                    start = '!';
                    end = '}';
                } else if (strcmp(cmd, ":space:") == 0) {
                    start = end = ' ';
                } else {
                    start = end = '\0';
                }

                if (start != '\0' && end != '\0') {
                    char *chars;

                    chars = string_create_char_range(start, end);
                    strncat(settings.allowed_chars[type], chars, sizeof(settings.allowed_chars[type]) - strlen(settings.allowed_chars[type]) - 1);
                    efree(chars);
                }
            } else {
                strncat(settings.allowed_chars[type], cmd, sizeof(settings.allowed_chars[type]) - strlen(settings.allowed_chars[type]) - 1);
            }

            efree(cmd);
        }
    }
}

static void clioptions_option_control_allowed_ips(const char *arg)
{
    strncpy(settings.control_allowed_ips, arg, sizeof(settings.control_allowed_ips) - 1);
    settings.control_allowed_ips[sizeof(settings.control_allowed_ips) - 1] = '\0';
}

static void clioptions_option_control_player(const char *arg)
{
    strncpy(settings.control_player, arg, sizeof(settings.control_player) - 1);
    settings.control_player[sizeof(settings.control_player) - 1] = '\0';
}

static void clioptions_option_recycle_tmp_maps(const char *arg)
{
    settings.recycle_tmp_maps = 1;
}

static void clioptions_option_http_url(const char *arg)
{
    snprintf(settings.http_url, sizeof(settings.http_url), "%s", arg);
}

static void clioptions_option_logger_filter_stdout(const char *arg)
{
    logger_set_filter_stdout(arg);
}

static void clioptions_option_logger_filter_logfile(const char *arg)
{
    logger_set_filter_logfile(arg);
}

static void clioptions_option_speed(const char *arg)
{
    set_max_time(atol(arg));
}

static void clioptions_option_speed_multiplier(const char *arg)
{
    set_max_time_multiplier(atoi(arg));
}

/**
 * It is vital that init_library() is called by any functions using this
 * library.
 *
 * If you want to lessen the size of the program using the library, you
 * can replace the call to init_library() with init_globals() and
 * init_function_pointers(). Good idea to also call init_vars() and
 * init_hash_table() if you are doing any object loading. */
static void init_library(int argc, char *argv[])
{
    toolkit_import(signals);

    toolkit_import(clioptions);
    toolkit_import(console);
    toolkit_import(datetime);
    toolkit_import(logger);
    toolkit_import(math);
    toolkit_import(memory);
    toolkit_import(mempool);
    toolkit_import(packet);
    toolkit_import(path);
    toolkit_import(porting);
    toolkit_import(shstr);
    toolkit_import(string);
    toolkit_import(stringbuffer);

    /* Add console commands. */
    console_command_add(
            "shutdown",
            console_command_shutdown,
            "Shuts down the server.",
            "Shuts down the server, saving all the data and disconnecting all players.\n\n"
            "All of the used memory is freed, if possible."
            );

    console_command_add(
            "speed",
            console_command_speed,
            "Changes the server's speed.",
            "Changes the speed of the server, which in turn affects how quickly everything is processed."
            "Without an argument, shows the current speed and the default speed."
            );

    console_command_add(
            "speed_multiplier",
            console_command_speed_multiplier,
            "Changes the server's speed multiplier.",
            "Changes the speed multiplier of the server, which in turn affects how quickly everything is processed."
            "Without an argument, shows the current speed multiplier and the default speed multiplier."
            );

    /* Add command-line options. */
    clioptions_add(
            "unit",
            NULL,
            clioptions_option_unit,
            0,
            "Runs the unit tests.",
            "Runs the unit tests."
            );

    clioptions_add(
            "worldmaker",
            NULL,
            clioptions_option_worldmaker,
            0,
            "Generates the region maps.",
            "Generates the region maps using the world maker module.\n\n"
            );

    clioptions_add(
            "version",
            NULL,
            clioptions_option_version,
            0,
            "Displays the server version.",
            "Displays the server version."
            );

    clioptions_add(
            "logfile",
            NULL,
            clioptions_option_logfile,
            1,
            "Sets the file to write log to.",
            "All of the output that is normally written to stdout will also be written to the specified file."
            );

    clioptions_add(
            "port",
            NULL,
            clioptions_option_port,
            1,
            "Sets the port to use.",
            "Sets the port to use for server/client communication."
            );

    clioptions_add(
            "libpath",
            NULL,
            clioptions_option_libpath,
            1,
            "Read-only data files location.",
            "Where the read-only files such as the collected treasures, artifacts,"
            "archetypes etc reside."
            );

    clioptions_add(
            "datapath",
            NULL,
            clioptions_option_datapath,
            1,
            "Read and write data files location.",
            "Where to read and write player data, unique maps, etc."
            );

    clioptions_add(
            "mapspath",
            NULL,
            clioptions_option_mapspath,
            1,
            "Map files location.",
            "Where the maps are."
            );

    clioptions_add(
            "httppath",
            NULL,
            clioptions_option_httppath,
            1,
            "HTTP files location.",
            "Where the HTTP server files are."
            );

    clioptions_add(
            "metaserver_url",
            NULL,
            clioptions_option_metaserver_url,
            1,
            "URL of the metaserver.",
            "URL of the metaserver."
            );

    clioptions_add(
            "server_host",
            NULL,
            clioptions_option_server_host,
            1,
            "Hostname of the server.",
            "Hostname of the server."
            );

    clioptions_add(
            "server_name",
            NULL,
            clioptions_option_server_name,
            1,
            "Name of the server.",
            "Name of the server."
            );

    clioptions_add(
            "server_desc",
            NULL,
            clioptions_option_server_desc,
            1,
            "Description about the server.",
            "Text that describes the server in a few sentences."
            );

    clioptions_add(
            "magic_devices_level",
            NULL,
            clioptions_option_magic_devices_level,
            1,
            "Magic devices level for players.",
            "Adjustment to maximum magical device level the player may use."
            );

    clioptions_add(
            "item_power_factor",
            NULL,
            clioptions_option_item_power_factor,
            1,
            "Item power factor.",
            "item_power_factor is the relation of how the players equipped item_power"
            "total relates to their overall level. If 1.0, then sum of the character's"
            "equipped item's item_power can not be greater than their overall level."
            "If 2.0, then that sum can not exceed twice the character's overall level."
            "By setting this to a high enough value, you can effectively disable"
            "the item_power code."
            );

    clioptions_add(
            "python_reload_modules",
            NULL,
            clioptions_option_python_reload_modules,
            1,
            "Whether to reload Python modules.",
            "Whether to reload Python user modules (eg Interface.py and the like)"
            "each time a Python script executes. If enabled, executing scripts will"
            "be slower, but allows for easy development of modules. This should not"
            "be enabled on a production server."
            );

    clioptions_add(
            "default_permission_groups",
            NULL,
            clioptions_option_default_permission_groups,
            1,
            "Permission groups applied to all players.",
            "Comma-delimited list of permission groups that every player will be"
            "able to access, eg, '[MOD],[DEV]'. 'None' is the same as not using"
            "the option in the first place, ie, no default permission groups."
            );

    clioptions_add(
            "allowed_chars",
            NULL,
            clioptions_option_allowed_chars,
            1,
            "",
            ""
            );

    clioptions_add(
            "control_allowed_ips",
            NULL,
            clioptions_option_control_allowed_ips,
            1,
            "",
            ""
            );

    clioptions_add(
            "control_player",
            NULL,
            clioptions_option_control_player,
            1,
            "",
            ""
            );

    clioptions_add(
            "recycle_tmp_maps",
            NULL,
            clioptions_option_recycle_tmp_maps,
            0,
            "If enabled, reuse temporary map files across server runs.",
            "Set this if you want the temporary maps to be saved and reused "
            "across Atrinik runs. This can be especially useful for single player "
            "servers, but even holds use for multiplayer servers."
            );

    clioptions_add(
            "http_server",
            NULL,
            NULL,
            1,
            "Whether to bring up the simple HTTP server.",
            "If 'on', will bring up the simple HTTP server for serving the game "
            "server's data files to clients. Use 'off' to disable and configure "
            "http_url option to your own HTTP server as you see fit."
            );

    clioptions_add(
            "http_url",
            NULL,
            clioptions_option_http_url,
            1,
            "URL of the HTTP server.",
            "URL pointing to the HTTP server."
            );

    clioptions_add(
            "logger_filter_stdout",
            NULL,
            clioptions_option_logger_filter_stdout,
            1,
            "",
            ""
            );

    clioptions_add(
            "logger_filter_logfile",
            NULL,
            clioptions_option_logger_filter_logfile,
            1,
            "",
            ""
            );

    clioptions_add(
            "speed",
            NULL,
            clioptions_option_speed,
            1,
            "",
            ""
            );

    clioptions_add(
            "speed_multiplier",
            NULL,
            clioptions_option_speed_multiplier,
            1,
            "",
            ""
            );

    memset(&settings, 0, sizeof(settings));

    clioptions_load_config("server.cfg", "[General]");

    if (path_exists("server-custom.cfg")) {
        clioptions_load_config("server-custom.cfg", "[General]");
    }

    if (argv != NULL) {
        clioptions_parse(argc, argv);
    }

    toolkit_import(commands);

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
    account_init();

    /* init some often used default archetypes */
    if (level_up_arch == NULL) {
        level_up_arch = find_archetype(ARCHETYPE_LEVEL_UP);
    }

    if (!level_up_arch) {
        logger_print(LOG(BUG), "Can't find '%s' arch", ARCHETYPE_LEVEL_UP);
    }
}

/**
 * Initializes all global variables.
 * Might use environment variables as default for some of them. */
void init_globals(void)
{
    /* Global round ticker */
    global_round_tag = 1;

    first_player = NULL;
    last_player = NULL;
    first_map = NULL;
    first_treasurelist = NULL;
    first_artifactlist = NULL;
    first_archetype = NULL;
    first_region = NULL;
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

    while (at) {
        if (at->clone.type == MAP && EXIT_PATH(&at->clone)) {
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

    snprintf(filename, sizeof(filename), "%s/clockdata", settings.datapath);

    if ((fp = fopen(filename, "w")) == NULL) {
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

    if (has_been_done) {
        return;
    } else {
        has_been_done = 1;
    }

    snprintf(filename, sizeof(filename), "%s/clockdata", settings.datapath);

    if ((fp = fopen(filename, "r")) == NULL) {
        logger_print(LOG(DEBUG), "Can't open %s.", filename);
        todtick = 0;
        write_todclock();
        return;
    }

    if (fscanf(fp, "%lu", &todtick)) {
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

    /* Must be called early */
    init_library(argc, argv);
    init_world_darkness();

    /* Load up the old temp map files */
    read_map_log();
    regions_init();
    hiscore_init();

    init_beforeplay();
    init_ericserver();
    metaserver_init();
    load_bans_file();
    statistics_init();
    reset_sleep();
    init_plugins();
}

/**
 * Initialize before playing. */
static void init_beforeplay(void)
{
    init_archetypes();
    init_spells();
    race_init();
    init_readable();
    init_archetype_pointers();
    init_new_exp_system();
}
