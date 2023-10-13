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
 *
 *
 * Functions for compressing ("stacking") BLOCKMAP lumps.
 */

#include <stdint.h>
#include <string.h>

#include "blockmap.h"
#include "sort.h"
#include "waddir.h"
#include "wadptr.h"

// Blockmap lumps have a vanilla limit of ~64KiB; the 16-bit integers
// are interpreted by the engine as signed integers.
#define MAX_BLOCKMAP_OFFSET 0x7fff /* 16-bit ints */

typedef struct {
    uint16_t *elements;
    size_t len;
} block_t;

typedef struct {
    uint16_t *elements;
    size_t len, size;
    int num_blocks;
} blockmap_t;

static blockmap_t ReadBlockmap(int lumpnum, FILE *fp);
static bool WriteBlockmap(const blockmap_t *blockmap, FILE *fp);

static blockmap_t blockmap_result;

static block_t *MakeBlocklist(const blockmap_t *blockmap)
{
    block_t *blocklist;
    block_t *block;
    int i, start_index, end_index;

    blocklist = ALLOC_ARRAY(block_t, blockmap->num_blocks);

    for (i = 0; i < blockmap->num_blocks; i++)
    {
        block = &blocklist[i];
        // TODO: Need to do some array bounds checking here.
        start_index = blockmap->elements[4 + i];
        block->elements = &blockmap->elements[start_index];

        end_index = start_index;
        while (blockmap->elements[end_index] != 0xffff)
        {
            ++end_index;
        }
        block->len = end_index - start_index + 1;
    }

    return blocklist;
}

// TODO: We don't really need it right now, but this doesn't update the
// blockmap->blocklist[]->elements pointers when reallocating.
static void AppendBlockmapElements(blockmap_t *blockmap, uint16_t *elements,
                                   size_t count)
{
    while (blockmap->len + count > blockmap->size)
    {
        blockmap->size *= 2;
        blockmap->elements =
            REALLOC_ARRAY(uint16_t, blockmap->elements, blockmap->size);
    }

    memcpy(&blockmap->elements[blockmap->len], elements,
           count * sizeof(uint16_t));
    blockmap->len += count;
}

static int FindIdenticalBlock(const block_t *blocklist, size_t num_blocks,
                              const block_t *block)
{
    int i;

    for (i = 0; i < num_blocks; i++)
    {
        const block_t *ib = &blocklist[i];

        // We allow suffixes, but unless the blockmap is in "engine
        // format" it probably won't make a difference.
        if (block->len > ib->len)
        {
            continue;
        }

        if (!memcmp(block->elements, ib->elements + ib->len - block->len,
                    block->len * 2))
        {
            return i;
        }
    }

    return -1;
}

static blockmap_t RebuildBlockmap(const blockmap_t *blockmap, bool compress)
{
    blockmap_t result;
    block_t *blocklist;
    uint16_t *block_offsets;
    int i;

    blocklist = MakeBlocklist(blockmap);

    result.size = blockmap->len;
    result.elements = ALLOC_ARRAY(uint16_t, result.size);
    result.len = 4 + blockmap->num_blocks;
    result.num_blocks = blockmap->num_blocks;
    block_offsets = &result.elements[4];

    // Header is identical:
    memcpy(result.elements, blockmap->elements, 4 * sizeof(uint16_t));

    for (i = 0; i < blockmap->num_blocks; i++)
    {
        const block_t *block = &blocklist[i];
        int match_index = -1;

        if (compress)
        {
            match_index = FindIdenticalBlock(blocklist, i, block);
        }

        if (match_index >= 0)
        {
            // Copy the offset of the other block, but if it's a suffix
            // match then we need to offset.
            block_offsets[i] = block_offsets[match_index] +
                               blocklist[match_index].len - block->len;
        }
        else if (result.len > MAX_BLOCKMAP_OFFSET)
        {
            free(result.elements);
            result.elements = NULL;
            result.len = 0;
            break;
        }
        else
        {
            block_offsets[i] = result.len;
            AppendBlockmapElements(&result, block->elements, block->len);
            block_offsets = &result.elements[4];
        }
    }

    free(blocklist);

    return result;
}

bool B_Stack(int lumpnum)
{
    blockmap_t blockmap = ReadBlockmap(lumpnum, wadfp);
    if (blockmap.len == 0)
    {
        // TODO: Need some better error handling.
        ErrorExit("failed to read blockmap?");
    }

    blockmap_result = RebuildBlockmap(&blockmap, true);
    if (blockmap_result.len == 0)
    {
        blockmap_result = blockmap;
        return false;
    }

    // Check the rebuilt blockmap really is smaller. If it was built
    // using eg. ZokumBSP, the original is probably better than what
    // we've produced.
    if (blockmap_result.len < blockmap.len)
    {
        free(blockmap_result.elements);
        blockmap_result = blockmap;
        return true;
    }

    free(blockmap.elements);
    return true;
}

bool B_Unstack(int lumpnum)
{
    blockmap_t blockmap = ReadBlockmap(lumpnum, wadfp);
    if (blockmap.len == 0)
    {
        // TODO: Need some better error handling.
        ErrorExit("failed to read blockmap?");
    }

    blockmap_result = RebuildBlockmap(&blockmap, false);
    if (blockmap_result.len == 0)
    {
        blockmap_result = blockmap;
        return false;
    }

    free(blockmap.elements);
    return true;
}

static int CompareBlockOffsets(unsigned int i1, unsigned int i2,
                               const void *callback_data)
{
    const uint16_t *block_offsets = callback_data;
    return (int) block_offsets[i2] - (int) block_offsets[i1];
}

bool B_IsStacked(int lumpnum)
{
    unsigned int *sorted_map;
    uint16_t *block_offsets;
    bool result = false;
    int i;

    blockmap_t blockmap = ReadBlockmap(lumpnum, wadfp);
    if (blockmap.len == 0)
    {
        // TODO: Need some better error handling.
        ErrorExit("failed to read blockmap?");
    }

    block_offsets = &blockmap.elements[4];
    sorted_map = MakeSortedMap(blockmap.num_blocks, CompareBlockOffsets,
                               block_offsets);

    for (i = 1; i < blockmap.num_blocks; i++)
    {
        if (block_offsets[sorted_map[i]] == block_offsets[sorted_map[i - 1]])
        {
            result = true;
            break;
        }
    }

    free(sorted_map);
    free(blockmap.elements);
    return result;
}

size_t B_WriteBlockmap(FILE *fstream)
{
    if (!WriteBlockmap(&blockmap_result, fstream))
    {
        ErrorExit("Error writing blockmap");
    }
    free(blockmap_result.elements);
    return blockmap_result.len * 2;
}

static blockmap_t ReadBlockmap(int lumpnum, FILE *fp)
{
    blockmap_t result;
    int i;

    result.len = wadentry[lumpnum].length / sizeof(uint16_t);
    if (result.len < 4)
    {
        result.elements = NULL;
        result.len = 0;
        return result;
    }
    result.elements = CacheLump(lumpnum);

    for (i = 0; i < result.len; i++)
    {
        result.elements[i] = READ_SHORT((uint8_t *) &result.elements[i]);
    }

    result.num_blocks = result.elements[2] * result.elements[3];
    if (result.len < result.num_blocks + 4)
    {
        free(result.elements);
        result.elements = NULL;
        result.len = 0;
    }

    return result;
}

static bool WriteBlockmap(const blockmap_t *blockmap, FILE *fp)
{
    uint8_t *buffer;
    bool result;
    int i;

    buffer = ALLOC_ARRAY(uint8_t, blockmap->len * 2);

    for (i = 0; i < blockmap->len; i++)
    {
        WRITE_SHORT(&buffer[i * 2], blockmap->elements[i]);
    }

    result = fwrite(buffer, 2, blockmap->len, fp) == blockmap->len;

    free(buffer);

    return result;
}
