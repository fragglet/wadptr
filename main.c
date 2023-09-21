/**************************************************************************
 *                                                                        *
 * Copyright(C) 1998-2011 Simon Howard, Andreas Dehmel                    *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 2 of the License, or      *
 * (at your option) any later version.                                    *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program; if not, write to the Free Software            *
 * Foundation, Inc, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA *
 *                                                                        *
 *                          The WADPTR project                            *
 *                                                                        *
 * Compresses Doom WAD files through several methods:                     *
 *                                                                        *
 * - Merges identical lumps (see wadmerge.c)                              *
 * - 'Squashes' graphics (see lumps.c)                                    *
 * - Packs the sidedefs in levels (see lumps.c)                           *
 *                                                                        *
 **************************************************************************/

/******************************* INCLUDES **********************************/

#include "wadptr.h"

static void Compress();
static void DoAction();
static int FindPerc(int before, int after);
static void Help();
static int IwadWarning();
static void ListEntries();
static int OpenWad();
static int ParseCommandLine();
static void Uncompress();

/******************************* GLOBALS ***********************************/

int g_argc; /* global cmd-line list */
char **g_argv;
int filelist_index;
static char *wadname;
static char outputwad[256] = "";
int action;          /* list, Compress, Uncompress */
int allowpack = 1;   /* level packing on */
int allowsquash = 1; /* picture squashing on */
int allowmerge = 1;  /* lump merging on */

const char *pwad_name = "PWAD";
const char *iwad_name = "IWAD";

static const char *tempwad_name = "~wptmp" EXTSEP "wad";

/* Main ********************************************************************/

int main(int argc, char *argv[])
{
    int index;

    g_argc = argc; /* Set global cmd-line list */
    g_argv = argv;

    /* set error signals */
    signal(SIGINT, SignalFunction);
    signal(SIGSEGV, SignalFunction);
    signal(SIGFPE, SignalFunction);

    ParseCommandLine(); /* Check cmd-lines */

    for (index = filelist_index; index < g_argc; ++index)
    {
        wadname = g_argv[index];
        if (!(OpenWad(wadname)))
        {
            /* no problem with wad.. do whatever (Compress, Uncompress etc) */
            DoAction();
        }
        fclose(wadfp);
    }

    return 0;
}

/****************** Command line handling, open wad etc. *******************/

/* Parse cmd-line options **************************************************/

static int ParseCommandLine()
{
    int count;

    action = HELP; /* default to Help if not told what to do */

    count = 1;
    filelist_index = -1;
    while (count < g_argc)
    {
        if ((!strcmp(g_argv[count], "-Help")) || (!strcmp(g_argv[count], "-h")))
            action = HELP;

        if ((!strcmp(g_argv[count], "-list")) || (!strcmp(g_argv[count], "-l")))
            action = LIST;

        if ((!strcmp(g_argv[count], "-Compress")) ||
            (!strcmp(g_argv[count], "-c")))
            action = COMPRESS;

        if ((!strcmp(g_argv[count], "-Uncompress")) ||
            (!strcmp(g_argv[count], "-u")))
            action = UNCOMPRESS;

        /* specific disabling */
        if (!strcmp(g_argv[count], "-nomerge"))
            allowmerge = 0;
        if (!strcmp(g_argv[count], "-nosquash"))
            allowsquash = 0;
        if (!strcmp(g_argv[count], "-nopack"))
            allowpack = 0;

        if (g_argv[count][0] != '-')
        {
            filelist_index = count;
            break;
        }

        if ((!strcmp(g_argv[count], "-output")) ||
            (!strcmp(g_argv[count], "-o")))
            strcpy(outputwad, g_argv[++count]);

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

    if (action == UNCOMPRESS && allowmerge == 0)
        ErrorExit("Sorry, uncompressing will undo any lump merging on WADs.\n"
                  "The -nomerge command is not available with the "
                  "-u(Uncompress) option.\n");

    return 0;
}

/* Open the original WAD ***************************************************/

static int OpenWad(char *filename)
{
    int a;

    if (!action)
    {
        /* no action but i've got a wad.. whats in it? default to list */
        action = LIST;
    }

    wadfp = fopen(filename, "rb+");
    if (!wadfp)
    {
        printf("%s does not exist\n", wadname);
        return 1;
    }

    printf("\nSearching WAD: %s\n", filename);
    a = ReadWad();
    if (a)
        return 1;

    printf("\n");
    return 0;
}

/* Does an action based on command line ************************************/

static void DoAction()
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

/************************ Command line functions ***************************/

/* Display Help ************************************************************/

static void Help()
{
    printf("\n"
           "WADPTR - WAD Compressor  Version " VERSION "\n"
           "Copyright (c)1997-2011 Simon Howard.\n"
           "Enhancements/Portability by Andreas Dehmel\n"
           "http://www.soulsphere.org/projects/wadptr/\n");

    printf("\n"
           "Usage:  WADPTR inputwad [outputwad] options\n"
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

/* Compress a WAD **********************************************************/

static void Compress()
{
    int count, findshrink;
    long wadsize; /* wad size(to find % smaller) */
    FILE *fstream;
    int written = 0; /* if 0:write 1:been written 2:write silently */
    char *temp, resname[10], a[50];

    if (wad == IWAD)
        if (!IwadWarning())
            return;

    wadsize = diroffset + (ENTRY_SIZE * numentries);

    fstream = fopen(tempwad_name, "wb+");
    if (!fstream)
        ErrorExit("Compress: Couldn't write a temporary file\n");

    memset(a, 0, 12);
    fwrite(a, 12, 1, fstream); /* temp header. */

    for (count = 0; count < numentries; count++)
    {
        strcpy(resname, ConvertString8(wadentry[count]));
        written = 0;
        /* hide individual level entries */
        if (!IsLevelEntry(resname))
        {
            printf("Adding: %s       ", resname);
            fflush(stdout);
        }
        else
        {
            /* silently write entry: level entry */
            written = 2;
        }

        if (allowpack)
        {
            if (IsLevel(count))
            {
                printf("\tPacking ");
                fflush(stdout);
                findshrink = FindLevelSize(resname);

                /* pack the level */
                P_Pack(resname);

                findshrink = FindPerc(findshrink, FindLevelSize(resname));
                printf("(%i%%), done.\n", findshrink);

                written = 2; /* silently write this lump (if any) */
            }
            if (!strcmp(resname, "SIDEDEFS"))
            {
                /* write the pre-packed sidedef entry */
                wadentry[count].offset = ftell(fstream);
                WriteSidedefs((sidedef_t *) p_sidedefres,
                              wadentry[count].length, fstream);
                free(p_sidedefres);
                written = 1;
            }
            if (!strcmp(resname, "LINEDEFS"))
            {
                /* write the pre-packed linedef entry */
                wadentry[count].offset = ftell(fstream);
                WriteLinedefs((linedef_t *) p_linedefres,
                              wadentry[count].length, fstream);
                free(p_linedefres);
                written = 1;
            }
        }

        if (allowsquash)
        {
            if (S_IsGraphic(resname))
            {
                printf("\tSquashing ");
                fflush(stdout);
                findshrink = wadentry[count].length;

                temp = S_Squash(resname);
                wadentry[count].offset = ftell(fstream); /*update dir */
                fwrite(temp, wadentry[count].length, 1, fstream);

                free(temp);
                findshrink = FindPerc(findshrink, wadentry[count].length);
                printf("(%i%%), done.\n", findshrink);
                written = 1;
            }
        }

        if ((written == 0) || (written == 2))
        {
            if (written == 0)
            {
                printf("\tStoring ");
                fflush(stdout);
            }
            temp = CacheLump(count);
            wadentry[count].offset = ftell(fstream); /*update dir */
            fwrite(temp, wadentry[count].length, 1, fstream);
            free(temp);
            if (written == 0) /* always 0% */
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
        printf("\nMerging identical lumps.. ");
        fflush(stdout);

        if (outputwad[0] == 0)
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
        printf("done.\n");
        fclose(wadfp);
        remove(tempwad_name);
    }
    else
    {
        if (outputwad[0] == 0)
        {
            remove(wadname);
            rename(tempwad_name, wadname);
        }
        else
        {
            rename(tempwad_name, outputwad);
        }
    }

    wadfp = fopen(wadname, "rb+"); /* so there is something to close */

    findshrink = FindPerc(wadsize, diroffset + (numentries * ENTRY_SIZE));

    printf("*** %s is %i%% smaller ***\n", wadname, findshrink);
}

/* Uncompress a WAD ********************************************************/

static void Uncompress()
{
    char tempstr[50];
    FILE *fstream;
    char *tempres;
    char resname[10];
    int written; /* see Compress() */
    long fileloc;
    int count;

    if (wad == IWAD)
        if (!IwadWarning())
            return;

    fstream = fopen(tempwad_name, "wb+");
    memset(tempstr, 0, 12);
    fwrite(tempstr, 12, 1, fstream); /* temp header */

    for (count = 0; count < numentries; count++)
    {
        strcpy(resname, ConvertString8(wadentry[count]));

        written = 0;

        if (IsLevelEntry(resname))
            written = 2; /* silently write (level entry) */
        else
        {
            printf("Adding: %s       ", resname);
            fflush(stdout);
        }

        if (allowpack)
        {
            if (IsLevel(count))
            {
                printf("\tUnpacking");
                fflush(stdout);
                P_Unpack(resname);
                printf(", done.\n");
                written = 2; /* silently write this lump */
            }
            if (!strcmp(resname, "SIDEDEFS"))
            {
                wadentry[count].offset = ftell(fstream);
                WriteSidedefs((sidedef_t *) p_sidedefres,
                              wadentry[count].length, fstream);
                free(p_sidedefres);
                written = 1;
            }
            if (!strcmp(resname, "LINEDEFS"))
            {
                wadentry[count].offset = ftell(fstream);
                WriteLinedefs((linedef_t *) p_linedefres,
                              wadentry[count].length, fstream);
                free(p_linedefres);
                written = 1;
            }
        }
        if (allowsquash)
            if (S_IsGraphic(resname))
            {
                printf("\tUnsquashing");
                fflush(stdout);
                tempres = S_Unsquash(resname);
                wadentry[count].offset = ftell(fstream);
                fwrite(tempres, wadentry[count].length, 1, fstream);
                free(tempres);
                printf(", done\n");
                written = 1;
            }
        if ((written == 0) || (written == 2))
        {
            if (!written)
            {
                printf("\tStoring %s", resname);
                fflush(stdout);
            }
            tempres = CacheLump(count);
            fileloc = ftell(fstream);
            fwrite(tempres, wadentry[count].length, 1, fstream);
            free(tempres);
            wadentry[count].offset = fileloc;
            if (!written)
                printf(", done.\n");
        }
    }
    /* update the directory location */
    diroffset = ftell(fstream);
    WriteWadDirectory(fstream);
    WriteWadHeader(fstream);

    fclose(fstream);
    fclose(wadfp);

    if (outputwad[0] == 0)
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

/* List WAD entries ********************************************************/

static void ListEntries()
{
    int count, count2;
    int ypos;
    char resname[10];

    printf(" Number Length  Offset          Method      Name        Shared\n"
           " ------ ------  ------          ------      ----        ------\n");

    for (count = 0; count < numentries; count++)
    {
        strcpy(resname, ConvertString8(wadentry[count]));
        if (IsLevelEntry(resname))
            continue;
        ypos = wherey();

        /* wad entry number */
        printf(" %i  \t", count + 1);

        /* size */
        if (IsLevel(count))
        {
            /* the whole level not just the id lump */
            printf("%i\t", FindLevelSize(resname));
        }
        else
        {
            /*not a level, doesn't matter */
            printf("%ld\t", wadentry[count].length);
        }

        /* file offset */
        printf("0x%08lx     \t", wadentry[count].offset);

        /* compression method */
        if (IsLevel(count))
        {
            /* this is a level */
            if (P_IsPacked(resname))
                printf("Packed      ");
            else
                printf("Unpacked    ");
        }
        else
        {
            if (S_IsGraphic(resname))
            {
                /* this is a graphic */
                if (S_IsSquashed(resname))
                    printf("Squashed    ");
                else
                    printf("Unsquashed  ");
            }
            else
            {
                /* ordinary lump w/no compression */
                printf("Stored      ");
            }
        }

        /* resource name */
        printf("%s%s\t", resname, (strlen(resname) < 4) ? "\t" : "");

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
                printf("%s\n", ConvertString8(wadentry[count2]));
                break;
            }
        }
        /* no identical lumps if it reached the last one */
        if (count2 == count)
            printf("No\n");
    }
}

/************************** Misc. functions ********************************/

/* Find how much smaller something is: returns a percentage ****************/

static int FindPerc(int before, int after)
{
    double perc;

    perc = 1 - (((double) after) / before);

    return (int) (100 * perc);
}

/* Warning not to change IWAD **********************************************/

static int IwadWarning()
{
    char tempchar;
    printf("Are you sure you want to change the main IWAD?");
    fflush(stdout);
    while (1)
    {
        tempchar = fgetc(stdin);
        if ((tempchar == 'Y') || (tempchar == 'y'))
        {
            printf("\n");
            return 1;
        }
        if ((tempchar == 'N') || (tempchar == 'n'))
        {
            printf("\n");
            return 0;
        }
    }
}

int wherex(void)
{
    return 0;
}

int wherey(void)
{
    return 0;
}

int gotoxy(int x, int y)
{
    printf("                \r");
    return 0;
}
