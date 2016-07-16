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
 * Server initialization.
 */

#include <global.h>
#include <packet.h>
#include <toolkit_string.h>
#include <faction.h>
#include <arch.h>
#include <artifact.h>
#include <ban.h>
#include <loader.h>
#include <player.h>
#include <object_methods.h>
#include <clioptions.h>
#include <curl.h>
#include <server.h>
#include <socket_crypto.h>
#include <path.h>
#include <resources.h>

/**
 * The server's settings.
 */
struct settings_struct settings;

/** The shared constants. */
shstr_constants shstr_cons;

/** World's darkness value. */
int world_darkness;

/** Time of day tick. */
unsigned long todtick;

/** The starting map. */
char first_map_path[MAX_BUF];

/** Enter X coordinate on the starting map. */
int first_map_x;

/** Enter Y coordinate on the starting map. */
int first_map_y;

/** Name of the archetype to use for the level up effect. */
#define ARCHETYPE_LEVEL_UP "level_up"

static void init_beforeplay(void);
static void init_dynamic(void);
static void init_clocks(void);

/**
 * Initialize the ::shstr_cons structure.
 */
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
 * Free the string constants.
 */
void free_strings(void)
{
    int nrof_strings = sizeof(shstr_cons) / sizeof(const char *);
    const char **ptr = (const char **) &shstr_cons;
    int i = 0;

    for (i = 0; i < nrof_strings; i++) {
        FREE_ONLY_HASH(ptr[i]);
    }
}

/**
 * Free the server settings.
 */
static void
free_settings (void)
{
    if (settings.server_cert != NULL) {
        efree(settings.server_cert);
    }

    if (settings.server_cert_sig != NULL) {
        efree(settings.server_cert_sig);
    }
}

static void console_command_shutdown(const char *params)
{
    server_shutdown();
}

static void
console_command_config (const char *params)
{
    if (params == NULL) {
        LOG(INFO,
            "Usage: 'config <name> = <value>' to write config, "
            "'config <name>' to read config");
        return;
    }

    if (strchr(params, '=') == NULL) {
        const char *value = clioptions_get(params);
        if (value == NULL) {
            LOG(INFO, "No such option: %s", params);
        } else {
            LOG(INFO, "Configuration: %s = %s", params, value);
        }
    } else {
        char *errmsg;
        if (clioptions_load_str(params, &errmsg)) {
            LOG(INFO, "Configuration successful.");
        } else {
            LOG(INFO, "Configuration failed: %s",
                errmsg != NULL ? errmsg : "<no error message>");
            if (errmsg != NULL) {
                efree(errmsg);
            }
        }
    }
}

/**
 * Dump the active objects list, or just the number of objects on the list.
 *
 * @param params
 * Parameters from the console.
 */
static void
console_command_active_objects (const char *params)
{
    bool show_list = params != NULL && strcmp(params, "list") == 0;
    if (show_list) {
        LOG(INFO, "=== Active objects list ===");
    }

    uint64_t num = 0;
    for (object *tmp = active_objects; tmp != NULL; tmp = tmp->active_next) {
        num++;
        if (show_list) {
            LOG(INFO, "%s", object_get_str(tmp));
        }
    }

    LOG(INFO, "Total number of active objects: %" PRIu64, num);
}

/**
 * Free all data before exiting.
 */
void cleanup(void)
{
    cache_remove_all();
    remove_plugins();
    player_deinit();
    account_deinit();
    resources_deinit();
    free_all_maps();
    free_style_maps();
    arch_deinit();
    free_all_treasures();
    artifact_deinit();
    free_all_images();
    free_socket_images();
    free_all_readable();
    free_all_anim();
    free_strings();
    race_free();
    regions_free();
    objectlink_deinit();
    object_deinit();
    metaserver_deinit();
    party_deinit();
    free_settings();
    toolkit_deinit();
    free_object_loader();
    free_random_map_loader();
    free_map_header_loader();
}

/**
 * Description of the --unit command.
 */
static const char *clioptions_option_unit_desc =
"Runs the unit tests. Resulting logs will be stored in 'tests/**/*.out', with "
"XML results in 'tests/**/*.xml'.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_unit (const char *arg,
                        char      **errmsg)
{
    settings.unit_tests = 1;
    return true;
}

/**
 * Description of the --plugin_unit command.
 */
static const char *clioptions_option_plugin_unit_desc =
"Runs the plugin unit tests. Resulting XMLs will be stored in "
"'tests/unit/plugins/**/*.xml'.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_plugin_unit (const char *arg,
                               char      **errmsg)
{
    settings.plugin_unit_tests = 1;

    if (arg != NULL) {
        snprintf(VS(settings.plugin_unit_test), "%s", arg);
    }

    return true;
}

/**
 * Description of the --worldmaker command.
 */
static const char *clioptions_option_worldmaker_desc =
"Generates the region maps using the world maker module.\n\n"
"This should be done before starting up production servers.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_worldmaker (const char *arg,
                              char      **errmsg)
{
    settings.world_maker = 1;
    return true;
}

/**
 * Description of the --no_console command.
 */
static const char *clioptions_option_no_console_desc =
"Disables the interactive console. Useful when debugging or "
"running the server non-interactively.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_no_console (const char *arg,
                              char      **errmsg)
{
    settings.no_console = true;
    return true;
}

/**
 * Description of the --version command.
 */
static const char *clioptions_option_version_desc =
"Displays the server version and exits.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_version (const char *arg,
                           char      **errmsg)
{
    version(NULL);
    exit(0);

    /* Not reached */
    return true;
}

/**
 * Description of the --port command.
 */
static const char *clioptions_option_port_desc =
"Sets the port to use for server/client communication. Set to zero to disable.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_port (const char *arg,
                        char      **errmsg)
{
    int val = atoi(arg);
    if (val < 0 || val > UINT16_MAX) {
        string_fmt(*errmsg,
                   "%d is an invalid port number, must be 1-%d",
                   val,
                   UINT16_MAX);
        return false;
    }

    settings.port = val;
    return true;
}

/**
 * Description of the --port_crypto command.
 */
static const char *clioptions_option_port_crypto_desc =
"Sets the port to use for crypto server/client communication. Set to zero to "
"disable.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_port_crypto (const char *arg,
                               char      **errmsg)
{
    int val = atoi(arg);
    if (val < 0 || val > UINT16_MAX) {
        string_fmt(*errmsg,
                   "%d is an invalid port number, must be 1-%d",
                   val,
                   UINT16_MAX);
        return false;
    }

    settings.port_crypto = val;
    return true;
}

/**
 * Description of the --libpath command.
 */
static const char *clioptions_option_libpath_desc =
"Where the read-only files such as the collected treasures, artifacts,"
"archetypes etc reside.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_libpath (const char *arg,
                           char      **errmsg)
{
    snprintf(VS(settings.libpath), "%s", arg);
    return true;
}

/**
 * Description of the --datapath command.
 */
static const char *clioptions_option_datapath_desc =
"Where to read and write player data, unique maps, etc.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_datapath (const char *arg,
                            char      **errmsg)
{
    snprintf(VS(settings.datapath), "%s", arg);
    return true;
}

/**
 * Description of the --mapspath command.
 */
static const char *clioptions_option_mapspath_desc =
"Where the maps, Python scripts, etc reside.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_mapspath (const char *arg,
                            char      **errmsg)
{
    snprintf(VS(settings.mapspath), "%s", arg);
    return true;
}

/**
 * Description of the --httppath command.
 */
static const char *clioptions_option_httppath_desc =
"Where the HTTP server data files reside.\n\n"
"The server must have read/write access to this directory, as it will create "
"files inside it.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_httppath (const char *arg,
                            char      **errmsg)
{
    snprintf(VS(settings.httppath), "%s", arg);
    return true;
}

/**
 * Description of the --resourcespath command.
 */
static const char *clioptions_option_resourcespath_desc =
"Where the resource data files reside.\n\n"
"The server must have read access to this directory.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_resourcespath (const char *arg,
                                 char      **errmsg)
{
    snprintf(VS(settings.resourcespath), "%s", arg);
    return true;
}

/**
 * Description of the --metaserver_url command.
 */
static const char *clioptions_option_metaserver_url_desc =
"URL of the metaserver. The server will send POST requests to this URL to "
"update the metaserver data.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_metaserver_url (const char *arg,
                                  char      **errmsg)
{
    snprintf(VS(settings.metaserver_url), "%s", arg);
    return true;
}

/**
 * Description of the --server_host command.
 */
static const char *clioptions_option_server_host_desc =
"Hostname of the server. If set, the server will send regular updates to the "
"metaserver (using the URL specified with --metaserver_url).\n\n"
"Updates will be refused if the hostname does not resolve to the incoming IP.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_server_host (const char *arg,
                               char      **errmsg)
{
    snprintf(VS(settings.server_host), "%s", arg);
    return true;
}

/**
 * Description of the --server_name command.
 */
static const char *clioptions_option_server_name_desc =
"Name of the server. This is how the server will be named in the list of "
"servers (the metaserver)";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_server_name (const char *arg,
                               char      **errmsg)
{
    snprintf(VS(settings.server_name), "%s", arg);
    return true;
}

/**
 * Description of the --server_desc command.
 */
static const char *clioptions_option_server_desc_desc =
"Description of the server. This should describe the server to players.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_server_desc (const char *arg,
                               char      **errmsg)
{
    snprintf(VS(settings.server_desc), "%s", arg);
    return true;
}

/**
 * Description of the --server_cert command.
 */
static const char *clioptions_option_server_cert_desc =
"Server certificate, in the format specified by ADS-7. Void unless signed by "
"the Atrinik staff.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_server_cert (const char *arg,
                               char      **errmsg)
{
    if (settings.server_cert != NULL) {
        efree(settings.server_cert);
    }

    settings.server_cert = estrdup(arg);
    return true;
}

/**
 * Description of the --server_cert_sig command.
 */
static const char *clioptions_option_server_cert_sig_desc =
"Signature of the server certificate, as provided by the Atrinik staff.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_server_cert_sig (const char *arg,
                                   char      **errmsg)
{
    if (settings.server_cert_sig != NULL) {
        efree(settings.server_cert_sig);
    }

    settings.server_cert_sig = estrdup(arg);
    return true;
}

/**
 * Description of the --magic_devices_level command.
 */
static const char *clioptions_option_magic_devices_level_desc =
"Adjustment to maximum magical device level the player may use.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_magic_devices_level (const char *arg,
                                       char      **errmsg)
{
    int val = atoi(arg);
    if (val < INT8_MIN || val > INT8_MAX) {
        string_fmt(*errmsg,
                   "Invalid value: %d; must be %d-%d",
                   val, INT8_MIN, INT8_MAX);
        return false;
    }

    settings.magic_devices_level = val;
    return true;
}

/**
 * Description of the --item_power_factor command.
 */
static const char *clioptions_option_item_power_factor_desc =
"Item power factor is the relation of how the player's equipped item_power "
"total relates to their overall level. If 1.0, then sum of the character's "
"equipped item's item_power can not be greater than their overall level. "
"If 2.0, then that sum can not exceed twice the character's overall level. "
"By setting this to a high enough value, you can effectively disable "
"the item_power code.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_item_power_factor (const char *arg,
                                     char      **errmsg)
{
    settings.item_power_factor = atof(arg);
    return true;
}

/**
 * Description of the --python_reload_modules command.
 */
static const char *clioptions_option_python_reload_modules_desc =
"Whether to reload Python user modules (eg Interface.py and the like) "
"each time a Python script executes. If enabled, executing scripts will "
"be slower, but allows for easy development of modules. This should not "
"be enabled on production servers.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_python_reload_modules (const char *arg,
                                         char      **errmsg)
{
    if (KEYWORD_IS_TRUE(arg)) {
        settings.python_reload_modules = 1;
    } else if (KEYWORD_IS_FALSE(arg)) {
        settings.python_reload_modules = 0;
    } else {
        string_fmt(*errmsg, "Invalid value: %s", arg);
        return false;
    }

    return true;
}

/**
 * Description of the --default_permission_groups command.
 */
static const char *clioptions_option_default_permission_groups_desc =
"Comma-delimited list of permission groups that every player will be "
"able to access, eg, '[MOD],[DEV]'. 'None' is the same as not using "
"the option in the first place, ie, no default permission groups.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_default_permission_groups (const char *arg,
                                             char      **errmsg)
{
    if (strcmp(arg, "None") == 0) {
        settings.default_permission_groups[0] = '\0';
    } else {
        strncpy(settings.default_permission_groups, arg, sizeof(settings.default_permission_groups) - 1);
        settings.default_permission_groups[sizeof(settings.default_permission_groups) - 1] = '\0';
    }

    return true;
}

/**
 * Description of the --allowed_chars command.
 */
static const char *clioptions_option_allowed_chars_desc =
"Sets limits for allowed characters in account names/passwords, character "
"names, and their maximum length.\n\n"
"!!! DO NOT CHANGE FROM THE DEFAULTS UNLESS YOU ARE ABSOLUTELY SURE OF THE "
"CONSEQUENCES !!!\n\n"
"Removing characters from the sets will make players using the characters "
"unable to log in.\n\n"
"Adding special characters to account/character name limitations could pose "
"a security risk depending on the file-system you're using (eg, dots and "
"slashes or colons on Windows)."
"The syntax is:\n"
"<limit name>:<min>-<max> [characters] [characters2] ...\n\n"
"For example:\n"
"charname:4-20 [:alphaupper:] [:alphalower:] [:numeric:] [:space:] ['-]\n\n"
"The above sets the following rules for character names:\n"
" - Length must be at least four"
" - Length must be no more than twenty"
" - Characters may contain upper/lower-case letters, digits, spaces, "
"single-quotes and dashes.\n\n"
"The recognized values for the 'limit name' are: account, charname and "
"password\n"
"The special syntax allowed in the characters list expands to:\n"
" - :alphalower: All lower-case letters in ASCII\n"
" - :alphaupper: All upper-case letters in ASCII\n"
" - :numeric: All digits\n"
" - :print: All printable characters (does not include whitespace)\n"
" - :space: Space (you can't use a regular space in the list since spaces are "
"used to separate the entries)";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_allowed_chars (const char *arg,
                                 char      **errmsg)
{
    static const char *allowed_chars_names[] = {
        "account", "charname", "password",
    };
    CASSERT_ARRAY(allowed_chars_names, ALLOWED_CHARS_NUM);

    char word[MAX_BUF];
    size_t pos = 0;
    if (!string_get_word(arg, &pos, ' ', word, sizeof(word), 0)) {
        string_fmt(*errmsg,
                   "Invalid argument for allowed_chars option: %s",
                   arg);
        return false;
    }

    char *cps[2];
    if (string_split(word, cps, arraysize(cps), ':') != arraysize(cps)) {
        string_fmt(*errmsg,
                   "Invalid word in allowed_chars option: %s",
                   word);
        return false;
    }

    size_t type;
    for (type = 0; type < ALLOWED_CHARS_NUM; type++) {
        if (strcmp(cps[0], allowed_chars_names[type]) == 0) {
            break;
        }
    }

    if (type == ALLOWED_CHARS_NUM) {
        string_fmt(*errmsg,
                   "Invalid allowed_chars option type: %s",
                   cps[0]);
        return false;
    }

    int lower, upper;
    if (sscanf(cps[1], "%d-%d", &lower, &upper) != 2) {
        string_fmt(*errmsg,
                   "Lower/upper bounds for allowed_chars option in invalid "
                   "format: %s",
                   cps[1]);
        return false;
    }

    settings.limits[type][0] = lower;
    settings.limits[type][1] = upper;
    settings.allowed_chars[type][0] = '\0';

    while (string_get_word(arg, &pos, ' ', word, sizeof(word), 0)) {
        if (!string_startswith(word, "[") || !string_endswith(word, "]")) {
            continue;
        }

        char *cmd = string_sub(word, 1, -1);

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
                char *chars = string_create_char_range(start, end);
                snprintfcat(VS(settings.allowed_chars[type]), "%s", chars);
                efree(chars);
            }
        } else {
            snprintfcat(VS(settings.allowed_chars[type]), "%s", cmd);
        }

        efree(cmd);
    }

    return true;
}

/**
 * Description of the --control_allowed_ips command.
 */
static const char *clioptions_option_control_allowed_ips_desc =
"Comma-separated list of IPs that are allowed to send special control-related "
"commands to the server.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_control_allowed_ips (const char *arg,
                                       char      **errmsg)
{
    snprintf(VS(settings.control_allowed_ips), "%s", arg);
    return true;
}

/**
 * Description of the --control_player command.
 */
static const char *clioptions_option_control_player_desc =
"Player name that will be used to perform specific control-related operations "
"if none is supplied in the control commands sent to the server.\n\n"
"The first player is used if this option is not specified.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_control_player (const char *arg,
                                  char      **errmsg)
{
    snprintf(VS(settings.control_player), "%s", arg);
    return true;
}

/**
 * Description of the --recycle_tmp_maps command.
 */
static const char *clioptions_option_recycle_tmp_maps_desc =
"If set, will preserve temporary maps across restarts.\n\n"
"This effectively means the state of maps (eg, items dropped on the ground) is "
"preserved across server restarts.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_recycle_tmp_maps (const char *arg,
                                    char      **errmsg)
{
    settings.recycle_tmp_maps = true;
    return true;
}

/**
 * Description of the --http_url command.
 */
static const char *clioptions_option_http_url_desc =
"Specifies the URL to use for data HTTP requests. The files under the "
"directory specified by --httppath must be reachable using this URL.\n\n"
"If this URL is incorrect or inaccessible from the public network, clients "
"will be unable to connect to the server.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_http_url (const char *arg,
                            char      **errmsg)
{
    snprintf(VS(settings.http_url), "%s", arg);
    return true;
}

/**
 * Description of the --speed command.
 */
static const char *clioptions_option_speed_desc =
"Specifies the number of microseconds each tick lasts for, eg, a value of "
"125000 results in an effective server tick rate of 8 ticks per second.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_speed (const char *arg,
                         char      **errmsg)
{
    set_max_time(atol(arg));
    return true;
}

/**
 * Description of the --speed_multiplier command.
 */
static const char *clioptions_option_speed_multiplier_desc =
"This command is used to increase the server processing speed, without "
"affecting the effective game tick-rate. This means the server will be able "
"to send out priority packets with slightly less delay, possibly improving "
"network latency.\n\n"
"For example, a value of two doubles the amount of processing, three triples "
"it and so on.";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_speed_multiplier (const char *arg,
                                    char      **errmsg)
{
    set_max_time_multiplier(atoi(arg));
    return true;
}

/**
 * Description of the --network_stack command.
 */
static const char *clioptions_option_network_stack_desc =
"Selects the network stack to use. This is a comma-separated list of address "
"families to listen on, for example: ipv4, ipv6\n\n"
"It is also possible to specify IP addresses to bind to, for example: "
"ipv4=127.0.0.1, ipv6=::1\n\n"
"A value of 'dual' can be used to enable dual-stack IPv6 system with IPv4 "
"tunneling (this may not be supported on all systems).";
/** @copydoc clioptions_handler_func */
static bool
clioptions_option_network_stack (const char *arg,
                                 char      **errmsg)
{
    snprintf(VS(settings.network_stack), "%s", arg);
    return true;
}

/**
 * It is vital that init_library() is called by any functions using this
 * library.
 *
 * If you want to lessen the size of the program using the library, you
 * can replace the call to init_library() with init_globals() and
 * init_function_pointers(). Good idea to also call init_vars() and
 * init_hash_table() if you are doing any object loading.
 */
static void init_library(int argc, char *argv[])
{
    toolkit_import(memory);
    toolkit_import(signals);

    toolkit_import(clioptions);
    toolkit_import(console);
    toolkit_import(curl);
    toolkit_import(datetime);
    toolkit_import(logger);
    toolkit_import(math);
    toolkit_import(mempool);
    toolkit_import(packet);
    toolkit_import(path);
    toolkit_import(porting);
    toolkit_import(shstr);
    toolkit_import(socket);
    toolkit_import(socket_crypto);
    toolkit_import(string);
    toolkit_import(stringbuffer);

    /* Store user agent for cURL, including if this is a GNU/Linux build of
     * the server or a Windows one. */
    char user_agent[MAX_BUF];
#if defined(WIN32)
    snprintf(VS(user_agent), "Atrinik Server (Win32)/%s (%d)",
             PACKAGE_VERSION, SOCKET_VERSION);
#elif defined(__GNUC__)
    snprintf(VS(user_agent), "Atrinik Server (GNU/Linux)/%s (%d)",
             PACKAGE_VERSION, SOCKET_VERSION);
#else
    snprintf(VS(user_agent), "Atrinik Server (Unknown)/%s (%d)",
             PACKAGE_VERSION, SOCKET_VERSION);
#endif

    curl_set_user_agent(user_agent);

    /* Add console commands. */
    console_command_add("shutdown",
                        console_command_shutdown,
                        "Shuts down the server.",
                        "Shuts down the server, saving all the data and "
                        "disconnecting all players.\n\n"
                        "All of the used memory is freed, if possible.");

    console_command_add("config",
                        console_command_config,
                        "Changes server configuration.",
                        "Configuration that is marked as changeable can be "
                        "modified with this command, and any applied "
                        "configuration can be read.");

    console_command_add("active_objects",
                        console_command_active_objects,
                        "Show the number of active objects.",
                        "Show the number of active objects in the game world. "
                        "Use 'list' to show a string representation of each "
                        "object (and where it is).");

    clioption_t *cli;

    /* Non-argument options */
    CLIOPTIONS_CREATE(cli, unit, "Runs the unit tests");
    CLIOPTIONS_CREATE(cli, plugin_unit, "Runs the plugin unit tests");
    CLIOPTIONS_CREATE(cli, worldmaker, "Generates the region maps");
    CLIOPTIONS_CREATE(cli, no_console, "Disables the interactive console");
    CLIOPTIONS_CREATE(cli, version, "Displays the server version");

    /* Argument options */
    CLIOPTIONS_CREATE_ARGUMENT(cli, port, "Sets the port to use");
    CLIOPTIONS_CREATE_ARGUMENT(cli, port_crypto, "Sets the crypto port to use");
    CLIOPTIONS_CREATE_ARGUMENT(cli, libpath, "Read-only data files location");
    CLIOPTIONS_CREATE_ARGUMENT(cli, datapath, "Read/write data files location");
    CLIOPTIONS_CREATE_ARGUMENT(cli, mapspath, "Map files location");
    CLIOPTIONS_CREATE_ARGUMENT(cli, httppath, "HTTP data files location");
    CLIOPTIONS_CREATE_ARGUMENT(cli, resourcespath, "Resource files location");
    CLIOPTIONS_CREATE_ARGUMENT(cli, metaserver_url, "URL of the metaserver");
    CLIOPTIONS_CREATE_ARGUMENT(cli, http_url, "URL of the HTTP server");
    CLIOPTIONS_CREATE_ARGUMENT(cli, server_host, "Hostname of the server");
    CLIOPTIONS_CREATE_ARGUMENT(cli, server_name, "Name of the server");
    CLIOPTIONS_CREATE_ARGUMENT(cli, server_desc, "Description of the server");
    CLIOPTIONS_CREATE_ARGUMENT(cli, server_cert, "Server certificate");
    CLIOPTIONS_CREATE_ARGUMENT(cli, server_cert_sig, "Certificate signature");
    CLIOPTIONS_CREATE_ARGUMENT(cli, allowed_chars, "Limits for accounts/names");

    /* Changeable options */
    CLIOPTIONS_CREATE_ARGUMENT(cli, magic_devices_level, "Magic devices level");
    clioptions_enable_changeable(cli);
    CLIOPTIONS_CREATE_ARGUMENT(cli, item_power_factor, "Item power factor");
    clioptions_enable_changeable(cli);
    CLIOPTIONS_CREATE_ARGUMENT(cli,
                               python_reload_modules,
                               "Whether to reload Python modules");
    clioptions_enable_changeable(cli);
    CLIOPTIONS_CREATE_ARGUMENT(cli,
                               default_permission_groups,
                               "Permission groups applied to all players");
    clioptions_enable_changeable(cli);
    CLIOPTIONS_CREATE_ARGUMENT(cli,
                               control_allowed_ips,
                               "IP allowed to control the server");
    clioptions_enable_changeable(cli);
    CLIOPTIONS_CREATE_ARGUMENT(cli,
                               control_player,
                               "Default player for control commands");
    clioptions_enable_changeable(cli);
    CLIOPTIONS_CREATE(cli,
                      recycle_tmp_maps,
                      "Preserve loaded map state across restarts");
    clioptions_enable_changeable(cli);
    CLIOPTIONS_CREATE_ARGUMENT(cli, speed, "Server speed");
    clioptions_enable_changeable(cli);
    CLIOPTIONS_CREATE_ARGUMENT(cli, speed_multiplier, "Speed multiplier");
    clioptions_enable_changeable(cli);
    CLIOPTIONS_CREATE_ARGUMENT(cli, network_stack, "Configure network stack");

    cli = clioptions_create("http_server", NULL);

    /* Import game APIs that don't need settings */
    toolkit_import(commands);
    toolkit_import(pathfinder);

    memset(&settings, 0, sizeof(settings));

    clioptions_load("server.cfg", NULL);
    clioptions_load("server-custom.cfg", NULL);

    if (argv != NULL) {
        clioptions_parse(argc, argv);
    }

    /* Verify the data directory is valid. */
    DIR *dir = opendir(settings.datapath);
    if (dir == NULL) {
        if (errno == ENOENT) {
#ifdef WIN32
#   define STARTUP_SCRIPT "server.bat"
#else
#   define STARTUP_SCRIPT "./server.sh"
#endif

            LOG(ERROR,
                "The data directory %s does not exist.",
                settings.datapath);
            LOG(ERROR,
                "Please refer to the README file on the proper way of "
                "launching the Atrinik server, or use " STARTUP_SCRIPT " to "
                "launch the server.");
            exit(EXIT_FAILURE);
        } else {
            LOG(ERROR,
                "Failed to open the data directory %s: %s (%d)",
                settings.datapath,
                strerror(errno),
                errno);
            exit(EXIT_FAILURE);
        }

#undef STARTUP_SCRIPT
    }

    closedir(dir);

    curl_set_data_dir(settings.datapath);
    socket_crypto_set_path(settings.datapath);

    /* Import game APIs that need settings */
    toolkit_import(ban);
    toolkit_import(faction);
    toolkit_import(socket_server);

    map_init();
    init_globals();
    objectlink_init();
    object_init();
    player_init();
    party_init();
    init_block();
    read_bmap_names();
    material_init();
    /* Must be after we read in the bitmaps */
    init_anim();
    /* Reads all archetypes from file */
    arch_init();
    init_dynamic();
    init_clocks();
    account_init();
    resources_init();
}

/**
 * Initializes all global variables.
 * Might use environment variables as default for some of them.
 */
void init_globals(void)
{
    /* Global round ticker */
    global_round_tag = 1;

    first_player = NULL;
    last_player = NULL;
    first_map = NULL;
    first_treasurelist = NULL;
    first_artifactlist = NULL;
    first_region = NULL;
    init_strings();
    num_animations = 0;
    animations = NULL;
    animations_allocated = 0;
    object_methods_init();
}

/**
 * Initializes first_map_path from the archetype collection.
 */
static void init_dynamic(void)
{
    archetype_t *at, *tmp;
    HASH_ITER(hh, arch_table, at, tmp) {
        if (at->clone.type == MAP && EXIT_PATH(&at->clone) != NULL) {
            snprintf(VS(first_map_path), "%s", EXIT_PATH(&at->clone));
            first_map_x = at->clone.stats.hp;
            first_map_y = at->clone.stats.sp;
            return;
        }
    }

    LOG(ERROR, "You need an archetype called 'map' and it has to contain "
            "start map.");
    exit(1);
}

/**
 * Write out the current time to a file so time does not reset every
 * time the server reboots.
 */
void write_todclock(void)
{
    char filename[MAX_BUF];
    FILE *fp;

    snprintf(filename, sizeof(filename), "%s/clockdata", settings.datapath);

    if ((fp = fopen(filename, "w")) == NULL) {
        LOG(BUG, "Cannot open %s for writing.", filename);
        return;
    }

    fprintf(fp, "%lu", todtick);
    fclose(fp);
}

/**
 * Initializes the gametime and TOD counters.
 *
 * Called by init_library().
 */
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
        LOG(DEBUG, "Can't open %s.", filename);
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
 * @param argc
 * Length of argv.
 * @param argv
 * Arguments.
 */
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
    read_client_images();
    updates_init();
    init_srv_files();
    metaserver_init();
    statistics_init();
    reset_sleep();
    init_plugins();
}

/**
 * Initialize before playing.
 */
static void init_beforeplay(void)
{
    init_spells();
    race_init();
    init_readable();
    init_new_exp_system();
}
