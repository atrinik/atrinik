/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
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

    if ((number = tmp->nrof) == 0) {
        number = 1;
    }

    /* Money is always identified */
    if (tmp->type == MONEY) {
        return (number * tmp->value);
    }

    /* Handle identified items */
    if (QUERY_FLAG(tmp, FLAG_IDENTIFIED) || !need_identify(tmp)) {
        if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED)) {
            return 0;
        }
        else {
            val = tmp->value * number;
        }
    }
    /* This area deals with objects that are not identified, but can be */
    else {
        if (tmp->arch != NULL) {
            if (flag == COST_BUY) {
                logger_print(LOG(BUG), "Asking for buy-value of unidentified object %s.", query_name(tmp, NULL));
                val = tmp->arch->clone.value * number;
            }
            /* Trying to sell something, or get true value */
            else {
                /* Selling unidentified gems is *always* stupid */
                if (tmp->type == GEM || tmp->type == JEWEL || tmp->type == NUGGET || tmp->type == PEARL) {
                    val = number * 3;
                }
                /* Don't want to give anything away */
                else if (tmp->type == POTION) {
                    val = number * 50;
                }
                else {
                    val = number * tmp->arch->clone.value;
                }
            }
        }
        else {
            /* No archetype with this object - we generate some dummy values to
             * avoid server break */
            logger_print(LOG(BUG), "Have object with no archetype: %s", query_name(tmp, NULL));

            if (flag == COST_BUY) {
                logger_print(LOG(BUG), "Asking for buy-value of unidentified object without arch.");
                val = number * 100;
            }
            else {
                val = number * 80;
            }
        }
    }

    /* Wands will count special. The base value is for a wand with one charge */
    if (tmp->type == WAND) {
        val += (val * tmp->level) * tmp->stats.food;
    }
    else if (tmp->type == ROD || tmp->type == POTION || tmp->type == SCROLL) {
        val += val * tmp->level;
    }

    /* We are done if we only want get the real value */
    if (flag == COST_TRUE) {
        return val;
    }

    /* Now adjust for sell or buy multiplier */
    if (flag == COST_BUY) {
        diff = 1.0;
    }
    else {
        diff = 0.20;
    }

    val = (val * (long) (1000 * (diff))) / 1000;

    /* We want to give at least 1 copper for items which have any
     * value. */
    if (val < 1 && tmp->value > 0) {
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
        if (coins[*cointype] == NULL) {
            return NULL;
        }

        coin = find_archetype(coins[*cointype]);

        if (coin == NULL) {
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

    if (coin == NULL) {
        return "nothing";
    }

    num = cost / coin->clone.value;
    cost -= num * coin->clone.value;

    if (num == 1) {
        snprintf(buf, sizeof(buf), "1 %s%s", material_real[coin->clone.material_real].name, coin->clone.name);
    }
    else {
        snprintf(buf, sizeof(buf), "%"FMT64 " %s%ss", num, material_real[coin->clone.material_real].name, coin->clone.name);
    }

    next_coin = find_next_coin(cost, &cointype);

    if (next_coin == NULL) {
        return buf;
    }

    do
    {
        endbuf = buf + strlen(buf);

        coin = next_coin;
        num = cost / coin->clone.value;
        cost -= num * coin->clone.value;

        if (cost == 0.0) {
            next_coin = NULL;
        }
        else {
            next_coin = find_next_coin(cost, &cointype);
        }

        if (next_coin) {
            /* There will be at least one more string to add to the list,
             * use a comma. */
            strcat(endbuf, ", ");
            endbuf += 2;
        }
        else {
            strcat(endbuf, " and ");
            endbuf += 5;
        }

        if (num == 1) {
            sprintf(endbuf, "1 %s%s", material_real[coin->clone.material_real].name, coin->clone.name);
        }
        else {
            sprintf(endbuf, "%"FMT64 " %s%ss", num, material_real[coin->clone.material_real].name, coin->clone.name);
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
    sint64 total = 0;

    if (op->type != PLAYER && op->type != CONTAINER) {
        logger_print(LOG(BUG), "Called with non player/container.");
        return 0;
    }

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        if (tmp->type == MONEY) {
            total += tmp->nrof * tmp->value;
        }
        else if (tmp->type == CONTAINER && ((!tmp->race || strstr(tmp->race, "gold")) || QUERY_FLAG(tmp, FLAG_APPLIED))) {
            total += query_money(tmp);
        }
        else if (tmp->arch->name == shstr_cons.player_info && tmp->name == shstr_cons.BANK_GENERAL) {
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

    if (to_pay == 0) {
        return 1;
    }

    if (to_pay > query_money(pl)) {
        return 0;
    }

    to_pay = pay_from_container(NULL, pl, to_pay);

    for (pouch = pl->inv; (pouch != NULL) && (to_pay > 0); pouch = pouch->below) {
        if (pouch->type == CONTAINER && pouch->inv && (QUERY_FLAG(pouch, FLAG_APPLIED) || (!pouch->race || strstr(pouch->race, "gold")))) {
            to_pay = pay_from_container(NULL, pouch, to_pay);
        }
    }

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
    sint64 to_pay = query_cost(op, pl, COST_BUY);
    object *pouch;

    if (to_pay == 0.0) {
        return 1;
    }

    if (to_pay > query_money(pl)) {
        return 0;
    }

    to_pay = pay_from_container(op, pl, to_pay);

    for (pouch = pl->inv; (pouch != NULL) && (to_pay > 0); pouch = pouch->below) {
        if (pouch->type == CONTAINER && pouch->inv && (QUERY_FLAG(pouch, FLAG_APPLIED) || (!pouch->race || strstr(pouch->race, "gold")))) {
            to_pay = pay_from_container(op, pouch, to_pay);
        }
    }

    fix_player(pl);
    return 1;
}

/**
 * This pays for the item, and takes the proper amount of money off the
 * player.
 * @param op Player paying.
 * @param pouch Container (pouch or player) to remove the coins from.
 * @param to_pay Required amount.
 * @return Amount still not paid after using "pouch".
 * @todo Should be able to avoid the extra object allocations...
 */
static sint64 pay_from_container(object *op, object *pouch, sint64 to_pay)
{
    sint64 remain;
    int count, i;
    object *tmp, *coin_objs[NUM_COINS], *next, *bank_object = NULL;

    (void) op;

    if (pouch->type != PLAYER && pouch->type != CONTAINER) {
        return to_pay;
    }

    remain = to_pay;

    for (i = 0; i < NUM_COINS; i++) {
        coin_objs[i] = NULL;
    }

    /* This hunk should remove all the money objects from the player/container
     * */
    for (tmp = pouch->inv; tmp; tmp = next) {
        next = tmp->below;

        if (tmp->type == MONEY) {
            for (i = 0; i < NUM_COINS; i++) {
                if (!strcmp(coins[NUM_COINS - 1 - i], tmp->arch->name) && (tmp->value == tmp->arch->clone.value)) {
                    /* This should not happen, but if it does, just merge
                     * the two. */
                    if (coin_objs[i] != NULL) {
                        logger_print(LOG(BUG), "%s has two money entries of (%s)", query_name(pouch, NULL), coins[NUM_COINS - 1 - i]);
                        object_remove(tmp, 0);
                        coin_objs[i]->nrof += tmp->nrof;
                    }
                    else {
                        object_remove(tmp, 0);
                        coin_objs[i] = tmp;
                    }

                    break;
                }
            }

            if (i == NUM_COINS) {
                logger_print(LOG(BUG), "Did not find string match for %s", tmp->arch->name);
            }
        }
        else if (tmp->arch->name == shstr_cons.player_info && tmp->name == shstr_cons.BANK_GENERAL) {
            bank_object = tmp;
        }
    }

    /* Fill in any gaps in the coin_objs array - needed to make change. */
    /* Note that the coin_objs array goes from least value to greatest value */
    for (i = 0; i < NUM_COINS; i++) {
        if (coin_objs[i] == NULL) {
            coin_objs[i] = get_archetype(coins[NUM_COINS - 1 - i]);
            coin_objs[i]->nrof = 0;
        }
    }

    for (i = 0; i < NUM_COINS; i++) {
        sint64 num_coins;

        if ((sint64) (coin_objs[i]->nrof * coin_objs[i]->value) > remain) {
            num_coins = remain / coin_objs[i]->value;

            if ((num_coins * coin_objs[i]->value) < remain) {
                num_coins++;
            }
        }
        else {
            num_coins = coin_objs[i]->nrof;
        }

        if (num_coins > SINT32_MAX) {
            logger_print(LOG(DEBUG), "Money overflow value->nrof: number of coins > SINT32_MAX (type coin %d)", i);
            num_coins = SINT32_MAX;
        }

        remain -= num_coins * coin_objs[i]->value;
        coin_objs[i]->nrof -= (uint32) num_coins;
        /* Now start making change.  Start at the coin value
         * below the one we just did, and work down to
         * the lowest value. */
        count = i - 1;

        while (remain < 0 && count >= 0) {
            num_coins = -remain / coin_objs[count]->value;
            coin_objs[count]->nrof += (uint32) num_coins;
            remain += num_coins * coin_objs[count]->value;
            count--;
        }
    }

    /* If there's still some remain, that means we could try to pay from
     * bank. */
    if (bank_object && bank_object->value != 0 && remain != 0 && bank_object->value >= remain) {
        bank_object->value -= remain;
        remain = 0;
    }

    for (i = 0; i < NUM_COINS; i++) {
        if (coin_objs[i]->nrof) {
            insert_ob_in_ob(coin_objs[i], pouch);
        }
        else {
            object_destroy(coin_objs[i]);
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
    int ret = 1;

    if (op != NULL && op->inv) {
        ret = get_payment(pl, op->inv);
    }

    if (ret == 0) {
        return 0;
    }

    if (op != NULL && op->below) {
        ret = get_payment(pl, op->below);
    }

    if (ret == 0) {
        return 0;
    }

    if (op != NULL && QUERY_FLAG(op, FLAG_UNPAID)) {
        if (!pay_for_item(op, pl)) {
            CLEAR_FLAG(op, FLAG_UNPAID);
            draw_info_format(COLOR_WHITE, pl, "You lack %s to buy %s.", cost_string_from_value(query_cost(op, pl, COST_BUY) - query_money(pl)), query_name(op, NULL));
            SET_FLAG(op, FLAG_UNPAID);
            return 0;
        }
        else {
            CLEAR_FLAG(op, FLAG_UNPAID);
            CLEAR_FLAG(op, FLAG_STARTEQUIP);

            if (pl->type == PLAYER) {
                draw_info_format(COLOR_WHITE, pl, "You paid %s for %s.", query_cost_string(op, pl, COST_BUY), query_name(op, NULL));
            }

            /* If the object wasn't merged, send flags update. */
            if (object_merge(op) == op) {
                esrv_update_item(UPD_FLAGS, op);
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

    if (pl == NULL || pl->type != PLAYER) {
        logger_print(LOG(DEBUG), "Object other than player tried to sell something.");
        return;
    }

    if (op == NULL) {
        i = value;
    }
    else {
        i = query_cost(op, pl, COST_SELL);
    }

    if (op && op->custom_name) {
        FREE_AND_CLEAR_HASH(op->custom_name);
    }

    if (!i) {
        if (op) {
            draw_info_format(COLOR_WHITE, pl, "We're not interested in %s.", query_name(op, NULL));
        }
    }

    i = insert_coins(pl, i);

    if (!op) {
        return;
    }

    if (i != 0) {
        logger_print(LOG(BUG), "Warning - payment not zero: %"FMT64, i);
    }

    draw_info_format(COLOR_WHITE, pl, "You receive %s for %s.", query_cost_string(op, pl, 1), query_name(op, NULL));
    SET_FLAG(op, FLAG_UNPAID);

    /* Identify the item. Makes any unidentified item sold to unique shop appear
     * identified. */
    identify(op);
}

/**
 * Get money from a string.
 * @param text Text to get money from.
 * @param money Money block structure.
 * @return One of @ref MONEYSTRING_xxx. */
int get_money_from_string(const char *text, struct _money_block *money)
{
    size_t pos = 0;
    char word[MAX_BUF];
    int value;

    memset(money, 0, sizeof(struct _money_block));

    /* Kill all whitespace */
    while (*text !='\0' && (isspace(*text) || !isprint(*text))) {
        text++;
    }

    /* Easy, special case: all money */
    if (!strncasecmp(text, "all", 3)) {
        money->mode = MONEYSTRING_ALL;
        return money->mode;
    }

    money->mode = MONEYSTRING_NOTHING;

    while (string_get_word(text, &pos, ' ', word, sizeof(word), 0)) {
        if (!string_isdigit(word)) {
            continue;
        }

        value = atoi(word);

        if (value > 0 && value < 1000000) {
            if (string_get_word(text, &pos, ' ', word, sizeof(word), 0)) {
                size_t len;

                len = strlen(word);

                if (!strncasecmp("mithril", word, len)) {
                    money->mode = MONEYSTRING_AMOUNT;
                    money->mithril += value;
                }
                else if (!strncasecmp("gold", word, len)) {
                    money->mode = MONEYSTRING_AMOUNT;
                    money->gold += value;
                }
                else if (!strncasecmp("silver", word, len)) {
                    money->mode = MONEYSTRING_AMOUNT;
                    money->silver += value;
                }
                else if (!strncasecmp("copper", word, len)) {
                    money->mode = MONEYSTRING_AMOUNT;
                    money->copper += value;
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

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        if (tmp->type == MONEY && tmp->value == value) {
            total += tmp->nrof;
        }
        else if (tmp->type == CONTAINER && !tmp->slaying && ((!tmp->race || strstr(tmp->race, "gold")))) {
            total += query_money_type(tmp, value);
        }

        if (total >= (sint64) value) {
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

    for (tmp = op->inv; tmp; tmp = tmp2) {
        tmp2 = tmp->below;

        if (!amount && value != -1) {
            return amount;
        }

        if (tmp->type == MONEY && (tmp->value == value || value == -1)) {
            if ((sint64) tmp->nrof <= amount || value == -1) {
                if (value == -1) {
                    amount += (tmp->nrof * tmp->value);
                }
                else {
                    amount -= tmp->nrof;
                }

                object_remove(tmp, 0);
            }
            else {
                tmp->nrof -= (uint32) amount;
                amount = 0;
            }
        }
        else if (tmp->type == CONTAINER && !tmp->slaying && ((!tmp->race || strstr(tmp->race, "gold")))) {
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
    insert_ob_in_ob(tmp, pl);
}

/**
 * Find bank player info object in player's inventory.
 * @param op Where to look for the player info object.
 * @return The player info object if found, NULL otherwise. */
object *bank_get_info(object *op)
{
    object *tmp;

    for (tmp = op->inv; tmp; tmp = tmp->below) {
        if (tmp->arch->name == shstr_cons.player_info && tmp->name == shstr_cons.BANK_GENERAL) {
            return tmp;
        }
    }

    return NULL;
}

/**
 * Create a new bank player info object and insert it to 'op'.
 * @param op Player.
 * @return The created player info object. */
object *bank_create_info(object *op)
{
    object *bank = get_archetype(shstr_cons.player_info);

    FREE_AND_COPY_HASH(bank->name, shstr_cons.BANK_GENERAL);
    insert_ob_in_ob(bank, op);

    return bank;
}

/**
 * Convenience function to either find a bank player info object and if
 * not found, create a new one.
 * @param op Player object.
 * @return The bank player info object. Never NULL. */
object *bank_get_create_info(object *op)
{
    object *bank = bank_get_info(op);

    if (!bank) {
        bank = bank_create_info(op);
    }

    return bank;
}

/**
 * Query how much money player has stored in bank.
 * @param op Player to query for.
 * @return The money stored. */
sint64 bank_get_balance(object *op)
{
    object *bank = bank_get_info(op);

    if (!bank) {
        return 0;
    }

    return bank->value;
}

/**
 * Deposit money to player's bank object.
 * @param op Player.
 * @param text What was said to trigger this.
 * @param[out] value Will contain the deposited amount.
 * @return One of @ref BANK_xxx. */
int bank_deposit(object *op, const char *text, sint64 *value)
{
    _money_block money;
    object *bank;

    get_money_from_string(text, &money);
    *value = 0;

    if (!money.mode) {
        return BANK_SYNTAX_ERROR;
    }
    else if (money.mode == MONEYSTRING_ALL) {
        bank = bank_get_create_info(op);
        *value = remove_money_type(op, op, -1, 0);
        bank->value += *value;
        fix_player(op);
    }
    else {
        if (money.mithril) {
            if (query_money_type(op, coins_arch[0]->clone.value) < money.mithril) {
                return BANK_DEPOSIT_MITHRIL;
            }
        }

        if (money.gold) {
            if (query_money_type(op, coins_arch[1]->clone.value) < money.gold) {
                return BANK_DEPOSIT_GOLD;
            }
        }

        if (money.silver) {
            if (query_money_type(op, coins_arch[2]->clone.value) < money.silver) {
                return BANK_DEPOSIT_SILVER;
            }
        }

        if (money.copper) {
            if (query_money_type(op, coins_arch[3]->clone.value) < money.copper) {
                return BANK_DEPOSIT_COPPER;
            }
        }

        if (money.mithril) {
            remove_money_type(op, op, coins_arch[0]->clone.value, money.mithril);
        }

        if (money.gold) {
            remove_money_type(op, op, coins_arch[1]->clone.value, money.gold);
        }

        if (money.silver) {
            remove_money_type(op, op, coins_arch[2]->clone.value, money.silver);
        }

        if (money.copper) {
            remove_money_type(op, op, coins_arch[3]->clone.value, money.copper);
        }

        bank = bank_get_create_info(op);
        *value = money.mithril * coins_arch[0]->clone.value + money.gold * coins_arch[1]->clone.value + money.silver * coins_arch[2]->clone.value + money.copper * coins_arch[3]->clone.value;
        bank->value += *value;
        fix_player(op);
    }

    return BANK_SUCCESS;
}

/**
 * Withdraw money player previously stored in bank object.
 * @param op Player.
 * @param bank Bank object in player's inventory.
 * @param text What was said to trigger this.
 * @param[out] value Will contain the withdrawn amount.
 * @return One of @ref BANK_xxx. */
int bank_withdraw(object *op, const char *text, sint64 *value)
{
    sint64 big_value;
    _money_block money;
    object *bank;

    get_money_from_string(text, &money);

    bank = bank_get_info(op);
    *value = 0;

    if (!bank || !bank->value) {
        return BANK_WITHDRAW_MISSING;
    }

    if (!money.mode) {
        return BANK_SYNTAX_ERROR;
    }
    else if (money.mode == MONEYSTRING_ALL) {
        *value = bank->value;
        sell_item(NULL, op, bank->value);
        bank->value = 0;
        fix_player(op);
    }
    else {
        /* Just to set a border. */
        if (money.mithril > 100000 || money.gold > 100000 || money.silver > 1000000 || money.copper > 1000000) {
            return BANK_WITHDRAW_HIGH;
        }

        big_value = money.mithril * coins_arch[0]->clone.value + money.gold * coins_arch[1]->clone.value + money.silver * coins_arch[2]->clone.value + money.copper * coins_arch[3]->clone.value;

        if (big_value > bank->value) {
            return BANK_WITHDRAW_MISSING;
        }

        if (!player_can_carry(op, money.mithril * coins_arch[0]->clone.weight + money.gold * coins_arch[1]->clone.weight + money.silver * coins_arch[2]->clone.weight + money.copper * coins_arch[3]->clone.weight)) {
            return BANK_WITHDRAW_OVERWEIGHT;
        }

        if (money.mithril) {
            insert_money_in_player(op, &coins_arch[0]->clone, money.mithril);
        }

        if (money.gold) {
            insert_money_in_player(op, &coins_arch[1]->clone, money.gold);
        }

        if (money.silver) {
            insert_money_in_player(op, &coins_arch[2]->clone, money.silver);
        }

        if (money.copper) {
            insert_money_in_player(op, &coins_arch[3]->clone, money.copper);
        }

        *value = big_value;
        bank->value -= big_value;
        fix_player(op);
    }

    return BANK_SUCCESS;
}

/**
 * Insert coins into a player.
 * @param pl Player.
 * @param value Value of coins to insert (for example, 120 for 1 silver and 20
 * copper).
 * @return value. */
sint64 insert_coins(object *pl, sint64 value)
{
    int count;
    object *tmp, *pouch;
    archetype *at;
    uint32 n;

    for (count = 0; coins[count]; count++) {
        at = find_archetype(coins[count]);

        if (at == NULL) {
            logger_print(LOG(BUG), "Could not find %s archetype", coins[count]);
        }
        else if ((value / at->clone.value) > 0) {
            for (pouch = pl->inv; pouch; pouch = pouch->below) {
                if (pouch->type == CONTAINER && QUERY_FLAG(pouch, FLAG_APPLIED) && pouch->race && strstr(pouch->race, "gold")) {
                    double w;

                    w = (float) at->clone.weight * pouch->weapon_speed;
                    n = (uint32) (value / at->clone.value);

                    if (n > 0 && (!pouch->weight_limit || pouch->carrying + w <= (sint32) pouch->weight_limit)) {
                        if (w > 0.0 && pouch->weight_limit && ((sint32) pouch->weight_limit - pouch->carrying) / w < (sint32) n) {
                            n = (pouch->weight_limit - pouch->carrying) / w;
                        }

                        tmp = get_object();
                        copy_object(&at->clone, tmp, 0);
                        tmp->nrof = n;
                        value -= tmp->nrof * tmp->value;
                        insert_ob_in_ob(tmp, pouch);
                    }
                }
            }

            if (value / at->clone.value > 0) {
                n = (uint32) (value / at->clone.value);

                if (n > 0 && pl->carrying + at->clone.weight <= (sint32) weight_limit[MIN(pl->stats.Str, MAX_STAT)]) {
                    if (((sint32) weight_limit[MIN(pl->stats.Str, MAX_STAT)] - pl->carrying) / at->clone.weight < (sint32) n) {
                        n = ((sint32) weight_limit[MIN(pl->stats.Str, MAX_STAT)] - pl->carrying) / at->clone.weight;
                    }

                    tmp = get_object();
                    copy_object(&at->clone, tmp, 0);
                    tmp->nrof = n;
                    value -= tmp->nrof * tmp->value;
                    insert_ob_in_ob(tmp, pl);
                }
            }

            if (value / at->clone.value > 0) {
                tmp = get_object();
                copy_object(&at->clone, tmp, 0);
                tmp->nrof = (uint32) (value / at->clone.value);
                value -= tmp->nrof * tmp->value;
                tmp->x = pl->x;
                tmp->y = pl->y;
                insert_ob_in_map(tmp, pl->map, NULL, 0);
            }
        }
    }

    return value;
}
