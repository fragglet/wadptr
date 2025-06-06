//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     SHA-1 digest.
//

#ifndef __SHA1_H__
#define __SHA1_H__

#include <stdint.h>
#include <stdlib.h>

typedef struct sha1_context_s sha1_context_t;
typedef uint8_t sha1_digest_t[20];

struct sha1_context_s {
    uint32_t h0, h1, h2, h3, h4;
    uint32_t nblocks;
    uint8_t buf[64];
    int count;
};

void SHA1_Init(sha1_context_t *context);
void SHA1_Update(sha1_context_t *context, uint8_t *buf, size_t len);
void SHA1_Final(sha1_digest_t digest, sha1_context_t *context);

#endif /* #ifndef __SHA1_H__ */
