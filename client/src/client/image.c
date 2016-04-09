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
 * Handles image related code.
 */

#include <global.h>
#include <packet.h>
#include <toolkit_string.h>
#include <path.h>

/**
 * Bitmaps loaded from image packs.
 */
static bmap_hash_t *image_bmap_packs = NULL;
/**
 * Bitmaps loaded from the server bmaps file.
 */
static bmap_t *image_bmaps = NULL;
/**
 * Number of entries in ::image_bmaps.
 */
static size_t image_bmaps_size = 0;

/**
 * Free data associated with a bmap_t structure.
 */
static void
bmap_free (bmap_t *bmap)
{
    HARD_ASSERT(bmap != NULL);
    efree(bmap->name);
}

/**
 * Read bmaps from image packs, calculate checksums, etc.
 */
void
image_init (void)
{
    FILE *fp = path_fopen(FILE_ATRINIK_P0, "rb");
    if (fp == NULL) {
        return;
    }

    size_t tmp_buf_size = 24 * 1024;
    char *tmp_buf = emalloc(tmp_buf_size);

    char buf[HUGE_BUF];
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        if (strncmp(buf, "IMAGE ", 6)) {
            LOG(ERROR, "The file %s is corrupted.", FILE_ATRINIK_P0);
            exit(1);
        }

        char *cp;
        /* Skip across the image ID data. */
        for (cp = buf + 6; *cp != ' '; cp++) {
        }

        size_t len = atoi(cp);

        /* Skip across the length data. */
        for (cp = cp + 1; *cp != ' '; cp++) {
        }

        /* Adjust the buffer if necessary. */
        if (len > tmp_buf_size) {
            tmp_buf_size = len;
            tmp_buf = erealloc(tmp_buf, tmp_buf_size);
        }

        long pos = ftell(fp);
        if (fread(tmp_buf, 1, len, fp) != len) {
            break;
        }

        string_strip_newline(cp);

        /* Trim left whitespace. */
        while (*cp == ' ') {
            cp++;
        }

        bmap_hash_t *bmap = ecalloc(1, sizeof(*bmap));
        bmap->bmap.name = estrdup(cp);
        bmap->bmap.crc32 = crc32(1L, (const unsigned char FAR *) tmp_buf, len);
        bmap->bmap.len = len;
        bmap->bmap.pos = pos;
        HASH_ADD_KEYPTR(hh,
                        image_bmap_packs,
                        bmap->bmap.name,
                        strlen(bmap->bmap.name),
                        bmap);
    }

    efree(tmp_buf);
    fclose(fp);
}

/*
 * Deinitialize the image packs.
 */
void
image_deinit (void)
{
    bmap_hash_t *curr, *tmp;

    HASH_ITER(hh, image_bmap_packs, curr, tmp) {
        HASH_DEL(image_bmap_packs, curr);
        bmap_free(&curr->bmap);
        efree(curr);
    }
}

/**
 * Read bmaps server file.
 */
void
image_bmaps_init (void)
{
    FILE *fp = server_file_open_name(SERVER_FILE_BMAPS);
    if (fp == NULL) {
        return;
    }

    /* Free previously allocated bmaps. */
    image_bmaps_deinit();

    char buf[HUGE_BUF];
    while (fgets(buf, sizeof(buf), fp)) {
        uint32_t len, crc;
        char name[HUGE_BUF];
        if (sscanf(buf, "%x %x %s", &len, &crc, name) != 3) {
            LOG(BUG, "Syntax error in server bmaps file: %s", buf);
            break;
        }

        bmap_hash_t *bmap;
        HASH_FIND_STR(image_bmap_packs, name, bmap);

        /* Expand the array. */
        image_bmaps = erealloc(image_bmaps,
                               sizeof(*image_bmaps) * (image_bmaps_size + 1));

        /* Does it exist, and the lengths and checksums match? */
        if (bmap != NULL && bmap->bmap.len == len && bmap->bmap.crc32 == crc) {
            image_bmaps[image_bmaps_size].pos = bmap->bmap.pos;
        } else {
            /* It doesn't exist in the atrinik.p0 file. */
            image_bmaps[image_bmaps_size].pos = -1;
        }

        image_bmaps[image_bmaps_size].len = len;
        image_bmaps[image_bmaps_size].crc32 = crc;
        image_bmaps[image_bmaps_size].name = estrdup(name);

        image_bmaps_size++;
    }

    fclose(fp);
}

/**
 * Deinitialize the bmaps.
 */
void
image_bmaps_deinit (void)
{
    if (image_bmaps != NULL) {
        for (size_t i = 0; i < image_bmaps_size; i++) {
            efree(image_bmaps[i].name);
        }

        efree(image_bmaps);
        image_bmaps = NULL;
        image_bmaps_size = 0;
    }

    for (size_t i = 0; i < MAX_FACE_TILES; i++) {
        if (FaceList[i].name != NULL) {
            efree(FaceList[i].name);
            FaceList[i].name = NULL;
            sprite_free_sprite(FaceList[i].sprite);
            FaceList[i].sprite = NULL;
            FaceList[i].checksum = 0;
            FaceList[i].flags = 0;
        }
    }

    sprite_cache_free_all();
}

/**
 * Finish face command.
 *
 * @param pnum
 * ID of the face.
 * @param checksum
 * Face checksum.
 * @param face
 * Face name.
 */
void
finish_face_cmd (int facenum, uint32_t checksum, const char *face)
{
    HARD_ASSERT(facenum >= 0);
    HARD_ASSERT(face != NULL);

    /* Loaded or requested. */
    if (FaceList[facenum].name != NULL) {
        if (strcmp(face, FaceList[facenum].name) == 0 &&
            checksum == FaceList[facenum].checksum &&
            FaceList[facenum].sprite != NULL) {
            return;
        }

        /* Something is different. */
        efree(FaceList[facenum].name);
        FaceList[facenum].name = NULL;
        sprite_free_sprite(FaceList[facenum].sprite);
        FaceList[facenum].sprite = NULL;
    }

    char buf[HUGE_BUF];
    snprintf(VS(buf), "%s.png", face);
    FaceList[facenum].name = estrdup(buf);
    FaceList[facenum].checksum = checksum;

    /* Check private cache first */
    snprintf(VS(buf), DIRECTORY_CACHE "/%s", FaceList[facenum].name);

    FILE *fp = path_fopen(buf, "rb");
    if (fp != NULL) {
        struct stat statbuf;
        fstat(fileno(fp), &statbuf);
        size_t len = statbuf.st_size;
        unsigned char *data = emalloc(len);
        len = fread(data, 1, len, fp);
        fclose(fp);
        uint32_t newsum = 0;

        /* Something is wrong... Unlink the file and let it reload. */
        if (len == 0) {
            unlink(buf);
            checksum = 1;
        } else {
            /* Checksum check */
            newsum = crc32(1L, data, len);
        }

        efree(data);

        if (newsum == checksum) {
            FaceList[facenum].sprite = sprite_tryload_file(buf, 0, NULL);
            if (FaceList[facenum].sprite != NULL) {
                return;
            }
        }
    }

    packet_struct *packet = packet_new(SERVER_CMD_ASK_FACE, 16, 0);
    packet_append_uint16(packet, facenum);
    socket_send_packet(packet);
}

/**
 * Load picture from the image pack file.
 *
 * @param num
 * ID of the picture to load.
 */
static void
load_picture_from_pack (int num)
{
    FILE *fp = path_fopen(FILE_ATRINIK_P0, "rb");
    if (fp == NULL) {
        LOG(ERROR, "Failed to open %s", FILE_ATRINIK_P0);
        return;
    }

    if (lseek(fileno(fp), image_bmaps[num].pos, SEEK_SET) == -1) {
        LOG(ERROR, "Failed to seek to %ld: %s",
            image_bmaps[num].pos, strerror(errno));
        fclose(fp);
        return;
    }

    char *buf = emalloc(image_bmaps[num].len);
    size_t num_read = fread(buf, 1, image_bmaps[num].len, fp);
    if (num_read != image_bmaps[num].len) {
        LOG(ERROR, "Expected %" PRIu64 " bytes but read %" PRIu64 " bytes",
            (uint64_t) image_bmaps[num].len,
            (uint64_t) num_read);
        efree(buf);
        fclose(fp);
        return;
    }

    fclose(fp);

    SDL_RWops *rwop = SDL_RWFromMem(buf, image_bmaps[num].len);
    if (rwop == NULL) {
        LOG(ERROR, "Failed to load image from pack using SDL_RWFromMem(): %s",
            SDL_GetError());
    } else {
        FaceList[num].sprite = sprite_tryload_file(NULL, 0, rwop);
        SDL_FreeRW(rwop);
    }

    efree(buf);
}

/**
 * Load face from user's graphics directory.
 *
 * @param num
 * ID of the face to load.
 * @return
 * True on success, false on failure.
 */
static bool
load_gfx_user_face (uint16_t num)
{
    /* First check for this image in gfx_user directory. */
    char buf[MAX_BUF];
    snprintf(VS(buf), DIRECTORY_GFX_USER "/%s.png", image_bmaps[num].name);

    FILE *fp = path_fopen(buf, "rb");
    if (fp == NULL) {
        return false;
    }

    struct stat statbuf;
    fstat(fileno(fp), &statbuf);
    size_t len = statbuf.st_size;
    unsigned char *data = emalloc(len);
    len = fread(data, 1, len, fp);

    bool ret = false;
    if (len == 0) {
        goto out;
    }

    if (FaceList[num].sprite != NULL) {
        sprite_free_sprite(FaceList[num].sprite);
    }

    if (FaceList[num].name != NULL) {
        efree(FaceList[num].name);
        FaceList[num].name = NULL;
    }

    /* Try to load it. */
    FaceList[num].sprite = sprite_tryload_file(buf, 0, NULL);
    if (FaceList[num].sprite == NULL) {
        goto out;
    }

    FaceList[num].name = estrdup(buf);
    FaceList[num].checksum = crc32(1L, data, len);
    ret = true;

out:
    efree(data);
    fclose(fp);

    return ret;
}

/**
 * We got a face - test if we have it loaded. If not, ask the server to
 * send us face command.
 *
 * @param pnum
 * Face ID.
 */
void
image_request_face (int pnum)
{
    char buf[MAX_BUF];
    uint16_t num = (uint16_t) (pnum &~0x8000);

    if (setting_get_int(OPT_CAT_DEVEL, OPT_RELOAD_GFX) &&
        load_gfx_user_face(num)) {
        return;
    }

    /* Loaded or requested */
    if (FaceList[num].name != NULL || FaceList[num].flags & FACE_REQUESTED) {
        return;
    }

    if (num >= image_bmaps_size) {
        LOG(ERROR, "Server sent picture ID too loarge (%d, max: %" PRIu64 ")",
            num, (uint64_t) image_bmaps_size);
        return;
    }

    if (load_gfx_user_face(num)) {
        return;
    }

    if (image_bmaps[num].pos != -1) {
        snprintf(VS(buf), "%s.png", image_bmaps[num].name);
        FaceList[num].name = estrdup(buf);
        FaceList[num].checksum = image_bmaps[num].crc32;
        load_picture_from_pack(num);
    } else {
        FaceList[num].flags |= FACE_REQUESTED;
        finish_face_cmd(num, image_bmaps[num].crc32, image_bmaps[num].name);
    }
}

/**
 * Find a face ID by name. Request the face by finding it, loading it or
 * requesting it.
 *
 * @param name
 * Face name to find.
 * @return
 * Face ID if found, -1 otherwise.
 */
int
image_get_id (const char *name)
{
    int l = 0, r = image_bmaps_size - 1;

    /* All the faces in ::image_bmaps are already sorted, so we can use a
     * binary search here. */
    while (r >= l) {
        int x = (l + r) / 2;
        int cmp = strcmp(name, image_bmaps[x].name);
        if (cmp < 0) {
            r = x - 1;
        } else if (cmp > 0) {
            l = x + 1;
        } else {
            image_request_face(x);
            return x;
        }
    }

    return -1;
}
