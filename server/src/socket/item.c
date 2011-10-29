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
 * This contains item logic for client/server. It doesn't contain the
 * actual commands that send the data, but does contain the logic for
 * what items should be sent. */

#include <global.h>

static int check_container(object *pl, object *con);

/** This is the maximum number of bytes we expect any item to take up. */
#define MAXITEMLEN 300

/**
 * This is a similar to query_name, but returns flags to be sent to
 * client.
 * @param op Object to query the flags for.
 * @return Flags. */
unsigned int query_flags(object *op)
{
	unsigned int flags = 0;

	if (QUERY_FLAG(op, FLAG_APPLIED))
	{
		switch (op->type)
		{
			case BOW:
			case WAND:
			case ROD:
			case HORN:
				flags = a_readied;
				break;

			case WEAPON:
				flags = a_wielded;
				break;

			case SKILL:
			case ARMOUR:
			case HELMET:
			case SHIELD:
			case RING:
			case BOOTS:
			case GLOVES:
			case AMULET:
			case GIRDLE:
			case BRACERS:
			case CLOAK:
				flags = a_worn;
				break;

			case CONTAINER:
				flags = a_active;
				break;

			default:
				flags = a_applied;
				break;
		}
	}

	if (op->type == CONTAINER && (op->attacked_by || (!op->env && QUERY_FLAG(op, FLAG_APPLIED))))
	{
		flags |= F_OPEN;
	}

	if (QUERY_FLAG(op, FLAG_IS_TRAPPED))
	{
		flags |= F_TRAPPED;
	}

	if (QUERY_FLAG(op, FLAG_IDENTIFIED) || QUERY_FLAG(op, FLAG_APPLIED))
	{
		if (QUERY_FLAG(op, FLAG_DAMNED))
		{
			flags |= F_DAMNED;
		}
		else if (QUERY_FLAG(op, FLAG_CURSED))
		{
			flags |= F_CURSED;
		}
	}

	if (QUERY_FLAG(op, FLAG_IS_MAGICAL) && QUERY_FLAG(op, FLAG_IDENTIFIED))
	{
		flags |= F_MAGIC;
	}

	if (QUERY_FLAG(op, FLAG_UNPAID))
	{
		flags |= F_UNPAID;
	}

	if (QUERY_FLAG(op, FLAG_INV_LOCKED))
	{
		flags |= F_LOCKED;
	}

	if (QUERY_FLAG(op, FLAG_IS_INVISIBLE))
	{
		flags |= F_INVISIBLE;
	}

	if (QUERY_FLAG(op, FLAG_IS_ETHEREAL))
	{
		flags |= F_ETHEREAL;
	}

	if (QUERY_FLAG(op, FLAG_NO_PICK))
	{
		flags |= F_NOPICK;
	}

	return flags;
}

/**
 * Add data about object to a SockList instance.
 * @param sl SockList instance to add to.
 * @param op Object to add information about.
 * @param pl Player that will receive the data.
 * @param flags Combination of @ref UPD_XXX. */
static void add_object_to_socklist(SockList *sl, object *op, object *pl, uint32 flags)
{
	SockList_AddInt(sl, op->count);

	if (flags & UPD_LOCATION)
	{
		SockList_AddInt(sl, op->env ? op->env->count : 0);
	}

	if (flags & UPD_FLAGS)
	{
		SockList_AddInt(sl, query_flags(op));
	}

	if (flags & UPD_WEIGHT)
	{
		SockList_AddInt(sl, WEIGHT(op));
	}

	if (flags & UPD_FACE)
	{
		if (op->inv_face && QUERY_FLAG(op, FLAG_IDENTIFIED))
		{
			SockList_AddInt(sl, op->inv_face->number);
		}
		else
		{
			SockList_AddInt(sl, op->face->number);
		}
	}

	if (flags & UPD_DIRECTION)
	{
		SockList_AddChar(sl, op->facing);
	}

	if (flags & UPD_TYPE)
	{
		SockList_AddChar(sl, op->type);
		SockList_AddChar(sl, op->sub_type);

		if (QUERY_FLAG(op, FLAG_IDENTIFIED))
		{
			SockList_AddChar(sl, op->item_quality);
			SockList_AddChar(sl, op->item_condition);
			SockList_AddChar(sl, op->item_level);
			SockList_AddChar(sl, op->item_skill);
		}
		else
		{
			SockList_AddChar(sl, (char) 255);
			SockList_AddChar(sl, (char) 255);
			SockList_AddChar(sl, (char) 255);
			SockList_AddChar(sl, (char) 255);
		}
	}

	if (flags & UPD_NAME)
	{
		size_t len;
		char item_name[MAX_BUF];

		if (op->custom_name)
		{
			memcpy(item_name, op->custom_name, 127);
		}
		else
		{
			memcpy(item_name, query_base_name(op, pl), 127);
		}

		item_name[127] = '\0';
		len = strlen(item_name);
		SockList_AddLen8Data(sl, item_name, len);
	}

	if (flags & UPD_ANIM)
	{
		if (!(flags & UPD_ANIM_NO_INV) && op->inv_animation_id)
		{
			SockList_AddShort(sl, op->inv_animation_id);
		}
		else
		{
			SockList_AddShort(sl, op->animation_id);
		}
	}

	if (flags & UPD_ANIMSPEED)
	{
		int anim_speed = 0;

		if (QUERY_FLAG(op, FLAG_ANIMATE))
		{
			if (op->anim_speed)
			{
				anim_speed = op->anim_speed;
			}
			else
			{
				if (FABS(op->speed) < 0.001)
				{
					anim_speed = 255;
				}
				else if (FABS(op->speed) >= 1.0)
				{
					anim_speed = 1;
				}
				else
				{
					anim_speed = (int) (1.0 / FABS(op->speed));
				}
			}

			if (anim_speed > 255)
			{
				anim_speed = 255;
			}
		}

		SockList_AddChar(sl, (char) anim_speed);
	}

	if (flags & UPD_NROF)
	{
		SockList_AddInt(sl, op->nrof);
	}
}

/**
 * Recursively draw inventory of an object for DMs.
 * @param pl DM.
 * @param sl SockList instance to append to.
 * @param op Object of which inventory is going to be sent.
 * @return Number of items sent. */
static int esrv_draw_look_rec(object *pl, SockList *sl, object *op)
{
	char buf[MAX_BUF];
	object *tmp;
	int got_one = 0;

	SockList_AddInt(sl, 0);
	SockList_AddInt(sl, 0);
	SockList_AddInt(sl, -1);
	SockList_AddInt(sl, (uint32) blank_face->number);
	SockList_AddChar(sl, 0);
	strncpy(buf, "in inventory", sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	SockList_AddLen8Data(sl, buf, MIN(strlen(buf), 255));
	SockList_AddShort(sl, 0);
	SockList_AddChar(sl, 0);
	SockList_AddInt(sl, 0);

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		add_object_to_socklist(sl, HEAD(tmp), pl, UPD_FLAGS | UPD_WEIGHT | UPD_FACE | UPD_DIRECTION | UPD_NAME | UPD_ANIM | UPD_ANIM_NO_INV | UPD_ANIMSPEED | UPD_NROF);
		got_one++;

		if (sl->len > (MAXSOCKBUF - MAXITEMLEN))
		{
			Send_With_Handling(&CONTR(pl)->socket, sl);
			SOCKET_SET_BINARY_CMD(sl, BINARY_CMD_ITEMY);
			SockList_AddInt(sl, -2);
			SockList_AddInt(sl, 0);
			got_one = 0;
		}

		if (tmp->inv && tmp->type != PLAYER)
		{
			got_one = esrv_draw_look_rec(pl, sl, tmp);
		}
	}

	SockList_AddInt(sl, 0);
	SockList_AddInt(sl, 0);
	SockList_AddInt(sl, -1);
	SockList_AddInt(sl, (uint32) blank_face->number);
	SockList_AddChar(sl, 0);
	strncpy(buf, "end inventory", sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	SockList_AddLen8Data(sl, buf, MIN(strlen(buf), 255));
	SockList_AddShort(sl, 0);
	SockList_AddChar(sl, 0);
	SockList_AddInt(sl, 0);
	return got_one;
}

/**
 * Draw the look window. Don't need to do animations here.
 *
 * This sends all the faces to the client, not just updates. This is
 * because object ordering would otherwise be inconsistent.
 * @param pl Player to draw the look window for. */
void esrv_draw_look(object *pl)
{
	socket_struct *ns = &CONTR(pl)->socket;
	char buf[MAX_BUF];
	object *tmp, *last;
	int got_one = 0, start_look = 0, end_look = 0, wiz;
	SockList sl;

	if (QUERY_FLAG(pl, FLAG_REMOVED) || pl->map == NULL || pl->map->in_memory != MAP_IN_MEMORY || OUT_OF_MAP(pl->map, pl->x, pl->y))
	{
		return;
	}

	wiz = QUERY_FLAG(pl, FLAG_WIZ);
	/* Grab last (top) object without browsing the objects. */
	tmp = GET_MAP_OB_LAST(pl->map, pl->x, pl->y);

	sl.buf = malloc(MAXSOCKBUF);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMY);

	SockList_AddInt(&sl, 0);
	SockList_AddInt(&sl, 0);

	if (CONTR(pl)->socket.look_position)
	{
		SockList_AddInt(&sl, 0x80000000 | (CONTR(pl)->socket.look_position - NUM_LOOK_OBJECTS));
		SockList_AddInt(&sl, 0);
		SockList_AddInt(&sl, -1);
		SockList_AddInt(&sl, prev_item_face->number);
		SockList_AddChar(&sl, 0);
		snprintf(buf, sizeof(buf), "Apply to see %d previous items", NUM_LOOK_OBJECTS);
		SockList_AddLen8Data(&sl, buf, MIN(strlen(buf), 255));
		SockList_AddShort(&sl, 0);
		SockList_AddChar(&sl, 0);
		SockList_AddInt(&sl, 0);
	}

	for (last = NULL; tmp != last; tmp = tmp->below)
	{
		if (tmp == pl)
		{
			continue;
		}

		/* Skip map mask, sys_objects and invisible objects when we can't
		 * see them. */
		if (tmp->layer <= LAYER_FMASK || IS_SYS_INVISIBLE(tmp) || (!QUERY_FLAG(pl, FLAG_SEE_INVISIBLE) && QUERY_FLAG(tmp, FLAG_IS_INVISIBLE)))
		{
			/* But only when we are not a DM */
			if (!QUERY_FLAG(pl, FLAG_WIZ))
			{
				continue;
			}
		}

		if (++start_look < CONTR(pl)->socket.look_position)
		{
			continue;
		}

		/* If we have too many items to send, send a 'next group' object
		 * and leave here. */
		if (++end_look > NUM_LOOK_OBJECTS)
		{
			SockList_AddInt(&sl, 0x80000000 | (CONTR(pl)->socket.look_position + NUM_LOOK_OBJECTS));
			SockList_AddInt(&sl, 0);
			SockList_AddInt(&sl, -1);
			SockList_AddInt(&sl, next_item_face->number);
			SockList_AddChar(&sl, 0);
			snprintf(buf, sizeof(buf), "Apply to see next group of items");
			SockList_AddLen8Data(&sl, buf, MIN(strlen(buf), 255));
			SockList_AddShort(&sl, 0);
			SockList_AddChar(&sl, 0);
			SockList_AddInt(&sl, 0);
			break;
		}

		add_object_to_socklist(&sl, HEAD(tmp), pl, UPD_FLAGS | UPD_WEIGHT | UPD_FACE | UPD_DIRECTION | UPD_NAME | UPD_ANIM | UPD_ANIM_NO_INV | UPD_ANIMSPEED | UPD_NROF);
		got_one++;

		if (sl.len > (MAXSOCKBUF - MAXITEMLEN))
		{
			Send_With_Handling(ns, &sl);
			SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMY);
			/* do no delinv */
			SockList_AddInt(&sl, -2);
			SockList_AddInt(&sl, 0);
			got_one = 0;
		}

		if (wiz && tmp->inv && tmp->type != PLAYER)
		{
			got_one = esrv_draw_look_rec(pl, &sl, tmp);
		}
	}

	if (got_one || (!got_one && !ns->below_clear))
	{
		Send_With_Handling(ns, &sl);
		ns->below_clear = 0;
	}

	free(sl.buf);
}

/**
 * Close a container.
 * @param op Player to close container of. */
void esrv_close_container(object *op)
{
	SockList sl;

	sl.buf = malloc(256);
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMX);
	/* Container mode flag */
	SockList_AddInt(&sl, -1);
	SockList_AddInt(&sl, -1);

	Send_With_Handling(&CONTR(op)->socket, &sl);
	free(sl.buf);
}

/**
 * Sends a whole inventory of an object.
 * @param pl Player to send the inventory to.
 * @param op Object to send inventory of. Can be same as 'pl' to send
 * inventory of the player. */
void esrv_send_inventory(object *pl, object *op)
{
	object *tmp;
	int got_one = 0;
	SockList sl;

	sl.buf = malloc(MAXSOCKBUF);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMY);

	/* In this case we're sending a container inventory */
	if (pl != op)
	{
		/* Container mode flag */
		SockList_AddInt(&sl, -1);
	}
	else
	{
		SockList_AddInt(&sl, op->count);
	}

	SockList_AddInt(&sl, op->count);

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (!QUERY_FLAG(pl, FLAG_SEE_INVISIBLE) && QUERY_FLAG(tmp, FLAG_IS_INVISIBLE))
		{
			/* Skip this for DMs */
			if (!QUERY_FLAG(pl, FLAG_WIZ))
			{
				continue;
			}
		}

		if (LOOK_OBJ(tmp) || QUERY_FLAG(pl, FLAG_WIZ))
		{
			add_object_to_socklist(&sl, tmp, pl, UPD_FLAGS | UPD_WEIGHT | UPD_FACE | UPD_DIRECTION | UPD_TYPE | UPD_NAME | UPD_ANIM | UPD_ANIMSPEED | UPD_NROF);
			got_one++;

			/* It is possible for players to accumulate a huge amount of
			 * items (especially with some of the bags out there) to
			 * overflow the buffer. If so, send multiple item1
			 * commands. */
			if (sl.len > (MAXSOCKBUF - MAXITEMLEN))
			{
				Send_With_Handling(&CONTR(pl)->socket, &sl);
				SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMY);
				/* no del inv */
				SockList_AddInt(&sl, -3);
				SockList_AddInt(&sl, op->count);
				got_one = 0;
			}
		}
	}

	/* Container can be empty */
	if (got_one || pl != op)
	{
		Send_With_Handling(&CONTR(pl)->socket, &sl);
	}

	free(sl.buf);
}

/**
 * Informs client about new object data.
 * @param flags List of values to update.
 * @param pl The player.
 * @param op The object to update. */
static void esrv_update_item_send(int flags, object *pl, object *op)
{
	SockList sl;

	if (!CONTR(pl))
	{
		return;
	}

	/* Can't see that item... */
	if (IS_INVISIBLE(op, pl))
	{
		return;
	}

	sl.buf = malloc(MAXSOCKBUF);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_UPITEM);
	SockList_AddShort(&sl, (uint16) flags);
	add_object_to_socklist(&sl, op, pl, flags);
	Send_With_Handling(&CONTR(pl)->socket, &sl);
	free(sl.buf);
}

/**
 * Updates specified data about the specified object for all involved
 * clients.
 * @param flags List of values to update.
 * @param op The object to update. */
void esrv_update_item(int flags, object *op)
{
	if (op->type == PLAYER)
	{
		esrv_update_item_send(flags, op, op);
	}
	else if (op->env)
	{
		if (op->env->type == CONTAINER)
		{
			object *tmp;

			for (tmp = op->env->attacked_by; tmp; tmp = CONTR(tmp)->container_above)
			{
				esrv_update_item_send(flags, tmp, op);
			}
		}
		else if (op->env->type == PLAYER)
		{
			esrv_update_item_send(flags, op->env, op);
		}
	}
}

/**
 * Informs a client about the specified object.
 * @param pl The player.
 * @param op Object to send information of. */
static void esrv_send_item_send(object *pl, object *op)
{
	SockList sl;

	if (!CONTR(pl))
	{
		return;
	}

	/* Can't see that item... */
	if (IS_INVISIBLE(op, pl))
	{
		return;
	}

	sl.buf = malloc(MAXSOCKBUF);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMX);
	SockList_AddInt(&sl, -4);
	SockList_AddInt(&sl, op->env->count);
	add_object_to_socklist(&sl, HEAD(op), pl, UPD_FLAGS | UPD_WEIGHT | UPD_FACE | UPD_DIRECTION | UPD_TYPE | UPD_NAME | UPD_ANIM | UPD_ANIMSPEED | UPD_NROF);

	Send_With_Handling(&CONTR(pl)->socket, &sl);
	free(sl.buf);
}

/**
 * Informs all involved clients about the specified object.
 * @param op Object to send information of. */
void esrv_send_item(object *op)
{
	object *tmp;

	/* No object or object is not in inventory, nothing to do here. */
	if (!op || !op->env)
	{
		return;
	}

	if (op->env->type == CONTAINER)
	{
		/* Send the item information to all players that are looking
		 * inside this container. */
		for (tmp = op->env->attacked_by; tmp; tmp = CONTR(tmp)->container_above)
		{
			esrv_send_item_send(tmp, op);
		}
	}
	else if (op->env->type == PLAYER)
	{
		esrv_send_item_send(op->env, op);
	}
}

/**
 * Informs a client about item deletion.
 * @param pl Player.
 * @param op The item that was deleted. */
static void esrv_del_item_send(object *pl, object *op)
{
	SockList sl;

	if (!CONTR(pl))
	{
		return;
	}

	/* Can't see that item... */
	if (IS_INVISIBLE(op, pl))
	{
		return;
	}

	sl.buf = malloc(MAXSOCKBUF);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_DELITEM);
	SockList_AddInt(&sl, op->count);

	Send_With_Handling(&CONTR(pl)->socket, &sl);
	free(sl.buf);
}

/**
 * Informs involved clients about item deletion.
 * @param op The item that was deleted. */
void esrv_del_item(object *op)
{
	/* No object or object is not inside an inventory, nothing to do. */
	if (!op || !op->env)
	{
		return;
	}

	if (op->env->type == CONTAINER)
	{
		object *tmp;

		for (tmp = op->env->attacked_by; tmp; tmp = CONTR(tmp)->container_above)
		{
			esrv_del_item_send(tmp, op);
		}
	}
	else if (op->env->type == PLAYER)
	{
		esrv_del_item_send(op->env, op);
	}
}

/**
 * Recursive part of esrv_get_ob_from_count(). */
static object *get_ob_from_count_rec(object *pl, object *where, tag_t count)
{
	object *tmp, *head, *tmp2;

	for (tmp = where; tmp; tmp = tmp->below)
	{
		head = HEAD(tmp);

		if (head->count == count)
		{
			return head;
		}
		else if (head->inv && (QUERY_FLAG(pl, FLAG_WIZ) || (head->type == CONTAINER && CONTR(pl)->container == head)))
		{
			tmp2 = get_ob_from_count_rec(pl, head->inv, count);

			if (tmp2)
			{
				return tmp2;
			}
		}
	}

	return NULL;
}

/**
 * Looks for an object player's client requested in player's inventory
 * and below player.
 * @param pl Player.
 * @param count ID of the object to look for.
 * @return The found object, NULL if it can't be found. */
object *esrv_get_ob_from_count(object *pl, tag_t count)
{
	object *tmp;

	if (pl->count == count)
	{
		return pl;
	}

	tmp = get_ob_from_count_rec(pl, pl->inv, count);

	if (tmp)
	{
		return tmp;
	}

	tmp = get_ob_from_count_rec(pl, GET_MAP_OB_LAST(pl->map, pl->x, pl->y), count);

	if (tmp)
	{
		return tmp;
	}

	return NULL;
}

/**
 * Client wants to examine some object.
 * @param buf Buffer, contains ID of the object to examine.
 * @param len Unused.
 * @param pl Player. */
void ExamineCmd(char *buf, int len, player *pl)
{
	long tag;
	object *op;

	if (!buf || !len)
	{
		return;
	}

	tag = atoi(buf);
	op = esrv_get_ob_from_count(pl->ob, tag);

	if (!op)
	{
		return;
	}

	examine(pl->ob, op, NULL);
}

/**
 * Remove any quickslots of player 'pl' matching slot 'slot'.
 * @param slot ID of the quickslot to look for.
 * @param pl Inside which player to search in. */
static void remove_quickslot(uint8 slot, player *pl)
{
	object *tmp;

	if (pl->spell_quickslots[slot - 1] != SP_NO_SPELL)
	{
		pl->spell_quickslots[slot - 1] = SP_NO_SPELL;
		return;
	}

	for (tmp = pl->ob->inv; tmp; tmp = tmp->below)
	{
		if (tmp->quickslot && tmp->quickslot == slot)
		{
			tmp->quickslot = 0;
		}
	}
}

/**
 * Send quickslots to player.
 * @param pl Player to send the quickslots to. */
void send_quickslots(player *pl)
{
	SockList sl;
	unsigned char buf[MAXSOCKBUF];
	object *tmp;
	uint8 i;

	sl.buf = buf;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_QUICKSLOT);

	for (tmp = pl->ob->inv; tmp; tmp = tmp->below)
	{
		if (tmp->quickslot)
		{
			SockList_AddChar(&sl, QUICKSLOT_TYPE_ITEM);
			SockList_AddChar(&sl, tmp->quickslot - 1);
			SockList_AddInt(&sl, tmp->count);
		}
	}

	for (i = 0; i < MAX_QUICKSLOT; i++)
	{
		if (pl->spell_quickslots[i] != SP_NO_SPELL)
		{
			SockList_AddChar(&sl, QUICKSLOT_TYPE_SPELL);
			SockList_AddChar(&sl, i);
			SockList_AddString(&sl, spells[pl->spell_quickslots[i]].name);
		}
	}

	Send_With_Handling(&pl->socket, &sl);
}

/**
 * Quick slot command.
 * @param buf Data.
 * @param len Length of 'buf'.
 * @param pl Player. */
void QuickSlotCmd(uint8 *buf, int len, player *pl)
{
	uint8 command, quickslot;

	if (!buf || len < 2)
	{
		return;
	}

	command = buf[0];
	quickslot = buf[1];

	if (quickslot < 1 || quickslot > MAX_QUICKSLOT)
	{
		return;
	}

	if (command == CMD_QUICKSLOT_SET)
	{
		tag_t tag;
		object *op;

		if (len < 6)
		{
			return;
		}

		tag = GetInt_String(buf + 2);
		op = esrv_get_ob_from_count(pl->ob, tag);

		if (!op)
		{
			return;
		}

		remove_quickslot(quickslot, pl);
		op->quickslot = quickslot;
	}
	else if (command == CMD_QUICKSLOT_SETSPELL)
	{
		sint16 spell_id;

		/* Assumes that all spells have at least 2 letters. */
		if (len < 4)
		{
			return;
		}

		spell_id = look_up_spell_name((char *) buf + 2);

		if (spell_id == SP_NO_SPELL)
		{
			return;
		}

		remove_quickslot(quickslot, pl);
		pl->spell_quickslots[quickslot - 1] = spell_id;
	}
	else if (command == CMD_QUICKSLOT_UNSET)
	{
		remove_quickslot(quickslot, pl);
	}
	else
	{
		LOG(llevSystem, "Client %s@%s sent invalid quickslot command.\n", pl->ob->name, pl->socket.host);
		pl->socket.status = Ns_Dead;
	}
}

/**
 * Client wants to apply an object.
 * @param buf Buffer, contains ID of the object to apply.
 * @param len Unused.
 * @param pl Player. */
void ApplyCmd(char *buf, int len, player *pl)
{
	uint32 tag;
	object *op;

	if (!buf || !len)
	{
		return;
	}

	tag = atoi(buf);
	op = esrv_get_ob_from_count(pl->ob, tag);

	if (QUERY_FLAG(pl->ob, FLAG_REMOVED))
	{
		return;
	}

	/* If the high bit is set, player applied a pseudo object. */
	if (tag & 0x80000000)
	{
		pl->socket.look_position = tag & 0x7fffffff;
		pl->socket.update_tile = 0;
		return;
	}

	if (!op)
	{
		return;
	}

	player_apply(pl->ob, op, 0, 0);
}

/**
 * Client wants to lock an object.
 * @param data Buffer, contains ID of the object to lock.
 * @param len Unused.
 * @param pl Player. */
void LockItem(uint8 *data, int len, player *pl)
{
	int flag, tag;
	object *op;

	if (!data || !len)
	{
		return;
	}

	flag = data[0];
	tag = GetInt_String(data + 1);
	op = esrv_get_ob_from_count(pl->ob, tag);

	/* Can happen as result of latency or client/server async. */
	if (!op)
	{
		return;
	}

	/* Only lock item inside the player's own inventory */
	if (is_player_inv(op) != pl->ob)
	{
		draw_info(COLOR_WHITE, pl->ob, "You can't lock items outside your inventory!");
		return;
	}

	if (!flag)
	{
		CLEAR_FLAG(op, FLAG_INV_LOCKED);
	}
	else
	{
		SET_FLAG(op, FLAG_INV_LOCKED);
	}

	esrv_update_item(UPD_FLAGS, op);
}

/**
 * Client wants to mark an object.
 * @param data Buffer, contains ID of the object to mark.
 * @param len Unused.
 * @param pl Player. */
void MarkItem(uint8 *data, int len, player *pl)
{
	int tag;
	object *op;

	if (!data || !len)
	{
		return;
	}

	tag = GetInt_String(data);
	op = esrv_get_ob_from_count(pl->ob, tag);

	if (!op)
	{
		return;
	}

	if (pl->mark_count == op->count)
	{
		draw_info_format(COLOR_WHITE, pl->ob, "Unmarked item %s.", query_name(op, NULL));
		pl->mark = NULL;
		pl->mark_count = -1;
	}
	else
	{
		draw_info_format(COLOR_WHITE, pl->ob, "Marked item %s.", query_name(op, NULL));
		pl->mark_count = op->count;
		pl->mark = op;
	}
}

/**
 * Move an object to a new location.
 * @param pl Player.
 * @param to ID of the object to move the object. If 0, it's on ground.
 * @param tag ID of the object to drop.
 * @param nrof How many objects to drop. */
void esrv_move_object(object *pl, tag_t to, tag_t tag, long nrof)
{
	object *op, *env;
	int tmp;

	op = esrv_get_ob_from_count(pl, tag);

	if (!op)
	{
		return;
	}

	/* drop it to the ground */
	if (!to)
	{
		if (op->map && !op->env)
		{
			return;
		}

		CLEAR_FLAG(pl, FLAG_INV_LOCKED);

		if ((tmp = check_container(pl, op)))
		{
			draw_info(COLOR_WHITE, pl, "First remove all god-given items from this container!");
		}
		else if (QUERY_FLAG(pl, FLAG_INV_LOCKED))
		{
			draw_info(COLOR_WHITE, pl, "You can't drop a container with locked items inside!");
		}
		else
		{
			drop_object(pl, op, nrof, 0);
		}

		CLEAR_FLAG(pl, FLAG_INV_LOCKED);

		return;
	}
	/* Pick it up to the inventory */
	else if (to == pl->count || (to == op->count && !op->env))
	{
		/* Return if player has already picked it up */
		if (op->env == pl)
		{
			return;
		}

		CONTR(pl)->count = nrof;
		/* It goes in player inv or readied container */
		pick_up(pl, op, 0);
		return;
	}

	/* If not dropped or picked up, we are putting it into a sack */
	env = esrv_get_ob_from_count(pl, to);

	if (!env)
	{
		return;
	}

	/* put_object_in_sack() presumes that necessary sanity checking has
	 * already been done (eg, it can be picked up and fits in in a sack,
	 * so check for those things. We should also check and make sure env
	 * is in fact a container for that matter. */
	if (env->type == CONTAINER && can_pick(pl, op) && sack_can_hold(pl, env, op, nrof))
	{
		CLEAR_FLAG(pl, FLAG_INV_LOCKED);
		tmp = check_container(pl, op);

		if (QUERY_FLAG(pl, FLAG_INV_LOCKED) && env->env != pl)
		{
			draw_info(COLOR_WHITE, pl, "You can't drop a container with locked items inside!");
		}
		else if (tmp && env->env != pl)
		{
			draw_info(COLOR_WHITE, pl, "First remove all god-given items from this container!");
		}
		else if (QUERY_FLAG(op, FLAG_STARTEQUIP) && env->env != pl)
		{
			draw_info(COLOR_WHITE, pl, "You can't store god-given items outside your inventory!");
		}
		else
		{
			put_object_in_sack(pl, env, op, nrof);
		}

		CLEAR_FLAG(pl, FLAG_INV_LOCKED);

		return;
	}
}

/**
 * Check if container can be dropped.
 *
 * Locked or FLAG_STARTEQUIP items cannot be dropped, so we check if the
 * container carries one (or one of containers in that container).
 * @param pl Player.
 * @param con Container.
 * @return 0 if it can be dropped, non-zero otherwise. */
static int check_container(object *pl, object *con)
{
	object *current, *next;
	int ret = 0;

	/* Only check stuff *inside* a container */
	if (con->type != CONTAINER)
	{
		return 0;
	}

	for (current = con->inv; current != NULL; current = next)
	{
		next = current->below;
		ret += check_container(pl, current);

		if (QUERY_FLAG(current, FLAG_STARTEQUIP))
		{
			ret += 1;
		}

		if (QUERY_FLAG(current, FLAG_INV_LOCKED))
		{
			SET_FLAG(pl, FLAG_INV_LOCKED);
		}
	}

	return ret;
}

/**
 * Sends a @ref BINARY_CMD_READY to the player's client.
 * @param pl The player.
 * @param tag Object UID that the client should be notified about being
 * readied.
 * @param type Type of the object as returned by cmd_ready_determine(). */
void cmd_ready_send(player *pl, tag_t tag, int type)
{
	SockList sl;
	unsigned char sockbuf[6];

	sl.buf = sockbuf;
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_READY);
	SockList_AddChar(&sl, type);
	SockList_AddInt(&sl, tag);
	Send_With_Handling(&pl->socket, &sl);
}

/**
 * Determines one of @ref READY_OBJ_xxx for the specified object.
 * @param tmp The object.
 * @return One of @ref READY_OBJ_xxx, -1 if not applicable to any of the
 * defined constants. */
int cmd_ready_determine(object *tmp)
{
	/* System object, can't be any of the below. */
	if (QUERY_FLAG(tmp, FLAG_SYS_OBJECT))
	{
		return -1;
	}

	/* Objects that can be thrown. */
	if (QUERY_FLAG(tmp, FLAG_IS_THROWN))
	{
		return READY_OBJ_THROW;
	}
	/* Arrows, or containers like quivers. */
	else if (tmp->type == ARROW || tmp->type == CONTAINER)
	{
		return READY_OBJ_ARROW;
	}
	else
	{
		return -1;
	}
}

/**
 * Look through player's inventory and unready readied object of the
 * specified type.
 * @param op Player.
 * @param type One of @ref READY_OBJ_xxx that the readied object must
 * match. */
void cmd_ready_clear(object *op, int type)
{
	object *tmp;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (QUERY_FLAG(tmp, FLAG_IS_READY) && cmd_ready_determine(tmp) == type)
		{
			CLEAR_FLAG(tmp, FLAG_IS_READY);
			break;
		}
	}
}
