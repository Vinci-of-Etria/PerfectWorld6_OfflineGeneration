#pragma once

// --- Map Sizes --------------------------------------------------------------

#undef MAP_SIZE_TYPES
#define MAP_SIZE_TYPES         \
    CONVERT(MAPSIZE_CUSTOM)    \
\
    CONVERT(MAPSIZE_DUEL)      \
    CONVERT(MAPSIZE_TINY)      \
    CONVERT(MAPSIZE_SMALL)     \
    CONVERT(MAPSIZE_STANDARD)  \
    CONVERT(MAPSIZE_LARGE)     \
    CONVERT(MAPSIZE_HUGE)      \
\
    CONVERT(MAPSIZE_MASSIVE)   \
    CONVERT(MAPSIZE_GIGANTIC)  \
    CONVERT(MAPSIZE_COLOSSAL)  \
\
    CONVERT(MAPSIZE_ENORMOUS)  \
    CONVERT(MAPSIZE_GIANT)     \
    CONVERT(MAPSIZE_LUDICROUS) \
    CONVERT(MAPSIZE_OVERSIZED) \

//               Base         PW6
//      DUEL :  44 x 26     50 x  30
//      TINY :  60 x 38     70 x  44
//     SMALL :  74 x 46     86 x  52
//  STANDARD :  84 x 54     98 x  62
//     LARGE :  96 x 60    112 x  40
//      HUGE : 106 x 66    122 x  76
//
//   MASSIVE :             148 x  92
//  GIGANTIC :             166 x 104
//  COLOSSAL :             184 x 115
//
//  ENORMOUS :             148 x  92
//     GIANT :             212 x 106
// LUDICROUS :             230 x 115
// OVERSIZED :             

static char const* mapSizeStrs[] =
{
#undef CONVERT
#define CONVERT(n) #n,
    MAP_SIZE_TYPES

    "INVALID"
};

enum MapSize
{
#undef CONVERT
#define CONVERT(n) ms##n,
    MAP_SIZE_TYPES

    msNum
};


// --- Natural Wonders (are treated as Features) ------------------------------

// By Release:
//   Base Game
//   Rise and Fall
//   Gathering Storm
//   Maya & Gran Colombia
//   Vikings Scenario
//   Khmer and Indonesia Civ & Scenario
//   Australia Civ & Scenario
// Mods:
//   Terra Mirabilis

// FEATURE_DEVILSTOWER == Mato Tipila ?
// FEATURE_WHITEDESERT == Sahara el Beyda ?

#undef WONDER_TYPES
#define WONDER_TYPES                     \
    CONVERT(FEATURE_BARRIER_REEF)        \
    CONVERT(FEATURE_CLIFFS_DOVER)        \
    CONVERT(FEATURE_CRATER_LAKE)         \
    CONVERT(FEATURE_DEAD_SEA)            \
    CONVERT(FEATURE_EVEREST)             \
    CONVERT(FEATURE_GALAPAGOS)           \
    CONVERT(FEATURE_KILIMANJARO)         \
    CONVERT(FEATURE_PANTANAL)            \
    CONVERT(FEATURE_PIOPIOTAHI)          \
    CONVERT(FEATURE_TORRES_DEL_PAINE)    \
    CONVERT(FEATURE_TSINGY)              \
    CONVERT(FEATURE_YOSEMITE)            \
\
    CONVERT(FEATURE_DELICATE_ARCH)       \
    CONVERT(FEATURE_EYE_OF_THE_SAHARA)   \
    CONVERT(FEATURE_LAKE_RETBA)          \
    CONVERT(FEATURE_MATTERHORN)          \
    CONVERT(FEATURE_RORAIMA)             \
    CONVERT(FEATURE_UBSUNUR_HOLLOW)      \
    CONVERT(FEATURE_ZHANGYE_DANXIA)      \
\
    CONVERT(FEATURE_CHOCOLATEHILLS)      \
    CONVERT(FEATURE_DEVILSTOWER)         \
    CONVERT(FEATURE_GOBUSTAN)            \
    CONVERT(FEATURE_IKKIL)               \
    CONVERT(FEATURE_PAMUKKALE)           \
    CONVERT(FEATURE_VESUVIUS)            \
    CONVERT(FEATURE_WHITEDESERT)         \
\
    CONVERT(FEATURE_BERMUDA_TRIANGLE)    \
    CONVERT(FEATURE_FOUNTAIN_OF_YOUTHc)  \
    CONVERT(FEATURE_PAITITI)             \
\
    CONVERT(FEATURE_EYJAFJALLAJOKULL)    \
    CONVERT(FEATURE_GIANTS_CAUSEWAY)     \
    CONVERT(FEATURE_LYSEFJORDEN)         \
\
    CONVERT(FEATURE_HA_LONG_BAY)         \
\
    CONVERT(FEATURE_ULURU)               \
\
    CONVERT(FEATURE_BARRINGER_CRATER)    \
    CONVERT(FEATURE_BIOLUMINESCENT_BAY)  \
    CONVERT(FEATURE_CERRO_DE_POTOSI)     \
    CONVERT(FEATURE_DALLOL)              \
    CONVERT(FEATURE_GIBRALTAR)           \
    CONVERT(FEATURE_GRAND_MESA)          \
    CONVERT(FEATURE_KAILASH)             \
    CONVERT(FEATURE_KRAKATOA)            \
    CONVERT(FEATURE_LAKE_VICTORIA)       \
    CONVERT(FEATURE_LENCOIS_MARANHENSES) \
    CONVERT(FEATURE_MOSI_OA_TUNYA)       \
    CONVERT(FEATURE_MOTLATSE_CANYON)     \
    CONVERT(FEATURE_NAMIB)               \
    CONVERT(FEATURE_OLD_FAITHFUL)        \
    CONVERT(FEATURE_OUNIANGA)            \
    CONVERT(FEATURE_SALAR_DE_UYUNI)      \
    CONVERT(FEATURE_SINAI)               \
    CONVERT(FEATURE_SRI_PADA)            \
    CONVERT(FEATURE_VREDEFORT_DOME)      \
    CONVERT(FEATURE_WULINGYUAN)          \


// --- Features

#undef FEATURE_TYPES
#define FEATURE_TYPES                      \
    CONVERT(FEATURE_NONE)                  \
\
    CONVERT(FEATURE_FLOODPLAINS)           \
    CONVERT(FEATURE_FLOODPLAINS_GRASSLAND) \
    CONVERT(FEATURE_FLOODPLAINS_PLAINS)    \
    CONVERT(FEATURE_FOREST)                \
    CONVERT(FEATURE_GEOTHERMAL_FISSURE)    \
    CONVERT(FEATURE_ICE)                   \
    CONVERT(FEATURE_JUNGLE)                \
    CONVERT(FEATURE_MARSH)                 \
    CONVERT(FEATURE_OASIS)                 \
    CONVERT(FEATURE_REEF)                  \
    CONVERT(FEATURE_VOLCANIC_SOIL)         \
    CONVERT(FEATURE_VOLCANO)               \

static char const* featureStrs[] =
{
#undef CONVERT
#define CONVERT(n) #n,
    FEATURE_TYPES
    WONDER_TYPES

    "INVALID"
};

enum Feature
{
#undef CONVERT
#define CONVERT(n) f##n,
    FEATURE_TYPES
    WONDER_TYPES

    fNum
};


// --- Resources --------------------------------------------------------------

// By Release:
//   Base Game
//   Rise and Fall
//   Maya & Gran Colombia
//   CIVITAS Resources

#undef RESOURCE_TYPES
#define RESOURCE_TYPES                 \
    CONVERT(RESOURCE_NONE)             \
\
    CONVERT(RESOURCE_BANANAS)          \
    CONVERT(RESOURCE_CATTLE)           \
    CONVERT(RESOURCE_COPPER)           \
    CONVERT(RESOURCE_CRABS)            \
    CONVERT(RESOURCE_DEER)             \
    CONVERT(RESOURCE_FISH)             \
    CONVERT(RESOURCE_RICE)             \
    CONVERT(RESOURCE_SHEEP)            \
    CONVERT(RESOURCE_STONE)            \
    CONVERT(RESOURCE_WHEAT)            \
    CONVERT(RESOURCE_CITRUS)           \
    CONVERT(RESOURCE_COCOA)            \
    CONVERT(RESOURCE_COFFEE)           \
    CONVERT(RESOURCE_COTTON)           \
    CONVERT(RESOURCE_DIAMONDS)         \
    CONVERT(RESOURCE_DYES)             \
    CONVERT(RESOURCE_FURS)             \
    CONVERT(RESOURCE_GYPSUM)           \
    CONVERT(RESOURCE_INCENSE)          \
    CONVERT(RESOURCE_IVORY)            \
    CONVERT(RESOURCE_JADE)             \
    CONVERT(RESOURCE_MARBLE)           \
    CONVERT(RESOURCE_MERCURY)          \
    CONVERT(RESOURCE_PEARLS)           \
    CONVERT(RESOURCE_SALT)             \
    CONVERT(RESOURCE_SILK)             \
    CONVERT(RESOURCE_SILVER)           \
    CONVERT(RESOURCE_SPICES)           \
    CONVERT(RESOURCE_SUGAR)            \
    CONVERT(RESOURCE_TEA)              \
    CONVERT(RESOURCE_TOBACCO)          \
    CONVERT(RESOURCE_TRUFFLES)         \
    CONVERT(RESOURCE_WHALES)           \
    CONVERT(RESOURCE_WINE)             \
    CONVERT(RESOURCE_ALUMINUM)         \
    CONVERT(RESOURCE_COAL)             \
    CONVERT(RESOURCE_HORSES)           \
    CONVERT(RESOURCE_IRON)             \
    CONVERT(RESOURCE_NITER)            \
    CONVERT(RESOURCE_OIL)              \
    CONVERT(RESOURCE_URANIUM)          \
\
    CONVERT(RESOURCE_AMBER)            \
    CONVERT(RESOURCE_OLIVES)           \
    CONVERT(RESOURCE_TURTLES)          \
\
    CONVERT(RESOURCE_MAIZE)            \
    CONVERT(RESOURCE_HONEY)            \
\
    CONVERT(RESOURCE_P0K_PAPYRUS)      \
    CONVERT(RESOURCE_P0K_PENGUINS)     \
    CONVERT(RESOURCE_P0K_PLUMS)        \
    CONVERT(RESOURCE_CVS_POMEGRANATES) \
    CONVERT(RESOURCE_P0K_MAPLE)        \
    CONVERT(RESOURCE_P0K_OPAL)         \

static char const* resourceStrs[] =
{
#undef CONVERT
#define CONVERT(n) #n,
    RESOURCE_TYPES

    "INVALID"
};

enum Resource
{
#undef CONVERT
#define CONVERT(n) r##n,
    RESOURCE_TYPES

    rNum
};


// --- Tile Flow Direction ----------------------------------------------------

// NOTE: In the map SQL data NO_FLOW is -1 and the rest are 0-5
//   Thus to convert to the SQL values you need to cast the enum values to ints
//   and subtract 1.

#undef FLOWDIRECTION_TYPES
#define FLOWDIRECTION_TYPES          \
    CONVERT(NO_FLOW)                 \
\
    CONVERT(FLOWDIRECTION_NORTH)     \
    CONVERT(FLOWDIRECTION_NORTHEAST) \
    CONVERT(FLOWDIRECTION_SOUTHEAST) \
    CONVERT(FLOWDIRECTION_SOUTH)     \
    CONVERT(FLOWDIRECTION_SOUTHWEST) \
    CONVERT(FLOWDIRECTION_NORTHWEST) \

static char const* flowDirStrs[] =
{
#undef CONVERT
#define CONVERT(n) #n,
    FLOWDIRECTION_TYPES

    "INVALID"
};

enum TileFlowDirection
{
#undef CONVERT
#define CONVERT(n) tfd##n,
    FLOWDIRECTION_TYPES

    tfdNum
};


// --- Terrain ----------------------------------------------------------------

#undef TERRAIN_TYPES
#define TERRAIN_TYPES \
    CONVERT(TERRAIN_COAST)        \
    CONVERT(TERRAIN_DESERT)       \
    CONVERT(TERRAIN_DESERT_HILLS) \
    CONVERT(TERRAIN_DESERT_MOUNTAIN) \
    CONVERT(TERRAIN_GRASS) \
    CONVERT(TERRAIN_GRASS_HILLS) \
    CONVERT(TERRAIN_GRASS_MOUNTAIN) \
    CONVERT(TERRAIN_OCEAN) \
    CONVERT(TERRAIN_PLAINS) \
    CONVERT(TERRAIN_PLAINS_HILLS) \
    CONVERT(TERRAIN_PLAINS_MOUNTAIN) \
    CONVERT(TERRAIN_SNOW) \
    CONVERT(TERRAIN_SNOW_HILLS) \
    CONVERT(TERRAIN_SNOW_MOUNTAIN) \
    CONVERT(TERRAIN_TUNDRA) \
    CONVERT(TERRAIN_TUNDRA_HILLS) \
    CONVERT(TERRAIN_TUNDRA_MOUNTAIN) \

static char const* terrainStrs[] =
{
#undef CONVERT
#define CONVERT(n) #n,
    TERRAIN_TYPES

    "INVALID"
};

enum Terrain
{
#undef CONVERT
#define CONVERT(n) t##n,
    TERRAIN_TYPES

    tNum
};


// --- Continents -------------------------------------------------------------

#define MAX_NUM_CONTINENTS 0xF

