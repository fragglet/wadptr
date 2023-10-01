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

struct block
{
    uint16_t *elements;
    size_t len;
};

static void P_FindInfo(void);
static void P_DoPack(sidedef_t *sidedefs);
static void P_BuildLinedefs(linedef_t *linedefs);
static void P_Rebuild(void);

static int S_FindRedundantColumns(uint8_t *s);
static int S_FindColumnSize(uint8_t *col1);

static linedef_t *ReadLinedefs(int lumpnum, FILE *fp);
static sidedef_t *ReadSidedefs(int lumpnum, FILE *fp);
static bool ReadBlockmap(int lumpnum, FILE *fp);

static int p_levelnum;   /* entry number (index in WAD directory) */
static int p_sidedefnum; /* sidedef wad entry number */
static int p_linedefnum; /* linedef wad entry number */
static int p_num_linedefs = 0,
           p_num_sidedefs = 0; /* number of sd/lds do not get confused */
static const char *p_working;  /* the name of the level resource eg. "MAP01" */

static sidedef_t *p_newsidedef;       /* the new sidedefs */
static linedef_t *p_newlinedef;       /* the new linedefs */
static size_t p_newsidedef_count = 0; /* the new number of sidedefs */
static size_t p_newsidedef_size;      /* allocated size of p_newsidedef buffer*/
static int *p_newsidedef_index;       /* maps old sidedef number to new */

uint8_t *p_linedefres = 0; /* the new linedef resource */
uint8_t *p_sidedefres = 0; /* the new sidedef resource */

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
static uint16_t *b_blockmap;
static size_t b_blockmap_len;
static unsigned int b_num_blocks;
static uint16_t *b_newblockmap;
static size_t b_newblockmap_len;
static size_t b_newblockmap_size;
static struct block *b_blocklist;

/* Pack a level */

/* Call P_Pack() with the level name eg. p_pack("MAP01");. p_pack will then */
/* pack that level. The new sidedef and linedef lumps are pointed to by */
/* p_sidedefres and p_linedefres. These must be free()d by other functions */
/* when they are no longer needed, as P_Pack does not do this. */

void P_Pack(char *levelname)
{
    sidedef_t *sidedefs;
    linedef_t *linedefs;

    p_working = levelname;

    P_FindInfo();

    P_Rebuild(); /* remove unused sidedefs */

    sidedefs = p_newsidedef;
    linedefs = p_newlinedef;

    P_FindInfo(); /* find what's needed */

    P_DoPack(sidedefs); /* pack the sidedefs in memory */

    P_BuildLinedefs(linedefs); /* update linedefs + save to *??res */

    /* saving of the wad directory is left to external sources */

    /* point p_linedefres and p_sidedefres at the new lumps */
    p_linedefres = (uint8_t *) p_newlinedef;
    p_sidedefres = (uint8_t *) p_newsidedef;

    wadentry[p_sidedefnum].length = p_newsidedef_count * SDEF_SIZE;
}

/* Unpack a level */

/* Same thing, in reverse. Saves the new sidedef and linedef lumps to */
/* p_sidedefres and p_linedefres. */

void P_Unpack(char *resname)
{
    p_working = resname;

    P_FindInfo();
    P_Rebuild();

    p_linedefres = (uint8_t *) p_newlinedef;
    p_sidedefres = (uint8_t *) p_newsidedef;
}

/* Sanity check a linedef's sidedef reference is valid */
static void CheckSidedefIndex(int linedef_index, int sidedef_index)
{
    if (sidedef_index != NO_SIDEDEF &&
        (sidedef_index < 0 || sidedef_index >= p_num_sidedefs))
    {
        ErrorExit("Linedef #%d contained invalid sidedef reference %d",
                  linedef_index, sidedef_index);
    }
}

/* Find if a level is packed */

bool P_IsPacked(char *s)
{
    linedef_t *linedefs;
    uint8_t *sidedef_used;
    bool packed = false;
    int count;

    p_working = s;

    P_FindInfo();

    linedefs = ReadLinedefs(p_linedefnum, wadfp);

    sidedef_used = ALLOC_ARRAY(uint8_t, p_num_sidedefs);
    for (count = 0; count < p_num_sidedefs; count++)
        sidedef_used[count] = 0;

    for (count = 0; count < p_num_linedefs; count++) /* now check */
    {
        if (linedefs[count].sidedef1 != NO_SIDEDEF)
        {
            CheckSidedefIndex(count, linedefs[count].sidedef1);
            if (sidedef_used[linedefs[count].sidedef1])
            {
                /* side already used on a previous linedef */
                packed = true;
                break;
            }
            /* mark it as used for later reference */
            sidedef_used[linedefs[count].sidedef1] = 1;
        }
        if (linedefs[count].sidedef2 != NO_SIDEDEF)
        {
            CheckSidedefIndex(count, linedefs[count].sidedef2);
            if (sidedef_used[linedefs[count].sidedef2])
            {
                packed = true;
                break;
            }
            sidedef_used[linedefs[count].sidedef2] = 1;
        }
    }
    free(linedefs);
    free(sidedef_used);
    return packed;
}

/* Find necessary stuff before processing */

static void P_FindInfo(void)
{
    int count, n;

    /* first find the level entry */
    for (count = 0; count < numentries; count++)
    {
        if (!strncmp(wadentry[count].name, p_working, 8))
        { /* matches the name given */
            p_levelnum = count;
            break;
        }
    }
    if (count == numentries)
        ErrorExit("P_FindInfo: Couldn't find level: %.8s", p_working);

    n = 0;

    /* now find the sidedefs */
    for (count = p_levelnum + 1; count < numentries; count++)
    {
        if (!IsLevelEntry(wadentry[count].name))
        {
            ErrorExit("P_FindInfo: Can't find sidedef/linedef entries!");
        }

        if (!strncmp(wadentry[count].name, "SIDEDEFS", 8))
        {
            n++;
            p_sidedefnum = count;
        }
        if (!strncmp(wadentry[count].name, "LINEDEFS", 8))
        {
            n++;
            p_linedefnum = count;
        }
        if (n == 2)
            break; /* found both :) */
    }
    /* find number of linedefs and sidedefs for later.. */
    p_num_linedefs = wadentry[p_linedefnum].length / LDEF_SIZE;
    p_num_sidedefs = wadentry[p_sidedefnum].length / SDEF_SIZE;
}

/* Append the given sidedef to the p_newsidedef array. */
static int AppendNewSidedef(const sidedef_t *s)
{
    int result;

    while (p_newsidedef_count >= p_newsidedef_size)
    {
        p_newsidedef_size *= 2;
        p_newsidedef =
            REALLOC_ARRAY(sidedef_t, p_newsidedef, p_newsidedef_size);
    }

    memcpy(&p_newsidedef[p_newsidedef_count], s, sizeof(sidedef_t));
    result = p_newsidedef_count;
    ++p_newsidedef_count;

    return result;
}

// Check the p_newsidedef array and try to find a sidedef that is the
// same as the given sidedef. Returns -1 if none is found.
static int FindNewSidedef(const sidedef_t *sidedef)
{
    int i;

    if (sidedef->special)
    {
        return -1;
    }

    for (i = 0; i < p_newsidedef_count; i++)
    {
        if (!p_newsidedef[i].special &&
            !memcmp(&p_newsidedef[i], sidedef, sizeof(sidedef_t)))
        {
            return i;
        }
    }

    return -1;
}

/* Actually pack the sidedefs */

static void P_DoPack(sidedef_t *sidedefs)
{
    int count, ns;

    p_newsidedef_size = p_num_sidedefs;
    p_newsidedef = ALLOC_ARRAY(sidedef_t, p_newsidedef_size);
    p_newsidedef_count = 0;
    p_newsidedef_index = ALLOC_ARRAY(int, p_num_sidedefs);

    for (count = 0; count < p_num_sidedefs; count++)
    {
        if ((count % 100) == 0)
        {
            /* time for a percent-done update */
            PrintProgress(count, p_num_sidedefs);
        }

        ns = FindNewSidedef(&sidedefs[count]);
        if (ns < p_newsidedef_count)
        {
            p_newsidedef_index[count] = ns;
        }
        else
        {
            /* a sidedef like this does not yet exist: add one */
            p_newsidedef_index[count] = AppendNewSidedef(&sidedefs[count]);
        }
    }
    free(sidedefs);
}

/* Update the linedefs and save sidedefs */

static void P_BuildLinedefs(linedef_t *linedefs)
{
    int count;

    /* update the linedefs with the new sidedef indexes. note that we
       do not need to perform the CheckSidedefIndex() checks here
       because they have already been checked by a previous call to
       P_Rebuild. */

    for (count = 0; count < p_num_linedefs; count++)
    {
        if (linedefs[count].sidedef1 != NO_SIDEDEF)
            linedefs[count].sidedef1 =
                p_newsidedef_index[linedefs[count].sidedef1];
        if (linedefs[count].sidedef2 != NO_SIDEDEF)
            linedefs[count].sidedef2 =
                p_newsidedef_index[linedefs[count].sidedef2];
    }

    p_newlinedef = linedefs;

    free(p_newsidedef_index);
}

static void CheckLumpSizes(void)
{
    if ((wadentry[p_linedefnum].length % LDEF_SIZE) != 0)
    {
        ErrorExit("P_FindInfo: %.8s linedef lump is %d bytes, "
                  "not a multiple of %d",
                  p_working, wadentry[p_linedefnum].length, LDEF_SIZE);
    }
    if ((wadentry[p_sidedefnum].length % SDEF_SIZE) != 0)
    {
        ErrorExit("P_FindInfo: %.8s sidedef lump is %d bytes, "
                  "not a multiple of %d",
                  p_working, wadentry[p_sidedefnum].length, SDEF_SIZE);
    }
}

/* Rebuild the sidedefs */

static void P_Rebuild(void)
{
    sidedef_t *sidedefs;
    linedef_t *linedefs, *ld;
    bool is_special;
    int count;

    CheckLumpSizes();

    sidedefs = ReadSidedefs(p_sidedefnum, wadfp);
    linedefs = ReadLinedefs(p_linedefnum, wadfp);

    p_newsidedef_size = p_num_sidedefs;
    p_newsidedef = ALLOC_ARRAY(sidedef_t, p_newsidedef_size);
    p_newsidedef_count = 0;

    for (count = 0; count < p_num_linedefs; count++)
    {
        ld = &linedefs[count];
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
        if (linedefs[count].sidedef1 != NO_SIDEDEF)
        {
            CheckSidedefIndex(count, linedefs[count].sidedef1);
            linedefs[count].sidedef1 =
                AppendNewSidedef(&sidedefs[linedefs[count].sidedef1]);
            p_newsidedef[linedefs[count].sidedef1].special = is_special;
        }
        if (linedefs[count].sidedef2 != NO_SIDEDEF)
        {
            CheckSidedefIndex(count, linedefs[count].sidedef2);
            linedefs[count].sidedef2 =
                AppendNewSidedef(&sidedefs[linedefs[count].sidedef2]);
            p_newsidedef[linedefs[count].sidedef2].special = is_special;
        }
    }
    /* update the wad directory */
    wadentry[p_sidedefnum].length = p_newsidedef_count * SDEF_SIZE;

    /* no longer need the old sidedefs */
    free(sidedefs);
    /* still need the old linedefs: they have been updated */
    p_newlinedef = linedefs;
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

static void MakeBlocklist(void)
{
    struct block *b_blocklist;
    int i, start_index, end_index;
    bool engine_format = false;

    b_blocklist = ALLOC_ARRAY(struct block, b_num_blocks);

    for (i = 0; i < b_num_blocks; i++)
    {
        start_index = b_blockmap[4 + i];
        b_blocklist[i].elements = &b_blockmap[start_index];

        end_index = start_index;
        while (b_blockmap[end_index] != 0xffff)
        {
            ++end_index;
        }
        b_blocklist[i].len = end_index - start_index;

        engine_format = engine_format || b_blockmap[start_index] != 0;
    }

    // Convert to "engine format" by stripping leading zeroes:
    // https://doomwiki.org/wiki/Blockmap#Blocklists
    if (!engine_format)
    {
        for (i = 0; i < b_num_blocks; i++)
        {
            ++b_blocklist[i].elements;
            --b_blocklist[i].len;
        }
    }

}

static void AppendBlockmapElements(uint16_t *elements, size_t count)
{
    while (b_newblockmap_len + count > b_newblockmap_size)
    {
        b_newblockmap_size *= 2;
        b_newblockmap = REALLOC_ARRAY(uint16_t, b_newblockmap,
                                      b_newblockmap_size);
    }

    memcpy(&b_newblockmap[b_newblockmap_len], elements,
           count * sizeof(uint16_t));
    b_newblockmap_len += count;
}

// TODO: This is not yet finished.
void B_Stack(int lumpnum, FILE *fp)
{
    uint16_t *block_offsets;
    int i, j;

    if (!ReadBlockmap(lumpnum, fp))
    {
        return;
    }

    MakeBlocklist();

    b_newblockmap_size = b_blockmap_len;
    b_newblockmap = ALLOC_ARRAY(uint16_t, b_newblockmap_size);
    b_newblockmap_len = 4 + b_num_blocks;
    block_offsets = &b_newblockmap[4];

    // Header is identical:
    memcpy(b_newblockmap, b_blockmap, 4 * sizeof(uint16_t));

    for (i = 0; i < b_newblockmap_len; i++)
    {
        for (j = 0; j < i; j++)
        {
            uint16_t suffix_index;

            // We allow suffixes.
            if (b_blocklist[i].len > b_blocklist[j].len)
            {
                continue;
            }

            suffix_index = b_blocklist[j].len - b_blocklist[i].len;

            // Same elements? They can use the same block data.
            if (!memcmp(b_blocklist[i].elements,
                        b_blocklist[j].elements + suffix_index,
                        b_blocklist[i].len * sizeof(uint16_t)))
            {
                block_offsets[i] = block_offsets[j] + suffix_index;
                break;
            }
        }

        // Not found. Add new elements.
        if (j >= i)
        {
            block_offsets[i] = b_newblockmap_len;
            AppendBlockmapElements(b_blocklist[i].elements,
                                   b_blocklist[i].len + 1);
        }
    }
}

/*
 *  portable reading / writing of linedefs and sidedefs
 *  by Andreas Dehmel (dehmel@forwiss.tu-muenchen.de)
 */

static const int convbuffsize = 0x8000;
static uint8_t convbuffer[0x8000];

static linedef_t *ReadLinedefs(int lumpnum, FILE *fp)
{
    linedef_t *lines;
    int i, numlines, validbytes;
    uint8_t *cptr;

    numlines = wadentry[lumpnum].length / LDEF_SIZE;
    lines = ALLOC_ARRAY(linedef_t, numlines);
    fseek(fp, wadentry[lumpnum].offset, SEEK_SET);
    validbytes = 0;
    cptr = convbuffer;
    for (i = 0; i < numlines; i++)
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
        lines[i].vertex1 = READ_SHORT(cptr + LDEF_VERT1);
        lines[i].vertex2 = READ_SHORT(cptr + LDEF_VERT2);
        lines[i].flags = READ_SHORT(cptr + LDEF_FLAGS);
        lines[i].type = READ_SHORT(cptr + LDEF_TYPES);
        lines[i].tag = READ_SHORT(cptr + LDEF_TAG);
        lines[i].sidedef1 = READ_SHORT(cptr + LDEF_SDEF1);
        lines[i].sidedef2 = READ_SHORT(cptr + LDEF_SDEF2);
        cptr += LDEF_SIZE;
        validbytes -= LDEF_SIZE;
    }
    return lines;
}

int WriteLinedefs(linedef_t *lines, int bytes, FILE *fp)
{
    int i;
    uint8_t *cptr;

    cptr = convbuffer;
    for (i = 0; bytes > 0; i++)
    {
        if (cptr - convbuffer > convbuffsize - LDEF_SIZE)
        {
            fwrite(convbuffer, 1, cptr - convbuffer, fp);
            cptr = convbuffer;
        }
        WRITE_SHORT(cptr + LDEF_VERT1, lines[i].vertex1);
        WRITE_SHORT(cptr + LDEF_VERT2, lines[i].vertex2);
        WRITE_SHORT(cptr + LDEF_FLAGS, lines[i].flags);
        WRITE_SHORT(cptr + LDEF_TYPES, lines[i].type);
        WRITE_SHORT(cptr + LDEF_TAG, lines[i].tag);
        WRITE_SHORT(cptr + LDEF_SDEF1, lines[i].sidedef1);
        WRITE_SHORT(cptr + LDEF_SDEF2, lines[i].sidedef2);
        cptr += LDEF_SIZE;
        bytes -= LDEF_SIZE;
    }
    if (cptr != convbuffer)
    {
        fwrite(convbuffer, 1, cptr - convbuffer, fp);
    }
    return 0;
}

static sidedef_t *ReadSidedefs(int lumpnum, FILE *fp)
{
    sidedef_t *sides;
    int i, numsides, validbytes;
    uint8_t *cptr;

    numsides = wadentry[lumpnum].length / SDEF_SIZE;
    sides = ALLOC_ARRAY(sidedef_t, numsides);
    fseek(fp, wadentry[lumpnum].offset, SEEK_SET);
    validbytes = 0;
    cptr = convbuffer;
    for (i = 0; i < numsides; i++)
    {
        if (validbytes < SDEF_SIZE)
        {
            if (validbytes != 0)
                memcpy(convbuffer, cptr, validbytes);
            validbytes += fread(convbuffer + validbytes, 1,
                                convbuffsize - validbytes, fp);
            cptr = convbuffer;
        }
        sides[i].xoffset = READ_SHORT(cptr + SDEF_XOFF);
        sides[i].yoffset = READ_SHORT(cptr + SDEF_YOFF);
        memset(sides[i].upper, 0, 8);
        strncpy(sides[i].upper, (char *) cptr + SDEF_UPPER, 8);
        memset(sides[i].middle, 0, 8);
        strncpy(sides[i].middle, (char *) cptr + SDEF_MIDDLE, 8);
        memset(sides[i].lower, 0, 8);
        strncpy(sides[i].lower, (char *) cptr + SDEF_LOWER, 8);
        sides[i].sector_ref = READ_SHORT(cptr + SDEF_SECTOR);
        sides[i].special = false;
        cptr += SDEF_SIZE;
        validbytes -= SDEF_SIZE;
    }
    return sides;
}

int WriteSidedefs(sidedef_t *sides, int bytes, FILE *fp)
{
    int i;
    uint8_t *cptr;

    cptr = convbuffer;
    for (i = 0; bytes > 0; i++)
    {
        if (cptr - convbuffer > convbuffsize - SDEF_SIZE)
        {
            fwrite(convbuffer, 1, cptr - convbuffer, fp);
            cptr = convbuffer;
        }
        WRITE_SHORT(cptr + SDEF_XOFF, sides[i].xoffset);
        WRITE_SHORT(cptr + SDEF_YOFF, sides[i].yoffset);
        memset(cptr + SDEF_UPPER, 0, 8);
        strncpy((char *) cptr + SDEF_UPPER, sides[i].upper, 8);
        memset(cptr + SDEF_MIDDLE, 0, 8);
        strncpy((char *) cptr + SDEF_MIDDLE, sides[i].middle, 8);
        memset(cptr + SDEF_LOWER, 0, 8);
        strncpy((char *) cptr + SDEF_LOWER, sides[i].lower, 8);
        WRITE_SHORT(cptr + SDEF_SECTOR, sides[i].sector_ref);
        cptr += SDEF_SIZE;
        bytes -= SDEF_SIZE;
    }
    if (cptr != convbuffer)
    {
        fwrite(convbuffer, 1, cptr - convbuffer, fp);
    }
    return 0;
}

static bool ReadBlockmap(int lumpnum, FILE *fp)
{
    int i;

    b_blockmap_len = wadentry[lumpnum].length / sizeof(uint16_t);
    if (b_blockmap_len < 4)
    {
        return false;
    }
    b_blockmap = ALLOC_ARRAY(uint16_t, b_blockmap_len);
    fseek(fp, wadentry[lumpnum].offset, SEEK_SET);
    if (fread(b_blockmap, sizeof(uint16_t),
              b_blockmap_len, fp) != b_blockmap_len)
    {
        free(b_blockmap);
        return false;
    }

    for (i = 0; i < b_blockmap_len; i++)
    {
        b_blockmap[i] = READ_SHORT((uint8_t *) &b_blockmap[i]);
    }

    b_num_blocks = b_blockmap[2] * b_blockmap[3];
    if (b_blockmap_len < b_num_blocks + 4)
    {
        free(b_blockmap);
        return false;
    }

    return true;
}
