/**************************************************************************
 *                     The WADPTR Project: WADMERGE.C                     *
 *                                                                        *
 *  Compresses WAD files by merging identical lumps in a WAD file and     *
 *  sharing them between multiple wad directory entries.                  *
 *                                                                        *
 **************************************************************************/

#ifndef __WADMERGE_H_INCLUDED__
#define __WADMERGE_H_INCLUDED__

/***************************** Includes ***********************************/

#include <stdio.h>
#include <stdlib.h>
#ifndef ANSILIBS
#include <conio.h>
#endif

#include "waddir.h"
#include "errors.h"

/****************************** Defines ***********************************/

#define MAXLINKS 50000

/***************************** Prototypes *********************************/

void rebuild(char *newname);

#endif

