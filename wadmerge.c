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
#include "wadmerge.h"
#include "wadptr.h"

typedef struct {
    sha1_digest_t hash;
    uint32_t offset;
} lump_data_t;

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

void RebuildMergedWad(FILE *newwad)
{
    lump_data_t *lumps;
    int i, num_lumps;
    uint8_t *cached;
    long along = 0;

    lumps = ALLOC_ARRAY(lump_data_t, numentries);
    num_lumps = 0;

    fwrite(iwad_name, 1, 4, newwad);
    fwrite(&along, 4, 1, newwad);
    fwrite(&along, 4, 1, newwad);

    for (i = 0; i < numentries; i++)
    {
        sha1_digest_t hash;
        const lump_data_t *ld;

        if ((i % 100) == 0)
        {
            PrintProgress(i, numentries);
        }

        cached = CacheLump(i);
        HashData(cached, wadentry[i].length, hash);
        ld = FindExistingLump(lumps, num_lumps, hash);

        if (ld == NULL)
        {
            memcpy(lumps[num_lumps].hash, hash, sizeof(sha1_digest_t));
            lumps[num_lumps].offset = ftell(newwad);
            fwrite(cached, 1, wadentry[i].length, newwad);
            ld = &lumps[num_lumps];
            ++num_lumps;
        }

        wadentry[i].offset = ld->offset;
        free(cached);
    }

    /* write the wad directory */
    diroffset = ftell(newwad);
    WriteWadDirectory(newwad);
    WriteWadHeader(newwad);

    free(lumps);
}
