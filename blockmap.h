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
