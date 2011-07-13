/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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

#include <global.h>

keybind_struct **keybindings = NULL;
size_t keybindings_num = 0;

void keybind_load()
{
	FILE *fp;
	char buf[HUGE_BUF], *cp;
	keybind_struct *keybind = NULL;

	fp = fopen_wrapper(FILE_KEYBIND, "r");

	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		cp = strchr(buf, '\n');

		if (cp)
		{
			*cp = '\0';
		}

		cp = buf;

		while (*cp != '\0')
		{
			if (isspace(*cp))
			{
				cp++;
			}
			else
			{
				break;
			}
		}

		if (*cp == '#' || *cp == '\0')
		{
			continue;
		}

		if (!strcmp(cp, "end"))
		{
			keybindings = realloc(keybindings, sizeof(keybindings) * (keybindings_num + 1));
			keybindings[keybindings_num] = keybind;
			keybindings_num++;
			keybind = NULL;
		}
		else if (keybind)
		{
			if (!strncmp(cp, "command ", 8))
			{
				keybind->command = strdup(cp + 8);
			}
			else if (!strncmp(cp, "key ", 4))
			{
				keybind->key = atoi(cp + 4);
			}
			else if (!strncmp(cp, "mod ", 4))
			{
				keybind->mod = atoi(cp + 4);
			}
			else if (!strncmp(cp, "repeat ", 7))
			{
				keybind->repeat = atoi(cp + 7);
			}
		}
		else if (!strcmp(cp, "bind"))
		{
			keybind = calloc(1, sizeof(*keybind));
		}
	}

	fclose(fp);
}

void keybind_save()
{
	FILE *fp;
	size_t i;

	fp = fopen_wrapper(FILE_KEYBIND, "w");

	for (i = 0; i < keybindings_num; i++)
	{
		fprintf(fp, "bind\n");
		fprintf(fp, "\t# %s\n\tkey %d\n", SDL_GetKeyName(keybindings[i]->key), keybindings[i]->key);

		if (keybindings[i]->mod)
		{
			fprintf(fp, "\tmod %d\n", keybindings[i]->mod);
		}

		if (keybindings[i]->repeat)
		{
			fprintf(fp, "\trepeat %d\n", keybindings[i]->repeat);
		}

		if (keybindings[i]->command)
		{
			fprintf(fp, "\tcommand %s\n", keybindings[i]->command);
		}

		fprintf(fp, "end\n");
	}

	fclose(fp);
}

void keybind_deinit()
{
	size_t i;

	keybind_save();

	for (i = 0; i < keybindings_num; i++)
	{
		free(keybindings[i]->command);
		free(keybindings[i]);
	}

	free(keybindings);
	keybindings = NULL;
	keybindings_num = 0;
}

void keybind_remove(size_t i)
{
	size_t j;

	if (i >= keybindings_num)
	{
		return;
	}

	free(keybindings[i]->command);
	free(keybindings[i]);

	for (j = i + 1; j < keybindings_num; j++)
	{
		keybindings[j - 1] = keybindings[j];
	}

	keybindings_num--;
	keybindings = realloc(keybindings, sizeof(*keybindings) * keybindings_num);
}

void keybind_repeat_toggle(size_t i)
{
	if (i >= keybindings_num)
	{
		return;
	}

	keybindings[i]->repeat = !keybindings[i]->repeat;
}

SDLMod keybind_adjust_kmod(SDLMod mod)
{
	mod &= KMOD_SHIFT | KMOD_CTRL | KMOD_ALT | KMOD_META;

	if (mod & KMOD_SHIFT)
	{
		mod |= KMOD_SHIFT;
	}

	if (mod & KMOD_CTRL)
	{
		mod |= KMOD_CTRL;
	}

	if (mod & KMOD_ALT)
	{
		mod |= KMOD_ALT;
	}

	if (mod & KMOD_META)
	{
		mod |= KMOD_META;
	}

	return mod;
}

keybind_struct *keybind_add(SDLKey key, SDLMod mod, const char *command)
{
	keybind_struct *keybind;

	keybind = calloc(1, sizeof(*keybind));
	keybind->key = key;
	keybind->mod = keybind_adjust_kmod(mod);
	keybind->command = strdup(command);

	keybindings = realloc(keybindings, sizeof(keybindings) * (keybindings_num + 1));
	keybindings[keybindings_num] = keybind;
	keybindings_num++;

	return keybind;
}

void keybind_edit(size_t i, SDLKey key, SDLMod mod, const char *command)
{
	if (i >= keybindings_num)
	{
		return;
	}

	keybindings[i]->key = key;
	keybindings[i]->mod = mod;
	free(keybindings[i]->command);
	keybindings[i]->command = strdup(command);
}

void keybind_get_key_shortcut(SDLKey key, SDLMod mod, char *buf, size_t len)
{
	buf[0] = '\0';

	if (mod)
	{
		if (mod & KMOD_SHIFT)
		{
			strncat(buf, "shift + ", len - strlen(buf) - 1);
		}

		if (mod & KMOD_CTRL)
		{
			strncat(buf, "ctrl + ", len - strlen(buf) - 1);
		}

		if (mod & KMOD_ALT)
		{
			strncat(buf, "alt + ", len - strlen(buf) - 1);
		}

		if (mod & KMOD_META)
		{
			strncat(buf, "super + ", len - strlen(buf) - 1);
		}
	}

	if (key != SDLK_UNKNOWN)
	{
		strncat(buf, SDL_GetKeyName(key), len - strlen(buf) - 1);
	}
}

keybind_struct *keybind_find_by_command(const char *cmd)
{
	size_t i;

	for (i = 0; i < keybindings_num; i++)
	{
		if (!strcmp(cmd, keybindings[i]->command))
		{
			return keybindings[i];
		}
	}

	return NULL;
}

SDLKey key_find_by_command(const char *cmd)
{
	size_t i;

	for (i = 0; i < keybindings_num; i++)
	{
		if (!strcmp(cmd, keybindings[i]->command))
		{
			return keybindings[i]->key;
		}
	}

	return SDLK_UNKNOWN;
}

int keybind_command_matches_event(const char *cmd, SDL_KeyboardEvent *event)
{
	size_t i;

	for (i = 0; i < keybindings_num; i++)
	{
		if (!strcmp(keybindings[i]->command, cmd) && event->keysym.sym == keybindings[i]->key && (!keybindings[i]->mod || keybindings[i]->mod == keybind_adjust_kmod(event->keysym.mod)))
		{
			return 1;
		}
	}

	return 0;
}

int keybind_process_event(SDL_KeyboardEvent *event)
{
	size_t i;

	for (i = 0; i < keybindings_num; i++)
	{
		if (event->keysym.sym == keybindings[i]->key && (!keybindings[i]->mod || keybindings[i]->mod == keybind_adjust_kmod(event->keysym.mod)))
		{
			keybind_process(keybindings[i], event->type);
			return 1;
		}
	}

	return 0;
}

void keybind_repeat()
{
	size_t i;

	for (i = 0; i < keybindings_num; i++)
	{
		if (keys[keybindings[i]->key].pressed && keybindings[i]->repeat && ((*keybindings[i]->command == '?' && !keybindings[i]->mod) || keybindings[i]->mod == keybind_adjust_kmod(SDL_GetModState())))
		{
			/* If time to repeat */
			if (keys[keybindings[i]->key].time + KEY_REPEAT_TIME - 5 < LastTick)
			{
				/* Repeat x times */
				while ((keys[keybindings[i]->key].time += KEY_REPEAT_TIME - 5) < LastTick)
				{
					keybind_process(keybindings[i], SDL_KEYDOWN);
				}
			}
		}
	}
}

void keybind_process(keybind_struct *keybind, uint8 type)
{
	char command[MAX_BUF], *cp;

	strncpy(command, keybind->command, sizeof(command) - 1);
	command[sizeof(command) - 1] = '\0';

	cp = strtok(command, ";");

	while (cp)
	{
		while (*cp == ' ')
		{
			cp++;
		}

		if (type == SDL_KEYDOWN)
		{
			keybind_process_command(cp);
		}
		else
		{
			keybind_process_command_up(cp);
		}

		cp = strtok(NULL, ";");
	}
}

int keybind_process_command_up(const char *cmd)
{
	if (*cmd == '?')
	{
		cmd++;

		if (!strcmp(cmd, "INVENTORY"))
		{
			cpl.inventory_win = IWIN_BELOW;
		}
		else if (!strcmp(cmd, "RUNON"))
		{
			send_command("/run_stop");
			cpl.run_on = 0;
		}
		else if (!strcmp(cmd, "FIREON"))
		{
			cpl.fire_on = 0;
		}

		return 1;
	}

	return 0;
}

int keybind_process_command(const char *cmd)
{
	if (*cmd == '?')
	{
		int tag = 0, loc = 0;
		object *it;

		cmd++;

		if (!strncmp(cmd, "MOVE_", 5))
		{
			cmd += 5;

			if (!strcmp(cmd, "N"))
			{
				move_keys(8);
			}
			else if (!strcmp(cmd, "NE"))
			{
				move_keys(9);
			}
			else if (!strcmp(cmd, "E"))
			{
				move_keys(6);
			}
			else if (!strcmp(cmd, "SE"))
			{
				move_keys(3);
			}
			else if (!strcmp(cmd, "S"))
			{
				move_keys(2);
			}
			else if (!strcmp(cmd, "SW"))
			{
				move_keys(1);
			}
			else if (!strcmp(cmd, "W"))
			{
				move_keys(4);
			}
			else if (!strcmp(cmd, "NW"))
			{
				move_keys(7);
			}
			else if (!strcmp(cmd, "N"))
			{
				move_keys(8);
			}
			else if (!strcmp(cmd, "STAY"))
			{
				move_keys(5);
			}
		}
		else if (!strncmp(cmd, "PAGE", 4))
		{
			widgetdata *widget;
			int scroll_adjust = 0;

			cmd += 4;

			if (!strncmp(cmd, "UP", 2))
			{
				widget = cur_widget[*(cmd + 2) == '\0' ? CHATWIN_ID : MSGWIN_ID];
				scroll_adjust = -TEXTWIN_ROWS_VISIBLE(widget);
			}
			else if (!strncmp(cmd, "DOWN", 4))
			{
				widget = cur_widget[*(cmd + 4) == '\0' ? CHATWIN_ID : MSGWIN_ID];
				scroll_adjust = TEXTWIN_ROWS_VISIBLE(widget);
			}

			if (scroll_adjust)
			{
				TEXTWIN(widget)->scroll += scroll_adjust;
				textwin_scroll_adjust(widget);
				WIDGET_REDRAW(widget);
			}
		}
		else if (!strcmp(cmd, "CONSOLE"))
		{
			reset_keys();

			if (cpl.input_mode == INPUT_MODE_NO)
			{
				cpl.input_mode = INPUT_MODE_CONSOLE;
				text_input_open(253);
			}
			else if (cpl.input_mode == INPUT_MODE_CONSOLE)
			{
				cpl.input_mode = INPUT_MODE_NO;
			}
		}
		else if (!strcmp(cmd, "APPLY"))
		{
			if (cpl.inventory_win == IWIN_BELOW)
			{
				tag = cpl.win_below_tag;
			}
			else
			{
				tag = cpl.win_inv_tag;
			}

			if (tag == -1 || !object_find(tag))
			{
				return 0;
			}

			draw_info_format(COLOR_DGOLD, "apply %s", object_find(tag)->s_name);
			client_send_apply(tag);
		}
		else if (!strcmp(cmd, "EXAMINE"))
		{
			if (cpl.inventory_win == IWIN_BELOW)
			{
				tag = cpl.win_below_tag;
			}
			else
			{
				tag = cpl.win_inv_tag;
			}

			if (tag == -1 || !object_find(tag))
			{
				return 0;
			}

			draw_info_format(COLOR_DGOLD, "examine %s", object_find(tag)->s_name);
			client_send_examine(tag);
		}
		else if (!strcmp(cmd, "MARK"))
		{
			if (cpl.inventory_win == IWIN_BELOW)
			{
				tag = cpl.win_below_tag;
			}
			else
			{
				tag = cpl.win_inv_tag;
			}

			if (tag == -1 || !object_find(tag))
			{
				return 0;
			}

			it = object_find(tag);
			draw_info_format(COLOR_DGOLD, "%smark %s", it->tag == cpl.mark_count ? "un" : "", it->s_name);
			object_send_mark(it);
		}
		else if (!strcmp(cmd, "LOCK"))
		{
			if (cpl.inventory_win == IWIN_BELOW)
			{
				tag = cpl.win_below_tag;
			}
			else
			{
				tag = cpl.win_inv_tag;
			}

			if (tag == -1  || !object_find(tag))
			{
				return 0;
			}

			it = object_find(tag);
			toggle_locked(it);

			if (it->flags & F_LOCKED)
			{
				draw_info_format(COLOR_DGOLD, "unlock %s", it->s_name);
			}
			else
			{
				draw_info_format(COLOR_DGOLD, "lock %s", it->s_name);
			}
		}
		else if (!strcmp(cmd, "GET"))
		{
			int nrof = 1;

			/* From below to inv. */
			if (cpl.inventory_win == IWIN_BELOW)
			{
				tag = cpl.win_below_tag;

				if (cpl.container)
				{
					if (cpl.container->tag != cpl.win_below_ctag)
					{
						loc = cpl.container->tag;
					}
					else
					{
						loc = cpl.ob->tag;
					}
				}
				else
				{
					loc = cpl.ob->tag;
				}
			}
			/* Inventory */
			else
			{
				if (cpl.container)
				{
					if (cpl.container->tag == cpl.win_inv_ctag)
					{
						tag = cpl.win_inv_tag;
						loc = cpl.ob->tag;
					}
					/* From inventory to container - if the container is in inv. */
					else
					{
						object *tmp;

						tag = -1;

						if (cpl.ob)
						{
							for (tmp = cpl.ob->inv; tmp; tmp = tmp->next)
							{
								if (tmp->tag == cpl.container->tag)
								{
									tag = cpl.win_inv_tag;
									loc = cpl.container->tag;
									break;
								}
							}

							if (tag == -1)
							{
								draw_info(COLOR_DGOLD, "You already have it.");
							}
						}
					}
				}
				else
				{
					draw_info(COLOR_DGOLD, "You have no open container to put it in.");
					tag = -1;
				}
			}

			if (tag == -1 || !object_find(tag))
			{
				return 0;
			}

			it = object_find(tag);
			nrof = it->nrof;

			if (nrof == 1)
			{
				nrof = 0;
			}
			else if (!(setting_get_int(OPT_CAT_GENERAL, OPT_COLLECT_MODE) & 1))
			{
				char buf[MAX_BUF];

				reset_keys();
				cpl.input_mode = INPUT_MODE_NUMBER;
				text_input_open(22);
				cpl.loc = loc;
				cpl.tag = tag;
				cpl.nrof = nrof;
				cpl.nummode = NUM_MODE_GET;
				snprintf(buf, sizeof(buf), "%d", nrof);
				text_input_add_string(buf);
				strncpy(cpl.num_text, it->s_name, sizeof(cpl.num_text) - 1);
				cpl.num_text[sizeof(cpl.num_text) - 1] = '\0';
				return 0;
			}

			draw_info_format(COLOR_DGOLD, "get %s", it->s_name);
			client_send_move(loc, tag, nrof);
		}
		else if (!strcmp(cmd, "DROP"))
		{
			int nrof = 1;

			/* Drop from inventory */
			if (cpl.inventory_win == IWIN_INV)
			{
				object *tmp;

				tag = cpl.win_inv_tag;
				loc = cpl.below->tag;

				if (cpl.win_inv_ctag == -1 && cpl.container && cpl.below)
				{
					for (tmp = cpl.below->inv; tmp; tmp = tmp->next)
					{
						if (tmp->tag == cpl.container->tag)
						{
							tag = cpl.win_inv_tag;
							loc = cpl.container->tag;
							break;
						}
					}
				}
			}
			else
			{
				draw_info(COLOR_DGOLD, "The item is already on the floor.");
				return 0;
			}

			if (tag == -1 || !object_find(tag))
			{
				return 0;
			}

			it = object_find(tag);
			nrof = it->nrof;

			if (it->flags & F_LOCKED)
			{
				draw_info(COLOR_DGOLD, "Unlock item first!");
				return 0;
			}

			if (nrof == 1)
			{
				nrof = 0;
			}
			else if (!(setting_get_int(OPT_CAT_GENERAL, OPT_COLLECT_MODE) & 2))
			{
				char buf[MAX_BUF];

				reset_keys();
				cpl.input_mode = INPUT_MODE_NUMBER;
				text_input_open(22);
				cpl.loc = loc;
				cpl.tag = tag;
				cpl.nrof = nrof;
				cpl.nummode = NUM_MODE_DROP;
				snprintf(buf, sizeof(buf), "%d", nrof);
				text_input_add_string(buf);
				strncpy(cpl.num_text, it->s_name, sizeof(cpl.num_text) - 1);
				cpl.num_text[sizeof(cpl.num_text) - 1] = '\0';
				return 0;
			}

			draw_info_format(COLOR_DGOLD, "drop %s", it->s_name);
			client_send_move(loc, tag, nrof);
		}
		else if (!strcmp(cmd, "HELP"))
		{
			show_help("main");
		}
		else if (!strcmp(cmd, "QLIST"))
		{
			cs_write_string("qlist", 5);
		}
		else if (!strcmp(cmd, "RANGE"))
		{
			if (RangeFireMode++ == FIRE_MODE_INIT - 1)
			{
				RangeFireMode = 0;
			}
		}
		else if (!strcmp(cmd, "TARGET_ENEMY"))
		{
			send_command("/target 0");
		}
		else if (!strcmp(cmd, "TARGET_FRIEND"))
		{
			send_command("/target 1");
		}
		else if (!strcmp(cmd, "TARGET_SELF"))
		{
			send_command("/target 2");
		}
		else if (!strcmp(cmd, "COMBAT"))
		{
			send_command("/combat");
		}
		else if (!strcmp(cmd, "FIRE_READY"))
		{
			if (cpl.inventory_win == IWIN_INV && cpl.win_inv_tag != -1)
			{
				ready_object(object_find(cpl.win_inv_tag));
			}
		}
		else if (!strcmp(cmd, "SPELL_LIST"))
		{
			cur_widget[SPELLS_ID]->show = 1;
			SetPriorityWidget(cur_widget[SPELLS_ID]);
		}
		else if (!strcmp(cmd, "SKILL_LIST"))
		{
			cur_widget[SKILLS_ID]->show = 1;
			SetPriorityWidget(cur_widget[SKILLS_ID]);
		}
		else if (!strcmp(cmd, "PARTY_LIST"))
		{
			send_command("/party list");
		}
		else if (!strncmp(cmd, "MCON ", 4))
		{
			keybind_process_command("?CONSOLE");
			text_input_add_string(cmd + 4);
		}
		else if (!strcmp(cmd, "UP"))
		{
			cursor_keys(0);
		}
		else if (!strcmp(cmd, "DOWN"))
		{
			cursor_keys(1);
		}
		else if (!strcmp(cmd, "LEFT"))
		{
			cursor_keys(2);
		}
		else if (!strcmp(cmd, "RIGHT"))
		{
			cursor_keys(3);
		}
		else if (!strncmp(cmd, "INVENTORY", 9))
		{
			if (!strcmp(cmd + 9, "_TOGGLE"))
			{
				if (cpl.inventory_win == IWIN_INV)
				{
					cpl.inventory_win = IWIN_BELOW;
					return 1;
				}
			}

			SetPriorityWidget(cur_widget[MAIN_INV_ID]);

			if (!setting_get_int(OPT_CAT_GENERAL, OPT_PLAYERDOLL))
			{
				SetPriorityWidget(cur_widget[PDOLL_ID]);
			}

			cpl.inventory_win = IWIN_INV;
		}
		else if (!strcmp(cmd, "RUNON"))
		{
			cpl.run_on = 1;
		}
		else if (!strcmp(cmd, "FIREON"))
		{
			cpl.fire_on = 1;
		}
		else if (!strncmp(cmd, "QUICKSLOT_", 10))
		{
			cmd += 10;

			if (!strcmp(cmd, "GROUP_PREV"))
			{
				quickslot_group--;

				if (quickslot_group < 1)
				{
					quickslot_group = MAX_QUICKSLOT_GROUPS;
				}
			}
			else if (!strcmp(cmd, "GROUP_NEXT"))
			{
				quickslot_group++;

				if (quickslot_group > MAX_QUICKSLOT_GROUPS)
				{
					quickslot_group = 1;
				}
			}
			else
			{
				quickslots_handle_key(MAX(1, MIN(8, atoi(cmd))) - 1);
			}
		}

		return 1;
	}
	else
	{
		draw_info(COLOR_DGOLD, cmd);

		if (!client_command_check(cmd))
		{
			send_command(cmd);
		}
	}

	return 0;
}
