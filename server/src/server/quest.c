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

static void add_one_drop_quest_item(object *op, object *quest_item, const char *quest_name);
static object *find_quest(object *op, const char *quest_name);
static int has_quest_item(object *op, object *quest_item, sint32 flag);

/**
 * Create a quest container inside the specified object.
 * @param op The object that will get the new quest container.
 * @return The newly created container. */
object *create_quest_container(object *op)
{
	object *quest_container = get_object();

	copy_object(get_archetype(QUEST_CONTAINER_ARCHETYPE), quest_container, 0);
	insert_ob_in_ob(quest_container, op);

	return quest_container;
}

/**
 * When a monster drops inventory and there is quest container object in
 * it, this function is called to parse the quest container and its
 * contents for any possible quests player may be running.
 * @warning <b>ONLY</b> call on player objects.
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

	/* This allows new quest types to be added fairly easily. */
	switch (quest_container->sub_type)
	{
		case QUEST_TYPE_ITEM:
			/* Sanity checks. */
			if (!tmp || (!QUERY_FLAG(tmp, FLAG_ONE_DROP) && has_quest_item(op, tmp, 0)) || (QUERY_FLAG(tmp, FLAG_ONE_DROP) && quest_object))
			{
				return;
			}

			/* Remove the object first. */
			remove_ob(tmp);
			/* Now add it to the player. */
			add_one_drop_quest_item(op, tmp, quest_container->name);
			snprintf(buf, sizeof(buf), "You solved the one drop quest %s!\n", quest_container->name);
			new_draw_info(NDI_UNIQUE | NDI_NAVY | NDI_ANIM, op, buf);
			play_sound_player_only(CONTR(op), SOUND_LEVEL_UP, NULL, 0, 0, 0, 0);
			esrv_send_item(op, tmp);

			break;

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
					play_sound_player_only(CONTR(op), SOUND_LEVEL_UP, NULL, 0, 0, 0, 0);
				}
				else
				{
					snprintf(buf, sizeof(buf), "Quest %s: %d/%d.\n", quest_container->name, quest_object->last_sp, quest_object->last_grace);
				}

				new_draw_info(NDI_UNIQUE | NDI_NAVY | NDI_ANIM, op, buf);
			}

			break;

		case QUEST_TYPE_KILL_ITEM:
			/* Sanity checks. */
			if (!tmp || has_quest_item(op, tmp, FLAG_QUEST_ITEM))
			{
				return;
			}

			/* Remove the object first. */
			remove_ob(tmp);

			/* Set and clear common flags. */
			SET_FLAG(tmp, FLAG_QUEST_ITEM);
			SET_FLAG(tmp, FLAG_STARTEQUIP);
			CLEAR_FLAG(tmp, FLAG_SYS_OBJECT);
			/* Insert the object to the player. */
			insert_ob_in_ob(tmp, op);
			snprintf(buf, sizeof(buf), "Quest %s: You found the quest item %s!\n", quest_container->name, query_base_name(tmp, NULL));
			new_draw_info(NDI_UNIQUE | NDI_NAVY | NDI_ANIM, op, buf);
			play_sound_player_only(CONTR(op), SOUND_LEVEL_UP, NULL, 0, 0, 0, 0);
			esrv_send_item(op, tmp);

			break;

		default:
			LOG(llevBug, "BUG: Quest object '%s' has unknown sub type (%d).\n", quest_container->name, quest_container->sub_type);
	}
}

/**
 * Add a one drop quest item to player's quest container, to mark that
 * it has been dropped for that player and will never drop for him again.
 * @param op The player.
 * @param quest_item The quest object we're going to give to the player.
 * @param quest_name Name of the quest. */
static void add_one_drop_quest_item(object *op, object *quest_item, const char *quest_name)
{
	object *quest_container = present_in_ob(QUEST_CONTAINER, op), *quest_item_tmp = get_object();

	/* If the quest container doesn't exist yet, create it. */
	if (!quest_container)
	{
		quest_container = create_quest_container(op);
	}

	/* Copy the object data. */
	copy_object(quest_item, quest_item_tmp, 1);
	/* Remove it from the active list. */
	quest_item_tmp->speed = 0.0f;
	/* Mark this quest as completed. */
	quest_item_tmp->magic = QUEST_STATUS_COMPLETED;
	/* Clear some flags the quest marker shouldn't have. */
	CLEAR_FLAG(quest_item_tmp, FLAG_ANIMATE);
	CLEAR_FLAG(quest_item_tmp, FLAG_ALIVE);
	/* Store the quest name. */
	FREE_AND_COPY_HASH(quest_item_tmp->name, quest_name);
	/* Insert it inside the quest container. */
	insert_ob_in_ob(quest_item_tmp, quest_container);

	SET_FLAG(quest_item, FLAG_IDENTIFIED);
	/* Insert the quest item inside the player. */
	insert_ob_in_ob(quest_item, op);
}

/**
 * Find a quest inside player's quest container.
 * @param op The player object.
 * @param quest_name Name of the quest.
 * @return The object that is used to represent the quest in the quest
 * container, NULL if no matching quest found. */
static object *find_quest(object *op, const char *quest_name)
{
	object *tmp = present_in_ob(QUEST_CONTAINER, op);

	/* No quest container? */
	if (!tmp)
	{
		return NULL;
	}

	/* Go through the objects in the quest container */
	for (tmp = tmp->inv; tmp; tmp = tmp->below)
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
 * inventories inside the player until it finds a matching item.
 * @param op The player object.
 * @param quest_item The quest item we'll be comparing values from.
 * @param flag Flag to compare the quest item against.
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
