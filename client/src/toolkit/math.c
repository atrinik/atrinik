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
 * @author Alex Tokar
 */

#include <toolkit.h>
#include <toolkit_string.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>

/**
 * @defgroup DRNG_xxx Intel DRNG support flags
 *
 * Flags that determine which DRNG instructions are available.
 *@{*/
#define DRNG_NO_SUPPORT	0x0 ///< DRNG is not supported.
#define DRNG_HAS_RDRAND	0x1 ///< RDRAND is available.
#define DRNG_HAS_RDSEED	0x2 ///< RDSEED is available.
/*@}*/

#ifndef __arm__
/**
 * When defined, Intel's DRNG is compiled.
 */
#define INTEL_DRNG
#endif

/**
 * Used by nearest_pow_two_exp() for a fast lookup.
 */
static const size_t exp_lookup[65] = {
    0, 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
};

#ifdef INTEL_DRNG
/**
 * Combination of @ref DRNG_xxx.
 */
static int drng_features;
#endif

#ifndef __arm__
/* Prototypes */
static uint64_t
rdtsc(void);
#endif

#ifdef INTEL_DRNG
static int
get_drng_features(void);
static bool
rdseed64_step(uint64_t *seed);
#endif

TOOLKIT_API(DEPENDS(logger));

TOOLKIT_INIT_FUNC(math)
{
    uint64_t seed = time(NULL);

#ifdef INTEL_DRNG
    drng_features = get_drng_features();
    if (drng_features & DRNG_HAS_RDSEED) {
        LOG(DEVEL, "CPU supports RDSEED opcode, attempting to generate a seed");
        uint64_t new_seed;
        /* Give it ten tries... */
        for (int i = 0; i < 10; i++) {
            if (rdseed64_step(&new_seed)) {
                seed = new_seed;
                LOG(DEVEL, "RDSEED generated seed: %" PRIu64, seed);
                break;
            }
        }
    } else
#endif
#ifndef __arm__
    {
        LOG(DEVEL, "CPU supports RDTSC opcode, attempting to generate a seed");
        seed = rdtsc();
        LOG(DEVEL, "RDTSC generated seed: %" PRIu64, seed);
    }
#endif

#ifdef INTEL_DRNG
    if (drng_features & DRNG_HAS_RDRAND) {
        LOG(DEVEL, "CPU supports RDRAND opcode, will use DRNG for RNG");
    }
#endif

    SRANDOM(seed);
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(math)
{
}
TOOLKIT_DEINIT_FUNC_FINISH

#ifndef __arm__
/**
 * Acquire the processor time stamp using the rdtsc opcode.
 *
 * @return
 * Processor time stamp.
 */
static uint64_t
rdtsc (void)
{
    unsigned int lo, hi;
    asm volatile ("rdtsc"
                  : "=a" (lo), "=d" (hi));
    return ((uint64_t) hi << 32) | lo;
}
#endif

#ifdef INTEL_DRNG
/**
 * CPU ID information structure.
 */
typedef struct cpuid {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
} cpuid_t;

/**
 * Executes the cpuid opcode, acquiring information about the current CPU
 * into the specified cpuid_t structure.
 *
 * @param info
 * Where to store the information.
 * @param leaf
 * Leaf information (the EAX register).
 * @param subleaf
 * Sub-leaf information (the ECX register).
 */
static void
cpuid (cpuid_t *info, unsigned int leaf, unsigned int subleaf)
{
    HARD_ASSERT(info != NULL);
    asm volatile ("cpuid"
                  : "=a" (info->eax),
                    "=b" (info->ebx),
                    "=c" (info->ecx),
                    "=d" (info->edx)
                  : "a" (leaf),
                    "c" (subleaf));
}

/**
 * Determine if the system is running under an Intel CPU.
 *
 * @return
 * Whether the system is running under an Intel CPU.
 */
static bool
is_intel_cpu (void)
{
#ifndef __arm__
    cpuid_t info;
    cpuid(&info, 0, 0);

    if (memcmp((char *) &info.ebx, "Genu", 4) == 0 &&
        memcmp((char *) &info.edx, "ineI", 4) == 0 &&
        memcmp((char *) &info.ecx, "ntel", 4) == 0) {
        return true;
    }
#endif

    return false;
}

/**
 * Acquire the DRNG features supported by the CPU.
 *
 * @return
 * A combination of @ref DRNG_xxx "DRNG flags".
 */
static int
get_drng_features (void)
{
    if (!is_intel_cpu()) {
        /* Not an Intel CPU; no support for DRNG. */
        return DRNG_NO_SUPPORT;
    }

    int features = DRNG_NO_SUPPORT;

    cpuid_t info;
    /* Get the feature bits leaf */
    cpuid(&info, 1, 0);

    /* RDRAND instruction is bit #30 in the ECX register */
    if (BIT_QUERY(info.ecx, 30)) {
        features |= DRNG_HAS_RDRAND;
    }

    /* Get the extended feature flags leaf */
    cpuid(&info, 7, 0);

    /* RDSEED instruction is bit #18 in the EBX register */
    if (BIT_QUERY(info.ebx, 18)) {
        features |= DRNG_HAS_RDSEED;
    }

    return features;
}

/**
 * Seeds the Intel DRNG number generator.
 *
 * @param seed
 * The seed to seed with.
 * @return
 * True on success, false on failure.
 */
static bool
rdseed64_step (uint64_t *seed)
{
    HARD_ASSERT(seed != NULL);

    unsigned char ok;
    asm volatile ("rdseed %0; setc %1"
                  : "=r" (*seed), "=qm" (ok));
    return !!ok;
}

/**
 * Generates a random number using the Intel rdrand opcode.
 *
 * @param number
 * Will contain the random number on success.
 * @return
 * True on success, false on failure.
 */
static bool
rdrand64_step (unsigned long long int *number)
{
    HARD_ASSERT(number != NULL);

#if !defined(__x86_64__)
    unsigned char ok;
    asm volatile ("rdrand %0; setc %1"
                  : "=r" (*number), "=qm" (ok));
    return !!ok;
#else
    unsigned long long int i;
    int ok;

    asm volatile ("rdrand %%rax; \
                   mov $1,%%edx; \
                   cmovae %%rax,%%rdx; \
                   mov %%edx,%1; \
                   mov %%rax, %0;"
                  : "=r" (i), "=r" (ok)
                  :: "%rax", "%rdx");
    *number = i;
    return !!ok;
#endif
}

#endif

/**
 * Computes the integer square root.
 *
 * @param n
 * Number of which to compute the root.
 * @return
 * Integer square root.
 */
unsigned long
isqrt (unsigned long n)
{
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
 * It is suggested one uses this function rather than RANDOM()%, as it
 * would appear that a number of off-by-one-errors exist due to improper
 * use of %.
 *
 * This should also prevent SIGFPE.
 *
 * @param min
 * Starting range.
 * @param max
 * Ending range.
 * @return
 * The random number.
 */
int
rndm (int min, int max)
{
    TOOLKIT_PROTECT();

    if (max - min + 1 < 1) {
        log_error("Calling rndm() with min=%d max=%d", min, max);
        return min;
    }

    if (max == 0) {
        return min;
    }

#ifdef INTEL_DRNG
    if (drng_features & DRNG_HAS_RDRAND) {
        unsigned long long int i;
        if (rdrand64_step(&i)) {
            return min + i % (max - min + 1);
        }
    }
#endif

    return min + RANDOM() / (RAND_MAX / (max - min + 1) + 1);
}

/**
 * Calculates a chance of 1 in 'n'.
 *
 * @param n
 * Number.
 * @return
 * 1 if the chance of 1/n was successful, 0 otherwise.
 */
int
rndm_chance (uint32_t n)
{
    TOOLKIT_PROTECT();

    if (n == 0) {
        log_error("Calling rndm_chance() with n=0.");
        return 0;
    }

#ifdef INTEL_DRNG
    if (drng_features & DRNG_HAS_RDRAND) {
        unsigned long long int i;
        if (rdrand64_step(&i)) {
            return (i % n) == 0;
        }
    }
#endif

    return (uint32_t) RANDOM() < (RAND_MAX + 1U) / n;
}

/**
 * Generate a random 64-bit unsigned number.
 *
 * @return
 * 64-bit unsigned number.
 */
uint64_t
rndm_u64 (void)
{
#ifdef INTEL_DRNG
    if (drng_features & DRNG_HAS_RDRAND) {
        unsigned long long int i;
        if (rdrand64_step(&i)) {
            return i;
        }
    }
#endif

    union {
        uint64_t u64;
        uint8_t  u8[64 / CHAR_BIT];
    } num;

    for (size_t i = 0; i < arraysize(num.u8); i++) {
        num.u8[i] = rndm(0, UINT8_MAX);
    }

    return num.u64;
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
void *
sort_linked_list (void          *p,
                  unsigned      index,
                  int          (*compare)(void *, void *, void *),
                  void          *pointer,
                  unsigned long *pcount,
                  void          *end_marker)
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
                } else if ((*compare)(tape0->first, tape1->first,
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
size_t
nearest_pow_two_exp (size_t n)
{
    TOOLKIT_PROTECT();

    if (n <= 64) {
        return exp_lookup[n];
    }

    size_t i;
    for (i = 7; (1U << i) < n; i++) {
    }

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
bool
math_base64_decode (const char     *str,
                    unsigned char **buf,
                    size_t         *buf_len)
{
    HARD_ASSERT(str != NULL);
    HARD_ASSERT(buf != NULL);
    HARD_ASSERT(buf_len != NULL);

    char *cp = estrdup(str);
    size_t len = strlen(cp);

    *buf_len = ((len * 3) + 3) / 4;
    *buf = emalloc(*buf_len);

    BIO *bio = BIO_new_mem_buf(cp, len);
    if (bio == NULL) {
        LOG(ERROR, "BIO_new_mem_buf() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
        goto error;
    }

    BIO *bio_base64 = BIO_new(BIO_f_base64());
    if (bio_base64 == NULL) {
        LOG(ERROR, "BIO_new() failed: %s",
            ERR_error_string(ERR_get_error(), NULL));
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
