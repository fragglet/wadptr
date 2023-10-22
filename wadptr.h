/*
 * Copyright(C) 1998-2023 Simon Howard, Andreas Dehmel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <dirent.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "3.3"

extern bool allowpack;   // level packing on
extern bool allowsquash; // picture squashing on
extern bool allowmerge;  // lump merging on
extern bool extsides;    // extended sidedefs limit
extern bool extblocks;   // extended blockmap limit

#ifdef _WIN32
#define DIRSEP "\\"
#else
#define DIRSEP "/"
#endif

#define READ_SHORT(p) (uint16_t)((p)[0] | ((p)[1] << 8))
#define READ_LONG(p) \
    (uint32_t)((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24))
#define WRITE_SHORT(p, s) (p)[0] = (s) &0xff, (p)[1] = ((s) >> 8) & 0xff
#define WRITE_LONG(p, l)                            \
    (p)[0] = (l) &0xff, (p)[1] = ((l) >> 8) & 0xff, \
    (p)[2] = ((l) >> 16) & 0xff, (p)[3] = ((l) >> 24) & 0xff

#define REALLOC_ARRAY(type, old, count) \
    (type *) CheckedRealloc(old, sizeof(type) * (count))
#define ALLOC_ARRAY(type, count) REALLOC_ARRAY(type, 0, count)

extern void PrintProgress(int numerator, int denominator);
extern void *CheckedRealloc(void *old, size_t nbytes);
