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

#include "waddir.h"
#include "wadptr.h"

static int ReadWadHeader(FILE *fp);
static int ReadWadDirectory(FILE *fp);
static int ReadWadEntry(FILE *fp, entry_t *entry);
static int WriteWadEntry(FILE *fp, entry_t *entry);

FILE *wadfp;
long numentries, diroffset;
entry_t wadentry[MAXENTRIES];
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
    wadType = NONWAD;
    if (strcmp((char *) buff, pwad_name) == 0)
        wadType = PWAD;
    if (strcmp((char *) buff, iwad_name) == 0)
        wadType = IWAD;
    if (wadType == NONWAD)
    {
        printf("File not IWAD or PWAD!\n");
        return wadType;
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
        return true;

    fseek(wadfp, diroffset, SEEK_SET);

    if (numentries > MAXENTRIES)
    {
        printf("Cannot handle > %i entry wads!\n", MAXENTRIES);
        return true;
    }
    ReadWadDirectory(wadfp);

    return false;
}

int WriteWadHeader(FILE *fp)
{
    unsigned char buff[5];

    rewind(fp);
    strcpy((char *) buff, "SFSF");
    if (wad == PWAD)
        strcpy((char *) buff, pwad_name);
    if (wad == IWAD)
        strcpy((char *) buff, iwad_name);
    fwrite(buff, 1, 4, fp);
    WRITE_LONG(buff, numentries);
    fwrite(buff, 1, 4, fp);
    WRITE_LONG(buff, diroffset);
    fwrite(buff, 1, 4, fp);
    return 0;
}

static int WriteWadEntry(FILE *fp, entry_t *entry)
{
    unsigned char buff[ENTRY_SIZE];

    WRITE_LONG(buff + ENTRY_OFF, entry->offset);
    WRITE_LONG(buff + ENTRY_LEN, entry->length);
    memset(buff + ENTRY_NAME, 0, 8);
    strncpy((char *) buff + ENTRY_NAME, entry->name, 8);
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

void WriteWad(void)
{
    WriteWadHeader(wadfp);

    fseek(wadfp, diroffset, SEEK_SET);

    WriteWadDirectory(wadfp);
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
    char *working = ALLOC_ARRAY(char, wadentry[entrynum].length);

    fseek(wadfp, wadentry[entrynum].offset, SEEK_SET);
    fread(working, wadentry[entrynum].length, 1, wadfp);

    return working;
}

/* Various WAD-related is??? functions */

bool IsLevel(int entry)
{
    if (entry >= numentries)
        return false;

    /* 9/9/99: generalised support: if the next entry is a */
    /* things resource then its a level */
    return !strncmp(wadentry[entry + 1].name, "THINGS", 8);
}

bool IsLevelEntry(char *s)
{
    if (!strncmp(s, "LINEDEFS", 8))
        return true;
    if (!strncmp(s, "SIDEDEFS", 8))
        return true;
    if (!strncmp(s, "SECTORS", 8))
        return true;
    if (!strncmp(s, "VERTEXES", 8))
        return true;
    if (!strncmp(s, "REJECT", 8))
        return true;
    if (!strncmp(s, "BLOCKMAP", 8))
        return true;
    if (!strncmp(s, "NODES", 8))
        return true;
    if (!strncmp(s, "THINGS", 8))
        return true;
    if (!strncmp(s, "SEGS", 8))
        return true;
    if (!strncmp(s, "SSECTORS", 8))
        return true;

    if (!strncmp(s, "BEHAVIOR", 8))
        return true; /* hexen "behavior" lump */
    return false;
}

/* Find the total size of a level */

int FindLevelSize(char *s)
{
    int entrynum, count, sizecount = 0;

    entrynum = EntryExists(s);

    for (count = entrynum + 1; count < entrynum + 11; count++)
        sizecount += wadentry[count].length;

    return sizecount;
}
