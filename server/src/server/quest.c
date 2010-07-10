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
 * Quest related code. */

#include <global.h>

/**
 * Add a one drop quest item to player's quest container, to mark that
 * it has been dropped for that player and will never drop for him again.
 * @param op The player.
 * @param quest_name Name of the quest. */
static void add_one_drop_quest_item(object *op, const char *quest_name)
{
	object *quest_container = get_archetype(QUEST_CONTAINER_ARCHETYPE);

	/* Mark this quest as completed. */
	quest_container->magic = QUEST_STATUS_COMPLETED;
	/* Store the quest name. */
	FREE_AND_COPY_HASH(quest_container->name, quest_name);
	/* Insert it inside player's quest container. */
	insert_ob_in_ob(quest_container, CONTR(op)->quest_container);
}

/**
 * Find a quest inside player's quest container.
 * @param op The player object.
 * @param quest_name Name of the quest.
 * @return The object that is used to represent the quest in the quest
 * container, NULL if no matching quest found. */
static object *find_quest(object *op, const char *quest_name)
{
	object *tmp;

	/* Go through the objects in the quest container */
	for (tmp = CONTR(op)->quest_container->inv; tmp; tmp = tmp->below)
	{
		if (tmp->name == quest_name)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Check if specified player object has a given quest item in their
 * inventory (name and arch name are compared).
 * @note This function is recursive and will call itself on any
 * non-system inventories inside the player until it finds a matching item.
 * @param op The player object.
 * @param quest_item The quest item we'll be comparing values from.
 * @param flag Flag to compare the quest item against, 0 for no flag comparison.
 * @return 1 if the player has the quest item, 0 otherwise. */
static int has_quest_item(object *op, object *quest_item, sint32 flag)
{
	object *tmp;

	/* Go through the objects in the object's inventory. */
	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		/* Compare the values. */
		if (tmp->name == quest_item->name && tmp->arch->name == quest_item->arch->name && (!flag || QUERY_FLAG(tmp, flag)))
		{
			return 1;
		}

		/* If it has inventory and is not a system object, go on recursively. */
		if (tmp->inv && !QUERY_FLAG(tmp, FLAG_SYS_OBJECT))
		{
			int ret = has_quest_item(tmp, quest_item, flag);

			if (ret)
			{
				return 1;
			}
		}
	}

	return 0;
}

/**
 * When a monster drops inventory and there is quest container object in
 * it, this function is called to parse the quest container and its
 * contents for any possible quests player may be running.
 * @warning <b>ONLY</b> call on player objects.
 * @todo Add support for multiple objects in quest_container's inventory?
 * @param op The player object.
 * @param quest_container The quest container. */
void check_quest(object *op, object *quest_container)
{
	char buf[HUGE_BUF];
	object *quest_object = find_quest(op, quest_container->name), *tmp;

	/* If this is not a one-drop item quest, it must first be accepted. */
	if (quest_container->sub_type != QUEST_TYPE_ITEM && (!quest_object || quest_object->magic == QUEST_STATUS_COMPLETED))
	{
		return;
	}

	tmp = quest_container->inv;

	/* Check for events in this quest container. */
	if (HAS_EVENT(quest_container, EVENT_TRIGGER))
	{
		/* Advance through the quest object's inventory, skipping the
		 * event object. */
		if (tmp->type == EVENT_OBJECT)
		{
			tmp = tmp->below;
		}

		if (trigger_event(EVENT_TRIGGER, op, quest_container, tmp, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
		{
			return;
		}
	}

	switch (quest_container->sub_type)
	{
		case QUEST_TYPE_ITEM:
		{
			object *clone;
			int one_drop;

			if (!tmp)
			{
				return;
			}

			one_drop = QUERY_FLAG(tmp, FLAG_ONE_DROP);

			/* Not one-drop, but we already have the quest object (keys,
			 * for example). */
			if (!one_drop && has_quest_item(op, tmp, 0))
			{
				return;
			}
			/* One-drop and we already found this quest object before
			 * (Rhun's cloak, for example). */
			else if (one_drop && quest_object)
			{
				return;
			}

			clone = get_object();
			copy_object_with_inv(tmp, clone);
			SET_FLAG(clone, FLAG_IDENTIFIED);
			/* Insert the quest item inside the player. */
			insert_ob_in_ob(clone, op);
			esrv_send_item(op, clone);

			if (one_drop)
			{
				/* So the item will never drop again for this player. */
				add_one_drop_quest_item(op, quest_container->name);
				snprintf(buf, sizeof(buf), "You solved the one drop quest %s!\n", quest_container->name);
			}
			else
			{
				snprintf(buf, sizeof(buf), "You found the special drop %s!\n", query_short_name(clone, NULL));
			}

			new_draw_info(NDI_UNIQUE | NDI_NAVY | NDI_ANIM, op, buf);
			play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "event01.ogg", 0, 0, 0, 0);
			break;
		}

		case QUEST_TYPE_KILL:
			/* The +1 makes it so no messages are displayed when player
			 * kills more same monsters. */
			if (quest_object->last_sp <= quest_object->last_grace + 1)
			{
				quest_object->last_sp++;
			}

			/* Display an informative message about the quest status. */
			if (quest_object->last_sp <= quest_object->last_grace)
			{
				if (quest_object->last_sp == quest_object->last_grace)
				{
					snprintf(buf, sizeof(buf), "Quest '%s' completed!\n", quest_container->name);
					play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "event01.ogg", 0, 0, 0, 0);
				}
				else
				{
					snprintf(buf, sizeof(buf), "Quest %s: %d/%d.\n", quest_container->name, quest_object->last_sp, quest_object->last_grace);
				}

				new_draw_info(NDI_UNIQUE | NDI_NAVY | NDI_ANIM, op, buf);
			}

			break;

		case QUEST_TYPE_KILL_ITEM:
		{
			object *clone;

			/* Have we found this item already? */
			if (!tmp || has_quest_item(op, tmp, FLAG_QUEST_ITEM))
			{
				return;
			}

			clone = get_object();
			copy_object_with_inv(tmp, clone);
			SET_FLAG(clone, FLAG_QUEST_ITEM);
			SET_FLAG(clone, FLAG_STARTEQUIP);
			CLEAR_FLAG(clone, FLAG_SYS_OBJECT);
			/* Insert the quest item inside the player. */
			insert_ob_in_ob(clone, op);
			esrv_send_item(op, clone);

			new_draw_info_format(NDI_UNIQUE | NDI_NAVY | NDI_ANIM, op, "Quest %s: You found the quest item %s!\n", quest_container->name, query_base_name(clone, NULL));
			play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "event01.ogg", 0, 0, 0, 0);
			break;
		}

		default:
			LOG(llevBug, "BUG: Quest object '%s' has unknown sub type (%d).\n", quest_container->name, quest_container->sub_type);
	}
}
