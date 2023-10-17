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
 * Compresses WAD files by merging identical lumps in a WAD file and
 * sharing them between multiple wad directory entries.
 */

#include <stdlib.h>

#include "sha1.h"
#include "sort.h"
#include "waddir.h"
#include "wadmerge.h"
#include "wadptr.h"

typedef struct {
    sha1_digest_t hash;
    uint32_t offset;
} lump_data_t;

static int CompareFunc(unsigned int index1, unsigned int index2,
                       const void *callback_data)
{
    const wad_file_t *wf = callback_data;
    return strncmp(wf->entries[index1].name, wf->entries[index2].name, 8);
}

static void HashData(uint8_t *data, size_t data_len, sha1_digest_t hash)
{
    sha1_context_t ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, data, data_len);
    SHA1_Final(hash, &ctx);
}

// We use SHA1 hash to identify identical lumps that have already been
// written. In theory this could result in a hash collision, but in practice
// unless you're using a Doom WAD file to store your SHA1 collision proof of
// concept data, it should be fine.
static const lump_data_t *FindExistingLump(lump_data_t *lumps, int num_lumps,
                                           sha1_digest_t hash)
{
    int i;

    for (i = num_lumps - 1; i >= 0; --i)
    {
        if (!memcmp(lumps[i].hash, hash, sizeof(sha1_digest_t)))
        {
            return &lumps[i];
        }
    }

    return NULL;
}

// TODO: This function mutates the directory of the passed wad_file_t, and
// it should not.
void RebuildMergedWad(wad_file_t *wf, FILE *newwad)
{
    unsigned int *sorted_map;
    lump_data_t *lumps;
    int i, num_lumps;
    uint8_t *cached;

    // This is an optimization not for WAD size, but for compressed WAD size.
    // We write out lumps not in WAD directory order, but ordered by lump
    // name. This causes similar lumps to be grouped together within the WAD
    // file; for compression algorithms such as LZ77 or LZSS, which work by
    // keeping a sliding window of recently written data, this means that
    // similar data from one lump can be reused by the next. A good example
    // is SIDEDEFS lumps which contain large numbers of texture names; placing
    // all within the same location in the WAD file assists the compression
    // algorithm.
    sorted_map = MakeSortedMap(wf->num_entries, CompareFunc, wf);

    lumps = ALLOC_ARRAY(lump_data_t, wf->num_entries);
    num_lumps = 0;

    for (i = 0; i < wf->num_entries; i++)
    {
        sha1_digest_t hash;
        const lump_data_t *ld;
        int lumpnum = sorted_map[i];

        if ((i % 100) == 0)
        {
            PrintProgress(i, wf->num_entries);
        }

        cached = CacheLump(wf, lumpnum);
        HashData(cached, wf->entries[lumpnum].length, hash);
        ld = FindExistingLump(lumps, num_lumps, hash);

        if (ld == NULL)
        {
            memcpy(lumps[num_lumps].hash, hash, sizeof(sha1_digest_t));
            lumps[num_lumps].offset =
                WriteWadLump(newwad, cached, wf->entries[lumpnum].length);
            ld = &lumps[num_lumps];
            ++num_lumps;
        }

        wf->entries[lumpnum].offset = ld->offset;
        free(cached);
    }

    // Write the wad directory for the new WAD:
    WriteWadDirectory(newwad, wf->type, wf->entries, wf->num_entries);

    free(lumps);
    free(sorted_map);
}
