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
 * Player login/save/logout related functions. */

#include <global.h>
#include <sproto.h>
#include <loader.h>

/** Objects link of DMs. */
objectlink *dm_list = NULL;

/** Minimum length a player name must have. */
#define PLAYER_NAME_MIN 2

/** Maximum length a player name can have. */
#define PLAYER_NAME_MAX 12

/**
 * Save all players.
 * @param flag If non zero, it means that we want to try and save
 * everyone, but keep the game running. Thus, we don't want to free any
 * information. */
void emergency_save(int flag)
{
#ifndef NO_EMERGENCY_SAVE
	LOG(llevSystem, "Emergency save:  ");

	for (pl = first_player; pl; pl = pl->next)
	{
		if (!pl->ob)
		{
			LOG(llevSystem, "No name, ignoring this.\n");
			continue;
		}

		LOG(llevSystem, "%s ", pl->ob->name);
		new_draw_info(NDI_UNIQUE, pl->ob, "Emergency save...");

		/* If we are not exiting the game (ie, this is sort of a backup
		 * save), then don't change the location back to the village.
		 * Note that there are other options to have backup saves be done
		 * at the starting village */
		if (!flag)
		{
			strcpy(pl->maplevel, first_map_path);

			if (pl->ob->map != NULL)
			{
				pl->ob->map = NULL;
			}

			pl->ob->x = -1;
			pl->ob->y = -1;
		}

		container_unlink(pl, NULL);

		if (!save_player(pl->ob, flag))
		{
			LOG(llevSystem, "(failed) ");
			new_draw_info(NDI_UNIQUE, pl->ob, "Emergency save failed, checking score...");
		}

		hiscore_check(pl->ob, 1);
	}

	LOG(llevSystem, "\n");
#else
	(void) flag;
	LOG(llevSystem, "Emergency saves disabled, no save attempted\n");
#endif
}

/**
 * Checks to see if the passed player name is valid or not. Does checks
 * like min/max name length, whether there is anyone else playing by that
 * name, and whether there are illegal characters in the player name.
 * @param pl Player.
 * @param name Name to check.
 * @return 1 if the name is ok, 0 otherwise. */
int check_name(player *pl, char *name)
{
	size_t name_len;

	if (name[0] == '\0')
	{
		send_socket_message(NDI_RED, &pl->socket, "You must provide a name to log in.");
		return 0;
	}

	name_len = strlen(name);

	if (name_len < PLAYER_NAME_MIN || name_len > PLAYER_NAME_MAX)
	{
		send_socket_message(NDI_RED, &pl->socket, "That name has an invalid length.");
		return 0;
	}

	if (!playername_ok(name))
	{
		send_socket_message(NDI_RED, &pl->socket, "That name contains illegal characters.");
		return 0;
	}

	return 1;
}

/**
 * Saves a player to file.
 * @param op Player to save.
 * @param flag If set, it's only backup, i.e. don't remove objects from
 * inventory.
 * @return Non zero if successful. */
int save_player(object *op, int flag)
{
	FILE *fp;
	char filename[MAX_BUF], backupfile[MAX_BUF];
	player *pl = CONTR(op);
	int i, wiz = QUERY_FLAG(op, FLAG_WIZ);

	/* No experience, no save */
	if (!op->stats.exp && (!CONTR(op) || !CONTR(op)->player_loaded))
	{
		return 0;
	}

	flag &= 1;

	/* Sanity check - some stuff changes this when player is exiting */
	if (op->type != PLAYER)
	{
		return 0;
	}

	/* Prevent accidental saves if connection is reset after player has
	 * mostly exited. */
	if (pl->state != ST_PLAYING)
	{
		return 0;
	}

	snprintf(filename, sizeof(filename), "%s/%s/%s/%s.pl", settings.localdir, settings.playerdir, op->name, op->name);
	make_path_to_file(filename);
	fp = fopen(filename, "w");
	snprintf(backupfile, sizeof(backupfile), "%s.tmp", filename);
	rename(filename, backupfile);

	if (!fp)
	{
		new_draw_info(NDI_UNIQUE, op, "Can't open file for saving.");
		LOG(llevDebug, "Can't open file for saving (%s).\n", filename);
		rename(backupfile, filename);
		return 0;
	}

	fprintf(fp, "password %s\n", pl->password);
	fprintf(fp, "dm_stealth %d\n", pl->dm_stealth);
	fprintf(fp, "gen_hp %d\n", pl->gen_hp);
	fprintf(fp, "gen_sp %d\n", pl->gen_sp);
	fprintf(fp, "gen_grace %d\n", pl->gen_grace);
	fprintf(fp, "spell %d\n", pl->chosen_spell);
	fprintf(fp, "shoottype %d\n", pl->shoottype);
	fprintf(fp, "digestion %d\n", pl->digestion);

	if (op->map)
	{
		fprintf(fp, "map %s\n", op->map->path);
	}
	else
	{
		fprintf(fp, "map %s\n", EMERGENCY_MAPPATH);
	}

	fprintf(fp, "savebed_map %s\n", pl->savebed_map);
	fprintf(fp, "bed_x %d\nbed_y %d\n", pl->bed_x, pl->bed_y);
	fprintf(fp, "Str %d\n", pl->orig_stats.Str);
	fprintf(fp, "Dex %d\n", pl->orig_stats.Dex);
	fprintf(fp, "Con %d\n", pl->orig_stats.Con);
	fprintf(fp, "Int %d\n", pl->orig_stats.Int);
	fprintf(fp, "Pow %d\n", pl->orig_stats.Pow);
	fprintf(fp, "Wis %d\n", pl->orig_stats.Wis);
	fprintf(fp, "Cha %d\n", pl->orig_stats.Cha);

	/* Save hp table */
	fprintf(fp, "lev_hp %d\n", op->level);

	for (i = 1; i <= op->level; i++)
	{
		fprintf(fp, "%d\n", pl->levhp[i]);
	}

	/* Save sp table */
	fprintf(fp, "lev_sp %d\n", pl->sp_exp_ptr->level);

	for (i = 1; i <= pl->sp_exp_ptr->level; i++)
	{
		fprintf(fp, "%d\n", pl->levsp[i]);
	}

	/* Save grace table */
	fprintf(fp, "lev_grace %d\n", pl->grace_exp_ptr->level);

	for (i = 1; i <= pl->grace_exp_ptr->level; i++)
	{
		fprintf(fp, "%d\n", pl->levgrace[i]);
	}

	for (i = 0; i < pl->nrofknownspells; i++)
	{
		fprintf(fp, "known_spell %s\n", spells[pl->known_spells[i]].name);
	}

	fprintf(fp, "endplst\n");

	SET_FLAG(op, FLAG_NO_FIX_PLAYER);
	CLEAR_FLAG(op, FLAG_WIZ);

	/* Don't check and don't remove */
	save_object(fp, op, 3);

	/* Make sure the write succeeded */
	if (fclose(fp) == EOF)
	{
		new_draw_info(NDI_UNIQUE, op, "Can't save character.");
		CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);
		rename(backupfile, filename);
		return 0;
	}

	if (wiz)
	{
		SET_FLAG(op, FLAG_WIZ);
	}

	rename(backupfile, filename);
	chmod(filename, SAVE_MODE);
	CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);
	return 1;
}

/**
 * Sort loaded spells by comparing a1 against a2.
 * @param a1 Spell ID to compare
 * @param a2 Spell ID to compare
 * @return Return value of strcmp on the two spell names. */
static int spell_sort(const void *a1, const void *a2)
{
	return strcmp(spells[(int)*(sint16 *) a1].name, spells[(int)*(sint16 *) a2].name);
}

/**
 * Reorder the inventory of an object (reverses the order of the inventory
 * objects).
 * Helper function to reorder the reverse loaded player inventory.
 *
 * @note This will recursively reorder the container inventories.
 * @param op The object to reorder. */
static void reorder_inventory(object *op)
{
	object *tmp, *tmp2;

	tmp2 = op->inv->below;
	op->inv->above = NULL;
	op->inv->below = NULL;

	for (; tmp2; )
	{
		tmp = tmp2;
		/* Save the following element */
		tmp2 = tmp->below;
		tmp->above = NULL;
		/* Resort it like in insert_ob_in_ob() */
		tmp->below = op->inv;
		tmp->below->above = tmp;
		op->inv = tmp;

		if (tmp->inv)
		{
			reorder_inventory(tmp);
		}
	}
}

/**
 * Player in login failed to provide a correct password.
 *
 * After several repeated password failures, kill the socket.
 * @param pl Player. */
static void wrong_password(player *pl)
{
	pl->socket.password_fails++;

	LOG(llevSystem, "CRACK: %s@%s: Failed to provide correct password.\n", query_name(pl->ob, NULL), pl->socket.host);

	if (pl->socket.password_fails >= MAX_PASSWORD_FAILURES)
	{
		LOG(llevSystem, "CRACK: %s@%s: Failed to provide a correct password too many times!\n", query_name(pl->ob, NULL), pl->socket.host);
		send_socket_message(NDI_RED, &pl->socket, "You have failed to provide a correct password too many times.");
		pl->socket.status = Ns_Dead;
	}
	else
	{
		FREE_AND_COPY_HASH(pl->ob->name, "noname");
		get_name(pl->ob);
	}
}

/**
 * Login a player.
 * @param op Player. */
void check_login(object *op)
{
	FILE *fp;
	void *mybuffer;
	char filename[MAX_BUF], buf[MAX_BUF], bufall[MAX_BUF];
	int i, value, comp, correct = 0;
	player *pl = CONTR(op), *pltmp;
	time_t elapsed_save_time = 0;
	struct stat	statbuf;

	strcpy(pl->maplevel, first_map_path);

	/* Check if this matches a connected player, and if so disconnect old
	 * and connect new. */
	for (pltmp = first_player; pltmp != NULL; pltmp = pltmp->next)
	{
		if (pltmp != pl && pltmp->ob->name != NULL && !strcmp(pltmp->ob->name, op->name))
		{
			if (check_password(pl->write_buf + 1, pltmp->password))
			{
				pltmp->socket.status = Ns_Dead;
				break;
			}
			else
			{
				wrong_password(pl);
				return;
			}
		}
	}

	if (pl->state == ST_PLAYING)
	{
		LOG(llevSystem, "CRACK: >%s< from IP %s - double login!\n", op->name, pl->socket.host);
		send_socket_message(NDI_RED, &pl->socket, "Connection refused.\nYou manipulated the login procedure.");
		pl->socket.status = Ns_Dead;
		return;
	}

	if (checkbanned(op->name, pl->socket.host))
	{
		LOG(llevInfo, "BAN: Banned player tried to login. [%s@%s]\n", op->name, pl->socket.host);
		send_socket_message(NDI_RED, &pl->socket, "Connection refused.\nYou are banned!");
		pl->socket.status = Ns_Dead;
		return;
	}

	LOG(llevInfo, "LOGIN: >%s< from IP %s\n", op->name, pl->socket.host);

	snprintf(filename, sizeof(filename), "%s/%s/%s/%s.pl", settings.localdir, settings.playerdir, op->name, op->name);

	/* If no file, must be a new player, so lets get confirmation of
	 * the password.  Return control to the higher level dispatch,
	 * since the rest of this just deals with loading of the file. */
	if ((fp = open_and_uncompress(filename, 1, &comp)) == NULL)
	{
		confirm_password(op);
		return;
	}

	if (fstat(fileno(fp), &statbuf))
	{
		LOG(llevBug, "BUG: Unable to stat %s?\n", filename);
		elapsed_save_time = 0;
	}
	else
	{
		elapsed_save_time = time(NULL) - statbuf.st_mtime;

		if (elapsed_save_time < 0)
		{
			LOG(llevBug, "BUG: Player file %s was saved in the future? (%d time)\n", filename, elapsed_save_time);
			elapsed_save_time = 0;
		}
	}

	if (fgets(bufall, sizeof(bufall), fp))
	{
		if (sscanf(bufall, "password %s\n", buf))
		{
			correct = check_password(pl->write_buf + 1, buf);
		}
	}

	if (!correct)
	{
		wrong_password(pl);
		return;
	}

	pl->afk = 0;

#ifdef SAVE_INTERVAL
	pl->last_save_time = time(NULL);
#endif

	pl->party = NULL;

	pl->orig_stats.Str = 0;
	pl->orig_stats.Dex = 0;
	pl->orig_stats.Con = 0;
	pl->orig_stats.Int = 0;
	pl->orig_stats.Pow = 0;
	pl->orig_stats.Wis = 0;
	pl->orig_stats.Cha = 0;
	strcpy(pl->savebed_map, first_map_path);
	pl->bed_x = 0;
	pl->bed_y = 0;

	/* Loop through the file, loading the rest of the values. */
	while (fgets(bufall, sizeof(bufall), fp))
	{
		sscanf(bufall, "%s %d\n", buf, &value);

		if (!strcmp(buf, "endplst"))
		{
			break;
		}
		else if (!strcmp(buf, "dm_stealth"))
		{
			pl->dm_stealth = value;
		}
		else if (!strcmp(buf, "gen_hp"))
		{
			pl->gen_hp = value;
		}
		else if (!strcmp(buf, "shoottype"))
		{
			pl->shoottype = (rangetype) value;
		}
		else if (!strcmp(buf, "gen_sp"))
		{
			pl->gen_sp = value;
		}
		else if (!strcmp(buf, "gen_grace"))
		{
			pl->gen_grace = value;
		}
		else if (!strcmp(buf, "spell"))
		{
			pl->chosen_spell = value;
		}
		else if (!strcmp(buf, "digestion"))
		{
			pl->digestion = value;
		}
		else if (!strcmp(buf, "map"))
		{
			sscanf(bufall, "map %s", pl->maplevel);
		}
		else if (!strcmp(buf, "savebed_map"))
		{
			sscanf(bufall, "savebed_map %s", pl->savebed_map);
		}
		else if (!strcmp(buf, "bed_x"))
		{
			pl->bed_x = value;
		}
		else if (!strcmp(buf, "bed_y"))
		{
			pl->bed_y = value;
		}
		else if (!strcmp(buf, "Str"))
		{
			pl->orig_stats.Str = value;
		}
		else if (!strcmp(buf, "Dex"))
		{
			pl->orig_stats.Dex = value;
		}
		else if (!strcmp(buf, "Con"))
		{
			pl->orig_stats.Con = value;
		}
		else if (!strcmp(buf, "Int"))
		{
			pl->orig_stats.Int = value;
		}
		else if (!strcmp(buf, "Pow"))
		{
			pl->orig_stats.Pow = value;
		}
		else if (!strcmp(buf, "Wis"))
		{
			pl->orig_stats.Wis = value;
		}
		else if (!strcmp(buf, "Cha"))
		{
			pl->orig_stats.Cha = value;
		}
		else if (!strcmp(buf, "lev_hp"))
		{
			int j;

			for (i = 1; i <= value; i++)
			{
				if (fscanf(fp, "%d\n", &j))
				{
					pl->levhp[i] = j;
				}
			}
		}
		else if (!strcmp(buf, "lev_sp"))
		{
			int j;

			for (i = 1; i <= value; i++)
			{
				if (fscanf(fp, "%d\n", &j))
				{
					pl->levsp[i] = j;
				}
			}
		}
		else if (!strcmp(buf, "lev_grace"))
		{
			int j;

			for (i = 1; i <= value; i++)
			{
				if (fscanf(fp, "%d\n", &j))
				{
					pl->levgrace[i] = j;
				}
			}
		}
		else if (!strcmp(buf, "known_spell"))
		{
			char *cp = strchr(bufall, '\n');
			*cp = '\0';
			cp = strchr(bufall, ' ');
			cp++;

			for (i = 0; i < NROFREALSPELLS; i++)
			{
				if (!strcmp(spells[i].name, cp))
				{
					pl->known_spells[pl->nrofknownspells++] = i;
					break;
				}
			}

			if (i == NROFREALSPELLS)
			{
				LOG(llevDebug, "BUG: check_login(): Bogus spell (%s) in %s\n", cp, filename);
			}
		}
	}

	/* Take the player object out from the void */
	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
		remove_ob(op);
	}

	/* We transfer it to a new object */
	op->custom_attrset = NULL;

	LOG(llevDebug, "load obj for player: %s\n", op->name);

	/* Create a new object for the real player data */
	op = get_object();

	/* This loads the standard objects values. */
	mybuffer = create_loader_buffer(fp);
	load_object(fp, op, mybuffer, LO_REPEAT, 0);
	delete_loader_buffer(mybuffer);
	close_and_delete(fp, comp);

	/* The inventory of players is reverse loaded, so let's exchange the
	 * order here. */
	if (op->inv)
	{
		reorder_inventory(op);
	}

	op->custom_attrset = pl;
	pl->ob = op;
	CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);

	op->type = PLAYER;

	/* This is a funny thing: what happens when the autosave function saves a player
	 * with negative hp? Well, the sever tries to create a gravestone and heals the
	 * player... and then server tries to insert gravestone and anim on a map - but
	 * player is still in login! So, we are nice and set hp to 1 if here negative. */
	if (op->stats.hp < 0)
	{
		op->stats.hp = 1;
	}

	pl->state = ST_PLAYING;
#ifdef AUTOSAVE
	pl->last_save_tick = pticks;
#endif
	op->carrying = sum_weight(op);

	init_player_exp(op);
	link_player_skills(op);

	if (!legal_range(op, pl->shoottype))
	{
		pl->shoottype = range_none;
	}

	fix_player(op);

	/* If it's a dragon player, set the correct title here */
	if (is_dragon_pl(op) && op->inv)
	{
		object *tmp, *abil = NULL, *skin = NULL;

		for (tmp = op->inv; tmp; tmp = tmp->below)
		{
			if (tmp->type == FORCE)
			{
				if (strcmp(tmp->arch->name, "dragon_ability_force") == 0)
				{
					abil = tmp;
				}
				else if (strcmp(tmp->arch->name, "dragon_skin_force") == 0)
				{
					skin = tmp;
				}
			}
		}

		set_dragon_name(op, abil, skin);
	}

	/* Important: there is a player file */
	pl->player_loaded = 1;

	/* Display Message of the Day */
	display_motd(op);

	if (!pl->dm_stealth)
	{
		new_draw_info_format(NDI_UNIQUE | NDI_ALL | NDI_DK_ORANGE, NULL, "%s has entered the game.", query_name(pl->ob, NULL));

		if (dm_list)
		{
			objectlink *ol;
			player *pl_tmp;
			int players;

			for (pl_tmp = first_player, players = 0; pl_tmp; pl_tmp = pl_tmp->next, players++)
			{
			}

			for (ol = dm_list; ol; ol = ol->next)
			{
				new_draw_info_format(NDI_UNIQUE, ol->objlink.ob, "DM: %d players now playing.", players);
			}
		}
	}

	/* Trigger the global LOGIN event */
	trigger_global_event(EVENT_LOGIN, pl, pl->socket.host);

	esrv_new_player(pl, op->weight + op->carrying);
	esrv_send_inventory(op, op);

	/* This seems to compile without warnings now.  Don't know if it works
	 * on SGI's or not, however. */
	qsort((void *) pl->known_spells, pl->nrofknownspells, sizeof(pl->known_spells[0]), (int (*)()) spell_sort);

	if (!QUERY_FLAG(op, FLAG_FRIENDLY))
	{
		LOG(llevBug, "BUG: Player %s was loaded without friendly flag!", query_name(op, NULL));
		SET_FLAG(op, FLAG_FRIENDLY);
		add_friendly_object(op);
	}

	enter_exit(op, NULL);

	pl->socket.update_tile = 0;
	pl->socket.look_position = 0;
	pl->socket.ext_title_flag = 1;
	esrv_new_player(pl, op->weight + op->carrying);
	send_spelllist_cmd(op, NULL, SPLIST_MODE_ADD);
	send_skilllist_cmd(op, NULL, SPLIST_MODE_ADD);
	send_quickslots(pl);
}
