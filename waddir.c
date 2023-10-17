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

static void ReadWadHeader(wad_file_t *wf)
{
    uint8_t buf[WAD_HEADER_SIZE];
    int bytes;

    rewind(wf->fp);

    bytes = fread(buf, 1, WAD_HEADER_SIZE, wf->fp);
    if (bytes != WAD_HEADER_SIZE)
    {
        ErrorExit("Failed to read WAD header: read %d / %d bytes", bytes,
                  WAD_HEADER_SIZE);
    }

    if (memcmp(buf + WAD_HEADER_MAGIC, PWAD_MAGIC, 4) == 0)
    {
        wf->type = WAD_FILE_PWAD;
    }
    else if (memcmp(buf + WAD_HEADER_MAGIC, IWAD_MAGIC, 4) == 0)
    {
        wf->type = WAD_FILE_IWAD;
    }
    else
    {
        ErrorExit("File does not have IWAD or PWAD magic string!");
    }

    wf->num_entries = READ_LONG(buf + WAD_HEADER_NUM_ENTRIES);
    wf->diroffset = READ_LONG(buf + WAD_HEADER_DIR_OFFSET);
}

static bool ReadWadEntry(wad_file_t *wf, entry_t *entry)
{
    unsigned char buf[ENTRY_SIZE];

    if (fread(buf, 1, ENTRY_SIZE, wf->fp) != ENTRY_SIZE)
    {
        return false;
    }

    entry->offset = READ_LONG(buf + ENTRY_OFF);
    entry->length = READ_LONG(buf + ENTRY_LEN);
    memcpy(entry->name, buf + ENTRY_NAME, 8);

    return true;
}

static void ReadWadDirectory(wad_file_t *wf)
{
    unsigned int i;

    if (fseek(wf->fp, wf->diroffset, SEEK_SET) != 0)
    {
        perror("fseek");
        ErrorExit("Failed to seek to WAD directory");
    }

    wf->entries = REALLOC_ARRAY(entry_t, wf->entries, wf->num_entries);
    for (i = 0; i < wf->num_entries; i++)
    {
        if (!ReadWadEntry(wf, &wf->entries[i]))
        {
            ErrorExit("Failed to read WAD directory; read %d / %d entries", i,
                      wf->num_entries);
        }
    }
}

void ReadWad(wad_file_t *wf)
{
    ReadWadHeader(wf);
    ReadWadDirectory(wf);
}

void WriteWadHeader(wad_file_t *wf, FILE *fp)
{
    uint8_t buf[WAD_HEADER_SIZE];
    size_t bytes;

    rewind(fp);

    switch (wf->type)
    {
    case WAD_FILE_PWAD:
        memcpy(buf + WAD_HEADER_MAGIC, PWAD_MAGIC, 4);
        break;
    case WAD_FILE_IWAD:
        memcpy(buf + WAD_HEADER_MAGIC, IWAD_MAGIC, 4);
        break;
    }

    WRITE_LONG(buf + WAD_HEADER_NUM_ENTRIES, wf->num_entries);
    WRITE_LONG(buf + WAD_HEADER_DIR_OFFSET, wf->diroffset);

    bytes = fwrite(buf, 1, WAD_HEADER_SIZE, fp);
    if (bytes != WAD_HEADER_SIZE)
    {
        ErrorExit("Failed to write WAD header: wrote %d / %d bytes", bytes,
                  WAD_HEADER_SIZE);
    }
}

static void WriteWadEntry(FILE *fp, entry_t *entry)
{
    uint8_t buf[ENTRY_SIZE];
    size_t bytes;

    WRITE_LONG(buf + ENTRY_OFF, entry->offset);
    WRITE_LONG(buf + ENTRY_LEN, entry->length);
    memcpy(buf + ENTRY_NAME, entry->name, 8);

    bytes = fwrite(buf, 1, ENTRY_SIZE, fp);
    if (bytes != ENTRY_SIZE)
    {
        ErrorExit("Failed to write WAD entry: wrote %d / %d bytes", bytes,
                  ENTRY_SIZE);
    }
}

void WriteWadDirectory(wad_file_t *wf, FILE *fp)
{
    unsigned int i;

    for (i = 0; i < wf->num_entries; i++)
    {
        WriteWadEntry(fp, wf->entries + i);
    }
}

int EntryExists(wad_file_t *wf, char *entrytofind)
{
    int count;
    for (count = 0; count < wf->num_entries; count++)
    {
        if (!strncmp(wf->entries[count].name, entrytofind, 8))
            return count;
    }
    return -1;
}

// Load a lump into memory.
// The name is misleading; nothing is being cached.
void *CacheLump(wad_file_t *wf, int entrynum)
{
    uint8_t *working = ALLOC_ARRAY(uint8_t, wf->entries[entrynum].length);
    size_t read;

    if (fseek(wf->fp, wf->entries[entrynum].offset, SEEK_SET) != 0)
    {
        perror("fseek");
        ErrorExit("Error during seek to read %.8s lump, offset 0x%08x",
                  wf->entries[entrynum].name, wf->entries[entrynum].offset);
    }
    read = fread(working, 1, wf->entries[entrynum].length, wf->fp);
    if (read < wf->entries[entrynum].length)
    {
        perror("fread");
        ErrorExit("Error reading %.8s lump: %d of %d bytes read",
                  wf->entries[entrynum].name, read,
                  wf->entries[entrynum].length);
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
