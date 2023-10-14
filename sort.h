/*
 * Copyright(C) 2023 Simon Howard
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
 *
 *
 * Generic code for sorting data.
 */

#ifndef __SORT_H_INCLUDED__
#define __SORT_H_INCLUDED__

typedef int (*sort_compare_fn_t)(unsigned int index1, unsigned int index2,
                                 const void *callback_data);

unsigned int *MakeSortedMap(unsigned num_elements, sort_compare_fn_t compare_fn,
                            const void *callback_data);

#endif
