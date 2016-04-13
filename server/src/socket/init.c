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
 * Socket initialization related code.
 */

#include "zlib.h"

#include <global.h>
#include <packet.h>
#include <toolkit_string.h>
#include <exp.h>

/** Socket information. */
Socket_Info socket_info;
/** Established connections for clients not yet playing. */
socket_struct *init_sockets;

/**
 * Initializes a connection - really, it just sets up the data structure,
 * socket setup is handled elsewhere.
 *
 * Sends server version to the client.
 * @param ns
 * Client's socket.
 * @todo Remove version sending legacy support for older clients at some
 * point.
 */
bool init_connection(socket_struct *ns)
{
    if (!socket_opt_non_blocking(ns->sc, true)) {
        return false;
    }
    if (!socket_opt_send_buffer(ns->sc, 65535)) {
        return false;
    }

    ns->login_count = 0;
    ns->keepalive = 0;
    ns->addme = 0;
    ns->faceset = 0;
    ns->sound = 0;
    ns->ext_title_flag = 1;
    ns->state = ST_LOGIN;
    ns->mapx = 17;
    ns->mapy = 17;
    ns->mapx_2 = 8;
    ns->mapy_2 = 8;
    ns->password_fails = 0;
    ns->is_bot = 0;
    ns->account = NULL;
    ns->socket_version = 0;

    ns->packet_recv = packet_new(0, 1024 * 3, 0);
    ns->packet_recv_cmd = packet_new(0, 1024 * 64, 0);

    memset(&ns->lastmap, 0, sizeof(struct Map));
    ns->packets = NULL;

    return true;
}

/**
 * This sets up the socket and reads all the image information into
 * memory.
 */
void init_ericserver(void)
{
#ifndef WIN32
#ifdef HAVE_SYSCONF
    socket_info.max_filedescriptor = sysconf(_SC_OPEN_MAX);
#else
#ifdef HAVE_GETDTABLESIZE
    socket_info.max_filedescriptor = getdtablesize();
#else
    "Unable to find usable function to get max filedescriptors";
#endif
#endif
#else
    /* Used in select, ignored in winsockets */
    socket_info.max_filedescriptor = 1;
#endif

    socket_info.timeout.tv_sec = 0;
    socket_info.timeout.tv_usec = 0;
    socket_info.nconns = 0;

    socket_info.nconns = 1;
    init_sockets = emalloc(sizeof(socket_struct));
    socket_info.allocated_sockets = 1;

    init_sockets[0].sc = socket_create(NULL, settings.port);
    if (init_sockets[0].sc == NULL) {
        exit(1);
    }
    if (!socket_opt_linger(init_sockets[0].sc, false, 0)) {
        exit(1);
    }
    if (!socket_opt_reuse_addr(init_sockets[0].sc, true)) {
        exit(1);
    }
    if (!socket_bind(init_sockets[0].sc)) {
        exit(1);
    }

    init_sockets[0].state = ST_WAITING;
}

/**
 * Frees all the memory that ericserver allocates.
 */
void free_all_newserver(void)
{
    int i;

    free_socket_images();

    for (i = 1; i < socket_info.allocated_sockets; i++) {
        if (init_sockets[i].state != ST_AVAILABLE) {
            free_newsocket(&init_sockets[i]);
        }
    }

    socket_destroy(init_sockets[0].sc);
    efree(init_sockets);
}

/**
 * Basically, all we need to do here is free all data structures that
 * might be associated with the socket.
 *
 * It is up to the caller to update the list.
 * @param ns
 * The socket.
 */
void free_newsocket(socket_struct *ns)
{
    socket_destroy(ns->sc);

    if (ns->account) {
        efree(ns->account);
    }

    if (ns->packet_recv != NULL) {
        packet_free(ns->packet_recv);
    }

    if (ns->packet_recv_cmd != NULL) {
        packet_free(ns->packet_recv_cmd);
    }

    socket_buffer_clear(ns);
    efree(ns);
}

/**
 * Load server file.
 * @param fname
 * Filename of the server file.
 * @param listing
 * Open file pointer to the listings file.
 * @param cmd
 * The data command.
 */
static void load_srv_file(char *fname, FILE *listing)
{
    FILE *fp;
    char *contents, *compressed, *cp;
    StringBuffer *sb;
    size_t fsize, numread;
    struct stat statbuf;
    unsigned long crc;

    if ((fp = fopen(fname, "rb")) == NULL) {
        LOG(ERROR, "Can't open file %s", fname);
        exit(1);
    }

    fstat(fileno(fp), &statbuf);
    fsize = statbuf.st_size;
    /* Allocate a buffer to hold the whole file. */
    contents = emalloc(fsize);
    numread = fread(contents, 1, fsize, fp);
    fclose(fp);

    /* Get a crc from the uncompressed file. */
    crc = crc32(1L, (const unsigned char FAR *) contents, numread);

    /* Calculate the upper bound of the compressed size. */
    numread = compressBound(fsize);
    /* Allocate a buffer to hold the compressed file. */
    compressed = emalloc(numread);
    compress2((Bytef *) compressed, (uLong *) & numread,
            (const unsigned char FAR *) contents, fsize, Z_BEST_COMPRESSION);

    sb = stringbuffer_new();
    cp = path_basename(fname);
    stringbuffer_append_printf(sb, "%s/http/data/%s.zz", settings.datapath, cp);
    fprintf(listing, "%s:%"PRIx64":%"PRIx64"\n", cp, (uint64_t) crc,
            (uint64_t) fsize);

    efree(cp);
    cp = stringbuffer_finish(sb);
    path_ensure_directories(cp);
    fp = fopen(cp, "wb");

    if (fp == NULL) {
        LOG(ERROR, "Could not open %s for writing.", cp);
        exit(1);
    }

    fwrite(compressed, 1, numread, fp);
    fclose(fp);

    /* Free temporary buffers. */
    efree(cp);
    efree(contents);
    efree(compressed);
}

/**
 * Get the lib/server_settings default file and create the
 * data/server_settings file from it.
 */
static void create_server_settings(void)
{
    char buf[MAX_BUF];
    size_t i;
    FILE *fp;

    snprintf(buf, sizeof(buf), "%s/settings", settings.datapath);

    fp = fopen(buf, "wb");

    if (!fp) {
        LOG(ERROR, "Couldn't create %s.", buf);
        exit(1);
    }

    /* Copy the default. */
    snprintf(buf, sizeof(buf), "%s/server_settings", settings.libpath);

    if (!path_copy_file(buf, fp, "r")) {
        LOG(ERROR, "Couldn't copy %s.", buf);
        exit(1);
    }

    for (i = 0; i < ALLOWED_CHARS_NUM; i++) {
        fprintf(fp, "text %s\n", settings.allowed_chars[i]);
        fprintf(fp, "text %"PRIu64 "-%"PRIu64 "\n", (uint64_t) settings.limits[i][0], (uint64_t) settings.limits[i][1]);
    }

    /* Add the level information. */
    fprintf(fp, "level %d\n", MAXLEVEL);

    for (i = 0; i <= MAXLEVEL; i++) {
        fprintf(fp, "%"PRIx64 "\n", new_levels[i]);
    }

    fclose(fp);
}

/**
 * Initialize animations file for the client.
 */
static void create_server_animations(void)
{
    char buf[MAX_BUF];
    snprintf(VS(buf), "%s/anims", settings.datapath);

    FILE *fp = fopen(buf, "wb");
    if (fp == NULL) {
        LOG(ERROR, "Couldn't create %s.", buf);
        exit(1);
    }

    snprintf(VS(buf), "%s/animations", settings.libpath);
    FILE *fp2 = fopen(buf, "rb");
    if (fp2 == NULL) {
        LOG(ERROR, "Couldn't open %s.", buf);
        exit(1);
    }

    while (fgets(VS(buf), fp2)) {
        /* Copy anything but face names. */
        if (strncmp(buf, "anim ", 5) == 0 || strcmp(buf, "mina\n") == 0 ||
                strncmp(buf, "facings ", 8) == 0) {
            fputs(buf, fp);
        } else {
            char *end = strchr(buf, '\n');
            if (end != NULL) {
                *end = '\0';
            }

            /* Transform face names into IDs. */
            fprintf(fp, "%d\n", find_face(buf, 0));
        }
    }

    fclose(fp2);
    fclose(fp);
}

/**
 * Load all the server files we can send to client.
 *
 * client_bmaps is generated from the server at startup out of the
 * Atrinik png file.
 */
void init_srv_files(void)
{
    char buf[MAX_BUF];
    FILE *fp;

    snprintf(buf, sizeof(buf), "%s/http/data/listing.txt", settings.datapath);
    path_ensure_directories(buf);
    fp = fopen(buf, "w");

    if (fp == NULL) {
        LOG(ERROR, "Could not open %s for writing.", buf);
        exit(1);
    }

    snprintf(buf, sizeof(buf), "%s/bmaps", settings.datapath);
    load_srv_file(buf, fp);

    snprintf(buf, sizeof(buf), "%s/"UPDATES_FILE_NAME, settings.datapath);
    load_srv_file(buf, fp);

    create_server_settings();
    snprintf(buf, sizeof(buf), "%s/settings", settings.datapath);
    load_srv_file(buf, fp);

    create_server_animations();
    snprintf(buf, sizeof(buf), "%s/anims", settings.datapath);
    load_srv_file(buf, fp);

    snprintf(buf, sizeof(buf), "%s/effects", settings.libpath);
    load_srv_file(buf, fp);

    snprintf(buf, sizeof(buf), "%s/hfiles", settings.libpath);
    load_srv_file(buf, fp);

    fclose(fp);
}
