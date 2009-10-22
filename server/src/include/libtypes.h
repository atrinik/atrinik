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

/* altar.c */
extern int apply_altar(object *altar, object *sacrifice, object *originator);

/* armour_improver.c */
extern void apply_armour_improver(object *op, object *tmp);

/* book.c */
extern void apply_book(object *op, object *tmp);

/* container.c */
extern int esrv_apply_container(object *op, object *sack);
extern int container_link(player *pl, object *sack);
extern int container_unlink(player *pl, object *sack);
extern int container_trap(object *op, object *container);

/* converter.c */
extern int convert_item(object *item, object *converter);

/* food.c */
extern void apply_food(object *op, object *tmp);
extern void create_food_force(object *who, object *food, object *force);
extern void eat_special_food(object *who, object *food);
extern int dragon_eat_flesh(object *op, object *meal);

/* gravestone.c */
extern char *gravestone_text(object *op);

/* identify_altar.c */
extern int apply_identify_altar(object *money, object *altar, object *pl);

/* poison.c */
extern void apply_poison(object *op, object *tmp);

/* potion.c */
extern int apply_potion(object *op, object *tmp);

/* power_crystal.c */
extern void apply_power_crystal(object *op, object *crystal);

/* savebed.c */
extern void apply_savebed(object *op);

/* scroll.c */
extern void apply_scroll(object *op, object *tmp);

/* shop_mat.c */
extern int apply_shop_mat(object *shop_mat, object *op);

/* sign.c */
extern void apply_sign(object *op, object *sign);

/* skillscroll.c */
extern void apply_skillscroll(object *op, object *tmp);

/* spellbook.c */
extern void apply_spellbook(object *op, object *tmp);

/* treasure.c */
extern void apply_treasure(object *op, object *tmp);

/* weapon_improver.c */
extern int check_weapon_power(object *who, int improvs);
extern void apply_weapon_improver(object *op, object *tmp);
