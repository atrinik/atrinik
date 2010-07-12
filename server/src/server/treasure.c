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
 * Everything concerning treasures and artifacts. */

/* TREASURE_DEBUG does some checking on the treasurelists after loading.
 * It is useful for finding bugs in the treasures file.  Since it only
 * slows the startup some (and not actual game play), it is by default
 * left on */
#define TREASURE_DEBUG

/* TREASURE_VERBOSE enables copious output concerning artifact generation */
/*#define TREASURE_VERBOSE*/

#include <global.h>
#include <treasure.h>
#include <spellist.h>
#include <loader.h>

/** All the coin arches. */
char *coins[NUM_COINS + 1] =
{
	"mitcoin",
	"goldcoin",
	"silvercoin",
	"coppercoin",
	NULL
};

/** Pointers to coin archetypes. */
archetype *coins_arch[NUM_COINS];

/** Give 1 re-roll attempt per artifact */
#define ARTIFACT_TRIES 2

/** Chance fix. */
#define CHANCE_FIX (-1)

/** Pointer to the 'ring_generic' archetype. */
static archetype *ring_arch = NULL;
/** Pointer to the 'ring_normal' archetype. */
static archetype *ring_arch_normal = NULL;
/** Pointer to the 'amulet_generic' archetype. */
static archetype *amulet_arch = NULL;
/** Pointer to the 'amulet_normal' archetype. */
static archetype *amulet_arch_normal = NULL;

static treasure *load_treasure(FILE *fp, int *t_style, int *a_chance);
static void change_treasure(struct _change_arch *ca, object *op);
static treasurelist *get_empty_treasurelist();
static treasure *get_empty_treasure();
static void put_treasure(object *op, object *creator, int flags);
static artifactlist *get_empty_artifactlist();
static artifact *get_empty_artifact();
static void check_treasurelist(treasure *t, treasurelist *tl);
static void set_material_real(object *op, struct _change_arch *change_arch);
static void create_money_table();
static void create_all_treasures(treasure *t, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *change_arch);
static void create_one_treasure(treasurelist *tl, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *change_arch);
static int set_ring_bonus(object *op, int bonus, int level);
static int get_magic(int diff);
static void dump_monster_treasure_rec(const char *name, treasure *t, int depth);
static void free_treasurestruct(treasure *t);
static void free_charlinks(linked_char *lc);
static void free_artifactlist();
static void free_artifact(artifact *at);

/**
 * Opens LIBDIR/treasure and reads all treasure declarations from it.
 *
 * Each treasure is parsed with the help of load_treasure(). */
void load_treasures()
{
	FILE *fp;
	char filename[MAX_BUF], buf[MAX_BUF], name[MAX_BUF];
	treasurelist *previous = NULL;
	treasure *t;
	int comp, t_style, a_chance;

	snprintf(filename, sizeof(filename), "%s/%s", settings.datadir, settings.treasures);

	if ((fp = open_and_uncompress(filename, 0, &comp)) == NULL)
	{
		LOG(llevError, "ERROR: Can't open treasures file: %s\n", filename);
		return;
	}

	while (fgets(buf, MAX_BUF, fp) != NULL)
	{
		/* Ignore comments and blank lines */
		if (*buf == '#' || *buf == '\n')
		{
			continue;
		}

		if (sscanf(buf, "treasureone %s\n", name) || sscanf(buf, "treasure %s\n", name))
		{
			treasurelist *tl = get_empty_treasurelist();
			FREE_AND_COPY_HASH(tl->name, name);

			if (previous == NULL)
			{
				first_treasurelist = tl;
			}
			else
			{
				previous->next = tl;
			}

			previous = tl;
			t_style= T_STYLE_UNSET;
			a_chance = ART_CHANCE_UNSET;
			tl->items = load_treasure(fp, &t_style, &a_chance);

			if (tl->t_style == T_STYLE_UNSET)
			{
				tl->t_style = t_style;
			}

			if (tl->artifact_chance == ART_CHANCE_UNSET)
			{
				tl->artifact_chance = a_chance;
			}

			/* This is a one of the many items on the list should be generated.
			 * Add up the chance total, and check to make sure the yes and no
			 * fields of the treasures are not being used. */
			if (!strncmp(buf, "treasureone", 11))
			{
				for (t = tl->items; t != NULL; t = t->next)
				{
#ifdef TREASURE_DEBUG
					if (t->next_yes || t->next_no)
					{
						LOG(llevBug, "BUG: Treasure %s is one item, but on treasure %s\n", tl->name, t->item ? t->item->name : t->name);
						LOG(llevBug, "BUG:  the next_yes or next_no field is set");
					}
#endif
					tl->total_chance += t->chance;
				}
			}
		}
		else
		{
			LOG(llevError, "ERROR: Treasure list didn't understand: %s\n", buf);
		}
	}

	close_and_delete(fp, comp);

#ifdef TREASURE_DEBUG
	/* Perform some checks on how valid the treasure data actually is.
	 * Verify that list transitions work (ie, the list that it is
	 * supposed to transition to exists). Also, verify that at least the
	 * name or archetype is set for each treasure element. */
	for (previous = first_treasurelist; previous != NULL; previous = previous->next)
	{
		check_treasurelist(previous->items, previous);
	}
#endif

	create_money_table();
}

/**
 * Create money table, setting up pointers to the archetypes.
 *
 * This is done for faster access of the coins archetypes. */
static void create_money_table()
{
	int i;

	for (i = 0; coins[i]; i++)
	{
		coins_arch[i] = find_archetype(coins[i]);

		if (!coins_arch[i])
		{
			LOG(llevError, "ERROR: create_money_table(): Can't find %s.\n", coins[i] ? coins[i] : "NULL");
			return;
		}
	}
}

/**
 * Reads one treasure from the file, including the 'yes', 'no' and 'more'
 * options.
 * @param fp File to read from.
 * @param t_style Treasure style.
 * @param a_chance Artifact chance.
 * @return Read structure, never NULL. */
static treasure *load_treasure(FILE *fp, int *t_style, int *a_chance)
{
	char buf[MAX_BUF], *cp = NULL, variable[MAX_BUF];
	treasure *t = get_empty_treasure();
	int value, start_marker = 0, t_style2, a_chance2;

	nroftreasures++;

	while (fgets(buf, MAX_BUF, fp) != NULL)
	{
		if (*buf == '#')
		{
			continue;
		}

		if ((cp = strchr(buf, '\n')) != NULL)
		{
			*cp = '\0';
		}

		cp = buf;

		/* Skip blanks */
		while (!isalpha(*cp))
		{
			cp++;
		}

		if (sscanf(cp, "t_style %d", &value))
		{
			if (start_marker)
			{
				t->t_style = value;
			}
			else
			{
				/* No, it's global for the while treasure list entry */
				*t_style = value;
			}
		}
		else if (sscanf(cp, "artifact_chance %d", &value))
		{
			if (start_marker)
			{
				t->artifact_chance = value;
			}
			else
			{
				/* No, it's global for the while treasure list entry */
				*a_chance = value;
			}
		}
		else if (sscanf(cp, "arch %s", variable))
		{
			if ((t->item = find_archetype(variable)) == NULL)
			{
				LOG(llevBug, "BUG: Treasure lacks archetype: %s\n", variable);
			}

			start_marker = 1;
		}
		else if (sscanf(cp, "list %s", variable))
		{
			start_marker = 1;
			FREE_AND_COPY_HASH(t->name, variable);
		}
		else if (sscanf(cp, "name %s", variable))
		{
			FREE_AND_COPY_HASH(t->change_arch.name, cp + 5);
		}
		else if (sscanf(cp, "title %s", variable))
		{
			FREE_AND_COPY_HASH(t->change_arch.title, cp + 6);
		}
		else if (sscanf(cp, "slaying %s", variable))
		{
			FREE_AND_COPY_HASH(t->change_arch.slaying, cp + 8);
		}
		else if (sscanf(cp, "item_race %d", &value))
		{
			t->change_arch.item_race = value;
		}
		else if (sscanf(cp, "quality %d", &value))
		{
			t->change_arch.quality = value;
		}
		else if (sscanf(cp, "quality_range %d", &value))
		{
			t->change_arch.quality_range = value;
		}
		else if (sscanf(cp, "material %d", &value))
		{
			t->change_arch.material = value;
		}
		else if (sscanf(cp, "material_quality %d", &value))
		{
			t->change_arch.material_quality = value;
		}
		else if (sscanf(cp, "material_range %d", &value))
		{
			t->change_arch.material_range = value;
		}
		else if (sscanf(cp, "chance_fix %d", &value))
		{
			t->chance_fix = (sint16) value;
			/* Important or the chance will stay 100% when not set to 0
			 * in treasure list! */
			t->chance = 0;
		}
		else if (sscanf(cp, "chance %d", &value))
		{
			t->chance = (uint8) value;
		}
		else if (sscanf(cp, "nrof %d", &value))
		{
			t->nrof = (uint16) value;
		}
		else if (sscanf(cp, "magic %d", &value))
		{
			t->magic = value;
		}
		else if (sscanf(cp, "magic_fix %d", &value))
		{
			t->magic_fix = value;
		}
		else if (sscanf(cp, "magic_chance %d", &value))
		{
			t->magic_chance = value;
		}
		else if (sscanf(cp, "difficulty %d", &value))
		{
			t->difficulty = value;
		}
		else if (!strncmp(cp, "yes", 3))
		{
			t_style2 = T_STYLE_UNSET;
			a_chance2 = ART_CHANCE_UNSET;
			t->next_yes = load_treasure(fp, &t_style2, &a_chance2);

			if (t->next_yes->artifact_chance == ART_CHANCE_UNSET)
			{
				t->next_yes->artifact_chance = a_chance2;
			}

			if (t->next_yes->t_style == T_STYLE_UNSET)
			{
				t->next_yes->t_style = t_style2;
			}
		}
		else if (!strncmp(cp, "no", 2))
		{
			t_style2 = T_STYLE_UNSET;
			a_chance2 = ART_CHANCE_UNSET;
			t->next_no = load_treasure(fp, &t_style2, &a_chance2);

			if (t->next_no->artifact_chance == ART_CHANCE_UNSET)
			{
				t->next_no->artifact_chance = a_chance2;
			}

			if (t->next_no->t_style == T_STYLE_UNSET)
			{
				t->next_no->t_style = t_style2;
			}
		}
		else if (!strncmp(cp, "end", 3))
		{
			return t;
		}
		else if (!strncmp(cp, "more", 4))
		{
			t_style2 = T_STYLE_UNSET;
			a_chance2 = ART_CHANCE_UNSET;
			t->next = load_treasure(fp, &t_style2, &a_chance2);

			if (t->next->artifact_chance == ART_CHANCE_UNSET)
			{
				t->next->artifact_chance = a_chance2;
			}

			if (t->next->t_style == T_STYLE_UNSET)
			{
				t->next->t_style = t_style2;
			}

			return t;
		}
		else
		{
			LOG(llevBug, "BUG: Unknown treasure command: '%s', last entry %s\n", cp, t->name ? t->name : "null");
		}
	}

	LOG(llevBug, "BUG: Treasure %s lacks 'end'.>%s<\n", t->name ? t->name : "NULL", cp ? cp : "NULL");

	return t;
}

/**
 * Builds up the lists of artifacts from the file in the libdir.
 *
 * Can be called multiple times without ill effects. */
void init_artifacts()
{
	static int has_been_inited = 0;
	archetype *atemp;
	long old_pos, file_pos;
	FILE *fp;
	char filename[MAX_BUF], buf[MAX_BUF], *cp, *next;
	artifact *art = NULL;
	linked_char *tmp;
	int value, comp, none_flag = 0;
	size_t lcount;
	artifactlist *al;
	char buf_text[10 * 1024];

	if (has_been_inited)
	{
		return;
	}

	has_been_inited = 1;

	snprintf(filename, sizeof(filename), "%s/artifacts", settings.datadir);
	LOG(llevDebug, " reading artifacts from %s...", filename);

	if ((fp = open_and_uncompress(filename, 0, &comp)) == NULL)
	{
		LOG(llevError, "ERROR: Can't open %s.\n", filename);
		return;
	}

	/* Start read in the artifact list */
	while (fgets(buf, MAX_BUF, fp) != NULL)
	{
		if (*buf == '#')
		{
			continue;
		}

		if ((cp = strchr(buf, '\n')) != NULL)
		{
			*cp = '\0';
		}

		/* Skip blank lines. */
		if (*buf == '\0')
		{
			continue;
		}

		cp = buf;

		/* Skip blanks */
		while (*cp == ' ')
		{
			cp++;
		}

		/* We have a single artifact */
		if (!strncmp(cp, "Allowed", 7))
		{
			art = get_empty_artifact();
			nrofartifacts++;
			none_flag = 0;
			cp = strchr(cp, ' ') + 1;

			if (!strcmp(cp, "all"))
			{
				continue;
			}

			if (!strcmp(cp, "none"))
			{
				none_flag = 1;
				continue;
			}

			do
			{
				nrofallowedstr++;

				if ((next = strchr(cp, ',')) != NULL)
				{
					*(next++) = '\0';
				}

				tmp = (linked_char *) malloc(sizeof(linked_char));
				tmp->name = NULL;
				FREE_AND_COPY_HASH(tmp->name, cp);
				tmp->next = art->allowed;
				art->allowed = tmp;
			}
			while ((cp = next) != NULL);
		}
		else if (sscanf(cp, "t_style %d", &value))
		{
			art->t_style = value;
		}
		else if (sscanf(cp, "chance %d", &value))
		{
			art->chance = (uint16) value;
		}
		else if (sscanf(cp, "difficulty %d", &value))
		{
			art->difficulty = (uint8) value;
		}
		else if (!strncmp(cp, "artifact", 8))
		{
			FREE_AND_COPY_HASH(art->name, cp + 9);
		}
		/* Chain a default arch to this treasure */
		else if (!strncmp(cp, "def_arch", 8))
		{
			if ((atemp = find_archetype(cp + 9)) == NULL)
			{
				LOG(llevError, "ERROR: init_artifacts(): Can't find def_arch %s.\n", cp + 9);
			}

			/* Ok, we have a name and an archetype */

			/* Store the non fake archetype name */
			FREE_AND_COPY_HASH(art->def_at_name, cp + 9);
			/* Copy the default arch */
			memcpy(&art->def_at, atemp, sizeof(archetype));
			ADD_REF_NOT_NULL_HASH(art->def_at.clone.name);
			ADD_REF_NOT_NULL_HASH(art->def_at.clone.title);
			ADD_REF_NOT_NULL_HASH(art->def_at.clone.race);
			ADD_REF_NOT_NULL_HASH(art->def_at.clone.slaying);
			ADD_REF_NOT_NULL_HASH(art->def_at.clone.msg);
			art->def_at.clone.arch = &art->def_at;

			/* we patch this .clone object after Object read with the artifact data.
			 * in find_artifact, this archetype object will be returned. For the server,
			 * it will be the same as it comes from the archlist, defined in the arches.
			 * This will allow us the generate for every artifact a "default one" and we
			 * will have always a non-magical base for every artifact */
		}
		/* All text after Object is now like an arch file until an end comes */
		else if (!strncmp(cp, "Object", 6))
		{
			old_pos = ftell(fp);

			if (!load_object(fp, &(art->def_at.clone), NULL, LO_LINEMODE, MAP_STYLE))
			{
				LOG(llevError, "ERROR: init_artifacts(): Could not load object.\n");
			}

			if (!art->name)
			{
				LOG(llevError, "ERROR: init_artifacts(): Object %s has no arch id name\n", art->def_at.clone.name);
			}

			if (!art->def_at_name)
			{
				LOG(llevError, "ERROR: init_artifacts(): Artifact %s has no def arch\n", art->name);
			}

			/* Ok, now let's catch and copy the commands to our artifacts
			 * buffer. Let's do some file magic here - that's the easiest way. */
			file_pos = ftell(fp);

			if (fseek(fp, old_pos, SEEK_SET))
			{
				LOG(llevError, "ERROR: init_artifacts(): Could not fseek(fp, %ld, SEEK_SET).\n", old_pos);
			}

			/* The lex reader will bug when it don't get feed with a
			 * <text>+0x0a+0 string. So, we do it here and in the lex
			 * part we simply do a strlen and point to every part without
			 * copying it. */
			lcount = 0;

			while (fgets(buf, MAX_BUF - 3, fp))
			{
				strcpy(buf_text + lcount, buf);
				lcount += strlen(buf) + 1;

				if (ftell(fp) == file_pos)
				{
					break;
				}

				/* Should not possible. */
				if (ftell(fp) > file_pos)
				{
					LOG(llevError, "ERROR: init_artifacts(): fgets() read too much data! (%ld - %ld)\n", file_pos, ftell(fp));
				}
			}

			/* Now store the parse text in the artifacts list entry */
			if ((art->parse_text = malloc(lcount)) == NULL)
			{
				LOG(llevError, "ERROR: init_artifacts(): out of memory in ->parse_text (size %"FMT64U")\n", (uint64) lcount);
			}

			memcpy(art->parse_text, buf_text, lcount);

			/* Finally, change the archetype name of our fake arch to the
			 * fake arch name. Without it, treasures will get the
			 * original arch, not this. */
			FREE_AND_COPY_HASH(art->def_at.name, art->name);
			/* Now handle the <Allowed none> in the artifact to create
			 * unique items or add them to the given type list. */
			al = find_artifactlist(none_flag == 0 ? art->def_at.clone.type : -1);

			if (al == NULL)
			{
				al = get_empty_artifactlist();
				al->type = none_flag == 0 ? art->def_at.clone.type : -1;
				al->next = first_artifactlist;
				first_artifactlist = al;
			}

			art->next = al->items;
			al->items = art;
		}
		else
		{
			LOG(llevBug, "BUG: Unknown input in artifact file: %s\n", buf);
		}
	}

	close_and_delete(fp, comp);

	for (al = first_artifactlist; al != NULL; al = al->next)
	{
		for (art = al->items; art != NULL; art = art->next)
		{
			/* We don't use our unique artifacts as pick table */
			if (al->type == -1)
			{
				continue;
			}

			if (!art->chance)
			{
				LOG(llevBug, "BUG: Artifact with no chance: %s\n", art->name);
			}
			else
			{
				al->total_chance += art->chance;
			}
		}
	}

	LOG(llevDebug, "done.\n");
}

/**
 * Initialize global archetype pointers. */
void init_archetype_pointers()
{
	if (ring_arch_normal == NULL)
	{
		ring_arch_normal = find_archetype("ring_normal");
	}

	if (!ring_arch_normal)
	{
		LOG(llevBug, "BUG: Can't find 'ring_normal' arch (from artifacts)\n");
	}

	if (ring_arch == NULL)
	{
		ring_arch = find_archetype("ring_generic");
	}

	if (!ring_arch)
	{
		LOG(llevBug, "BUG: Can't find 'ring_generic' arch\n");
	}

	if (amulet_arch_normal == NULL)
	{
		amulet_arch_normal = find_archetype("amulet_normal");
	}

	if (!amulet_arch_normal)
	{
		LOG(llevBug, "BUG: Can't find 'amulet_normal' arch (from artifacts)\n");
	}

	if (amulet_arch == NULL)
	{
		amulet_arch = find_archetype("amulet_generic");
	}

	if (!amulet_arch)
	{
		LOG(llevBug, "BUG: Can't find 'amulet_generic' arch\n");
	}
}

/**
 * Allocate and return the pointer to an empty treasurelist structure.
 * @return New structure, blanked, never NULL. */
static treasurelist *get_empty_treasurelist()
{
	treasurelist *tl = (treasurelist *) malloc(sizeof(treasurelist));

	if (tl == NULL)
	{
		LOG(llevError, "ERROR: get_empty_treasurelist(): Out of memory.\n");
	}

	tl->name = NULL;
	tl->next = NULL;
	tl->items = NULL;
	/* -2 is the "unset" marker and will be virtually handled as 0 which
	 * can be overruled. */
	tl->t_style = T_STYLE_UNSET;
	tl->artifact_chance = ART_CHANCE_UNSET;
	tl->chance_fix = CHANCE_FIX;
	tl->total_chance = 0;

	return tl;
}

/**
 * Allocate and return the pointer to an empty treasure structure.
 * @return New structure, blanked, never NULL. */
static treasure *get_empty_treasure()
{
	treasure *t = (treasure *) malloc(sizeof(treasure));

	if (t == NULL)
	{
		LOG(llevError, "ERROR: get_empty_treasure(): Out of memory.\n");
	}

	t->change_arch.item_race = -1;
	t->change_arch.name = NULL;
	t->change_arch.slaying = NULL;
	t->change_arch.title = NULL;
	/* -2 is the "unset" marker and will be virtually handled as 0 which
	 * can be overruled. */
	t->t_style = T_STYLE_UNSET;
	t->change_arch.material = -1;
	t->change_arch.material_quality = -1;
	t->change_arch.material_range = -1;
	t->change_arch.quality = -1;
	t->change_arch.quality_range = -1;
	t->chance_fix = CHANCE_FIX;
	t->item = NULL;
	t->name = NULL;
	t->next = NULL;
	t->next_yes = NULL;
	t->next_no = NULL;
	t->artifact_chance = ART_CHANCE_UNSET;
	t->chance = 100;
	t->magic_fix = 0;
	t->difficulty = 0;
	t->magic_chance = 3;
	t->magic = 0;
	t->nrof = 0;

	return t;
}

/**
 * Searches for the given treasurelist in the globally linked list of
 * treasure lists which has been built by load_treasures().
 * @param name Treasure list to search for. */
treasurelist *find_treasurelist(const char *name)
{
	const char *tmp = find_string(name);
	treasurelist *tl;

	/* Special cases - randomitems of none is to override default.  If
	 * first_treasurelist is null, it means we are on the first pass of
	 * of loading archetyps, so for now, just return - second pass will
	 * init these values. */
	if (!strcmp(name, "none") || !first_treasurelist)
	{
		return NULL;
	}

	if (tmp != NULL)
	{
		for (tl = first_treasurelist; tl != NULL; tl = tl->next)
		{
			if (tmp == tl->name)
			{
				return tl;
			}
		}
	}

	LOG(llevBug, "Bug: Couldn't find treasurelist %s\n", name);
	return NULL;
}

/**
 * This is similar to the old generate treasure function. However, it
 * instead takes a treasurelist. It is really just a wrapper around
 * create_treasure(). We create a dummy object that the treasure gets
 * inserted into, and then return that treasure.
 * @param t Treasure list to generate from.
 * @param difficulty Treasure difficulty.
 * @return Generated treasure. Can be NULL if no suitable treasure was
 * found. */
object *generate_treasure(treasurelist *t, int difficulty, int a_chance)
{
	object *ob = get_object(), *tmp;

	create_treasure(t, ob, 0, difficulty, t->t_style, a_chance, 0, NULL);

	/* Don't want to free the object we are about to return */
	tmp = ob->inv;

	/* Remove from inv - no move off */
	if (tmp != NULL)
	{
		remove_ob(tmp);
	}

	if (ob->inv)
	{
		LOG(llevError, "ERROR: generate_treasure(): Created multiple objects.\n");
	}

	return tmp;
}

/**
 * This calls the appropriate treasure creation function.
 * @param t What to generate.
 * @param op For who to generate the treasure.
 * @param flag Combination of @ref GT_xxx values.
 * @param difficulty Map difficulty.
 * @param t_style Treasure style.
 * @param a_chance Artifact chance.
 * @param tries To avoid infinite recursion.
 * @param arch_change Arch change. */
void create_treasure(treasurelist *t, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *arch_change)
{
	if (tries++ > 100)
	{
		LOG(llevDebug, "DEBUG: create_treasure(): tries >100 for t-list %s.", t->name ? t->name : "<noname>");
		return;
	}

	if (t->t_style != T_STYLE_UNSET)
	{
		t_style = t->t_style;
	}

	if (t->artifact_chance != ART_CHANCE_UNSET)
	{
		a_chance = t->artifact_chance;
	}

	if (t->total_chance)
	{
		create_one_treasure(t, op, flag, difficulty, t_style, a_chance, tries, arch_change);
	}
	else
	{
		create_all_treasures(t->items, op, flag, difficulty, t_style, a_chance, tries, arch_change);
	}
}

/**
 * Creates all the treasures.
 * @param t What to generate.
 * @param op For who to generate the treasure.
 * @param flag Combination of @ref GT_xxx values.
 * @param difficulty Map difficulty.
 * @param t_style Treasure style.
 * @param a_chance Artifact chance.
 * @param tries To avoid infinite recursion.
 * @param change_arch Arch change. */
static void create_all_treasures(treasure *t, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *change_arch)
{
	object *tmp;

	if (t->t_style != T_STYLE_UNSET)
	{
		t_style = t->t_style;
	}

	if (t->artifact_chance != ART_CHANCE_UNSET)
	{
		a_chance = t->artifact_chance;
	}

	if ((t->chance_fix != CHANCE_FIX && rndm_chance(t->chance_fix)) || (int) t->chance >= 100 || (rndm(1, 100) < (int) t->chance))
	{
		if (t->name)
		{
			if (t->name != shstr_cons.NONE && difficulty >= t->difficulty)
			{
				create_treasure(find_treasurelist(t->name), op, flag, difficulty, t_style, a_chance, tries, change_arch ? change_arch : &t->change_arch);
			}
		}
		else if (difficulty >= t->difficulty)
		{
			if (IS_SYS_INVISIBLE(&t->item->clone) || !(flag & GT_INVISIBLE))
			{
				if (t->item->clone.type != WEALTH)
				{
					tmp = arch_to_object(t->item);

					if (t->nrof && tmp->nrof <= 1)
					{
						tmp->nrof = rndm(1, t->nrof);
					}

					/* Ret 1 = artifact is generated - don't overwrite anything here */
					set_material_real(tmp, change_arch ? change_arch : &t->change_arch);

					if (!fix_generated_item(&tmp, op, difficulty, a_chance, t_style, t->magic, t->magic_fix, t->magic_chance, flag))
					{
						change_treasure(change_arch ? change_arch : &t->change_arch, tmp);
					}

					put_treasure(tmp, op, flag);

					/* If treasure is "identified", created items are too */
					if (op->type == TREASURE && QUERY_FLAG(op, FLAG_IDENTIFIED))
					{
						SET_FLAG(tmp, FLAG_IDENTIFIED);
					}
				}
				/* We have a wealth object - expand it to real money */
				else
				{
					/* If t->magic is != 0, that's our value - if not use default setting */
					int i, value = t->magic ? t->magic : t->item->clone.value;

					value *= (difficulty / 2) + 1;

					/* So we have 80% to 120% of the fixed value */
					value = (int) ((float) value * 0.8f + (float) value * ((float) rndm(1, 40) / 100.0f));

					for (i = 0; i < NUM_COINS; i++)
					{
						if (value / coins_arch[i]->clone.value > 0)
						{
							tmp = get_object();
							copy_object(&coins_arch[i]->clone, tmp, 0);
							tmp->nrof = value / tmp->value;
							value -= tmp->nrof * tmp->value;
							put_treasure(tmp, op, flag);
						}
					}
				}
			}
		}

		if (t->next_yes != NULL)
		{
			create_all_treasures(t->next_yes, op, flag, difficulty, (t->next_yes->t_style == T_STYLE_UNSET) ? t_style : t->next_yes->t_style, a_chance, tries, change_arch);
		}
	}
	else if (t->next_no != NULL)
	{
		create_all_treasures(t->next_no, op, flag, difficulty, (t->next_no->t_style == T_STYLE_UNSET) ? t_style : t->next_no->t_style, a_chance, tries, change_arch);
	}

	if (t->next != NULL)
	{
		create_all_treasures(t->next, op, flag, difficulty, (t->next->t_style == T_STYLE_UNSET) ? t_style : t->next->t_style, a_chance, tries, change_arch);
	}
}

/**
 * Creates one treasure from the list.
 * @param tl What to generate.
 * @param op For who to generate the treasure.
 * @param flag Combination of @ref GT_xxx values.
 * @param difficulty Map difficulty.
 * @param t_style Treasure style.
 * @param a_chance Artifact chance.
 * @param tries To avoid infinite recursion.
 * @param change_arch Arch change.
 * @todo Get rid of the goto. */
static void create_one_treasure(treasurelist *tl, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *change_arch)
{
	int value, diff_tries = 0;
	treasure *t;
	object *tmp;

	if (tries++ > 100)
	{
		return;
	}

	/* Well, at some point we should rework this whole system... */
create_one_treasure_again_jmp:
	if (diff_tries > 10)
	{
		return;
	}

	value = rndm(1, tl->total_chance) - 1;

	for (t = tl->items; t != NULL; t = t->next)
	{
		/* chance_fix will overrule the normal chance stuff!. */
		if (t->chance_fix != CHANCE_FIX)
		{
			if (rndm_chance(t->chance_fix))
			{
				/* Only when allowed, we go on! */
				if (difficulty >= t->difficulty)
				{
					value = 0;
					break;
				}

				/* Ok, difficulty is bad let's try again or break! */
				if (tries++ > 100)
				{
					return;
				}

				diff_tries++;
				goto create_one_treasure_again_jmp;
			}

			if (!t->chance)
			{
				continue;
			}
		}

		value -= t->chance;

		/* We got one! */
		if (value <= 0)
		{
			/* Only when allowed, we go on! */
			if (difficulty >= t->difficulty)
			{
				break;
			}

			/* Ok, difficulty is bad let's try again or break! */
			if (tries++ > 100)
			{
				return;
			}

			diff_tries++;
			goto create_one_treasure_again_jmp;
		}
	}

	if (t->t_style != T_STYLE_UNSET)
	{
		t_style = t->t_style;
	}

	if (t->artifact_chance != ART_CHANCE_UNSET)
	{
		a_chance = t->artifact_chance;
	}

	if (!t || value > 0)
	{
		LOG(llevBug, "BUG: create_one_treasure: got null object or not able to find treasure - tl:%s op:%s\n", tl ? tl->name : "(null)", op ? op->name : "(null)");
		return;
	}

	if (t->name)
	{
		if (t->name == shstr_cons.NONE)
		{
			return;
		}

		if (difficulty >= t->difficulty)
		{
			create_treasure(find_treasurelist(t->name), op, flag, difficulty, t_style, a_chance, tries, change_arch);
		}
		else if (t->nrof)
		{
			create_one_treasure(tl, op, flag, difficulty, t_style, a_chance, tries, change_arch);
		}

		return;
	}

	if (IS_SYS_INVISIBLE(&t->item->clone) || flag != GT_INVISIBLE)
	{
		if (t->item->clone.type != WEALTH)
		{
			tmp = arch_to_object(t->item);

			if (t->nrof && tmp->nrof <= 1)
			{
				tmp->nrof = rndm(1, t->nrof);
			}

			set_material_real(tmp, change_arch ? change_arch : &t->change_arch);

			if (!fix_generated_item(&tmp, op, difficulty, a_chance, (t->t_style == T_STYLE_UNSET) ? t_style : t->t_style, t->magic, t->magic_fix, t->magic_chance, flag))
			{
				change_treasure(change_arch ? change_arch : &t->change_arch, tmp);
			}

			put_treasure(tmp, op, flag);

			/* If treasure is "identified", created items are too */
			if (op->type == TREASURE && QUERY_FLAG(op, FLAG_IDENTIFIED))
			{
				SET_FLAG(tmp, FLAG_IDENTIFIED);
			}
		}
		/* We have a wealth object - expand it to real money */
		else
		{
			/* If t->magic is != 0, that's our value - if not use default setting */
			int i, value = t->magic ? t->magic : t->item->clone.value;

			value *= difficulty;

			/* So we have 80% to 120% of the fixed value */
			value = (int) ((float) value * 0.8f + (float) value * ((float) rndm(1, 40) / 100.0f));

			for (i = 0; i < NUM_COINS; i++)
			{
				if (value / coins_arch[i]->clone.value > 0)
				{
					tmp = get_object();
					copy_object(&coins_arch[i]->clone, tmp, 0);
					tmp->nrof = value / tmp->value;
					value -= tmp->nrof * tmp->value;
					put_treasure(tmp, op, flag);
				}
			}
		}
	}
}

/**
 * Inserts generated treasure where it should go.
 * @param op Treasure just generated.
 * @param creator For which object the treasure is being generated.
 * @param flags Combination of @ref GT_xxx values. */
static void put_treasure(object *op, object *creator, int flags)
{
	if (flags & GT_ENVIRONMENT)
	{
		op->x = creator->x;
		op->y = creator->y;
		insert_ob_in_map(op, creator->map, op, INS_NO_MERGE | INS_NO_WALK_ON);
	}
	else
	{
		object *tmp;

		op = insert_ob_in_ob(op, creator);

		if ((flags & GT_UPDATE_INV) && (tmp = is_player_inv(creator)) != NULL)
		{
			esrv_send_item(tmp, op);
		}
	}
}

/**
 * If there are change_xxx commands in the treasure, we include the
 * changes in the generated object.
 * @param ca Arch to change to.
 * @param op Actual generated treasure. */
static void change_treasure(struct _change_arch *ca, object *op)
{
	if (ca->name)
	{
		FREE_AND_COPY_HASH(op->name, ca->name);
	}

	if (ca->title)
	{
		FREE_AND_COPY_HASH(op->title, ca->title);
	}

	if (ca->slaying)
	{
		FREE_AND_COPY_HASH(op->slaying, ca->slaying);
	}
}

/**
 * Sets a random magical bonus in the given object based upon the given
 * difficulty, and the given max possible bonus.
 *
 * Item will be cursed if magic is negative.
 * @param difficulty Difficulty we want the item to be.
 * @param op The object.
 * @param max_magic What should be the maximum magic of the item.
 * @param fix_magic Fixed value of magic for the object.
 * @param chance_magic Chance to get a magic bonus.
 * @param flags Combination of @ref GT_xxx flags. */
static void set_magic(int difficulty, object *op, int max_magic, int fix_magic, int chance_magic, int flags)
{
	int i;

	/* If we have a fixed value, force it */
	if (fix_magic)
	{
		i = fix_magic;
	}
	else
	{
		i = 0;

		/* chance_magic 0 means always no magic bonus */
		if (rndm(1, 100) <= chance_magic || rndm(1, 200) <= difficulty)
		{
			i = rndm(1, abs(max_magic));

			if (max_magic < 0)
			{
				i = -i;
			}
		}
	}

	if ((flags & GT_ONLY_GOOD) && i < 0)
	{
		i = 0;
	}

	set_abs_magic(op, i);

	if (i < 0)
	{
		SET_FLAG(op, FLAG_CURSED);
	}

	if (i != 0)
	{
		SET_FLAG(op, FLAG_IS_MAGICAL);
	}
}

/**
 * Sets magical bonus in an object, and recalculates the effect on the
 * armour variable, and the effect on speed of armour.
 *
 * This function doesn't work properly, should add use of archetypes to
 * make it truly absolute.
 * @param op Object we're modifying.
 * @param magic Magic modifier. */
void set_abs_magic(object *op, int magic)
{
	if (!magic)
	{
		return;
	}

	SET_FLAG(op, FLAG_IS_MAGICAL);

	op->magic = magic;

	if (op->arch)
	{
		if (magic == 1)
		{
			op->value += 5300;
		}
		else if (magic == 2)
		{
			op->value += 12300;
		}
		else if (magic == 3)
		{
			op->value += 24300;
		}
		else if (magic == 4)
		{
			op->value += 52300;
		}
		else
		{
			op->value += 88300;
		}

		if (op->type == ARMOUR)
		{
			ARMOUR_SPEED(op) = (ARMOUR_SPEED(&op->arch->clone) * (100 + magic * 10)) / 100;
		}

		/* You can't just check the weight always */
		if (magic < 0 && !rndm(0, 2))
		{
			magic = (-magic);
		}

		op->weight = (op->arch->clone.weight * (100 - magic * 10)) / 100;
	}
	else
	{
		if (op->type == ARMOUR)
		{
			ARMOUR_SPEED(op) = (ARMOUR_SPEED(op) * (100 + magic * 10)) / 100;
		}

		/* You can't just check the weight always */
		if (magic < 0 && !rndm(0, 2))
		{
			magic = (-magic);
		}

		op->weight = (op->weight * (100 - magic * 10)) / 100;
	}
}

/**
 * Randomly adds one magical ability to the given object.
 *
 * Modified for Partial Resistance in many ways:
 *
 * -# Since rings can have multiple bonuses, if the same bonus is rolled
 *    again, increase it - the bonuses now stack with other bonuses
 *    previously rolled and ones the item might natively have.
 * -# Add code to deal with new PR method.
 *
 * Changes the item's value.
 * @param op Ring or amulet to change.
 * @param bonus Bonus to add to item.
 * @param level Level.
 * @return 1.
 * @todo Get rid of the gotos in here. */
static int set_ring_bonus(object *op, int bonus, int level)
{
	int tmp, r, off;
	off = (level >= 50 ? 1 : 0) + (level >= 60 ? 1 : 0) + (level >= 70 ? 1 : 0) + (level >= 80 ? 1 : 0);

	/* Let's repeat, too lazy for a loop */
set_ring_bonus_jump1:
	r = RANDOM() % (bonus > 0 ? 25 : 13);

	SET_FLAG(op, FLAG_IS_MAGICAL);

	if (op->type == AMULET)
	{
		if (!(RANDOM() % 21))
		{
			r = 20 + RANDOM() % 2;
		}
		else if (!(RANDOM() % 20))
		{
			tmp = RANDOM() % 3;

			if (tmp == 2)
			{
				r = 0;
			}
			else if (!tmp)
			{
				r = 11;
			}
			else
			{
				r = 12;
			}
		}
		else if (RANDOM() & 2)
		{
			r = 10;
		}
		else
		{
			r = 13 + RANDOM() % 7;
		}
	}

	switch (r % 25)
	{
		/* We are creating hp stuff! */
		case 0:
			tmp = 5;

			if (level < 5)
			{
				tmp += RANDOM() % 10;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.3f);
				}
			}
			else if (level < 10)
			{
				tmp += 10 + RANDOM() % 10;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.32f);
				}
			}
			else if (level < 15)
			{
				tmp += 15 + RANDOM() % 20;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.34f);
				}
			}
			else if (level < 20)
			{
				tmp += 20 + RANDOM() % 21;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.36f);
				}
			}
			else if (level < 25)
			{
				tmp += 25 + RANDOM() % 23;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.38f);
				}
			}
			else if (level < 30)
			{
				tmp += 30 + RANDOM() % 25;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.4f);
				}
			}
			else if (level < 40)
			{
				tmp += 40 + RANDOM() % 30;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.42f);
				}
			}
			else
			{
				tmp += (int) ((double) level * 0.65) + 50 + RANDOM() % 40;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.44f);
				}
			}

			if (bonus < 0)
			{
				tmp = -tmp;
			}
			else
			{
				op->item_level = (int) ((double) level * (0.5 + ((double) (RANDOM() % 40) / 100.0)));
			}

			op->stats.maxhp = tmp;
			break;

		/* Stats */
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			set_attr_value(&op->stats, r, (sint8) (bonus + get_attr_value(&op->stats, r)));
			break;

		case 7:
			op->stats.dam += bonus;

			if (bonus > 0 && (RANDOM() % 20 > 16 ? 1 : 0))
			{
				op->value = (int) ((float) op->value * 1.3f);
				op->stats.dam++;
			}

			break;

		case 8:
			op->stats.wc += bonus;

			if (bonus > 0 && (RANDOM() % 20 > 16 ? 1 : 0))
			{
				op->value = (int) ((float) op->value * 1.3f);
				op->stats.wc++;
			}

			break;

		/* Hunger/sustenance */
		case 9:
			op->stats.food += bonus;

			if (bonus > 0 && (RANDOM() % 20 > 16 ? 1 : 0))
			{
				op->value = (int) ((float) op->value * 1.2f);
				op->stats.food++;
			}

			break;

		case 10:
			op->stats.ac += bonus;

			if (bonus > 0 && (RANDOM() % 20 > 16 ? 1 : 0))
			{
				op->value = (int) ((float) op->value * 1.3f);
				op->stats.ac++;
			}

			break;

		case 11:
		case 12:
			if (!RANDOM() % 3)
			{
				goto make_prot_items;
			}

			tmp = 3;

			if (level < 5)
			{
				tmp += RANDOM() % 3;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.3f);
				}
			}
			else if (level < 10)
			{
				tmp += 3 + RANDOM() % 4;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.32f);
				}
			}
			else if (level < 15)
			{
				tmp += 4 + RANDOM() % 6;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.34f);
				}
			}
			else if (level < 20)
			{
				tmp += 6 + RANDOM() % 8;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.36f);
				}
			}
			else if (level < 25)
			{
				tmp += 8 + RANDOM() % 10;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.38f);
				}
			}
			else if (level < 33)
			{
				tmp += 10 + RANDOM() % 12;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.4f);
				}
			}
			else if (level < 44)
			{
				tmp += 15 + RANDOM() % 15;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.42f);
				}
			}
			else
			{
				tmp += (int) ((double) level * 0.53) + 20 + RANDOM() % 20;

				if (bonus > 0)
				{
					op->value = (int) ((float) op->value * 1.44f);
				}
			}

			if (bonus < 0)
			{
				tmp = -tmp;
			}
			else
			{
				op->item_level = (int) ((double) level * (0.5 + ((double) (RANDOM() % 40) / 100.0)));
			}

			op->stats.maxsp = tmp;
			break;

		case 13:
make_prot_items:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:
		case 19:
		{
			int b = 5 + FABS(bonus), val, protect = RANDOM() % (LAST_PROTECTION - 4 + off);

			/* Roughly generate a bonus between 100 and 35 (depending on the bonus) */
			val = 10 + RANDOM() % b + RANDOM() % b + RANDOM() % b + RANDOM() % b;

			/* Cursed items need to have higher negative values to equal
			 * out with positive values for how protections work out. Put
			 * another little random element in since that they don't
			 * always end up with even values. */
			if (bonus < 0)
			{
				val = 2 * -val - RANDOM() % b;
			}

			/* Upper limit */
			if (val > 35)
			{
				val = 35;
			}

			b = 0;

			while (op->protection[protect] != 0)
			{
				/* Not able to find a free protection */
				if (b++ >= 4)
				{
					goto set_ring_bonus_jump1;
				}

				protect = RANDOM() % (LAST_PROTECTION - 4 + off);
			}

			op->protection[protect] = val;
			break;
		}

		case 20:
			if (op->type == AMULET)
			{
				SET_FLAG(op, FLAG_REFL_SPELL);
				op->value *= 11;
			}
			else
			{
				/* Regenerate hit points */
				op->stats.hp = 1;
				op->value = (int) ((float) op->value * 1.3f);
			}

			break;

		case 21:
			if (op->type == AMULET)
			{
				SET_FLAG(op, FLAG_REFL_MISSILE);
				op->value *= 9;
			}
			else
			{
				/* Regenerate spell points */
				op->stats.sp = 1;
				op->value = (int) ((float) op->value * 1.35f);
			}
			break;

		default:
			if (!bonus)
			{
				bonus = 1;
			}

			/* Speed! */
			op->stats.exp += bonus;
			op->value = (int) ((float) op->value * 1.4f);
			break;
	}

	if (bonus > 0)
	{
		op->value = (int) ((float) op->value * 2.0f * (float) bonus);
	}
	else
	{
		op->value = -(op->value * 2 * bonus) / 2;
	}

	/* Check possible overflow */
	if (op->value < 0)
	{
		op->value = 0;
	}

	return 1;
}

/**
 * Calculate the item power of the given ring/amulet.
 * @param ob The ring/amulet. */
static void set_ring_item_power(object *ob)
{
	int tmp, i;

	if (ob->stats.maxhp > 0)
	{
		ob->item_power += ob->stats.maxhp / 50;
	}

	if (ob->stats.maxsp > 0)
	{
		ob->item_power += ob->stats.maxsp / 50;
	}

	if (ob->stats.exp >= 2)
	{
		ob->item_power += ob->stats.exp - 1;
	}

	if (ob->stats.ac > 0)
	{
		ob->item_power += ob->stats.ac;
	}

	if (ob->stats.dam > 0)
	{
		ob->item_power += ob->stats.dam;
	}

	if (ob->stats.wc > 0)
	{
		ob->item_power += ob->stats.wc;
	}

	if (QUERY_FLAG(ob, FLAG_REFL_MISSILE))
	{
		ob->item_power++;
	}

	if (QUERY_FLAG(ob, FLAG_REFL_SPELL))
	{
		ob->item_power += 2;
	}

	if (ob->stats.hp > 0)
	{
		ob->item_power++;
	}

	if (ob->stats.sp > 0)
	{
		ob->item_power += 2;
	}

	tmp = 0;

	for (i = 0; i < NROFATTACKS; i++)
	{
		tmp += ob->protection[i];
	}

	if (tmp > 0)
	{
		ob->item_power += (tmp + 10) / 20;
	}

	for (i = 0; i < NUM_STATS; i++)
	{
		tmp = get_attr_value(&ob->stats, i);

		if (tmp >= 2)
		{
			ob->item_power += tmp - 1;
		}
	}
}

/**
 * Will return a random number between 0 and 4.
 *
 * It is only used in fix_generated_treasure() to set bonuses on rings
 * and amulets. Another scheme is used to calculate the magic of weapons
 * and armours.
 * @param diff Any value above 2. The higher the value, the higher is the
 * chance of returning a low number.
 * @return The random number. */
static int get_magic(int diff)
{
	int i;

	if (diff < 3)
	{
		diff = 3;
	}

	for (i = 0; i < 4; i++)
	{
		if (rndm(0, diff - 1))
		{
			return i;
		}
	}

	return 4;
}

/**
 * Get a random spell from the spell list.
 *
 * Used for item generation which uses spells like wands, rods, etc.
 * @param level Level of the spell.
 * @param flags @ref SPELL_USE_xxx to check for.
 * @return SP_NO_SPELL if no valid spell matches, ID of the spell
 * otherwise. */
static int get_random_spell(int level, int flags)
{
	int i, num_spells = 0, possible_spells[NROFREALSPELLS];

	/* Collect the list of spells we can choose from. */
	for (i = 0; i < NROFREALSPELLS; i++)
	{
		if (level >= spells[i].level && spells[i].spell_use & flags)
		{
			possible_spells[num_spells] = i;
			num_spells++;
		}
	}

	/* If we found any spells we can use, select randomly. */
	if (num_spells)
	{
		return possible_spells[rndm(1, num_spells) - 1];
	}

	/* If we are here, there is no fitting spell. */
	return SP_NO_SPELL;
}

/**
 * Assign a random slaying race to an object, for weapons, arrows
 * and such.
 * @param op Object. */
static void add_random_race(object *op)
{
	ob_race *race = race_get_random();

	if (race)
	{
		FREE_AND_COPY_HASH(op->slaying, race->name);
	}
}

#define DICE2 (get_magic(2) == 2 ? 2 : 1)

/**
 * This is called after an item is generated, in order to set it up
 * right. This produced magical bonuses, puts spells into
 * scrolls/books/wands, makes it unidentified, hides the value, etc.
 * @param op_ptr Object to fix.
 * @param creator For who op was created. Can be NULL.
 * @param difficulty Difficulty level.
 * @param a_chance Artifact chance.
 * @param t_style Treasure style.
 * @param max_magic Maximum magic for the item.
 * @param fix_magic Fixed magic value.
 * @param chance_magic Chance of magic.
 * @param flags One of @ref GT_xxx */
int fix_generated_item(object **op_ptr, object *creator, int difficulty, int a_chance, int t_style, int max_magic, int fix_magic, int chance_magic, int flags)
{
	/* Just to make things easy */
	object *op = *op_ptr;
	int temp, retval = 0, was_magic = op->magic;
	int too_many_tries = 0, is_special = 0;

	/* Safety and to prevent polymorphed objects giving attributes */
	if (!creator || creator->type == op->type)
	{
		creator = op;
	}

	if (difficulty < 1)
	{
		difficulty = 1;
	}

	if (op->type != POTION && op->type != SCROLL && op->type != FOOD)
	{
		if ((!op->magic && max_magic) || fix_magic)
		{
			set_magic(difficulty, op, max_magic, fix_magic, chance_magic, flags);
		}

		if (a_chance != 0)
		{
			if ((!was_magic && rndm_chance(CHANCE_FOR_ARTIFACT)) || op->type == HORN || difficulty >= 999 || rndm(1, 100) <= a_chance)
			{
				retval = generate_artifact(op, difficulty, t_style, a_chance);
			}
		}
	}

	/* Only modify object if not special */
	if (!op->title || op->type == RUNE)
	{
		switch (op->type)
		{
			/* We create scrolls now in artifacts file too */
			case SCROLL:
				while (op->stats.sp == SP_NO_SPELL)
				{
					generate_artifact(op, difficulty, t_style, 100);

					if (too_many_tries++ > 3)
					{
						break;
					}
				}

				/* Ok, forget it... */
				if (op->stats.sp == SP_NO_SPELL)
				{
					break;
				}

				/* Marks as magical */
				SET_FLAG(op, FLAG_IS_MAGICAL);
				/* Charges */
				op->stats.food = rndm(1, spells[op->stats.sp].charges);
				temp = (((difficulty * 100) - (difficulty * 20)) + (difficulty * rndm(0, 34))) / 100;

				if (temp < 1)
				{
					temp = 1;
				}
				else if (temp > MAXLEVEL)
				{
					temp = MAXLEVEL;
				}

				op->level = temp;

				if (temp < spells[op->stats.sp].level)
				{
					temp = spells[op->stats.sp].level;
				}

				break;

			case POTION:
			{
				/* Balm */
				if (!op->sub_type)
				{
					if ((op->stats.sp = get_random_spell(difficulty, SPELL_USE_BALM)) == SP_NO_SPELL)
					{
						break;
					}

					SET_FLAG(op, FLAG_IS_MAGICAL);
					op->value = (int) (150.0f * spells[op->stats.sp].value_mul);
				}
				/* Dust */
				else if (op->sub_type > 128)
				{
					if ((op->stats.sp = get_random_spell(difficulty, SPELL_USE_DUST)) == SP_NO_SPELL)
					{
						break;
					}

					SET_FLAG(op, FLAG_IS_MAGICAL);
					op->value = (int) (125.0f * spells[op->stats.sp].value_mul);
				}
				else
				{
					while (!(is_special = special_potion(op)) && op->stats.sp == SP_NO_SPELL)
					{
						generate_artifact(op, difficulty, t_style, 100);

						if (too_many_tries++ > 3)
						{
							goto jump_break1;
						}
					}
				}

				temp = (((difficulty * 100) - (difficulty * 20)) + (difficulty * rndm(0, 34))) / 100;

				if (temp < 1)
				{
					temp = 1;
				}
				else if (temp > MAXLEVEL)
				{
					temp = MAXLEVEL;
				}

				if (!is_special && temp < spells[op->stats.sp].level)
				{
					temp = spells[op->stats.sp].level;
				}

				op->level = temp;

				/* Chance to make special potions damned or cursed. The
				 * chance is somewhat high to make the game more
				 * difficult. Applying this potions without identify is a
				 * great risk! */
				if (is_special && !(flags & GT_ONLY_GOOD))
				{
					if (rndm_chance(2))
					{
						SET_FLAG(op, (rndm_chance(2) ? FLAG_CURSED : FLAG_DAMNED));
					}
				}

jump_break1:
				break;
			}

			case AMULET:
				/* Since it's not just decoration */
				if (op->arch == amulet_arch)
				{
					op->value *= 5;
				}

			case RING:
				if (op->arch == NULL)
				{
					remove_ob(op);
					*op_ptr = op = NULL;
					break;
				}

				/* It's a special artifact! For these items, there is no point
				 * carrying on, as the next bit is for regular rings only. */
				if (op->arch != ring_arch && op->arch != amulet_arch)
				{
					break;
				}

				/* We have no special ring or amulet - now we create one. We first
				 * get us a value, material and face changed prototype.
				 * Then we cast the powers over it. */

				/* This is called before we inserted it in the map or elsewhere */
				if (!QUERY_FLAG(op, FLAG_REMOVED))
				{
					remove_ob(op);
				}

				/* Here we give the ring or amulet a random material.
				 * First we use a special arch for this. Only this archtype is
				 * allowed to be masked with a special material artifact. */
				if (op->arch == ring_arch)
				{
					*op_ptr = op = arch_to_object(ring_arch_normal);
				}
				else
				{
					*op_ptr = op = arch_to_object(amulet_arch_normal);
				}

				generate_artifact(op, difficulty, t_style, 99);

				/* Now we add the random boni/mali to the item */
				if (!(flags & GT_ONLY_GOOD) && rndm_chance(4))
				{
					SET_FLAG(op, FLAG_CURSED);
				}

				set_ring_bonus(op, QUERY_FLAG(op, FLAG_CURSED) ? -DICE2 : DICE2, difficulty);

				if (op->type == RING)
				{
					if (rndm_chance(4))
					{
						int d = (!rndm_chance(2) || QUERY_FLAG(op, FLAG_CURSED)) ? -DICE2 : DICE2;

						if (set_ring_bonus(op, d, difficulty))
						{
							op->value = (int) ((float) op->value * 1.95f);
						}

						if (rndm_chance(4))
						{
							int d = (!rndm_chance(3) || QUERY_FLAG(op, FLAG_CURSED)) ? -DICE2 : DICE2;

							if (set_ring_bonus(op, d, difficulty))
							{
								op->value = (int) ((float) op->value * 1.95f);
							}
						}
					}
				}

				set_ring_item_power(op);

				break;

			case BOOK:
				/* Is it an empty book? If yes let's make a special msg
				 * for it, and tailor its properties based on the creator
				 * and/or map level we found it on. */
				if (!op->msg && !rndm_chance(10))
				{
					/* Set the book level properly. */
					if (creator->level == 0 || IS_LIVE(creator))
					{
						object *ob;

						for (ob = creator; ob && ob->env; ob = ob->env)
						{
						}

						if (ob->map && ob->map->difficulty)
						{
							op->level = MIN(rndm(1, ob->map->difficulty) + rndm(0, 2), MAXLEVEL);
						}
						else
						{
							op->level = rndm(1, 20);
						}
					}
					else
					{
						op->level = rndm(1, creator->level);
					}

					tailor_readable_ob(op, (creator && creator->stats.sp) ? creator->stats.sp : -1);
					generate_artifact(op, 1, T_STYLE_UNSET, 100);

					/* Books with info are worth more! */
					if (op->msg && strlen(op->msg) > 0)
					{
						op->value *= ((op->level > 10 ? op->level : (op->level + 1) / 2) * ((strlen(op->msg) / 250) + 1));
					}

					/* For library, chained books! */
					if (creator->type != MONSTER && QUERY_FLAG(creator, FLAG_NO_PICK))
					{
						SET_FLAG(op, FLAG_NO_PICK);
					}

					/* For check_inv floors */
					if (creator->slaying && !op->slaying)
					{
						FREE_AND_COPY_HASH(op->slaying, creator->slaying);
					}

					/* Add exp so reading it gives xp (once) */
					op->stats.exp = op->value > 10000 ? op->value / 5 : op->value / 10;
				}

				break;

			case SPELLBOOK:
				LOG(llevDebug, "DEBUG: fix_generated_item(): called for disabled object SPELLBOOK (%s)\n", query_name(op, NULL));
				break;

			case WAND:
				if ((op->stats.sp = get_random_spell(difficulty, SPELL_USE_WAND)) == SP_NO_SPELL)
				{
					break;
				}

				/* Marks as magical */
				SET_FLAG(op, FLAG_IS_MAGICAL);
				/* Charges */
				op->stats.food = rndm(1, spells[op->stats.sp].charges) + 12;

				temp = (((difficulty * 100) - (difficulty * 20)) + (difficulty * rndm(0, 34))) / 100;

				if (temp < 1)
				{
					temp = 1;
				}
				else if (temp > MAXLEVEL)
				{
					temp = MAXLEVEL;
				}

				if (temp < spells[op->stats.sp].level)
				{
					temp = spells[op->stats.sp].level;
				}

				op->level = temp;
				op->value = (int) (16.3f * spells[op->stats.sp].value_mul);

				break;

			case HORN:
			case ROD:
				if ((op->stats.sp = get_random_spell(difficulty, op->type == HORN ? SPELL_USE_HORN : SPELL_USE_ROD)) == SP_NO_SPELL)
				{
					break;
				}

				/* Marks as magical */
				SET_FLAG(op, FLAG_IS_MAGICAL);

				if (op->stats.maxhp)
				{
					op->stats.maxhp += rndm(1, op->stats.maxhp);
				}

				op->stats.hp = op->stats.maxhp;
				temp = (((difficulty * 100) - (difficulty * 20)) + (difficulty * rndm(0, 34))) / 100;

				if (temp < 1)
				{
					temp = 1;
				}
				else if (temp > MAXLEVEL)
				{
					temp = MAXLEVEL;
				}

				op->level = temp;

				if (temp < spells[op->stats.sp].level)
				{
					temp = spells[op->stats.sp].level;
				}

				op->value = (int) (1850.0f * spells[op->stats.sp].value_mul);

				break;

			case RUNE:
				/* Artifact AND normal treasure runes! */
				trap_adjust(op, difficulty);
				break;

			/* Generate some special food */
			case FOOD:
				if (rndm_chance(4))
				{
					generate_artifact(op, difficulty, T_STYLE_UNSET, 100);
				}

				/* Small chance to become cursed food */
				if (!(flags & GT_ONLY_GOOD) && rndm_chance(20))
				{
					int strong_curse = rndm(0, 1), i;

					SET_FLAG(op, FLAG_CURSED);
					SET_FLAG(op, FLAG_PERM_CURSED);

					/* Pick a random stat to put negative value on */
					change_attr_value(&op->stats, rndm(1, NUM_STATS) - 1, strong_curse ? -2 : -1);

					/* If this is strong curse food, give it half a chance to curse another stat */
					if (strong_curse && rndm(0, 1))
					{
						change_attr_value(&op->stats, rndm(1, NUM_STATS) - 1, strong_curse ? -2 : -1);
					}

					/* Put a negative value on random protection. */
					op->protection[rndm(1, LAST_PROTECTION) - 1] = strong_curse ? -25 : -10;

					/* And again, if this is strong curse food, half a chance to curse another protection. */
					if (strong_curse && rndm(0, 1))
					{
						op->protection[rndm(1, LAST_PROTECTION) - 1] = strong_curse ? -25 : -10;
					}

					/* Change food, hp, mana and grace bonuses to negative values */
					if (op->stats.food)
					{
						op->stats.food = -op->stats.food;
					}

					if (op->stats.hp)
					{
						op->stats.hp = -op->stats.hp;
					}

					if (op->stats.sp)
					{
						op->stats.sp = -op->stats.sp;
					}

					if (op->stats.grace)
					{
						op->stats.grace = -op->stats.grace;
					}

					/* Change any positive stat bonuses to negative bonuses. */
					for (i = 0; i < NUM_STATS; i++)
					{
						sint8 val = get_attr_value(&op->stats, i);

						if (val > 0)
						{
							set_attr_value(&op->stats, i, -val);
						}
					}

					/* And the same for protections. */
					for (i = 0; i < NROFATTACKS; i++)
					{
						if (op->protection[i] > 0)
						{
							op->protection[i] = -op->protection[i];
						}
					}

					if (!op->title)
					{
						FREE_AND_ADD_REF_HASH(op->title, (strong_curse ? shstr_cons.of_hideous_poison : shstr_cons.of_poison));
					}
				}

				break;
		}
	}
	/* Title is not NULL. */
	else
	{
		switch (op->type)
		{
			case ARROW:
				if (op->slaying == shstr_cons.none)
				{
					add_random_race(op);
				}

				break;

			case WEAPON:
				if (op->slaying == shstr_cons.none)
				{
					add_random_race(op);
				}

				break;
		}
	}

	if ((flags & GT_NO_VALUE) && op->type != MONEY)
	{
		op->value = 0;
	}

	if (flags & GT_STARTEQUIP)
	{
		if (op->nrof < 2 && op->type != CONTAINER && op->type != MONEY && !QUERY_FLAG(op, FLAG_IS_THROWN))
		{
			SET_FLAG(op, FLAG_STARTEQUIP);
		}
		else if (op->type != MONEY)
		{
			op->value = 0;
		}
	}

	return retval;
}

/**
 * Allocate and return the pointer to an empty artifactlist structure.
 * @return New structure blanked, never NULL. */
static artifactlist *get_empty_artifactlist()
{
	artifactlist *tl = (artifactlist *) malloc(sizeof(artifactlist));

	if (tl == NULL)
	{
		LOG(llevError, "ERROR: get_empty_artifactlist(): Out of memory.\n");
	}

	tl->next = NULL;
	tl->items = NULL;
	tl->total_chance = 0;

	return tl;
}

/**
 * Allocate and return the pointer to an empty artifact structure.
 * @return New structure blanked, never NULL. */
static artifact *get_empty_artifact(void)
{
	artifact *t = (artifact *) malloc(sizeof(artifact));

	if (t == NULL)
	{
		LOG(llevError, "ERROR: get_empty_artifact(): Out of memory.\n");
	}

	t->next = NULL;
	t->name = NULL;
	t->def_at_name = NULL;
	t->t_style = 0;
	t->chance = 0;
	t->difficulty = 0;
	t->allowed = NULL;

	return t;
}

/**
 * Searches the artifact lists and returns one that has the same type of
 * objects on it.
 * @param type Type to search for.
 * @return NULL if no suitable list found. */
artifactlist *find_artifactlist(int type)
{
	artifactlist *al;

	for (al = first_artifactlist; al != NULL; al = al->next)
	{
		if (al->type == type)
		{
			return al;
		}
	}

	return NULL;
}

/**
 * Find the default archetype from artifact by internal artifact list
 * name.
 * @param name Name.
 * @return The archetype if found, NULL otherwise. */
archetype *find_artifact_archtype(const char *name)
{
	artifactlist *al;
	artifact *art = NULL;

	for (al = first_artifactlist; al != NULL; al = al->next)
	{
		art = al->items;

		do
		{
			if (art->name && !strcmp(art->name, name))
			{
				return &art->def_at;
			}

			art = art->next;
		}
		while (art != NULL);
	}

	return NULL;
}

/**
 * For debugging purposes. Dumps all tables. */
void dump_artifacts()
{
	artifactlist *al;
	artifact *art;
	linked_char *next;

	for (al = first_artifactlist; al != NULL; al = al->next)
	{
		LOG(llevInfo, "Artifact has type %d, total_chance=%d\n", al->type, al->total_chance);

		for (art = al->items; art != NULL; art = art->next)
		{
			LOG(llevInfo, "Artifact %-30s Difficulty %3d T-Style %d Chance %5d\n", art->name, art->difficulty, art->t_style, art->chance);

			if (art->allowed != NULL)
			{
				LOG(llevInfo, "\tAllowed combinations:");

				for (next = art->allowed; next != NULL; next = next->next)
				{
					LOG(llevInfo, "%s,", next->name);
				}

				LOG(llevInfo, "\n");
			}
		}
	}

	LOG(llevInfo, "\n");
}

/**
 * For debugging purposes. Dumps all treasures recursively.
 * @see dump_monster_treasure() */
static void dump_monster_treasure_rec(const char *name, treasure *t, int depth)
{
	treasurelist *tl;
	int i;

	if (depth > 100)
	{
		return;
	}

	while (t != NULL)
	{
		if (t->name != NULL)
		{
			for (i = 0; i < depth; i++)
			{
				LOG(llevInfo, "  ");
			}

			LOG(llevInfo, "{   (list: %s)\n", t->name);
			tl = find_treasurelist(t->name);
			dump_monster_treasure_rec(name, tl->items, depth + 2);

			for (i = 0; i < depth; i++)
			{
				LOG(llevInfo, "  ");
			}

			LOG(llevInfo, "}   (end of list: %s)\n", t->name);
		}
		else
		{
			for (i = 0; i < depth; i++)
			{
				LOG(llevInfo, "  ");
			}

			if (t->item->clone.type == FLESH)
			{
				LOG(llevInfo, "%s's %s\n", name, t->item->clone.name);
			}
			else
			{
				LOG(llevInfo, "%s\n", t->item->clone.name);
			}
		}

		if (t->next_yes != NULL)
		{
			for (i = 0; i < depth; i++)
			{
				LOG(llevInfo, "  ");
			}

			LOG(llevInfo, " (if yes)\n");
			dump_monster_treasure_rec(name, t->next_yes, depth + 1);
		}

		if (t->next_no != NULL)
		{
			for (i = 0; i < depth; i++)
			{
				LOG(llevInfo, "  ");
			}

			LOG(llevInfo, " (if no)\n");
			dump_monster_treasure_rec(name, t->next_no, depth + 1);
		}

		t = t->next;
	}
}

/**
 * Checks if op can be combined with art. */
static int legal_artifact_combination(object *op, artifact *art)
{
	int neg, success = 0;
	linked_char *tmp;
	const char *name;

	/* Ie, "all" */
	if (art->allowed == NULL)
	{
		return 1;
	}

	for (tmp = art->allowed; tmp; tmp = tmp->next)
	{
#ifdef TREASURE_VERBOSE
		LOG(llevDebug, "legal_art: %s\n", tmp->name);
#endif

		if (*tmp->name == '!')
		{
			name = tmp->name + 1, neg = 1;
		}
		else
		{
			name = tmp->name, neg = 0;
		}

		/* If we match name, then return the opposite of 'neg' */
		if (!strcmp(name, op->name) || (op->arch && !strcmp(name, op->arch->name)))
		{
			return !neg;
		}
		/* Set success as true, since if the match was an inverse, it
		 * means everything is allowed except what we match. */
		else if (neg)
		{
			success = 1;
		}
	}

	return success;
}

/**
 * Fixes the given object, giving it the abilities and titles it should
 * have due to the second artifact template. */
void give_artifact_abilities(object *op, artifact *art)
{
	int tmp_value = op->value;

	op->value = 0;

	if (!load_object(art->parse_text, op, NULL, LO_MEMORYMODE, MAP_ARTIFACT))
	{
		LOG(llevError, "ERROR: give_artifact_abilities(): load_object() error (ob: %s art: %s).\n", op->name, art->name);
	}

	FREE_AND_ADD_REF_HASH(op->artifact, art->name);

	/* This will solve the problem to adjust the value for different
	 * items of same artification. Also we can safely use negative
	 * values. */
	op->value += tmp_value;

	if (op->value < 0)
	{
		op->value = 0;
	}

	return;
}

/**
 * Decides randomly which artifact the object should be turned into.
 * Makes sure that the item can become that artifact (means magic,
 * difficulty, and Allowed fields properly). Then calls
 * give_artifact_abilities() in order to actually create the artifact.
 * @param op Object.
 * @param difficulty Difficulty.
 * @param t_style Treasure style.
 * @param a_chance Artifact chance.
 * @return 1 on success, 0 on failure. */
int generate_artifact(object *op, int difficulty, int t_style, int a_chance)
{
	artifactlist *al;
	artifact *art, *art_tmp = NULL;
	int i = 0;

	al = find_artifactlist(op->type);

	/* Now we overrule unset to 0 */
	if (t_style == T_STYLE_UNSET)
	{
		t_style = 0;
	}

	if (al == NULL)
	{
#ifdef TREASURE_VERBOSE
		LOG(llevDebug, "Couldn't change %s into artifact - no table.\n", op->name);
#endif
		return 0;
	}

	for (i = 0; i < ARTIFACT_TRIES; i++)
	{
		int roll = rndm(1, al->total_chance) - 1;

		for (art = al->items; art != NULL; art = art->next)
		{
			roll -= art->chance;

			if (roll < 0)
			{
				break;
			}
		}

		if (art == NULL || roll >= 0)
		{
			LOG(llevBug, "BUG: Got null entry and non zero roll in generate_artifact, type %d\n", op->type);
			return 0;
		}

		/* Map difficulty not high enough OR the t_style is set and don't match */
		if (difficulty < art->difficulty || (t_style == -1 && art->t_style && art->t_style != T_STYLE_UNSET) || (t_style && art->t_style && art->t_style != t_style && art->t_style != T_STYLE_UNSET))
		{
			continue;
		}

		if (!legal_artifact_combination(op, art))
		{
#ifdef TREASURE_VERBOSE
			LOG(llevDebug, "%s of %s was not a legal combination.\n", op->name, art->item->name);
#endif
			continue;
		}

		give_artifact_abilities(op, art);
		return 1;
	}

	/* If we are here then we failed to generate an artifact by chance. */
	if (a_chance > 0)
	{
		for (art = al->items; art != NULL; art = art->next)
		{
			if (art->chance <= 0)
			{
				continue;
			}

			if (difficulty < art->difficulty || (t_style == -1 && art->t_style && art->t_style != T_STYLE_UNSET) || (t_style && art->t_style && art->t_style != t_style && art->t_style != T_STYLE_UNSET))
			{
				continue;
			}

			if (!legal_artifact_combination(op, art))
			{
				continue;
			}

			/* There we go! */
			art_tmp = art;
		}
	}

	/* Now we MUST have one - if there was at least one legal possible
	 * artifact. */
	if (art_tmp)
	{
		give_artifact_abilities(op, art_tmp);
	}

	return 1;
}

/**
 * Frees a treasure, including its yes, no and next items.
 * @param t Treasure to free. Pointer is free()d too, so becomes
 * invalid. */
static void free_treasurestruct(treasure *t)
{
	if (t->next)
	{
		free_treasurestruct(t->next);
	}

	if (t->next_yes)
	{
		free_treasurestruct(t->next_yes);
	}

	if (t->next_no)
	{
		free_treasurestruct(t->next_no);
	}

	FREE_AND_CLEAR_HASH2(t->name);
	FREE_AND_CLEAR_HASH2(t->change_arch.name);
	FREE_AND_CLEAR_HASH2(t->change_arch.slaying);
	FREE_AND_CLEAR_HASH2(t->change_arch.title);
	free(t);
}

/**
 * Frees a link structure and its next items.
 * @param lc Item to free. Pointer is free()d too, so becomes invalid. */
static void free_charlinks(linked_char *lc)
{
	linked_char *tmp, *next;

	for (tmp = lc; tmp; tmp = next)
	{
		next = tmp->next;
		FREE_AND_CLEAR_HASH(tmp->name);
		free(tmp);
	}
}

/**
 * Totally frees an artifact, its next items, and such.
 * @param at Artifact to free. Pointer is free()d too, so becomes
 * invalid. */
static void free_artifact(artifact *at)
{
	FREE_AND_CLEAR_HASH2(at->name);
	FREE_AND_CLEAR_HASH2(at->def_at.name);

	if (at->next)
	{
		free_artifact(at->next);
	}

	if (at->allowed)
	{
		free_charlinks(at->allowed);
	}

	if (at->parse_text)
	{
		free(at->parse_text);
	}

	FREE_AND_CLEAR_HASH2(at->def_at.clone.name);
	FREE_AND_CLEAR_HASH2(at->def_at.clone.race);
	FREE_AND_CLEAR_HASH2(at->def_at.clone.slaying);
	FREE_AND_CLEAR_HASH2(at->def_at.clone.msg);
	FREE_AND_CLEAR_HASH2(at->def_at.clone.title);
	free_key_values(&at->def_at.clone);
	free(at);
}

/**
 * Free the artifact list. */
static void free_artifactlist()
{
	artifactlist *al, *nextal;

	LOG(llevDebug, "Freeing artifact list.\n");

	for (al = first_artifactlist; al; al = nextal)
	{
		nextal = al->next;

		if (al->items)
		{
			free_artifact(al->items);
		}

		free(al);
	}
}

/**
 * Free all treasure related memory. */
void free_all_treasures()
{
	treasurelist *tl, *next;

	LOG(llevDebug, "Freeing treasure lists.\n");

	for (tl = first_treasurelist; tl; tl = next)
	{
		next = tl->next;
		FREE_AND_CLEAR_HASH2(tl->name);

		if (tl->items)
		{
			free_treasurestruct(tl->items);
		}

		free(tl);
	}

	free_artifactlist();
}

/**
 * Set object's object::material_real.
 * @param op Object to set material_real for.
 * @param change_arch Change arch. */
static void set_material_real(object *op, struct _change_arch *change_arch)
{
	if (change_arch->item_race != -1)
	{
		op->item_race = (uint8) change_arch->item_race;
	}

	/* This must be tested - perhaps we want that change_arch->material
	 * also overrule the material_real -1 marker? */
	if (op->material_real == -1)
	{
		/* WARNING: material_real == -1 skips also the quality modifier.
		 * this is really for objects which don't fit in the material/quality
		 * system (like system objects, forces, effects and stuff). */
		op->material_real = 0;
		return;
	}

	/* We overrule the material settings in any case when this is set */
	if (change_arch->material != -1)
	{
		op->material_real = change_arch->material;

		/* Skip if material is 0 (aka neutralized material setting) */
		if (change_arch->material_range > 0 && change_arch->material)
		{
			op->material_real += rndm(0, change_arch->material_range);
		}
	}

	/* If 0, grab a valid material class. We should assign to all objects
	 * a valid material_real value to avoid problems here. */
	else if (!op->material_real && op->material != M_ADAMANT)
	{
		if (op->material & M_IRON)
		{
			op->material_real = M_START_IRON;
		}
		else if (op->material & M_LEATHER)
		{
			op->material_real = M_START_LEATHER;
		}
		else if (op->material & M_PAPER)
		{
			op->material_real = M_START_PAPER;
		}
		else if (op->material & M_GLASS)
		{
			op->material_real = M_START_GLASS;
		}
		else if (op->material & M_WOOD)
		{
			op->material_real = M_START_WOOD;
		}
		else if (op->material & M_ORGANIC)
		{
			op->material_real = M_START_ORGANIC;
		}
		else if (op->material & M_STONE)
		{
			op->material_real = M_START_STONE;
		}
		else if (op->material & M_CLOTH)
		{
			op->material_real = M_START_CLOTH;
		}
		else if (op->material & M_ADAMANT)
		{
			op->material_real = M_START_ADAMANT;
		}
		else if (op->material & M_LIQUID)
		{
			op->material_real = M_START_LIQUID;
		}
		else if (op->material & M_SOFT_METAL)
		{
			op->material_real = M_START_SOFT_METAL;
		}
		else if (op->material & M_BONE)
		{
			op->material_real = M_START_BONE;
		}
		else if (op->material & M_ICE)
		{
			op->material_real = M_START_ICE;
		}
	}

	/* Now we do some work: we define a (material) quality and try to
	 * find a best matching pre-set material_real for that item. This is
	 * a bit more complex but we are with that free to define different
	 * materials without having a strong fixed material table. */
	if (change_arch->material_quality != -1)
	{
		int i, q_tmp = -1;
		int m_range = change_arch->material_quality;

		if (change_arch->material_range > 0)
		{
			m_range += rndm(0, change_arch->material_range);
		}

		if (op->material_real)
		{
			int m_tmp = op->material_real / NROFMATERIALS_REAL;

			/* The first entry of the material_real of material table */
			m_tmp = m_tmp * 64 + 1;

			/* We should add paper & cloth here too later */
			if (m_tmp == M_START_IRON || m_tmp == M_START_WOOD || m_tmp == M_START_LEATHER)
			{
				for (i = 0; i < NROFMATERIALS_REAL; i++)
				{
					/* We have a full hit */
					if (material_real[m_tmp + i].quality == m_range)
					{
						op->material_real = m_tmp + i;
						goto set_material_real;
					}

					/* Find nearest quality we want */
					if (material_real[m_tmp + i].quality >= change_arch->material_quality && material_real[m_tmp + i].quality <= m_range && material_real[m_tmp + i].quality > q_tmp)
					{
						q_tmp = m_tmp + i;
					}
				}

				/* If we have no match, we simply use the (always valid)
				 * first material_real entry and forcing the
				 * material_quality to quality! */
				if (q_tmp == -1)
				{
					op->material_real = m_tmp;
					op->item_quality = change_arch->material_quality;
					op->item_condition = op->item_quality;
					return;
				}

				/* That's now our best match! */
				op->material_real = q_tmp;
			}
			/* Exluded material table! */
			else
			{
				op->item_quality = m_range;
				op->item_condition = op->item_quality;
				return;
			}
		}
		/* We have material_real 0 but we modify at least the quality! */
		else
		{
			op->item_quality = m_range;
			op->item_condition = op->item_quality;
			return;
		}
	}

set_material_real:
	/* Adjust quality - use material default value or quality adjustment */
	if (change_arch->quality != -1)
	{
		op->item_quality = change_arch->quality;
	}
	else
	{
		op->item_quality = material_real[op->material_real].quality;
	}

	if (change_arch->quality_range > 0)
	{
		op->item_quality += rndm(0, change_arch->quality_range);

		if (op->item_quality > 100)
		{
			op->item_quality = 100;
		}
	}

	op->item_condition = op->item_quality;
}

/**
 * For debugging purposes. Dumps all treasures for a given monster.
 * @param name Name of the monster to dump treasures for. */
void dump_monster_treasure(const char *name)
{
	archetype *at;
	int found;

	found = 0;
	LOG(llevInfo, "\n");

	for (at = first_archetype; at != NULL; at = at->next)
	{
		if (!strcasecmp(at->name, name))
		{
			LOG(llevInfo, "treasures for %s (arch: %s)\n", at->clone.name, at->name);

			if (at->clone.randomitems != NULL)
			{
				dump_monster_treasure_rec(at->clone.name, at->clone.randomitems->items, 1);
			}
			else
			{
				LOG(llevInfo, "(nothing)\n");
			}

			LOG(llevInfo, "\n");
			found++;
		}
	}

	if (found == 0)
	{
		LOG(llevInfo, "No objects have the name %s!\n\n", name);
	}
}

/**
 * Gets the environment level for treasure generation for the given
 * object.
 * @param op Object to get environment level of.
 * @return The environment level, always at least 1. */
int get_enviroment_level(object *op)
{
	object *env;

	if (!op)
	{
		LOG(llevBug, "get_enviroment_level() called for NULL object!\n");
		return 1;
	}

	/* Return object level or map level... */
	if (op->level)
	{
		return op->level;
	}

	if (op->map)
	{
		return op->map->difficulty ? op->map->difficulty : 1;
	}

	/* Let's check for env */
	env = op->env;

	while (env)
	{
		if (env->level)
		{
			return env->level;
		}

		if (env->map)
		{
			return env->map->difficulty ? env->map->difficulty : 1;
		}

		env = env->env;
	}

	return 1;
}

/**
 * Create an artifact.
 * @param op Object to turn into an artifact.
 * @param artifactname Artifact to create.
 * @return Always returns NULL. */
object *create_artifact(object *op, char *artifactname)
{
	artifactlist *al = find_artifactlist(op->type);
	artifact *art;

	if (al == NULL)
	{
		return NULL;
	}

	for (art = al->items; art != NULL; art = art->next)
	{
		if (!strcmp(art->name, artifactname))
		{
			give_artifact_abilities(op, art);
		}
	}

	return NULL;
}

#ifdef TREASURE_DEBUG
/**
 * Checks if a treasure if valid. Will also check its yes and no options.
 * @param t Treasure to check.
 * @param tl Needed only so that the treasure name can be printed out. */
static void check_treasurelist(treasure *t, treasurelist *tl)
{
	if (t->item == NULL && t->name == NULL)
	{
		LOG(llevError, "ERROR: Treasurelist %s has element with no name or archetype\n", tl->name);
	}

	if (t->chance >= 100 && t->next_yes && (t->next || t->next_no))
	{
		LOG(llevBug, "BUG: Treasurelist %s has element that has 100%% generation, next_yes field as well as next or next_no\n", tl->name);
	}

	/* find_treasurelist will print out its own error message */
	if (t->name && t->name != shstr_cons.NONE)
	{
		(void) find_treasurelist(t->name);
	}

	if (t->next)
	{
		check_treasurelist(t->next, tl);
	}

	if (t->next_yes)
	{
		check_treasurelist(t->next_yes, tl);
	}

	if (t->next_no)
	{
		check_treasurelist(t->next_no, tl);
	}
}
#endif
