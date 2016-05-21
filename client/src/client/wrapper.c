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
 * General convenience functions for the client.
 */

#include <global.h>
#include <toolkit_string.h>
#include <path.h>

/**
 * Start the base system, setting caption name and window icon.
 */
void system_start(void)
{
    SDL_Surface *icon;

    icon = IMG_Load_wrapper("textures/"CLIENT_ICON_NAME);

    if (icon) {
        SDL_WM_SetIcon(icon, NULL);
        SDL_FreeSurface(icon);
    }

    SDL_WM_SetCaption(PACKAGE_NAME, PACKAGE_NAME);
}

/**
 * End the system.
 */
void system_end(void)
{
    tooltip_dismiss();
    object_deinit();
    notification_destroy();
    popup_destroy_all();
    toolkit_widget_deinit();
    client_socket_deinitialize();
    metaserver_clear_data();
    effects_deinit();
    sound_ambient_clear();
    interface_deinit();
    sound_deinit();
    intro_deinit();
    cmd_aliases_deinit();
    server_settings_deinit();
    texture_deinit();
    text_deinit();
    hfiles_deinit();
    settings_deinit();
    keybind_deinit();
    image_bmaps_deinit();
    anims_deinit();
    skills_deinit();
    spells_deinit();
    clioption_settings_deinit();
    server_files_deinit();
    image_deinit();
    toolkit_deinit();
    SDL_Quit();
}

/**
 * Recursively creates directories from path.
 *
 * Used by file_path().
 * @param path
 * The path
 * @return
 * 0 on success, -1 otherwise
 */
static int mkdir_recurse(const char *path)
{
    char *copy, *p;

    p = copy = estrdup(path);

    do {
        p = strchr(p + 1, '/');

        if (p) {
            *p = '\0';
        }

        if (mkdir(copy, 0755) == -1 && errno != EEXIST) {
            efree(copy);
            return -1;
        }

        if (p) {
            *p = '/';
        }
    } while (p);

    efree(copy);

    return 0;
}

/**
 * Use mkdir_recurse() to ensure that destination 'path' exists, creating
 * sub-directories of the path, if they do not exist yet. If you're using
 * this to ensure directory path exists, make sure to end the 'path'
 * string with a forward slash, otherwise the function will assume that
 * the path is a file path.
 * @param path
 * The path to ensure.
 */
void mkdir_ensure(const char *path)
{
    char *stmp;

    stmp = strrchr(path, '/');

    if (stmp) {
        char ctmp;

        ctmp = stmp[0];
        stmp[0] = '\0';
        mkdir_recurse(path);
        stmp[0] = ctmp;
    }
}

/**
 * Copy a file.
 * @param filename
 * Source file.
 * @param filename_out
 * Destination file.
 */
void copy_file(const char *filename, const char *filename_out)
{
    FILE *fp, *fp_out;
    char buf[HUGE_BUF];

    fp = fopen(filename, "r");

    if (!fp) {
        LOG(BUG, "Failed to open '%s' for reading.", filename);
        return;
    }

    mkdir_ensure(filename_out);

    fp_out = fopen(filename_out, "w");

    if (!fp_out) {
        LOG(BUG, "Failed to open '%s' for writing.", filename_out);
        fclose(fp);
        return;
    }

    while (fgets(buf, sizeof(buf), fp)) {
        fputs(buf, fp_out);
    }

    fclose(fp);
    fclose(fp_out);
}

/**
 * Copy a file/directory if it exists.
 * @param from
 * Directory where to copy from.
 * @param to
 * Directort to copy to.
 * @param src
 * File/directory to copy.
 * @param dst
 * Where to copy the file/directory to.
 */
void copy_if_exists(const char *from, const char *to, const char *src, const char *dst)
{
    char src_path[HUGE_BUF], dst_path[HUGE_BUF];

    snprintf(src_path, sizeof(src_path), "%s/%s", from, src);
    snprintf(dst_path, sizeof(dst_path), "%s/%s", to, dst);

    if (access(src_path, R_OK) == 0) {
        copy_rec(src_path, dst_path);
    }
}

/**
 * Removes all the files in the specified directory.
 *
 * @param dir
 * The directory.
 * @param path
 * Path to the directory.
 */
static void
_rmrf (DIR *dir, const char *path)
{
    HARD_ASSERT(dir != NULL);

    struct dirent *file;
    while ((file = readdir(dir)) != NULL) {
        if (strcmp(file->d_name, ".") == 0 ||
            strcmp(file->d_name, "..") == 0) {
            continue;
        }

        char buf[HUGE_BUF];
        snprintf(VS(buf), "%s/%s", path, file->d_name);

        DIR *dir2 = opendir(buf);
        if (dir2 != NULL) {
            _rmrf(dir2, buf);
            closedir(dir2);
            rmdir(buf);
        } else {
            unlink(buf);
        }
    }
}

/**
 * Recursively remove a directory and its contents.
 *
 * Effectively same as 'rf -rf path'.
 *
 * @param path
 * What to remove.
 */
void rmrf(const char *path)
{
    HARD_ASSERT(path != NULL);

    DIR *dir = opendir(path);
    if (dir == NULL) {
        return;
    }

    _rmrf(dir, path);
    closedir(dir);
    rmdir(path);
}

/**
 * Recursively copy a file or directory.
 * @param src
 * Source file/directory to copy.
 * @param dst
 * Where to copy to.
 */
void copy_rec(const char *src, const char *dst)
{
    /* Copy directory contents. */
    DIR *dir = opendir(src);
    if (dir != NULL) {
        struct dirent *currentfile;
        char dir_src[HUGE_BUF], dir_dst[HUGE_BUF];

        /* Try to make the new directory. */
        if (access(dst, R_OK) != 0) {
            mkdir(dst, 0755);
        }

        while ((currentfile = readdir(dir))) {
            if (currentfile->d_name[0] == '.') {
                continue;
            }

            snprintf(dir_src, sizeof(dir_src), "%s/%s", src, currentfile->d_name);
            snprintf(dir_dst, sizeof(dir_dst), "%s/%s", dst, currentfile->d_name);
            copy_rec(dir_src, dir_dst);
        }

        closedir(dir);
    } else {
        copy_file(src, dst);
    }
}

/**
 * Get configuration directory.
 * @return
 * The configuration directory.
 */
const char *get_config_dir(void)
{
    const char *desc;

#ifndef WIN32
    desc = getenv("HOME");
#else
    desc = getenv("APPDATA");
#endif

    /* Failed to find an usable destination, so store it in the
     * current directory. */
    if (!desc || !*desc) {
        desc = ".";
    }

    return desc;
}

/**
 * Get path to a file in the data directory.
 * @param buf
 * Buffer where to store the path.
 * @param len
 * Size of buf.
 * @param fname
 * File.
 */
void get_data_dir_file(char *buf, size_t len, const char *fname)
{
    /* Try the current directory first. */
    snprintf(buf, len, "./%s", fname);

#ifdef INSTALL_SUBDIR_SHARE

    /* Not found, try the share directory since it was defined... */
    if (access(buf, R_OK)) {
        char *prefix;

        /* Get the prefix. */
        prefix = binreloc_find_prefix("./");
        /* Construct the path. */
        snprintf(buf, len, "%s/"INSTALL_SUBDIR_SHARE "/%s", prefix, fname);
        efree(prefix);
    }
#endif
}

/**
 * Get absolute path to the specified relative path. This path will typically
 * point to the to client data directory (which is usually located in the user's
 * home/appdata directory), but depending on the specified mode, extra actions
 * may be performed. These ensure that if you're trying to access a file that
 * does not yet exist in the client data directory, it will be read from the
 * client installation directory instead (unless it's being appended to, in
 * which case it will be copied to the client data directory first).
 *
 * Generally, you should almost always use this when you need to construct a
 * path, or use one of the many @ref file_wrapper_functions.
 * @param fname
 * The file path.
 * @param mode
 * File mode.
 * @return
 * The absolute path. Must be freed.
 */
char *file_path(const char *path, const char *mode)
{
    bool is_write, is_append;
    StringBuffer *sb;
    char version[MAX_BUF], client_path[HUGE_BUF], *new_path;

    HARD_ASSERT(path != NULL);
    HARD_ASSERT(mode != NULL);

    SOFT_ASSERT_RC(path[0] != '/', estrdup(path),
            "Path is already absolute: %s", path);

    sb = stringbuffer_new();
    stringbuffer_append_printf(sb, "%s/.atrinik/%s/%s", get_config_dir(),
            package_get_version_partial(VS(version)), path);
    new_path = stringbuffer_sub(sb, 0, 0);

    is_write = is_append = false;

    if (strchr(mode, 'w') != NULL) {
        is_write = true;
    } else if (strchr(mode, '+') != NULL || strchr(mode, 'a') != NULL) {
        is_append = true;
    }

    if (is_write || is_append) {
        if (access(new_path, W_OK) != 0) {
            char *dirname;

            /* Ensure directories exist if we're going to use this path for
             * writing/appending. */
            dirname = path_dirname(new_path);
            mkdir_recurse(dirname);
            efree(dirname);

            if (is_append) {
                get_data_dir_file(VS(client_path), path);
                copy_file(client_path, new_path);
            }
        }
    } else {
        if (access(new_path, R_OK) != 0) {
            get_data_dir_file(VS(client_path), path);
            stringbuffer_seek(sb, 0);
            stringbuffer_append_string(sb, client_path);
        }
    }

    efree(new_path);

    return stringbuffer_finish(sb);
}

/**
 * Constructs a path leading to the chosen server settings directory. Used
 * internally by file_path_player() and file_path_server().
 * @return
 *
 */
static StringBuffer *file_path_server_internal(void)
{
    StringBuffer *sb;

    sb = stringbuffer_new();
    stringbuffer_append_string(sb, "settings/");

    SOFT_ASSERT_RC(selected_server != NULL, sb, "Selected server is NULL.");
    SOFT_ASSERT_RC(!string_isempty(selected_server->hostname), sb,
            "Selected server has empty hostname.");

    stringbuffer_append_printf(sb, "servers/%s-%d/", selected_server->hostname,
            selected_server->port);

    return sb;
}

/**
 * Create a path to the per-player settings directory.
 * @param path
 * Path inside the per-player settings directory.
 * @return
 * New path. Must be freed.
 */
char *file_path_player(const char *path)
{
    StringBuffer *sb;

    HARD_ASSERT(path != NULL);

    sb = file_path_server_internal();

    SOFT_ASSERT_LABEL(*cpl.account != '\0', done, "Account name is empty.");
    SOFT_ASSERT_LABEL(*cpl.name != '\0', done, "Player name is empty.");

    stringbuffer_append_printf(sb, "%s/%s/%s", cpl.account, cpl.name, path);

done:
    return stringbuffer_finish(sb);
}

/**
 * Create a path to the per-server settings directory.
 * @param path
 * Path inside the per-server settings directory.
 * @return
 * New path. Must be freed.
 */
char *file_path_server(const char *path)
{
    StringBuffer *sb;

    HARD_ASSERT(path != NULL);

    sb = file_path_server_internal();
    stringbuffer_append_printf(sb, ".common/%s", path);

    return stringbuffer_finish(sb);
}

/**
 * @defgroup file_wrapper_functions File wrapper functions
 * These functions are used as replacement to common C and SDL functions
 * that are related to file opening and reading/writing.
 *
 * For GNU/Linux, they call file_path() to determine the path to the file
 * to open in ~/.atrinik, and if the file doesn't exist there, copy it
 * there from the directory the client is running in.
 *@{*/

/**
 * fopen wrapper.
 * @param fname
 * The file name.
 * @param mode
 * File mode.
 * @return
 * Return value of fopen().
 */
FILE *
client_fopen_wrapper (const char *fname, const char *mode)
{
    char *path = file_path(fname, mode);
    FILE *fp = fopen(path, mode);
    efree(path);

    return fp;
}

/**
 * IMG_Load wrapper.
 * @param file
 * The file name
 * @return
 * Return value of IMG_Load().
 */
SDL_Surface *IMG_Load_wrapper(const char *file)
{
    char *path;
    SDL_Surface *surface;

    path = file_path(file, "r");
    surface = IMG_Load(path);
    efree(path);

    return surface;
}

/**
 * TTF_OpenFont wrapper.
 * @param file
 * The file name.
 * @param ptsize
 * Size of font.
 * @return
 * Return value of TTF_OpenFont().
 */
TTF_Font *TTF_OpenFont_wrapper(const char *file, int ptsize)
{
    char *path;
    TTF_Font *font;

    path = file_path(file, "r");
    font = TTF_OpenFont(path, ptsize);
    efree(path);

    return font;
}
/*@}*/
