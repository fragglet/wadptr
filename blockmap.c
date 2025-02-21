/*
 * Copyright(C) 1998-2023 Simon Howard, Andreas Dehmel
 *
 * You can redistribute and/or modify this program under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, or any later version. This program is distributed WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 * Functions for compressing ("stacking") BLOCKMAP lumps.
 */

#include "blockmap.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "errors.h"
#include "sort.h"
#include "waddir.h"
#include "wadptr.h"

// Blockmap lumps have a vanilla limit of ~64KiB; the 16-bit integers
// are interpreted by the engine as signed integers.
#define VANILLA_MAX_BLOCKMAP_OFFSET 0x7fff /* 16-bit ints */

// Extended limit, if we treat the blockmap elements as unsigned 16-bit
// integers. Note that we cannot include 0xffff because it is used as
// the sentinel value to end a block list.
#define EXTENDED_MAX_BLOCKMAP_OFFSET 0xfffe

typedef struct {
    uint16_t *elements;
    size_t len;
} block_t;

typedef struct {
    uint16_t *elements;
    size_t len, size;
    unsigned int num_blocks;
} blockmap_t;

static blockmap_t ReadBlockmap(wad_file_t *wf, unsigned int lumpnum);
static uint32_t WriteBlockmap(const blockmap_t *blockmap, FILE *fp);

static blockmap_t blockmap_result;

static block_t *MakeBlocklist(const blockmap_t *blockmap)
{
    block_t *blocklist;
    block_t *block;
    unsigned int i, start_index, end_index;

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

static int FindIdenticalBlock(const block_t *blocklist,
                              unsigned int *sorted_map, size_t num_blocks,
                              const block_t *block)
{
    unsigned int i;

    for (i = 0; i < num_blocks; i++)
    {
        unsigned int bi = sorted_map[i];
        const block_t *ib = &blocklist[bi];

        // We allow suffixes, but unless the blockmap is in "engine
        // format" it probably won't make a difference.
        if (block->len > ib->len)
        {
            continue;
        }

        if (!memcmp(block->elements, ib->elements + ib->len - block->len,
                    block->len * 2))
        {
            return (int) bi;
        }
    }

    return -1;
}

static int LargestBlockCompare(unsigned int i1, unsigned int i2,
                               const void *callback_data)
{
    const block_t *blocklist = callback_data;

    return (int) blocklist[i2].len - (int) blocklist[i1].len;
}

// We never generate a blockmap that will exceed the vanilla 16-bit
// signed limit. However, we support an option that instead treats the
// values as unsigned values, since some source ports support this.
static size_t BlockmapLimit(void)
{
    if (extblocks)
    {
        return EXTENDED_MAX_BLOCKMAP_OFFSET;
    }
    return VANILLA_MAX_BLOCKMAP_OFFSET;
}

static blockmap_t RebuildBlockmap(const blockmap_t *blockmap, bool compress)
{
    blockmap_t result;
    block_t *blocklist;
    uint16_t *block_offsets;
    unsigned int *sorted_map;
    unsigned int i;

    blocklist = MakeBlocklist(blockmap);

    result.size = blockmap->len;
    result.elements = ALLOC_ARRAY(uint16_t, result.size);
    result.len = 4 + blockmap->num_blocks;
    result.num_blocks = blockmap->num_blocks;
    block_offsets = &result.elements[4];

    // Header is identical:
    memcpy(result.elements, blockmap->elements, 4 * sizeof(uint16_t));

    // We process blocks in order of decreasing size (ie. largest first), This
    // allows us to do suffix matching more effectively where it is possible.
    sorted_map =
        MakeSortedMap(blockmap->num_blocks, LargestBlockCompare, blocklist);

    // NOTE: There is one corner case with this approach. Marginal levels that
    // are just on the edge of overflowing the block limit may still fit if the
    // very largest block is at the very end of the lump. So we place the
    // largest block at the very end in case this helps.
    if (blockmap->num_blocks > 1)
    {
        unsigned int largest = sorted_map[0];
        memmove(sorted_map, sorted_map + 1,
                sizeof(unsigned int) * (blockmap->num_blocks - 1));
        sorted_map[blockmap->num_blocks - 1] = largest;
    }

    for (i = 0; i < blockmap->num_blocks; i++)
    {
        unsigned int bi = sorted_map[i];
        const block_t *block = &blocklist[bi];
        int match_index = -1;

        PrintProgress(i, blockmap->num_blocks);
#ifdef DEBUG
        printf("block %5d: len=%d\n", bi, block->len);
#endif
        if (compress)
        {
            match_index = FindIdenticalBlock(blocklist, sorted_map, i, block);
        }

        if (match_index >= 0)
        {
            // Copy the offset of the other block, but if it's a suffix
            // match then we need to offset.
            block_offsets[bi] = block_offsets[match_index] +
                                blocklist[match_index].len - block->len;
#ifdef DEBUG
            printf("\tmatches block %d (+%d offset)\n", match_index,
                   blocklist[match_index].len - block->len);
#endif
        }
        else if (result.len > BlockmapLimit())
        {
            free(result.elements);
            result.elements = NULL;
            result.len = 0;
            break;
        }
        else
        {
            block_offsets[bi] = result.len;
            AppendBlockmapElements(&result, block->elements, block->len);
            block_offsets = &result.elements[4];
        }
    }

    free(blocklist);
    free(sorted_map);
#ifdef DEBUG
    printf("total blockmap length=%d elements (%d bytes)\n", result.len,
           result.len * 2);
#endif

    return result;
}

// Bad node builders can generate invalid BLOCKMAP lumps for very large
// levels. We can detect this case by looking for sentinel values beyond
// the 16-bit offset range; it is okay to go a little bit beyond the
// range so long as it is only a single block list.
static bool IsOverflowedBlockmap(const blockmap_t *blockmap)
{
    unsigned int i, sentinel_count = 0;

    if (blockmap->len < EXTENDED_MAX_BLOCKMAP_OFFSET)
    {
        return false;
    }

    for (i = EXTENDED_MAX_BLOCKMAP_OFFSET; i < blockmap->len; i++)
    {
        if (blockmap->elements[i] == 0xffff)
        {
            ++sentinel_count;
            if (sentinel_count > 1)
            {
                return true;
            }
        }
    }

    return blockmap->elements[blockmap->len - 1] != 0xffff;
}

// IsValidBlockmap performs basic sanity checking on the given blockmap to
// confirm that it meets the minimum length. If it doesn't, a message is
// printed to stderr and false is returned. It isn't considered a fatal error
// because some WADs contain empty BLOCKMAP lumps and rely on the source port
// to do the build internally.
static bool IsValidBlockmap(blockmap_t *blockmap)
{
    unsigned int num_blocks;

    if (blockmap->len < 4)
    {
        Warning("Lump too short: %d < %d header size", blockmap->len, 4);
        return false;
    }

    num_blocks = blockmap->elements[2] * blockmap->elements[3];
    if (blockmap->len < num_blocks + 4U)
    {
        Warning("Lump too short: %d blocks < %d "
                "(%d x %d = %d blocks, + 4 for header",
                blockmap->len, num_blocks + 4, blockmap->elements[2],
                blockmap->elements[3], num_blocks);
        return false;
    }

    return true;
}

bool B_Stack(wad_file_t *wf, unsigned int lumpnum)
{
    blockmap_t blockmap = ReadBlockmap(wf, lumpnum);

    if (!IsValidBlockmap(&blockmap))
    {
        blockmap_result = blockmap;
        return true;
    }

    blockmap.num_blocks = blockmap.elements[2] * blockmap.elements[3];

    if (IsOverflowedBlockmap(&blockmap))
    {
        Warning("Lump overflows the 16-bit offset limit and is invalid; not "
                "trying to stack this BLOCKMAP. You should maybe try using "
                "a tool like ZokumBSP to fit this level within the vanilla "
                "limit.");
        blockmap_result = blockmap;
        return false;
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
    if (blockmap_result.len > blockmap.len)
    {
        free(blockmap_result.elements);
        blockmap_result = blockmap;
        return true;
    }

    free(blockmap.elements);
    return true;
}

bool B_Unstack(wad_file_t *wf, unsigned int lumpnum)
{
    blockmap_t blockmap = ReadBlockmap(wf, lumpnum);

    if (!IsValidBlockmap(&blockmap))
    {
        blockmap_result = blockmap;
        return true;
    }

    blockmap.num_blocks = blockmap.elements[2] * blockmap.elements[3];

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

bool B_IsStacked(wad_file_t *wf, unsigned int lumpnum)
{
    unsigned int *sorted_map;
    uint16_t *block_offsets;
    bool result = false;
    unsigned int i;

    blockmap_t blockmap = ReadBlockmap(wf, lumpnum);

    if (!IsValidBlockmap(&blockmap))
    {
        free(blockmap.elements);
        return false;
    }

    blockmap.num_blocks = blockmap.elements[2] * blockmap.elements[3];
    block_offsets = &blockmap.elements[4];
    sorted_map =
        MakeSortedMap(blockmap.num_blocks, CompareBlockOffsets, block_offsets);

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

void B_WriteBlockmap(FILE *fstream, entry_t *entry)
{
    entry->offset = WriteBlockmap(&blockmap_result, fstream);
    entry->length = blockmap_result.len * 2;
    free(blockmap_result.elements);
}

static blockmap_t ReadBlockmap(wad_file_t *wf, unsigned int lumpnum)
{
    blockmap_t result;
    unsigned int i;

    result.len = wf->entries[lumpnum].length / sizeof(uint16_t);
    result.elements = CacheLump(wf, lumpnum);
    for (i = 0; i < result.len; i++)
    {
        result.elements[i] = READ_SHORT((uint8_t *) &result.elements[i]);
    }

    return result;
}

static uint32_t WriteBlockmap(const blockmap_t *blockmap, FILE *fp)
{
    uint8_t *buffer;
    uint32_t result;
    unsigned int i;

    buffer = ALLOC_ARRAY(uint8_t, blockmap->len * 2);

    for (i = 0; i < blockmap->len; i++)
    {
        WRITE_SHORT(&buffer[i * 2], blockmap->elements[i]);
    }

    result = WriteWadLump(fp, buffer, blockmap->len * 2);
    free(buffer);

    return result;
}
