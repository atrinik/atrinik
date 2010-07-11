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
 * Contains all alchemy related code. */

#include <global.h>

/** Define this for some helpful debuging information. */
#define ALCHEMY_DEBUG

/** Define this for loads of (marginal) debuging information. */
#define EXTREME_ALCHEMY_DEBUG

/** Random cauldron effects. */
static char *cauldron_effect[] =
{
	"vibrates briefly",
	"produces a cloud of steam",
	"emits bright flames",
	"pours forth heavy black smoke",
	"emits sparks",
	"shoots out small flames",
	"whines painfully",
	"hiccups loudly",
	"wheezes",
	"burps",
	"shakes",
	"rattles",
	"makes chugging sounds",
	"smokes heavily for a while"
};

static const char *cauldron_sound();
static int content_recipe_value(object *op);
static int numb_ob_inside(object *op);
static void adjust_product(object *item, int lvl, int yield);
static object *make_item_from_recipe(object *cauldron, recipe *rp);
static void alchemy_failure_effect(object *op, object *cauldron, recipe *rp, int danger);
static void remove_contents(object *first_ob, object *save_item);
static int calc_alch_danger(object *caster, object *cauldron);
static object *find_transmution_ob(object *first_ingred, recipe *rp);
static object *attempt_recipe(object *caster, object *cauldron, int ability, recipe *rp, int nbatches);

/**
 * Returns a random selection from ::cauldron_effect. */
static const char *cauldron_sound()
{
	int size = sizeof(cauldron_effect) / sizeof(char *);

	return cauldron_effect[rndm(1, size) - 1];
}

/**
 * Main part of the alchemy code. From this we call functions that take a
 * look at the contents of the 'cauldron' and, using these ingredients,
 * we construct an integer formula value which is referenced (randomly)
 * against a formula list (the formula list chosen is based on the #
 * contents of the cauldron).
 *
 * If we get a match between the recipe indicated in cauldron contents
 * and a randomly chosen one, an item is created and experience awarded.
 * Otherwise various failure effects are possible (getting worse and
 * worse with # cauldron ingredients). Note that the 'item' to be made
 * can be *anything* listed on the artifacts list in lib/artifacts which
 * has a recipe listed in lib/formulae.
 *
 * To those wondering why I am using the funky formula index method:
 *   1) I want to match recipe to ingredients regardless of ordering.
 *   2) I want a fast search for the 'right' recipe.
 *
 * Note: it is just possible that a totally different combination of
 * ingredients will result in a match with a given recipe. This is not a
 * bug! There is no good reason (in my mind) why alchemical processes
 * have to be unique -- such a 'feature' is one reason why players might
 * want to experiment around. :)
 * @param caster Who is doing alchemy.
 * @param cauldron The cauldron in which alchemy should take place. */
static void attempt_do_alchemy(object *caster, object *cauldron)
{
	recipelist *fl;
	recipe *rp = NULL;
	float success_chance;
	int numb, ability = 1;
	int formula = 0;

	/* Only players for now */
	if (caster->type != PLAYER)
	{
		return;
	}

	/* If no ingredients, no formula! Let's forget it */
	if (!(formula = content_recipe_value(cauldron)))
	{
		return;
	}

	numb = numb_ob_inside(cauldron);

	if ((fl = get_formulalist(numb)))
	{
		/* The caster only gets an increase in ability if they know
		 * alchemy skill */
		if (find_skill(caster, SK_ALCHEMY) != NULL)
		{
			change_skill(caster, SK_ALCHEMY);
			ability += (int) ((float) SK_level(caster) * (float) ((float) (4 + cauldron->magic) / 4.0f));
		}

#ifdef ALCHEMY_DEBUG
		LOG(llevDebug, "DEBUG: Got alchemy ability lvl = %d\n", ability);
#endif

		if (QUERY_FLAG(caster, FLAG_WIZ))
		{
			rp = fl->items;

			while (rp && (formula % rp->index) != 0)
			{
#ifdef EXTREME_ALCHEMY_DEBUG
				LOG(llevDebug, "DEBUG: found list %d formula: %s of %s (%d)\n", numb, rp->arch_name, rp->title, rp->index);
#endif
				rp = rp->next;
			}

			if (rp)
			{
#ifdef ALCHEMY_DEBUG
				if (rp->title != shstr_cons.NONE)
				{
					LOG(llevDebug, "DEBUG: WIZ got formula: %s of %s\n", rp->arch_name, rp->title);
				}
				else
				{
					LOG(llevDebug, "DEBUG: WIZ got formula: %s (nbatches:%d)\n", rp->arch_name, formula / rp->index);
				}
#endif
				attempt_recipe(caster, cauldron, ability, rp, formula / rp->index);
			}
			else
			{
				LOG(llevDebug, "DEBUG: WIZ couldnt find formula for ingredients.\n");
			}

			return;
		}

		/* Find the recipe */
		for (rp = fl->items; rp != NULL && (formula % rp->index) != 0; rp = rp->next)
		{
		}

		/* If we found a recipe */
		if (rp)
		{
			float ave_chance = fl->total_chance / (float) fl->number;
			object *item;

			/* Create the object **FIRST**, then decide whether to keep it. */
			if ((item = attempt_recipe(caster, cauldron, ability, rp, formula / rp->index)) != NULL)
			{
				/* Compute base chance of recipe success */
				success_chance = ((float) (15 * ability) / (float) (15 * ability + numb * item->level * (numb + item->level + formula / rp->index)));

				if (ave_chance == 0)
				{
					ave_chance = 1;
				}

				/* Adjust the success chance by the chance from the recipe list	*/
				if (ave_chance > rp->chance)
				{
					success_chance *= ((float) rp->chance + ave_chance) / (2.0f * ave_chance);
				}
				else
				{
					success_chance = 1.0f - ((1.0f - success_chance) * ((float) rp->chance + ave_chance) / (2.0f * (float) rp->chance));
				}

#ifdef ALCHEMY_DEBUG
				LOG(llevDebug, "DEBUG: percent success chance =  %f\n", success_chance);
#endif

				/* Roll the dice */
				if ((float) (rndm(0, 99)) <= 100.0 * success_chance)
				{
					/* We learn from our experience IF we know something of the alchemical arts */
					if (caster->chosen_skill && caster->chosen_skill->stats.sp == SK_ALCHEMY)
					{
						/* More exp is given for higher ingred number recipes */
						sint64 amount = numb * numb * calc_skill_exp(caster, item, -1);
						add_exp(caster, amount, SK_ALCHEMY);
						/* So when skill id this item, less xp is awarded */
						item->stats.exp = 0;
#ifdef EXTREME_ALCHEMY_DEBUG
						LOG(llevDebug, "DEBUG: %s gains %"FMT64" experience points.\n", caster->name, amount);
#endif
					}

					return;
				}
			}
		}
	}

	/* If we get here, we failed! */
	alchemy_failure_effect(caster, cauldron, rp, calc_alch_danger(caster, cauldron));
}

/**
 * Recipe value of the entire contents of a container.
 *
 * This appears to just generate a hash value, which I guess for now
 * works ok, but the possibility of duplicate hashes is certainly
 * possible.
 * @param op Container for which to generate a hash.
 * @return Hash value. */
static int content_recipe_value(object *op)
{
	char name[MAX_BUF];
	object *tmp = op->inv;
	int tval = 0, formula = 0;

	while (tmp)
	{
		tval = 0;
		strcpy(name, tmp->name);

		if (tmp->title)
		{
			snprintf(name, sizeof(name), "%s %s", tmp->name, tmp->title);
		}

		tval = (strtoint(name) * (tmp->nrof ? tmp->nrof : 1));

#ifdef ALCHEMY_DEBUG
		LOG(llevDebug, "DEBUG: Got ingredient %d %s(%d)\n", tmp->nrof ? tmp->nrof : 1, name, tval);
#endif

		formula += tval;
		tmp = tmp->below;
	}

#ifdef ALCHEMY_DEBUG
	LOG(llevDebug, "DEBUG: Formula value=%d\n", formula);
#endif

	return formula;
}

/**
 * Returns the total number of items in op, excluding ones in item's
 * items.
 * @param op Container.
 * @return Total item count. */
static int numb_ob_inside(object *op)
{
	object *tmp = op->inv;
	int o_number = 0;

	while (tmp)
	{
		o_number++;
		tmp = tmp->below;
	}

#ifdef ALCHEMY_DEBUG
	LOG(llevDebug, "DEBUG: numb_ob_inside(%s): found %d ingredients\n", op->name, o_number);
#endif

	return o_number;
}

/**
 * Essentially a wrapper for make_item_from_recipe() and
 * insert_ob_in_ob(). If the caster has some alchemy skill, then they
 * might gain some exp from (successful) fabrication of the product.
 * If nbatches==-1, don't give exp for this creation (random generation/
 * failed recipe)
 * @param caster Who is trying to do alchemy.
 * @param cauldron Container used for alchemy.
 * @param rp Recipe attempted.
 * @param nbatches If -1, don't give exp for this creation (random
 * generation/failed recipe)
 * @return Generated item, can be NULL if contents were destroyed. */
static object *attempt_recipe(object *caster, object *cauldron, int ability, recipe *rp, int nbatches)
{
	object *item = NULL;
	/* This should be passed to this function, not too effiecent cpu use this way */
	int batches = abs(nbatches);

	/* Code required for this recipe, search the caster */
	if (rp->keycode)
	{
		object *tmp;

		for (tmp = caster->inv; tmp != NULL; tmp = tmp->below)
		{
			if (tmp->type == FORCE && tmp->slaying && !strcmp(rp->keycode, tmp->slaying))
			{
				break;
			}
		}

		/* Failure - no code found */
		if (tmp == NULL)
		{
			new_draw_info(NDI_UNIQUE, caster, "You know the ingredients, but not the technique. Go learn how to do this recipe.");
			return NULL;
		}
	}

#ifdef EXTREME_ALCHEMY_DEBUG
	LOG(llevDebug, "DEBUG: attempt_recipe(): got %d nbatches\n", nbatches);
	LOG(llevDebug, "DEBUG: attempt_recipe(): using recipe %s\n", rp->title ? rp->title : "unknown");
#endif

	if ((item = make_item_from_recipe(cauldron, rp)) != NULL)
	{
		remove_contents(cauldron->inv, item);
		/* adj lvl, nrof on caster level */
		adjust_product(item, ability, rp->yield ? (rp->yield * batches) : batches);

		if (!item->env && (item = insert_ob_in_ob(item, cauldron)) == NULL)
		{
			new_draw_info(NDI_UNIQUE, caster, "Nothing happened.");
		}
		else
		{
			new_draw_info_format(NDI_UNIQUE, caster, "The %s %s.", cauldron->name, cauldron_sound());
		}
	}

	return item;
}

/**
 * Adjust the nrof, exp and level of the final product, based on the
 * item's default parameters, and the relevant caster skill level.
 * @param item Item to adjust.
 * @param lvl Alchemy skill level.
 * @param yield How many products the recipe returns at maximum. */
static void adjust_product(object *item, int lvl, int yield)
{
	int nrof = 1;

	if (!yield)
	{
		yield = 1;
	}

	/* Avoid division by zero */
	if (lvl <= 0)
	{
		lvl = 1;
	}

	if (item->nrof)
	{
		nrof = (int) ((1.0f - 1.0f / ((float) lvl / 10.0f + 1.0f)) * (float) (rndm(0, yield - 1) + (float) rndm(0, yield - 1) + (float) rndm(0, yield - 1)) + 1.0f);

		if (nrof > yield)
		{
			nrof = yield;
		}

		item->nrof = nrof;
	}

	/* Item exp. This will be used later for experience calculation */
	item->stats.exp += lvl * lvl * nrof;

#if 0
	/* avg between default and caster levels */
	item->level = (lvl + item->level) / 2;
#endif
}

/**
 * Use a list of items and a recipe to make an artifact.
 * @param cauldron The cauldron (including the ingredients) used to make
 * the item.
 * @param rp The recipe to make the artifact from.
 * @return The newly created object, NULL if something failed. */
static object *make_item_from_recipe(object *cauldron, recipe *rp)
{
	artifact *art = NULL;
	object *item = NULL;

	if (rp == NULL)
	{
		return NULL;
	}

	/* Find the appropriate object to transform...*/
	if ((item = find_transmution_ob(cauldron->inv, rp)) == NULL)
	{
		LOG(llevDebug, "DEBUG: make_alchemy_item(): Failed to create alchemical object.\n");
		return NULL;
	}

	/* Find the appropriate artifact template...*/
	if (rp->title != shstr_cons.NONE)
	{
		if ((art = locate_recipe_artifact(rp)) == NULL)
		{
			LOG(llevBug, "BUG: make_alchemy_item(): failed to locate recipe artifact.\n");
			LOG(llevBug, "BUG: --requested recipe: %s of %s.\n", rp->arch_name, rp->title);
			return NULL;
		}

		give_artifact_abilities(item, art);
	}

	if (QUERY_FLAG(cauldron, FLAG_CURSED))
	{
		SET_FLAG(item, FLAG_CURSED);
	}

	if (QUERY_FLAG(cauldron, FLAG_DAMNED))
	{
		SET_FLAG(item, FLAG_DAMNED);
	}

	return item;
}

/**
 * Looks through the ingredient list, if we find a suitable object in it,
 * we will use that to make the requested artifact. Otherwise the code
 * returns a 'generic' item.
 * @param first_ingred Pointer to first item to check.
 * @param rp Recipe the player is trying.
 * @return NULL if no suitable item was found, new item otherwise. */
static object *find_transmution_ob(object *first_ingred, recipe *rp)
{
	object *item = NULL;

	/* Look for matching ingredient/prod archs */
	if (rp->transmute)
	{
		for (item = first_ingred; item; item = item->below)
		{
			if (!strcmp(item->arch->name, rp->arch_name))
			{
				break;
			}
		}
	}

	/* Failed, create a fresh object. Note no nrof > 1 because that would
	 * allow players to create massive amounts of artifacts easily. */
	if (!item || item->nrof > 1)
	{
		item = get_archetype(rp->arch_name);
	}

#ifdef ALCHEMY_DEBUG
	LOG(llevDebug, "DEBUG: recipe calls for%stransmution.\n", rp->transmute ? " " : " no ");

	if (strcmp(item->arch->name, rp->arch_name))
	{
		LOG(llevDebug, "WARNING: recipe calls for arch: %s\n", rp->arch_name);
	}

	LOG(llevDebug, "DEBUG: find_transmutable_ob(): returns arch %s(sp:%d)\n", item->arch->name, item->stats.sp);
#endif

	return item;
}

/**
 * Ouch. We didn't get the formula we wanted. This function simulates the
 * backfire effects -- worse effects as the level increases. If
 * SPELL_FAILURE_EFFECTS is defined some really evil things can happen to
 * the would be alchemist. This table probably needs some adjustments for
 * playbalance.
 * @param op Who tried to do alchemy.
 * @param cauldron Container that was used.
 * @param rp Rrecipe that failed.
 * @param danger Danger value, the higher the more evil the effect. */
static void alchemy_failure_effect(object *op, object *cauldron, recipe *rp, int danger)
{
	int level = 0;

	if (!op || !cauldron)
	{
		return;
	}

	if (danger > 1)
	{
		level = rndm(1, danger);
	}

#ifdef ALCHEMY_DEBUG
	LOG(llevDebug, "DEBUG: Alchemy_failure_effect(): using level=%d\n", level);
#endif

	/* possible outcomes based on level */

	/* Ingredients destroyed, and possibly create slag. */
	if (level < 25)
	{
		object *item = NULL;

		/* Slag created */
		if (rndm(0, 2))
		{
			object *tmp = cauldron->inv;
			int weight = 0;
			uint16 material = M_STONE;

			/* Slag has coadded ingredient properties */
			while (tmp)
			{
				weight += tmp->weight;

				if (!(material & tmp->material))
				{
					material = material | tmp->material;
				}

				tmp = tmp->below;
			}

			tmp = get_archetype("rock");
			tmp->weight = weight;
			tmp->value = 0;
			tmp->material = material;
			FREE_AND_COPY_HASH(tmp->name, "slag");
			item = insert_ob_in_ob(tmp, cauldron);
			CLEAR_FLAG(tmp, FLAG_CAN_ROLL);
			CLEAR_FLAG(tmp, FLAG_NO_PICK);
			CLEAR_FLAG(tmp, FLAG_NO_PASS);
		}

		remove_contents(cauldron->inv, item);
		new_draw_info_format(NDI_UNIQUE, op, "The %s %s.", cauldron->name, cauldron_sound());
		return;
	}
	/* Make tained item. */
	else if (level < 40)
	{
		object *tmp = NULL;

		if (!rp)
		{
			if ((rp = get_random_recipe(NULL)) == NULL)
			{
				return;
			}
		}

		if ((tmp = attempt_recipe(op, cauldron, 1, rp, -1)))
		{
			/* Curse it */
			if (!QUERY_FLAG(tmp, FLAG_CURSED))
			{
				SET_FLAG(tmp, FLAG_CURSED);
			}

			/* Special stuff for consumables */
			if (tmp->type == FOOD)
			{
				tmp->stats.sp = 0;

				/* Poisonous */
				if (rndm(0, 1))
				{
					tmp->type = FOOD;
					tmp->stats.hp = rndm(0, 149);
				}
			}

			/* Unsaleable item */
			tmp->value = 0;

			/* Change stats downward */
			do
			{
				change_attr_value(&tmp->stats, rndm(0, 6), (signed char) (-1 * (rndm(1, 3))));
			}
			while (rndm(0, 2));
		}

		return;
	}

	/* Make random recipe. */
	if (level == 40)
	{
		recipelist *fl;
		int numb = numb_ob_inside(cauldron);

		/* Take a lower recipe list */
		fl = get_formulalist(numb - 1);

		if (fl && (rp = get_random_recipe(fl)))
		{
			/* Even though random, don't grant user any EXP for it */
			(void) attempt_recipe(op, cauldron, 1, rp, -1);
		}
		else
		{
			alchemy_failure_effect(op, cauldron, rp, level - 1);
		}

		return;
	}
	/* Infuriate NPCs */
	else if (level < 45)
	{
		alchemy_failure_effect(op, cauldron, rp, level - 5);
		return;
	}
	/* Minor explosion/fireball */
	else if (level < 50)
	{
		object *tmp;

		remove_contents(cauldron->inv, NULL);

		switch (rndm(0, 2))
		{
			case 0:
				tmp = get_archetype("bomb");
				tmp->stats.dam = rndm(1, level);
				tmp->stats.hp = rndm(1, level);
				new_draw_info_format(NDI_UNIQUE, op, "The %s creates a bomb!", cauldron->name);
				break;

			default:
				tmp = get_archetype("fireball");
				tmp->stats.dam = rndm(1, level) / 5 + 1;
				tmp->stats.hp = rndm(1, level) / 10 + 2;
				new_draw_info_format(NDI_UNIQUE, op, "The %s erupts in flame!", cauldron->name);
				break;
		}

		tmp->x = cauldron->x, tmp->y = cauldron->y;
		insert_ob_in_map(tmp, op->map, NULL, 0);
		return;
	}
	/* Create monster */
	else if (level < 60)
	{
		new_draw_info_format(NDI_UNIQUE, op, "The %s %s.", cauldron->name, cauldron_sound());
		remove_contents(cauldron->inv, NULL);
		return;
	}
	/* Major fire */
	else if (level < 80)
	{
		remove_contents(cauldron->inv, NULL);
#if 0
		fire_arch_from_position(cauldron, cauldron, cauldron->x, cauldron->y, 0, spellarch[SP_L_FIREBALL], SP_L_FIREBALL, NULL);
#endif
		new_draw_info_format(NDI_UNIQUE, op, "The %s erupts in flame!", cauldron->name);
		return;
	}
	/* Whammy the cauldron */
	else if (level < 100)
	{
		if (!QUERY_FLAG(cauldron, FLAG_CURSED))
		{
			SET_FLAG(cauldron, FLAG_CURSED);
		}
		else
		{
			cauldron->magic--;
		}

		cauldron->magic -= rndm(0, 4);

		if (rndm(0, 1))
		{
			remove_contents(cauldron->inv, NULL);
			new_draw_info_format(NDI_UNIQUE, op, "Your %s turns darker then makes a gulping sound!", cauldron->name);
		}
		else
		{
			new_draw_info_format(NDI_UNIQUE, op, "Your %s becomes darker.", cauldron->name);
		}

		return;
	}
	/* Summon evil monsters */
	else if (level < 110)
	{
		object *tmp = get_random_mon();

		remove_contents(cauldron->inv, NULL);

		if (!tmp)
		{
			alchemy_failure_effect(op, cauldron, rp, level);
		}

		return;
	}
	/* Combo effect */
	else if (level < 150)
	{
		int roll = rndm(1, 3);

		while (roll)
		{
			alchemy_failure_effect(op, cauldron, rp, level - 39);
			roll--;
		}

		return;
	}
	/* Create random artifact */
	else if (level == 151)
	{
		object *tmp;

		/* this is meant to be better than prior possiblity,
		 * in this one, we allow *any* valid alchemy artifact
		 * to be made (rather than only those on the given
		 * formulalist) */
		if (!rp)
		{
			rp = get_random_recipe((recipelist *) NULL);
		}

		if (rp && (tmp = get_archetype(rp->arch_name)))
		{
			generate_artifact(tmp, rndm(1, op->level / 2 + 1) + 1, 0, 99);

			if ((tmp = insert_ob_in_ob(tmp, cauldron)))
			{
				remove_contents(cauldron->inv, tmp);
				new_draw_info_format(NDI_UNIQUE, op, "The %s %s.", cauldron->name, cauldron_sound());
			}
		}

		return;
	}
	/* Mana storm -- watch out! */
	else if (level < 200)
	{
		new_draw_info(NDI_UNIQUE, op, "You unwisely release potent forces!");
		remove_contents(cauldron->inv, NULL);
		cast_magic_storm(op, get_archetype("loose_magic"), level);
		return;
	}
}

/**
 * All but object "save_item" are removed from the container list. Note
 * we have to becareful to remove the inventories of objects in the
 * cauldron inventory (ex icecube has stuff in it).
 * @param first_ob Container from which to remove.
 * @param save_item What item not to remove. Can be NULL. */
static void remove_contents(object *first_ob, object *save_item)
{
	object *next, *tmp = first_ob;

	while (tmp)
	{
		next = tmp->below;

		if (tmp == save_item)
		{
			if (!(tmp = next))
			{
				break;
			}
			else
			{
				next = next->below;
			}
		}

		if (tmp->inv)
		{
			remove_contents(tmp->inv, NULL);
		}

		remove_ob(tmp);
		tmp = next;
	}
}

/**
 * "Danger" level will determine how bad the backfire could be if the
 * user fails to concoct a recipe properly. Factors include the number of
 * ingredients, the length of the name of each ingredient, the user's
 * effective level, the user's Int and the enchantment on the mixing
 * device (aka "cauldron"). Higher values of 'danger' indicate more
 * danger. Note that we assume that we have had the caster ready the
 * alchemy skill *before *this routine is called (no longer auto-readies
 * that skill).
 * @param caster Who is trying alchemy.
 * @param cauldron Container used.
 * @return Danger value. */
static int calc_alch_danger(object *caster, object *cauldron)
{
	object *item;
	char name[MAX_BUF];
	int danger = 0, nrofi = 0;

	/* Knowing alchemy skill reduces yer risk */
	if (caster->chosen_skill && caster->chosen_skill->stats.sp == SK_ALCHEMY)
	{
		danger -= SK_level(caster);
	}

	/* Better cauldrons reduce risk */
	danger -= cauldron->magic;

	/* Higher Int, lower the risk */
	danger -= 3 * (caster->stats.Int - 15);

	/* Ingredients. Longer names usually mean rarer stuff. Thus the
	 * backfire is worse. Also, more ingredients means we are attempting
	 * a more powerful potion, and thus the backfire will be worse. */
	for (item = cauldron->inv; item; item = item->below)
	{
		strcpy(name, item->name);

		if (item->title)
		{
			snprintf(name, sizeof(name), "%s %s", item->name, item->title);
		}

		danger += (strtoint(name) / 1000) + 3;
		nrofi++;
	}

	if (nrofi > 1)
	{
		danger *= nrofi;
	}

	/* Using a bad device is *very* stupid */
	if (QUERY_FLAG(cauldron, FLAG_CURSED))
	{
		danger += 80;
	}

	if (QUERY_FLAG(cauldron, FLAG_DAMNED))
	{
		danger += 200;
	}

#ifdef ALCHEMY_DEBUG
	LOG(llevDebug, "DEBUG: calc_alch_danger() returned danger=%d\n", danger);
#endif

	return danger;
}

/**
 * Handle use_skill for alchemy-like items.
 *
 * Will inform player if attempting to use unpaid cauldron or ingredient.
 * @param op Player trying to do alchemy.
 * @return 1 if any recipe was attempted, 0 otherwise. */
int use_alchemy(object *op)
{
	object *tmp, *item, *next;
	object *unpaid_cauldron = NULL;
	object *unpaid_item = NULL;
	int did_alchemy = 0;

	for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp != NULL; tmp = next)
	{
		next = tmp->above;

		if (QUERY_FLAG(tmp, FLAG_IS_CAULDRON))
		{
			if (QUERY_FLAG(tmp, FLAG_UNPAID))
			{
				unpaid_cauldron = tmp;
				continue;
			}

			unpaid_item = NULL;

			for (item = tmp->inv; item; item = item->below)
			{
				if (QUERY_FLAG(item, FLAG_UNPAID))
				{
					unpaid_item = item;
					break;
				}
			}

			if (unpaid_item != NULL)
			{
				continue;
			}

			attempt_do_alchemy(op, tmp);

			if (QUERY_FLAG(tmp, FLAG_APPLIED))
			{
				esrv_send_inventory(op, tmp);
			}

			did_alchemy = 1;
		}
	}

	if (unpaid_cauldron)
	{
		new_draw_info_format(NDI_UNIQUE, op, "You must pay for your %s first!", query_base_name(unpaid_cauldron, NULL));
	}
	else if (unpaid_item)
	{
		new_draw_info_format(NDI_UNIQUE, op, "You must pay for your %s first!", query_base_name(unpaid_item, NULL));
	}

	return did_alchemy;
}
