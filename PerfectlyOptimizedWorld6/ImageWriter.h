#pragma once

#include "typedefs.h"

static const uint32 hexOffsets[] =
{
    // w, h, body length
    18, 20, 10,

    // offsets from bottom to top:
    // start, count
    8, 2,
    6, 6,
    5, 8,
    3, 12,
    1, 16,

    1, 16,
    3, 12,
    4, 10,
    6, 6,
    8, 2,
};

typedef void(*FilterToBGRFn)(void*, uint8[3]);

void WriteHexMapToFile(char const* filename, uint32 const* hexDef, uint32 width, uint32 height,
    void* data, uint32 dataTypeByteWidth, FilterToBGRFn filterFn);
