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
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>

#include "blockmap.h"
#include "graphics.h"
#include "sidedefs.h"
#include "waddir.h"
#include "wadmerge.h"
#include "wadptr.h"

#define SPAMMY_PRINTF \
    if (!quiet_mode)  \
    printf

static bool Compress(const char *filename);
static bool Uncompress(const char *filename);
static bool ListEntries(const char *filename);
static bool DoAction(const char *filename);
static int FindPerc(int before, int after);
static void Help(void);
static bool IwadWarning(const char *);
static void ParseCommandLine(void);

// Global command line list, copied in main().
static int g_argc;
static char **g_argv;

static int filelist_index;
static const char *outputwad = NULL;
static enum { HELP, COMPRESS, UNCOMPRESS, LIST } action;

bool allowpack = true;   // level packing on
bool allowsquash = true; // picture squashing on
bool allowmerge = true;  // lump merging on
bool allowstack = true;  // blockmap stacking
bool hexen_format_wad;
static bool quiet_mode = false;

static bool FileExists(const char *filename)
{
    FILE *fs;

    fs = fopen(filename, "rb");
    if (fs != NULL)
    {
        fclose(fs);
        return true;
    }

    return false;
}

static FILE *OpenTempFile(const char *file_in_same_dir, char **filename)
{
    FILE *result;
    size_t filename_len;
    char *p;
    int i;

    filename_len = strlen(file_in_same_dir) + 24;
    *filename = ALLOC_ARRAY(char, filename_len);

    strncpy(*filename, file_in_same_dir, filename_len);
    p = strrchr(*filename, DIRSEP[0]);
    if (p != NULL)
    {
        ++p;
    }
    else
    {
        p = *filename;
    }

    for (i = 0; i < 100; i++)
    {
        snprintf(p, filename_len - (p - *filename), ".wadptr-temp-%03d", i);
        if (FileExists(*filename))
        {
            continue;
        }

        // The x modifier guarantees we never overwrite a file.
        result = fopen(*filename, "w+xb");
        if (result != NULL)
        {
            return result;
        }

        if (errno != EEXIST)
        {
            ErrorExit("Failed to open %s for writing.", *filename);
        }
    }

    ErrorExit("Failed to open a temporary file in same directory as '%s'",
              file_in_same_dir);
    return NULL;
}

void PrintProgress(int numerator, int denominator)
{
    static clock_t last_progress_time = 0;
    static int last_numerator = 0;
    clock_t now = clock();

    if (numerator < last_numerator ||
        now - last_progress_time >= (CLOCKS_PER_SEC / 20))
    {
        SPAMMY_PRINTF("%4d%%\b\b\b\b\b", (int) (100 * numerator) / denominator);
        fflush(stdout);
        last_progress_time = now;
    }

    last_numerator = numerator;
}

int main(int argc, char *argv[])
{
    int index;
    bool success = true;

    g_argc = argc; // Set global cmd-line list
    g_argv = argv;

    ParseCommandLine();

    for (index = filelist_index; index < g_argc; ++index)
    {
        if (!DoAction(g_argv[index]))
        {
            success = false;
        }
    }

    return !success;
}

static void ParseCommandLine(void)
{
    int i;

    action = HELP; // default if not told what to do

    filelist_index = -1;

    for (i = 1; i < g_argc; i++)
    {
        const char *arg = g_argv[i];

        // We allow GNU-style (eg. --list) by ignoring the first '-':
        if (arg[0] == '-' && arg[1] == '-')
        {
            ++arg;
        }

        if (!strcmp(arg, "-help") || !strcmp(arg, "-h"))
        {
            action = HELP;
            break;
        }
        else if (!strcmp(arg, "-list") || !strcmp(arg, "-l"))
        {
            action = LIST;
        }
        else if (!strcmp(arg, "-compress") || !strcmp(arg, "-c"))
        {
            action = COMPRESS;
        }
        else if (!strcmp(arg, "-uncompress") || !strcmp(arg, "-u"))
        {
            action = UNCOMPRESS;
        }
        else if (!strcmp(arg, "-quiet") || !strcmp(arg, "-q"))
        {
            quiet_mode = true;
        }
        else if (!strcmp(arg, "-nomerge"))
        {
            allowmerge = false;
        }
        else if (!strcmp(arg, "-nosquash"))
        {
            allowsquash = false;
        }
        else if (!strcmp(arg, "-nopack"))
        {
            allowpack = false;
        }
        else if (!strcmp(arg, "-nostack"))
        {
            allowstack = false;
        }
        else if (!strcmp(arg, "-version") || !strcmp(arg, "-v"))
        {
            printf("%s\n", VERSION);
            exit(0);
        }
        else if (!strcmp(arg, "-output") || !strcmp(arg, "-o"))
        {
            if (i + 1 >= g_argc)
            {
                ErrorExit("The -o argument requires a filename "
                          "to be specified.");
            }
            if (outputwad != NULL)
            {
                ErrorExit("The -o argument can only be specified once.");
            }
            outputwad = g_argv[i + 1];
            ++i;
        }
        else if (arg[0] != '-')
        {
            filelist_index = i;
            break;
        }
        else
        {
            ErrorExit("Invalid command line argument '%s'.", arg);
        }
    }

    if (action == HELP)
    {
        Help();
        exit(0);
    }

    if (filelist_index < 0)
    {
        ErrorExit("No input WAD(s) file specified.");
    }
    else if (outputwad != NULL && g_argc - filelist_index != 1)
    {
        ErrorExit("Only one input file can be specified when using -output.");
    }

    if (action == UNCOMPRESS && !allowmerge)
    {
        ErrorExit("Sorry, uncompressing will undo any lump merging on WADs. \n"
                  "The -nomerge command is not available with the "
                  "-u (uncompress) option.");
    }
}

static bool DoAction(const char *wadname)
{
    switch (action)
    {
    case LIST:
        return ListEntries(wadname);
    case UNCOMPRESS:
        return Uncompress(wadname);
    case COMPRESS:
        return Compress(wadname);
    default:
        return false;
    }
}

static void Help(void)
{
    printf(
        "wadptr - WAD Compressor version " VERSION "\n"
        "Copyright (c) 1997-2023 Simon Howard, Andreas Dehmel\n"
        "Distributed under the GNU GPL v2; see COPYING for details\n"
        "<https://soulsphere.org/projects/wadptr/>\n"
        "\n"
        "Usage: wadptr [options] <-c|-u|-l> inputwad [inputwad inputwad...]\n"
        "\n"
        " Commands:            Options:\n"
        " -c  Compress WAD     -o <file>  Write output WAD to <file>\n"
        " -u  Uncompress WAD   -q         Quiet mode; suppress normal output\n"
        " -l  List WAD         -nomerge   Disable lump merging\n"
        " -v  Display version  -nosquash  Disable graphic squashing\n"
        "                      -nopack    Disable sidedef packing\n"
        "                      -nostack   Disable blockmap stacking\n"
        "\n");
}

// Hexen levels have a slightly different format, and we can detect this by
// looking for the presence of a BEHAVIOR lump, which is unique to this format.
static void CheckHexenFormat(wad_file_t *wf, const char *filename)
{
    hexen_format_wad = EntryExists(wf, "BEHAVIOR") >= 0;
}

// LINEDEFS and SIDEDEFS lumps follow each other in Doom WADs. This is
// baked into the engine - Doom doesn't actually even look at the names.
static bool IsSidedefs(wad_file_t *wf, int count)
{
    return !strncmp(wf->entries[count].name, "SIDEDEFS", 8) && count > 0 &&
           !strncmp(wf->entries[count - 1].name, "LINEDEFS", 8);
}

static bool Compress(const char *wadname)
{
    wad_file_t wf;
    int count, findshrink;
    long wadsize; // previous wad size (to find % smaller)
    FILE *fstream;
    bool written;
    uint8_t *temp;
    char *tempwad_name, a[50];

    // TODO: This should be unnecessary:
    memset(&wf, 0, sizeof(wad_file_t));

    wf.fp = fopen(wadname, "rb");
    if (wf.fp == NULL)
    {
        perror("fopen");
        return false;
    }
    ReadWad(&wf);
    if (wf.type == WAD_FILE_IWAD && !IwadWarning(wadname))
    {
        return false;
    }
    CheckHexenFormat(&wf, wadname);

    wadsize = wf.diroffset + (ENTRY_SIZE * wf.num_entries);

    fstream =
        OpenTempFile(outputwad != NULL ? outputwad : wadname, &tempwad_name);

    // TODO: Remove this temp header code once everything uses WriteWadLump()
    memset(a, 0, 12);
    fwrite(a, 12, 1, fstream);

    for (count = 0; count < wf.num_entries; count++)
    {
        SPAMMY_PRINTF("Adding: %.8s       ", wf.entries[count].name);
        fflush(stdout);
        written = false;

        if (allowpack)
        {
            if (count + 1 < wf.num_entries && IsSidedefs(&wf, count + 1))
            {
                // We will write both LINEDEFS and SIDEDEFS when we reach
                // the next lump.
                SPAMMY_PRINTF("\tDeferred... (0%%)\n");
                written = true;
            }
            else if (IsSidedefs(&wf, count))
            {
                bool success;

                SPAMMY_PRINTF("\tPacking");
                fflush(stdout);
                findshrink = wf.entries[count].length;

                success = P_Pack(&wf, count);

                wf.entries[count - 1].offset = ftell(fstream);
                wf.entries[count - 1].length = P_WriteLinedefs(fstream);

                wf.entries[count].offset = ftell(fstream);
                wf.entries[count].length = P_WriteSidedefs(fstream);

                written = true;

                findshrink = FindPerc(findshrink, wf.entries[count].length);
                if (success)
                {
                    SPAMMY_PRINTF(" (%i%%), done.\n", findshrink);
                }
                else
                {
                    // TODO: Print info message if compression failed.
                    SPAMMY_PRINTF(" (0%%), failed.\n");
                }
            }
        }

        if (allowstack && !strncmp(wf.entries[count].name, "BLOCKMAP", 8))
        {
            bool success;

            SPAMMY_PRINTF("\tStacking ");
            findshrink = wf.entries[count].length;

            success = B_Stack(&wf, count);
            wf.entries[count].offset = ftell(fstream);
            wf.entries[count].length = B_WriteBlockmap(fstream);

            findshrink = FindPerc(findshrink, wf.entries[count].length);
            if (success)
            {
                SPAMMY_PRINTF("(%i%%), done.\n", findshrink);
            }
            else
            {
                // TODO: Print some kind of info message at the end of
                // compression to notify the user that not all blockmaps
                // could be stacked.
                SPAMMY_PRINTF("(0%%), failed.\n");
            }
            written = true;
        }

        if (allowsquash && S_IsGraphic(&wf, count))
        {
            SPAMMY_PRINTF("\tSquashing ");
            fflush(stdout);
            findshrink = wf.entries[count].length;

            temp = S_Squash(&wf, count);
            wf.entries[count].offset =
                WriteWadLump(fstream, temp, wf.entries[count].length);
            free(temp);

            findshrink = FindPerc(findshrink, wf.entries[count].length);
            SPAMMY_PRINTF("(%i%%), done.\n", findshrink);
            written = true;
        }

        if (!written && wf.entries[count].length == 0)
        {
            SPAMMY_PRINTF("\tEmpty (0%%).\n");
            wf.entries[count].offset = 0;
            written = true;
        }

        if (!written)
        {
            SPAMMY_PRINTF("\tStoring ");
            fflush(stdout);
            temp = CacheLump(&wf, count);
            wf.entries[count].offset =
                WriteWadLump(fstream, temp, wf.entries[count].length);
            free(temp);
            SPAMMY_PRINTF("(0%%), done.\n");
        }
    }

    // Write new directory:
    wf.diroffset = ftell(fstream);
    WriteWadDirectory(&wf, fstream);
    WriteWadHeader(&wf, fstream);

    fclose(fstream);
    fclose(wf.fp);

    if (allowmerge)
    {
        char *tempwad2_name;

        wf.fp = fopen(tempwad_name, "rb+");
        fstream = OpenTempFile(outputwad != NULL ? outputwad : wadname,
                               &tempwad2_name);

        SPAMMY_PRINTF("\nMerging identical lumps...");
        fflush(stdout);
        RebuildMergedWad(&wf, fstream);
        SPAMMY_PRINTF(" done.\n");

        fclose(fstream);
        fclose(wf.fp);

        remove(tempwad_name);
        free(tempwad_name);
        tempwad_name = tempwad2_name;
    }

    // We only overwrite the original input file once we have generated
    // the new one as a temporary file, so that it takes place as a
    // simple rename() call.
    if (outputwad == NULL)
    {
        remove(wadname);
        rename(tempwad_name, wadname);
    }
    else
    {
        remove(outputwad);
        rename(tempwad_name, outputwad);
    }

    free(tempwad_name);

    findshrink =
        FindPerc(wadsize, wf.diroffset + (wf.num_entries * ENTRY_SIZE));
    SPAMMY_PRINTF("*** %s is %ld bytes smaller (%d%%) ***\n",
                  outputwad != NULL ? outputwad : wadname,
                  wadsize - (wf.diroffset + wf.num_entries * ENTRY_SIZE),
                  findshrink);

    free(wf.entries);

    return true;
}

static bool Uncompress(const char *wadname)
{
    wad_file_t wf;
    char tempstr[50], *tempwad_name;
    FILE *fstream;
    uint8_t *tempres;
    bool written, blockmap_failures = false, sidedefs_failures = false;
    int count;

    memset(&wf, 0, sizeof(wad_file_t));

    wf.fp = fopen(wadname, "rb");
    if (wf.fp == NULL)
    {
        perror("fopen");
        return false;
    }
    ReadWad(&wf);
    if (wf.type == WAD_FILE_IWAD && !IwadWarning(wadname))
    {
        return false;
    }
    CheckHexenFormat(&wf, wadname);

    fstream = OpenTempFile(wadname, &tempwad_name);

    // TODO: Remove this temp header code once everything uses WriteWadLump()
    memset(tempstr, 0, 12);
    fwrite(tempstr, 12, 1, fstream);

    for (count = 0; count < wf.num_entries; count++)
    {
        written = false;

        SPAMMY_PRINTF("Adding: %.8s       ", wf.entries[count].name);
        fflush(stdout);

        if (allowpack)
        {
            if (count + 1 < wf.num_entries && IsSidedefs(&wf, count + 1))
            {
                // Write on next loop.
                SPAMMY_PRINTF("\tDeferred...\n");
                written = true;
            }
            else if (IsSidedefs(&wf, count))
            {
                bool success;

                SPAMMY_PRINTF("\tUnpacking");
                fflush(stdout);

                success = P_Unpack(&wf, count);

                wf.entries[count - 1].offset = ftell(fstream);
                wf.entries[count - 1].length = P_WriteLinedefs(fstream);

                wf.entries[count].offset = ftell(fstream);
                wf.entries[count].length = P_WriteSidedefs(fstream);

                if (success)
                {
                    SPAMMY_PRINTF(", done.\n");
                }
                else
                {
                    SPAMMY_PRINTF(", failed.\n");
                    sidedefs_failures = true;
                }
                written = true;
            }
        }
        if (allowstack && !strncmp(wf.entries[count].name, "BLOCKMAP", 8))
        {
            bool success;

            SPAMMY_PRINTF("\tUnstacking");
            fflush(stdout);

            success = B_Unstack(&wf, count);
            wf.entries[count].offset = ftell(fstream);
            wf.entries[count].length = B_WriteBlockmap(fstream);

            if (success)
            {
                SPAMMY_PRINTF(", done.\n");
            }
            else
            {
                SPAMMY_PRINTF(", failed.\n");
                blockmap_failures = true;
            }
            written = true;
        }

        if (allowsquash && S_IsGraphic(&wf, count))
        {
            SPAMMY_PRINTF("\tUnsquashing");
            fflush(stdout);
            tempres = S_Unsquash(&wf, count);
            wf.entries[count].offset
                = WriteWadLump(fstream, tempres, wf.entries[count].length);
            free(tempres);
            SPAMMY_PRINTF(", done\n");
            written = true;
        }

        if (!written && wf.entries[count].length == 0)
        {
            SPAMMY_PRINTF("\tEmpty.\n");
            wf.entries[count].offset = 0;
            written = true;
        }

        if (!written)
        {
            SPAMMY_PRINTF("\tStoring");
            fflush(stdout);
            tempres = CacheLump(&wf, count);
            wf.entries[count].offset =
                WriteWadLump(fstream, tempres,  wf.entries[count].length);
            free(tempres);
            SPAMMY_PRINTF(", done.\n");
        }
    }
    // update the directory location
    wf.diroffset = ftell(fstream);
    WriteWadDirectory(&wf, fstream);
    WriteWadHeader(&wf, fstream);

    fclose(fstream);
    fclose(wf.fp);
    free(wf.entries);

    if (outputwad == NULL)
    {
        // Replace the original wad with the new one:
        remove(wadname);
        rename(tempwad_name, wadname);
    }
    else
    {
        rename(tempwad_name, outputwad);
    }

    free(tempwad_name);

    if (blockmap_failures)
    {
        SPAMMY_PRINTF("\nSome BLOCKMAP lumps could not be unstacked because "
                      "the decompressed\nversion would exceed the vanilla "
                      "BLOCKMAP size limit.\n");
    }
    if (sidedefs_failures)
    {
        SPAMMY_PRINTF("\nSome SIDEDEFS lumps could not be unpacked because "
                      "the decompressed\nversion would exceed the vanilla "
                      "sidedef count limit.\n");
    }

    return true;
}

static const char *CompressionMethod(wad_file_t *wf, int lumpnum)
{
    if (wf->entries[lumpnum].length == 0)
    {
        return "Empty";
    }
    else if (IsSidedefs(wf, lumpnum))
    {
        // This is a level:
        if (P_IsPacked(wf, lumpnum))
        {
            return "Packed";
        }
        else
        {
            return "Unpacked";
        }
    }
    else if (S_IsGraphic(wf, lumpnum))
    {
        // This is a graphic:
        if (S_IsSquashed(wf, lumpnum))
        {
            return "Squashed";
        }
        else
        {
            return "Unsquashed";
        }
    }
    else if (!strncmp(wf->entries[lumpnum].name, "BLOCKMAP", 8))
    {
        if (B_IsStacked(wf, lumpnum))
        {
            return "Stacked";
        }
        else
        {
            return "Unstacked";
        }
    }
    else
    {
        // Ordinary lump w/no compression.
        return "Stored";
    }
}

static bool ListEntries(const char *wadname)
{
    wad_file_t wf;
    int i, j;

    memset(&wf, 0, sizeof(wad_file_t));

    wf.fp = fopen(wadname, "rb");
    if (wf.fp == NULL)
    {
        perror("fopen");
        return false;
    }
    ReadWad(&wf);
    CheckHexenFormat(&wf, wadname);

    SPAMMY_PRINTF(
        " Number  Length  Offset      Method      Name        Shared\n"
        " ------  ------  ------      ------      ----        ------\n");

    for (i = 0; i < wf.num_entries; i++)
    {
        printf("%7d %7ld  0x%08lx  %-11s %-8.8s    ", i + 1,
               wf.entries[i].length, wf.entries[i].offset,
               CompressionMethod(&wf, i), wf.entries[i].name);

        // shared resource?
        for (j = 0; wf.entries[i].length > 0 && j < i; j++)
        {
            if (wf.entries[j].offset == wf.entries[i].offset &&
                wf.entries[j].length == wf.entries[i].length)
            {
                printf("%.8s\n", wf.entries[j].name);
                break;
            }
        }
        if (wf.entries[i].length == 0 || j >= i)
        {
            printf("No\n");
        }
    }

    fclose(wf.fp);
    free(wf.entries);

    return true;
}

// Find how much smaller something is: returns a percentage.
static int FindPerc(int before, int after)
{
    double perc;

    perc = 1 - (((double) after) / before);

    return (int) (100 * perc);
}

// ReadResponse reads a line from standard input containing a single character
// and returns that character. Any other response (eg. >1 char) returns 0.
static char ReadResponse(void)
{
    int nchars = 0;
    int last_char = 0;

    for (;;)
    {
        int c = fgetc(stdin);
        if (c == EOF)
        {
            ErrorExit("EOF on reading response from user");
        }
        if (c == '\n')
        {
            break;
        }
        ++nchars;
        last_char = c;
    }

    if (nchars != 1)
    {
        return 0;
    }

    return (char) last_char;
}

static bool IwadWarning(const char *wadname)
{
    int response;
    // In quiet mode we silently proceed; if an output WAD is specified
    // then we are not modifying the original file.
    if (quiet_mode || outputwad != NULL)
    {
        return true;
    }
    while (1)
    {
        printf("%s is an IWAD file; are you sure you want to "
               "change it (y/n)? ",
               wadname);
        fflush(stdout);
        response = tolower(ReadResponse());
        if (response == 'y')
        {
            printf("\n");
            return true;
        }
        if (response == 'n')
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
        ErrorExit("Failed to allocate %ld bytes", nbytes);
    }

    return result;
}
