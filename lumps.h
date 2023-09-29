/*
 * Functions for compressing individual lumps
 *
 * P_* : Sidedef packing extension routines. Combines sidedefs which are
 *       identical in a level, and shares them between multiple linedefs
 *
 * S_* : Graphic squashing routines. Combines identical columns in
 *       graphic lumps to make them smaller
 */

#ifndef __LUMPS_H_INCLUDED__
#define __LUMPS_H_INCLUDED__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"
#include "waddir.h"

extern char *p_linedefres; /* the new linedef resource */
extern char *p_sidedefres; /* the new sidedef resource */

void P_Pack(char *levelname);
void P_Unpack(char *resname);
bool P_IsPacked(char *s);

char *S_Squash(int entrynum);
char *S_Unsquash(int entrynum);
bool S_IsSquashed(int entrynum);
bool S_IsGraphic(int entrynum);

typedef struct {
    short xoffset;
    short yoffset;
    char upper[8];
    char middle[8];
    char lower[8];
    unsigned short sector_ref;
} sidedef_t;

typedef struct {
    unsigned short vertex1;
    unsigned short vertex2;
    unsigned short flags;
    unsigned short types;
    unsigned short tag;
    unsigned short sidedef1;
    unsigned short sidedef2;
} linedef_t;

#define NO_SIDEDEF ((unsigned short) -1)

/*
 * Portable structure IO
 * (to handle endianness; also neither struct is a multiple of 4 in size)
 */
#define SDEF_XOFF   0
#define SDEF_YOFF   2
#define SDEF_UPPER  4
#define SDEF_MIDDLE 12
#define SDEF_LOWER  20
#define SDEF_SECTOR 28
#define SDEF_SIZE   30

#define LDEF_VERT1 0
#define LDEF_VERT2 2
#define LDEF_FLAGS 4
#define LDEF_TYPES 6
#define LDEF_TAG   8
#define LDEF_SDEF1 10
#define LDEF_SDEF2 12
#define LDEF_SIZE  14

int WriteLinedefs(linedef_t *lines, int bytes, FILE *fp);
int WriteSidedefs(sidedef_t *sides, int bytes, FILE *fp);

#endif
