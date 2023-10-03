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
 * Functions for compressing individual lumps
 *
 * P_* : Sidedef packing extension routines. Combines sidedefs which are
 *       identical in a level, and shares them between multiple linedefs
 *
 * S_* : Graphic squashing routines. Combines identical columns in
 *       graphic lumps to make them smaller
 */

#include <stdint.h>
#include <string.h>

#include "lumps.h"
#include "wadptr.h"

typedef struct {
    uint16_t *elements;
    size_t len;
} block_t;

typedef struct {
    uint16_t *elements;
    size_t len, size;
    int num_blocks;
    block_t *blocklist;
} blockmap_t;

typedef struct {
    linedef_t *lines;
    size_t len, size;
} linedef_array_t;

typedef struct {
    sidedef_t *sides;
    size_t len, size;
} sidedef_array_t;

static void CheckLumpSizes(void);
static sidedef_array_t P_DoPack(sidedef_array_t *sidedefs);
static void P_RemapLinedefs(linedef_array_t *linedefs);
static sidedef_array_t P_RebuildSidedefs(linedef_array_t *linedefs,
                                         const sidedef_array_t *sidedefs);

static int S_FindRedundantColumns(uint8_t *s);
static int S_FindColumnSize(uint8_t *col1);

static linedef_array_t ReadLinedefs(int lumpnum, FILE *fp);
static sidedef_array_t ReadSidedefs(int lumpnum, FILE *fp);
static int WriteLinedefs(const linedef_array_t *linedefs, FILE *fp);
static int WriteSidedefs(const sidedef_array_t *sidedefs, FILE *fp);
static blockmap_t ReadBlockmap(int lumpnum, FILE *fp);
static bool WriteBlockmap(const blockmap_t *blockmap, FILE *fp);

static int p_sidedefnum; /* sidedef wad entry number */
static int p_linedefnum; /* linedef wad entry number */

static linedef_array_t p_linedefs_result;
static sidedef_array_t p_sidedefs_result;

static int *p_newsidedef_index; /* maps old sidedef number to new */

/* Graphic squashing globals */
static bool s_unsquash_mode = false; /* True when we are inside a
                                        S_Unsquash() call. */
static int s_equalcolumn[400];  /* 1 for each column: another column which is */
                                /* identical or -1 if there isn't one */
static short s_height, s_width; /* picture width, height etc. */
static short s_loffset, s_toffset;
static uint8_t *s_columns;  /* the location of each column in the lump */
static long s_colsize[400]; /* the length(in bytes) of each column */

/* Blockmap stacking globals */
static blockmap_t b_blockmap;

/* Pack a level */

/* Call P_Pack() with the level name eg. p_pack("MAP01");. p_pack will then */
/* pack that level. The new sidedef and linedef lumps are pointed to by */
/* p_sidedefres and p_linedefres. These must be free()d by other functions */
/* when they are no longer needed, as P_Pack does not do this. */

void P_Pack(int sidedef_num)
{
    sidedef_array_t sidedefs, sidedefs2;

    p_linedefnum = sidedef_num - 1;
    p_sidedefnum = sidedef_num;

    CheckLumpSizes();

    sidedefs = ReadSidedefs(p_sidedefnum, wadfp);
    p_linedefs_result = ReadLinedefs(p_linedefnum, wadfp);

    sidedefs2 = P_RebuildSidedefs(&p_linedefs_result, &sidedefs);
    free(sidedefs.sides);

    p_sidedefs_result = P_DoPack(&sidedefs2);
    free(sidedefs2.sides);

    P_RemapLinedefs(&p_linedefs_result); /* update sidedef indexes */
}

size_t P_WriteLinedefs(FILE *fstream)
{
    WriteLinedefs(&p_linedefs_result, fstream);
    free(p_linedefs_result.lines);
    return p_linedefs_result.len * LDEF_SIZE;
}

size_t P_WriteSidedefs(FILE *fstream)
{
    WriteSidedefs(&p_sidedefs_result, fstream);
    free(p_sidedefs_result.sides);
    return p_sidedefs_result.len * SDEF_SIZE;
}

/* Unpack a level */

/* Same thing, in reverse. Saves the new sidedef and linedef lumps to */
/* p_sidedefres and p_linedefres. */

void P_Unpack(int sidedef_num)
{
    sidedef_array_t sidedefs;

    p_linedefnum = sidedef_num - 1;
    p_sidedefnum = sidedef_num;

    CheckLumpSizes();

    p_linedefs_result = ReadLinedefs(p_linedefnum, wadfp);

    sidedefs = ReadSidedefs(p_sidedefnum, wadfp);
    p_sidedefs_result = P_RebuildSidedefs(&p_linedefs_result, &sidedefs);
    free(sidedefs.sides);
}

/* Sanity check a linedef's sidedef reference is valid */
static void CheckSidedefIndex(int linedef_index, int sidedef_index,
                              int num_sidedefs)
{
    if (sidedef_index != NO_SIDEDEF &&
        (sidedef_index < 0 || sidedef_index >= num_sidedefs))
    {
        ErrorExit("Linedef #%d contained invalid sidedef reference %d",
                  linedef_index, sidedef_index);
    }
}

/* Find if a level is packed */

bool P_IsPacked(int sidedef_num)
{
    linedef_array_t linedefs;
    uint8_t *sidedef_used;
    int num_sidedefs;
    bool packed = false;
    int count;

    // SIDEDEFS always follows LINEDEFS.
    p_linedefnum = sidedef_num - 1;
    p_sidedefnum = sidedef_num;

    linedefs = ReadLinedefs(p_linedefnum, wadfp);

    num_sidedefs = wadentry[p_sidedefnum].length / SDEF_SIZE;
    sidedef_used = ALLOC_ARRAY(uint8_t, num_sidedefs);
    for (count = 0; count < num_sidedefs; count++)
    {
        sidedef_used[count] = 0;
    }

    for (count = 0; count < linedefs.len; count++) /* now check */
    {
        if (linedefs.lines[count].sidedef1 != NO_SIDEDEF)
        {
            int sdi = linedefs.lines[count].sidedef1;
            CheckSidedefIndex(count, sdi, num_sidedefs);
            packed = packed || sidedef_used[sdi];
            sidedef_used[sdi] = 1;
        }
        if (linedefs.lines[count].sidedef2 != NO_SIDEDEF)
        {
            int sdi = linedefs.lines[count].sidedef2;
            CheckSidedefIndex(count, sdi, num_sidedefs);
            packed = packed || sidedef_used[sdi];
            sidedef_used[sdi] = 1;
        }
    }
    free(linedefs.lines);
    free(sidedef_used);
    return packed;
}

/* Append the given sidedef to the given array. */
static int AppendNewSidedef(sidedef_array_t *sidedefs, const sidedef_t *s)
{
    int result;

    while (sidedefs->len >= sidedefs->size)
    {
        sidedefs->size *= 2;
        sidedefs->sides =
            REALLOC_ARRAY(sidedef_t, sidedefs->sides, sidedefs->size);
    }

    memcpy(&sidedefs->sides[sidedefs->len], s, sizeof(sidedef_t));
    result = sidedefs->len;
    ++sidedefs->len;

    return result;
}

// Check the given array and try to find a sidedef that is the
// same as the given sidedef. Returns -1 if none is found.
static int FindSidedef(const sidedef_array_t *sidedefs,
                       const sidedef_t *sidedef)
{
    int i;

    if (sidedef->special)
    {
        return -1;
    }

    for (i = 0; i < sidedefs->len; i++)
    {
        if (!sidedefs->sides[i].special &&
            !memcmp(&sidedefs->sides[i], sidedef, sizeof(sidedef_t)))
        {
            return i;
        }
    }

    return -1;
}

/* Actually pack the sidedefs */

static sidedef_array_t P_DoPack(sidedef_array_t *sidedefs)
{
    sidedef_array_t result;
    int count, ns;

    result.size = sidedefs->len;
    result.sides = ALLOC_ARRAY(sidedef_t, result.size);
    result.len = 0;
    p_newsidedef_index = ALLOC_ARRAY(int, sidedefs->len);

    for (count = 0; count < sidedefs->len; count++)
    {
        if ((count % 100) == 0)
        {
            /* time for a percent-done update */
            PrintProgress(count, sidedefs->len);
        }

        ns = FindSidedef(&result, &sidedefs->sides[count]);
        if (ns >= 0)
        {
            p_newsidedef_index[count] = ns;
        }
        else
        {
            /* a sidedef like this does not yet exist: add one */
            p_newsidedef_index[count] =
                AppendNewSidedef(&result, &sidedefs->sides[count]);
        }
    }

    return result;
}

/* Update the linedefs and save sidedefs */

static void P_RemapLinedefs(linedef_array_t *linedefs)
{
    int count;

    /* update the linedefs with the new sidedef indexes. note that we
       do not need to perform the CheckSidedefIndex() checks here
       because they have already been checked by a previous call to
       P_Rebuild. */

    for (count = 0; count < linedefs->len; count++)
    {
        if (linedefs->lines[count].sidedef1 != NO_SIDEDEF)
            linedefs->lines[count].sidedef1 =
                p_newsidedef_index[linedefs->lines[count].sidedef1];
        if (linedefs->lines[count].sidedef2 != NO_SIDEDEF)
            linedefs->lines[count].sidedef2 =
                p_newsidedef_index[linedefs->lines[count].sidedef2];
    }

    free(p_newsidedef_index);
}

static void CheckLumpSizes(void)
{
    if ((wadentry[p_linedefnum].length % LDEF_SIZE) != 0)
    {
        ErrorExit("P_RebuildSidedefs: LINEDEFS lump (#%d) is %d bytes, "
                  "not a multiple of %d",
                  p_linedefnum, wadentry[p_linedefnum].length, LDEF_SIZE);
    }
    if ((wadentry[p_sidedefnum].length % SDEF_SIZE) != 0)
    {
        ErrorExit("P_RebuildSidedefs: SIDEDEFS lump (#%d) is %d bytes, "
                  "not a multiple of %d",
                  p_sidedefnum, wadentry[p_sidedefnum].length, SDEF_SIZE);
    }
}

/* Rebuild the sidedefs */

static sidedef_array_t P_RebuildSidedefs(linedef_array_t *linedefs,
                                         const sidedef_array_t *sidedefs)
{
    sidedef_array_t result;
    linedef_t *ld;
    bool is_special;
    int count;

    result.size = sidedefs->len;
    result.sides = ALLOC_ARRAY(sidedef_t, result.size);
    result.len = 0;

    for (count = 0; count < linedefs->len; count++)
    {
        ld = &linedefs->lines[count];
        // Special lines always get their own dedicated sidedefs, because:
        //  * If a scrolling linedef shares a sidedef with another linedef,
        //    it will make that other linedef scroll, or if multiple
        //    scrolling linedefs share a sidedef, it will scroll too fast.
        //    An example is with the spinning podium at the top of the stairs
        //    at the start of E1M1.
        //  * Switch linedefs change the texture of the front sidedef when
        //    the switch is activated. Similarly this could cause multiple
        //    switches to all mistakenly animate.
        // This could be more selective but different source ports add their
        // own new linedef types. For simplicity we just exclude sidedef
        // packing for all special lines.
        is_special = ld->type != 0;
        if (linedefs->lines[count].sidedef1 != NO_SIDEDEF)
        {
            int sdi = linedefs->lines[count].sidedef1;
            CheckSidedefIndex(count, sdi, sidedefs->len);
            linedefs->lines[count].sidedef1 =
                AppendNewSidedef(&result, &sidedefs->sides[sdi]);
            result.sides[sdi].special = is_special;
        }
        if (linedefs->lines[count].sidedef2 != NO_SIDEDEF)
        {
            int sdi = linedefs->lines[count].sidedef2;
            CheckSidedefIndex(count, sdi, sidedefs->len);
            linedefs->lines[count].sidedef2 =
                AppendNewSidedef(&result, &sidedefs->sides[sdi]);
            result.sides[sdi].special = is_special;
        }
    }

    return result;
}

/* Graphic squashing routines */

/* Squash a graphic lump */

/* Squashes a graphic. Call with the lump name - eg. S_Squash("TITLEPIC"); */
/* returns a pointer to the new(compressed) lump. This must be free()d when */
/* it is no longer needed, as S_Squash() does not do this itself. */

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

/* Find the redundant columns */

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

/* Find the size of a column */

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

/* Find if a graphic is squashed */

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

/* Is this a graphic ? */

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
    if (s[0] == 'D' && ((s[1] == '_') || (s[1] == 'S')))
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

    if ((width > 320) || (height > 200) || (width == 0) || (height == 0) ||
        (width < 0) || (height < 0))
    {
        free(graphic);
        return false;
    }

    /* it could be a graphic, but better safe than sorry */
    if ((wadentry[entrynum].length == 4096) || /* flat; */
        (wadentry[entrynum].length == 4000))   /* endoom */
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

static void MakeBlocklist(blockmap_t *blockmap)
{
    block_t *block;
    int i, start_index, end_index;

    blockmap->blocklist = ALLOC_ARRAY(block_t, blockmap->num_blocks);

    for (i = 0; i < blockmap->num_blocks; i++)
    {
        block = &blockmap->blocklist[i];
        // TODO: Need to do some array bounds checking here.
        start_index = blockmap->elements[4 + i];
        block->elements = &blockmap->elements[start_index];

        end_index = start_index;
        while (blockmap->elements[end_index] != 0xffff)
        {
            ++end_index;
        }
        block->len = end_index - start_index + 1;
    }
}

// TODO: We don't really need it right now, but this doesn't update the
// blockmap->blocklist[]->elements pointers when reallocating.
static void AppendBlockmapElements(blockmap_t *blockmap, uint16_t *elements,
                                   size_t count)
{
    while (blockmap->len + count > blockmap->size)
    {
        blockmap->size *= 2;
        blockmap->elements =
            REALLOC_ARRAY(uint16_t, blockmap->elements, blockmap->size);
    }

    memcpy(&blockmap->elements[blockmap->len], elements,
           count * sizeof(uint16_t));
    blockmap->len += count;
}

static int FindIdenticalBlock(const blockmap_t *blockmap, size_t num_blocks,
                              const block_t *block)
{
    int i;

    for (i = 0; i < num_blocks; i++)
    {
        const block_t *ib = &blockmap->blocklist[i];

        // We allow suffixes, but unless the blockmap is in "engine
        // format" it probably won't make a difference.
        if (block->len > ib->len)
        {
            continue;
        }

        if (!memcmp(block->elements, ib->elements + ib->len - block->len,
                    block->len * 2))
        {
            return i;
        }
    }

    return -1;
}

static blockmap_t RebuildBlockmap(blockmap_t *blockmap, bool compress)
{
    blockmap_t result;
    uint16_t *block_offsets;
    int i;

    MakeBlocklist(blockmap);

    result.size = blockmap->len;
    result.elements = ALLOC_ARRAY(uint16_t, result.size);
    result.len = 4 + blockmap->num_blocks;
    result.num_blocks = blockmap->num_blocks;
    block_offsets = &result.elements[4];

    // Header is identical:
    memcpy(result.elements, blockmap->elements, 4 * sizeof(uint16_t));

    for (i = 0; i < blockmap->num_blocks; i++)
    {
        const block_t *block = &blockmap->blocklist[i];
        int match_index = -1;

        if (compress)
        {
            match_index = FindIdenticalBlock(blockmap, i, block);
        }

        if (match_index >= 0)
        {
            // Copy the offset of the other block, but if it's a suffix
            // match then we need to offset.
            block_offsets[i] = block_offsets[match_index] +
                               blockmap->blocklist[match_index].len -
                               block->len;
        }
        else
        {
            block_offsets[i] = result.len;
            AppendBlockmapElements(&result, block->elements, block->len);
        }
    }

    free(blockmap->blocklist);

    return result;
}

void B_Stack(int lumpnum)
{
    blockmap_t blockmap = ReadBlockmap(lumpnum, wadfp);
    if (blockmap.len == 0)
    {
        // TODO: Need some better error handling.
        ErrorExit("failed to read blockmap?");
    }

    b_blockmap = RebuildBlockmap(&blockmap, true);

    // TODO: Check the rebuilt blockmap really is smaller. If it was
    // built using eg. ZokumBSP, the original is probably better than
    // what we've produced.

    free(blockmap.elements);
}

void B_Unstack(int lumpnum)
{
    blockmap_t blockmap = ReadBlockmap(lumpnum, wadfp);
    if (blockmap.len == 0)
    {
        // TODO: Need some better error handling.
        ErrorExit("failed to read blockmap?");
    }

    b_blockmap = RebuildBlockmap(&blockmap, false);

    free(blockmap.elements);
}

size_t B_WriteBlockmap(FILE *fstream)
{
    if (!WriteBlockmap(&b_blockmap, fstream))
    {
        ErrorExit("Error writing blockmap");
    }
    free(b_blockmap.elements);
    return b_blockmap.len * 2;
}

/*
 *  portable reading / writing of linedefs and sidedefs
 *  by Andreas Dehmel (dehmel@forwiss.tu-muenchen.de)
 */

static const int convbuffsize = 0x8000;
static uint8_t convbuffer[0x8000];

static linedef_array_t ReadLinedefs(int lumpnum, FILE *fp)
{
    linedef_array_t result;
    int i, validbytes;
    uint8_t *cptr;

    result.len = wadentry[lumpnum].length / LDEF_SIZE;
    result.lines = ALLOC_ARRAY(linedef_t, result.len);
    fseek(fp, wadentry[lumpnum].offset, SEEK_SET);
    validbytes = 0;
    cptr = convbuffer;
    for (i = 0; i < result.len; i++)
    {
        /* refill buffer? */
        if (validbytes < LDEF_SIZE)
        {
            if (validbytes != 0)
                memcpy(convbuffer, cptr, validbytes);
            validbytes += fread(convbuffer + validbytes, 1,
                                convbuffsize - validbytes, fp);
            cptr = convbuffer;
        }
        result.lines[i].vertex1 = READ_SHORT(cptr + LDEF_VERT1);
        result.lines[i].vertex2 = READ_SHORT(cptr + LDEF_VERT2);
        result.lines[i].flags = READ_SHORT(cptr + LDEF_FLAGS);
        result.lines[i].type = READ_SHORT(cptr + LDEF_TYPES);
        result.lines[i].tag = READ_SHORT(cptr + LDEF_TAG);
        result.lines[i].sidedef1 = READ_SHORT(cptr + LDEF_SDEF1);
        result.lines[i].sidedef2 = READ_SHORT(cptr + LDEF_SDEF2);
        cptr += LDEF_SIZE;
        validbytes -= LDEF_SIZE;
    }
    return result;
}

static int WriteLinedefs(const linedef_array_t *linedefs, FILE *fp)
{
    int i;
    uint8_t *cptr;

    cptr = convbuffer;
    for (i = 0; i < linedefs->len; i++)
    {
        if (cptr - convbuffer > convbuffsize - LDEF_SIZE)
        {
            fwrite(convbuffer, 1, cptr - convbuffer, fp);
            cptr = convbuffer;
        }
        WRITE_SHORT(cptr + LDEF_VERT1, linedefs->lines[i].vertex1);
        WRITE_SHORT(cptr + LDEF_VERT2, linedefs->lines[i].vertex2);
        WRITE_SHORT(cptr + LDEF_FLAGS, linedefs->lines[i].flags);
        WRITE_SHORT(cptr + LDEF_TYPES, linedefs->lines[i].type);
        WRITE_SHORT(cptr + LDEF_TAG, linedefs->lines[i].tag);
        WRITE_SHORT(cptr + LDEF_SDEF1, linedefs->lines[i].sidedef1);
        WRITE_SHORT(cptr + LDEF_SDEF2, linedefs->lines[i].sidedef2);
        cptr += LDEF_SIZE;
    }
    if (cptr != convbuffer)
    {
        fwrite(convbuffer, 1, cptr - convbuffer, fp);
    }
    return 0;
}

static sidedef_array_t ReadSidedefs(int lumpnum, FILE *fp)
{
    sidedef_array_t result;
    int i, validbytes;
    uint8_t *cptr;

    result.len = wadentry[lumpnum].length / SDEF_SIZE;
    result.sides = ALLOC_ARRAY(sidedef_t, result.len);
    fseek(fp, wadentry[lumpnum].offset, SEEK_SET);
    validbytes = 0;
    cptr = convbuffer;
    for (i = 0; i < result.len; i++)
    {
        if (validbytes < SDEF_SIZE)
        {
            if (validbytes != 0)
                memcpy(convbuffer, cptr, validbytes);
            validbytes += fread(convbuffer + validbytes, 1,
                                convbuffsize - validbytes, fp);
            cptr = convbuffer;
        }
        result.sides[i].xoffset = READ_SHORT(cptr + SDEF_XOFF);
        result.sides[i].yoffset = READ_SHORT(cptr + SDEF_YOFF);
        memset(result.sides[i].upper, 0, 8);
        strncpy(result.sides[i].upper, (char *) cptr + SDEF_UPPER, 8);
        memset(result.sides[i].middle, 0, 8);
        strncpy(result.sides[i].middle, (char *) cptr + SDEF_MIDDLE, 8);
        memset(result.sides[i].lower, 0, 8);
        strncpy(result.sides[i].lower, (char *) cptr + SDEF_LOWER, 8);
        result.sides[i].sector_ref = READ_SHORT(cptr + SDEF_SECTOR);
        result.sides[i].special = false;
        cptr += SDEF_SIZE;
        validbytes -= SDEF_SIZE;
    }
    return result;
}

static int WriteSidedefs(const sidedef_array_t *sidedefs, FILE *fp)
{
    int i;
    uint8_t *cptr;

    cptr = convbuffer;
    for (i = 0; i < sidedefs->len; i++)
    {
        if (cptr - convbuffer > convbuffsize - SDEF_SIZE)
        {
            fwrite(convbuffer, 1, cptr - convbuffer, fp);
            cptr = convbuffer;
        }
        WRITE_SHORT(cptr + SDEF_XOFF, sidedefs->sides[i].xoffset);
        WRITE_SHORT(cptr + SDEF_YOFF, sidedefs->sides[i].yoffset);
        memset(cptr + SDEF_UPPER, 0, 8);
        strncpy((char *) cptr + SDEF_UPPER, sidedefs->sides[i].upper, 8);
        memset(cptr + SDEF_MIDDLE, 0, 8);
        strncpy((char *) cptr + SDEF_MIDDLE, sidedefs->sides[i].middle, 8);
        memset(cptr + SDEF_LOWER, 0, 8);
        strncpy((char *) cptr + SDEF_LOWER, sidedefs->sides[i].lower, 8);
        WRITE_SHORT(cptr + SDEF_SECTOR, sidedefs->sides[i].sector_ref);
        cptr += SDEF_SIZE;
    }
    if (cptr != convbuffer)
    {
        fwrite(convbuffer, 1, cptr - convbuffer, fp);
    }
    return 0;
}

static blockmap_t ReadBlockmap(int lumpnum, FILE *fp)
{
    blockmap_t result;
    int i;

    result.len = wadentry[lumpnum].length / sizeof(uint16_t);
    if (result.len < 4)
    {
        result.elements = NULL;
        result.len = 0;
        return result;
    }
    result.elements = CacheLump(lumpnum);
    result.blocklist = 0;

    for (i = 0; i < result.len; i++)
    {
        result.elements[i] = READ_SHORT((uint8_t *) &result.elements[i]);
    }

    result.num_blocks = result.elements[2] * result.elements[3];
    if (result.len < result.num_blocks + 4)
    {
        free(result.elements);
        result.elements = NULL;
        result.len = 0;
    }

    return result;
}

static bool WriteBlockmap(const blockmap_t *blockmap, FILE *fp)
{
    uint8_t *buffer;
    bool result;
    int i;

    buffer = ALLOC_ARRAY(uint8_t, blockmap->len * 2);

    for (i = 0; i < blockmap->len; i++)
    {
        WRITE_SHORT(&buffer[i * 2], blockmap->elements[i]);
    }

    result = fwrite(buffer, 2, blockmap->len, fp) == blockmap->len;

    free(buffer);

    return result;
}
