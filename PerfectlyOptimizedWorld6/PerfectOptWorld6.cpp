#include "PerfectOptWorld6.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <vector>
#include <string>

#include "MapEnums.h"
#include "ImageWriter.h"

#pragma warning( disable : 6011 6387 26451 )

// --- Data Types -------------------------------------------------------------

struct Coord
{
    uint16 x;
    uint16 y;
};

// lua floats are 64bit
struct FloatMap
{
    Dim dim;
    uint32 length;
    bool wrapX : 1;
    bool wrapY : 1;

    float64* data = nullptr;

    // TODO: check member size
    int32 rectX;
    int32 rectY;
    int32 rectWidth;
    int32 rectHeight;
};

struct ElevationMap
{
    FloatMap base;
    float64 seaThreshold;
};

struct PWArea
{
    uint32 ind;
    Coord coord;
    bool trueMatch;
    uint32 size;
    bool debug;
};

struct LineSeg
{
    int16 y;
    int16 xLeft;
    int16 xRight;
    int16 dy;
};

struct PWAreaMap
{
    FloatMap base;

    // same length as map
    PWArea* areaList;
    std::vector<LineSeg> segStack;
};

struct RiverJunction
{
    Coord coord;
    bool isNorth;
    float64 altitude;
    uint8 flow;
    float64 size;
    bool submerged;
    RiverJunction* outflow;
    bool isOutflow;
    uint32 rawID;
    uint32 id;
    std::vector<RiverJunction*> parentJunctions;
};

struct RiverHex
{
    Coord coord;
    RiverJunction northJunction;
    RiverJunction southJunction;
    uint32 lakeID;
    float64 rainfall;
};

struct River
{
    RiverJunction* sourceJunc;
    uint32 riverID;
    std::vector<RiverJunction*> junctions;
};

struct RiverMap
{
    ElevationMap* eMap;

    RiverHex* riverData;
    River* rivers;
    uint32 riverCnt;
    float64 riverThreshold;
};

typedef bool (*Match)(Coord);
typedef bool (*MatchI)(uint32);

struct LakeDataUtil
{
    uint32 lakesToAdd;
    uint32 lakesAdded;
    uint32 currentLakeID;
    uint32 currentLakeSize;
};

struct RefMap
{
    Coord c;
    float64 val;
};

struct PangaeaBreaker
{
    ElevationMap* map;
    PWAreaMap areaMap;
    uint8* terrainTypes;
    float64 oldWorldPercent;

    uint32* distanceMap;

    bool* newWorld;
    bool* newWorldMap;
};

struct CentralityScore
{
    ElevationMap* map;
    Coord c;
    uint32 centrality;
    std::vector<uint32> neighborList;
};


// --- Map

#define MAP_ID "Default"
#define MAP_SCRIPT "Continents.lua"
//#define MAP_SCRIPT "PerfectOptWorld6.lua"
#define RULESET "RULESET_STANDARD"

// with Standard map settings
struct MapSettings
{
    uint16 width  = 84;
    uint16 height = 54;

    int16 topLat  = 90;
    int16 botLat  = -90;

    uint8 wrapX = 1;
    uint8 wrapY = 0;

    uint8 mapSizeType = msSTANDARD;
};

struct Thresholds
{
    // height based
    float64 ocean;
    float64 coast;
    float64 hills;
    float64 mountains;

    // rain based
    float64 desert;
    float64 plains;
    float64 zeroTrees;
    float64 jungle;

    // temp based in global settings
};

// TODO: Considerations:
//   Oasis exclusion flag

// YMMV, but bitfields work fine on my system for my use case
struct MapTile
{
#if 1
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
#else
    uint8 terrain : 5;

    uint8 isNEOfCliff;
    uint8 isWOfCliff;
    uint8 isNWOfCliff;

    uint8 feature;

    uint8 isNEOfRiver;
    uint8 isNWOfRiver;
    uint8 isWOfRiver;

    uint8 flowDirE;
    uint8 flowDirSE;
    uint8 flowDirSW;

    uint8 isImpassable;

    uint8 continentID;

    uint8 resource;
#endif
};
STATIC_ASSERT(  fNum - 1 <= 0x7F);
STATIC_ASSERT(  rNum - 1 <= 0x3F);
STATIC_ASSERT(tfdNum - 1 <= 0x7);
STATIC_ASSERT(  tNum - 1 <= 0x1F);
STATIC_ASSERT(MAX_NUM_CONTINENTS <= 0xF);
STATIC_ASSERT(sizeof(MapTile) <= sizeof(char[5]));

struct Cliffs
{
    uint32 i : 29;

    uint32 isNEOfCliff : 1;
    uint32 isWOfCliff  : 1;
    uint32 isNWOfCliff : 1;
};

// --- Enums and Constants ----------------------------------------------------

enum Dir
{
    dC = 0,
    dW = 1,
    dNW = 2,
    dNE = 3,
    dE = 4,
    dSE = 5,
    dSW = 6,

    dNum,
    dInv
};

enum FlowDir
{
    fdNone,

    fdWest,
    fdEast,
    fdVert,
};

enum WindZone
{
    wNo,

    wNPolar,
    wNTemperate,
    wNEquator,

    wSEquator,
    wSTemperate,
    wSPolar,
};

enum Quadrant
{
    qA,
    qB,
    qC,
    qD,
};

enum PlotType
{
    ptOcean,
    ptLand,
    ptHills,
    ptMountain,
};

// Maps look bad if too many meteors are required to break up a pangaea.
// Map will regen after this many meteors are thrown.
static const uint32 maximumMeteorCount = 12;

// Minimum size for a meteor strike that attemps to break pangaeas.
// Don't bother to change this it will be overwritten depending on map size.
static const uint32 minimumMeteorSize = 2;

// Hex maps are shorter in the y direction than they are wide per unit by
// this much. We need to know this to sample the perlin maps properly
// so they don't look squished.
// sqrt(.75) == 0.86602540378443864676372317075294
static const float64 YtoXRatio = 1.5 / (0.86602540378443864676372317075294 * 2.0);



// --- Static Globals ---------------------------------------------------------

static PW6Settings gSet;
// TODO: make thread local
static Thresholds gThrs;
// Create a 1 mb buffer
static MapTile gMap[200000];


// --- Forward Declarations ---------------------------------------------------

uint32 GetRectIndex(FloatMap* map, int32 x, int32 y);
bool IsOnMap(FloatMap* map, Coord c);
void InitPWArea(PWArea* area, uint32 ind, Coord c, bool trueMatch);
void InitLineSeg(LineSeg* seg, int16 y, int16 xLeft, int16 xRight, int16 dy);
void FillArea(PWAreaMap* map, Coord c, PWArea* area, MatchI mFunc);
void ScanAndFillLine(PWAreaMap* map, LineSeg seg, PWArea* area, Match mFunc);
uint16 ValidateY(PWAreaMap* map, int16 y);
uint16 ValidateX(PWAreaMap* map, int16 x);
void InitRiverHex(RiverHex* hex, Coord c);
bool ValidLakeHex(RiverMap* map, RiverHex* lakeHex, LakeDataUtil* ldu);
uint32 GetRandomLakeSize(RiverMap* map);
void GrowLake(RiverMap* map, RiverHex* lakeHex, uint32 lakeSize, LakeDataUtil* ldu,
    std::vector<RiverHex*>& lakeList, std::vector<RiverHex*>& growthQueue);
RiverJunction* GetLowestJunctionAroundHex(RiverMap* map, RiverHex* lakeHex);
std::vector<FlowDir> GetValidFlows(RiverMap* map, RiverJunction* junc);
void SetOutflowForLakeHex(RiverMap* map, RiverHex* lakeHex, RiverJunction* outflow);
RiverJunction* GetNextJunctionInFlow(RiverMap* map, RiverJunction* junc);
void InitRiver(River* river, RiverJunction* sourceJunc, uint32 rawID);
void Add(River* river, RiverJunction* junc);
void AddParent(RiverJunction* junc, RiverJunction* parent);
void InitRiverJunction(RiverJunction* junc, Coord c, bool isNorth);
float64 GetAttenuationFactor(Dim dim, Coord c);

void GetNeighbor(FloatMap*, Coord coord, Dir dir, Coord* out);
uint32 GetIndex(FloatMap* map, Coord coord);
bool IsBelowSeaLevel(ElevationMap* map, uint32 i);
void FillArea(PWAreaMap* map, Coord c, PWArea* area, MatchI mFunc);
void ScanAndFillLine(PWAreaMap* map, LineSeg seg, PWArea* area, MatchI mFunc);
std::vector<uint32> GetRadiusAroundCell(Dim dim, Coord c, uint32 rad);
uint32 GeneratePlotTypes(Dim dim, ElevationMap* outElev, FloatMap* outRain, FloatMap* outTemp, uint8** outPlot);
uint32 GenerateTerrain(ElevationMap* map, FloatMap* rainMap, FloatMap* tempMap, uint8** out);
void FinalAlterations(ElevationMap* map, uint8* plotTypes, uint8* terrainTypes);
void GenerateCoasts(ElevationMap* map, uint8* plotTypes, uint8* terrainTypes);
void InitPangaeaBreaker(PangaeaBreaker* pb, ElevationMap* map, uint8* terrainTypes);
bool BreakPangaeas(PangaeaBreaker* pb, uint8* plotTypes, uint8* terrainTypes);
void ExitPangaeaBreaker(PangaeaBreaker* pb);
void CreateNewWorldMap(PangaeaBreaker* pb);
void ApplyTerrain(uint32 len, uint8* plotTypes, uint8* terrainTypes);
void AddLakes(RiverMap* map);
void AddRivers(RiverMap* map);
void AddFeatures(ElevationMap* map, FloatMap* rainMap, FloatMap* tempMap);
void ClearFloodPlains(RiverMap* map);
void DistributeRain(Coord c, ElevationMap* map, FloatMap* temperatureMap,
    FloatMap* pressureMap, FloatMap* rainfallMap, FloatMap* moistureMap, bool isGeostrophic);
float64 GetRainCost(float64 upLiftSource, float64 upLiftDest);
void GetRiverSidesForJunction(RiverMap* map, RiverJunction* junc, MapTile** out0, MapTile** out1);
bool IsPangea(PangaeaBreaker* pb);
Coord GetMeteorStrike(PangaeaBreaker* pb);
void CastMeteorUponTheEarth(PangaeaBreaker* pb, Coord c, uint8* plotTypes, uint8* terrainTypes);
Coord GetHighestCentrality(PangaeaBreaker* pb, uint32 id);
std::vector<CentralityScore> CreateCentralityList(PangaeaBreaker* pb, uint32 id);
void InitCentralityScore(CentralityScore* score, ElevationMap* map, Coord c);


// --- Map Painting -----------------------------------------------------------

void PaintElevationMap(void* data, uint8 bgrOut[3])
{
    float64 val = *(float64*)data;

    // ocean
    if (val <= gThrs.ocean)
    {
        printf("Ocean");
        // rgb: #12142b
        bgrOut[0] = 0x2b;
        bgrOut[1] = 0x14;
        bgrOut[2] = 0x12;
    }
    // coast
    else if (val < gThrs.coast)
    {
        printf("Coast");
        // rgb: #5e6f8d
        bgrOut[0] = 0x8d;
        bgrOut[1] = 0x6f;
        bgrOut[2] = 0x5e;
    }
    // land
    else if (val < gThrs.hills)
    {
        printf("Land");
        // rgb: #6f943d
        bgrOut[0] = 0x3d;
        bgrOut[1] = 0x94;
        bgrOut[2] = 0x6f;
    }
    // hills
    else if (val < gThrs.mountains)
    {
        printf("Hills");
        // rgb: #a68672
        bgrOut[0] = 0x72;
        bgrOut[1] = 0x86;
        bgrOut[2] = 0xa6;
    }
    // mountain
    else
    {
        printf("Mountain");
        // rgb: #6e5e57
        bgrOut[0] = 0x57;
        bgrOut[1] = 0x5e;
        bgrOut[2] = 0x6e;
    }
}

void PaintUnitFloatGradient(void* data, uint8 bgrOut[3])
{
    float64 val = *(float64*)data;
    assert(val >= 0.0 && val <= 3.0);
    //val /= 3;

    bgrOut[0] = (uint8)(val * 0xFF);
    bgrOut[1] = (uint8)(val * 0xFF);
    bgrOut[2] = (uint8)(val * 0xFF);
}

void PaintPlotTypes(void* data, uint8 bgrOut[3])
{
    uint8 val = *(uint8*)data;


    switch (val)
    {
    case ptOcean:
        //printf("Ocean");
        // rgb: #12142b
        bgrOut[0] = 0x2b;
        bgrOut[1] = 0x14;
        bgrOut[2] = 0x12;
        break;
    case ptLand:
        //printf("Land");
        // rgb: #6f943d
        bgrOut[0] = 0x3d;
        bgrOut[1] = 0x94;
        bgrOut[2] = 0x6f;
        break;
    case ptHills:
        //printf("Hills");
        // rgb: #a68672
        bgrOut[0] = 0x72;
        bgrOut[1] = 0x86;
        bgrOut[2] = 0xa6;
        break;
    case ptMountain:
        //printf("Mountain");
        // rgb: #6e5e57
        bgrOut[0] = 0x57;
        bgrOut[1] = 0x5e;
        bgrOut[2] = 0x6e;
        break;
    default:
        printf("Miss");
        break;
    }
}

void PaintTerrainTypes(void* data, uint8 bgrOut[3])
{
    uint8 val = *(uint8*)data;

    switch (val)
    {
    case tSNOW:
        // rgb: #cec4df
        bgrOut[0] = 0xdf;
        bgrOut[1] = 0xc4;
        bgrOut[2] = 0xce;
        break;
    case tTUNDRA:
        // rgb: #8d6c65
        bgrOut[0] = 0x65;
        bgrOut[1] = 0x6c;
        bgrOut[2] = 0x8d;
        break;
    case tPLAINS:
        // rgb: #abaa45
        bgrOut[0] = 0x45;
        bgrOut[1] = 0xaa;
        bgrOut[2] = 0xab;
        break;
    case tGRASS:
        // rgb: #6f8c35
        bgrOut[0] = 0x35;
        bgrOut[1] = 0x8c;
        bgrOut[2] = 0x6f;
        break;
    case tDESERT:
        // rgb: #e9b76e
        bgrOut[0] = 0x6e;
        bgrOut[1] = 0xb7;
        bgrOut[2] = 0xe9;
        break;
    case tCOAST:
        // rgb: #5e6f8d
        bgrOut[0] = 0x8d;
        bgrOut[1] = 0x6f;
        bgrOut[2] = 0x5e;
        break;
    default:
        //printf("Ocean");
        // rgb: #12142b
        bgrOut[0] = 0x2b;
        bgrOut[1] = 0x14;
        bgrOut[2] = 0x12;
        break;
    }
}

void PaintIDS(void* data, uint8 bgrOut[3])
{
    float64 val = *(float64*)data;
    val /= 500.0;

    bgrOut[0] = (uint8)(val * 0xFF);
    bgrOut[1] = (uint8)(val * 0xFF);
    bgrOut[2] = (uint8)(val * 0xFF);
}

StampSet StampElevationViaPlotTypes(void* data)
{
    uint8 val = *(uint8*)data;

    StampSet stamp = { 0, 0, 0 };

    switch (val)
    {
    case ptHills:
        stamp.elevation = esHills;
        break;
    case ptMountain:
        stamp.elevation = esMountains;
        break;
    }

    return stamp;
}

StampSet StampViaMapTile(void* data)
{
    MapTile* plot = (MapTile*)data;

    StampSet stamp = { 0, 0, 0 };

    if (plot->terrain >= tMountainsStart)
        stamp.elevation = esMountains;
    else if (plot->terrain >= tHillsStart)
        stamp.elevation = esHills;

    stamp.feature = plot->feature;
    stamp.resource = plot->resource;

    return stamp;
}


// --- Base Game Source Functions ---------------------------------------------




// --- Base Game Lua Functions ------------------------------------------------

bool IsAdjacentToLand(ElevationMap* map, uint32 len, uint8* plotTypes, Coord c)
{
    for (uint32 dir = dW; dir < dNum; ++dir)
    {
        Coord n;
        GetNeighbor(NULL, c, (Dir)dir, &n);
        uint32 i = GetIndex(&map->base, n);
        if (i < map->base.length &&
            plotTypes[i] != ptOcean)
            return true;
    }

    return false;
}


// --- Loading Settings -------------------------------------------------------

inline void GetFloatSetting(char const* line, char const * substr,
    char const* dataPos, float64* setting)
{
    if (!strstr(line, substr))
        return;

    char* end;
    float64 val = strtod(dataPos, &end);
    if (end > dataPos)
    {
        printf("   %s has been set to %.3f\n", substr, val);
        *setting = val;
    }
}

inline void GetIntSetting(char const* line, char const* substr,
    char const* dataPos, int32* setting)
{
    if (!strstr(line, substr))
        return;

    char* end;
    int32 val = strtol(dataPos, &end, 10);
    if (end > dataPos)
    {
        printf("   %s has been set to %d\n", substr, val);
        *setting = val;
    }
}

inline void GetUIntSetting(char const* line, char const* substr,
    char const* dataPos, uint32* setting)
{
    if (!strstr(line, substr))
        return;

    char* end;
    uint32 val = strtoul(dataPos, &end, 10);
    if (end > dataPos)
    {
        printf("   %s has been set to %u\n", substr, val);
        *setting = val;
    }
}

inline void GetBoolSetting(char const* line, char const* substr,
    char const* dataPos, bool* setting)
{
    if (!strstr(line, substr))
        return;

    static const uint32 len = 1024;
    char data[len];
    // tolower substring
    char* it = data;
    for (; *dataPos != '\0'; ++dataPos, ++it)
        *it = tolower(*dataPos);
    assert(it < data + len);
    *it = '\0';

    if (strstr(data, "false"))
    {
        printf("   %s has been set to false\n", substr);
        *setting = false;
        return;
    }
    else if (strstr(data, "true"))
    {
        printf("   %s has been set to true\n", substr);
        *setting = true;
        return;
    }
    else
    {
        char* end;
        int32 val = strtol(dataPos, &end, 10);
        if (end > dataPos)
        {
            printf("   %s has been set to %s\n", substr, val ? "true" : "false");
            *setting = val ? true : false;
        }
    }
}

void LoadDefaultSettings(char const* settingsFile)
{
    FILE* fd;

    // Load defaults first
    gSet = PW6Settings();

    if (settingsFile &&
        (fd = fopen(settingsFile, "r")))
    {
        printf("Settings loaded from file:\n");

        static const uint32 len = 1024;
        char line[len];

        while (fgets(line, len, fd))
        {
            // quick exit for comment lines
            if (line[0] == '/')
                continue;

            char* equalsPos = strchr(line, '=');
            if (!equalsPos)
                continue;
            char* dataPos = equalsPos + 1;

            switch (line[0])
            {
            case 'a': case 'A':
                GetBoolSetting(line,  "allowPangeas", dataPos, &gSet.allowPangeas);
                break;
            case 'b': case 'B':
                GetIntSetting(line,   "bottomLatitude", dataPos, &gSet.bottomLatitude);
                break;
            case 'c': case 'C':
                break;
            case 'd': case 'D':
                GetFloatSetting(line, "desertPercent", dataPos, &gSet.desertPercent);
                GetFloatSetting(line, "desertMinTemperature", dataPos, &gSet.desertMinTemperature);
                break;
            case 'e': case 'E':
                GetFloatSetting(line, "eastAttenuationFactor", dataPos, &gSet.eastAttenuationFactor);
                GetFloatSetting(line, "eastAttenuationRange", dataPos, &gSet.eastAttenuationRange);
                break;
            case 'f': case 'F':
                GetUIntSetting(line,  "fixedSeed", dataPos, (uint32*)&gSet.fixedSeed);
                break;
            case 'g': case 'G':
                GetFloatSetting(line, "geostrophicFactor", dataPos, &gSet.geostrophicFactor);
                GetFloatSetting(line, "geostrophicLateralWindStrength", dataPos, &gSet.geostrophicLateralWindStrength);
                break;
            case 'h': case 'H':
                GetUIntSetting(line,  "height", dataPos, (uint32*)&gSet.height);
                GetFloatSetting(line, "hillsPercent", dataPos, &gSet.hillsPercent);
                GetIntSetting(line,   "horseLatitudes", dataPos, &gSet.horseLatitudes);
                break;
            case 'i': case 'I':
                GetIntSetting(line,   "iceNorthLatitudeLimit", dataPos, &gSet.iceNorthLatitudeLimit);
                GetIntSetting(line,   "iceSouthLatitudeLimit", dataPos, &gSet.iceSouthLatitudeLimit);
                break;
            case 'j': case 'J':
                GetFloatSetting(line, "junglePercent", dataPos, &gSet.junglePercent);
                GetFloatSetting(line, "jungleMinTemperature", dataPos, &gSet.jungleMinTemperature);
                GetUIntSetting(line,  "jungleToPlains", dataPos, (uint32*)&gSet.jungleToPlains);
                break;
            case 'k': case 'K':
                break;
            case 'l': case 'L':
                GetFloatSetting(line, "landPercent", dataPos, &gSet.landPercent);
                GetFloatSetting(line, "lakePercent", dataPos, &gSet.lakePercent);
                break;
            case 'm': case 'M':
                GetFloatSetting(line, "mountainFreq", dataPos, &gSet.mountainFreq);
                GetFloatSetting(line, "maxNewWorldSize", dataPos, &gSet.maxNewWorldSize);
                GetFloatSetting(line, "mountainsPercent", dataPos, &gSet.mountainsPercent);
                GetUIntSetting(line,  "minOceanSize", dataPos, &gSet.minOceanSize);
                GetFloatSetting(line, "mountainWeight", dataPos, &gSet.mountainWeight);
                GetFloatSetting(line, "minimumRainCost", dataPos, &gSet.minimumRainCost);
                GetUIntSetting(line,  "minRiverLength", dataPos, &gSet.minRiverLength);
                GetFloatSetting(line, "marshPercent", dataPos, &gSet.marshPercent);
                GetFloatSetting(line, "maxReefChance", dataPos, &gSet.maxReefChance);
                GetFloatSetting(line, "minWaterTemp", dataPos, &gSet.minWaterTemp);
                GetFloatSetting(line, "maxWaterTemp", dataPos, &gSet.maxWaterTemp);
                break;
            case 'n': case 'N':
                GetFloatSetting(line, "northAttenuationFactor", dataPos, &gSet.northAttenuationFactor);
                GetFloatSetting(line, "northAttenuationRange", dataPos, &gSet.northAttenuationRange);
                GetIntSetting(line,   "naturalWonderExtra", dataPos, &gSet.naturalWonderExtra);
                GetUIntSetting(line,  "numPlayers", dataPos, &gSet.numPlayers);
                GetUIntSetting(line,  "numCityStates", dataPos, &gSet.numCityStates);
                break;
            case 'o': case 'O':
                GetBoolSetting(line,  "oldWorldStart", dataPos, &gSet.oldWorldStart);
                break;
            case 'p': case 'P':
                GetFloatSetting(line, "pangaeaSize", dataPos, &gSet.pangaeaSize);
                GetFloatSetting(line, "plainsPercent", dataPos, &gSet.plainsPercent);
                GetIntSetting(line,   "polarFrontLatitude", dataPos, &gSet.polarFrontLatitude);
                GetFloatSetting(line, "polarRainBoost", dataPos, &gSet.polarRainBoost);
                GetFloatSetting(line, "percentRiversFloodplains", dataPos, &gSet.percentRiversFloodplains);
                GetBoolSetting(line,  "proportionalMinors", dataPos, &gSet.proportionalMinors);
                break;
            case 'q': case 'Q':
                break;
            case 'r': case 'R':
                GetFloatSetting(line, "riverPercent", dataPos, &gSet.riverPercent);
                GetUIntSetting(line,  "resources", dataPos, (uint32*)&gSet.resources);
                GetIntSetting(line,   "realEstateMin", dataPos, &gSet.realEstateMin);
                break;
            case 's': case 'S':
                GetFloatSetting(line, "southAttenuationFactor", dataPos, &gSet.southAttenuationFactor);
                GetFloatSetting(line, "southAttenuationRange", dataPos, &gSet.southAttenuationRange);
                GetFloatSetting(line, "snowTemperature", dataPos, &gSet.snowTemperature);
                GetUIntSetting(line,  "start", dataPos, (uint32*)&gSet.start);
                break;
            case 't': case 'T':
                GetIntSetting(line,   "topLatitude", dataPos, &gSet.topLatitude);
                GetFloatSetting(line, "twistMinFreq", dataPos, &gSet.twistMinFreq);
                GetFloatSetting(line, "twistMaxFreq", dataPos, &gSet.twistMaxFreq);
                GetFloatSetting(line, "twistVar", dataPos, &gSet.twistVar);
                GetFloatSetting(line, "tundraTemperature", dataPos, &gSet.tundraTemperature);
                GetIntSetting(line,   "tropicLatitudes", dataPos, &gSet.tropicLatitudes);
                GetFloatSetting(line, "treesMinTemperature", dataPos, &gSet.treesMinTemperature);
                break;
            case 'u': case 'U':
                GetIntSetting(line,   "upLiftExponent", dataPos, &gSet.upLiftExponent);
                break;
            case 'v': case 'V':
                break;
            case 'w': case 'W':
                GetUIntSetting(line,  "width", dataPos, (uint32*)&gSet.width);
                GetBoolSetting(line,  "wrapX", dataPos, &gSet.wrapX);
                GetBoolSetting(line,  "wrapY", dataPos, &gSet.wrapY);
                GetFloatSetting(line, "westAttenuationFactor", dataPos, &gSet.westAttenuationFactor);
                GetFloatSetting(line, "westAttenuationRange", dataPos, &gSet.westAttenuationRange);
                break;
            case 'x': case 'X':
                break;
            case 'y': case 'Y':
                break;
            case 'z': case 'Z':
                GetFloatSetting(line, "zeroTreesPercent", dataPos, &gSet.zeroTreesPercent);
                break;
            default:
                break;
            }
        }

        // Note: may want to add debugging? gonna assume an informed user for now

        fclose(fd);
    }
    else
        printf("No settings file, just loading defaults.");
}


// --- Util Functions ---------------------------------------------------------

static Dir FlipDir(Dir dir)
{
    return (Dir)((dir + 3) % 6);
}

static float64 BellCurve(float64 value)
{
    return sin(value * M_PI * 2.0 - M_PI_2) * 0.5 + 0.5;
}

static void PWRandSeed(uint32 fixed_seed = 0)
{
    uint32 seed = fixed_seed;
    //seed = 394527185;
    if (seed == 0)
    {
        seed = rand() % 256;
        seed = (seed << 8) + (rand() % 256);
        seed = (seed << 8) + (rand() % 256);
        seed = (seed << 4) + (rand() % 256);
    }

    printf("Random seed for this map is: %i\n", seed);
    srand(seed);
}

static float64 PWRand()
{
    return rand() / (float64)RAND_MAX;
}

static int32 PWRandInt(int32 min, int32 max)
{
    assert(max - min <= RAND_MAX);
    return (rand() % ((max+1) - min)) + min;
}


// --- Interpolation and Perlin Functions

inline float64 CubicInterpolate(float64 r[4], float64 mu)
{
    float64 mu2 = mu * mu;
    float64 a0 = (r[3] - r[2]) - (r[0] - r[1]);
    float64 a1 = (r[0] - r[1]) - a0;
    float64 a2 =  r[2] - r[0];
    float64 a3 =  r[1];

    return a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;
}

// TODO: see if this can be composited row by row for the entire map (+ transposition?)
float64 BicubicInterpolate(float64 r0[16], float64 muX, float64 muY)
{
    float64 a[4];
    a[0] = CubicInterpolate(r0, muX);
    a[1] = CubicInterpolate(r0 + 4, muX);
    a[2] = CubicInterpolate(r0 + 8, muX);
    a[3] = CubicInterpolate(r0 + 12, muX);

    return CubicInterpolate(a, muY);
}

inline float64 CubicDerivative(float64 r[4], float64 mu)
{
    float64 mu2 = mu * mu;
    float64 a0 = (r[3] - r[2]) - (r[0] - r[1]);
    float64 a1 = (r[0] - r[1]) - a0;
    float64 a2 =  r[2] - r[0];
    //float64 a3 =  r[1];

    return 3 * a0 * mu2 + 2 * a1 * mu + a2;
}

// TODO: see if this can be composited row by row for the entire map (+ transposition?)
float64 BicubicDerivative(float64 r0[16], float64 muX, float64 muY)
{
    float64 a[4];
    a[0] = CubicInterpolate(r0, muX);
    a[1] = CubicInterpolate(r0 + 4, muX);
    a[2] = CubicInterpolate(r0 + 8, muX);
    a[3] = CubicInterpolate(r0 + 12, muX);

    return CubicDerivative(a, muY);
}

// This function gets a smoothly interpolated value from srcMap.
// xand y are non - integer coordinates of where the value is to
// be calculated, and wrap in both directions.srcMap is an object
// of type FloatMap.
float64 GetInterpolatedValue(float64 x, float64 y, FloatMap* srcMap)
{
    float64 points[16];
    int32 fX = floor(x);
    int32 fY = floor(y);
    float64 fractionX = x - fX;
    float64 fractionY = y - fY;

    // wrappedX and wrappedY are set to -1,-1 of the sampled area
    // so that the sample area is in the middle quad of the 4x4 grid
    int32 wrappedX = ((fX - 1) % srcMap->rectWidth) + srcMap->rectX;
    int32 wrappedY = ((fY - 1) % srcMap->rectHeight) + srcMap->rectY;

    for (uint16 pY = 0; pY < 4; ++pY)
    {
        int32 cY = pY + wrappedY;
        for (uint16 pX = 0; pX < 4; ++pX)
        {
            int32 cX = pX + wrappedX;
            uint32 srcIndex = GetRectIndex(srcMap, cX, cY);
            points[(pY * 4 + pX)] = srcMap->data[srcIndex];
        }
    }

    float64 finalValue = BicubicInterpolate(points, fractionX, fractionY);

    return finalValue;
}

float64 GetDerivativeValue(float64 x, float64 y, FloatMap* srcMap)
{
    float64 points[16];
    int32 fX = floor(x);
    int32 fY = floor(y);
    float64 fractionX = x - fX;
    float64 fractionY = y - fY;

    // wrappedX and wrappedY are set to -1,-1 of the sampled area
    // so that the sample area is in the middle quad of the 4x4 grid
    int32 wrappedX = ((fX - 1) % srcMap->rectWidth) + srcMap->rectX;
    int32 wrappedY = ((fY - 1) % srcMap->rectHeight) + srcMap->rectY;

    for (uint16 pY = 0; pY < 4; ++pY)
    {
        int32 cY = pY + wrappedY;
        for (uint16 pX = 0; pX < 4; ++pX)
        {
            int32 cX = pX + wrappedX;
            uint32 srcIndex = GetRectIndex(srcMap, cX, cY);
            points[(pY * 4 + pX)] = srcMap->data[srcIndex];
        }
    }

    float64 finalValue = BicubicDerivative(points, fractionX, fractionY);

    return finalValue;
}

// This function gets Perlin noise for the destination coordinates.Note
// that in order for the noise to wrap, the area sampled on the noise map
// must change to fit each octave.
float64 GetPerlinNoise(float64 x, float64 y, uint16 destMapWidth, float64 destMapHeight, 
    float64 initialFrequency, float64 initialAmplitude, float64 amplitudeChange, 
    uint8 octaves, FloatMap* noiseMap)
{
    assert(octaves > 0);
    assert(noiseMap);

    float64 finalValue = 0.0;
    float64 freq = initialFrequency;
    float64 amp = initialAmplitude;
    float64 freqX; // slight adjustment for seamless wrapping
    float64 freqY; //        "                  "

    for (int i = 0; i < octaves; ++i)
    {
        // TODO: clean up branching
        if (noiseMap->wrapX)
        {
            int32 rX = (int32)floor(noiseMap->dim.w / 2.0 - (destMapWidth * freq) / 2.0);
            assert(rX >= 0 - 0x7FFF && rX <= 0x7FFF);
            noiseMap->rectX = rX;
            int32 rW = std::max((int32)floor(destMapWidth * freq), 1);
            assert(rW >= 0 && rW <= 0xFFFF);
            noiseMap->rectWidth = rW;
            freqX = noiseMap->rectWidth / (float64)destMapWidth;
        }
        else
        {
            noiseMap->rectX = 0;
            noiseMap->rectWidth = noiseMap->dim.w;
            freqX = freq;
        }

        if (noiseMap->wrapY)
        {
            int32 rY = (int32)floor(noiseMap->dim.h / 2.0 - (destMapHeight * freq) / 2.0);
            assert(rY >= 0 - 0x7FFF && rY <= 0x7FFF);
            noiseMap->rectY = rY;
            int32 rH = std::max((int32)floor(destMapHeight * freq), 1);
            assert(rH >= 0 && rH <= 0xFFFF);
            noiseMap->rectHeight = rH;
            freqY = noiseMap->rectHeight / (float64)destMapHeight;
        }
        else
        {
            noiseMap->rectY = 0;
            noiseMap->rectHeight = noiseMap->dim.h;
            freqY = freq;
        }

        finalValue += GetInterpolatedValue(x * freqX, y * freqY, noiseMap) * amp;
        freq *= 2.0;
        amp *= amplitudeChange;
    }

    return finalValue / octaves;
}

float64 GetPerlinDerivative(float64 x, float64 y, uint16 destMapWidth, float64 destMapHeight,
    float64 initialFrequency, float64 initialAmplitude, float64 amplitudeChange,
    uint8 octaves, FloatMap* noiseMap)
{
    assert(octaves > 0);
    assert(noiseMap);

    float64 finalValue = 0.0;
    float64 freq = initialFrequency;
    float64 amp = initialAmplitude;
    float64 freqX; // slight adjustment for seamless wrapping
    float64 freqY; //        "                  "

    for (int i = 0; i < octaves; ++i)
    {
        // TODO: clean up branching
        if (noiseMap->wrapX)
        {
            int32 rX = (int32)floor(noiseMap->dim.w / 2.0 - (destMapWidth * freq) / 2.0);
            assert(rX >= 0 - 0x7FFF && rX <= 0x7FFF);
            noiseMap->rectX = rX;
            int32 rW = (int32)floor(destMapWidth * freq);
            assert(rW >= 0 && rW <= 0xFFFF);
            noiseMap->rectWidth = rW;
            freqX = noiseMap->rectWidth / destMapWidth;
        }
        else
        {
            noiseMap->rectX = 0;
            noiseMap->rectWidth = noiseMap->dim.w;
            freqX = freq;
        }

        if (noiseMap->wrapY)
        {
            int32 rY = (int32)floor(noiseMap->dim.h / 2.0 - (destMapHeight * freq) / 2.0);
            assert(rY >= 0 - 0x7FFF && rY <= 0x7FFF);
            noiseMap->rectY = rY;
            int32 rH = (int32)floor(destMapHeight * freq);
            assert(rH >= 0 && rH <= 0xFFFF);
            noiseMap->rectHeight = rH;
            freqY = noiseMap->rectHeight / destMapHeight;
        }
        else
        {
            noiseMap->rectY = 0;
            noiseMap->rectHeight = noiseMap->dim.h;
            freqY = freq;
        }

        finalValue += GetDerivativeValue(x * freqX, y * freqY, noiseMap) * amp;
        freq *= 2.0;
        amp *= amplitudeChange;
    }

    return finalValue / octaves;
}



// --- Member Functions -------------------------------------------------------------

// --- MapTile

inline bool IsWater(MapTile* tile)
{
    return tile->terrain < tWaterEnd;
}

// --- FloatMap

void InitFloatMap(FloatMap * map, Dim dim, bool wrapX, bool wrapY)
{
    assert(map);
    assert(!map->data);

    map->dim = dim;
    map->length = dim.w * dim.h;
    map->wrapX = wrapX;
    map->wrapY = wrapY;

    map->data = (float64*)calloc(map->length, sizeof(*map->data));

    // TODO: check member size
    map->rectX = 0;
    map->rectY = 0;
    map->rectWidth = dim.w;
    map->rectHeight = dim.h;
}

void ExitFloatMap(FloatMap* map)
{
    free(map->data);
}

void GetNeighbor(FloatMap *, Coord coord, Dir dir, Coord * out)
{
    // TODO: precalculate offsets on map generation so there is no branching
    uint16 odd = coord.y % 2;

    switch (dir)
    {
    case dC:
        out->x = coord.x;
        out->y = coord.y;
        break;
    case dW:
        out->x = coord.x - 1;
        out->y = coord.y;
        break;
    case dNW:
        out->x = coord.x - 1 + odd;
        out->y = coord.y + 1;
        break;
    case dNE:
        out->x = coord.x + odd;
        out->y = coord.y + 1;
        break;
    case dE:
        out->x = coord.x + 1;
        out->y = coord.y;
        break;
    case dSE:
        out->x = coord.x + odd;
        out->y = coord.y - 1;
        break;
    case dSW:
        out->x = coord.x - 1 + odd;
        out->y = coord.y - 1;
        break;
    default:
        assert(0);
        break;
    }
}

uint32 GetIndex(FloatMap* map, Coord coord)
{
    uint32 x = 0, y = 0;

    // TODO: remove branching
    if (map->wrapX)
        x = coord.x % map->dim.w;
    else if (coord.x > map->dim.w - 1)
        return UINT32_MAX;// TODO: assert(0);
    else
        x = coord.x;

    if (map->wrapY)
        y = coord.y % map->dim.h;
    else if (coord.y > map->dim.h - 1)
        return UINT32_MAX;// TODO: assert(0);
    else
        y = coord.y;

    return y * map->dim.w + x;
}

// TODO: Don't use
Coord GetXYFromIndex(FloatMap* map, uint32 ind)
{
    return { (uint16)(ind % map->dim.w), (uint16)(ind / map->dim.w) };
}

// quadrants are labeled
// A B
// D C
Quadrant GetQuadrant(FloatMap* map, Coord coord)
{
    // TODO: convert to pure math
    if (coord.x < map->dim.w / 2)
    {
        if (coord.y < map->dim.h / 2)
            return qA;
        else
            return qD;
    }
    else
    {
        if (coord.y < map->dim.h / 2)
            return qB;
        else
            return qC;
    }
}

// Gets an index for x and y based on the current
// rect settings. x and y are local to the defined rect.
// Wrapping is assumed in both directions
uint32 GetRectIndex(FloatMap* map, int32 x, int32 y)
{
    int32 mX = map->rectX + (x % map->rectWidth);
    int32 mY = map->rectY + (y % map->rectHeight);
    mX %= map->dim.w;
    mY %= map->dim.h;
    if (mX < 0)
        mX += map->dim.w;
    if (mY < 0)
        mY += map->dim.h;

    return GetIndex(map, { (uint16)mX, (uint16)mY });
}

void Normalize(FloatMap* map)
{
    float64 maxAlt = *map->data;
    float64 minAlt = *map->data;

    float64* it = map->data + 1;
    float64* end = map->data + map->length;
    for (; it < end; ++it)
    {
        if (*it < minAlt)
            minAlt = *it;
        else if (*it > maxAlt)
            maxAlt = *it;
    }

    // subtract minAlt from all values so that
    // all values are zero and above
    for (it = map->data; it < end; ++it)
        *it -= minAlt;

    // subract minAlt also from maxAlt
    maxAlt -= minAlt;

    float64 normVal = maxAlt <= 0 ? 0.0 : 1.0 / maxAlt;

    for (it = map->data; it < end; ++it)
        *it *= normVal;
}

void GenerateNoise(FloatMap* map)
{
    float64* it = map->data;
    float64* end = map->data + map->length;

    for (; it < end; ++it)
        *it = PWRand();
}

void GenerateBinaryNoise(FloatMap* map)
{
    float64* it = map->data;
    float64* end = map->data + map->length;

    for (; it < end; ++it)
        *it = rand() % 2;
}

float64 FindThresholdFromPercent(FloatMap* map, float64 percent, bool excludeZeros)
{
    // TODO: very small scope testing function, so this should be acceptable
    // ideally we wouldn't allocate at all though
    std::vector<float64> maplist;
    maplist.resize(map->length);

    // The far majority of cases shouldn't fulfill this
    // if it is truly necessary it can be re-added, but we are in full control here
    //if (percent >= 1.0)
    //    return 1.01;
    //if (percent <= 0.0)
    //    return -0.01;
    assert(percent >= 0.0 && percent <= 1.0);
    uint32 size = 0;

    if (excludeZeros)
    {
        float64* it = map->data;
        float64* end = it + map->length;
        float64* ins = maplist.data();
        for (; it < end; ++it)
            if (*it > 0.0)
            {
                *ins = *it;
                ++ins;
                ++size;
            }
    }
    else
    {
        memcpy(maplist.data(), map->data, map->length * sizeof(float64));
        size = map->length;
    }

    std::sort(maplist.begin(), maplist.begin() + size);
    // storing the value locally to prevent potential issues with alloca
    float64 retval = maplist[(uint32)(size * percent)];
    return retval;
}

float64 GetLatitudeForY(FloatMap* map, uint16 y)
{
    int32 range = gSet.topLatitude - gSet.bottomLatitude;
    return (y / (float64)map->dim.h) * range + gSet.bottomLatitude;
}

uint16 GetYForLatitude(FloatMap* map, float64 lat)
{
    int32 range = gSet.topLatitude - gSet.bottomLatitude;
    return (uint16)floor((((lat - gSet.bottomLatitude) / range) * map->dim.h) + 0.5);
}

WindZone GetZone(FloatMap* map, uint16 y)
{
    if (y >= map->dim.h)
        return wNo;

    // TODO: preassign values to bit fields
    float64 lat = GetLatitudeForY(map, y);

    if (lat > gSet.polarFrontLatitude)
        return wNPolar;
    else if (lat >= gSet.horseLatitudes)
        return wNTemperate;
    else if (lat >= 0.0)
        return wNEquator;
    else if (lat > -gSet.horseLatitudes)
        return wSEquator;
    else if (lat >= -gSet.polarFrontLatitude)
        return wSTemperate;

    return wSPolar;
}

uint16 GetYFromZone(FloatMap* map, WindZone zone, bool bTop)
{
    // TODO: pre calc these values on loading settings
    if (bTop)
    {
        for (uint16 y = map->dim.h; y > 0;)
        {
            --y;
            if (GetZone(map, y) == zone)
                return y;
        }
    }
    else
    {
        for (uint16 y = 0; y < map->dim.h; ++y)
            if (GetZone(map, y) == zone)
                return y;
    }

    return UINT16_MAX;
}

std::pair<Dir, Dir> GetGeostrophicWindDirections(WindZone zone)
{
    // TODO: no pairs
    switch (zone)
    {
    case wNPolar:
        return { dSW, dW };
    case wNTemperate:
        return { dNE, dE };
    case wNEquator:
        return { dSW, dW };
    case wSEquator:
        return { dNW, dW };
    case wSTemperate:
        return { dSE, dE };
    case wSPolar:
    default:
        return { dNW, dW };
    }

    // This should not be reached
    assert(0);
    return { dInv, dInv };
}

float64 GetGeostrophicPressure(FloatMap* map, float64 lat)
{
    float64 latRange;
    float64 latPercent;
    float64 pressure;

    if (lat > gSet.polarFrontLatitude)
    {
        latRange = 90.0 - gSet.polarFrontLatitude;
        latPercent = (lat - gSet.polarFrontLatitude) / latRange;
        pressure = 1.0 - latPercent;
    }
    else if (lat >= gSet.horseLatitudes)
    {
        latRange = gSet.polarFrontLatitude - gSet.horseLatitudes;
        latPercent = (lat - gSet.horseLatitudes) / latRange;
        pressure = latPercent;
    }
    else if (lat >= 0.0)
    {
        latRange = gSet.horseLatitudes - 0.0;
        latPercent = (lat - 0.0) / latRange;
        pressure = 1.0 - latPercent;
    }
    else if (lat > -gSet.horseLatitudes)
    {
        latRange = 0.0 + gSet.horseLatitudes;
        latPercent = (lat + gSet.horseLatitudes) / latRange;
        pressure = latPercent;
    }
    else if (lat >= -gSet.polarFrontLatitude)
    {
        latRange = -gSet.horseLatitudes + gSet.polarFrontLatitude;
        latPercent = (lat + gSet.polarFrontLatitude) / latRange;
        pressure = 1.0 - latPercent;
    }
    else
    {
        latRange = -gSet.polarFrontLatitude + 90.0;
        latPercent = (lat + 90) / latRange;
        pressure = latPercent;
    }

    return pressure;
}

typedef void (*Mutator)(float64 *);

void ApplyFunction(FloatMap* map, Mutator func)
{
    float64* it = map->data;
    float64* end = it + map->length;

    for (; it < end; ++it)
        func(it);
}

uint32 GetRadiusAroundHex(FloatMap* map, Coord c, uint32 rad, Coord ** out)
{
    uint32 binCoef = (rad * (rad + 1)) / 2;
    uint32 maxTiles = 1 + 6 * binCoef;
    Coord* coords = (Coord*)calloc(maxTiles, sizeof(Coord));
    *coords = c;

    Coord ref = c;
    Coord* it = coords + 1;

    // make a circle for each radius
    for (uint32 r = 0; r < rad; ++r)
    {
        // start 1 to the west
        GetNeighbor(map, ref, dW, it);
        ref = *it;

        if (IsOnMap(map, ref))
            ++it;

        // Go r times to the NE
        for (uint32 z = 0; z <= r; ++z)
        {
            GetNeighbor(map, ref, dNE, it);
            ref = *it;

            if (IsOnMap(map, ref))
                ++it;
        }

        // Go r times to the E
        for (uint32 z = 0; z <= r; ++z)
        {
            GetNeighbor(map, ref, dE, it);
            ref = *it;

            if (IsOnMap(map, ref))
                ++it;
        }

        // Go r times to the SE
        for (uint32 z = 0; z <= r; ++z)
        {
            GetNeighbor(map, ref, dSE, it);
            ref = *it;

            if (IsOnMap(map, ref))
                ++it;
        }

        // Go r times to the SW
        for (uint32 z = 0; z <= r; ++z)
        {
            GetNeighbor(map, ref, dSW, it);
            ref = *it;

            if (IsOnMap(map, ref))
                ++it;
        }

        // Go r times to the W
        for (uint32 z = 0; z <= r; ++z)
        {
            GetNeighbor(map, ref, dW, it);
            ref = *it;

            if (IsOnMap(map, ref))
                ++it;
        }

        // Go r - 1 times to the NW
        if (r)
            for (uint32 z = 0; z <= r - 1; ++z)
            {
                GetNeighbor(map, ref, dNW, it);
                ref = *it;

                if (IsOnMap(map, ref))
                    ++it;
            }

        // one extra NW to set up for next circle
        GetNeighbor(map, ref, dNW, &ref);
    }

    *out = coords;
    return it - coords;
}

float64 GetAverageInHex(FloatMap* map, Coord c, uint32 rad)
{
    Coord* refCoords = NULL;
    uint32 size = GetRadiusAroundHex(map, c, rad, &refCoords);
    float64 avg = 0.0;

    Coord* end = refCoords + size;
    for (Coord* it = refCoords; it < end; ++it)
    {
        uint32 i = GetIndex(map, *it);
        avg += map->data[i];
    }

    free(refCoords);
    return avg / size;
}

float64 GetStdDevInHex(FloatMap* map, Coord c, uint32 rad)
{
    Coord* refCoords = NULL;
    uint32 size = GetRadiusAroundHex(map, c, rad, &refCoords);
    float64 avg = 0.0;

    Coord* end = refCoords + size;
    for (Coord* it = refCoords; it < end; ++it)
    {
        uint32 i = GetIndex(map, *it);
        avg += map->data[i];
    }

    avg /= size;

    float64 deviation = 0.0;
    for (Coord* it = refCoords; it < end; ++it)
    {
        uint32 i = GetIndex(map, *it);
        float64 sqr = map->data[i] - avg;
        deviation += sqr * sqr;
    }

    free(refCoords);
    return sqrt(deviation / size);
}

// TODO: this needs MASSIVE optimization
void Smooth(FloatMap* map, uint32 rad)
{
    float64* smoothedData = (float64*)malloc(map->length * sizeof *smoothedData);

    float64* it = smoothedData;
    Coord c;
    for (c.y = 0; c.y < map->dim.h; ++c.y)
        for (c.x = 0; c.x < map->dim.w; ++c.x, ++it)
            *it = GetAverageInHex(map, c, rad);

    float64* old = map->data;
    map->data = smoothedData;
    free(old);
}

// TODO: this needs MASSIVE optimization
void Deviate(FloatMap* map, uint32 rad)
{
    float64* deviatedData = (float64*)malloc(map->length * sizeof *deviatedData);

    float64* it = deviatedData;
    Coord c;
    for (c.y = 0; c.y < map->dim.h; ++c.y)
        for (c.x = 0; c.x < map->dim.w; ++c.x, ++it)
            *it = GetStdDevInHex(map, c, rad);

    assert(it - deviatedData == map->length);
    float64* old = map->data;
    map->data = deviatedData;
    free(old);
}

// TODO: obviate the need for such a function
bool IsOnMap(FloatMap* map, Coord c)
{
    uint32 i = GetIndex(map, c);

    return i < map->length;
}

// TODO: test
void Save(FloatMap* map)
{
    FILE* fp;
    fopen_s(&fp, "map_data.csv", "w");

    if (fp)
    {
        float64* it = map->data;
        float64* end = it + map->length;

        for (; it < end; ++it)
            fprintf(fp, "%lf,", *it);

        fclose(fp);
    }
}


// --- ElevationMap

void InitElevationMap(ElevationMap* map, Dim dim, bool xWrap, bool yWrap)
{
    InitFloatMap(&map->base, dim, xWrap, yWrap);

    map->seaThreshold = 0.0;
}

bool IsBelowSeaLevel(ElevationMap* map, Coord c)
{
    uint32 i = GetIndex(&map->base, c);
    return map->base.data[i] < map->seaThreshold;
}

bool IsBelowSeaLevel(ElevationMap* map, uint32 i)
{
    return map->base.data[i] < map->seaThreshold;
}

// --- AreaMap

void InitPWAreaMap(PWAreaMap* map, Dim dim, bool xWrap, bool yWrap)
{
    InitFloatMap(&map->base, dim, xWrap, yWrap);

    map->areaList = NULL;
}

void ExitPWAreaMap(PWAreaMap* map)
{
    if (map->areaList)
        free(map->areaList);
    ExitFloatMap(&map->base);
}

void Clear(PWAreaMap* map)
{
    memset(map->base.data, 0, map->base.length * sizeof(float64));
}

void DefineAreas(PWAreaMap* map, MatchI mFunc, bool bDebug)
{
    Clear(map);

    if (map->areaList)
        free(map->areaList);
    map->areaList = (PWArea*)malloc(map->base.length * sizeof(PWArea));

    uint32 currentAreaID = 0;
    Coord c;
    //float64* it = map->base.data;
    PWArea* it = map->areaList;
    uint32 i = 0;

    for (c.y = 0; c.y < map->base.dim.h; ++c.y)
        for (c.x = 0; c.x < map->base.dim.w; ++c.x, ++it, ++i)
        {
            ++currentAreaID;
            InitPWArea(it, currentAreaID, c, mFunc(i));
            it->debug = bDebug;

            FillArea(map, c, it, mFunc);
        }
}

void FillArea(PWAreaMap* map, Coord c, PWArea* area, MatchI mFunc)
{
    map->segStack.resize(2);
    InitLineSeg(&map->segStack[0], c.y, c.x, c.x, 1);
    InitLineSeg(&map->segStack[1], c.y + 1, c.x, c.x, -1);

    while (!map->segStack.empty())
    {
        LineSeg seg = map->segStack.back();
        map->segStack.pop_back();
        ScanAndFillLine(map, seg, area, mFunc);
    }
}

// TODO: clean up, this is quite the mess for a scanline function
// TODO: we do all this work and nothing happens? maybe the mFunc in the print is important
void ScanAndFillLine(PWAreaMap* map, LineSeg seg, PWArea* area, MatchI mFunc)
{
    int16 dy = seg.y + seg.dy;
    uint16 vy = ValidateY(map, dy);
    if (vy == UINT16_MAX)
        return;

    uint16 odd = vy % 2;
    uint16 notOdd = !odd;
    int32 xStop = map->base.wrapX ? map->base.dim.w * -30 : -1;

    int32 lineFound = 0;
    int32 leftExtreme = INT32_MAX;

    for (int32 leftExt = seg.xLeft - odd; leftExt > xStop; --leftExt)
    {
        leftExtreme = leftExt;

        Coord c;
        c.x = ValidateX(map, leftExtreme);
        c.y = vy;
        uint32 i = GetIndex(&map->base, c);

        if (map->base.data[i] == 0 && area->trueMatch == mFunc(i))
        {
            map->base.data[i] = area->ind;
            ++area->size;
            lineFound = 1;
        }
        else
        {
            // if no line was found, then leftExtreme is fine, but if
            // a line was found going left, then we need to increment
            // xLeftExtreme to represent the inclusive end of the line
            if (lineFound)
            {
                ++leftExtreme;
            }
            break;
        }
    }

    int32 rightExtreme = INT32_MAX;
    xStop = map->base.wrapX ? map->base.dim.w * 20 : map->base.dim.w;

    for (int32 rightExt = seg.xLeft + lineFound - odd; rightExt < xStop; ++rightExt)
    {
        rightExtreme = rightExt; // need this saved

        Coord c;
        c.x = ValidateX(map, rightExtreme);
        c.y = vy;
        uint32 i = GetIndex(&map->base, c);

        if (map->base.data[i] == 0 && area->trueMatch == mFunc(i))
        {
            map->base.data[i] = area->ind;
            ++area->size;
            if (!lineFound)
            {
                lineFound = 1; // starting new line
                leftExtreme = rightExtreme;
            }
        }
        else if (lineFound) // found the right end of a line segment
        {
            lineFound = 0;

            // put same direction on stack
            map->segStack.emplace_back(LineSeg());
            InitLineSeg(&map->segStack.back(), c.y, leftExtreme, rightExtreme - 1, seg.dy);

            // determine if we must put reverse direction on stack
            if (leftExtreme < seg.xLeft - odd || rightExtreme >= seg.xRight + notOdd)
            {
                // out of shadow so put reverse direction on stack
                map->segStack.emplace_back(LineSeg());
                InitLineSeg(&map->segStack.back(), c.y, leftExtreme, rightExtreme - 1, -seg.dy);
            }

            if (rightExtreme >= seg.xRight + notOdd)
                break; // past the end of the parent line and no line found
        }
        else if (rightExtreme >= seg.xRight + notOdd)
            break;
    }

    if (lineFound) // still needing a line to be put on stack
    {
        if (leftExtreme != INT32_MAX && rightExtreme != INT32_MAX)
        {
            // put same direction on stack
            map->segStack.emplace_back(LineSeg());
            InitLineSeg(&map->segStack.back(), dy, leftExtreme, rightExtreme - 1, seg.dy);

            // determine if we must put reverse direction on stack
            if (leftExtreme < seg.xLeft - odd || rightExtreme >= seg.xRight + notOdd)
            {
                // out of shadow so put reverse direction on stack
                map->segStack.emplace_back(LineSeg());
                InitLineSeg(&map->segStack.back(), dy, leftExtreme, rightExtreme - 1, -seg.dy);
            }
        }
        else
            printf("seed filler has encountered a problem. Let's pretend it didn't happen\n");
    }
}

PWArea* GetAreaByID(PWAreaMap* map, uint32 id)
{
    PWArea* it = map->areaList;
    PWArea* end = it + map->base.length;

    for (; it < end; ++it)
        if (it->ind == id)
            return it;
    return NULL;
}

uint16 ValidateY(PWAreaMap* map, int16 y)
{
    if (map->base.wrapY)
    {
        int16 wrappedY = y % map->base.dim.h;
        return wrappedY >= 0 ? wrappedY : wrappedY + map->base.dim.h;
    }
    else if (y >= map->base.dim.h || y < 0)
        return UINT16_MAX;

    return y;
}

uint16 ValidateX(PWAreaMap* map, int16 x)
{
    if (map->base.wrapX)
    {
        int16 wrappedX = x % map->base.dim.w;
        return wrappedX >= 0 ? wrappedX : wrappedX + map->base.dim.w;
    }
    else if (x >= map->base.dim.w || x < 0)
        return UINT16_MAX;

    return x;
}

void PrintAreaList(PWAreaMap* map)
{
    if (map->areaList == NULL)
        return;

    PWArea* it = map->areaList;
    PWArea* end = it + map->base.length;

    for (; it < end; ++it)
    {
        printf("area id = %d, trueMatch = %d, size = %d, seedx = %d, seedy = %d\n",
            it->ind, it->trueMatch, it->size, it->coord.x, it->coord.y);
    }
}

// --- AreaMap

void InitPWArea(PWArea* area, uint32 ind, Coord c, bool trueMatch)
{
    area->ind = ind;
    area->coord = c;
    area->trueMatch = trueMatch;
    area->size = 0;
    area->debug = false;
}

// TODO: smarter debuging
void debugPrint(PWArea* area, char const* str)
{
    if (area->debug)
        printf(str);
}


// --- LineSeg

void InitLineSeg(LineSeg* seg, int16 y, int16 xLeft, int16 xRight, int16 dy)
{
    seg->y = y;
    seg->xLeft = xLeft;
    seg->xRight = xRight;
    seg->dy = dy;
}


// --- RiverMap

void InitRiverMap(RiverMap* map, ElevationMap * elevMap)
{
    map->eMap = elevMap;
    map->riverData = (RiverHex*)malloc(elevMap->base.length * sizeof(RiverHex));
    map->rivers = (River*)malloc(elevMap->base.length * 2 * sizeof(River));
    map->riverCnt = 0;
    map->riverThreshold = 0.0;

    Coord c;
    RiverHex* it = map->riverData;
    for (c.y = 0; c.y < map->eMap->base.dim.h; ++c.y)
        for (c.x = 0; c.x < map->eMap->base.dim.w; ++c.x, ++it)
            InitRiverHex(it, c);
}

RiverJunction* GetJunction(RiverMap* map, Coord c, bool isNorth)
{
    uint32 i = GetIndex(&map->eMap->base, c);
    if (isNorth)
        return &map->riverData[i].northJunction;
    return &map->riverData[i].southJunction;
}

RiverJunction* GetJunctionNeighbor(RiverMap* map, FlowDir dir, RiverJunction* junc)
{
    uint16 odd = junc->coord.y % 2;
    Coord c;

    switch (dir)
    {
    case fdWest:
        c.x = junc->coord.x + odd - 1;
        c.y = junc->isNorth ? junc->coord.y + 1 : junc->coord.y - 1;
        break;
    case fdEast:
        c.x = junc->coord.x + odd;
        c.y = junc->isNorth ? junc->coord.y + 1 : junc->coord.y - 1;
        break;
    case fdVert:
        c.x = junc->coord.x;
        c.y = junc->isNorth ? junc->coord.y + 2 : junc->coord.y - 2;
        break;
    case fdNone:
    default:
        assert(0 && "can't get junction neighbor in direction NOFLOW");
        break;
    }

    if (GetIndex(&map->eMap->base, c) != UINT32_MAX)
        return GetJunction(map, c, !junc->isNorth);

    return NULL;
}

// Get the west or east hex neighboring this junction
RiverHex* GetRiverHexNeighbor(RiverMap* map, RiverJunction* junc, bool westNeighbor)
{
    uint16 odd = junc->coord.y % 2;
    Coord c;

    c.y = junc->isNorth ? junc->coord.y + 1 : junc->coord.y - 1;
    c.x = westNeighbor ? junc->coord.x + odd - 1 : junc->coord.y + odd;

    uint32 i = GetIndex(&map->eMap->base, c);
    if (i != UINT32_MAX)
        return &map->riverData[i];
    return NULL;
}

uint8 GetJunctionsAroundHex(RiverMap* map, RiverHex* hex, RiverJunction* out[6])
{
    out[0] = &hex->northJunction;
    out[1] = &hex->southJunction;

    uint8 ind = 2;

    RiverJunction* junc = GetJunctionNeighbor(map, fdWest, &hex->northJunction);
    if (junc)
    {
        out[ind] = junc;
        ++ind;
    }

    junc = GetJunctionNeighbor(map, fdEast, &hex->northJunction);
    if (junc)
    {
        out[ind] = junc;
        ++ind;
    }

    junc = GetJunctionNeighbor(map, fdWest, &hex->southJunction);
    if (junc)
    {
        out[ind] = junc;
        ++ind;
    }

    junc = GetJunctionNeighbor(map, fdEast, &hex->southJunction);
    if (junc)
    {
        out[ind] = junc;
        ++ind;
    }

    return ind;
}

void SetJunctionAltitudes(RiverMap* map)
{
    Coord c;
    float64* elevIt = map->eMap->base.data;
    RiverHex* rivIt = map->riverData;

    // TODO: iterative
    for (c.y = 0; c.y < map->eMap->base.dim.h; ++c.y)
    {
        for (c.x = 0; c.x < map->eMap->base.dim.w; ++c.x, ++elevIt, ++rivIt)
        {
            float64 vertAlt = *elevIt;
            RiverHex* vertNbr = rivIt;
            // first do north
            RiverHex* westNbr = GetRiverHexNeighbor(map, &vertNbr->northJunction, true);
            RiverHex* eastNbr = GetRiverHexNeighbor(map, &vertNbr->northJunction, false);
            float64 westAlt = vertAlt;
            float64 eastAlt = vertAlt;

            if (westNbr)
            {
                uint32 i = GetIndex(&map->eMap->base, westNbr->coord);
                westAlt = map->eMap->base.data[i];
            }

            if (eastNbr)
            {
                uint32 i = GetIndex(&map->eMap->base, eastNbr->coord);
                eastAlt = map->eMap->base.data[i];
            }

            vertNbr->northJunction.altitude = std::min(std::min(vertAlt, westAlt), eastAlt);

            // then south
            westNbr = GetRiverHexNeighbor(map, &vertNbr->southJunction, true);
            eastNbr = GetRiverHexNeighbor(map, &vertNbr->southJunction, false);
            westAlt = vertAlt;
            eastAlt = vertAlt;

            if (westNbr)
            {
                uint32 i = GetIndex(&map->eMap->base, westNbr->coord);
                westAlt = map->eMap->base.data[i];
            }

            if (eastNbr)
            {
                uint32 i = GetIndex(&map->eMap->base, eastNbr->coord);
                eastAlt = map->eMap->base.data[i];
            }

            vertNbr->southJunction.altitude = std::min(std::min(vertAlt, westAlt), eastAlt);
        }
    }
}

bool isLake(RiverMap* map, RiverJunction* junc)
{
    // first exclude the map edges that don't have neighbors
    if ((junc->coord.y == 0 && !junc->isNorth) ||
        (junc->coord.y == map->eMap->base.dim.h - 1 && junc->isNorth) ||
        // exclude altitudes below sea level
        (junc->altitude < map->eMap->seaThreshold))
        return false;

    RiverJunction* vertNbr = GetJunctionNeighbor(map, fdVert, junc);
    float64 vertAlt = vertNbr ? vertNbr->altitude : junc->altitude;

    RiverJunction* westNbr = GetJunctionNeighbor(map, fdWest, junc);
    float64 westAlt = westNbr ? westNbr->altitude : junc->altitude;

    RiverJunction* eastNbr = GetJunctionNeighbor(map, fdEast, junc);
    float64 eastAlt = eastNbr ? eastNbr->altitude : junc->altitude;

    float64 lowest = std::min(std::min(vertAlt, westAlt), std::min(eastAlt, junc->altitude));

    return junc->altitude == lowest;
}

float64 GetNeighborAverage(RiverMap* map, RiverJunction* junc)
{
    uint8 count = 0;

    RiverJunction* vertNbr = GetJunctionNeighbor(map, fdVert, junc);
    float64 vertAlt = 0;
    if (vertNbr)
    {
        vertAlt = vertNbr->altitude;
        ++count;
    }

    RiverJunction* westNbr = GetJunctionNeighbor(map, fdWest, junc);
    float64 westAlt = 0;
    if (westNbr)
    {
        westAlt = westNbr->altitude;
        ++count;
    }

    RiverJunction* eastNbr = GetJunctionNeighbor(map, fdEast, junc);
    float64 eastAlt = 0;
    if (eastNbr)
    {
        eastAlt = eastNbr->altitude;
        ++count;
    }

    float64 lowest = westAlt < eastAlt ? westAlt : eastAlt;
    if (vertAlt != 0.0 && lowest > vertAlt)
        lowest = vertAlt;

    // local avg = (vertAltitude + westAltitude + eastAltitude)/count

    return lowest + 0.0000001;
}

// this function alters the drainage pattern. written by Bobert13
void SiltifyLakes(RiverMap* map)
{
    uint32 cap = map->eMap->base.length * 2;
    RiverJunction** lakeList = (RiverJunction**)malloc(cap * sizeof(void*));
    RiverJunction** lakeListEnd = lakeList + cap;
    bool* onQueueMapNorth = (bool*)malloc(map->eMap->base.length * sizeof(bool));
    bool* onQueueMapSouth = (bool*)malloc(map->eMap->base.length * sizeof(bool));

    RiverHex* it = map->riverData;
    RiverHex* end = it + map->eMap->base.length;
    RiverJunction** lakeIns = lakeList;
    bool* northIns = onQueueMapNorth;
    bool* southIns = onQueueMapSouth;

    for (; it < end; ++it, ++northIns, ++southIns)
    {
        if (isLake(map, &it->northJunction))
        {
            *lakeIns = &it->northJunction;
            ++lakeIns;
            *northIns = true;
        }
        else
            *northIns = false;

        if (isLake(map, &it->southJunction))
        {
            *lakeIns = &it->southJunction;
            ++lakeIns;
            *southIns = true;
        }
        else
            *southIns = false;
    }

    uint32 numLakes = (uint32)(lakeIns - lakeList);
    --lakeIns;

    uint32 iter = 0;
    for (; lakeIns >= lakeList; --lakeIns)
    {
        ++iter;
        if (iter > 100000000)
        {
            printf("ERROR - Endless loop in lake siltification.");
            break;
        }

        RiverJunction* lake = *lakeIns;
        --lakeIns;

        uint32 i = GetIndex(&map->eMap->base, lake->coord);
        if (lake->isNorth)
            onQueueMapNorth[i] = false;
        else
            onQueueMapSouth[i] = false;

        lake->altitude += GetNeighborAverage(map, lake);

        for (uint32 dir = fdWest; dir <= fdVert; ++dir)
        {
            RiverJunction* neighbor = GetJunctionNeighbor(map, (FlowDir)dir, lake);

            if (neighbor && isLake(map, neighbor))
            {
                uint32 ii = GetIndex(&map->eMap->base, neighbor->coord);

                if (neighbor->isNorth)
                {
                    if (!onQueueMapNorth[ii])
                    {
                        if (lakeIns == lakeListEnd)
                            assert(0 && "Need to expand the allocation");
                        *(++lakeIns) = neighbor;
                        onQueueMapNorth[ii] = true;
                    }
                }
                else if (!onQueueMapSouth[ii])
                {
                    if (lakeIns == lakeListEnd)
                        assert(0 && "Need to expand the allocation");
                    *(++lakeIns) = neighbor;
                    onQueueMapSouth[ii] = true;
                }
            }
        }
    }

    free(onQueueMapSouth);
    free(onQueueMapNorth);
    free(lakeList);
}

void RecreateNewLakes(RiverMap* map, float64* rainfallMap)
{
    LakeDataUtil ldu;
    ldu.lakesToAdd = (uint32)(map->eMap->base.length * gSet.landPercent * gSet.lakePercent);
    ldu.lakesAdded = 0;
    ldu.currentLakeID = 1;

    RiverHex** riverHexList = (RiverHex **)malloc(map->eMap->base.length * sizeof(void*));
    RiverHex** riverHexIns = riverHexList;

    float64* it = map->eMap->base.data;
    float64* end = it + map->eMap->base.length;
    RiverHex* rivIt = map->riverData;
    float64* rainIt = rainfallMap;

    for (; it < end; ++it, ++rivIt, ++rainIt)
    {
        if (*it > map->eMap->seaThreshold)
        {
            rivIt->rainfall = *rainIt;
            *riverHexIns = rivIt;
            ++riverHexIns;
        }
    }

    std::sort(riverHexList, riverHexIns, [](RiverHex* a, RiverHex* b) { return a->rainfall > b->rainfall; });

    uint32 rListSize = (uint32)(riverHexIns - riverHexList);
    uint32 portion = rListSize / 4u; // dividing ints automatically floors
    riverHexIns -= portion;

    std::random_shuffle(riverHexList, riverHexIns);

    RiverHex** rivHexIt = riverHexList;
    RiverHex** rivHexEnd = riverHexIns;

    for (; rivHexIt < rivHexEnd && ldu.lakesAdded < ldu.lakesToAdd; ++rivHexIt)
    {
        RiverHex* hex = *rivHexIt;

        if (ValidLakeHex(map, hex, &ldu))
        {
            std::vector<RiverHex*> growthQueue;
            std::vector<RiverHex*> lakeList;
            ldu.currentLakeSize = 0;
            uint32 lakeSize = GetRandomLakeSize(map);
            GrowLake(map, hex, lakeSize, &ldu, lakeList, growthQueue);
            while (!growthQueue.empty())
            {
                RiverHex* nextLake = growthQueue.back();
                growthQueue.pop_back();
                GrowLake(map, nextLake, lakeSize, &ldu, lakeList, growthQueue);
            }
            // process junctions for all lake tiles
            // first find lowest junction
            RiverJunction* lowestJunction = &lakeList.front()->northJunction;
            for (RiverHex* thisLake : lakeList)
            {
                RiverJunction* thisLowest = GetLowestJunctionAroundHex(map, thisLake);
                if (lowestJunction->altitude > thisLowest->altitude)
                    lowestJunction = thisLowest;
            }
            // set up outflow
            lowestJunction->isOutflow = true;
            std::vector<FlowDir> dirs = GetValidFlows(map, lowestJunction);
            if (dirs.size() == 1)
                lowestJunction->flow = dirs.front();
            else
            {
                printf("ERROR - Bad assumption made. Lake outflow has %llu valid flows", dirs.size());
            }
            // then update all junctions with outflow
            for (RiverHex* thisLake : lakeList)
            {
                SetOutflowForLakeHex(map, thisLake, lowestJunction);
            }

            ++ldu.currentLakeID;
        }
    }

    free(riverHexList);
}

void GrowLake(RiverMap* map, RiverHex * lakeHex, uint32 lakeSize, LakeDataUtil* ldu,
    std::vector<RiverHex*> & lakeList, std::vector<RiverHex*> & growthQueue)
{
    // Creates a lake here and places valid neighbors on the queue for later
    //   growth stages return if lake has met size reqs
    if (ldu->currentLakeSize >= lakeSize || ldu->lakesAdded >= ldu->lakesToAdd)
        return;

    // lake has grown, increase size
    lakeHex->lakeID = ldu->currentLakeID;
    ++ldu->currentLakeSize;
    ++ldu->lakesAdded;
    lakeList.push_back(lakeHex);

    // choose random neighbors to put on queue
    std::vector<uint32> neighbors = GetRadiusAroundCell(map->eMap->base.dim, lakeHex->coord, 1);
    for (uint32 ind : neighbors)
    {
        RiverHex* neighbor = map->riverData + ind;
        if (ValidLakeHex(map, neighbor, ldu) &&
            PWRandInt(1, 3) == 1)
            growthQueue.push_back(neighbor);
    }
}

RiverJunction* GetLowestJunctionAroundHex(RiverMap* map, RiverHex * lakeHex)
{
    RiverJunction* lowestJunction = NULL;
    RiverJunction* nJunctionList[6];
    uint8 count = GetJunctionsAroundHex(map, lakeHex, nJunctionList);

    lowestJunction = *nJunctionList;
    RiverJunction** it = nJunctionList;
    RiverJunction** end = it + count;
    ++it;

    for (; it < end; ++it)
        if (lowestJunction->altitude > (*it)->altitude)
            lowestJunction = *it;

    return lowestJunction;
}

void SetOutflowForLakeHex(RiverMap* map, RiverHex * lakeHex, RiverJunction* outflow)
{
    RiverJunction* nJunctionList[6];
    uint8 count = GetJunctionsAroundHex(map, lakeHex, nJunctionList);

    RiverJunction** it = nJunctionList;
    RiverJunction** end = it + count;

    for (; it < end; ++it)
        // skip actual outflow
        if (!(*it)->isOutflow)
        {
            (*it)->submerged = true;
            (*it)->flow = fdNone;
            (*it)->outflow = outflow;
        }
}

uint32 GetRandomLakeSize(RiverMap* map)
{
    // puts a bell curve on desired lake size
    uint32 d3 = PWRandInt(1, 3);
    uint32 d4m1 = PWRandInt(0, 3);
    return d3 + d4m1;
}

bool ValidLakeHex(RiverMap* map, RiverHex* lakeHex, LakeDataUtil* ldu)
{
    // a valid lake hex must not be protected and must not be adjacent to
    // ocean or lake with different lake ID
    uint32 ii = GetIndex(&map->eMap->base, lakeHex->coord);

    // can't be on a volcano
    MapTile* plot = gMap + ii;
    if (plot->feature != fNONE)
        return false;

    std::vector<uint32> cellList = GetRadiusAroundCell(map->eMap->base.dim, lakeHex->coord, 1);
    for (uint32 i : cellList)
    {
        RiverHex* nHex = map->riverData + i;

        if (map->eMap->base.data[i] < map->eMap->seaThreshold)
            return false;
        else if (nHex->lakeID != UINT32_MAX && nHex->lakeID != ldu->currentLakeSize)
            return false;
    }

    // assume true until problem occurs
    return true;
}

RiverHex* GetInitialLake(RiverMap* map, RiverJunction* junc, FlowDir prospectiveFlow)
{
    switch (prospectiveFlow)
    {
    case fdVert:
        return map->riverData + GetIndex(&map->eMap->base, junc->coord);
    case fdWest:
        return GetRiverHexNeighbor(map, junc, false);
    case fdEast:
        return GetRiverHexNeighbor(map, junc, true);
    default:
        break;
    }

    return nullptr;
}

static uint32 GetJuncData(RiverMap* map, RiverJunction*** out)
{
    uint32 juncListLen = map->eMap->base.length * 2;
    *out = (RiverJunction**)malloc(juncListLen * sizeof(void*));
    RiverHex* it = map->riverData;
    RiverHex* end = it + map->eMap->base.length;
    RiverJunction** juncIns = *out;
    for (; it < end; ++it)
    {
        *juncIns = &it->northJunction;
        ++juncIns;
        *juncIns = &it->southJunction;
        ++juncIns;
    }

    return (uint32)(juncIns - *out);
}

void SetFlowDestinations(RiverMap* map)
{
    RiverJunction** junctionList = nullptr;
    uint32 size = GetJuncData(map, &junctionList);

    RiverJunction** it = junctionList;
    RiverJunction** end = it + size;

    std::sort(it, end, [](RiverJunction* a, RiverJunction* b) { return a->altitude > b->altitude; });

    uint32 validFlowCount = 0;

    for (; it < end; ++it)
    {
        RiverJunction* junction = *it;

        // don't overwrite lake outflows
        if (!junction->isOutflow && !junction->submerged)
        {
            std::vector<FlowDir> validList = GetValidFlows(map, junction);

            if (validList.empty())
                junction->flow = fdNone;
            else
            {
                uint32 choice = PWRandInt(0, (int32)(validList.size() - 1));
                junction->flow = validList[choice];
                ++validFlowCount;
            }
        }
    }

    free(junctionList);
}

std::vector<FlowDir> GetValidFlows(RiverMap* map, RiverJunction* junc)
{
    std::vector<FlowDir> validList;

    for (uint8 dir = fdWest; dir <= fdVert; ++dir)
    {
        RiverJunction* neighbor = GetJunctionNeighbor(map, (FlowDir)dir, junc);
        if (neighbor && neighbor->altitude < junc->altitude)
            validList.push_back((FlowDir)dir);
    }

    return validList;
}

bool IsTouchingOcean(RiverMap* map, RiverJunction* junc)
{
    uint32 i = GetIndex(&map->eMap->base, junc->coord);
    MapTile* plot = gMap + i;

    if (!junc || IsWater(plot))
        return true;

    RiverHex* westNeighbor = GetRiverHexNeighbor(map, junc, true);
    if (!westNeighbor || IsWater(gMap + GetIndex(&map->eMap->base, westNeighbor->coord)))
        return true;

    RiverHex* eastNeighbor = GetRiverHexNeighbor(map, junc, false);
    if (!eastNeighbor || IsWater(gMap + GetIndex(&map->eMap->base, eastNeighbor->coord)))
        return true;

    return false;
}

static uint32 GetFilteredJuncData(RiverMap* map, RiverJunction*** out)
{
    uint32 juncListLen = map->eMap->base.length * 2;
    *out = (RiverJunction**)malloc(juncListLen * sizeof(void*));
    RiverHex* it = map->riverData;
    RiverHex* end = it + map->eMap->base.length;
    RiverJunction** juncIns = *out;
    for (; it < end; ++it)
    {
        if (!IsTouchingOcean(map, &it->northJunction))
        {
            *juncIns = &it->northJunction;
            ++juncIns;
        }
        if (!IsTouchingOcean(map, &it->southJunction))
        {
            *juncIns = &it->southJunction;
            ++juncIns;
        }
    }

    return (uint32)(juncIns - *out);
}

void SetRiverSizes(RiverMap* map, float64 * locRainfallMap)
{
    // only include junctions not touching ocean in this list
    RiverJunction** junctionList = nullptr;
    uint32 size = GetFilteredJuncData(map, &junctionList);

    RiverJunction** it = junctionList;
    RiverJunction** end = it + size;

    // highest altitude first
    std::sort(it, end, [](RiverJunction* a, RiverJunction* b) { return a->altitude > b->altitude; });

    for (; it < end; ++it)
    {
        RiverJunction* junction = *it;
        RiverJunction* next = *it;
        uint32 i = GetIndex(&map->eMap->base, junction->coord);

        uint32 courseLength = 0;
        float64 rainToAdd = locRainfallMap[i];

        for (;;)
        {
            if (next->isOutflow)
                rainToAdd *= 2;

            next->size += rainToAdd;
            ++courseLength;

            if ((next->flow == fdNone || IsTouchingOcean(map, next)) &&
                // make sure it has no flow if touching water, unless it's an outflow
                next->isOutflow == false)
            {
                next->flow = fdNone;

                // if there is no outflow to jump to, this is the end
                if (!next->outflow)
                    break;
            }

            next = GetNextJunctionInFlow(map, next);

            if (!next)
                break;
        }
    }

    // now sort by river size to find river threshold
    it = junctionList;
    std::sort(it, end, [](RiverJunction* a, RiverJunction* b) { return a->size > b->size; });

    uint32 riverIndex = (uint32)(floor(gSet.riverPercent * size));
    map->riverThreshold = junctionList[riverIndex]->size;

    free(junctionList);
}

RiverJunction* GetNextJunctionInFlow(RiverMap* map, RiverJunction* junc)
{
    // use outflow if valid
    if (junc)
        return junc->outflow;

    return GetJunctionNeighbor(map, (FlowDir)junc->flow, junc);
}

// this function must be called AFTER sizes are determined.
bool IsRiverSource(RiverMap* map, RiverJunction* junc)
{
    // are we big enough to be a river?
    if (junc->size <= map->riverThreshold)
        return false;
    // am I a lake outflow?
    if (junc->isOutflow)
        // outflows are also sources
        return true;
    // am i touching water?
    if (IsTouchingOcean(map, junc))
        return false;

    // are my predescessors that flow into me big enough to be rivers?
    RiverJunction* westJunc = GetJunctionNeighbor(map, fdWest, junc);
    if (westJunc && westJunc->flow == fdEast && westJunc->size > map->riverThreshold)
        return false;
    RiverJunction* eastJunc = GetJunctionNeighbor(map, fdEast, junc);
    if (eastJunc && eastJunc->flow == fdWest && eastJunc->size > map->riverThreshold)
        return false;
    RiverJunction* vertJunc = GetJunctionNeighbor(map, fdVert, junc);
    if (vertJunc && vertJunc->flow == fdVert && vertJunc->size > map->riverThreshold)
        return false;

    // no big rivers flowing into me, so I must be a source
    return true;
}

bool NotLongEnough(River& river)
{
    if (river.junctions.size() >= gSet.minRiverLength || river.sourceJunc->isOutflow)
        return false;
    return true;
}

// TODO: this function is appalling
void CreateRiverList(RiverMap* map)
{
    // this list describes rivers from source to  water, (lake or ocean)
    // with longer rivers overwriting the rawID of shorter riverSizeMap
    // riverID used in game is a different variable. Every river source
    // has a rawID but not all with end up with a riverID
    assert(map->rivers);

    uint32 currentRawID = 0;
    RiverHex* it = map->riverData;
    RiverHex* end = it + map->eMap->base.length;
    River* rIns = map->rivers;
    for (; it < end; ++it)
    {
        if (IsRiverSource(map, &it->northJunction))
        {
            InitRiver(rIns, &it->northJunction, currentRawID);
            ++rIns;
            ++currentRawID;
        }

        if (IsRiverSource(map, &it->southJunction))
        {
            InitRiver(rIns, &it->southJunction, currentRawID);
            ++rIns;
            ++currentRawID;
        }
    }

    // idea: if rivers grow at one length per loop, you can allow late comers to overwrite
    // their predecessors, as they will always be the longer river. Just make sure to clarify
    // that river length is not the length of a particular rawID, but length from source
    // to the ocean
    bool growing = true;
    River* rIt = map->rivers;
    River* rEnd = rIns;
    while (growing)
    {
        // assume this until something grows
        growing = false;
        rIt = map->rivers;

        for (; rIt < rEnd; ++rIt)
        {
            RiverJunction* lastJunc = rIt->junctions.back();

            if (lastJunc->flow != fdNone)
            {
                RiverJunction* nextJunc = GetJunctionNeighbor(map, (FlowDir)lastJunc->flow, lastJunc);
                if (nextJunc)
                {
                    // overwrite whatever was there
                    nextJunc->rawID = lastJunc->rawID;
                    // Add to river for now, but all will be deleted before next pass
                    Add(rIt, nextJunc);
                    AddParent(nextJunc, lastJunc);
                    growing = true;
                }
            }
        }
    }

    // TODO: this seems terribly unnecesary . . .
    currentRawID = 0;
    it = map->riverData;
    rIns = map->rivers;
    // river sources in self.riverList again
    for (; it < end; ++it)
    {
        if (IsRiverSource(map, &it->northJunction))
        {
            InitRiver(rIns, &it->northJunction, currentRawID);
            ++rIns;
            ++currentRawID;
        }

        if (IsRiverSource(map, &it->southJunction))
        {
            InitRiver(rIns, &it->southJunction, currentRawID);
            ++rIns;
            ++currentRawID;
        }
    }

    // for this pass add to river list until rawID changes
    rIt = map->rivers;
    rEnd = rIns;
    for (; rIt < rEnd; ++rIt)
    {
        RiverJunction* lastJunc = rIt->junctions.back();

        for (; lastJunc->flow != fdNone;)
        {
            RiverJunction* nextJunc = GetJunctionNeighbor(map, (FlowDir)lastJunc->flow, lastJunc);
            if (nextJunc && nextJunc->rawID == lastJunc->rawID)
            {
                Add(rIt, nextJunc);
                lastJunc = nextJunc;
            }
            else
                break;
        }
    }

    // now strip out all the shorties
    if (gSet.minRiverLength)
        map->riverCnt = (uint32)(std::remove_if(map->rivers, rEnd, NotLongEnough) - map->rivers);
    else
        map->riverCnt = (uint32)(rEnd - map->rivers);
}

void AssignRiverIDs(RiverMap* map)
{
    // sort river list by largest first
    River* it = map->rivers;
    River* end = it + map->riverCnt;
    // TODO: don't think this will work properly w/o proper constructors
    std::sort(map->rivers, end, [](River& a, River& b) { return a.junctions.size() > b.junctions.size(); });

    // this should closely match river index witch should be id+1
    uint32 currentRiverID = 0;

    for (; it < end; ++it, ++currentRiverID)
    {
        it->riverID = currentRiverID;
        for (RiverJunction* junc : it->junctions)
            junc->id = currentRiverID;
    }
}

RiverHex* GetRiverHexForJunction(RiverMap* map, RiverJunction* junc)
{
    switch (junc->flow)
    {
    case fdVert:
        return GetRiverHexNeighbor(map, junc, true);
    case fdWest:
        return junc->isNorth ?
            GetRiverHexNeighbor(map, junc, true) :
            map->riverData + GetIndex(&map->eMap->base, junc->coord);
    case fdEast:
        return junc->isNorth ?
            GetRiverHexNeighbor(map, junc, false) :
            map->riverData + GetIndex(&map->eMap->base, junc->coord);
    default:
        break;
    }

    return nullptr;
}

struct FlowDirRet
{
    TileFlowDirection WOfRiver;
    TileFlowDirection NWOfRiver;
    TileFlowDirection NEOfRiver;

    uint32 id;
};

// This function returns the flow directions needed by civ
FlowDirRet GetFlowDirections(RiverMap* map, Coord c)
{
    uint32 i = GetIndex(&map->eMap->base, c);

    TileFlowDirection WOfRiver = tfdNO_FLOW;
    uint32 WID = UINT32_MAX;

    Coord coord;
    GetNeighbor(&map->eMap->base, c, dNE, &coord);
    uint32 ii = GetIndex(&map->eMap->base, coord);

    if (ii != UINT32_MAX &&
        map->riverData[ii].southJunction.flow == fdVert &&
        map->riverData[ii].southJunction.size > map->riverThreshold &&
        map->riverData[ii].southJunction.id != UINT32_MAX)
    {
        WOfRiver = tfdSOUTH;
        WID = map->riverData[ii].southJunction.id;
    }

    GetNeighbor(&map->eMap->base, c, dSE, &coord);
    ii = GetIndex(&map->eMap->base, coord);

    if (ii != UINT32_MAX &&
        map->riverData[ii].northJunction.flow == fdVert &&
        map->riverData[ii].northJunction.size > map->riverThreshold &&
        map->riverData[ii].northJunction.id != UINT32_MAX)
    {
        WOfRiver = tfdNORTH;
        WID = map->riverData[ii].northJunction.id;
    }


    TileFlowDirection NWOfRiver = tfdNO_FLOW;
    uint32 NWID = UINT32_MAX;

    GetNeighbor(&map->eMap->base, c, dSE, &coord);
    ii = GetIndex(&map->eMap->base, coord);

    if (ii != UINT32_MAX &&
        map->riverData[ii].northJunction.flow == fdWest &&
        map->riverData[ii].northJunction.size > map->riverThreshold &&
        map->riverData[ii].northJunction.id != UINT32_MAX)
    {
        NWOfRiver = tfdSOUTHWEST;
        NWID = map->riverData[ii].northJunction.id;
    }

    if (map->riverData[i].southJunction.flow == fdEast &&
        map->riverData[i].southJunction.size > map->riverThreshold &&
        map->riverData[i].southJunction.id != UINT32_MAX)
    {
        NWOfRiver = tfdNORTHEAST;
        NWID = map->riverData[i].southJunction.id;
    }


    TileFlowDirection NEOfRiver = tfdNO_FLOW;
    uint32 NEID = UINT32_MAX;

    GetNeighbor(&map->eMap->base, c, dSW, &coord);
    ii = GetIndex(&map->eMap->base, coord);

    if (ii != UINT32_MAX &&
        map->riverData[ii].northJunction.flow == fdEast &&
        map->riverData[ii].northJunction.size > map->riverThreshold &&
        map->riverData[ii].northJunction.id != UINT32_MAX)
    {
        NEOfRiver = tfdSOUTHEAST;
        NEID = map->riverData[ii].northJunction.id;
    }

    if (map->riverData[i].southJunction.flow == fdWest &&
        map->riverData[i].southJunction.size > map->riverThreshold &&
        map->riverData[i].southJunction.id != UINT32_MAX)
    {
        NEOfRiver = tfdNORTHWEST;
        NEID = map->riverData[i].southJunction.id;
    }

    // none of this works if river list has been sorted!
    uint32 ID = UINT32_MAX;
    // use ID of longest river
    uint32 WIDLength = 0;
    uint32 NWIDLength = 0;
    uint32 NEIDLength = 0;

    if (WID != UINT32_MAX)
        WIDLength = (uint32)map->rivers[WID].junctions.size();
    if (NWID != UINT32_MAX)
        NWIDLength = (uint32)map->rivers[NWID].junctions.size();
    if (NEID != UINT32_MAX)
        NEIDLength = (uint32)map->rivers[NEID].junctions.size();

    // fight between WID and NWID
    if (WIDLength >= NWIDLength && WIDLength >= NEIDLength)
        ID = WID;
    else if (NWIDLength >= WIDLength && NWIDLength >= NEIDLength)
        ID = NWID;
    else
        ID = NEID;

    return { WOfRiver, NWOfRiver, NEOfRiver, ID };
}


// --- RiverHex

void InitRiverHex(RiverHex* hex, Coord c)
{
    hex->coord = c;
    InitRiverJunction(&hex->northJunction, c, true);
    InitRiverJunction(&hex->southJunction, c, false);
    hex->lakeID = UINT32_MAX;
    hex->rainfall = 0.0;
}


// --- RiverJunction

void InitRiverJunction(RiverJunction* junc, Coord c, bool isNorth)
{
    junc->coord = c;
    junc->isNorth = isNorth;
    junc->altitude = 0.0;
    junc->flow = fdNone;
    junc->size = 0.0;
    junc->submerged = false;
    junc->outflow = NULL;
    junc->isOutflow = false;
    junc->rawID = UINT32_MAX;
    junc->id = UINT32_MAX;
}

void AddParent(RiverJunction* junc, RiverJunction* parent)
{
    for (RiverJunction* p : junc->parentJunctions)
        if (p == parent)
            return;
    junc->parentJunctions.push_back(parent);
}

void PrintRiverJunction(RiverJunction* junc)
{
    char const* flowStr[] = { "NONE", "WEST", "EAST", "VERT"};
    printf("junction at %d, %d isNorth=%d, flow=%s, size=%f, submerged=%d, outflow=%llx, isOutflow=%d riverID = %d",
        junc->coord.x, junc->coord.y, junc->isNorth, flowStr[junc->flow], junc->size, junc->submerged, (uint64)junc->outflow, junc->isOutflow, junc->id);
}


// --- River

void InitRiver(River * river, RiverJunction * sourceJunc, uint32 rawID)
{
    river->sourceJunc = sourceJunc;
    river->riverID = UINT32_MAX;

    new (&river->junctions) std::vector<RiverJunction*>;
    sourceJunc->rawID = rawID;
    river->junctions.push_back(sourceJunc);
}

void Add(River* river, RiverJunction* junc)
{
    river->junctions.push_back(junc);
}

uint32 GetLength(River* river)
{
    return (uint32)river->junctions.size();
}


// --- Civ Hook Functions -----------------------------------------------------

uint32 CountLand(uint32 len, uint8* plotTypes, uint8* terrainTypes)
{
    // this function is used sometimes for sanity checks during debugging
    // and also for initializing the feateure
    uint32 landCount = 0;

    uint8* it = plotTypes;
    uint8* end = it + len;

    for (; it < end; ++it)
        if (*it != ptOcean)
            ++landCount;

    return landCount;
}

// the "main()" of the alg
void GenerateMap()
{
    Dim dim = { gSet.width, gSet.height };
    uint32 len = dim.w * dim.h;

    InitImageWriter(dim.w, dim.h, hexOffsets);

    uint8* plotTypes = (uint8*)calloc(len, sizeof *plotTypes);
    uint8* terrainTypes = (uint8*)calloc(len, sizeof *terrainTypes);
    ElevationMap map;
    FloatMap rainMap;
    FloatMap tempMap;
    PangaeaBreaker pb;

    uint32 iter = 0;

    for (; iter < 10; ++iter)
    {
        GeneratePlotTypes(dim, &map, &rainMap, &tempMap, &plotTypes);
        GenerateTerrain(&map, &rainMap, &tempMap, &terrainTypes);
        FinalAlterations(&map, plotTypes, terrainTypes);
        GenerateCoasts(&map, plotTypes, terrainTypes);

        DrawHexes(plotTypes, sizeof *plotTypes, PaintPlotTypes);
        SaveMap("22_PlotTypes.bmp");
        DrawHexes(terrainTypes, sizeof *terrainTypes, PaintTerrainTypes);
        AddStamps(plotTypes, sizeof * plotTypes, StampElevationViaPlotTypes);
        SaveMap("23_TerrainTypes.bmp");

        InitPangaeaBreaker(&pb, &map, terrainTypes);
        if (BreakPangaeas(&pb, plotTypes, terrainTypes))
            break;

        // TODO: don't be this destructive
        ExitPangaeaBreaker(&pb);
        ExitFloatMap(&tempMap);
        ExitFloatMap(&rainMap);
        ExitFloatMap(&map.base);
    }

    gThrs.coast = map.seaThreshold;
    //DrawHexes(map.base.data, sizeof *map.base.data, PaintElevationMap);
    //DrawHexes(plotTypes, sizeof *plotTypes, PaintPlotTypes);
    DrawHexes(map.base.data, sizeof *map.base.data, PaintUnitFloatGradient);
    SaveMap("map.bmp");

    if (iter == 10)
    {
        printf("Failed to break up Pangea!");
        return;
    }

    CreateNewWorldMap(&pb);

    ApplyTerrain(len, plotTypes, terrainTypes);

    // TODO:
    //AreaBuilder.Recalculate()
    //TerrainBuilder.AnalyzeChokepoints()
    //TerrainBuilder.StampContinents()
    //
    //local iContinentBoundaryPlots = nil;
    //if g_FEATURE_VOLCANO ~= nil then
    //    iContinentBoundaryPlots = GetContinentBoundaryPlotCount(g_iW, g_iH);
    //end
    //local biggest_area = Areas.FindBiggestArea(false);
    //print("After Adding Hills: ", biggest_area:GetPlotCount());
    //if g_FEATURE_VOLCANO ~= nil then
    //    AddTerrainFromContinents(terrainTypes, g_iW, g_iH, iContinentBoundaryPlots);
    //end

    RiverMap riverMap;
    InitRiverMap(&riverMap, &map);
    SetJunctionAltitudes(&riverMap);
    SiltifyLakes(&riverMap);
    RecreateNewLakes(&riverMap, rainMap.data);
    SetFlowDestinations(&riverMap);
    SetRiverSizes(&riverMap, rainMap.data);
    CreateRiverList(&riverMap);
    AssignRiverIDs(&riverMap);

    AddLakes(&riverMap);
    AddRivers(&riverMap);

    // TODO: ? maybe not
    //AreaBuilder.Recalculate()
    //local biggest_area = Areas.FindBiggestArea(false);
    //print("Biggest area size = ", biggest_area:GetPlotCount());

    // making this global for later use
    gThrs.zeroTrees = FindThresholdFromPercent(&rainMap, gSet.zeroTreesPercent, true);
    gThrs.jungle = FindThresholdFromPercent(&rainMap, gSet.junglePercent, true);

    //local nwGen = NaturalWonderGenerator.Create({
    //    numberToPlace = GameInfo.Maps[Map.GetMapSize()].NumNaturalWonders + mc.naturalWonderExtra,
    //});

    AddFeatures(&map, &rainMap, &tempMap);

    // TODO: add expansion filters (Gathering Storm)
    //if Gathering Storm
    {
        ClearFloodPlains(&riverMap);
        uint32 iMinFloodplainSize = 2;
        uint32 iMaxFloodplainSize = 12;
        // TODO:
        //TerrainBuilder.GenerateFloodplains(true, iMinFloodplainSize, iMaxFloodplainSize);
    }

    //AddCliffs(plotTypes, terrainTypes);

    //if Gathering Storm
    {
        //local args = {rainfall = 3}
        //featuregen = FeatureGenerator.Create(args)
        //featuregen.iNumLandPlots = CountLand(nil, terrainTypes)
        //featuregen:AddIceToMap();
        //featuregen:AddFeaturesFromContinents()
        //
        //MarkCoastalLowlands();
    }

    //resourcesConfig = MapConfiguration.GetValue("resources");
    //local resGen = ResourceGenerator.Create({
    //    resources = resourcesConfig,
    //    bLandBias = true,
    //});

    //print("Creating start plot database.");
    //local startConfig = MapConfiguration.GetValue("start");
    //local start_plot_database = AssignStartingPlots.Create({
    //    MIN_MAJOR_CIV_FERTILITY = 300,
    //    MIN_MINOR_CIV_FERTILITY = 50,
    //    MIN_BARBARIAN_FERTILITY = 1,
    //    START_MIN_Y = 15,
    //    START_MAX_Y = 15,
    //    START_CONFIG = startConfig,
    //    LAND = true,
    //})

    //AddGoodies(dim.w, dim.h);
    //print("finished adding goodies")

    ExitImageWriter();
}


// --- Generation Functions ---------------------------------------------------

void GenerateTwistedPerlinMap(Dim dim, bool xWrap, bool yWrap,
    float64 minFreq, float64 maxFreq, float64 varFreq,
    FloatMap* out)
{
    FloatMap inputNoise;
    InitFloatMap(&inputNoise, dim, xWrap, yWrap);
    GenerateNoise(&inputNoise);
    Normalize(&inputNoise);
    DrawHexes(inputNoise.data, sizeof *inputNoise.data, PaintUnitFloatGradient);
    SaveMap("00_initNoise.bmp");

    FloatMap freqMap;
    InitFloatMap(&freqMap, dim, xWrap, yWrap);

    float64* ins = freqMap.data;
    Coord c;
    uint16 w = dim.w;
    float64 h = dim.h * YtoXRatio;
    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        uint16 odd = c.y % 2;

        for (c.x = 0; c.x < dim.w; ++c.x, ++ins)
        {
            float64 x = c.x + odd * 0.5;
            *ins = GetPerlinNoise(x, c.y * YtoXRatio, w, h, varFreq, 1.0, 0.1, 8, &inputNoise);
        }
    }
    Normalize(&freqMap);
    DrawHexes(freqMap.data, sizeof *freqMap.data, PaintUnitFloatGradient);
    SaveMap("01_freqNoise.bmp");

    FloatMap* twistMap = out;
    InitFloatMap(twistMap, dim, xWrap, yWrap);

    ins = twistMap->data;
    float64* fIt = freqMap.data;
    float64 freqRange = (maxFreq - minFreq);
    float64 mid = freqRange / 2.0 + minFreq;
    float64 invMid = 1.0 / mid;
    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        uint16 odd = c.y % 2;

        for (c.x = 0; c.x < dim.w; ++c.x, ++ins, ++fIt)
        {
            float64 freq = *fIt * freqRange + minFreq;
            float64 coordScale = freq * invMid;
            float64 offset = (1.0 - coordScale) * invMid;
            float64 ampChange = 0.85 - *fIt * 0.5;

            float64 x = c.x + odd * 0.5;
            *ins = GetPerlinNoise(x + offset, (c.y + offset) * YtoXRatio, w, h, mid, 1.0, ampChange, 8, &inputNoise);
        }
    }
    Normalize(twistMap);
    DrawHexes(twistMap->data, sizeof *twistMap->data, PaintUnitFloatGradient);
    SaveMap("02_twistNoise.bmp");

    ExitFloatMap(&freqMap);
    ExitFloatMap(&inputNoise);
}

void GenerateMountainMap(Dim dim, bool xWrap, bool yWrap, float64 initFreq,
    FloatMap* out)
{
    FloatMap inputNoise;
    InitFloatMap(&inputNoise, dim, xWrap, yWrap);
    GenerateBinaryNoise(&inputNoise);
    Normalize(&inputNoise);
    DrawHexes(inputNoise.data, sizeof *inputNoise.data, PaintUnitFloatGradient);
    SaveMap("03_inputNoise.bmp");

    FloatMap inputNoise2;
    InitFloatMap(&inputNoise2, dim, xWrap, yWrap);
    GenerateBinaryNoise(&inputNoise2);
    Normalize(&inputNoise2);
    DrawHexes(inputNoise2.data, sizeof *inputNoise2.data, PaintUnitFloatGradient);
    SaveMap("04_input2Noise.bmp");

    FloatMap* mountainMap = out;
    InitFloatMap(mountainMap, dim, xWrap, yWrap);
    FloatMap stdDevMap;
    InitFloatMap(&stdDevMap, dim, xWrap, yWrap);
    FloatMap noiseMap;
    InitFloatMap(&noiseMap, dim, xWrap, yWrap);

    float64* mtnIns = mountainMap->data;
    Coord c;
    uint16 w = dim.w;
    float64 h = dim.h * YtoXRatio;
    // init mountain map
    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        uint16 odd = c.y % 2;
        for (c.x = 0; c.x < dim.w; ++c.x, ++mtnIns)
        {
            float64 x = c.x + odd * 0.5;
            *mtnIns = GetPerlinNoise(x, c.y * YtoXRatio, w, h, initFreq, 1.0, 0.4, 8, &inputNoise);
        }
    }
    // mirror data
    memcpy(stdDevMap.data, mountainMap->data, dim.w * dim.h * sizeof float64);
    // init noise map
    float64* noiIns = noiseMap.data;
    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        uint16 odd = c.y % 2;
        for (c.x = 0; c.x < dim.w; ++c.x, ++noiIns)
        {
            float64 x = c.x + odd * 0.5;
            *noiIns = GetPerlinNoise(x, c.y * YtoXRatio, w, h, initFreq, 1.0, 0.4, 8, &inputNoise2);
        }
    }

    Normalize(mountainMap);
    Deviate(&stdDevMap, 7);
    Normalize(&stdDevMap);
    Normalize(&noiseMap);
    DrawHexes(mountainMap->data, sizeof *mountainMap->data, PaintUnitFloatGradient);
    SaveMap("05_mtnNoise.bmp");
    DrawHexes(stdDevMap.data, sizeof *stdDevMap.data, PaintUnitFloatGradient);
    SaveMap("06_stdevNoise.bmp");
    DrawHexes(noiseMap.data, sizeof *noiseMap.data, PaintUnitFloatGradient);
    SaveMap("07_noiseNoise.bmp");

    FloatMap moundMap;
    InitFloatMap(&moundMap, dim, xWrap, yWrap);
    float64* mtnIt = mountainMap->data;
    float64* mndIns = moundMap.data;
    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        for (c.x = 0; c.x < dim.w; ++c.x, ++mtnIt, ++mndIns)
        {
            float64 val = *mtnIt;
            *mndIns = (sin(val * M_PI * 2 - M_PI_2) * 0.5 + 0.5) * GetAttenuationFactor(dim, c);
            //if (val < 0.5)
            //    val = val * 4;
            //else
            //    val = (1 - val) * 4;
            //*mtnIt = val;
            *mtnIt = *mndIns;
        }
    }

    Normalize(mountainMap);

    mtnIt = mountainMap->data;
    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        for (c.x = 0; c.x < dim.w; ++c.x, ++mtnIt)
        {
            float64 val = *mtnIt;
            float64 p1 = sin(val * 3 * M_PI + M_PI_2);
            float64 p2 = p1 * p1;
            float64 p4 = p2 * p2;
            float64 p8 = p4 * p4;
            float64 p16 = p8 * p8;
            float64 res = sqrt(p16 * val);

            *mtnIt = res > 0.2 ? 1.0 : 0.0;
        }
    }

    float64 stdDevThreshold = FindThresholdFromPercent(&stdDevMap, 1.0 - gSet.landPercent, false);
    float64 dblThres = 2.0 * stdDevThreshold;

    mtnIt = mountainMap->data;
    float64* mndIt = moundMap.data;
    float64* sdvIt = stdDevMap.data;
    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        for (c.x = 0; c.x < dim.w; ++c.x, ++mtnIt, ++mndIt, ++sdvIt)
        {
            float64 dev = 2.0 * *sdvIt - dblThres;
            *mtnIt = (*mtnIt + *mndIt) * dev;
        }
    }

    Normalize(mountainMap);
    DrawHexes(mountainMap->data, sizeof *mountainMap->data, PaintUnitFloatGradient);
    SaveMap("08_mtn2Noise.bmp");
}

float64 GetAttenuationFactor(Dim dim, Coord c)
{
    float64 yAttenuation = 1.0;
    float64 xAttenuation = 1.0;

    float64 southRange = dim.h * gSet.southAttenuationRange;
    float64 southY = southRange;
    if (c.y < southY)
        yAttenuation = gSet.southAttenuationFactor + (c.y / southRange) *
            (1.0 - gSet.southAttenuationFactor);

    float64 northRange = dim.h * gSet.northAttenuationRange;
    float64 northY = dim.h - northRange;
    if (c.y > northY)
        yAttenuation = gSet.northAttenuationFactor + ((dim.h - c.y) / northRange) *
            (1.0 - gSet.northAttenuationFactor);

    float64 eastRange = dim.w * gSet.eastAttenuationRange;
    float64 eastX = dim.w - eastRange;
    if (c.x > eastX)
        xAttenuation = gSet.eastAttenuationFactor + ((dim.w - c.x) / eastRange) *
            (1.0 - gSet.eastAttenuationFactor);

    float64 westRange = dim.w * gSet.westAttenuationRange;
    float64 westX = westRange;
    if (c.x < westX)
        xAttenuation = gSet.westAttenuationFactor + (c.x / westRange) *
            (1.0 - gSet.westAttenuationFactor);

    return yAttenuation * xAttenuation;
}

void GenerateElevationMap(Dim dim, bool xWrap, bool yWrap, ElevationMap * out)
{
    float64 scale = 128.0 / dim.w;
    float64 twistMinFreq = scale * gSet.twistMinFreq; // 0.02 /128
    float64 twistMaxFreq = scale * gSet.twistMaxFreq; // 0.12 /128
    float64 twistVar = scale * gSet.twistVar;         // 0.042/128
    float64 mountainFreq = scale * gSet.mountainFreq; // 0.05 /128

    FloatMap twistMap;
    GenerateTwistedPerlinMap(dim, xWrap, yWrap, twistMinFreq, twistMaxFreq, twistVar, &twistMap);
    FloatMap mountainMap;
    GenerateMountainMap(dim, xWrap, yWrap, mountainFreq, &mountainMap);
    ElevationMap* elevationMap = out;
    InitElevationMap(elevationMap, dim, xWrap, yWrap);

    float64* it = twistMap.data;
    float64* end = it + twistMap.length;
    float64* mIt = mountainMap.data;
    float64* eIt = elevationMap->base.data;

    for (; it < end; ++it, ++mIt, ++eIt)
    {
        float64 tVal = *it;
        //this formula adds a curve flattening the extremes
        tVal = sin(tVal * M_PI - M_PI_2) * 0.5 + 0.5;
        tVal = sqrt(sqrt(tVal));
        *eIt = tVal + ((*mIt * 2) - 1) * gSet.mountainWeight;
    }

    Normalize(&elevationMap->base);

    // attentuation should not break normalization
    eIt = elevationMap->base.data;
    Coord c;
    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++eIt)
            *eIt *= GetAttenuationFactor(dim, c);

    elevationMap->seaThreshold = FindThresholdFromPercent(&elevationMap->base, 1.0 - gSet.landPercent, false);

    DrawHexes(elevationMap->base.data, sizeof *elevationMap->base.data, PaintUnitFloatGradient);
    SaveMap("09_emapNoise.bmp");
}

// TODO: no lambdas
ElevationMap* lambdaMap;

void FillInLakes(ElevationMap* map)
{
    lambdaMap = map;
    PWAreaMap areaMap;
    InitPWAreaMap(&areaMap, map->base.dim, map->base.wrapX, map->base.wrapY);
    DefineAreas(&areaMap, [](uint32 i) {return IsBelowSeaLevel(lambdaMap, i); }, false);

    PWArea* it = areaMap.areaList;
    PWArea* end = it + map->base.length;

    for (; it < end; ++it)
        if (it->trueMatch && it->size < gSet.minOceanSize)
            for (uint32 i = 0; i < areaMap.base.length; ++i)
                if (areaMap.base.data[i] == it->ind)
                    map->base.data[i] = map->seaThreshold;
}

void GenerateTempMaps(ElevationMap* map, FloatMap* outSummer, FloatMap* outWinter, FloatMap* outTemp)
{
    Dim dim = map->base.dim;
    uint32 reducedWidth = (uint32)floor(dim.w / 8.0);
    Coord c;

    FloatMap aboveSeaLevelMap;
    InitFloatMap(&aboveSeaLevelMap, dim, map->base.wrapX, map->base.wrapY);
    float64* it = aboveSeaLevelMap.data;
    float64* eIt = map->base.data;
    //float64* end = it + aboveSeaLevelMap.length;

    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++it, ++eIt)
        {
            if (IsBelowSeaLevel(map, c))
                *it = 0.0;
            else
                *it = *eIt - map->seaThreshold;
        }

    Normalize(&aboveSeaLevelMap);

    DrawHexes(aboveSeaLevelMap.data, sizeof *aboveSeaLevelMap.data, PaintUnitFloatGradient);
    SaveMap("11_LandMap.bmp");

    FloatMap* summerMap = outSummer;
    InitFloatMap(summerMap, dim, map->base.wrapX, map->base.wrapY);
    float64 zenith = gSet.tropicLatitudes;
    float64 topTempLat = gSet.topLatitude + zenith;
    float64 bottomTempLat = gSet.bottomLatitude;
    float64 latRange = topTempLat - bottomTempLat;
    it = summerMap->data;

    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        float64 lat = GetLatitudeForY(summerMap, c.y);
        float64 latPercent = (lat - bottomTempLat) / latRange;
        float64 temp = sin(latPercent * M_PI * 2 - M_PI_2) * 0.5 + 0.5;
        float64 tempAlt = temp * gSet.maxWaterTemp + gSet.minWaterTemp;

        for (c.x = 0; c.x < dim.w; ++c.x, ++it)
            *it = IsBelowSeaLevel(map, c) ? tempAlt : temp;
    }

    Smooth(summerMap, reducedWidth);
    Normalize(summerMap);

    DrawHexes(summerMap->data, sizeof *summerMap->data, PaintUnitFloatGradient);
    SaveMap("12_SummerTempMap.bmp");

    FloatMap* winterMap = outWinter;
    InitFloatMap(winterMap, dim, map->base.wrapX, map->base.wrapY);
    zenith = -gSet.tropicLatitudes;
    topTempLat = gSet.topLatitude;
    bottomTempLat = gSet.bottomLatitude + zenith;
    latRange = topTempLat - bottomTempLat;
    it = winterMap->data;

    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        float64 lat = GetLatitudeForY(winterMap, c.y);
        float64 latPercent = (lat - bottomTempLat) / latRange;
        float64 temp = sin(latPercent * M_PI * 2 - M_PI_2) * 0.5 + 0.5;
        float64 tempAlt = temp * gSet.maxWaterTemp + gSet.minWaterTemp;

        for (c.x = 0; c.x < dim.w; ++c.x, ++it)
            *it = IsBelowSeaLevel(map, c) ? tempAlt : temp;
    }

    Smooth(winterMap, reducedWidth);
    Normalize(winterMap);

    DrawHexes(winterMap->data, sizeof *winterMap->data, PaintUnitFloatGradient);
    SaveMap("13_WinterTempMap.bmp");

    FloatMap* temperatureMap = outTemp;
    InitFloatMap(temperatureMap, dim, map->base.wrapX, map->base.wrapY);
    it = temperatureMap->data;
    float64* end = it + map->base.length;
    float64* sIt = summerMap->data;
    float64* wIt = winterMap->data;
    float64* aIt = aboveSeaLevelMap.data;

    for (; it < end; ++it, ++sIt, ++wIt, ++aIt)
        *it = (*wIt + *sIt) * (1.0 - *aIt);

    Normalize(temperatureMap);
    ExitFloatMap(&aboveSeaLevelMap);

    DrawHexes(temperatureMap->data, sizeof *temperatureMap->data, PaintUnitFloatGradient);
    SaveMap("14_TempMap.bmp");
}

void GenerateRainfallMap(ElevationMap* map, FloatMap* outRain, FloatMap* outTemp)
{
    Dim dim = map->base.dim;
    Coord c;

    FloatMap summerMap, winterMap;
    FloatMap* temperatureMap = outTemp;
    GenerateTempMaps(map, &summerMap, &winterMap, temperatureMap);

    FloatMap geoMap;
    InitFloatMap(&geoMap, dim, map->base.wrapX, map->base.wrapY);
    float64* it = geoMap.data;

    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        float64 lat = GetLatitudeForY(&map->base, c.y);
        float64 pressure = GetGeostrophicPressure(&map->base, lat);

        for (c.x = 0; c.x < dim.w; ++c.x, ++it)
            *it = pressure;
    }

    Normalize(&geoMap);

    DrawHexes(geoMap.data, sizeof *geoMap.data, PaintUnitFloatGradient);
    SaveMap("15_GeoMap.bmp");

    // Create sorted summer map
    RefMap* sortedSummerMap = (RefMap*)malloc(map->base.length * sizeof(RefMap));
    RefMap* sIns = sortedSummerMap;
    float64* sIt = geoMap.data;

    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++sIns, ++sIt)
            *sIns = { c, *sIt };

    std::sort(sortedSummerMap, sIns, [](RefMap& a, RefMap& b) { return a.val < b.val; });

    // Create sorted winter map
    RefMap* sortedWinterMap = (RefMap*)malloc(map->base.length * sizeof(RefMap));
    RefMap* wIns = sortedWinterMap;
    float64* wIt = geoMap.data;

    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++wIns, ++wIt)
            *wIns = { c, *wIt };

    std::sort(sortedWinterMap, wIns, [](RefMap& a, RefMap& b) { return a.val < b.val; });

    RefMap* sortedGeoMap = (RefMap*)malloc(map->base.length * sizeof(RefMap));
    RefMap* geoIns = sortedGeoMap;

    for (uint32 w = wNPolar; w <= wSPolar; ++w)
    {
        uint16 topY = GetYFromZone(&map->base, (WindZone)w, true);
        uint16 bottomY = GetYFromZone(&map->base, (WindZone)w, false);

        if (topY != UINT16_MAX || bottomY != UINT16_MAX)
        {
            if (topY == UINT16_MAX)
                topY = dim.h - 1;
            if (bottomY == UINT16_MAX)
                bottomY = 0;
            std::pair<Dir, Dir> dir = GetGeostrophicWindDirections((WindZone)w);

            int32 xStart = 0, xStop = 0, yStart = 0, yStop = 0;
            int32 incX = 0, incY = 0;

            if (dir.first == dSW || dir.first == dSE)
            {
                yStart = topY;
                yStop = bottomY;
                incY = -1;
            }
            else
            {
                yStart = bottomY;
                yStop = topY;
                incY = 1;
            }
            
            if (dir.second == dW)
            {
                xStart = dim.w - 1;
                xStop = 0;
                incX = -1;
            }
            else
            {
                xStart = 0;
                xStop = dim.w;
                incX = 1;
            }

            // TODO: resolve terrible condition
            for (int32 y = yStart; incY > 0 ? y <= yStop : y >= yStop; y += incY)
            {
                assert(y >= 0 && y < dim.h);
                int32 x = xStart;
                int32 xEnd = xStop;

                // each line should start on water to avoid vast areas without rain
                for (; incX > 0 ? x <= xStop : x >= xStop; x += incX)
                {
                    assert(x >= 0 && x < dim.w);
                    if (IsBelowSeaLevel(map, { (uint16)x, (uint16)y }))
                    {
                        xEnd = x + dim.w * incX;
                        break;
                    }
                }

                for (; incX > 0 ? x < xEnd : x > xEnd; x += incX, ++geoIns)
                {
                    int32 rx = x;
                    if (rx < 0)
                        rx += dim.w;
                    if (rx >= dim.w)
                        rx -= dim.w;
                    assert(rx >= 0 && rx < dim.w);
                    Coord crd = { (uint16)rx, (uint16)y };
                    uint32 i = GetIndex(&map->base, crd);
                    *geoIns = { crd, geoMap.data[i] };
                }
            }
        }
    }
    uint32 dbgGeoCnt = geoIns - sortedGeoMap;
    assert(dbgGeoCnt <= map->base.length);

    FloatMap rainfallSummerMap;
    InitFloatMap(&rainfallSummerMap, dim, map->base.wrapX, map->base.wrapY);
    FloatMap moistureMap;
    InitFloatMap(&moistureMap, dim, map->base.wrapX, map->base.wrapY);
    RefMap* sumIt = sortedSummerMap;
    RefMap* sumEnd = sIns;

    for (; sumIt < sumEnd; ++sumIt)
        DistributeRain(sumIt->c, map, temperatureMap, &summerMap, &rainfallSummerMap, &moistureMap, false);

    FloatMap rainfallWinterMap;
    InitFloatMap(&rainfallWinterMap, dim, map->base.wrapX, map->base.wrapY);
    FloatMap moistureMap2;
    InitFloatMap(&moistureMap2, dim, map->base.wrapX, map->base.wrapY);
    RefMap* winIt = sortedWinterMap;
    RefMap* winEnd = wIns;

    for (; winIt < winEnd; ++winIt)
        DistributeRain(winIt->c, map, temperatureMap, &winterMap, &rainfallWinterMap, &moistureMap2, false);

    FloatMap rainfallGeostrophicMap;
    InitFloatMap(&rainfallGeostrophicMap, dim, map->base.wrapX, map->base.wrapY);
    FloatMap moistureMap3;
    InitFloatMap(&moistureMap3, dim, map->base.wrapX, map->base.wrapY);
    RefMap* geoIt = sortedGeoMap;
    RefMap* geoEnd = geoIns;

    for (; geoIt < geoEnd; ++geoIt)
        DistributeRain(geoIt->c, map, temperatureMap, &geoMap, &rainfallGeostrophicMap, &moistureMap3, true);

    float64* rsIt = rainfallSummerMap.data;
    float64* rwIt = rainfallWinterMap.data;
    float64* rgIt = rainfallGeostrophicMap.data;

    // zero below sea level for proper percent threshold finding
    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++rsIt, ++rwIt, ++rgIt)
            if (IsBelowSeaLevel(map, c))
            {
                *rsIt = 0.0;
                *rwIt = 0.0;
                *rgIt = 0.0;
            }

    Normalize(&rainfallSummerMap);
    Normalize(&rainfallWinterMap);
    Normalize(&rainfallGeostrophicMap);

    DrawHexes(rainfallSummerMap.data, sizeof *rainfallSummerMap.data, PaintUnitFloatGradient);
    SaveMap("16_SummerRainMap.bmp");
    DrawHexes(rainfallWinterMap.data, sizeof *rainfallWinterMap.data, PaintUnitFloatGradient);
    SaveMap("17_WinterRainMap.bmp");
    DrawHexes(rainfallGeostrophicMap.data, sizeof *rainfallGeostrophicMap.data, PaintUnitFloatGradient);
    SaveMap("18_GeoRainMap.bmp");

    FloatMap* rainfallMap = outRain;
    InitFloatMap(rainfallMap, dim, map->base.wrapX, map->base.wrapY);
    float64* rIns = rainfallMap->data;
    float64* rEnd = rIns + map->base.length;
    rsIt = rainfallSummerMap.data;
    rwIt = rainfallWinterMap.data;
    rgIt = rainfallGeostrophicMap.data;
    for (; rIns < rEnd; ++rIns, ++rsIt, ++rwIt, ++rgIt)
        *rIns = *rsIt + *rwIt + (*rgIt * gSet.geostrophicFactor);

    Normalize(rainfallMap);

    DrawHexes(rainfallMap->data, sizeof *rainfallMap->data, PaintUnitFloatGradient);
    SaveMap("19_RainMap.bmp");

    ExitFloatMap(&moistureMap3);
    ExitFloatMap(&rainfallGeostrophicMap);
    ExitFloatMap(&moistureMap2);
    ExitFloatMap(&rainfallWinterMap);
    ExitFloatMap(&moistureMap);
    ExitFloatMap(&rainfallSummerMap);
    free(sortedGeoMap);
    free(sortedWinterMap);
    free(sortedSummerMap);
    ExitFloatMap(&geoMap);
    ExitFloatMap(&winterMap);
    ExitFloatMap(&summerMap);
}

void DistributeRain(Coord c, ElevationMap * map, FloatMap * temperatureMap, 
    FloatMap * pressureMap, FloatMap* rainfallMap, FloatMap* moistureMap, bool isGeostrophic)
{
    uint32 i = GetIndex(&map->base, c);
    float64 temp = temperatureMap->data[i];
    float64 pressure = pressureMap->data[i];

    float64 upLiftSource = std::max(std::pow(pressure, gSet.upLiftExponent), 1.0 - temp);

    if (IsBelowSeaLevel(map, c))
        moistureMap->data[i] = std::max(moistureMap->data[i], temp);

    uint32 nList[6];
    uint32 ins = 0;
    WindZone zone = GetZone(&map->base, c.y);

    if (isGeostrophic)
    {
        std::pair<Dir, Dir> dir = GetGeostrophicWindDirections(zone);
        Coord n;
        GetNeighbor(&map->base, c, dir.first, &n);
        uint32 ii = GetIndex(&map->base, n);

        if (ii < map->base.length && zone == GetZone(&map->base, n.y))
        {
            nList[ins] = ii;
            ++ins;
        }

        GetNeighbor(&map->base, c, dir.second, &n);
        ii = GetIndex(&map->base, n);

        if (ii < map->base.length && zone == GetZone(&map->base, n.y))
        {
            nList[ins] = ii;
            ++ins;
        }
    }
    else
    {
        for (uint32 dir = dW; dir <= dSW; ++dir)
        {
            Coord n;
            GetNeighbor(&map->base, c, (Dir)dir, &n);
            uint32 ii = GetIndex(&map->base, n);

            if (ii < map->base.length && pressure <= pressureMap->data[ii])
            {
                nList[ins] = ii;
                ++ins;
            }
        }
    }

    if (ins == 0 || (isGeostrophic && ins == 1))
    {
        rainfallMap->data[i] = moistureMap->data[i];
        return;
    }

    float64 moisturePerNeighbor = moistureMap->data[i] / ins;

    // drop rain and pass moisture to neighbors
    uint32* it = nList;
    uint32* end = nList + ins;
    float64 bonus = 0.0;
    if (zone == wNPolar || zone == wSPolar)
        bonus = gSet.polarRainBoost;

    for (; it < end; ++it)
    {
        float64 upLiftDest = std::max(std::pow(pressureMap->data[*it], gSet.upLiftExponent), 1.0 - temperatureMap->data[*it]);
        float64 cost = GetRainCost(upLiftSource, upLiftDest);

        if (isGeostrophic)
        {
            if (it == nList)
                moisturePerNeighbor = (1.0 - gSet.geostrophicLateralWindStrength) * moistureMap->data[i];
            else
                moisturePerNeighbor = gSet.geostrophicLateralWindStrength * moistureMap->data[i];
        }

        rainfallMap->data[i] += cost * moisturePerNeighbor + bonus;

        // pass to neighbor
        moistureMap->data[*it] += moisturePerNeighbor - (cost * moisturePerNeighbor);
    }
}

float64 GetRainCost(float64 upLiftSource, float64 upLiftDest)
{
    float64 cost = gSet.minimumRainCost;
    if (upLiftDest > upLiftSource)
        cost += upLiftDest - upLiftSource;
    return cost < 0.0 ? 0.0 : cost;
}

float64 GetDifferenceAroundHex(ElevationMap* map, Coord c)
{
    float64 avg = GetAverageInHex(&map->base, c, 1);
    uint32 i = GetIndex(&map->base, c);
    return map->base.data[i] - avg;
}

void PlacePossibleOasis(FloatMap * map, MapTile* plot, Coord c)
{
    if (plot->terrain == tDESERT)
    {
        // too many oasis clustered together looks bad
        // reject if within 3 tiles of another oasis
        Coord* coords;
        uint32 len = GetRadiusAroundHex(map, c, 3, &coords);
        Coord* it = coords;
        Coord* end = it + len;
        char const* name = "Hellow "   "World.";

        for (; it < end; ++it)
        {
            uint32 ind = (map->dim.w * it->y) + it->x;
            if (gMap[ind].feature == fOASIS)
            {
                free(coords);
                return;
            }
        }

        free(coords);

        len = GetRadiusAroundHex(map, c, 1, &coords);
        it = coords;
        end = it + len;

        for (; it < end; ++it)
        {
            uint32 ind = (map->dim.w * it->y) + it->x;
            MapTile* nPlot = gMap + ind;
            if (nPlot->feature != fNONE ||
                nPlot->terrain < tDESERT ||
                // TODO: make mountain/hill testing smoother
                (nPlot->terrain - tDESERT) % 5 != 0)
            {
                free(coords);
                return;
            }
        }

        free(coords);
        plot->feature = fOASIS;
    }
}

void PlacePossibleIce(FloatMap* map, float64 temp, MapTile* plot, Coord c)
{
    if (IsWater(plot))
    {
        float64 latitude = GetLatitudeForY(map, c.y);
        float64 randvalNorth = PWRand() * (gSet.iceNorthLatitudeLimit - gSet.topLatitude) + gSet.topLatitude - 2;
        float64 randvalSouth = PWRand() * (gSet.bottomLatitude - gSet.iceSouthLatitudeLimit) + gSet.iceSouthLatitudeLimit;

        if (latitude > randvalNorth || latitude < randvalSouth)
            plot->feature = fICE;
    }
}

void PlacePossibleReef(MapTile* plot)
{
    if (IsWater(plot) &&
        plot->feature == fNONE &&
        plot->resource == rNONE &&
        true // TODO: TerrainBuilder.CanHaveFeature(..., fREEF)
        )
    {
        if (PWRand() < gSet.maxReefChance)
            plot->feature = fREEF;
    }
}

void AddTerrainFromContinents(Dim dim, uint8* terrainTypes)
{
    // TODO:
}

void ApplyVolcanoBump(ElevationMap* map, uint32 i, Coord c)
{
    // this function adds a bump to volcano plots to guide the river system.
    map->base.data[i] *= 1.5;

    Coord* list;
    uint32 len = GetRadiusAroundHex(&map->base, c, 1, &list);
    Coord* it = list;
    Coord* end = it + len;

    for (; it < end; ++it)
    {
        uint32 ii = GetIndex(&map->base, *it);
        map->base.data[ii] *= 1.25;
    }
    free(list);
}

void ApplyTerrain(uint32 len, uint8* plotTypes, uint8* terrainTypes)
{
    MapTile* mIt = gMap;
    MapTile* mEnd = mIt + len;
    uint8_t* pIt = plotTypes;
    uint8_t* tIt = terrainTypes;

    for (; mIt < mEnd; ++mIt, ++pIt, ++tIt)
    {
        uint8 terrain = *tIt;
        if (*pIt == ptHills)
            terrain += tDESERT_HILLS - tDESERT;
        else if (*pIt == ptMountain)
            terrain += tDESERT_MOUNTAIN - tDESERT;

        mIt->terrain = terrain;
    }
}

uint32 GeneratePlotTypes(Dim dim, ElevationMap* outElev, FloatMap* outRain, FloatMap* outTemp, uint8 ** outPlot)
{
    PWRandSeed(gSet.fixedSeed);

    ElevationMap* eMap = outElev;
    GenerateElevationMap(dim, gSet.wrapX, gSet.wrapY, eMap);
    FillInLakes(eMap);

    DrawHexes(eMap->base.data, sizeof *eMap->base.data, PaintUnitFloatGradient);
    SaveMap("10_FilledLakesNoise.bmp");

    FloatMap* rainfallMap = outRain;
    FloatMap* temperatureMap = outTemp;
    GenerateRainfallMap(eMap, rainfallMap, temperatureMap);

    FloatMap diffMap;
    InitFloatMap(&diffMap, dim, true, false);
    Coord c;
    float64* ins = diffMap.data;

    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++ins)
            if (IsBelowSeaLevel(eMap, c))
                *ins = 0.0;
            else
                *ins = GetDifferenceAroundHex(eMap, c);

    Normalize(&diffMap);

    DrawHexes(diffMap.data, sizeof *diffMap.data, PaintUnitFloatGradient);
    SaveMap("20_DiffMap.bmp");

    ins = diffMap.data;
    float64* eIt = eMap->base.data;

    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++ins, ++eIt)
            if (!IsBelowSeaLevel(eMap, c))
                *ins += *eIt * 1.1;

    Normalize(&diffMap);

    DrawHexes(diffMap.data, sizeof *diffMap.data, PaintUnitFloatGradient);
    SaveMap("21_DiffMapBoost.bmp");

    gThrs.hills = FindThresholdFromPercent(&diffMap, gSet.hillsPercent, true);
    gThrs.mountains = FindThresholdFromPercent(&diffMap, gSet.mountainsPercent, true);

    uint32_t len = dim.w * dim.h;
    // Note: allocating outside of function to reduce reallocs
    uint8* plotTypes = *outPlot;//(uint8*)malloc(sizeof * plotTypes * len);
    uint8* pIns = plotTypes;
    float64* dIt = diffMap.data;

    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++pIns, ++dIt)
            if (IsBelowSeaLevel(eMap, c))
                *pIns = ptOcean;
            else if (*dIt < gThrs.hills)
                *pIns = ptLand;
            else if (*dIt < gThrs.mountains)
                *pIns = ptHills;
            else
                *pIns = ptMountain;

    ExitFloatMap(&diffMap);
    // TODO: see if above maps are still needed
    //*outPlot = plotTypes;
    return len;
}

uint32 GenerateTerrain(ElevationMap* map, FloatMap* rainMap, FloatMap* tempMap, uint8** out)
{
    Dim dim = map->base.dim;
    float64 minRain = 100.0;
    float64* eIt = map->base.data;
    float64* eEnd = eIt + map->base.length;
    float64* rIt = rainMap->data;

    // first find minimum rain above sea level for a soft desert transition
    for (; eIt < eEnd; ++eIt, ++rIt)
        if (*eIt >= map->seaThreshold &&
            *rIt < minRain)
            minRain = *rIt;

    // find exact thresholds, making these global for subsequent use
    gThrs.desert = FindThresholdFromPercent(rainMap, gSet.desertPercent, true);
    gThrs.plains = FindThresholdFromPercent(rainMap, gSet.plainsPercent, true);

    eIt = map->base.data;
    rIt = rainMap->data;
    float64* tIt = tempMap->data;

    uint32_t len = dim.w * dim.h;
    uint8* terrainTypes = *out;
    uint8* ins = terrainTypes;
    float64 transition = gThrs.plains - gThrs.desert;

    for (; eIt < eEnd; ++eIt, ++rIt, ++tIt, ++ins)
        if (*eIt >= map->seaThreshold)
            if (*tIt < gSet.snowTemperature)
                *ins = tSNOW;
            else if (*tIt < gSet.tundraTemperature)
                *ins = tTUNDRA;
            else if (*rIt < gThrs.desert)
            {
                if (*tIt < gSet.desertMinTemperature)
                    *ins = tPLAINS;
                else
                    *ins = tDESERT;
            }
            else if (*rIt < gThrs.plains)
            {
                if (*rIt < (PWRand() * transition + transition) / 2.0 + gThrs.desert)
                    *ins = tPLAINS;
                else
                    *ins = tGRASS;
            }
            else
                *ins = tGRASS;

    return len;
}

void FinalAlterations(ElevationMap* map, uint8* plotTypes, uint8* terrainTypes)
{
    Dim dim = map->base.dim;
    Coord c;
    float64* eIt = map->base.data;
    uint8* pIt = terrainTypes;
    uint8* tIt = terrainTypes;

    // now we fix things up so that the border of tundra and ice regions are hills
    // this looks a bit more believable. Also keep desert away from tundra and ice
    // by turning it into plains
    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++eIt, ++pIt, ++tIt)
            if (*eIt >= map->seaThreshold)
            {
                if (*tIt == tSNOW)
                {
                    bool lowerFound = false;

                    for (uint32 dir = dW; dir < dNum; ++dir)
                    {
                        Coord n;
                        GetNeighbor(&map->base, c, (Dir)dir, &n);
                        uint32 i = GetIndex(&map->base, n);

                        if (i < map->base.length)
                        {
                            uint8 t = terrainTypes[i];

                            if (!IsBelowSeaLevel(map, i) &&
                                t != tSNOW)
                                lowerFound = true;

                            if (t == tDESERT)
                                *tIt = tPLAINS;
                        }
                    }

                    if (lowerFound && *pIt == ptLand)
                        *pIt = ptHills;
                }
                else if (*tIt == tTUNDRA)
                {
                    bool lowerFound = false;

                    for (uint32 dir = dW; dir < dNum; ++dir)
                    {
                        Coord n;
                        GetNeighbor(&map->base, c, (Dir)dir, &n);
                        uint32 i = GetIndex(&map->base, n);

                        if (i < map->base.length)
                        {
                            uint8 t = terrainTypes[i];

                            if (!IsBelowSeaLevel(map, i) &&
                                t != tSNOW &&
                                t != tTUNDRA)
                                lowerFound = true;

                            if (t == tDESERT)
                                *tIt = tPLAINS;
                        }
                    }

                    if (lowerFound && *pIt == ptLand)
                        *pIt = ptHills;
                }
                else if (*pIt == ptHills)
                {
                    for (uint32 dir = dW; dir < dNum; ++dir)
                    {
                        Coord n;
                        GetNeighbor(&map->base, c, (Dir)dir, &n);
                        uint32 i = GetIndex(&map->base, n);

                        if (i < map->base.length &&
                            terrainTypes[i] == tSNOW ||
                            terrainTypes[i] == tTUNDRA)
                        {
                            *pIt = ptLand;
                            break;
                        }
                    }
                }
            }
}

void GenerateCoasts(ElevationMap* map, uint8* plotTypes, uint8* terrainTypes)
{
    Dim dim = map->base.dim;
    uint32 len = dim.w * dim.h;
    Coord c;
    uint8* pIt = plotTypes;
    uint8* tIt = terrainTypes;
    float64* eIt = map->base.data;
    gThrs.coast = map->seaThreshold * 0.90;

    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++pIt, ++tIt, ++eIt)
            if (*pIt == ptOcean)
            {
                if (IsAdjacentToLand(map, len, plotTypes, c) ||
                    *eIt > gThrs.coast)
                    *tIt = tCOAST;
                else
                    *tIt = tOCEAN;
            }
}


// --- Generation Functions ---------------------------------------------------

void AddFeatures(ElevationMap* map, FloatMap* rainMap, FloatMap* tempMap)
{
    Dim dim = map->base.dim;
    float64 zeroTreesThreshold = FindThresholdFromPercent(rainMap, gSet.zeroTreesPercent, true);
    float64 jungleThreshold = FindThresholdFromPercent(rainMap, gSet.junglePercent, true);
    float64 treeRange = jungleThreshold - zeroTreesThreshold;
    float64 marshRange = 1.0 - jungleThreshold;

    Coord c;
    MapTile* plot = gMap;
    MapTile* end = plot + map->base.length;
    float64* rIt = rainMap->data;
    float64* tIt = tempMap->data;

    for (; plot < end; ++plot, ++rIt, ++tIt)
        // avoid overwriting existing features
        if (!IsWater(plot) && plot->feature == fNONE)
        {
            if (*rIt < jungleThreshold)
            {
                if (plot->terrain < tMountainsStart &&
                    *tIt > gSet.treesMinTemperature &&
                    *rIt > PWRand() * treeRange + zeroTreesThreshold)
                    plot->feature = fFOREST;
            }
            else
            {
                if (*tIt > gSet.treesMinTemperature &&
                    *rIt > PWRand() * marshRange + jungleThreshold)
                {
                    plot->terrain = tGRASS;
                    plot->feature = fMARSH;
                }
                else if (plot->terrain < tMountainsStart)
                {
                    if (*tIt >= gSet.jungleMinTemperature)
                    {
                        plot->feature = fJUNGLE;

                        if (gSet.jungleToPlains >= jcHillsOnly)
                        {
                            if (plot->terrain >= tHillsStart)
                                plot->terrain = tPLAINS_HILLS;
                            else if (gSet.jungleToPlains == jcAll)
                                plot->terrain = tPLAINS;
                        }
                    }
                    else if (*tIt > gSet.treesMinTemperature)
                        plot->feature = fFOREST;
                }

                if (true)//TODO: CanHaveFeature(plot, tF
                    plot->feature = fFLOODPLAINS;
            }
        }

    float64 minTemp = 1000.0;
    float64 maxTemp = -1.0;

    plot = gMap;
    tIt = tempMap->data;

    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++plot, ++tIt)
        {
            if (!IsWater(plot))
                PlacePossibleOasis(&map->base, plot, c);
            else
            {
                if (*tIt > maxTemp)
                    maxTemp = *tIt;

                if (*tIt < minTemp)
                    minTemp = *tIt;

                // TODO: add expansion filters (Gathering Storm)
                PlacePossibleIce(&map->base, *tIt, plot, c);
                PlacePossibleReef(plot);
            }
        }
}

// TODO:
void AddRivers(RiverMap * map)
{
    River* river = map->rivers;
    River* end = river + map->riverCnt;
    RiverHex* riverHex;
    // TODO: set ref value on tile itself
    uint8* checklist = (uint8*)calloc(map->eMap->base.length, sizeof uint8);

    for (; river < end; ++river)
        for (RiverJunction* junc : river->junctions)
            if (riverHex = GetRiverHexForJunction(map, junc))
            {
                uint32_t i = GetIndex(&map->eMap->base, riverHex->coord);

                if (!checklist[i])
                {
                    MapTile* plot = gMap + i;

                    FlowDirRet data = GetFlowDirections(map, riverHex->coord);

                    if (data.id != UINT32_MAX && river->riverID == data.id)
                    {
                        checklist[i] = true;
                        if (data.WOfRiver != tfdNO_FLOW)
                        {
                            plot->isWOfRiver = 1;
                            // TODO: flow dir
                            // my guess is this is only locked in if there is an adjacent tile
                        }
                        if (data.NWOfRiver != tfdNO_FLOW)
                        {
                            plot->isNWOfRiver = 1;
                            // TODO: flow dir
                        }
                        if (data.NEOfRiver != tfdNO_FLOW)
                        {
                            plot->isNEOfRiver = 1;
                            // TODO: flow dir
                        }
                    }
                }
            }

    free(checklist);
}

void ClearFloodPlains(RiverMap* map)
{
    River* river = map->rivers;
    // only do the largest percent of rivers
    River* end = river + (uint32)(map->riverCnt * gSet.percentRiversFloodplains);

    for (; river < end; ++river)
    {
        // flood the bottom half
        auto jIt = river->junctions.begin() + (uint32)(river->junctions.size() * 0.5f);
        auto jEnd = river->junctions.end();

        for (; jIt < jEnd; ++jIt)
        {
            RiverJunction* junc = *jIt;
            MapTile* plot0, * plot1;
            GetRiverSidesForJunction(map, junc, &plot0, &plot1);

            if (plot0)
            {
                if (plot0->terrain < tHillsStart &&
                    plot0->feature == fFOREST ||
                    plot0->feature == fJUNGLE ||
                    plot0->feature == fMARSH ||
                    plot0->feature == fFLOODPLAINS)
                    plot0->feature = fNONE;
                if (plot1->terrain < tHillsStart &&
                    plot1->feature == fFOREST ||
                    plot1->feature == fJUNGLE ||
                    plot1->feature == fMARSH ||
                    plot1->feature == fFLOODPLAINS)
                    plot1->feature = fNONE;
            }
        }
    }
}

void GetRiverSidesForJunction(RiverMap* map, RiverJunction* junc, MapTile** out0, MapTile** out1)
{
    RiverHex* hex0, * hex1;

    switch (junc->flow)
    {
    case fdVert:
        hex0 = GetRiverHexNeighbor(map, junc, true);
        hex1 = GetRiverHexNeighbor(map, junc, false);
        break;
    case fdEast:
        hex0 = map->riverData + GetIndex(&map->eMap->base, junc->coord);
        hex1 = GetRiverHexNeighbor(map, junc, false);
        break;
    case fdWest:
        hex0 = GetRiverHexNeighbor(map, junc, true);
        hex1 = map->riverData + GetIndex(&map->eMap->base, junc->coord);
        break;
    default:
        *out0 = NULL;
        *out1 = NULL;
        return;
    }

    *out0 = gMap + GetIndex(&map->eMap->base, hex0->coord);
    *out1 = gMap + GetIndex(&map->eMap->base, hex1->coord);
}

void AddLakes(RiverMap* map)
{
    RiverHex* it = map->riverData;
    RiverHex* end = it + map->eMap->base.length;
    MapTile* ins = gMap;

    for (; it < end; ++it, ++ins)
        if (it->lakeID != UINT32_MAX)
            ins->terrain = tCOAST;
}

std::vector<uint32> GetRadiusAroundCell(Dim dim, Coord c, uint32 rad)
{
    std::vector<uint32> cellList;

    for (uint32 r = 1; r <= rad; ++r)
    {
        // move here 1 West
        GetNeighbor(NULL, c, dW, &c);
        if (c.x < dim.w && c.y < dim.h)
            cellList.push_back((uint32)c.y * dim.w + c.x);

        for (uint32 i = 0; i < r; ++i)
        {
            GetNeighbor(NULL, c, dNE, &c);
            if (c.x < dim.w && c.y < dim.h)
                cellList.push_back((uint32)c.y * dim.w + c.x);
        }

        for (uint32 i = 0; i < r; ++i)
        {
            GetNeighbor(NULL, c, dE, &c);
            if (c.x < dim.w && c.y < dim.h)
                cellList.push_back((uint32)c.y * dim.w + c.x);
        }

        for (uint32 i = 0; i < r; ++i)
        {
            GetNeighbor(NULL, c, dSE, &c);
            if (c.x < dim.w && c.y < dim.h)
                cellList.push_back((uint32)c.y * dim.w + c.x);
        }

        for (uint32 i = 0; i < r; ++i)
        {
            GetNeighbor(NULL, c, dSW, &c);
            if (c.x < dim.w && c.y < dim.h)
                cellList.push_back((uint32)c.y * dim.w + c.x);
        }

        for (uint32 i = 0; i < r; ++i)
        {
            GetNeighbor(NULL, c, dW, &c);
            if (c.x < dim.w && c.y < dim.h)
                cellList.push_back((uint32)c.y * dim.w + c.x);
        }

        for (uint32 i = 0; i < r - 1; ++i)
        {
            GetNeighbor(NULL, c, dNW, &c);
            if (c.x < dim.w && c.y < dim.h)
                cellList.push_back((uint32)c.y * dim.w + c.x);
        }

        GetNeighbor(NULL, c, dNW, &c);
    }

    return cellList;
}

std::vector<uint32> GetRingAroundCell(Dim dim, Coord c, uint32 rad)
{
    std::vector<uint32> cellList;

    for (uint32 i = 0; i < rad; ++i)
        GetNeighbor(NULL, c, dW, &c);

    // TODO: remove, temporarily recreating bug in original program
    if (c.x < dim.w && c.y < dim.h)
        cellList.push_back((uint32)c.y * dim.w + c.x);

    for (uint32 i = 0; i < rad; ++i)
    {
        GetNeighbor(NULL, c, dNE, &c);
        if (c.x < dim.w && c.y < dim.h)
            cellList.push_back((uint32)c.y * dim.w + c.x);
    }

    for (uint32 i = 0; i < rad; ++i)
    {
        GetNeighbor(NULL, c, dE, &c);
        if (c.x < dim.w && c.y < dim.h)
            cellList.push_back((uint32)c.y * dim.w + c.x);
    }

    for (uint32 i = 0; i < rad; ++i)
    {
        GetNeighbor(NULL, c, dSE, &c);
        if (c.x < dim.w && c.y < dim.h)
            cellList.push_back((uint32)c.y * dim.w + c.x);
    }

    for (uint32 i = 0; i < rad; ++i)
    {
        GetNeighbor(NULL, c, dSW, &c);
        if (c.x < dim.w && c.y < dim.h)
            cellList.push_back((uint32)c.y * dim.w + c.x);
    }

    for (uint32 i = 0; i < rad; ++i)
    {
        GetNeighbor(NULL, c, dW, &c);
        if (c.x < dim.w && c.y < dim.h)
            cellList.push_back((uint32)c.y * dim.w + c.x);
    }

    for (uint32 i = 0; i < rad; ++i)
    {
        GetNeighbor(NULL, c, dNW, &c);
        if (c.x < dim.w && c.y < dim.h)
            cellList.push_back((uint32)c.y * dim.w + c.x);
    }

    return cellList;
}


// --- Generation Functions ---------------------------------------------------

void InitPangaeaBreaker(PangaeaBreaker * pb, ElevationMap* map, uint8* terrainTypes) 
{
    pb->map = map;
    InitPWAreaMap(&pb->areaMap, map->base.dim, map->base.wrapX, map->base.wrapY);
    pb->terrainTypes = terrainTypes;
    pb->oldWorldPercent = 1.0;

    pb->distanceMap = (uint32*)calloc(map->base.length, sizeof(uint32));

    pb->newWorld = (bool*)calloc(map->base.length, sizeof(bool));
    pb->newWorldMap = (bool*)calloc(map->base.length, sizeof(bool));
}

void ExitPangaeaBreaker(PangaeaBreaker* pb)
{
    free(pb->newWorldMap);
    free(pb->newWorld);
    free(pb->distanceMap);
    ExitPWAreaMap(&pb->areaMap);
}

// TODO: rem
uint8* ttMatch;

bool BreakPangaeas(PangaeaBreaker* pb, uint8* plotTypes, uint8* terrainTypes)
{
    bool meteorThrown = false;
    bool pangeaDetected = false;

    ttMatch = pb->terrainTypes;
    DefineAreas(&pb->areaMap, [](uint32 i) { return ttMatch[i] == tOCEAN; }, false);
    DrawHexes(pb->areaMap.base.data, sizeof *pb->areaMap.base.data, PaintIDS);
    SaveMap("areas00.bmp");

    uint32 meteorCount = 0;

    if (!gSet.allowPangeas)
        while (IsPangea(pb) && meteorCount < maximumMeteorCount)
        {
            pangeaDetected = true;
            Coord c = GetMeteorStrike(pb);
            CastMeteorUponTheEarth(pb, c, plotTypes, terrainTypes);

            meteorThrown = true;
            ++meteorCount;

            DefineAreas(&pb->areaMap, [](uint32 i) { return ttMatch[i] == tOCEAN; }, false);
            DrawHexes(pb->areaMap.base.data, sizeof * pb->areaMap.base.data, PaintIDS);
            char name[] = "areas00.bmp";
            name[6] = '0' + (meteorCount % 10);
            name[5] = '0' + (meteorCount / 10);
            SaveMap(name);
        }

    if (meteorCount == maximumMeteorCount)
        return false;

    if (gSet.allowPangeas)
        pb->oldWorldPercent = 1.0;

    return true;
}

bool IsPangea(PangaeaBreaker* pb)
{
    std::vector<PWArea*> continentList;

    PWArea* area = pb->areaMap.areaList;
    PWArea* end = area + pb->map->base.length;

    for (; area < end; ++area)
        if (!area->trueMatch)
            continentList.push_back(area);

    uint32 totalLand = 0;

    for (PWArea* area : continentList)
        totalLand += area->size;

    // sort all the continents by size, largest first
    std::sort(continentList.begin(), continentList.end(), [](PWArea* a, PWArea* b) { return a->size > b->size; });

    uint32 biggest = continentList.front()->size;
    pb->oldWorldPercent = biggest / (float64)totalLand;

    return gSet.pangaeaSize < pb->oldWorldPercent;
}

Coord GetMeteorStrike(PangaeaBreaker* pb)
{
    std::vector<PWArea*> continentList;

    PWArea* area = pb->areaMap.areaList;
    PWArea* end = area + pb->map->base.length;

    for (; area < end; ++area)
        if (!area->trueMatch)
            continentList.push_back(area);

    std::sort(continentList.begin(), continentList.end(), [](PWArea* a, PWArea* b) { return a->size > b->size; });

    uint32 biggest = continentList.front()->ind;

    return GetHighestCentrality(pb, biggest);
}

void CastMeteorUponTheEarth(PangaeaBreaker* pb, Coord c, uint8* plotTypes, uint8* terrainTypes)
{
    Dim dim = pb->map->base.dim;
    uint32 radius = PWRandInt(minimumMeteorSize + 1, (uint32)floor(dim.w / 16.0f));
    std::vector<uint32> ringList = GetRingAroundCell(dim, c, radius);

    // destroy center
    uint32 i = GetIndex(&pb->map->base, c);
    terrainTypes[i] = tOCEAN;
    plotTypes[i] = ptOcean;
    pb->map->base.data[i] = pb->map->seaThreshold - 0.01;

    for (i = 0; i < ringList.size(); ++i)
    {
        uint32 ind = ringList[i];
        if (terrainTypes[ind] != tOCEAN)
        {
            terrainTypes[ind] = tCOAST;
            pb->map->base.data[i] = pb->map->seaThreshold - 0.01;
        }
        plotTypes[ind] = ptOcean;
        pb->map->base.data[ind] = pb->map->seaThreshold - 0.01;
    }

    std::vector<uint32> innerList = GetRadiusAroundCell(dim, c, radius - 1);

    for (i = 0; i < innerList.size(); ++i)
    {
        uint32 ind = innerList[i];
        terrainTypes[ind] = tOCEAN;
        plotTypes[ind] = ptOcean;
        pb->map->base.data[ind] = pb->map->seaThreshold - 0.01;
    }
}

// UNUSED: void CreateDistanceMap(PangaeaBreaker* pb)

Coord GetHighestCentrality(PangaeaBreaker* pb, uint32 id)
{
    std::vector<CentralityScore> cs = CreateCentralityList(pb, id);
    std::sort(cs.begin(), cs.end(), [](CentralityScore& a, CentralityScore& b) { return a.centrality > b.centrality; });
    return cs.front().c;
}

// creates the sub portion of the continent to be analyzed this was meant to save processing time
std::vector<CentralityScore> CreateContinentList(PangaeaBreaker* pb, uint32 id)
{
    Dim dim = pb->map->base.dim;
    std::vector<CentralityScore> cs;
    std::vector<uint32> indexMap;
    uint32 cnt = 0;

    float64* aID = pb->areaMap.base.data;
    Coord c;

    for (c.y = 0; c.y < dim.h; ++c.y)
        for (c.x = 0; c.x < dim.w; ++c.x, ++aID)
            if (*aID == id)
            {
                cs.push_back({});
                InitCentralityScore(&cs.back(), pb->map, c);
                indexMap.push_back(cnt);
                ++cnt;
            }
            else
                indexMap.push_back(-1);

    for (CentralityScore& s : cs)
    {
        std::vector<uint32> nList = GetRadiusAroundCell(dim, s.c, 1);
        for (uint32 i : nList)
            if (pb->areaMap.base.data[i] == id)
                s.neighborList.push_back(indexMap[i]);
    }

    return cs;
}

std::vector<CentralityScore> CreateCentralityList(PangaeaBreaker* pb, uint32 id)
{
    std::vector<CentralityScore> cs = CreateContinentList(pb, id);

    std::vector<uint32> delta;
    std::vector<uint32> sigma;
    std::vector<int32> d;
    std::vector<std::vector<uint32>> P;
    std::vector<uint32> Q;
    std::vector<uint32> S;

    sigma.resize(cs.size());
    d.resize(cs.size());
    P.resize(cs.size());
    delta.resize(cs.size());

    for (uint32 i = 0; i < cs.size(); ++i)
    {
        Q.clear();
        S.clear();
        std::fill(sigma.begin(), sigma.end(), 0);
        std::fill(d.begin(), d.end(), -1);
        std::for_each(P.begin(), P.end(), [](std::vector<uint32>& list) { list.clear(); });
        std::fill(delta.begin(), delta.end(), 0);

        sigma[i] = 1;
        d[i] = 0;
        Q.push_back(i);

        // lazy fifo
        uint32 qIt = 0;
        while (qIt != Q.size())
        {
            uint32 v = Q[qIt];
            ++qIt;
            S.push_back(v);

            for (uint32 w : cs[v].neighborList)
            {
                if (d[w] < 0)
                {
                    Q.push_back(w);
                    d[w] = d[v] + 1;
                }

                if (d[w] == d[v] + 1)
                {
                    //assert(sigma[w] + sigma[v] > sigma[w]);
                    sigma[w] += sigma[v];
                    P[w].push_back(v);
                }
            }
        }

        while (!S.empty())
        {
            uint32 w = S.back();
            S.pop_back();

            for (uint32 v : P[w])
                delta[v] += (uint32)floor(sigma[v] / (float64)sigma[w]) * (1 + delta[w]);

            if (w != i)
                cs[w].centrality += delta[w];
        }
    }

    return cs;
}

void CreateNewWorldMap(PangaeaBreaker* pb)
{
    Dim dim = pb->map->base.dim;

    ttMatch = pb->terrainTypes;
    DefineAreas(&pb->areaMap, [](uint32 i) { return ttMatch[i] == tOCEAN; }, false);

    std::vector<PWArea*> continentList;

    PWArea* area = pb->areaMap.areaList;
    PWArea* aEnd = area + pb->map->base.length;

    for (; area < aEnd; ++area)
        if (!area->trueMatch)
            continentList.push_back(area);

    // sort all the continents by size, largest first
    std::sort(continentList.begin(), continentList.end(), [](PWArea* a, PWArea* b) { return a->size > b->size; });

    uint32 biggest = continentList.front()->size;

    uint32 totalLand = 0;

    for (PWArea* area : continentList)
        totalLand += area->size;

    std::vector<uint32> newWorldList;
    newWorldList.push_back(continentList[1]->ind);
    uint32 newWorldSize = continentList[1]->size;

    // sort remaining continents by ID, to mix it up
    std::sort(continentList.begin() + 2, continentList.end(), [](PWArea* a, PWArea* b) { return a->ind > b->ind; });

    // add new world continents until mc.maxNewWorldSize is reached
    for (auto it = continentList.begin() + 2; it < continentList.end(); ++it)
    {
        PWArea* c = *it;

        if ((newWorldSize + c->size) / totalLand >= gSet.maxNewWorldSize)
            break;

        newWorldList.push_back(c->ind);
        newWorldSize += c->size;
    }

    // first assume old world
    memset(pb->newWorldMap, 0, pb->map->base.length * sizeof *pb->newWorldMap);

    // mark new world
    for (uint32 id : newWorldList)
    {
        bool* it = pb->newWorldMap;
        bool* end = it + pb->map->base.length;
        area = pb->areaMap.areaList;;
        for (; it < end; ++it, ++area)
            if (area->ind == id)
                *it = true;
    }
}

// UNUSED: bool IsTileNewWorld(PangaeaBreaker* pb)

std::vector<uint32> GetLargeOldWorldPlots(PangaeaBreaker* pb)
{
    Dim dim = pb->map->base.dim;
    bool* it = pb->newWorld;
    bool* end = it + pb->map->base.length;
    MapTile* plot = gMap;
    float64* id = pb->areaMap.base.data;
    uint32 i = 0;

    std::vector<uint32> plots;

    for (; it < end; ++it, ++plot, ++id, ++i)
        if (!*it && !IsWater(plot))
        {
            PWArea* area = GetAreaByID(&pb->areaMap, (uint32)*id);

            if (area->size > 30)
                plots.push_back(i);
        }

    return plots;
}


// --- CentralityScore

void InitCentralityScore(CentralityScore * score, ElevationMap* map, Coord c)
{
    score->map = map;
    score->c = c;
    score->centrality = 0;
}


// --- Starting Plot Stuff ----------------------------------------------------

bool IsCityRealEstateMatch(CentralityScore * score, Coord c)
{
    uint32 i = GetIndex(&score->map->base, c);
    MapTile* plot = gMap + i;

    if (plot->isImpassable || IsWater(plot))
        return false;

    // TODO:
    uint32 dist = 0;
    //dist = Map.GetPlotDistance(latestStartPlotIndex, i)

    return dist <= 3;
}

std::vector<uint32> GetOldWorldPlots(PangaeaBreaker* pb)
{
    Dim dim = pb->map->base.dim;

    bool* it = pb->newWorld;
    bool* end = it + pb->map->base.length;
    MapTile* plot = gMap;
    uint32 i = 0;

    std::vector<uint32> plots;

    for (; it < end; ++it, ++plot, ++i)
        if (!*it && !IsWater(plot))
            plots.push_back(i);

    return plots;
}

std::vector<uint32> GetNewWorldPlots(PangaeaBreaker* pb)
{
    Dim dim = pb->map->base.dim;

    bool* it = pb->newWorld;
    bool* end = it + pb->map->base.length;
    MapTile* plot = gMap;
    uint32 i = 0;

    std::vector<uint32> plots;

    for (; it < end; ++it, ++plot, ++i)
        if (*it && !IsWater(plot))
            plots.push_back(i);

    return plots;
}


// --- AssignStartingPlots ----------------------------------------------------

struct AssignStartingPlots
{
    uint32 iNumMajorCivs = 0;
    uint32 iNumMinorCivs = 0;
    uint32 iResourceEraModifier = 1;
    uint32 iNumRegions = 0;
    uint32 iDefaultNumberMajor = 0;
    uint32 iDefaultNumberMinor = 0;
    int32 uiMinMajorCivFertility = 0;
    int32 uiMinMinorCivFertility = 0;
    uint32 uiStartMinY = 0;
    uint32 uiStartMaxY = 0;
    uint32 uiStartConfig = 2;
    bool waterMap = false;
    bool landMap = false;
    bool noStartBiases = false;
    bool startLargestLandmassOnly = false;

    void* majorStartPlots;
    void* majorCopy;
    void* minorStartPlots;
    void* minorCopy;
    void* majorList;
    void* minorList;
    void* playerstarts;
    void* sortedArray;
    void* sortedFertilityArray;
};

void InitAssignStartingPlots(AssignStartingPlots* asp)
{
    //uiMinMajorCivFertility = args.MIN_MAJOR_CIV_FERTILITY
    //uiMinMinorCivFertility = args.MIN_MINOR_CIV_FERTILITY
    //uiStartMinY = args.START_MIN_Y
    //uiStartMaxY = args.START_MAX_Y
    //uiStartConfig = args.START_CONFIG
    //waterMap  = args.WATER
    //landMap  = args.LAND
    //noStartBiases = args.IGNORESTARTBIAS
    //startLargestLandmassOnly = args.START_LARGEST_LANDMASS_ONLY
}

std::vector<uint32> FilterBadStarts(AssignStartingPlots* asp, PangaeaBreaker* pb, std::vector<uint32>& badStarts, bool bMajor)
{
    std::vector<uint32> betterStarts;

    for (uint32 start : badStarts)
    {
        uint32 fertility = 0;//TODO: = __BaseFertility(start);

        PWArea* area = GetAreaByID(&pb->areaMap, (uint32)pb->areaMap.base.data[start]);

        if (bMajor && fertility >= 10)
        {
            if (area->size > 30)
                betterStarts.push_back(start);
        }
        else if (!bMajor && fertility >= 5)
        {
            if (area->size > 5)
                betterStarts.push_back(start);
        }
    }

    if (!betterStarts.empty())
        printf("No better starts");

    return betterStarts;
}

void __InitStartingData(AssignStartingPlots* asp)
{
    if (asp->uiMinMajorCivFertility <= 0)
        asp->uiMinMajorCivFertility = 5;
    if (asp->uiMinMinorCivFertility <= 0)
        asp->uiMinMinorCivFertility = 5;

    // Find Default Number

}

void __SetStartMajor() {}
void AddCliffs(uint8* plotTypes, uint8* terrainTypes) {}
void SetCliff() {}


// --- FeatureGenerator -------------------------------------------------------

void AddFeaturesFromContinents() {}

// TODO: add expansion natural wonders list

void GetNaturalWonderString() {}
void NW_IsMountain() {}
void NW_IsPassableLand() {}
void NW_IsDesert() {}


// --- NaturalWonderGenerator -------------------------------------------------

void __FindValidLocs() {}
void __PlaceWonders() {}
void PWCanHaveFeature() {}
void PWSetFeatureType() {}
void HijackedByPW() {}
void PWDebugCheat() {}
void PWSetCraterLake() {}
void PWSetChocoHills() {}
void PWSetDanxia() {}
void PWSetBarrierReef() {}
void PWSetGalapagos() {}
void PWSetMatterhorn() {}
void PWSetIkkil() {}
void PWSetUbsunurHollow() {}
void PWSetPantanal() {}
void PWSetEverest() {}
void PWSetRoraima() {}
void PWSetYosemiteOrTDP() {}
void PWCanHaveCraterLake() {}
void PWCanHaveChocoHills() {}
void PWCanHaveDanxia() {}
void PWCanHaveBarrierReef() {}
void PWCanHaveGalapagos() {}
void PWCanHaveMatterhorn() {}
void PWCanHaveIkkil() {}
void PWCanHaveUbsunurHollow() {}
void PWCanHavePanatal() {}
void PWCanHaveEverest() {}
void PWCanHaveRoraima() {}
void PWCanHaveYosemiteOrTDP() {}
void HasInternalRivers() {}
void HasVolcanoesOrWater() {}
void HasOnlyPassableLand() {}
void GetAdjacentPlots() {}
void GetOppositeDirection() {}


// --- TerrainArea ---------------------------------------------------------------

struct TerrainArea
{

};

void InitTerrainArea(TerrainArea * area) {}
void CalculateCenter(TerrainArea * area) {}
