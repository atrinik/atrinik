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
 * Arch related functions. */

#include <global.h>
#include <loader.h>

/** If set, does a little timing on the archetype load. */
#define TIME_ARCH_LOAD 0

/** The arch table. */
static archetype *arch_table[ARCHTABLE];
/** How many strcmp's */
int arch_cmp = 0;
/** How many searches */
int arch_search = 0;
/** True if doing arch initialization */
int arch_init;

static archetype *find_archetype_by_object_name(const char *name);
static void clear_archetable();
static void init_archetable();
static archetype *get_archetype_struct();
static void first_arch_pass(FILE *fp);
static void second_arch_pass(FILE *fp_start);
static void load_archetypes();
static object *create_singularity(const char *name);
static unsigned long hasharch(const char *str, int tablesize);
static void add_arch(archetype *at);
static archetype *type_to_archetype(int type);

/**
 * This function retrieves an archetype given the name that appears
 * during the game (for example, "writing pen" instead of "stylus").
 * It does not use the hashtable system, but browse the whole archlist each time.
 * I suggest not to use it unless you really need it because of performance issue.
 * It is currently used by scripting extensions (create-object).
 * @param name The name we're searching for (ex: "writing pen").
 * @return The archetype found or NULL if nothing was found. */
static archetype *find_archetype_by_object_name(const char *name)
{
	archetype *at;

	if (name == NULL)
	{
		return NULL;
	}

	for (at = first_archetype; at != NULL; at = at->next)
	{
		if (!strcmp(at->clone.name, name))
		{
			return at;
		}
	}

	return NULL;
}

/**
 * Creates an object given the name that appears during the game
 * (for example, "writing pen" instead of "stylus").
 * @param name The name we're searching for (ex: "writing pen"), must not
 * be NULL.
 * @return A corresponding object if found; a singularity object if not
 * found. */
object *get_archetype_by_object_name(const char *name)
{
	archetype *at;
	char tmpname[MAX_BUF];
	size_t i;

	strncpy(tmpname, name, MAX_BUF - 1);
	tmpname[MAX_BUF - 1] = '\0';

	for (i = strlen(tmpname); i > 0; i--)
	{
		tmpname[i] = '\0';
		at = find_archetype_by_object_name(tmpname);

		if (at != NULL)
		{
			return arch_to_object(at);
		}
	}

	return create_singularity(name);
}

/**
 * Get the skill object for skill ID.
 * @param skillnr The skill number to find object for.
 * @return Object if found, NULL otherwise. */
archetype *get_skill_archetype(int skillnr)
{
	archetype *at;

	for (at = first_archetype; at != NULL; at = at->next)
	{
		if (at->clone.type == SKILL && at->clone.stats.sp == skillnr)
		{
			return at;
		}
	}

	return NULL;
}

/**
 * This is a subset of the parse_id command. Basically, name can be a
 * string seperated lists of things to match, with certain keywords.
 *
 * Calling function takes care of what action might need to be done and
 * if it is valid (pickup, drop, etc).
 *
 * Brief outline of the procedure:
 *
 * We take apart the name variable into the individual components.
 * cases for 'all' and unpaid are pretty obvious.
 *
 * Next, we check for a count (either specified in name, or in the
 * player object). If count is 1, make a quick check on the name. If
 * count is >1, we need to make plural name.  Return if match.
 *
 * Last, make a check on the full name.
 * @param pl Player (only needed to set count properly).
 * @param op The item we're trying to match.
 * @param name String we're searching.
 * @return Non-zero if we have a match. A higher value means a better
 * match. Zero means no match. */
int item_matched_string(object *pl, object *op, const char *name)
{
	char *cp, local_name[MAX_BUF];
	int count, retval = 0;

	/* strtok is destructive to name */
	strcpy(local_name, name);

	for (cp = strtok(local_name, ","); cp; cp = strtok(NULL, ","))
	{
		/* Get rid of spaces */
		while (cp[0] == ' ')
		{
			cp++;
		}

		/* All is a very generic match - low match value */
		if (!strcmp(cp, "all"))
		{
			return 1;
		}

		/* Unpaid is a little more specific */
		if (!strcmp(cp, "unpaid") && QUERY_FLAG(op, FLAG_UNPAID))
		{
			return 2;
		}

		if (!strcmp(cp, "cursed") && (QUERY_FLAG(op, FLAG_KNOWN_CURSED) || QUERY_FLAG(op, FLAG_IDENTIFIED)) && (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)))
		{
			return 2;
		}

		if (!strcmp(cp, "unlocked") && !QUERY_FLAG(op, FLAG_INV_LOCKED))
		{
			return 2;
		}

		/* Allow for things like '100 arrows' */
		if ((count = atoi(cp)) != 0)
		{
			cp = strchr(cp, ' ');

			/* Get rid of spaces */
			while (cp && cp[0] == ' ')
			{
				cp++;
			}
		}
		else
		{
			if (pl->type == PLAYER)
			{
				count = CONTR(pl)->count;
			}
			else
			{
				count = 0;
			}
		}

		if (!cp || cp[0] == '\0' || count < 0)
		{
			return 0;
		}

		/* Base name matched - not bad */
		if (strcasecmp(cp, op->name) == 0 && !count)
		{
			return 4;
		}
		/* Need to plurify name for proper match */
		else if (count > 1)
		{
			char newname[MAX_BUF];
			strcpy(newname, op->name);

			if (!strcasecmp(newname, cp))
			{
				/* May not do anything */
				CONTR(pl)->count = count;
				return 6;
			}
		}
		else if (count == 1)
		{
			if (!strcasecmp(op->name, cp))
			{
				/* May not do anything */
				CONTR(pl)->count = count;
				return 6;
			}
		}

		if (!strcasecmp(cp, query_name(op, NULL)))
		{
			retval = 20;
		}
		else if (!strcasecmp(cp, query_short_name(op, NULL)))
		{
			retval = 18;
		}
		else if (!strcasecmp(cp, query_base_name(op, pl)))
		{
			retval = 16;
		}
		else if (!strncasecmp(cp, query_base_name(op, pl), MIN(strlen(cp), strlen(query_base_name(op, pl)))))
		{
			retval = 14;
		}

		if (retval)
		{
			if (pl->type == PLAYER)
			{
				CONTR(pl)->count = count;
			}

			return retval;
		}
	}

	return 0;
}

/**
 * Initializes the internal linked list of archetypes (read from file).
 * Some commonly used archetype pointers like ::empty_archetype,
 * ::base_info_archetype are initialized.
 *
 * Can be called multiple times, will just return. */
void init_archetypes()
{
	/* Only do this once */
	if (first_archetype != NULL)
	{
		return;
	}

	arch_init = 1;
	load_archetypes();
	arch_init = 0;
	empty_archetype = find_archetype("empty_archetype");
	base_info_archetype = find_archetype("base_info");
	wp_archetype = find_archetype("waypoint");
}

/**
 * Prints how efficient the hashtable used for archetypes has been in.
 * @param op Player object to print the information to. */
void arch_info(object *op)
{
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "%d searches and %d strcmp()'s", arch_search, arch_cmp);
	new_draw_info(NDI_WHITE, op, buf);
}

/**
 * Initialize the hashtable used by the archetypes. */
static void clear_archetable()
{
	memset((void *) arch_table, 0, ARCHTABLE * sizeof(archetype *));
}

/**
 * An alternative way to init the hashtable which is slower, but
 * _works_... */
static void init_archetable()
{
	archetype *at;

	LOG(llevDebug, "Setting up archetable...");

	for (at = first_archetype; at != NULL; at = (at->more == NULL) ? at->next : at->more)
	{
		add_arch(at);
	}

	LOG(llevDebug, " done.\n");
}

/**
 * Dumps _all_ archetypes and artifacts to debug-level output. */
void dump_all_archetypes()
{
	archetype *at;
	artifactlist *al;
	artifact *art = NULL;
	StringBuffer *sb;
	char *diff;

	for (at = first_archetype; at != NULL; at = (at->more == NULL) ? at->next : at->more)
	{
		sb = stringbuffer_new();
		dump_object(&at->clone, sb);
		diff = stringbuffer_finish(sb);
		LOG(llevInfo, "%s\n", diff);
		free(diff);
	}

	LOG(llevInfo, "Artifacts fake arch list:\n");

	for (al = first_artifactlist; al != NULL; al = al->next)
	{
		art = al->items;

		do
		{
			sb = stringbuffer_new();
			dump_object(&art->def_at.clone, sb);
			diff = stringbuffer_finish(sb);
			LOG(llevInfo, "%s\n", diff);
			free(diff);
			art = art->next;
		}
		while (art);
	}
}

/**
 * Frees all memory allocated to archetypes.
 *
 * After calling this, it's possible to call again init_archetypes() to
 * reload data. */
void free_all_archs()
{
	archetype *at, *next;
	int i = 0;

	for (at = first_archetype; at != NULL; at = next)
	{
		if (at->more)
		{
			next = at->more;
		}
		else
		{
			next = at->next;
		}

		FREE_AND_CLEAR_HASH(at->name);
		FREE_AND_CLEAR_HASH(at->clone.name);
		FREE_AND_CLEAR_HASH(at->clone.title);
		FREE_AND_CLEAR_HASH(at->clone.race);
		FREE_AND_CLEAR_HASH(at->clone.slaying);
		FREE_AND_CLEAR_HASH(at->clone.msg);
		free_key_values(&at->clone);
		free(at);
		i++;
	}

	LOG(llevDebug, "Freed %d archetypes\n", i);
}

/**
 * Allocates, initializes and returns the pointer to an archetype
 * structure.
 * @return New archetype structure, will never be NULL. */
static archetype *get_archetype_struct()
{
	archetype *new;

	new = (archetype *) CALLOC(1, sizeof(archetype));

	if (new == NULL)
	{
		LOG(llevError, "ERROR: get_archetype_struct(): Out of memory\n");
	}

	/* To initial state other also */
	initialize_object(&new->clone);

	return new;
}

/**
 * Reads/parses the archetype-file, and copies into a linked list
 * of archetype structures.
 * @param fp Opened file descriptor which will be used to read the
 * archetypes. */
static void first_arch_pass(FILE *fp)
{
	object *op;
	void *mybuffer;
	archetype *at,*prev = NULL, *last_more = NULL;
	int i;

	op = get_object();
	op->arch = first_archetype = at = get_archetype_struct();
	mybuffer = create_loader_buffer(fp);

	while ((i = load_object(fp, op, mybuffer, LO_REPEAT, MAP_STYLE)))
	{
		/* Use copy_object_data() - we don't want adjust any speed_left here! */
		copy_object_data(op,&at->clone);

		/* Now we have the right speed_left value for out object.
		 * copy_object() now will track down negative speed values, to
		 * alter speed_left to garantie a random & senseful start value. */
		if (!op->layer && !QUERY_FLAG(op, FLAG_SYS_OBJECT))
		{
			LOG(llevDebug, "WARNING: Archetype %s has layer 0 without being sys_object!\n", STRING_OBJ_ARCH_NAME(op));
		}

		if (op->layer && QUERY_FLAG(op, FLAG_SYS_OBJECT))
		{
			LOG(llevDebug, "WARNING: Archetype %s has layer %d (!= 0) and is sys_object!\n", STRING_OBJ_ARCH_NAME(op), op->layer);
		}

		switch (i)
		{
			/* A new archetype, just link it with the previous */
			case LL_NORMAL:
				if (last_more != NULL)
				{
					last_more->next = at;
				}

				if (prev != NULL)
				{
					prev->next = at;
				}

				prev = last_more = at;

				if (op->animation_id && !op->anim_speed)
				{
					LOG(llevDebug, "WARNING: Archetype %s has animation but no anim_speed!\n", STRING_OBJ_ARCH_NAME(op));
				}

				if (op->animation_id && !QUERY_FLAG(op, FLAG_CLIENT_SENT))
				{
					LOG(llevDebug, "WARNING: Archetype %s has animation but no explicitly set is_animated!\n", STRING_OBJ_ARCH_NAME(op));
				}

				if (!op->type)
				{
					LOG(llevDebug, "WARNING: Archetype %s has no type!\n", STRING_OBJ_ARCH_NAME(op));
				}

				break;

			/* Another part of the previous archetype, link it correctly */
			case LL_MORE:
				at->head = prev;
				at->clone.head = &prev->clone;

				if (last_more != NULL)
				{
					last_more->more = at;
					last_more->clone.more = &at->clone;
				}

				last_more = at;

				break;
		}

		/* We are using this flag for debugging - ignore */
		CLEAR_FLAG((&at->clone), FLAG_CLIENT_SENT);
		at = get_archetype_struct();
		initialize_object(op);
		op->arch = at;
	}

	delete_loader_buffer(mybuffer);
	/* Make sure our temp object is gc:ed */
	mark_object_removed(op);
	free(at);
}

/**
 * Reads the archetype file once more, and links all pointers between
 * archetypes and treasure lists. Must be called after first_arch_pass().
 * @param fp_start File from which to read. Won't be rewinded. */
static void second_arch_pass(FILE *fp_start)
{
	FILE *fp = fp_start;
	int comp;
	char filename[MAX_BUF], buf[MAX_BUF], *variable = buf, *argument, *cp;
	archetype *at = NULL, *other;

	while (fgets(buf, MAX_BUF, fp) != NULL)
	{
		if (*buf == '#')
		{
			continue;
		}

		if ((argument = strchr(buf, ' ')) != NULL)
		{
			*argument = '\0', argument++;
			cp = argument + strlen(argument) - 1;

			while (isspace(*cp))
			{
				*cp = '\0';
				cp--;
			}
		}

		if (!strcmp("Object", variable))
		{
			if ((at = find_archetype(argument)) == NULL)
			{
				LOG(llevBug, "BUG: Failed to find arch %s\n", STRING_SAFE(argument));
			}
		}
		else if (!strcmp("other_arch", variable))
		{
			if (at != NULL && at->clone.other_arch == NULL)
			{
				if ((other = find_archetype(argument)) == NULL)
				{
					LOG(llevBug, "BUG: Failed to find other_arch %s\n", STRING_SAFE(argument));
				}
				else if (at != NULL)
				{
					at->clone.other_arch = other;
				}
			}
		}
		else if (!strcmp("randomitems", variable))
		{
			if (at != NULL)
			{
				treasurelist *tl = find_treasurelist(argument);

				if (tl == NULL)
				{
					LOG(llevBug, "BUG: Failed to link treasure to arch. (arch: %s ->%s\n", STRING_OBJ_NAME(&at->clone), STRING_SAFE(argument));
				}
				else
				{
					at->clone.randomitems = tl;
				}
			}
		}
	}

	/* Now reparse the artifacts file too! */
	snprintf(filename, sizeof(filename), "%s/artifacts", settings.datadir);

	if ((fp = open_and_uncompress(filename, 0, &comp)) == NULL)
	{
		LOG(llevError, "ERROR: Can't open %s.\n", filename);
		return;
	}

	while (fgets(buf, MAX_BUF, fp) != NULL)
	{
		if (*buf == '#')
		{
			continue;
		}

		if ((argument = strchr(buf, ' ')) != NULL)
		{
			*argument = '\0', argument++;
			cp = argument + strlen(argument) - 1;

			while (isspace(*cp))
			{
				*cp = '\0';
				cp--;
			}
		}

		/* Now we get our artifact. if we hit "def_arch", we first copy
		 * from it other_arch and treasure list to our artifact. Then we
		 * search the object for other_arch and randomitems - perhaps we
		 * override them here. */
		if (!strcmp("artifact", variable))
		{
			if ((at = find_archetype(argument)) == NULL)
			{
				LOG(llevBug, "BUG: Second artifacts pass: Failed to find artifact %s\n", STRING_SAFE(argument));
			}
		}
		else if (!strcmp("def_arch", variable))
		{
			if ((other = find_archetype(argument)) == NULL)
			{
				LOG(llevBug, "BUG: Second artifacts pass: Failed to find def_arch %s from artifact %s\n", STRING_SAFE(argument), STRING_ARCH_NAME(at));
			}

			/* now copy from real arch the stuff from above to our "fake" arches */
			at->clone.other_arch = other->clone.other_arch;
			at->clone.randomitems = other->clone.randomitems;
		}
		else if (!strcmp("other_arch", variable))
		{
			if ((other = find_archetype(argument)) == NULL)
			{
				LOG(llevBug, "BUG: Second artifacts pass: Failed to find other_arch %s\n", STRING_SAFE(argument));
			}
			else if (at != NULL)
			{
				at->clone.other_arch = other;
			}
		}
		else if (!strcmp("randomitems", variable))
		{
			treasurelist *tl = find_treasurelist(argument);

			if (tl == NULL)
			{
				LOG(llevBug, "BUG: Second artifacts pass: Failed to link treasure to arch. (arch: %s ->%s)\n", STRING_OBJ_NAME(&at->clone), STRING_SAFE(argument));
			}
			else if (at != NULL)
			{
				at->clone.randomitems = tl;
			}
		}
	}

	close_and_delete(fp, comp);
}

/**
 * Loads all archetypes and treasures.
 *
 * First initialises the archtype hash-table (init_archetable()).
 * Reads and parses the archetype file (with the first and second-pass
 * functions).
 * Then initialises treasures by calling load_treasures(). */
static void load_archetypes()
{
	FILE *fp;
	char filename[MAX_BUF];
	int comp;
#if TIME_ARCH_LOAD
	struct timeval tv1, tv2;
#endif

	snprintf(filename, sizeof(filename), "%s/%s", settings.datadir, settings.archetypes);
	LOG(llevDebug, "Reading archetypes from %s...\n", filename);

	if ((fp = open_and_uncompress(filename, 0, &comp)) == NULL)
	{
		LOG(llevError, "ERROR: Can't open archetype file.\n");
		return;
	}

	clear_archetable();
	LOG(llevDebug, "arch-pass 1...\n");

#if TIME_ARCH_LOAD
	GETTIMEOFDAY(&tv1);
#endif

	first_arch_pass(fp);

#if TIME_ARCH_LOAD
	int sec, usec;
	GETTIMEOFDAY(&tv2);
	sec = tv2.tv_sec - tv1.tv_sec;
	usec = tv2.tv_usec - tv1.tv_usec;

	if (usec < 0)
	{
		usec += 1000000;
		sec--;
	}

	LOG(llevDebug, "Load took %d.%06d seconds\n", sec, usec);
#endif

	LOG(llevDebug, " done.\n");
	init_archetable();

	/* Do a close and reopen instead of a rewind - necessary in case the
	 * file has been compressed. */
	close_and_delete(fp, comp);
	fp = open_and_uncompress(filename, 0, &comp);

	/* If not called before, reads all artifacts from file */
	init_artifacts();
	LOG(llevDebug, " loading treasure...\n");
	load_treasures();
	LOG(llevDebug, " done\n arch-pass 2...\n");
	second_arch_pass(fp);
	LOG(llevDebug, " done.\n");

	close_and_delete(fp, comp);
	LOG(llevDebug, "Reading archetypes done.\n");
}

/**
 * Creates and returns a new object which is a copy of the given
 * archetype.
 * @param at Archetype from which to get an object.
 * @return Object of specified type. */
object *arch_to_object(archetype *at)
{
	object *op;

	if (at == NULL)
	{
		LOG(llevBug, "BUG: arch_to_object(): Archetype at is NULL.\n");
		return NULL;
	}

	op = get_object();
	copy_object(&at->clone, op);
	op->arch = at;

	return op;
}

/**
 * Creates a dummy object. This function is called by get_archetype()
 * if it fails to find the appropriate archetype.
 *
 * Thus get_archetype() will be guaranteed to always return
 * an object, and never NULL.
 * @param name Name to give the dummy object.
 * @return Object of specified name. It fill have the ::FLAG_NO_PICK flag
 * set. */
static object *create_singularity(const char *name)
{
	object *op;
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), "singularity (%s)", name);
	op = get_object();
	FREE_AND_COPY_HASH(op->name, buf);
	SET_FLAG(op, FLAG_NO_PICK);

	return op;
}

/**
 * Finds which archetype matches the given name, and returns a new
 * object containing a copy of the archetype.
 * @param name Archetype name.
 * @return Object of specified archetype, or a singularity. Will never be
 * NULL. */
object *get_archetype(const char *name)
{
	archetype *at = find_archetype(name);

	if (at == NULL)
	{
		return create_singularity(name);
	}

	return arch_to_object(at);
}

/**
 * Hash-function used by the arch-hashtable.
 * @param str Archetype name.
 * @param tablesize Size of the hash table
 * @return Hash of the archetype name */
static unsigned long hasharch(const char *str, int tablesize)
{
	unsigned long hash = 0;
	int i = 0, rot = 0;
	const char *p;

	for (p = str; i < MAXSTRING && *p; p++, i++)
	{
		hash ^= (unsigned long) *p << rot;
		rot += 2;

		if (rot >= ((int) sizeof(long) - (int) sizeof(char)) * 8)
		{
			rot = 0;
		}
	}

	return (hash % tablesize);
}

/**
 * Finds, using the hashtable, which archetype matches the given name.
 * @return Pointer to the found archetype, otherwise NULL. */
archetype *find_archetype(const char *name)
{
	archetype *at;
	unsigned long index;

	if (name == NULL)
	{
		return NULL;
	}

	index = hasharch(name, ARCHTABLE);
	arch_search++;

	for (; ;)
	{
		at = arch_table[index];

		/* Not in archetype list - let's try the artifacts file */
		if (at == NULL)
		{
			return find_artifact_archtype(name);
		}

		arch_cmp++;

		if (!strcmp(at->name, name))
		{
			return at;
		}

		if (++index >= ARCHTABLE)
		{
			index = 0;
		}
	}
}

/**
 * Adds an archetype to the hashtable. */
static void add_arch(archetype *at)
{
	int index = hasharch(at->name, ARCHTABLE), org_index = index;

	for (; ;)
	{
		if (arch_table[index] && !strcmp(arch_table[index]->name, at->name))
		{
			LOG(llevError, "ERROR: add_arch(): Double use of arch name %s.\n", STRING_ARCH_NAME(at));
		}

		if (arch_table[index] == NULL)
		{
			arch_table[index] = at;
			return;
		}

		if (++index == ARCHTABLE)
		{
			index = 0;
		}

		if (index == org_index)
		{
			LOG(llevError, "ERROR: add_arch(): Archtable too small.\n");
		}
	}
}

/**
 * Returns the first archetype using the given types.
 *
 * Used in treasure generation.
 * @param type Type to look for.
 * @return The archetype if found, NULL otherwise. */
static archetype *type_to_archetype(int type)
{
	archetype *at;

	for (at = first_archetype; at != NULL; at = (at->more == NULL) ? at->next : at->more)
	{
		if (at->clone.type == type)
		{
			return at;
		}
	}

	return NULL;
}

/**
 * Returns a new object copied from the first archetype matching the
 * given type.
 *
 * Used in treasure-generation.
 * @param type The type.
 * @return New object if the type is found, NULL otherwise. */
object *clone_arch(int type)
{
	archetype *at;
	object *op = get_object();

	if ((at = type_to_archetype(type)) == NULL)
	{
		LOG(llevBug, "BUG: Can't clone archetype %d.\n", type);
		return NULL;
	}

	copy_object(&at->clone, op);
	return op;
}
