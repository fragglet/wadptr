/**************************************************************************
 *                     WAD loading and saving routines                    *
 *                                                                        *
 * WAD loading and reading routines: by me, me, me!                       *
 *                                                                        *
 **************************************************************************/

#ifndef __WADDIR_H_INCLUDED__
#define __WADDIR_H_INCLUDED__

/*************************** Includes *************************************/

#include <stdio.h>
#include <stdlib.h>

#include "errors.h"

/*************************** Defines **************************************/

#define MAXENTRIES 5000

typedef enum { IWAD, PWAD, NONWAD } wadtype;

/*************************** Structs *************************************/

typedef struct {
    long offset;
    long length;
    char name[8];
} entry_t;

/* portable structure IO (see lumps.h) */
#define ENTRY_OFF  0
#define ENTRY_LEN  4
#define ENTRY_NAME 8
#define ENTRY_SIZE 16

/*************************** Prototypes ***********************************/

int ReadWad();
void WriteWad();
char *ConvertString8(entry_t entry);
entry_t FindInfo(char *entrytofind);
void AddEntry(entry_t entry);
int EntryExists(char *entrytofind);
void *CacheLump(int entrynum);
void CopyWad(char *newfile);

int ReadWadHeader(FILE *fp);
int WriteWadHeader(FILE *fp);
int ReadWadDirectory(FILE *fp);
int ReadWadEntry(FILE *fp, entry_t *entry);
int WriteWadDirectory(FILE *fp);
int WriteWadEntry(FILE *fp, entry_t *entry);

int IsLevel(int entry);
int IsLevelEntry(char *s);
int FindLevelSize(char *s);

/*************************** Globals *************************************/

/** WADDIR.C **/

extern FILE *wadfp;
extern char picentry[8];
extern long numentries, diroffset;
extern entry_t wadentry[MAXENTRIES];
extern wadtype wad;

#endif
