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

/**
 * @file
 *  */

#include <include.h>

SDLKey get_action_keycode, drop_action_keycode;

_keys keys[MAX_KEYS];

static key_macro defkey_macro[] =
{
	{"?M_SOUTHWEST",    	"southwest",        KEYFUNC_MOVE,  			1, SC_NORMAL},
	{"?M_SOUTH",	      	"south",	        KEYFUNC_MOVE,  			2, SC_NORMAL},
	{"?M_SOUTHEAST",    	"southeast",        KEYFUNC_MOVE,  			3, SC_NORMAL},
	{"?M_WEST",		      	"west",		        KEYFUNC_MOVE,  			4, SC_NORMAL},
	{"?M_STAY",		      	"stay",		        KEYFUNC_MOVE,  			5, SC_NORMAL},
	{"?M_EAST",		      	"east",		        KEYFUNC_MOVE,  			6, SC_NORMAL},
	{"?M_NORTHWEST",    	"northwest",        KEYFUNC_MOVE,  			7, SC_NORMAL},
	{"?M_NORTH",	      	"north",	        KEYFUNC_MOVE,  			8, SC_NORMAL},
	{"?M_NORTHEAST",    	"northeast",        KEYFUNC_MOVE,  			9, SC_NORMAL},
	{"?M_RUN",		      	"run",		        KEYFUNC_RUN,   			0, SC_NORMAL},
	{"?M_CONSOLE",	    	"console",          KEYFUNC_CONSOLE,		0, SC_NORMAL},
	{"?M_UP",		        "up",		        KEYFUNC_CURSOR,			0, SC_NORMAL},
	{"?M_DOWN",		      	"down",				KEYFUNC_CURSOR,			1, SC_NORMAL},
	{"?M_LEFT",		      	"left",             KEYFUNC_CURSOR,			2, SC_NORMAL},
	{"?M_RIGHT",	      	"right",            KEYFUNC_CURSOR,			3, SC_NORMAL},
	{"?M_RANGE",	      	"toggle range",     KEYFUNC_RANGE,			0, SC_NORMAL},
	{"?M_APPLY",	      	"apply <tag>",      KEYFUNC_APPLY,			0, SC_NORMAL},
	{"?M_EXAMINE",	    	"examine <tag>",    KEYFUNC_EXAMINE,		0, SC_NORMAL},
	{"?M_DROP",		      	"drop <tag>",       KEYFUNC_DROP,			0, SC_NORMAL},
	{"?M_GET",		      	"get <tag>",        KEYFUNC_GET,			0, SC_NORMAL},
	{"?M_LOCK",		      	"lock <tag>",       KEYFUNC_LOCK,			0, SC_NORMAL},
	{"?M_MARK",		      	"mark<tag>",        KEYFUNC_MARK,			0, SC_NORMAL},
	{"?M_OPTION",		    "option",           KEYFUNC_OPTION,       	0, SC_NORMAL},
	{"?M_KEYBIND",	    	"key bind",         KEYFUNC_KEYBIND,      	0, SC_NORMAL},
	{"?M_SKILL_LIST",   	"skill list",       KEYFUNC_SKILL,        	0, SC_NORMAL},
	{"?M_SPELL_LIST",   	"spell list",       KEYFUNC_SPELL,        	0, SC_NORMAL},
	{"?M_PAGEUP",		    "scroll up",        KEYFUNC_PAGEUP,       	0, SC_NORMAL},
	{"?M_PAGEDOWN",	    	"scroll down",      KEYFUNC_PAGEDOWN,     	0, SC_NORMAL},
	{"?M_FIRE_READY",   	"fire_ready <tag>", KEYFUNC_FIREREADY,    	0, SC_NORMAL},
	{"?M_HELP",             "show help",        KEYFUNC_HELP,         	0, SC_NORMAL},
	{"?M_PAGEUP_TOP",	  	"scroll up",        KEYFUNC_PAGEUP_TOP,   	0, SC_NORMAL},
	{"?M_PAGEDOWN_TOP", 	"scroll down",      KEYFUNC_PAGEDOWN_TOP, 	0, SC_NORMAL},
	{"?M_TARGET_ENEMY", 	"/target enemy",    KEYFUNC_TARGET_ENEMY, 	0, SC_NORMAL},
	{"?M_TARGET_FRIEND",	"/target friend",   KEYFUNC_TARGET_FRIEND,	0, SC_NORMAL},
	{"?M_TARGET_SELF",		"/target self",     KEYFUNC_TARGET_SELF,  	0, SC_NORMAL},
	{"?M_COMBAT_TOGGLE",	"/combat",          KEYFUNC_COMBAT,       	0, SC_NORMAL},
	{"?M_QLIST",            "quest list",       KEYFUNC_QLIST,          0, SC_NORMAL},
};

#define DEFAULT_KEYMAP_MACROS (sizeof(defkey_macro) / sizeof(struct key_macro))

/* Magic console macro: when this is found at the beginning of a user defined macro, then
 * what follows this macro will be put in the input console ready to be edited */
char macro_magic_console[] = "?M_MCON";

/**
 * Initialize keys and movement queue. */
void init_keys()
{
	int i;

	for (i = 0; i < MAX_KEYS; i++)
	{
		keys[i].time = 0;
	}

	reset_keys();
}

void reset_keys()
{
	int i;

	SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);

	text_input_string_flag = 0;
	text_input_string_end_flag = 0;
	text_input_string_esc_flag = 0;

	for (i = 0; i < MAX_KEYS; i++)
		keys[i].pressed = 0;
}

/* Here we look in the user defined keymap and try to get same useful macros */
int check_menu_macros(char *text)
{
	if (!strcmp("?M_SPELL_LIST", text))
	{
		if (cpl.menustatus == MENU_KEYBIND)
			save_keybind_file(KEYBIND_FILE);

		map_udate_flag = 2;

		if (cpl.menustatus != MENU_SPELL)
			cpl.menustatus = MENU_SPELL;
		else
			cpl.menustatus = MENU_NO;

		sound_play_effect("scroll.ogg", 100);
		reset_keys();
		return 1;
	}
	else if (!strcmp("?M_SKILL_LIST", text))
	{
		if (cpl.menustatus == MENU_KEYBIND)
			save_keybind_file(KEYBIND_FILE);

		map_udate_flag = 2;

		if (cpl.menustatus != MENU_SKILL)
			cpl.menustatus = MENU_SKILL;
		else
			cpl.menustatus = MENU_NO;

		sound_play_effect("scroll.ogg", 100);
		reset_keys();
		return 1;
	}
	else if (!strcmp("?M_KEYBIND", text))
	{
		map_udate_flag = 2;

		if (cpl.menustatus != MENU_KEYBIND)
		{
			keybind_status = KEYBIND_STATUS_NO;
			cpl.menustatus = MENU_KEYBIND;
		}
		else
		{
			save_keybind_file(KEYBIND_FILE);
			cpl.menustatus = MENU_NO;
		}

		sound_play_effect("scroll.ogg", 100);
		reset_keys();
		return 1;
	}

	return 0;
}

/* Here we handle menu change direct from open menu to
 * open menu, menu close by double press the trigger key
 * and other menu handling stuff - but NOT the keys
 * inside a menu! */
int check_keys_menu_status(int key)
{
	int i, j;

	/* Groups */
	for (j = 0; j < BINDKEY_LIST_MAX; j++)
	{
		for (i = 0; i < OPTWIN_MAX_OPT; i++)
		{
			if (key == bindkey_list[j].entry[i].key)
			{
				if (check_menu_macros(bindkey_list[j].entry[i].text))
					return 1;
			}
		}
	}

	return 0;
}

static int check_macro_keys(char *text)
{
	int i;
	size_t magic_len = strlen(macro_magic_console);

	if (!strncmp(macro_magic_console, text, magic_len) && strlen(text) > magic_len)
	{
		process_macro_keys(KEYFUNC_CONSOLE, 0);
		text_input_add_string(&text[magic_len]);
		return 0;
	}

	for (i = 0; i < (int) DEFAULT_KEYMAP_MACROS; i++)
	{
		if (!strcmp(defkey_macro[i].macro, text))
		{
			if (!process_macro_keys(defkey_macro[i].key, defkey_macro[i].value))
			{
				return 0;
			}

			return 1;
		}
	}

	return 1;
}

/**
 * Process a single macro.
 * @param macro The macro to process. */
void process_macro(_keymap macro)
{
	char command[MAX_BUF], *cp;

	strncpy(command, macro.text, sizeof(command) - 1);
	command[MAX_BUF - 1] = '\0';
	cp = strtok(command, ";");

	while (cp)
	{
		while (*cp == ' ')
		{
			cp++;
		}

		if (check_macro_keys(cp))
		{
			draw_info(cp, COLOR_DGOLD);

			if (!client_command_check(cp))
			{
				send_command(cp);
			}
		}

		cp = strtok(NULL, ";");
	}
}

/**
 * Check a key for macros.
 * @param key Key to check. */
void check_keys(int key)
{
	int i, j;

	/* groups */
	for (j = 0; j < BINDKEY_LIST_MAX; j++)
	{
		for (i = 0; i < OPTWIN_MAX_OPT; i++)
		{
			if (key == bindkey_list[j].entry[i].key)
			{
				process_macro(bindkey_list[j].entry[i]);
				return;
			}
		}
	}
}

int process_macro_keys(int id, int value)
{
	int nrof, tag = 0, loc = 0;
	char buf[MAX_BUF];
	object *it, *tmp;
	widgetdata *widget;

	switch (id)
	{
		case KEYFUNC_FIREREADY:
			if (cpl.inventory_win == IWIN_INV && cpl.win_inv_tag != -1)
			{
				ready_object(object_find(cpl.win_inv_tag));
			}

			break;

		case KEYFUNC_PAGEUP:
			widget = cur_widget[CHATWIN_ID];
			TEXTWIN(widget)->scroll -= TEXTWIN_ROWS_VISIBLE(widget);
			textwin_scroll_adjust(widget);
			WIDGET_REDRAW(widget);
			break;

		case KEYFUNC_PAGEDOWN:
			widget = cur_widget[CHATWIN_ID];
			TEXTWIN(widget)->scroll += TEXTWIN_ROWS_VISIBLE(widget);
			textwin_scroll_adjust(widget);
			WIDGET_REDRAW(widget);
			break;

		case KEYFUNC_PAGEUP_TOP:
			widget = cur_widget[MSGWIN_ID];
			TEXTWIN(widget)->scroll -= TEXTWIN_ROWS_VISIBLE(widget);
			textwin_scroll_adjust(widget);
			WIDGET_REDRAW(widget);
			break;

		case KEYFUNC_PAGEDOWN_TOP:
			widget = cur_widget[MSGWIN_ID];
			TEXTWIN(widget)->scroll += TEXTWIN_ROWS_VISIBLE(widget);
			textwin_scroll_adjust(widget);
			WIDGET_REDRAW(widget);
			break;

		case KEYFUNC_TARGET_ENEMY:
			send_command("/target 0");
			break;

		case KEYFUNC_TARGET_FRIEND:
			send_command("/target 1");
			break;

		case KEYFUNC_TARGET_SELF:
			send_command("/target 2");
			break;

		case KEYFUNC_COMBAT:
			send_command("/combat");
			break;

		case KEYFUNC_SPELL:
			map_udate_flag = 2;
			sound_play_effect("scroll.ogg", 100);

			if (cpl.menustatus == MENU_KEYBIND)
				save_keybind_file(KEYBIND_FILE);

			if (cpl.menustatus != MENU_SPELL)
				cpl.menustatus = MENU_SPELL;
			else
				cpl.menustatus = MENU_NO;

			reset_keys();
			break;

		case KEYFUNC_SKILL:
			map_udate_flag = 2;

			if (cpl.menustatus == MENU_KEYBIND)
				save_keybind_file(KEYBIND_FILE);

			sound_play_effect("scroll.ogg", 100);

			if (cpl.menustatus != MENU_SKILL)
				cpl.menustatus = MENU_SKILL;
			else
				cpl.menustatus = MENU_NO;

			reset_keys();
			break;

		case KEYFUNC_KEYBIND:
			map_udate_flag = 2;

			if (cpl.menustatus != MENU_KEYBIND)
			{
				keybind_status = KEYBIND_STATUS_NO;
				cpl.menustatus = MENU_KEYBIND;
			}
			else
			{
				save_keybind_file(KEYBIND_FILE);
				cpl.menustatus = MENU_NO;
			}

			sound_play_effect("scroll.ogg", 100);
			reset_keys();
			break;

		case KEYFUNC_CONSOLE:
			sound_play_effect("console.ogg", 100);
			reset_keys();

			if (cpl.input_mode == INPUT_MODE_NO)
			{
				cpl.input_mode = INPUT_MODE_CONSOLE;
				text_input_open(253);
			}
			else if (cpl.input_mode == INPUT_MODE_CONSOLE)
				cpl.input_mode = INPUT_MODE_NO;

			map_udate_flag = 2;
			break;

		case KEYFUNC_RUN:
			cpl.runkey_on = !cpl.runkey_on;

			if (!cpl.runkey_on)
			{
				send_command("/run_stop");
			}

			snprintf(buf, sizeof(buf), "runmode %s", cpl.runkey_on ? "on" : "off");
#if 0
			draw_info(buf, COLOR_DGOLD);
#endif
			break;

		case KEYFUNC_MOVE:
			move_keys(value);
			break;

		case KEYFUNC_CURSOR:
			cursor_keys(value);
			break;

		case KEYFUNC_RANGE:
			if (RangeFireMode++ == FIRE_MODE_INIT - 1)
				RangeFireMode = 0;

			map_udate_flag = 2;
			return 0;

		case KEYFUNC_APPLY:
			if (cpl.inventory_win == IWIN_BELOW)
				tag = cpl.win_below_tag;
			else
				tag = cpl.win_inv_tag;

			if (tag == -1 || !object_find(tag))
				return 0;

			snprintf(buf, sizeof(buf), "apply %s", object_find(tag)->s_name);
			draw_info(buf, COLOR_DGOLD);
			client_send_apply(tag);
			return 0;

		case KEYFUNC_EXAMINE:
			if (cpl.inventory_win == IWIN_BELOW)
				tag = cpl.win_below_tag;
			else
				tag = cpl.win_inv_tag;

			if (tag == -1 || !object_find(tag))
				return 0;

			client_send_examine(tag);
			snprintf(buf, sizeof(buf), "examine %s", object_find(tag)->s_name);
			draw_info(buf, COLOR_DGOLD);
			return 0;

		case KEYFUNC_MARK:
			if (cpl.inventory_win == IWIN_BELOW)
				tag = cpl.win_below_tag;
			else
				tag = cpl.win_inv_tag;

			if (tag == -1 || !object_find(tag))
				return 0;

			it = object_find(tag);
			draw_info_format(COLOR_DGOLD, "%smark %s", it->tag == cpl.mark_count ? "un" : "", it->s_name);
			object_send_mark(it);
			return 0;

		case KEYFUNC_LOCK:
			if (cpl.inventory_win == IWIN_BELOW)
				tag = cpl.win_below_tag;
			else
				tag = cpl.win_inv_tag;

			if (tag == -1  || !object_find(tag))
				return 0;

			toggle_locked((it = object_find(tag)));

			if (!it)
				return 0;

			if (it->flags & F_LOCKED)
				snprintf(buf, sizeof(buf), "unlock %s", it->s_name);
			else
				snprintf(buf, sizeof(buf), "lock %s", it->s_name);

			draw_info(buf, COLOR_DGOLD);
			return 0;

		case KEYFUNC_GET:
			/* Number of items */
			nrof = 1;

			/* From below to inv */
			if (cpl.inventory_win == IWIN_BELOW)
			{
				tag = cpl.win_below_tag;

				if (cpl.container)
				{
					if (cpl.container->tag != cpl.win_below_ctag)
						loc = cpl.container->tag;
					else
						loc = cpl.ob->tag;
				}
				else
					loc = cpl.ob->tag;
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
					/* From inventory to container - if the container is in inv */
					else
					{
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
								draw_info("You already have it.", COLOR_DGOLD);
						}
					}
				}
				else
				{
					draw_info("You have no open container to put it in.", COLOR_DGOLD);
#if 0
					tag = cpl.win_inv_tag;
					loc = cpl.ob->tag;
#endif
					tag = -1;
				}
			}

			if (tag == -1 || !object_find(tag))
				return 0;

			if ((it = object_find(tag)))
				nrof = it->nrof;
			else
				return 0;

			if (nrof == 1)
			{
				nrof = 0;
			}
			else
			{
				if (!(options.collect_mode & 1))
				{
					reset_keys();
					cpl.input_mode = INPUT_MODE_NUMBER;
					text_input_open(22);
					cpl.loc = loc;
					cpl.tag = tag;
					cpl.nrof = nrof;
					cpl.nummode = NUM_MODE_GET;
					snprintf(buf, sizeof(buf), "%d", nrof);
					text_input_add_string(buf);
					strncpy(cpl.num_text,it->s_name, 250);
					cpl.num_text[250] = 0;
					return 0;
				}
			}

			sound_play_effect("get.ogg", 100);
			snprintf(buf, sizeof(buf), "get %s", it->s_name);
			draw_info(buf, COLOR_DGOLD);
			client_send_move(loc, tag, nrof);
			return 0;

		case KEYFUNC_HELP:
			show_help("main");
			break;

		case KEYFUNC_DROP:
			nrof = 1;

			/* Drop from inventory */
			if (cpl.inventory_win == IWIN_INV)
			{
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
				snprintf(buf, sizeof(buf), "The item is already on the floor.");
				draw_info(buf, COLOR_DGOLD);
				return 0;
			}

			if (tag == -1 || !object_find(tag))
				return 0;

			if ((it = object_find(tag)))
				nrof = it->nrof;
			else
				return 0;

			if (it->flags & F_LOCKED)
			{
				sound_play_effect("click_fail.ogg", 100);
				draw_info("Unlock item first!", COLOR_DGOLD);
				return 0;
			}

			if (nrof == 1)
			{
				nrof = 0;
			}
			else
			{
				if (!(options.collect_mode & 2))
				{
					reset_keys();
					cpl.input_mode = INPUT_MODE_NUMBER;
					text_input_open(22);
					cpl.loc = loc;
					cpl.tag = tag;
					cpl.nrof = nrof;
					cpl.nummode = NUM_MODE_DROP;
					snprintf(buf, sizeof(buf), "%d", nrof);
					text_input_add_string(buf);
					strncpy(cpl.num_text, it->s_name, 250);
					cpl.num_text[250] = 0;
					return 0;
				}
			}

			sound_play_effect("drop.ogg", 100);
			snprintf(buf, sizeof(buf), "drop %s", it->s_name);
			draw_info(buf, COLOR_DGOLD);
			client_send_move(loc, tag, nrof);
			return 0;

		case KEYFUNC_QLIST:
			cs_write_string("qlist", 5);
			break;

		default:
			return 1;
	}

	return 0;
}

/* Import the key-binding file. */
void read_keybind_file(char *fname)
{
	FILE *stream;
	char line[255];
	int i, pos;

	if ((stream = fopen_wrapper(fname, "r")))
	{
		bindkey_list_set.group_nr = -1;
		bindkey_list_set.entry_nr = 0;

		while (fgets(line, 255, stream))
		{
			/* Skip empty or incomplete lines */
			if (strlen(line) < 4)
				continue;

			i = 1;
			/* Found key group */
			if (line[0] == '+')
			{
				if (++bindkey_list_set.group_nr == BINDKEY_LIST_MAX)
					break;

				while (line[++i] && line[i] != '"' && i < OPTWIN_MAX_TABLEN)
					bindkey_list[bindkey_list_set.group_nr].name[i - 2] = line[i];

				bindkey_list[bindkey_list_set.group_nr].name[++i] = 0;
				bindkey_list_set.entry_nr = 0;
				continue;
			}

			/* Something is wrong with the file */
			if (bindkey_list_set.group_nr < 0)
				break;

			/* Found a key entry */
			sscanf(line, " %d %d", &bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].key, &bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].repeatflag);
			pos = 0;

			/* start of 1. string */
			while (line[++i] && line[i] != '"');

			while (line[++i] && line[i] != '"')
				bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].keyname[pos++] = line[i];

			bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].keyname[pos] = 0;
			pos = 0;

			/* start of 2. string */
			while (line[++i] && line[i] != '"');

			while (line[++i] && line[i] != '"')
				bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text[pos++] = line[i];

			bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text[pos] = 0;

			if (!strcmp(bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text, "?M_GET"))
				get_action_keycode = bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].key;

			if (!strcmp(bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text, "?M_DROP"))
				drop_action_keycode = bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].key;

			if (++bindkey_list_set.entry_nr == OPTWIN_MAX_OPT)
				break;
		}

		fclose(stream);
	}

	if (bindkey_list_set.group_nr <= 0)
	{
		sprintf(bindkey_list[0].entry[0].keyname, "File %s is corrupt!", fname);
		strcpy(bindkey_list[0].entry[0].text, "ERROR!");
		LOG(llevBug, "key-binding file %s is corrupt.\n", fname);
	}

	bindkey_list_set.group_nr = 0;
	bindkey_list_set.entry_nr = 0;
}

/* Export the key-binding file. */
void save_keybind_file(char *fname)
{
	FILE *stream;
	int entry, group;
	char buf[256];

	if (!(stream = fopen_wrapper(fname, "w+")))
		return;

	for (group=0; group< BINDKEY_LIST_MAX; group++)
	{
		/* This group is empty, what about the next one? */
		if (!bindkey_list[group].name[0])
			continue;

		if (group)
			fputs("\n", stream);

		sprintf(buf, "+\"%s\"\n", bindkey_list[group].name);
		fputs(buf, stream);

		for (entry = 0; entry < OPTWIN_MAX_OPT; entry++)
		{
			/* This entry is empty, what about the next one? */
			if (!bindkey_list[group].entry[entry].text[0])
				continue;

			/* We need to know for INPUT_MODE_NUMBER "quick get" this key */
			if (!strcmp(bindkey_list[group].entry[entry].text, "?M_GET"))
				get_action_keycode = bindkey_list[group].entry[entry].key;

			if (!strcmp(bindkey_list[group].entry[entry].text, "?M_DROP"))
				drop_action_keycode = bindkey_list[group].entry[entry].key;

			/* Save key entry */
			sprintf(buf, "%.3d %d \"%s\" \"%s\"\n", bindkey_list[group].entry[entry].key, bindkey_list[group].entry[entry].repeatflag, bindkey_list[group].entry[entry].keyname, bindkey_list[group].entry[entry].text);
			fputs(buf, stream);
		}
	}

	fclose(stream);
}
