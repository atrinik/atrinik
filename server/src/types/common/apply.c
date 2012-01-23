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

#include <global.h>

int common_object_apply(object *op, object *applier, int aflags)
{
	(void) aflags;

	if (op->msg)
	{
		draw_info(COLOR_WHITE, applier, op->msg);
		return OBJECT_METHOD_OK;
	}

	return OBJECT_METHOD_UNHANDLED;
}

int object_apply_item(object *op, object *applier, int aflags)
{
	int basic_aflag;
	object *tmp;
	uint8 ring_left;

	if (!op || !applier)
	{
		return OBJECT_METHOD_UNHANDLED;
	}

	if (applier->type != PLAYER)
	{
		return OBJECT_METHOD_UNHANDLED;
	}

	if (op->env != applier)
	{
		return OBJECT_METHOD_ERROR;
	}

	basic_aflag = aflags & AP_BASIC_FLAGS;

	if (!QUERY_FLAG(op, FLAG_APPLIED))
	{
		if (op->item_power != 0 && op->item_power + CONTR(applier)->item_power > settings.item_power_factor * applier->level)
		{
			draw_info(COLOR_WHITE, applier, "Equipping that combined with other items would consume your soul!");
			return OBJECT_METHOD_ERROR;
		}
	}
	else
	{
		/* Always apply, so no reason to unapply. */
		if (basic_aflag == AP_APPLY)
		{
			return OBJECT_METHOD_OK;
		}

		if (!(aflags & AP_IGNORE_CURSE) && (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)))
		{
			draw_info_format(COLOR_WHITE, applier, "No matter how hard you try, you just can't remove it!");
			return OBJECT_METHOD_ERROR;
		}

		if (QUERY_FLAG(op, FLAG_PERM_CURSED))
		{
			SET_FLAG(op, FLAG_CURSED);
		}

		if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
		{
			SET_FLAG(op, FLAG_DAMNED);
		}

		CLEAR_FLAG(op, FLAG_APPLIED);
		esrv_update_item(UPD_FLAGS, op);

		switch (op->type)
		{
			case WEAPON:
				change_abil(applier, op);
				CLEAR_FLAG(applier, FLAG_READY_WEAPON);
				draw_info_format(COLOR_WHITE, applier, "You unwield %s.", query_name(op, applier));
				break;

			case SKILL:
				change_abil(applier, op);
				applier->chosen_skill = NULL;
				break;

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
				change_abil(applier, op);
				draw_info_format(COLOR_WHITE, applier, "You unwear %s.", query_name(op, applier));
				break;

			case BOW:
			case WAND:
			case ROD:
				draw_info_format(COLOR_WHITE, applier, "You unready %s.", query_name(op, applier));
				break;

			default:
				draw_info_format(COLOR_WHITE, applier, "You unapply %s.", query_name(op, applier));
				break;
		}

		fix_player(applier);

		if (!(aflags & AP_NO_MERGE))
		{
			object_merge(op);
		}

		return OBJECT_METHOD_OK;
	}

	if (basic_aflag == AP_UNAPPLY)
	{
		return OBJECT_METHOD_OK;
	}

	ring_left = 0;

	/* This goes through and checks to see if the player already has
	 * something of that type applied - if so, unapply it. */
	for (tmp = applier->inv; tmp; tmp = tmp->below)
	{
		if ((tmp->type == op->type || ((op->type == WAND || op->type == ROD) && (tmp->type == WAND || tmp->type == ROD))) && QUERY_FLAG(tmp, FLAG_APPLIED) && tmp != op)
		{
			if (tmp->type == RING && !ring_left)
			{
				ring_left = 1;
			}
			else if (object_apply_item(tmp, applier, AP_UNAPPLY) != OBJECT_METHOD_OK)
			{
				return OBJECT_METHOD_ERROR;
			}
		}
	}

	op = object_stack_get_reinsert(op, 1);

	switch (op->type)
	{
		case WEAPON:
			if (!QUERY_FLAG(applier, FLAG_USE_WEAPON))
			{
				draw_info_format(COLOR_WHITE, applier, "You can't use %s.", query_name(op, applier));
				return OBJECT_METHOD_ERROR;
			}

			/* If we have applied a shield, don't allow applying of polearm or two-handed weapons. */
			if ((op->sub_type >= WEAP_POLE_IMPACT || op->sub_type >= WEAP_2H_IMPACT) && CONTR(applier)->equipment[PLAYER_EQUIP_SHIELD])
			{
				draw_info(COLOR_WHITE, applier, "You can't wield this weapon and a shield.");
				return OBJECT_METHOD_ERROR;
			}

			if (!check_skill_to_apply(applier, op))
			{
				return OBJECT_METHOD_ERROR;
			}

			draw_info_format(COLOR_WHITE, applier, "You wield %s.", query_name(op, applier));
			SET_FLAG(op, FLAG_APPLIED);
			SET_FLAG(applier, FLAG_READY_WEAPON);
			change_abil(applier, op);
			break;

		case SHIELD:
			/* Don't allow polearm or two-handed weapons with a shield. */
			if (CONTR(applier)->equipment[PLAYER_EQUIP_WEAPON] && (CONTR(applier)->equipment[PLAYER_EQUIP_WEAPON]->sub_type >= WEAP_POLE_IMPACT || CONTR(applier)->equipment[PLAYER_EQUIP_WEAPON]->sub_type >= WEAP_2H_IMPACT))
			{
				draw_info(COLOR_WHITE, applier, "You can't use a shield with your current weapon.");
				return OBJECT_METHOD_ERROR;
			}

		case ARMOUR:
		case HELMET:
		case BOOTS:
		case GLOVES:
		case GIRDLE:
		case BRACERS:
		case CLOAK:
			if (!QUERY_FLAG(applier, FLAG_USE_ARMOUR))
			{
				draw_info_format(COLOR_WHITE, applier, "You can't use %s.", query_name(op, applier));
				return OBJECT_METHOD_ERROR;
			}

		case RING:
		case AMULET:
			draw_info_format(COLOR_WHITE, applier, "You wear %s.", query_name(op, applier));
			SET_FLAG(op, FLAG_APPLIED);
			change_abil(applier, op);
			break;

		case SKILL:
			send_ready_skill(applier, skills[op->stats.sp].name);
			SET_FLAG(op, FLAG_APPLIED);
			change_abil(applier, op);
			applier->chosen_skill = op;
			break;

		case WAND:
		case ROD:
		case BOW:
			if (!check_skill_to_apply(applier, op))
			{
				return OBJECT_METHOD_ERROR;
			}

			draw_info_format(COLOR_WHITE, applier, "You ready %s.", query_name(op, applier));
			SET_FLAG(op, FLAG_APPLIED);

			if (op->type == BOW)
			{
				draw_info_format(COLOR_WHITE, applier, "You will now fire %s with %s.", op->race ? op->race : "nothing", query_name(op, applier));
			}

			break;

		default:
			draw_info_format(COLOR_WHITE, applier, "You apply %s.", query_name(op, applier));
	}

	if (!QUERY_FLAG(op, FLAG_APPLIED))
	{
		SET_FLAG(op, FLAG_APPLIED);
	}

	fix_player(applier);
	SET_FLAG(op, FLAG_BEEN_APPLIED);

	if (QUERY_FLAG(op, FLAG_PERM_CURSED))
	{
		SET_FLAG(op, FLAG_CURSED);
	}

	if (QUERY_FLAG(op, FLAG_PERM_DAMNED))
	{
		SET_FLAG(op, FLAG_DAMNED);
	}

	esrv_update_item(UPD_FLAGS, op);

	if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED))
	{
		draw_info(COLOR_WHITE, applier, "Oops, it feels deadly cold!");
	}

	return OBJECT_METHOD_OK;
}
