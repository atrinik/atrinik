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
 * Handles code for handling @ref CONTAINER "containers". */

#include <global.h>

static int container_trap(object *op, object *container);

/**
 * Handle apply on containers.
 * @note There are three states for any container - closed (not applied),
 * applied (not open, but objects that match get tossed into it) and open
 * (appled flag set, and op->container points to the open container).
 * @param op The player.
 * @param sack The container the player is opening or closing.
 * @return 1 if an object is apllied somehow or another, 0 if error/no
 * apply. */
int esrv_apply_container(object *op, object *sack)
{
	object *cont, *tmp;

	if (op->type != PLAYER)
	{
		LOG(llevBug, "BUG: esrv_apply_container: called from non player: <%s>!\n", query_name(op, NULL));
		return 0;
	}

	/* cont is NULL or the container player already has opened */
	cont = CONTR(op)->container;

	if (sack == NULL || sack->type != CONTAINER || (cont && cont->type != CONTAINER))
	{
		LOG(llevBug, "BUG: esrv_apply_container: object *sack = %s is not container (cont:<%s>)!\n", query_name(sack, NULL), query_name(cont, NULL));
		return 0;
	}

	/* close container?
	 * if cont != sack || cont == sack - in both cases we close cont */
	if (cont)
	{
		/* Trigger the CLOSE event */
		if (trigger_event(EVENT_CLOSE, op, cont, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL))
		{
			return 1;
		}

		if (container_unlink(CONTR(op), cont))
		{
			new_draw_info_format(NDI_UNIQUE, op, "You close %s.", query_name(cont, op));
		}
		else
		{
			new_draw_info_format(NDI_UNIQUE, op, "You leave %s.", query_name(cont, op));
		}

		/* we closing the one we applied */
		if (cont == sack)
		{
			return 1;
		}
	}

	/* at this point we ready a container OR we open it! */

	/* If the player is trying to open it (which he must be doing if we got here),
	 * and it is locked, check to see if player has the equipment to open it. */
	if (sack->slaying || sack->stats.maxhp)
	{
		/* Locked container */
		if (sack->sub_type == ST1_CONTAINER_NORMAL)
		{
			tmp = find_key(op, sack);

			if (tmp)
			{
				if (tmp->type == KEY)
				{
					new_draw_info_format(NDI_UNIQUE, op, "You unlock %s with %s.", query_name(sack, op), query_name(tmp, op));
				}
				else if (tmp->type == FORCE)
				{
					new_draw_info_format(NDI_UNIQUE, op, "The %s is unlocked for you.", query_name(sack, op));
				}
			}
			else
			{
				new_draw_info_format(NDI_UNIQUE, op, "You don't have the key to unlock %s.", query_name(sack, op));
				return 0;
			}
		}
		/* Personalized container */
		else
		{
			/* Party corpse */
			if (sack->sub_type == ST1_CONTAINER_CORPSE_party && !party_can_open_corpse(op, sack))
			{
				return 0;
			}
			/* Only give player with right name access */
			else if (sack->sub_type == ST1_CONTAINER_CORPSE_player && sack->slaying != op->name)
			{
				new_draw_info(NDI_UNIQUE, op, "It's not your bounty.");
				return 0;
			}
		}
	}

	SET_FLAG(sack, FLAG_BEEN_APPLIED);

	/* By the time we get here, we have made sure any other container has been closed and
	 * if this is a locked container, the player they key to open it. */

	/* There are really two cases - the sack is either on the ground, or the sack is
	 * part of the players inventory.  If on the ground, we assume that the player is
	 * opening it, since if it was being closed, that would have been taken care of above.
	 * If it in the players inventory, we can READY the container. */
	/* container is NOT in players inventory */
	if (sack->env != op)
	{
		/* this is not possible - opening a container inside another container or an another player */
		if (sack->env)
		{
			new_draw_info_format(NDI_UNIQUE, op, "You can't open %s.", query_name(sack, op));
			return 0;
		}

		new_draw_info_format(NDI_UNIQUE, op, "You open %s.", query_name(sack, op));
		container_link(CONTR(op), sack);

		if (sack->slaying && sack->sub_type == ST1_CONTAINER_CORPSE_party)
		{
			party_handle_corpse(op, sack);
		}
	}
	/* Sack is in player's inventory */
	else
	{
		/* readied sack becoming open */
		if (QUERY_FLAG (sack, FLAG_APPLIED))
		{
			new_draw_info_format(NDI_UNIQUE, op, "You open %s.", query_name(sack, op));
			container_link(CONTR(op), sack);
		}
		else
		{
			CLEAR_FLAG (sack, FLAG_APPLIED);
			new_draw_info_format(NDI_UNIQUE, op, "You readied %s.", query_name(sack, op));
			SET_FLAG (sack, FLAG_APPLIED);
			update_object(sack, UP_OBJ_FACE);
			esrv_update_item(UPD_FLAGS, op, sack);
			/* search & explode a rune in the container */
			container_trap(op, sack);
		}
	}

	return 1;
}

/**
 * A player has opened a container - link him to the list of players
 * which have (perhaps) it opened too.
 * @param pl The player object.
 * @param sack The container.
 * @return 1 if we are the first opening this container, 0 otherwise. */
int container_link(player *pl, object *sack)
{
	int ret = 0;

	/* For safety reasons, let's check this is valid */
	if (sack->attacked_by)
	{
		if (sack->attacked_by->type != PLAYER || !CONTR(sack->attacked_by) || CONTR(sack->attacked_by)->container != sack)
		{
			LOG(llevBug, "BUG: container_link() - invalid player linked: <%s>\n", query_name(sack->attacked_by, NULL));
			sack->attacked_by = NULL;
		}
	}

	/* the open/close logic should be handled elsewhere.
	 * for that reason, this function should only be called
	 * when valid - broken open/close logic elsewhere is bad.
	 * so, give a bug warning out! */
	if (pl->container)
	{
		LOG(llevBug, "BUG: container_link() - called from player with open container!: <%s> sack:<%s>\n", query_name(sack->attacked_by, NULL), query_name(sack, NULL));
		container_unlink(pl, sack);
	}

	/* Check for quest containers. */
	if (HAS_EVENT(sack, EVENT_QUEST))
	{
		object *tmp;

		for (tmp = sack->inv; tmp; tmp = tmp->below)
		{
			if (tmp->type == QUEST_CONTAINER)
			{
				check_quest(pl->ob, tmp);
			}
		}
	}

	pl->container = sack;
	pl->container_count = sack->count;

	pl->container_above = sack->attacked_by;

	if (sack->attacked_by)
	{
		CONTR(sack->attacked_by)->container_below = pl->ob;
	}
	/* we are the first one opening this container */
	else
	{
		SET_FLAG(sack, FLAG_APPLIED);

		/* faking open container face */
		if (sack->other_arch)
		{
			sack->face = sack->other_arch->clone.face;
			sack->animation_id = sack->other_arch->clone.animation_id;

			if (sack->animation_id)
			{
				SET_ANIMATION(sack, (NUM_ANIMATIONS(sack) / NUM_FACINGS(sack)) * sack->direction);
			}

			update_object(sack, UP_OBJ_FACE);
		}

		update_object(sack, UP_OBJ_FACE);
		esrv_update_item (UPD_FLAGS|UPD_FACE, pl->ob, sack);
		container_trap(pl->ob, sack);
		ret = 1;
	}

	esrv_send_inventory(pl->ob, sack);
	/* we are first element */
	pl->container_below = NULL;
	sack->attacked_by = pl->ob;
	sack->attacked_by_count = pl->ob->count;

	return ret;
}

/**
 * Remove a player from the container list.
 *
 * Unlinking is a bit more tricky - pl OR sack can be NULL.
 * @param pl The player object. If NULL, we unlink all players from the
 * container identified by 'sack'.
 * @param sack The container object. If NULL, unlink this container from
 * player object identified by 'pl'. */
int container_unlink(player *pl, object *sack)
{
	object *tmp, *tmp2;

	if (pl == NULL && sack == NULL)
	{
		LOG(llevBug, "BUG: container_unlink() - *pl AND *sack == NULL!\n");
		return 0;
	}

	if (pl)
	{
		if (!pl->container)
		{
			return 0;
		}

		if (pl->container->count != pl->container_count)
		{
			pl->container = NULL;
			pl->container_count = 0;
			return 0;
		}

		sack = pl->container;
		update_object(sack, UP_OBJ_FACE);
		esrv_close_container(pl->ob);

		/* ok, there is a valid container - unlink the player now */

		/* we are only applier */
		if (!pl->container_below && !pl->container_above)
		{
			/* we should be that object... */
			if (pl->container->attacked_by != pl->ob)
			{
				LOG(llevBug, "BUG: container_unlink() - container link don't match player!: <%s> sack:<%s> (%s)\n", query_name(pl->ob, NULL), query_name(sack->attacked_by, NULL), query_name(sack, NULL));
				return 0;
			}

			pl->container = NULL;
			pl->container_count = 0;

			CLEAR_FLAG(sack, FLAG_APPLIED);

			if (sack->other_arch)
			{
				sack->face = sack->arch->clone.face;
				sack->animation_id = sack->arch->clone.animation_id;

				if (sack->animation_id)
				{
					SET_ANIMATION(sack, (NUM_ANIMATIONS(sack) / NUM_FACINGS(sack)) * sack->direction);
				}

				update_object(sack, UP_OBJ_FACE);
			}

			sack->attacked_by = NULL;
			sack->attacked_by_count = 0;
			esrv_update_item (UPD_FLAGS|UPD_FACE, pl->ob, sack);
			return 1;
		}

		/* because there is another player applying that container, we don't close it */

		/* we are first player in list */
		if (!pl->container_below)
		{
			/* mark above as first player applying this container */
			sack->attacked_by = pl->container_above;
			sack->attacked_by_count = pl->container_above->count;
			CONTR(pl->container_above)->container_below = NULL;

			pl->container_above = NULL;
			pl->container = NULL;
			pl->container_count = 0;
			return 0;
		}

		/* we are somehwere in the middle or last one - it don't matter */
		CONTR(pl->container_below)->container_above = pl->container_above;

		if (pl->container_above)
		{
			CONTR(pl->container_above)->container_below = pl->container_below;
		}

		pl->container_below=NULL;
		pl->container_above=NULL;
		pl->container = NULL;
		pl->container_count = 0;
		return 0;
	}

	CLEAR_FLAG(sack, FLAG_APPLIED);

	if (sack->other_arch)
	{
		sack->face = sack->arch->clone.face;
		sack->animation_id = sack->arch->clone.animation_id;

		if (sack->animation_id)
		{
			SET_ANIMATION(sack, (NUM_ANIMATIONS(sack) / NUM_FACINGS(sack)) * sack->direction);
		}

		update_object(sack, UP_OBJ_FACE);
	}

	tmp = sack->attacked_by;
	sack->attacked_by = NULL;
	sack->attacked_by_count = 0;

	/* if we are here, we are called with (NULL,sack) */
	while (tmp)
	{
		/* valid player in list? */
		if (!CONTR(tmp) || CONTR(tmp)->container != sack)
		{
			LOG(llevBug,"BUG: container_unlink() - container link list mismatch!: player?:<%s> sack:<%s> (%s)\n", query_name(tmp, NULL), query_name(sack, NULL), query_name(sack->attacked_by, NULL));
			return 1;
		}

		tmp2 = CONTR(tmp)->container_above;
		CONTR(tmp)->container = NULL;
		CONTR(tmp)->container_count = 0;
		CONTR(tmp)->container_below = NULL;
		CONTR(tmp)->container_above = NULL;
		esrv_update_item(UPD_FLAGS|UPD_FACE, tmp, sack);
		esrv_close_container(tmp);
		tmp = tmp2;
	}

	return 1;
}

/**
 * Frees a monster trapped in container when opened by a player.
 * @param monster The monster trapped.
 * @param op The player that opened the container. */
void free_container_monster(object *monster, object *op)
{
	int i;
	object *container = monster->env;

	if (container == NULL)
	{
		return;
	}

	/* in container, no walk off check */
	remove_ob(monster);
	monster->x = container->x;
	monster->y = container->y;
	i = find_free_spot(monster->arch, NULL, op->map, monster->x, monster->y, 0, 9);

	if (i != -1)
	{
		monster->x += freearr_x[i];
		monster->y += freearr_y[i];
	}

	fix_monster(monster);

	if (insert_ob_in_map(monster, op->map, monster, 0))
	{
		new_draw_info_format(NDI_UNIQUE, op, "A %s jumps out of the %s.", query_name(monster, NULL), query_name(container, NULL));
	}
}

/**
 * Examine the items in a container which gets readied or opened by a
 * player.
 *
 * Explode or trigger every trap and rune in there and free trapped
 * monsters.
 * @param op The player opening the container.
 * @param container The container object.
 * @return 0 if no trap or monster found/exploded/freed, count of all
 * found/exploded/freed traps and monsters otherwise. */
static int container_trap(object *op, object *container)
{
	int ret = 0;
	object *tmp;

	for (tmp = container->inv; tmp; tmp = tmp->below)
	{
		/* Search for traps and runes */
		if (tmp->type == RUNE)
		{
			ret++;
			spring_trap(tmp, op);
		}
		/* Search for monsters living in containers */
		else if (tmp->type == MONSTER)
		{
			ret++;
			free_container_monster(tmp, op);
		}
	}

	return ret;
}

/**
 * We don't to allow putting magical container inside another magical
 * container, so we check for it here.
 * @param op Object being put into the container.
 * @param container The container.
 * @return 1 if both op and container are magical containers, 0 otherwise. */
int check_magical_container(object *op, object *container)
{
	if (op->type == CONTAINER && container->type == CONTAINER && op->weapon_speed != 1.0f && container->weapon_speed != 1.0f)
	{
		return 1;
	}

	return 0;
}
