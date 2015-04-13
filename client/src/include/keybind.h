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
 * Keybindings header file. */

#ifndef KEYBIND_H
#define KEYBIND_H

/** Where keybindings are saved. */
#define FILE_KEYBIND "settings/keys.dat"

/**
 * One keybind. */
typedef struct keybind_struct {
    /** Command to execute. */
    char *command;

    /** Key bound. */
    SDLKey key;

    /** Ctrl/shift/etc modifiers. */
    SDLMod mod;

    /** Whether to trigger repeat. */
    uint8_t repeat;
} keybind_struct;

/** How quickly the key repeats. */
#define KEY_REPEAT_TIME (35)
/** Ticks that must pass before the key begins repeating. */
#define KEY_REPEAT_TIME_INIT (175)
/** Check whether the specified key is a modifier key. */
#define KEY_IS_MODIFIER(_key) ((_key) >= SDLK_NUMLOCK && (_key) <= SDLK_COMPOSE)

#endif
