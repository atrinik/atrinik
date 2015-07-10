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
 * Bank related code.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <arch.h>

/**
 * @defgroup BANK_STRING_xxx Bank info string modes
 * Modes used for #bank_info_t and bank_parse_string().
 *@{*/
/** Invalid string (did not include any valid amount). */
#define BANK_STRING_NONE 0
/** Got a valid amount of money from string. */
#define BANK_STRING_AMOUNT 1
/** The string was "all". */
#define BANK_STRING_ALL -1
/*@}*/

/**
 * Used for depositing/withdrawing coins from the bank, and using string
 * to get information about how many coins to deposit/withdraw.
 */
typedef struct bank_info {
    /** One of @ref BANK_STRING_xxx. */
    int mode;

    /** Number of amber coins. */
    uint32_t amber;

    /** Number of mithril coins. */
    uint32_t mithril;

    /** Number of jade coins. */
    uint32_t jade;

    /** Number of gold coins. */
    uint32_t gold;

    /** Number of silver coins. */
    uint32_t silver;

    /** Number of copper coins. */
    uint32_t copper;
} bank_info_t;

/**
 * Parse a string into the bank into structure.
 * @param str Text to get money from.
 * @param info Bank info structure.
 */
static void bank_parse_string(const char *str, bank_info_t *info)
{
    memset(info, 0, sizeof(*info));

    while (isspace(*str)) {
        str++;
    }

    /* Easy, special case: all money */
    if (strncasecmp(str, "all", 3) == 0) {
        info->mode = BANK_STRING_ALL;
        return;
    }

    info->mode = BANK_STRING_NONE;

    char word[MAX_BUF];
    size_t pos = 0;
    while (string_get_word(str, &pos, ' ', VS(word), 0)) {
        if (!string_isdigit(word)) {
            continue;
        }

        unsigned long value = strtoul(word, NULL, 10);

        if (!string_get_word(str, &pos, ' ', VS(word), 0)) {
            continue;
        }

        if (strncasecmp("amber", word, 5) == 0) {
            info->mode = BANK_STRING_AMOUNT;
            info->amber += value;
        } else if (strncasecmp("mithril", word, 7) == 0) {
            info->mode = BANK_STRING_AMOUNT;
            info->mithril += value;
        } else if (strncasecmp("jade", word, 4) == 0) {
            info->mode = BANK_STRING_AMOUNT;
            info->jade += value;
        } else if (strncasecmp("gold", word, 4) == 0) {
            info->mode = BANK_STRING_AMOUNT;
            info->gold += value;
        } else if (strncasecmp("silver", word, 6) == 0) {
            info->mode = BANK_STRING_AMOUNT;
            info->silver += value;
        } else if (strncasecmp("copper", word, 6) == 0) {
            info->mode = BANK_STRING_AMOUNT;
            info->copper += value;
        }
    }
}

/**
 * Get number of specific coins in the object's inventory.
 * @param op Object to search in.
 * @param at Archetype the coins must match.
 * @return Number of coins in the object's inventory.
 */
static uint32_t bank_get_coins_num(object *op, archetype_t *at)
{
    uint32_t num = 0;
    FOR_INV_PREPARE(op, tmp) {
        if (tmp->type == MONEY && tmp->arch == at) {
            num += tmp->nrof;
        } else if (tmp->type == CONTAINER && (tmp->race == NULL ||
                strstr(tmp->race, "gold") != NULL)) {
            num += bank_get_coins_num(tmp, at);
        }
    } FOR_INV_FINISH();

    return num;
}

/**
 * Remove money by the specified coin type.
 * @param op Object we're removing from.
 * @param at Archetype the coins must match.
 * @param nrof Amount of money to remove. Has no effect if 'at' is NULL.
 * @return Removed amount.
 */
static int64_t bank_remove_coins(object *op, archetype_t *at, uint32_t nrof)
{
    int64_t amount = 0;

    FOR_INV_PREPARE(op, tmp) {
        if (nrof == 0 && at != NULL) {
            return amount;
        }

        if (tmp->type == MONEY && (at == NULL || tmp->arch == at)) {
            if (at == NULL || tmp->nrof <= nrof) {
                if (at != NULL) {
                    nrof -= tmp->nrof;
                }

                amount += tmp->nrof * tmp->value;
                object_remove(tmp, 0);
                object_destroy(tmp);
            } else {
                tmp->nrof -= nrof;
                amount += nrof * tmp->value;
                nrof = 0;
            }
        } else if (tmp->type == CONTAINER && (tmp->race == NULL ||
                strstr(tmp->race, "gold") != NULL)) {
            amount += bank_remove_coins(tmp, at, nrof);
        }
    } FOR_INV_FINISH();

    return amount;
}

/**
 * Insert coins into the object.
 * @param op Object.
 * @param at Money arch to insert.
 * @param nrof Number of coins.
 */
static void bank_insert_coins(object *op, archetype_t *at, uint32_t nrof)
{
    object *tmp = get_object();
    copy_object(&at->clone, tmp, 0);
    tmp->nrof = nrof;
    insert_ob_in_ob(tmp, op);
}

/**
 * Find bank player info object in player's inventory.
 * @param op Where to look for the player info object.
 * @return The player info object if found, NULL otherwise.
 */
static object *bank_find_info(object *op)
{
    FOR_INV_PREPARE(op, tmp) {
        if (tmp->arch->name == shstr_cons.player_info &&
                tmp->name == shstr_cons.BANK_GENERAL) {
            return tmp;
        }
    } FOR_INV_FINISH();

    return NULL;
}

/**
 * Create a new bank player info object and insert it to 'op'.
 * @param op Player.
 * @return The created player info object. */
static object *bank_create_info(object *op)
{
    object *bank = arch_get(shstr_cons.player_info);

    FREE_AND_COPY_HASH(bank->name, shstr_cons.BANK_GENERAL);
    return object_insert_into(bank, op, 0);
}

/**
 * Convenience function to either find a bank player info object and if
 * not found, create a new one.
 * @param op Player object.
 * @return The bank player info object. Never NULL.
 */
static object *bank_get_info(object *op)
{
    object *bank = bank_find_info(op);
    if (bank == NULL) {
        bank = bank_create_info(op);
    }
    return bank;
}

/**
 * Query how much money player has stored in bank.
 * @param op Player to query for.
 * @return The money stored.
 */
int64_t bank_get_balance(object *op)
{
    HARD_ASSERT(op != NULL);

    object *bank = bank_find_info(op);
    if (bank == NULL) {
        return 0;
    }

    return bank->value;
}

/**
 * Deposit money to player's bank object.
 * @param op Player.
 * @param text What was said to trigger this.
 * @param[out] value Will contain the deposited amount.
 * @return One of @ref BANK_xxx.
 */
int bank_deposit(object *op, const char *text, int64_t *value)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(text != NULL);
    HARD_ASSERT(value != NULL);

    bank_info_t info;
    bank_parse_string(text, &info);
    *value = 0;

    if (info.mode == BANK_STRING_NONE) {
        return BANK_SYNTAX_ERROR;
    } else if (info.mode == BANK_STRING_ALL) {
        *value = bank_remove_coins(op, NULL, 0);
        object *bank = bank_get_info(op);
        bank->value += *value;
    } else {
        if (info.amber != 0) {
            if (bank_get_coins_num(op, coins_arch[0]) < info.amber) {
                return BANK_DEPOSIT_AMBER;
            }
        }

        if (info.mithril != 0) {
            if (bank_get_coins_num(op, coins_arch[1]) < info.mithril) {
                return BANK_DEPOSIT_MITHRIL;
            }
        }

        if (info.jade != 0) {
            if (bank_get_coins_num(op, coins_arch[2]) < info.jade) {
                return BANK_DEPOSIT_JADE;
            }
        }

        if (info.gold != 0) {
            if (bank_get_coins_num(op, coins_arch[3]) < info.gold) {
                return BANK_DEPOSIT_GOLD;
            }
        }

        if (info.silver != 0) {
            if (bank_get_coins_num(op, coins_arch[4]) < info.silver) {
                return BANK_DEPOSIT_SILVER;
            }
        }

        if (info.copper != 0) {
            if (bank_get_coins_num(op, coins_arch[5]) < info.copper) {
                return BANK_DEPOSIT_COPPER;
            }
        }

        if (info.amber != 0) {
            bank_remove_coins(op, coins_arch[0], info.amber);
        }

        if (info.mithril != 0) {
            bank_remove_coins(op, coins_arch[1], info.mithril);
        }

        if (info.jade != 0) {
            bank_remove_coins(op, coins_arch[2], info.jade);
        }

        if (info.gold != 0) {
            bank_remove_coins(op, coins_arch[3], info.gold);
        }

        if (info.silver != 0) {
            bank_remove_coins(op, coins_arch[4], info.silver);
        }

        if (info.copper != 0) {
            bank_remove_coins(op, coins_arch[5], info.copper);
        }

        *value = info.amber * coins_arch[0]->clone.value +
                info.mithril * coins_arch[1]->clone.value +
                info.jade * coins_arch[2]->clone.value +
                info.gold * coins_arch[3]->clone.value +
                info.silver * coins_arch[4]->clone.value +
                info.copper * coins_arch[5]->clone.value;
        object *bank = bank_get_info(op);
        bank->value += *value;
    }

    return BANK_SUCCESS;
}

/**
 * Withdraw money player previously stored in bank object.
 * @param op Player.
 * @param bank Bank object in player's inventory.
 * @param text What was said to trigger this.
 * @param[out] value Will contain the withdrawn amount.
 * @return One of @ref BANK_xxx.
 */
int bank_withdraw(object *op, const char *text, int64_t *value)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(text != NULL);
    HARD_ASSERT(value != NULL);

    bank_info_t info;
    bank_parse_string(text, &info);

    object *bank = bank_find_info(op);
    *value = 0;

    if (bank == NULL || bank->value == 0) {
        return BANK_WITHDRAW_MISSING;
    }

    if (info.mode == BANK_STRING_NONE) {
        return BANK_SYNTAX_ERROR;
    } else if (info.mode == BANK_STRING_ALL) {
        *value = bank->value;
        bank->value = 0;
        shop_insert_coins(op, *value);
    } else {
        if (info.amber > 100000 || info.mithril > 100000 ||
                info.jade > 100000 || info.gold > 100000 ||
                info.silver > 1000000 || info.copper > 1000000) {
            return BANK_WITHDRAW_HIGH;
        }

        int64_t big_value = info.amber * coins_arch[0]->clone.value +
                info.mithril * coins_arch[1]->clone.value +
                info.jade * coins_arch[2]->clone.value +
                info.gold * coins_arch[3]->clone.value +
                info.silver * coins_arch[4]->clone.value +
                info.copper * coins_arch[5]->clone.value;

        if (big_value > bank->value) {
            return BANK_WITHDRAW_MISSING;
        }

        if (!player_can_carry(op, info.amber * coins_arch[0]->clone.weight +
                info.mithril * coins_arch[1]->clone.weight +
                info.jade * coins_arch[2]->clone.weight +
                info.gold * coins_arch[3]->clone.weight +
                info.silver * coins_arch[4]->clone.weight +
                info.copper * coins_arch[5]->clone.weight)) {
            return BANK_WITHDRAW_OVERWEIGHT;
        }

        if (info.amber != 0) {
            bank_insert_coins(op, coins_arch[0], info.amber);
        }

        if (info.mithril != 0) {
            bank_insert_coins(op, coins_arch[1], info.mithril);
        }

        if (info.jade != 0) {
            bank_insert_coins(op, coins_arch[2], info.jade);
        }

        if (info.gold != 0) {
            bank_insert_coins(op, coins_arch[3], info.gold);
        }

        if (info.silver != 0) {
            bank_insert_coins(op, coins_arch[4], info.silver);
        }

        if (info.copper != 0) {
            bank_insert_coins(op, coins_arch[5], info.copper);
        }

        *value = big_value;
        bank->value -= big_value;
    }

    return BANK_SUCCESS;
}
