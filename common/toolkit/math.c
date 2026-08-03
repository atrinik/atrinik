/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * @author Zoey Rose
 */

#include "math.h"
#include "string.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

/**
 * Used by nearest_pow_two_exp() for a fast lookup.
 */
static const size_t exp_lookup[65] = {0, 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
                                      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6,
                                      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6};

/** Default gameplay random number stream. */
static rng_state_t gameplay_rng;
/** Serializes access to the default gameplay stream. */
static pthread_mutex_t gameplay_rng_mutex;

TOOLKIT_API(DEPENDS(logger));

TOOLKIT_INIT_FUNC(math) {
    uint64_t seed;

    if (RAND_bytes((unsigned char *)&seed, sizeof(seed)) != 1) {
        LOG(ERROR,
            "RAND_bytes() failed while seeding gameplay RNG: %s; falling back to wall-clock time",
            ERR_error_string(ERR_get_error(), NULL));
        seed = (uint64_t)time(NULL);
    }

    rng_seed(&gameplay_rng, seed);
    pthread_mutex_init(&gameplay_rng_mutex, NULL);
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(math) {
    pthread_mutex_destroy(&gameplay_rng_mutex);
}
TOOLKIT_DEINIT_FUNC_FINISH

/**
 * Advance a PCG-XSH-RR stream and return 32 random bits.
 *
 * @param rng
 * Random number stream.
 * @return
 * Random value.
 */
static uint32_t rng_u32(rng_state_t *rng) {
    uint64_t old_state = rng->state;
    uint32_t xor_shifted = (uint32_t)(((old_state >> 18U) ^ old_state) >> 27U);
    uint32_t rotation = (uint32_t)(old_state >> 59U);

    rng->state = old_state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    return (xor_shifted >> rotation) | (xor_shifted << ((-rotation) & 31U));
}

/**
 * Seed a deterministic random number stream.
 *
 * @param rng
 * Random number stream.
 * @param seed
 * Stream seed. Equal seeds produce equal sequences.
 */
void rng_seed(rng_state_t *rng, uint64_t seed) {
    HARD_ASSERT(rng != NULL);

    rng->state = 0;
    rng_u32(rng);
    rng->state += seed;
    rng_u32(rng);
}

/**
 * Generate 64 random bits from a deterministic stream.
 *
 * @param rng
 * Random number stream.
 * @return
 * Random value.
 */
uint64_t rng_u64(rng_state_t *rng) {
    HARD_ASSERT(rng != NULL);

    uint64_t high = rng_u32(rng);
    uint64_t low = rng_u32(rng);
    return (high << 32U) | low;
}

/**
 * Generate an unbiased random integer in an inclusive range.
 *
 * @param rng
 * Random number stream.
 * @param min
 * Minimum result.
 * @param max
 * Maximum result.
 * @return
 * Random value, or min if the range is invalid.
 */
int rng_range(rng_state_t *rng, int min, int max) {
    HARD_ASSERT(rng != NULL);

    if (max < min) {
        log_error("Calling rng_range() with min=%d max=%d", min, max);
        return min;
    }

    uint64_t span = (uint64_t)((int64_t)max - (int64_t)min) + 1U;
    uint64_t threshold = -span % span;
    uint64_t value;

    do {
        value = rng_u64(rng);
    } while (value < threshold);

    return (int)((int64_t)min + (int64_t)(value % span));
}

/**
 * Calculate a chance of one in n using a deterministic stream.
 *
 * @param rng
 * Random number stream.
 * @param n
 * Chance denominator.
 * @return
 * 1 on success, 0 otherwise or if n is zero.
 */
int rng_chance(rng_state_t *rng, uint32_t n) {
    HARD_ASSERT(rng != NULL);

    if (n == 0) {
        log_error("Calling rng_chance() with n=0.");
        return 0;
    }

    uint64_t span = n;
    uint64_t threshold = -span % span;
    uint64_t value;

    do {
        value = rng_u64(rng);
    } while (value < threshold);

    return value % span == 0;
}

/**
 * Generate a deterministic real number in the half-open range [0, 1).
 *
 * @param rng
 * Random number stream.
 * @return
 * Random value.
 */
double rng_real(rng_state_t *rng) {
    return (double)(rng_u64(rng) >> 11U) * 0x1.0p-53;
}

/**
 * Computes the integer square root.
 *
 * @param n
 * Number of which to compute the root.
 * @return
 * Integer square root.
 */
unsigned long isqrt(unsigned long n) {
    TOOLKIT_PROTECT();

    /* "one" starts at the highest power of four <= than the argument. */
    unsigned long one = 1 << 30;

    unsigned long op = n;
    while (one > op) {
        one >>= 2;
    }

    unsigned long res = 0;
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
 * This function uses rejection sampling so every value in the requested
 * range has the same probability.
 *
 * @param min
 * Starting range.
 * @param max
 * Ending range.
 * @return
 * The random number.
 */
int rndm(int min, int max) {
    TOOLKIT_PROTECT();

    pthread_mutex_lock(&gameplay_rng_mutex);
    int result = rng_range(&gameplay_rng, min, max);
    pthread_mutex_unlock(&gameplay_rng_mutex);
    return result;
}

/**
 * Calculates a chance of 1 in 'n'.
 *
 * @param n
 * Number.
 * @return
 * 1 if the chance of 1/n was successful, 0 otherwise.
 */
int rndm_chance(uint32_t n) {
    TOOLKIT_PROTECT();

    pthread_mutex_lock(&gameplay_rng_mutex);
    int result = rng_chance(&gameplay_rng, n);
    pthread_mutex_unlock(&gameplay_rng_mutex);
    return result;
}

/**
 * Generate a random 64-bit unsigned number.
 *
 * @return
 * 64-bit unsigned number.
 */
uint64_t rndm_u64(void) {
    TOOLKIT_PROTECT();

    pthread_mutex_lock(&gameplay_rng_mutex);
    uint64_t result = rng_u64(&gameplay_rng);
    pthread_mutex_unlock(&gameplay_rng_mutex);
    return result;
}

/**
 * Generate a random real number in the half-open range [0, 1).
 *
 * @return
 * Random value.
 */
double rndm_real(void) {
    TOOLKIT_PROTECT();

    pthread_mutex_lock(&gameplay_rng_mutex);
    double result = rng_real(&gameplay_rng);
    pthread_mutex_unlock(&gameplay_rng_mutex);
    return result;
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
void *sort_linked_list(void *p,
                       unsigned index,
                       int (*compare)(void *, void *, void *),
                       void *pointer,
                       unsigned long *pcount,
                       void *end_marker) {
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
        struct record *next = ((struct record *)p)->next[index];
        ((struct record *)p)->next[index] = tape[base].first;
        tape[base].first = ((struct record *)p);
        tape[base].count++;
        p = next;
        base ^= 1;
    }

    /* If the list is empty or contains only a single record, then */
    /* tape[1].count == 0L and this part is vacuous.               */
    for (base = 0, block_size = 1L; tape[base + 1].count != 0L; base ^= 2, block_size <<= 1) {
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
                } else if ((*compare)(tape0->first, tape1->first, pointer) > 0) {
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
size_t nearest_pow_two_exp(size_t n) {
    TOOLKIT_PROTECT();

    if (n <= 64) {
        return exp_lookup[n];
    }

    size_t i;
    for (i = 7; (1U << i) < n; i++) {}

    return i;
}

/**
 * Determine whether the specified point X,Y is in an ellipse.
 *
 * @param x
 * X of the point.
 * @param y
 * Y of the point.
 * @param cx
 * X center of the ellipse.
 * @param cy
 * Y center of the ellipse.
 * @param dx
 * X diameter of the ellipse.
 * @param dy
 * Y diameter of the ellipse.
 * @param angle
 * Angle of the ellipse.
 * @return
 * True if the point is inside the ellipse, false otherwise.
 */
bool math_point_in_ellipse(int x, int y, double cx, double cy, int dx, int dy, double angle) {
    double sin_angle, cos_angle;
    sincos(angle, &sin_angle, &cos_angle);

    double a = pow(cos_angle * (x - cx) + sin_angle * (y - cy), 2.0);
    double b = pow(sin_angle * (x - cx) + cos_angle * (y - cy), 2.0);

    return a / (dx / 2.0 * dx / 2.0) + b / (dy / 2.0 * dy / 2.0) < 1.0;
}

/**
 * Determine whether the specified point X,Y is on the edge of an ellipse.
 *
 * @param x
 * X of the point.
 * @param y
 * Y of the point.
 * @param cx
 * X center of the ellipse.
 * @param cy
 * Y center of the ellipse.
 * @param dx
 * X diameter of the ellipse.
 * @param dy
 * Y diameter of the ellipse.
 * @param angle
 * Angle of the ellipse.
 * @param[out] deg On success, will contain the angle the point is at in
 * relation to the center of the ellipse, in degrees (0-359), with up=0,
 * right=90, etc. Can be NULL. Undefined if the function returns false.
 * @return
 * True if the point is on the edge of the ellipse, false otherwise.
 */
bool math_point_edge_ellipse(int x,
                             int y,
                             double cx,
                             double cy,
                             int dx,
                             int dy,
                             double angle,
                             int *deg) {
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

/**
 * Decode the specified BASE64 encoded buffer.
 *
 * @param str
 * What to decode.
 * @param[out] buf
 * On success, will contain a pointer to the decoded data. Must be freed.
 * @param[out] buf_len
 * Length of the decoded data.
 * @return
 * True on success, false on failure.
 * @todo
 * This should really go in a new API.
 */
bool math_base64_decode(const char *str, unsigned char **buf, size_t *buf_len) {
    HARD_ASSERT(str != NULL);
    HARD_ASSERT(buf != NULL);
    HARD_ASSERT(buf_len != NULL);

    char *cp = estrdup(str);
    size_t len = strlen(cp);

    *buf_len = ((len * 3) + 3) / 4;
    *buf = emalloc(*buf_len);

    BIO *bio = BIO_new_mem_buf(cp, len);
    if (bio == NULL) {
        LOG(ERROR, "BIO_new_mem_buf() failed: %s", ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    BIO *bio_base64 = BIO_new(BIO_f_base64());
    if (bio_base64 == NULL) {
        LOG(ERROR, "BIO_new() failed: %s", ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    bio = BIO_push(bio_base64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    int num_read = BIO_read(bio, *buf, len);
    if (num_read <= 0) {
        goto error;
    }

    *buf_len = num_read;

    bool ret = true;
    goto out;

error:
    ret = false;

    if (*buf != NULL) {
        efree(*buf);
    }

out:
    BIO_free_all(bio);
    efree(cp);

    return ret;
}
