/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 *  */

#include <global.h>

/** Quick slot entries */
static _quickslot quick_slots[MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS];

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
	packet_struct *packet;

	packet = packet_new(SERVER_CMD_QUICKSLOT, 32, 0);
	packet_append_uint8(packet, CMD_QUICKSLOT_SET);
	packet_append_uint8(packet, slot);
	packet_append_uint32(packet, tag);
	socket_send_packet(packet);
}

/**
 * Tell the server to set quickslot with ID 'slot' to the spell with name
 * 'spell_name'.
 * @param slot Quickslot ID.
 * @param spell_name Name of the spell to set. */
static void quickslot_set_spell(uint8 slot, char *spell_name)
{
	packet_struct *packet;

	packet = packet_new(SERVER_CMD_QUICKSLOT, 32, 0);
	packet_append_uint8(packet, CMD_QUICKSLOT_SETSPELL);
	packet_append_uint8(packet, slot);
	packet_append_string_terminated(packet, spell_name);
	socket_send_packet(packet);
}

/**
 * Tell the server to unset the specified quickslot ID.
 * @param slot Quickslot ID to unset. */
static void quickslot_unset(uint8 slot)
{
	packet_struct *packet;

	packet = packet_new(SERVER_CMD_QUICKSLOT, 32, 0);
	packet_append_uint8(packet, CMD_QUICKSLOT_UNSET);
	packet_append_uint8(packet, slot);
	socket_send_packet(packet);
}

void quickslots_init(void)
{
	size_t slot;

	/* Clear all quickslots. */
	for (slot = 0; slot < MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS; slot++)
	{
		quick_slots[slot].tag = -1;
		quick_slots[slot].spell = NULL;
	}
}

/**
 * Remove item from the quickslots by tag.
 *
 * Will also make sure all the quickslot tags still resolve to the
 * correct inventory objects.
 * @param tag Item tag to remove from quickslots. -1 not to remove
 * anything. */
static void quickslots_remove(int tag)
{
	int i;

	for (i = 0; i < MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS; i++)
	{
		if (quick_slots[i].tag == tag)
		{
			quick_slots[i].tag = -1;
		}
		else if (quick_slots[i].tag != -1 && !object_find_object_inv(cpl.ob, quick_slots[i].tag))
		{
			quick_slots[i].tag = -1;
		}
	}
}

/* Handle quickslot key event. */
void quickslots_handle_key(int slot)
{
	int real_slot;

	real_slot = slot;
	slot = MAX_QUICK_SLOTS * quickslot_group - MAX_QUICK_SLOTS + slot;

	/* Put item into quickslot */
	if (cpl.inventory_focus == MAIN_INV_ID)
	{
		object *ob;

		ob = widget_inventory_get_selected(cur_widget[MAIN_INV_ID]);

		if (!ob)
		{
			return;
		}

		quick_slots[slot].spell = NULL;

		if (quick_slots[slot].tag == ob->tag)
		{
			quick_slots[slot].tag = -1;
			quickslot_unset(slot + 1);
		}
		else
		{
			quickslots_remove(ob->tag);

			quick_slots[slot].tag = ob->tag;
			quickslot_set_item(slot + 1, ob->tag);

			draw_info_format(COLOR_DGOLD, "Set F%d of group %d to %s", real_slot + 1, quickslot_group, ob->s_name);
		}
	}
	/* Apply item or ready spell */
	else
	{
		if (quick_slots[slot].spell)
		{
			fire_mode_tab[FIRE_MODE_SPELL].spell = quick_slots[slot].spell;
			RangeFireMode = FIRE_MODE_SPELL;
		}
		else if (quick_slots[slot].tag != -1 && object_find(quick_slots[slot].tag))
		{
			draw_info_format(COLOR_DGOLD, "F%d of group %d quick apply %s", real_slot + 1, quickslot_group, object_find(quick_slots[slot].tag)->s_name);

			client_send_apply(quick_slots[slot].tag);
		}
		else
		{
			draw_info_format(COLOR_DGOLD, "F%d of group %d quick slot is empty", real_slot + 1, quickslot_group);
		}
	}
}

/**
 * Get the current quickslot ID based on mouse coordinates.
 * @param x Mouse X.
 * @param y Mouse Y.
 * @return Quickslot ID if mouse is over it, -1 if not. */
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
		{
			return j;
		}
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
	int i, j;
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

	quickslots_remove(-1);

	/* Loop through quickslots. Do not loop through all the quickslots,
	 * like MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS. */
	for (i = 0; i < MAX_QUICK_SLOTS; i++)
	{
		/* Now calculate the real quickslot, according to the selected group */
		j = MAX_QUICK_SLOTS * quickslot_group - MAX_QUICK_SLOTS + i;

		/* Spell in quickslot */
		if (quick_slots[j].spell)
		{
			/* Output the sprite */
			blit_face(quick_slots[j].spell->icon, x + quickslots_pos[i][qsx] + xoff, y + quickslots_pos[i][qsy]);
		}
		/* Item in quickslot */
		else if (quick_slots[j].tag != -1)
		{
			object *tmp = object_find_object(cpl.ob, quick_slots[j].tag);

			/* If we located the item */
			if (tmp)
			{
				/* Show it */
				object_blit_inventory(tmp, x + quickslots_pos[i][qsx] + xoff, y + quickslots_pos[i][qsy]);
			}
		}

		/* For each quickslot, output the F1-F8 shortcut */
		snprintf(buf, sizeof(buf), "F%d", i + 1);
		string_blt(ScreenSurface, FONT_ARIAL10, buf, x + quickslots_pos[i][qsx] + xoff + 12, y + quickslots_pos[i][qsy] - 7, COLOR_WHITE, TEXT_OUTLINE, NULL);
	}

	snprintf(buf, sizeof(buf), "Group %d", quickslot_group);

	/* Now output the group */
	if (vertical_quickslot)
	{
		string_blt(ScreenSurface, FONT_ARIAL10, buf, x - 1, y + Bitmaps[BITMAP_QUICKSLOTSV]->bitmap->h, COLOR_WHITE, TEXT_OUTLINE, NULL);
	}
	else
	{
		string_blt(ScreenSurface, FONT_ARIAL10, buf, x, y + Bitmaps[BITMAP_QUICKSLOTS]->bitmap->h, COLOR_WHITE, TEXT_OUTLINE, NULL);
	}
}

/** We come here for quickslots shown during the game */
void widget_quickslots(widgetdata *widget)
{
	if (widget->ht > 34)
	{
		show_quickslots(widget->x1, widget->y1, 1);
	}
	else
	{
		show_quickslots(widget->x1, widget->y1, 0);
	}
}

/**
 * Handle mouse events over quickslots.
 * @param widget The quickslots widget.
 * @param event The event to handle. */
void widget_quickslots_mouse_event(widgetdata *widget, SDL_Event *event)
{
	if (event->type == SDL_MOUSEBUTTONUP)
	{
		int i;

		i = get_quickslot(event->motion.x, event->motion.y);

		/* Valid slot */
		if (i != -1)
		{
			if (draggingInvItem(DRAG_GET_STATUS) == DRAG_QUICKSLOT_SPELL)
			{
				quick_slots[i].tag = -1;
				quick_slots[i].spell = cpl.dragging.spell;
				quickslot_set_spell(i + 1, quick_slots[i].spell->name);
				cpl.dragging.spell = NULL;
			}
			else if (draggingInvItem(DRAG_GET_STATUS) == DRAG_QUICKSLOT)
			{
				quickslots_remove(cpl.dragging.tag);
				quick_slots[i].tag = cpl.dragging.tag;
				quick_slots[i].spell = NULL;

				if (!object_find_object_inv(cpl.ob, cpl.dragging.tag))
				{
					draw_info(COLOR_RED, "Only items from main inventory are allowed in quickslots.");
				}
				else
				{
					quickslot_set_item(i + 1, cpl.dragging.tag);
					draw_info_format(COLOR_DGOLD, "Set F%d of group %d to %s", i + 1 - MAX_QUICK_SLOTS * quickslot_group + MAX_QUICK_SLOTS, quickslot_group, object_find(cpl.dragging.tag)->s_name);
				}
			}
		}

		draggingInvItem(DRAG_NONE);
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		int i = get_quickslot(event->motion.x, event->motion.y);

		if (i != -1)
		{
			if (event->button.button == SDL_BUTTON_LEFT)
			{
				if (quick_slots[i].spell)
				{
					cpl.dragging.spell = quick_slots[i].spell;
					draggingInvItem(DRAG_QUICKSLOT_SPELL);
					quick_slots[i].spell = NULL;
				}
				else if (quick_slots[i].tag != -1)
				{
					cpl.dragging.tag = quick_slots[i].tag;
					draggingInvItem(DRAG_QUICKSLOT);
					quick_slots[i].tag = -1;
				}

				quickslot_unset(i + 1);
			}
			else
			{
				draw_info_format(COLOR_DGOLD, "apply %s", object_find(quick_slots[i].tag)->s_name);
				client_send_apply(quick_slots[i].tag);
			}
		}
		else if (event->motion.x >= widget->x1 + 266 && event->motion.x <= widget->x1 + 282 && event->motion.y >= widget->y1 && event->motion.y <= widget->y1 + 34 && (widget->ht <= 34))
		{
			widget->wd = 34;
			widget->ht = 282;
			widget->x1 += 266;
		}
		else if (event->motion.x >= widget->x1 && event->motion.x <= widget->x1 + 34 && event->motion.y >= widget->y1 && event->motion.y <= widget->y1 + 15 && (widget->ht > 34))
		{
			widget->wd = 282;
			widget->ht = 34;
			widget->x1 -= 266;
		}
	}
	else if (event->type == SDL_MOUSEMOTION)
	{
		int i, j, qsx, qsy, xoff;

		if (widget->ht > 34)
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
			/* Now calculate the real quickslot, according to the selected group */
			j = MAX_QUICK_SLOTS * quickslot_group - MAX_QUICK_SLOTS + i;

			if (event->motion.x >= widget->x1 + quickslots_pos[i][qsx] + xoff && event->motion.x < widget->x1 + quickslots_pos[i][qsx] + xoff + 33 && event->motion.y >= widget->y1 + quickslots_pos[i][qsy] && event->motion.y < widget->y1 + quickslots_pos[i][qsy] + 33)
			{
				if (quick_slots[j].spell)
				{
					tooltip_create(event->motion.x, event->motion.y, FONT_ARIAL10, quick_slots[j].spell->name);
				}
				else if (quick_slots[j].tag != -1)
				{
					object *tmp = object_find_object(cpl.ob, quick_slots[j].tag);

					/* If we located the item */
					if (tmp)
					{
						tooltip_create(event->motion.x, event->motion.y, FONT_ARIAL10, tmp->s_name);
					}
				}
			}
		}
	}
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_quickslots(uint8 *data, size_t len, size_t pos)
{
	uint8 type, slot;

	/* Clear all quickslots. */
	for (slot = 0; slot < MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS; slot++)
	{
		quick_slots[slot].tag = -1;
		quick_slots[slot].spell = NULL;
	}

	while (pos < len)
	{
		type = packet_to_uint8(data, len, &pos);
		slot = packet_to_uint8(data, len, &pos);

		if (type == QUICKSLOT_TYPE_ITEM)
		{
			quick_slots[slot].tag = packet_to_uint32(data, len, &pos);
		}
		else if (type == QUICKSLOT_TYPE_SPELL)
		{
			char spell_name[MAX_BUF];
			size_t spell_path, spell_id;

			packet_to_string(data, len, &pos, spell_name, sizeof(spell_name));

			if (spell_find(spell_name, &spell_path, &spell_id))
			{
				quick_slots[slot].spell = spell_get(spell_path, spell_id);
			}
		}
	}
}
