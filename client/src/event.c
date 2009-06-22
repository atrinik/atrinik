/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 3 of the License, or     *
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

#include "include.h"

extern char d_ServerName[2048];
extern int  d_ServerPort;
static int get_action_keycode,drop_action_keycode; /* thats the key for G'et command from keybind */
static int menuRepeatKey =-1;

typedef struct _keys
{
	Boolean pressed; /*true: key is pressed*/
	uint32 time; /*tick time last repeat is initiated*/
} _keys;
static _keys keys[MAX_KEYS];

_key_macro defkey_macro[] =
{
	{"?M_SOUTHWEST",    	"southwest",        KEYFUNC_MOVE,  			1, SC_NORMAL, MENU_NO},
	{"?M_SOUTH",	      	"south",	        KEYFUNC_MOVE,  			2, SC_NORMAL, MENU_NO},
	{"?M_SOUTHEAST",    	"southeast",        KEYFUNC_MOVE,  			3, SC_NORMAL, MENU_NO},
	{"?M_WEST",		      	"west",		        KEYFUNC_MOVE,  			4, SC_NORMAL, MENU_NO},
	{"?M_STAY",		      	"stay",		        KEYFUNC_MOVE,  			5, SC_NORMAL, MENU_NO},
	{"?M_EAST",		      	"east",		        KEYFUNC_MOVE,  			6, SC_NORMAL, MENU_NO},
	{"?M_NORTHWEST",    	"northwest",        KEYFUNC_MOVE,  			7, SC_NORMAL, MENU_NO},
	{"?M_NORTH",	      	"north",	        KEYFUNC_MOVE,  			8, SC_NORMAL, MENU_NO},
	{"?M_NORTHEAST",    	"northeast",        KEYFUNC_MOVE,  			9, SC_NORMAL, MENU_NO},
	{"?M_RUN",		      	"run",		        KEYFUNC_RUN,   			0, SC_NORMAL, MENU_NO},
	{"?M_CONSOLE",	    	"console",          KEYFUNC_CONSOLE,		0, SC_NORMAL, MENU_NO},
	{"?M_UP",		        "up",		        KEYFUNC_CURSOR,			0, SC_NORMAL, MENU_NO},
	{"?M_DOWN",		      	"down",				KEYFUNC_CURSOR,			1, SC_NORMAL, MENU_NO},
	{"?M_LEFT",		      	"left",             KEYFUNC_CURSOR,			2, SC_NORMAL, MENU_NO},
	{"?M_RIGHT",	      	"right",            KEYFUNC_CURSOR,			3, SC_NORMAL, MENU_NO},
	{"?M_RANGE",	      	"toggle range",     KEYFUNC_RANGE,			0, SC_NORMAL, MENU_NO},
	{"?M_APPLY",	      	"apply <tag>",      KEYFUNC_APPLY,			0, SC_NORMAL, MENU_NO},
	{"?M_EXAMINE",	    	"examine <tag>",    KEYFUNC_EXAMINE,		0, SC_NORMAL, MENU_NO},
	{"?M_DROP",		      	"drop <tag>",       KEYFUNC_DROP,			0, SC_NORMAL, MENU_NO},
	{"?M_GET",		      	"get <tag>",        KEYFUNC_GET,			0, SC_NORMAL, MENU_NO},
	{"?M_LOCK",		      	"lock <tag>",       KEYFUNC_LOCK,			0, SC_NORMAL, MENU_NO},
	{"?M_MARK",		      	"mark<tag>",        KEYFUNC_MARK,			0, SC_NORMAL, MENU_NO},
	{"?M_STATUS",		    "status",           KEYFUNC_STATUS,       	0, SC_NORMAL, MENU_ALL},
	{"?M_OPTION",		    "option",           KEYFUNC_OPTION,       	0, SC_NORMAL, MENU_ALL},
	{"?M_KEYBIND",	    	"key bind",         KEYFUNC_KEYBIND,      	0, SC_NORMAL, MENU_ALL},
	{"?M_SKILL_LIST",   	"skill list",       KEYFUNC_SKILL,        	0, SC_NORMAL, MENU_ALL},
	{"?M_SPELL_LIST",   	"spell list",       KEYFUNC_SPELL,        	0, SC_NORMAL, MENU_ALL},
	{"?M_PAGEUP",		    "scroll up",        KEYFUNC_PAGEUP,       	0, SC_NORMAL, MENU_NO},
	{"?M_PAGEDOWN",	    	"scroll down",      KEYFUNC_PAGEDOWN,     	0, SC_NORMAL, MENU_NO},
	{"?M_FIRE_READY",   	"fire_ready <tag>", KEYFUNC_FIREREADY,    	0, SC_NORMAL, MENU_NO},
	{"?M_LAYER0",		    "l0",               KEYFUNC_LAYER0,       	0, SC_NORMAL, MENU_NO},
	{"?M_LAYER1",		    "l1",               KEYFUNC_LAYER1,       	0, SC_NORMAL, MENU_NO},
	{"?M_LAYER2",		    "l2",               KEYFUNC_LAYER2,       	0, SC_NORMAL, MENU_NO},
	{"?M_LAYER3",		    "l3",               KEYFUNC_LAYER3,       	0, SC_NORMAL, MENU_NO},
	{"?M_HELP",             "show help",        KEYFUNC_HELP,         	0, SC_NORMAL, MENU_NO},
	{"?M_PAGEUP_TOP",	  	"scroll up",        KEYFUNC_PAGEUP_TOP,   	0, SC_NORMAL, MENU_NO},
	{"?M_PAGEDOWN_TOP", 	"scroll down",      KEYFUNC_PAGEDOWN_TOP, 	0, SC_NORMAL, MENU_NO},
	{"?M_TARGET_ENEMY", 	"/target enemy",    KEYFUNC_TARGET_ENEMY, 	0, SC_NORMAL, MENU_NO},
	{"?M_TARGET_FRIEND",	"/target friend",   KEYFUNC_TARGET_FRIEND,	0, SC_NORMAL, MENU_NO},
	{"?M_TARGET_SELF",		"/target self",     KEYFUNC_TARGET_SELF,  	0, SC_NORMAL, MENU_NO},
	{"?M_COMBAT_TOGGLE",	"/combat",          KEYFUNC_COMBAT,       	0, SC_NORMAL, MENU_NO},
};

#define DEFAULT_KEYMAP_MACROS (sizeof(defkey_macro)/sizeof(struct _key_macro))

/* Magic console macro: when this is found at the beginning of a user defined macro, then
 * what follows this macro will be put in the input console ready to be edited */
char macro_magic_console[] = "?M_MCON";

/* for debug/alpha , remove later */
int KeyScanFlag;
int cursor_type = 0;

#define KEY_REPEAT_TIME 35
#define KEY_REPEAT_TIME_INIT 175

static Uint32 menuRepeatTicks = 0, menuRepeatTime = KEY_REPEAT_TIME_INIT;

/* cmds for fire/move/run - used from move_keys()*/
static char *directions[10] = {
	"null", 	"/sw", 	"/s", 	"/se", 	"/w",
	"/stay", 	"/e", 	"/nw", 	"/n", 	"/ne"
};

static char *directions_name[10] = {
	"null",	"southwest", 	"south", 		"southeast",	"west",
	"stay", "east",			"northwest", 	"north", 		"northeast"
};

static char *directionsrun[10] = {
	"/run 0",	"/run 6",	"/run 5",	"/run 4",	"/run 7",
	"/run 9", 	"/run 3", 	"/run 8", 	"/run 1",	"/run 2"
};

static char *directionsfire[10] = {
	"fire 0",	"fire 6",	"fire 5",	"fire 4",	"fire 7",
	"fire 0",	"fire 3", 	"fire 8", 	"fire 1",	"fire 2"
};

static int key_event(SDL_KeyboardEvent *key);
static void key_string_event(SDL_KeyboardEvent *key);
static void check_keys(int key);
static Boolean check_macro_keys(char *text);
static void move_keys(int num);
static void key_repeat(void);
static void cursor_keys(int num);
int key_meta_menu(SDL_KeyboardEvent *key);
void key_connection_event(SDL_KeyboardEvent *key);
void check_menu_keys(int menu, int key);
static Boolean check_menu_macros(char *text);
static void quickslot_key(SDL_KeyboardEvent *key, int slot);

void init_keys(void)
{
	register int i;

	for (i = 0; i < MAX_KEYS; i++)
		keys[i].time = 0;

	reset_keys();
}

void reset_keys(void)
{
	register int i;

	InputStringFlag = 0;
	InputStringEndFlag = 0;
	InputStringEscFlag = 0;

	for (i = 0; i < MAX_KEYS; i++)
		keys[i].pressed = 0;
}

/* x: mouse x-pos ; y: mouse y-pos
 * ret:
 * 	0  if mousepointer is in the game-field.
 * 	-1 if mousepointer is in a menu-field. */
int mouseInPlayfield(x, y)
{
	x += 45;
	y -= 127;

	if (x < 445)
	{
		/* upper left */
		if ((y <  200) && (y + y + x > 400))
			return -1;

		/* lower left */
		if ((y >= 200) && (y + y - x < 400))
			return -1;
	}
	else
	{
  		x -= 445;

		/* upper right */
		if ((y <  200) && (y + y > x))
			return -1;

		/* lower right */
		if ((y >= 200) && (y + y + x < 845))
			return -1;
	}

	return 0;
}

/* src:  (if != DRAG_GET_STATUS) set actual dragging source.
 * item: (if != NULL) set actual dragging item.
 * ret:  the actual dragging source. */
int draggingInvItem(int src)
{
	static int drag_src = DRAG_NONE;

	if (src != DRAG_GET_STATUS)
		drag_src = src;

	return drag_src;
}

/* Wait for user to input a number. */
static void mouse_InputNumber()
{
	static int delta = 0;
	static int timeVal = 1;
	int x, y;

	if (!(SDL_GetMouseState(&x, &y) & SDL_BUTTON(SDL_BUTTON_LEFT)))
	{
		timeVal = 1;
		delta = 0;
		return;
	}

	if (x < 330 || x > 337 || y < 510 || delta++ & 15)
		return;

	/* Plus */
	if (y > 518)
	{
		x = atoi(InputString) + timeVal;

		if (x > cpl.nrof)
			x = cpl.nrof;
	}
	/* Minus */
	else
	{
		x = atoi(InputString)- timeVal;

		if (x < 1)
			x = 1;
	}

	snprintf(InputString, sizeof(InputString), "%d", x);
	InputCount = strlen(InputString);
	timeVal += (timeVal / 8) + 1;
}

/* Move our hero with a mouse. */
static void mouse_moveHero()
{
#define MY_POS 8
	int x,y, tx, ty;
	static int delta = 0;

	/* Don't move too fast */
	if (delta++ & 7)
		return;

	/* Still dragging an item */
	if (draggingInvItem(DRAG_GET_STATUS))
		return;

	if (cpl.input_mode == INPUT_MODE_NUMBER)
		return;

	if (cpl.menustatus != MENU_NO)
		return;

	/* textwin events */
	if (textwin_flags & (TW_RESIZE + TW_SCROLL))
		return;

	if (!(SDL_GetMouseState(&x, &y) & SDL_BUTTON(SDL_BUTTON_LEFT)))
	{
		delta = 0;
		return;
	}

	/* textwin has high priority, so don't move if playfield is overlapping */
 	if ((options.use_TextwinSplit) && x > 538 &&  y > 560 - (txtwin[TW_MSG].size + txtwin[TW_CHAT].size) * 10)
		return;

	if (get_tile_position(x, y, &tx, &ty))
		return;

	if (tx == MY_POS)
	{
		if (ty == MY_POS)
			process_macro_keys(KEYFUNC_MOVE, 5);
		else if (ty > MY_POS)
			process_macro_keys(KEYFUNC_MOVE, 2);
		else if (ty < MY_POS)
			process_macro_keys(KEYFUNC_MOVE, 8);
	}
	else if (tx <  MY_POS)
	{
		if (ty == MY_POS)
			process_macro_keys(KEYFUNC_MOVE, 4);
		else if (ty >  MY_POS)
			process_macro_keys(KEYFUNC_MOVE, 1);
		else if (ty <  MY_POS)
			process_macro_keys(KEYFUNC_MOVE, 7);
	}
	/* (x > MY_POS) */
	else
	{
		if (ty == MY_POS)
			process_macro_keys(KEYFUNC_MOVE, 6);

		if (ty <  MY_POS)
			process_macro_keys(KEYFUNC_MOVE, 9);
		if (ty >  MY_POS)
			process_macro_keys(KEYFUNC_MOVE, 3);
	}

#undef MY_POS
}

int Event_PollInputDevice(void)
{
	SDL_Event event;
	int x, y, done = 0;
	static int active_scrollbar = 0;
	/* only print text once per dnd */
	static int itemExamined  = 0;
	static Uint32 Ticks= 0;

	if ((SDL_GetTicks() - Ticks > 10) || !Ticks)
	{
		Ticks = SDL_GetTicks();
		if (GameStatus >= GAME_STATUS_PLAY)
		{
			if (InputStringFlag && cpl.input_mode == INPUT_MODE_NUMBER)
				mouse_InputNumber();
			else if (!active_scrollbar && !cursor_type)
				mouse_moveHero();
		}
	}

	while (SDL_PollEvent(&event))
	{
		static int old_mouse_y = 0;
		x = event.motion.x;
		y = event.motion.y;

		switch (event.type)
		{
			case SDL_MOUSEBUTTONUP:
				if (GameStatus < GAME_STATUS_PLAY)
					break;

				if (InputStringFlag && cpl.input_mode == INPUT_MODE_NUMBER)
					break;

				mb_clicked = 0;
				cursor_type = 0;
				active_scrollbar = 0;

				/* Drag and drop events */
				if (draggingInvItem(DRAG_GET_STATUS) > DRAG_IWIN_BELOW)
				{
					/* KEYFUNC_APPLY and KEYFUNC_DROP works only if cpl.inventory_win = IWIN_INV. The tag must
					 * be placed in cpl.win_inv_tag. So we do this and after DnD we restore the old values. */
					int old_inv_win = cpl.inventory_win;
     				int old_inv_tag = cpl.win_inv_tag;
					cpl.inventory_win = IWIN_INV;

					if (draggingInvItem(DRAG_GET_STATUS) == DRAG_PDOLL)
					{
						cpl.win_inv_tag = cpl.win_pdoll_tag;

						/* Drop to inventory */
						if (x < 223 && y > 450)
							process_macro_keys(KEYFUNC_APPLY, 0);
					}

					if (draggingInvItem(DRAG_GET_STATUS) == DRAG_QUICKSLOT)
					{
						cpl.win_inv_tag = cpl.win_quick_tag;

						/* Drop to player doll */
     					if (x < 223 && y < 300 && !(locate_item(cpl.win_inv_tag))->applied)
							process_macro_keys(KEYFUNC_APPLY, 0);
					}

					/* range field */
					if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_INV && x < 90 && y > 400 && y <440)
					{
						RangeFireMode = 4;
						/* Drop to player doll */
						process_macro_keys(KEYFUNC_FIREREADY, 0);
					}

					if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_INV && x < 223 && y < 300)
					{
						if ((locate_item(cpl.win_inv_tag))->applied)
							draw_info("This is applied already!", COLOR_WHITE);
						/* Drop to player doll */
						else
							process_macro_keys(KEYFUNC_APPLY, 0);

                    }

					/* Drop to quickslots */
					if (x >= SKIN_POS_QUICKSLOT_X && x < SKIN_POS_QUICKSLOT_X + 282 && y >= SKIN_POS_QUICKSLOT_Y && y < SKIN_POS_QUICKSLOT_Y + 42)
					{
						int ind = get_quickslot(x, y);

						/* valid slot */
						if (ind != -1)
						{
							if (draggingInvItem(DRAG_GET_STATUS) == DRAG_QUICKSLOT_SPELL)
							{
								quick_slots[ind].spell = 1;
								quick_slots[ind].groupNr = quick_slots[cpl.win_quick_tag].groupNr;
								quick_slots[ind].classNr = quick_slots[cpl.win_quick_tag].classNr;
								quick_slots[ind].tag = quick_slots[cpl.win_quick_tag].spellNr;
								cpl.win_quick_tag = -1;
  					        }
  					        else
  					        {
								if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_INV || draggingInvItem(DRAG_GET_STATUS) == DRAG_PDOLL)
									cpl.win_quick_tag = cpl.win_inv_tag;

								quick_slots[ind].tag = cpl.win_quick_tag;
								quick_slots[ind].invSlot = ind;
								quick_slots[ind].spell = 0;

								/* Now we do some tests... First, ensure this item can fit */
								update_quickslots(-1);

								/* Now: if this is null, item is *not* in the main inventory
							 	 * of the player - then we can't put it in quickbar!
							 	 * Server will not allow apply of items in containers! */
								if (!locate_item_from_inv(cpl.ob->inv, cpl.win_quick_tag))
								{
									sound_play_effect(SOUND_CLICKFAIL, 0, 0, 100);
									draw_info("Only items from main inventory are allowed in quickslot!", COLOR_WHITE);
								}
								else
								{
    								char buf[256];

									/* We 'get' it in quickslots */
									sound_play_effect(SOUND_GET, 0, 0, 100);
                                	snprintf(buf, sizeof(buf), "Set F%d to %s", ind + 1, locate_item(cpl.win_quick_tag)->s_name);
                                	draw_info(buf, COLOR_DGOLD);
								}
							}
						}
					}

					/* Drop to ground */
					if (mouseInPlayfield(x, y) || (y > 565 && x >265 && x < 529))
					{
						if (draggingInvItem(DRAG_GET_STATUS) != DRAG_QUICKSLOT_SPELL)
							process_macro_keys(KEYFUNC_DROP, 0);
					}

					cpl.inventory_win = old_inv_win;
					cpl.win_inv_tag = old_inv_tag;
				}

    			else if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_BELOW)
				{
					if (!mouseInPlayfield(x, y) && y < 550)
					{
			            sound_play_effect(SOUND_GET, 0, 0, 100);
						process_macro_keys(KEYFUNC_GET, 0);
					}
				}

				draggingInvItem(DRAG_NONE);

				/* Ready for next item */
				itemExamined  = 0;
				break;

			case SDL_MOUSEMOTION:
				mb_clicked = 0;

				if (GameStatus < GAME_STATUS_PLAY)
					break;

				/* textWindow: slider/resize event */
				textwin_event(TW_CHECK_MOVE, &event);

				/* Scrollbar sliders */
				if (event.button.button == SDL_BUTTON_LEFT && !draggingInvItem(DRAG_GET_STATUS))
				{
					/* IWIN_INV Slider */
					if (active_scrollbar == 1 || (cpl.inventory_win == IWIN_INV && y > 506 && y < 583 && x > 230 && x < 238))
					{
						active_scrollbar = 1;

						if (old_mouse_y - y > 0)
      						cpl.win_inv_slot-= INVITEMXLEN;
						else if (old_mouse_y - y < 0)
      						cpl.win_inv_slot += INVITEMXLEN;
						if  (cpl.win_inv_slot > cpl.win_inv_count)
      						cpl.win_inv_slot = cpl.win_inv_count;

    					break;
        			}
				}

#if 0
				/* Examine an item */
				if ((cpl.inventory_win == IWIN_INV) && y > 85 && y < 120 && x < 140)
				{
					if (!itemExamined)
					{
						check_keys(SDLK_e);
						itemExamined = 1;
			      	}
					break;
    			}
#endif

				break;

			case SDL_MOUSEBUTTONDOWN:
				mb_clicked = 1;

				if (GameStatus < GAME_STATUS_PLAY)
					break;

				textwin_event(TW_CHECK_BUT_DOWN, &event);

				/* Close number input */
				if (InputStringFlag && cpl.input_mode == INPUT_MODE_NUMBER)
				{
					if (x > 339 && x < 349 && y > 510 && y < 522)
					{
						SDL_EnableKeyRepeat(0 , SDL_DEFAULT_REPEAT_INTERVAL);
						InputStringFlag = 0;
						InputStringEndFlag = 1;
					}
					break;
				}

				/* Toggle range */
				if (x > 3 && x < 37 && y > 403 && y < 437)
				{
      				process_macro_keys(KEYFUNC_RANGE, 0);
      				break;
      			}

				/* Show menu buttons */
				if (x >= 748 && x <= 790)
				{
					/* Spell list */
					if (y >= 1 && y <= 24)
						check_menu_macros("?M_SPELL_LIST");
					/* Skill list */
					else if (y >= 26 && y <= 49)
						check_menu_macros("?M_SKILL_LIST");
					/* Party GUI */
					else if (y >= 51 && y <= 74)
					{
						char buf[HUGE_BUF];
						sprintf(buf, "pt list");
    					cs_write_string(csocket.fd, buf, strlen(buf));
					}
					/* Help system */
					else if (y >= 76 && y <= 99)
						show_help("main");
				}

				if (cpl.menustatus == MENU_PARTY)
				{
					if (event.button.button == 4 || event.button.button == 5 || event.button.button == SDL_BUTTON_LEFT)
					{
						gui_party_interface_mouse(&event);
						break;
					}
				}

				if (cpl.menustatus == MENU_BOOK && gui_interface_book && event.button.button == SDL_BUTTON_LEFT)
				{
					int l_len = 0, w_len, i, ii, yoff;
					_gui_book_page *page1, *page2;
					char *p, *p2, line[MAX_BUF], *helpBuf = NULL, word[MAX_BUF];

					/* get the 2 pages we show */
					page1 = gui_interface_book->start;
					for (i = 0; i != gui_interface_book->page_show && page1; i++, page1 = page1->next);
					page2 = page1->next;

					if (page1)
					{
						for (yoff = 0, i = 0, ii = 0; ii < BOOK_PAGE_LINES; ii++, yoff += 16)
						{
							if (!page1->line[i])
								break;

							if (page1->line[i]->line)
							{
								l_len = 0;

								sprintf(line, "%s", page1->line[i]->line);

								p = strtok(line, " ");
								while (p)
								{
									w_len = StringWidth(&MediumFont, p) + 1;

									if (strstr(p, "^") && y < global_book_data.y + yoff + 85 && y > global_book_data.y + yoff + 75 && x > global_book_data.x + 50 + l_len && x < global_book_data.x + 50 + l_len + w_len + 1)
									{
										helpBuf = p;
										break;
									}

									l_len += w_len;

									p = strtok(NULL, " ");
								}

								if (helpBuf)
								{
									sprintf(word, "%s", helpBuf);
									p2 = strtok(word, "^");
									book_clear();
									show_help(p2);
									break;
								}
							}

							i++;
						}
					}

					if (!helpBuf)
					{
						if (page2)
						{
							for (yoff = 0, i = 0, ii = 0; ii < BOOK_PAGE_LINES; ii++, yoff += 16)
							{
								if (!page2->line[i])
									break;

								if (page2->line[i]->line)
								{
									l_len = 0;

									sprintf(line, "%s", page2->line[i]->line);

									p = strtok(line, " ");
									while (p)
									{
										w_len = StringWidth(&MediumFont, p) + 1;

										if (strstr(p, "^") && y < global_book_data.y + yoff + 85 && y > global_book_data.y + yoff + 75 && x > global_book_data.x + 280 + l_len && x < global_book_data.x + 280 + l_len + w_len + 1)
										{
											helpBuf = p;
											break;
										}

										l_len += w_len;

										p = strtok(NULL, " ");
									}

									if (helpBuf)
									{
										sprintf(word, "%s", helpBuf);
										p2 = strtok(word, "^");
										book_clear();
										show_help(p2);
										break;
									}
								}

								i++;
							}
						}
					}
				}

				/* Toggle textwin */
				if (x >= 488 && x < 528 && y < 536 && y > 521)
				{
					if (options.use_TextwinSplit)
     					options.use_TextwinSplit = 0;
					else
     					options.use_TextwinSplit = 1;

					sound_play_effect(SOUND_SCROLL, 0, 0, 100);
					break;
				}

				/* Prayer button */
				if (x > 85 && x < 115 && y < 435 && y > 410)
				{
				   if (!client_command_check("/pray"))
                      send_command("/pray", -1, SC_NORMAL);

                   break;
                }

				/* Beyond here only when no menu is active. */
				if (cpl.menustatus != MENU_NO)
					break;

				/* Mouse in play field */
				if (mouseInPlayfield(event.motion.x, event.motion.y))
				{
					/* Targetting */
					if ((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)))
					{
						int tx, ty;
						char tbuf[32];

						cpl.inventory_win = IWIN_BELOW;
						get_tile_position(x, y, &tx, &ty);
						snprintf(tbuf, sizeof(tbuf), "/target !%d %d", tx - MAP_MAX_SIZE / 2, ty - MAP_MAX_SIZE / 2);
						send_command(tbuf, -1, SC_NORMAL);
					}

					break;
				}

				/* Mouse in Menu field */

				/* Combat mode */
				if ((cpl.inventory_win == IWIN_BELOW) && y > 498 && y < 521 && x < 27)
				{
					check_keys(SDLK_c);
					break;
    			}

				/* Talk button */
				if ((cpl.inventory_win == IWIN_BELOW) && y > 498 + 27 && y < 521 + 27 && x > 200 + 70 && x < 240 + 70)
				{
					if (cpl.target_code)
						send_command("/t_tell hello", -1, SC_NORMAL);

					break;
    			}

				/* Inventory (open / close) */
				if (x < 112 && y > 466 && y < 496)
				{
					if (cpl.inventory_win == IWIN_INV)
						cpl.inventory_win = IWIN_BELOW;
					else
						cpl.inventory_win = IWIN_INV;

					break;
				}


				/* Drag from quickslots */
				else if (x >= SKIN_POS_QUICKSLOT_X && x < SKIN_POS_QUICKSLOT_X + 282 && y >= SKIN_POS_QUICKSLOT_Y && y < SKIN_POS_QUICKSLOT_Y + 42)
				{
					int ind = get_quickslot(x, y);

					/* Valid slot */
					if (ind != -1 && quick_slots[ind].tag !=-1)
					{
						cpl.win_quick_tag = quick_slots[ind].tag;

						if ((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
						{
							if (quick_slots[ind].spell)
							{
							    draggingInvItem(DRAG_QUICKSLOT_SPELL);
							    quick_slots[ind].spellNr = quick_slots[ind].tag;
							    cpl.win_quick_tag = ind;
							}
							else
							{
								draggingInvItem(DRAG_QUICKSLOT);
					        }
							quick_slots[ind].tag = -1;
						}
						else
						{
							int stemp = cpl.inventory_win, itemp = cpl.win_inv_tag;

							cpl.inventory_win = IWIN_INV;
							cpl.win_inv_tag= quick_slots[ind].tag;
							process_macro_keys(KEYFUNC_APPLY, 0);
							cpl.inventory_win = stemp;
							cpl.win_inv_tag= itemp;
						}
					}

					break;
				}

				/* Inventory ( IWIN_INV ) */
				if (y > 497 && y < 593 && x > 8 && x < 238)
				{
					/* Scrollbar */
					if (x > 230)
					{
						if (y < 506 && cpl.win_inv_slot >= INVITEMXLEN)
       						cpl.win_inv_slot -= INVITEMXLEN;
						else if (y > 583)
       					{
							cpl.win_inv_slot += INVITEMXLEN;

							if (cpl.win_inv_slot > cpl.win_inv_count)
        						cpl.win_inv_slot = cpl.win_inv_count;
						}
					}
					/* Stuff */
					else
					{
						if (event.button.button == 4 && cpl.win_inv_slot >= INVITEMXLEN)
							cpl.win_inv_slot -= INVITEMXLEN;
						else if (event.button.button == 5)
						{
							cpl.win_inv_slot += INVITEMXLEN;

							if  (cpl.win_inv_slot > cpl.win_inv_count)
        						cpl.win_inv_slot = cpl.win_inv_count;
						}
						else if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)
       					{
							cpl.win_inv_slot = (y - 497) / 32 * INVITEMXLEN + (x - 8) / 32 + cpl.win_inv_start;
							cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);

							if (event.button.button == SDL_BUTTON_RIGHT)
								process_macro_keys(KEYFUNC_MARK, 0);
							else
							{
								if (cpl.inventory_win == IWIN_INV)
									draggingInvItem(DRAG_IWIN_INV);
							}
   						}
					}
					break;
     			}

				/* Ground ( IWIN_BELOW )  */
				if (y > 565 && x > 265 && x < 529)
				{
					item *Item;

					if (cpl.inventory_win == IWIN_INV)
						cpl.inventory_win = IWIN_BELOW;

					cpl.win_below_slot = (x - 265) / 32;
					cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count,	INVITEMBELOWXLEN, INVITEMBELOWYLEN);
					Item = locate_item(cpl.win_below_tag);

					if ((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
						draggingInvItem(DRAG_IWIN_BELOW);
					else
						process_macro_keys(KEYFUNC_APPLY, 0);

					break;
				}
	    		break;

			case SDL_KEYUP:
				/* end of key-repeat */
				menuRepeatKey = -1;
				menuRepeatTime = KEY_REPEAT_TIME_INIT;
				/* Fall through */

			case SDL_KEYDOWN:
				if (cpl.menustatus == MENU_NO && (!InputStringFlag || cpl.input_mode != INPUT_MODE_NUMBER))
				{
					if (event.key.keysym.mod & KMOD_SHIFT)
						cpl.inventory_win = IWIN_INV;
					else
						cpl.inventory_win = IWIN_BELOW;

					if (event.key.keysym.mod & KMOD_RCTRL || event.key.keysym.mod & KMOD_LCTRL || event.key.keysym.mod & KMOD_CTRL)
						cpl.fire_on = 1;
					else
						cpl.fire_on = 0;
				}

				if (InputStringFlag)
				{
					if (cpl.input_mode != INPUT_MODE_NUMBER)
						cpl.inventory_win = IWIN_BELOW;

					key_string_event(&event.key);
				}
				else if (!InputStringEndFlag)
        		{
					if (GameStatus <= GAME_STATUS_WAITLOOP)
						done = key_meta_menu(&event.key);
          			else if (GameStatus == GAME_STATUS_PLAY || GAME_STATUS_NEW_CHAR)
						done = key_event(&event.key);
          			else
						key_connection_event(&event.key);
				}
        		break;

			case SDL_QUIT:
				done = 1;
				break;

			default:
				break;
		}
		old_mouse_y = y;
	}

	/* OK, now we have processed all real events.
	 * Now run through the list of keybinds and control repeat time value.
	 * If the key is still marked as pressed in our keyboard mirror table,
	 * and the time this is pressed <= keybind press value + repeat value,
	 * we assume a repeat flag is true.
	 * Sadly, SDL doesn't have a tick count inside the event messages, which
	 * means the tick value when the event really was triggered. So, the
	 * client can't simulate the buffered "rythm" of the key pressings when
	 * the client lags. */
	key_repeat();

	return done;
}

void key_connection_event(SDL_KeyboardEvent *key)
{
	char buf[256];

	if (key->type == SDL_KEYDOWN)
	{
		switch (key->keysym.sym)
		{
			case SDLK_ESCAPE:
				snprintf(buf, sizeof(buf), "Connection closed. Select new server.");
				draw_info(buf, COLOR_RED);
				GameStatus = GAME_STATUS_START;
				break;

			default:
				break;
		}
	}
}

/* Metaserver menu key */
int key_meta_menu(SDL_KeyboardEvent *key)
{
	if (key->type == SDL_KEYDOWN)
	{
		switch (key->keysym.sym)
		{
			case SDLK_UP:
				if (metaserver_sel)
				{
					metaserver_sel--;

					if (metaserver_start > metaserver_sel)
						metaserver_start = metaserver_sel;
				}

				break;

			case SDLK_DOWN:
				if (metaserver_sel < metaserver_count - 1)
				{
					metaserver_sel++;

					if (metaserver_sel >= MAXMETAWINDOW)
						metaserver_start = (metaserver_sel + 1) - MAXMETAWINDOW;
				}

				break;

			case SDLK_RETURN:
			case SDLK_KP_ENTER:
				get_meta_server_data(metaserver_sel, ServerName, &ServerPort);
				GameStatus = GAME_STATUS_STARTCONNECT;
				break;

			case SDLK_ESCAPE:
				return 1;

			default:
				break;
		}
	}

	return 0;
}

/* We get TEXT from keyboard. This is for console input */
static void key_string_event(SDL_KeyboardEvent *key)
{
	register char c;
	register int i;

	if (key->type == SDL_KEYDOWN)
	{
        switch (key->keysym.sym)
		{
			case SDLK_ESCAPE:
				SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
				InputStringEscFlag = 1;
				return;

			case SDLK_KP_ENTER:
			case SDLK_RETURN:
			case SDLK_TAB:
				if (key->keysym.sym != SDLK_TAB || GameStatus < GAME_STATUS_WAITFORPLAY)
				{
					SDL_EnableKeyRepeat(0 , SDL_DEFAULT_REPEAT_INTERVAL);
					InputStringFlag = 0;
					/* Mark that we've got something here */
					InputStringEndFlag = 1;

					/* record this line in input history only if we are in console mode */
					if (cpl.input_mode == INPUT_MODE_CONSOLE)
						textwin_addhistory(InputString);
				}
				break;

			/* Erases the previous character or word if CTRL is pressed */
			case SDLK_BACKSPACE:
				if (InputCount && CurrentCursorPos)
				{
					register int ii;

					/* Actual position of the cursor */
					ii = CurrentCursorPos;
					/* Where we will end up, by default one character back */
					i = ii - 1;

					if (key->keysym.mod & KMOD_CTRL)
					{
						/* Jumps eventual whitespaces */
						while (InputString[i] == ' ' && i >= 0)
							i--;

						/* Jumps a word */
						while (InputString[i] != ' ' && i >= 0)
							i--;

						/* we end up at the beginning of the current word */
						i++;
					}

					/* This loop copies even the terminating \0 of the buffer */
					while (ii <= InputCount)
						InputString[i++] = InputString[ii++];

					CurrentCursorPos -= (ii - i);
					InputCount -= (ii - i);
				}
				break;

			/* Shifts a character or a word if CTRL is pressed */
			case SDLK_LEFT:
				if (key->keysym.mod & KMOD_CTRL)
				{
					i = CurrentCursorPos - 1;

					/* Jumps eventual whitespaces */
					while (InputString[i] == ' ' && i >= 0)
						i--;

					/* Jumps a word */
					while (InputString[i] != ' ' && i >= 0)
						i--;

					/* Places the cursor on the first letter of this word */
					CurrentCursorPos = i + 1;
				}
				else if (CurrentCursorPos > 0)
					CurrentCursorPos--;

				break;

			/* Shifts a character or a word if CTRL is pressed */
			case SDLK_RIGHT:
				if (key->keysym.mod & KMOD_CTRL)
				{
					i = CurrentCursorPos;

					/* Jumps eventual whitespaces */
					while (InputString[i] == ' ' && i < InputCount)
						i++;

					/* Jumps a word */
					while (InputString[i] != ' ' && i < InputCount)
						i++;

					/* Places the cursor right after the jumped word */
					CurrentCursorPos = i;
				}
				else if (CurrentCursorPos < InputCount)
					CurrentCursorPos++;

				break;

			/* If we are in CONSOLE mode, let player scroll back the lines in history */
			case SDLK_UP:
				if (cpl.input_mode == INPUT_MODE_CONSOLE && HistoryPos < MAX_HISTORY_LINES && InputHistory[HistoryPos + 1][0])
				{
					/* First history line is special, it records what we were writing before
					 * scrolling back the history; so, by returning back to zero, we can continue
					 * our editing where we left it. */
					if (HistoryPos == 0)
						strncpy(InputHistory[0], InputString, InputCount);

					HistoryPos++;
					textwin_putstring(InputHistory[HistoryPos]);
				}

				break;

			/* If we are in CONSOLE mode, let player scroll forward the lines in history */
			case SDLK_DOWN:
				if (cpl.input_mode == INPUT_MODE_CONSOLE && HistoryPos > 0)
				{
					HistoryPos--;
					textwin_putstring(InputHistory[HistoryPos]);
				}

				break;

			case SDLK_DELETE:
			{
				register int ii;

				/* Actual position of the cursor */
				ii = CurrentCursorPos;

				/* Where we will end up, by default one character ahead */
				i = ii + 1;

				if (ii == InputCount)
					break;

				if (key->keysym.mod & KMOD_CTRL)
				{
					/* Jumps eventual whitespaces */
					while (InputString[i] == ' ' && i < InputCount)
						i++;

					/* Jumps a word */
					while (InputString[i] != ' ' && i < InputCount)
						i++;
				}

				/* This loop copies even the terminating \0 of the buffer */
				while (i <= InputCount)
					InputString[ii++] = InputString[i++];

				InputCount -= (i - ii);

				break;
			}

			case SDLK_HOME:
				CurrentCursorPos = 0;
				break;

			case SDLK_END:
				CurrentCursorPos = InputCount;
				break;

			default:
				/* If we are in number console mode, use GET as quick enter
				 * mode - this is a very handy shortcut */
				if (cpl.input_mode == INPUT_MODE_NUMBER && (key->keysym.sym == get_action_keycode || key->keysym.sym == drop_action_keycode))
				{
					SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
					InputStringFlag = 0;
					/* Mark that we got some text here */
					InputStringEndFlag = 1;
				}

				/* Now keyboard magic - transform a sym (kind of scancode)
				 * to a layout code */
				if (InputCount < InputMax)
				{
					c = 0;

					/* We want only numbers in number mode - even when shift is held */
					if (cpl.input_mode == INPUT_MODE_NUMBER)
					{
						switch (key->keysym.sym)
						{
							case SDLK_0:
							case SDLK_KP0:
								c = '0';
								break;

							case SDLK_KP1:
							case SDLK_1:
								c = '1';
								break;

							case SDLK_KP2:
							case SDLK_2:
								c = '2';
								break;

							case SDLK_KP3:
							case SDLK_3:
								c = '3';
								break;

							case SDLK_KP4:
							case SDLK_4:
								c = '4';
								break;

							case SDLK_KP5:
							case SDLK_5:
								c = '5';
								break;

							case SDLK_KP6:
							case SDLK_6:
								c = '6';
								break;

							case SDLK_KP7:
							case SDLK_7:
								c = '7';
								break;

							case SDLK_KP8:
							case SDLK_8:
								c = '8';
								break;

							case SDLK_KP9:
							case SDLK_9:
								c = '9';
								break;

							default:
							    c = 0;
								break;
						}

						if (c)
						{
							InputString[CurrentCursorPos++] = c;
							InputCount++;
							InputString[InputCount] = 0;
						}
					}
					else
					{
						if ((key->keysym.unicode & 0xFF80) == 0)
							c = key->keysym.unicode & 0x7F;

						c = key->keysym.unicode & 0xff;

						if (c >= 32)
						{
							if (key->keysym.mod & KMOD_SHIFT)
								c = toupper(c);

							i = InputCount;

							while (i >= CurrentCursorPos)
							{
								InputString[i + 1] = InputString[i];
								i--;
							}

							InputString[CurrentCursorPos] = c;
							CurrentCursorPos++;
							InputCount++;
							InputString[InputCount] = 0;
						}
					}
				}
				break;
		}
	}
}

/* We have a key event */
int key_event(SDL_KeyboardEvent *key)
{
    if (GameStatus != GAME_STATUS_PLAY && GameStatus != GAME_STATUS_NEW_CHAR)
		return 0;

	if (key->type == SDL_KEYUP)
	{
		if (KeyScanFlag)
		{
			char buf[256];

			snprintf(buf, sizeof(buf), "Scancode: %d", key->keysym.sym);
			draw_info(buf, COLOR_RED);
		}

		if (cpl.menustatus != MENU_NO)
		{
			keys[key->keysym.sym].pressed = 0;
		}
		else
		{
			keys[key->keysym.sym].pressed = 0;

			switch (key->keysym.sym)
			{
				case SDLK_LSHIFT:
				case SDLK_RSHIFT:
						cpl.inventory_win = IWIN_BELOW;
					break;

				case SDLK_LALT:
				case SDLK_RALT:
					send_command("/run_stop", -1, SC_FIRERUN);
#if 0
					draw_info("run_stop", COLOR_DGOLD);
#endif
					cpl.run_on = 0;
					break;

				case SDLK_RCTRL:
				case SDLK_LCTRL:
					cpl.fire_on = 0;
					break;

				default:
					break;
			}
		}
	}
	else if (key->type == SDL_KEYDOWN)
	{
		if (cpl.menustatus != MENU_NO)
		{
			/* We catch here the keybind key, when we insert a new macro there */
			if (cpl.menustatus == MENU_KEYBIND)
			{
				if (keybind_status == KEYBIND_STATUS_EDITKEY)
				{
					if (key->keysym.sym != SDLK_ESCAPE)
     				{
						sound_play_effect(SOUND_SCROLL, 0, 0, 100);
						strcpy(bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].keyname, SDL_GetKeyName(key->keysym.sym));
						bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].key = key->keysym.sym;
					}

					keybind_status = KEYBIND_STATUS_NO;
					return 0;
				}
			}

			keys[key->keysym.sym].pressed = 1;
			keys[key->keysym.sym].time = LastTick + KEY_REPEAT_TIME_INIT;
			check_menu_keys(cpl.menustatus, key->keysym.sym);
		}
		/* no menu */
		else
		{
			if (esc_menu_flag != 1)
			{
				keys[key->keysym.sym].pressed = 1;
				keys[key->keysym.sym].time = LastTick + KEY_REPEAT_TIME_INIT;
     			check_keys(key->keysym.sym);
			}

			switch ((int)key->keysym.sym)
			{
				case SDLK_F1:
					quickslot_key(key, 0);
					break;

				case SDLK_F2:
					quickslot_key(key, 1);
					break;

				case SDLK_F3:
					quickslot_key(key, 2);
					break;

				case SDLK_F4:
					quickslot_key(key, 3);
					break;

				case SDLK_F5:
					quickslot_key(key, 4);
					break;

				case SDLK_F6:
					quickslot_key(key, 5);
					break;

				case SDLK_F7:
					quickslot_key(key, 6);
					break;

				case SDLK_F8:
					quickslot_key(key, 7);
					break;

				case SDLK_LSHIFT:
				case SDLK_RSHIFT:
					cpl.inventory_win = IWIN_INV;
					break;

				case SDLK_RALT:
				case SDLK_LALT:
					cpl.run_on = 1;
					break;

				case SDLK_RCTRL:
				case SDLK_LCTRL:
					cpl.fire_on = 1;
					break;

				case SDLK_ESCAPE:
					if (esc_menu_flag == 0)
					{
						map_udate_flag = 1;
						esc_menu_flag = 1;
						esc_menu_index = ESC_MENU_BACK;
					}
					else
						esc_menu_flag = 0;

		        	sound_play_effect(SOUND_SCROLL, 0, 0, 100);
					break;

		   		default:
					if (esc_menu_flag == 1)
					{
						reset_keys();

						switch ((int)key->keysym.sym)
						{
							case SDLK_RETURN:
							case SDLK_KP_ENTER:
								if (esc_menu_index == ESC_MENU_KEYS)
								{
									keybind_status = KEYBIND_STATUS_NO;
									cpl.menustatus = MENU_KEYBIND;
								}
								else if (esc_menu_index == ESC_MENU_SETTINGS)
								{
									keybind_status = KEYBIND_STATUS_NO;

									if (cpl.menustatus == MENU_KEYBIND)
										save_keybind_file(KEYBIND_FILE);

									cpl.menustatus = MENU_OPTION;
								}
								else if (esc_menu_index == ESC_MENU_LOGOUT)
								{
									save_quickslots_entrys();
									SOCKET_CloseSocket(csocket.fd);
									GameStatus = GAME_STATUS_INIT;
								}

				        		sound_play_effect(SOUND_SCROLL, 0, 0, 100);
								esc_menu_flag = 0;

								break;

							case SDLK_UP:
								esc_menu_index--;

								if (esc_menu_index < 0)
									esc_menu_index = ESC_MENU_INDEX - 1;

								break;

							case SDLK_DOWN:
								esc_menu_index++;

								if (esc_menu_index >= ESC_MENU_INDEX)
									esc_menu_index = 0;

								break;
						}
					}
					break;
			}
		}
	}

	return 0;
}


/* Here we look in the user defined keymap and try to get same useful macros */
static Boolean check_menu_macros(char *text)
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

        sound_play_effect(SOUND_SCROLL, 0, 0, 100);
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

        sound_play_effect(SOUND_SCROLL, 0, 0, 100);
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

        sound_play_effect(SOUND_SCROLL, 0, 0, 100);
        reset_keys();
        return 1;
    }
    else if (!strcmp("?M_STATUS", text))
    {
        if (cpl.menustatus == MENU_KEYBIND)
            save_keybind_file(KEYBIND_FILE);

        map_udate_flag = 2;

        if (cpl.menustatus != MENU_STATUS)
            cpl.menustatus = MENU_STATUS;
		else
			cpl.menustatus = MENU_NO;

		sound_play_effect(SOUND_SCROLL, 0, 0, 100);
		reset_keys();
		return 1;
	}

	return 0;
}

/* Here we handle menu change direct from open menu to
 * open menu, menu close by double press the trigger key
 * and other menu handling stuff - but NOT the keys
 * inside a menu! */
static int check_keys_menu_status(int key)
{
	int i, j;

	/* Groups */
	for (j = 0; j< BINDKEY_LIST_MAX; j++)
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


static void check_keys(int key)
{
	int i, j;
	char buf[512];

	/* groups */
	for (j = 0; j < BINDKEY_LIST_MAX; j++)
	{
		for (i = 0; i < OPTWIN_MAX_OPT; i++)
		{
			if (key == bindkey_list[j].entry[i].key)
			{
				/* if no key macro, submit the text as cmd*/
				if (check_macro_keys(bindkey_list[j].entry[i].text))
				{
					draw_info(bindkey_list[j].entry[i].text, COLOR_DGOLD);
					strcpy(buf, bindkey_list[j].entry[i].text);

					if (!client_command_check(buf))
						send_command(buf, -1, bindkey_list[j].entry[i].mode);
				}

				return;
			}
		}
	}
}


static Boolean check_macro_keys(char *text)
{
	register int i;
	int magic_len;

	magic_len = strlen(macro_magic_console);

	if (!strncmp(macro_magic_console, text, magic_len) && (int)strlen(text) > magic_len)
	{
		process_macro_keys(KEYFUNC_CONSOLE, 0);
		textwin_putstring(&text[magic_len]);
		return 0;
	}

	for (i = 0; i < DEFAULT_KEYMAP_MACROS; i++)
	{
		if (!strcmp(defkey_macro[i].macro, text))
		{
			if (!process_macro_keys(defkey_macro[i].internal, defkey_macro[i].value))
				return 0;

			return 1;
		}
	}

	return 1;
}


Boolean process_macro_keys(int id, int value)
{
	int nrof, tag = 0, loc = 0;
	char buf[256];
	item *it, *tmp;

	switch (id)
	{
 		case KEYFUNC_FIREREADY:
 			if (cpl.inventory_win == IWIN_BELOW)
 				tag = cpl.win_below_tag;
 			else
				tag = cpl.win_inv_tag;

			examine_range_marks(tag);
 			break;

 		case KEYFUNC_PAGEUP:
			if (options.use_TextwinSplit)
				txtwin[TW_MSG].scroll++;
			else
				txtwin[TW_MIX].scroll++;

			break;

		case KEYFUNC_PAGEDOWN:
			if (options.use_TextwinSplit)
				txtwin[TW_MSG].scroll--;
			else
				txtwin[TW_MIX].scroll--;

			break;

		case KEYFUNC_PAGEUP_TOP:
			txtwin[TW_CHAT].scroll++;
			break;

		case KEYFUNC_PAGEDOWN_TOP:
			txtwin[TW_CHAT].scroll--;
			break;

        case KEYFUNC_TARGET_ENEMY:
			send_command("/target 0", -1, SC_NORMAL);
        	break;

        case KEYFUNC_TARGET_FRIEND:
			send_command("/target 1", -1, SC_NORMAL);
        	break;

        case KEYFUNC_TARGET_SELF:
			send_command("/target 2", -1, SC_NORMAL);
        	break;

        case KEYFUNC_COMBAT:
			send_command("/combat", -1, SC_NORMAL);
        	break;

        case KEYFUNC_SPELL:
            map_udate_flag = 2;
            sound_play_effect(SOUND_SCROLL, 0, 0, 100);

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

            sound_play_effect(SOUND_SCROLL, 0, 0, 100);

            if (cpl.menustatus != MENU_SKILL)
                cpl.menustatus = MENU_SKILL;
            else
                cpl.menustatus = MENU_NO;

            reset_keys();
            break;

        case KEYFUNC_STATUS:
            map_udate_flag = 2;

			if (cpl.menustatus == MENU_KEYBIND)
				save_keybind_file(KEYBIND_FILE);

            if (cpl.menustatus != MENU_STATUS)
                cpl.menustatus = MENU_STATUS;
            else
                cpl.menustatus = MENU_NO;

            sound_play_effect(SOUND_SCROLL, 0, 0, 0);
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

            sound_play_effect(SOUND_SCROLL, 0, 0, 100);
            reset_keys();
            break;

      case KEYFUNC_CONSOLE:
            sound_play_effect(SOUND_CONSOLE, 0, 0, 100);
            reset_keys();

            if (cpl.input_mode == INPUT_MODE_NO)
			{
				cpl.input_mode = INPUT_MODE_CONSOLE;
                open_input_mode(253);
			}
			else if (cpl.input_mode == INPUT_MODE_CONSOLE)
				cpl.input_mode = INPUT_MODE_NO;

			map_udate_flag = 2;
			break;

        case KEYFUNC_RUN:
			if (!(cpl.runkey_on = cpl.runkey_on ? 0 : 1))
				send_command("/run_stop", -1, SC_FIRERUN);

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

			if (tag == -1 || !locate_item(tag))
				return 0;

			snprintf(buf, sizeof(buf), "apply %s", locate_item(tag)->s_name);
			draw_info(buf, COLOR_DGOLD);
			client_send_apply(tag);
			return 0;

		case KEYFUNC_EXAMINE:
			if (cpl.inventory_win == IWIN_BELOW)
				tag = cpl.win_below_tag;
			else
			    tag = cpl.win_inv_tag;

			if (tag == -1 || !locate_item(tag))
				return 0;

			client_send_examine(tag);
			snprintf(buf, sizeof(buf), "examine %s", locate_item(tag)->s_name);
			draw_info(buf, COLOR_DGOLD);
			return 0;

		case KEYFUNC_MARK:
			if (cpl.inventory_win == IWIN_BELOW)
				tag = cpl.win_below_tag;
			else
			    tag = cpl.win_inv_tag;

			if (tag == -1 || !locate_item(tag))
				return 0;

			send_mark_obj((it = locate_item(tag)));
			snprintf(buf, sizeof(buf), "mark %s", it->s_name);
			draw_info(buf, COLOR_DGOLD);
            return 0;

		case KEYFUNC_LOCK:
            if (cpl.inventory_win == IWIN_BELOW)
				tag = cpl.win_below_tag;
			else
			    tag = cpl.win_inv_tag;

			if (tag == -1  || !locate_item(tag))
				return 0;

			toggle_locked((it = locate_item(tag)));

			if (!it)
				return 0;

			if (it->locked)
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
					/* container, aber nicht der gleiche */
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

			if (tag == -1 || !locate_item(tag))
				return 0;

			if ((it = locate_item(tag)))
				nrof = it->nrof;
            else
                return 0;

           	if (nrof == 1)
				nrof = 0;
            else
            {
            	if (options.collectAll == 1)
              	{
               		nrof = cpl.nrof;
               		goto collectAll;
              	}

                reset_keys();
                cpl.input_mode = INPUT_MODE_NUMBER;
                open_input_mode(22);
                cpl.loc = loc;
                cpl.tag = tag;
				cpl.nrof = nrof;
				cpl.nummode = NUM_MODE_GET;
				snprintf(buf, sizeof(buf), "%d", nrof);
				textwin_putstring(buf);
				strncpy(cpl.num_text,it->s_name, 250);
				cpl.num_text[250] = 0;
                return 0;
            }

			collectAll:
			sound_play_effect(SOUND_GET, 0, 0, 100);
            snprintf(buf, sizeof(buf), "get %s", it->s_name);
			draw_info(buf, COLOR_DGOLD);
			client_send_move(loc, tag, nrof);
            return 0;

		case KEYFUNC_LAYER0:
			if (debug_layer[0])
				debug_layer[0] = 0;
			else
				debug_layer[0] = 1;

			snprintf(buf, sizeof(buf), "debug: map layer 0 %s.", debug_layer[0] ? "activated" : "deactivated");
			draw_info(buf, COLOR_DGOLD);
			return 0;

		case KEYFUNC_LAYER1:
			if (debug_layer[1])
				debug_layer[1] = 0;
			else
				debug_layer[1] = 1;

			snprintf(buf, sizeof(buf), "debug: map layer 1 %s.", debug_layer[1] ? "activated" : "deactivated");
			draw_info(buf, COLOR_DGOLD);
			return 0;

		case KEYFUNC_LAYER2:
			if (debug_layer[2])
				debug_layer[2] = 0;
			else
				debug_layer[2] = 1;

			snprintf(buf, sizeof(buf), "debug: map layer 2 %s.", debug_layer[2] ? "activated" : "deactivated");
			draw_info(buf, COLOR_DGOLD);
			return 0;

		case KEYFUNC_LAYER3:
			if (debug_layer[3])
				debug_layer[3] = 0;
			else
				debug_layer[3] = 1;

			snprintf(buf, sizeof(buf), "debug: map layer 3 %s.", debug_layer[3] ? "activated" : "deactivated");
			draw_info(buf, COLOR_DGOLD);
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

			if (tag == -1 || !locate_item(tag))
				return 0;

			if ((it = locate_item(tag)))
				nrof = it->nrof;
			else
				return 0;

			if (it->locked)
	   		{
      			sound_play_effect(SOUND_CLICKFAIL, 0, 0, 100);
	   			draw_info("Unlock item first!", COLOR_DGOLD);
				return 0;
	   		}

			if (nrof == 1)
				nrof = 0;
			else
			{
				reset_keys();
				cpl.input_mode = INPUT_MODE_NUMBER;
				open_input_mode(22);
				cpl.loc = loc;
				cpl.tag = tag;
				cpl.nrof = nrof;
				cpl.nummode = NUM_MODE_DROP;
				snprintf(buf, sizeof(buf), "%d", nrof);
				textwin_putstring(buf);
				strncpy(cpl.num_text, it->s_name, 250);
				cpl.num_text[250] = 0;
				return 0;
			}

			sound_play_effect(SOUND_DROP, 0, 0, 100);
			snprintf(buf, sizeof(buf), "drop %s", it->s_name);
			draw_info(buf, COLOR_DGOLD);
			client_send_move(loc, tag, nrof);
			return 0;

		default:
			return 1;
	}

	return 0;
}

static void cursor_keys(int num)
{
	switch (num)
	{
		case 0:
			if (cpl.inventory_win == IWIN_BELOW)
			{
				if (cpl.win_below_slot - INVITEMBELOWXLEN >= 0)
					cpl.win_below_slot -= INVITEMBELOWXLEN;

				cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
			}
			else
			{
			    if (cpl.win_inv_slot - INVITEMXLEN >= 0)
				    cpl.win_inv_slot -= INVITEMXLEN;

				cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
			}
			break;

		case 1:
			if (cpl.inventory_win == IWIN_BELOW)
			{
				if (cpl.win_below_slot + INVITEMXLEN < cpl.win_below_count)
					cpl.win_below_slot += INVITEMXLEN;

				cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
			}
			else
			{
		        if (cpl.win_inv_slot + INVITEMXLEN < cpl.win_inv_count)
					cpl.win_inv_slot += INVITEMXLEN;

				cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
			}
			break;

		case 2:
			if (cpl.inventory_win == IWIN_BELOW)
			{
				cpl.win_below_slot--;

				cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
			}
			else
			{
				cpl.win_inv_slot--;

				cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
			}
			break;

		case 3:
			if (cpl.inventory_win == IWIN_BELOW)
			{
				cpl.win_below_slot++;

				cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
			}
			else
			{
				cpl.win_inv_slot++;
				cpl.win_inv_tag =

				get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
			}
			break;
	}
}

/* Handle quickslot key event. */
static void quickslot_key(SDL_KeyboardEvent *key, int slot)
{
   	int tag;
   	char buf[256];

   	/* Put spell into quickslot */
   	if (!key && cpl.menustatus == MENU_SPELL)
   	{
		if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].flag == LIST_ENTRY_KNOWN)
      	{
		  	if (quick_slots[slot].spell && quick_slots[slot].tag == spell_list_set.entry_nr)
		  	{
			  	quick_slots[slot].spell = 0;
			  	quick_slots[slot].tag = -1;

			  	snprintf(buf, sizeof(buf), "unset F%d.", slot + 1);
			  	draw_info(buf, COLOR_DGOLD);
		  	}
		  	else
		  	{
			  	quick_slots[slot].spell = 1;
			  	quick_slots[slot].groupNr = spell_list_set.group_nr;
			  	quick_slots[slot].classNr = spell_list_set.class_nr;
			  	quick_slots[slot].tag = spell_list_set.entry_nr;

			  	snprintf(buf, sizeof(buf), "set F%d to %s", slot + 1, spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].name);
			  	draw_info(buf, COLOR_DGOLD);
		  	}
      	}
   	}
   	/* Put item into quickslot */
   	else if (key && key->keysym.mod & (KMOD_SHIFT | KMOD_ALT))
	{
		if (cpl.inventory_win == IWIN_BELOW)
			return;

      	tag = cpl.win_inv_tag;

      	if (tag == -1 || !locate_item(tag))
         	return;

      	quick_slots[slot].spell = 0;

	  	if (quick_slots[slot].tag == tag)
		  	quick_slots[slot].tag = -1;
	  	else
	  	{
		  	quick_slots[slot].tag = tag;
		  	quick_slots[slot].invSlot = cpl.win_inv_slot;

		  	snprintf(buf, sizeof(buf), "set F%d to %s", slot + 1, locate_item(tag)->s_name);
		  	draw_info(buf, COLOR_DGOLD);
	  	}
   	}
   	/* Apply item or ready spell */
   	else if (key)
   	{
		if (quick_slots[slot].tag != -1)
      	{
          	if (quick_slots[slot].spell)
          	{
             	fire_mode_tab[FIRE_MODE_SPELL].spell = &spell_list[quick_slots[slot].groupNr].entry[quick_slots[slot].classNr][quick_slots[slot].tag];
             	RangeFireMode = 1;
             	spell_list_set.group_nr = quick_slots[slot].groupNr;
             	spell_list_set.class_nr = quick_slots[slot].classNr;
             	spell_list_set.entry_nr = quick_slots[slot].tag;
             	return;
          	}

          	if (locate_item(quick_slots[slot].tag))
          	{
             	snprintf(buf, sizeof(buf), "F%d quick apply %s", slot + 1, locate_item(quick_slots[slot].tag)->s_name);
             	draw_info(buf, COLOR_DGOLD);
             	client_send_apply(quick_slots[slot].tag);
             	return;
          	}
      	}

      	snprintf(buf, sizeof(buf), "F%d quick slot is empty", slot + 1);
      	draw_info(buf, COLOR_DGOLD);
   	}
}

static void move_keys(int num)
{
	char buf[256];
	char msg[256];

	/* Move will overrule from fire
	 * because real toggle mode doesn't work, this works a bit different
	 * pressing alt will not set move mode until unpressed when firemode is on
	 * but it stops running when released */

	/* Runmode on, or ALT key trigger */
	if ((cpl.runkey_on || cpl.run_on) && (!cpl.firekey_on && !cpl.fire_on))
	{
		send_command(directionsrun[num], -1, SC_FIRERUN);
		strcpy(buf, "run ");
	}
	/* That's the range menu - we handle its messages unique */
	else if (cpl.firekey_on || cpl.fire_on)
	{
        if (RangeFireMode == FIRE_MODE_SKILL)
        {
            if (!fire_mode_tab[FIRE_MODE_SKILL].skill || fire_mode_tab[FIRE_MODE_SKILL].skill->flag == -1)
			{
				draw_info("No skill selected.", COLOR_WHITE);
                return;
			}

            snprintf(buf, sizeof(buf), "/%s %d %d %s", directionsfire[num], RangeFireMode, -1,fire_mode_tab[RangeFireMode].skill->name);
            snprintf(msg, sizeof(msg), "use %s %s", fire_mode_tab[RangeFireMode].skill->name, directions_name[num]);
        }
        else if (RangeFireMode == FIRE_MODE_SPELL)
        {
            if (!fire_mode_tab[FIRE_MODE_SPELL].spell || fire_mode_tab[FIRE_MODE_SPELL].spell->flag == -1)
			{
				draw_info("No spell selected.", COLOR_WHITE);
                return;
			}

            snprintf(buf, sizeof(buf), "/%s %d %d %s", directionsfire[num], RangeFireMode, -1, fire_mode_tab[RangeFireMode].spell->name);
            snprintf(msg, sizeof(msg), "cast %s %s", fire_mode_tab[RangeFireMode].spell->name, directions_name[num]);
        }
        else
            snprintf(buf, sizeof(buf), "/%s %d %d %d", directionsfire[num], RangeFireMode, fire_mode_tab[RangeFireMode].item, fire_mode_tab[RangeFireMode].amun);

        if (RangeFireMode == FIRE_MODE_BOW)
		{
            if (fire_mode_tab[FIRE_MODE_BOW].item == FIRE_ITEM_NO)
			{
				draw_info("No range weapon selected.", COLOR_WHITE);
                return;
			}
            else if (fire_mode_tab[FIRE_MODE_BOW].amun == FIRE_ITEM_NO)
			{
				draw_info("No ammunition selected.", COLOR_WHITE);
                return;
			}

            snprintf(msg, sizeof(msg), "fire %s", directions_name[num]);
		}
        else if (RangeFireMode == FIRE_MODE_THROW)
		{
            if (fire_mode_tab[FIRE_MODE_THROW].item == FIRE_ITEM_NO)
			{
				draw_info("No item selected.", COLOR_WHITE);
                return;
			}

            snprintf(msg, sizeof(msg), "throw %s", directions_name[num]);
		}
        else if (RangeFireMode == FIRE_MODE_WAND)
		{
            if (fire_mode_tab[FIRE_MODE_WAND].item == FIRE_ITEM_NO)
			{
				draw_info("No device selected.", COLOR_WHITE);
                return;
			}

            snprintf(msg, sizeof(msg), "fire device %s", directions_name[num]);
		}
        else if (RangeFireMode == FIRE_MODE_SUMMON)
		{
            snprintf(msg, sizeof(msg), "cmd golem %s", directions_name[num]);
		}

		fire_command(buf);
#if 0
		draw_info(msg, COLOR_DGOLD);
#endif
		return;
	}
	else
	{
		send_command(directions[num], -1, SC_FIRERUN);
		buf[0] = 0;
	}

	strcat(buf, directions_name[num]);

#if 0
	draw_info(buf, COLOR_DGOLD);
#endif
}

/* Handle key repeating. */
static void key_repeat(void)
{
	register int i, j;
	char buf[512];

	/* TODO: optimize this one, too */
	if (cpl.menustatus == MENU_NO)
	{
		/* Groups */
		for (j = 0; j < BINDKEY_LIST_MAX; j++)
		{
			for (i = 0; i < OPTWIN_MAX_OPT; i++)
			{
				/* Key for this keymap is pressed */
        		if (keys[bindkey_list[j].entry[i].key].pressed && bindkey_list[j].entry[i].repeatflag)
        		{
					/* If time to repeat */
					if (keys[bindkey_list[j].entry[i].key].time + KEY_REPEAT_TIME - 5 < LastTick)
					{
			    		/* Repeat x times*/
    					while ((keys[bindkey_list[j].entry[i].key].time += KEY_REPEAT_TIME - 5) < LastTick)
						{
							/* If no key macro, submit the text as command */
		    				if (check_macro_keys(bindkey_list[j].entry[i].text))
							{
								strcpy(buf, bindkey_list[j].entry[i].text);

								if (!client_command_check(buf))
									send_command(buf, -1, bindkey_list[j].entry[i].mode);

								draw_info(bindkey_list[j].entry[i].text, COLOR_DGOLD);
							}
			    		}
    				}
    			}
			}
		}
	}
	/* check menu keys for repeat */
	else
	{
		if (SDL_GetTicks() - menuRepeatTicks > menuRepeatTime || !menuRepeatTicks || menuRepeatKey < 0)
		{
			menuRepeatTicks = SDL_GetTicks();

			if (menuRepeatKey >= 0)
			{
				check_menu_keys(cpl.menustatus, menuRepeatKey);
				menuRepeatTime = KEY_REPEAT_TIME;
			}
		}
	}
}

/* Import the key-binding file. */
void read_keybind_file(char *fname)
{
	FILE *stream;
	char line[255];
	int i, pos;

	if ((stream = fopen(fname, "r")))
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
		LOG(LOG_ERROR, "ERROR: key-binding file %s is corrupt.\n", fname);
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

	if (!(stream = fopen(fname, "w+")))
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


/* Handle keystrokes in menu dialog. */
void check_menu_keys(int menu, int key)
{
   	int shiftPressed = SDL_GetModState() & KMOD_SHIFT;

   	if (cpl.menustatus == MENU_NO)
		return;

   	/* close menu */
   	if (key == SDLK_ESCAPE)
   	{
      	if (cpl.menustatus == MENU_KEYBIND)
      		save_keybind_file(KEYBIND_FILE);

      	if (cpl.menustatus == MENU_CREATE)
      	{
         	SOCKET_CloseSocket(csocket.fd);
         	GameStatus = GAME_STATUS_INIT;
      	}

      	cpl.menustatus = MENU_NO;
      	map_udate_flag = 2;
      	reset_keys();
      	return;
   	}

   	if (check_keys_menu_status(key))
      	return;

   	switch (menu)
   	{
		case MENU_BOOK:
	   		if (!gui_interface_book || gui_interface_book->pages == 0)
		   		return;

	   		switch (key)
	   		{
	   			case SDLK_LEFT:
		   			gui_interface_book->page_show -= 2;

			   		if (gui_interface_book->page_show < 0)
		   			{
			   			gui_interface_book->page_show = 0;
			   			sound_play_effect(SOUND_CLICKFAIL, 0, 0, MENU_SOUND_VOL);
		   			}
		   			else
		   			{
			   			sound_play_effect(SOUND_PAGE, 0, 0, MENU_SOUND_VOL);
		   			}

		   			break;

	   			case SDLK_RIGHT:
		   			gui_interface_book->page_show += 2;

		   			if (gui_interface_book->page_show > (gui_interface_book->pages - 1))
		   			{
			   			gui_interface_book->page_show = (gui_interface_book->pages - 1) &~ 1;
			   			sound_play_effect(SOUND_CLICKFAIL, 0, 0, MENU_SOUND_VOL);
		   			}
		   			else
		   			{
			   			sound_play_effect(SOUND_PAGE, 0, 0, MENU_SOUND_VOL);
		   			}

		   			break;
	   		}

	   		break;

      	case MENU_OPTION:
         	switch (key)
			{
            	case SDLK_LEFT:
               		option_list_set.key_change =-1;
               		/*sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);*/
               		menuRepeatKey = SDLK_LEFT;
               		break;

            	case SDLK_RIGHT:
               		option_list_set.key_change = 1;
               		/*sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);*/
               		menuRepeatKey = SDLK_RIGHT;
               		break;

            	case SDLK_UP:
               		if (!shiftPressed)
					{
                  		if (option_list_set.entry_nr > 0)
                     		option_list_set.entry_nr--;
                  		else
                     		sound_play_effect(SOUND_CLICKFAIL, 0, 0, MENU_SOUND_VOL);
               		}
					else
					{
                  		if (option_list_set.group_nr > 0)
                  		{
                      		option_list_set.group_nr--;
							option_list_set.entry_nr = 0;
                  		}
               		}

               		menuRepeatKey = SDLK_UP;
               		break;

            	case SDLK_DOWN:
               		if (!shiftPressed)
					{
                   		option_list_set.entry_nr++;
                	}
					else
					{
                    	if (opt_tab[option_list_set.group_nr + 1])
                    	{
                       		option_list_set.group_nr++;
                       		option_list_set.entry_nr = 0;
                    	}
                	}

                	menuRepeatKey = SDLK_DOWN;
                	break;

            	case SDLK_d:
                	sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
                	map_udate_flag = 2;

                	if (cpl.menustatus == MENU_KEYBIND)
                   		save_keybind_file(KEYBIND_FILE);

                 	if (cpl.menustatus == MENU_OPTION)
                   		save_options_dat();

                 	cpl.menustatus = MENU_NO;
               		reset_keys();
               		break;
           	}
           	break;

		/* The party GUI menu */
		case MENU_PARTY:
			if (!gui_interface_party)
				return;

			switch (key)
			{
				/* Arrow up */
				case SDLK_UP:
					/* If shift is pressed too */
					if (shiftPressed)
					{
						/* If the current tab is not 0, go to the above tab and switch tab. */
						if (gui_interface_party->tab > 0)
						{
							gui_interface_party->tab--;
							gui_interface_party->selected = 0;
							switch_tabs();
						}
					}
					/* Otherwise, we're scrolling in the GUI, and adjusting the selected row too */
					else
					{
						gui_interface_party->selected--;

						if (gui_interface_party->yoff > gui_interface_party->selected)
							gui_interface_party->yoff--;
					}

					menuRepeatKey = SDLK_UP;
					break;

				/* Arrow down */
				case SDLK_DOWN:
					/* If shift is pressed too */
					if (shiftPressed)
					{
						/* If the next time won't be over maximum, go to the below tab and switch tab. */
						if (gui_interface_party->tab + 1 < (strcmp(cpl.partyname, "") == 0 ? PARTY_TAB_LIST + 1 : PARTY_TABS))
						{
							gui_interface_party->tab++;
							gui_interface_party->selected = 0;
							switch_tabs();
						}
					}
					/* Otherwise, we're scrolling in the GUI, and adjusting the selected row too */
					else
					{
						gui_interface_party->selected++;

						if (gui_interface_party->selected >= DIALOG_LIST_ENTRY + gui_interface_party->yoff)
							gui_interface_party->yoff++;
					}

					menuRepeatKey = SDLK_DOWN;
					break;

				/* Enter or return key */
				case SDLK_KP_ENTER:
				case SDLK_RETURN:
				{
					/* Was pressed when we were in the parties list */
					if (strcmp(gui_interface_party->command, "list") == 0)
					{
						_gui_party_line *party_line = gui_interface_party->start;
						int i = 0;
						char partyname[HUGE_BUF], buf[HUGE_BUF];

						partyname[0] = '\0';

						/* Go through the lines, looking for our selected party */
						while (party_line)
						{
							/* Got it! */
							if (i == gui_interface_party->selected)
							{
								sscanf(party_line->line, "Name: %32[^\t]", partyname);
								break;
							}

							i++;
							party_line = party_line->next;
						}

						/* If we found it... */
						if (partyname[0] != '\0')
						{
							/* ... and it's not party we're member of, send command to server and close the GUI. */
							if (strcmp(partyname, cpl.partyname))
							{
								sprintf(buf, "pt join %s", partyname);
								cs_write_string(csocket.fd, buf, strlen(buf));

								map_udate_flag = 2;
								cpl.menustatus = MENU_NO;
								reset_keys();
							}
						}
					}
				}

				/* y key */
				case SDLK_y:
					/* Coming from the GUI that asks you if you're sure you want to leave... */
					if (strcmp(gui_interface_party->command, "askleave") == 0)
					{
						/* Check the command */
						if (!client_command_check("/party leave"))
							send_command("/party leave", -1, SC_NORMAL);

						/* Close the menu */
						cpl.menustatus = MENU_NO;
					}
					/* Otherwise it's GUI that asks us if we really want to change the password. :-) */
					else if (strcmp(gui_interface_party->command, "askpassword") == 0)
					{
						/* Load the party interface - mostly we need it to set the command. */
						gui_interface_party = load_party_interface("passwordset", 11);
						cpl.input_mode = INPUT_MODE_CONSOLE;

						/* Open the console, with a maximum of 8 chars. */
						open_input_mode(8);

						/* Output some info as to why the console magically opened. */
						draw_info("Type the new party password, or press ESC to cancel.", 10);

						/* Close the menu */
						cpl.menustatus = MENU_NO;
					}

					break;

				/* n key*/
				case SDLK_n:
					/* If used from the GUI to change party password, or leave party, just close the menu */
					if (strcmp(gui_interface_party->command, "askleave") == 0 || strcmp(gui_interface_party->command, "askpassword") == 0)
						cpl.menustatus = MENU_NO;

					break;

				/* f key */
				case SDLK_f:
					/* From the list... */
					if (strcmp(gui_interface_party->command, "list") == 0)
					{
						/* Load the party interface */
						gui_interface_party = load_party_interface("form", 4);

						cpl.input_mode = INPUT_MODE_CONSOLE;

						/* Open the console with a 60 chars limit */
						open_input_mode(60);

						/* Output some info as to why the console magically opened */
						draw_info("Type the party name to form, or press ESC to cancel.", 10);

						/* Close the menu */
						cpl.menustatus = MENU_NO;
					}

					break;
			}

			/* Some sanity checks to make sure yoff doesn't go off-limits. */
			if (gui_interface_party->yoff < 0 || gui_interface_party->lines < DIALOG_LIST_ENTRY)
            	gui_interface_party->yoff = 0;
			else if (gui_interface_party->yoff >= gui_interface_party->lines - DIALOG_LIST_ENTRY)
				gui_interface_party->yoff = gui_interface_party->lines - DIALOG_LIST_ENTRY;

			/* Checks for the selected row too. */
			if (gui_interface_party->selected < 0 || gui_interface_party->lines == 0)
				gui_interface_party->selected = 0;
			else if (gui_interface_party->selected >= gui_interface_party->lines)
				gui_interface_party->selected = gui_interface_party->lines - 1;

			break;

      	case MENU_SKILL:
         	switch (key)
			{
            	case SDLK_RETURN:
				case SDLK_KP_ENTER:
               		if (skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].flag == LIST_ENTRY_KNOWN)
               		{
                  		fire_mode_tab[FIRE_MODE_SKILL].skill = &skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr];
                  		RangeFireMode = FIRE_MODE_SKILL;
                  		sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
               		}
					else
                   		sound_play_effect(SOUND_CLICKFAIL, 0, 0, MENU_SOUND_VOL);

               		map_udate_flag = 2;
               		cpl.menustatus = MENU_NO;
               		reset_keys();
               		break;

            	case SDLK_UP:
               		if (!shiftPressed)
					{
                  		if (skill_list_set.entry_nr > 0)
                     		skill_list_set.entry_nr--;
                   	}
					else
					{
                      	if (skill_list_set.group_nr > 0)
                      	{
                         	skill_list_set.group_nr--;
                         	skill_list_set.entry_nr=0;
                      	}
               		}

               		menuRepeatKey = SDLK_UP;
               		break;

            	case SDLK_DOWN:
               		if (!shiftPressed)
					{
                  		if (skill_list_set.entry_nr < DIALOG_LIST_ENTRY - 1)
                     		skill_list_set.entry_nr++;
                  	}
					else
					{
                     	if (skill_list_set.group_nr < SKILL_LIST_MAX - 1)
                     	{
                        	skill_list_set.group_nr++;
                        	skill_list_set.entry_nr = 0;
                     	}
                  	}

                	menuRepeatKey = SDLK_DOWN;
                	break;
            }

            break;

      	case MENU_SPELL:
		  	switch (key)
			{
				case SDLK_F1:
					quickslot_key(NULL, 0);
					break;

				case SDLK_F2:
					quickslot_key(NULL, 1);
					break;

				case SDLK_F3:
					quickslot_key(NULL, 2);
					break;

				case SDLK_F4:
					quickslot_key(NULL, 3);
					break;

				case SDLK_F5:
					quickslot_key(NULL, 4);
					break;

				case SDLK_F6:
					quickslot_key(NULL, 5);
					break;

				case SDLK_F7:
					quickslot_key(NULL, 6);
					break;

				case SDLK_F8:
					quickslot_key(NULL, 7);
					break;

				case SDLK_RETURN:
				case SDLK_KP_ENTER:
					if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].flag == LIST_ENTRY_KNOWN)
					{
						fire_mode_tab[FIRE_MODE_SPELL].spell = &spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr];
						RangeFireMode = FIRE_MODE_SPELL;
						sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
					}
					else
						sound_play_effect(SOUND_CLICKFAIL, 0, 0, MENU_SOUND_VOL);

					map_udate_flag = 2;
					cpl.menustatus = MENU_NO;
					reset_keys();
					break;

				case SDLK_LEFT:
					if (spell_list_set.class_nr > 0)
						spell_list_set.class_nr--;

					sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
					break;

				case SDLK_RIGHT:
					if (spell_list_set.class_nr < SPELL_LIST_CLASS - 1)
						spell_list_set.class_nr++;

					sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
					break;

				case SDLK_UP:
					if (!shiftPressed)
					{
						if (spell_list_set.entry_nr > 0)
							spell_list_set.entry_nr--;
					}
					else
					{
						if (spell_list_set.group_nr > 0)
						{
							spell_list_set.group_nr--;
							spell_list_set.entry_nr = 0;
						}
					}

					menuRepeatKey = SDLK_UP;
					break;

				case SDLK_DOWN:
					if (!shiftPressed)
					{
						if (spell_list_set.entry_nr < DIALOG_LIST_ENTRY - 1)
							spell_list_set.entry_nr++;
					}
					else
					{
						if (spell_list_set.group_nr < SPELL_LIST_MAX - 1)
						{
							spell_list_set.group_nr++;
							spell_list_set.entry_nr = 0;
						}
					}

					menuRepeatKey = SDLK_DOWN;
					break;
			}

			break;

      	case MENU_KEYBIND:
         	switch (key)
			{
				case SDLK_UP:
					if (!shiftPressed)
					{
						if (bindkey_list_set.entry_nr > 0)
							bindkey_list_set.entry_nr--;
					}
					else
					{
						if (bindkey_list_set.group_nr > 0)
						{
							bindkey_list_set.group_nr--;
							bindkey_list_set.entry_nr = 0;
						}
					}

					menuRepeatKey = SDLK_UP;
					break;

				case SDLK_DOWN:
					if (!shiftPressed)
					{
						if (bindkey_list_set.entry_nr < OPTWIN_MAX_OPT - 1)
							bindkey_list_set.entry_nr++;
					}
					else
					{
						if (bindkey_list_set.group_nr < BINDKEY_LIST_MAX - 1 && bindkey_list[bindkey_list_set.group_nr+1].name[0])
						{
							bindkey_list_set.group_nr++;
							bindkey_list_set.entry_nr = 0;
						}
					}

					menuRepeatKey = SDLK_DOWN;
					break;

				case SDLK_d:
					save_keybind_file(KEYBIND_FILE);
					sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
					map_udate_flag = 2;
					cpl.menustatus = MENU_NO;
					reset_keys();
					sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
					break;

				case SDLK_RETURN:
				case SDLK_KP_ENTER:
					sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
					keybind_status = KEYBIND_STATUS_EDIT;
					reset_keys();
					open_input_mode(240);
					textwin_putstring(bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text);
					cpl.input_mode = INPUT_MODE_GETKEY;
					sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
					break;

				case SDLK_r:
					sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
					bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].repeatflag = bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].repeatflag ? 0 : 1;
					sound_play_effect(SOUND_SCROLL, 0, 0, MENU_SOUND_VOL);
					break;
			}

			break;

       	case MENU_CREATE:
          	switch (key)
			{
             	case SDLK_RETURN:
                	break;

             	case SDLK_c:
                 	if (new_character.stat_points)
                 	{
                    	dialog_new_char_warn = 1;
                    	sound_play_effect(SOUND_CLICKFAIL, 0, 0, 100);
                    	break;
                 	}

                 	dialog_new_char_warn = 0;
                 	new_char(&new_character);
                 	GameStatus = GAME_STATUS_WAITFORPLAY;
                 	cpl.menustatus = MENU_NO;

                 	break;

				case SDLK_LEFT:
					create_list_set.key_change = -1;
					menuRepeatKey = SDLK_LEFT;

					break;

				case SDLK_RIGHT:
					create_list_set.key_change = 1;
					menuRepeatKey = SDLK_RIGHT;

					break;

				case SDLK_UP:
					if (create_list_set.entry_nr > 0)
						create_list_set.entry_nr--;

					menuRepeatKey = SDLK_UP;

					break;

				case SDLK_DOWN:
					create_list_set.entry_nr++;
					menuRepeatKey = SDLK_DOWN;

					break;
      		}

      		break;
   	}
}
