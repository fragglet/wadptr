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

#include <stdlib.h>

#include "sort.h"
#include "wadptr.h"

static void SortMapElements(unsigned int *elements, size_t num_elements,
                            sort_compare_fn_t compare_fn,
                            const void *callback_data)
{
    unsigned int pivot, pivot_index;
    size_t arr1_len;
    int i;

    if (num_elements <= 1)
    {
        // no-op.
        return;
    }

    // Pick pivot from the middle, in case the list is already sorted
    // (or mostly sorted)
    pivot_index = num_elements / 2;
    pivot = elements[pivot_index];
    elements[pivot_index] = elements[num_elements - 1];

    for (i = 0, arr1_len = 0; i < num_elements - 1; i++)
    {
        if (compare_fn(elements[i], pivot, callback_data) < 0)
        {
            unsigned int tmp = elements[i];
            elements[i] = elements[arr1_len];
            elements[arr1_len] = tmp;
            ++arr1_len;
        }
    }

    elements[num_elements - 1] = elements[arr1_len];
    elements[arr1_len] = pivot;

    SortMapElements(elements, arr1_len, compare_fn, callback_data);
    SortMapElements(&elements[arr1_len + 1], num_elements - arr1_len - 1,
                    compare_fn, callback_data);
}

// Generates a "sorted map", which is an array that maps from original
// element index to the index that element would be at if the array was
// sorted.
unsigned int *MakeSortedMap(unsigned num_elements,
                            sort_compare_fn_t compare_fn,
                            const void *callback_data)
{
    unsigned int *result = ALLOC_ARRAY(unsigned int, num_elements);
    unsigned int i;

    for (i = 0; i < num_elements; i++)
    {
        result[i] = i;
    }

    SortMapElements(result, num_elements, compare_fn, callback_data);

    return result;
}

