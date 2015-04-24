/*-*- mode:C; -*- */
/*
 * Check: a unit test framework for C
 * Copyright (C) 2001, 2002, Arien Malec
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if CHECK_MAJOR_VERSION == 0 && CHECK_MINOR_VERSION == 9 && CHECK_MICRO_VERSION <= 9

#undef ck_assert
#define ck_assert(C) ck_assert_msg(C, NULL)
#undef ck_assert_msg
#define ck_assert_msg(expr, ...) \
  _ck_assert_msg(expr, __FILE__, __LINE__,\
    "Assertion '"#expr"' failed" , ## __VA_ARGS__, NULL)

/* Always fail */
#undef ck_abort
#define ck_abort() ck_abort_msg(NULL)
#undef ck_abort_msg
#define ck_abort_msg(...) _ck_assert_msg(0, __FILE__, __LINE__, "Failed" , ## __VA_ARGS__, NULL)

/* Signed and unsigned integer comparsion macros with improved output compared to ck_assert(). */
/* OP may be any comparion operator. */
#undef _ck_assert_int
#define _ck_assert_int(X, OP, Y) do { \
  int64_t _ck_x = (X); \
  int64_t _ck_y = (Y); \
  ck_assert_msg(_ck_x OP _ck_y, "Assertion '"#X#OP#Y"' failed: "#X"==%jd, "#Y"==%jd", _ck_x, _ck_y); \
} while (0)
#undef ck_assert_int_eq
#define ck_assert_int_eq(X, Y) _ck_assert_int(X, ==, Y)
#undef ck_assert_int_ne
#define ck_assert_int_ne(X, Y) _ck_assert_int(X, !=, Y)
#undef ck_assert_int_lt
#define ck_assert_int_lt(X, Y) _ck_assert_int(X, <, Y)
#undef ck_assert_int_le
#define ck_assert_int_le(X, Y) _ck_assert_int(X, <=, Y)
#undef ck_assert_int_gt
#define ck_assert_int_gt(X, Y) _ck_assert_int(X, >, Y)
#undef ck_assert_int_ge
#define ck_assert_int_ge(X, Y) _ck_assert_int(X, >=, Y)

#undef _ck_assert_uint
#define _ck_assert_uint(X, OP, Y) do { \
  uint64_t _ck_x = (X); \
  uint64_t _ck_y = (Y); \
  ck_assert_msg(_ck_x OP _ck_y, "Assertion '"#X#OP#Y"' failed: "#X"==%ju, "#Y"==%ju", _ck_x, _ck_y); \
} while (0)
#undef ck_assert_uint_eq
#define ck_assert_uint_eq(X, Y) _ck_assert_uint(X, ==, Y)
#undef ck_assert_uint_ne
#define ck_assert_uint_ne(X, Y) _ck_assert_uint(X, !=, Y)
#undef ck_assert_uint_lt
#define ck_assert_uint_lt(X, Y) _ck_assert_uint(X, <, Y)
#undef ck_assert_uint_le
#define ck_assert_uint_le(X, Y) _ck_assert_uint(X, <=, Y)
#undef ck_assert_uint_gt
#define ck_assert_uint_gt(X, Y) _ck_assert_uint(X, >, Y)
#undef ck_assert_uint_ge
#define ck_assert_uint_ge(X, Y) _ck_assert_uint(X, >=, Y)

/* String comparsion macros with improved output compared to ck_assert() */
/* OP might be any operator that can be used in '0 OP strcmp(X,Y)' comparison */
/* The x and y parameter swap in strcmp() is needed to handle >, >=, <, <= operators */
#undef _ck_assert_str
#define _ck_assert_str(X, OP, Y) do { \
  const char* _ck_x = (X); \
  const char* _ck_y = (Y); \
  ck_assert_msg(0 OP strcmp(_ck_y, _ck_x), \
    "Assertion '"#X#OP#Y"' failed: "#X"==\"%s\", "#Y"==\"%s\"", _ck_x, _ck_y); \
} while (0)
#undef ck_assert_str_eq
#define ck_assert_str_eq(X, Y) _ck_assert_str(X, ==, Y)
#undef ck_assert_str_ne
#define ck_assert_str_ne(X, Y) _ck_assert_str(X, !=, Y)
#undef ck_assert_str_lt
#define ck_assert_str_lt(X, Y) _ck_assert_str(X, <, Y)
#undef ck_assert_str_le
#define ck_assert_str_le(X, Y) _ck_assert_str(X, <=, Y)
#undef ck_assert_str_gt
#define ck_assert_str_gt(X, Y) _ck_assert_str(X, >, Y)
#undef ck_assert_str_ge
#define ck_assert_str_ge(X, Y) _ck_assert_str(X, >=, Y)

/* Pointer comparsion macros with improved output compared to ck_assert(). */
/* OP may only be == or !=  */
#undef _ck_assert_ptr
#define _ck_assert_ptr(X, OP, Y) do { \
  const void* _ck_x = (X); \
  const void* _ck_y = (Y); \
  ck_assert_msg(_ck_x OP _ck_y, "Assertion '"#X#OP#Y"' failed: "#X"==%p, "#Y"==%p", _ck_x, _ck_y); \
} while (0)
#undef ck_assert_ptr_eq
#define ck_assert_ptr_eq(X, Y) _ck_assert_ptr(X, ==, Y)
#undef ck_assert_ptr_ne
#define ck_assert_ptr_ne(X, Y) _ck_assert_ptr(X, !=, Y)

#endif
