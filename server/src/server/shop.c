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
 * Functions dealing with shop handling, bargaining, etc.
 */

#include <global.h>
#include <toolkit_string.h>
#include <arch.h>

/**
 * Calculate the price of an item.
 * @param tmp Object we're querying the price of.
 * @param mode One of @ref COST_xxx.
 * @return The price of the item.
 */
int64_t shop_get_cost(object *op, int mode)
{
    HARD_ASSERT(op != NULL);

    SOFT_ASSERT_RC(op->arch != NULL, 0, "Object has no archetype: %s",
            object_get_str(op));

    uint32_t nrof = MAX(1, op->nrof);
    /* Money is always identified */
    if (op->type == MONEY) {
        return op->value * nrof;
    }

    int64_t val;

    if (QUERY_FLAG(op, FLAG_IDENTIFIED) || !need_identify(op)) {
        /* Handle identified items */
        if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)) {
            /* Cursed or damned items have no value at all. */
            return 0;
        } else {
            val = op->value * nrof;
        }
    } else {
        /* This area deals with objects that are not identified, but can be. */
        if (mode == COST_BUY) {
            log_error("Asking for buy value of unidentified object: %s",
                    object_get_str(op));
            val = op->arch->clone.value * nrof;
        } else {
            /* Trying to sell something, or get true value. */
            if (op->type == GEM || op->type == JEWEL || op->type == NUGGET ||
                    op->type == PEARL) {
                val = 3 * nrof;
            } else if (op->type == POTION) {
                val = 50 * nrof;
            } else {
                val = op->arch->clone.value * nrof;
            }
        }
    }

    if (op->type == WAND) {
        val += (val * op->level) * op->stats.food;
    } else if (op->type == ROD || op->type == POTION || op->type == SCROLL) {
        val += val * op->level;
    } else if (op->type == BOOK_SPELL) {
        if (op->stats.sp >= 0 && op->stats.sp < NROFREALSPELLS) {
            val += val * spells[op->stats.sp].at->clone.level;
            val += spells[op->stats.sp].at->clone.value;
        }
    }

    /* We are done if we only want get the real value. */
    if (mode == COST_TRUE) {
        return val;
    }

    double diff;
    /* Now adjust for sell or buy multiplier. */
    if (mode == COST_BUY) {
        diff = 1.0;
    } else {
        diff = 0.20;
    }

    val = (int64_t) (val * diff);
    if (val < 1 && op->value > 0) {
        val = 1;
    }

    return val;
}

/**
 * Find the coin type that is worth more than 'cost'. Starts at the 'cointype'
 * placement.
 * @param cost Value we're searching.
 * @param[in,out] cointype First coin type to search. Will contain the next
 * coin ID.
 * @return Coin archetype, NULL if none found.
 */
static archetype_t *shop_get_next_coin(int64_t cost, int *cointype)
{
    archetype_t *coin;

    do {
        if (coins[*cointype] == NULL) {
            return NULL;
        }

        coin = arch_find(coins[*cointype]);
        if (coin == NULL) {
            return NULL;
        }

        *cointype += 1;
    } while (coin->clone.value > cost);

    return coin;
}

/**
 * Converts a price to number of coins.
 * @param cost Value to transform to currency.
 * @return Static buffer containing the price. Will be overwritten with the next
 * call to this function.
 */
char *shop_get_cost_string(int64_t cost)
{
    static char buf[MAX_BUF];

    int cointype = 0;
    archetype_t *coin = shop_get_next_coin(cost, &cointype);
    if (coin == NULL) {
        return "nothing";
    }

    int64_t num = cost / coin->clone.value;
    cost -= num * coin->clone.value;

    snprintf(VS(buf), "%" PRId64 " %s%s%s", num,
            material_real[coin->clone.material_real].name,
            coin->clone.name, num == 1 ? "" : "s");

    archetype_t *next_coin = shop_get_next_coin(cost, &cointype);
    if (next_coin == NULL) {
        return buf;
    }

    do {
        coin = next_coin;
        num = cost / coin->clone.value;
        cost -= num * coin->clone.value;

        if (cost == 0) {
            next_coin = NULL;
        } else {
            next_coin = shop_get_next_coin(cost, &cointype);
        }

        if (next_coin != NULL) {
            /* There will be at least one more string to add to the list,
             * use a comma. */
            snprintfcat(VS(buf), ", ");
        } else {
            snprintfcat(VS(buf), " and ");
        }

        snprintfcat(VS(buf), "%" PRId64 " %s%s%s", num,
                material_real[coin->clone.material_real].name,
                coin->clone.name, num == 1 ? "" : "s");
    } while (next_coin != NULL);

    return buf;
}

/**
 * Query the cost of an item.
 *
 * This is really a wrapper for shop_get_cost_string() and shop_get_cost().
 * @param op Object we're querying the price of.
 * @param mode One of @ref COST_xxx.
 * @return The cost string.
 */
char *shop_get_cost_string_item(object *op, int mode)
{
    return shop_get_cost_string(shop_get_cost(op, mode));
}

/**
 * Finds out how much money the player is carrying, including what is in
 * containers and in bank.
 * @param op Item to get money for. Must be a player or a container.
 * @return Total money the player is carrying.
 */
int64_t shop_get_money(object *op)
{
    HARD_ASSERT(op != NULL);

    SOFT_ASSERT_RC(op->type == PLAYER || op->type == CONTAINER, 0,
            "Called with invalid object type: %s", object_get_str(op));

    int64_t total = 0;
    FOR_INV_PREPARE(op, tmp) {
        if (tmp->type == MONEY) {
            total += tmp->nrof * tmp->value;
        } else if (tmp->type == CONTAINER && (QUERY_FLAG(tmp, FLAG_APPLIED) ||
                tmp->race == NULL || strstr(tmp->race, "gold") != NULL)) {
            total += shop_get_money(tmp);
        } else if (tmp->arch->name == shstr_cons.player_info &&
                tmp->name == shstr_cons.BANK_GENERAL) {
            total += tmp->value;
        }
    } FOR_INV_FINISH();

    return total;
}

/**
 * Pays the specified amount, taking the proper amount of money from the
 * object's inventory.
 * @param obj Object to remove the money for.
 * @param to_pay Required amount.
 * @return Amount still left to pay.
 */
static int64_t shop_pay_inventory(object *obj, int64_t to_pay)
{
    int64_t remain = to_pay;

    object *coins_objects[NUM_COINS];
    for (int i = 0; i < NUM_COINS; i++) {
        coins_objects[i] = NULL;
    }

    object *bank_object = NULL;

    /* Remove all the money objects from the container, and store them in
     * the coin_objs pointers array. */
    FOR_INV_PREPARE(obj, tmp) {
        if (tmp->type == MONEY) {
            bool found = false;
            for (int i = 0; i < NUM_COINS; i++) {
                if (strcmp(coins[NUM_COINS - 1 - i], tmp->arch->name) != 0 ||
                        tmp->value != tmp->arch->clone.value) {
                    continue;
                }

                /* This should not happen, but if it does, just merge them. */
                if (coins_objects[i] != NULL) {
                    log_error("Two money entries of %s in object: %s",
                            coins[NUM_COINS - 1 - i],
                            object_get_str(obj));
                    coins_objects[i]->nrof += tmp->nrof;
                    object_remove(tmp, 0);
                    object_destroy(tmp);
                } else {
                    object_remove(tmp, 0);
                    coins_objects[i] = tmp;
                }

                found = true;
                break;
            }

            if (!found) {
                log_error("Did not find string match for %s", tmp->arch->name);
            }
        } else if (tmp->arch->name == shstr_cons.player_info &&
                tmp->name == shstr_cons.BANK_GENERAL) {
            bank_object = tmp;
        }
    } FOR_INV_FINISH();

    for (int i = 0; i < NUM_COINS; i++) {
        if (coins_objects[i] == NULL) {
            continue;
        }

        int64_t num_coins;
        if ((int64_t) coins_objects[i]->nrof * coins_objects[i]->value >
                remain) {
            num_coins = remain / coins_objects[i]->value;

            if (num_coins * coins_objects[i]->value < remain) {
                num_coins++;
            }
        } else {
            num_coins = coins_objects[i]->nrof;
        }

        if (num_coins > UINT32_MAX) {
            LOG(ERROR, "Money overflow value->nrof: number of coins > "
                    "UINT32_MAX (type coin %d)", i);
            num_coins = UINT32_MAX;
        }

        remain -= num_coins * coins_objects[i]->value;
        coins_objects[i]->nrof -= (uint32_t) num_coins;

        /* Now start making change.  Start at the coin value
         * below the one we just did, and work down to
         * the lowest value. */
        int count = i - 1;
        while (remain < 0 && count >= 0) {
            if (coins_objects[count] == NULL) {
                coins_objects[count] = arch_get(coins[NUM_COINS - 1 - count]);
                coins_objects[count]->nrof = 0;
            }

            num_coins = -remain / coins_objects[count]->value;
            coins_objects[count]->nrof += (uint32_t) num_coins;
            remain += num_coins * coins_objects[count]->value;
            count--;
        }
    }

    /* If there's still some remain, that means we could try to pay from
     * the bank. */
    if (bank_object != NULL && bank_object->value > 0 && remain > 0 &&
            bank_object->value >= remain) {
        bank_object->value -= remain;
        remain = 0;
    }

    for (int i = 0; i < NUM_COINS; i++) {
        if (coins_objects[i] == NULL) {
            continue;
        }

        insert_ob_in_ob(coins_objects[i], obj);
    }

    return remain;
}

/**
 * Recursively attempts to pay the specified amount of money.
 * @param op Who is paying.
 * @param to_pay Amount to pay.
 * @return Amount left to pay.
 */
static int64_t shop_pay_amount(object *op, int64_t to_pay)
{
    to_pay = shop_pay_inventory(op, to_pay);
    FOR_INV_PREPARE(op, tmp) {
        if (to_pay <= 0) {
            break;
        }

        if (tmp->type != CONTAINER || tmp->inv == NULL) {
            continue;
        }

        if (QUERY_FLAG(tmp, FLAG_APPLIED) || tmp->race == NULL ||
                strstr(tmp->race, "gold") != NULL) {
            to_pay = shop_pay_amount(tmp, to_pay);
        }
    } FOR_INV_FINISH();

    return to_pay;
}

/**
 * Pays the specified amount of money.
 * @param op Object paying.
 * @param to_pay Amount to pay.
 * @return False if not enough money, in which case nothing is removed, true
 * if money was removed.
 */
bool shop_pay(object *op, int64_t to_pay)
{
    if (to_pay == 0) {
        return true;
    }

    if (to_pay > shop_get_money(op)) {
        return false;
    }

    to_pay = shop_pay_amount(op, to_pay);
    SOFT_ASSERT_RC(to_pay == 0, true, "Still %" PRId64 " left to pay, op: %s",
            to_pay, object_get_str(op));

    return true;
}

/**
 * Attempts to pay for the specified item.
 * @param op Object buying.
 * @param item Item to buy.
 * @return Whether the object was purchased successfully (and money removed).
 */
bool shop_pay_item(object *op, object *item)
{
    return shop_pay(op, shop_get_cost(item, COST_BUY));
}

/**
 * Recursively pay for items in inventories. Used by shop_pay_items().
 * @param op Object buying the stuff.
 * @param where Where to look.
 * @return True if everything has been paid for, false otherwise.
 */
static bool shop_pay_items_rec(object *op, object *where)
{
    FOR_INV_PREPARE(where, tmp) {
        if (tmp->inv != NULL) {
            if (!shop_pay_items_rec(op, tmp)) {
                return false;
            }
        }

        if (!QUERY_FLAG(tmp, FLAG_UNPAID)) {
            continue;
        }

        if (!shop_pay_item(op, tmp)) {
            CLEAR_FLAG(tmp, FLAG_UNPAID);
            int64_t need = shop_get_cost(tmp, COST_BUY) - shop_get_money(op);
            draw_info_format(COLOR_WHITE, op, "You lack %s to buy %s.",
                    shop_get_cost_string(need), query_name(tmp, op));
            SET_FLAG(tmp, FLAG_UNPAID);
            return false;
        } else {
            CLEAR_FLAG(tmp, FLAG_UNPAID);
            CLEAR_FLAG(tmp, FLAG_STARTEQUIP);
            draw_info_format(COLOR_WHITE, op, "You paid %s for %s.",
                    shop_get_cost_string_item(tmp, COST_BUY),
                    query_name(tmp, op));

            /* If the object wasn't merged, send flags update. */
            if (object_merge(tmp) == tmp) {
                esrv_update_item(UPD_FLAGS, tmp);
            }
        }
    } FOR_INV_FINISH();

    return true;
}

/**
 * Descends inventories looking for unpaid items, and pays for them.
 * @param op Object buying the stuff.
 * @return True if everything has been paid for, false otherwise.
 */
bool shop_pay_items(object *op)
{
    return shop_pay_items_rec(op, op);
}

/**
 * Sell an item.
 * @param op Who is selling the item.
 * @param item The item to sell.
 */
void shop_sell_item(object *op, object *item)
{
    HARD_ASSERT(op != NULL);

    if (item->custom_name) {
        FREE_AND_CLEAR_HASH(item->custom_name);
    }

    int64_t value = shop_get_cost(item, COST_SELL);
    if (value == 0) {
        draw_info_format(COLOR_WHITE, op, "We're not interested in %s.",
                query_name(item, op));
    }

    shop_insert_coins(op, value);
    draw_info_format(COLOR_WHITE, op, "You receive %s for %s.",
            shop_get_cost_string(value), query_name(item, op));

    SET_FLAG(item, FLAG_UNPAID);
    /* Identify the item. Makes any unidentified item sold to unique shop appear
     * identified. */
    identify(item);
}

/**
 * Insert coins into an object.
 * @param op Object to receive the coins.
 * @param value Value of coins to insert (for example, 120 for 1 silver and 20
 * copper).
 */
void shop_insert_coins(object *op, int64_t value)
{
    for (int i = 0; coins[i] != NULL; i++) {
        archetype_t *at = arch_find(coins[i]);
        if (at == NULL) {
            log_error("Could not find archetype: %s", coins[i]);
            continue;
        }

        if (value / at->clone.value <= 0) {
            continue;
        }

        FOR_INV_PREPARE(op, tmp) {
            if (tmp->type != CONTAINER) {
                continue;
            }

            if (!QUERY_FLAG(tmp, FLAG_APPLIED)) {
                continue;
            }

            if (tmp->race == NULL || strstr(tmp->race, "gold") == NULL) {
                continue;
            }

            uint32_t nrof = (uint32_t) (value / at->clone.value);
            if (nrof == 0) {
                continue;
            }

            double weight = at->clone.weight * tmp->weapon_speed;
            if (tmp->weight_limit != 0 && tmp->carrying + weight >
                    tmp->weight_limit) {
                if (weight > 0.0 && tmp->weight_limit != 0 &&
                        (tmp->weight_limit - tmp->carrying) / weight < nrof) {
                    nrof = (tmp->weight_limit - tmp->carrying) / weight;
                }

                object *coin = get_object();
                copy_object(&at->clone, coin, 0);
                coin->nrof = nrof;
                value -= coin->nrof * coin->value;
                insert_ob_in_ob(coin, tmp);
            }
        } FOR_INV_FINISH();

        if (value / at->clone.value > 0) {
            uint32_t nrof = (uint32_t) (value / at->clone.value);
            uint32_t weight_max = weight_limit[MIN(op->stats.Str, MAX_STAT)];

            if (nrof > 0 && op->carrying + at->clone.weight <= weight_max) {
                if ((weight_max - op->carrying) / at->clone.weight < nrof) {
                    nrof = (weight_max - op->carrying) / at->clone.weight;
                }

                object *coin = get_object();
                copy_object(&at->clone, coin, 0);
                coin->nrof = nrof;
                value -= coin->nrof * coin->value;
                insert_ob_in_ob(coin, op);
            }
        }

        if (value / at->clone.value > 0) {
            object *coin = get_object();
            copy_object(&at->clone, coin, 0);
            coin->nrof = (uint32_t) (value / at->clone.value);
            value -= coin->nrof * coin->value;
            coin->x = op->x;
            coin->y = op->y;
            insert_ob_in_map(coin, op->map, NULL, 0);
        }
    }

    SOFT_ASSERT(value == 0, "Value is not zero: %" PRId64 ", object: %s",
            value, object_get_str(op));
}
