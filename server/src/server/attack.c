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

#include <global.h>
#include <material.h> /* this must stay here - there is a double init in the module*/

#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/* this is our "attackform to protection" table.
   it maps 32+ attack forms to our ~20 protections */
int protection_tab[NROFATTACKS] =
{
	PROTECT_PHYSICAL,10,5,7,6,11,9,
	11,4,17,8,12,12,   /* drain to mind and ghosthit to death... */
	18,11,10,15,17,16, /* turn undead and godpower to holy - thats the hard one */
	10,18,14,12,18,13, /* holy word to energy (pure grace power) and life steal is psionic */
	1,2,3,15,14,19,13  /* internal to holy... just a joke, we never use this entry */
};

#define ATTACK_HIT_DAMAGE(_op, _anum)       dam=dam*((double)_op->attack[_anum]*(double)0.01);dam>=1.0f?(damage=(int)dam):(damage=1)
#define ATTACK_RESIST_DAMAGE(_op, _anum)    dam=dam*((double)(100-_op->resist[_anum])*(double)0.01)
#define ATTACK_PROTECT_DAMAGE(_op, _anum)    dam=dam*((double)(100-_op->protection[protection_tab[_anum]])*(double)0.01)

/*#define ATTACK_DEBUG*/

/* some static defines */
static void thrown_item_effect (object *, object *);
static int get_attack_mode (object **target, object **hitter,int *simple_attack);
static int abort_attack (object *target, object *hitter, int simple_attack);
static int attack_ob_simple (object *op, object *hitter, int base_dam, int base_wc);
static void send_attack_msg(object *op, object *hitter, int attacknum, int dam, int damage);


/* if we attack something, we start here.
 * this function just checks for right head part of possible multi arches,
 * and selects the right dam/wc
 * for special events, we can assign different dam/wc as coming on default from
 * object - in this cases, attack_ob_simple() is called from them internal.
 */
int attack_ob(object *op, object *hitter)
{
	if (op->head)
		op = op->head;

	if (hitter->head)
		hitter = hitter->head;

	return attack_ob_simple (op, hitter, hitter->stats.dam, hitter->stats.wc);
}

/* attack_ob_simple()
 * here we decide a attack will happen and how. blocking, parry, missing is handled
 * here inclusive the sounds. All whats needed before we count damage and say "hit you".
 */
static int attack_ob_simple(object *op, object *hitter, int base_dam, int base_wc)
{
	int simple_attack, roll, dam = 0;
	uint32 type;
	tag_t op_tag, hitter_tag;

	if (op->head)
		op = op->head;

	if (hitter->head)
		hitter = hitter->head;

	if (get_attack_mode(&op, &hitter, &simple_attack))
		goto error;

	/* Trigger the ATTACK event */
	trigger_event(EVENT_ATTACK, hitter, hitter, op, NULL, 0, &base_dam, &base_wc, SCRIPT_FIX_ALL);

	op_tag = op->count;
	hitter_tag = hitter->count;

	if (!hitter->stats.wc_range)
	{
		LOG(llevDebug, "BUG attack.c: hitter %s has wc_range == 0! (set to 20)\n", query_name(hitter, NULL));
		hitter->stats.wc_range = 20;
	}
	roll = random_roll(0, hitter->stats.wc_range, hitter, PREFER_HIGH);

	/* Adjust roll for various situations. */
	if (!simple_attack)
		roll += adj_attackroll(hitter, op);

	/* so we do one swing */
	if (hitter->type == PLAYER)
		CONTR(hitter)->anim_flags |= PLAYER_AFLAG_ENEMY;

	/* Force player to face enemy */
	if (hitter->type == PLAYER)
	{
		rv_vector dir;

		if (get_rangevector(hitter, op, &dir, RV_NO_DISTANCE))
		{
			if (hitter->head)
			{
				hitter->head->anim_enemy_dir = dir.direction;
				hitter->head->facing = dir.direction;
			}
			else
			{
				hitter->anim_enemy_dir = dir.direction;
				hitter->facing = dir.direction;
			}
		}
	}

	/* See if we hit the creature */
	if (roll >= hitter->stats.wc_range || op->stats.ac <= base_wc+roll)
	{
		int hitdam = base_dam;

		/* at this point NO ONE will still sleep */
		CLEAR_FLAG(op, FLAG_SLEEP);

		/* i don't use sub_type atm - using it should be smarter i the future */
		if (hitter->type == ARROW)
			play_sound_map(hitter->map, hitter->x, hitter->y, SOUND_ARROW_HIT, SOUND_NORMAL);
		else
		{
			if (hitter->attack[ATNR_SLASH])
				play_sound_map(hitter->map, hitter->x, hitter->y, SOUND_HIT_SLASH, SOUND_NORMAL);
			else if (hitter->attack[ATNR_CLEAVE])
				play_sound_map(hitter->map, hitter->x, hitter->y, SOUND_HIT_CLEAVE, SOUND_NORMAL);
			else if (hitter->attack[ATNR_PHYSICAL])
				play_sound_map(hitter->map, hitter->x, hitter->y, SOUND_HIT_IMPACT, SOUND_NORMAL);
			else
				play_sound_map(hitter->map, hitter->x, hitter->y, SOUND_HIT_PIERCE, SOUND_NORMAL);
		}

		if (!simple_attack)
		{

			/* A NPC call for help - this should be part of AI
			if (op->type != PLAYER && ! can_see_enemy (op, hitter)
			    && ! get_owner (op) && rndm(0, op->stats.Int))
			    npc_call_help (op);
			*/
			/* old HIDE code
			if (op->hide && QUERY_FLAG (hitter, FLAG_ALIVE)) {
			    make_visible (op);
			    if (op->type == PLAYER)
			        new_draw_info (NDI_UNIQUE, 0, op,
			                       "You were hit by a wild attack. "
			                       "You are no longer hidden!");
			}
			*/

			/* thrown items (hitter) will have various effects
			 * when they hit the victim.  For things like thrown daggers,
			 * this sets 'hitter' to the actual dagger, and not the
			 * wrapper object.
			 */
			thrown_item_effect(hitter, op);
			if (was_destroyed (hitter, hitter_tag) || was_destroyed (op, op_tag) || abort_attack (op, hitter, simple_attack))
				goto leave;
		}

		/* Need to do at least 1 damage, otherwise there is no point
		 * to go further and it will cause FPE's below. */
		if (hitdam <= 0)
			hitdam = 1;

		/* attacktype will be removed !*/
		type = hitter->attacktype;
		if (!type)
			type = AT_PHYSICAL;

		/* Handle monsters that hit back */
		if (!simple_attack && QUERY_FLAG(op, FLAG_HITBACK) && IS_LIVE(hitter))
		{
			hit_player(hitter, random_roll(0, (op->stats.dam), hitter, PREFER_LOW), op, op->attacktype);
			if (was_destroyed(op, op_tag) || was_destroyed(hitter, hitter_tag) || abort_attack(op, hitter, simple_attack))
				goto leave;
		}

		/* In the new attack code, it should handle multiple attack
		 * types in its area, so remove it from here.
		 * i increased dmg output ... from 1 to max to 50% to max. */
		dam = hit_player(op, random_roll(hitdam / 2 + 1, hitdam, hitter, PREFER_HIGH), hitter, type);
		if (was_destroyed(op, op_tag) || was_destroyed(hitter, hitter_tag) || abort_attack(op, hitter, simple_attack))
			goto leave;
	}
	/* end of if hitter hit op */
	/* if we missed, dam=0 */
	else
	{
#ifdef ATTACK_TIMING_DEBUG
		{
			char buf[256];
			sprintf(buf,"MISS %s :%d>=%d-%d l:%d aw:%d/%d d:%d/%d h:%d/%d (%d)", hitter->name, op->stats.ac,base_wc, roll, hitter->level, hitter->stats.ac, hitter->stats.wc, hitter->stats.dam_adj, hitter->stats.dam, hitter->stats.hp, hitter->stats.maxhp_adj, op->stats.hp);
			new_draw_info(NDI_ALL, 1, NULL, buf);
			/*
			sprintf(buf,"MISS! attacker %s:: roll:%d ac:%d wc:%d dam:%d", hitter->name, roll,hitter->stats.ac,hitter->stats.wc, hitter->stats.dam);
			new_draw_info(NDI_UNIQUE, 0,hitter, buf);
			sprintf(buf,"defender %s:: ac:%d wc:%d dam:%d", op->name, op->stats.ac,op->stats.wc, op->stats.dam);
			new_draw_info(NDI_UNIQUE, 0,hitter, buf);
			*/
		}
#endif
		if (hitter->type != ARROW)
		{
			if (hitter->type == PLAYER)
				play_sound_map(hitter->map, hitter->x, hitter->y, SOUND_MISS_PLAYER, SOUND_NORMAL);
			else
				play_sound_map(hitter->map, hitter->x, hitter->y, SOUND_MISS_MOB, SOUND_NORMAL);
		}
	}

	goto leave;

error:
	dam = 1;
	goto leave;

leave:
	return dam;
}

/* This isn't used just for players, but in fact most objects.
 * op is the object to be hit, dam is the amount of damage, hitter
 * is what is hitting the object, and type is the attacktype.
 * dam is base damage - protections/vulnerabilities/slaying matches can
 * modify it. */

/* Oct 95 - altered the following slightly for MULTIPLE_GODS hack
 * which needs new attacktype AT_HOLYWORD to work . b.t. */

/* 05.2002: I rewroted this in major parts. I removed much of the to
 * complex or redundant stuff. Most of the problems here should be handled outside. */
int hit_player(object *op, int dam, object *hitter, int type)
{
	object *hit_obj, *target_obj;
	int maxdam = 0;
	int attacknum, hit_level;
	int simple_attack;
	tag_t op_tag, hitter_tag;
	int rtn_kill = 0;

	/* if our target has no_damage 1 set or is wiz, we can't hurt him */
	if (QUERY_FLAG (op, FLAG_WIZ) || QUERY_FLAG(op, FLAG_INVULNERABLE))
		return 0;

	if (hitter->head)
		hitter = hitter->head;

	if (op->head)
		op = op->head;

	/* TODO:i must fix here a bit for scrolls, rods, wands and other fix level stuff! */
	/* the trick: we get for the TARGET the real level - even for player.
	 * but for hitter, we always use the SKILL LEVEL if player! */
	if (!(hit_obj = get_owner(hitter)))
		hit_obj = hitter;

	if (!(target_obj = get_owner(op)))
		target_obj = op;

	/* get from hitter object the right skill level! */
	if (hit_obj->type == PLAYER)
		hit_level = SK_level(hit_obj);
	else
		hit_level = hitter->level;

	/* very useful sanity check! */
	if (hit_level == 0 || target_obj->level == 0)
	{
		LOG(llevDebug,"DEBUG: hit_player(): hit or target object level == 0(h:>%s< (o:>%s<) l->%d t:>%s< (>%s<)(o:>%s<) l->%d\n", query_name(hitter, NULL), query_name(get_owner(hitter), NULL), hit_level, query_name(op, NULL), target_obj->arch->name, query_name(get_owner(op), NULL), target_obj->level);
	}

	/* Do not let friendly objects attack each other. */
	if (is_friend_of(hit_obj, op))
	{
		return 0;
	}

	/* I disabled this for the time being - it's a ridiculous piece of code. Makes lvl 110 raas do about 10000 damage to lvl 10. - A.T. 2009 */
#if 0
	/* i turned it now off for players! */
	if (hit_level > target_obj->level && hit_obj->type == MONSTER)
	{
		dam += (int)((float)(dam/2)*((float)(hit_level-target_obj->level)/
									 (target_obj->level>25?25.0f:(float)target_obj->level)));
		/*LOG(llevDebug,"DMG-ADD: hl:%d tl_%d -> d:%d \n", hit_level,target_obj->level, dam);*/
	}
#endif

	/* something hit player (can be disease or poison too - break praying! */
	if (op->type == PLAYER && CONTR(op)->was_praying)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Your praying is disrupted.");
		CONTR(op)->praying = 0;
		CONTR(op)->was_praying = 0;
	}

	/* Check for pvp! Only when at THIS moment both possible player are in pvp area - then we do damage.
	 * This avoid this kind of heros, standing on pvp border, firing in and running back to save.
	 * on the other side, running in safe areas will help you when hunted - and thats always a great fun. */
	if (op->type == PLAYER || (get_owner(op) && op->owner->type == PLAYER))
	{
		if (hitter->type == PLAYER || (get_owner(hitter) && hitter->owner->type == PLAYER))
		{
			/* now we are sure player are involved. Get the real player object now and test! */
			if (!pvp_area(op->type == PLAYER ? op : get_owner(op), hitter->type == PLAYER ? hitter : get_owner(hitter)))
				return 0;
		}
	}

	/* this checks objects are valid, on same map and set them to head when needed! */
	/* also, simple_attack is set to 1, when one of the parts hav ->env != null
	 * atm, this value is only used in the door attack */
	if (get_attack_mode(&op, &hitter, &simple_attack))
		return 0;

	op_tag = op->count;
	hitter_tag = hitter->count;

	/* i removed the "is_alive" check. We should handle this before we are here.
	 * if a object is IS_PLAYER or IS_ALIVE - it should can be attackable.
	 * It it has HP>0, it is still alive/not destroyed. */
	if (op->stats.hp < 0)
	{
		/* FIXME: If a player is killed by a rune in a door, the
		 * was_destroyed() check above doesn't return, and might get here.
		 */
		/* seems to happen with throwing items alot... i let it still in to see
		 * what else possible invoke this glitch. we catch it here and ok.
		 */
		LOG(llevDebug, "FIXME: victim (arch %s, name %s (%x - %d)) already dead in hit_player()\n", op->arch->name, query_name(op, NULL), op, op->count);
		return 0;
	}

	/* Go through and hit the player with each attacktype, one by one.
	 * hit_player_attacktype only figures out the damage, doesn't inflict
	 * it.  It will do the appropriate action for attacktypes with
	 * effects (slow, paralization, etc.
	 */
	for (attacknum = 0; attacknum < NROFATTACKS; attacknum++)
	{
		if (hitter->attack[attacknum])

#ifdef ATTACK_DEBUG
		{
			int tmp;
			tmp = hit_player_attacktype(op, hitter, dam, attacknum);
			/*new_draw_info_format(NDI_ALL|NDI_UNIQUE,5,NULL,"%s hits %s with attack #%d with %d damage\n",hitter->name, op->name, attacknum,tmp);*/
			maxdam += tmp;
		}
#else
		{
			/*			LOG(-1, "hitter: %f - %s (dam:%d/%d) (wc:%d/%d)(wcr:%d/%d)(ac:%d/%d) ap:%d\n",hitter->speed,
							hitter->name,hitter->stats.dam,op->stats.dam, hitter->stats.wc,op->stats.wc,hitter->stats.wc_range,op->stats.wc_range,
							hitter->stats.ac,op->stats.ac,hitter->attack[attacknum]);
			*/
			maxdam += hit_player_attacktype(op, hitter, dam, attacknum);
		}
#endif
	}

	/* if one get attacked, the attacker will become the enemy */
	if (!OBJECT_VALID(op->enemy, op->enemy_count))
	{
		/* assign the owner as bad boy */
		if (get_owner(hitter))
			set_npc_enemy(op, hitter->owner, NULL);
		/* or normal mob */
		else if (QUERY_FLAG(hitter, FLAG_MONSTER))
			set_npc_enemy(op, hitter, NULL);
	}
	/* TODO: also handle op->attacked_by here */

	/* this is needed to send the hit number animations to the clients */
	if (op->damage_round_tag != ROUND_TAG)
	{
		op->last_damage = 0;
		op->damage_round_tag = ROUND_TAG;
	}
	if ((op->last_damage + maxdam) > 64000)
		op->last_damage = 64000;
	else
		op->last_damage += maxdam;

	/* thats the damage the target got */
	op->stats.hp -= maxdam;

	/* Eneq(@csd.uu.se): Check to see if monster runs away. */
	if ((op->stats.hp >= 0) && QUERY_FLAG(op, FLAG_MONSTER) && op->stats.hp < (signed short)(((float)op->run_away / 100.0f) * (float)op->stats.maxhp))
		SET_FLAG(op, FLAG_RUN_AWAY);

	if (QUERY_FLAG(op, FLAG_TEAR_DOWN))
	{
		tear_down_wall(op);
		/* nothing more to do for wall */
		return maxdam;
	}

	/* Start of creature kill processing */

	/* rtn_kill is here negative! */
	if ((rtn_kill = kill_object(op, dam, hitter, type)))
		return (maxdam + rtn_kill + 1);

	/* End of creature kill processing */

	/* Used to be ghosthit removal - we now use the ONE_HIT flag.  Note
	 * that before if the player was immune to ghosthit, the monster
	 * remained - that is no longer the case. */
	if (QUERY_FLAG(hitter, FLAG_ONE_HIT))
	{
		if (QUERY_FLAG(hitter, FLAG_FRIENDLY))
			remove_friendly_object(hitter);

		/* Remove, but don't drop inventory */
		remove_ob(hitter);
		check_walk_off(hitter, NULL, MOVE_APPLY_VANISHED);
	}
	/* Lets handle creatures that are splitting now */
	else if ((type & AT_PHYSICAL) && !OBJECT_FREE(op) && QUERY_FLAG(op, FLAG_SPLITTING))
	{
		int i;
		int friendly = QUERY_FLAG(op, FLAG_FRIENDLY);
		int unaggressive = QUERY_FLAG(op, FLAG_UNAGGRESSIVE);
		object *owner = get_owner(op);

		if (!op->other_arch)
		{
			LOG(llevBug, "BUG: SPLITTING without other_arch error.\n");
			return maxdam;
		}
		if (friendly)
			remove_friendly_object(op);

		remove_ob(op);
		if (check_walk_off(op, NULL,MOVE_APPLY_VANISHED) == CHECK_WALK_OK)
		{
			/* This doesn't handle op->more yet */
			for (i = 0; i < NROFNEWOBJS(op); i++)
			{
				object *tmp = arch_to_object(op->other_arch);
				int j;

				tmp->stats.hp = op->stats.hp;
				if (friendly)
				{
					SET_FLAG(tmp, FLAG_FRIENDLY);
					add_friendly_object(tmp);
					tmp->move_type = PETMOVE;
					if (owner != NULL)
						set_owner(tmp, owner);
				}

				if (unaggressive)
					SET_FLAG(tmp, FLAG_UNAGGRESSIVE);

				j = find_first_free_spot(tmp->arch, op->map,op->x, op->y);
				/* Found spot to put this monster */
				if (j >= 0)
				{
					tmp->x = op->x + freearr_x[j], tmp->y = op->y + freearr_y[j];
					insert_ob_in_map(tmp,op->map, NULL, 0);
				}
			}
		}
	}
	else if ((type & AT_DRAIN) && hitter->type == GRIMREAPER && hitter->value++ > 10)
	{
		remove_ob(hitter);
		check_walk_off(hitter, NULL, MOVE_APPLY_VANISHED);
	}
	return maxdam;
}

/* if we drop for example a spell object like a fireball to the map,
 * they move from tile to tile. Every time they check the object they
 * "hit on this map tile". If they find some - we are here. */
int hit_map(object *op, int dir, int type)
{
	object *tmp, *next, *tmp_obj, *tmp_head;
	mapstruct *map;
	int x, y;
	/* added this flag..  will return 1 if it hits a monster */
	int mflags, retflag = 0;
	tag_t op_tag, next_tag = 0;

	if (OBJECT_FREE(op))
	{
		LOG(llevBug, "BUG: hit_map(): free object\n");
		return 0;
	}

	if (QUERY_FLAG(op, FLAG_REMOVED) || op->env != NULL)
	{
		LOG(llevBug, "BUG: hit_map(): hitter (arch %s, name %s) not on a map\n", op->arch->name, query_name(op, NULL));
		return 0;
	}

	if (op->head)
		op = op->head;

	op_tag = op->count;

	if (!op->map)
	{
		LOG(llevBug, "BUG: hit_map(): %s has no map.\n", query_name(op, NULL));
		return 0;
	}

	x = op->x + freearr_x[dir];
	y = op->y + freearr_y[dir];
	if (!(map = out_of_map(op->map, &x, &y)))
		return 0;

	mflags = GET_MAP_FLAGS(map, x, y);

	next = get_map_ob(map, x, y);
	if (next)
		next_tag = next->count;

	if (!(tmp_obj = get_owner(op)))
		tmp_obj = op;

	if (tmp_obj->head)
		tmp_obj = tmp_obj->head;

	while (next)
	{
		if (was_destroyed(next, next_tag))
		{
			/* There may still be objects that were above 'next', but there is no
			 * simple way to find out short of copying all object references and
			 * tags into a temporary array before we start processing the first
			 * object.  That's why we just abort.
			 *
			 * This happens whenever attack spells (like fire) hit a pile
			 * of objects. This is not a bug - nor an error.
			 *
			 * Gecko: this may be a little different now, since we don't really destroy object until
			 * end of timestep. */
			break;
		}
		tmp = next;
		next = tmp->above;
		if (next)
			next_tag = next->count;

		if (OBJECT_FREE(tmp))
		{
			LOG(llevBug, "BUG: hit_map(): found freed object (%s)\n", tmp->arch->name ? tmp->arch->name : "<NULL>");
			break;
		}

		/* Something could have happened to 'tmp' while 'tmp->below' was processed.
		 * For example, 'tmp' was put in an icecube.
		 * This is one of the few cases where on_same_map should not be used. */
		if (tmp->map != map || tmp->x != x || tmp->y != y)
			continue;

		/* first, we check player .... */
		if (QUERY_FLAG(tmp, FLAG_IS_PLAYER))
		{
			hit_player(tmp, op->stats.dam, op, type);
			retflag |= 1;
			if (was_destroyed(op, op_tag))
				break;
		}
		else if (IS_LIVE(tmp))
		{
			tmp_head = tmp->head ? tmp->head : tmp;
			if (tmp_head->type == MONSTER)
			{
				/* monster vs. monster */
				if (tmp_obj->type == MONSTER)
				{
					/* lets decide that monster of same kind don't hurt itself */
					if (QUERY_FLAG(tmp_head, FLAG_FRIENDLY))
					{
						if (QUERY_FLAG(tmp_obj, FLAG_FRIENDLY))
							continue;
					}
					else
					{
						if (!QUERY_FLAG(tmp_obj, FLAG_FRIENDLY))
							continue;
					}
				}

			}
			/*LOG(-1,"HM: %s hit %s (%d)with dam %d\n",op->name,tmp->name,tmp->type,op->stats.dam);*/
			hit_player(tmp, op->stats.dam, op, type);
			retflag |= 1;
			if (was_destroyed(op, op_tag))
				break;
		}
		else if (tmp->material && op->stats.dam > 0)
		{
			save_throw_object(tmp, op);
			if (was_destroyed(op, op_tag))
				break;
		}
	}

	return 0;
}

/* This returns the amount of damage hitter does to op with the
 * appropriate attacktype.  Only 1 attacktype should be set at a time.
 * This doesn't damage the player, but returns how much it should
 * take.  However, it will do other effects (paralyzation, slow, etc.)
 * Note - changed for PR code - we now pass the attack number and not
 * the attacktype.  Makes it easier for the PR code.  */
int hit_player_attacktype(object *op, object *hitter, int damage, uint32 attacknum)
{
	double dam = (double) damage;
	int doesnt_slay = 1;

	/* just a sanity check */
	if (dam < 0)
	{
		LOG(llevBug, "BUG: hit_player_attacktype called with negative damage: %d from object: %s\n", dam, query_name(op, NULL));
		return 0;
	}

	if (hitter->slaying)
	{
		if (((op->race != NULL) && strstr(hitter->slaying, op->race)) || (op->arch && (op->arch->name != NULL) &&  strstr(op->arch->name, hitter->slaying)))
		{
			doesnt_slay = 0;
			if (QUERY_FLAG(hitter, FLAG_IS_ASSASSINATION))
				damage = (int)((double)damage * 2.25);
			else
				damage = (int)((double)damage * 1.75);

			dam = (double) damage;
		}
	}

	/* AT_INTERNAL is supposed to do exactly dam.  Put a case here so
	 * people can't mess with that or it otherwise get confused.  */
	/* I extended this  function - it now maps the special damage to the
	 * depending on the attacking object. For example we fake a poison attack
	 * from a poison object - we need to handle this here because we don't want
	 * repoison the target every time recursive.
	 */
	if (attacknum == ATNR_INTERNAL)
	{
		/* adjust damage */
		dam = dam * ((double)hitter->attack[ATNR_INTERNAL] / 100.0);

		/* handle special object attacks */
		/* we have a poison force object (thats the poison we had inserted) */
		if (hitter->type == POISONING)
		{
			/* map to poison... */
			attacknum = ATNR_POISON;
			if (op->resist[attacknum] == 100 || op->protection[protection_tab[attacknum]] == 100)
			{
				dam = 0;
				send_attack_msg(op, hitter, attacknum, (int) dam, damage);
				goto jump_show_dmg;
			}

			/* reduce to % resistance */
			if (op->resist[attacknum])
				ATTACK_RESIST_DAMAGE(op, attacknum);

			/* reduce to % protection */
			ATTACK_PROTECT_DAMAGE(op, attacknum);
		}

		if (damage && dam < 1.0)
			dam = 1.0;

		send_attack_msg(op, hitter, attacknum, (int) dam, damage);
		goto jump_show_dmg;
	}

	/* quick check for immunity - if so, we skip here.
	 * our formula is (100 - resist) / 100 - so test for 100 = zero division */
	if (op->resist[attacknum]==100 || op->protection[protection_tab[attacknum]] == 100)
	{
		dam = 0;
		send_attack_msg(op, hitter, attacknum, (int) dam, damage);
		goto jump_show_dmg;
	}

	switch (attacknum)
	{
			/* quick check for disease! */
		case ATNR_PHYSICAL:
			check_physically_infect(op, hitter);
		case ATNR_SLASH:
		case ATNR_CLEAVE:
		case ATNR_PIERCE:
#ifdef ATTACK_DEBUG
			/*new_draw_info_format(NDI_ALL|NDI_UNIQUE,5,NULL,"** Start attack #%d with %f damage\n",attacknum,dam);*/
#endif
			/* get % of dam from this attack form */
			ATTACK_HIT_DAMAGE(hitter, attacknum);
#ifdef ATTACK_DEBUG
			/*new_draw_info_format(NDI_ALL|NDI_UNIQUE,5,NULL,"** After attack[%d]: %f damage\n",hitter->attack[attacknum],dam);*/
#endif
			/* reduce to % resistance */
			if (op->resist[attacknum])
				ATTACK_RESIST_DAMAGE(op, attacknum);
#ifdef ATTACK_DEBUG
			/*new_draw_info_format(NDI_ALL|NDI_UNIQUE,5,NULL,"** After resist[%d]: %f damage\n",op->resist[attacknum],dam);*/
#endif
			/* reduce to % protection */
			ATTACK_PROTECT_DAMAGE(op, attacknum);
#ifdef ATTACK_DEBUG
			/*new_draw_info_format(NDI_ALL|NDI_UNIQUE,5,NULL,"** After protect[%d]: %f damage\n",op->protection[protection_tab[attacknum]],dam);*/
#endif

			if (damage && dam < 1.0)
				dam = 1.0;

			send_attack_msg(op, hitter, attacknum, (int) dam, damage);
			break;

		case ATNR_POISON:
			ATTACK_HIT_DAMAGE(hitter, attacknum);
			if (op->resist[attacknum])
				ATTACK_RESIST_DAMAGE(op, attacknum);
			ATTACK_PROTECT_DAMAGE(op, attacknum);

			/* i must adjust this... alive stand for mobs and player include
			 * golems, undead and demons - but not for doors or "technical objects"
			 * like earth walls. */
			/* if we had done any damage AND this is a ALIVE creature - poison it!
			 * We don't need to calc level here - the level is implicit calced
			 * in the damage! */
			if (damage && dam < 1.0)
				dam = 1.0;

			send_attack_msg(op, hitter, attacknum, (int) dam, damage);

			if (dam && IS_LIVE(op))
				poison_player(op, hitter, (float)dam);
			break;

		default:
			/*LOG(llevBug,"attack(): find unimplemented special attack: #%d obj:%s\n", attacknum, query_name(hitter));*/
			/* get % of dam from this attack form */
			ATTACK_HIT_DAMAGE(hitter, attacknum);

			/* reduce to % resistance */
			if (op->resist[attacknum])
				ATTACK_RESIST_DAMAGE(op, attacknum);

			/* reduce to % protection */
			ATTACK_PROTECT_DAMAGE(op, attacknum);

			if (damage && dam < 1.0)
				dam = 1.0;
			send_attack_msg(op, hitter, attacknum, (int) dam, damage);
			break;

		case ATNR_CONFUSION:
		case ATNR_SLOW:
		case ATNR_PARALYZE:
		case ATNR_FEAR:
		case ATNR_DEPLETE:
		case ATNR_BLIND:
		{
			int level_diff = MIN(110, MAX(0, op->level - hitter->level));

			if (op->speed && (QUERY_FLAG(op, FLAG_MONSTER) || op->type == PLAYER) && !(rndm(0, (attacknum == ATNR_SLOW ? 6 : 3) - 1)) && ((random_roll(1, 20, op, PREFER_LOW) + op->resist[attacknum] / 10) < savethrow[level_diff]))
			{
				if (attacknum == ATNR_CONFUSION)
				{
					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_ORANGE, 0, hitter, "You confuse %s!", op->name);
					}

					if (op->type == PLAYER)
					{
						new_draw_info_format(NDI_PURPLE, 0, op, "%s confused you!", hitter->name);
					}

					confuse_living(op, hitter, (int) dam);
				}
				else if (attacknum == ATNR_SLOW)
				{
					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_ORANGE, 0, hitter, "You slow %s!", op->name);
					}

					if (op->type == PLAYER)
					{
						new_draw_info_format(NDI_PURPLE, 0, op, "%s slowed you!", hitter->name);
					}

					slow_living(op, hitter, (int) dam);
				}
				else if (attacknum == ATNR_PARALYZE)
				{
					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_ORANGE, 0, hitter, "You paralyze %s!", op->name);
					}

					if (op->type == PLAYER)
					{
						new_draw_info_format(NDI_PURPLE, 0, op, "%s paralyzed you!", hitter->name);
					}

					paralyze_living(op, hitter, (int) dam);
				}
				else if (attacknum == ATNR_FEAR)
				{
					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_ORANGE, 0, hitter, "You scare %s!", op->name);
					}

					if (op->type == PLAYER)
					{
						new_draw_info_format(NDI_PURPLE, 0, op, "%s scared you!", hitter->name);
					}

					SET_FLAG(op, FLAG_SCARED);
				}
				else if (attacknum == ATNR_DEPLETE)
				{
					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_ORANGE, 0, hitter, "You deplete %s!", op->name);
					}

					if (op->type == PLAYER)
					{
						new_draw_info_format(NDI_PURPLE, 0, op, "%s depleted you!", hitter->name);
					}

					drain_stat(op);
				}
				else if (attacknum == ATNR_BLIND && !QUERY_FLAG(op, FLAG_UNDEAD))
				{
					if (hitter->type == PLAYER)
					{
						new_draw_info_format(NDI_ORANGE, 0, hitter, "You blind %s!", op->name);
					}

					if (op->type == PLAYER)
					{
						new_draw_info_format(NDI_PURPLE, 0, op, "%s blinded you!", hitter->name);
					}

					blind_living(op, hitter, (int) dam);
				}
			}

			dam = 0;
		}

		break;
	}

jump_show_dmg:
	return (int) dam;
}


/* we need this called spread in the function before because sometimes we want drop
 * a message BEFORE we tell the damage and sometimes we want a message after it. */
static void send_attack_msg(object *op, object *hitter, int attacknum, int dam, int damage)
{
	if (op->type == PLAYER)
		new_draw_info_format(NDI_PURPLE, 0, op, "%s hit you for %d (%d) damage.", hitter->name, (int)dam, ((int)dam) - damage);

	if (hitter->type == PLAYER)
		new_draw_info_format(NDI_ORANGE, 0, hitter, "You hit %s for %d (%d) %s.", op->name, (int)dam, ((int)dam) - damage, attacktype_desc[attacknum]);
}

/* OLD CODE FOR ATTACKS!
 * i let it in to show what and where we must browse to reimplement and/or
 * change it

    case ATNR_ACID:
      {
	int flag=0;

	if (!op_on_battleground(op, NULL, NULL) &&
	    (op->resist[ATNR_ACID] < 50))
	  {
	    object *tmp;
	    for(tmp=op->inv; tmp!=NULL; tmp=tmp->below) {
		if(!QUERY_FLAG(tmp, FLAG_APPLIED) ||
		   (tmp->resist[ATNR_ACID] >= 10))
		  continue;
		if(!(tmp->material & M_IRON))
		  continue;
		if(tmp->magic < -4)
		  continue;
		if(tmp->type==RING ||
		   tmp->type==GIRDLE ||
		   tmp->type==AMULET ||
		   tmp->type==WAND ||
		   tmp->type==ROD ||
		   tmp->type==HORN)
		  continue;

		if(rndm(0, (int)dam+4) >
		    random_roll(0, 39, op, PREFER_HIGH)+2*tmp->magic) {
		    if(op->type == PLAYER)
			new_draw_info_format(NDI_UNIQUE|NDI_RED,0, op,
				 "The %s's acid corrodes your %s!",
				 query_name(hitter), query_name(tmp));
		    flag = 1;
		    tmp->magic--;
		    if(op->type == PLAYER)
			esrv_send_item(op, tmp);
		}
	    }
	    if(flag)
	      fix_player(op);
	}
      }
      break;
    case ATNR_DRAIN:
      {
	int rate;

	if(op->resist[ATNR_DRAIN] > 0)
	  rate = 50 + op->resist[ATNR_DRAIN] / 2;
	else if(op->resist[ATNR_DRAIN] < 0)
	  rate = 5000 / (100 - op->resist[ATNR_DRAIN]);

	if(!rate)
	  return 0;

	if(op->stats.exp <= rate) {
	    if(op->type == GOLEM)
		dam = 999;
	    else
		dam = hit_player_attacktype(op, hitter, (int)dam, ATNR_PHYSICAL);
	} else {
	    if(hitter->stats.hp<hitter->stats.maxhp &&
	       (op->level > hitter->level) &&
	       random_roll(0, (op->level-hitter->level+2), hitter, PREFER_HIGH)>3)
	      hitter->stats.hp++;

	    if (!op_on_battleground(hitter, NULL, NULL)) {

        if(!QUERY_FLAG(op,FLAG_WAS_WIZ))
		    add_exp(hitter,op->stats.exp/(rate*2),hitter->chosen_skill->stats.sp);

        add_exp(op,-op->stats.exp/rate, CHOSEN_SKILL_NO);
	    }
	    dam = 0;
	}
      } break;
    case ATNR_TIME:
      {
	if (QUERY_FLAG(op,FLAG_UNDEAD)) {
	    object *owner = get_owner(hitter) == NULL ? hitter : get_owner(hitter);
            object *god = find_god (determine_god (owner));
            int div = 1;

            if (! god || ! god->slaying ||
		 strstr (god->slaying, undead_name) == NULL)
                div = 2;
	    if (op->level * div <
		(turn_bonus[owner->stats.Wis]+owner->level +
		 (op->resist[ATNR_TIME]/100)))
	      SET_FLAG(op, FLAG_SCARED);
	}
	else
	  dam = 0;
      } break;
    case ATNR_DEATH:
	break;
    case ATNR_CHAOS:
	LOG(llevBug, "BUG: %s was hit by %s with non-specific chaos.\n",
									query_name(op), query_name(hitter));
	dam = 0;
	break;
    case ATNR_COUNTERSPELL:
	LOG(llevBug, "BUG: %s was hit by %s with counterspell attack.\n",
									query_name(op),query_name(hitter));
	dam = 0;
	break;
    case ATNR_HOLYWORD:
      {

	object *owner = get_owner(hitter)==NULL?hitter:get_owner(hitter);

	if((op->level+(op->resist[ATNR_HOLYWORD]/100)) <
	   owner->level+turn_bonus[owner->stats.Wis])
	  SET_FLAG(op, FLAG_SCARED);
      } break;
    case ATNR_LIFE_STEALING:
      {
	int new_hp;
	if ((op->type == GOLEM) || (QUERY_FLAG(op, FLAG_UNDEAD))) return 0;
	if (op->resist[ATNR_DRAIN] >= op->resist[ATNR_LIFE_STEALING])
	  dam = (dam*(100 - op->resist[ATNR_DRAIN])) / 3000;
	else dam = (dam*(100 - op->resist[ATNR_LIFE_STEALING])) / 3000;
	if (dam > (op->stats.hp+1)) dam = op->stats.hp+1;
	new_hp = hitter->stats.hp + (int)dam;
	if (new_hp > hitter->stats.maxhp) new_hp = hitter->stats.maxhp;
	if (new_hp > hitter->stats.hp) hitter->stats.hp = new_hp;
      }

*/

/* GROS: This code comes from hit_player. It has been made external to
 * allow script procedures to "kill" objects in a combat-like fashion.
 * It was initially used by (kill-object) developed for the Collector's
 * Sword. Note that nothing has been changed from the original version
 * of the following code. */

/* ok, when i have finished the different attacks i must clean this up here too
 * looks like some artifact code in here - MT-2003 */
int kill_object(object *op,int dam, object *hitter, int type)
{
	char buf[MAX_BUF];
	/* this is used in case of servant monsters */
	object *old_hitter = NULL;
	int maxdam = 0;
	int exp = 0;
	/* true if op standing on battleground */
	int battleg = 0;
	object *owner = NULL;
	mapstruct *map;

	(void) dam;

	if (op->stats.hp >= 0)
		return -1;

	/* Trigger the DEATH event */
	if (trigger_event(EVENT_DEATH, hitter, op, NULL, NULL, &type, 0, 0, SCRIPT_FIX_ALL))
	{
		return 0;
	}

	/* Trigger the global GKILL event */
	trigger_global_event(EVENT_GKILL, hitter, op);

	maxdam = op->stats.hp - 1;

	if (op->type == DOOR)
	{
		op->speed = 0.1f;
		update_ob_speed(op);
		op->speed_left = -0.05f;
		return maxdam;
	}

	/* Show Damage System for clients */
	/* whatever is dead now, we check map. If it on map, we redirect last_damage
	to map space, giving player the chance to see the last hit damage they had
	done. If there is more as one object killed on a single map tile, we overwrite
	it now. This visual effect works pretty good. MT */

	/* no pet/player/monster checking now, perhaps not needed */
	/* only when some damage is stored */
	if (op->damage_round_tag == ROUND_TAG)
	{
		/* is on map */
		/* hm, can we sure we are on a legal map position... hope so*/
		if ((map = op->map))
		{
			SET_MAP_DAMAGE(op->map, op->x, op->y, op->last_damage);
			SET_MAP_RTAG(op->map, op->x, op->y, ROUND_TAG);
		}
	}

	if (op->map)
		play_sound_map(op->map, op->x, op->y, SOUND_PLAYER_KILLS, SOUND_NORMAL);

	/* Now lets start dealing with experience we get for killing something */
	owner = get_owner(hitter);
	if (owner == NULL)
		owner = hitter;

	/* is the victim (op) standing on battleground? */
	if (pvp_area(NULL, op))
		battleg = 1;

	/* Player killed something */
	if (owner->type == PLAYER)
	{
		if (owner != hitter)
		{
			(void) sprintf(buf, "You killed %s with %s.", query_name(op, NULL), query_name(hitter, NULL));
			old_hitter = hitter;
			owner->exp_obj = hitter->exp_obj;
		}
		else
			(void) sprintf(buf, "You killed %s.", query_name(op, NULL));

		/* message should be displayed */
		new_draw_info(NDI_WHITE, 0, owner, buf);
	}
	/* was a player that hit this creature */

	/* Pet killed something. */
	if (get_owner(hitter) != NULL)
	{
		(void) sprintf(buf, "%s killed %s with %s%s.", hitter->owner->name, query_name(op, NULL), query_name(hitter, NULL), battleg ? " (duel)" : "");
		old_hitter = hitter;
		owner->exp_obj = hitter->exp_obj;
		hitter = hitter->owner;
	}
	else
		(void) sprintf(buf, "%s killed %s%s.", hitter->name, op->name, battleg ? " (duel)" : "");

	/* If you didn't kill yourself, and you're not the wizard */
	if (hitter != op && !QUERY_FLAG(op, FLAG_WIZ))
	{
		/* new exp system in here. Try to insure the right skill is modifying gained exp */
		/* only calc exp for a player who has not killed a player */
		if (hitter->type == PLAYER && !old_hitter && op->type != PLAYER)
			exp = calc_skill_exp(hitter, op);

		/* case for attack spells, summoned monsters killing */
		if (old_hitter && hitter->type == PLAYER)
		{
			object *old_skill = hitter->chosen_skill;

			hitter->chosen_skill = old_hitter->chosen_skill;

			if (hitter->type==PLAYER && op->type != PLAYER)
				exp = calc_skill_exp(hitter, op);

			hitter->chosen_skill = old_skill;
		}

		/* here is the skill fix:
		 * We REALLY want assign to our owner (who is the hitter or owner of hitter)
		 * the right skill. This is set from set_owner() for spells and all objects
		 * which does indirect (not from owner object) damage. */

		/* when != NULL, it is our non owner object (spell, arrow) */
		if (!old_hitter)
			old_hitter = hitter;

		/* Really don't give much experience for killing other players */
		if (op->type == PLAYER && owner->type == PLAYER)
		{
			if (battleg)
			{
				new_draw_info(NDI_UNIQUE, 0, owner, "Your foe has fallen!");
				new_draw_info(NDI_UNIQUE, 0, owner, "VICTORY!!!");
			}
			/* never xp for pvp */
			else
				exp = 0;
		}

		/* if op is standing on pvp map, no way to gain
		 * exp by killing him */
		if (battleg)
			exp = 0;

		if (hitter->type == PLAYER && !battleg)
		{
			/* If this player is not in party, it's simple. */
			if (CONTR(hitter)->party_number <= 0)
			{
				/* only player gets exp - when we have exp */
				if (exp)
					new_draw_info_format(NDI_UNIQUE, 0, hitter, "You got %d exp in skill %s.", add_exp(hitter, exp, old_hitter->chosen_skill->stats.sp), skills[old_hitter->chosen_skill->stats.sp].name);
				else
					new_draw_info(NDI_UNIQUE, 0, hitter, "Your enemy was too low for exp.");
			}
			/* However, if we are in a party, things get a little more tricky. */
			else
			{
				int shares = 0, count = 0;
				player *pl;
				int no = CONTR(hitter)->party_number;
#ifdef PARTY_KILL_LOG
				add_kill_to_party(no, query_name(hitter), query_name(op), exp);
#endif

				for (pl = first_player; pl != NULL; pl = pl->next)
				{
					if (CONTR(pl->ob)->party_number == no && pl->skill_ptr[old_hitter->chosen_skill->stats.sp] && on_same_map(pl->ob, hitter))
					{
						count++;
						shares += (pl->ob->level + 4);
					}
				}

				if (count == 1 || shares > exp)
				{
					if (exp)
						new_draw_info_format(NDI_UNIQUE, 0, hitter, "You got %d exp in skill %s.", add_exp(hitter, exp, old_hitter->chosen_skill->stats.sp), skills[old_hitter->chosen_skill->stats.sp].name);
					else
						new_draw_info(NDI_UNIQUE, 0, hitter, "Your enemy was too low for exp.");
				}
				else
				{
					int share = exp / shares, given = 0, nexp;
					for (pl = first_player; pl != NULL; pl = pl->next)
					{
						if (CONTR(pl->ob)->party_number == no && on_same_map(pl->ob, hitter))
						{
							if (exp)
							{
								nexp = (pl->ob->level + 4) * share;
								new_draw_info_format(NDI_UNIQUE, 0, pl->ob, "You got %d exp in skill %s.", add_exp(pl->ob, nexp, old_hitter->chosen_skill->stats.sp), skills[old_hitter->chosen_skill->stats.sp].name);
								given += nexp;
							}
							else
								new_draw_info(NDI_UNIQUE, 0, pl->ob, "Your enemy was too low for exp.");
						}
					}
				}
			}
		}

		if (op->type != PLAYER)
		{
			/*new_draw_info(NDI_ALL, 10, NULL, buf);*/
			if (QUERY_FLAG(op, FLAG_FRIENDLY))
			{
				object *owner = get_owner(op);

				if (owner != NULL && owner->type == PLAYER)
				{
					sprintf(buf, "Your pet, the %s, is killed by %s.", op->name, hitter->name);
					play_sound_player_only(CONTR(owner), SOUND_PET_IS_KILLED, SOUND_NORMAL, 0, 0);
					new_draw_info(NDI_UNIQUE, 0, owner, buf);
				}

				remove_friendly_object(op);
			}
			op->speed = 0;
			/* remove from active list (if on) */
			update_ob_speed(op);

			/* rules:
			 * a.) mob will drop corpse for his target, not for kill hit giving player.
			 * b.) npc kill hit WILL overwrite player target = on drop
			 * c.) we are nice: kill hit will count if target was a npc (of mob).
			 * will allow a bit "cheating" by serving only one hit and let kill the mob
			 * by the npc to 99% - but this needs brain, tactic and a good timing and
			 * so we will give him a present for it. */
			if (owner->type != PLAYER || !op->enemy || op->enemy->type != PLAYER)
			{
				/* no set_npc_enemy since we are killing it... */
				op->enemy = owner;
				op->enemy_count = owner->count;
			}

			/* harder drop rules: if exp == 0 or not a player or not a player invoked hitter: no drop */
			if (!exp || exp == 0 || hitter->type != PLAYER || (get_owner(hitter) && hitter->owner->type != PLAYER))
				SET_FLAG(op, FLAG_STARTEQUIP);

			destruct_ob(op);
		}
		/* Player has been killed! */
		else
		{
			new_draw_info(NDI_ALL, 1, NULL, buf);
			if (hitter->type == PLAYER)
			{
				sprintf(buf, "%s the %s", hitter->name, hitter->race);
				strncpy(CONTR(op)->killer, buf, BIG_NAME);
			}
			else
			{
				strncpy(CONTR(op)->killer, hitter->name, BIG_NAME);
				CONTR(op)->killer[BIG_NAME - 1] = '\0';
			}
		}
	}

	return maxdam;
}

static int get_attack_mode(object **target, object **hitter, int *simple_attack)
{
	if (OBJECT_FREE(*target) || OBJECT_FREE(*hitter))
	{
		LOG(llevBug, "BUG: get_attack_mode(): freed object\n");
		return 1;
	}
	if ((*target)->head)
		*target = (*target)->head;
	if ((*hitter)->head)
		*hitter = (*hitter)->head;
	if ((*hitter)->env != NULL || (*target)->env != NULL)
	{
		*simple_attack = 1;
		return 0;
	}
	if (QUERY_FLAG(*target, FLAG_REMOVED) || QUERY_FLAG(*hitter, FLAG_REMOVED) || (*hitter)->map == NULL || !on_same_map((*hitter), (*target)))
	{
		LOG(llevBug, "BUG: hitter (arch %s, name %s) with no relation to target\n", (*hitter)->arch->name, query_name(*hitter, NULL));
		return 1;
	}
	*simple_attack = 0;
	return 0;
}

static int abort_attack(object *target, object *hitter, int simple_attack)
{
	/* Check if target and hitter are still in a relation similar to the one
	 * determined by get_attack_mode().  Returns true if the relation has changed. */
	int new_mode;

	if (hitter->env == target || target->env == hitter)
		new_mode = 1;
	else if (QUERY_FLAG(target, FLAG_REMOVED) || QUERY_FLAG(hitter, FLAG_REMOVED) || hitter->map == NULL || !on_same_map(hitter, target))
		return 1;
	else
		new_mode = 0;

	return new_mode != simple_attack;
}

/* op is the arrow, tmp is what is stopping the arrow.
 *
 * Returns 1 if op was inserted into tmp's inventory, 0 otherwise.
 */
#if 0
static int stick_arrow (object *op, object *tmp)
{
	/* If the missile hit a player, we insert it in their inventory.
	 * However, if the missile is heavy, we don't do so (assume it falls
	 * to the ground after a hit).  What a good value for this is up to
	 * debate - 5000 is 5 kg, so arrows, knives, and other light weapons
	 * stick around. */
	if (op->weight <= 5000 && tmp->stats.hp >= 0)
	{
		if (tmp->head != NULL)
			tmp = tmp->head;

		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		op = insert_ob_in_ob(op, tmp);

		if (tmp->type == PLAYER)
			esrv_send_item(tmp, op);

#ifdef PLUGINS
		/* GROS: Handle for plugin stop event */
		if (op->event_flags&EVENT_FLAG_STOP)
		{
			CFParm CFP;
			int k, l, m;
			object *event_obj = get_event_object(op, EVENT_STOP);
			k = EVENT_STOP;
			l = SCRIPT_FIX_NOTHING;
			m = 0;
			CFP.Value[0] = &k;
			CFP.Value[1] = tmp; /* Activator = whatever we hit */
			CFP.Value[2] = op;
			CFP.Value[3] = NULL;
			CFP.Value[4] = NULL;
			CFP.Value[5] = &m;
			CFP.Value[6] = &m;
			CFP.Value[7] = &m;
			CFP.Value[8] = &l;
			CFP.Value[9] = (char *)event_obj->race;
			CFP.Value[10] = (char *)event_obj->slaying;
			if (findPlugin(event_obj->name) >= 0)
				((PlugList[findPlugin(event_obj->name)].eventfunc) (&CFP));
		}
#endif

		return 1;
	}
	else
		return 0;
}
#endif

/* hit_with_arrow() disassembles the missile, attacks the victim and
 * reassembles the missile.
 *
 * It returns a pointer to the reassembled missile, or NULL if the missile
 * isn't available anymore.
 */
object *hit_with_arrow(object *op, object *victim)
{
	object *container, *hitter;
	int hit_something = 0;
	tag_t victim_tag, hitter_tag;
	sint16 victim_x, victim_y;

	/* Disassemble missile */
	if (op->inv)
	{
		container = op;
		hitter = op->inv;
		remove_ob(hitter);
		insert_ob_in_map(hitter, container->map, hitter, INS_NO_MERGE | INS_NO_WALK_ON);
		/* Note that we now have an empty THROWN_OBJ on the map.  Code that
		 * might be called until this THROWN_OBJ is either reassembled or
		 * removed at the end of this function must be able to deal with empty
		 * THROWN_OBJs. */
	}
	else
	{
		container = NULL;
		hitter = op;
	}

	/* Try to hit victim */
	victim_x = victim->x;
	victim_y = victim->y;
	victim_tag = victim->count;
	hitter_tag = hitter->count;

	if (hitter->event_flags & EVENT_FLAG_ATTACK)
	{
		/* Trigger the ATTACK event */
		trigger_event(EVENT_ATTACK, hitter, hitter, victim, NULL, 0, (int *) &(op->stats.dam), (int *) &(op->stats.wc), SCRIPT_FIX_ALL);
	}
	else
	{
		hit_something = attack_ob_simple(victim, hitter, op->stats.dam, op->stats.wc);
	}

	/*LOG(-1, "hit: %s (%d %d)\n", hitter->name, op->stats.dam, op->stats.wc);*/

	/* Arrow attacks door, rune of summoning is triggered, demon is put on
	 * arrow, move_apply() calls this function, arrow sticks in demon,
	 * attack_ob_simple() returns, and we've got an arrow that still exists
	 * but is no longer on the map. Ugh. (Beware: Such things can happen at
	 * other places as well!) */
	/* hopefully the walk_off event was triggered somewhere there */
	if (was_destroyed (hitter, hitter_tag) || hitter->env != NULL)
	{
		if (container)
		{
			remove_ob(container);
			check_walk_off (container, NULL, MOVE_APPLY_VANISHED);
		}
		return NULL;
	}

	/* Missile hit victim */
	if (hit_something)
	{
		/* Stop arrow */
		if (container == NULL)
		{
			hitter = fix_stopped_arrow(hitter);
			if (hitter == NULL)
				return NULL;
		}
		else
		{
			remove_ob(container);
			check_walk_off(container, NULL, MOVE_APPLY_VANISHED);
		}
		/* Try to stick arrow into victim */
		/* disabled - this will not work very well now with
		 * the loot system of corpses.. if several people shot
		 * at same guys.
		 */
		/*
		if (!was_destroyed(victim, victim_tag) && stick_arrow(hitter, victim))
		    return NULL;
		*/

		/* Trigger the STOP event */
		trigger_event(EVENT_STOP, victim, hitter, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING);

		/* Else try to put arrow on victim's map square */
		if ((victim_x != hitter->x || victim_y != hitter->y) && !wall(hitter->map, victim_x, victim_y))
		{
			remove_ob(hitter);
			if (check_walk_off(hitter, NULL, MOVE_APPLY_DEFAULT) == CHECK_WALK_OK)
			{
				hitter->x = victim_x;
				hitter->y = victim_y;
				insert_ob_in_map (hitter, victim->map, hitter, 0);
			}
		}
		/* Else leave arrow where it is */
		else
			hitter = merge_ob(hitter, NULL);

		return NULL;
	}

	/* Missile missed victim - reassemble missile */
	if (container)
	{
		/* technical remove, no walk check */
		remove_ob(hitter);
		insert_ob_in_ob(hitter, container);
	}
	return op;
}


void tear_down_wall(object *op)
{
	int perc = 0;

	if (!op->stats.maxhp)
	{
		LOG(llevBug, "BUG: TEAR_DOWN wall %s had no maxhp.\n", op->name);
		perc = 1;
	}
	else if (!GET_ANIM_ID(op))
	{
		/* Object has been called - no animations, so remove it */
		if (op->stats.hp < 0)
			destruct_ob(op); /* Should update LOS */
		/* no animations, so nothing more to do */
		return;
	}

	perc = NUM_ANIMATIONS(op) - ((int)NUM_ANIMATIONS(op) * op->stats.hp) / op->stats.maxhp;

	if (perc >= (int) NUM_ANIMATIONS(op))
		perc = NUM_ANIMATIONS(op)-1;
	else if (perc < 1)
		perc = 1;

	SET_ANIMATION(op, perc);
	update_object(op, UP_OBJ_FACE);
	/* Reached the last animation */
	if (perc == NUM_ANIMATIONS(op) - 1)
	{
		/* If the last face is blank, remove the ob */
		if (op->face == blank_face)
			/* Should update LOS */
			destruct_ob(op);
		/* The last face was not blank, leave an image */
		else
		{
			CLEAR_FLAG(op, FLAG_BLOCKSVIEW);
			CLEAR_FLAG(op, FLAG_NO_PASS);
			CLEAR_FLAG(op, FLAG_ALIVE);
		}
	}
}


/* TODO: i have not the time now - but later we should do this: a marker value
 * in the poison force flags kind of poison - food/poison force or weapon/spell
 * poison. Weapon/spell force will block now poisoning, but will not be blocked
 * by food/poison forces - and food/poison forces can stack. MT-2003
 * NOTE: only poison now insert a poison force - food&drink insert different forces.
 * For Poison objects, we use simply the base dam * level */
void poison_player(object *op, object *hitter, float dam)
{
	float pmul;
	archetype *at = find_archetype("poisoning");
	object *tmp = present_arch_in_ob(at,op);

	/* this is to avoid stacking poison forces... Like we get hit 10 times
	 * by a spell and sword and get 10 poison force in us
	 * The bad point is, that we can cheat here... Lets say we poison us self
	 * with a mild food poison and then we battle the god of poison - he can't
	 * hurt us with poison until the food poison wears out */

	/* we only poison players and mobs! */
	if (op->type!= PLAYER && !QUERY_FLAG(op, FLAG_MONSTER))
		return;

	if (tmp == NULL || hitter->type == POISON)
	{
		if ((tmp = arch_to_object(at)) == NULL)
		{
			LOG(llevBug, "BUG: Failed to clone arch poisoning.\n");
			return;
		}
		else
		{
			if (hitter->type == POISON)
			{
				dam /= 2.0f;
				tmp->stats.dam = (int) (((dam + RANDOM() % ((int)dam + 1)) * LEVEL_DAMAGE(hitter->level)) * 0.9f);
				if (tmp->stats.dam > op->stats.maxhp / 3)
					tmp->stats.dam = op->stats.maxhp / 3;
				if (tmp->stats.dam < 1)
					tmp->stats.dam = 1;
			}
			/* spell or weapon will be handled different! */
			else
			{
				dam /= 2.0f;
				tmp->stats.dam = (int)((int)dam + RANDOM() % (int)(dam + 1));
				if (tmp->stats.dam > op->stats.maxhp / 3)
					tmp->stats.dam = op->stats.maxhp / 3;
				if (tmp->stats.dam < 1)
					tmp->stats.dam = 1;
			}

			tmp->level = hitter->level;
			/* so we get credit for poisoning kills */
			copy_owner(tmp, hitter);

			/* now we adjust numbers of ticks of the DOT force and speed of DOT ticks */
			if (hitter->type == POISON)
			{
				/* # of ticks */
				tmp->stats.food = hitter->last_heal;
				/* speed of ticks */
				tmp->speed = tmp->speed_left;
			}

			if (op->type == PLAYER)
			{
				/* spells should add here too later */
				/* here we handle consuming poison */
				if (hitter->type == POISON)
				{
					/* this insert the food force in player too */
					create_food_force(op, hitter, tmp);
					new_draw_info(NDI_UNIQUE, 0, op, "You suddenly feel very ill.");
				}
				/* and here we have hit with weapon or something */
				else
				{

					if (op->stats.Con > 1 && !(RANDOM() % 2))
					{
						pmul = (hitter->level + (RANDOM() % (hitter->level / 4 + 1)) / 2) * 0.01f + 0.2f;
						tmp->stats.Con = 1 + (sint8)(pmul * (op->stats.Con - 1));
						if (tmp->stats.Con >= op->stats.Con)
							tmp->stats.Con = op->stats.Con - 1;
						tmp->stats.Con *= -1;

					}

					if (op->stats.Str > 1 && !(RANDOM() % 2))
					{
						pmul = (hitter->level + (RANDOM() % (hitter->level / 4 + 1)) / 2) * 0.01f + 0.2f;
						tmp->stats.Str = 1 + (sint8)(pmul * (op->stats.Str - 1));
						if (tmp->stats.Str >= op->stats.Str)
							tmp->stats.Str = op->stats.Str - 1;
						tmp->stats.Str *=- 1;
					}

					if (op->stats.Dex > 1 && !(RANDOM() % 2))
					{
						pmul = (hitter->level + (RANDOM() % (hitter->level / 4 + 1)) / 2) * 0.01f + 0.2f;
						tmp->stats.Dex = 1 + (sint8)(pmul * (op->stats.Dex - 1));
						if (tmp->stats.Dex >= op->stats.Dex)
							tmp->stats.Dex = op->stats.Dex - 1;
						tmp->stats.Dex *= -1;
					}

					if (op->stats.Int > 1 && !(RANDOM() % 3))
					{
						pmul = (hitter->level + (RANDOM() % (hitter->level / 4 + 1)) / 2) * 0.01f + 0.2f;
						tmp->stats.Int = 1 + (sint8)(pmul * (op->stats.Int - 1));
						if (tmp->stats.Int >= op->stats.Int)
							tmp->stats.Int = op->stats.Int - 1;
						tmp->stats.Int *= -1;
					}

					if (op->stats.Cha > 1 && !(RANDOM() % 3))
					{
						pmul = (hitter->level + (RANDOM() % (hitter->level / 4 + 1)) / 2) * 0.01f + 0.2f;
						tmp->stats.Cha = 1 + (sint8)(pmul * (op->stats.Cha - 1));
						if (tmp->stats.Cha >= op->stats.Cha)
							tmp->stats.Cha = op->stats.Cha - 1;
						tmp->stats.Cha *= -1;
					}

					if (op->stats.Pow > 1 && !(RANDOM() % 4))
					{
						pmul = (hitter->level + (RANDOM() % (hitter->level / 4 + 1)) / 2) * 0.01f + 0.2f;
						tmp->stats.Pow = 1 + (sint8)(pmul * (op->stats.Pow - 1));
						if (tmp->stats.Pow >= op->stats.Pow)
							tmp->stats.Pow = op->stats.Pow - 1;
						tmp->stats.Pow *= -1;
					}

					if (op->stats.Wis > 1 && !(RANDOM() % 4))
					{
						pmul = (hitter->level + (RANDOM() % (hitter->level / 4 + 1)) / 2) * 0.01f + 0.2f;
						tmp->stats.Wis = 1 + (sint8)(pmul * (op->stats.Wis - 1));
						if (tmp->stats.Wis >= op->stats.Wis)
							tmp->stats.Wis = op->stats.Wis - 1;
						tmp->stats.Wis *= -1;
					}

					new_draw_info_format(NDI_UNIQUE, 0, op, "%s has poisoned you!", query_name(hitter, NULL));
					insert_ob_in_ob(tmp, op);
					SET_FLAG(tmp, FLAG_APPLIED);
					fix_player(op);
				}
			}
			/* its a mob! */
			else
			{
				/* mob eats poison.. */
				if (hitter->type == POISON)
				{
					/* TODO */
				}
				/* is hit from poison force! */
				else
				{
					insert_ob_in_ob(tmp, op);
					SET_FLAG(tmp, FLAG_APPLIED);
					fix_monster(op);
					if (hitter->type == PLAYER)
						new_draw_info_format(NDI_UNIQUE, 0, hitter, "You poisoned %s!", query_name(op, NULL));
					else if (get_owner(hitter) && hitter->owner->type == PLAYER)
						new_draw_info_format(NDI_UNIQUE, 0, hitter->owner, "%s poisoned %s!", query_name(hitter, NULL), query_name(op, NULL));

				}
			}
		}
		tmp->speed_left = 0;
	}
	else
		tmp->stats.food++;
}

void slow_living(object *op, object *hitter, int dam)
{
	archetype *at = find_archetype("slowness");
	object *tmp;

	(void) hitter;
	(void) dam;

	if (at == NULL)
		LOG(llevBug, "BUG: Can't find slowness archetype.\n");

	if ((tmp = present_arch_in_ob(at, op)) == NULL)
	{
		tmp = arch_to_object(at);
		tmp = insert_ob_in_ob(tmp, op);
		new_draw_info(NDI_UNIQUE, 0, op, "The world suddenly moves very fast!");
	}
	else
		tmp->stats.food++;

	SET_FLAG(tmp, FLAG_APPLIED);
	tmp->speed_left = 0;
	fix_player(op);
}

void confuse_living(object *op, object *hitter, int dam)
{
	object *tmp;
	int maxduration;

	(void) hitter;
	(void) dam;

	tmp = present_in_ob(CONFUSION, op);
	if (!tmp)
	{
		tmp = get_archetype("confusion");
		tmp = insert_ob_in_ob(tmp, op);
	}

	/* Duration added per hit and max. duration of confusion both depend on the player's resistance */
	tmp->stats.food += MAX(1, 5 * (100 - op->resist[ATNR_CONFUSION]) / 100);
	maxduration = MAX(2, 30 * (100 - op->resist[ATNR_CONFUSION]) / 100);

	if (tmp->stats.food > maxduration)
		tmp->stats.food = maxduration;

	if (op->type == PLAYER && !QUERY_FLAG(op, FLAG_CONFUSED))
		new_draw_info(NDI_UNIQUE, 0, op, "You suddenly feel very confused!");

	SET_FLAG(op, FLAG_CONFUSED);
}

void blind_living(object *op, object *hitter, int dam)
{
	object *tmp, *owner;

	/* Save some work if we know it isn't going to affect the player */
	if (op->resist[ATNR_BLIND] == 100)
		return;

	tmp = present_in_ob(BLINDNESS, op);
	if (!tmp)
	{
		tmp = get_archetype("blindness");
		SET_FLAG(tmp, FLAG_BLIND);
		SET_FLAG(tmp, FLAG_APPLIED);
		/* use floats so we don't lose too much precision due to rounding errors.
		 * speed is a float anyways. */
		tmp->speed = tmp->speed * ((float)100.0 - (float)op->resist[ATNR_BLIND]) / (float)100;

		tmp = insert_ob_in_ob(tmp, op);
		/* Mostly to display any messages */
		change_abil(op, tmp);
		/* This takes care of some other stuff */
		fix_player(op);

		if (hitter->owner)
			owner = get_owner(hitter);
		else
			owner = hitter;

		new_draw_info_format(NDI_UNIQUE, 0, owner, "Your attack blinds %s!", query_name(op, NULL));
	}
	tmp->stats.food += dam;

	if (tmp->stats.food > 10)
		tmp->stats.food = 10;
}

void paralyze_living(object *op, object *hitter, int dam)
{
	float effect, max;
	/* object *tmp; */

	(void) hitter;

	/* Do this as a float - otherwise, rounding might very well reduce this to 0 */
	effect = (float)dam * (float)3.0 * ((float)100.0 - (float)op->resist[ATNR_PARALYZE]) / (float)100;

	if (effect == 0)
		return;

	/* we mark this object as paralyzed */
	SET_FLAG(op, FLAG_PARALYZED);

	op->speed_left -= FABS(op->speed) * effect;
	/* tmp->stats.food+=(signed short) effect / op->speed; */

	/* max number of ticks to be affected for. */
	max = ((float)100 - (float)op->resist[ATNR_PARALYZE]) / (float) 2;
	if (op->speed_left < -(FABS(op->speed) * max))
		op->speed_left = (float) -(FABS(op->speed) * max);

	/* tmp->stats.food = (signed short) (max / FABS(op->speed)); */
}


/* Attempts to kill 'op'. hitter is the attack object, dam i
 * the computed damaged. */
void deathstrike_player(object *op, object *hitter, int *dam)
{
	/*  The intention of a death attack is to kill outright things
	**  that are a lot weaker than the attacker, have a chance of killing
	**  things somewhat weaker than the caster, and no chance of
	**  killing something equal or stronger than the attacker.
	**  Also, if a deathstrike attack has a slaying, any monster
	**  whose name or race matches a comma-delimited list in the slaying
	**  field of the deathstriking object  */

	int atk_lev, def_lev, kill_lev;

	if (hitter->slaying)
		if (!((QUERY_FLAG(op, FLAG_UNDEAD) && strstr(hitter->slaying, undead_name)) || (op->race && strstr(hitter->slaying, op->race))))
			return;

	def_lev = op->level;
	if (def_lev < 1)
	{
		LOG(llevBug, "BUG: arch %s, name %s with level < 1\n", op->arch->name, query_name(op, NULL));
		def_lev = 1;
	}
	atk_lev = SK_level (hitter) / 2;
	/* LOG(llevDebug,"Deathstrike - attack level %d, defender level %d\n", atk_lev, def_lev); */

	if (atk_lev >= def_lev)
	{
		kill_lev = random_roll(0, atk_lev-1, hitter, PREFER_HIGH);

		/* Note that the below effectively means the ratio of the atk vs
		 * defener level is important - if level 52 character has very little
		 * chance of killing a level 50 monster.  This should probably be
		 * redone. */
		if (kill_lev >= def_lev)
		{
			*dam = op->stats.hp + 10;
			/* take all hp. they can still save for 1/2 */
			/* I think this doesn't really do much.  Because of
			 * integer rounding, this only makes any difference if the
			 * attack level is double the defender level. */
			*dam *= kill_lev / def_lev;
		}
	}
	/* no harm done */
	else
		*dam = 0;
}

/* thrown_item_effect() - handles any special effects of thrown
 * items (like attacking living creatures--a potion thrown at a
 * monster). */
static void thrown_item_effect(object *hitter, object *victim)
{
	if (!IS_LIVE(hitter))
	{
		/* May not need a switch for just 2 types, but this makes it
		 * easier for expansion. */
		/* i removed a resist check here - we handle resist checks BEFORE this (skill "avoid get hit" or whatever)
		 * OR after this (resist against the damage this here perhaps does) */
		switch (hitter->type)
		{
			case POTION:
				/* OLD: should player get a save throw instead of checking magic protection? */
				/*if(QUERY_FLAG(victim,FLAG_ALIVE)&&!QUERY_FLAG(victim,FLAG_UNDEAD))
					apply_potion(victim,hitter);
				*/
				/* ok, we do something new here:
				 * a potion has hit a object.
				 * at this stage, we only explode area effects.
				 * ALL other potion will take here no effect.
				 * later we should take care about confusion & paralyze.
				 * we should NEVER allow cure/heal stuff here - it will
				 * allow too many possible exploits. */
				if (hitter->stats.sp != SP_NO_SPELL && spells[hitter->stats.sp].flags&SPELL_DESC_DIRECTION)
					/* apply potion ALWAYS fire on the spot the applier stands - good for healing - bad for firestorm */
					cast_spell(hitter, hitter, hitter->direction, hitter->stats.sp, 1, spellPotion, NULL);

				decrease_ob(hitter);
				break;

				/* poison drinks */
			case POISON:
				/* As with potions, should monster get a save? */
				if (IS_LIVE(victim) && !QUERY_FLAG(victim, FLAG_UNDEAD))
					apply_poison(victim, hitter);
				break;

				/* Removed case statements that did nothing.
				 * food may be poisonous, but monster must be willing to eat it,
				 * so we don't handle it here.
				 * Containers should perhaps break open, but that code was disabled. */
		}
	}
}

/* adj_attackroll() - adjustments to attacks by various conditions */
int adj_attackroll(object *hitter, object *target)
{
	object *attacker = hitter;
	int adjust = 0;

	/* safety */
	if (!target || !hitter || !hitter->map || !target->map || !on_same_map(hitter, target))
	{
		LOG(llevBug, "BUG: adj_attackroll(): hitter and target not on same map\n");
		return 0;
	}

	/* aimed missiles use the owning object's sight */
	if (is_aimed_missile(hitter))
	{
		if ((attacker = get_owner(hitter)) == NULL)
			attacker = hitter;
	}
	else if (!IS_LIVE(hitter))
		return 0;

	/* determine the condtions under which we make an attack.
	 * Add more cases, as the need occurs. */

	/* is invisible means, we can't see it - same for blind */
	if (IS_INVISIBLE(target, attacker) || QUERY_FLAG(attacker, FLAG_BLIND))
		adjust -= 12;

	if (QUERY_FLAG(attacker, FLAG_SCARED))
		adjust -= 3;

	if (QUERY_FLAG(target, FLAG_UNAGGRESSIVE))
		adjust += 1;

	if (QUERY_FLAG(target, FLAG_SCARED))
		adjust += 1;

	if (QUERY_FLAG(attacker, FLAG_CONFUSED))
		adjust -= 3;

	/* if we attack at a different 'altitude' its harder */
	if (QUERY_FLAG(attacker, FLAG_FLYING) != QUERY_FLAG(target, FLAG_FLYING))
		adjust -= 2;

#if 0
	/* slower attacks are less likely to succeed. We should use a
	 * comparison between attacker/target speeds BUT, players have
	 * a generally faster speed, so this will wind up being a HUGE
	 * disadantage for the monsters! Too bad, because missiles which
	 * fly fast should have a better chance of hitting a slower target.
	 */
	if (hitter->speed < target->speed)
		adjust += ((float) hitter->speed-target->speed);
#endif

#if 0
	LOG(llevDebug, "adj_attackroll() returns %d (%d)\n", adjust, attacker->count);
#endif

	return adjust;
}


/* determine if the object is an 'aimed' missile */
int is_aimed_missile(object *op)
{
	if (op && QUERY_FLAG(op, FLAG_FLYING) &&  (op->type == ARROW || op->type == THROWN_OBJ || op->type == FBULLET || op->type == FBALL))
		return 1;

	return 0;
}

/* improved melee test function.
 * test for objects are in range for melee attack.
 * used from attack() functions but also from animation().
 * Return:
 * 0: enemy target is not in melee range.
 * 1: target is in range and we face it.
 * TODO: 2: target is range but not in front.
 * TODO: 3: target is in back
 * NOTE: check for valid target outside here.
 */
int is_melee_range(object *hitter, object *enemy)
{
	int xt, yt, s;
	object *tmp;
	mapstruct *mt;

	/* check squares around AND our own position */
	for (s = 0; s < 9; s++)
	{
		xt = hitter->x + freearr_x[s];
		yt = hitter->y + freearr_y[s];

		if (!(mt = out_of_map(hitter->map, &xt, &yt)))
			continue;

		for (tmp = enemy; tmp != NULL; tmp = tmp->more)
		{
			/* strike! */
			if (tmp->map == mt && tmp->x == xt && tmp->y == yt)
				return 1;
		}
	}

	return 0;
}

/* did_make_save_item just checks to make sure the item actually
 * made its saving throw based on the tables.  It does not take
 * any further action (like destroying the item). */
int did_make_save_item(object *op, object *originator)
{
	int i, saves = 0, materials = 0, number;

	if (originator->attack[ATNR_ELECTRICITY] > 0)
		number = ATNR_ELECTRICITY;
	else if (originator->attack[ATNR_FIRE] > 0)
		number = ATNR_FIRE;
	else if (originator->attack[ATNR_COLD] > 0)
		number = ATNR_COLD;
	else
		return 1;

	/* If the object is immune, no effect */
	if (op->resist[number] == 100)
		return 1;

	for (i = 0; i < NROFMATERIALS; i++)
	{
		if (op->material&(1 << i))
		{
			materials++;
			if (rndm(1, 20) >= material[i].save[number]-op->magic-op->resist[number] / 100)
				saves++;
			/* if the attack is too weak */
			if ((20 - material[i].save[number]) / 3 > originator->stats.dam)
				saves++;
		}
	}

	if (saves == materials || materials == 0)
		return 1;

	if ((saves == 0) || (rndm(1, materials) > saves))
		return 0;

	return 1;
}

/* This function calls did_make_save_item.  It then performs the
 * appropriate actions to the item (such as burning the item up,
 * calling cancellation, etc.) */
void save_throw_object(object *op, object *originator)
{
	/* If the object is magical AND identified */
	if (QUERY_FLAG(op, FLAG_IS_MAGICAL) && QUERY_FLAG(op, FLAG_IDENTIFIED))
		return;

	/* If the object is non pickable and it's not player/party corpse */
	if (QUERY_FLAG(op, FLAG_NO_PICK) && !(op->sub_type1 == ST1_CONTAINER_CORPSE_player || op->sub_type1 == ST1_CONTAINER_CORPSE_party))
		return;

	/* If this is player corpse BUT it's not our corpse */
	if (op->sub_type1 == ST1_CONTAINER_CORPSE_player && op->slaying && strcmp(op->slaying, originator->owner->name) != 0)
		return;

	/* If this is party corpse BUT we're not in the party */
	if (op->sub_type1 == ST1_CONTAINER_CORPSE_party && op->stats.maxhp && (CONTR(originator->owner)->party_number == -1 || op->stats.maxhp != CONTR(originator->owner)->party_number))
		return;

	if (originator->attack[ATNR_COLD] > 0)
	{
		if (!did_make_save_item(op, originator))
		{
			object *tmp;
			archetype *at = find_archetype("icecube");

			if (at == NULL)
			{
				LOG(llevBug, "BUG: save_throw_object(): could not find archetype icecube.\n");
				return;
			}
			op = stop_item(op);
			if (op == NULL)
				return;

			if ((tmp = present_arch(at, op->map, op->x, op->y)) == NULL)
			{
				tmp = arch_to_object(at);
				tmp->x = op->x,
						 tmp->y = op->y;
				insert_ob_in_map(tmp, op->map, originator, 0);
			}

			if (!QUERY_FLAG(op, FLAG_REMOVED))
				remove_ob(op);

			(void)insert_ob_in_ob(op, tmp);
		}
		return;
	}

	if (!did_make_save_item(op, originator))
	{
		object *env = op->env;
		mapstruct *m = op->map;

		op = stop_item(op);
		if (op == NULL)
			return;

		if (op->nrof > 1)
		{
			op = decrease_ob_nr(op, rndm(0, op->nrof - 1));

			if (op)
				fix_stopped_item(op, m, originator);
		}
		else
		{
			if (op->env)
			{
				object *tmp = is_player_inv(op->env);

				if (tmp)
				{
					esrv_del_item(CONTR(tmp), op->count,op->env);
					esrv_update_item(UPD_WEIGHT, tmp, tmp);
				}
			}

			if (!QUERY_FLAG(op, FLAG_REMOVED))
			{
				destruct_ob(op);
				op->speed = 0;
				update_ob_speed(op);
			}
		}

		if (originator->attack[ATNR_FIRE] > 0 || originator->attack[ATNR_ELECTRICITY] > 0)
		{
			if (env)
			{
				op = get_archetype("burnout");
				op->x = env->x, op->y = env->y;
				insert_ob_in_ob(op, env);
			}
			else
				replace_insert_ob_in_map("burnout", originator);
		}

		return;
	}
}
