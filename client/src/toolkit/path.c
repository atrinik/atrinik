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
 * OS path API. */

#include <global.h>

/**
 * Name of the API. */
#define API_NAME path

/**
 * If 1, the API has been initialized. */
static uint8 did_init = 0;

/**
 * Initialize the path API.
 * @internal */
void toolkit_path_init(void)
{
    TOOLKIT_INIT_FUNC_START(path)
    {
        toolkit_import(logger);
        toolkit_import(string);
        toolkit_import(stringbuffer);
    }
    TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the path API.
 * @internal */
void toolkit_path_deinit(void)
{
    TOOLKIT_DEINIT_FUNC_START(path)
    {
    }
    TOOLKIT_DEINIT_FUNC_END()
}

/**
 * Joins two path components, eg, '/usr' and 'bin' -> '/usr/bin'.
 * @param path First path component.
 * @param path2 Second path component.
 * @return The joined path; should be freed when no longer needed. */
char *path_join(const char *path, const char *path2)
{
    StringBuffer *sb;
    size_t len;
    char *cp;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    sb = stringbuffer_new();
    stringbuffer_append_string(sb, path);

    len = strlen(path);

    if (len && path[len - 1] != '/') {
        stringbuffer_append_string(sb, "/");
    }

    stringbuffer_append_string(sb, path2);
    cp = stringbuffer_finish(sb);

    return cp;
}

/**
 * Extracts the directory component of a path.
 *
 * Example:
 * @code
 * path_dirname("/usr/local/foobar"); --> "/usr/local"
 * @endcode
 * @param path A path.
 * @return A directory name. This string should be freed when no longer
 * needed.
 * @author Hongli Lai (public domain) */
char *path_dirname(const char *path)
{
    const char *end;
    char *result;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (!path) {
        return NULL;
    }

    end = strrchr(path, '/');

    if (!end) {
        return estrdup(".");
    }

    while (end > path && *end == '/') {
        end--;
    }

    result = estrndup(path, end - path + 1);

    if (result[0] == '\0') {
        efree(result);
        return estrdup("/");
    }

    return result;
}

/**
 * Extracts the basename from path.
 *
 * Example:
 * @code
 * path_basename("/usr/bin/kate"); --> "kate"
 * @endcode
 * @param path A path.
 * @return The basename of the path. Should be freed when no longer
 * needed. */
char *path_basename(const char *path)
{
    const char *slash;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (!path) {
        return NULL;
    }

    while ((slash = strrchr(path, '/'))) {
        if (*(slash + 1) != '\0') {
            return estrdup(slash + 1);
        }
    }

    return estrdup(path);
}

/**
 * Normalize a path, eg, foo//bar, foo/foo2/../bar, foo/./bar all become
 * foo/bar.
 *
 * If the path begins with either a forward slash or a dot *and* a forward
 * slash, they will be preserved.
 * @param path Path to normalize.
 * @return The normalized path; never NULL. Must be freed. */
char *path_normalize(const char *path)
{
    StringBuffer *sb;
    size_t pos, startsbpos;
    char component[MAX_BUF];
    ssize_t last_slash;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (string_isempty(path)) {
        return estrdup(".");
    }

    sb = stringbuffer_new();
    pos = 0;

    if (string_startswith(path, "/")) {
        stringbuffer_append_string(sb, "/");
    }
    else if (string_startswith(path, "./")) {
        stringbuffer_append_string(sb, "./");
    }

    startsbpos = sb->pos;

    while (string_get_word(path, &pos, '/', component, sizeof(component), 0)) {
        if (strcmp(component, ".") == 0) {
            continue;
        }

        if (strcmp(component, "..") == 0) {
            if (sb->pos > startsbpos) {
                last_slash = stringbuffer_rindex(sb, '/');

                if (last_slash == -1) {
                    logger_print(LOG(BUG), "Should have found a forward slash, but didn't: %s", path);
                    continue;
                }

                sb->pos = last_slash;
            }
        }
        else {
            if (sb->pos == 0 || sb->buf[sb->pos - 1] != '/') {
                stringbuffer_append_string(sb, "/");
            }

            stringbuffer_append_string(sb, component);
        }
    }

    if (sb->pos == 0) {
        stringbuffer_append_string(sb, ".");
    }

    return stringbuffer_finish(sb);
}

/**
 * Checks whether any directories in the given path don't exist, and
 * creates them if necessary.
 * @param path The path to check. */
void path_ensure_directories(const char *path)
{
    char buf[MAXPATHLEN], *cp;
    struct stat statbuf;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (path == NULL || *path == '\0') {
        return;
    }

    snprintf(VS(buf), "%s", path);
    cp = buf;

    while ((cp = strchr(cp + 1, '/')) != NULL) {
        *cp = '\0';

        if (mkdir(buf, 0777) != 0 && errno != EEXIST) {
            log(LOG(BUG), "Cannot mkdir %s (path: %s): %s", buf, path,
                strerror(errno));
            return;
        }

        if (stat(buf, &statbuf) != 0) {
            log(LOG(BUG), "Cannot stat %s (path: %s): %s", buf, path,
                strerror(errno));
            return;
        }

        if (!S_ISDIR(statbuf.st_mode)) {
            log(LOG(BUG), "Not a directory: %s (path: %s)", buf, path);
            return;
        }

        *cp = '/';
    }
}

/**
 * Copy the contents of file 'src' into 'dst'.
 * @param src Path of the file to copy contents from.
 * @param dst Where to put the contents of 'src'.
 * @param mode Mode to open 'src' in.
 * @return 1 on success, 0 on failure. */
int path_copy_file(const char *src, FILE *dst, const char *mode)
{
    FILE *fp;
    char buf[HUGE_BUF];

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (!src || !dst || !mode) {
        return 0;
    }

    fp = fopen(src, mode);

    if (!fp) {
        return 0;
    }

    while (fgets(buf, sizeof(buf), fp)) {
        fputs(buf, dst);
    }

    fclose(fp);

    return 1;
}

/**
 * Check if the specified path exists.
 * @param path Path to check.
 * @return 1 if 'path' exists, 0 otherwise. */
int path_exists(const char *path)
{
    struct stat statbuf;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (stat(path, &statbuf) != 0) {
        return 0;
    }

    return 1;
}

/**
 * Create a new blank file.
 * @param path Path to the file.
 * @return 1 on success, 0 on failure. */
int path_touch(const char *path)
{
    FILE *fp;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    path_ensure_directories(path);
    fp = fopen(path, "w");

    if (!fp) {
        return 0;
    }

    if (fclose(fp) == EOF) {
        return 0;
    }

    return 1;
}

/**
 * Get size of the specified file, in bytes.
 * @param path Path to the file.
 * @return Size of the file. */
size_t path_size(const char *path)
{
    struct stat statbuf;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (stat(path, &statbuf) != 0) {
        return 0;
    }

    return statbuf.st_size;
}

/**
 * Load the entire contents of file 'path' into a StringBuffer instance,
 * then return the created string.
 * @param path File to load contents of.
 * @return The loaded contents. Must be freed. */
char *path_file_contents(const char *path)
{
    FILE *fp;
    StringBuffer *sb;
    char buf[MAX_BUF];

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    fp = fopen(path, "rb");

    if (!fp) {
        return NULL;
    }

    sb = stringbuffer_new();

    while (fgets(buf, sizeof(buf), fp)) {
        stringbuffer_append_string(sb, buf);
    }

    fclose(fp);

    return stringbuffer_finish(sb);
}
