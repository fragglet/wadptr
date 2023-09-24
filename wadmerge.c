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
 * Compresses WAD files by merging identical lumps in a WAD file and
 * sharing them between multiple wad directory entries.
 */

#include "wadmerge.h"
#include "wadptr.h"

static short sameas[4000];

static int Suggest(void)
{
    int count, count2, linkcnt = 0;
    short *links; /* possible links */
    char *check1, *check2;
    int perctime = 20;
    int maxlinks = MAXLINKS;

    links = ALLOC_ARRAY(short, 2 * maxlinks);

    /* find similar entries */
    for (count = 0; count < numentries; count++)
    {
        /* check against previous */
        for (count2 = 0; count2 < count; count2++)
        {
            if ((wadentry[count].length == wadentry[count2].length) &&
                wadentry[count].length != 0)
            {
                /* same length, might be same lump */
                if (linkcnt >= maxlinks)
                {
                    maxlinks += MAXLINKS;
                    links = REALLOC_ARRAY(short, links, 2 * maxlinks);
                }
                links[2 * linkcnt] = count;
                links[2 * linkcnt + 1] = count2;
                ++linkcnt;
            }
        }
    }

    /* now check 'em out + see if they really are the same */
    memset(sameas, -1, 2 * 4000);

    for (count = 0; count < linkcnt; count++)
    {
        if (sameas[links[2 * count]] != -1)
        {
            /* already done that one */
            continue;
        }

        check1 = CacheLump(links[2 * count]);
        check2 = CacheLump(links[2 * count + 1]);

        if (!memcmp(check1, check2, wadentry[links[2 * count]].length))
        {
            /* they are the same ! */
            sameas[links[2 * count]] = links[2 * count + 1];
        }

        free(check1); /* free back both lumps */
        free(check2);

        perctime--; /* % count */
        if (!perctime)
        {
            printf("%4d%%\b\b\b\b\b", (50 * count) / linkcnt);
            fflush(stdout);
            perctime = 200;
        }
    }

    free(links);

    return 0;
}

/* Rebuild the WAD, making it smaller in the process */

void Rebuild(const char *newname)
{
    int count;
    char *tempchar;
    FILE *newwad;
    long along = 0, filepos;
    int nextperc = 50;

    /* first run Suggest mode to find how to make it smaller */
    Suggest();

    newwad = fopen(newname, "wb+"); /* open the new wad */

    if (!newwad)
        ErrorExit("Rebuild: Couldn't open to %s\n", newname);

    fwrite(iwad_name, 1, 4, newwad);
    fwrite(&along, 4, 1, newwad);
    fwrite(&along, 4, 1, newwad);

    for (count = 0; count < numentries; count++)
    {
        /* first check if it's compressed(linked) */
        if (sameas[count] != -1)
        {
            wadentry[count].offset = wadentry[sameas[count]].offset;
        }
        else
        {
            filepos = ftell(newwad);
            tempchar = CacheLump(count);
            fwrite(tempchar, 1, wadentry[count].length, newwad);
            free(tempchar);
            wadentry[count].offset = filepos;
        }
        nextperc--; /* % count */
        if (!nextperc)
        {
            printf("%4d%%\b\b\b\b\b", (int) (50 + (count * 50) / numentries));
            fflush(stdout);
            nextperc = 50;
        }
    }
    /* write the wad directory */
    diroffset = ftell(newwad);
    WriteWadDirectory(newwad);
    WriteWadHeader(newwad);
    fclose(newwad);

    fflush(stdout); /* remove % count */
}
