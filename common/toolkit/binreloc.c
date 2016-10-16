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
 * @author Hongli Lai \<h.lai@chello.nl\>
 */

#include "string.h"
#include "path.h"
#include "binreloc.h"

/**
 * Canonical filename of the executable. May be NULL.
 */
static char *exe = NULL;

static char *_binreloc_find_exe(void);

TOOLKIT_API(DEPENDS(path));

TOOLKIT_INIT_FUNC(binreloc)
{
    exe = _binreloc_find_exe();
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(binreloc)
{
    if (exe != NULL) {
        efree(exe);
        exe = NULL;
    }
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Finds the canonical filename of the executable.
 *
 * @return
 * The filename (which must be freed) or NULL on error.
 */
static char
*_binreloc_find_exe (void)
{
#ifndef ENABLE_BINRELOC
    TOOLKIT_PROTECT();
    return NULL;
#else
    char *path, *path2, *line, *result;
    size_t buf_size;
    ssize_t size;
    struct stat stat_buf;
    FILE *f;

    TOOLKIT_PROTECT();

    /* Read from /proc/self/exe (symlink) */
    if (sizeof(path) > SSIZE_MAX) {
        buf_size = SSIZE_MAX - 1;
    } else {
        buf_size = PATH_MAX - 1;
    }

    path = emalloc(buf_size);
    path2 = emalloc(buf_size);

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
            efree(path2);
            break;
        }

        /* stat() success. */
        if (!S_ISLNK(stat_buf.st_mode)) {
            /* path is not a symlink. Done. */
            efree(path2);
            return path;
        }

        /* path is a symlink. Continue loop and resolve this. */
        strncpy(path, path2, buf_size - 1);
    }

    /* readlink() or stat() failed; this can happen when the program is
     * running in Valgrind 2.2. Read from /proc/self/maps as fallback. */
    buf_size = PATH_MAX + 128;
    line = erealloc(path, buf_size);

    f = fopen("/proc/self/maps", "r");

    if (!f) {
        efree(line);
        return NULL;
    }

    /* The first entry should be the executable name. */
    result = fgets(line, buf_size, f);

    if (!result) {
        fclose(f);
        efree(line);
        return NULL;
    }

    /* Get rid of newline character. */
    buf_size = strlen(line);

    if (buf_size <= 0) {
        /* Huh? An empty string? */
        fclose(f);
        efree(line);
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
        efree(line);
        return NULL;
    }

    path = estrdup(path);
    efree(line);
    fclose(f);
    return path;
#endif
}

char *
binreloc_find_exe (const char *default_exe)
{
    TOOLKIT_PROTECT();

    if (exe) {
        return estrdup(exe);
    }

    if (default_exe) {
        return estrdup(default_exe);
    }

    return NULL;
}

char *
binreloc_find_exe_dir (const char *default_dir)
{
    TOOLKIT_PROTECT();

    if (exe) {
        return path_dirname(exe);
    }

    if (default_dir) {
        return estrdup(default_dir);
    }

    return NULL;
}

char *
binreloc_find_prefix (const char *default_prefix)
{
    TOOLKIT_PROTECT();

    if (exe) {
        char *dir1, *dir2;

        dir1 = path_dirname(exe);
        dir2 = path_dirname(dir1);
        efree(dir1);
        return dir2;
    }

    if (default_prefix) {
        return estrdup(default_prefix);
    }

    return NULL;
}

char *
binreloc_find_bin_dir (const char *default_bin_dir)
{
    char *prefix, *dir;

    TOOLKIT_PROTECT();

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_bin_dir) {
            return estrdup(default_bin_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "bin");
    efree(prefix);
    return dir;
}

char *
binreloc_find_sbin_dir (const char *default_sbin_dir)
{
    char *prefix, *dir;

    TOOLKIT_PROTECT();

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_sbin_dir) {
            return estrdup(default_sbin_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "sbin");
    efree(prefix);
    return dir;
}

char *
binreloc_find_data_dir (const char *default_data_dir)
{
    char *prefix, *dir;

    TOOLKIT_PROTECT();

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_data_dir) {
            return estrdup(default_data_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "share");
    efree(prefix);
    return dir;
}

char *
binreloc_find_locale_dir (const char *default_locale_dir)
{
    char *data_dir, *dir;

    TOOLKIT_PROTECT();

    data_dir = binreloc_find_data_dir(NULL);

    if (!data_dir) {
        if (default_locale_dir) {
            return estrdup(default_locale_dir);
        }

        return NULL;
    }

    dir = path_join(data_dir, "locale");
    efree(data_dir);
    return dir;
}

char *
binreloc_find_lib_dir (const char *default_lib_dir)
{
    char *prefix, *dir;

    TOOLKIT_PROTECT();

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_lib_dir) {
            return estrdup(default_lib_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "lib");
    efree(prefix);
    return dir;
}

char *
binreloc_find_libexec_dir (const char *default_libexec_dir)
{
    char *prefix, *dir;

    TOOLKIT_PROTECT();

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_libexec_dir) {
            return estrdup(default_libexec_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "libexec");
    efree(prefix);
    return dir;
}

char *
binreloc_find_etc_dir (const char *default_etc_dir)
{
    char *prefix, *dir;

    TOOLKIT_PROTECT();

    prefix = binreloc_find_prefix(NULL);

    if (!prefix) {
        if (default_etc_dir) {
            return estrdup(default_etc_dir);
        }

        return NULL;
    }

    dir = path_join(prefix, "etc");
    efree(prefix);
    return dir;
}
