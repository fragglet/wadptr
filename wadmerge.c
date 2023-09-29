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

static int *Suggest(void)
{
    int count, count2, linkcnt = 0;
    int *result;
    short *links; /* possible links */
    uint8_t *check1, *check2;
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
    result = ALLOC_ARRAY(int, numentries);
    for (count = 0; count < numentries; count++)
    {
        result[count] = -1;
    }

    for (count = 0; count < linkcnt; count++)
    {
        if (result[links[2 * count]] != -1)
        {
            /* already done that one */
            continue;
        }

        check1 = CacheLump(links[2 * count]);
        check2 = CacheLump(links[2 * count + 1]);

        if (!memcmp(check1, check2, wadentry[links[2 * count]].length))
        {
            /* they are the same ! */
            result[links[2 * count]] = links[2 * count + 1];
        }

        free(check1); /* free back both lumps */
        free(check2);

        if ((count % 100) == 0)
        {
            PrintProgress(count, linkcnt);
        }
    }

    free(links);

    return result;
}

/* Rebuild the WAD, making it smaller in the process */

void Rebuild(FILE *newwad)
{
    int count;
    int *sameas;
    uint8_t *cached;
    long along = 0, filepos;

    /* first run Suggest mode to find how to make it smaller */
    sameas = Suggest();

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
            cached = CacheLump(count);
            fwrite(cached, 1, wadentry[count].length, newwad);
            free(cached);
            wadentry[count].offset = filepos;
        }
    }
    /* write the wad directory */
    diroffset = ftell(newwad);
    WriteWadDirectory(newwad);
    WriteWadHeader(newwad);

    fflush(stdout); /* remove % count */
    free(sameas);
}
