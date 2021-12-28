#pragma once

#include "typedefs.h"

static uint32 const hexOffsets[] =
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


/// Stamps

// Features

static uint8 const hillStamp[] =
{
    // y offset, rows
    4, 3,

    4, 4, 9, 10, 13,
    4, 5, 8, 11, 12,
    2, 6, 7,
};

static uint8 const mtnStamp[] =
{
    // y offset, rows
    6, 11,

    3, 2, 10, 15,
    3, 3, 9, 14,
    3, 3, 9, 14,
    3, 4, 8, 13,
    3, 4, 8, 13,
    4, 5, 7, 9, 12,
    4, 5, 7, 9, 12,
    3, 6, 9, 11,
    2, 10, 11,
    1, 10,
    1, 10,
};

static uint8 const volcanoStamp[] =
{
    // y offset, rows
    5, 13,

    2, 4, 13,
    2, 5, 12,
    2, 5, 12,
    2, 6, 11,
    2, 6, 11,
    2, 7, 10,
    2, 7, 10,
    1, 9,
    1, 8,
    1, 9,
    3, 7, 8, 10,
    4, 6, 7, 10, 11,
    1, 9,
};

static uint8 const iceStamp[] =
{
    // y offset, rows
    2, 13,

    1, 8,
    4, 7, 8, 9, 11,
    7, 6, 7, 8, 9, 10, 11, 12,
    7, 6, 7, 8, 9, 10, 11, 12,
    9, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    10, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    11, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    9, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    14, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16,
    6, 3, 6, 8, 11, 13, 15,
    4, 7, 10, 11, 12,
    3, 7, 8, 10,
    1, 9,
};

static uint8 const oasisStamp[] =
{
    // y offset, rows
    3, 4,

    4, 7, 8, 9, 10,
    8, 5, 6, 7, 8, 9, 10, 11, 12,
    10, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    12, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
};

static uint8 const reefStamp[] =
{
    // y offset, rows
    2, 6,

    2, 9, 10,
    3, 5, 6, 7,
    3, 9, 10, 11,
    4, 4, 6, 7, 13,
    1, 10,
    1, 7,
};

static uint8 const forestStamp[] =
{
    // y offset, rows
    7, 9,

    1, 13,
    7, 10, 11, 12, 13, 14, 15, 16,
    5, 11, 12, 13, 14, 15,
    1, 13,
    5, 11, 12, 13, 14, 15,
    3, 12, 13, 14,
    1, 13,
    3, 12, 13, 14,
    1, 13,
};

static uint8 const jungleStamp[] =
{
    // y offset, rows
    7, 8,

    3, 11, 12, 13,
    1, 12,
    1, 12,
    1, 11,
    2, 11, 15,
    2, 12, 14,
    4, 9, 12, 13, 16,
    4, 10, 11, 14, 15,
};

static uint8 const marshStamp[] =
{
    // y offset, rows
    9, 5,

    5, 11, 12, 13, 14, 15,
    5, 10, 12, 13, 14, 16,
    3, 12, 13, 14,
    3, 11, 13, 15,
    1, 13,
};

static uint8 const floodplainStamp[] =
{
    // y offset, rows
    8, 7,

    6, 11, 12, 13, 14, 15, 16,
    5, 10, 11, 12, 13, 14,
    4, 10, 11, 12, 13,
    3, 10, 11, 12,
    5, 10, 11, 12, 13, 16,
    6, 11, 12, 13, 14, 15, 16,
    4, 12, 13, 14, 15,
};

static uint8 const volcanicSoilStamp[] =
{
    // y offset, rows
    10, 3,

    7, 10, 11, 12, 13, 14, 15, 16,
    5, 11, 12, 13, 14, 15,
    2, 12, 13,
};

static uint8 const geothermalStamp[] =
{
    // y offset, rows
    8, 5,

    3, 12, 13, 14,
    3, 10, 11, 15,
    5, 9, 12, 13, 14, 15,
    4, 10, 11, 12, 16,
    3, 13, 14, 15,
};

static uint8 const naturalWonderStamp[] =
{
    // y offset, rows
    8, 7,

    1, 13,
    3, 12, 13, 14,
    3, 12, 13, 14,
    3, 11, 13, 15,
    3, 12, 13, 14,
    3, 12, 13, 14,
    3, 13,
};

// Resources

static uint8 const bonusStamp[] =
{
    // y offset, rows
    9, 6,

    2, 4, 5,
    2, 4, 5,
    6, 2, 3, 4, 5, 6, 7,
    6, 2, 3, 4, 5, 6, 7,
    2, 4, 5,
    2, 4, 5,
};

static uint8 const luxuryStamp[] =
{
    // y offset, rows
    9, 6,

    3, 3, 4, 5,
    2, 2, 6,
    2, 1, 7,
    0,
    2, 3, 5,
    2, 3, 5,
};

static uint8 const strategicStamp[] =
{
    // y offset, rows
    9, 6,

    2, 2, 7,
    2, 3, 6,
    2, 4, 5,
    2, 4, 5,
    2, 3, 6,
    2, 2, 7,
};

enum ElevationStamp
{
    esNone,
    esHills,
    esMountains,
};

struct StampSet
{
    uint8 elevation;
    uint8 feature;
    uint8 resource;
};

typedef void(*FilterToBGRFn)(void*, uint8[3]);
typedef StampSet(*FilterStampsFn)(void*);

void InitImageWriter(uint32 width, uint32 height, uint32 const* hexDef);
void ExitImageWriter();

void DrawHexes(void* data, uint32 dataTypeByteWidth, FilterToBGRFn filterFn);
void AddStamps(void* data, uint32 dataTypeByteWidth, FilterStampsFn filterFn);
//TODO: void AddRivers();
//TODO: void AddCliffs();

void SaveMap(char const* filename);
