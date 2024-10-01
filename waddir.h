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
 * WAD loading and reading routines: by me, me, me!
 */

#ifndef __WADDIR_H_INCLUDED__
#define __WADDIR_H_INCLUDED__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"

typedef struct {
    uint32_t offset;
    uint32_t length;
    char name[8];
} entry_t;

typedef enum {
    WAD_FILE_IWAD,
    WAD_FILE_PWAD,
} wad_file_type_t;

typedef struct {
    FILE *fp;
    wad_file_type_t type;
    uint32_t num_entries;
    entry_t *entries;
} wad_file_t;

#define PWAD_MAGIC "PWAD"
#define IWAD_MAGIC "IWAD"

// Portable structure I/O:
#define WAD_HEADER_MAGIC       0
#define WAD_HEADER_NUM_ENTRIES 4
#define WAD_HEADER_DIR_OFFSET  8
#define WAD_HEADER_SIZE        12

#define ENTRY_OFF  0
#define ENTRY_LEN  4
#define ENTRY_NAME 8
#define ENTRY_SIZE 16

bool OpenWadFile(wad_file_t *wf, const char *filename);
void CloseWadFile(wad_file_t *wf);

int EntryExists(wad_file_t *wf, char *entrytofind);
void *CacheLump(wad_file_t *wf, unsigned int entrynum);

void WriteWadDirectory(FILE *fp, wad_file_type_t type, entry_t *entries,
                       size_t num_entries);
uint32_t WriteWadLump(FILE *fp, void *buf, size_t len);

bool IsLevelEntry(char *s);

#endif
