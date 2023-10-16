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
 * Graphic squashing routines. Combines identical columns in graphic
 * lumps to make them smaller
 */

#ifndef __GRAPHICS_H_INCLUDED__
#define __GRAPHICS_H_INCLUDED__

#include <stdbool.h>
#include <stdint.h>

#include "waddir.h"

uint8_t *S_Squash(wad_file_t *wf, int entrynum);
uint8_t *S_Unsquash(wad_file_t *wf, int entrynum);
bool S_IsSquashed(wad_file_t *wf, int entrynum);
bool S_IsGraphic(wad_file_t *wf, int entrynum);

#endif
