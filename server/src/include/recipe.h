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

/* 'recipe' and 'recipelist' are used by the alchemy code */
typedef struct recipestruct
{
	/* distinguishing name of product */
	const char *title;

	/* the archetype of the final product made */
	const char *arch_name;

	/* chance that recipe for this item will appear
	 * in an alchemical grimore */
	int chance;

	/* an index value derived from formula ingredients */
	int index;

	/* if defined, one of the formula ingredients is
	 * used as the basis for the product object */
	int transmute;

	/* The maximum number of items produced by the recipe */
	int yield;

	/* comma delimited list of ingredients */
	linked_char *ingred;

	struct recipestruct *next;

	/* keycode needed to use the recipe */
	const char *keycode;
} recipe;

typedef struct recipeliststruct
{
	int total_chance;

	/* number of recipes in this list */
	int number;

	/* pointer to first recipe in this list */
	struct recipestruct *items;

	/* pointer to next recipe list */
	struct recipeliststruct *next;
} recipelist;
