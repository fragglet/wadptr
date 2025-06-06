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
 * Graphic squashing routines. Combines identical columns in graphic
 * lumps to make them smaller
 */

#ifndef __GRAPHICS_H_INCLUDED__
#define __GRAPHICS_H_INCLUDED__

#include <stdbool.h>
#include <stdint.h>

#include "waddir.h"

uint8_t *S_Squash(wad_file_t *wf, unsigned int entrynum);
uint8_t *S_Unsquash(wad_file_t *wf, unsigned int entrynum);
bool S_IsSquashed(wad_file_t *wf, unsigned int entrynum);
bool S_IsGraphic(wad_file_t *wf, unsigned int entrynum);

#endif
