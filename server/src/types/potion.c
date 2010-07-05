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
 * Handles potion related code. */

#include <global.h>

/**
 * Apply a potion.
 * @param op Object applying the potion.
 * @param tmp The potion.
 * @return 1 if the potion was successfully applied, 0 otherwise. */
int apply_potion(object *op, object *tmp)
{
	int i;

	/* some sanity checks */
	if (!op || !tmp || (op->type == PLAYER && (!CONTR(op) || !CONTR(op)->exp_ptr[EXP_MAGICAL] || !CONTR(op)->exp_ptr[EXP_WISDOM])))
	{
		LOG(llevBug,"apply_potion() called with invalid objects! obj: %s (%s - %s) tmp:%s\n", query_name(op, NULL), CONTR(op) ? query_name(CONTR(op)->exp_ptr[EXP_MAGICAL], NULL) : "<no contr>", CONTR(op) ? query_name(CONTR(op)->exp_ptr[EXP_WISDOM], NULL) : "<no contr>", query_name(tmp, NULL));
		return 0;
	}

	if (op->type == PLAYER)
	{
		/* set chosen_skill to "magic device" - thats used when we "use" a potion */
		if (!change_skill(op, SK_USE_MAGIC_ITEM))
		{
			/* no skill, no potion use (dust & balm too!) */
			return 0;
		}

		if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
			identify(tmp);
		}

		/* special potions. Only players get this */
		if (tmp->last_eat == -1)
		{
			/* create a force and copy the effects in */
			object *force = get_archetype("force");

			if (!force)
			{
				LOG(llevBug, "apply_potion: Can't create force object?\n");
				return 0;
			}

			force->type = POTION_EFFECT;
			/* or it will auto destroy with first tick */
			SET_FLAG(force, FLAG_IS_USED_UP);
			/* how long this force will stay */
			force->stats.food += tmp->stats.food;

			if (force->stats.food <= 0)
			{
				force->stats.food = 1;
			}

			/* negative effects because cursed or damned */
			if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				/* now we have a bit work because we change (multiply,...) the
				 * base values of the potion - that can invoke out of bounce
				 * values we must catch here. */

				/* effects stays 3 times longer */
				force->stats.food *= 3;

				for (i = 0; i < NROFATTACKS; i++)
				{
					int tmp_a;
					int tmp_p;

					tmp_a = tmp->attack[i] > 0 ? -tmp->attack[i] : tmp->attack[i];
					tmp_p = tmp->protection[i] > 0 ? -tmp->protection[i] : tmp->protection[i];

					/* double bad effect when damned */
					if (QUERY_FLAG(tmp, FLAG_DAMNED))
					{
						tmp_a *= 2;
						tmp_p *= 2;
					}

					/* we don't want out of bound values ... */
					if ((int) force->protection[i] + tmp_p > 100)
					{
						force->protection[i] = 100;
					}
					else if ((int) force->protection[i] + tmp_p < -100)
					{
						force->protection[i] = -100;
					}
					else
					{
						force->protection[i] += (sint8) tmp_p;
					}

					if ((int) force->attack[i] + tmp_a > 100)
					{
						force->attack[i] = 100;
					}
					else if ((int) force->attack[i] + tmp_a < 0)
					{
						force->attack[i] = 0;
					}
					else
					{
						force->attack[i] += tmp_a;
					}
				}

				insert_spell_effect("meffect_purple", op->map, op->x, op->y);
				play_sound_map(op->map, CMD_SOUND_EFFECT, "poison.ogg", op->x, op->y, 0, 0);
			}
			/* all positive (when not on default negative) */
			else
			{
				/* we don't must do the hard way like cursed/damned (no multiplication or
				 * sign change). */
				memcpy(force->protection, tmp->protection, sizeof(tmp->protection));
				memcpy(force->attack, tmp->attack, sizeof(tmp->attack));
				insert_spell_effect("meffect_green", op->map, op->x, op->y);
				play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
			}

			/* now copy stats values */
			force->stats.Str = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Str > 0 ? -tmp->stats.Str : tmp->stats.Str) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Str > 0 ? (-tmp->stats.Str) * 2 : tmp->stats.Str * 2) : tmp->stats.Str);
			force->stats.Con = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Con > 0 ? -tmp->stats.Con : tmp->stats.Con) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Con > 0 ? (-tmp->stats.Con) * 2 : tmp->stats.Con * 2) : tmp->stats.Con);
			force->stats.Dex = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Dex > 0 ? -tmp->stats.Dex : tmp->stats.Dex) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Dex > 0 ? (-tmp->stats.Dex) * 2 : tmp->stats.Dex * 2) : tmp->stats.Dex);
			force->stats.Int = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Int > 0 ? -tmp->stats.Int : tmp->stats.Int) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Int > 0 ? (-tmp->stats.Int) * 2 : tmp->stats.Int * 2) : tmp->stats.Int);
			force->stats.Wis = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Wis > 0 ? -tmp->stats.Wis : tmp->stats.Wis) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Wis > 0 ? (-tmp->stats.Wis) * 2 : tmp->stats.Wis * 2) : tmp->stats.Wis);
			force->stats.Pow = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Pow > 0 ? -tmp->stats.Pow : tmp->stats.Pow) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Pow > 0 ? (-tmp->stats.Pow) * 2 : tmp->stats.Pow * 2) : tmp->stats.Pow);
			force->stats.Cha = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Cha > 0 ? -tmp->stats.Cha : tmp->stats.Cha) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Cha > 0 ? (-tmp->stats.Cha) * 2 : tmp->stats.Cha * 2) : tmp->stats.Cha);

			/* kick the force in, and apply it to player */
			force->speed_left = -1;
			force = insert_ob_in_ob(force, op);
			CLEAR_FLAG(tmp, FLAG_APPLIED);
			SET_FLAG(force, FLAG_APPLIED);

			/* implicit fix_player() here */
			if (!change_abil(op, force))
			{
				new_draw_info(NDI_UNIQUE, op, "Nothing happened.");
			}

			decrease_ob(tmp);
			return 1;
		}

		/* Potion of minor restoration */
		if (tmp->last_eat == 1)
		{
			object *depl;
			archetype *at;

			if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				if (QUERY_FLAG(tmp, FLAG_DAMNED))
				{
					drain_stat(op);
					drain_stat(op);
					drain_stat(op);
					drain_stat(op);
				}
				else
				{
					drain_stat(op);
					drain_stat(op);
				}

				fix_player(op);
				decrease_ob(tmp);
				insert_spell_effect("meffect_purple", op->map, op->x, op->y);
				play_sound_map(op->map, CMD_SOUND_EFFECT, "poison.ogg", op->x, op->y, 0, 0);
				return 1;
			}

			if ((at = find_archetype("depletion")) == NULL)
			{
				LOG(llevBug, "BUG: Could not find archetype depletion.");
				return 0;
			}

			depl = present_arch_in_ob(at, op);

			if (depl != NULL)
			{
				for (i = 0; i < 7; i++)
				{
					if (get_attr_value(&depl->stats, i))
					{
						new_draw_info(NDI_UNIQUE, op, restore_msg[i]);
					}
				}

				/* in inventory of ... */
				remove_ob(depl);
				fix_player(op);
			}
			else
			{
				new_draw_info(NDI_UNIQUE, op, "You feel a great loss...");
			}

			decrease_ob(tmp);
			insert_spell_effect("meffect_green", op->map, op->x, op->y);
			play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
			return 1;
		}
		/* improvement potion */
		else if (tmp->last_eat == 2)
		{
			int success_flag = 0, hp_flag = 0, sp_flag = 0, grace_flag = 0;

			if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				/* jump in by random - goto power */
				if (RANDOM() % 2)
				{
					goto hp_jump;
				}
				else if (RANDOM() % 2)
				{
					goto sp_jump;
				}
				else
				{
					goto grace_jump;
				}

				while (!hp_flag || !sp_flag || !grace_flag)
				{
hp_jump:
					/* mark we have checked hp chain */
					hp_flag = 1;

					for (i = 2; i <= op->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levhp[i] != 1)
						{
							CONTR(op)->levhp[i] = 1;
							success_flag = 2;
							goto improve_done;
						}
					}
sp_jump:
					/* mark we have checked sp chain */
					sp_flag = 1;

					for (i = 2; i <= CONTR(op)->exp_ptr[EXP_MAGICAL]->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levsp[i] != 1)
						{
							CONTR(op)->levsp[i] = 1;
							success_flag = 2;
							goto improve_done;
						}
					}
grace_jump:
					/* mark we have checked grace chain */
					grace_flag = 1;

					for (i = 2; i <= CONTR(op)->exp_ptr[EXP_WISDOM]->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levgrace[i] != 1)
						{
							CONTR(op)->levgrace[i] = 1;
							success_flag = 2;
							goto improve_done;
						}
					}
				}

				success_flag = 3;
			}
			else
			{
				/* jump in by random - goto power */
				if (RANDOM() % 2)
				{
					goto hp_jump2;
				}
				else if (RANDOM() % 2)
				{
					goto sp_jump2;
				}
				else
				{
					goto grace_jump2;
				}

				while (!hp_flag || !sp_flag || !grace_flag)
				{
hp_jump2:
					/* mark we have checked hp chain */
					hp_flag = 1;

					for (i = 1; i <= op->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levhp[i] != (char) op->arch->clone.stats.maxhp)
						{
							CONTR(op)->levhp[i] = (char) op->arch->clone.stats.maxhp;
							success_flag = 1;
							goto improve_done;
						}
					}
sp_jump2:
					/* mark we have checked sp chain */
					sp_flag = 1;

					for (i = 1; i <= CONTR(op)->exp_ptr[EXP_MAGICAL]->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levsp[i] != (char) op->arch->clone.stats.maxsp)
						{
							CONTR(op)->levsp[i] = (char) op->arch->clone.stats.maxsp;
							success_flag = 1;
							goto improve_done;
						}
					}
grace_jump2:
					/* mark we have checked grace chain */
					grace_flag = 1;

					for (i = 1; i <= CONTR(op)->exp_ptr[EXP_WISDOM]->level; i++)
					{
						/* move one value to max */
						if (CONTR(op)->levgrace[i] != (char) op->arch->clone.stats.maxgrace)
						{
							CONTR(op)->levgrace[i] = (char) op->arch->clone.stats.maxgrace;
							success_flag = 1;
							goto improve_done;
						}

					}
				}
			}

improve_done:
			CLEAR_FLAG(tmp, FLAG_APPLIED);

			if (!success_flag)
			{
				new_draw_info(NDI_UNIQUE, op, "The potion had no effect - you are already perfect.");
				play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
			}
			else if (success_flag == 1)
			{
				fix_player(op);
				insert_spell_effect("meffect_yellow", op->map, op->x, op->y);
				play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
				new_draw_info(NDI_UNIQUE, op, "You feel a little more perfect!");
			}
			else if (success_flag == 2)
			{
				fix_player(op);
				insert_spell_effect("meffect_purple", op->map, op->x, op->y);
				play_sound_map(op->map, CMD_SOUND_EFFECT, "poison.ogg", op->x, op->y, 0, 0);
				new_draw_info(NDI_UNIQUE, op, "The foul potion burns like fire in you!");
			}
			/* bad potion but all values of this player are 1! poor poor guy.... */
			else
			{
				insert_spell_effect("meffect_purple", op->map, op->x, op->y);
				play_sound_map(op->map, CMD_SOUND_EFFECT, "poison.ogg", op->x, op->y, 0, 0);
				new_draw_info(NDI_UNIQUE, op, "The potion was foul but had no effect on your tortured body.");
			}

			decrease_ob(tmp);
			return 1;
		}
	}

	if (tmp->stats.sp == SP_NO_SPELL)
	{
		new_draw_info(NDI_UNIQUE, op, "Nothing happens as you apply it.");
		decrease_ob(tmp);
		return 0;
	}

	/* A potion that casts a spell.  Healing, restore spellpoint (power
	 * potion) and heroism all fit into this category. */
	if (tmp->stats.sp != SP_NO_SPELL)
	{
		/* apply potion fires in player's facing direction, unless the spell is SELF one, ie, healing or cure ilness. */
		cast_spell(op, tmp, spells[tmp->stats.sp].flags & SPELL_DESC_SELF ? 0 : op->facing, tmp->stats.sp, 1, spellPotion, NULL);
		decrease_ob(tmp);

		/* if you're dead, no point in doing this... */
		if (!QUERY_FLAG(op, FLAG_REMOVED))
		{
			fix_player(op);
		}

		return 1;
	}

	/* CLEAR_FLAG is so that if the character has other potions
	 * that were grouped with the one consumed, his
	 * stat will not be raised by them.  fix_player just clears
	 * up all the stats. */
	CLEAR_FLAG(tmp, FLAG_APPLIED);
	fix_player(op);
	decrease_ob(tmp);
	return 1;
}
