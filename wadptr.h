/**************************************************************************
 *                          The WADPTR project                            *
 *                                                                        *
 *                     Header file for the project                        *
 *                                                                        *
 **************************************************************************/

/***************************** INCLUDES ************************************/

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <stdarg.h>
#include <dirent.h>
#include <string.h>

#include "lumps.h"
#include "waddir.h"
#include "wadmerge.h"
#include "errors.h"

/****************************** DEFINES ************************************/

/** MAIN.C **/

#define VERSION "2.1"

#define HELP 0
#define COMPRESS 1
#define UNCOMPRESS 2
#define LIST 3

/****************************** GLOBALS ************************************/

/** MAIN.C **/

extern int g_argc;                      // global cmd-line list
extern char **g_argv;
extern char wadname[50];
extern char filespec[50];                  // -tweak file name

extern int allowpack;                     // level packing on
extern int allowsquash;                   // picture squashing on
extern int allowmerge;                    // lump merging on


/******************************* PROTOTYPES ********************************/

/** MAIN.C **/

int parsecmdline();
int openwad();
void doaction();
void eachwad(char *filespec);

void help();
void compress();
void uncompress();
void list_entries();

char *find_filename(char *s);
int filecmp(char *filename, char *templaten);
void *__crt0_glob_function();   // needed to disable globbing(expansion of
                                  // wildcards on the command line)
int iwad_warning();

