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
#include "sort.h"
#include "waddir.h"
#include "wadptr.h"

static bool ParseLump(uint8_t *lump, size_t lump_len);
static bool FindColumnLength(unsigned int x, const uint8_t *column, size_t len,
                             unsigned *result);

// True when we are inside an S_Unsquash call.
static bool unsquash_mode = false;

// Picture width, height, etc. from header.
static unsigned short height, width;
static unsigned short loffset, toffset;

static uint8_t **columns = NULL;
static unsigned int *colsize = NULL;

static void AppendBytes(uint8_t **ptr, size_t *len, size_t *sz,
                        const uint8_t *newdata, const size_t newdata_len)
{
    while (*len + newdata_len > *sz)
    {
        *sz *= 2;
        *ptr = REALLOC_ARRAY(uint8_t, *ptr, *sz);
    }

    memcpy(*ptr + *len, newdata, newdata_len);
    *len += newdata_len;
}

static int LargestColumnCompare(unsigned int index1, unsigned int index2,
                                const void *callback_data)
{
    (void) callback_data;
    return colsize[index2] - colsize[index1];
}

// Certain tools (though I'm not sure which?) generate inefficient columns
// that get split across multiple posts unnecessarily. An example can be
// found in eg. btsx_e2a.wad's TITLEPIC and CREDITS lumps. We can save a
// few bytes by combining them.
static void CombinePosts(void)
{
    uint8_t *post, *next_post;
    unsigned int x, i;

    for (x = 0; x < width; x++)
    {
        post = columns[x];

        i = 0;
        while (post[i] != 0xff)
        {
            uint8_t off = post[0], len = post[1];
            unsigned int next_i = i + post[i + 1] + 4;

            next_post = post + next_i;
            if (next_post[0] != 0xff)
            {
                uint8_t next_off = next_post[0], next_len = next_post[1];

                // If the next post exactly follows on from this one,
                // and the new length won't overflow, we can merge.
                if (((int) off + (int) len) == next_off &&
                    ((int) len + (int) next_len) < 0x100)
                {
                    post[1] += next_len;
                    memmove(post + 3 + len, next_post + 3,
                            colsize[x] - next_i - 3);
                    colsize[x] -= 4;
                    continue;
                }
            }
            i = next_i;
        }
    }
}

// Squashes a graphic. Call with the lump number, returns a pointer to the
// new(compressed) lump. This must be free()d when it is no longer needed, as
// S_Squash() does not do this itself.
uint8_t *S_Squash(wad_file_t *wf, unsigned int entrynum)
{
    uint8_t *oldlump, *newres;
    size_t newres_len, newres_size;
    unsigned int *sorted_map;
    unsigned int i, i2;

    oldlump = CacheLump(wf, entrynum);

    // It is possible in some cases that we encounter a corrupt graphic
    // lump; in these cases ParseLump() prints an error message, but we
    // otherwise just ignore the problem lump and keep using the same
    // contents as before.
    if (!ParseLump(oldlump, wf->entries[entrynum].length))
    {
        fprintf(stderr,
                "%.8s is a badly-formed or corrupt graphic lump. "
                "No attempt will be made to process it.\n",
                wf->entries[entrynum].name);
        return oldlump;
    }
    CombinePosts();

    // We build the sorted map so that we iterate over columns by order of
    // decreasing size; this maximizes the chance of being able to make a
    // prefix match against previous (larger) columns.
    sorted_map = MakeSortedMap(width, LargestColumnCompare, NULL);

    newres_len = 8 + (width * 4);
    newres_size = 8 + (width * 4);
    newres = ALLOC_ARRAY(uint8_t, newres_size);

    // Copy header
    memcpy(newres, oldlump, 8);

    for (i = 0; i < width; i++)
    {
        unsigned int x = sorted_map[i];
#ifdef DEBUG
        printf("column: %4d len: %4d\n", sorted_map[i], colsize[sorted_map[i]]);
#endif
        for (i2 = 0; !unsquash_mode && i2 < i; i2++)
        {
            uint8_t *suffix_ptr;
            unsigned int x2 = sorted_map[i2];

            if (colsize[x2] < colsize[x])
            {
                continue;
            }

            // We allow prefix matches.
            suffix_ptr = columns[x2] + colsize[x2] - colsize[x];
            if (!memcmp(suffix_ptr, columns[x], colsize[x]))
            {
#ifdef DEBUG
                printf("\tmatches %4d\n", x2);
#endif
                WRITE_LONG(newres + 8 + 4 * x, READ_LONG(newres + 8 + 4 * x2) +
                                                   colsize[x2] - colsize[x]);
                break;
            }
        }

        // Not found, append new column.
        if (unsquash_mode || i2 >= i)
        {
            WRITE_LONG(newres + 8 + 4 * x, newres_len);
            AppendBytes(&newres, &newres_len, &newres_size, columns[x],
                        colsize[x]);
        }
    }

    free(sorted_map);

    if (!unsquash_mode && newres_len > wf->entries[entrynum].length)
    {
        // The new resource was bigger than the old one!
        free(newres);
        return oldlump;
    }
    else
    {
        wf->entries[entrynum].length = newres_len;
        free(oldlump);
        return newres;
    }
}

// Unsquash a picture. Unsquashing rebuilds the image, just like when we
// do the squashing, except that we set a special flag that skips
// searching for identical columns.
uint8_t *S_Unsquash(wad_file_t *wf, unsigned int entrynum)
{
    uint8_t *result;

    unsquash_mode = true;
    result = S_Squash(wf, entrynum);
    unsquash_mode = false;

    return result;
}

static bool ParseLump(uint8_t *lump, size_t lump_len)
{
    int x;

    width = READ_SHORT(lump);
    height = READ_SHORT(lump + 2);
    loffset = READ_SHORT(lump + 4);
    toffset = READ_SHORT(lump + 6);

    columns = REALLOC_ARRAY(uint8_t *, columns, width);
    colsize = REALLOC_ARRAY(unsigned int, colsize, width);

    for (x = 0; x < width; x++)
    {
        uint32_t offset = READ_LONG(lump + 8 + 4 * x);
        if (offset > lump_len)
        {
            ErrorExit("Column %d offset invalid: %08x > length %ld", x, offset,
                      lump_len);
        }
        columns[x] = lump + offset;
        if (!FindColumnLength(x, columns[x], lump_len - offset, &colsize[x]))
        {
            return false;
        }
    }

    return true;
}

static bool FindColumnLength(unsigned int x, const uint8_t *column, size_t len,
                             unsigned int *result)
{
    unsigned int i = 0;

    while (1)
    {
        if (column[i] == 0xff)
        {
            *result = i + 1;
            return true;
        }
        // jump to the beginning of the next post
        i += column[i + 1] + 4;
        if (i > len)
        {
            fprintf(stderr,
                    "Column %d overruns the end of the lump with no 0xff "
                    "terminating byte.\n",
                    x);
            return false;
        }
    }
}

bool S_IsSquashed(wad_file_t *wf, unsigned int entrynum)
{
    bool result = false;
    uint8_t *pic;
    unsigned int x, x2;

    pic = CacheLump(wf, entrynum);
    if (!ParseLump(pic, wf->entries[entrynum].length))
    {
        free(pic);
        return false;
    }

    for (x = 0; !result && x < width; x++)
    {
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

bool S_IsGraphic(wad_file_t *wf, unsigned int entrynum)
{
    uint8_t *graphic, *columns;
    char *s = wf->entries[entrynum].name;
    unsigned int count;
    unsigned short width, height;

    if (!strncmp(s, "ENDOOM", 8))
        return false;
    if (IsLevelEntry(s))
        return false;
    if (!strncmp(s, "DS", 2) || !strncmp(s, "DP", 2) || !strncmp(s, "D_", 2))
    {
        // sfx or music
        return false;
    }

    if (wf->entries[entrynum].length < 8)
    {
        // too short
        return false;
    }
    graphic = CacheLump(wf, entrynum);

    width = READ_SHORT(graphic);
    height = READ_SHORT(graphic + 2);
    columns = graphic + 8;

    if (width > 320 || height > 200 || width <= 0 || height <= 0 ||
        width * 4 + 8U > wf->entries[entrynum].length)
    {
        free(graphic);
        return false;
    }

    if (wf->entries[entrynum].length == 4096 || // flat
        wf->entries[entrynum].length == 4000)   // endoom
    {
        // It could be a graphic, but better safe than sorry
        free(graphic);
        return false;
    }

    for (count = 0; count < width; count++)
    {
        if (READ_LONG(columns + 4 * count) > wf->entries[entrynum].length)
        {
            // Can't be a graphic resource; offset outside lump
            free(graphic);
            return false;
        }
    }
    free(graphic);

    // If it has passed all these checks, it must be a graphic (well, probably)
    return true;
}
