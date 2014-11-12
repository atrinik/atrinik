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
 * Math related functions.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Name of the API. */
#define API_NAME math

/**
 * If 1, the API has been initialized. */
static uint8 did_init = 0;

/**
 * Initialize the math API.
 * @internal */
void toolkit_math_init(void)
{

    TOOLKIT_INIT_FUNC_START(math)
    {
        SRANDOM(time(NULL));
    }
    TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the math API.
 * @internal */
void toolkit_math_deinit(void)
{

    TOOLKIT_DEINIT_FUNC_START(math)
    {
    }
    TOOLKIT_DEINIT_FUNC_END()
}

/**
 * Computes the integer square root.
 * @param n Number of which to compute the root.
 * @return Integer square root. */
unsigned long isqrt(unsigned long n)
{
    unsigned long op = n, res = 0, one;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    /* "one" starts at the highest power of four <= than the argument. */
    one = 1 << 30;

    while (one > op) {
        one >>= 2;
    }

    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            /* Faster than 2 * one. */
            res += one << 1;
        }

        res >>= 1;
        one >>= 2;
    }

    return res;
}

/**
 * Calculates a random number between min and max.
 *
 * It is suggested one uses this function rather than RANDOM()%, as it
 * would appear that a number of off-by-one-errors exist due to improper
 * use of %.
 *
 * This should also prevent SIGFPE.
 * @param min Starting range.
 * @param max Ending range.
 * @return The random number. */
int rndm(int min, int max)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (max < 1 || max - min + 1 < 1) {
        logger_print(LOG(BUG), "Calling rndm() with min=%d max=%d", min, max);
        return min;
    }

    return min + RANDOM() / (RAND_MAX / (max - min + 1) + 1);
}

/**
 * Calculates a chance of 1 in 'n'.
 * @param n Number.
 * @return 1 if the chance of 1/n was successful, 0 otherwise. */
int rndm_chance(uint32 n)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (!n) {
        logger_print(LOG(BUG), "Calling rndm_chance() with n=0.");
        return 0;
    }

    return (uint32) RANDOM() < (RAND_MAX + 1U) / n;
}
