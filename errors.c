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
 *                          The WADPTR project                            *
 *                                                                        *
 * Error handling routines:                                               *
 *                                                                        *
 **************************************************************************/

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"

/* Display an error (Last remnant of the DMWAD heritage) ******************/

void ErrorExit(char *s, ...)
{
    va_list args;
    va_start(args, s);

    vfprintf(stderr, s, args);

    exit(0xff);
}
