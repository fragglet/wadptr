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

#include <string.h>

#include "lumps.h"
#include "wadptr.h"

static void P_FindInfo(void);
static void P_DoPack(sidedef_t *sidedefs);
static void P_BuildLinedefs(linedef_t *linedefs);
static void P_Rebuild(void);

static int S_FindRedundantColumns(unsigned char *s);
static int S_FindColumnSize(unsigned char *col1);

static linedef_t *ReadLinedefs(int lumpnum, FILE *fp);
static sidedef_t *ReadSidedefs(int lumpnum, FILE *fp);

static int p_levelnum;   /* entry number (index in WAD directory) */
static int p_sidedefnum; /* sidedef wad entry number */
static int p_linedefnum; /* linedef wad entry number */
static int p_num_linedefs = 0,
           p_num_sidedefs = 0; /* number of sd/lds do not get confused */
static const char *p_working;  /* the name of the level resource eg. "MAP01" */

static sidedef_t *p_newsidedef; /* the new sidedefs */
static linedef_t *p_newlinedef; /* the new linedefs */
static int *p_movedto;          /* keep track of where the sidedefs are now */
static int p_newnum = 0;        /* the new number of sidedefs */

char *p_linedefres = 0; /* the new linedef resource */
char *p_sidedefres = 0; /* the new sidedef resource */

/* Graphic squashing globals */
static bool s_unsquash_mode = false; /* True when we are inside a
                                        S_Unsquash() call. */
static int s_equalcolumn[400];  /* 1 for each column: another column which is */
                                /* identical or -1 if there isn't one */
static short s_height, s_width; /* picture width, height etc. */
static short s_loffset, s_toffset;
static unsigned char *s_columns; /* the location of each column in the lump */
static long s_colsize[400];      /* the length(in bytes) of each column */

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
    p_linedefres = (char *) p_newlinedef;
    p_sidedefres = (char *) p_newsidedef;

    wadentry[p_sidedefnum].length = p_newnum * SDEF_SIZE;
}

/* Unpack a level */

/* Same thing, in reverse. Saves the new sidedef and linedef lumps to */
/* p_sidedefres and p_linedefres. */

void P_Unpack(char *resname)
{
    p_working = resname;

    P_FindInfo();
    P_Rebuild();

    p_linedefres = (char *) p_newlinedef;
    p_sidedefres = (char *) p_newsidedef;
}

/* Find if a level is packed */

bool P_IsPacked(char *s)
{
    linedef_t *linedefs;
    int count;

    p_working = s;

    P_FindInfo();

    linedefs = ReadLinedefs(p_linedefnum, wadfp);

    /* 10 extra sidedefs for safety */
    p_movedto = ALLOC_ARRAY(int, p_num_sidedefs + 10);

    /* uses p_movedto to find if same sidedef has already been used
       on an earlier linedef checked */

    for (count = 0; count < p_num_sidedefs; count++)
        p_movedto[count] = 0;

    for (count = 0; count < p_num_linedefs; count++) /* now check */
    {
        if (linedefs[count].sidedef1 != NO_SIDEDEF)
        {
            if (p_movedto[linedefs[count].sidedef1])
            {
                /* side already used on a previous linedef */
                free(linedefs);
                free(p_movedto);
                return true; /* must be packed */
            }
            else
            {
                /* mark it as used for later reference */
                p_movedto[linedefs[count].sidedef1] = 1;
            }
        }
        if (linedefs[count].sidedef2 != NO_SIDEDEF)
        {
            if (p_movedto[linedefs[count].sidedef2])
            {
                free(linedefs);
                free(p_movedto);
                return true;
            }
            else
                p_movedto[linedefs[count].sidedef2] = 1;
        }
    }
    free(linedefs);
    free(p_movedto);
    return false; /* cant be packed: none of the sidedefs are shared */
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
        ErrorExit("P_FindInfo: Couldn't find level: %.8s\n", p_working);

    n = 0;

    /* now find the sidedefs */
    for (count = p_levelnum + 1; count < numentries; count++)
    {
        if (!IsLevelEntry(wadentry[count].name))
        {
            ErrorExit("P_FindInfo: Can't find sidedef/linedef entries!\n");
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

/* Actually pack the sidedefs */

static void P_DoPack(sidedef_t *sidedefs)
{
    int count, count2;

    p_newsidedef = malloc(wadentry[p_sidedefnum].length * 10);
    if (!p_newsidedef)
    {
        ErrorExit("P_DoPack: could not alloc memory for new sidedefs\n");
    }

    p_movedto = ALLOC_ARRAY(int, p_num_sidedefs + 10);

    p_newnum = 0;
    for (count = 0; count < p_num_sidedefs; count++)
    {
        if ((count % 100) == 0)
        {
            /* time for a percent-done update */
            double c = count;
            double p = p_num_sidedefs;
            printf("%4d%%\b\b\b\b\b",
                   (int) (100 * (c * c + c) / (p * p + p)));
            fflush(stdout);
        }
        for (count2 = 0; count2 < p_newnum; count2++) /* check previous */
        {
            if (!memcmp(&p_newsidedef[count2], &sidedefs[count],
                        sizeof(sidedef_t)))
            {
                /* they are identical: this one can be removed */
                p_movedto[count] = count2;
                break;
            }
        }
        /* a sidedef like this does not yet exist: add one */
        if (count2 >= p_newnum)
        {
            memcpy(&p_newsidedef[p_newnum], &sidedefs[count],
                   sizeof(sidedef_t));
            p_movedto[count] = p_newnum;
            p_newnum++;
        }
    }
    free(sidedefs);
}

/* Update the linedefs and save sidedefs */

static void P_BuildLinedefs(linedef_t *linedefs)
{
    int count;

    /* update the linedefs with where the sidedefs have been moved to, */
    /* using p_movedto[] to find where they now are.. */

    for (count = 0; count < p_num_linedefs; count++)
    {
        if (linedefs[count].sidedef1 != NO_SIDEDEF)
            linedefs[count].sidedef1 = p_movedto[linedefs[count].sidedef1];
        if (linedefs[count].sidedef2 != NO_SIDEDEF)
            linedefs[count].sidedef2 = p_movedto[linedefs[count].sidedef2];
    }

    p_newlinedef = linedefs;

    free(p_movedto);
}

/* Rebuild the sidedefs */

static void P_Rebuild(void)
{
    sidedef_t *sidedefs;
    linedef_t *linedefs;
    int count;

    sidedefs = ReadSidedefs(p_sidedefnum, wadfp);
    linedefs = ReadLinedefs(p_linedefnum, wadfp);

    p_newsidedef = malloc(wadentry[p_sidedefnum].length * 10);

    if (!p_newsidedef)
        ErrorExit("P_Rebuild: could not alloc memory for new sidedefs\n");

    p_newnum = 0;

    for (count = 0; count < p_num_linedefs; count++)
    {
        if (linedefs[count].sidedef1 != NO_SIDEDEF)
        {
            memcpy(&(p_newsidedef[p_newnum]),
                   &(sidedefs[linedefs[count].sidedef1]), sizeof(sidedef_t));
            linedefs[count].sidedef1 = p_newnum;
            p_newnum++;
        }
        if (linedefs[count].sidedef2 != NO_SIDEDEF)
        {
            memcpy(&(p_newsidedef[p_newnum]),
                   &(sidedefs[linedefs[count].sidedef2]), sizeof(sidedef_t));
            linedefs[count].sidedef2 = p_newnum;
            p_newnum++;
        }
    }
    /* update the wad directory */
    wadentry[p_sidedefnum].length = p_newnum * SDEF_SIZE;

    /* no longer need the old sidedefs */
    free(sidedefs);
    /* still need the old linedefs: they have been updated */
    p_newlinedef = linedefs;
}

/*
 *  Compress by matching entire columns if defined (old approach), or by
 *  matching postfixes as well if undefined (new approach, experimental)
 */
/*#define ENTIRE_COLUMNS*/

/* Graphic squashing routines */

/* Squash a graphic lump */

/* Squashes a graphic. Call with the lump name - eg. S_Squash("TITLEPIC"); */
/* returns a pointer to the new(compressed) lump. This must be free()d when */
/* it is no longer needed, as S_Squash() does not do this itself. */

char *S_Squash(char *s)
{
    unsigned char *working, *newres;
    int entrynum, count;
    unsigned char *newptr;
    long lastpt;

    if (!S_IsGraphic(s))
        return NULL;
    entrynum = EntryExists(s);
    working = CacheLump(entrynum);
    if ((long) working == -1)
        ErrorExit("squash: Couldn't find %.8s\n", s);

    /* find posts to be killed; if none, return original lump */
    if (S_FindRedundantColumns(working) == 0 && !s_unsquash_mode)
        return (char *) working;

    /* TODO: Fix static limit. */
    newres = ALLOC_ARRAY(unsigned char, 100000);

    /* find various info: size,offset etc. */
    WRITE_SHORT(newres, s_width);
    WRITE_SHORT(newres + 2, s_height);
    WRITE_SHORT(newres + 4, s_loffset);
    WRITE_SHORT(newres + 6, s_toffset);

    /* the new column pointers for the new lump */
    newptr = (unsigned char *) (newres + 8);

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
#ifdef ENTIRE_COLUMNS
            /* identical column already in: use that one */
            memcpy(newptr + 4 * count, newptr + 4 * s_equalcolumn[count], 4);
#else
            /* postfix compression, see S_FindRedundantColumns() */
            long identOff;

            identOff = READ_LONG(newptr + 4 * s_equalcolumn[count]);
            identOff += s_colsize[s_equalcolumn[count]] - s_colsize[count];
            WRITE_LONG(newptr + 4 * count, identOff);
#endif
        }
    }

    if (!s_unsquash_mode && lastpt > wadentry[entrynum].length)
    {
        /* the new resource was bigger than the old one! */
        free(newres);
        return (char *) working;
    }
    else
    {
        wadentry[entrynum].length = lastpt;
        free(working);
        return (char *) newres;
    }
}

/* Unsquash a picture. Unsquashing rebuilds the image, just like when we
 * do the squashing, except that we set a special flag that skips
 * searching for identical columns. */
char *S_Unsquash(char *s)
{
    char *result;

    s_unsquash_mode = true;
    result = S_Squash(s);
    s_unsquash_mode = false;

    return result;
}

/* Find the redundant columns */

static int S_FindRedundantColumns(unsigned char *x)
{
    int count, count2;
    int num_killed = 0;

    s_width = READ_SHORT(x);
    s_height = READ_SHORT(x + 2);
    s_loffset = READ_SHORT(x + 4);
    s_toffset = READ_SHORT(x + 6);

    s_columns = (unsigned char *) (x + 8);

    for (count = 0; count < s_width; count++)
    {
        long tmpcol;

        /* first assume no identical column exists */
        s_equalcolumn[count] = -1;

        /* find the column size */
        tmpcol = READ_LONG(s_columns + 4 * count);
        s_colsize[count] = S_FindColumnSize((unsigned char *) x + tmpcol);

        /* Unsquash mode is identical to squash mode but we just don't
           look for any identical columns. */
        if (s_unsquash_mode)
        {
            continue;
        }

        /* check all previous columns */
        for (count2 = 0; count2 < count; count2++)
        {
#ifdef ENTIRE_COLUMNS
            if (s_colsize[count] != s_colsize[count2])
            {
                /* columns are different sizes: must be different */
                continue;
            }
            if (!memcmp(x + tmpcol, x + READ_LONG(s_columns + 4 * count2),
                        s_colsize[count]))
            {
                /* columns are identical */
                s_equalcolumn[count] = count2;
                num_killed++; /* increase deathcount */
                break;        /* found one, exit the loop */
            };
#else
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
#endif
        }
    }
    /* tell squash how many can be 'got rid of' */
    return num_killed;
}

/* Find the size of a column */

static int S_FindColumnSize(unsigned char *col1)
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

bool S_IsSquashed(char *s)
{
    int entrynum;
    char *pic;
    int count, count2;

    entrynum = EntryExists(s);
    if (entrynum == -1)
        ErrorExit("is_squashed: %.8s does not exist!\n", s);
    pic = CacheLump(entrynum); /* cache the lump */

    s_width = READ_SHORT(pic);
    s_height = READ_SHORT(pic + 2);
    s_loffset = READ_SHORT(pic + 4);
    s_toffset = READ_SHORT(pic + 6);

    /* find the column locations */
    s_columns = (unsigned char *) (pic + 8);

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

bool S_IsGraphic(char *s)
{
    unsigned char *graphic;
    int entrynum, count;
    short width, height;
    unsigned char *columns;

    if (!strncmp(s, "ENDOOM", 8))
        return false;
    if (IsLevelEntry(s))
        return false;
    if (s[0] == 'D' && ((s[1] == '_') || (s[1] == 'S')))
    {
        /* sfx or music */
        return false;
    }

    entrynum = EntryExists(s);
    if (entrynum == -1)
        ErrorExit("isgraphic: %.8s does not exist!\n", s);
    if (wadentry[entrynum].length <= 0)
    {
        /* don't read data from 0 size lumps */
        return false;
    }
    graphic = CacheLump(entrynum);

    width = READ_SHORT(graphic);
    height = READ_SHORT(graphic + 2);
    columns = (unsigned char *) (graphic + 8);

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

/*
 *  portable reading / writing of linedefs and sidedefs
 *  by Andreas Dehmel (dehmel@forwiss.tu-muenchen.de)
 */

static const int convbuffsize = 0x8000;
static unsigned char convbuffer[0x8000];

static linedef_t *ReadLinedefs(int lumpnum, FILE *fp)
{
    linedef_t *lines;
    int i, numlines, validbytes;
    unsigned char *cptr;

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
        lines[i].types = READ_SHORT(cptr + LDEF_TYPES);
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
    unsigned char *cptr;

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
        WRITE_SHORT(cptr + LDEF_TYPES, lines[i].types);
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
    unsigned char *cptr;

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
        cptr += SDEF_SIZE;
        validbytes -= SDEF_SIZE;
    }
    return sides;
}

int WriteSidedefs(sidedef_t *sides, int bytes, FILE *fp)
{
    int i;
    unsigned char *cptr;

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
