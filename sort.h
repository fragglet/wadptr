/*
 * Copyright(C) 2023 Simon Howard
 *
 * You can redistribute and/or modify this program under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, or any later version. This program is distributed WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 * Generic code for sorting data.
 */

#ifndef __SORT_H_INCLUDED__
#define __SORT_H_INCLUDED__

typedef int (*sort_compare_fn_t)(unsigned int index1, unsigned int index2,
                                 const void *callback_data);

unsigned int *MakeSortedMap(unsigned int num_elements,
                            sort_compare_fn_t compare_fn,
                            const void *callback_data);

#endif
