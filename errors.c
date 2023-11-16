/*
 * Copyright(C) 1998-2023 Simon Howard, Andreas Dehmel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"

static const char *context_filename, *context_lump;

void SetContextFilename(const char *filename)
{
    context_filename = filename;
    context_lump = NULL;
}

void SetContextLump(const char *lump)
{
    context_lump = lump;
}

static void PrintContext(void)
{
    if (context_filename != NULL)
    {
        fprintf(stderr, "%s: ", context_filename);
        if (context_lump != NULL)
        {
            fprintf(stderr, "%.8s: ", context_lump);
        }
    }
}

void Warning(char *s, ...)
{
    va_list args;
    va_start(args, s);

    PrintContext();
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");
}

void ErrorExit(char *s, ...)
{
    va_list args;
    va_start(args, s);

    puts("");
    PrintContext();
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");

    exit(1);
}
