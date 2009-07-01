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

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/* object op is trying to move in direction dir.
 * originator is typically the same as op, but
 * can be different if originator is causing op to
 * move (originator is pushing op)
 * returns 0 if the object is not able to move to the
 * desired space, 1 otherwise (in which case we also
 * move the object accordingly
 * Return -1 if the object is destroyed in the move process (most likely
 * when hit a deadly trap or something). */
int move_ob(object *op, int dir, object *originator)
{
    object *tmp;
	mapstruct *m;
	int xt, yt, flags;

    if (op == NULL)
	{
		LOG(llevBug, "BUG: move_ob(): Trying to move NULL.\n");
		return 0;
    }

    if (QUERY_FLAG(op, FLAG_REMOVED))
	{
		LOG(llevBug, "BUG: move_ob: monster has been removed - will not process further\n");
		return 0;
    }

    /* this function should now only be used on the head - it won't call itself
     * recursively, and functions calling us should pass the right part. */
    if (op->head)
	{
		LOG(llevDebug, "DEBUG: move_ob() called with non head object: %s %s (%d,%d)\n", query_name(op->head, NULL), op->map->path ? op->map->path : "<no map>", op->x, op->y);
		op = op->head;
    }

	/* animation stuff */
    if (op->head)
        op->head->anim_moving_dir = dir;
    else
        op->anim_moving_dir = dir;

    op->direction = dir;

	xt = op->x + freearr_x[dir];
	yt = op->y + freearr_y[dir];

	/* we have here a out_of_map - we can skip all */
	if (!(m = out_of_map(op->map, &xt, &yt)))
		return 0;

	/* totally new logic here... blocked() handles now ALL map flags... blocked_two()
	* is called implicit from blocked() - really only called for nodes where a checker
	* is inside. blocked_link() is used for multi arch blocked_test().
	* Inside here we use a extended version of blocked_link(). Reason is, that even when
	* we can't move the multi arch on the new spot, we have perhaps a legal earthwall
	* in the step - we need to hit it here too. That a 3x3 multi arch monster can't
	* enter a corridor of 2 tiles is right - but when the entry is closed by a wall
	* then there is no reason why we can't hit the wall - even when we can't enter in. */

	/* multi arch objects... */
    if (op->more)
	{
		/* insert new blocked_link() here which can hit ALL earthwalls */
		/* but as long monster don't destroy walls and no mult arch player
		 * are ingame - we can stay with this */
		/* look in single tile move to see how we handle doors.
		 * This needs to be done before we allow multi tile mobs to do
		 * more fancy things. */
		if (blocked_link(op, freearr_x[dir], freearr_y[dir]))
			return 0;

		remove_ob(op);
		if (check_walk_off(op, originator, MOVE_APPLY_MOVE) & (CHECK_WALK_DESTROYED | CHECK_WALK_MOVED))
			return 1;

		for (tmp = op; tmp != NULL; tmp = tmp->more)
			tmp->x += freearr_x[dir], tmp->y += freearr_y[dir];

		if (op->type == PLAYER)
		{
			esrv_map_scroll(&CONTR(op)->socket, freearr_x[dir], freearr_y[dir]);
			CONTR(op)->socket.look_position = 0;
		}

		insert_ob_in_map(op, op->map, op, 0);

		return 1;
	}

    /* single arch */
	if (!QUERY_FLAG(op, FLAG_WIZPASS))
	{
		/* is the spot blocked from something? */
		if ((flags = blocked(op, m, xt, yt, op->terrain_flag)))
		{
			/* blocked!... BUT perhaps we have a door here to open.
			 * If P_DOOR_CLOSED returned by blocked() then we have a door here.
			 * If there is a door but not touchable from op, then blocked()
			 * will hide the flag! So, if the flag is set, we can try our
			 * luck - but only if op can open doors! */
			/* a (closed) door which we can open? */
			if ((flags & P_DOOR_CLOSED) && (op->will_apply & 8))
			{
				/* yes, we can open this door */
				if (open_door(op, m, xt, yt, 1))
					return 1;
			}

			/* in any case we don't move - door or not. This will avoid we open the door
			 * and do the move in one turn. */
			return 0;
		}
	}

	remove_ob(op);
	if (check_walk_off(op, originator, MOVE_APPLY_MOVE) & (CHECK_WALK_DESTROYED | CHECK_WALK_MOVED))
		return 1;

	op->x += freearr_x[dir];
	op->y += freearr_y[dir];
	if (op->type == PLAYER)
	{
		esrv_map_scroll(&CONTR(op)->socket, freearr_x[dir], freearr_y[dir]);
		CONTR(op)->socket.look_position = 0;
	}
	insert_ob_in_map(op, op->map, originator, 0);

	return 1;
}


/* transfer_ob(): Move an object (even linked objects) to another spot
 * on the same map.
 *
 * Does nothing if there is no free spot.
 *
 * randomly: If true, use find_free_spot() to find the destination, otherwise
 * use find_first_free_spot().
 *
 * Return value: 1 if object was destroyed, 0 otherwise. */
int transfer_ob(object *op, int x, int y, int randomly, object *originator, object *trap)
{
	int i, ret;
	object *tmp;

	/* this is not 100% tested for mobs - enter_exit will still fail to return for mobs */
	/* but some testing should make it for mobs too */
    if (trap != NULL && EXIT_PATH(trap))
	{
		if (op->type == PLAYER && trap->msg && strncmp(EXIT_PATH(trap), "/!", 2) && strncmp(EXIT_PATH(trap), "/random/", 8))
		    new_draw_info (NDI_NAVY, 0, op, trap->msg);

		enter_exit(op, trap);
		return 1;
    }
	else if (randomly)
		i = find_free_spot(op->arch, op->map, x, y, 0, SIZEOFFREE);
	else
		i = find_first_free_spot(op->arch, op->map, x, y);

	/* No free spot */
	if (i == -1)
		return 0;

	if (op->head != NULL)
		op = op->head;

	remove_ob(op);

	if (check_walk_off(op, NULL, MOVE_APPLY_DEFAULT) != CHECK_WALK_OK)
		return 1;

	for (tmp = op; tmp != NULL; tmp = tmp->more)
	{
		tmp->x = x + freearr_x[i] + (tmp->arch == NULL ? 0 : tmp->arch->clone.x);
		tmp->y = y + freearr_y[i] + (tmp->arch == NULL ? 0 : tmp->arch->clone.y);
	}

	ret = (insert_ob_in_map(op, op->map, originator, 0) == NULL);
	if (op->type == PLAYER)
		MapNewmapCmd(CONTR(op));

  	return ret;
}

/* Return value: 1 if object was destroyed, 0 otherwise.
 * Modified so that instead of passing the 'originator' that had no
 * real use, instead we pass the 'user' of the teleporter.  All the
 * callers know what they wanted to teleporter (move_teleporter or
 * shop map code)
 * tele_type is the type of teleporter we want to match against -
 * currently, this is either set to SHOP_MAT or TELEPORTER.
 * It is basically used so that shop_mats and normal teleporters can
 * be used close to each other and not have the player put to the
 * one of another type. */
int teleport(object *teleporter, uint8 tele_type, object *user)
{
	/* Better use c/malloc here in the future */
    object *altern[120];
    int i, j, k, nrofalt = 0, xt, yt;
    object *other_teleporter, *tmp;
	mapstruct *m;

    if (user == NULL)
		return 0;

    if (user->head != NULL)
		user = user->head;

    /* Find all other teleporters within range.  This range
     * should really be setable by some object attribute instead of
     * using hard coded values. */
    for (i = -5; i < 6; i++)
	{
		for (j = -5; j < 6; j++)
		{
			if (i == 0 && j == 0)
				continue;

			xt = teleporter->x + i;
			yt = teleporter->y + j;

			if (!(m = out_of_map(teleporter->map, &xt, &yt)))
				continue;

			other_teleporter = get_map_ob(m, xt, yt);

			while (other_teleporter)
			{
				if (other_teleporter->type == tele_type)
					break;

				other_teleporter = other_teleporter->above;
			}

			if (other_teleporter)
				altern[nrofalt++] = other_teleporter;
		}
	}

    if (!nrofalt)
	{
		LOG(llevBug, "BUG: teleport(): No alternative teleporters around!\n");
		return 0;
    }

    other_teleporter = altern[RANDOM() % nrofalt];
    k = find_free_spot(user->arch, other_teleporter->map, other_teleporter->x, other_teleporter->y, 1, 9);

    if (k == -1)
		return 0;

    remove_ob(user);

	if (check_walk_off(user, NULL, MOVE_APPLY_VANISHED) != CHECK_WALK_OK)
		return 1;

    /* Update location for the object */
    for(tmp=user;tmp!=NULL;tmp=tmp->more)
	{
		tmp->x = other_teleporter->x + freearr_x[k] + (tmp->arch == NULL ? 0 : tmp->arch->clone.x);
		tmp->y = other_teleporter->y + freearr_y[k] + (tmp->arch == NULL ? 0 : tmp->arch->clone.y);
    }

    tmp = insert_ob_in_map(user, other_teleporter->map, NULL, 0);

    if (tmp && tmp->type == PLAYER)
		MapNewmapCmd(CONTR(tmp));

    return (tmp == NULL);
}

void recursive_roll(object *op, int dir, object *pusher)
{
	if (!roll_ob(op, dir, pusher))
	{
		new_draw_info_format(NDI_UNIQUE, 0, pusher, "You fail to push the %s.", query_name(op, NULL));
		return;
	}

	(void) move_ob(pusher, dir, pusher);
	new_draw_info_format(NDI_WHITE, 0, pusher, "You roll the %s.", query_name(op, NULL));
	return;
}

/* This is a new version of blocked, this one handles objects
 * that can be passed through by monsters with the CAN_PASS_THRU defined.
 *
 * very new version handles also multipart objects */
int try_fit(object *op, int x, int y)
{
    object *tmp, *more;
	mapstruct *m;
    int tx, ty;

    if (op->head)
		op = op->head;

    for (more = op; more; more = more->more)
	{
		tx = x + more->x - op->x;
		ty = y + more->y - op->y;
		if (!(m = out_of_map(op->map, &tx, &ty)))
			return 1;

		for (tmp = get_map_ob(m, tx, ty); tmp; tmp = tmp->above)
		{
			if (tmp->head == op || tmp == op)
				continue;

			if (IS_LIVE(tmp))
				return 1;

			if (QUERY_FLAG(tmp, FLAG_NO_PASS) && (!QUERY_FLAG(tmp, FLAG_PASS_THRU) || !QUERY_FLAG(more, FLAG_CAN_PASS_THRU)))
				return 1;
		}
    }
    return 0;
}

/* this is not perfect yet. *
 * it does not roll objects behind multipart objects properly. */
int roll_ob(object *op, int dir, object *pusher)
{
    object *tmp;
	mapstruct *m;
    int x, y;

    if (op->head)
		op = op->head;

    if (!QUERY_FLAG(op, FLAG_CAN_ROLL) || (op->weight && random_roll(0, op->weight / 50000 - 1, pusher, PREFER_LOW) > pusher->stats.Str))
		return 0;

    x = op->x + freearr_x[dir];
    y = op->y + freearr_y[dir];

    if (!(m = out_of_map(op->map, &x, &y)))
		return 0;

    for (tmp = get_map_ob(m, x, y); tmp != NULL; tmp = tmp->above)
	{
		if (tmp->head == op)
			continue;

		if (IS_LIVE(tmp) || (QUERY_FLAG(tmp, FLAG_NO_PASS) && !roll_ob(tmp, dir, pusher)))
			return 0;
    }

    if (try_fit(op, op->x + freearr_x[dir], op->y + freearr_y[dir]))
		return 0;

    remove_ob(op);

	if (check_walk_off(op, NULL, MOVE_APPLY_VANISHED) != CHECK_WALK_OK)
		return 0;

	for (tmp = op; tmp != NULL; tmp = tmp->more)
		tmp->x += freearr_x[dir], tmp->y += freearr_y[dir];

    insert_ob_in_map(op, op->map, pusher, 0);
    return 1;
}

/* returns 1 if pushing invokes a attack, 0 when not */
/* new combat command do the attack now - i disabled push attacks */
int push_ob(object *who, int dir, object *pusher)
{
    int str1, str2;
    object *owner;

    if (who->head != NULL)
		who = who->head;

    owner = get_owner(who);

    /* Wake up sleeping monsters that may be pushed */
    CLEAR_FLAG(who, FLAG_SLEEP);

    /* player change place with his pets or summoned creature */
    /* TODO: allow multi arch pushing. Can't be very difficult */
    if (who->more == NULL && owner == pusher)
	{
		int temp;
		temp = pusher->x;
		temp = pusher->y;
		remove_ob(who);

		if (check_walk_off(who, NULL, MOVE_APPLY_DEFAULT) != CHECK_WALK_OK)
			return 0;

		remove_ob(pusher);

		if (check_walk_off(pusher, NULL, MOVE_APPLY_DEFAULT) != CHECK_WALK_OK)
		{
			/* something is wrong, put who back */
			insert_ob_in_map(who, who->map, NULL, 0);
			return 0;
		}

		pusher->x = who->x;
		who->x = temp;
		pusher->y = who->y;
		who->y = temp;
		/* we presume that if the player is pushing his put, he moved in
		* direction 'dir'.  I can' think of any case where this would not be
		* the case.  Putting the map_scroll should also improve performance some. */
		if (pusher->type == PLAYER )
		{
			esrv_map_scroll(&CONTR(pusher)->socket, freearr_x[dir], freearr_y[dir]);
			CONTR(pusher)->socket.look_position = 0;
		}
		insert_ob_in_map(who, who->map,pusher, 0);
		insert_ob_in_map(pusher, pusher->map, pusher, 0);

		return 0;
  	}

    /* now, lets test stand still we NEVER can psuh stand_still monsters. */
    if (QUERY_FLAG(who, FLAG_STAND_STILL))
    {
		new_draw_info_format(NDI_UNIQUE, 0, pusher, "You can't push %s.", who->name);
		return 0;
    }

    /* This block is basically if you are pushing friendly but
     * non pet creaturs.
     * It basically does a random strength comparision to
     * determine if you can push someone around.  Note that
     * this pushes the other person away - its not a swap. */

   	str1 = (who->stats.Str > 0 ? who->stats.Str : who->level);
    str2 = (pusher->stats.Str > 0 ? pusher->stats.Str : pusher->level);

    if (QUERY_FLAG(who, FLAG_WIZ) || random_roll(str1, str1 / 2 + str1 * 2, who, PREFER_HIGH) >= random_roll(str2, str2 / 2 + str2 * 2, pusher, PREFER_HIGH) || !move_ob(who, dir, pusher))
    {
		if (who ->type == PLAYER)
	    	new_draw_info_format(NDI_UNIQUE, 0, who, "%s tried to push you.", pusher->name);
		return 0;
    }

    /* If we get here, the push succeeded.  Let each now the
     * status.  I'm not sure if the second statement really needs
     * to be in an else block - the message is going to a different
     * player */
    if (who->type == PLAYER)
		new_draw_info_format(NDI_UNIQUE, 0, who, "%s pushed you.",pusher->name);
    else if (QUERY_FLAG(who, FLAG_MONSTER))
		new_draw_info_format(NDI_UNIQUE, 0, pusher, "You pushed %s back.", who->name);

    return 1;
}

int push_roll_object(object *op, int dir, const int flag)
{
    object *tmp;
    mapstruct *m;
    int xt, yt, ret;
    ret = 0;

	(void) flag;

	/* we check for all conditions where op can't push anything */
    if (dir <= 0 || QUERY_FLAG(op, FLAG_PARALYZED))
        return 0;

    xt = op->x + freearr_x[dir];
    yt = op->y + freearr_y[dir];

    if (!(m = out_of_map(op->map, &xt, &yt)))
        return 0;

	for (tmp = get_map_ob(m, xt, yt); tmp != NULL; tmp = tmp->above)
	{
        if (IS_LIVE(tmp) || QUERY_FLAG(tmp, FLAG_CAN_ROLL))
			break;
	}

	if (tmp == NULL)
	{
        new_draw_info(NDI_UNIQUE, 0, op, "You fail to push anything.");
		return 0;
	}

	/* No pushing of players on PVP maps. */
	if (tmp->type == PLAYER && pvp_area(op, tmp))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You cannot push players here.");
		return 0;
	}

	/* here we try to push pets, players and mobs */
	if ((get_owner(tmp) == op || IS_LIVE(tmp)))
    {
        play_sound_map(op->map, op->x, op->y, SOUND_PUSH_PLAYER, SOUND_NORMAL);
        if (push_ob(tmp, dir, op))
            ret = 1;

        if (op->hide)
            make_visible(op);

        return ret;
    }
	/* here we try to push moveable objects */
    else if (QUERY_FLAG(tmp, FLAG_CAN_ROLL))
    {
        tmp->direction = dir;
        recursive_roll(tmp, dir, op);
        if (action_makes_visible(op))
            make_visible(op);
    }
    return ret;
}

int missile_reflection_adjust(object *op, int flag)
{
	/* no more direction/reflection! */
	if (!op->stats.maxgrace)
		return FALSE;

	op->stats.maxgrace--;
	/* restore the "how long we can fly" counter */
	if (!flag)
		op->last_sp = op->stats.grace;

	/* go on with reflection/direction */
	return TRUE;
}
