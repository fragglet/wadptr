/**************************************************************************
 *                  The WADPTR project : LUMPS.C                          *
 *                      Header file for LUMPS.C                           *
 *                                                                        *
 *            Functions for compressing individual lumps                  *
 *                                                                        *
 * P_* : Sidedef packing extension routines. Combines sidedefs which are  *
 *       identical in a level, and shares them between multiple linedefs  *
 *                                                                        *
 * S_* : Graphic squashing routines. Combines identical columns in        *
 *       graphic lumps to make them smaller                               *
 *                                                                        *
 **************************************************************************/

#ifndef __LUMPS_H_INCLUDED__
#define __LUMPS_H_INCLUDED__

/****************************** Includes **********************************/

#include<stdio.h>
#include<stdlib.h>

#include "waddir.h"
#include "errors.h"

/****************************** Globals ***********************************/

extern char *p_linedefres;         // the new linedef resource
extern char *p_sidedefres;         // the new sidedef resource

/***************************** Prototypes *********************************/

void p_pack(char *levelname);
void p_unpack(char *resname);
int p_ispacked(char *s);

char *s_squash(char *s);
char *s_unsquash(char *s);
int s_is_squashed(char *s);
int s_isgraphic(char *s);

/******************************* Structs **********************************/

typedef struct
{
         short xoffset;
         short yoffset;
         char upper[8];
         char middle[8];
         char lower[8];
         short sector_ref;
} sidedef_t;

typedef struct
{
         short vertex1;
         short vertex2;
         short flags;
         short types;
         short tag;
         short sidedef1;
         short sidedef2;
} linedef_t;

#endif
