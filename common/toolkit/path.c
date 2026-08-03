/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * OS path API.
 */

#include "path.h"
#include "string.h"
#include <openssl/crypto.h>

TOOLKIT_API(DEPENDS(logger), DEPENDS(string), DEPENDS(stringbuffer));

/**
 * Function to use for path_fopen(). Can be overriden.
 */
path_fopen_t path_fopen;

/**
 * Simple wrapper for fopen(); ensures directories leading up to path 'path'
 * exist and are accessible.
 *
 * @param path
 * Path.
 * @param modes
 * Modes.
 * @return
 * Opened file, NULL on failure.
 */
static FILE *
fopen_wrapper (const char *path, const char *modes)
{
    path_ensure_directories(path);
    return fopen(path, modes);
}

TOOLKIT_INIT_FUNC(path)
{
    path_fopen = fopen_wrapper;
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(path)
{
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Joins two path components, eg, '/usr' and 'bin' -> '/usr/bin'.
 * @param path
 * First path component.
 * @param path2
 * Second path component.
 * @return
 * The joined path; should be freed when no longer needed.
 */
char *path_join(const char *path, const char *path2)
{
    StringBuffer *sb;
    size_t len;
    char *cp;

    TOOLKIT_PROTECT();

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
 * @param path
 * A path.
 * @return
 * A directory name. This string should be freed when no longer
 * needed.
 * @author Hongli Lai (public domain)
 */
char *path_dirname(const char *path)
{
    const char *end;
    char *result;

    TOOLKIT_PROTECT();

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
 * @param path
 * A path.
 * @return
 * The basename of the path. Should be freed when no longer
 * needed.
 */
char *path_basename(const char *path)
{
    const char *slash;

    TOOLKIT_PROTECT();

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
 * @param path
 * Path to normalize.
 * @return
 * The normalized path; never NULL. Must be freed.
 */
char *path_normalize(const char *path)
{
    StringBuffer *sb;
    size_t pos, startsbpos;
    char component[MAX_BUF];
    ssize_t last_slash;

    TOOLKIT_PROTECT();

    if (string_isempty(path)) {
        return estrdup(".");
    }

    sb = stringbuffer_new();
    pos = 0;

    if (string_startswith(path, "/")) {
        stringbuffer_append_string(sb, "/");
    } else if (string_startswith(path, "./")) {
        stringbuffer_append_string(sb, "./");
    }

    startsbpos = stringbuffer_length(sb);

    while (string_get_word(path, &pos, '/', component, sizeof(component), 0)) {
        if (strcmp(component, ".") == 0) {
            continue;
        }

        if (strcmp(component, "..") == 0) {
            if (stringbuffer_length(sb) > startsbpos) {
                last_slash = stringbuffer_rindex(sb, '/');

                if (last_slash == -1) {
                    LOG(BUG, "Should have found a forward slash, but didn't: %s", path);
                    continue;
                }

                stringbuffer_seek(sb, last_slash);
            }
        } else {
            size_t len = stringbuffer_length(sb);
            if (len == 0 || stringbuffer_data(sb)[len - 1] != '/') {
                stringbuffer_append_string(sb, "/");
            }

            stringbuffer_append_string(sb, component);
        }
    }

    if (stringbuffer_length(sb) == 0) {
        stringbuffer_append_string(sb, ".");
    }

    return stringbuffer_finish(sb);
}

/**
 * Checks whether any directories in the given path don't exist, and
 * creates them if necessary.
 * @param path
 * The path to check.
 */
void path_ensure_directories(const char *path)
{
    char buf[MAXPATHLEN], *cp;
    struct stat statbuf;

    TOOLKIT_PROTECT();

    if (path == NULL || *path == '\0') {
        return;
    }

    snprintf(VS(buf), "%s", path);
    cp = buf;

    while ((cp = strchr(cp + 1, '/')) != NULL) {
        *cp = '\0';

        if (mkdir(buf, 0777) != 0 && errno != EEXIST) {
            LOG(BUG, "Cannot mkdir %s (path: %s): %s", buf, path,
                    strerror(errno));
            return;
        }

        if (stat(buf, &statbuf) != 0) {
            LOG(BUG, "Cannot stat %s (path: %s): %s", buf, path,
                    strerror(errno));
            return;
        }

        if (!S_ISDIR(statbuf.st_mode)) {
            LOG(BUG, "Not a directory: %s (path: %s)", buf, path);
            return;
        }

        *cp = '/';
    }
}

/**
 * Copy the contents of file 'src' into 'dst'.
 * @param src
 * Path of the file to copy contents from.
 * @param dst
 * Where to put the contents of 'src'.
 * @param mode
 * Mode to open 'src' in.
 * @return
 * 1 on success, 0 on failure.
 */
int path_copy_file(const char *src, FILE *dst, const char *mode)
{
    FILE *fp;
    char buf[HUGE_BUF];

    TOOLKIT_PROTECT();

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
 * @param path
 * Path to check.
 * @return
 * 1 if 'path' exists, 0 otherwise.
 */
int path_exists(const char *path)
{
    struct stat statbuf;

    TOOLKIT_PROTECT();

    if (stat(path, &statbuf) != 0) {
        return 0;
    }

    return 1;
}

/**
 * Create a new blank file.
 * @param path
 * Path to the file.
 * @return
 * 1 on success, 0 on failure.
 */
int path_touch(const char *path)
{
    FILE *fp;

    TOOLKIT_PROTECT();

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
 * @param path
 * Path to the file.
 * @return
 * Size of the file.
 */
size_t path_size(const char *path)
{
    struct stat statbuf;

    TOOLKIT_PROTECT();

    if (stat(path, &statbuf) != 0) {
        return 0;
    }

    return statbuf.st_size;
}

/**
 * Load the entire contents of file 'path' into a StringBuffer instance,
 * then return the created string.
 * @param path
 * File to load contents of.
 * @return
 * The loaded contents. Must be freed.
 */
char *path_file_contents(const char *path)
{
    FILE *fp;
    StringBuffer *sb;
    char buf[MAX_BUF];

    TOOLKIT_PROTECT();

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

/**
 * Changes name of the specified file in an atomic manner.
 *
 * On POSIX systems, rename() is used; on Windows, the MoveFileEx() API is
 * used.
 *
 * @param old
 * File that is to be renamed.
 * @param new
 * New path for the file.
 * @return
 * 0 on success, an error number otherwise.
 */
int
path_rename (const char *old, const char *new)
{
#ifdef WIN32
    if (!MoveFileEx(old, new, MOVEFILE_REPLACE_EXISTING)) {
        return GetLastError();
    }

    return 0;
#else
    return rename(old, new);
#endif
}

bool
path_write_atomic (const char   *path,
                   const void   *data,
                   size_t        size,
                   unsigned int  mode)
{
    HARD_ASSERT(path != NULL);
    HARD_ASSERT(data != NULL || size == 0);

    path_ensure_directories(path);
    char temporary[HUGE_BUF];
    if (snprintf(VS(temporary), "%s.tmp.XXXXXX", path) >=
        (int) sizeof(temporary)) {
        return false;
    }

    int fd = mkstemp(temporary);
    if (fd == -1) {
        return false;
    }
#ifndef WIN32
    if (fchmod(fd, (mode_t) mode) != 0) {
        close(fd);
        unlink(temporary);
        return false;
    }
#else
    (void) mode;
#endif

    FILE *fp = fdopen(fd, "wb");
    if (fp == NULL) {
        close(fd);
        unlink(temporary);
        return false;
    }
    bool ok = fwrite(data, 1, size, fp) == size && fflush(fp) == 0;
#ifndef WIN32
    if (ok && fsync(fd) != 0) {
        ok = false;
    }
#endif
    if (fclose(fp) != 0) {
        ok = false;
    }
    if (!ok || path_rename(temporary, path) != 0) {
        unlink(temporary);
        return false;
    }
    return true;
}

path_secret_error_t
path_read_secret (const char *path,
                  char       *secret,
                  size_t      secret_size,
                  bool       *permissive_mode)
{
    HARD_ASSERT(path != NULL);
    HARD_ASSERT(secret != NULL);
    HARD_ASSERT(secret_size >= 2);

    OPENSSL_cleanse(secret, secret_size);
    if (permissive_mode != NULL) {
        *permissive_mode = false;
    }

#ifdef WIN32
    FILE *fp = fopen(path, "rb");
#else
    int flags = O_RDONLY;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    int fd = open(path, flags);
    FILE *fp = fd == -1 ? NULL : fdopen(fd, "rb");
    if (fp == NULL && fd != -1) {
        close(fd);
    }
#endif
    if (fp == NULL) {
        return PATH_SECRET_OPEN_ERROR;
    }

    path_secret_error_t result = PATH_SECRET_OK;
    unsigned char trailing[256] = {0};
    struct stat metadata;
    if (fstat(fileno(fp), &metadata) != 0) {
        result = PATH_SECRET_METADATA_ERROR;
        goto out;
    }
    if (!S_ISREG(metadata.st_mode)) {
        result = PATH_SECRET_NOT_REGULAR;
        goto out;
    }
#ifndef WIN32
    if (permissive_mode != NULL &&
            (metadata.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        *permissive_mode = true;
    }
#endif

    size_t length = fread(secret, 1, secret_size, fp);
    if (ferror(fp)) {
        result = PATH_SECRET_READ_ERROR;
        goto out;
    }

    char *newline = memchr(secret, '\n', length);
    if (newline != NULL) {
        for (char *cp = newline + 1; cp < secret + length; cp++) {
            if (*cp != '\r' && *cp != '\n') {
                result = PATH_SECRET_TRAILING_DATA;
                goto out;
            }
        }

        size_t trailing_length;
        while ((trailing_length = fread(trailing,
                                        1,
                                        sizeof(trailing),
                                        fp)) > 0) {
            for (size_t i = 0; i < trailing_length; i++) {
                if (trailing[i] != '\r' && trailing[i] != '\n') {
                    result = PATH_SECRET_TRAILING_DATA;
                    goto out;
                }
            }
            OPENSSL_cleanse(trailing, sizeof(trailing));
        }
        if (ferror(fp)) {
            result = PATH_SECRET_READ_ERROR;
            goto out;
        }

        size_t used = (size_t) (newline - secret);
        if (used > 0 && secret[used - 1] == '\r') {
            used--;
        }
        secret[used] = '\0';
    } else {
        if (!feof(fp) || length == secret_size) {
            result = PATH_SECRET_TOO_LONG;
            goto out;
        }
        secret[length] = '\0';
    }

    if (*secret == '\0') {
        result = PATH_SECRET_EMPTY;
    }

out:
    OPENSSL_cleanse(trailing, sizeof(trailing));
    if (fclose(fp) != 0 && result == PATH_SECRET_OK) {
        result = PATH_SECRET_READ_ERROR;
    }
    if (result != PATH_SECRET_OK) {
        OPENSSL_cleanse(secret, secret_size);
    }
    return result;
}

const char *
path_secret_error_string (path_secret_error_t error)
{
    switch (error) {
    case PATH_SECRET_OK:
        return "success";
    case PATH_SECRET_OPEN_ERROR:
        return "cannot open the file";
    case PATH_SECRET_METADATA_ERROR:
        return "cannot inspect the file";
    case PATH_SECRET_NOT_REGULAR:
        return "the path is not a regular file";
    case PATH_SECRET_EMPTY:
        return "the file is empty";
    case PATH_SECRET_TOO_LONG:
        return "the first line is too long";
    case PATH_SECRET_TRAILING_DATA:
        return "the file contains data after the first line";
    case PATH_SECRET_READ_ERROR:
        return "cannot read the file";
    }

    return "unknown error";
}

bool
path_is_safe_relative (const char *path)
{
    if (path == NULL || *path == '\0' || *path == '/' || *path == '\\') {
        return false;
    }

    const char *component = path;
    for (const char *cp = path; ; cp++) {
        if (*cp == '\\' || *cp == ':') {
            return false;
        }
        if (*cp != '/' && *cp != '\0') {
            continue;
        }

        size_t length = (size_t) (cp - component);
        if (length == 0 || (length == 1 && component[0] == '.') ||
                (length == 2 && component[0] == '.' &&
                 component[1] == '.')) {
            return false;
        }
        if (*cp == '\0') {
            return true;
        }
        component = cp + 1;
    }
}
