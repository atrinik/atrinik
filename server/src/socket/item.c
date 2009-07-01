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

/* This containes item logic for client/server.  IT doesn't contain
 * the actual commands that send the data, but does contain
 * the logic for what items should be sent. */

#include <global.h>
#include <object.h>
#include <newclient.h>
#include <newserver.h>
#include <sproto.h>

static object *esrv_get_ob_from_count_DM(object *pl, tag_t count);
static int check_container(object *pl, object *con);

/* This is the maximum number of bytes we expect any one item to take up */
#define MAXITEMLEN  300

/*******************************************************************************
 * Functions related to sending object data to the client.                     *
 ******************************************************************************/

/* This is more or less stolen from the query_weight function. */
#define WEIGHT(op) (op->nrof ? op->weight : op->weight + op->carrying)

/* This is a simple function that we use a lot here.  It basically
 * adds the specified buffer into the socklist, but prepends a
 * single byte in length.  If the data is longer than that byte, it is
 * truncated approprately. */
inline void add_stringlen_to_sockbuf(char *buf, SockList *sl)
{
    int len;

    len = strlen(buf);

    if (len > 255)
		len = 255;

    SockList_AddChar(sl, (char) len);
    strncpy((char*)sl->buf + sl->len, buf, len);
    sl->len += len;
}

/* This is a similar to query_name, but returns flags
 * to be sent to client. */
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
		flags |= F_OPEN;

	if(QUERY_FLAG(op, FLAG_IS_TRAPED))
		flags |= F_TRAPED;

    if (QUERY_FLAG(op,FLAG_KNOWN_CURSED))
	{
		if (QUERY_FLAG(op, FLAG_DAMNED))
			flags |= F_DAMNED;
		else if (QUERY_FLAG(op, FLAG_CURSED))
			flags |= F_CURSED;
    }

    if ((QUERY_FLAG(op, FLAG_KNOWN_MAGICAL) && QUERY_FLAG(op, FLAG_IS_MAGICAL)) || (QUERY_FLAG(op, FLAG_IS_MAGICAL) && QUERY_FLAG(op, FLAG_IDENTIFIED)))
		flags |= F_MAGIC;

    if (QUERY_FLAG(op, FLAG_UNPAID))
		flags |= F_UNPAID;

    if (QUERY_FLAG(op, FLAG_INV_LOCKED))
		flags |= F_LOCKED;

    if (QUERY_FLAG(op, FLAG_IS_INVISIBLE))
		flags |= F_INVISIBLE;

    if (QUERY_FLAG(op, FLAG_IS_ETHEREAL))
		flags |= F_ETHEREAL;

    return flags;
}

/* draw the look window.  Don't need to do animations here
 * This sends all the faces to the client, not just updates.  This is
 * because object ordering would otherwise be inconsistent */
void esrv_draw_look(object *pl)
{
	NewSocket  *ns = &CONTR(pl)->socket;
	char *tmp_sp;
    object *head, *tmp, *last;
    int len, flags, got_one = 0, anim_speed, start_look = 0, end_look = 0;
    SockList sl;
    char buf[MAX_BUF];

	/* change out_of_map to OUT_OF_REAL_MAP(). we don't even think here about map crossing */
    if (QUERY_FLAG(pl, FLAG_REMOVED) || pl->map == NULL || pl->map->in_memory != MAP_IN_MEMORY || OUT_OF_REAL_MAP(pl->map, pl->x, pl->y))
	    return;

	/*LOG(-1, "send look of: %s\n", query_name(pl));*/
	/* another layer feature: grab last (top) object without browsing the objects */
	tmp = GET_MAP_OB_LAST(pl->map, pl->x, pl->y);

    sl.buf = malloc(MAXSOCKBUF);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMY);

	SockList_AddInt(&sl, 0);
	SockList_AddInt(&sl, 0);

    if (CONTR(pl)->socket.look_position)
	{
		SockList_AddInt(&sl, 0x80000000 | (CONTR(pl)->socket.look_position- NUM_LOOK_OBJECTS));
		SockList_AddInt(&sl, 0);
		SockList_AddInt(&sl, -1);
		SockList_AddInt(&sl, prev_item_face->number);
		SockList_AddChar(&sl, 0);
		sprintf(buf, "Click here to see %d previous items", NUM_LOOK_OBJECTS);
		add_stringlen_to_sockbuf(buf, &sl);
		SockList_AddShort(&sl,0);
		SockList_AddChar(&sl, 0);
		SockList_AddInt(&sl, 0);
    }

    for (last = NULL; tmp != last; tmp = tmp->below)
	{
		if (tmp == pl)
			continue;

		/* skip map mask, sys_objects and invisible objects when we can't see them */
		if (tmp->layer <= 2 || IS_SYS_INVISIBLE(tmp) || (!QUERY_FLAG(pl, FLAG_SEE_INVISIBLE) && QUERY_FLAG(tmp, FLAG_IS_INVISIBLE)))
		{
			/* but only when we are not a active DM */
			if (!QUERY_FLAG(pl, FLAG_WIZ))
				continue;
		}

		/* skip all items we had send before of the 'max shown items of a tile space' */
		if (++start_look < CONTR(pl)->socket.look_position)
			continue;

		/* if we have too much items to send, send a 'next group' object and leave here */
		if (++end_look > NUM_LOOK_OBJECTS)
		{
			SockList_AddInt(&sl, 0x80000000 | (CONTR(pl)->socket.look_position + NUM_LOOK_OBJECTS));
			SockList_AddInt(&sl, 0);
			SockList_AddInt(&sl, -1);
			SockList_AddInt(&sl, next_item_face->number);
			SockList_AddChar(&sl, 0);
			sprintf(buf,"Click here to see next group of items");
			add_stringlen_to_sockbuf(buf, &sl);
			SockList_AddShort(&sl, 0);
			SockList_AddChar(&sl, 0);
			SockList_AddInt(&sl, 0);
			break;
		}

		/* ok, now we start sending this item here */
		flags = query_flags(tmp);
		if (QUERY_FLAG(tmp, FLAG_NO_PICK))
			flags |= F_NOPICK;

		SockList_AddInt(&sl, tmp->count);
		SockList_AddInt(&sl, flags);
		SockList_AddInt(&sl, QUERY_FLAG(tmp, FLAG_NO_PICK) ? -1 : WEIGHT(tmp));

		if (tmp->head)
		{
			if (tmp->head->inv_face && QUERY_FLAG(tmp, FLAG_IDENTIFIED))
			{
			    SockList_AddInt(&sl, tmp->head->inv_face->number);
			}
			else
			{
			    SockList_AddInt(&sl, tmp->head->face->number);
			}
		}
		else
		{
			if (tmp->inv_face && QUERY_FLAG(tmp, FLAG_IDENTIFIED))
			{
			    SockList_AddInt(&sl, tmp->inv_face->number);
			}
			else
			{
			    SockList_AddInt(&sl, tmp->face->number);
			}
		}
		SockList_AddChar(&sl, tmp->facing);

		if (tmp->head)
			head = tmp->head;
		else
			head = tmp;

			/* +1 = 0 marker for string end */
		len = strlen((tmp_sp = query_base_name(head, pl))) + 1;
		if (len > 128)
		{
			/* 127 chars + 0 marker */
			len = 128;
			SockList_AddChar(&sl, (char ) len);
			strncpy((char *) sl.buf + sl.len, tmp_sp, 127);
			sl.len += len;
			*(sl.buf + sl.len) = 0;
		}
		else
		{
			SockList_AddChar(&sl, (char )len);
    		strcpy((char *) sl.buf + sl.len, tmp_sp);
			sl.len += len;
		}

		/* handle animations... this will change 100% when we add client
		 * sided animations.*/
		SockList_AddShort(&sl, tmp->animation_id);
		anim_speed = 0;
		if (QUERY_FLAG(tmp, FLAG_ANIMATE))
		{
			if (tmp->anim_speed)
				anim_speed = tmp->anim_speed;
			else
			{
				if (FABS(tmp->speed) < 0.001)
					anim_speed = 255;
				else if (FABS(tmp->speed) >= 1.0)
					anim_speed = 1;
				else
					anim_speed = (int) (1.0 / FABS(tmp->speed));
			}

			if (anim_speed > 255)
				anim_speed = 255;
		}
		SockList_AddChar(&sl, (char) anim_speed);

		SockList_AddInt(&sl, tmp->nrof);
		SET_FLAG(tmp, FLAG_CLIENT_SENT);
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

		/* We do a special for DMs - forcing the
		 * inventory of ALL objects we send here... This is a
		 * wonderful feature for controling & finding bugs. */
		if (QUERY_FLAG(pl, FLAG_WIZ))
		{
			if (tmp->inv)
				got_one = esrv_draw_DM_inv(pl, &sl, tmp);
		}
	}

    if (got_one || (!got_one && !ns->below_clear))
	{
		Send_With_Handling(ns, &sl);
		ns->below_clear = 0;
	}

    free(sl.buf);
}


/* used for a active DM - implicit sending the inventory of all
 * items we see in inventory & in below. For controling & debug.
 * Do a examine cmd over the item and you will see a dump. */
int esrv_draw_DM_inv(object *pl, SockList *sl, object *op)
{
	char *tmp_sp;
	object *tmp, *head;
	int got_one = 0, flags, len, anim_speed;

	SockList_AddInt(sl, 0);
	SockList_AddInt(sl, 0);
	SockList_AddInt(sl, -1);
	SockList_AddInt(sl, blank_face->number);
	len = strlen("in inventory") + 1;
	SockList_AddChar(sl, (char)len);
	add_stringlen_to_sockbuf("in inventory", sl);
	SockList_AddShort(sl, 0);
	SockList_AddChar(sl, 0);
	SockList_AddInt(sl, 0);

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		flags = query_flags(tmp);
		if (QUERY_FLAG(tmp, FLAG_NO_PICK))
			flags |=  F_NOPICK;

		SockList_AddInt(sl, tmp->count);
		SockList_AddInt(sl, flags);
		SockList_AddInt(sl, QUERY_FLAG(tmp, FLAG_NO_PICK) ? -1 : WEIGHT(tmp));

		if (tmp->head)
		{
			SockList_AddInt(sl, tmp->head->face->number);
		}
		else
		{
			SockList_AddInt(sl, tmp->face->number);
		}
		SockList_AddChar(sl, tmp->facing);

		if (tmp->head)
			head = tmp->head;
		else
			head = tmp;

		/* +1 = 0 marker for string end */
		len = strlen((tmp_sp = query_base_name(head, pl))) + 1;
		if (len > 128)
		{
			/* 127 chars + 0 marker */
			len = 128;
			SockList_AddChar(sl, (char ) len);
			strncpy((char *) sl->buf + sl->len, tmp_sp, 127);
			sl->len += len;
			*(sl->buf + sl->len) = 0;
		}
		else
		{
			SockList_AddChar(sl, (char ) len);
    		strcpy((char *) sl->buf + sl->len, tmp_sp);
			sl->len += len;
		}

		/* handle animations... this will change 100% when we add client
		 * sided animations. */
		SockList_AddShort(sl, tmp->animation_id);
		anim_speed = 0;

		if (QUERY_FLAG(tmp, FLAG_ANIMATE))
		{
			if (tmp->anim_speed)
				anim_speed = tmp->anim_speed;
			else
			{
				if (FABS(tmp->speed) < 0.001)
					anim_speed = 255;
				else if (FABS(tmp->speed) >= 1.0)
					anim_speed = 1;
				else
					anim_speed = (int) (1.0 / FABS(tmp->speed));
			}

			if (anim_speed > 255)
				anim_speed = 255;
		}
		SockList_AddChar(sl, (char) anim_speed);

		SockList_AddInt(sl, tmp->nrof);
		SET_FLAG(tmp, FLAG_CLIENT_SENT);
		got_one++;

		if (sl->len > (MAXSOCKBUF - MAXITEMLEN))
		{
			Send_With_Handling(&CONTR(pl)->socket, sl);
			SOCKET_SET_BINARY_CMD(sl, BINARY_CMD_ITEMY);
			/* do no delinv */
			SockList_AddInt(sl, -2);
			SockList_AddInt(sl, 0);
			got_one = 0;
		}

		/* oh well... */
		if (tmp->inv)
			got_one = esrv_draw_DM_inv(pl, sl, tmp);
	}

	SockList_AddInt(sl, 0);
	SockList_AddInt(sl, 0);
	SockList_AddInt(sl, -1);
	SockList_AddInt(sl, blank_face->number);
	len = strlen("end of inventory") + 1;
	SockList_AddChar(sl, (char)len);
	add_stringlen_to_sockbuf("end of inventory", sl);
	SockList_AddShort(sl, 0);
	SockList_AddChar(sl, 0);
	SockList_AddInt(sl, 0);
	return got_one;
}

void esrv_close_container(object *op)
{
    SockList sl;
    sl.buf = malloc(256);

	/*LOG(-1, "close container of: %s\n", query_name(op));*/
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMX);
	/* container mode flag */
	SockList_AddInt(&sl, -1);
	SockList_AddInt(&sl, -1);

	Send_With_Handling(&CONTR(op)->socket, &sl);
    free(sl.buf);
}

static int esrv_send_inventory_DM(object *pl, SockList *sl, object *op)
{
    object *tmp;
    int flags, got_one = 0, anim_speed, len;
    char item_n[MAX_BUF];

    for (tmp = op->inv; tmp; tmp = tmp->below)
	{
	    flags = query_flags(tmp);
	    if (QUERY_FLAG(tmp, FLAG_NO_PICK))
			flags |= F_NOPICK;

		SockList_AddInt(sl, tmp->count);
	    SockList_AddInt(sl, flags);
	    SockList_AddInt(sl, QUERY_FLAG(tmp, FLAG_NO_PICK) ? -1 : WEIGHT(tmp));

		if (tmp->inv_face && QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
		    SockList_AddInt(sl, tmp->inv_face->number);
		}
		else
		{
		    SockList_AddInt(sl, tmp->face->number);
		}

        SockList_AddChar(sl, tmp->facing);
		SockList_AddChar(sl, tmp->type);
		SockList_AddChar(sl, tmp->sub_type1);

		if (QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
			SockList_AddChar(sl, tmp->item_quality);
			SockList_AddChar(sl, tmp->item_condition);
			SockList_AddChar(sl, tmp->item_level);
			SockList_AddChar(sl, tmp->item_skill);
		}
		else
		{
			SockList_AddChar(sl, (char) 255);
			SockList_AddChar(sl, (char) 255);
			SockList_AddChar(sl, (char)255);
			SockList_AddChar(sl, (char)255);
		}

        strncpy(item_n, query_base_name(tmp, pl), 127);
		item_n[127] = 0;
		len = strlen(item_n) + 1;
		SockList_AddChar(sl, (char) len);
		memcpy(sl->buf + sl->len, item_n, len);
		sl->len += len;

		if (tmp->inv_animation_id)
		{
		    SockList_AddShort(sl, tmp->inv_animation_id);
		}
		else
		{
		    SockList_AddShort(sl, tmp->animation_id);
		}

		/* i use for both the same anim_speed - when we need a different,
		 * i adding inv_anim_speed. */
	    anim_speed = 0;
	    if (QUERY_FLAG(tmp, FLAG_ANIMATE))
		{
			if (tmp->anim_speed)
				anim_speed = tmp->anim_speed;
			else
			{
				if (FABS(tmp->speed) < 0.001)
					anim_speed = 255;
				else if (FABS(tmp->speed) >= 1.0)
					anim_speed = 1;
				else
					anim_speed = (int) (1.0 / FABS(tmp->speed));
			}

			if (anim_speed > 255)
				anim_speed = 255;
	    }
	    SockList_AddChar(sl, (char)anim_speed);
	    SockList_AddInt(sl, tmp->nrof);
	    SET_FLAG(tmp, FLAG_CLIENT_SENT);
	    got_one++;

	    /* IT is possible for players to accumulate a huge amount of
	     * items (especially with some of the bags out there) to
	     * overflow the buffer.  IF so, send multiple item1 commands. */
	    if (sl->len > (MAXSOCKBUF - MAXITEMLEN))
		{
			Send_With_Handling(&CONTR(pl)->socket, sl);
			SOCKET_SET_BINARY_CMD(sl, BINARY_CMD_ITEMY);
			/* no delinv */
			SockList_AddInt(sl, -3);
			SockList_AddInt(sl, op->count);
			got_one = 0;
	    }
    }

	return got_one;
}

/* send_inventory send the inventory for the player BUT also the inventory for
 * items. When the player obens a chest on the ground, this function is called to
 * send the inventory for the chest - and the items are shown in below. The client
 * should take care, that the items in the below windows shown can be changed here
 * too without calling the function for the look window. */

/* we have here no invisible flag - we can "see" invisible when we have it in the inventory.
 * that simulate the effect that we can "feel" the object in our hands.
 * but when we drop it, it rolls out of sight and vanish... */
void esrv_send_inventory(object *pl, object *op)
{
    object *tmp;
    int flags, got_one = 0, anim_speed, len;
    SockList sl;
    char item_n[MAX_BUF];

    sl.buf = malloc(MAXSOCKBUF);

	/*LOG(llevDebug,"send inventory of: %s\n", query_name(op));*/
	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMY);

	/* in this case we send a container inventory! */
	if (pl != op)
	{
		/* container mode flag */
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
			/* skip this for DMs */
			if (!QUERY_FLAG(pl, FLAG_WIZ))
				continue;
		}

		if (LOOK_OBJ(tmp) || QUERY_FLAG(pl, FLAG_WIZ))
		{
			flags = query_flags(tmp);
			if (QUERY_FLAG(tmp, FLAG_NO_PICK))
				flags |= F_NOPICK;

			SockList_AddInt(&sl, tmp->count);
			SockList_AddInt(&sl, flags);
			SockList_AddInt(&sl, QUERY_FLAG(tmp, FLAG_NO_PICK) ? -1 : WEIGHT(tmp));

			if (tmp->inv_face && QUERY_FLAG(tmp, FLAG_IDENTIFIED))
			{
				SockList_AddInt(&sl, tmp->inv_face->number);
			}
			else
			{
				SockList_AddInt(&sl, tmp->face->number);
			}

			SockList_AddChar(&sl, tmp->facing);
			SockList_AddChar(&sl, tmp->type);
			SockList_AddChar(&sl, tmp->sub_type1);
			if (QUERY_FLAG(tmp, FLAG_IDENTIFIED))
			{
				SockList_AddChar(&sl, tmp->item_quality);
				SockList_AddChar(&sl, tmp->item_condition);
				SockList_AddChar(&sl, tmp->item_level);
				SockList_AddChar(&sl, tmp->item_skill);
			}
			else
			{
				SockList_AddChar(&sl, (char) 255);
				SockList_AddChar(&sl, (char) 255);
				SockList_AddChar(&sl, (char)255);
				SockList_AddChar(&sl, (char)255);
			}

			strncpy(item_n, query_base_name(tmp, pl), 127);
			item_n[127] = 0;
			len = strlen(item_n) + 1;
			SockList_AddChar(&sl, (char) len);
			memcpy(sl.buf + sl.len, item_n, len);
			sl.len += len;

			if (tmp->inv_animation_id)
			{
				SockList_AddShort(&sl,tmp->inv_animation_id);
			}
			else
			{
				SockList_AddShort(&sl,tmp->animation_id);
			}

			/* i use for both the same anim_speed - when we need a different,
			 * i adding inv_anim_speed. */
			anim_speed = 0;
			if (QUERY_FLAG(tmp, FLAG_ANIMATE))
			{
				if (tmp->anim_speed)
					anim_speed = tmp->anim_speed;
				else
				{
					if (FABS(tmp->speed) < 0.001)
						anim_speed = 255;
					else if (FABS(tmp->speed) >= 1.0)
						anim_speed = 1;
					else
						anim_speed = (int) (1.0 / FABS(tmp->speed));
				}

				if (anim_speed > 255)
					anim_speed = 255;
			}
			SockList_AddChar(&sl, (char)anim_speed);
			SockList_AddInt(&sl, tmp->nrof);
			SET_FLAG(tmp, FLAG_CLIENT_SENT);
			got_one++;

			/* IT is possible for players to accumulate a huge amount of
			 * items (especially with some of the bags out there) to
			 * overflow the buffer.  IF so, send multiple item1 commands. */
			if (sl.len > (MAXSOCKBUF - MAXITEMLEN))
			{
				Send_With_Handling(&CONTR(pl)->socket, &sl);
				SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMY);
				/* no delinv */
				SockList_AddInt(&sl, -3);
				SockList_AddInt(&sl, op->count);
				got_one=0;
			}

			if (QUERY_FLAG(pl, FLAG_WIZ))
			{
				if (tmp->inv && tmp->type != CONTAINER)
					got_one = esrv_send_inventory_DM(pl, &sl, tmp);
			}
		}
    }

	/* container can be empty... */
    if (got_one || pl != op)
		Send_With_Handling(&CONTR(pl)->socket, &sl);

    free(sl.buf);
}

static void esrv_update_item_send(int flags, object *pl, object *op)
{
    SockList sl;

	/*LOG(llevDebug, "update item: %s\n", query_name(op));*/

    /* If we have a request to send the player item, skip a few checks. */
    if (op != pl)
	{
		if (!LOOK_OBJ(op) && !QUERY_FLAG(pl, FLAG_WIZ))
			return;
    }
    sl.buf = malloc(MAXSOCKBUF);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_UPITEM);
    SockList_AddShort(&sl, (uint16) flags);
    SockList_AddInt(&sl, op->count);

    if (flags & UPD_LOCATION)
	{
		SockList_AddInt(&sl, op->env? op->env->count:0);
	}

    if (flags & UPD_FLAGS)
	{
		SockList_AddInt(&sl, query_flags(op));
	}

    if (flags & UPD_WEIGHT)
	{
		SockList_AddInt(&sl, WEIGHT(op));
	}

    if (flags & UPD_FACE)
	{
		if (op->inv_face && QUERY_FLAG(op, FLAG_IDENTIFIED))
		{
			SockList_AddInt(&sl, op->inv_face->number);
		}
		else
		{
			SockList_AddInt(&sl, op->face->number);
		}
    }

    if (flags & UPD_DIRECTION)
	{
		SockList_AddChar(&sl, (char)op->facing);
	}

    if (flags & UPD_NAME)
	{
		if (CONTR(pl)->socket.sc_version >= 1024)
		{
			int len;
			char *item_p, item_n[MAX_BUF];

			strncpy(item_n, query_base_name(op, pl), 127);
			item_n[127] = 0;
			len = strlen(item_n);
			item_p = query_base_name(op, pl);
			strncpy(item_n + len + 1, item_p, 127);
			item_n[254] = 0;
			len += strlen(item_n + 1 + len) + 1;
			SockList_AddChar(&sl, (char)len);
			memcpy(sl.buf + sl.len, item_n, len);
			sl.len += len;
		}
		else
			add_stringlen_to_sockbuf(query_base_name(op, pl), &sl);
    }

    if (flags & UPD_ANIM)
	{
		if (op->inv_animation_id)
		{
		    SockList_AddShort(&sl, op->inv_animation_id);
		}
		else
		{
		    SockList_AddShort(&sl, op->animation_id);
		}
	}

    if (flags & UPD_ANIMSPEED)
	{
		int anim_speed=0;
		if (QUERY_FLAG(op, FLAG_ANIMATE))
		{
			if (op->anim_speed)
				anim_speed = op->anim_speed;
			else
			{
				if (FABS(op->speed) < 0.001)
					anim_speed = 255;
				else if (FABS(op->speed) >= 1.0)
					anim_speed = 1;
				else
					anim_speed = (int) (1.0 / FABS(op->speed));
			}

			if (anim_speed > 255)
				anim_speed = 255;
		}

		SockList_AddChar(&sl, (char)anim_speed);
    }

    if (flags & UPD_NROF)
	{
	    SockList_AddInt(&sl, op->nrof);
	}

    Send_With_Handling(&CONTR(pl)->socket, &sl);
    free(sl.buf);
}

/* Updates object *op for player *pl.  flags is a list of values to update
 * to the client (as defined in newclient.h - might as well use the
 * same value both places. */
void esrv_update_item(int flags, object *pl, object *op)
{
	object * tmp;

	/* special case: update something in a container.
	 * we don't care about where the container is,
	 * because always is the container link list valid! */
	if (op->env && op->env->type == CONTAINER)
	{
		for (tmp = op->env->attacked_by; tmp; tmp = CONTR(tmp)->container_above)
			esrv_update_item_send(flags, tmp, op);

		return;
	}

	esrv_update_item_send(flags, pl, op);
}

static void esrv_send_item_send(object *pl, object*op)
{
    int anim_speed;
    SockList sl;
    char item_n[MAX_BUF];

    /* If this is not the player object, do some more checks */
    if (op != pl)
	{
		/* We only send 'visibile' objects to the client */
		if (!LOOK_OBJ(op))
		    return;
    }

	/*LOG(-1, "send item: %s\n", query_name(op));*/
    sl.buf = malloc(MAXSOCKBUF);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_ITEMX);
	/* no delinv */
    SockList_AddInt(&sl, -4);
    SockList_AddInt(&sl, (op->env? op->env->count:0));
    SockList_AddInt(&sl, op->count);
    SockList_AddInt(&sl, query_flags(op));
    SockList_AddInt(&sl, WEIGHT(op));

	if (op->head)
	{
		if (op->head->inv_face && QUERY_FLAG(op, FLAG_IDENTIFIED))
		{
		    SockList_AddInt(&sl, op->head->inv_face->number);
		}
		else
		{
		    SockList_AddInt(&sl, op->head->face->number);
		}
	}
	else
	{
		if (op->inv_face && QUERY_FLAG(op, FLAG_IDENTIFIED))
		{
		    SockList_AddInt(&sl, op->inv_face->number);
		}
		else
		{
		    SockList_AddInt(&sl, op->face->number);
		}
	}

    SockList_AddChar(&sl, op->facing);
	/* if not below */
    if (op->env)
    {
        SockList_AddChar(&sl, op->type);
        SockList_AddChar(&sl, op->sub_type1);

        if (QUERY_FLAG(op, FLAG_IDENTIFIED))
        {
            SockList_AddChar(&sl, op->item_quality);
            SockList_AddChar(&sl, op->item_condition);
            SockList_AddChar(&sl, op->item_level);
            SockList_AddChar(&sl, op->item_skill);
        }
        else
        {
            SockList_AddChar(&sl, (char)255);
            SockList_AddChar(&sl, (char)255);
            SockList_AddChar(&sl, (char)255);
            SockList_AddChar(&sl, (char)255);
        }
    }

    if (CONTR(pl)->socket.sc_version >= 1024)
	{
		int len;
		strncpy(item_n, query_base_name(op, pl), 127);
		item_n[127] = 0;
		len=strlen(item_n) + 1;
		SockList_AddChar(&sl, (char)len);
		memcpy(sl.buf + sl.len, item_n, len);
		sl.len += len;
    }
	else
		add_stringlen_to_sockbuf(query_base_name(op, pl), &sl);

	if (op->env && op->inv_animation_id)
	{
	    SockList_AddShort(&sl,op->inv_animation_id);
	}
	else
	{
		SockList_AddShort(&sl,op->animation_id);
	}

    anim_speed = 0;
    if (QUERY_FLAG(op, FLAG_ANIMATE))
	{
		if (op->anim_speed)
			anim_speed = op->anim_speed;
		else
		{
			if (FABS(op->speed) < 0.001)
				anim_speed = 255;
			else if (FABS(op->speed) >= 1.0)
				anim_speed = 1;
			else
				anim_speed = (int) (1.0 / FABS(op->speed));
		}

        if (anim_speed > 255)
			anim_speed = 255;
    }

    SockList_AddChar(&sl, (char)anim_speed);
    SockList_AddInt(&sl, op->nrof);
    Send_With_Handling(&CONTR(pl)->socket, &sl);
    SET_FLAG(op, FLAG_CLIENT_SENT);
    free(sl.buf);
}

void esrv_send_item(object *pl, object*op)
{
	object * tmp;

	/* special case: update something in a container.
	 * we don't care about where the container is,
	 * because always is the container link list valid! */
	if (op->env && op->env->type == CONTAINER)
	{
		for (tmp = op->env->attacked_by; tmp; tmp = CONTR(tmp)->container_above)
			esrv_send_item_send(tmp, op);
		return;
	}

	if (pl->type != PLAYER)
	{
		LOG(llevBug, "esrv_send_item(): called for non PLAYER/CONTAINER object! (%s) (%s)\n", query_name(pl, NULL), query_name(op, NULL));
		return;
	}

	esrv_send_item_send(pl, op);
}

static void esrv_del_item_send(player *pl, int tag)
{
    SockList sl;

    sl.buf = malloc(MAXSOCKBUF);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_DELITEM);
    SockList_AddInt(&sl, tag);

    Send_With_Handling(&pl->socket, &sl);
    free(sl.buf);
}

/* Tells the client to delete an item.
 * cont is the container - it must be seperated
 * from tag because the "tag" object can be destroyed
 * at this point on the server - we need to notify it
 * to the clients now. */
void esrv_del_item(player *pl, int tag, object *cont)
{
	object * tmp;

	if (cont && cont->type == CONTAINER)
	{
		for (tmp = cont->attacked_by; tmp; tmp = CONTR(tmp)->container_above)
			esrv_del_item_send(CONTR(tmp), tag);

		return;
	}

	esrv_del_item_send(pl, tag);
}


/*******************************************************************************
 * Client has requested us to do something with an object.                     *
 ******************************************************************************/

/* Takes a player and object count (tag) and returns the actual object
 * pointer, or null if it can't be found. */
object *esrv_get_ob_from_count(object *pl, tag_t count)
{
    object *op, *tmp;

    if (pl->count == count)
		return pl;

	/* this is special case... We can examine deep inside every inventory
	 * even from non containers. */
	if (QUERY_FLAG(pl, FLAG_WIZ))
	{
	    for (op = pl->inv; op; op = op->below)
		{
			if (op->count == count)
				return op;
			else if (op->inv)
			{
				if ((tmp = esrv_get_ob_from_count_DM(op->inv, count)))
					return tmp;
			}
		}

		for (op = get_map_ob(pl->map, pl->x, pl->y); op; op = op->above)
		{
			if (op->count == count)
				return op;
			else if (op->inv)
			{
				if ((tmp = esrv_get_ob_from_count_DM(op->inv, count)))
					return tmp;
			}
		}

		return NULL;
	}

    if (pl->count == count)
		return pl;

    for (op = pl->inv; op; op = op->below)
		if (op->count == count)
	    	return op;
		else if (op->type == CONTAINER && CONTR(pl)->container == op)
	    	for (tmp = op->inv; tmp; tmp = tmp->below)
				if (tmp->count == count)
		    		return tmp;

    for (op = get_map_ob(pl->map, pl->x, pl->y); op; op = op->above)
		if (op->count == count)
	    	return op;
		else if (op->type == CONTAINER && CONTR(pl)->container == op)
	    	for (tmp = op->inv; tmp; tmp = tmp->below)
				if (tmp->count == count)
		    		return tmp;

    return NULL;
}

/* recursive function for DM access to non container inventories */
static object *esrv_get_ob_from_count_DM(object *pl, tag_t count)
{
	object *tmp, *op;

	for (op = pl; op; op = op->below)
	{
		if (op->count == count)
			return op;
		else if (op->inv)
		{
			if ((tmp = esrv_get_ob_from_count_DM(op->inv, count)))
				return tmp;
		}
	}

	return NULL;
}

/* Client wants to examine some object.  So lets do so. */
void ExamineCmd(char *buf, int len, player *pl)
{
    long tag = atoi(buf);
    object *op = esrv_get_ob_from_count(pl->ob, tag);

	(void) len;

    if (!op)
	{
		/*LOG(llevDebug, "Player '%s' tried examine the unknown object (%d)\n",pl->ob->name, tag);*/
		return;
    }

    examine(pl->ob, op);
}

/* Go through ob's inventory, and find any quickslot match with slot.
 * If match is found, if the object is a force, remove it, if not,
 * just set the quickslot to 0. Not recursive, because players should
 * not have items in containers inside quickslots. */
static void remove_quickslot(int slot, object *ob)
{
	object *op;

	for (op = ob->inv; op; op = op->below)
	{
		if (op->quickslot && op->quickslot == slot)
		{
			if (op->arch->name && op->name && strcmp(op->arch->name, "force") == 0 && strcmp(op->name, "spell_quickslot") == 0)
			{
				remove_ob(op);
				check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
			}
			else
				op->quickslot = 0;
		}
	}
}

#define MAX_QUICKSLOT 32
/* Quick slot command. The new quickslot system works like this:
 *  - For spells:
 *    - Player adds the spell to his quick slot, client sends
 *      data to store that information. The information is
 *      stored in a force inside player's inventory.
 *  - For items:
 *    - Same as spells, except the information is instead
 *      stored inside the object's "quickslot" field. */
void QuickSlotCmd(char *buf, int len, player *pl)
{
    long tag;
	object *op;
	char *cp, tmp[HUGE_BUF * 12], tmpbuf[MAX_BUF];
	int quickslot;

	(void) len;

	/* Set command. We want to set an object's  */
	if (strncmp(buf, "set ", 4) == 0)
	{
		buf += 4;

		/* Get the slot ID */
		quickslot = atoi(buf);

		if (!(cp = strchr(buf, ' ')))
			return;

		/* Now get the count */
		tag = atoi(cp);

		/* And find the object */
		op = esrv_get_ob_from_count(pl->ob, tag);

		/* Sanity checks */
		if (!op || quickslot < 1 || quickslot > MAX_QUICKSLOT)
			return;

		/* First, find any old items/spells for this quickslot */
		remove_quickslot(quickslot, pl->ob);

		/* Then set this item a new quickslot ID */
		op->quickslot = quickslot;
	}
	/* Bit different logic for spell quickslots */
	else if (strncmp(buf, "setspell ", 9) == 0)
	{
		buf += 9;

		/* Get the slot ID */
		quickslot = atoi(buf);
		snprintf(tmpbuf, sizeof(tmpbuf), "%d", quickslot);
		buf += strlen(tmpbuf);

		if (!(cp = strchr(buf, ' ')))
			return;

		/* Sanity checks */
		if (quickslot < 1 || quickslot > MAX_QUICKSLOT)
			return;

		/* First, find any old items/spells for this quickslot, and remove old force */
		remove_quickslot(quickslot, pl->ob);

		/* Create a new force */
		op = get_archetype("force");
		op->x = pl->ob->x, op->y = pl->ob->y;
		FREE_AND_COPY_HASH(op->name, "spell_quickslot");
		FREE_AND_COPY_HASH(op->slaying, cp);
		op->quickslot = quickslot;
		op->speed = 0.0;
		update_ob_speed(op);
		insert_ob_in_ob(op, pl->ob);
	}
	/* Unset command. */
	else if (strncmp(buf, "unset ", 6) == 0)
	{
		buf += 6;

		/* Just look for the item/spell to remove from the quickslot, nothing else */
		remove_quickslot(atoi(buf), pl->ob);
	}
	/* Load quick slots */
	else if (strncmp(buf, "load", 4) == 0)
	{
		snprintf(tmp, sizeof(tmp), "X");

		/* Go through the inventory */
		for (op = pl->ob->inv; op; op = op->below)
		{
			/* If this has quickslot set */
			if (op->quickslot)
			{
				/* It's a force, so a spell! */
				if (strcmp(op->arch->name, "force") == 0 && strcmp(op->name, "spell_quickslot") == 0)
					snprintf(tmpbuf, sizeof(tmpbuf), "\ns %d %s", op->quickslot, op->slaying);
				/* Otherwise an item */
				else
					snprintf(tmpbuf, sizeof(tmpbuf), "\ni %d %d", op->count, op->quickslot);

				strncat(tmp, tmpbuf, sizeof(tmp) - strlen(tmpbuf) - 1);
			}
		}

		/* Write it to the client if we found any quickslot entries */
		if (strlen(tmp) != 1)
			Write_String_To_Socket(&pl->socket, BINARY_CMD_QUICKSLOT, tmp, strlen(tmp));
	}
}

/* Client wants to apply some object.  Lets do so. */
void ApplyCmd(char *buf, int len, player *pl)
{
    uint32 tag = atoi(buf);
    object *op = esrv_get_ob_from_count(pl->ob, tag);

	(void) len;

    /* sort of a hack, but if the player saves and the player then manually
     * applies a savebed (or otherwise tries to do stuff), we run into trouble. */
    if (QUERY_FLAG(pl->ob, FLAG_REMOVED))
		return;

    /* If the high bit is set, player applied a pseudo object. */
    if (tag & 0x80000000)
	{
		pl->socket.look_position = tag & 0x7fffffff;
		pl->socket.update_tile = 0;
		return;
    }

    if (!op)
	{
		/*LOG(llevDebug, "Player '%s' tried apply the unknown object (%d)\n",pl->ob->name, tag);*/
		return;
    }

    player_apply(pl->ob, op, 0, 0);
}

/* Client wants to lock some object.  Lets do so. */
void LockItem(uint8 *data, int len, player *pl)
{
    int flag, tag;
    object *op;

	(void) len;

    flag = data[0];
    tag = GetInt_String(data + 1);
    op = esrv_get_ob_from_count(pl->ob, tag);

	/* can happen as result of latency or client/server async. */
    if (!op)
		return;

	/* only lock item inside the players own inventory */
	if (is_player_inv(op) != pl->ob)
	{
		new_draw_info(NDI_UNIQUE, 0, pl->ob, "You can't lock items outside your inventory!");
		return;
	}

    if (!flag)
		CLEAR_FLAG(op, FLAG_INV_LOCKED);
    else
		SET_FLAG(op, FLAG_INV_LOCKED);

    esrv_update_item(UPD_FLAGS, pl->ob, op);
}

/* Client wants to mark some object.  Lets do so. */
void MarkItem(uint8 *data, int len, player *pl)
{
    int tag;
    object *op;

	(void) len;

    tag = GetInt_String(data);
    op = esrv_get_ob_from_count(pl->ob, tag);
    if (!op)
	{
		/*new_draw_info(NDI_UNIQUE, 0, pl->ob, "Could not find object to mark");*/
		return;
    }

    pl->mark = op;
    pl->mark_count = op->count;
    new_draw_info_format(NDI_UNIQUE, 0, pl->ob, "Marked item %s", query_name(op, NULL));
}

/* look_at prints items on the specifc square.
 * [ removed EARTHWALL check and added check for containers inventory.
 *   Tero.Haatanen@lut.fi ] */
void look_at(object *op, int dx, int dy)
{
    object *tmp;
    int flag = 0, x, y;
    mapstruct *m;

    x = op->x + dx;
    y = op->y + dy;

    if(!(m = out_of_map(op->map, &x, &y)))
		return;

    for (tmp = get_map_ob(m, x ,y); tmp != NULL && tmp->above != NULL; tmp = tmp->above);

    for ( ; tmp != NULL; tmp = tmp->below)
	{
	 	if (IS_SYS_INVISIBLE(tmp) && !QUERY_FLAG(op, FLAG_WIZ))
			continue;

	 	if (!flag)
		{
	    	if (dx || dy)
				new_draw_info(NDI_UNIQUE, 0, op, "There you see:");
	    	else
			{
				new_draw_info(NDI_UNIQUE, 0, op, "You see:");
	    	}
	    	flag = 1;
	 	}

	 	if (QUERY_FLAG(op, FLAG_WIZ))
	    	new_draw_info_format(NDI_UNIQUE, 0, op, "- %s (%d).", query_name(tmp, NULL), tmp->count);
	 	else
	    	new_draw_info_format(NDI_UNIQUE, 0, op, "- %s.", query_name(tmp, NULL));

	 	if ((tmp->inv != NULL || (tmp->head && tmp->head->inv)) && ((!dx && !dy) || tmp->type != CONTAINER || QUERY_FLAG(op, FLAG_WIZ) || !(tmp->type) || tmp->type != FLESH))
	    	inventory(op, tmp->head == NULL ? tmp : tmp->head);

		/* don't continue under the floor */
	 	if (QUERY_FLAG(tmp, FLAG_IS_FLOOR) && !QUERY_FLAG(op, FLAG_WIZ))
	    	break;
    }

    if (!flag)
	{
		if (dx || dy)
	    	new_draw_info(NDI_UNIQUE, 0, op, "You see nothing there.");
		else
	    	new_draw_info(NDI_UNIQUE, 0, op, "You see nothing.");
    }
}


/* Client wants to look some object.  Lets do so. */
void LookAt(char *buf, int len, player *pl)
{
    int dx, dy;
    char *cp;

	(void) len;

    dx = atoi(buf);
    if (!(cp = strchr(buf, ' ')))
		return;

    dy = atoi(cp);

    if (FABS(dx) > MAP_CLIENT_X / 2 || FABS(dy)>MAP_CLIENT_Y / 2)
		return;

    if (pl->blocked_los[dx + (pl->socket.mapx_2)][dy + (pl->socket.mapy_2)] <= BLOCKED_LOS_BLOCKSVIEW)
		return;

    look_at(pl->ob, dx, dy);
}

/* Move an object to a new location */
void esrv_move_object(object *pl, tag_t to, tag_t tag, long nrof)
{
    object *op, *env;
	int tmp;

	/*LOG(llevDebug,"Move item %d (nrof=%d) to %d.\n", tag, nrof,to);*/

    op = esrv_get_ob_from_count(pl, tag);
	/* latency effect - we have moved before we applied this (or below from player changed) */
    if (!op)
		return;

	if (op->quickslot)
		op->quickslot = 0;

	/* drop it to the ground */
    if (!to)
	{
		if (op->map && !op->env)
			return;

		if (MAP_UNIQUE(pl->map) && !MAP_NOSAVE(pl->map))
		{
			if (!check_map_owner(pl->map, pl))
			{
				new_draw_info(NDI_UNIQUE, 0, pl, "You can't drop this here.");
				return;
			}
			else if (check_map_owner(pl->map, pl) && (op->type != CONTAINER || op->sub_type1 != 1))
			{
				new_draw_info(NDI_UNIQUE, 0, pl, "You can only drop items into special containers here.");
				return;
			}
		}

		/*LOG(-1, "drop it... (%d)\n", check_container(pl, op));*/
		/* funny trick see check container */
		CLEAR_FLAG(pl, FLAG_INV_LOCKED);
		if ((tmp = check_container(pl, op)))
			new_draw_info(NDI_UNIQUE, 0, pl, "Remove first all one-drop items from this container!");
		else if (QUERY_FLAG(pl, FLAG_INV_LOCKED))
			new_draw_info(NDI_UNIQUE, 0, pl, "You can't drop a container with locked items inside!");
		else
			drop_object(pl, op, nrof);

		return;
	}
	/* pick it up to the inventory */
	else if (to == pl->count || (to == op->count && !op->env))
	{
		/* return if player has already picked it up */
		if (op->env == pl)
			return;

		CONTR(pl)->count = nrof;
		/*LOG(-1, "pick up...\n");*/
		/* it goes in player inv or readied container */
		pick_up(pl, op);
		return ;
    }

    /* If not dropped or picked up, we are putting it into a sack */
    env = esrv_get_ob_from_count(pl, to);
    if (!env)
      	return;

    /* put_object_in_sack presumes that necessary sanity checking
     * has already been done (eg, it can be picked up and fits in
     * in a sack, so check for those things.  We should also check
     * an make sure env is in fact a container for that matter. */
	/* player have for example a opened container in the inventory */
    if (env->type == CONTAINER && can_pick(pl, op) && sack_can_hold(pl, env, op, nrof))
	{
		/*LOG(-1, "put in sack...\n");*/
		/* funny trick see check container */
		CLEAR_FLAG(pl, FLAG_INV_LOCKED);
		tmp = check_container(pl, op);

		if (QUERY_FLAG(pl, FLAG_INV_LOCKED) && env->env != pl)
			new_draw_info(NDI_UNIQUE, 0, pl, "You can't drop a container with locked items inside!");
		else if (tmp && env->env != pl)
			new_draw_info(NDI_UNIQUE, 0, pl, "Remove first all one-drop items from this container!");
		else if (QUERY_FLAG(op, FLAG_STARTEQUIP) && env->env != pl)
			new_draw_info(NDI_UNIQUE, 0, pl, "You can't store one-drop items outside your inventory!");
		else
			put_object_in_sack(pl, env, op, nrof);

		return;
    }
}


/* thats the safest rule: you can't drop containers which holds
 * a startequip item or a container holding one.
 * return is the number of one drops in this container chain. */
static int check_container(object *pl, object *con)
{
	object *current, *next;
	int ret = 0;

	/* only check stuff *inside* a container */
	if (con->type != CONTAINER)
		return ret;

	for (current = con->inv; current != NULL; current = next)
	{
		next = current->below;
		ret += check_container(pl,current);

		if (QUERY_FLAG(current, FLAG_STARTEQUIP))
			ret += 1;

		if (QUERY_FLAG(current, FLAG_INV_LOCKED))
			SET_FLAG(pl, FLAG_INV_LOCKED);
	}

	return ret;
}
