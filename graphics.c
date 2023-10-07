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
#include "waddir.h"
#include "wadptr.h"

static void S_ParseLump(uint8_t *x);
static int S_FindColumnLength(uint8_t *col1);

/* Graphic squashing globals */
static bool unsquash_mode = false; /* True when we are inside a
                                      S_Unsquash() call. */
static short height, width;     /* picture width, height etc. */
static short loffset, toffset;
static uint8_t **columns = NULL; /* the location of each column in the lump */
static int *colsize = NULL;     /* the length(in bytes) of each column */

static void AppendBytes(uint8_t **ptr, size_t *len, size_t *sz,
                        uint8_t *newdata, size_t newdata_len)
{
    while (*len + newdata_len > *sz)
    {
        *sz *= 2;
        *ptr = REALLOC_ARRAY(uint8_t, *ptr, *sz);
    }

    memcpy(*ptr + *len, newdata, newdata_len);
    *len += newdata_len;
}

/* Squashes a graphic. Call with the lump name - eg. S_Squash("TITLEPIC");
   returns a pointer to the new(compressed) lump. This must be free()d when
   it is no longer needed, as S_Squash() does not do this itself. */
uint8_t *S_Squash(int entrynum)
{
    uint8_t *oldlump, *newres;
    size_t newres_len, newres_size;
    int x, x2;

    if (!S_IsGraphic(entrynum))
    {
        return NULL;
    }
    oldlump = CacheLump(entrynum);

    S_ParseLump(oldlump);

    newres_len = 8 + (width * 4);
    newres_size = 8 + (width * 4);
    newres = ALLOC_ARRAY(uint8_t, newres_size);

    // Copy header
    memcpy(newres, oldlump, 8);

    for (x = 0; x < width; x++)
    {
        for (x2 = 0; !unsquash_mode && x2 < x; x2++)
        {
            uint8_t *suffix_ptr;

            if (colsize[x2] < colsize[x])
            {
                continue;
            }

            // We allow prefix matches.
            suffix_ptr = columns[x2] + colsize[x2] - colsize[x];
            if (!memcmp(suffix_ptr, columns[x], colsize[x]))
            {
                memcpy(newres + 8 + x * 4, newres + 8 + x2 * 4, 4);
                break;
            }
        }

        // Not found, append new column.
        if (x2 >= x)
        {
            WRITE_LONG(newres + 8 + 4 * x, newres_len);
            AppendBytes(&newres, &newres_len, &newres_size,
                        columns[x], colsize[x]);
        }
    }

    if (!unsquash_mode && newres_len > wadentry[entrynum].length)
    {
        /* the new resource was bigger than the old one! */
        free(newres);
        return oldlump;
    }
    else
    {
        wadentry[entrynum].length = newres_len;
        free(oldlump);
        return newres;
    }
}

/* Unsquash a picture. Unsquashing rebuilds the image, just like when we
 * do the squashing, except that we set a special flag that skips
 * searching for identical columns. */
uint8_t *S_Unsquash(int entrynum)
{
    uint8_t *result;

    unsquash_mode = true;
    result = S_Squash(entrynum);
    unsquash_mode = false;

    return result;
}

static void S_ParseLump(uint8_t *lump)
{
    int x;

    width = READ_SHORT(lump);
    height = READ_SHORT(lump + 2);
    loffset = READ_SHORT(lump + 4);
    toffset = READ_SHORT(lump + 6);

    columns = REALLOC_ARRAY(uint8_t *, columns, width);
    colsize = REALLOC_ARRAY(int, colsize, width);

    for (x = 0; x < width; x++)
    {
        columns[x] = lump + READ_LONG(lump + 8 + 4 * x);
        colsize[x] = S_FindColumnLength(columns[x]);
    }
}

static int S_FindColumnLength(uint8_t *col1)
{
    int count = 0;

    while (1)
    {
        if (col1[count] == 255)
        {
            return count + 1;
        }
        /* jump to the beginning of the next post */
        count += col1[count + 1] + 4;
    }
}

bool S_IsSquashed(int entrynum)
{
    bool result = false;
    uint8_t *pic;
    int x, x2;

    pic = CacheLump(entrynum); /* cache the lump */
    S_ParseLump(pic);

    for (x = 0; !result && x < width; x++)
    {
        /* every previous column */
        for (x2 = 0; x2 < x; x2++)
        {
            if (columns[x] == columns[x2])
            {
                result = true;
            }
        }
    }

    free(pic);

    return result;
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
