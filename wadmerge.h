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
 * Compresses WAD files by merging identical lumps in a WAD file and
 * sharing them between multiple wad directory entries.
 */

#ifndef __WADMERGE_H_INCLUDED__
#define __WADMERGE_H_INCLUDED__

#include <stdio.h>

#include "waddir.h"

void RebuildMergedWad(wad_file_t *wf, FILE *outwad);

#endif
