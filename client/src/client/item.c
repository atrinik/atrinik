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

#include <include.h>
/* Needed for isdigit */
#include <ctype.h>

/* The list of free (unused) items */
static item *free_items;
static item *player;

/* How many items are reserved initially
 * for the item spool */
#define NROF_ITEMS 50

/* This should be modified to read the definition from a file */
void init_item_types()
{
}

/* Item it has gotten an item type, so we need to resort its location */
void update_item_sort(item *it)
{
	item *itmp, *last = NULL;

	/* If not in some environment or the map, return. */
	/* Sorting on the map doesn't work.  In theory, it would be nice,
	 * but the server really must know the map order for things to
	 * work. */
	if (!it->env || it->env == it || it->env == cpl.below)
		return;

	/* If we have the same type as either the previous or next object,
	 * we are already in the right place, so don't change. */
	if ((it->prev && it->prev->type == it->type) || (it->next && it->next->type == it->type))
		return;

	/* Remove this item from the list */
	if (it->prev)
		it->prev->next = it->next;

	if (it->next)
		it->next->prev = it->prev;

	if (it->env->inv == it)
		it->env->inv = it->next;

	for (itmp = it->env->inv; itmp != NULL; itmp = itmp->next)
	{
		/* If the next item is higher in the order, insert here */
		if (itmp->type >= it->type)
		{
			/* If we have a previous object, update the list.  If
			 * not, we need to update the environment to point to us */
			if (last)
			{
				last->next = it;
				it->prev = last;
			}
			else
			{
				it->env->inv = it;
				it->prev = NULL;
			}

			it->next = itmp;
			itmp->prev = it;

			/* Update so we get a redraw */
			it->env->inv_updated = 1;
			return;
		}

		last = itmp;
	}

	/* No match - put it at the end */

	/* If there was a previous item, update pointer.  IF no previous
	 * item, we need to update the environment to point to us */
	if (last)
		last->next = it;
	else
		it->env->inv = it;

	it->prev = last;
	it->next = NULL;
}

/* Stolen from common/item.c */

/* get_number(integer) returns the text-representation of the given number
 * in a static buffer.  The buffer might be overwritten at the next
 * call to get_number().
 * It is currently only used by the query_name() function. */
char *get_number(int i)
{
	static char numbers[21][20] =
	{
		"no",		"a", 		"two", 			"three",	"four",
		"five",		"six",		"seven",		"eight",	"nine",
		"ten",		"eleven",	"twelve",		"thirteen",	"fourteen",
		"fifteen",	"sixteen",	"seventeen",	"eighteen",	"nineteen",
		"twenty"
	};

	if (i <= 20)
		return numbers[i];
	else
	{
		static char buf[MAX_BUF];
		snprintf(buf, sizeof(buf), "%d", i);
		return buf;
	}
}


/*  new_item() returns pointer to new item which
 *  is allocated and initialized correctly */
static item *new_item()
{
	item *op = _malloc(sizeof(item), "new_item (): new item");

	if (!op)
	{
		LOG(LOG_ERROR, "ERROR: new_item(): Out of memory.\n");
		exit(0);
	}

	op->next = op->prev = NULL;
	copy_name(op->d_name, "");
	copy_name(op->s_name, "");
	copy_name(op->p_name, "");
	op->inv = NULL;
	op->env = NULL;
	op->tag = 0;
	op->face = 0;
	op->weight = 0;
	op->magical = op->cursed = op->damned = 0;
	op->traped = op->unpaid = op->locked = op->applied = 0;
	op->flagsval = 0;
	op->animation_id = 0;
	op->last_anim = 0;
	op->anim_state = 0;
	op->nrof = 0;
	op->open = 0;
	op->type = 255;

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
void free_all_items (item *op)
{
	item *tmp;
	void *tmp_free;

	while (op)
	{
		if (op->inv)
			free_all_items(op->inv);

		tmp = op->next;
		tmp_free = &op;
		FreeMemory(tmp_free);
		op = tmp;
	}
}

int locate_item_nr_from_tag(item *op, int tag)
{
	int count = 0;

	for (; op != NULL; count++, op = op->next)
	{
		if (op->tag == tag)
			return count;
	}

	return -1;
}

int locate_item_tag_from_nr(item *op, int nr)
{
	int count = 0;

	for (; op != NULL; op = op->next, count++)
	{
		if (count == nr)
			return op->tag;
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
			return op;
	}

	return NULL;
}

/* Recursive function, used by locate_item() */
item *locate_item_from_item (item *op, sint32 tag)
{
	item *tmp;

	for (; op != NULL; op = op->next)
	{
		if (op->tag == tag)
			return op;
		else if (op->inv)
		{
			if ((tmp = locate_item_from_item(op->inv, tag)))
				return tmp;
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
		return cpl.below;

	if (tag == -1)
		return cpl.sack;

	if (tag == -2)
	{
		return cpl.shop;
	}

	if (cpl.below && (op = locate_item_from_item(cpl.below->inv, tag)) != NULL)
		return op;

	if (cpl.shop && (op = locate_item_from_item(cpl.shop->inv, tag)) != NULL)
		return op;

	if (cpl.sack && (op = locate_item_from_item(cpl.sack->inv, tag)) != NULL)
		return op;

	if ((op = locate_item_from_item(player, tag)) != NULL)
		return op;

	return NULL;
}

/* remove_item() inserts op the the list of free items
 * Note that it don't clear all fields in item */
void remove_item(item *op)
{
	/* IF no op, or it is the player */
	if (!op || op == player || op == cpl.below || op == cpl.sack)
		return;

	op->env->inv_updated = 1;

	/* Do we really want to do this? */
	if (op->inv)
		remove_item_inventory (op);

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

	if (op->next != NULL)
		op->next->prev = op;

	free_items = op;

	/* Clear all these values, since this item will get re-used */
	op->prev = NULL;
	op->env = NULL;
	op->tag = 0;
	copy_name(op->d_name, "");
	copy_name(op->s_name, "");
	copy_name(op->p_name, "");
	op->inv = NULL;
	op->env = NULL;
	op->tag = 0;
	op->face = 0;
	op->weight = 0;
	op->magical = op->cursed = op->damned = 0;
	op->unpaid = op->locked = op->applied = 0;
	op->flagsval = 0;
	op->animation_id = 0;
	op->last_anim = 0;
	op->anim_state = 0;
	op->nrof = 0;
	op->open = 0;
	op->type = 255;
}

/* remove_item_inventory() recursive frees items inventory */
void remove_item_inventory(item *op)
{
	if (!op)
		return;

	op->inv_updated = 1;

	while (op->inv)
		remove_item(op->inv);
}

/* We adding it now to the start inv.
 *  OLD: add_item() adds item op to end of the inventory of item env */
static void add_item(item *env, item *op, int bflag)
{
	item *tmp;

	if (!op)
		return;

	if (bflag == 0)
	{
		op->next = env->inv;

		if (op->next)
			op->next->prev = op;

		op->prev = NULL;
		env->inv = op;
		op->env = env;
	}
	/* Sort reverse - for inventory lists */
	else
	{
		for (tmp = env->inv; tmp && tmp->next; tmp = tmp->next);

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
				tmp->next->prev = op;

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
		free_items = alloc_items(NROF_ITEMS);

	op = free_items;
	free_items = free_items->next;

	if (free_items)
		free_items->prev = NULL;

	op->tag = tag;
	op->locked = 0;

	if (env)
		add_item(env, op, bflag);

	return op;
}

/*  Hardcoded now, server could send these at initiation phase. */
static char *apply_string[] =
{
	"", " (readied)", " (wielded)", " (worn)", " (active)", " (applied)"
};

static void set_flag_string(item *op)
{
	op->flags[0] = 0;

	if (op->locked)
		strcat(op->flags, " *");

	if (op->apply_type)
	{
		if (op->apply_type < sizeof (apply_string) / sizeof(apply_string[0]))
			strcat(op->flags, apply_string[op->apply_type]);
		else
			strcat(op->flags, " (undefined)");
	}

	if (op->open)
		strcat(op->flags, " (open)");

	if (op->damned)
		strcat(op->flags, " (damned)");

	if (op->cursed)
		strcat(op->flags, " (cursed)");

	if (op->magical)
		strcat(op->flags, " (magic)");

	if (op->unpaid)
		strcat(op->flags, " (unpaid)");
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
	op->traped = flags & F_TRAPED ? 1 : 0;
	op->flagsval = flags;
	op->apply_type = flags & F_APPLIED;
	set_flag_string(op);
}

/*  get_nrof() functions tries to get number of items from the item name */
static sint32 get_nrof(char *name)
{
	static char *numbers[21] =
	{
		"no ",		"a ",		"two ",			"three ",		"four ",
		"five ",	"six ",		"seven ",		"eight ",		"nine ",
		"ten ",		"eleven ",	"twelve ",		"thirteen ",	"fourteen ",
		"fifteen ",	"sixteen ",	"seventeen ",	"eighteen ",	"nineteen ",
		"twenty "
	};

	static char *numbers_10[10] =
	{
		"zero ",	"ten ",		"twenty ",		"thirty ",	"fourty ",
		"fifty ",	"sixty ",	"seventy ",		"eighty ",	"ninety "
	};

	sint32 nrof = 0;
	int i;

	if (isdigit(*name))
		nrof = atol(name);
	else if (strncmp(name, "a ", 2) == 0 || strncmp(name, "an ", 3) == 0)
		nrof = 1;
	else
	{
		for (i = 1; i < (int) sizeof(numbers) / (int) sizeof(numbers[0]); i++)
		{
			if (strncmp(name, numbers[i], strlen(numbers[i])) == 0)
			{
				nrof = i;
				break;
			}
		}

		if (!nrof)
		{
			for (i = 1; i < (int) sizeof(numbers_10) / (int) sizeof(numbers_10[0]); i++)
			{
				if (strncmp(name, numbers_10[i], strlen(numbers_10[i])) == 0)
				{
					nrof = i * 10;
					break;
				}
			}
		}
	}

	return nrof ? nrof : 1;
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
		copy_name(op->s_name, "Warning: Old name cmd! This is a bug.");
	}
	/* we have a nrof - item1 command */
	else
	{
		/* Program always expect at least 1 object internal */
		if (nrof == 0)
			nrof = 1;

		op->nrof = nrof;

		if (*name != '\0')
		{
			copy_name(op->s_name, name);
		}
	}

	if (op->env)
		op->env->inv_updated = 1;

	op->weight = (float) weight / 1000;

	if (itype != 254)
		op->itype = itype;

	if (stype != 254)
		op->stype = stype;

	if (qual != 254)
		op->item_qua = qual;

	if (cond != 254)
		op->item_con = cond;

	if (skill != 254)
		op->item_skill = skill;

	if (level != 254)
		op->item_level = level;

	op->face = face;
	op->animation_id = anim;
	op->anim_speed = animspeed;
	op->direction = dir;
	get_flags(op, flags);

	/* We don't sort the map, so lets not do this either */
	if (op->env != cpl.below)
		op->type = 255;

	update_item_sort(op);
}

void fire_command(char *buf)
{
	SockList sl;

	sl.buf = (unsigned char *) buf;
	sl.len = strlen(buf);
	send_socklist(csocket.fd, sl);
}

void combat_command(char *buf)
{
	SockList sl;

	sl.buf = (unsigned char *) buf;
	sl.len = strlen(buf);
	send_socklist(csocket.fd, sl);
}

void toggle_locked(item *op)
{
	SockList sl;
	char buf[MAX_BUF];

	/* if item is on the ground, don't lock it */
	if (!op || !op->env || op->env->tag == 0)
		return;

	sl.buf = (unsigned char *) buf;
	strcpy((char *) sl.buf, "lock ");
	sl.len = 5;

	if (op->locked)
		sl.buf[sl.len++] = 0;
	else
		sl.buf[sl.len++] = 1;

	SockList_AddInt(&sl, op->tag);
	send_socklist(csocket.fd, sl);
}

void send_mark_obj(item *op)
{
	SockList sl;
	char buf[MAX_BUF];

	/* if item is on the ground, don't mark it */
	if (!op || !op->env || op->env->tag == 0)
		return;

	sl.buf = (unsigned char *) buf;
	strcpy((char *) sl.buf, "mark ");
	sl.len = 5;
	SockList_AddInt(&sl, op->tag);
	send_socklist(csocket.fd, sl);
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
		copy_name(player->d_name, name);

		/* I don't think this makes sense, as you can have
		 * two players merged together, so nrof should always be one */
		player->nrof = get_nrof(name);
		player->weight = (float) weight / 1000;
		player->face = face;
		get_flags(player, flags);

		if (player->inv)
			player->inv->inv_updated = 1;

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

		set_item_values(ip ? ip : create_new_item(env, tag, bflag), name, weight, (uint16) face, flags, (uint16)anim, (uint16) animspeed, nrof, itype, stype, qual, cond, skill, level, direction);
	}
}

/* Prints players inventory, contain extra information for debugging purposes
 * This isn't pretty, but is only used for debugging, so it doesn't need to be. */
void print_inventory(item *op)
{
	char buf[MAX_BUF];
	char buf2[MAX_BUF];
	item *tmp;
	static int l = 0;
	/*int info_width = 20;*/

	if (l == 0)
	{
		snprintf(buf, sizeof(buf), "AA %s's inventory (%d):", op->d_name, op->tag);
		snprintf(buf2, sizeof(buf2), "BB %s%6.1f kg", buf, op->weight);
		draw_info(buf2, COLOR_DEFAULT);
	}

	l += 2;
	for (tmp = op->inv; tmp; tmp = tmp->next)
	{
		snprintf(buf, sizeof(buf), "CC %*s- %d %s%s (%d)", l - 2, "", tmp->nrof, tmp->d_name, tmp->flags, tmp->tag);
		draw_info(buf, COLOR_DEFAULT);

		if (tmp->inv)
			print_inventory(tmp);
	}

	l -= 2;
}

/* Check the objects, animate the ones as necessary */
void animate_objects()
{
	item *ip;
	int got_one = 0;

	if (player)
	{
		/* For now, only the players inventory needs to be animated */
		for (ip = player->inv; ip; ip = ip->next)
		{
			if (ip->animation_id > 0)
				check_animation_status(ip->animation_id);

			if (ip->animation_id > 0 && ip->anim_speed)
			{
				ip->last_anim++;

				if (ip->last_anim >= ip->anim_speed)
				{
					if (++ip->anim_state >= animations[ip->animation_id].frame)
						ip->anim_state = 0;

					if (ip->direction > animations[ip->animation_id].facings)
						ip->face = animations[ip->animation_id].faces[ip->anim_state];
					else
						ip->face = animations[ip->animation_id].faces[animations[ip->animation_id].frame * ip->direction + ip->anim_state];

					ip->last_anim = 0;
					got_one = 1;
				}
			}
		}
	}

	if (cpl.below)
	{
		for (ip = cpl.below->inv; ip; ip = ip->next)
		{
			if (ip->animation_id > 0)
				check_animation_status(ip->animation_id);

			if (ip->animation_id > 0 && ip->anim_speed)
			{
				ip->last_anim++;

				if (ip->last_anim >= ip->anim_speed)
				{
					if (++ip->anim_state >= animations[ip->animation_id].frame)
						ip->anim_state = 0;

					if (ip->direction > animations[ip->animation_id].facings)
						ip->face = animations[ip->animation_id].faces[ip->anim_state];
					else
						ip->face = animations[ip->animation_id].faces[animations[ip->animation_id].frame * ip->direction + ip->anim_state];

					ip->last_anim = 0;
					got_one = 1;
				}
			}
		}
	}

	if (cpl.container)
	{
		for (ip = cpl.sack->inv; ip; ip = ip->next)
		{
			if (ip->animation_id > 0)
				check_animation_status(ip->animation_id);

			if (ip->animation_id > 0 && ip->anim_speed)
			{
				ip->last_anim++;

				if (ip->last_anim >= ip->anim_speed)
				{
					if (++ip->anim_state >= animations[ip->animation_id].frame)
						ip->anim_state = 0;

					if (ip->direction > animations[ip->animation_id].facings)
						ip->face = animations[ip->animation_id].faces[ip->anim_state];
					else
						ip->face = animations[ip->animation_id].faces[animations[ip->animation_id].frame * ip->direction + ip->anim_state];

					ip->last_anim = 0;
					got_one = 1;
				}
			}
		}
	}

	if (cpl.shop)
	{
		for (ip = cpl.shop->inv; ip; ip = ip->next)
		{
			if (ip->animation_id > 0)
				check_animation_status(ip->animation_id);

			if (ip->animation_id > 0 && ip->anim_speed)
			{
				ip->last_anim++;

				if (ip->last_anim >= ip->anim_speed)
				{
					if (++ip->anim_state >= animations[ip->animation_id].frame)
						ip->anim_state = 0;

					if (ip->direction > animations[ip->animation_id].facings)
						ip->face = animations[ip->animation_id].faces[ip->anim_state];
					else
						ip->face = animations[ip->animation_id].faces[animations[ip->animation_id].frame * ip->direction + ip->anim_state];

					ip->last_anim = 0;
					got_one = 1;
				}
			}
		}
	}
}
