/**
 * \file sha1.h
 *
 * \brief SHA-1 cryptographic hash function
 *
 *  Copyright (C) 2006-2010, Brainspark B.V.
 *
 *  This file is part of PolarSSL (http://www.polarssl.org)
 *  Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef POLARSSL_SHA1_H
#define POLARSSL_SHA1_H
#include <string.h>
#include "toolkit.h"

/**
 * \brief          SHA-1 context structure
 */
typedef struct {
    unsigned long total[2]; /*!< number of bytes processed  */
    unsigned long state[5]; /*!< intermediate digest state  */
    unsigned char buffer[64]; /*!< data block being processed */

    unsigned char ipad[64]; /*!< HMAC: inner padding        */
    unsigned char opad[64]; /*!< HMAC: outer padding        */
}
sha1_context;

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(sha1);

void sha1_starts(sha1_context *ctx);
void sha1_update(sha1_context *ctx, const unsigned char *input, size_t ilen);
void sha1_finish(sha1_context *ctx, unsigned char output[20]);
void sha1(const unsigned char *input, size_t ilen, unsigned char output[20]);
int sha1_file(const char *path, unsigned char output[20]);
void sha1_hmac_starts(sha1_context *ctx, const unsigned char *key, size_t keylen);
void sha1_hmac_update(sha1_context *ctx, const unsigned char *input, size_t ilen);
void sha1_hmac_finish(sha1_context *ctx, unsigned char output[20]);
void sha1_hmac_reset(sha1_context *ctx);
void sha1_hmac(const unsigned char *key, size_t keylen, const unsigned char *input, size_t ilen, unsigned char output[20]);

#endif
