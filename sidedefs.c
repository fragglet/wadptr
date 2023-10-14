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

// Vanilla sidedefs count limit.
// TODO: Support the Boom extended format.
#define MAX_SIDEDEFS 0x7fff

#define NO_SIDEDEF ((sidedef_ref_t) UINT32_MAX)

/*
 * Portable structure IO
 * (to handle endianness; also neither struct is a multiple of 4 in size)
 */
#define SDEF_XOFF   0
#define SDEF_YOFF   2
#define SDEF_UPPER  4
#define SDEF_LOWER  12
#define SDEF_MIDDLE 20
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

// Hexen linedef format:
#define HX_LDEF_VERT1 0
#define HX_LDEF_VERT2 2
#define HX_LDEF_FLAGS 4
#define HX_LDEF_TYPES 6
#define HX_LDEF_ARGS  7
#define HX_LDEF_SDEF1 12
#define HX_LDEF_SDEF2 14
#define HX_LDEF_SIZE  16

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

    uint8_t args[5];  // Hexen format only

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
static void RebuildSidedefs(const linedef_array_t *linedefs,
                            const sidedef_array_t *sidedefs,
                            linedef_array_t *ldresult,
                            sidedef_array_t *sdresult);

static linedef_array_t ReadDoomLinedefs(int lumpnum);
static linedef_array_t ReadHexenLinedefs(int lumpnum);
static sidedef_array_t ReadSidedefs(int lumpnum);
static void WriteDoomLinedefs(const linedef_array_t *linedefs, FILE *fp);
static void WriteHexenLinedefs(const linedef_array_t *linedefs, FILE *fp);
static void WriteSidedefs(const sidedef_array_t *sidedefs, FILE *fp);

static int sidedefnum; /* sidedef wad entry number */
static int linedefnum; /* linedef wad entry number */

static linedef_array_t linedefs_result;
static sidedef_array_t sidedefs_result;

static sidedef_ref_t *newsidedef_index; /* maps old sidedef number to new */

static size_t LinedefSize(void)
{
    if (hexen_format_wad)
    {
        return HX_LDEF_SIZE;
    }
    return LDEF_SIZE;
}

/* Call P_Pack() with the level name eg. pack("MAP01"); P_Pack will then
   pack that level. The new sidedef and linedef lumps are pointed to by
   sidedefres and linedefres. These must be free()d by other functions
   when they are no longer needed, as P_Pack does not do this. */
bool P_Pack(int sidedef_num)
{
    sidedef_array_t orig_sidedefs, unpacked_sidedefs;
    linedef_array_t orig_linedefs;

    linedefnum = sidedef_num - 1;
    sidedefnum = sidedef_num;

    CheckLumpSizes();

    orig_sidedefs = ReadSidedefs(sidedefnum);
    orig_linedefs = ReadDoomLinedefs(linedefnum);

    RebuildSidedefs(&orig_linedefs, &orig_sidedefs, &linedefs_result,
                    &unpacked_sidedefs);

    sidedefs_result = DoPack(&unpacked_sidedefs);
    free(unpacked_sidedefs.sides);

    // TODO: Check that the SIDEDEFS lump is never larger than the
    // original one?

    // We never generate a corrupt (overflowed) SIDEDEFS list.
    if (sidedefs_result.len > MAX_SIDEDEFS)
    {
        free(sidedefs_result.sides);
        sidedefs_result = orig_sidedefs;
        free(linedefs_result.lines);
        linedefs_result = orig_linedefs;
        return false;
    }

    free(orig_sidedefs.sides);
    free(orig_linedefs.lines);
    RemapLinedefs(&linedefs_result); /* update sidedef indexes */
    return true;
}

size_t P_WriteLinedefs(FILE *fstream)
{
    WriteDoomLinedefs(&linedefs_result, fstream);
    free(linedefs_result.lines);
    return linedefs_result.len * LinedefSize();
}

size_t P_WriteSidedefs(FILE *fstream)
{
    WriteSidedefs(&sidedefs_result, fstream);
    free(sidedefs_result.sides);
    return sidedefs_result.len * SDEF_SIZE;
}

/* Same thing, in reverse. Saves the new sidedef and linedef lumps to
   sidedefres and linedefres. */
bool P_Unpack(int sidedef_num)
{
    linedef_array_t orig_linedefs;
    sidedef_array_t orig_sidedefs;

    linedefnum = sidedef_num - 1;
    sidedefnum = sidedef_num;

    CheckLumpSizes();

    orig_linedefs = ReadDoomLinedefs(linedefnum);
    orig_sidedefs = ReadSidedefs(sidedefnum);

    RebuildSidedefs(&orig_linedefs, &orig_sidedefs, &linedefs_result,
                    &sidedefs_result);

    // It is possible that the decompressed sidedefs list overflows the
    // limits of the SIDEDEFS on-disk format. We never want to save a
    // corrupted sidedefs list.
    // TODO: Some source ports (eg. Boom compatible) support up to 64K
    // sidedefs, and we should support this as an option.
    if (sidedefs_result.len > MAX_SIDEDEFS)
    {
        free(sidedefs_result.sides);
        sidedefs_result = orig_sidedefs;
        free(linedefs_result.lines);
        linedefs_result = orig_linedefs;
        return false;
    }

    free(orig_sidedefs.sides);
    free(orig_linedefs.lines);
    return true;
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

    linedefs = ReadDoomLinedefs(linedefnum);

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
    {                    \
        int diff = expr; \
        if (diff != 0)   \
        {                \
            return diff; \
        }                \
    }

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
    return CompareSidedefs(&sidedefs->sides[index1], &sidedefs->sides[index2]);
}

#ifdef DEBUG
static void PrintLinedef(const linedef_t *l)
{
    printf("v1: %5d v2: %5d f: %5x t: %5d s1: %5d s2: %5d\n", l->vertex1,
           l->vertex2, l->flags, l->type, l->sidedef1, l->sidedef2);
}

static void PrintSidedef(const sidedef_t *s)
{
    printf("x: %5d y: %5d s: %7d m: %-8.8s l: %-8.8s: u: %-8.8s sp: %d\n",
           s->xoffset, s->yoffset, s->sector_ref, s->middle, s->upper, s->lower,
           s->special);
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
        if (!sidedef->special && prev_sidedef != NULL &&
            !prev_sidedef->special &&
            CompareSidedefs(sidedef, prev_sidedef) == 0)
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
#ifdef DEBUG
        PrintLinedef(&linedefs->lines[count]);
#endif
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
    unsigned int linedef_size = LinedefSize();
    if ((wadentry[linedefnum].length % linedef_size) != 0)
    {
        ErrorExit("RebuildSidedefs: LINEDEFS lump (#%d) is %d bytes, "
                  "not a multiple of %d",
                  linedefnum, wadentry[linedefnum].length, linedef_size);
    }
    if ((wadentry[sidedefnum].length % SDEF_SIZE) != 0)
    {
        ErrorExit("RebuildSidedefs: SIDEDEFS lump (#%d) is %d bytes, "
                  "not a multiple of %d",
                  sidedefnum, wadentry[sidedefnum].length, SDEF_SIZE);
    }
}

static void RebuildSidedefs(const linedef_array_t *linedefs,
                            const sidedef_array_t *sidedefs,
                            linedef_array_t *ldresult,
                            sidedef_array_t *sdresult)
{
    bool is_special;
    int count;

    ldresult->len = linedefs->len;
    ldresult->lines = ALLOC_ARRAY(linedef_t, ldresult->len);
    memcpy(ldresult->lines, linedefs->lines, sizeof(linedef_t) * linedefs->len);

    sdresult->size = sidedefs->len;
    sdresult->sides = ALLOC_ARRAY(sidedef_t, sdresult->size);
    sdresult->len = 0;

    for (count = 0; count < linedefs->len; count++)
    {
        linedef_t *ld = &ldresult->lines[count];
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
        if (ld->sidedef1 != NO_SIDEDEF)
        {
            CheckSidedefIndex(count, ld->sidedef1, sidedefs->len);
            ld->sidedef1 =
                AppendNewSidedef(sdresult, &sidedefs->sides[ld->sidedef1]);
            sdresult->sides[ld->sidedef1].special = is_special;
        }
        if (ld->sidedef2 != NO_SIDEDEF)
        {
            CheckSidedefIndex(count, ld->sidedef2, sidedefs->len);
            ld->sidedef2 =
                AppendNewSidedef(sdresult, &sidedefs->sides[ld->sidedef2]);
            sdresult->sides[ld->sidedef2].special = is_special;
        }
    }
}

/*
 *  portable reading / writing of linedefs and sidedefs
 *  by Andreas Dehmel (dehmel@forwiss.tu-muenchen.de)
 */

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

static linedef_array_t ReadDoomLinedefs(int lumpnum)
{
    linedef_array_t result;
    uint8_t *cptr, *lump;
    int i;

    result.len = wadentry[lumpnum].length / LDEF_SIZE;
    result.lines = ALLOC_ARRAY(linedef_t, result.len);
    lump = CacheLump(lumpnum);
    cptr = lump;
    for (i = 0; i < result.len; i++)
    {
        result.lines[i].vertex1 = READ_SHORT(cptr + LDEF_VERT1);
        result.lines[i].vertex2 = READ_SHORT(cptr + LDEF_VERT2);
        result.lines[i].flags = READ_SHORT(cptr + LDEF_FLAGS);
        result.lines[i].type = READ_SHORT(cptr + LDEF_TYPES);
        result.lines[i].tag = READ_SHORT(cptr + LDEF_TAG);
        result.lines[i].sidedef1 = MapSidedefRef(READ_SHORT(cptr + LDEF_SDEF1));
        result.lines[i].sidedef2 = MapSidedefRef(READ_SHORT(cptr + LDEF_SDEF2));
        cptr += LDEF_SIZE;
    }
    free(lump);
    return result;
}

static void WriteDoomLinedefs(const linedef_array_t *linedefs, FILE *fp)
{
    uint8_t convbuffer[LDEF_SIZE];
    int i;

    for (i = 0; i < linedefs->len; i++)
    {
        WRITE_SHORT(convbuffer + LDEF_VERT1, linedefs->lines[i].vertex1);
        WRITE_SHORT(convbuffer + LDEF_VERT2, linedefs->lines[i].vertex2);
        WRITE_SHORT(convbuffer + LDEF_FLAGS, linedefs->lines[i].flags);
        WRITE_SHORT(convbuffer + LDEF_TYPES, linedefs->lines[i].type);
        WRITE_SHORT(convbuffer + LDEF_TAG, linedefs->lines[i].tag);
        WRITE_SHORT(convbuffer + LDEF_SDEF1,
                    linedefs->lines[i].sidedef1 & 0xffff);
        WRITE_SHORT(convbuffer + LDEF_SDEF2,
                    linedefs->lines[i].sidedef2 & 0xffff);
        if (fwrite(convbuffer, LDEF_SIZE, 1, fp) != 1)
        {
            perror("fwrite");
            ErrorExit("Failed writing linedef #%d to output file", i);
        }
    }
}

static linedef_array_t ReadHexenLinedefs(int lumpnum)
{
    linedef_array_t result;
    uint8_t *cptr, *lump;
    int i;

    result.len = wadentry[lumpnum].length / HX_LDEF_SIZE;
    result.lines = ALLOC_ARRAY(linedef_t, result.len);
    lump = CacheLump(lumpnum);
    cptr = lump;
    for (i = 0; i < result.len; i++)
    {
        result.lines[i].vertex1 = READ_SHORT(cptr + HX_LDEF_VERT1);
        result.lines[i].vertex2 = READ_SHORT(cptr + HX_LDEF_VERT2);
        result.lines[i].flags = READ_SHORT(cptr + HX_LDEF_FLAGS);
        result.lines[i].type = cptr[HX_LDEF_TYPES];
        memcpy(&result.lines[i].args, cptr + HX_LDEF_ARGS, 5);
        result.lines[i].sidedef1 = MapSidedefRef(READ_SHORT(cptr + HX_LDEF_SDEF1));
        result.lines[i].sidedef2 = MapSidedefRef(READ_SHORT(cptr + HX_LDEF_SDEF2));
        cptr += HX_LDEF_SIZE;
    }
    free(lump);
    return result;
}

static void WriteHexenLinedefs(const linedef_array_t *linedefs, FILE *fp)
{
    uint8_t convbuffer[HX_LDEF_SIZE];
    int i;

    for (i = 0; i < linedefs->len; i++)
    {
        WRITE_SHORT(convbuffer + HX_LDEF_VERT1, linedefs->lines[i].vertex1);
        WRITE_SHORT(convbuffer + HX_LDEF_VERT2, linedefs->lines[i].vertex2);
        WRITE_SHORT(convbuffer + HX_LDEF_FLAGS, linedefs->lines[i].flags);
        convbuffer[HX_LDEF_TYPES] = linedefs->lines[i].type;
        memcpy(convbuffer + HX_LDEF_ARGS, linedefs->lines[i].args, 5);
        WRITE_SHORT(convbuffer + HX_LDEF_SDEF1,
                    linedefs->lines[i].sidedef1 & 0xffff);
        WRITE_SHORT(convbuffer + HX_LDEF_SDEF2,
                    linedefs->lines[i].sidedef2 & 0xffff);
        if (fwrite(convbuffer, HX_LDEF_SIZE, 1, fp) != 1)
        {
            perror("fwrite");
            ErrorExit("Failed writing linedef #%d to output file", i);
        }
    }
}

static sidedef_array_t ReadSidedefs(int lumpnum)
{
    sidedef_array_t result;
    uint8_t *cptr, *lump;
    int i;

    result.len = wadentry[lumpnum].length / SDEF_SIZE;
    result.sides = ALLOC_ARRAY(sidedef_t, result.len);
    lump = CacheLump(lumpnum);
    cptr = lump;
    for (i = 0; i < result.len; i++)
    {
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
    }
    free(lump);
    return result;
}

static void WriteSidedefs(const sidedef_array_t *sidedefs, FILE *fp)
{
    uint8_t convbuffer[SDEF_SIZE];
    int i;

    for (i = 0; i < sidedefs->len; i++)
    {
        memset(convbuffer, 0, sizeof(convbuffer));
        WRITE_SHORT(convbuffer + SDEF_XOFF, sidedefs->sides[i].xoffset);
        WRITE_SHORT(convbuffer + SDEF_YOFF, sidedefs->sides[i].yoffset);
        strncpy((char *) convbuffer + SDEF_UPPER, sidedefs->sides[i].upper, 8);
        strncpy((char *) convbuffer + SDEF_MIDDLE, sidedefs->sides[i].middle,
                8);
        strncpy((char *) convbuffer + SDEF_LOWER, sidedefs->sides[i].lower, 8);
        WRITE_SHORT(convbuffer + SDEF_SECTOR, sidedefs->sides[i].sector_ref);
        if (fwrite(convbuffer, SDEF_SIZE, 1, fp) != 1)
        {
            perror("fwrite");
            ErrorExit("Failed writing sidedef #%d to output file", i);
        }
    }
}
