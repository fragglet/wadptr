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

#include "waddir.h"

// The sidedef packing functions take the index of a SIDEDEFS lump to pack,
// and assume that the preceding lump is the LINEDEFS lump.
bool P_Pack(wad_file_t *wf, int sidedef_num);
bool P_Unpack(wad_file_t *wf, int sidedef_num);
bool P_IsPacked(wad_file_t *wf, int sidedef_num);

void P_WriteLinedefs(FILE *fstream, entry_t *entry);
size_t P_WriteSidedefs(FILE *fstream);

#endif
