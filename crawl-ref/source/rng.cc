/**
 * @file
 * @brief Random number generator wrapping.
**/

#include "AppHdr.h"

#include "rng.h"

#include "endianness.h"
#include "asg.h"
#include "syscalls.h"

#ifdef UNIX
// for times()
#include <sys/times.h>
#endif

// for getpid()
#include <sys/types.h>
#include <unistd.h>

void seed_rng(uint32_t* seed_key, size_t num_keys)
{
    seed_asg(seed_key, num_keys);
}

void seed_rng(uint32_t seed)
{
    uint32_t sarg[1] = { seed };
    seed_rng(sarg, 1);

    // for std::random_shuffle()
    srand(seed);
}

void seed_rng()
{
    uint32_t seed = time(NULL);

    /* Use a 160-bit wide seed */
    uint32_t seed_key[5];

#ifdef UNIX
    struct tms buf;
    seed += times(&buf);
#endif

    seed += getpid();
    seed_key[0] = seed;

    read_urandom((char*)(&seed_key[1]), sizeof(seed_key[0]) * 7);
    seed_rng(seed_key, 5);
}

uint32_t random_int(void)
{
    return (get_uint32());
}

void push_rng_state()
{
    push_asg_state();
}

void pop_rng_state()
{
    pop_asg_state();
}

//-----------------------------------------------------------------------------
// MurmurHash2, by Austin Appleby
uint32_t hash(const void *data, int len)
{
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    const uint32_t m = 0x5bd1e995;

    // Initialize the hash to a 'random' value
    uint32_t h = len;

    const uint8_t *d = (const uint8_t*)data;
    // Mix 4 bytes at a time into the hash
    while(len >= 4)
    {
        uint32_t k = htole32(*(uint32_t *)d);

        k *= m;
        k ^= k >> 24;
        k *= m;

        h *= m;
        h ^= k;

        d += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array
    switch(len)
    {
    case 3: h ^= (uint32_t)d[2] << 16;
    case 2: h ^= (uint32_t)d[1] << 8;
    case 1: h ^= (uint32_t)d[0];
            h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}
