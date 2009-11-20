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

/**
 * @file
 * Routines that are executed from objects based on their speed have been
 * collected in this file. */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

static void remove_force(object *op);
static void remove_blindness(object *op);
static void remove_confusion(object *op);
static void execute_wor(object *op);
static void animate_trigger(object *op);
static void change_object(object *op);
static void move_firechest(object *op);

/**
 * Remove a force object from player, like potion effect.
 * @param op Force object to remove */
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

static void remove_blindness(object *op)
{
	if (--op->stats.food > 0)
	{
		return;
	}

	CLEAR_FLAG(op, FLAG_APPLIED);

	if (op->env != NULL)
	{
		change_abil(op->env, op);
		fix_player(op->env);
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
}

static void remove_confusion(object *op)
{
	if (--op->stats.food > 0)
	{
		return;
	}

	if (op->env != NULL)
	{
		CLEAR_FLAG(op->env, FLAG_CONFUSED);

		if (op->env->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, 0, op->env, "You regain your senses.");
		}
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
}

static void execute_wor(object *op)
{
	object *wor = op;

	while (op != NULL && op->type != PLAYER)
	{
		op = op->env;
	}

	if (op != NULL)
	{
		if (blocks_magic(op->map, op->x, op->y))
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You feel something fizzle inside you.");
		}
		else
		{
			enter_exit(op, wor);
		}
	}

	remove_ob(wor);
	check_walk_off(wor, NULL, MOVE_APPLY_VANISHED);
}

static void animate_trigger(object *op)
{
	if ((unsigned char) ++op->stats.wc >= NUM_ANIMATIONS(op) / NUM_FACINGS(op))
	{
		op->stats.wc = 0;
		check_trigger(op, NULL);
	}
	else
	{
		op->state = (uint8) op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		update_object(op, UP_OBJ_FACE);
	}
}

/* stop_item() returns a pointer to the stopped object.  The stopped object
 * may or may not have been removed from maps or inventories.  It will not
 * have been merged with other items.
 *
 * This function assumes that only items on maps need special treatment.
 *
 * If the object can't be stopped, or it was destroyed while trying to stop
 * it, NULL is returned.
 *
 * fix_stopped_item() should be used if the stopped item should be put on
 * the map. */
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

/* fix_stopped_item() - put stopped item where stop_item() had found it.
 * Inserts item into the old map, or merges it if it already is on the map.
 *
 * 'map' must be the value of op->map before stop_item() was called. */
void fix_stopped_item(object *op, mapstruct *map, object *originator)
{
	if (map == NULL)
	{
		return;
	}

	if (QUERY_FLAG(op, FLAG_REMOVED))
	{
		insert_ob_in_map (op, map, originator, 0);
	}
	/* only some arrows actually need this */
	else if (op->type == ARROW)
	{
		merge_ob(op, NULL);
	}
}

/* This routine doesnt seem to work for "inanimate" objects that
 * are being carried, ie a held torch leaps from your hands!.
 * Modified this routine to allow held objects. b.t. */
/* Doesn't handle linked objs yet */
static void change_object(object *op)
{
	object *tmp, *env;
	int i, j;

	/* In non-living items only change when food value is 0 */
	if (!IS_LIVE(op))
	{
		if (op->stats.food-- > 0)
		{
			return;
		}
		else
		{
			/* we had hooked applyable light object here - handle them special */
			if (op->type == TYPE_LIGHT_APPLY)
			{
				CLEAR_FLAG(op, FLAG_CHANGING);

				/* thats special lights like lamp which can be refilled */
				if (op->other_arch == NULL)
				{
					op->stats.food = 0;

					if (op->other_arch && op->other_arch->clone.sub_type1 & 1)
					{
						op->animation_id = op->other_arch->clone.animation_id;
						SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
					}
					else
					{
						CLEAR_FLAG(op, FLAG_ANIMATE);
						op->face = op->arch->clone.face;
					}

					/* not on map? */
					if (op->env)
					{
						/* inside player char? */
						if (op->env->type == PLAYER)
						{
							new_draw_info_format(NDI_UNIQUE, 0, op->env, "The %s burnt out.", query_name(op, NULL));
							op->glow_radius = 0;
							esrv_send_item(op->env, op);
							/* fix player will take care about adjust light masks */
							fix_player(op->env);
						}
						/* atm, lights inside other inv as players don't set light masks */
						else
						{
							/* but we need to update container which are possible watched by players */
							op->glow_radius = 0;

							if (op->env->type == CONTAINER)
							{
								esrv_send_item(NULL, op);
							}
						}
					}
					/* object is on map */
					else
					{
						/* remove light mask from map */
						adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));
						/* tell map update we have something changed */
						update_object(op, UP_OBJ_FACE);
						op->glow_radius = 0;
					}

					return;
				}
				/* this object will be deleted and exchanged with other_arch */
				else
				{
					/* but give the player a note about it too */
					if (op->env && op->env->type == PLAYER)
					{
						new_draw_info_format(NDI_UNIQUE, 0, op->env, "The %s burnt out.", query_name(op, NULL));
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

	/* atm we only generate per change tick *ONE* object */
	for (i = 0; i < 1; i++)
	{
		tmp = arch_to_object(op->other_arch);
		/* The only variable it keeps. */
		tmp->stats.hp = op->stats.hp;

		if (env)
		{
			tmp->x = env->x, tmp->y = env->y;
			tmp = insert_ob_in_ob(tmp, env);

			/* this should handle in future insert_ob_in_ob() */
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
			j = find_first_free_spot(tmp->arch, op->map, op->x, op->y);

			/* Found a free spot */
			if (j != -1)
			{
				tmp->x = op->x + freearr_x[j], tmp->y = op->y + freearr_y[j];
				insert_ob_in_map(tmp, op->map, op, 0);
			}
		}
	}
}

/* peterm:  firewalls generalized to be able to shoot any type
 * of spell at all.  the stats.dam field of a firewall object
 * contains it's spelltype. The direction of the wall is stored
 * in op->direction. walls can have hp, so they can be torn down. */

/* added some new features to FIREWALL - on/off features by connected,
 * advanced spell selection and full turnable by connected and
 * autoturn. MT-2003 */
void move_firewall(object *op)
{
	/* last_eat 0 = off */
	/* dm has created a firewall in his inventory or no legal spell selected */
	if (!op->map || !op->last_eat || op->stats.dam == -1)
		return;

	cast_spell(op, op, op->direction, op->stats.dam, 1, spellNPC, NULL);
}

static void move_firechest(object *op)
{
	/* dm has created a firechest in his inventory */
	if (!op->map)
		return;

	fire_a_ball(op, rndm(1, 8), 7);
}

int process_object(object *op)
{
	if (QUERY_FLAG(op, FLAG_MONSTER))
	{
		if (move_monster(op) || OBJECT_FREE(op))
		{
			return 1;
		}
	}

	if (QUERY_FLAG(op, FLAG_CHANGING) && !op->state)
	{
		change_object(op);
		return 1;
	}

	if (QUERY_FLAG(op, FLAG_IS_USED_UP) && --op->stats.food <= 0)
	{
		if (QUERY_FLAG(op, FLAG_APPLIED) && op->type != CONTAINER)
		{
			remove_force(op);
		}
		else
		{
			/* we have a decying container on the floor (asuming its only possible here) ! */
			if (op->type == CONTAINER && (op->sub_type1&1) == ST1_CONTAINER_CORPSE)
			{
				/* this means someone access the corpse */
				if (op->attacked_by)
				{
					/* then stop decaying! - we don't want delete this under the hand of the guy! */

					/* give him a bit time back */
					op->stats.food += 3;
					/* go on */
					goto process_object_dirty_jump;
				}

				/* now we do something funny: WHEN the corpse is a (personal) bounty,
				 * we delete the bounty marker (->slaying) and reseting the counter.
				 * Now other people can access the corpse for stuff which are left
				 * here perhaps. */
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
					/* another lame way to update view of players... */
					remove_ob(op);
					insert_ob_in_map(op, op->map, NULL, INS_NO_WALK_ON);
					return 1;
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
				return 1;
			}

			/* IF necessary, delete the item from the players inventory */
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

		return 1;
	}

process_object_dirty_jump:

	/* Trigger the TIME event */
	trigger_event(EVENT_TIME, NULL, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING);

	switch (op->type)
	{
		case ROD:
		case HORN:
			regenerate_rod(op);
			return 1;

		case FORCE:
		case POTION_EFFECT:
			if (!QUERY_FLAG(op, FLAG_IS_USED_UP))
			{
				remove_force(op);
			}

			return 1;

		case SPAWN_POINT:
			spawn_point(op);
			return 0;

		case BLINDNESS:
			remove_blindness(op);
			return 0;

		case CONFUSION:
			remove_confusion(op);
			return 0;

		case POISONING:
			poison_more(op);
			return 0;

		case DISEASE:
			move_disease(op);
			return 0;

		case SYMPTOM:
			move_symptom(op);
			return 0;

		case WORD_OF_RECALL:
			execute_wor(op);
			return 0;

		case BULLET:
			move_fired_arch(op);
			return 0;

		case MMISSILE:
			move_missile(op);
			return 0;

		case THROWN_OBJ:
		case ARROW:
			move_arrow(op);
			return 0;

		case FBULLET:
			move_fired_arch(op);
			return 0;

		case FBALL:
		case POISONCLOUD:
			explosion(op);
			return 0;

		/* It now moves twice as fast */
		case LIGHTNING:
			move_bolt(op);
			return 0;

		case CONE:
			move_cone(op);
			return 0;

		case DOOR:
			remove_door(op);
			return 0;

		/* handle autoclosing */
		case LOCKED_DOOR:
			close_locked_door(op);
			return 0;

		case TELEPORTER:
			move_teleporter(op);
			return 0;

		case BOMB:
			animate_bomb(op);
			return 0;

		case GOLEM:
			move_golem(op);
			return 0;

		case EARTHWALL:
			hit_player(op, 2, op, AT_PHYSICAL);
			return 0;

		case FIREWALL:
			move_firewall(op);
			return 0;

		case FIRECHEST:
			move_firechest(op);
			return 0;

		case MOOD_FLOOR:
			do_mood_floor(op, op);
			return 0;

		case GATE:
			move_gate(op);
			return 0;

		case TIMED_GATE:
			move_timed_gate(op);
			return 0;

		case TRIGGER:
		case TRIGGER_BUTTON:
		case TRIGGER_PEDESTAL:
		case TRIGGER_ALTAR:
			animate_trigger(op);
			return 0;

		case DETECTOR:
			move_detector(op);
			return 0;

		case PIT:
			move_pit(op);
			return 0;

		case DEEP_SWAMP:
			move_deep_swamp(op);
			return 0;

		case CANCELLATION:
			move_cancellation(op);
			return 0;

		case BALL_LIGHTNING:
			move_ball_lightning(op);
			return 0;

		case SWARM_SPELL:
			move_swarm_spell(op);
			return 0;

		case PLAYERMOVER:
			move_player_mover(op);
			return 0;

		case CREATOR:
			move_creator(op);
			return 0;

		case MARKER:
			move_marker(op);
			return 0;

		case AURA:
			move_aura(op);
			return 0;

		case PEACEMAKER:
			move_peacemaker(op);
			return 0;
	}

	return 0;
}
