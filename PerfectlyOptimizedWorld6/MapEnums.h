#pragma once

// --- Map Sizes --------------------------------------------------------------

#undef MAP_SIZE_TYPES
#define MAP_SIZE_TYPES         \
    CONVERT(CUSTOM)    \
\
    CONVERT(DUEL)      \
    CONVERT(TINY)      \
    CONVERT(SMALL)     \
    CONVERT(STANDARD)  \
    CONVERT(LARGE)     \
    CONVERT(HUGE)      \
\
    CONVERT(MASSIVE)   \
    CONVERT(GIGANTIC)  \
    CONVERT(COLOSSAL)  \
\
    CONVERT(ENORMOUS)  \
    CONVERT(GIANT)     \
    CONVERT(LUDICROUS) \
    CONVERT(OVERSIZED) \

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
#define CONVERT(n) "MAPSIZE_" #n,
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
    CONVERT(BARRIER_REEF)        \
    CONVERT(CLIFFS_DOVER)        \
    CONVERT(CRATER_LAKE)         \
    CONVERT(DEAD_SEA)            \
    CONVERT(EVEREST)             \
    CONVERT(GALAPAGOS)           \
    CONVERT(KILIMANJARO)         \
    CONVERT(PANTANAL)            \
    CONVERT(PIOPIOTAHI)          \
    CONVERT(TORRES_DEL_PAINE)    \
    CONVERT(TSINGY)              \
    CONVERT(YOSEMITE)            \
\
    CONVERT(DELICATE_ARCH)       \
    CONVERT(EYE_OF_THE_SAHARA)   \
    CONVERT(LAKE_RETBA)          \
    CONVERT(MATTERHORN)          \
    CONVERT(RORAIMA)             \
    CONVERT(UBSUNUR_HOLLOW)      \
    CONVERT(ZHANGYE_DANXIA)      \
\
    CONVERT(CHOCOLATEHILLS)      \
    CONVERT(DEVILSTOWER)         \
    CONVERT(GOBUSTAN)            \
    CONVERT(IKKIL)               \
    CONVERT(PAMUKKALE)           \
    CONVERT(VESUVIUS)            \
    CONVERT(WHITEDESERT)         \
\
    CONVERT(BERMUDA_TRIANGLE)    \
    CONVERT(FOUNTAIN_OF_YOUTHc)  \
    CONVERT(PAITITI)             \
\
    CONVERT(EYJAFJALLAJOKULL)    \
    CONVERT(GIANTS_CAUSEWAY)     \
    CONVERT(LYSEFJORDEN)         \
\
    CONVERT(HA_LONG_BAY)         \
\
    CONVERT(ULURU)               \
\
    CONVERT(BARRINGER_CRATER)    \
    CONVERT(BIOLUMINESCENT_BAY)  \
    CONVERT(CERRO_DE_POTOSI)     \
    CONVERT(DALLOL)              \
    CONVERT(GIBRALTAR)           \
    CONVERT(GRAND_MESA)          \
    CONVERT(KAILASH)             \
    CONVERT(KRAKATOA)            \
    CONVERT(LAKE_VICTORIA)       \
    CONVERT(LENCOIS_MARANHENSES) \
    CONVERT(MOSI_OA_TUNYA)       \
    CONVERT(MOTLATSE_CANYON)     \
    CONVERT(NAMIB)               \
    CONVERT(OLD_FAITHFUL)        \
    CONVERT(OUNIANGA)            \
    CONVERT(SALAR_DE_UYUNI)      \
    CONVERT(SINAI)               \
    CONVERT(SRI_PADA)            \
    CONVERT(VREDEFORT_DOME)      \
    CONVERT(WULINGYUAN)          \


// --- Features

#undef FEATURE_TYPES
#define FEATURE_TYPES                      \
    CONVERT(NONE)                  \
\
    CONVERT(FLOODPLAINS)           \
    CONVERT(FLOODPLAINS_GRASSLAND) \
    CONVERT(FLOODPLAINS_PLAINS)    \
    CONVERT(FOREST)                \
    CONVERT(GEOTHERMAL_FISSURE)    \
    CONVERT(ICE)                   \
    CONVERT(JUNGLE)                \
    CONVERT(MARSH)                 \
    CONVERT(OASIS)                 \
    CONVERT(REEF)                  \
    CONVERT(VOLCANIC_SOIL)         \
    CONVERT(VOLCANO)               \

static char const* featureStrs[] =
{
#undef CONVERT
#define CONVERT(n) "FEATURE_" #n,
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

    fNum,

    fWondersStart = fBARRIER_REEF,

    fWondersEndBase = fYOSEMITE + 1,
    fWondersEndExp1 = fZHANGYE_DANXIA + 1,
    fWondersEndExp2 = fWHITEDESERT + 1,
    fWondersEndOfficial = fULURU + 1,
    fWondersEndMods = fWULINGYUAN + 1,

    fWondersEnd = fWondersEndOfficial,
};


// --- Resources --------------------------------------------------------------

// By Release:
//   Base Game
//   Rise and Fall
//   Maya & Gran Colombia
//   CIVITAS Resources

#undef RESOURCE_TYPES
#define RESOURCE_TYPES                 \
    CONVERT(NONE)             \
\
    CONVERT(BANANAS)          \
    CONVERT(CATTLE)           \
    CONVERT(COPPER)           \
    CONVERT(CRABS)            \
    CONVERT(DEER)             \
    CONVERT(FISH)             \
    CONVERT(RICE)             \
    CONVERT(SHEEP)            \
    CONVERT(STONE)            \
    CONVERT(WHEAT)            \
    CONVERT(CITRUS)           \
    CONVERT(COCOA)            \
    CONVERT(COFFEE)           \
    CONVERT(COTTON)           \
    CONVERT(DIAMONDS)         \
    CONVERT(DYES)             \
    CONVERT(FURS)             \
    CONVERT(GYPSUM)           \
    CONVERT(INCENSE)          \
    CONVERT(IVORY)            \
    CONVERT(JADE)             \
    CONVERT(MARBLE)           \
    CONVERT(MERCURY)          \
    CONVERT(PEARLS)           \
    CONVERT(SALT)             \
    CONVERT(SILK)             \
    CONVERT(SILVER)           \
    CONVERT(SPICES)           \
    CONVERT(SUGAR)            \
    CONVERT(TEA)              \
    CONVERT(TOBACCO)          \
    CONVERT(TRUFFLES)         \
    CONVERT(WHALES)           \
    CONVERT(WINE)             \
    CONVERT(ALUMINUM)         \
    CONVERT(COAL)             \
    CONVERT(HORSES)           \
    CONVERT(IRON)             \
    CONVERT(NITER)            \
    CONVERT(OIL)              \
    CONVERT(URANIUM)          \
\
    CONVERT(AMBER)            \
    CONVERT(OLIVES)           \
    CONVERT(TURTLES)          \
\
    CONVERT(MAIZE)            \
    CONVERT(HONEY)            \
\
    CONVERT(P0K_PAPYRUS)      \
    CONVERT(P0K_PENGUINS)     \
    CONVERT(P0K_PLUMS)        \
    CONVERT(CVS_POMEGRANATES) \
    CONVERT(P0K_MAPLE)        \
    CONVERT(P0K_OPAL)         \

static char const* resourceStrs[] =
{
#undef CONVERT
#define CONVERT(n) "RESOURCE_" #n,
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
    CONVERT(NORTH)     \
    CONVERT(NORTHEAST) \
    CONVERT(SOUTHEAST) \
    CONVERT(SOUTH)     \
    CONVERT(SOUTHWEST) \
    CONVERT(NORTHWEST) \

static char const* flowDirStrs[] =
{
#undef CONVERT
#define CONVERT(n) "FLOWDIRECTION_" #n,
    FLOWDIRECTION_TYPES

    "INVALID"
};

enum TileFlowDirection
{
#undef CONVERT
#define CONVERT(n) tfd##n,
    FLOWDIRECTION_TYPES

    tfdNum,

    tfdStart = tfdNORTH,
    tfdEnd = tfdNORTHWEST + 1,
};


// --- Terrain ----------------------------------------------------------------

#undef TERRAIN_TYPES
#define TERRAIN_TYPES                \
    CONVERT(OCEAN)           \
    CONVERT(COAST)           \
    CONVERT(DESERT)          \
    CONVERT(GRASS)           \
    CONVERT(PLAINS)          \
    CONVERT(SNOW)            \
    CONVERT(TUNDRA)          \
    CONVERT(DESERT_HILLS)    \
    CONVERT(GRASS_HILLS)     \
    CONVERT(PLAINS_HILLS)    \
    CONVERT(SNOW_HILLS)      \
    CONVERT(TUNDRA_HILLS)    \
    CONVERT(DESERT_MOUNTAIN) \
    CONVERT(GRASS_MOUNTAIN)  \
    CONVERT(PLAINS_MOUNTAIN) \
    CONVERT(SNOW_MOUNTAIN)   \
    CONVERT(TUNDRA_MOUNTAIN) \

static char const* terrainStrs[] =
{
#undef CONVERT
#define CONVERT(n) "TERRAIN_" #n,
    TERRAIN_TYPES

    "INVALID"
};

enum Terrain
{
#undef CONVERT
#define CONVERT(n) t##n,
    TERRAIN_TYPES

    tNum,

    tWaterStart = tOCEAN,
    tWaterEnd = tCOAST + 1,
    tLandStart = tDESERT,
    tLandEnd = tTUNDRA_MOUNTAIN + 1,

    tBaseStart = tOCEAN,
    tBaseEnd = tTUNDRA + 1,
    tHillsStart = tDESERT_HILLS,
    tHillsEnd = tTUNDRA_HILLS + 1,
    tMountainsStart = tDESERT_MOUNTAIN,
    tMountainsEnd = tTUNDRA_MOUNTAIN + 1,
};


// --- Continents -------------------------------------------------------------

#define MAX_NUM_CONTINENTS 0xF

