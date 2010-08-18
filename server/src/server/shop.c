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
 * Functions dealing with shop handling, bargaining, etc. */

#include <global.h>

static sint64 pay_from_container(object *op, object *pouch, sint64 to_pay);

/**
 * Return the price of an item for a character.
 * @param tmp Object we're querying the price of.
 * @param who Who is inquiring. Can be NULL, only meaningful if player.
 * @param flag Combination of @ref F_xxx "F_xxx" flags.
 * @return The price for the item. */
sint64 query_cost(object *tmp, object *who, int flag)
{
	sint64 val;
	double diff;
	int number;
	int charisma = 11;

	if ((number = tmp->nrof) == 0)
	{
		number = 1;
	}

	/* Money is always identified */
	if (tmp->type == MONEY)
	{
		return (number * tmp->value);
	}

	/* Handle identified items */
	if (QUERY_FLAG(tmp, FLAG_IDENTIFIED) || !need_identify(tmp))
	{
		if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
		{
			return 0;
		}
		else
		{
			val = tmp->value * number;
		}
	}
	/* This area deals with objects that are not identified, but can be */
	else
	{
		if (tmp->arch != NULL)
		{
			if (flag == F_BUY)
			{
				LOG(llevBug, "BUG: query_cost(): Asking for buy-value of unidentified object %s.\n", query_name(tmp, NULL));
				val = tmp->arch->clone.value * number;
			}
			/* Trying to sell something, or get true value */
			else
			{
				/* Selling unidentified gems is *always* stupid */
				if (tmp->type == GEM || tmp->type == JEWEL || tmp->type == NUGGET || tmp->type == PEARL)
				{
					val = number * 3;
				}
				/* Don't want to give anything away */
				else if (tmp->type == POTION)
				{
					val = number * 50;
				}
				else
				{
					val = number * tmp->arch->clone.value;
				}
			}
		}
		else
		{
			/* No archetype with this object - we generate some dummy values to avoid server break */
			LOG(llevBug, "BUG: query_cost(): Have object with no archetype: %s\n", query_name(tmp, NULL));

			if (flag == F_BUY)
			{
				LOG(llevBug, "BUG: query_cost(): Asking for buy-value of unidentified object without arch.\n");
				val = number * 100;
			}
			else
			{
				val = number * 80;
			}
		}
	}

	/* Wands will count special. The base value is for a wand with one charge */
	if (tmp->type == WAND)
	{
		val += (val * tmp->level) * tmp->stats.food;
	}
	else if (tmp->type == ROD || tmp->type == HORN || tmp->type == POTION || tmp->type == SCROLL)
	{
		val += val * tmp->level;
	}

	/* We are done if we only want get the real value */
	if (flag == F_TRUE)
	{
		return val;
	}

	/* First, we adjust charisma for players and count skills in */
	if (who != NULL && who->type == PLAYER)
	{
		/* Used for SK_BARGAINING modification */
		charisma = who->stats.Cha;

		/* This skill will give us a charisma boost */
		if (find_skill(who, SK_BARGAINING))
		{
			charisma += 4;

			if (charisma > MAX_STAT)
			{
				charisma = MAX_STAT;
			}
		}
	}

	/* Now adjust for sell or buy multiplier */
	if (flag == F_BUY)
	{
		diff = (double) (1.0 - (double) cha_bonus[charisma]);
	}
	else
	{
		diff = (double) (0.20 + (double) cha_bonus[charisma]);
	}

	val = (val * (long) (1000 * (diff))) / 1000;

	/* We want to give at least 1 copper for items which have any
	 * value. */
	if (val < 1 && tmp->value > 0)
	{
		val = 1;
	}

	return val;
}

/**
 * Find the coin type that is worth more than 'c'. Starts at the cointype
 * placement.
 * @param c Value we're searching.
 * @param cointype First coin type to search.
 * @return Coin archetype, NULL if none found. */
static archetype *find_next_coin(sint64 c, int *cointype)
{
	archetype *coin;

	do
	{
		if (coins[*cointype] == NULL)
		{
			return NULL;
		}

		coin = find_archetype(coins[*cointype]);

		if (coin == NULL)
		{
			return NULL;
		}

		*cointype += 1;
	}
	while (coin->clone.value > c);

	return coin;
}

/**
 * Converts a price to number of coins.
 * @param cost Value to transform to currency.
 * @return Buffer containing the price. */
char *cost_string_from_value(sint64 cost)
{
	static char buf[MAX_BUF];
	archetype *coin, *next_coin;
	char *endbuf;
	sint64 num;
	int cointype = 0;

	coin = find_next_coin(cost, &cointype);

	if (coin == NULL)
	{
		return "nothing";
	}

	num = cost / coin->clone.value;
	cost -= num * coin->clone.value;

	if (num == 1)
	{
		snprintf(buf, sizeof(buf), "1 %s%s", material_real[coin->clone.material_real].name, coin->clone.name);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%"FMT64" %s%ss", num, material_real[coin->clone.material_real].name, coin->clone.name);
	}

	next_coin = find_next_coin(cost, &cointype);

	if (next_coin == NULL)
	{
		return buf;
	}

	do
	{
		endbuf = buf + strlen(buf);

		coin = next_coin;
		num = cost / coin->clone.value;
		cost -= num * coin->clone.value;

		if (cost == 0.0)
		{
			next_coin = NULL;
		}
		else
		{
			next_coin = find_next_coin(cost, &cointype);
		}

		if (next_coin)
		{
			/* There will be at least one more string to add to the list,
			 * use a comma. */
			strcat(endbuf, ", ");
			endbuf += 2;
		}
		else
		{
			strcat(endbuf, " and ");
			endbuf += 5;
		}

		if (num == 1)
		{
			sprintf(endbuf, "1 %s%s", material_real[coin->clone.material_real].name, coin->clone.name);
		}
		else
		{
			sprintf(endbuf, "%"FMT64" %s%ss", num, material_real[coin->clone.material_real].name, coin->clone.name);
		}

	}
	while (next_coin);

	return buf;
}

/**
 * Query the cost of an object.
 *
 * This is really a wrapper for cost_string_from_value() and
 * query_cost().
 * @param tmp Object we're querying the price of.
 * @param who Who is inquiring. Can be NULL, only meaningful if player.
 * @param flag Combination of @ref F_xxx "F_xxx" flags.
 * @return The cost string. */
char *query_cost_string(object *tmp, object *who, int flag)
{
	return cost_string_from_value(query_cost(tmp, who, flag));
}

/**
 * Finds out how much money the player is carrying, including what is in
 * containers and in bank.
 * @param op Item to get money for. Must be a player or a container.
 * @return Total money the player is carrying. */
sint64 query_money(object *op)
{
	object *tmp;
	sint64	total = 0;

	if (op->type != PLAYER && op->type != CONTAINER)
	{
		LOG(llevBug, "BUG: query_money(): Called with non player/container.\n");
		return 0;
	}

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == MONEY)
		{
			total += tmp->nrof * tmp->value;
		}
		else if (tmp->type == CONTAINER && ((!tmp->race || strstr(tmp->race, "gold")) || QUERY_FLAG(tmp, FLAG_APPLIED)))
		{
			total += query_money(tmp);
		}
		else if (tmp->arch->name == shstr_cons.player_info && tmp->name == shstr_cons.BANK_GENERAL)
		{
			total += tmp->value;
		}
	}

	return total;
}

/**
 * Takes the amount of money from the the player inventory and from it's
 * various pouches using the pay_from_container() function.
 * @param to_pay Amount to pay.
 * @param pl Player paying.
 * @return 0 if not enough money, in which case nothing is removed, 1 if
 * money was removed. */
int pay_for_amount(sint64 to_pay, object *pl)
{
	object *pouch;

	if (to_pay == 0)
	{
		return 1;
	}

	if (to_pay > query_money(pl))
	{
		return 0;
	}

	to_pay = pay_from_container(NULL, pl, to_pay);

	for (pouch = pl->inv; (pouch != NULL) && (to_pay > 0); pouch = pouch->below)
	{
		if (pouch->type == CONTAINER && pouch->inv && (QUERY_FLAG(pouch, FLAG_APPLIED) || (!pouch->race || strstr(pouch->race, "gold"))))
		{
			to_pay = pay_from_container(NULL, pouch, to_pay);
		}
	}

#ifndef REAL_WIZ
	if (QUERY_FLAG(pl, FLAG_WAS_WIZ))
	{
		SET_FLAG(op, FLAG_WAS_WIZ);
	}
#endif

	fix_player(pl);
	return 1;
}

/**
 * This is a wrapper for pay_from_container, which is called for the
 * player, then for each active container that can hold money until op is
 * paid for. Change will be left wherever the last of the price was paid
 * from.
 * @param op Object to buy.
 * @param pl Player buying.
 * @return 1 if object was bought, 0 otherwise. */
int pay_for_item(object *op, object *pl)
{
	sint64 to_pay = query_cost(op, pl, F_BUY);
	object *pouch;

	if (to_pay == 0.0)
	{
		return 1;
	}

	if (to_pay > query_money(pl))
	{
		return 0;
	}

	to_pay = pay_from_container(op, pl, to_pay);

	for (pouch = pl->inv; (pouch != NULL) && (to_pay > 0); pouch = pouch->below)
	{
		if (pouch->type == CONTAINER && pouch->inv && (QUERY_FLAG(pouch, FLAG_APPLIED) || (!pouch->race || strstr(pouch->race, "gold"))))
		{
			to_pay = pay_from_container(op, pouch, to_pay);
		}
	}

#ifndef REAL_WIZ
	if (QUERY_FLAG(pl, FLAG_WAS_WIZ))
	{
		SET_FLAG(op, FLAG_WAS_WIZ);
	}
#endif

	fix_player(pl);
	return 1;
}

/**
 * This pays for the item, and takes the proper amount of money off the
 * player.
 * @param op Player paying.
 * @param pouch Container (pouch or player) to remove the coins from.
 * @param to_pay Required amount.
 * @return Amount still not paid after using "pouch". */
static sint64 pay_from_container(object *op, object *pouch, sint64 to_pay)
{
	sint64 remain;
	int count, i;
	object *tmp, *coin_objs[NUM_COINS], *next, *bank_object = NULL;
	archetype *at;
	object *who;

	(void) op;

	if (pouch->type != PLAYER && pouch->type != CONTAINER)
	{
		return to_pay;
	}

	remain = to_pay;

	for (i = 0; i < NUM_COINS; i++)
	{
		coin_objs[i] = NULL;
	}

	/* This hunk should remove all the money objects from the player/container */
	for (tmp = pouch->inv; tmp; tmp = next)
	{
		next = tmp->below;

		if (tmp->type == MONEY)
		{
			for (i = 0; i < NUM_COINS; i++)
			{
				if (!strcmp(coins[NUM_COINS - 1 - i], tmp->arch->name) && (tmp->value == tmp->arch->clone.value))
				{
					/* This should not happen, but if it does, just merge
					 * the two. */
					if (coin_objs[i] != NULL)
					{
						LOG(llevBug, "BUG: pay_from_container(): %s has two money entries of (%s)\n", query_name(pouch, NULL), coins[NUM_COINS - 1 - i]);
						remove_ob(tmp);
						coin_objs[i]->nrof += tmp->nrof;
						esrv_del_item(CONTR(pouch), tmp->count, tmp->env);
					}
					else
					{
						remove_ob(tmp);

						if (pouch->type == PLAYER)
						{
							esrv_del_item(CONTR(pouch), tmp->count,tmp->env);
						}

						coin_objs[i] = tmp;
					}

					break;
				}
			}

			if (i == NUM_COINS)
			{
				LOG(llevBug, "BUG: pay_from_container(): Did not find string match for %s\n", tmp->arch->name);
			}
		}
		else if (tmp->arch->name == shstr_cons.player_info && tmp->name == shstr_cons.BANK_GENERAL)
		{
			bank_object = tmp;
		}
	}

	/* Fill in any gaps in the coin_objs array - needed to make change. */
	/* Note that the coin_objs array goes from least value to greatest value */
	for (i = 0; i < NUM_COINS; i++)
	{
		if (coin_objs[i] == NULL)
		{
			at = find_archetype(coins[NUM_COINS - 1 - i]);

			if (at == NULL)
			{
				LOG(llevBug, "BUG: pay_from_container(): Could not find %s archetype", coins[NUM_COINS - 1 - i]);
			}

			coin_objs[i] = get_object();
			copy_object(&at->clone, coin_objs[i], 0);
			coin_objs[i]->nrof = 0;
		}
	}

	for (i = 0; i < NUM_COINS; i++)
	{
		sint64 num_coins;

		if ((sint64) (coin_objs[i]->nrof * coin_objs[i]->value) > remain)
		{
			num_coins = remain / coin_objs[i]->value;

			if ((num_coins * coin_objs[i]->value) < remain)
			{
				num_coins++;
			}
		}
		else
		{
			num_coins = coin_objs[i]->nrof;
		}

		if (num_coins > ((sint64) 1 << 31))
		{
			LOG(llevDebug, "DEBUG: pay_from_container(): Money overflow value->nrof: number of coins > 2 ^ 32 (type coin %d)\n", i);
			num_coins = ((sint64) 1 << 31);
		}

		remain -= num_coins * coin_objs[i]->value;
		coin_objs[i]->nrof -= (uint32) num_coins;
		/* Now start making change.  Start at the coin value
		 * below the one we just did, and work down to
		 * the lowest value. */
		count = i - 1;

		while (remain < 0 && count >= 0)
		{
			num_coins = -remain / coin_objs[count]->value;
			coin_objs[count]->nrof += (uint32) num_coins;
			remain += num_coins * coin_objs[count]->value;
			count--;
		}
	}

	/* If there's still some remain, that means we could try to pay from
	 * bank. */
	if (bank_object && bank_object->value != 0 && remain != 0 && bank_object->value >= remain)
	{
		bank_object->value -= remain;
		remain = 0;
	}

	for (i = 0; i < NUM_COINS; i++)
	{
		if (coin_objs[i]->nrof)
		{
			object *tmp = insert_ob_in_ob(coin_objs[i], pouch);

			for (who = pouch; who && who->type != PLAYER && who->env != NULL; who = who->env)
			{
			}

			esrv_send_item(who, tmp);
			esrv_send_item (who, pouch);
			esrv_update_item(UPD_WEIGHT, who, pouch);

			if (pouch->type != PLAYER)
			{
				esrv_send_item(who, who);
				esrv_update_item(UPD_WEIGHT, who, who);
			}
		}
	}

	return remain;
}

/**
 * Descends containers looking for unpaid items, and pays for them.
 * @param pl Player buying the stuff.
 * @param op Object we are examining. If op has and inventory, we examine
 * that. Ii there are objects below op, we descend down.
 * @retval 0 Player still has unpaid items.
 * @retval 1 Player has paid for everything. */
int get_payment(object *pl, object *op)
{
	char buf[MAX_BUF];
	int ret = 1;

	if (op != NULL && op->inv)
	{
		ret = get_payment(pl, op->inv);
	}

	if (!ret)
	{
		return 0;
	}

	if (op != NULL && op->below)
	{
		ret = get_payment(pl, op->below);
	}

	if (!ret)
	{
		return 0;
	}

	if (op != NULL && QUERY_FLAG(op, FLAG_UNPAID))
	{
		strncpy(buf, query_cost_string(op, pl, F_BUY), sizeof(buf));

		if (!pay_for_item(op, pl))
		{
			sint64 i = query_cost(op, pl, F_BUY) - query_money(pl);

			CLEAR_FLAG(op, FLAG_UNPAID);
			new_draw_info_format(NDI_UNIQUE, pl, "You lack %s to buy %s.", cost_string_from_value(i), query_name(op, NULL));
			SET_FLAG(op, FLAG_UNPAID);
			return 0;
		}
		else
		{
			object *tmp, *c_cont = op->env;
			tag_t c = op->count;

			CLEAR_FLAG(op, FLAG_UNPAID);
			CLEAR_FLAG(op, FLAG_STARTEQUIP);

			if (pl->type == PLAYER)
			{
				new_draw_info_format(NDI_UNIQUE, pl, "You paid %s for %s.", buf, query_name(op, NULL));
			}

			tmp = merge_ob(op, NULL);

			if (pl->type == PLAYER)
			{
				/* It was merged */
				if (tmp)
				{
					esrv_del_item(CONTR(pl), c, c_cont);
					op = tmp;
				}

				esrv_send_item(pl, op);
			}
		}
	}

	return 1;
}

/**
 * Player is selling an item. Give money, print appropriate messages.
 * @param op Object to sell.
 * @param pl Player. Shouldn't be NULL or non player.
 * @param value If op is NULL, this value is used instead of using
 * query_cost(). */
void sell_item(object *op, object *pl, sint64 value)
{
	sint64 i;

	if (pl == NULL || pl->type != PLAYER)
	{
		LOG(llevDebug, "DEBUG: sell_item(): Object other than player tried to sell something.\n");
		return;
	}

	if (op == NULL)
	{
		i = value;
	}
	else
	{
		i = query_cost(op, pl, F_SELL);
	}

	if (op && op->custom_name)
	{
		FREE_AND_CLEAR_HASH(op->custom_name);
	}

	if (!i)
	{
		if (op)
		{
			new_draw_info_format(NDI_UNIQUE, pl, "We're not interested in %s.", query_name(op, NULL));
		}
	}

	i = insert_coins(pl, i);

	if (!op)
	{
		return;
	}

	if (i != 0)
	{
		LOG(llevBug, "BUG: Warning - payment not zero: %"FMT64"\n", i);
	}

	new_draw_info_format(NDI_UNIQUE, pl, "You receive %s for %s.", query_cost_string(op, pl, 1), query_name(op, NULL));
	SET_FLAG(op, FLAG_UNPAID);

	/* Identify the item. Makes any unidentified item sold to unique shop appear identified. */
	identify(op);
}

/**
 * Get money from a string.
 * @param text Text to get money from.
 * @param money Money block structure.
 * @return One of @ref MONEYSTRING_xxx. */
int get_money_from_string(char *text, struct _money_block *money)
{
	int pos = 0;
	char *word;

	memset(money, 0, sizeof(struct _money_block));

	/* Kill all whitespace */
	while (*text !='\0' && (isspace(*text) || !isprint(*text)))
	{
		text++;
	}

	/* Easy, special case: all money */
	if (!strncasecmp(text, "all", 3))
	{
		money->mode = MONEYSTRING_ALL;
		return money->mode;
	}

	money->mode = MONEYSTRING_NOTHING;

	while ((word = get_word_from_string(text, &pos)))
	{
		int i = 0, flag = *word;

		while (*(word + i) != '\0')
		{
			if (*(word + i) < '0' || *(word + i) > '9')
			{
				flag = 0;
			}

			i++;
		}

		/* If still set, we have a valid number in the word string */
		if (flag)
		{
			int value = atoi(word);

			/* A valid number - now lets look we have a valid money keyword */
			if (value > 0 && value < 1000000)
			{
				if ((word = get_word_from_string(text, &pos)) && *word != '\0')
				{
					size_t len = strlen(word);

					if (!strncasecmp("mithril", word, len))
					{
						money->mode = MONEYSTRING_AMOUNT;
						money->mithril += value;
					}
					else if (!strncasecmp("gold", word, len))
					{
						money->mode = MONEYSTRING_AMOUNT;
						money->gold += value;
					}
					else if (!strncasecmp("silver", word, len))
					{
						money->mode = MONEYSTRING_AMOUNT;
						money->silver += value;
					}
					else if (!strncasecmp("copper", word, len))
					{
						money->mode = MONEYSTRING_AMOUNT;
						money->copper += value;
					}
				}
			}
		}
	}

	return money->mode;
}

/**
 * Query money by type.
 * @param op Object.
 * @param value Value of the coin to query for.
 * @return Total number of coins of the specified type. */
int query_money_type(object *op, int value)
{
	object *tmp;
	sint64 total = 0;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == MONEY && tmp->value == value)
		{
			total += tmp->nrof;
		}
		else if (tmp->type == CONTAINER && !tmp->slaying && ((!tmp->race || strstr(tmp->race, "gold"))))
		{
			total += query_money_type(tmp, value);
		}

		if (total >= (sint64) value)
		{
			break;
		}
	}

	return (int) total;
}

/**
 * Remove money by type.
 * @param who Player object who is getting the money removed.
 * @param op Object we're removing from.
 * @param value Value of the coin type to remove.
 * @param amount Amount of money to remove.
 * @return Removed amount. */
sint64 remove_money_type(object *who, object *op, sint64 value, sint64 amount)
{
	object *tmp, *tmp2;

	for (tmp = op->inv; tmp; tmp = tmp2)
	{
		tmp2 = tmp->below;

		if (!amount && value != -1)
		{
			return amount;
		}

		if (tmp->type == MONEY && (tmp->value == value || value == -1))
		{
			if ((sint64) tmp->nrof <= amount || value == -1)
			{
				object *env = tmp->env;

				if (value == -1)
				{
					amount += (tmp->nrof * tmp->value);
				}
				else
				{
					amount -= tmp->nrof;
				}

				remove_ob(tmp);

				if (op->type == PLAYER)
				{
					esrv_del_item(CONTR(op), tmp->count, NULL);
				}
				else
				{
					esrv_del_item(NULL, tmp->count, env);
				}
			}
			else
			{
				tmp->nrof -= (uint32) amount;
				amount = 0;

				esrv_send_item(who, tmp);
				esrv_send_item(who, op);
				esrv_update_item(UPD_WEIGHT, who, op);

				if (op->type != PLAYER)
				{
					esrv_send_item(who, who);
					esrv_update_item(UPD_WEIGHT, who, who);
				}
			}
		}
		else if (tmp->type == CONTAINER && !tmp->slaying && ((!tmp->race || strstr(tmp->race, "gold"))))
		{
			amount = remove_money_type(who, tmp, value, amount);
		}
	}

	return amount;
}

/**
 * Insert money inside player.
 * @param pl Player.
 * @param money Money object to insert.
 * @param nrof How many money objects to insert to player. */
void insert_money_in_player(object *pl, object *money, uint32 nrof)
{
	object *tmp = get_object();
	copy_object(money, tmp, 0);
	tmp->nrof = nrof;
	tmp = insert_ob_in_ob(tmp, pl);
	esrv_send_item(pl, tmp);
	esrv_send_item(pl, pl);
	esrv_update_item(UPD_WEIGHT, pl, pl);
}

/**
 * Deposit money to player's bank object.
 * @param op Player.
 * @param bank Bank object in player's inventory.
 * @param text What was said to trigger this.
 * @return One of @ref BANK_xxx. */
int bank_deposit(object *op, object *bank, char *text)
{
	int pos = 0;
	_money_block money;

	get_word_from_string(text, &pos);
	get_money_from_string(text + pos , &money);

	if (!money.mode)
	{
		return BANK_SYNTAX_ERROR;
	}
	else if (money.mode == MONEYSTRING_ALL)
	{
		bank->value += remove_money_type(op, op, -1, 0);
		fix_player(op);
	}
	else
	{
		if (money.mithril)
		{
			if (query_money_type(op, coins_arch[0]->clone.value) < money.mithril)
			{
				return BANK_DEPOSIT_MITHRIL;
			}
		}

		if (money.gold)
		{
			if (query_money_type(op, coins_arch[1]->clone.value) < money.gold)
			{
				return BANK_DEPOSIT_GOLD;
			}
		}

		if (money.silver)
		{
			if (query_money_type(op, coins_arch[2]->clone.value) < money.silver)
			{
				return BANK_DEPOSIT_SILVER;
			}
		}

		if (money.copper)
		{
			if (query_money_type(op, coins_arch[3]->clone.value) < money.copper)
			{
				return BANK_DEPOSIT_COPPER;
			}
		}

		if (money.mithril)
		{
			remove_money_type(op, op, coins_arch[0]->clone.value, money.mithril);
		}

		if (money.gold)
		{
			remove_money_type(op, op, coins_arch[1]->clone.value, money.gold);
		}

		if (money.silver)
		{
			remove_money_type(op, op, coins_arch[2]->clone.value, money.silver);
		}

		if (money.copper)
		{
			remove_money_type(op, op, coins_arch[3]->clone.value, money.copper);
		}

		bank->value += money.mithril * coins_arch[0]->clone.value + money.gold * coins_arch[1]->clone.value + money.silver * coins_arch[2]->clone.value + money.copper * coins_arch[3]->clone.value;
		fix_player(op);
	}

	return BANK_SUCCESS;
}

/**
 * Withdraw money player previously stored in bank object.
 * @param op Player.
 * @param bank Bank object in player's inventory.
 * @param text What was said to trigger this.
 * @return One of @ref BANK_xxx. */
int bank_withdraw(object *op, object *bank, char *text)
{
	int pos = 0;
	sint64 big_value;
	_money_block money;

	get_word_from_string(text, &pos);
	get_money_from_string(text + pos , &money);

	if (!money.mode)
	{
		return BANK_SYNTAX_ERROR;
	}
	else if (money.mode == MONEYSTRING_ALL)
	{
		sell_item(NULL, op, bank->value);
		bank->value = 0;
		fix_player(op);
	}
	else
	{
		/* Just to set a border. */
		if (money.mithril > 100000 || money.gold > 100000 || money.silver > 1000000 || money.copper > 1000000)
		{
			return BANK_WITHDRAW_HIGH;
		}

		big_value = money.mithril * coins_arch[0]->clone.value + money.gold * coins_arch[1]->clone.value + money.silver * coins_arch[2]->clone.value + money.copper * coins_arch[3]->clone.value;

		if (big_value > bank->value)
		{
			return BANK_WITHDRAW_MISSING;
		}

		if (!player_can_carry(op, money.mithril * coins_arch[0]->clone.weight + money.gold * coins_arch[1]->clone.weight + money.silver * coins_arch[2]->clone.weight + money.copper * coins_arch[3]->clone.weight))
		{
			return BANK_WITHDRAW_OVERWEIGHT;
		}

		if (money.mithril)
		{
			insert_money_in_player(op, &coins_arch[0]->clone, money.mithril);
		}

		if (money.gold)
		{
			insert_money_in_player(op, &coins_arch[1]->clone, money.gold);
		}

		if (money.silver)
		{
			insert_money_in_player(op, &coins_arch[2]->clone, money.silver);
		}

		if (money.copper)
		{
			insert_money_in_player(op, &coins_arch[3]->clone, money.copper);
		}

		bank->value -= big_value;
		fix_player(op);
	}

	return BANK_SUCCESS;
}

/**
 * Insert coins into a player.
 * @param pl Player.
 * @param value Value of coins to insert (for example, 120 for 1 silver and 20 copper).
 * @return value. */
sint64 insert_coins(object *pl, sint64 value)
{
	int count;
	object *tmp, *pouch;
	archetype *at;

	for (count = 0; coins[count]; count++)
	{
		at = find_archetype(coins[count]);

		if (at == NULL)
		{
			LOG(llevBug, "BUG: Could not find %s archetype", coins[count]);
		}
		else if ((value / at->clone.value) > 0)
		{
			for (pouch = pl->inv; pouch; pouch = pouch->below)
			{
				if (pouch->type == CONTAINER && QUERY_FLAG(pouch, FLAG_APPLIED) && pouch->race && strstr(pouch->race, "gold"))
				{
					int w = (int) ((float) at->clone.weight * pouch->weapon_speed);
					uint32 n = (uint32) (value / at->clone.value);

					/* Prevent FPE */
					if (w == 0)
					{
						w = 1;
					}

					if (n > 0 && (!pouch->weight_limit || pouch->carrying + w <= (sint32) pouch->weight_limit))
					{
						if (pouch->weight_limit && ((sint32)pouch->weight_limit-pouch->carrying) / w < (sint32) n)
						{
							n = (pouch->weight_limit-pouch->carrying) / w;
						}

						tmp = get_object();
						copy_object(&at->clone, tmp, 0);
						tmp->nrof = n;
						value -= tmp->nrof * tmp->value;
						tmp = insert_ob_in_ob(tmp, pouch);
						esrv_send_item(pl, tmp);
						esrv_send_item(pl, pouch);
						esrv_update_item(UPD_WEIGHT, pl, pouch);
						esrv_send_item(pl, pl);
						esrv_update_item(UPD_WEIGHT, pl, pl);
					}
				}
			}

			if (value / at->clone.value > 0)
			{
				tmp = get_object();
				copy_object(&at->clone, tmp, 0);
				tmp->nrof = (uint32) (value / tmp->value);
				value -= tmp->nrof * tmp->value;
				tmp = insert_ob_in_ob(tmp, pl);
				esrv_send_item(pl, tmp);
				esrv_send_item(pl, pl);
				esrv_update_item(UPD_WEIGHT, pl, pl);
			}
		}
	}

	return value;
}
