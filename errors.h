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
 * Error-message routines
 */

#ifndef __ERRORS_H_INCLUDED__
#define __ERRORS_H_INCLUDED__

void SetContextFilename(const char *filename);
void SetContextLump(const char *lump);
void ErrorExit(char *s, ...);

#endif
