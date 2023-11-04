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
static const char *PercentSmaller(int before, int after);
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
bool extsides = false;   // extended sidedefs limit
bool extblocks = false;  // extended blockmap limit
static bool psx_format = false;
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

#ifdef _WIN32
#define EXCLUSIVE ""
#else
#define EXCLUSIVE "x"
#endif
        // The x modifier guarantees we never overwrite a file.
        result = fopen(*filename, "w+b" EXCLUSIVE);
        if (result != NULL)
        {
            return result;
        }

        if (errno != EEXIST)
        {
            perror("fopen");
            ErrorExit("Failed to open %s for writing.", *filename);
        }
    }

    ErrorExit("Failed to open a temporary file in same directory as '%s'",
              file_in_same_dir);
    return NULL;
}

static long FileSize(FILE *fp)
{
    long result;
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        perror("fseek");
        goto failed;
    }
    result = ftell(fp);
    if (result < 0)
    {
        perror("ftell");
        goto failed;
    }
    return result;

failed:
    ErrorExit("Failed to read file size");
    return -1;
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
        else if (!strcmp(arg, "-extsides"))
        {
            extsides = true;
        }
        else if (!strcmp(arg, "-extblocks"))
        {
            extblocks = true;
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
        ErrorExit("No input WAD files specified.");
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
        "                      -extsides  Extended sidedefs limit\n"
        "                      -extblocks Extended blockmap limit\n"
        "\n");
}

// Checks for the lump names unique to PSX levels (and Doom 64).
static bool IsPlaystationWad(wad_file_t *wf)
{
    return EntryExists(wf, "LEAFS") >= 0 || EntryExists(wf, "LIGHTS") >= 0;
}

// LINEDEFS and SIDEDEFS lumps follow each other in Doom WADs. This is
// baked into the engine - Doom doesn't actually even look at the names.
static bool IsSidedefs(wad_file_t *wf, int count)
{
    return !strncmp(wf->entries[count].name, "SIDEDEFS", 8) && count > 0 &&
           !strncmp(wf->entries[count - 1].name, "LINEDEFS", 8);
}

static bool TryPack(wad_file_t *wf, unsigned int lump_index, FILE *out_file)
{
    uint32_t orig_lump_len = wf->entries[lump_index].length;

    if (lump_index + 1 < wf->num_entries && IsSidedefs(wf, lump_index + 1))
    {
        // We will write both LINEDEFS and SIDEDEFS when we reach
        // the next lump.
        SPAMMY_PRINTF("Deferred... (0%%)\n");
        return true;
    }
    else if (IsSidedefs(wf, lump_index))
    {
        bool success;

        SPAMMY_PRINTF("Packing");
        fflush(stdout);

        success = P_Pack(wf, lump_index);

        P_WriteLinedefs(out_file, &wf->entries[lump_index - 1]);
        P_WriteSidedefs(out_file, &wf->entries[lump_index]);

        if (success)
        {
            SPAMMY_PRINTF(
                " (%s), done.\n",
                PercentSmaller(orig_lump_len, wf->entries[lump_index].length));
        }
        else
        {
            // TODO: Print info message if compression failed.
            SPAMMY_PRINTF(" (0%%), failed.\n");
        }

        return true;
    }

    return false;
}

static bool TryStack(wad_file_t *wf, unsigned int lump_index, FILE *out_file)
{
    uint32_t orig_lump_len = wf->entries[lump_index].length;
    bool success;

    if (strncmp(wf->entries[lump_index].name, "BLOCKMAP", 8) != 0)
    {
        return false;
    }

    SPAMMY_PRINTF("Stacking ");
    fflush(stdout);

    success = B_Stack(wf, lump_index);
    B_WriteBlockmap(out_file, &wf->entries[lump_index]);

    if (success)
    {
        SPAMMY_PRINTF(
            "(%s), done.\n",
            PercentSmaller(orig_lump_len, wf->entries[lump_index].length));
    }
    else
    {
        // TODO: Print some kind of info message at the end of
        // compression to notify the user that not all blockmaps
        // could be stacked.
        SPAMMY_PRINTF("(0%%), failed.\n");
    }

    return true;
}

static bool TrySquash(wad_file_t *wf, unsigned int lump_index, FILE *out_file)
{
    uint32_t orig_lump_len = wf->entries[lump_index].length;
    uint8_t *temp;

    if (!S_IsGraphic(wf, lump_index))
    {
        return false;
    }

    SPAMMY_PRINTF("Squashing ");
    fflush(stdout);

    temp = S_Squash(wf, lump_index);
    wf->entries[lump_index].offset =
        WriteWadLump(out_file, temp, wf->entries[lump_index].length);
    free(temp);

    SPAMMY_PRINTF(
        "(%s), done.\n",
        PercentSmaller(orig_lump_len, wf->entries[lump_index].length));
    return true;
}

static bool Compress(const char *wadname)
{
    wad_file_t wf;
    unsigned int count;
    long orig_size, new_size;
    FILE *fstream;
    bool written;
    char *tempwad_name;

    if (!OpenWadFile(&wf, wadname))
    {
        return false;
    }
    if (wf.type == WAD_FILE_IWAD && !IwadWarning(wadname))
    {
        return false;
    }
    psx_format = IsPlaystationWad(&wf);

    orig_size = FileSize(wf.fp);

    fstream =
        OpenTempFile(outputwad != NULL ? outputwad : wadname, &tempwad_name);

    for (count = 0; count < wf.num_entries; count++)
    {
        SPAMMY_PRINTF("Adding: %-8.8s       ", wf.entries[count].name);
        fflush(stdout);
        written = false;

        if (!written && allowpack && !psx_format)
        {
            written = TryPack(&wf, count, fstream);
        }

        if (!written && allowstack)
        {
            written = TryStack(&wf, count, fstream);
        }

        if (!written && allowsquash)
        {
            written = TrySquash(&wf, count, fstream);
        }

        if (!written && wf.entries[count].length == 0)
        {
            SPAMMY_PRINTF("Empty (0%%).\n");
            wf.entries[count].offset = 0;
            written = true;
        }

        if (!written)
        {
            uint8_t *temp;
            SPAMMY_PRINTF("Storing ");
            fflush(stdout);
            temp = CacheLump(&wf, count);
            wf.entries[count].offset =
                WriteWadLump(fstream, temp, wf.entries[count].length);
            free(temp);
            SPAMMY_PRINTF("(0%%), done.\n");
        }
    }

    WriteWadDirectory(fstream, wf.type, wf.entries, wf.num_entries);
    new_size = FileSize(fstream);

    fclose(fstream);
    CloseWadFile(&wf);

    if (allowmerge)
    {
        char *tempwad2_name;

        OpenWadFile(&wf, tempwad_name);
        fstream = OpenTempFile(outputwad != NULL ? outputwad : wadname,
                               &tempwad2_name);

        SPAMMY_PRINTF("\nMerging identical lumps...");
        fflush(stdout);
        RebuildMergedWad(&wf, fstream);
        SPAMMY_PRINTF(" done.\n");

        new_size = FileSize(fstream);

        fclose(fstream);
        CloseWadFile(&wf);

        if (remove(tempwad_name) < 0)
        {
            // We couldn't remove the old temporary WAD, but this isn't
            // a fatal error. Report the error to the console, but keep
            // on going.
            perror("remove");
        }
        free(tempwad_name);
        tempwad_name = tempwad2_name;
    }

    // We only overwrite the original input file once we have generated
    // the new one as a temporary file, so that it takes place as a
    // simple rename() call. However! The Windows version of rename()
    // does not overwrite existing files, so we have to delete first.
#ifdef _WIN32
    if (remove(outputwad != NULL ? outputwad : wadname) < 0 && errno != ENOENT)
    {
        perror("remove");
        ErrorExit("Failed to remove old input file '%s' for rename.",
                  outputwad != NULL ? outputwad : wadname);
    }
#endif
    if (rename(tempwad_name, outputwad != NULL ? outputwad : wadname) < 0)
    {
        perror("rename");
        ErrorExit("Failed to rename temporary file '%s' to '%s'", tempwad_name,
                  outputwad != NULL ? outputwad : wadname);
    }

    free(tempwad_name);

    SPAMMY_PRINTF("*** %s is %ld bytes %s (%s) ***\n",
                  outputwad != NULL ? outputwad : wadname,
                  labs(orig_size - new_size),
                  new_size <= orig_size ? "smaller" : "larger",
                  PercentSmaller(orig_size, new_size));

    return true;
}

static bool TryUnpack(wad_file_t *wf, unsigned int lump_index, FILE *out_file,
                      bool *had_failure)
{
    if (lump_index + 1 < wf->num_entries && IsSidedefs(wf, lump_index + 1))
    {
        // Write on next loop.
        SPAMMY_PRINTF("Deferred...\n");
        return true;
    }
    else if (IsSidedefs(wf, lump_index))
    {
        bool success;

        SPAMMY_PRINTF("Unpacking");
        fflush(stdout);

        success = P_Unpack(wf, lump_index);

        P_WriteLinedefs(out_file, &wf->entries[lump_index - 1]);
        P_WriteSidedefs(out_file, &wf->entries[lump_index]);

        if (success)
        {
            SPAMMY_PRINTF(", done.\n");
        }
        else
        {
            SPAMMY_PRINTF(", failed.\n");
            *had_failure = true;
        }
        return true;
    }

    return false;
}

static bool TryUnstack(wad_file_t *wf, unsigned int lump_index, FILE *out_file,
                       bool *had_failure)
{
    bool success;

    if (strncmp(wf->entries[lump_index].name, "BLOCKMAP", 8) != 0)
    {
        return false;
    }

    SPAMMY_PRINTF("Unstacking");
    fflush(stdout);

    success = B_Unstack(wf, lump_index);
    B_WriteBlockmap(out_file, &wf->entries[lump_index]);

    if (success)
    {
        SPAMMY_PRINTF(", done.\n");
    }
    else
    {
        SPAMMY_PRINTF(", failed.\n");
        *had_failure = true;
    }

    return true;
}

static bool TryUnsquash(wad_file_t *wf, unsigned int lump_index, FILE *out_file)
{
    uint8_t *temp;

    if (!S_IsGraphic(wf, lump_index))
    {
        return false;
    }

    SPAMMY_PRINTF("Unsquashing");
    fflush(stdout);
    temp = S_Unsquash(wf, lump_index);
    wf->entries[lump_index].offset =
        WriteWadLump(out_file, temp, wf->entries[lump_index].length);
    free(temp);
    SPAMMY_PRINTF(", done\n");

    return true;
}

static bool Uncompress(const char *wadname)
{
    wad_file_t wf;
    char *tempwad_name;
    FILE *fstream;
    uint8_t *tempres;
    bool written, blockmap_failures = false, sidedefs_failures = false;
    unsigned int count;

    if (!OpenWadFile(&wf, wadname))
    {
        return false;
    }
    if (wf.type == WAD_FILE_IWAD && !IwadWarning(wadname))
    {
        return false;
    }
    psx_format = IsPlaystationWad(&wf);

    fstream = OpenTempFile(wadname, &tempwad_name);

    for (count = 0; count < wf.num_entries; count++)
    {
        written = false;

        SPAMMY_PRINTF("Adding: %-8.8s       ", wf.entries[count].name);
        fflush(stdout);

        if (allowpack && !psx_format)
        {
            written = TryUnpack(&wf, count, fstream, &sidedefs_failures);
        }
        if (!written && allowstack)
        {
            written = TryUnstack(&wf, count, fstream, &blockmap_failures);
        }
        if (!written && allowsquash)
        {
            written = TryUnsquash(&wf, count, fstream);
        }

        if (!written && wf.entries[count].length == 0)
        {
            SPAMMY_PRINTF("Empty.\n");
            wf.entries[count].offset = 0;
            written = true;
        }

        if (!written)
        {
            SPAMMY_PRINTF("Storing");
            fflush(stdout);
            tempres = CacheLump(&wf, count);
            wf.entries[count].offset =
                WriteWadLump(fstream, tempres, wf.entries[count].length);
            free(tempres);
            SPAMMY_PRINTF(", done.\n");
        }
    }

    WriteWadDirectory(fstream, wf.type, wf.entries, wf.num_entries);

    fclose(fstream);
    CloseWadFile(&wf);

#ifdef _WIN32
    // Windows version of rename() does not overwrite; we must delete first.
    if (remove(outputwad != NULL ? outputwad : wadname) < 0 && errno != ENOENT)
    {
        perror("remove");
        ErrorExit("Failed to remove old input file '%s' for rename.",
                  outputwad != NULL ? outputwad : wadname);
    }
#endif
    if (rename(tempwad_name, outputwad != NULL ? outputwad : wadname) < 0)
    {
        perror("rename");
        ErrorExit("Failed to rename temporary file '%s' to '%s'", tempwad_name,
                  outputwad != NULL ? outputwad : wadname);
    }

    free(tempwad_name);

    if (blockmap_failures)
    {
        SPAMMY_PRINTF("\nSome BLOCKMAP lumps could not be unstacked because "
                      "the decompressed\nversion would exceed the BLOCKMAP "
                      "size limit.\n");
        if (!extblocks)
        {
            SPAMMY_PRINTF("If this is not a vanilla WAD, you can try using "
                          "the -extblocks command\nline argument to use the "
                          "extended blockmap limit.\n");
        }
    }
    if (sidedefs_failures)
    {
        SPAMMY_PRINTF("\nSome SIDEDEFS lumps could not be unpacked because "
                      "the decompressed\nversion would exceed the sidedef "
                      "count limit.\n");
        if (!extsides)
        {
            SPAMMY_PRINTF("If this is not a vanilla WAD, you can try using "
                          "the -extsides command\nline argument to use the "
                          "extended sidedefs limit.\n");
        }
    }

    return true;
}

static const char *CompressionMethod(wad_file_t *wf, int lumpnum)
{
    if (wf->entries[lumpnum].length == 0)
    {
        return "Empty";
    }
    else if (!psx_format && IsSidedefs(wf, lumpnum))
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
    unsigned int i, j;

    if (!OpenWadFile(&wf, wadname))
    {
        return false;
    }
    psx_format = IsPlaystationWad(&wf);

    SPAMMY_PRINTF(
        " Number  Length  Offset      Method      Name        Shared\n"
        " ------  ------  ------      ------      ----        ------\n");

    for (i = 0; i < wf.num_entries; i++)
    {
        printf("%7d %7d  0x%08x  %-11s %-8.8s    ", i + 1, wf.entries[i].length,
               wf.entries[i].offset, CompressionMethod(&wf, i),
               wf.entries[i].name);

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

    CloseWadFile(&wf);

    return true;
}

// Find how much smaller something is: returns a percentage.
static const char *PercentSmaller(int before, int after)
{
    static char buf[32];
    char percentbuf[16];
    int permille;

    if (before == 0)
    {
        permille = 0;
    }
    else
    {
        permille = abs((1000 * (before - after)) / before);
    }

    // No change is represented as "0%" but a small change of less than
    // 0.1% is represented as "0.0%":
    if (after != before && permille < 100)
    {
        snprintf(percentbuf, sizeof(percentbuf), "%d.%d", permille / 10,
                 permille % 10);
    }
    else
    {
        snprintf(percentbuf, sizeof(percentbuf), "%d", permille / 10);
    }

    if (after <= before)
    {
        snprintf(buf, sizeof(buf), "%s%%", percentbuf);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%s%% larger!", percentbuf);
    }

    return buf;
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
