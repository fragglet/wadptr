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
 * Error-message routines
 */

#ifndef __ERRORS_H_INCLUDED__
#define __ERRORS_H_INCLUDED__

void SetContextFilename(const char *filename);
void SetContextLump(const char *lump);
void Warning(char *s, ...);
void ErrorExit(char *s, ...);

#endif
