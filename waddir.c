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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "waddir.h"
#include "wadptr.h"

FILE *wadfp;
long numentries, diroffset;
entry_t *wadentry = NULL;
wadtype wad;

static int ReadWadHeader(FILE *fp)
{
    unsigned char buff[5];
    int wadType;

    rewind(fp);
    buff[4] = 0;
    fread(buff, 4, 1, fp);
    numentries = 0;
    diroffset = 0;

    if (strcmp((char *) buff, pwad_name) == 0)
    {
        wadType = PWAD;
    }
    else if (strcmp((char *) buff, iwad_name) == 0)
    {
        wadType = IWAD;
    }
    else
    {
        fprintf(stderr, "File is not an IWAD or a PWAD!\n");
        return NONWAD;
    }
    fread(buff, 4, 1, fp);
    numentries = READ_LONG(buff);
    fread(buff, 4, 1, fp);
    diroffset = READ_LONG(buff);
    return wadType;
}

static int ReadWadEntry(FILE *fp, entry_t *entry)
{
    unsigned char buff[ENTRY_SIZE];

    if (fread(buff, 1, ENTRY_SIZE, fp) != ENTRY_SIZE)
        return -1;
    entry->offset = READ_LONG(buff + ENTRY_OFF);
    entry->length = READ_LONG(buff + ENTRY_LEN);
    memset(entry->name, 0, 8);
    strncpy(entry->name, (char *) buff + ENTRY_NAME, 8);
    return 0;
}

static int ReadWadDirectory(FILE *fp)
{
    long i;

    wadentry = REALLOC_ARRAY(entry_t, wadentry, numentries);
    for (i = 0; i < numentries; i++)
    {
        if (ReadWadEntry(fp, wadentry + i) != 0)
            return -1;
    }
    return 0;
}

bool ReadWad(void)
{
    if ((wad = ReadWadHeader(wadfp)) == NONWAD)
    {
        return false;
    }

    fseek(wadfp, diroffset, SEEK_SET);
    ReadWadDirectory(wadfp);

    return true;
}

int WriteWadHeader(FILE *fp)
{
    char buff[5];

    rewind(fp);
    if (wad == PWAD)
    {
        strncpy(buff, pwad_name, 4);
    }
    else if (wad == IWAD)
    {
        strncpy(buff, iwad_name, 4);
    }
    else
    {
        ErrorExit("Trying to write a WAD of type %d?", wad);
    }
    fwrite(buff, 1, 4, fp);
    WRITE_LONG(buff, numentries);
    fwrite(buff, 1, 4, fp);
    WRITE_LONG(buff, diroffset);
    fwrite(buff, 1, 4, fp);
    return 0;
}

static int WriteWadEntry(FILE *fp, entry_t *entry)
{
    char buff[ENTRY_SIZE + 1];

    WRITE_LONG(buff + ENTRY_OFF, entry->offset);
    WRITE_LONG(buff + ENTRY_LEN, entry->length);
    memset(buff + ENTRY_NAME, 0, 8);
    strncpy(buff + ENTRY_NAME, entry->name, 8);
    return (fwrite(buff, 1, ENTRY_SIZE, fp) == ENTRY_SIZE) ? 0 : -1;
}

int WriteWadDirectory(FILE *fp)
{
    long i;

    for (i = 0; i < numentries; i++)
    {
        if (WriteWadEntry(fp, wadentry + i) != 0)
            return -1;
    }
    return 0;
}

int EntryExists(char *entrytofind)
{
    int count;
    for (count = 0; count < numentries; count++)
    {
        if (!strncmp(wadentry[count].name, entrytofind, 8))
            return count;
    }
    return -1;
}

/* Load a lump to memory */

void *CacheLump(int entrynum)
{
    uint8_t *working = ALLOC_ARRAY(uint8_t, wadentry[entrynum].length);

    fseek(wadfp, wadentry[entrynum].offset, SEEK_SET);
    fread(working, wadentry[entrynum].length, 1, wadfp);

    return working;
}

static const char *level_lump_names[] = {
    "THINGS",   // Level things data
    "LINEDEFS", // Level linedef data
    "SIDEDEFS", // Level sidedef data
    "VERTEXES", // Level vertex data
    "SEGS",     // Level wall segments
    "SSECTORS", // Level subsectors
    "NODES",    // Level BSP nodes
    "SECTORS",  // Level sector data
    "REJECT",   // Level reject table
    "BLOCKMAP", // Level blockmap data
    "BEHAVIOR", // Hexen compiled scripts
    "SCRIPTS",  // Hexen script source
    "LEAFS",    // PSX/D64 node leaves
    "LIGHTS",   // PSX/D64 colored lights
    "MACROS",   // Doom 64 Macros
    "GL_VERT",  // OpenGL extra vertices
    "GL_SEGS",  // OpenGL line segments
    "GL_SSECT", // OpenGL subsectors
    "GL_NODES", // OpenGL BSP nodes
    "GL_PVS",   // Potential Vis. Set
    "TEXTMAP",  // UDMF level data
    "DIALOGUE", // Strife conversations
    "ZNODES",   // UDMF BSP data
    "ENDMAP",   // UDMF end of level
};

// Returns true if the specified lump name is one of the "sub-lumps"
// associated with levels (in the list above).
bool IsLevelEntry(char *s)
{
    int i;

    for (i = 0; i < sizeof(level_lump_names) / sizeof(*level_lump_names); i++)
    {
        if (!strncmp(s, level_lump_names[i], 8))
        {
            return true;
        }
    }
    return false;
}
