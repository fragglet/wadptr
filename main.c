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

/******************************* GLOBALS ***********************************/

int g_argc; /* global cmd-line list */
char **g_argv;
int filelist_index;
static char *wadname;
static char outputwad[256] = "";
int action;          /* list, compress, uncompress */
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

    printf("\n" /* Display startup message */
           "WADPTR - WAD Compressor  Version " VERSION "\n"
           "Copyright (c)1997-2011 Simon Howard.\n"
           "Enhancements/Portability by Andreas Dehmel\n"
           "http://www.soulsphere.org/projects/wadptr/\n");

    /* set error signals */
    signal(SIGINT, sig_func);
    signal(SIGSEGV, sig_func);
    signal(SIGFPE, sig_func);

    parsecmdline(); /* Check cmd-lines */

    for (index = filelist_index; index < g_argc; ++index)
    {
        wadname = g_argv[index];
        if (!(openwad(wadname)))
        {
            /* no problem with wad.. do whatever (compress, uncompress etc) */
            doaction();
        }
        fclose(wadfp);
    }

    return 0;
}

/****************** Command line handling, open wad etc. *******************/

/* Parse cmd-line options **************************************************/

int parsecmdline()
{
    int count;

    action = HELP; /* default to help if not told what to do */

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
            doaction();
            exit(0);
        }
        else
            errorexit("No input WAD file specified.\n");
    }

    if (action == UNCOMPRESS && allowmerge == 0)
        errorexit("Sorry, uncompressing will undo any lump merging on WADs.\n"
                  "The -nomerge command is not available with the "
                  "-u(uncompress) option.\n");

    return 0;
}

/* Open the original WAD ***************************************************/

int openwad(char *filename)
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
    a = readwad();
    if (a)
        return 1;

    printf("\n");
    return 0;
}

/* Does an action based on command line ************************************/

void doaction()
{
    switch (action)
    {
    case HELP:
        help();
        break;

    case LIST:
        list_entries();
        break;

    case UNCOMPRESS:
        uncompress();
        break;

    case COMPRESS:
        compress();
        break;
    }
}

/************************ Command line functions ***************************/

/* Display help ************************************************************/

void help()
{
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

void compress()
{
    int count, findshrink;
    long wadsize; /* wad size(to find % smaller) */
    FILE *fstream;
    int written = 0; /* if 0:write 1:been written 2:write silently */
    char *temp, resname[10], a[50];

    if (wad == IWAD)
        if (!iwad_warning())
            return;

    wadsize = diroffset + (ENTRY_SIZE * numentries);

    fstream = fopen(tempwad_name, "wb+");
    if (!fstream)
        errorexit("compress: Couldn't write a temporary file\n");

    memset(a, 0, 12);
    fwrite(a, 12, 1, fstream); /* temp header. */

    for (count = 0; count < numentries; count++)
    {
        strcpy(resname, convert_string8(wadentry[count]));
        written = 0;
        /* hide individual level entries */
        if (!islevelentry(resname))
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
            if (islevel(count))
            {
                printf("\tPacking ");
                fflush(stdout);
                findshrink = findlevelsize(resname);

                /* pack the level */
                p_pack(resname);

                findshrink = findperc(findshrink, findlevelsize(resname));
                printf("(%i%%), done.\n", findshrink);

                written = 2; /* silently write this lump (if any) */
            }
            if (!strcmp(resname, "SIDEDEFS"))
            {
                /* write the pre-packed sidedef entry */
                wadentry[count].offset = ftell(fstream);
                writesidedefs((sidedef_t *) p_sidedefres,
                              wadentry[count].length, fstream);
                free(p_sidedefres);
                written = 1;
            }
            if (!strcmp(resname, "LINEDEFS"))
            {
                /* write the pre-packed linedef entry */
                wadentry[count].offset = ftell(fstream);
                writelinedefs((linedef_t *) p_linedefres,
                              wadentry[count].length, fstream);
                free(p_linedefres);
                written = 1;
            }
        }

        if (allowsquash)
        {
            if (s_isgraphic(resname))
            {
                printf("\tSquashing ");
                fflush(stdout);
                findshrink = wadentry[count].length;

                temp = s_squash(resname);
                wadentry[count].offset = ftell(fstream); /*update dir */
                fwrite(temp, wadentry[count].length, 1, fstream);

                free(temp);
                findshrink = findperc(findshrink, wadentry[count].length);
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
            temp = cachelump(count);
            wadentry[count].offset = ftell(fstream); /*update dir */
            fwrite(temp, wadentry[count].length, 1, fstream);
            free(temp);
            if (written == 0) /* always 0% */
                printf("(0%%), done.\n");
        }
    }

    /* write new directory */
    diroffset = ftell(fstream);
    writewaddir(fstream);
    writewadheader(fstream);

    fclose(fstream);
    fclose(wadfp);

    if (allowmerge)
    {
        wadfp = fopen(tempwad_name, "rb+");
        printf("\nMerging identical lumps.. ");
        fflush(stdout);

        if (outputwad[0] == 0)
        {
            /* remove identical lumps: rebuild them back to
               the original filename */
            remove(wadname);
            rebuild(wadname);
        }
        else
        {
            rebuild(outputwad);
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

    findshrink = findperc(wadsize, diroffset + (numentries * ENTRY_SIZE));

    printf("*** %s is %i%% smaller ***\n", wadname, findshrink);
}

/* Uncompress a WAD ********************************************************/

void uncompress()
{
    char tempstr[50];
    FILE *fstream;
    char *tempres;
    char resname[10];
    int written; /* see compress() */
    long fileloc;
    int count;

    if (wad == IWAD)
        if (!iwad_warning())
            return;

    fstream = fopen(tempwad_name, "wb+");
    memset(tempstr, 0, 12);
    fwrite(tempstr, 12, 1, fstream); /* temp header */

    for (count = 0; count < numentries; count++)
    {
        strcpy(resname, convert_string8(wadentry[count]));

        written = 0;

        if (islevelentry(resname))
            written = 2; /* silently write (level entry) */
        else
        {
            printf("Adding: %s       ", resname);
            fflush(stdout);
        }

        if (allowpack)
        {
            if (islevel(count))
            {
                printf("\tUnpacking");
                fflush(stdout);
                p_unpack(resname);
                printf(", done.\n");
                written = 2; /* silently write this lump */
            }
            if (!strcmp(resname, "SIDEDEFS"))
            {
                wadentry[count].offset = ftell(fstream);
                writesidedefs((sidedef_t *) p_sidedefres,
                              wadentry[count].length, fstream);
                free(p_sidedefres);
                written = 1;
            }
            if (!strcmp(resname, "LINEDEFS"))
            {
                wadentry[count].offset = ftell(fstream);
                writelinedefs((linedef_t *) p_linedefres,
                              wadentry[count].length, fstream);
                free(p_linedefres);
                written = 1;
            }
        }
        if (allowsquash)
            if (s_isgraphic(resname))
            {
                printf("\tUnsquashing");
                fflush(stdout);
                tempres = s_unsquash(resname);
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
            tempres = cachelump(count);
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
    writewaddir(fstream);
    writewadheader(fstream);

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

void list_entries()
{
    int count, count2;
    int ypos;
    char resname[10];

    printf(" Number Length  Offset          Method      Name        Shared\n"
           " ------ ------  ------          ------      ----        ------\n");

    for (count = 0; count < numentries; count++)
    {
        strcpy(resname, convert_string8(wadentry[count]));
        if (islevelentry(resname))
            continue;
        ypos = wherey();

        /* wad entry number */
        printf(" %i  \t", count + 1);

        /* size */
        if (islevel(count))
        {
            /* the whole level not just the id lump */
            printf("%i\t", findlevelsize(resname));
        }
        else
        {
            /*not a level, doesn't matter */
            printf("%ld\t", wadentry[count].length);
        }

        /* file offset */
        printf("0x%08lx     \t", wadentry[count].offset);

        /* compression method */
        if (islevel(count))
        {
            /* this is a level */
            if (p_ispacked(resname))
                printf("Packed      ");
            else
                printf("Unpacked    ");
        }
        else
        {
            if (s_isgraphic(resname))
            {
                /* this is a graphic */
                if (s_is_squashed(resname))
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
                printf("%s\n", convert_string8(wadentry[count2]));
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

int findperc(int before, int after)
{
    double perc;

    perc = 1 - (((double) after) / before);

    return (int) (100 * perc);
}

/* Warning not to change IWAD **********************************************/

int iwad_warning()
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
