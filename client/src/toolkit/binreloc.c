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
 * BinReloc - a library for creating relocatable executables
 *
 * This source code is public domain. You can relicense this code
 * under whatever license you want.
 *
 * See http://autopackage.org/docs/binreloc/ for
 * more information and how to use this.
 * @author Hongli Lai \<h.lai@chello.nl\> */

#include <global.h>

/**
 * Name of the API. */
#define API_NAME binreloc

/**
 * If 1, the API has been initialized. */
static uint8 did_init = 0;

/**
 * Canonical filename of the executable. May be NULL. */
static char *exe = NULL;

/**
 * Finds the canonical filename of the executable.
 * @return The filename (which must be freed) or NULL on error. */
static char *_binreloc_find_exe()
{
#ifndef ENABLE_BINRELOC
    TOOLKIT_FUNC_PROTECTOR(API_NAME);
    return NULL;
#else
    char *path, *path2, *line, *result;
    size_t buf_size;
    ssize_t size;
    struct stat stat_buf;
    FILE *f;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    /* Read from /proc/self/exe (symlink) */
    if (sizeof(path) > SSIZE_MAX) {
        buf_size = SSIZE_MAX - 1;
    }
    else {
        buf_size = PATH_MAX - 1;
    }

    path = malloc(buf_size);

    if (!path) {
        return NULL;
    }

    path2 = malloc(buf_size);

    if (!path2) {
        free(path);
        return NULL;
    }

    strncpy(path2, "/proc/self/exe", buf_size - 1);

    while (1) {
        int i;

        size = readlink(path2, path, buf_size - 1);

        if (size == -1) {
            /* Error. */
            free(path2);
            break;
        }

        /* readlink() success. */
        path[size] = '\0';

        /* Check whether the symlink's target is also a symlink.
         * We want to get the final target. */
        i = stat(path, &stat_buf);

        if (i == -1) {
            /* Error. */
            free(path2);
            break;
        }

        /* stat() success. */
        if (!S_ISLNK(stat_buf.st_mode)) {
            /* path is not a symlink. Done. */
            free(path2);
            return path;
        }

        /* path is a symlink. Continue loop and resolve this. */
        strncpy(path, path2, buf_size - 1);
    }

    /* readlink() or stat() failed; this can happen when the program is
     * running in Valgrind 2.2. Read from /proc/self/maps as fallback. */
    buf_size = PATH_MAX + 128;
    line = realloc(path, buf_size);

    if (!line) {
        free(path);
        return NULL;
    }

    f = fopen("/proc/self/maps", "r");

    if (!f) {
        free(line);
        return NULL;
    }

    /* The first entry should be the executable name. */
    result = fgets(line, buf_size, f);

    if (!result) {
        fclose(f);
        free(line);
        return NULL;
    }

    /* Get rid of newline character. */
    buf_size = strlen(line);

    if (buf_size <= 0) {
        /* Huh? An empty string? */
        fclose(f);
        free(line);
        return NULL;
    }

    if (line[buf_size - 1] == '\n') {
        line[buf_size - 1] = '\0';
    }

    /* Extract the filename; it is always an absolute path. */
    path = strchr(line, '/');

    /* Sanity check. */
    if (strstr(line, " r-xp ") == NULL || !path) {
        fclose(f);
        free(line);
        return NULL;
    }

    path = strdup(path);
    free(line);
    fclose(f);
    return path;
#endif
}

/**
 * Initialize the binreloc API.
 * @internal */
void toolkit_binreloc_init(void)
{
    TOOLKIT_INIT_FUNC_START(binreloc)
    {
        toolkit_import(path);
        exe = _binreloc_find_exe();
    }
    TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the binreloc API.
 * @internal */
void toolkit_binreloc_deinit(void)
{
    TOOLKIT_DEINIT_FUNC_START(binreloc)
    {
        if (exe) {
            free(exe);
            exe = NULL;
        }
    }
    TOOLKIT_DEINIT_FUNC_END()
}

/**
 * Finds the canonical filename of the current application.
 * @param default_exe A default filename which will be used as fallback.
 * @returns A string containing the application's canonical filename,
 * which must be freed when no longer necessary. If BinReloc is not
 * initialized, or if initialization failed, then a copy of default_exe
 * will be returned. If default_exe is NULL, then NULL will be returned. */
char *binreloc_find_exe(const char *default_exe)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (exe) {
        return strdup(exe);
    }

    if (default_exe) {
        return strdup(default_exe);
    }

    return NULL;
}

/**
 * Locate the directory in which the current application is installed.
 *
 * The prefix is generated by the following pseudo-code evaluation:
 * @code
 * dirname(exename)
 * @endcode
 * @param default_dir A default directory which will used as fallback.
 * @return A string containing the directory, which must be freed when no
 * longer necessary. If BinReloc is not initialized, or if the
 * initialization failed, then a copy of default_dir will be returned. If
 * default_dir is NULL, then NULL will be returned. */
char *binreloc_find_exe_dir(const char *default_dir)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (exe) {
        return path_dirname(exe);
    }

    if (default_dir) {
        return strdup(default_dir);
    }

    return NULL;
}

/**
 * Locate the prefix in which the current application is installed.
 *
 * The prefix is generated by the following pseudo-code evaluation:
 * @code
 * dirname(dirname(exename))
 * @endcode
 * @param default_prefix A default prefix which will used as fallback.
 * @return A string containing the prefix, which must be freed when no
 * longer necessary. If BinReloc is not initialized, or if the
 * initialization failed, then a copy of default_prefix will be returned.
 * If default_prefix is NULL, then NULL will be returned. */
char *binreloc_find_prefix(const char *default_prefix)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (exe) {
        char *dir1, *dir2;

        dir1 = path_dirname(exe);
        dir2 = path_dirname(dir1);
        free(dir1);
        return dir2;
    }

    if (default_prefix) {
        return strdup(default_prefix);
    }

    return NULL;
}

/**
 * Locate the application's binary folder.
 *
 * The path is generated by the following pseudo-code evaluation:
 * @code
 * prefix + "/bin"
 * @endcode
 * @param default_bin_dir A default path which will used as fallback.
 * @return A string containing the bin folder's path, which must be freed
 * when no longer necessary. If BinReloc is not initialized, or if the
 * initialization failed, then a copy of default_bin_dir will be
 * returned. If default_bin_dir is NULL, then NULL will be returned. */
char *binreloc_find_bin_dir(const char *default_bin_dir)
{
    char *prefix, *dir;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_bin_dir) {
            return strdup(default_bin_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "bin");
    free(prefix);
    return dir;
}

/**
 * Locate the application's superuser binary folder.
 *
 * The path is generated by the following pseudo-code evaluation:
 * @code
 * prefix + "/sbin"
 * @endcode
 * @param default_sbin_dir A default path which will used as fallback.
 * @return A string containing the sbin folder's path, which must be
 * freed when no longer necessary. If BinReloc is not initialized, or if
 * the initialization failed, then a copy of default_sbin_dir will be
 * returned. If default_bin_dir is NULL, then NULL will be returned. */
char *binreloc_find_sbin_dir (const char *default_sbin_dir)
{
    char *prefix, *dir;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_sbin_dir) {
            return strdup(default_sbin_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "sbin");
    free(prefix);
    return dir;
}

/**
 * Locate the application's data folder.
 *
 * The path is generated by the following pseudo-code evaluation:
 * @code
 * prefix + "/share"
 * @endcode
 * @param default_data_dir A default path which will used as fallback.
 * @return A string containing the data folder's path, which must be
 * freed when no longer necessary. If BinReloc is not initialized, or if
 * the initialization failed, then a copy of default_data_dir will be
 * returned. If default_data_dir is NULL, then NULL will be returned. */
char *binreloc_find_data_dir(const char *default_data_dir)
{
    char *prefix, *dir;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_data_dir) {
            return strdup(default_data_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "share");
    free(prefix);
    return dir;
}

/**
 * Locate the application's localization folder.
 *
 * The path is generated by the following pseudo-code evaluation:
 * @code
 * prefix + "/share/locale"
 * @endcode
 * @param default_locale_dir A default path which will used as fallback.
 * @return A string containing the localization folder's path, which must
 * be freed when no longer necessary. If BinReloc is not initialized, or
 * if the initialization failed, then a copy of default_locale_dir will
 * be returned. If default_locale_dir is NULL, then NULL will be
 * returned. */
char *binreloc_find_locale_dir(const char *default_locale_dir)
{
    char *data_dir, *dir;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    data_dir = binreloc_find_data_dir(NULL);

    if (!data_dir) {
        if (default_locale_dir) {
            return strdup(default_locale_dir);
        }

        return NULL;
    }

    dir = path_join(data_dir, "locale");
    free(data_dir);
    return dir;
}

/**
 * Locate the application's library folder.
 *
 * The path is generated by the following pseudo-code evaluation:
 * @code
 * prefix + "/lib"
 * @endcode
 * @param default_lib_dir A default path which will used as fallback.
 * @return A string containing the library folder's path, which must be
 * freed when no longer necessary. If BinReloc is not initialized, or if
 * the initialization failed, then a copy of default_lib_dir will be
 * returned. If default_lib_dir is NULL, then NULL will be returned. */
char *binreloc_find_lib_dir(const char *default_lib_dir)
{
    char *prefix, *dir;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_lib_dir) {
            return strdup(default_lib_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "lib");
    free(prefix);
    return dir;
}

/**
 * Locate the application's libexec folder.
 *
 * The path is generated by the following pseudo-code evaluation:
 * @code
 * prefix + "/libexec"
 * @endcode
 * @param default_libexec_dir A default path which will used as fallback.
 * @return A string containing the libexec folder's path, which must be
 * freed when no longer necessary. If BinReloc is not initialized, or if
 * the initialization failed, then a copy of default_libexec_dir will be
 * returned. If default_libexec_dir is NULL, then NULL will be
 * returned. */
char *binreloc_find_libexec_dir(const char *default_libexec_dir)
{
    char *prefix, *dir;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_libexec_dir) {
            return strdup(default_libexec_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "libexec");
    free(prefix);
    return dir;
}

/**
 * Locate the application's configuration files folder.
 *
 * The path is generated by the following pseudo-code evaluation:
 * @code
 * prefix + "/etc"
 * @endcode
 * @param default_etc_dir A default path which will used as fallback.
 * @return A string containing the etc folder's path, which must be freed
 * when no longer necessary. If BinReloc is not initialized, or if the
 * initialization failed, then a copy of default_etc_dir will be
 * returned. If default_etc_dir is NULL, then NULL will be returned. */
char *binreloc_find_etc_dir(const char *default_etc_dir)
{
    char *prefix, *dir;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_etc_dir) {
            return strdup(default_etc_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "etc");
    free(prefix);
    return dir;
}
