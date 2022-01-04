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

static int8 const hillStamp[] =
{
    // y offset, rows
    4, 3,

    4, 4, 9, 10, 13,
    4, 5, 8, 11, 12,
    2, 6, 7,
};

static int8 const mtnStamp[] =
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

static int8 const volcanoStamp[] =
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

static int8 const iceStamp[] =
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

static int8 const oasisStamp[] =
{
    // y offset, rows
    3, 4,

    4, 7, 8, 9, 10,
    8, 5, 6, 7, 8, 9, 10, 11, 12,
    10, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    12, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
};

static int8 const reefStamp[] =
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

static int8 const forestStamp[] =
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

static int8 const jungleStamp[] =
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

static int8 const marshStamp[] =
{
    // y offset, rows
    9, 5,

    5, 11, 12, 13, 14, 15,
    5, 10, 12, 13, 14, 16,
    3, 12, 13, 14,
    3, 11, 13, 15,
    1, 13,
};

static int8 const floodplainStamp[] =
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

static int8 const volcanicSoilStamp[] =
{
    // y offset, rows
    10, 3,

    7, 10, 11, 12, 13, 14, 15, 16,
    5, 11, 12, 13, 14, 15,
    2, 12, 13,
};

static int8 const geothermalStamp[] =
{
    // y offset, rows
    8, 5,

    3, 12, 13, 14,
    3, 10, 11, 15,
    5, 9, 12, 13, 14, 15,
    4, 10, 11, 12, 16,
    3, 13, 14, 15,
};

static int8 const naturalWonderStamp[] =
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

static int8 const bonusStamp[] =
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

static int8 const luxuryStamp[] =
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

static int8 const strategicStamp[] =
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


// Edges

static int8 const eEdgeStamp[] =
{
    // y offset, rows
    5, 10,

    1, 17,
    1, 17,
    1, 17,
    1, 17,
    1, 17,
    1, 17,
    1, 17,
    1, 17,
    1, 17,
    1, 17,
};

static int8 const seEdgeStamp[] =
{
    // y offset, rows
    0, 6,

    1, 9,
    2, 10, 11,
    2, 12, 13,
    1, 14,
    2, 15, 16,
    1, 17,
};

static int8 const swEdgeStamp[] =
{
    // y offset, rows
    0, 6,

    1, 8,
    2, 6, 7,
    2, 4, 5,
    1, 3,
    2, 1, 2,
    1, 0,
};

static int8 const wEdgeStamp[] =
{
    // y offset, rows
    5, 10,

    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
};

static int8 const nwEdgeStamp[] =
{
    // y offset, rows
    14, 6,

    1, 0,
    2, 1, 2,
    1, 3,
    2, 4, 5,
    2, 6, 7,
    1, 8,
};

static int8 const neEdgeStamp[] =
{
    // y offset, rows
    14, 6,

    1, 17,
    2, 15, 16,
    1, 14,
    2, 12, 13,
    2, 10, 11,
    1, 9,
};


#define EDGE_E  (1 << 0)
#define EDGE_SE (1 << 1)
#define EDGE_SW (1 << 2)
#define EDGE_W  (1 << 3)
#define EDGE_NW (1 << 4)
#define EDGE_NE (1 << 5)


// Verts

#if 1
static int8 const nVertStamp[] =
{
    // y offset, rows
    18, 4,

    2, 8, 9,
    4, 7, 8, 9, 10,
    4, 7, 8, 9, 10,
    2, 8, 9,
};

static int8 const sVertStamp[] =
{
    // y offset, rows
    -2, 4,

    2, 8, 9,
    4, 7, 8, 9, 10,
    4, 7, 8, 9, 10,
    2, 8, 9,
};
#else
static int8 const nVertStamp[] =
{
    // y offset, rows
    19, 2,

    2, 8, 9,
    2, 8, 9,
};

static int8 const sVertStamp[] =
{
    // y offset, rows
    -1, 2,

    2, 8, 9,
    2, 8, 9,
};
#endif


static int8 const nTopVertStamp[] =
{
    // y offset, rows
    18, 2,

    2, 8, 9,
    4, 7, 8, 9, 10,
};

static int8 const sBotVertStamp[] =
{
    // y offset, rows
    0, 2,

    4, 7, 8, 9, 10,
    2, 8, 9,
};

static int8 const swLeftVertStamp[] =
{
    // y offset, rows
    3, 4,

    1, 0,
    2, 0, 1,
    2, 0, 1,
    1, 0,
};

static int8 const nwLeftVertStamp[] =
{
    // y offset, rows
    13, 4,

    1, 0,
    2, 0, 1,
    2, 0, 1,
    1, 0,
};

static int8 const neRightVertStamp[] =
{
    // y offset, rows
    13, 4,

    1, 17,
    2, 16, 17,
    2, 16, 17,
    1, 17,
};

static int8 const seRightVertStamp[] =
{
    // y offset, rows
    3, 4,

    1, 17,
    2, 16, 17,
    2, 16, 17,
    1, 17,
};


#define VERT_IND_N  (0)
#define VERT_IND_S  (1)

#define VERT_N  (1 << VERT_IND_N )
#define VERT_S  (1 << VERT_IND_S )


// Stamp sets

static int8 const snFlowStamp[] =
{
    // y offset, rows
    -4, 2,

    2, 8, 9,
    2, 8, 9,
};

static int8 const swsFlowStamp[] =
{
    // y offset, rows
    1, 2,

    2, 6, 7,
    2, 6, 7,
};

static int8 const sesFlowStamp[] =
{
    // y offset, rows
    1, 2,

    2, 10, 11,
    2, 10, 11,
};

static int8 const nwnFlowStamp[] =
{
    // y offset, rows
    17, 2,

    2, 6, 7,
    2, 6, 7,
};

static int8 const nenFlowStamp[] =
{
    // y offset, rows
    17, 2,

    2, 10, 11,
    2, 10, 11,
};

static int8 const nsFlowStamp[] =
{
    // y offset, rows
    22, 2,

    2, 8, 9,
    2, 8, 9,
};


#define FLOW_S_N  (1 << 0)
#define FLOW_SW_S (1 << 1)
#define FLOW_SE_S (1 << 2)
#define FLOW_NW_N (1 << 3)
#define FLOW_NE_N (1 << 4)
#define FLOW_N_S  (1 << 5)


// Colors

static uint8 const stampBlack[3]  = { 0x00, 0x00, 0x00 };
static uint8 const stampBlue[3]   = { 0x00, 0xD1, 0xFF };
static uint8 const stampOrange[3] = { 0xFF, 0xA1, 0x00 };
static uint8 const stampRed[3]    = { 0xFF, 0x00, 0x00 };


// Typedefs

typedef void(*FilterToBGRFn)(void*, uint8[3]);
typedef StampSet(*FilterStampsFn)(void*);
typedef uint8(*FilterEdgeFn)(void*);
typedef uint8(*FilterVertFn)(void*, uint8[3*2]); // uint8
typedef uint8(*FilterBitsFn)(void*);


// Interface

void InitImageWriter(uint32 width, uint32 height,
    bool _wrapX, bool _wrapY, uint32 const* hexDef);
void ExitImageWriter();

void DrawHexes(void* data, uint32 dataTypeByteWidth, FilterToBGRFn FilterFn);
void AddStamps(void* data, uint32 dataTypeByteWidth, FilterStampsFn FilterFn);
void AddEdges(void* data, uint32 dataTypeByteWidth, FilterEdgeFn FilterFn, uint8 const color[3]);
void AddVerts(void* data, uint32 dataTypeByteWidth, FilterVertFn FilterFn);
void AddStampBits(void* data, uint32 dataTypeByteWidth, FilterBitsFn FilterFn, uint8 const color[3]);

void SaveMap(char const* filename);
