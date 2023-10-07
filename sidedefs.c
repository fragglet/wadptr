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
 * Sidedef packing extension routines. Combines sidedefs which are
 * identical in a level, and shares them between multiple linedefs.
 */

#include <stdint.h>
#include <string.h>

#include "sidedefs.h"
#include "wadptr.h"

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

static linedef_array_t ReadLinedefs(int lumpnum, FILE *fp);
static sidedef_array_t ReadSidedefs(int lumpnum, FILE *fp);
static int WriteLinedefs(const linedef_array_t *linedefs, FILE *fp);
static int WriteSidedefs(const sidedef_array_t *sidedefs, FILE *fp);

static int p_sidedefnum; /* sidedef wad entry number */
static int p_linedefnum; /* linedef wad entry number */

static linedef_array_t p_linedefs_result;
static sidedef_array_t p_sidedefs_result;

static int *p_newsidedef_index; /* maps old sidedef number to new */

/* Call P_Pack() with the level name eg. p_pack("MAP01"); P_Pack will then
   pack that level. The new sidedef and linedef lumps are pointed to by
   p_sidedefres and p_linedefres. These must be free()d by other functions
   when they are no longer needed, as P_Pack does not do this. */
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

/* Same thing, in reverse. Saves the new sidedef and linedef lumps to
   p_sidedefres and p_linedefres. */
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

    // TODO: We should catch the case where unpacking would exceed the
    // vanilla SIDEDEFS limit.
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

