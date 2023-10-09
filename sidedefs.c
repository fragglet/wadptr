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
#include <stdlib.h>
#include <string.h>

#include "errors.h"
#include "sidedefs.h"
#include "sort.h"
#include "waddir.h"
#include "wadptr.h"

#define NO_SIDEDEF ((sidedef_ref_t) UINT32_MAX)

/*
 * Portable structure IO
 * (to handle endianness; also neither struct is a multiple of 4 in size)
 */
#define SDEF_XOFF   0
#define SDEF_YOFF   2
#define SDEF_UPPER  4
#define SDEF_MIDDLE 12
#define SDEF_LOWER  20
#define SDEF_SECTOR 28
#define SDEF_SIZE   30

#define LDEF_VERT1 0
#define LDEF_VERT2 2
#define LDEF_FLAGS 4
#define LDEF_TYPES 6
#define LDEF_TAG   8
#define LDEF_SDEF1 10
#define LDEF_SDEF2 12
#define LDEF_SIZE  14

// On disk, these are 16-bit integers. But while processing, we unpack
// the sidedefs first, and the unpacked sidedefs might exceed the 16-bit
// limit. So in memory we use a 32-bit integer even though when we write
// the linedefs out again they will of course be 16-bit.
typedef uint32_t sidedef_ref_t;

typedef struct {
    short xoffset;
    short yoffset;
    char upper[8];
    char middle[8];
    char lower[8];
    unsigned short sector_ref;

    // If true, this sidedef is referenced by a linedef with a special
    // type. This fixes the "scrolling linedefs bug" most notably,
    // although switches are also potentially affected.
    bool special;
} sidedef_t;

typedef struct {
    unsigned short vertex1;
    unsigned short vertex2;
    unsigned short flags;
    unsigned short type;
    unsigned short tag;

    sidedef_ref_t sidedef1;
    sidedef_ref_t sidedef2;
} linedef_t;

typedef struct {
    linedef_t *lines;
    size_t len, size;
} linedef_array_t;

typedef struct {
    sidedef_t *sides;
    size_t len, size;
} sidedef_array_t;

static void CheckLumpSizes(void);
static sidedef_array_t DoPack(const sidedef_array_t *sidedefs);
static void RemapLinedefs(linedef_array_t *linedefs);
static sidedef_array_t RebuildSidedefs(linedef_array_t *linedefs,
                                       const sidedef_array_t *sidedefs);

static linedef_array_t ReadLinedefs(int lumpnum, FILE *fp);
static sidedef_array_t ReadSidedefs(int lumpnum, FILE *fp);
static int WriteLinedefs(const linedef_array_t *linedefs, FILE *fp);
static int WriteSidedefs(const sidedef_array_t *sidedefs, FILE *fp);

static int sidedefnum; /* sidedef wad entry number */
static int linedefnum; /* linedef wad entry number */

static linedef_array_t linedefs_result;
static sidedef_array_t sidedefs_result;

static sidedef_ref_t *newsidedef_index; /* maps old sidedef number to new */

/* Call P_Pack() with the level name eg. pack("MAP01"); P_Pack will then
   pack that level. The new sidedef and linedef lumps are pointed to by
   sidedefres and linedefres. These must be free()d by other functions
   when they are no longer needed, as P_Pack does not do this. */
void P_Pack(int sidedef_num)
{
    sidedef_array_t sidedefs, sidedefs2;

    linedefnum = sidedef_num - 1;
    sidedefnum = sidedef_num;

    CheckLumpSizes();

    sidedefs = ReadSidedefs(sidedefnum, wadfp);
    linedefs_result = ReadLinedefs(linedefnum, wadfp);

    sidedefs2 = RebuildSidedefs(&linedefs_result, &sidedefs);
    free(sidedefs.sides);

    sidedefs_result = DoPack(&sidedefs2);
    free(sidedefs2.sides);

    RemapLinedefs(&linedefs_result); /* update sidedef indexes */
}

size_t P_WriteLinedefs(FILE *fstream)
{
    WriteLinedefs(&linedefs_result, fstream);
    free(linedefs_result.lines);
    return linedefs_result.len * LDEF_SIZE;
}

size_t P_WriteSidedefs(FILE *fstream)
{
    WriteSidedefs(&sidedefs_result, fstream);
    free(sidedefs_result.sides);
    return sidedefs_result.len * SDEF_SIZE;
}

/* Same thing, in reverse. Saves the new sidedef and linedef lumps to
   sidedefres and linedefres. */
void P_Unpack(int sidedef_num)
{
    sidedef_array_t sidedefs;

    linedefnum = sidedef_num - 1;
    sidedefnum = sidedef_num;

    CheckLumpSizes();

    linedefs_result = ReadLinedefs(linedefnum, wadfp);

    sidedefs = ReadSidedefs(sidedefnum, wadfp);
    sidedefs_result = RebuildSidedefs(&linedefs_result, &sidedefs);
    free(sidedefs.sides);

    // TODO: We should catch the case where unpacking would exceed the
    // vanilla SIDEDEFS limit.
}

/* Sanity check a linedef's sidedef reference is valid */
static void CheckSidedefIndex(int linedef_index, sidedef_ref_t sidedef_index,
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
    size_t num_sidedefs;
    bool packed = false;
    int count;

    // SIDEDEFS always follows LINEDEFS.
    linedefnum = sidedef_num - 1;
    sidedefnum = sidedef_num;

    linedefs = ReadLinedefs(linedefnum, wadfp);

    num_sidedefs = wadentry[sidedefnum].length / SDEF_SIZE;
    sidedef_used = ALLOC_ARRAY(uint8_t, num_sidedefs);
    for (count = 0; count < num_sidedefs; count++)
    {
        sidedef_used[count] = 0;
    }

    for (count = 0; count < linedefs.len; count++) /* now check */
    {
        if (linedefs.lines[count].sidedef1 != NO_SIDEDEF)
        {
            sidedef_ref_t sdi = linedefs.lines[count].sidedef1;
            CheckSidedefIndex(count, sdi, num_sidedefs);
            packed = packed || sidedef_used[sdi];
            sidedef_used[sdi] = 1;
        }
        if (linedefs.lines[count].sidedef2 != NO_SIDEDEF)
        {
            sidedef_ref_t sdi = linedefs.lines[count].sidedef2;
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
static sidedef_ref_t AppendNewSidedef(sidedef_array_t *sidedefs,
                                      const sidedef_t *s)
{
    sidedef_ref_t result;

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

static int CompareSidedefs(const sidedef_t *s1, const sidedef_t *s2)
{
#define CHECK_DIFF(expr) \
    { int diff = expr; if (diff != 0) { return diff; } }

    CHECK_DIFF(strncmp(s1->middle, s2->middle, 8));
    CHECK_DIFF(strncmp(s1->upper, s2->upper, 8));
    CHECK_DIFF(strncmp(s1->lower, s2->lower, 8));
    CHECK_DIFF(s2->special - s1->special);
    CHECK_DIFF(s2->xoffset - s1->xoffset);
    CHECK_DIFF(s2->yoffset - s1->yoffset);
    CHECK_DIFF(s2->sector_ref - s1->sector_ref);
#undef CHECK_DIFF

    return 0;
}

static int CompareFunc(unsigned int index1, unsigned int index2,
                       const void *callback_data)
{
    const sidedef_array_t *sidedefs = callback_data;
    return CompareSidedefs(&sidedefs->sides[index1],
                           &sidedefs->sides[index2]);
}

#ifdef DEBUG
static void PrintSidedef(const sidedef_t *s)
{
    printf("x: %5d y: %5d s: %7d m: %-8.8s l: %-8.8s: u: %-8.8s sp: %d\n",
           s->xoffset, s->yoffset, s->sector_ref, s->middle, s->upper,
           s->lower, s->special);
}
#endif

static sidedef_array_t DoPack(const sidedef_array_t *sidedefs)
{
    sidedef_array_t packed;
    const sidedef_t *sidedef, *prev_sidedef = NULL;
    unsigned int *map;
    unsigned int mi;
    sidedef_ref_t sdi;

    // To pack the sidedefs we first create a "sorted map": an array of
    // all the sidedef numbers, ordered by that sidedef's contents. That
    // is to say that `sidedefs->sides[map[0]]` <= all other sidedefs,
    // while `sidedefs->sides[map[sidedefs->len - 1]]` >= than all other
    // sidedefs. Producing the map is an O(n log n) process, which makes
    // this much more efficient than earlier versions of this code that
    // were O(n^2).
    map = MakeSortedMap(sidedefs->len, CompareFunc, sidedefs);

    packed.size = sidedefs->len;
    packed.sides = ALLOC_ARRAY(sidedef_t, packed.size);
    packed.len = 0;
    newsidedef_index = ALLOC_ARRAY(sidedef_ref_t, sidedefs->len);

    // Now we iterate over map[], ie. each sidedef in sorted order.
    // This means that we will encounter any identical sidedefs
    // consecutively. It is then a matter of comparing the current
    // sidedef with the sidedef from the previous iteration
    // (prev_sidedef) to determine whether we need to add a new sidedef
    // to the packed array, or whether we can just reuse the previous
    // one.
    for (mi = 0; mi < sidedefs->len; mi++)
    {
        if ((mi % 100) == 0)
        {
            PrintProgress(mi, sidedefs->len);
        }
        sdi = map[mi];
        sidedef = &sidedefs->sides[sdi];
#ifdef DEBUG
        PrintSidedef(sidedef);
#endif
        // Special sidedefs (those attached to special lines) never get merged.
        if (!sidedef->special && prev_sidedef != NULL && !prev_sidedef->special
         && CompareSidedefs(sidedef, prev_sidedef) == 0)
        {
            newsidedef_index[sdi] = newsidedef_index[map[mi - 1]];
        }
        else
        {
            newsidedef_index[sdi] = packed.len;
            AppendNewSidedef(&packed, sidedef);
        }
        prev_sidedef = sidedef;
    }

    free(map);

    return packed;
}

/* Update the linedefs and save sidedefs */
static void RemapLinedefs(linedef_array_t *linedefs)
{
    int count;

    /* update the linedefs with the new sidedef indexes. note that we
       do not need to perform the CheckSidedefIndex() checks here
       because they have already been checked by a previous call to
       Rebuild. */

    for (count = 0; count < linedefs->len; count++)
    {
        if (linedefs->lines[count].sidedef1 != NO_SIDEDEF)
            linedefs->lines[count].sidedef1 =
                newsidedef_index[linedefs->lines[count].sidedef1];
        if (linedefs->lines[count].sidedef2 != NO_SIDEDEF)
            linedefs->lines[count].sidedef2 =
                newsidedef_index[linedefs->lines[count].sidedef2];
    }

    free(newsidedef_index);
}

static void CheckLumpSizes(void)
{
    if ((wadentry[linedefnum].length % LDEF_SIZE) != 0)
    {
        ErrorExit("RebuildSidedefs: LINEDEFS lump (#%d) is %d bytes, "
                  "not a multiple of %d",
                  linedefnum, wadentry[linedefnum].length, LDEF_SIZE);
    }
    if ((wadentry[sidedefnum].length % SDEF_SIZE) != 0)
    {
        ErrorExit("RebuildSidedefs: SIDEDEFS lump (#%d) is %d bytes, "
                  "not a multiple of %d",
                  sidedefnum, wadentry[sidedefnum].length, SDEF_SIZE);
    }
}

static sidedef_array_t RebuildSidedefs(linedef_array_t *linedefs,
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

// Translation to handle the fact that our internal sidedef references
// are 32-bit integers.
static sidedef_ref_t MapSidedefRef(uint16_t val)
{
    if (val == 0xffff)
    {
        return NO_SIDEDEF;
    }

    return val;
}

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
        result.lines[i].sidedef1 =
            MapSidedefRef(READ_SHORT(cptr + LDEF_SDEF1));
        result.lines[i].sidedef2 =
            MapSidedefRef(READ_SHORT(cptr + LDEF_SDEF2));
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
        WRITE_SHORT(cptr + LDEF_SDEF1,
                    linedefs->lines[i].sidedef1 & 0xffff);
        WRITE_SHORT(cptr + LDEF_SDEF2,
                    linedefs->lines[i].sidedef2 & 0xffff);
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
