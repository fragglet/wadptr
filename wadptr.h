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
 *                     Header file for the project                        *
 *                                                                        *
 **************************************************************************/

/***************************** INCLUDES ************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>

#include "errors.h"
#include "lumps.h"
#include "waddir.h"
#include "wadmerge.h"

/****************************** DEFINES ************************************/

/** MAIN.C **/

#define VERSION "2.4"

#define HELP       0
#define COMPRESS   1
#define UNCOMPRESS 2
#define LIST       3

/****************************** GLOBALS ************************************/

/** MAIN.C **/

extern int g_argc; /* global cmd-line list */
extern char **g_argv;
extern char filespec[256]; /* -tweak file name */
extern int allowpack;      /* level packing on */
extern int allowsquash;    /* picture squashing on */
extern int allowmerge;     /* lump merging on */

/******************************* PROTOTYPES ********************************/

/** MAIN.C **/

int ParseCommandLine();
int OpenWad();
void DoAction();
void eachwad(char *filespec);

void Help();
void Compress();
void Uncompress();
void ListEntries();

int IwadWarning();

int FindPerc(int before, int after);

extern const char *pwad_name;
extern const char *iwad_name;

int wherex(void);
int wherey(void);
int gotoxy(int x, int y);

#ifdef NORMALUNIX
#define DIRSEP "/"
#define EXTSEP "."
#define CURDIR "."
#else
#define DIRSEP "\\"
#define EXTSEP "."
#define CURDIR "."
#endif

#define READ_SHORT(p) (short) ((p)[0] | ((p)[1] << 8))
#define READ_LONG(p) \
    (long) ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24))
#define WRITE_SHORT(p, s) (p)[0] = (s) &0xff, (p)[1] = ((s) >> 8) & 0xff
#define WRITE_LONG(p, l)                            \
    (p)[0] = (l) &0xff, (p)[1] = ((l) >> 8) & 0xff, \
    (p)[2] = ((l) >> 16) & 0xff, (p)[3] = ((l) >> 24) & 0xff
