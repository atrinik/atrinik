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

				snprintf(buf, sizeof(buf), "qs unset %d", slot + 1);
				cs_write_string(buf, strlen(buf));

				snprintf(buf, sizeof(buf), "Unset F%d of group %d.", real_slot + 1, quickslot_group);
				draw_info(buf, COLOR_DGOLD);
			}
			else
			{
				quick_slots[slot].spell = 1;
				quick_slots[slot].groupNr = spell_list_set.group_nr;
				quick_slots[slot].classNr = spell_list_set.class_nr;
				quick_slots[slot].tag = spell_list_set.entry_nr;

				snprintf(buf, sizeof(buf), "qs setspell %d %d %d %d", slot + 1, spell_list_set.group_nr, spell_list_set.class_nr, spell_list_set.entry_nr);
				cs_write_string(buf, strlen(buf));

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
			snprintf(buf, sizeof(buf), "qs unset %d", slot + 1);
			cs_write_string(buf, strlen(buf));
		}
		else
		{
			update_quickslots(tag);

			quick_slots[slot].tag = tag;

			snprintf(buf, sizeof(buf), "qs set %d %d", slot + 1, tag);
			cs_write_string(buf, strlen(buf));

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
