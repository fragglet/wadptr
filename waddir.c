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

// TODO: Lots of existing code depends on this; eventually this global
// should be eliminated.
wad_file_t wadglobal;

static int ReadWadHeader(FILE *fp)
{
    unsigned char buff[5];
    int wadType;

    rewind(fp);
    buff[4] = 0;
    fread(buff, 4, 1, fp);
    wadglobal.num_entries = 0;
    wadglobal.diroffset = 0;

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
    wadglobal.num_entries = READ_LONG(buff);
    fread(buff, 4, 1, fp);
    wadglobal.diroffset = READ_LONG(buff);
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

    wadglobal.entries =
        REALLOC_ARRAY(entry_t, wadglobal.entries, wadglobal.num_entries);
    for (i = 0; i < wadglobal.num_entries; i++)
    {
        if (ReadWadEntry(fp, wadglobal.entries + i) != 0)
            return -1;
    }
    return 0;
}

bool ReadWad(void)
{
    if ((wadglobal.type = ReadWadHeader(wadglobal.fp)) == NONWAD)
    {
        return false;
    }

    fseek(wadglobal.fp, wadglobal.diroffset, SEEK_SET);
    ReadWadDirectory(wadglobal.fp);

    return true;
}

int WriteWadHeader(FILE *fp)
{
    char buff[5];

    rewind(fp);
    if (wadglobal.type == PWAD)
    {
        strncpy(buff, pwad_name, 4);
    }
    else if (wadglobal.type == IWAD)
    {
        strncpy(buff, iwad_name, 4);
    }
    else
    {
        ErrorExit("Trying to write a WAD of type %d?", wadglobal.type);
    }
    fwrite(buff, 1, 4, fp);
    WRITE_LONG(buff, wadglobal.num_entries);
    fwrite(buff, 1, 4, fp);
    WRITE_LONG(buff, wadglobal.diroffset);
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

    for (i = 0; i < wadglobal.num_entries; i++)
    {
        if (WriteWadEntry(fp, wadglobal.entries + i) != 0)
            return -1;
    }
    return 0;
}

int EntryExists(char *entrytofind)
{
    int count;
    for (count = 0; count < wadglobal.num_entries; count++)
    {
        if (!strncmp(wadglobal.entries[count].name, entrytofind, 8))
            return count;
    }
    return -1;
}

// Load a lump into memory.
// The name is misleading; nothing is being cached.
void *CacheLump(int entrynum)
{
    uint8_t *working = ALLOC_ARRAY(uint8_t, wadglobal.entries[entrynum].length);
    size_t read;

    if (fseek(wadglobal.fp, wadglobal.entries[entrynum].offset, SEEK_SET) != 0)
    {
        perror("fseek");
        ErrorExit("Error during seek to read %.8s lump, offset 0x%08x",
                  wadglobal.entries[entrynum].name,
                  wadglobal.entries[entrynum].offset);
    }
    read = fread(working, 1, wadglobal.entries[entrynum].length, wadglobal.fp);
    if (read < wadglobal.entries[entrynum].length)
    {
        perror("fread");
        ErrorExit("Error reading %.8s lump: %d of %d bytes read",
                  wadglobal.entries[entrynum].name, read,
                  wadglobal.entries[entrynum].length);
    }

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
