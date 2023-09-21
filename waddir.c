/**************************************************************************
 *                                                                        *
 * Copyright(C) 1998-2011 Simon Howard, Andreas Dehmel                    *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 2 of the License, or      *
 * (at your option) any later version.                                    *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program; if not, write to the Free Software            *
 * Foundation, Inc, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA *
 *                                                                        *
 *                          The WADPTR project                            *
 *                                                                        *
 * WAD loading and reading routines: by me, me, me!                       *
 *                                                                        *
 **************************************************************************/

/************************ INCLUDES ****************************************/

#include "waddir.h"
#include "wadptr.h"

enum { false, true };

/************************ Globals ******************************************/

FILE *wadfp;
long numentries, diroffset;
entry_t wadentry[MAXENTRIES];
wadtype wad;

/* Read the WAD ************************************************************/

int ReadWadHeader(FILE *fp)
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

int ReadWadEntry(FILE *fp, entry_t *entry)
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

int ReadWadDirectory(FILE *fp)
{
    long i;

    for (i = 0; i < numentries; i++)
    {
        if (ReadWadEntry(fp, wadentry + i) != 0)
            return -1;
    }
    return 0;
}

int ReadWad()
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

/* Write the WAD header and directory *************************************/

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

int WriteWadEntry(FILE *fp, entry_t *entry)
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

void WriteWad()
{
    WriteWadHeader(wadfp);

    fseek(wadfp, diroffset, SEEK_SET);

    WriteWadDirectory(wadfp);
}

/* Takes a string8 in an entry type and returns a valid string *************/

char *ConvertString8(entry_t entry)
{
    static char temp[100][10];
    static int tempnum = 1;

    tempnum++;
    if (tempnum == 100)
        tempnum = 1;

    memset(temp[tempnum], 0, 9);
    memcpy(temp[tempnum], entry.name, 8);

    return temp[tempnum];
}

/* Finds if an entry exists ************************************************/

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

/* Finds an entry and returns information about it *************************/

entry_t FindInfo(char *entrytofind)
{
    int count;
    static entry_t entry;
    /*char buffer[10];*/

    for (count = 0; count < numentries; count++)
    {
        if (!strncmp(wadentry[count].name, entrytofind, 8))
            return wadentry[count];
    }
    memset(&entry, 0, sizeof(entry_t));
    return entry;
}

/* Adds an entry to the WAD ************************************************/

void AddEntry(entry_t entry)
{
    char buffer[10];

    strcpy(buffer, ConvertString8(entry));
    if (EntryExists(buffer) != -1)
        printf("\tWarning! Resource %s already exists!\n",
               ConvertString8(entry));
    memcpy(&wadentry[numentries], &entry, sizeof(entry_t));
    numentries++;
    WriteWad();
}

/* Load a lump to memory **************************************************/

void *CacheLump(int entrynum)
{
    char *working;

    working = malloc(wadentry[entrynum].length);
    if (!working)
        ErrorExit("CacheLump: Couldn't malloc %i bytes\n",
                  wadentry[entrynum].length);

    fseek(wadfp, wadentry[entrynum].offset, SEEK_SET);
    fread(working, wadentry[entrynum].length, 1, wadfp);

    return working;
}

/* Copy a WAD ( make a backup ) *******************************************/

void CopyWad(char *newfile)
{
    FILE *newwad;
    char a;

    newwad = fopen(newfile, "wb");
    if (!newwad)
        ErrorExit("CopyWad: Couldn't copy a wad (filename:%s)\n", newfile);

    rewind(wadfp);
    while (!feof(wadfp))
    {
        fread(&a, 1, 1, wadfp);
        fwrite(&a, 1, 1, newwad);
    }
    fclose(newwad);
}

/* Various WAD-related is??? functions ************************************/

int IsLevel(int entry)
{
    if (entry >= numentries)
        return false;

    /* 9/9/99: generalised support: if the next entry is a */
    /* things resource then its a level */
    return !strncmp(wadentry[entry + 1].name, "THINGS", 8);
}

int IsLevelEntry(char *s)
{
    if (!strcmp(s, "LINEDEFS"))
        return true;
    if (!strcmp(s, "SIDEDEFS"))
        return true;
    if (!strcmp(s, "SECTORS"))
        return true;
    if (!strcmp(s, "VERTEXES"))
        return true;
    if (!strcmp(s, "REJECT"))
        return true;
    if (!strcmp(s, "BLOCKMAP"))
        return true;
    if (!strcmp(s, "NODES"))
        return true;
    if (!strcmp(s, "THINGS"))
        return true;
    if (!strcmp(s, "SEGS"))
        return true;
    if (!strcmp(s, "SSECTORS"))
        return true;

    if (!strcmp(s, "BEHAVIOR"))
        return true; /* hexen "behavior" lump */
    return false;
}

/* Find the total size of a level ******************************************/

int FindLevelSize(char *s)
{
    int entrynum, count, sizecount = 0;

    entrynum = EntryExists(s);

    for (count = entrynum + 1; count < entrynum + 11; count++)
        sizecount += wadentry[count].length;

    return sizecount;
}
