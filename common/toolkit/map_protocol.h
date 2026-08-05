/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

#ifndef TOOLKIT_MAP_PROTOCOL_H
#define TOOLKIT_MAP_PROTOCOL_H

#include "toolkit.h"

/**
 * Validate one complete protocol-v1068 CLIENT_CMD_MAP payload.
 *
 * No endpoint state is changed. The caller supplies the negotiated wire look
 * dimensions used to bound tile coordinates.
 */
bool map_protocol_validate(const uint8_t *data,
                           size_t len,
                           size_t pos,
                           int map_width,
                           int map_height);

#endif
