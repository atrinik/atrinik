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

#define ALLOWED_COMBINATION

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
#include <funcpoint.h>
#include <loader.h>

/* for quick search for string "none" */
static const char *treasure_string_none = NULL;
char *coins[NUM_COINS + 1] =
{
	"mitcoin",
	"goldcoin",
	"silvercoin",
	"coppercoin",
	NULL
};
archetype *coins_arch[NUM_COINS];

/* Give 1 re-roll attempt per artifact */
#define ARTIFACT_TRIES 2
#define CHANCE_FIX (-1)

static archetype *ring_arch = NULL, *ring_arch_normal = NULL, *amulet_arch = NULL;

/* static functions */
static treasure *load_treasure(FILE *fp, int *t_style, int *a_chance);
/* overrule default values */
static void change_treasure(struct _change_arch *ca, object *op);
static treasurelist *get_empty_treasurelist(void);
static treasure *get_empty_treasure(void);
static void put_treasure(object *op, object *creator, int flags);
static artifactlist *get_empty_artifactlist(void);
static artifact *get_empty_artifact(void);
static void check_treasurelist(treasure *t, treasurelist *tl);
static inline void set_material_real(object *op, struct _change_arch *change_arch);
static void create_money_table(void);

/* Opens LIBDIR/treasure and reads all treasure-declarations from it.
 * Each treasure is parsed with the help of load_treasure(). */
void load_treasures()
{
	FILE *fp;
	char filename[MAX_BUF], buf[MAX_BUF], name[MAX_BUF];
	treasurelist *previous = NULL;
	treasure *t;
	int comp, t_style, a_chance;

	sprintf(filename, "%s/%s", settings.datadir, settings.treasures);

	FREE_AND_COPY_HASH(treasure_string_none, "none");

	if ((fp = open_and_uncompress(filename, 0, &comp)) == NULL)
	{
		LOG(llevError, "ERROR: Can't open treasure file.\n");
		return;
	}

	while (fgets(buf, MAX_BUF, fp) != NULL)
	{
		/* Ignore comments and blank lines */
		if (*buf == '#' || *buf == '\n')
			continue;

		if (sscanf(buf, "treasureone %s\n", name) || sscanf(buf, "treasure %s\n", name))
		{
			treasurelist *tl = get_empty_treasurelist();
			FREE_AND_COPY_HASH(tl->name, name);

			if (previous == NULL)
				first_treasurelist = tl;
			else
				previous->next = tl;

			previous = tl;
			t_style= T_STYLE_UNSET;
			a_chance = ART_CHANCE_UNSET;
			tl->items = load_treasure(fp, &t_style, &a_chance);

			if (tl->t_style == T_STYLE_UNSET)
				tl->t_style = t_style;

			if (tl->artifact_chance == ART_CHANCE_UNSET)
				tl->artifact_chance = a_chance;

			/* This is a one of the many items on the list should be generated.
			* Add up the chance total, and check to make sure the yes & no
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
#if 0
				LOG(llevDebug, "Total chance for list %s is %d\n", tl->name, tl->total_chance);
#endif
			}
		}
		else
			LOG(llevError, "ERROR: Treasure-list didn't understand: %s\n", buf);
#if 0
		LOG(llevDebug, "Treasure-list found: %s", buf);
#endif
	}
	close_and_delete(fp, comp);

#ifdef TREASURE_DEBUG
	/* Perform some checks on how valid the treasure data actually is.
	  * verify that list transitions work (ie, the list that it is supposed
	  * to transition to exists).  Also, verify that at least the name
	  * or archetype is set for each treasure element. */
	for (previous = first_treasurelist; previous != NULL; previous = previous->next)
		check_treasurelist(previous->items, previous);
#endif

	create_money_table();
}

/* to generate from a value a set of coins (like 3 gold, 4 silver and 19 copper)
 * we collect the arch for it out of the arch name for faster access. */
static void create_money_table(void)
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

/* Reads the lib/treasure file from disk, and parses the contents
 * into an internal treasure structure (very linked lists) */
static treasure *load_treasure(FILE *fp, int *t_style, int *a_chance)
{
	char buf[MAX_BUF], *cp = NULL, variable[MAX_BUF];
	treasure *t = get_empty_treasure();
	int value;
	int start_marker = 0, t_style2, a_chance2;

	nroftreasures++;
	while (fgets(buf, MAX_BUF, fp) != NULL)
	{
		if (*buf == '#')
			continue;

		if ((cp = strchr(buf, '\n')) != NULL)
			*cp = '\0';

		cp = buf;

		/* Skip blanks */
		while (!isalpha(*cp))
			cp++;

		if (sscanf(cp, "t_style %d", &value))
		{
			if (start_marker)
				t->t_style = value;
			else
			{
				/* no, its global for the while treasure list entry */
				*t_style = value;
			}
		}
		else if (sscanf(cp, "artifact_chance %d", &value))
		{
			if (start_marker)
				t->artifact_chance = value;
			else
			{
				/* no, its global for the while treasure list entry */
				*a_chance = value;
			}
		}
		else if (sscanf(cp, "arch %s", variable))
		{
			if ((t->item = find_archetype(variable)) == NULL)
				LOG(llevBug, "BUG: Treasure lacks archetype: %s\n", variable);

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
			t->change_arch.item_race = value;
		else if (sscanf(cp, "quality %d", &value))
			t->change_arch.quality = value;
		else if (sscanf(cp, "quality_range %d", &value))
			t->change_arch.quality_range = value;
		else if (sscanf(cp, "material %d", &value))
			t->change_arch.material = value;
		else if (sscanf(cp, "material_quality %d", &value))
			t->change_arch.material_quality = value;
		else if (sscanf(cp, "material_range %d", &value))
			t->change_arch.material_range = value;
		else if (sscanf(cp, "chance_fix %d", &value))
		{
			t->chance_fix = (sint16) value;
			/* important or the chance will stay 100% when not set to 0 in treasure list! */
			t->chance = 0;
		}
		else if (sscanf(cp, "chance %d", &value))
			t->chance = (uint8) value;
		else if (sscanf(cp, "nrof %d", &value))
			t->nrof = (uint16) value;
		else if (sscanf(cp, "magic %d", &value))
			t->magic = value;
		else if (sscanf(cp, "magic_fix %d", &value))
			t->magic_fix = value;
		else if (sscanf(cp, "magic_chance %d", &value))
			t->magic_chance = value;
		else if (sscanf(cp, "difficulty %d", &value))
			t->difficulty = value;
		else if (!strncmp(cp, "yes", 3))
		{
			t_style2 = T_STYLE_UNSET;
			a_chance2 = ART_CHANCE_UNSET;
			t->next_yes = load_treasure(fp, &t_style2, &a_chance2);

			if (t->next_yes->artifact_chance == ART_CHANCE_UNSET)
				t->next_yes->artifact_chance = a_chance2;

			if (t->next_yes->t_style == T_STYLE_UNSET)
				t->next_yes->t_style = t_style2;
		}
		else if (!strncmp(cp, "no", 2))
		{
			t_style2 = T_STYLE_UNSET;
			a_chance2 = ART_CHANCE_UNSET;
			t->next_no = load_treasure(fp, &t_style2, &a_chance2);

			if (t->next_no->artifact_chance == ART_CHANCE_UNSET)
				t->next_no->artifact_chance = a_chance2;

			if (t->next_no->t_style == T_STYLE_UNSET)
				t->next_no->t_style = t_style2;
		}
		else if (!strncmp(cp, "end", 3))
			return t;
		else if (!strncmp(cp, "more", 4))
		{
			t_style2 = T_STYLE_UNSET;
			a_chance2 = ART_CHANCE_UNSET;
			t->next = load_treasure(fp, &t_style2, &a_chance2);

			if (t->next->artifact_chance == ART_CHANCE_UNSET)
				t->next->artifact_chance = a_chance2;

			if (t->next->t_style == T_STYLE_UNSET)
				t->next->t_style = t_style2;

			return t;
		}
		else
			LOG(llevBug, "BUG: Unknown treasure-command: '%s', last entry %s\n", cp, t->name ? t->name : "null");
	}

	LOG(llevBug, "BUG: treasure %s lacks 'end'.>%s<\n",t->name ? t->name : "NULL", cp ? cp : "NULL");
	return t;
}

/* Builds up the lists of artifacts from the file in the libdir. */
/* Remember: other_arch & treasurelists defined in the artifacts file
 * will be parsed in a second parse by hand - like the normal arches. */
void init_artifacts()
{
	static int has_been_inited = 0;
	archetype *atemp;
	long old_pos, file_pos;
	FILE *fp;
	char filename[MAX_BUF], buf[MAX_BUF], *cp, *next;
	artifact *art = NULL;
	linked_char *tmp;
	int lcount, value, comp, none_flag = 0;
	artifactlist *al;
	/* ok, 10k arch text... if we bug here, we have a design problem */
	char buf_text[10 * 1024];

	if (has_been_inited)
		return;

	has_been_inited = 1;

	sprintf(filename, "%s/artifacts", settings.datadir);
	LOG(llevDebug, " reading artifacts from %s...", filename);

	if ((fp = open_and_uncompress(filename, 0, &comp)) == NULL)
	{
		LOG(llevError, "ERROR: Can't open %s.\n", filename);
		return;
	}

	/* start read in the artifact list */
	while (fgets(buf, MAX_BUF, fp) != NULL)
	{
		if (*buf == '#')
			continue;

		if ((cp = strchr(buf, '\n')) != NULL)
			*cp = '\0';

		/* Skip blank lines. */
		if (*buf == '\0')
		{
			continue;
		}

		cp = buf;
		/* Skip blanks */
		while (*cp == ' ')
			cp++;

		/* we have a single artifact */
		if (!strncmp(cp, "Allowed", 7))
		{
			art = get_empty_artifact();
			nrofartifacts++;
			none_flag = FALSE;
			cp = strchr(cp, ' ') + 1;

			if (!strcmp(cp, "all"))
				continue;

			if (!strcmp(cp, "none"))
			{
				none_flag= TRUE;
				continue;
			}

			do
			{
				nrofallowedstr++;
				if ((next = strchr(cp, ',')) != NULL)
					*(next++) = '\0';

				tmp = (linked_char*) malloc(sizeof(linked_char));
				tmp->name = NULL;
				FREE_AND_COPY_HASH(tmp->name, cp);
				tmp->next = art->allowed;
				art->allowed = tmp;
			}
			while ((cp = next) != NULL);
		}
		else if (sscanf(cp, "t_style %d", &value))
			art->t_style = value;
		else if (sscanf(cp, "chance %d", &value))
			art->chance = (uint16) value;
		else if (sscanf(cp, "difficulty %d", &value))
			art->difficulty = (uint8) value;
		else if (!strncmp(cp, "artifact", 8))
		{
			FREE_AND_COPY_HASH(art->name, cp + 9);
		}
		/* chain a default arch to this treasure */
		else if (!strncmp(cp, "def_arch", 8))
		{
			if ((atemp = find_archetype(cp + 9)) == NULL)
				LOG(llevError, "ERROR: Init_Artifacts: Can't find def_arch %s.\n", cp + 9);

			/* ok, we have a name and a archtype */
			/* store the non fake archetype name */
			FREE_AND_COPY_HASH(art->def_at_name, cp + 9);
			/* copy the default arch */
			memcpy(&art->def_at, atemp, sizeof(archetype));
			art->def_at.base_clone = &atemp->clone;
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
		/* all text after Object is now like an arch file until an end comes */
		else if (!strncmp(cp, "Object", 6))
		{
			old_pos = ftell(fp);
			if (!load_object(fp, &(art->def_at.clone), NULL, LO_LINEMODE, MAP_STYLE))
				LOG(llevError, "ERROR: Init_Artifacts: Could not load object.\n");

			if (!art->name)
				LOG(llevError, "ERROR: Init_Artifacts: Object %s has no arch id name\n", art->def_at.clone.name);

			if (!art->def_at_name)
				LOG(llevError, "ERROR: Init_Artifacts: Artifact %s has no def arch\n", art->name);

			/* ok, now lets catch & copy the commands to our artifacts buffer.
			 * lets do some file magic here - thats the easiest way. */
			file_pos = ftell(fp);

			if (fseek(fp, old_pos, SEEK_SET))
				LOG(llevError, "ERROR: Init_Artifacts: Could not fseek(fp,%d,SEEK_SET).\n", old_pos);

			/* the lex reader will bug when it don't get feed with a <text>+0x0a+0 string.
			 * so, we do it here and in the lex part we simple do a strlen and point
			 * to every part without copy it. */
			lcount = 0;
			while (fgets(buf, MAX_BUF - 3, fp))
			{
				strcpy(buf_text + lcount, buf);
				lcount += strlen(buf) + 1;

				if (ftell(fp) == file_pos)
					break;

				/* should not possible! */
				if (ftell(fp) > file_pos)
					LOG(llevError, "ERROR: Init_Artifacts: fgets() read too much data! (%d - %d)\n", file_pos, ftell(fp));
			}

			/* now store the parse text in the artifacts list entry */
			if ((art->parse_text = malloc(lcount)) == NULL)
				LOG(llevError, "ERROR: Init_Artifacts: out of memory in ->parse_text (size %d)\n", lcount);

			memcpy(art->parse_text, buf_text, lcount);

			/* finally, change the archetype name of
			 * our fake arch to the fake arch name.
			 * without it, treasures will get the
			 * original arch, not this (hm, this
			 * can be a glitch in treasures too...) */
			FREE_AND_COPY_HASH(art->def_at.name, art->name);
			/* now handle the <Allowed none> in the artifact to create
			 * unique items or add them to the given type list. */
			al = find_artifactlist(none_flag == FALSE ? art->def_at.clone.type : -1);
			if (al == NULL)
			{
				al = get_empty_artifactlist();
				al->type = none_flag == FALSE ? art->def_at.clone.type : -1;
				al->next = first_artifactlist;
				first_artifactlist = al;
			}

			art->next = al->items;
			al->items = art;
		}
		else
			LOG(llevBug, "BUG: Unknown input in artifact file: %s\n", buf);
	}
	close_and_delete(fp, comp);

	for (al = first_artifactlist; al != NULL; al = al->next)
	{
		for (art = al->items; art != NULL; art = art->next)
		{
#if 0
			add_arch();
			LOG(llevDebug, "art: %s (%s %s)\n", art->name, art->def_at.name, query_name(&art->def_at.clone, NULL));
#endif

			/* we don't use our unique artifacts as pick table */
			if (al->type == -1)
				continue;

			if (!art->chance)
				LOG(llevBug, "BUG: artifact with no chance: %s\n", art->name);
			else
				al->total_chance += art->chance;
		}
	}

	LOG(llevDebug, "done.\n");
}

/* Initialize global archtype pointers: */
void init_archetype_pointers()
{
	if (ring_arch_normal == NULL)
		ring_arch_normal = find_archetype("ring_normal");

	if (!ring_arch_normal)
		LOG(llevBug, "BUG: Cant'find 'ring_normal' arch (from artifacts)\n");

	if (ring_arch == NULL)
		ring_arch = find_archetype("ring_generic");

	if (!ring_arch)
		LOG(llevBug, "BUG: Cant'find 'ring_generic' arch\n");

	if (amulet_arch == NULL)
		amulet_arch = find_archetype("amulet_generic");

	if (!amulet_arch)
		LOG(llevBug, "BUG: Cant'find 'amulet_generic' arch\n");
}

/* Allocate and return the pointer to an empty treasurelist structure. */
static treasurelist *get_empty_treasurelist(void)
{
	treasurelist *tl = (treasurelist *) malloc(sizeof(treasurelist));

	if (tl == NULL)
		LOG(llevError, "ERROR: get_empty_treasurelist(): Out of memory.\n");

	tl->name = NULL;
	tl->next = NULL;
	tl->items = NULL;
	/* -2 is the "unset" marker and will virtually handled as 0 which can be overruled */
	tl->t_style = T_STYLE_UNSET;
	tl->artifact_chance = ART_CHANCE_UNSET;
	tl->chance_fix = CHANCE_FIX;
	tl->total_chance = 0;
	return tl;
}

/* Allocate and return the pointer to an empty treasure structure. */

static treasure *get_empty_treasure(void)
{
	treasure *t = (treasure *) malloc(sizeof(treasure));

	if (t == NULL)
		LOG(llevError, "ERROR: get_empty_treasure(): Out of memory.\n");

	t->change_arch.item_race = -1;
	t->change_arch.name = NULL;
	t->change_arch.slaying = NULL;
	t->change_arch.title = NULL;
	/* -2 is the "unset" marker and will virtually handled as 0 which can be overruled */
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

/* Searches for the given treasurelist in the globally linked list
 * of treasurelists which has been built by load_treasures(). */
treasurelist *find_treasurelist(const char *name)
{
	const char *tmp = find_string(name);
	treasurelist *tl;

	/* Special cases - randomitems of none is to override default.  If
	  * first_treasurelist is null, it means we are on the first pass of
	  * of loading archetyps, so for now, just return - second pass will
	  * init these values. */
	if (!strcmp(name, "none") || (!first_treasurelist))
		return NULL;

	if (tmp != NULL)
		for (tl = first_treasurelist; tl != NULL; tl = tl->next)
			if (tmp == tl->name)
				return tl;

	LOG(llevBug, "Bug: Couldn't find treasurelist %s\n", name);
	return NULL;
}

/* This is similar to the old generate treasure function.  However,
 * it instead takes a treasurelist.  It is really just a wrapper around
 * create_treasure.  We create a dummy object that the treasure gets
 * inserted into, and then return that treausre */
object *generate_treasure(treasurelist *t, int difficulty)
{
	object *ob = get_object(), *tmp;

	create_treasure(t, ob, 0, difficulty, t->t_style, t->artifact_chance, 0, NULL);

	/* Don't want to free the object we are about to return */
	tmp = ob->inv;

	/* remove from inv - no move off */
	if (tmp != NULL)
		remove_ob(tmp);

	if (ob->inv)
		LOG(llevError, "ERROR: generate_treasure(): Created multiple objects.\n");

	return tmp;
}

/* This calls the appropriate treasure creation function.  tries is passed
 * to determine how many list transitions or attempts to create treasure
 * have been made.  It is really in place to prevent infinite loops with
 * list transitions, or so that excessively good treasure will not be
 * created on weak maps, because it will exceed the number of allowed tries
 * to do that. */
/* called from various sources, arch_change will be normally NULL the first time a
 * treasure list is generated. This is then the "base" list. We will use the real
 * first arch_change as base to other recursive calls. */
void create_treasure(treasurelist *t, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *arch_change)
{
	if (tries++ > 100)
	{
		LOG(llevDebug, "DEBUG: create_treasure(): tries >100 for t-list %s.", t->name ? t->name : "<noname>");
		return;
	}

	if (t->t_style != T_STYLE_UNSET)
		t_style = t->t_style;

	if (t->artifact_chance != ART_CHANCE_UNSET)
		a_chance = t->artifact_chance;

	if (t->total_chance)
		create_one_treasure(t, op, flag, difficulty, t_style, a_chance, tries, arch_change);
	else
		create_all_treasures(t->items, op, flag, difficulty, t_style, a_chance, tries, arch_change);
}


/* Generates the objects specified by the given treasure.
 * It goes recursively through the rest of the linked list.
 * If there is a certain percental chance for a treasure to be generated,
 * this is taken into consideration.
 * The second argument specifies for which object the treasure is
 * being generated.
 * If flag is GT_INVISIBLE, only invisible objects are generated (ie, only
 * abilities.  This is used by summon spells, thus no summoned monsters
 * start with equipment, but only their abilities). */
void create_all_treasures(treasure *t, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *change_arch)
{
	object *tmp;

	/* LOG(-1, "-CAT-: %s (%d)\n", t->name ? t->name : "NULL", change_arch ? t->change_arch.material_quality : 9999); */
	/* LOG(-1, "CAT: cs: %d (%d)(%s)\n", t->chance_fix, t->chance, t->name); */

	if (t->t_style != T_STYLE_UNSET)
		t_style = t->t_style;

	if (t->artifact_chance != ART_CHANCE_UNSET)
		a_chance = t->artifact_chance;

	if ((t->chance_fix != CHANCE_FIX && !(RANDOM() % (int)t->chance_fix)) || (int)t->chance >= 100 || ((RANDOM() % 100 + 1) < (int) t->chance))
	{
		/*LOG(-1, "CAT22: cs: %d (%d)(%s)\n", t->chance_fix, t->chance, t->name);*/
		if (t->name)
		{
			/*LOG(-1,"-CAT2: %s (%d)\n", t->name?t->name:"NULL",change_arch?t->change_arch.material_quality:9999); */
			if (strcmp(t->name, "NONE") && difficulty >= t->difficulty)
				create_treasure(find_treasurelist(t->name), op, flag, difficulty, t_style, a_chance, tries, change_arch ? change_arch : &t->change_arch);
		}
		else if (difficulty>=t->difficulty)
		{
			if (IS_SYS_INVISIBLE(&t->item->clone) || !(flag & GT_INVISIBLE))
			{
				if (t->item->clone.type != TYPE_WEALTH)
				{
					/*LOG(-1, "*CAT*: %s (%d)\n", t->item->clone.name, change_arch ? t->change_arch.material_quality : 9999); */
					tmp = arch_to_object(t->item);
					if (t->nrof && tmp->nrof <= 1)
						tmp->nrof = RANDOM() % ((int) t->nrof) + 1;

					/* ret 1 = artifact is generated - don't overwrite anything here */
					set_material_real(tmp, change_arch ? change_arch : &t->change_arch);

					if (!fix_generated_item(&tmp, op, difficulty, a_chance, t_style, t->magic, t->magic_fix, t->magic_chance, flag))
						change_treasure(change_arch ? change_arch : &t->change_arch, tmp);

					put_treasure(tmp, op, flag);

					/* if treasure is "identified", created items are too */
					if (op->type == TREASURE && QUERY_FLAG(op, FLAG_IDENTIFIED))
					{
						SET_FLAG(tmp, FLAG_IDENTIFIED);
						SET_FLAG(tmp, FLAG_KNOWN_MAGICAL);
						SET_FLAG(tmp, FLAG_KNOWN_CURSED);
					}
				}
				/* we have a wealth object - expand it to real money */
				else
				{
					/* if t->magic is != 0, thats our value - if not use default setting */
					int i, value = t->magic ? t->magic : t->item->clone.value;

					value *= (difficulty / 2) + 1;
					/* so we have 80% to 120% of the fixed value */
					value = (int) ((float) value * 0.8f + (float) value * ((float)(RANDOM() % 40) / 100.0f));

					for (i = 0; i < NUM_COINS; i++)
					{
						if (value / coins_arch[i]->clone.value > 0)
						{
							tmp = get_object();
							copy_object(&coins_arch[i]->clone, tmp);
							tmp->nrof = value / tmp->value;
							value -= tmp->nrof * tmp->value;
							put_treasure(tmp, op, flag);
						}
					}
				}
			}
		}

		if (t->next_yes != NULL)
			create_all_treasures(t->next_yes, op, flag, difficulty, (t->next_yes->t_style == T_STYLE_UNSET) ? t_style : t->next_yes->t_style, a_chance, tries, change_arch);
	}
	else if (t->next_no != NULL)
		create_all_treasures(t->next_no, op, flag, difficulty, (t->next_no->t_style == T_STYLE_UNSET) ? t_style : t->next_no->t_style, a_chance, tries, change_arch);

	if (t->next != NULL)
		create_all_treasures(t->next, op, flag, difficulty, (t->next->t_style == T_STYLE_UNSET) ? t_style : t->next->t_style, a_chance, tries, change_arch);
}

void create_one_treasure(treasurelist *tl, object *op, int flag, int difficulty, int t_style, int a_chance, int tries, struct _change_arch *change_arch)
{
	int value, diff_tries = 0;
	treasure *t;
	object *tmp;

	/*LOG(-1, "-COT-: %s (%d)\n", tl->name, change_arch ? tl->items->change_arch.material_quality : 9999); */
	/*LOG(-1, "COT: cs: %d (%s)\n", tl->chance_fix, tl->name );*/
	if (tries++ > 100)
		return;

	/* well, at some point we should rework this whole system... */
create_one_treasure_again_jmp:
	if (diff_tries > 10)
		return;

	value = RANDOM() % tl->total_chance;

	for (t = tl->items; t != NULL; t = t->next)
	{
		/* chance_fix will overrule the normal chance stuff! */
		if (t->chance_fix != CHANCE_FIX)
		{
			if (!(RANDOM() % t->chance_fix))
			{
				/* LOG(-1, "COT: HIT: cs: %d (%s)\n", t->chance_fix, t->name);*/
				/* only when allowed, we go on! */
				if (difficulty >= t->difficulty)
				{
					value = 0;
					break;
				}

				/* ok, difficulty is bad lets try again or break! */
				if (tries++ > 100)
					return;

				diff_tries++;
				goto create_one_treasure_again_jmp;
			}

			if (!t->chance)
				continue;
		}

		value -= t->chance;
		/* we got one! */
		if (value <= 0)
		{
			/* only when allowed, we go on! */
			if (difficulty >= t->difficulty)
				break;

			/* ok, difficulty is bad lets try again or break! */
			if (tries++ > 100)
				return;

			diff_tries++;
			goto create_one_treasure_again_jmp;
		}
	}

	if (t->t_style != T_STYLE_UNSET)
		t_style = t->t_style;

	if (t->artifact_chance != ART_CHANCE_UNSET)
		a_chance = t->artifact_chance;

	if (!t || value > 0)
	{
		LOG(llevBug, "BUG: create_one_treasure: got null object or not able to find treasure - tl:%s op:%s\n", tl ? tl->name : "(null)", op ? op->name : "(null)");
		return;
	}

	if (t->name)
	{
		if (!strcmp(t->name, "NONE"))
			return;

		if (difficulty >= t->difficulty)
			create_treasure(find_treasurelist(t->name), op, flag, difficulty, t_style, a_chance, tries, change_arch);
		else if (t->nrof)
			create_one_treasure(tl, op, flag, difficulty, t_style, a_chance, tries, change_arch);

		return;
	}

	if (IS_SYS_INVISIBLE(&t->item->clone) || flag != GT_INVISIBLE)
	{
		if (t->item->clone.type != TYPE_WEALTH)
		{
			/*LOG(-1,"*COT*: %s (%d)\n", t->item->clone.name,change_arch?t->change_arch.material_quality:9999); */
			tmp = arch_to_object(t->item);
			if (t->nrof && tmp->nrof <= 1)
				tmp->nrof = RANDOM()%((int) t->nrof) + 1;

			set_material_real(tmp, change_arch ? change_arch : &t->change_arch);

			if (!fix_generated_item(&tmp, op, difficulty, a_chance, (t->t_style == T_STYLE_UNSET) ? t_style : t->t_style, t->magic, t->magic_fix, t->magic_chance, flag))
				change_treasure(change_arch ? change_arch : &t->change_arch, tmp);

			put_treasure(tmp, op, flag);
			/* if treasure is "identified", created items are too */
			if (op->type == TREASURE && QUERY_FLAG(op, FLAG_IDENTIFIED))
			{
				SET_FLAG(tmp, FLAG_IDENTIFIED);
				SET_FLAG(tmp, FLAG_KNOWN_MAGICAL);
				SET_FLAG(tmp, FLAG_KNOWN_CURSED);
			}
		}
		/* we have a wealth object - expand it to real money */
		else
		{
			/* if t->magic is != 0, thats our value - if not use default setting */
			int i, value = t->magic ? t->magic : t->item->clone.value;

			value *= difficulty;
			/* so we have 80% to 120% of the fixed value */
			value = (int) ((float) value *0.8f + (float) value * ((float)(RANDOM() % 40) / 100.0f));

			for (i = 0; i < NUM_COINS; i++)
			{
				if (value / coins_arch[i]->clone.value > 0)
				{
					tmp = get_object();
					copy_object(&coins_arch[i]->clone, tmp);
					tmp->nrof = value / tmp->value;
					value -= tmp->nrof * tmp->value;
					put_treasure(tmp, op, flag);
				}
			}
		}
	}
}

static void put_treasure(object *op, object *creator, int flags)
{
	object *tmp;

	if (flags & GT_ENVIRONMENT)
	{
		op->x = creator->x;
		op->y = creator->y;
		/* this must be handled carefully... we don't want drop items on a button
		 * which is then not triggered. MT-2004 */
		insert_ob_in_map(op, creator->map, op, INS_NO_MERGE | INS_NO_WALK_ON);
	}
	else
	{
		op = insert_ob_in_ob(op, creator);

		if ((flags & GT_APPLY) && QUERY_FLAG(creator, FLAG_MONSTER))
			(void) (*monster_check_apply_func)(creator, op);

		if ((flags & GT_UPDATE_INV) && (tmp = is_player_inv(creator)) != NULL)
			(*esrv_send_item_func)(tmp, op);
	}
}

/* if there are change_xxx commands in the treasure, we include the changes
 * in the generated object */
static void change_treasure(struct _change_arch *ca, object *op)
{
	if (ca->name)
		FREE_AND_COPY_HASH(op->name, ca->name);

	if (ca->title)
		FREE_AND_COPY_HASH(op->title,ca->title);

	if (ca->slaying)
		FREE_AND_COPY_HASH(op->slaying, ca->slaying);
}

/* Sets a random magical bonus in the given object based upon
 * the given difficulty, and the given max possible bonus.
 * the old system based on difficulty for setting the +x value.
 * i changed this in 2 ways: difficulty now effects only artifacts
 * but now artifacts generation don't based on +x value anymore.
 * so, a sword+1 can be Sunword +3 if difficulty is right.
 * The reason why we don't want +3 or +4 "base" items is that
 * we want include crafting later - there we can build from 2 +1 items
 * of same kind one +2. */
static void set_magic(int difficulty, object *op, int max_magic, int fix_magic, int chance_magic, int flags)
{
	int i;

	/* if we have a fixed value, force it */
	if (fix_magic)
		i = fix_magic;
	else
	{
		i = 0;

		/* chance_magic 0 means allways no magic bonus */
		if (((RANDOM() % 100) + 1) <= chance_magic || ((RANDOM() % 200) + 1) <= difficulty)
		{
			i = (RANDOM() % abs(max_magic)) + 1;
			if (max_magic < 0)
				i = -i;
		}
	}

	if ((flags & GT_ONLY_GOOD) && i < 0)
		i = 0;

	set_abs_magic(op, i);

	if (i < 0)
		SET_FLAG(op, FLAG_CURSED);

	if (i != 0)
		SET_FLAG(op, FLAG_IS_MAGICAL);
}

/* Sets magical bonus in an object, and recalculates the effect on
 * the armour variable, and the effect on speed of armour.
 * This function doesn't work properly, should add use of archetypes
 * to make it truly absolute. */
void set_abs_magic(object *op, int magic)
{
	if (!magic)
		return;

	SET_FLAG(op, FLAG_IS_MAGICAL);

	op->magic = magic;

	if (op->arch)
	{
		if (magic == 1)
			op->value += 5300;
		else if (magic == 2)
			op->value += 12300;
		else if (magic == 3)
			op->value += 62300;
		else if (magic == 4)
			op->value += 130300;
		else
			op->value += 250300;

		if (op->type == ARMOUR)
			ARMOUR_SPEED(op) = (ARMOUR_SPEED(&op->arch->clone) * (100 + magic * 10)) / 100;

		/* You can't just check the weight always */
		if (magic < 0 && !(RANDOM() % 3))
			magic = (-magic);

		op->weight = (op->arch->clone.weight * (100 - magic * 10)) / 100;
	}
	else
	{
		if (op->type == ARMOUR)
			ARMOUR_SPEED(op) = (ARMOUR_SPEED(op) * (100 + magic * 10)) / 100;

		/* You can't just check the weight always */
		if (magic < 0 && !(RANDOM() % 3))
			magic = (-magic);

		op->weight = (op->weight * (100 - magic * 10)) / 100;
	}
}

/* Randomly adds one magical ability to the given object.
 * Modified for Partial Resistance in many ways:
 * 1) Since rings can have multiple bonuses, if the same bonus
 *  is rolled again, increase it - the bonuses now stack with
 *  other bonuses previously rolled and ones the item might natively have.
 * 2) Add code to deal with new PR method.
 * return 0: no special added. 1: something added. */
int set_ring_bonus(object *op, int bonus, int level)
{
	int tmp, r, off;
	off = (level >= 50 ? 1 : 0) + (level >= 60 ? 1 : 0) + (level >= 70 ? 1 : 0) + (level >= 80 ? 1 : 0);

	/* lets repeat, to lazy for a loop */
set_ring_bonus_jump1:
	r = RANDOM() % (bonus > 0 ? 25 : 13);

	SET_FLAG(op, FLAG_IS_MAGICAL);

	if (op->type == AMULET)
	{
		if (!(RANDOM() % 21))
			r = 20 + RANDOM() % 2;

		else if (!(RANDOM() % 20))
		{
			tmp = RANDOM() % 3;
			if (tmp == 2)
				r = 0;
			else if (!tmp)
				r = 11;
			else
				r = 12;
		}
		else if (RANDOM() & 2)
			r = 10;
		else
			r = 13 + RANDOM() % 7;
	}

	switch (r % 25)
	{
			/* Redone by MSW 2000-11-26 to have much less code.  Also,
			  * bonuses and penalties will stack and add to existing values.
			  * of the item. */
			/* we are creating hp stuff! */
		case 0:
			tmp = 5;
			if (level < 10)
			{
				tmp += RANDOM() % 10;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.3f);
			}
			else if (level < 20)
			{
				tmp += 10 + RANDOM() % 10;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.32f);
			}
			else if (level < 30)
			{
				tmp += 15 + RANDOM() % 20;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.34f);
			}
			else if (level < 40)
			{
				tmp += 20 + RANDOM() % 21;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.38f);
			}
			else if (level < 50)
			{
				tmp += 25 + RANDOM() % 23;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.4f);
			}
			else if (level < 60)
			{
				tmp += 30 + RANDOM() % 25;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.42f);
			}
			else if (level < 80)
			{
				tmp += 40 + RANDOM() % 30;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.44f);
			}
			else
			{
				tmp += 50 + RANDOM() % 40;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.5f);
			}

			if (bonus < 0)
				tmp = -tmp;
			else
				op->item_level = level;

			op->stats.maxhp = tmp;
			break;
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			set_attr_value(&op->stats, r, (signed char)(bonus + get_attr_value(&op->stats, r)));
			break;

		case 7:
			op->stats.dam += bonus;
			if (bonus > 0 && (RANDOM() % 20 > 16 ? 1 : 0))
			{
				op->value = (int)((float)op->value * 1.3f);
				op->stats.dam++;
			}
			break;

		case 8:
			op->stats.wc += bonus;
			if (bonus > 0 && (RANDOM() % 20 > 16 ? 1 : 0))
			{
				op->value = (int)((float)op->value * 1.3f);
				op->stats.wc++;
			}
			break;

		case 9:
			/* hunger/sustenance */
			op->stats.food += bonus;
			if (bonus > 0 && (RANDOM() % 20 > 16 ? 1 : 0))
			{
				op->value = (int)((float)op->value * 1.2f);
				op->stats.food++;
			}
			break;

		case 10:
			op->stats.ac += bonus;
			if (bonus > 0 && (RANDOM() % 20 > 16 ? 1 : 0))
			{
				op->value = (int)((float)op->value * 1.3f);
				op->stats.ac++;
			}
			break;

		case 11:
			if (!RANDOM() % 3)
				goto make_prot_items;

			tmp = 3;
			if (level < 10)
			{
				tmp += RANDOM() % 3;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.3f);
			}
			else if (level < 20)
			{
				tmp += 3 + RANDOM() % 4;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.32f);
			}
			else if (level < 30)
			{
				tmp += 4 + RANDOM() % 6;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.34f);
			}
			else if (level < 40)
			{
				tmp += 6 + RANDOM() % 8;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.38f);
			}
			else if (level < 50)
			{
				tmp += 8 + RANDOM() % 10;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.4f);
			}
			else if (level < 60)
			{
				tmp += 10 + RANDOM() % 12;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.42f);
			}
			else if (level < 80)
			{
				tmp += 15 + RANDOM() % 15;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.44f);
			}
			else
			{
				tmp += 20 + RANDOM() % 20;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.5f);
			}

			if (bonus < 0)
				tmp = -tmp;
			else
				op->item_level = level;

			op->stats.maxsp = tmp;
			break;

		case 12:
			if (!RANDOM() % 3)
				goto make_prot_items;

			tmp = 3;
			if (level < 10)
			{
				tmp += RANDOM() % 3;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.3f);
			}
			else if (level < 20)
			{
				tmp += 3 + RANDOM() % 4;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.32f);
			}
			else if (level < 30)
			{
				tmp += 4 + RANDOM() % 6;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.34f);
			}
			else if (level < 40)
			{
				tmp += 6 + RANDOM() % 8;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.38f);
			}
			else if (level < 50)
			{
				tmp += 8 + RANDOM() % 10;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.4f);
			}
			else if (level < 60)
			{
				tmp += 10 + RANDOM() % 12;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.42f);
			}
			else if (level < 80)
			{
				tmp += 15 + RANDOM() % 15;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.44f);
			}
			else
			{
				tmp += 20 + RANDOM() % 20;
				if (bonus > 0)
					op->value = (int)((float)op->value * 1.5f);
			}

			if (bonus < 0)
				tmp = -tmp;
			else
				op->item_level = level;

			op->stats.maxsp = tmp;
			break;

		case 13:
make_prot_items:
		case 14:
		case 15:
		case 16:
		case 17:
		{
			int b = 5 + FABS(bonus), val, protect = RANDOM() % (NROFPROTECTIONS - 4 + off);

			/* Roughly generate a bonus between 100 and 35 (depending on the bonus) */
			val = 10 + RANDOM() % b + RANDOM() % b + RANDOM() % b + RANDOM() % b;

			/* Cursed items need to have higher negative values to equal out with
			 * positive values for how protections work out.  Put another
			 * little random element in since that they don't always end up with
			 * even values. */
			if (bonus < 0)
				val = 2 * -val - RANDOM() % b;

			/* Upper limit */
			if (val > 35) val = 35;
			b = 0;

			while (op->protection[protect] != 0)
			{
				/* Not able to find a free protection*/
				if (b++ >= 4)
					goto set_ring_bonus_jump1;
				protect=RANDOM() % (NROFPROTECTIONS - 4 + off);
			}
			op->protection[protect] = val;
			break;
		}
		case 18:
		case 19:
		{
			int b = 5 + FABS(bonus), val, resist = RANDOM() % (num_resist_table - 4 + off);

			/* Roughly generate a bonus between 100 and 35 (depending on the bonus) */
			val = 10 + RANDOM() % b + RANDOM() % b + RANDOM() % b + RANDOM() % b;

			/* Cursed items need to have higher negative values to equal out with
			 * positive values for how protections work out.  Put another
			 * little random element in since that they don't always end up with
			 * even values. */
			if (bonus < 0)
				val = 2 * -val - RANDOM() % b;

			/* Upper limit */
			if (val > 35)
				val = 35;

			b = 0;
			while (op->resist[resist_table[resist]] != 0)
			{
				/* Not able to find a free resistance */
				if (b++ >= 4)
					goto set_ring_bonus_jump1;
				resist = RANDOM() % (num_resist_table - 4 + off);
			}
			op->resist[resist_table[resist]] = val;
			/* We should probably do something more clever here to adjust value
			 * based on how good a resistance we gave. */
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
				/* regenerate hit points */
				op->stats.hp = 1;
				op->value = (int)((float)op->value * 1.3f);
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
				/* regenerate spell points */
				op->stats.sp = 1;
				op->value = (int)((float)op->value * 1.35f);
			}
			break;

			/* case 22: */
		default:
			if (!bonus)
				bonus = 1;

			/* Speed! */
			op->stats.exp += bonus;
			op->value = (int)((float)op->value * 1.4f);
			break;
	}

	if (bonus > 0)
		op->value = (int)((float)op->value * 2.0f * (float)bonus);
	else
		op->value = -(op->value * 2 * bonus) / 2;

	/* check possible overflow */
	if (op->value < 0)
		op->value = 0;

	return 1;
}

/* get_magic(diff) will return a random number between 0 and 4.
 * diff can be any value above 2.  The higher the diff-variable, the
 * higher is the chance of returning a low number.
 * It is only used in fix_generated_treasure() to set bonuses on
 * rings and amulets.
 * Another scheme is used to calculate the magic of weapons and armours. */
int get_magic(int diff)
{
	int i;

	if (diff < 3)
		diff = 3;

	for (i = 0; i < 4; i++)
		if (RANDOM() % diff)
			return i;

	return 4;
}

/* get a random spell from the spelllist.
 * used for item generation which uses spells. */
static int get_random_spell(int level, int flags)
{
	int i,tmp = RANDOM() % NROFREALSPELLS;

	/* we start somewhere random in the list and get the first fitting spell */
	/* spell matches when: is active, spell level is same or lower as difficuly/level
	 * of mob or map and the flags matches. */
	for (i = tmp; i < NROFREALSPELLS; i++)
	{
		if (spells[i].is_active && level >= spells[i].level && spells[i].spell_use & flags)
			return i;
	}

	for (i = 0; i < tmp; i++)
	{
		if (spells[i].is_active && level >= spells[i].level && spells[i].spell_use & flags)
			return i;
	}

	/* if we are here, there is no fitting spell */
	return SP_NO_SPELL;
}

#define DICE2	(get_magic(2)==2?2:1)
#define DICESPELL (RANDOM()%3+RANDOM()%3+RANDOM()%3+RANDOM()%3+RANDOM()%3)

/* fix_generated_item():  This is called after an item is generated, in
 * order to set it up right.  This produced magical bonuses, puts spells
 * into scrolls/books/wands, makes it unidentified, hides the value, etc. */
/* 4/28/96 added creator object from which op may now inherit properties based on
 * op->type. Right now, which stuff the creator passes on is object type
 * dependant. I know this is a spagetti manuever, but is there a cleaner
 * way to do this? b.t. */
/*
 * ! (flags & GT_ENVIRONMENT):
 *     Automatically calls fix_flesh_item().
 *
 * flags & FLAG_STARTEQUIP:
 *     Sets FLAG_STARTEQIUP on item if appropriate, or clears the item's
 *     value. */
int fix_generated_item(object **op_ptr, object *creator, int difficulty, int a_chance, int t_style, int max_magic, int fix_magic, int chance_magic, int flags)
{
	/* just to make things easy */
	object *op = *op_ptr;
	int temp, retval = 0, was_magic = op->magic;
	int too_many_tries = 0, is_special = 0;

	/* safety & to prevent polymorphed objects giving attributes */
	if (!creator || creator->type == op->type)
		creator = op;

	if (difficulty < 1)
		difficulty = 1;

	if (op->type != POTION && op->type != SCROLL)
	{
		if ((!op->magic && max_magic) || fix_magic)
			set_magic(difficulty, op, max_magic, fix_magic, chance_magic, flags);

		if (a_chance != 0)
		{
			if ((!was_magic && !(RANDOM() % CHANCE_FOR_ARTIFACT)) || op->type == HORN || difficulty >= 999 || ((RANDOM() % 100) + 1) <= a_chance)
				retval = generate_artifact(op, difficulty, t_style, a_chance);
		}
	}

	/* Only modify object if not special */
	if (!op->title || op->type == RUNE)
	{
		switch (op->type)
		{
				/* we create scrolls now in artifacts file too */
			case SCROLL:
				while (op->stats.sp == SP_NO_SPELL)
				{
					generate_artifact(op, difficulty, t_style, 100);
					if (too_many_tries++ > 3)
						break;
				}

#if 0
				if ((op->stats.sp = get_random_spell(difficulty, SPELL_USE_SCROLL)) == SP_NO_SPELL)
					break;
#endif

				/* ok, forget it... */
				if (op->stats.sp == SP_NO_SPELL)
					break;

				/* marks as magical */
				SET_FLAG(op, FLAG_IS_MAGICAL);
				/* charges */
				op->stats.food = RANDOM() % spells[op->stats.sp].charges + 1;
				temp = (((difficulty * 100) - (difficulty * 20)) + (difficulty * (RANDOM() % 35))) / 100;

				if (temp < 1)
					temp = 1;
				else if (temp > 110)
					temp = 110;

				op->level = temp;
				if (temp < spells[op->stats.sp].level)
					temp = spells[op->stats.sp].level;
#if 0
				op->value = (int) (85.0f * spells[op->stats.sp].value_mul);
				op->nrof = RANDOM() % spells[op->stats.sp].scrolls + 1;
#endif
				break;

			case POTION:
			{
				/* balm */
				if (!op->sub_type1)
				{
					if ((op->stats.sp = get_random_spell(difficulty, SPELL_USE_BALM)) == SP_NO_SPELL)
						break;

					SET_FLAG(op, FLAG_IS_MAGICAL);
					op->value = (int) (150.0f * spells[op->stats.sp].value_mul);
				}
				/* dust */
				else if (op->sub_type1 >128)
				{
					if ((op->stats.sp = get_random_spell(difficulty, SPELL_USE_DUST)) == SP_NO_SPELL)
						break;

					SET_FLAG(op, FLAG_IS_MAGICAL);
					op->value = (int)  (125.0f * spells[op->stats.sp].value_mul);
				}
				else
				{
					while (!(is_special = special_potion(op)) && op->stats.sp == SP_NO_SPELL)
					{
						generate_artifact(op, difficulty, t_style, 100);

						if (too_many_tries++ > 3)
							goto jump_break1;
					}
				}

				temp = (((difficulty * 100) - (difficulty * 20)) + (difficulty * (RANDOM() % 35))) / 100;

				if (temp < 1)
					temp = 1;
				else if (temp > 110)
					temp = 110;

				if (!is_special && temp < spells[op->stats.sp].level)
					temp = spells[op->stats.sp].level;

				op->level = temp;
				/* chance to make special potions damned or cursed.
				 * The chance is somewhat high to make the game more
				 * difficult. Applying this potions without identify
				 * is a great risk! */
				if (is_special && !(flags & GT_ONLY_GOOD))
				{
					if (RANDOM() % 2)
						SET_FLAG(op, FLAG_CURSED);
					else if (RANDOM() % 2)
						SET_FLAG(op, FLAG_DAMNED);
				}

jump_break1:
				break;
			}

			case AMULET:
				/* Since it's not just decoration */
				if (op->arch == amulet_arch)
					op->value *= 5;

			case RING:
				if (op->arch == NULL)
				{
					remove_ob(op);
					*op_ptr = op = NULL;
					break;
				}

				/* It's a special artefact!*/
				if (op->arch != ring_arch && op->arch != amulet_arch)
					break;

				/* We have no special ring - now we create one.
				 * we first get us a value, material & face
				 * changed prototype. Then we cast the powers over it. */
				if (op->arch == ring_arch)
				{
					/* this is called before we inserted it in the map or elsewhere */
					if (!QUERY_FLAG(op, FLAG_REMOVED))
						remove_ob(op);

					*op_ptr = op = arch_to_object(ring_arch_normal);
					generate_artifact(op, difficulty, t_style, 99);
				}

				if (!(flags & GT_ONLY_GOOD) && !(RANDOM() % 3))
					SET_FLAG(op, FLAG_CURSED);

				set_ring_bonus(op, QUERY_FLAG(op, FLAG_CURSED) ? -DICE2 : DICE2, difficulty);

				/* Amulets have only one ability */
				if (op->type != RING)
					break;

				if (!(RANDOM() % 4))
				{
					int d = (RANDOM() % 2 || QUERY_FLAG(op, FLAG_CURSED)) ? -DICE2 : DICE2;

					if (set_ring_bonus(op, d, difficulty))
						op->value = (int)((float)op->value * 1.95f);

					if (!(RANDOM() % 4))
					{
						int d = (RANDOM() % 3 || QUERY_FLAG(op, FLAG_CURSED)) ? -DICE2 : DICE2;

						if (set_ring_bonus(op, d, difficulty))
							op->value = (int)((float)op->value * 1.95f);
					}
				}
				break;

			case BOOK:
				/* Is it an empty book?, if yes lets make a special
				 * msg for it, and tailor its properties based on the
				 * creator and/or map level we found it on. */
				if (!op->msg && RANDOM() % 10)
				{
					/* set the book level properly */
					if (creator->level == 0 || IS_LIVE(creator))
					{
						if (op->map && op->map->difficulty)
							op->level = RANDOM() % (op->map->difficulty) + RANDOM() % 10 + 1;
						else
							op->level = RANDOM() % 20 + 1;
					}
					else
						op->level = RANDOM() % creator->level;

					tailor_readable_ob(op, (creator && creator->stats.sp) ? creator->stats.sp : -1);

					/* books with info are worth more! */
					if (op->msg && strlen(op->msg) > 0)
						op->value *= ((op->level > 10 ? op->level : (op->level + 1) / 2) * ((strlen(op->msg) / 250) + 1));

					/* creator related stuff */

					/* for library, chained books! */
					if (creator->type != MONSTER && QUERY_FLAG(creator, FLAG_NO_PICK))
						SET_FLAG(op, FLAG_NO_PICK);

					/* for check_inv floors */
					if (creator->slaying && !op->slaying)
						FREE_AND_COPY_HASH(op->slaying, creator->slaying);

					/* add exp so reading it gives xp (once)*/
					op->stats.exp = op->value > 10000 ? op->value / 5 : op->value / 10;
				}
				break;

			case SPELLBOOK:
				LOG(llevDebug, "DEBUG: fix_generated_item(): called for disabled object SPELLBOOK (%s)\n", query_name(op, NULL));
				break;

			case WAND:
				if ((op->stats.sp = get_random_spell(difficulty, SPELL_USE_WAND)) == SP_NO_SPELL)
					break;

				/* marks as magical */
				SET_FLAG(op, FLAG_IS_MAGICAL);
				/* charges */
				op->stats.food = (RANDOM() % spells[op->stats.sp].charges + 1) + 12;

				temp = (((difficulty * 100) - (difficulty * 20)) + (difficulty * (RANDOM() % 35))) / 100;

				if (temp < 1)
					temp = 1;
				else if (temp > 110)
					temp = 110;

				if (temp < spells[op->stats.sp].level)
					temp = spells[op->stats.sp].level;

				op->level = temp;
				op->value = (int) (16.3f * spells[op->stats.sp].value_mul);
				break;

			case HORN:
				if ((op->stats.sp = get_random_spell(difficulty, SPELL_USE_HORN)) == SP_NO_SPELL)
					break;

				/* marks as magical */
				SET_FLAG(op, FLAG_IS_MAGICAL);
				if (op->stats.maxhp)
					op->stats.maxhp += RANDOM() % op->stats.maxhp;

				op->stats.hp = op->stats.maxhp;
				temp = (((difficulty * 100) - (difficulty * 20)) + (difficulty * (RANDOM() % 35))) / 100;

				if (temp < 1)
					temp = 1;
				else if (temp > 110)
					temp = 110;

				op->level = temp;
				if (temp < spells[op->stats.sp].level)
					temp = spells[op->stats.sp].level;

				op->value = (int) (1850.0f * spells[op->stats.sp].value_mul);
				break;

			case ROD:
				if ((op->stats.sp = get_random_spell(difficulty, SPELL_USE_ROD)) == SP_NO_SPELL)
					break;

				/* marks as magical */
				SET_FLAG(op, FLAG_IS_MAGICAL);
				if (op->stats.maxhp)
					op->stats.maxhp += RANDOM() % op->stats.maxhp;

				op->stats.hp = op->stats.maxhp;
				temp = (((difficulty * 100) - (difficulty * 20)) + (difficulty * (RANDOM() % 35))) / 100;

				if (temp < 1)
					temp = 1;
				else if (temp > 110)
					temp = 110;

				op->level = temp;
				if (temp < spells[op->stats.sp].level)
					temp = spells[op->stats.sp].level;

				op->value = (int) (1850.0f * spells[op->stats.sp].value_mul);
				break;

			case RUNE:
				/* artifact AND normal treasure runes!! */
				(*trap_adjust_func)(op, difficulty);
				break;
		}
	}
	/* ->title != NULL */
	else
	{
		switch (op->type)
		{
				/* lets check we have a slaying/assassination arrow */
			case ARROW:
				/* compare hash ptrs */
				if (op->slaying == treasure_string_none)
				{
					int tmp = RANDOM() % global_race_counter;
					racelink *list;

					/* get the right race */
					for (list = first_race; list && tmp; list = list->next, tmp--);

					FREE_AND_COPY_HASH(op->slaying, list->name);
				}
				break;
		}
	}

	if ((flags & GT_NO_VALUE) && op->type != MONEY)
		op->value = 0;

	if (flags & GT_STARTEQUIP)
	{
		if (op->nrof < 2 && op->type != CONTAINER && op->type != MONEY && !QUERY_FLAG(op, FLAG_IS_THROWN))
			SET_FLAG(op, FLAG_STARTEQUIP);
		else if (op->type != MONEY)
			op->value = 0;
	}

	if (!(flags & GT_ENVIRONMENT))
		fix_flesh_item (op, creator);

	return retval;
}

/*******************************************
 * CODE DEALING WITH ARTIFACTS STARTS HERE *
 ******************************************/

/* Allocate and return the pointer to an empty artifactlist structure. */
static artifactlist *get_empty_artifactlist(void)
{
	artifactlist *tl = (artifactlist *) malloc(sizeof(artifactlist));

	if (tl == NULL)
		LOG(llevError, "ERROR: get_empty_artifactlist(): Out of memory.\n");

	tl->next = NULL;
	tl->items = NULL;
	tl->total_chance = 0;
	return tl;
}

/* Allocate and return the pointer to an empty artifact structure. */
static artifact *get_empty_artifact(void)
{
	artifact *t = (artifact *) malloc(sizeof(artifact));

	if (t == NULL)
		LOG(llevError, "ERROR: get_empty_artifact(): Out of memory.\n");

	t->next = NULL;
	t->name = NULL;
	t->def_at_name = NULL;
	t->t_style = 0;
	t->chance = 0;
	t->difficulty = 0;
	t->allowed = NULL;

	return t;
}

/* Searches the artifact lists and returns one that has the same type
 * of objects on it. */
artifactlist *find_artifactlist(int type)
{
	artifactlist *al;

	for (al = first_artifactlist; al != NULL; al = al->next)
		if (al->type == type)
			return al;

	return NULL;
}

/* not used ATM - MT 2003 */
artifact *find_artifact(const char *name)
{
	artifactlist *al;
	artifact *art = NULL;

	/* this is the brute force way. We should use in the next release a hash table for it. MT */
	for (al = first_artifactlist; al != NULL; al = al->next)
	{
		art = al->items;
		do
		{
			if (art->name && !strcmp(art->name, name))
				return art;

			art = art->next;
		}
		while (art != NULL);
	}

	return NULL;
}

/* find the default archetype from artifact by intern artifactlist name */
archetype *find_artifact_archtype(const char *name)
{
	artifactlist *al;
	artifact *art = NULL;

	/* this is the brute force way. We should use in the next release a hash table for it. MT */
	for (al = first_artifactlist; al != NULL; al = al->next)
	{
		art = al->items;
		do
		{
			if (art->name && !strcmp(art->name, name))
				return &art->def_at;
			art = art->next;
		}
		while (art != NULL);
	}

	return NULL;
}

/* For debugging purposes.  Dumps all tables. */
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
					LOG(llevInfo, "%s,", next->name);

				LOG(llevInfo, "\n");
			}
		}
	}
	LOG(llevInfo, "\n");
}

/* For debugging purposes.  Dumps all treasures recursively (see below). */
void dump_monster_treasure_rec (const char *name, treasure *t, int depth)
{
	treasurelist *tl;
	int i;

	if (depth > 100)
		return;

	while (t != NULL)
	{
		if (t->name != NULL)
		{
			for (i = 0; i < depth; i++)
				LOG(llevInfo, "  ");

			LOG(llevInfo, "{   (list: %s)\n", t->name);
			tl = find_treasurelist(t->name);
			dump_monster_treasure_rec(name, tl->items, depth + 2);

			for (i = 0; i < depth; i++)
				LOG(llevInfo, "  ");

			LOG(llevInfo, "}   (end of list: %s)\n", t->name);
		}
		else
		{
			for (i = 0; i < depth; i++)
				LOG(llevInfo, "  ");

			if (t->item->clone.type == FLESH)
				LOG(llevInfo, "%s's %s\n", name, t->item->clone.name);
			else
				LOG(llevInfo, "%s\n", t->item->clone.name);
		}

		if (t->next_yes != NULL)
		{
			for (i = 0; i < depth; i++)
				LOG(llevInfo, "  ");

			LOG(llevInfo, " (if yes)\n");
			dump_monster_treasure_rec(name, t->next_yes, depth + 1);
		}

		if (t->next_no != NULL)
		{
			for (i = 0; i < depth; i++)
				LOG(llevInfo, "  ");

			LOG(llevInfo, " (if no)\n");
			dump_monster_treasure_rec(name, t->next_no, depth + 1);
		}
		t = t->next;
	}
}

static int legal_artifact_combination(object *op, artifact *art)
{
	int neg, success = 0;
	linked_char *tmp;
	const char *name;

	/* Ie, "all" */
	if (art->allowed == (linked_char *) NULL)
		return 1;

	for (tmp = art->allowed; tmp; tmp = tmp->next)
	{
#ifdef TREASURE_VERBOSE
		LOG(llevDebug, "legal_art: %s\n", tmp->name);
#endif
		if (*tmp->name == '!')
			name = tmp->name + 1, neg = 1;
		else
			name = tmp->name, neg = 0;

		/* If we match name, then return the opposite of 'neg' */
		if (!strcmp(name, op->name) || (op->arch && !strcmp(name, op->arch->name)))
			return !neg;

		/* Set success as true, since if the match was an inverse, it means
		  * everything is allowed except what we match */
		else if (neg)
			success = 1;
	}
	return success;
}

/* Fixes the given object, giving it the abilities and titles
 * it should have due to the second artifact-template. */
void give_artifact_abilities(object *op, artifact *art)
{
	int tmp_value = op->value;

	op->value = 0;
	if (!load_object(art->parse_text, op, NULL, LO_MEMORYMODE, MAP_ARTIFACT))
		LOG(llevError, "ERROR: give_artifact_abilities(): load_object() error (ob: %s art: %s).\n", op->name, art->name);

	/* this will solve the problem to adjust the value for different items
	 * of same artification. Also we can safely use negative values. */
	op->value += tmp_value;

	if (op->value < 0)
		op->value = 0;

#if 0
	/* Bit verbose, but keep it here until next time I need it... */
	{
		char identified = QUERY_FLAG(op, FLAG_IDENTIFIED);
		SET_FLAG(op, FLAG_IDENTIFIED);
		LOG(llevDebug, "Generated artifact %s %s [%s]\n", op->name, op->title, describe_item(op));
		if (!identified)
			CLEAR_FLAG(op, FLAG_IDENTIFIED);
	}
#endif

	return;
}

/* Decides randomly which artifact the object should be
 * turned into.  Makes sure that the item can become that
 * artifact (means magic, difficulty, and Allowed fields properly).
 * Then calls give_artifact_abilities in order to actually create
 * the artifact. */
int generate_artifact(object *op, int difficulty, int t_style, int a_chance)
{
	artifactlist *al;
	artifact *art;
	artifact *art_tmp = NULL;
	int i, chance_tmp = 0;

	al = find_artifactlist(op->type);

	/* NOW we overrule unset to 0 */
	if (t_style == T_STYLE_UNSET)
		t_style = 0;

	if (al == NULL)
	{
#ifdef TREASURE_VERBOSE
		LOG(llevDebug, "Couldn't change %s into artifact - no table.\n", op->name);
#endif
		return 0;
	}

	for (i = 0; i < ARTIFACT_TRIES; i++)
	{
		int roll = RANDOM() % al->total_chance;

		for (art = al->items; art != NULL; art = art->next)
		{
			roll -= art->chance;
			if (roll < 0)
				break;
		}

		if (art == NULL || roll >= 0)
		{
			LOG(llevBug, "BUG: Got null entry and non zero roll in generate_artifact, type %d\n", op->type);
			return 0;
		}

		/* Map difficulty not high enough OR the t_style is set and don't match */
		/*LOG(-1, "ARTIFACT: for %s \n%s\n t_style %d art->t_style:%d\n", query_name(op), art->parse_text, t_style, art->t_style);*/
		if (difficulty < art->difficulty || (t_style == -1 && (art->t_style && art->t_style != T_STYLE_UNSET)) || (t_style && (art->t_style != t_style && art->t_style != T_STYLE_UNSET)))
			continue;

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

	/* if we are here then we failed to generate a artifact by chance.
	 * the reasons can be many - most times we just skipped over the
	 * useful one.
	 * If (and only if) a a_chance  - then now we force our way:
	 * - lets get (if there are one) a legal artifact with the highest chance. */
	if (a_chance > 0)
	{
		for (art = al->items; art != NULL; art = art->next)
		{
			if (art->chance <= chance_tmp)
				continue;

			if (difficulty < art->difficulty || (t_style == -1 && (art->t_style && art->t_style != T_STYLE_UNSET)) || (t_style && (art->t_style != t_style && art->t_style != T_STYLE_UNSET)))
				continue;

			if (!legal_artifact_combination(op, art))
				continue;

			/* there we go! */
			art_tmp = art;
		}
	}

	/* now we MUST have one - if there was at last one legal possible artifact */
	if (art_tmp)
		give_artifact_abilities(op, art_tmp);

	return 1;
}


/* fix_flesh_item() - objects of type FLESH are similar to type
 * FOOD, except they inherit properties (name, food value, etc).
 * based on the original owner (or 'donor' if you like). -b.t. */
void fix_flesh_item(object *item, object *donor)
{
	char tmpbuf[MAX_BUF];
	int i;

	if (item->type == FLESH && donor)
	{
		/* change the name */
		sprintf(tmpbuf, "%s's %s", donor->name, item->name);
		FREE_AND_COPY_HASH(item->name, tmpbuf);

		/* weight is FLESH weight/100 * donor */
		if ((item->weight = (signed long) (((double)item->weight / (double)100.0) * (double)donor->weight)) == 0)
			item->weight = 1;

		/* value is multiplied by level of donor */
		item->value *= isqrt(donor->level * 2);

		/* food value */
		item->stats.food += (donor->stats.hp / 100) + donor->stats.Con;

		/* flesh items inherit some abilities of donor, but not
		 * full effect. */
		for (i = 0; i < NROFATTACKS; i++)
			item->resist[i] = donor->resist[i] / 2;

		/* item inherits donor's level (important for quezals) */
		item->level = donor->level;

		/* if donor has some attacktypes, the flesh is poisonous */
		if (donor->attack[ATNR_POISON])
			item->type = POISON;

		if (donor->attack[ATNR_ACID])
			item->stats.hp = -1 * item->stats.food;

		SET_FLAG(item, FLAG_NO_STEAL);
	}
}

void free_treasurestruct(treasure *t)
{
	if (t->next)
		free_treasurestruct(t->next);

	if (t->next_yes)
		free_treasurestruct(t->next_yes);

	if (t->next_no)
		free_treasurestruct(t->next_no);

	FREE_AND_CLEAR_HASH2(t->name);
	FREE_AND_CLEAR_HASH2(t->change_arch.name);
	FREE_AND_CLEAR_HASH2(t->change_arch.slaying);
	FREE_AND_CLEAR_HASH2(t->change_arch.title);
	free(t);
}

void free_charlinks(linked_char *lc)
{
	if (lc->next)
		free_charlinks(lc->next);

	free(lc);
}

void free_artifact(artifact *at)
{
	FREE_AND_CLEAR_HASH2(at->name);

	if (at->next)
		free_artifact(at->next);

	if (at->allowed)
		free_charlinks(at->allowed);

	if (at->parse_text)
		free(at->parse_text);

	FREE_AND_CLEAR_HASH2(at->def_at.clone.name);
	FREE_AND_CLEAR_HASH2(at->def_at.clone.race);
	FREE_AND_CLEAR_HASH2(at->def_at.clone.slaying);
	FREE_AND_CLEAR_HASH2(at->def_at.clone.msg);
	FREE_AND_CLEAR_HASH2(at->def_at.clone.title);
	free(at);
}

void free_artifactlist(artifactlist *al)
{
	artifactlist *nextal;

	for (al = first_artifactlist; al != NULL; al = nextal)
	{
		nextal = al->next;

		if (al->items)
			free_artifact(al->items);

		free(al);
	}
}

void free_all_treasures()
{
	treasurelist *tl, *next;

	for (tl = first_treasurelist; tl != NULL; tl = next)
	{
		next = tl->next;
		FREE_AND_CLEAR_HASH2(tl->name);

		if (tl->items)
			free_treasurestruct(tl->items);

		free(tl);
	}

	free_artifactlist(first_artifactlist);
	FREE_AND_CLEAR_HASH2(treasure_string_none);
}

/* set material_real... use fixed number when start == end or random range */
static inline void set_material_real(object *op, struct _change_arch *change_arch)
{
	if (change_arch->item_race != -1)
		op->item_race = (uint8) change_arch->item_race;

	/* this must be tested - perhaps we want that change_arch->material
	 * also overrule the material_real -1 marker? */
	/* skip all objects with -1 as marker */
	if (op->material_real == -1)
	{
		/* WARNING: material_real == -1 skips also the quality modifier.
		 * this is really for objects which don't fit in the material/quality
		 * system (like system objects, forces, effects and stuff). */
		op->material_real = 0;
		return;
	}

	/* we overrule the material settings in any case when this is set */
	if (change_arch->material != -1)
	{
		op->material_real = change_arch->material;
		/* this is tricky: material_range will be used
		 * for change_arch->material if change_arch->material
		 * is set - if not, it is used for material_quality
		 * if that is set. */

		/* skip if material == 0 (aka neutralized material setting) */
		/* change_arch->material_range == 1 means: 0 or +1 */
		if (change_arch->material_range > 0 && change_arch->material)
			op->material_real += (RANDOM() % (change_arch->material_range + 1));
	}
	/* if == 0, grab a valid material class.
	 * we should assign to all objects a valid
	 * material_real value to avoid problems here.
	 * So, this is a hack */
	else if (!op->material_real && op->material != M_ADAMANT)
	{
		if (op->material & M_IRON)
			op->material_real = M_START_IRON;
		else if (op->material & M_LEATHER)
			op->material_real = M_START_LEATHER;
		else if (op->material & M_PAPER)
			op->material_real = M_START_PAPER;
		else if (op->material & M_GLASS)
			op->material_real = M_START_GLASS;
		else if (op->material & M_WOOD)
			op->material_real = M_START_WOOD;
		else if (op->material & M_ORGANIC)
			op->material_real = M_START_ORGANIC;
		else if (op->material & M_STONE)
			op->material_real = M_START_STONE;
		else if (op->material & M_CLOTH)
			op->material_real = M_START_CLOTH;
		else if (op->material & M_ADAMANT)
			op->material_real = M_START_ADAMANT;
		else if (op->material & M_LIQUID)
			op->material_real = M_START_LIQUID;
		else if (op->material & M_SOFT_METAL)
			op->material_real = M_START_SOFT_METAL;
		else if (op->material & M_BONE)
			op->material_real = M_START_BONE;
		else if (op->material & M_ICE)
			op->material_real = M_START_ICE;
	}

	/* now lets see we have seomthing to change */

	/* ok - now we do some work: we define a (material) quality and try to find
	 * a best matching pre-set material_real for that item.
	 * this is a bit more complex but we are with that free to define
	 * different materials without having a strong fixed material
	 * table. */
	if (change_arch->material_quality != -1)
	{
		int i, q_tmp = -1;
		int m_range = change_arch->material_quality;

		if (change_arch->material_range > 0)
			m_range += (RANDOM() % (change_arch->material_range + 1));

		if (op->material_real)
		{
			int m_tmp = op->material_real / NROFMATERIALS_REAL;

			/* the first entry of the material_real of material table */
			m_tmp = m_tmp * 64 + 1;

			/* some material_real stuff works difference - for example
			 * organics (which defines scales, chitin and stuff).
			 * At this point we exclude the different used tables */

			/* we should add paper & cloth here too later */
			if (m_tmp == M_START_IRON || m_tmp == M_START_WOOD || m_tmp == M_START_LEATHER)
			{
				for (i = 0; i < NROFMATERIALS_REAL; i++)
				{
					/* we have a full hit */
					if (material_real[m_tmp + i].quality == m_range)
					{
						op->material_real = m_tmp + i;
						goto set_material_real;
					}

					/* find nearest quality we want */
					if (material_real[m_tmp + i].quality >= change_arch->material_quality && material_real[m_tmp + i].quality <= m_range && material_real[m_tmp + i].quality>q_tmp)
						q_tmp = m_tmp + i;
				}

				/* if we haven no match, we simply use the (always valid) first material_real entry
				 * and forcing the material_quality to quality! */
				if (q_tmp == -1)
				{
					op->material_real = m_tmp;
					op->item_quality = change_arch->material_quality;
					op->item_condition = op->item_quality;
					return;
				}

				/* thats now our best match! */
				op->material_real = q_tmp;
			}
			/* exluded material table! */
			else
			{
				op->item_quality = m_range;
				op->item_condition = op->item_quality;
				return;
			}
		}
		/* we have material_real == 0 but we modify at last the quality! */
		else
		{
			op->item_quality = m_range;
			op->item_condition = op->item_quality;
			return;
		}
	}

set_material_real:
	/* adjust quality - use material default value or quality adjustment */
	if (change_arch->quality != -1)
		op->item_quality = change_arch->quality;
	else
		op->item_quality = material_real[op->material_real].quality;

	if (change_arch->quality_range > 0)
	{
		op->item_quality += (RANDOM() % (change_arch->quality_range + 1));
		if (op->item_quality > 100)
			op->item_quality = 100;
	}

	op->item_condition = op->item_quality;
}

/* For debugging purposes.  Dumps all treasures for a given monster.
 * Created originally by Raphael Quinet for debugging the alchemy code. */
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
				dump_monster_treasure_rec (at->clone.name, at->clone.randomitems->items, 1);
			else
				LOG(llevInfo, "(nothing)\n");

			LOG(llevInfo, "\n");
			found++;
		}
	}

	if (found == 0)
		LOG(llevInfo, "No objects have the name %s!\n\n", name);
}

/* these function fetch the "enviroment level" for
 * treasure generation for the given object.
 * It checks first the object itself has a level.
 * If not, it checks the object is in a map.
 * If so, it use the map level.
 * If not, it recursive checks then ->env for level.
 * If there is none, it checks the last env maps.
 * If there is none, it use level 1 as default.
 * That gives us always a valid level.
 * This function never returns a value <1 ! */
int get_enviroment_level(object *op)
{
	object *env;

	if (!op)
	{
		LOG(llevBug, "get_enviroment_level() called for NULL object!\n");
		return 1;
	}

	/* return object level or map level... */
	if (op->level)
		return op->level;

	if (op->map)
		return op->map->difficulty ? op->map->difficulty : 1;

	/* ok, its not so easy... lets check for env */
	env = op->env;
	while (env)
	{
		if (env->level)
			return env->level;

		if (env->map)
			return env->map->difficulty ? env->map->difficulty : 1;

		env = env->env;
	}

	/* if we are here there is nothing which gives
	 * us a valid level or map difficulty...
	 * we give up and return a simple level 1.
	 * note: this don't *must* be a bug or error.
	 * There are possible setups where this value 1
	 * its the right value. */
	return 1;
}

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
/* recursived checks the linked list.  Treasurelist is passed only
 * so that the treasure name can be printed out */
/* TODO: i run in the problem, that i used the same treasurelist name in the treasures
 * file twice - the treasure list loader don't check for it. We should in include a check
 * here. */
static void check_treasurelist(treasure *t, treasurelist *tl)
{
	if (t->item == NULL && t->name == NULL)
		LOG(llevError, "ERROR: Treasurelist %s has element with no name or archetype\n", tl->name);

	if (t->chance >= 100 && t->next_yes && (t->next || t->next_no))
		LOG(llevBug, "BUG: Treasurelist %s has element that has 100% generation, next_yes field as well as next or next_no\n", tl->name);

	/* find_treasurelist will print out its own error message */
	if (t->name && strcmp(t->name, "NONE"))
		(void) find_treasurelist(t->name);

	if (t->next)
		check_treasurelist(t->next, tl);

	if (t->next_yes)
		check_treasurelist(t->next_yes, tl);

	if (t->next_no)
		check_treasurelist(t->next_no, tl);
}
#endif
