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
 * Used by nearest_pow_two_exp() for a fast lookup.
 */
static const size_t exp_lookup[65] = {
    0, 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
};

TOOLKIT_API();

TOOLKIT_INIT_FUNC(math)
{
    SRANDOM(time(NULL));
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(math)
{
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Computes the integer square root.
 * @param n Number of which to compute the root.
 * @return Integer square root. */
unsigned long isqrt(unsigned long n)
{
    unsigned long op = n, res = 0, one;

    TOOLKIT_PROTECT();

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
    TOOLKIT_PROTECT();

    if (max - min + 1 < 1) {
        log_error("Calling rndm() with min=%d max=%d", min, max);
        return min;
    }

    if (max == 0) {
        return min;
    }

    return min + RANDOM() / (RAND_MAX / (max - min + 1) + 1);
}

/**
 * Calculates a chance of 1 in 'n'.
 * @param n Number.
 * @return 1 if the chance of 1/n was successful, 0 otherwise. */
int rndm_chance(uint32_t n)
{
    TOOLKIT_PROTECT();

    if (!n) {
        log_error("Calling rndm_chance() with n=0.");
        return 0;
    }

    return (uint32_t) RANDOM() < (RAND_MAX + 1U) / n;
}

/**
 * A Linked-List Memory Sort
 * by Philip J. Erdelsky <pje@efgh.com>
 * http://www.alumni.caltech.edu/~pje/
 * (Public Domain)
 *
 * The function sort_linked_list() will sort virtually any kind of singly-linked
 * list, using a comparison function supplied by the calling program. It has
 * several advantages over qsort().
 *
 * The function sorts only singly linked lists. If a list is doubly linked, the
 * backward pointers can be restored after the sort by a few lines of code.
 *
 * Each element of a linked list to be sorted must contain, as its first
 * members, one or more pointers. One of the pointers, which must be in the same
 * relative position in each element, is a pointer to the next element. This
 * pointer is <end_marker> (usually NULL) in the last element.
 *
 * The index is the position of this pointer in each element. It is 0 for the
 * first pointer, 1 for the second pointer, etc.
 *
 * Let n = compare(p, q, pointer) be a comparison function that compares two
 * elements p and q as follows:
 *
 * void *pointer; user-defined pointer passed to compare() by sort_linked_list()
 * int n;         result of comparing *p and *q
 *                     >0 if *p is to be after *q in sorted order
 *                     <0 if *p is to be before *q in sorted order
 *                      0 if the order of *p and *q is irrelevant
 *
 *
 * The fourth argument (pointer) is passed to compare() without change. It can
 * be an invaluable feature if two or more comparison methods share a
 * substantial amount of code and differ only in one or more parameter values.
 *
 * The last argument (pcount) is of type (unsigned long *). If it is not NULL,
 * then *pcount is set equal to the number of records in the list.
 *
 * It is permissible to sort an empty list. If first == end_marker, the returned
 * value will also be end_marker.
 */
void *sort_linked_list(void *p, unsigned index,
        int (*compare) (void *, void *, void *) , void *pointer,
        unsigned long *pcount, void *end_marker)
{
    unsigned base;
    unsigned long block_size;
    struct record {
        struct record *next[1];
        /* other members not directly accessed by this function */
    };
    struct tape {
        struct record *first, *last;
        unsigned long count;
    } tape[4];

    /* Distribute the records alternately to tape[0] and tape[1]. */
    tape[0].count = tape[1].count = 0L;
    tape[0].first = NULL;
    base = 0;

    while (p != end_marker) {
        struct record  *next = ((struct record *) p)->next[index];
        ((struct record *) p)->next[index] = tape[base].first;
        tape[base].first = ((struct record *) p);
        tape[base].count++;
        p = next;
        base ^= 1;
    }

    /* If the list is empty or contains only a single record, then */
    /* tape[1].count == 0L and this part is vacuous.               */
    for (base = 0, block_size = 1L; tape[base + 1].count != 0L;
            base ^= 2, block_size <<= 1) {
        int dest;
        struct tape *tape0, *tape1;

        tape0 = tape + base;
        tape1 = tape + base + 1;
        dest = base ^ 2;
        tape[dest].count = tape[dest + 1].count = 0;

        for (; tape0->count != 0; dest ^= 1) {
            unsigned long n0, n1;
            struct tape *output_tape = tape + dest;

            n0 = n1 = block_size;

            while (1) {
                struct record *chosen_record;
                struct tape *chosen_tape;

                if (n0 == 0 || tape0->count == 0) {
                    if (n1 == 0 || tape1->count == 0) {
                        break;
                    }

                    chosen_tape = tape1;
                    n1--;
                } else if (n1 == 0 || tape1->count == 0) {
                    chosen_tape = tape0;
                    n0--;
                } else if ((*compare) (tape0->first, tape1->first,
                        pointer) > 0) {
                    chosen_tape = tape1;
                    n1--;
                } else {
                    chosen_tape = tape0;
                    n0--;
                }

                chosen_tape->count--;
                chosen_record = chosen_tape->first;
                chosen_tape->first = chosen_record->next[index];

                if (output_tape->count == 0) {
                    output_tape->first = chosen_record;
                } else {
                    output_tape->last->next[index] = chosen_record;
                }

                output_tape->last = chosen_record;
                output_tape->count++;
            }
        }
    }

    if (tape[base].count > 1L) {
        tape[base].last->next[index] = end_marker;
    }

    if (pcount != NULL) {
        *pcount = tape[base].count;
    }

    return tape[base].first;
}

/**
 * Return the exponent exp needed to round n up to the nearest power of two, so
 * that (1 << exp) >= n and (1 << (exp - 1)) \< n
 */
size_t nearest_pow_two_exp(size_t n)
{
    size_t i;

    TOOLKIT_PROTECT();

    if (n <= 64) {
        return exp_lookup[n];
    }

    for (i = 7; (1U << i) < n; i++) {
    }

    return i;
}

/**
 * Determine whether the specified point X,Y is in an ellipse.
 *
 * @param x X of the point.
 * @param y Y of the point.
 * @param cx X center of the ellipse.
 * @param cy Y center of the ellipse.
 * @param dx X diameter of the ellipse.
 * @param dy Y diameter of the ellipse.
 * @param angle Angle of the ellipse.
 * @return True if the point is inside the ellipse, false otherwise.
 */
bool
math_point_in_ellipse (int    x,
                       int    y,
                       double cx,
                       double cy,
                       int    dx,
                       int    dy,
                       double angle)
{
    double sin_angle, cos_angle;
    sincos(angle, &sin_angle, &cos_angle);

    double a = pow(cos_angle * (x - cx) + sin_angle * (y - cy), 2.0);
    double b = pow(sin_angle * (x - cx) + cos_angle * (y - cy), 2.0);

    return a / (dx / 2.0 * dx / 2.0) + b / (dy / 2.0 * dy / 2.0) < 1.0;
}

/**
 * Determine whether the specified point X,Y is on the edge of an ellipse.
 *
 * @param x X of the point.
 * @param y Y of the point.
 * @param cx X center of the ellipse.
 * @param cy Y center of the ellipse.
 * @param dx X diameter of the ellipse.
 * @param dy Y diameter of the ellipse.
 * @param angle Angle of the ellipse.
 * @param[out] deg On success, will contain the angle the point is at in
 * relation to the center of the ellipse, in degrees (0-359), with up=0,
 * right=90, etc. Can be NULL. Undefined if the function returns false.
 * @return True if the point is on the edge of the ellipse, false otherwise.
 */
bool
math_point_edge_ellipse (int    x,
                         int    y,
                         double cx,
                         double cy,
                         int    dx,
                         int    dy,
                         double angle,
                         int   *deg)
{
    double sin_angle, cos_angle;
    sincos(angle, &sin_angle, &cos_angle);

    double a = pow(cos_angle * (x - cx) + sin_angle * (y - cy), 2.0);
    double b = pow(sin_angle * (x - cx) + cos_angle * (y - cy), 2.0);
    double r = a / (dx / 2.0 * dx / 2.0) + b / (dy / 2.0 * dy / 2.0);

    if (r >= 1.0 || r <= 0.9) {
        return false;
    }

    if (deg != NULL) {
        double rad = atan2(y - cy, x - cx);
        *deg = rad * (180.0 / M_PI) + 90.0;
        *deg = (*deg + 360) % 360;
    }

    return true;
}
