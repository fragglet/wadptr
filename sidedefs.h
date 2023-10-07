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
 *
 *
 * Sidedef packing extension routines. Combines sidedefs which are
 * identical in a level, and shares them between multiple linedefs.
 */

#ifndef __SIDEDEFS_H_INCLUDED__
#define __SIDEDEFS_H_INCLUDED__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"

// The sidedef packing functions take the index of a SIDEDEFS lump to pack,
// and assume that the preceding lump is the LINEDEFS lump.
void P_Pack(int sidedef_num);
void P_Unpack(int sidedef_num);
bool P_IsPacked(int sidedef_num);

size_t P_WriteLinedefs(FILE *fstream);
size_t P_WriteSidedefs(FILE *fstream);

typedef struct {
    short xoffset;
    short yoffset;
    char upper[8];
    char middle[8];
    char lower[8];
    unsigned short sector_ref;

    // If true, this sidedef is referenced by a linedef with a special
    // type. This fixes the "scrolling linedefs bug" most notably,
    // although switches are also potentially affected.
    bool special;
} sidedef_t;

typedef struct {
    unsigned short vertex1;
    unsigned short vertex2;
    unsigned short flags;
    unsigned short type;
    unsigned short tag;
    unsigned short sidedef1;
    unsigned short sidedef2;
} linedef_t;

#define NO_SIDEDEF ((unsigned short) -1)

/*
 * Portable structure IO
 * (to handle endianness; also neither struct is a multiple of 4 in size)
 */
#define SDEF_XOFF   0
#define SDEF_YOFF   2
#define SDEF_UPPER  4
#define SDEF_MIDDLE 12
#define SDEF_LOWER  20
#define SDEF_SECTOR 28
#define SDEF_SIZE   30

#define LDEF_VERT1 0
#define LDEF_VERT2 2
#define LDEF_FLAGS 4
#define LDEF_TYPES 6
#define LDEF_TAG   8
#define LDEF_SDEF1 10
#define LDEF_SDEF2 12
#define LDEF_SIZE  14

#endif
