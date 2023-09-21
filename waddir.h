/*
 * WAD loading and reading routines: by me, me, me!
 */

#ifndef __WADDIR_H_INCLUDED__
#define __WADDIR_H_INCLUDED__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"

#define MAXENTRIES 5000

typedef enum { IWAD, PWAD, NONWAD } wadtype;

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

bool ReadWad();
void WriteWad();
char *ConvertString8(entry_t entry);
int EntryExists(char *entrytofind);
void *CacheLump(int entrynum);

int WriteWadHeader(FILE *fp);
int WriteWadDirectory(FILE *fp);

bool IsLevel(int entry);
bool IsLevelEntry(char *s);
int FindLevelSize(char *s);

extern FILE *wadfp;
extern char picentry[8];
extern long numentries, diroffset;
extern entry_t wadentry[MAXENTRIES];
extern wadtype wad;

#endif
