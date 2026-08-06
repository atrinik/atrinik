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
 * Keybindings header file.
 */

#ifndef KEYBIND_H
#define KEYBIND_H

/** Where keybindings are saved. */
#define FILE_KEYBIND "settings/keys.dat"

/**
 * One keybind.
 */
typedef struct keybind_struct {
    /** Command to execute. */
    char *command;

    /** Key bound. */
    SDL_Keycode key;

    /** Ctrl/shift/etc modifiers. */
    SDL_Keymod mod;

    /** Whether to trigger repeat. */
    uint8_t repeat;
} keybind_struct;

/** Public API implemented in src/client/keybind.c. */

extern keybind_struct **keybindings;

extern size_t keybindings_num;

extern void keybind_load(void);

extern void keybind_save(void);

extern void keybind_free(keybind_struct *keybind);

extern void keybind_deinit(void);

extern keybind_struct *keybind_add(SDL_Keycode key, SDL_Keymod mod, const char *command);

extern void keybind_edit(size_t i, SDL_Keycode key, SDL_Keymod mod, const char *command);

extern void keybind_remove(size_t i);

extern void keybind_repeat_toggle(size_t i);

extern char *keybind_get_key_shortcut(SDL_Keycode key, SDL_Keymod mod, char *buf, size_t len);

extern keybind_struct *keybind_find_by_command(const char *cmd);

extern int keybind_command_matches_event(const char *cmd, SDL_KeyboardEvent *event);

extern int keybind_command_matches_state(const char *cmd);

extern int keybind_process_event(SDL_KeyboardEvent *event);

extern void keybind_process(keybind_struct *keybind, SDL_EventType type, bool repeated);

extern int keybind_process_command_up(const char *cmd);

extern void keybind_state_ensure(void);

extern int keybind_process_command(const char *cmd);

/** Public API implemented in src/gui/popups/settings_keybinding.c. */

extern void settings_keybinding_open(void);

#endif
