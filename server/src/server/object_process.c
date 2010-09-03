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
 * Routines that are executed from objects based on their speed have been
 * collected in this file. */

#include <global.h>

static void remove_force(object *op);
static void remove_blindness(object *op);
static void remove_confusion(object *op);
static void execute_wor(object *op);
static void animate_trigger(object *op);
static void change_object(object *op);

/**
 * Remove a force object from player, like potion effect.
 * @param op Force object to remove. */
static void remove_force(object *op)
{
	if (op->env == NULL)
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	CLEAR_FLAG(op, FLAG_APPLIED);
	change_abil(op->env, op);
	fix_player(op->env);
	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
}

/**
 * Remove blindness force object.
 * @param op Force object to remove. */
static void remove_blindness(object *op)
{
	if (--op->stats.food > 0)
	{
		return;
	}

	CLEAR_FLAG(op, FLAG_APPLIED);

	if (op->env)
	{
		change_abil(op->env, op);
		fix_player(op->env);
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
}

/**
 * Remove confusion force object.
 * @param op Force object to remove. */
static void remove_confusion(object *op)
{
	if (--op->stats.food > 0)
	{
		return;
	}

	if (op->env)
	{
		CLEAR_FLAG(op->env, FLAG_CONFUSED);

		if (op->env->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, op->env, "You regain your senses.");
		}
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
}

/**
 * Execute word of recall force object, and remove the force object.
 * @param op The force object. */
static void execute_wor(object *op)
{
	object *wor = op;

	while (op && op->type != PLAYER)
	{
		op = op->env;
	}

	if (op != NULL)
	{
		if (blocks_magic(op->map, op->x, op->y))
		{
			new_draw_info(NDI_UNIQUE, op, "You feel something fizzle inside you.");
		}
		else
		{
			enter_exit(op, wor);
		}
	}

	remove_ob(wor);
	check_walk_off(wor, NULL, MOVE_APPLY_VANISHED);
}

/**
 * Animate a ::TRIGGER.
 * @param op Trigger. */
static void animate_trigger(object *op)
{
	if (++op->stats.wc >= NUM_ANIMATIONS(op) / NUM_FACINGS(op))
	{
		op->stats.wc = 0;
		check_trigger(op, NULL);
	}
	else
	{
		op->state = op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		update_object(op, UP_OBJ_FACE);
	}
}

/**
 * An item (::ARROW or such) stops moving.
 *
 * This function assumes that only items on maps need special treatment.
 *
 * If the object can't be stopped, or it was destroyed while trying to
 * stop it, NULL is returned.
 * @param op Object to check.
 * @return Pointer to stopped object, NULL if destroyed or can't be
 * stopped. */
object *stop_item(object *op)
{
	if (op->map == NULL)
	{
		return op;
	}

	switch (op->type)
	{
		case THROWN_OBJ:
		{
			object *payload = op->inv;

			if (payload == NULL)
			{
				return NULL;
			}

			remove_ob(payload);
			check_walk_off(payload, NULL, MOVE_APPLY_VANISHED);
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
			return payload;
		}

		case ARROW:
			if (op->speed >= MIN_ACTIVE_SPEED)
			{
				op = fix_stopped_arrow(op);
			}

			return op;

		case CONE:
			if (op->speed < MIN_ACTIVE_SPEED)
			{
				return op;
			}
			else
			{
				return NULL;
			}

		default:
			return op;
	}
}

/**
 * Put stopped item where stop_item() had found it.
 * Inserts item into the old map, or merges it if it already is on the
 * map.
 * @param op Object to stop.
 * @param map Must be the value of op->map before stop_item() was called.
 * @param originator What caused op to be stopped. */
void fix_stopped_item(object *op, mapstruct *map, object *originator)
{
	if (map == NULL)
	{
		return;
	}

	if (QUERY_FLAG(op, FLAG_REMOVED))
	{
		insert_ob_in_map(op, map, originator, 0);
	}
	else if (op->type == ARROW)
	{
		/* Only some arrows actually need this. */
		merge_ob(op, NULL);
	}
}

/**
 * Replaces op with its other_arch if it has reached its end of life.
 * @param op Object to change. Will be removed and replaced. */
static void change_object(object *op)
{
	object *tmp, *env;
	int j;

	/* In non-living items only change when food value is 0 */
	if (!IS_LIVE(op))
	{
		if (op->stats.food-- > 0)
		{
			return;
		}
		else
		{
			/* We had hooked applyable light object here - handle them special */
			if (op->type == LIGHT_APPLY)
			{
				CLEAR_FLAG(op, FLAG_CHANGING);

				/* Special light like lamp which can be refilled */
				if (op->other_arch == NULL)
				{
					op->stats.food = 0;

					if (op->other_arch && op->other_arch->clone.sub_type & 1)
					{
						op->animation_id = op->other_arch->clone.animation_id;
						SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
					}
					else
					{
						CLEAR_FLAG(op, FLAG_ANIMATE);
						op->face = op->arch->clone.face;
					}

					/* Not on map? */
					if (op->env)
					{
						/* Inside player? */
						if (op->env->type == PLAYER)
						{
							new_draw_info_format(NDI_UNIQUE, op->env, "The %s burnt out.", query_name(op, NULL));
							op->glow_radius = 0;
							esrv_send_item(op->env, op);
							/* Fix player will take care about adjusting light masks */
							fix_player(op->env);
						}
						else
						{
							op->glow_radius = 0;

							if (op->env->type == CONTAINER)
							{
								esrv_send_item(NULL, op);
							}
						}
					}
					/* Object is on map. */
					else
					{
						/* Remove light mask from map. */
						adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));
						/* Tell map update we have something changed. */
						update_object(op, UP_OBJ_FACE);
						op->glow_radius = 0;
					}

					return;
				}
				/* This object will be deleted and exchanged with other_arch */
				else
				{
					/* But give the player a note about it too. */
					if (op->env && op->env->type == PLAYER)
					{
						new_draw_info_format(NDI_UNIQUE, op->env, "The %s burnt out.", query_name(op, NULL));
					}
				}
			}
		}
	}

	if (op->other_arch == NULL)
	{
		LOG(llevBug, "BUG: Change object (%s) without other_arch error.\n", op->name);
		return;
	}

	env = op->env;
	remove_ob(op);
	check_walk_off(op, NULL,MOVE_APPLY_VANISHED);

	tmp = arch_to_object(op->other_arch);
	/* The only variable it keeps. */
	tmp->stats.hp = op->stats.hp;

	if (env)
	{
		tmp->x = env->x;
		tmp->y = env->y;
		tmp = insert_ob_in_ob(tmp, env);

		if (env->type == PLAYER)
		{
			esrv_del_item(CONTR(env), op->count, NULL);
			esrv_send_item(env, tmp);
		}
		else if (env->type == CONTAINER)
		{
			esrv_del_item(NULL, op->count, env);
			esrv_send_item(env, tmp);
		}
	}
	else
	{
		j = find_first_free_spot(tmp->arch, NULL, op->map, op->x, op->y);

		/* Found a free spot */
		if (j != -1)
		{
			tmp->x = op->x + freearr_x[j];
			tmp->y = op->y + freearr_y[j];
			insert_ob_in_map(tmp, op->map, op, 0);
		}
	}
}

/**
 * Move for ::FIREWALL.
 *
 * Firewalls fire other spells. The direction of the wall is stored in
 * op->stats.dam.
 * @param op Firewall. */
void move_firewall(object *op)
{
	/* DM has created a firewall in his inventory or no legal spell
	 * selected. */
	if (!op->map || !op->last_eat || op->stats.dam == -1)
	{
		return;
	}

	cast_spell(op, op, op->direction, op->stats.dam, 1, spellNPC, NULL);
}

/**
 * Main object move function.
 * @param op Object to move. */
void process_object(object *op)
{
	if (QUERY_FLAG(op, FLAG_MONSTER))
	{
		if (move_monster(op) || OBJECT_FREE(op))
		{
			return;
		}
	}

	if (QUERY_FLAG(op, FLAG_CHANGING) && !op->state)
	{
		change_object(op);
		return;
	}

	if (QUERY_FLAG(op, FLAG_IS_USED_UP) && --op->stats.food <= 0)
	{
		if (QUERY_FLAG(op, FLAG_APPLIED) && op->type != CONTAINER)
		{
			remove_force(op);
		}
		else
		{
			/* We have a decaying container on the floor (assuming it's
			 * only possible here) */
			if (op->type == CONTAINER && (op->sub_type & 1) == ST1_CONTAINER_CORPSE)
			{
				/* This means someone has the corpse open. */
				if (op->attacked_by)
				{
					/* Give him a bit time back */
					op->stats.food += 3;
					return;
				}

				/* When the corpse is a personal bounty, we delete the
				 * bounty marker (->slaying) and resetting the counter.
				 * Now other people can access the corpse for stuff which
				 * are left here. */
				if (op->slaying || op->stats.maxhp)
				{
					if (op->slaying)
					{
						FREE_AND_CLEAR_HASH2(op->slaying);
					}

					if (op->stats.maxhp)
					{
						op->stats.maxhp = 0;
					}

					op->stats.food = op->arch->clone.stats.food;
					remove_ob(op);
					insert_ob_in_map(op, op->map, NULL, INS_NO_WALK_ON);
					return;
				}

				if (op->env && op->env->type == CONTAINER)
				{
					esrv_del_item(NULL, op->count, op->env);
				}
				else
				{
					object *pl = is_player_inv(op);

					if (pl)
					{
						esrv_del_item(CONTR(pl), op->count, op->env);
					}
				}

				remove_ob(op);
				check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
				return;
			}

			/* If necessary, delete the item from the player's inventory. */
			if (op->env && op->env->type == CONTAINER)
			{
				esrv_del_item(NULL, op->count, op->env);
			}
			else
			{
				object *pl = is_player_inv(op);

				if (pl)
				{
					esrv_del_item(CONTR(pl), op->count, op->env);
				}
			}

			destruct_ob(op);
		}

		return;
	}

	if (HAS_EVENT(op, EVENT_TIME))
	{
		trigger_event(EVENT_TIME, NULL, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING);
	}

	switch (op->type)
	{
		case ROD:
		case HORN:
			regenerate_rod(op);
			return;

		case FORCE:
		case POTION_EFFECT:
			if (!QUERY_FLAG(op, FLAG_IS_USED_UP))
			{
				remove_force(op);
			}

			return;

		case SPAWN_POINT:
			spawn_point(op);
			return;

		case BLINDNESS:
			remove_blindness(op);
			return;

		case CONFUSION:
			remove_confusion(op);
			return;

		case POISONING:
			poison_more(op);
			return;

		case DISEASE:
			move_disease(op);
			return;

		case SYMPTOM:
			move_symptom(op);
			return;

		case WORD_OF_RECALL:
			execute_wor(op);
			return;

		case BULLET:
			move_fired_arch(op);
			return;

		case THROWN_OBJ:
		case ARROW:
		case POTION:
			move_arrow(op);
			return;

		/* It now moves twice as fast */
		case LIGHTNING:
			move_bolt(op);
			return;

		case CONE:
			move_cone(op);
			return;

		/* Handle autoclosing */
		case DOOR:
			close_locked_door(op);
			return;

		case TELEPORTER:
			move_teleporter(op);
			return;

		case BOMB:
			animate_bomb(op);
			return;

		case FIREWALL:
			move_firewall(op);
			return;

		case MOOD_FLOOR:
			do_mood_floor(op);
			return;

		case GATE:
			move_gate(op);
			return;

		case TIMED_GATE:
			move_timed_gate(op);
			return;

		case TRIGGER:
		case TRIGGER_BUTTON:
		case TRIGGER_PEDESTAL:
		case TRIGGER_ALTAR:
			animate_trigger(op);
			return;

		case DETECTOR:
			move_detector(op);
			return;

		case PIT:
			move_pit(op);
			return;

		case DEEP_SWAMP:
			move_deep_swamp(op);
			return;

		case SWARM_SPELL:
			move_swarm_spell(op);
			return;

		case PLAYERMOVER:
			move_player_mover(op);
			return;

		case CREATOR:
			move_creator(op);
			return;

		case MARKER:
			move_marker(op);
			return;
	}
}
