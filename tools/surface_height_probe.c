#include "../cubiomes/generator.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s <seed> <x> <z> [<x> <z> ...]\n", program);
}

int main(int argc, char **argv)
{
    if (argc < 4 || (argc & 1) != 0)
    {
        usage(argv[0]);
        return 2;
    }

    errno = 0;
    char *end = NULL;
    int64_t signedSeed = strtoll(argv[1], &end, 10);
    if (errno || !end || *end)
    {
        usage(argv[0]);
        return 2;
    }

    Generator generator;
    setupGenerator(&generator, MC_1_16_1, 0);
    applySeed(&generator, DIM_OVERWORLD, (uint64_t) signedSeed);

    SurfaceNoise noise;
    initSurfaceNoise(&noise, DIM_OVERWORLD, (uint64_t) signedSeed);

    int index;
    for (index = 2; index < argc; index += 2)
    {
        errno = 0;
        long x = strtol(argv[index], &end, 10);
        if (errno || !end || *end)
        {
            usage(argv[0]);
            return 2;
        }
        errno = 0;
        long z = strtol(argv[index + 1], &end, 10);
        if (errno || !end || *end)
        {
            usage(argv[0]);
            return 2;
        }

        int height = getFirstFreeHeight116(
            &generator, &noise, (int) x, (int) z);
        printf("H|%" PRId64 "|%ld|%ld|%d\n",
            signedSeed, x, z, height);
    }
    return 0;
}
