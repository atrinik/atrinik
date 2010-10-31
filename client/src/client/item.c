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
 * Object management. */

#include <include.h>

/* The list of free (unused) items */
static item *free_items;
static item *player;

/* How many items are reserved initially
 * for the item spool */
#define NROF_ITEMS 50

/*  new_item() returns pointer to new item which
 *  is allocated and initialized correctly */
static item *new_item()
{
	item *op = calloc(1, sizeof(item));

	if (!op)
	{
		LOG(llevError, "new_item(): Out of memory.\n");
	}

	return op;
}

/* alloc_items() returns pointer to list of allocated objects */
static item *alloc_items(int nrof)
{
	item *op, *list;
	int i;

	list = op = new_item();

	for (i = 1; i < nrof; i++)
	{
		op->next = new_item();
		op->next->prev = op;
		op = op->next;
	}

	return list;
}

/* free_items() frees all allocated items from list */
void free_all_items(item *op)
{
	item *tmp;

	while (op)
	{
		if (op->inv)
		{
			free_all_items(op->inv);
		}

		tmp = op->next;
		free(op);
		op = tmp;
	}
}

int locate_item_tag_from_nr(item *op, int nr)
{
	int count = 0;

	for (; op != NULL; op = op->next, count++)
	{
		if (count == nr)
		{
			return op->tag;
		}
	}

	return -1;
}

/* Recursive function, only check inventory of op
 * *not* items inside other containers. */
item *locate_item_from_inv(item *op, sint32 tag)
{
	for (; op != NULL; op = op->next)
	{
		if (op->tag == tag)
		{
			return op;
		}
	}

	return NULL;
}

/* Recursive function, used by locate_item() */
item *locate_item_from_item(item *op, sint32 tag)
{
	item *tmp;

	for (; op != NULL; op = op->next)
	{
		if (op->tag == tag)
		{
			return op;
		}
		else if (op->inv)
		{
			if ((tmp = locate_item_from_item(op->inv, tag)))
			{
				return tmp;
			}
		}
	}

	return NULL;
}

/* locate_item() returns pointer to the item which tag is given
 * as parameter or if item is not found returns NULL */
item *locate_item(sint32 tag)
{
	item *op;

	if (tag == 0)
	{
		return cpl.below;
	}

	if (tag == -1)
	{
		return cpl.sack;
	}

	if (tag == -2)
	{
		return cpl.shop;
	}

	if (cpl.below && (op = locate_item_from_item(cpl.below->inv, tag)) != NULL)
	{
		return op;
	}

	if (cpl.shop && (op = locate_item_from_item(cpl.shop->inv, tag)) != NULL)
	{
		return op;
	}

	if (cpl.sack && (op = locate_item_from_item(cpl.sack->inv, tag)) != NULL)
	{
		return op;
	}

	if ((op = locate_item_from_item(player, tag)) != NULL)
	{
		return op;
	}

	return NULL;
}

/* remove_item() inserts op the the list of free items
 * Note that it don't clear all fields in item */
void remove_item(item *op)
{
	/* IF no op, or it is the player */
	if (!op || op == player || op == cpl.below || op == cpl.sack)
	{
		return;
	}

	/* Do we really want to do this? */
	if (op->inv)
	{
		remove_item_inventory(op);
	}

	if (op->prev)
	{
		op->prev->next = op->next;
	}
	else
	{
		op->env->inv = op->next;
	}

	if (op->next)
	{
		op->next->prev = op->prev;
	}

	/* Add object to a list of free objects */
	op->next = free_items;

	if (op->next)
	{
		op->next->prev = op;
	}

	free_items = op;

	memset(op, 0, sizeof(item));
}

/* remove_item_inventory() recursive frees items inventory */
void remove_item_inventory(item *op)
{
	if (!op)
	{
		return;
	}

	while (op->inv)
	{
		remove_item(op->inv);
	}
}

/* We adding it now to the start inv.
 *  OLD: add_item() adds item op to end of the inventory of item env */
static void add_item(item *env, item *op, int bflag)
{
	item *tmp;

	if (!op)
	{
		return;
	}

	if (bflag == 0)
	{
		op->next = env->inv;

		if (op->next)
		{
			op->next->prev = op;
		}

		op->prev = NULL;
		env->inv = op;
		op->env = env;
	}
	/* Sort reverse - for inventory lists */
	else
	{
		for (tmp = env->inv; tmp && tmp->next; tmp = tmp->next)
		{
		}

		op->next = NULL;
		op->prev = tmp;
		op->env = env;

		if (!tmp)
		{
			env->inv = op;
		}
		else
		{
			if (tmp->next)
			{
				tmp->next->prev = op;
			}

			tmp->next = op;
		}
	}
}

/* create_new_item() returns pointer to a new item, inserts it to env
 * and sets its tag field and clears locked flag (all other fields
 * are unitialized and may contain random values) */
item *create_new_item(item *env, sint32 tag, int bflag)
{
	item *op;

	if (!free_items)
	{
		free_items = alloc_items(NROF_ITEMS);
	}

	op = free_items;
	free_items = free_items->next;

	if (free_items)
	{
		free_items->prev = NULL;
	}

	op->tag = tag;
	op->locked = 0;

	if (env)
	{
		add_item(env, op, bflag);
	}

	return op;
}

static void get_flags(item *op, int flags)
{
	op->open = flags & F_OPEN ? 1 : 0;
	op->damned = flags & F_DAMNED ? 1 : 0;
	op->cursed = flags & F_CURSED ? 1 : 0;
	op->magical = flags & F_MAGIC ? 1 : 0;
	op->unpaid = flags & F_UNPAID ? 1 : 0;
	op->applied = flags & F_APPLIED ? 1 : 0;
	op->locked = flags & F_LOCKED ? 1 : 0;
	op->trapped = flags & F_TRAPPED ? 1 : 0;
	op->flagsval = flags;
	op->apply_type = flags & F_APPLIED;
}

void set_item_values(item *op, char *name, sint32 weight, uint16 face, int flags, uint16 anim, uint16 animspeed, sint32 nrof, uint8 itype, uint8 stype, uint8 qual, uint8 cond, uint8 skill, uint8 level, uint8 dir)
{
	if (!op)
	{
		LOG (-1, "Error in set_item_values(): item pointer is NULL.\n");
		return;
	}

	if (nrof < 0)
	{
		op->nrof = 1;
		strncpy(op->s_name, "Warning: Old name cmd! This is a bug.", sizeof(op->s_name) - 1);
		op->s_name[sizeof(op->s_name) - 1] = '\0';
	}
	/* we have a nrof - item1 command */
	else
	{
		/* Program always expect at least 1 object internal */
		if (nrof == 0)
		{
			nrof = 1;
		}

		op->nrof = nrof;

		if (*name != '\0')
		{
			strncpy(op->s_name, name, sizeof(op->s_name) - 1);
			op->s_name[sizeof(op->s_name) - 1] = '\0';
		}
	}

	op->weight = (float) weight / 1000;

	if (itype != 254)
	{
		op->itype = itype;
	}

	if (stype != 254)
	{
		op->stype = stype;
	}

	if (qual != 254)
	{
		op->item_qua = qual;
	}

	if (cond != 254)
	{
		op->item_con = cond;
	}

	if (skill != 254)
	{
		op->item_skill = skill;
	}

	if (level != 254)
	{
		op->item_level = level;
	}

	op->face = face;
	op->animation_id = anim;
	op->anim_speed = animspeed;
	op->direction = dir;
	get_flags(op, flags);
}

void fire_command(char *buf)
{
	SockList sl;

	sl.buf = (unsigned char *) buf;
	sl.len = (int) strlen(buf);
	send_socklist(sl);
}

void toggle_locked(item *op)
{
	SockList sl;
	char buf[MAX_BUF];

	/* if item is on the ground, don't lock it */
	if (!op || !op->env || op->env->tag == 0)
	{
		return;
	}

	sl.buf = (unsigned char *) buf;
	strcpy((char *) sl.buf, "lock ");
	sl.len = 5;
	sl.buf[sl.len++] = op->locked ? 0 : 1;
	SockList_AddInt(&sl, op->tag);
	send_socklist(sl);
}

void send_mark_obj(item *op)
{
	SockList sl;
	char buf[MAX_BUF];

	/* if item is on the ground, don't mark it */
	if (!op || !op->env || op->env->tag == 0)
	{
		return;
	}

	if (cpl.mark_count == op->tag)
	{
		cpl.mark_count = -1;
	}
	else
	{
		cpl.mark_count = op->tag;
	}

	sl.buf = (unsigned char *) buf;
	strcpy((char *) sl.buf, "mark ");
	sl.len = 5;
	SockList_AddInt(&sl, op->tag);
	send_socklist(sl);
}

item *player_item()
{
	player = new_item();
	cpl.below = new_item();
	cpl.sack = new_item();
	cpl.shop = new_item();

	cpl.below->weight = -111;
	cpl.sack->weight = -111;
	cpl.shop->weight = -111;

	return player;
}

/* Upates an item with new attributes. */
void update_item(int tag, int loc, char *name, int weight, int face, int flags, int anim, int animspeed, int nrof, uint8 itype, uint8 stype, uint8 qual, uint8 cond, uint8 skill, uint8 level, uint8 direction, int bflag)
{
	item *ip, *env;

	ip = locate_item(tag);
	env = locate_item(loc);

	/* Need to do some special handling if this is the player that is
	 * being updated. */
	if (player->tag == tag)
	{
		player->nrof = 1;
		player->weight = (float) weight / 1000;
		player->face = face;
		get_flags(player, flags);

		player->animation_id = anim;
		player->anim_speed = animspeed;
		player->nrof = nrof;
		player->direction = direction;
	}
	else
	{
		if (ip && ip->env != env)
		{
			remove_item(ip);
			ip = NULL;
		}

		set_item_values(ip ? ip : create_new_item(env, tag, bflag), name, weight, (uint16) face, flags, (uint16) anim, (uint16) animspeed, nrof, itype, stype, qual, cond, skill, level, direction);
	}
}

/**
 * Animate one object.
 * @param ob The object to animate. */
static void animate_object(item *ob)
{
	if (ob->animation_id > 0)
	{
		check_animation_status(ob->animation_id);
	}

	if (ob->animation_id > 0 && ob->anim_speed)
	{
		ob->last_anim++;

		if (ob->last_anim >= ob->anim_speed)
		{
			if (++ob->anim_state >= animations[ob->animation_id].frame)
			{
				ob->anim_state = 0;
			}

			if (ob->direction > animations[ob->animation_id].facings)
			{
				ob->face = animations[ob->animation_id].faces[ob->anim_state];
			}
			else
			{
				ob->face = animations[ob->animation_id].faces[animations[ob->animation_id].frame * ob->direction + ob->anim_state];
			}

			ob->last_anim = 0;
		}
	}
}

/**
 * Animate all possible objects. */
void animate_objects()
{
	item *ob;

	if (player)
	{
		/* For now, only the players inventory needs to be animated */
		for (ob = player->inv; ob; ob = ob->next)
		{
			animate_object(ob);
		}
	}

	if (cpl.below)
	{
		for (ob = cpl.below->inv; ob; ob = ob->next)
		{
			animate_object(ob);
		}
	}

	if (cpl.container)
	{
		for (ob = cpl.sack->inv; ob; ob = ob->next)
		{
			animate_object(ob);
		}
	}

	if (cpl.shop)
	{
		for (ob = cpl.shop->inv; ob; ob = ob->next)
		{
			animate_object(ob);
		}
	}
}
