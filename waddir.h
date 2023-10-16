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
 * WAD loading and reading routines: by me, me, me!
 */

#ifndef __WADDIR_H_INCLUDED__
#define __WADDIR_H_INCLUDED__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"

typedef enum { IWAD, PWAD, NONWAD } wadtype;

typedef struct {
    long offset;
    long length;
    char name[8];
} entry_t;

typedef struct {
    FILE *fp;
    wadtype type;
    long num_entries, diroffset;
    entry_t *entries;
} wad_file_t;

// Portable structure I/O:
#define ENTRY_OFF  0
#define ENTRY_LEN  4
#define ENTRY_NAME 8
#define ENTRY_SIZE 16

// TODO: The API here for opening and closing WAD files is terrible and
// ought to be completely reworked.
bool ReadWad(wad_file_t *wf);
int EntryExists(wad_file_t *wf, char *entrytofind);
void *CacheLump(wad_file_t *wf, int entrynum);

int WriteWadHeader(wad_file_t *wf, FILE *fp);
int WriteWadDirectory(wad_file_t *wf, FILE *fp);

bool IsLevelEntry(char *s);

#endif
