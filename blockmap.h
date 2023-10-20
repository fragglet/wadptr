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
 * Functions for compressing BLOCKMAP lumps.
 */

#ifndef __BLOCKMAP_H_INCLUDED__
#define __BLOCKMAP_H_INCLUDED__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "waddir.h"

bool B_Stack(wad_file_t *wf, unsigned int lumpnum);
bool B_Unstack(wad_file_t *wf, unsigned int lumpnum);
bool B_IsStacked(wad_file_t *wf, unsigned int lumpnum);
void B_WriteBlockmap(FILE *fstream, entry_t *entry);

#endif
