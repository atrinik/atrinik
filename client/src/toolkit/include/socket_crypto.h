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
 * Socket crypto header file.
 *
 * @author Alex Tokar
 */

#ifndef SOCKET_CRYPTO_H
#define SOCKET_CRYPTO_H

#include <toolkit.h>
#include <packet.h>

/** Opaque typedef for the ::socket_crypto structure. */
typedef struct socket_crypto socket_crypto_t;

typedef enum socket_crypto_cb_id {
    SOCKET_CRYPTO_CB_SELFSIGNED,
    SOCKET_CRYPTO_CB_PUBCHANGED,

    SOCKET_CRYPTO_CB_MAX
} socket_crypto_cb_id_t;

typedef struct socket_crypto_cb_ctx {
    socket_crypto_cb_id_t id;
    char *hostname;
    union {
        void *ptr;
        char *str;
    } data;
} socket_crypto_cb_ctx_t;

typedef void (*socket_crypto_cb_t)(socket_crypto_t              *crypto,
                                   const socket_crypto_cb_ctx_t *ctx);

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(socket_crypto);

bool
socket_crypto_enabled(void);
bool
socket_crypto_has_curves(void);
bool
socket_crypto_curve_supported(const char *name, int *nid);
void
socket_crypto_packet_append_curves(packet_struct *packet);
const char *
socket_crypto_get_cert(void);
const char *
socket_crypto_get_cert_chain(void);
const char *
socket_crypto_get_cert_pubkey(void);
void
socket_crypto_set_path(const char *path);
bool
socket_crypto_check_cmd(uint8_t type, socket_crypto_t *crypto);
socket_crypto_t *
socket_crypto_create(socket_t *sc);
void
socket_crypto_set_nid(socket_crypto_t *crypto, int nid);
void
socket_crypto_set_cb(socket_crypto_t *crypto, socket_crypto_cb_t cb);
bool
socket_crypto_handle_cb(const socket_crypto_cb_ctx_t *ctx, char **errmsg);
void
socket_crypto_free_cb(const socket_crypto_cb_ctx_t *ctx);
void
socket_crypto_free(socket_crypto_t *crypto);
bool
socket_crypto_load_cert(socket_crypto_t *crypto,
                        const char      *cert_str,
                        const char      *chain_str);
bool
socket_crypto_load_pubkey(socket_crypto_t *crypto, const char *buf);
unsigned char *
socket_crypto_gen_pubkey(socket_crypto_t *crypto,
                         size_t          *pubkey_len);
const unsigned char *
socket_crypto_gen_iv(socket_crypto_t *crypto,
                     uint8_t         *iv_size);
const unsigned char *
socket_crypto_create_key(socket_crypto_t *crypto, uint8_t *len);
bool
socket_crypto_set_key(socket_crypto_t *crypto,
                      const uint8_t   *key,
                      uint8_t          key_len,
                      bool             reset_iv);
const unsigned char *
socket_crypto_get_iv(socket_crypto_t *crypto, uint8_t *len);
bool
socket_crypto_set_iv(socket_crypto_t *crypto,
                     const uint8_t   *iv,
                     uint8_t          iv_len);
const unsigned char *
socket_crypto_create_secret(socket_crypto_t *crypto,
                            uint8_t         *secret_len);
bool
socket_crypto_set_secret(socket_crypto_t *crypto,
                         uint8_t         *secret,
                         uint8_t          secret_len);
bool
socket_crypto_set_done(socket_crypto_t *crypto);
bool
socket_crypto_is_done(socket_crypto_t *crypto);
bool
socket_crypto_derive(socket_crypto_t     *crypto,
                     const unsigned char *pubkey,
                     size_t               pubkey_len,
                     const unsigned char *iv,
                     size_t               iv_size);
packet_struct *
socket_crypto_encrypt(socket_t      *sc,
                      packet_struct *packet_orig,
                      packet_struct *packet_meta,
                      bool           checksum_only);
bool
socket_crypto_decrypt(socket_t *sc,
                      uint8_t  *data,
                      size_t    len,
                      uint8_t **data_out,
                      size_t   *len_out);
bool
socket_crypto_client_should_encrypt(int type);
bool
socket_crypto_server_should_encrypt(int type);

#endif
