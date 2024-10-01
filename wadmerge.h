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
 * Compresses WAD files by merging identical lumps in a WAD file and
 * sharing them between multiple wad directory entries.
 */

#ifndef __WADMERGE_H_INCLUDED__
#define __WADMERGE_H_INCLUDED__

#include <stdio.h>

#include "waddir.h"

void RebuildMergedWad(wad_file_t *wf, FILE *outwad);

#endif
