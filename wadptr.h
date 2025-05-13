/*
 * Copyright(C) 1998-2023 Simon Howard, Andreas Dehmel
 *
 * You can redistribute and/or modify this program under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, or any later version. This program is distributed WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <dirent.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "3.7"

extern bool allowpack;   // level packing on
extern bool allowsquash; // picture squashing on
extern bool allowmerge;  // lump merging on
extern bool extsides;    // extended sidedefs limit
extern bool extblocks;   // extended blockmap limit
extern bool wipesides;   // clear unneeded texture references

#ifdef _WIN32
#define DIRSEP "\\"
#else
#define DIRSEP "/"
#endif

// TODO: Remove clang-format overrides once Github is on clang-format 19
// clang-format off
#define READ_SHORT(p) (uint16_t) ((p)[0] | ((p)[1] << 8))
#define READ_LONG(p) \
    (uint32_t) ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24))
// clang-format on
#define WRITE_SHORT(p, s) (p)[0] = (s) & 0xff, (p)[1] = ((s) >> 8) & 0xff
#define WRITE_LONG(p, l)                             \
    (p)[0] = (l) & 0xff, (p)[1] = ((l) >> 8) & 0xff, \
    (p)[2] = ((l) >> 16) & 0xff, (p)[3] = ((l) >> 24) & 0xff

#define REALLOC_ARRAY(type, old, count) \
    (type *) CheckedRealloc(old, sizeof(type) * (count))
#define ALLOC_ARRAY(type, count) REALLOC_ARRAY(type, 0, count)

#undef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#undef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))

extern void PrintProgress(int numerator, int denominator);
extern void *CheckedRealloc(void *old, size_t nbytes);
