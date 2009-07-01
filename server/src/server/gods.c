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

/* Oct 3, 1995 - Code laid down for initial gods, priest alignment, and
 * monster race initialization. b.t.
 */

/* Sept 1996 - moved code over to object -oriented gods -b.t. */

#include <global.h>
#include <sounds.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/* define this if you want to allow gods to assign more gifts
 * and limitations to priests */
#define MORE_PRIEST_GIFTS

static int god_gives_present(object *op, object *god, treasure *tr);
#if 0
static void follower_remove_similar_item(object *op, object *item);
#endif

int lookup_god_by_name(const char *name)
{
  	int godnr = -1, nmlen = strlen(name);

  	if (name && strcmp(name, "none"))
	{
		godlink *gl;
		for (gl = first_god; gl; gl = gl->next)
			if (!strncmp(name, gl->name, MIN((int)strlen(gl->name), nmlen)))
				break;

		if (gl)
			godnr = gl->id;
  	}

  	return godnr;
}

object *find_god(const char *name)
{
  	object *god = NULL;

	if (name)
	{
		godlink *gl;
		for (gl = first_god; gl; gl = gl->next)
			if (!strcmp(name, gl->name))
				break;

		if (gl)
			god = pntr_to_god_obj(gl);
	}

	return god;
}

void pray_at_altar(object *pl, object *altar)
{
    object *pl_god = find_god(determine_god(pl));
#ifdef PLUGINS
	/* GROS : This is for return value of script */
    int return_pray_script;
    /* GROS: Handle for plugin altar-parying (apply) event */
    if (altar->event_flags&EVENT_FLAG_APPLY)
    {
        CFParm CFP;
        CFParm* CFR;
        int k, l, m;
		object *event_obj = get_event_object(altar, EVENT_APPLY);
        k = EVENT_APPLY;
        l = SCRIPT_FIX_ALL;
        m = 0;
        CFP.Value[0] = &k;
        CFP.Value[1] = pl;
        CFP.Value[2] = altar;
        CFP.Value[3] = NULL;
        CFP.Value[4] = NULL;
        CFP.Value[5] = &m;
        CFP.Value[6] = &m;
        CFP.Value[7] = &m;
        CFP.Value[8] = &l;
        CFP.Value[9] = (char *)event_obj->race;
        CFP.Value[10] = (char *)event_obj->slaying;
        if (findPlugin(event_obj->name) >= 0)
        {
            CFR = (PlugList[findPlugin(event_obj->name)].eventfunc) (&CFP);
            return_pray_script = *(int *)(CFR->Value[0]);
            free(CFR);
            if (return_pray_script)
				return;
        }
    }
#endif

    /* If non consecrate altar, don't do anything */
    if (!altar->other_arch)
		return;

    /* hmm. what happened depends on pl's current god, level, etc */

	/* new convert */
    if (!pl_god)
	{
		become_follower(pl, &altar->other_arch->clone);
		return;
    }
	/* pray at your gods altar */
	else if (!strcmp(pl_god->name, altar->other_arch->clone.name))
	{
		int bonus = ((pl->stats.Wis / 10) + (SK_level(pl) / 10));

		new_draw_info_format(NDI_UNIQUE, 0, pl, "You feel the powers of your deity %s.", pl_god->name);

		/* we can get negative grace up faster */
		if (pl->stats.grace < 0)
			pl->stats.grace += (bonus > -1 * (pl->stats.grace / 10) ? bonus : -1 * (pl->stats.grace / 10));

		/* we can super-charge grace to 2x max */
		if (pl->stats.grace < (2 * pl->stats.maxgrace))
			pl->stats.grace += bonus / 2;

		/* I think there was a bug here in that this was nested
		 * in the if immediately above */
		if (pl->stats.grace > pl->stats.maxgrace)
			pl->stats.grace = pl->stats.maxgrace;

		/* Every once in a while, the god decides to checkup on their
		 * follower, and may intervene to help them out. */
		bonus = MAX(1, bonus + MAX(pl->stats.luck, -3));

		if (((random_roll(0, 399, pl, PREFER_LOW)) - bonus) < 0)
			god_intervention(pl, pl_god);
    }
	/* praying to another god! */
	else
	{
		int loss = 0, angry = 1;

		/* I believe the logic for detecting opposing gods was completely
		 * broken - I think it should work now.  altar->other_arch
		 * points to the god of this altar (which we have
		 * already verified is non null).  pl_god->other_arch
		 * is the opposing god - we need to verify that exists before
		 * using its values. */

		if (pl_god->other_arch && (altar->other_arch->name == pl_god->other_arch->name))
		{
			angry = 2;
			if (random_roll(0, SK_level(pl) + 2, pl, PREFER_LOW) - 5 > 0)
			{
				/* you really screwed up */
				angry = 3;
				new_draw_info_format(NDI_UNIQUE | NDI_NAVY, 0, pl, "Foul priest! %s punishes you!", pl_god->name);
				cast_mana_storm(pl, pl_god->level + 20);
			}
			new_draw_info_format(NDI_UNIQUE | NDI_NAVY, 0, pl, "Foolish heretic! %s is livid!", pl_god->name);
		}
		else
			new_draw_info_format(NDI_UNIQUE | NDI_NAVY, 0, pl, "Heretic! %s is angered!", pl_god->name);

		/* whether we will be successfull in defecting or not -
		 * we lose experience from the clerical experience obj */

		loss = (int)((float)0.1 * (float) pl->chosen_skill->exp_obj->stats.exp);
		if (loss)
			lose_priest_exp(pl, random_roll(0, loss * angry - 1, pl, PREFER_LOW));

		/* May switch Gods, but its random chance based on our current level
		 * note it gets harder to swap gods the higher we get */
		if ((angry == 1) && !(random_roll(0, pl->chosen_skill->exp_obj->level, pl, PREFER_LOW)))
			become_follower(pl, &altar->other_arch->clone);
		/* If angry... switching gods */
		else
		{
			/* toss this player off the altar.  He can try again. */
			new_draw_info_format(NDI_UNIQUE | NDI_NAVY, 0, pl, "A divine force pushes you off the altar.");
			/* back him off the way he came. */
			move_player(pl, absdir(pl->facing + 4));
		}
    }
}

static int get_spell_number(object *op)
{
    int spell;

    if (op->slaying && (spell = look_up_spell_name (op->slaying)) >= 0)
        return spell;
    else
        return op->stats.sp;
}

/* Ensure that 'op' doesn't know any special prayers that are not granted
 * by 'god'. */
static void check_special_prayers(object *op, object *god)
{
    treasure *tr;
    object *tmp, *next_tmp;
    int spell;

    /* Outer loop iterates over all special prayer marks */
    for (tmp = op->inv; tmp; tmp = next_tmp)
    {
        next_tmp = tmp->below;

        if (tmp->type != FORCE || tmp->slaying == NULL || strcmp(tmp->slaying, "special prayer"))
            continue;

        spell = tmp->stats.sp;

        if (god->randomitems == NULL)
		{
            LOG(llevBug, "BUG: check_special_prayers(): %s without randomitems\n", query_name(god, NULL));
            do_forget_spell(op, spell);
            continue;
		}

        /* Inner loop tries to find the special prayer in the god's treasure
         * list. */
        for (tr = god->randomitems->items; tr; tr = tr->next)
        {
            object *item;

            if (tr->item == NULL)
                continue;

            item = &tr->item->clone;

            if (item->type == SPELLBOOK && get_spell_number(item) == spell)
            {
                /* Current god allows this special prayer. */
                spell = -1;
                break;
            }
        }

        if (spell >= 0)
            do_forget_spell(op, spell);
    }
}

/* become_follower - This function is called whenever a player has
 * switched to a new god. It handles basically all the stat changes
 * that happen to the player, including the removal of godgiven
 * items (from the former cult). */
void become_follower(object *op, object *new_god)
{
	/* obj. containing god data */
    object *exp_obj = op->chosen_skill->exp_obj;
	 /* old god */
    object *old_god = NULL;
    treasure *tr;
    int i;

	CONTR(op)->socket.ext_title_flag = 1;
    /* get old god */
    if (exp_obj->title)
        old_god = find_god(exp_obj->title);

    /* give the player any special god-characteristic-items. */
    for (tr = new_god->randomitems->items; tr != NULL; tr = tr->next)
	{
       if (tr->item && IS_SYS_INVISIBLE(&tr->item->clone) && tr->item->clone.type != SPELLBOOK && tr->item->clone.type != BOOK)
        	god_gives_present(op, new_god, tr);
	}

    if (!op || !new_god)
		return;

    if (op->race && new_god->slaying && strstr(op->race, new_god->slaying))
	{
		new_draw_info_format(NDI_UNIQUE | NDI_NAVY, 0, op, "Fool! %s detests your kind!", new_god->name);
        if (random_roll(0, op->level - 1, op, PREFER_LOW) - 5 > 0)
	   		cast_mana_storm(op, new_god->level + 10);

		return;
    }

    new_draw_info_format(NDI_UNIQUE | NDI_NAVY, 0, op, "You become a follower of %s!", new_god->name);

	/* get rid of old god */
    if (exp_obj->title)
	{
       	new_draw_info_format(NDI_UNIQUE, 0, op, "%s's blessing is withdrawn from you.", exp_obj->title);
       	CLEAR_FLAG(exp_obj, FLAG_APPLIED);
       	(void) change_abil(op, exp_obj);
       	FREE_AND_CLEAR_HASH2(exp_obj->title);
    }

	/* now change to the new gods attributes to exp_obj */
	FREE_AND_COPY_HASH(exp_obj->title, new_god->name);
	exp_obj->path_attuned = new_god->path_attuned;
	exp_obj->path_repelled = new_god->path_repelled;
	exp_obj->path_denied = new_god->path_denied;
	/* copy god's resistances */
	memcpy(exp_obj->resist, new_god->resist, sizeof(new_god->resist));

    /* make sure that certain immunities do NOT get passed
     * to the follower! */
    for (i = 0; i < NROFATTACKS; i++)
      	if (exp_obj->resist[i] > 30 && (i == ATNR_FIRE || i == ATNR_COLD || i == ATNR_ELECTRICITY || i == ATNR_POISON))
			exp_obj->resist[i] = 30;

#ifdef MORE_PRIEST_GIFTS
    exp_obj->stats.hp = (sint16) new_god->last_heal;
    exp_obj->stats.sp = (sint16) new_god->last_sp;
    exp_obj->stats.grace = (sint16) new_god->last_grace;
    exp_obj->stats.food = (sint16) new_god->last_eat;
    exp_obj->stats.luck = (sint8) new_god->stats.luck;
    /* gods may pass on certain flag properties */
    update_priest_flag(new_god, exp_obj, FLAG_SEE_IN_DARK);
    update_priest_flag(new_god, exp_obj, FLAG_CAN_REFL_SPELL);
    update_priest_flag(new_god, exp_obj, FLAG_CAN_REFL_MISSILE);
    update_priest_flag(new_god, exp_obj, FLAG_STEALTH);
    update_priest_flag(new_god, exp_obj, FLAG_SEE_INVISIBLE);
    update_priest_flag(new_god, exp_obj, FLAG_UNDEAD);
    update_priest_flag(new_god, exp_obj, FLAG_BLIND);
	/* better have this if blind! */
    update_priest_flag(new_god, exp_obj, FLAG_XRAYS);
#endif

    new_draw_info_format(NDI_UNIQUE, 0, op, "You are bathed in %s's aura.", new_god->name);

#ifdef MORE_PRIEST_GIFTS
    /* Weapon/armour use are special...handle flag toggles here as this can
     * only happen when gods are worshipped and if the new priest could
     * have used armour/weapons in the first place */
    update_priest_flag(new_god,exp_obj, FLAG_USE_WEAPON);
    update_priest_flag(new_god,exp_obj, FLAG_USE_ARMOUR);

    if (worship_forbids_use(op, exp_obj, FLAG_USE_WEAPON, "weapons"))
		stop_using_item(op, WEAPON, 2);

    if (worship_forbids_use(op, exp_obj, FLAG_USE_ARMOUR,"armour"))
	{
		stop_using_item(op, ARMOUR, 1);
		stop_using_item(op, HELMET, 1);
		stop_using_item(op, BOOTS, 1);
		stop_using_item(op, GLOVES, 1);
		stop_using_item(op, SHIELD, 1);
    }
#endif

    SET_FLAG(exp_obj, FLAG_APPLIED);
    (void) change_abil(op, exp_obj);

    check_special_prayers(op, new_god);
}

/* op is the player.
 * exp_obj is the wisdom experience.
 * flag is the flag to check against.
 * string is the string to print out. */
int worship_forbids_use(object *op, object *exp_obj, uint32 flag, char *string)
{
	if (QUERY_FLAG(&op->arch->clone,flag))
	{
		if (QUERY_FLAG(op, flag) != QUERY_FLAG(exp_obj, flag))
		{
			update_priest_flag(exp_obj, op, flag);

			if (QUERY_FLAG(op, flag))
				new_draw_info_format(NDI_UNIQUE, 0, op, "You may use %s again.", string);
			else
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "You are forbidden to use %s.", string);
				return 1;
			}
		}
	}

	return 0;
}

/* stop_using_item() - unapplies up to number worth of items of type */
void stop_using_item(object *op, int type, int number)
{
  	object *tmp;

	for (tmp = op->inv; tmp && number; tmp = tmp->below)
		if (tmp->type == type && QUERY_FLAG(tmp, FLAG_APPLIED))
		{
			apply_special(op, tmp, AP_UNAPPLY | AP_IGNORE_CURSE);
			number--;
		}
}

/* update_priest_flag() - if the god does/doesnt have this flag, we
 * give/remove it from the experience object if it doesnt/does
 * already exist. For players only! */

void update_priest_flag(object *god, object *exp_ob, uint32 flag)
{
	if (QUERY_FLAG(god, flag) && !QUERY_FLAG(exp_ob, flag))
		SET_FLAG(exp_ob, flag);
    else if(QUERY_FLAG(exp_ob,flag)&&!QUERY_FLAG(god,flag))
    {
		/* When this is called with the exp_ob set to the player,
		* this check is broken, because most all players arch
		* allow use of weapons.  I'm not actually sure why this
		* check is here - I guess if you had a case where the
		* value in the archetype (wisdom) should over ride the restrictions
		* the god places on it, this may make sense.  But I don't think
		* there is any case like that. */

		/*if (!(QUERY_FLAG(&(exp_ob->arch->clone), flag)))*/
		CLEAR_FLAG(exp_ob, flag);
    };
}


/* determine_god() - determines if op worships a god. Returns
 * the godname if they do. In the case of an NPC, if they have
 * no god, we give them a random one. -b.t. */

const char *determine_god(object *op)
{
    int godnr = -1;

    /* spells */
    if ((op->type == FBULLET || op->type == CONE || op->type == FBALL || op->type == SWARM_SPELL) && op->title)
    {
		if (lookup_god_by_name(op->title) >= 0)
			return op->title;
    }

    if (op->type != PLAYER && QUERY_FLAG(op, FLAG_ALIVE))
	{
		if (!op->title)
		{
			godlink *gl = first_god;

			godnr = rndm(1, gl->id);
			while (gl)
			{
				if (gl->id == godnr)
					break;
				gl = gl->next;
			}
			FREE_AND_COPY_HASH(op->title, gl->name);
		}
		return op->title;
    }


    /* If we are player, lets search a bit harder for the god.  This
     * is a fix for perceive self (before, we just looked at the active
     * skill.) */
    if (op->type == PLAYER)
	{
		object *tmp;

        for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
        {
			if (tmp->type == EXPERIENCE && tmp->stats.Wis)
            {
				if (tmp->title)
	            	return tmp->title;
				else
					return "none";
            }
        }

    }

  	return "none";
}


archetype *determine_holy_arch(object *god, const char *type)
{
    treasure *tr;

    if (!god || !god->randomitems)
	{
        LOG(llevBug, "BUG: determine_holy_arch(): no god or god without randomitems\n");
        return NULL;
    }

    for (tr = god->randomitems->items; tr != NULL; tr = tr->next)
	{
        object *item;

        if (!tr->item)
            continue;

        item = &tr->item->clone;

        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp(item->name, type) == 0)
            return item->other_arch;
    }

    return NULL;
}


static int god_removes_curse(object *op, int remove_damnation)
{
    object *tmp;
    int success = 0;

    for (tmp = op->inv; tmp; tmp = tmp->below)
	{
        if (QUERY_FLAG(tmp, FLAG_DAMNED) && !remove_damnation)
            continue;

        if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
		{
            success = 1;
            CLEAR_FLAG(tmp, FLAG_DAMNED);
            CLEAR_FLAG(tmp, FLAG_CURSED);
            CLEAR_FLAG(tmp, FLAG_KNOWN_CURSED);
            if (op->type == PLAYER)
                esrv_send_item(op, tmp);
        }
    }

    if (success)
        new_draw_info(NDI_UNIQUE, 0, op, "You feel like someone is helping you.");

    return success;
}

static int follower_level_to_enchantments(int level, int difficulty)
{
    if (difficulty < 1)
	{
        LOG(llevBug, "BUG: follower_level_to_enchantments(): difficulty %d is invalid\n", difficulty);
        return 0;
    }

    if (level <= 20)
        return level / difficulty;

    if (level <= 40)
        return (20 + (level - 20) / 2) / difficulty;

    return (30 + (level - 40) / 4) / difficulty;
}

static int god_enchants_weapon(object *op, object *god, object *tr)
{
    char buf[MAX_BUF];
    object *weapon;
    uint32 attacktype;
    int tmp;

    for (weapon = op->inv; weapon; weapon = weapon->below)
        if (weapon->type == WEAPON && QUERY_FLAG(weapon, FLAG_APPLIED))
            break;

    if (weapon == NULL || god_examines_item(god, weapon) <= 0)
        return 0;

    /* First give it a title, so other gods won't touch it */
    if (!weapon->title)
	{
        sprintf(buf, "of %s", god->name);
        FREE_AND_COPY_HASH(weapon->title, buf);

        if (op->type == PLAYER)
	    	esrv_update_item (UPD_NAME, op, weapon);

        new_draw_info(NDI_UNIQUE, 0, op, "Your weapon quivers as if struck!");
    }

    /* Allow the weapon to slay enemies */
    if (!weapon->slaying && god->slaying)
	{
        FREE_AND_COPY_HASH(weapon->slaying, god->slaying);
        new_draw_info_format(NDI_UNIQUE, 0, op, "Your %s now hungers to slay enemies of your god!", weapon->name);
        return 1;
    }

    /* Add the gods attacktype */
    attacktype = (weapon->attacktype == 0) ? AT_PHYSICAL : weapon->attacktype;
    if ((attacktype & god->attacktype) != god->attacktype)
	{
        new_draw_info(NDI_UNIQUE, 0, op, "Your weapon suddenly glows!");
        weapon->attacktype = attacktype | god->attacktype;
        return 1;
    }

    /* Higher magic value */
    tmp = follower_level_to_enchantments(SK_level (op), tr->level);
    if (weapon->magic < tmp)
	{
        new_draw_info(NDI_UNIQUE, 0, op, "A phosphorescent glow envelops your weapon!");
        weapon->magic++;
        if (op->type == PLAYER)
            esrv_update_item(UPD_NAME, op, weapon);
        return 1;
    }

    return 0;
}

static int same_string(const char *s1, const char *s2)
{
    if (s1 == NULL)
        if (s2 == NULL)
            return 1;
        else
            return 0;
    else
        if (s2 == NULL)
            return 0;
        else
            return strcmp(s1, s2) == 0;
}

/* follower_has_similar_item - Checks for any occurrence of
 * the given 'item' in the inventory of 'op' (recursively).
 * Returns 1 if found, else 0. */
static int follower_has_similar_item(object *op, object *item)
{
    object *tmp;

    for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
        if (tmp->type == item->type && same_string(tmp->name, item->name) && same_string(tmp->title, item->title) && same_string(tmp->msg, item->msg) && same_string(tmp->slaying, item->slaying))
            return 1;

        if (tmp->inv && follower_has_similar_item(tmp, item))
            return 1;
    }
    return 0;
}

/* follower_remove_similar_item - Checks for any occurrence of
 * the given 'item' in the inventory of 'op' (recursively).
 * Any matching items in the inventory are deleted, and a
 * message is displayed to the player. */
#if 0
static void follower_remove_similar_item(object *op, object *item)
{
    object *tmp, *next;

    if (op && op->type == PLAYER && CONTR(op))
	{
        /* search the inventory */
        for (tmp = op->inv; tmp != NULL; tmp = next)
		{
			/* backup in case we remove tmp */
	    	next = tmp->below;

			if (tmp->type == item->type && same_string(tmp->name, item->name) && same_string(tmp->title, item->title) && same_string(tmp->msg, item->msg) && same_string(tmp->slaying, item->slaying))
			{
				/* message */
				if (tmp->nrof > 1)
					new_draw_info_format(NDI_UNIQUE, 0, op, "The %s crumble to dust!", query_short_name(tmp, NULL));
				else
					new_draw_info_format(NDI_UNIQUE, 0, op, "The %s crumbles to dust!", query_short_name(tmp, NULL));

				/* remove obj from players inv. */
				remove_ob(tmp);
				/* notify client */
				esrv_del_item(CONTR(op), tmp->count, tmp->env);
			}

	    	if (tmp->inv)
	      		follower_remove_similar_item(tmp, item);
		}
    }
}
#endif

static int god_gives_present(object *op, object *god, treasure *tr)
{
    object *tmp;

    if (follower_has_similar_item(op, &tr->item->clone))
        return 0;

    tmp = arch_to_object(tr->item);
    new_draw_info_format(NDI_UNIQUE, 0, op, "%s lets %s appear in your hands.", god->name, query_short_name(tmp, NULL));
    tmp = insert_ob_in_ob(tmp, op);

    if (op->type == PLAYER)
        esrv_send_item(op, tmp);

    return 1;
}


/* god_intervention() - called from praying() currently. Every
 * once in a while the god will intervene to help the worshiper.
 * Later, this fctn can be used to supply quests, etc for the
 * priest. -b.t.  */

void god_intervention(object *op, object *god)
{
    int level = SK_level(op);
    treasure *tr;

    if (!god || !god->randomitems)
	{
        LOG(llevBug, "BUG: god_intervention(): (p:%s) no god %s or god without randomitems\n", query_name(op, NULL), query_name(god, NULL));
        return;
    }

    check_special_prayers(op, god);

    /* lets do some checks of whether we are kosher with our god */
    if (god_examines_priest(op, god) < 0)
        return;

    new_draw_info(NDI_UNIQUE, 0, op, "You feel a holy presence!");

    for (tr = god->randomitems->items; tr != NULL; tr = tr->next)
	{
        object *item;

        if (tr->chance <= random_roll(0, 99, op, PREFER_HIGH))
            continue;

        /* Treasurelist - generate some treasure for the follower */
        if (tr->name)
		{
            treasurelist *tl = find_treasurelist(tr->name);

            if (tl == NULL)
                continue;

            new_draw_info(NDI_UNIQUE, 0, op, "Something appears before your eyes. You catch it before it falls to the ground.");
            create_treasure(tl, op, GT_STARTEQUIP | GT_ONLY_GOOD | GT_UPDATE_INV, level, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
            return;
        }

        if (!tr->item)
		{
            LOG(llevBug, "BUG: empty entry in %s's treasure list\n", query_name(god, NULL));
            continue;
        }
        item = &tr->item->clone;

        /* Grace limit */
        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp(item->name, "grace limit") == 0)
        {
            if (op->stats.grace < item->stats.grace || op->stats.grace < op->stats.maxgrace)
            {
                /* Follower lacks the required grace for the following
                 * treasure list items. */
                (void) cast_change_attr(op, op, op, 0, SP_HOLY_POSSESSION);
                return;
            }
            continue;
        }

        /* Restore grace */
        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp(item->name, "restore grace") == 0)
        {
            if (op->stats.grace >= 0)
                continue;

            op->stats.grace = random_roll(0, 9, op, PREFER_HIGH);
            new_draw_info(NDI_UNIQUE, 0, op, "You are returned to a state of grace.");
            return;
        }

        /* Heal damage */
        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp(item->name, "restore hitpoints") == 0)
        {
            if (op->stats.hp >= op->stats.maxhp)
                continue;

            new_draw_info(NDI_UNIQUE, 0, op, "A white light surrounds and heals you!");
            op->stats.hp = op->stats.maxhp;
            return;
        }

        /* Restore spellpoints */
        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp(item->name, "restore spellpoints") == 0)
        {
            int max = (int)((float)op->stats.maxsp * ((float)item->stats.maxsp / (float)100.0));
            /* Restore to 50 .. 100%, if sp < 50% */
            int new_sp = (int)((float)random_roll(1000, 1999, op, PREFER_HIGH) / (float)2000.0 * (float)max);

            if (op->stats.sp >= max / 2)
                continue;

            new_draw_info(NDI_UNIQUE, 0, op, "A blue lightning strikes your head but doesn't hurt you!");
            op->stats.sp = new_sp;
        }

        /* Various heal spells */
        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp(item->name, "heal spell") == 0)
        {
            if (cast_heal(op, 1, op, get_spell_number(item)))
                return;
            else
                continue;
        }

        /* Remove curse */
        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp(item->name, "remove curse") == 0)
        {
            if (god_removes_curse(op, 0))
                return;
            else
                continue;
        }

        /* Remove damnation */
        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp(item->name, "remove damnation") == 0)
        {
            if (god_removes_curse(op, 1))
                return;
            else
                continue;
        }

        /* Heal depletion */
        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp (item->name, "heal depletion") == 0)
        {
            object *depl;
            archetype *at;
            int i;

            if ((at = find_archetype("depletion")) == NULL)
			{
                LOG(llevBug, "BUG: Could not find archetype depletion.\n");
                continue;
            }
            depl = present_arch_in_ob(at, op);

            if (depl == NULL)
                continue;

            new_draw_info(NDI_UNIQUE, 0, op, "Shimmering light surrounds and restores you!");
            for (i = 0; i < 7; i++)
                if (get_attr_value(&depl->stats, i))
                    new_draw_info(NDI_UNIQUE, 0, op, restore_msg[i]);

            remove_ob(depl);
            fix_player(op);
            return;
        }

        /* Messages */
        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp(item->name, "message") == 0)
        {
            new_draw_info(NDI_UNIQUE, 0, op, item->msg);
            return;
        }

        /* Enchant weapon */
        if (item->type == BOOK && IS_SYS_INVISIBLE(item) && strcmp (item->name, "enchant weapon") == 0)
        {
            if (god_enchants_weapon(op, god, item))
                return;
            else
                continue;
        }

        /* Spellbooks - works correctly only for prayers */
        if (item->type == SPELLBOOK)
        {
            int spell = get_spell_number(item);

            if (check_spell_known(op, spell))
                continue;

            if (spells[spell].level > level)
                continue;

            if (IS_SYS_INVISIBLE(item))
			{
                new_draw_info_format(NDI_UNIQUE, 0, op, "%s grants you use of a special prayer!", god->name);
                do_learn_spell(op, spell, 1);
                return;
            }

            if (!QUERY_FLAG(item, FLAG_STARTEQUIP))
			{
                LOG(llevBug, "BUG: visible spellbook in %s's treasure list lacks FLAG_STARTEQUIP\n", query_name(god, NULL));
                continue;
            }

            if (!item->stats.Wis)
			{
                LOG(llevBug, "BUG: visible spellbook in %s's treasure list doesn't contain a special prayer\n", query_name(god, NULL));
                continue;
            }

            if (god_gives_present(op, god, tr))
                return;
            else
                continue;
        }

        /* Other gifts */
        if (!IS_SYS_INVISIBLE(item))
		{
          	if (god_gives_present(op, god, tr))
                return;
            else
                continue;
        }
    }

    new_draw_info(NDI_UNIQUE, 0, op, "You feel rapture.");
}


int god_examines_priest(object *op, object *god)
{
	int reaction = 1;
	object *item = NULL;

	for (item = op->inv; item; item = item->below)
	{
		if (QUERY_FLAG(item, FLAG_APPLIED))
			reaction += god_examines_item(god, item) * (item->magic ? abs(item->magic) : 1);
	}

	/* well, well. Looks like we screwed up. Time for god's revenge */
	if (reaction < 0)
	{
		char buf[MAX_BUF];
		int loss = 10000000;
		int angry = abs(reaction);

		if (op->chosen_skill->exp_obj)
			loss = (int)((float)0.05 * (float) op->chosen_skill->exp_obj->stats.exp);

		lose_priest_exp(op, random_roll(0, loss * angry - 1, op, PREFER_LOW));

		if (random_roll(0, angry, op, PREFER_LOW))
			cast_mana_storm(op, SK_level(op) + (angry * 3));

		sprintf(buf, "%s becomes angry and punishes you!", god->name);
		new_draw_info(NDI_UNIQUE | NDI_NAVY, 0, op, buf);
	}

	return reaction;
}

/* god_examines_item() - your god looks at the item you
 * are using and we return either -1 (bad), 0 (neutral) or
 * 1 (item is ok). If you are using the item of an enemy
 * god, it can be bad...-b.t. */
int god_examines_item(object *god, object *item)
{
  	char buf[MAX_BUF];

  	if (!god || !item)
		return 0;

	/* unclaimed item are ok */
  	if (!item->title)
		return 1;

  	sprintf(buf, "of %s", god->name);

	/* belongs to that God */
  	if (!strcmp(item->title, buf))
		return 1;

	/* check if we have any enemy blessed item */
	if (god->title)
	{
		sprintf(buf, "of %s", god->title);
		if (!strcmp(item->title, buf))
		{
			if (item->env)
			{
				char buf[MAX_BUF];
				sprintf(buf, "Heretic! You are using %s!", query_name(item, NULL));
				new_draw_info(NDI_UNIQUE | NDI_NAVY, 0, item->env, buf);
			}
			return -1;
		}
	}

	/* item is sacred to a non-enemy god/or is otherwise magical */
  	return 0;
}

/* get_god() - returns the gods index in linked list
 * if exists, if not, it returns -1. -b.t.  */
int get_god(object *priest)
{
  	int godnr = lookup_god_by_name(determine_god(priest));

  	return godnr;
}


/* tailor_god_spell() - changes the attributes of cone, smite,
 * and ball spells as needed by the code. Returns false if there
 * was no race to assign to the slaying field of the spell, but
 * the spell attacktype contains AT_HOLYWORD.  -b.t. */
int tailor_god_spell(object *spellop, object *caster)
{
    object *god = find_god(determine_god(caster));
    int caster_is_spell = 0;

    if (caster->type == FBULLET || caster->type == CONE || caster->type == FBALL || caster->type == SWARM_SPELL)
		caster_is_spell = 1;

    if (!god || (spellop->attacktype & AT_HOLYWORD && !god->race))
	{
        if (!caster_is_spell)
            new_draw_info(NDI_UNIQUE, 0, caster, "This prayer is useless unless you worship an appropriate god.");
        else
            LOG(llevBug, "BUG: tailor_god_spell(): no god\n");

        return 0;
    }

    /* either holy word or godpower attacks will set the slaying field */
    if (spellop->attacktype & AT_HOLYWORD || spellop->attacktype & AT_GODPOWER)
	{
		FREE_AND_CLEAR_HASH2(spellop->slaying);
        if (!caster_is_spell)
		{
            FREE_AND_COPY_HASH(spellop->slaying, god->slaying);
		}
	 	else if (caster->slaying)
	    	FREE_AND_COPY_HASH(spellop->slaying, caster->slaying);
    }

    /* only the godpower attacktype adds the god's attack onto the spell */
    if (spellop->attacktype & AT_GODPOWER)
         spellop->attacktype = spellop->attacktype | god->attacktype;

    /* tack on the god's name to the spell */
    if (spellop->attacktype & AT_HOLYWORD || spellop->attacktype & AT_GODPOWER)
	{
        FREE_AND_COPY_HASH(spellop->title, god->name);
        if (spellop->title)
		{
	   		char buf[MAX_BUF];
	   		sprintf(buf, "%s of %s", spellop->name, spellop->title);
	   		FREE_AND_COPY_HASH(spellop->name, buf);
		}
    }

    return 1;
}

/* we need a skill for this, not the skill group! */
void lose_priest_exp(object *pl, int loss)
{
	(void) pl;
	(void) loss;
#if 0
  	if (!pl || pl->type != PLAYER || !pl->chosen_skill || !pl->chosen_skill->exp_obj)
  	{
    	LOG(llevBug, "BUG: Bad call to lose_priest_exp() \n");
    	return;
  	}

  	if ((loss = check_dm_add_exp_to_obj(pl->chosen_skill->exp_obj, loss)))
    	add_exp(pl, -loss, pl->chosen_skill->stats.sp);
#endif
}
