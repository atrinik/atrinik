/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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

/** Quick slot entries */
_quickslot quick_slots[MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS];

/** Current quickslot group */
int quickslot_group = 1;

/**
 * Quickslot positions, because some things change depending on
 * which quickslot bitmap is displayed. */
int quickslots_pos[MAX_QUICK_SLOTS][2] =
{
	{17,	1},
	{50,	1},
	{83,	1},
	{116,	1},
	{149,	1},
	{182,	1},
	{215,	1},
	{248,	1}
};

/**
 * Tell the server to set quickslot with ID 'slot' to the item with ID
 * 'tag'.
 * @param slot Quickslot ID.
 * @param tag ID of the item to set. */
static void quickslot_set_item(uint8 slot, sint32 tag)
{
	SockList sl;
	unsigned char buf[MAX_BUF];

	sl.buf = buf;
	sl.len = 0;
	SockList_AddString(&sl, "qs ");
	SockList_AddChar(&sl, CMD_QUICKSLOT_SET);
	SockList_AddChar(&sl, slot);
	SockList_AddInt(&sl, tag);
	send_socklist(sl);
}

/**
 * Tell the server to set quickslot with ID 'slot' to the spell with name
 * 'spell_name'.
 * @param slot Quickslot ID.
 * @param spell_name Name of the spell to set. */
static void quickslot_set_spell(uint8 slot, char *spell_name)
{
	SockList sl;
	unsigned char buf[MAX_BUF];

	sl.buf = buf;
	sl.len = 0;
	SockList_AddString(&sl, "qs ");
	SockList_AddChar(&sl, CMD_QUICKSLOT_SETSPELL);
	SockList_AddChar(&sl, slot);
	SockList_AddString(&sl, spell_name);
	send_socklist(sl);
}

/**
 * Tell the server to unset the specified quickslot ID.
 * @param slot Quickslot ID to unset. */
static void quickslot_unset(uint8 slot)
{
	SockList sl;
	unsigned char buf[MAX_BUF];

	sl.buf = buf;
	sl.len = 0;
	SockList_AddString(&sl, "qs ");
	SockList_AddChar(&sl, CMD_QUICKSLOT_UNSET);
	SockList_AddChar(&sl, slot);
	send_socklist(sl);
}

/* Handle quickslot key event. */
void quickslot_key(SDL_KeyboardEvent *key, int slot)
{
	int tag, real_slot = slot;
	char buf[256];

	slot = MAX_QUICK_SLOTS * quickslot_group - MAX_QUICK_SLOTS + slot;

	/* Put spell into quickslot */
	if (!key && cpl.menustatus == MENU_SPELL)
	{
		if (spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].flag == LIST_ENTRY_KNOWN)
		{
			if (quick_slots[slot].spell && quick_slots[slot].tag == spell_list_set.entry_nr)
			{
				quick_slots[slot].spell = 0;
				quick_slots[slot].tag = -1;

				quickslot_unset(slot + 1);

				snprintf(buf, sizeof(buf), "Unset F%d of group %d.", real_slot + 1, quickslot_group);
				draw_info(buf, COLOR_DGOLD);
			}
			else
			{
				quick_slots[slot].spell = 1;
				quick_slots[slot].groupNr = spell_list_set.group_nr;
				quick_slots[slot].classNr = spell_list_set.class_nr;
				quick_slots[slot].tag = spell_list_set.entry_nr;

				quickslot_set_spell(slot + 1, spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].name);

				snprintf(buf, sizeof(buf), "Set F%d of group %d to %s", real_slot + 1, quickslot_group, spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].name);
				draw_info(buf, COLOR_DGOLD);
			}
		}
	}
	/* Put item into quickslot */
	else if (key && key->keysym.mod & KMOD_SHIFT && cpl.inventory_win == IWIN_INV)
	{
		tag = cpl.win_inv_tag;

		if (tag == -1 || !locate_item(tag))
			return;

		quick_slots[slot].spell = 0;

		if (quick_slots[slot].tag == tag)
		{
			quick_slots[slot].tag = -1;
			quickslot_unset(slot + 1);
		}
		else
		{
			update_quickslots(tag);

			quick_slots[slot].tag = tag;
			quickslot_set_item(slot + 1, tag);

			snprintf(buf, sizeof(buf), "Set F%d of group %d to %s", real_slot + 1, quickslot_group, locate_item(tag)->s_name);
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
				snprintf(buf, sizeof(buf), "F%d of group %d quick apply %s", real_slot + 1, quickslot_group, locate_item(quick_slots[slot].tag)->s_name);
				draw_info(buf, COLOR_DGOLD);
				client_send_apply(quick_slots[slot].tag);
				return;
			}
		}

		snprintf(buf, sizeof(buf), "F%d of group %d quick slot is empty", real_slot + 1, quickslot_group);
		draw_info(buf, COLOR_DGOLD);
	}
}

/**
 * Get the current quickslot ID based on mouse coordinates.
 * @param x Mouse X
 * @param y Mouse Y
 * @return Quickslot ID if mouse is over it, -1 if not */
int get_quickslot(int x, int y)
{
	int i, j;
	int qsx, qsy, xoff;

	if (cur_widget[QUICKSLOT_ID]->ht > 34)
	{
		qsx = 1;
		qsy = 0;
		xoff = 0;
	}
	else
	{
		qsx = 0;
		qsy = 1;
		xoff = -17;
	}

	for (i = 0; i < MAX_QUICK_SLOTS; i++)
	{
		j = MAX_QUICK_SLOTS * quickslot_group - MAX_QUICK_SLOTS + i;

		if (x >= cur_widget[QUICKSLOT_ID]->x1 + quickslots_pos[i][qsx] + xoff && x <= cur_widget[QUICKSLOT_ID]->x1 + quickslots_pos[i][qsx] + xoff + 32 && y >= cur_widget[QUICKSLOT_ID]->y1 + quickslots_pos[i][qsy] && y <= cur_widget[QUICKSLOT_ID]->y1 + quickslots_pos[i][qsy] + 32)
			return j;
	}

	return -1;
}

/**
 * Show quickslots widget.
 * @param x X position of the quickslots
 * @param y Y position of the quickslots
 * @param vertical_quickslot Is the quickslot vertical? 1 for vertical, 0 for horizontal. */
void show_quickslots(int x, int y, int vertical_quickslot)
{
	int i, j, mx, my;
	char buf[16];
	int qsx, qsy, xoff;

	/* Figure out which bitmap to use */
	if (vertical_quickslot)
	{
		qsx = 1;
		qsy = 0;
		xoff = 0;
		sprite_blt(Bitmaps[BITMAP_QUICKSLOTSV], x, y, NULL, NULL);
	}
	else
	{
		qsx = 0;
		qsy = 1;
		xoff = -17;
		sprite_blt(Bitmaps[BITMAP_QUICKSLOTS], x, y, NULL, NULL);
	}

	SDL_GetMouseState(&mx, &my);
	update_quickslots(-1);

	/* Loop through quickslots. Do not loop through all the quickslots,
	 * like MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS. */
	for (i = 0; i < MAX_QUICK_SLOTS; i++)
	{
		/* Now calculate the real quickslot, according to the selected group */
		j = MAX_QUICK_SLOTS * quickslot_group - MAX_QUICK_SLOTS + i;

		/* If it's not empty */
		if (quick_slots[j].tag != -1)
		{
			/* Spell in quickslot */
			if (quick_slots[j].spell)
			{
				/* Output the sprite */
				sprite_blt(spell_list[quick_slots[j].groupNr].entry[quick_slots[j].classNr][quick_slots[j].tag].icon, x + quickslots_pos[i][qsx] + xoff, y + quickslots_pos[i][qsy], NULL, NULL);

				/* If mouse is over the quickslot, show a tooltip */
				if (mx >= x + quickslots_pos[i][qsx] + xoff && mx < x + quickslots_pos[i][qsx] + xoff + 33 && my >= y + quickslots_pos[i][qsy] && my < y + quickslots_pos[i][qsy] + 33)
					show_tooltip(mx, my, spell_list[quick_slots[j].groupNr].entry[quick_slots[j].classNr][quick_slots[j].tag].name);
			}
			/* Item in quickslot */
			else
			{
				item *tmp = locate_item_from_item(cpl.ob, quick_slots[j].tag);

				/* If we located the item */
				if (tmp)
				{
					/* Show it */
					blt_inv_item(tmp, x + quickslots_pos[i][qsx] + xoff, y + quickslots_pos[i][qsy], 0);

					/* And show tooltip, if mouse is over it */
					if (mx >= x + quickslots_pos[i][qsx] + xoff && mx < x + quickslots_pos[i][qsx] + xoff + 33 && my >= y + quickslots_pos[i][qsy] && my < y + quickslots_pos[i][qsy] + 33)
					{
						show_tooltip(mx, my, tmp->s_name);
					}
				}
			}
		}

		/* For each quickslot, output the F1-F8 shortcut */
		snprintf(buf, sizeof(buf), "F%d", i + 1);
		StringBlt(ScreenSurface, &Font6x3Out, buf, x + quickslots_pos[i][qsx] + xoff + 12, y + quickslots_pos[i][qsy] - 6, COLOR_DEFAULT, NULL, NULL);
	}

	snprintf(buf, sizeof(buf), "Group %d", quickslot_group);

	/* Now output the group */
	if (vertical_quickslot)
		StringBlt(ScreenSurface, &Font6x3Out, buf, x + 3, y + Bitmaps[BITMAP_QUICKSLOTSV]->bitmap->h, COLOR_DEFAULT, NULL, NULL);
	else
		StringBlt(ScreenSurface, &Font6x3Out, buf, x, y + Bitmaps[BITMAP_QUICKSLOTS]->bitmap->h, COLOR_DEFAULT, NULL, NULL);
}

/** We come here for quickslots shown during the game */
void widget_quickslots(widgetdata *widget)
{
	if (widget->ht > 34)
		show_quickslots(widget->x1, widget->y1, 1);
	else
		show_quickslots(widget->x1, widget->y1, 0);
}

/**
 * Handle mouse events over quickslots.
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param MEvent Mouse event */
void widget_quickslots_mouse_event(widgetdata *widget, int x, int y, int MEvent)
{
	/* Mouseup Event */
	if (MEvent == 1)
	{
		if (draggingInvItem(DRAG_GET_STATUS) > DRAG_IWIN_BELOW)
		{
			int ind = get_quickslot(x, y);
			char buf[MAX_BUF];

			/* Valid slot */
			if (ind != -1)
			{
				if (draggingInvItem(DRAG_GET_STATUS) == DRAG_QUICKSLOT_SPELL)
				{
					quick_slots[ind].spell = 1;
					quick_slots[ind].groupNr = quick_slots[cpl.win_quick_tag].groupNr;
					quick_slots[ind].classNr = quick_slots[cpl.win_quick_tag].classNr;
					quick_slots[ind].tag = quick_slots[cpl.win_quick_tag].spellNr;

					quickslot_set_spell(ind + 1, spell_list[spell_list_set.group_nr].entry[spell_list_set.class_nr][spell_list_set.entry_nr].name);

					cpl.win_quick_tag = -1;
				}
				else
				{
					if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_INV)
						cpl.win_quick_tag = cpl.win_inv_tag;
					else if (draggingInvItem(DRAG_GET_STATUS) == DRAG_PDOLL)
						cpl.win_quick_tag = cpl.win_pdoll_tag;

					update_quickslots(cpl.win_quick_tag);

					quick_slots[ind].tag = cpl.win_quick_tag;
					quick_slots[ind].spell = 0;

					/* Now we do some tests... First, ensure this item can fit */
					update_quickslots(-1);

					/* Now: if this is null, item is *not* in the main inventory
					 * of the player - then we can't put it in quickbar!
					 * Server will not allow apply of items in containers! */
					if (!locate_item_from_inv(cpl.ob->inv, cpl.win_quick_tag))
					{
						sound_play_effect("click_fail.ogg", 100);
						draw_info("Only items from main inventory are allowed in quickslots!", COLOR_RED);
					}
					else
					{
						/* We 'get' it in quickslots */
						sound_play_effect("get.ogg", 100);
						quickslot_set_item(ind + 1, cpl.win_quick_tag);

						snprintf(buf, sizeof(buf), "Set F%d of group %d to %s", ind + 1 - MAX_QUICK_SLOTS * quickslot_group + MAX_QUICK_SLOTS, quickslot_group, locate_item(cpl.win_quick_tag)->s_name);
						draw_info(buf, COLOR_DGOLD);
					}
				}
			}

			draggingInvItem(DRAG_NONE);
			/* ready for next item */
			itemExamined = 0;
		}
	}
	/* Mousedown Event */
	else
	{
		/* Drag from quickslots */
		int ind = get_quickslot(x, y);

		/* valid slot */
		if (ind != -1 && quick_slots[ind].tag != -1)
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
				cpl.win_inv_tag = quick_slots[ind].tag;
				process_macro_keys(KEYFUNC_APPLY, 0);
				cpl.inventory_win = stemp;
				cpl.win_inv_tag = itemp;
			}

			quickslot_unset(ind + 1);
		}
		else if (x >= widget->x1 + 266 && x <= widget->x1 + 282 && y >= widget->y1 && y <= widget->y1 + 34 && (widget->ht <= 34))
		{
			widget->wd = 34;
			widget->ht = 282;
			widget->x1 += 266;
		}
		else if (x >= widget->x1 && x <= widget->x1 + 34 && y >= widget->y1 && y <= widget->y1 + 15 && (widget->ht > 34))
		{
			widget->wd = 282;
			widget->ht = 34;
			widget->x1 -= 266;
		}
	}
}

/**
 * Update quickslots. Makes sure no items are where they shouldn't be.
 * @param del_item Item tag to remove from quickslots, -1 to not remove anything */
void update_quickslots(int del_item)
{
	int i;

	for (i = 0; i < MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS; i++)
	{
		if (quick_slots[i].tag == del_item)
			quick_slots[i].tag = -1;

		if (quick_slots[i].tag == -1)
			continue;

		/* Only items in the *main* inventory can be used with quickslots */
		if (quick_slots[i].spell == 0)
		{
			if (!locate_item_from_inv(cpl.ob->inv, quick_slots[i].tag))
				quick_slots[i].tag = -1;
		}
	}
}

/**
 * Parses data returned by the server for quickslots.
 * @param data The data to parse.
 * @param len Length of 'data'. */
void QuickSlotCmd(unsigned char *data, int len)
{
	int pos = 0;
	uint8 type, slot;

	/* Clear all quickslots. */
	for (slot = 0; slot < MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS; slot++)
	{
		quick_slots[slot].spell = 0;
		quick_slots[slot].tag = -1;
		quick_slots[slot].classNr = 0;
		quick_slots[slot].spellNr = 0;
		quick_slots[slot].groupNr = 0;
	}

	while (pos < len)
	{
		type = data[pos++];
		slot = data[pos++];

		if (type == QUICKSLOT_TYPE_ITEM)
		{
			quick_slots[slot].tag = GetInt_String(data + pos);
			pos += 4;
		}
		else if (type == QUICKSLOT_TYPE_SPELL)
		{
			char spell_name[MAX_BUF], c;
			size_t i = 0;
			int spell_group, spell_class, spell_nr;

			spell_name[0] = '\0';

			while ((c = (char) (data[pos++])))
			{
				spell_name[i++] = c;
			}

			spell_name[i] = '\0';

			if (find_spell(spell_name, &spell_group, &spell_class, &spell_nr))
			{
				quick_slots[slot].spell = 1;
				quick_slots[slot].groupNr = spell_group;
				quick_slots[slot].classNr = spell_class;
				quick_slots[slot].spellNr = quick_slots[slot].tag = spell_nr;
			}
		}
	}
}
