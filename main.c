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
 * Compresses Doom WAD files through several methods:
 *
 * - Merges identical lumps (see wadmerge.c)
 * - 'Squashes' graphics (see lumps.c)
 * - Packs the sidedefs in levels (see lumps.c)
 */

#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "wadptr.h"

static void Compress(void);
static void DoAction(void);
static int FindPerc(int before, int after);
static void Help(void);
static bool IwadWarning(void);
static void ListEntries(void);
static bool OpenWad(char *filename);
static void ParseCommandLine(void);
static void Uncompress(void);

static int g_argc; /* global cmd-line list */
static char **g_argv;
static int filelist_index;
static char *wadname;
static const char *outputwad = NULL;
static enum { HELP, COMPRESS, UNCOMPRESS, LIST } action;
bool allowpack = true;   /* level packing on */
bool allowsquash = true; /* picture squashing on */
bool allowmerge = true;  /* lump merging on */

const char *pwad_name = "PWAD";
const char *iwad_name = "IWAD";

static const char *tempwad_name = "~wptmp" EXTSEP "wad";

void PrintProgress(int numerator, int denominator)
{
    static clock_t last_progress_time = 0;
    static int last_numerator = 0;
    clock_t now = clock();

    if (numerator < last_numerator
     || now - last_progress_time >= (CLOCKS_PER_SEC / 20))
    {
        printf("%4d%%\b\b\b\b\b", (int) (100 * numerator) / denominator);
        fflush(stdout);
        last_progress_time = now;
    }

    last_numerator = numerator;
}

int main(int argc, char *argv[])
{
    int index;
    bool success = true;

    g_argc = argc; /* Set global cmd-line list */
    g_argv = argv;

    ParseCommandLine(); /* Check cmd-lines */

    for (index = filelist_index; index < g_argc; ++index)
    {
        wadname = g_argv[index];
        if (!OpenWad(wadname))
        {
            /* no problem with wad.. do whatever (Compress, Uncompress etc) */
            DoAction();
            fclose(wadfp);
        }
        else
        {
            success = false;
        }
    }

    return !success;
}

static void ParseCommandLine(void)
{
    int count;

    action = HELP; /* default to Help if not told what to do */

    count = 1;
    filelist_index = -1;
    while (count < g_argc)
    {
        if ((!strcmp(g_argv[count], "-help")) || (!strcmp(g_argv[count], "-h")))
            action = HELP;

        if ((!strcmp(g_argv[count], "-list")) || (!strcmp(g_argv[count], "-l")))
            action = LIST;

        if ((!strcmp(g_argv[count], "-compress")) ||
            (!strcmp(g_argv[count], "-c")))
            action = COMPRESS;

        if ((!strcmp(g_argv[count], "-uncompress")) ||
            (!strcmp(g_argv[count], "-u")))
            action = UNCOMPRESS;

        /* specific disabling */
        if (!strcmp(g_argv[count], "-nomerge"))
            allowmerge = false;
        if (!strcmp(g_argv[count], "-nosquash"))
            allowsquash = false;
        if (!strcmp(g_argv[count], "-nopack"))
            allowpack = false;

        if (g_argv[count][0] != '-')
        {
            filelist_index = count;
            break;
        }

        if ((!strcmp(g_argv[count], "-output")) ||
            (!strcmp(g_argv[count], "-o")))
        {
            outputwad = g_argv[++count];
        }

        count++;
    }

    if (filelist_index < 0)
    {
        if (action == HELP)
        {
            DoAction();
            exit(0);
        }
        else
            ErrorExit("No input WAD file specified.\n");
    }

    if (action == UNCOMPRESS && !allowmerge)
        ErrorExit("Sorry, uncompressing will undo any lump merging on WADs.\n"
                  "The -nomerge command is not available with the "
                  "-u(Uncompress) option.\n");
}

static bool OpenWad(char *filename)
{
    wadfp = fopen(filename, "rb+");
    if (!wadfp)
    {
        printf("%s does not exist\n", wadname);
        return true;
    }

    return ReadWad();
}

/* Does an action based on command line */

static void DoAction(void)
{
    switch (action)
    {
    case HELP:
        Help();
        break;

    case LIST:
        ListEntries();
        break;

    case UNCOMPRESS:
        Uncompress();
        break;

    case COMPRESS:
        Compress();
        break;
    }
}

static void Help(void)
{
    printf("\n"
           "WADPTR - WAD Compressor version " VERSION "\n"
           "Copyright (c) 1997-2023 Simon Howard, Andreas Dehmel\n"
           "<http://www.soulsphere.org/projects/wadptr/>\n"
           "\n"
           "Usage:  WADPTR <-c|-u|-l> [options] inputwad [outputwad]\n"
           "\n"
           " -c        :   Compress WAD\n"
           " -u        :   Uncompress WAD\n"
           " -l        :   List WAD\n"
           " -o <file> :   Write output WAD to <file>\n"
           " -h        :   Help\n"
           "\n"
           " -nomerge  :   Disable lump merging\n"
           " -nosquash :   Disable graphic squashing\n"
           " -nopack   :   Disable sidedef packing\n");
}

/* Compress a WAD */

static void Compress(void)
{
    int count, findshrink;
    long wadsize; /* wad size(to find % smaller) */
    FILE *fstream;
    bool written, write_silent;
    char *temp, a[50];

    if (wad == IWAD && !IwadWarning())
    {
        return;
    }

    wadsize = diroffset + (ENTRY_SIZE * numentries);

    fstream = fopen(tempwad_name, "wb+");
    if (!fstream)
        ErrorExit("Compress: Couldn't write a temporary file\n");

    memset(a, 0, 12);
    fwrite(a, 12, 1, fstream); /* temp header. */

    for (count = 0; count < numentries; count++)
    {
        /* hide individual level entries */
        if (!IsLevelEntry(wadentry[count].name))
        {
            printf("Adding: %.8s       ", wadentry[count].name);
            fflush(stdout);
            write_silent = false;
        }
        else
        {
            /* we only print the level header */
            write_silent = true;
        }
        written = false;

        if (allowpack)
        {
            if (IsLevel(count))
            {
                printf("\tPacking");
                fflush(stdout);
                findshrink = FindLevelSize(wadentry[count].name);

                /* pack the level */
                P_Pack(wadentry[count].name);

                findshrink = FindPerc(findshrink,
                                      FindLevelSize(wadentry[count].name));
                printf(" (%i%%), done.\n", findshrink);

                write_silent = true;
            }
            else if (!strncmp(wadentry[count].name, "SIDEDEFS", 8))
            {
                /* write the pre-packed sidedef entry */
                wadentry[count].offset = ftell(fstream);
                WriteSidedefs((sidedef_t *) p_sidedefres,
                              wadentry[count].length, fstream);
                free(p_sidedefres);
                written = true;
            }
            else if (!strncmp(wadentry[count].name, "LINEDEFS", 8))
            {
                /* write the pre-packed linedef entry */
                wadentry[count].offset = ftell(fstream);
                WriteLinedefs((linedef_t *) p_linedefres,
                              wadentry[count].length, fstream);
                free(p_linedefres);
                written = true;
            }
        }

        if (allowsquash && S_IsGraphic(wadentry[count].name))
        {
            printf("\tSquashing ");
            fflush(stdout);
            findshrink = wadentry[count].length;

            temp = S_Squash(wadentry[count].name);
            wadentry[count].offset = ftell(fstream); /*update dir */
            fwrite(temp, wadentry[count].length, 1, fstream);

            free(temp);
            findshrink = FindPerc(findshrink, wadentry[count].length);
            printf("(%i%%), done.\n", findshrink);
            written = true;
        }

        if (!written && !write_silent)
        {
            printf("\tStoring ");
            fflush(stdout);
        }
        if (!written)
        {
            temp = CacheLump(count);
            wadentry[count].offset = ftell(fstream); /*update dir */
            fwrite(temp, wadentry[count].length, 1, fstream);
            free(temp);
        }
        if (!written && !write_silent)
        {
            printf("(0%%), done.\n");
        }
    }

    /* write new directory */
    diroffset = ftell(fstream);
    WriteWadDirectory(fstream);
    WriteWadHeader(fstream);

    fclose(fstream);
    fclose(wadfp);

    if (allowmerge)
    {
        wadfp = fopen(tempwad_name, "rb+");
        printf("\nMerging identical lumps...");
        fflush(stdout);

        if (outputwad == NULL)
        {
            /* remove identical lumps: Rebuild them back to
               the original filename */
            remove(wadname);
            Rebuild(wadname);
        }
        else
        {
            Rebuild(outputwad);
        }
        printf(" done.\n");
        fclose(wadfp);
        remove(tempwad_name);
    }
    else if (outputwad == NULL)
    {
        remove(wadname);
        rename(tempwad_name, wadname);
    }
    else
    {
        rename(tempwad_name, outputwad);
    }

    wadfp = fopen(wadname, "rb+"); /* so there is something to close */

    findshrink = FindPerc(wadsize, diroffset + (numentries * ENTRY_SIZE));

    printf("*** %s is %i%% smaller ***\n", wadname, findshrink);
}

/* Uncompress a WAD */
/* TODO: This can probably be merged with Compress() above. */
static void Uncompress(void)
{
    char tempstr[50];
    FILE *fstream;
    char *tempres;
    bool written, write_silent;
    long fileloc;
    int count;

    if (wad == IWAD && !IwadWarning())
    {
        return;
    }

    fstream = fopen(tempwad_name, "wb+");
    memset(tempstr, 0, 12);
    fwrite(tempstr, 12, 1, fstream); /* temp header */

    for (count = 0; count < numentries; count++)
    {
        written = false;

        if (IsLevelEntry(wadentry[count].name))
        {
            write_silent = true;
        }
        else
        {
            printf("Adding: %.8s       ", wadentry[count].name);
            fflush(stdout);
            write_silent = false;
        }

        if (allowpack)
        {
            if (IsLevel(count))
            {
                printf("\tUnpacking");
                fflush(stdout);
                P_Unpack(wadentry[count].name);
                printf(", done.\n");
                write_silent = true;
            }
            if (!strncmp(wadentry[count].name, "SIDEDEFS", 8))
            {
                wadentry[count].offset = ftell(fstream);
                WriteSidedefs((sidedef_t *) p_sidedefres,
                              wadentry[count].length, fstream);
                free(p_sidedefres);
                written = true;
            }
            if (!strncmp(wadentry[count].name, "LINEDEFS", 8))
            {
                wadentry[count].offset = ftell(fstream);
                WriteLinedefs((linedef_t *) p_linedefres,
                              wadentry[count].length, fstream);
                free(p_linedefres);
                written = true;
            }
        }
        if (allowsquash && S_IsGraphic(wadentry[count].name))
        {
            printf("\tUnsquashing");
            fflush(stdout);
            tempres = S_Unsquash(wadentry[count].name);
            wadentry[count].offset = ftell(fstream);
            fwrite(tempres, wadentry[count].length, 1, fstream);
            free(tempres);
            printf(", done\n");
            written = true;
        }

        if (!written && !write_silent)
        {
            printf("\tStoring");
            fflush(stdout);
        }
        if (!written)
        {
            tempres = CacheLump(count);
            fileloc = ftell(fstream);
            fwrite(tempres, wadentry[count].length, 1, fstream);
            free(tempres);
            wadentry[count].offset = fileloc;
        }
        if (!written && !write_silent)
        {
            printf(", done.\n");
        }
    }
    /* update the directory location */
    diroffset = ftell(fstream);
    WriteWadDirectory(fstream);
    WriteWadHeader(fstream);

    fclose(fstream);
    fclose(wadfp);

    if (outputwad == NULL)
    {
        /* replace the original wad with the new one */
        remove(wadname);
        rename(tempwad_name, wadname);
    }
    else
    {
        rename(tempwad_name, outputwad);
    }
    wadfp = fopen(wadname, "rb+");
}

/* List WAD entries */

static void ListEntries(void)
{
    int count, count2;

    printf(" Number  Length  Offset      Method      Name        Shared\n"
           " ------  ------  ------      ------      ----        ------\n");

    for (count = 0; count < numentries; count++)
    {
        if (IsLevelEntry(wadentry[count].name))
            continue;

        /* wad entry number */
        printf("%7d", count + 1);

        /* size */
        if (IsLevel(count))
        {
            /* the whole level not just the id lump */
            printf(" %7d", FindLevelSize(wadentry[count].name));
        }
        else
        {
            /* not a level, doesn't matter */
            printf(" %7ld", wadentry[count].length);
        }

        /* file offset */
        printf("  0x%08lx  ", wadentry[count].offset);

        /* compression method */
        if (IsLevel(count))
        {
            /* this is a level */
            if (P_IsPacked(wadentry[count].name))
                printf("Packed      ");
            else
                printf("Unpacked    ");
        }
        else if (S_IsGraphic(wadentry[count].name))
        {
            /* this is a graphic */
            if (S_IsSquashed(wadentry[count].name))
                printf("Squashed    ");
            else
                printf("Unsquashed  ");
        }
        else
        {
            /* ordinary lump w/no compression */
            printf("Stored      ");
        }

        /* resource name */
        printf("%-8.8s    ", wadentry[count].name);

        /* shared resource */
        if (wadentry[count].length == 0)
        {
            printf("No\n");
            continue;
        }
        for (count2 = 0; count2 < count; count2++)
        {
            /* same offset + size */
            if ((wadentry[count2].offset == wadentry[count].offset) &&
                (wadentry[count2].length == wadentry[count].length))
            {
                printf("%.8s\n", wadentry[count2].name);
                break;
            }
        }
        /* no identical lumps if it reached the last one */
        if (count2 == count)
            printf("No\n");
    }
}

/* Find how much smaller something is: returns a percentage */

static int FindPerc(int before, int after)
{
    double perc;

    perc = 1 - (((double) after) / before);

    return (int) (100 * perc);
}

static bool IwadWarning(void)
{
    char tempchar;
    printf("%s is an IWAD file; are you sure you want to change it? ", wadname);
    fflush(stdout);
    while (1)
    {
        tempchar = tolower(fgetc(stdin));
        if (tempchar == 'y')
        {
            printf("\n");
            return true;
        }
        if (tempchar == 'n')
        {
            printf("\n");
            return false;
        }
    }
}

void *CheckedRealloc(void *old, size_t nbytes)
{
    void *result = realloc(old, nbytes);

    if (result == NULL && nbytes > 0)
    {
        ErrorExit("Failed to allocate %ld bytes\n", nbytes);
    }

    return result;
}
