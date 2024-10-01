/*
 * Copyright(C) 1998-2023 Simon Howard, Andreas Dehmel
 *
 * You can redistribute and/or modify this program under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, or any later version. This program is distributed WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
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
bool P_Pack(wad_file_t *wf, unsigned int sidedef_num);
bool P_Unpack(wad_file_t *wf, unsigned int sidedef_num);
bool P_IsPacked(wad_file_t *wf, unsigned int sidedef_num);

void P_WriteLinedefs(FILE *fstream, entry_t *entry);
void P_WriteSidedefs(FILE *fstream, entry_t *entry);

#endif
