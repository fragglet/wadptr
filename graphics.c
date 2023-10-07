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
 * Graphic squashing routines. Combines identical columns in graphic
 * lumps to make them smaller
 */

#include <stdint.h>
#include <string.h>

#include "graphics.h"
#include "wadptr.h"

static int S_FindRedundantColumns(uint8_t *s);
static int S_FindColumnSize(uint8_t *col1);

/* Graphic squashing globals */
static bool s_unsquash_mode = false; /* True when we are inside a
                                        S_Unsquash() call. */
static int s_equalcolumn[400];  /* 1 for each column: another column which is */
                                /* identical or -1 if there isn't one */
static short s_height, s_width; /* picture width, height etc. */
static short s_loffset, s_toffset;
static uint8_t *s_columns;  /* the location of each column in the lump */
static long s_colsize[400]; /* the length(in bytes) of each column */

/* Squashes a graphic. Call with the lump name - eg. S_Squash("TITLEPIC");
   returns a pointer to the new(compressed) lump. This must be free()d when
   it is no longer needed, as S_Squash() does not do this itself. */

uint8_t *S_Squash(int entrynum)
{
    uint8_t *working, *newres, *newptr;
    int count;
    long lastpt;

    if (!S_IsGraphic(entrynum))
    {
        return NULL;
    }
    working = CacheLump(entrynum);
    if ((long) working == -1)
        ErrorExit("squash: Couldn't find %.8s", wadentry[entrynum].name);

    /* find posts to be killed; if none, return original lump */
    if (S_FindRedundantColumns(working) == 0 && !s_unsquash_mode)
        return working;

    /* TODO: Fix static limit. */
    newres = ALLOC_ARRAY(uint8_t, 100000);

    /* find various info: size,offset etc. */
    WRITE_SHORT(newres, s_width);
    WRITE_SHORT(newres + 2, s_height);
    WRITE_SHORT(newres + 4, s_loffset);
    WRITE_SHORT(newres + 6, s_toffset);

    /* the new column pointers for the new lump */
    newptr = newres + 8;

    lastpt = 8 + (s_width * 4); /* last point in the lump */

    for (count = 0; count < s_width; count++)
    {
        if (s_equalcolumn[count] == -1)
        {
            /* add a new column */
            WRITE_LONG(newptr + 4 * count,
                       lastpt); /* point this column to lastpt */
            memcpy(newres + lastpt, working + READ_LONG(s_columns + 4 * count),
                   s_colsize[count]);
            lastpt += s_colsize[count];
        }
        else
        {
            /* postfix compression, see S_FindRedundantColumns() */
            long identOff;

            identOff = READ_LONG(newptr + 4 * s_equalcolumn[count]);
            identOff += s_colsize[s_equalcolumn[count]] - s_colsize[count];
            WRITE_LONG(newptr + 4 * count, identOff);
        }
    }

    if (!s_unsquash_mode && lastpt > wadentry[entrynum].length)
    {
        /* the new resource was bigger than the old one! */
        free(newres);
        return working;
    }
    else
    {
        wadentry[entrynum].length = lastpt;
        free(working);
        return newres;
    }
}

/* Unsquash a picture. Unsquashing rebuilds the image, just like when we
 * do the squashing, except that we set a special flag that skips
 * searching for identical columns. */
uint8_t *S_Unsquash(int entrynum)
{
    uint8_t *result;

    s_unsquash_mode = true;
    result = S_Squash(entrynum);
    s_unsquash_mode = false;

    return result;
}

static int S_FindRedundantColumns(uint8_t *x)
{
    int count, count2;
    int num_killed = 0;

    s_width = READ_SHORT(x);
    s_height = READ_SHORT(x + 2);
    s_loffset = READ_SHORT(x + 4);
    s_toffset = READ_SHORT(x + 6);

    s_columns = x + 8;

    for (count = 0; count < s_width; count++)
    {
        long tmpcol;

        /* first assume no identical column exists */
        s_equalcolumn[count] = -1;

        /* find the column size */
        tmpcol = READ_LONG(s_columns + 4 * count);
        s_colsize[count] = S_FindColumnSize(x + tmpcol);

        /* Unsquash mode is identical to squash mode but we just don't
           look for any identical columns. */
        if (s_unsquash_mode)
        {
            continue;
        }

        /* check all previous columns */
        for (count2 = 0; count2 < count; count2++)
        {
            /* compression is also possible if col is a postfix of col2 */
            if (s_colsize[count] > s_colsize[count2])
            {
                /* new column longer than previous, can't be postfix */
                continue;
            }

            if (!memcmp(x + tmpcol,
                        x + READ_LONG(s_columns + 4 * count2) +
                            s_colsize[count2] - s_colsize[count],
                        s_colsize[count]))
            {
                s_equalcolumn[count] = count2;
                num_killed++;
                break;
            }
        }
    }
    /* tell squash how many can be 'got rid of' */
    return num_killed;
}

static int S_FindColumnSize(uint8_t *col1)
{
    int count = 0;

    while (1)
    {
        if (col1[count] == 255)
        {
            /* no more posts */
            return count + 1; /* must be +1 or the pic gets cacked up */
        }
        /* jump to the beginning of the next post */
        count += col1[count + 1] + 4;
    }
}

bool S_IsSquashed(int entrynum)
{
    uint8_t *pic;
    int count, count2;

    pic = CacheLump(entrynum); /* cache the lump */

    s_width = READ_SHORT(pic);
    s_height = READ_SHORT(pic + 2);
    s_loffset = READ_SHORT(pic + 4);
    s_toffset = READ_SHORT(pic + 6);

    /* find the column locations */
    s_columns = pic + 8;

    for (count = 0; count < s_width; count++)
    {
        long tmpcol;

        tmpcol = READ_LONG(s_columns + 4 * count);
        /* every previous column */
        for (count2 = 0; count2 < count; count2++)
        {
            if (tmpcol == READ_LONG(s_columns + 4 * count2))
            {
                /* these columns have the same lump location; it is squashed */
                free(pic);
                return true;
            }
        }
    }
    free(pic);
    /* it cant be: no 2 columns have the same lump location */
    return false;
}

bool S_IsGraphic(int entrynum)
{
    uint8_t *graphic, *columns;
    char *s = wadentry[entrynum].name;
    int count;
    short width, height;

    if (!strncmp(s, "ENDOOM", 8))
        return false;
    if (IsLevelEntry(s))
        return false;
    if (s[0] == 'D' && (s[1] == '_' || s[1] == 'S'))
    {
        /* sfx or music */
        return false;
    }

    if (wadentry[entrynum].length <= 0)
    {
        /* don't read data from 0 size lumps */
        return false;
    }
    graphic = CacheLump(entrynum);

    width = READ_SHORT(graphic);
    height = READ_SHORT(graphic + 2);
    columns = graphic + 8;

    if (width > 320 || height > 200 || width <= 0 || height <= 0)
    {
        free(graphic);
        return false;
    }

    /* it could be a graphic, but better safe than sorry */
    if (wadentry[entrynum].length == 4096 || /* flat; */
        wadentry[entrynum].length == 4000)   /* endoom */
    {
        free(graphic);
        return false;
    }

    for (count = 0; count < width; count++)
    {
        if (READ_LONG(columns + 4 * count) > wadentry[entrynum].length)
        {
            /* cant be a graphic resource then -offset outside lump */
            free(graphic);
            return false;
        }
    }
    free(graphic);
    /* if its passed all these checks it must be (well probably) */
    return true;
}

