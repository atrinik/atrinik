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

/**
 * @file
 * Basic stuff for use with the alchemy code.
 *
 * Our definition of 'formula' is any product of an alchemical process.
 * Ingredients are just comma delimited list of archetype (or object)
 * names.
 *
 * Example 'formula' entry:
 * <pre>
 * 	Object transparency
 *	chance 10
 *	ingred dust of beholdereye,gem
 *	arch potion_generic
 * </pre> */

#include <global.h>

/** Pointer to first recipelist. */
static recipelist *formulalist;

static void check_formulae();
static archetype *find_treasure_by_name(treasure *t, char *name, int depth);
static long find_ingred_cost(const char *name);
static const char *ingred_name(const char *name);
static int numb_ingred(const char *buf);
static recipelist *get_random_recipelist();

/**
 * Allocates a new recipelist.
 * @return New structure initialized. Never NULL. */
static recipelist *init_recipelist()
{
	recipelist *tl = (recipelist *) malloc(sizeof(recipelist));

	if (tl == NULL)
	{
		LOG(llevError, "init_recipelist(): Out of memory.\n");
	}

	tl->total_chance = 0;
	tl->number = 0;
	tl->items = NULL;
	tl->next = NULL;
	return tl;
}

/**
 * Allocates a new recipe.
 * @return New structure initialized. Never NULL. */
static recipe *get_empty_formula()
{
	recipe *t = (recipe *) malloc(sizeof(recipe));

	if (t == NULL)
	{
		LOG(llevError, "get_empty_formula(): Out of memory.\n");
	}

	t->chance = 0;
	t->index = 0;
	t->transmute = 0;
	t->yield = 0;
	t->keycode = 0;
	t->title = NULL;
	t->arch_name = NULL;
	t->ingred = NULL;
	t->next = NULL;
	return t;
}

/**
 * Gets a formula list by ingredients count.
 * @param i Number of ingredients.
 * @return Pointer to the formula list, or NULL if it doesn't exist. */
recipelist *get_formulalist(int i)
{
	recipelist *fl = formulalist;
	int number = i;

	while (fl && number > 1)
	{
		if (!(fl = fl->next))
		{
			break;
		}

		number--;
	}

	return fl;
}

/**
 * Makes sure we actually have the requested artifact and archetype.
 * @param rp Recipe we want to check.
 * @return 1 if recipe is ok, 0 if missing something. */
static int check_recipe(recipe *rp)
{
	if (find_archetype(rp->arch_name) != NULL)
	{
		artifact *art = locate_recipe_artifact(rp);

		if (!art && rp->title != shstr_cons.NONE)
		{
			LOG(llevBug, "Formula %s of %s has no artifact.\n", rp->arch_name, rp->title);
			return 0;
		}
	}
	else
	{
		LOG(llevBug, "Can't find archetype:%s for formula:%s\n", rp->arch_name, rp->title);
		return 0;
	}

	return 1;
}

/**
 * Builds up the lists of formula from the file in the libdir. */
void init_formulae()
{
	static int has_been_done = 0;
	FILE *fp;
	char filename[MAX_BUF], buf[MAX_BUF], *cp, *next;
	recipe *formula = NULL;
	recipelist *fl = init_recipelist();
	linked_char *tmp;
	int value, comp;

	if (!formulalist)
	{
		formulalist = fl;
	}

	if (has_been_done)
	{
		return;
	}
	else
	{
		has_been_done = 1;
	}

	snprintf(filename, sizeof(filename), "%s/formulae", settings.datadir);
	LOG(llevDebug, "Reading alchemical formulae from %s...",filename);

	if ((fp = open_and_uncompress(filename, 0, &comp)) == NULL)
	{
		LOG(llevBug, "Can't open %s.\n", filename);
		return;
	}

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
		while (*cp == ' ')
		{
			cp++;
		}

		if (!strncmp(cp, "Object", 6))
		{
			formula = get_empty_formula();
			FREE_AND_COPY_HASH(formula->title, strchr(cp, ' ') + 1);
		}
		else if (!strncmp(cp, "keycode", 7))
		{
			FREE_AND_COPY_HASH(formula->keycode, strchr(cp, ' ') + 1);
		}
		else if (sscanf(cp, "trans %d", &value))
		{
			formula->transmute = (uint16) value;
		}
		else if (sscanf(cp, "yield %d", &value))
		{
			formula->yield = (uint16) value;
		}
		else if (sscanf(cp, "chance %d", &value))
		{
			formula->chance = (uint16) value;
		}
		else if (!strncmp(cp, "ingred", 6))
		{
			int numb_ingred = 1;
			cp = strchr(cp, ' ') + 1;

			do
			{
				if ((next = strchr(cp, ',')) != NULL)
				{
					*(next++) = '\0';
					numb_ingred++;
				}

				tmp = (linked_char *) malloc(sizeof(linked_char));
				tmp->name = NULL;
				FREE_AND_COPY_HASH(tmp->name, cp);
				tmp->next = formula->ingred;
				formula->ingred = tmp;

				/* each ingredient's ASCII value is coadded. Later on this
				 * value will be used allow us to search the formula lists
				 * quickly for the right recipe. */
				formula->index += strtoint(cp);
			}
			while ((cp = next) != NULL);

			/* now find the correct (# of ingred ordered) formulalist */
			fl = formulalist;

			while (numb_ingred != 1)
			{
				if (!fl->next)
				{
					fl->next = init_recipelist();
				}

				fl = fl->next;
				numb_ingred--;
			}

			fl->total_chance += formula->chance;
			fl->number++;
			formula->next = fl->items;
			fl->items = formula;
		}
		else if (!strncmp(cp, "arch", 4))
		{
			FREE_AND_COPY_HASH(formula->arch_name, strchr(cp, ' ') + 1);
			(void) check_recipe(formula);
		}
		else
		{
			LOG(llevBug, "Unknown input in file %s: %s\n", filename, buf);
		}
	}

	LOG(llevDebug, "done.\n");
	close_and_delete(fp, comp);
	/* Lastly, lets check for problems in formula we got */
	check_formulae();
}

/**
 * Check if formula doesn't have the same index.
 *
 * Since we are doing a sequential search on the formulae lists now, we
 * have to be careful that we don't have 2 formula with the exact same
 * index value. Under the new nbatches code, it is possible to have
 * multiples of ingredients in a cauldron which could result in an index
 * formula mismatch. We *don't* check for that possibility here. */
static void check_formulae()
{
	recipelist *fl;
	recipe *check, *formula;
	int numb = 1;

	LOG(llevDebug,"Checking formulae lists...");

	for (fl = formulalist; fl != NULL; fl = fl->next)
	{
		for (formula = fl->items; formula != NULL; formula = formula->next)
		{
			for (check = formula->next; check != NULL; check = check->next)
			{
				if (check->index == formula->index)
				{
					LOG(llevBug, "On %d ingred list: ", numb);
					LOG(llevBug, "Formulae [%s] of %s and [%s] of %s have matching index id (%d)\n", formula->arch_name, formula->title, check->arch_name, check->title, formula->index);
				}
			}
		}

		numb++;
	}

	LOG(llevDebug, "done.\n");
}

/**
 * Dumps alchemy recipes using LOG(). */
void dump_alchemy()
{
	recipelist *fl = formulalist;
	recipe *formula = NULL;
	linked_char *next;
	int num_ingred = 1;

	LOG(llevInfo, "\n");

	while (fl)
	{
		LOG(llevInfo, "\n Formulae with %d ingredient%s  %d Formulae with total_chance=%d\n", num_ingred, num_ingred > 1 ? "s." : ".", fl->number, fl->total_chance);

		for (formula = fl->items; formula != NULL; formula = formula->next)
		{
			artifact *art = NULL;
			char buf[MAX_BUF], tmpbuf[MAX_BUF], *string;

			strncpy(tmpbuf, formula->arch_name, MAX_BUF - 1);
			tmpbuf[MAX_BUF - 1] = 0;
			string = strtok(tmpbuf, ",");

			while (string)
			{
				if (find_archetype(string) != NULL)
				{
					art = locate_recipe_artifact(formula);

					if (!art && formula->title != shstr_cons.NONE)
					{
						LOG(llevBug, "Formula %s has no artifact\n", formula->title);
					}
					else
					{
						if (formula->title != shstr_cons.NONE)
						{
							snprintf(buf, sizeof(buf), "%s of %s", string, formula->title);
						}
						else
						{
							snprintf(buf, sizeof(buf), "%s", string);
						}

						LOG(llevInfo, "%-30s(%d) bookchance %3d  ", buf, formula->index, formula->chance);
						LOG(llevInfo, "\n");

						if (formula->ingred != NULL)
						{
							int nval = 0, tval = 0;
							LOG(llevInfo, "\tIngred: ");

							for (next = formula->ingred; next != NULL; next = next->next)
							{
								if (nval != 0)
								{
									LOG(llevInfo, ",");
								}

								LOG(llevInfo, "%s(%d)", next->name, (nval = strtoint(next->name)));
								tval += nval;
							}

							LOG(llevInfo, "\n");

							if (tval != formula->index)
							{
								LOG(llevInfo, "ingredient list and formula values not equal.\n");
							}
						}
					}
				}
				else
				{
					LOG(llevBug, "Can't find archetype:%s for formula %s\n", string, formula->title);
				}

				string = strtok(NULL, ",");
			}
		}

		LOG(llevInfo, "\n");
		fl = fl->next;
		num_ingred++;
	}
}

/**
 * Find a treasure with a matching name. The 'depth' parameter is
 * only there to prevent infinite loops in treasure lists (a list
 * referencing another list pointing back to the first one).
 * @param t Item of treasure list to search from.
 * @param name Name we're trying to find. Doesn't need to be a shared string.
 * @param depth Current depth. Function will exit if greater than 10.
 * @return Archetype with name, or NULL if nothing found. */
static archetype *find_treasure_by_name(treasure *t, char *name, int depth)
{
	treasurelist *tl;
	archetype *at;

	if (depth > 10)
	{
		return NULL;
	}

	while (t != NULL)
	{
		if (t->name != NULL)
		{
			tl = find_treasurelist(t->name);
			at = find_treasure_by_name(tl->items, name, depth + 1);

			if (at != NULL)
			{
				return at;
			}
		}
		else
		{
			if (!strcasecmp(t->item->clone.name, name))
			{
				return t->item;
			}
		}

		if (t->next_yes != NULL)
		{
			at = find_treasure_by_name(t->next_yes, name, depth);

			if (at != NULL)
			{
				return at;
			}
		}

		if (t->next_no != NULL)
		{
			at = find_treasure_by_name(t->next_no, name, depth);

			if (at != NULL)
			{
				return at;
			}
		}

		t = t->next;
	}

	return NULL;
}

/**
 * Try to find an ingredient with specified name.
 *
 * If several archetypes have the same name, the value of the first one
 * with that name will be returned. This happens for the mushrooms
 * (mushroom_1, mushroom_2 and mushroom_3). For the monsters' body parts,
 * there may be several monsters with the same name. This is not a
 * problem if these monsters have the same level (e.g. sage & c_sage) or
 * if only one of the monsters generates the body parts that we are
 * looking for (e.g. big_dragon and big_dragon_worthless).
 *
 * Will also search in artifacts.
 * @param name Ingredient we're searching for. Can start with a number.
 * @return Cost of ingredient, -1 if wasn't found. */
static long find_ingred_cost(const char *name)
{
	archetype *at, *at2;
	artifactlist *al;
	artifact *art;
	long mult;
	char *cp;
	char part1[100], part2[100];

	/* Same as atoi(), but skip number */
	mult = 0;

	while (isdigit(*name))
	{
		mult = 10 * mult + (*name - '0');
		name++;
	}

	if (mult > 0)
	{
		name++;
	}
	else
	{
		mult = 1;
	}

	/* First, try to match the name of an archetype */
	for (at = first_archetype; at != NULL; at = at->next)
	{
		if (at->clone.title != NULL)
		{
			/* Inefficient, but who cares? */
			snprintf(part1, sizeof(part1), "%s %s", at->clone.name, at->clone.title);

			if (!strcasecmp(part1, name))
			{
				return mult * at->clone.value;
			}
		}

		if (!strcasecmp(at->clone.name, name))
		{
			return mult * at->clone.value;
		}
	}

	/* Second, try to match an artifact ("arch of something") */
	cp = strstr(name, " of ");

	if (cp != NULL)
	{
		strcpy(part1, name);
		part1[cp - name] = '\0';
		strcpy(part2, cp + 4);

		/* Find the first archetype matching the first part of the name */
		for (at = first_archetype; at != NULL; at = at->next)
		{
			if (!strcasecmp(at->clone.name, part1) && at->clone.title == NULL)
			{
				break;
			}
		}

		if (at != NULL)
		{
			/* Find the first artifact derived from that archetype (same type) */
			for (al = first_artifactlist; al != NULL; al = al->next)
			{
				if (al->type == at->clone.type)
				{
					for (art = al->items; art != NULL; art = art->next)
					{
						if (!strcasecmp(art->def_at.clone.name, part2))
						{
							return mult * at->clone.value * art->def_at.clone.value;
						}
					}
				}
			}
		}
	}

	/* Third, try to match a body part ("arch's something") */
	cp = strstr(name, "'s ");

	if (cp != NULL)
	{
		strcpy(part1, name);
		part1[cp - name] = '\0';
		strcpy(part2, cp + 3);

		/* Examine all archetypes matching the first part of the name */
		for (at = first_archetype; at != NULL; at = at->next)
		{
			if (!strcasecmp (at->clone.name, part1) && at->clone.title == NULL)
			{
				if (at->clone.randomitems != NULL)
				{
					at2 = find_treasure_by_name(at->clone.randomitems->items, part2, 0);

					if (at2 != NULL)
					{
						return mult * at2->clone.value * isqrt(at->clone.level * 2);
					}
				}
			}
		}
	}

	/* Failed to find any matching items -- formula should be checked */
	return -1;
}

/**
 * Dumps all costs of recipes using LOG(). */
void dump_alchemy_costs()
{
	recipelist *fl = formulalist;
	recipe *formula = NULL;
	linked_char *next;
	int num_ingred = 1, num_errors = 0;
	long cost, tcost;

	LOG(llevInfo, "\n");

	while (fl)
	{
		LOG(llevInfo, "\n Formulae with %d ingredient%s  %d Formulae with total_chance=%d\n", num_ingred, num_ingred > 1 ? "s." : ".", fl->number, fl->total_chance);

		for (formula = fl->items; formula != NULL; formula = formula->next)
		{
			artifact *art = NULL;
			archetype *at = NULL;
			char buf[MAX_BUF], tmpbuf[MAX_BUF], *string;

			strncpy(tmpbuf, formula->arch_name, MAX_BUF - 1);
			tmpbuf[MAX_BUF - 1] = '\0';
			string = strtok(tmpbuf, ",");

			while (string)
			{
				if ((at = find_archetype(string)) != NULL)
				{
					art = locate_recipe_artifact(formula);

					if (!art && formula->title != shstr_cons.NONE)
					{
						LOG(llevBug, "Formula %s has no artifact\n", formula->title);
					}
					else
					{
						if (formula->title == shstr_cons.NONE)
						{
							snprintf(buf, sizeof(buf), "%s", string);
						}
						else
						{
							snprintf(buf, sizeof(buf), "%s of %s", string, formula->title);
						}

						LOG(llevInfo, "\n%-40s bookchance %3d\n", buf, formula->chance);

						if (formula->ingred != NULL)
						{
							tcost = 0;

							for (next = formula->ingred; next != NULL; next = next->next)
							{
								cost = find_ingred_cost(next->name);

								if (cost < 0)
								{
									num_errors++;
								}

								LOG(llevInfo, "\t%-33s%5ld\n", next->name, cost);

								if (cost < 0 || tcost < 0)
								{
									tcost = -1;
								}
								else
								{
									tcost += cost;
								}
							}

							if (art != NULL && &art->def_at.clone != NULL)
							{
								cost = at->clone.value * art->def_at.clone.value;
							}
							else
							{
								cost = at->clone.value;
							}

							LOG(llevInfo, "\t\tBuying result costs: %5ld", cost);

							if (formula->yield > 1)
							{
								LOG(llevInfo, " to %ld (max %d items)\n", cost * formula->yield, formula->yield);
								cost = cost * (formula->yield + 1L) / 2L;
							}
							else
							{
								LOG(llevInfo, "\n");
							}

							LOG(llevInfo, "\t\tIngredients cost:    %5ld\n\t\tComment: ", tcost);

							if (tcost < 0)
							{
								LOG(llevInfo, "Could not find some ingredients. Check the formula!\n");
							}
							else if (tcost > cost)
							{
								LOG(llevInfo, "Ingredients are much expensive. Useless formula.\n");
							}
							else if (tcost * 2L > cost)
							{
								LOG(llevInfo, "Ingredients are too expensive.\n");
							}
							else if (tcost * 10L < cost)
							{
								LOG(llevInfo, "Ingredients are too cheap.\n");
							}
							else
							{
								LOG(llevInfo, "OK.\n");
							}
						}
					}
				}
				else
				{
					LOG(llevBug, "Can't find archetype:%s for formula %s\n", string, formula->title);
				}

				string = strtok(NULL, ",");
			}
		}

		LOG(llevInfo, "\n");
		fl = fl->next;
		num_ingred++;
	}

	if (num_errors > 0)
	{
		LOG(llevInfo, "%d objects required by the formulae do not exist in the game.\n", num_errors);
	}
}

/**
 * Extracts the name from an ingredient.
 * @param name Ingredient to extract from. Can contain a number at start.
 * @return Pointer in name to the first character of the ingredient's
 * name. */
static const char *ingred_name(const char *name)
{
	const char *cp = name;

	if (atoi(cp))
	{
		cp = strchr(cp, ' ') + 1;
	}

	return cp;
}

/**
 * Convert buf into an integer equal to the coadded sum of the
 * (lowercase) character.
 *
 * ASCII values in buf (times prepended integers).
 * @param buf Buffer we want to convert. Can contain an initial number.
 * @return Sum of lowercase characters of the ingredient's name. */
int strtoint(const char *buf)
{
	const char *cp = ingred_name(buf);
	int val = 0, mult = numb_ingred(buf);
	size_t len = strlen(cp);

	while (len)
	{
		val += tolower(*cp);
		cp++;
		len--;
	}

	return val * mult;
}

/**
 * Finds an artifact for a recipe.
 * @param rp Recipe.
 * @return Artifact, or NULL if not found. */
artifact *locate_recipe_artifact(recipe *rp)
{
	object *item = get_archetype(rp->arch_name);
	artifactlist *at = NULL;
	artifact *art = NULL;

	if (!item)
	{
		return NULL;
	}

	if ((at = find_artifactlist(item->type)))
	{
		for (art = at->items; art; art = art->next)
		{
			if (!strcmp(art->def_at.clone.name, rp->title))
			{
				break;
			}
		}
	}

	return art;
}

/**
 * Extracts the number part of an ingredient.
 * @param buf Ingredient.
 * @return Number part of an ingredient. */
static int numb_ingred(const char *buf)
{
	int numb;

	if ((numb = atoi(buf)))
	{
		return numb;
	}

	return 1;
}

/**
 * Gets a random recipe list.
 * @return Random recipe list. */
static recipelist *get_random_recipelist()
{
	recipelist *fl = NULL;
	int number = 0, roll = 0;

	/* First, determine # of recipelist we have */
	for (fl = get_formulalist(1); fl; fl = fl->next)
	{
		number++;
	}

	/* Now, randomly choose one */
	if (number > 0)
	{
		roll = RANDOM() % number;
	}

	fl = get_formulalist(1);

	while (roll && fl)
	{
		if (fl->next)
		{
			fl = fl->next;
		}
		else
		{
			break;
		}

		roll--;
	}

	/* Failed! */
	if (!fl)
	{
		LOG(llevBug, "get_random_recipelist(): no recipelists found!\n");
	}
	else if (fl->total_chance == 0)
	{
		fl = get_random_recipelist();
	}

	return fl;
}

/**
 * Gets a random recipe from a list, based on chance.
 * @param rpl Recipelist we want a recipe from. Can be NULL in which case
 * a random one is selected.
 * @return Random recipe. Can be NULL if recipelist has a total_chance of
 * 0. */
recipe *get_random_recipe(recipelist *rpl)
{
	recipelist *fl = rpl;
	recipe *rp = NULL;
	int r = 0;

	/* Looks like we have to choose a random one */
	if (fl == NULL)
	{
		if ((fl = get_random_recipelist()) == NULL)
		{
			return rp;
		}
	}

	if (fl->total_chance > 0)
	{
		r = RANDOM() % fl->total_chance;

		for (rp = fl->items; rp; rp = rp->next)
		{
			r -= rp->chance;

			if (r < 0)
			{
				break;
			}
		}
	}

	return rp;
}

/**
 * Frees all memory allocated to recipes and recipes lists. */
void free_all_recipes()
{
	recipelist *fl = formulalist, *flnext;
	recipe *formula = NULL, *next;
	linked_char *lchar, *charnext;

	LOG(llevDebug, "Freeing all the recipes\n");

	for (fl = formulalist; fl != NULL; fl = flnext)
	{
		flnext = fl->next;

		for (formula = fl->items; formula != NULL; formula = next)
		{
			next = formula->next;

			FREE_AND_CLEAR_HASH2(formula->arch_name);
			FREE_AND_CLEAR_HASH2(formula->title);

			for (lchar = formula->ingred; lchar; lchar = charnext)
			{
				charnext = lchar->next;
				FREE_AND_CLEAR_HASH2(lchar->name);
				free(lchar);
			}

			free(formula);
		}

		free(fl);
	}
}
