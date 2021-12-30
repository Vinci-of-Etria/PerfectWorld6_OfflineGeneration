#pragma once

#include "MapEnums.h"
#include "typedefs.h"

// Map Attributes
#define MAP_SCRIPT   "Continents.lua"
#define RULESET      "RULESET_STANDARD"

// Metadata
#define METADATA_VERSION   "1"
#define APP_VERSION        "1.0.0.0 (0)"
#define DISPLAY_NAME       "Zonama Sekot"
// TODO: set the to something either dynamic or unique
#define METADATA_ID        "DEADBEEF-3F78-4FBD-9301-6E2F6F75FF2C"
#define IS_MOD             "false"

struct MapDetails
{
    uint32 width;
    uint32 height;
    int32 topLatitude;
    int32 bottomLatitude;
    bool wrapX;
    bool wrapY;
    MapSize size;
};

// YMMV, but bitfields work fine on my system for my use case
struct MapTile
{
    uint8 terrain : 5;

    uint8 isNEOfCliff : 1;
    uint8 isWOfCliff : 1;
    uint8 isNWOfCliff : 1;

    uint8 feature : 7;

    uint8 isNEOfRiver : 1;
    uint8 isNWOfRiver : 1;
    uint8 isWOfRiver : 1;

    uint8 flowDirE : 3;
    uint8 flowDirSE : 3;
    uint8 flowDirSW : 3;

    uint8 isImpassable : 1;

    uint8 continentID : 4;

    uint8 resource : 6;

    uint8 __filler : 2;
};
STATIC_ASSERT(fNum - 1 <= 0x7F);
STATIC_ASSERT(rNum - 1 <= 0x3F);
STATIC_ASSERT(tfdNum - 1 <= 0x7);
STATIC_ASSERT(tNum - 1 <= 0x1F);
STATIC_ASSERT(MAX_NUM_CONTINENTS <= 0xF);
STATIC_ASSERT(sizeof(MapTile) <= sizeof(char[5]));
