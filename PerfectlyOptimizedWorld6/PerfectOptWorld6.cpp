#include "PerfectOptWorld6.h"

#define _USE_MATH_DEFINES
#include <cmath>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <vector>

#include "MapEnums.h"

#pragma warning( disable : 6011 6387 26451 )

// --- Data Types -------------------------------------------------------------

struct Coord
{
    uint16 x;
    uint16 y;
};

struct Dim
{
    uint16 w;
    uint16 h;
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
    uint16 rectX;
    uint16 rectY;
    uint16 rectWidth;
    uint16 rectHeight;
};

struct ElevationMap
{
    FloatMap base;
    float64 seaLevelThreshold;
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
    uint16 y;
    uint16 xLeft;
    uint16 xRight;
    int16 dy;
};

struct PWAreaMap
{
    FloatMap base;

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

struct LakeDataUtil
{
    uint32 lakesToAdd;
    uint32 lakesAdded;
    uint32 currentLakeID;
    uint32 currentLakeSize;
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

    uint8 mapSizeType = msMAPSIZE_STANDARD;
};

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

PW6Settings gSettings;
// Create a 1 mb buffer
MapTile gMap[200000];


// --- Forward Declarations ---------------------------------------------------

uint32 GetRectIndex(FloatMap* map, Coord coord);
bool IsOnMap(FloatMap* map, Coord c);
void InitPWArea(PWArea* area, uint32 ind, Coord c, bool trueMatch);
void InitLineSeg(LineSeg* seg, uint16 y, uint16 xLeft, uint16 xRight, int16 dy);
void FillArea(PWAreaMap* map, Coord c, PWArea* area, Match mFunc);
void ScanAndFillLine(PWAreaMap* map, LineSeg seg, PWArea* area, Match mFunc);
uint16 ValidateY(PWAreaMap* map, uint16 y);
uint16 ValidateX(PWAreaMap* map, uint16 x);
void InitRiverHex(RiverHex* hex, Coord c);
bool ValidLakeHex(RiverMap* map, RiverHex* lakeHex, LakeDataUtil* ldu);
uint32 GetRandomLakeSize(RiverMap* map);
void GrowLake(RiverMap* map, RiverHex* lakeHex, uint32 lakeSize, LakeDataUtil* ldu,
    std::vector<RiverHex*>& lakeList, std::vector<RiverHex*>& growthQueue);
RiverJunction* GetLowestJunctionAroundHex(RiverMap* map, RiverHex* lakeHex);
std::vector<FlowDir> GetValidFlows(RiverMap* map, RiverJunction* junc);
void SetOutflowForLakeHex(RiverMap* map, RiverHex* lakeHex, RiverJunction* outflow);
std::vector<uint32> GetRadiusAroundCell(Coord c);
RiverJunction* GetNextJunctionInFlow(RiverMap* map, RiverJunction* junc);
void InitRiver(River* river, RiverJunction* sourceJunc, uint32 rawID);
void Add(River* river, RiverJunction* junc);
void AddParent(RiverJunction* junc, RiverJunction* parent);
void InitRiverJunction(RiverJunction* junc, Coord c, bool isNorth);
float64 GetAttenuationFactor(FloatMap* map, float64 val, Coord c);


// --- Util Functions ---------------------------------------------------------

static Dir FlipDir(Dir dir)
{
    return (Dir)((dir + 3) % 6);
}

static float64 BellCurve(float64 value)
{
    return sin(value * M_PI * 2.0 - M_PI_2) * 0.5 + 0.5;
}

static float64 PWRandSeed(uint32 fixed_seed = 0)
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
    float64 fractionX = x - floor(x);
    float64 fractionY = y - floor(y);

    // wrappedX and wrappedY are set to -1,-1 of the sampled area
    // so that the sample area is in the middle quad of the 4x4 grid
    uint32 wrappedX = (((uint32)floor(x) - 1) % srcMap->rectWidth) + srcMap->rectX;
    uint32 wrappedY = (((uint32)floor(y) - 1) % srcMap->rectHeight) + srcMap->rectY;

    Coord c;

    for (uint16 pY = 0; pY < 4; ++pY)
    {
        c.y = pY + wrappedY;
        for (uint16 pX = 0; pX < 4; ++pX)
        {
            c.x = pX + wrappedX;
            uint32 srcIndex = GetRectIndex(srcMap, c);
            points[(pY * 4 + pX) + 1] = srcMap->data[srcIndex];
        }
    }

    float64 finalValue = BicubicInterpolate(points, fractionX, fractionY);

    return finalValue;
}

float64 GetDerivativeValue(float64 x, float64 y, FloatMap* srcMap)
{
    float64 points[16];
    float64 fractionX = x - floor(x);
    float64 fractionY = y - floor(y);

    // wrappedX and wrappedY are set to -1,-1 of the sampled area
    // so that the sample area is in the middle quad of the 4x4 grid
    uint32 wrappedX = (((uint32)floor(x) - 1) % srcMap->rectWidth) + srcMap->rectX;
    uint32 wrappedY = (((uint32)floor(y) - 1) % srcMap->rectHeight) + srcMap->rectY;

    Coord c;

    for (uint16 pY = 0; pY < 4; ++pY)
    {
        c.y = pY + wrappedY;
        for (uint16 pX = 0; pX < 4; ++pX)
        {
            c.x = pX + wrappedX;
            uint32 srcIndex = GetRectIndex(srcMap, c);
            points[(pY * 4 + pX) + 1] = srcMap->data[srcIndex];
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
            noiseMap->rectX = (uint32)floor(noiseMap->dim.w / 2 - (destMapWidth * freq) / 2);
            noiseMap->rectWidth = (uint32)std::max(floor(destMapWidth * freq), 1.0);
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
            noiseMap->rectY = (uint32)floor(noiseMap->dim.h / 2 - (destMapHeight * freq) / 2);
            noiseMap->rectHeight = (uint32)std::max(floor(destMapHeight * freq), 1.0);
            freqY = noiseMap->rectHeight / destMapHeight;
        }
        else
        {
            noiseMap->rectY = 0;
            noiseMap->rectHeight = noiseMap->dim.h;
            freqY = freq;
        }

        finalValue = finalValue + GetInterpolatedValue(x * freqX, y * freqY, noiseMap) * amp;
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
            noiseMap->rectX = (uint32)floor(noiseMap->dim.w / 2 - (destMapWidth * freq) / 2);
            noiseMap->rectWidth = (uint32)floor(destMapWidth * freq);
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
            noiseMap->rectY = (uint32)floor(noiseMap->dim.h / 2 - (destMapHeight * freq) / 2);
            noiseMap->rectHeight = (uint32)floor(destMapHeight * freq);
            freqY = noiseMap->rectHeight / destMapHeight;
        }
        else
        {
            noiseMap->rectY = 0;
            noiseMap->rectHeight = noiseMap->dim.h;
            freqY = freq;
        }

        finalValue = finalValue + GetDerivativeValue(x * freqX, y * freqY, noiseMap) * amp;
        freq *= 2.0;
        amp *= amplitudeChange;
    }

    return finalValue / octaves;
}



// --- Member Functions -------------------------------------------------------------

// --- MapTile

inline bool IsWater(MapTile* tile)
{
    return tile->terrain == tTERRAIN_COAST || tile->terrain == tTERRAIN_OCEAN;
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

void GetNeighbor(FloatMap * map, Coord coord, Dir dir, Coord * out)
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
uint32 GetRectIndex(FloatMap* map, Coord coord)
{
    Coord c = { (uint16)(map->rectX + (coord.x % map->rectWidth)),
                (uint16)(map->rectY + (coord.y % map->rectHeight)) };

    return GetIndex(map, c);
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
    float64* maplist = (float64 *)malloc(map->length * sizeof(float64));

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
        float64* ins = maplist;
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
        memcpy(maplist, map->data, map->length * sizeof(float64));
        size = map->length;
    }

    std::sort(maplist, maplist + size);
    // storing the value locally to prevent potential issues with alloca
    float64 retval = maplist[(uint32)(size * percent)];
    free(maplist);
    return retval;
}

float64 GetLatitudeForY(FloatMap* map, PW6Settings* settings, uint16 y)
{
    int32 range = settings->topLatitude - settings->bottomLatitude;
    return (y / (float64)map->dim.h) * range + settings->bottomLatitude;
}

uint16 GetYForLatitude(FloatMap* map, PW6Settings* settings, float64 lat)
{
    int32 range = settings->topLatitude - settings->bottomLatitude;
    return (uint16)floor((((lat - settings->bottomLatitude) / range) * map->dim.h) + 0.5);
}

WindZone GetZone(FloatMap* map, PW6Settings* settings, uint16 y)
{
    if (y >= map->dim.h)
        return wNo;

    // TODO: preassign values to bit fields
    float64 lat = GetLatitudeForY(map, settings, y);

    if (lat > settings->polarFrontLatitude)
        return wNPolar;
    else if (lat >= settings->horseLatitudes)
        return wNTemperate;
    else if (lat >= 0.0)
        return wNEquator;
    else if (lat > -settings->horseLatitudes)
        return wSEquator;
    else if (lat >= -settings->polarFrontLatitude)
        return wSTemperate;

    return wSPolar;
}

uint16 GetYFromZone(FloatMap* map, PW6Settings* settings, WindZone zone, bool bTop)
{
    // TODO: pre calc these values on loading settings
    if (bTop)
    {
        for (uint16 y = map->dim.h; y > 0;)
        {
            --y;
            if (GetZone(map, settings, y) == zone)
                return y;
        }
    }
    else
    {
        for (uint16 y = 0; y < map->dim.h; ++y)
            if (GetZone(map, settings, y) == zone)
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

float64 GetGeostrophicPressure(FloatMap* map, PW6Settings* settings, float64 lat)
{
    float64 latRange;
    float64 latPercent;
    float64 pressure;

    if (lat > settings->polarFrontLatitude)
    {
        latRange = 90.0 - settings->polarFrontLatitude;
        latPercent = (lat - settings->polarFrontLatitude) / latRange;
        pressure = 1.0 - latPercent;
    }
    else if (lat >= settings->horseLatitudes)
    {
        latRange = settings->polarFrontLatitude - settings->horseLatitudes;
        latPercent = (lat - settings->horseLatitudes) / latRange;
        pressure = latPercent;
    }
    else if (lat >= 0.0)
    {
        latRange = settings->horseLatitudes - 0.0;
        latPercent = (lat - 0.0) / latRange;
        pressure = 1.0 - latPercent;
    }
    else if (lat > -settings->horseLatitudes)
    {
        latRange = 0.0 + settings->horseLatitudes;
        latPercent = (lat + settings->horseLatitudes) / latRange;
        pressure = latPercent;
    }
    else if (lat >= -settings->polarFrontLatitude)
    {
        latRange = -settings->horseLatitudes + settings->polarFrontLatitude;
        latPercent = (lat + settings->polarFrontLatitude) / latRange;
        pressure = 1.0 - latPercent;
    }
    else
    {
        latRange = -settings->polarFrontLatitude + 90.0;
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

uint32 GetRadiusAroundHex(FloatMap* map, Coord c, uint32 rad, Coord * out)
{
    uint32 binCoef = (rad * (rad + 1)) / 2;
    uint32 maxTiles = 1 + 6 * binCoef;
    out = (Coord*)calloc(maxTiles, sizeof(Coord));
    *out = c;
    uint32 count = 1;

    Coord ref = c;
    Coord* it = out + 1;

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

    return count;
}

float64 GetAverageInHex(FloatMap* map, Coord c, uint32 rad)
{
    Coord* refCoords = NULL;
    uint32 size = GetRadiusAroundHex(map, c, rad, refCoords);
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
    uint32 size = GetRadiusAroundHex(map, c, rad, refCoords);
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
    float64* smoothedData = (float64*)malloc(map->length);

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
    float64* deviatedData = (float64*)malloc(map->length);

    float64* it = deviatedData;
    Coord c;
    for (c.y = 0; c.y < map->dim.h; ++c.y)
        for (c.x = 0; c.x < map->dim.w; ++c.x, ++it)
            *it = GetStdDevInHex(map, c, rad);

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

    map->seaLevelThreshold = 0.0;
}

bool IsBelowSeaLevel(ElevationMap* map, Coord c)
{
    uint32 i = GetIndex(&map->base, c);
    return map->base.data[i] < map->seaLevelThreshold;
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

void DefineAreas(PWAreaMap* map, Match mFunc, bool bDebug)
{
    Clear(map);

    if (map->areaList)
        free(map->areaList);
    map->areaList = (PWArea*)malloc(map->base.length * sizeof(PWArea));

    uint32 currentAreaID = 0;
    Coord c;
    //float64* it = map->base.data;
    PWArea* it = map->areaList;

    for (c.y = 0; c.y < map->base.dim.h; ++c.y)
        for (c.x = 0; c.x < map->base.dim.w; ++c.x, ++it)
        {
            ++currentAreaID;
            InitPWArea(it, currentAreaID, c, mFunc(c));
            it->debug = bDebug;

            FillArea(map, c, it, mFunc);
        }
}

void FillArea(PWAreaMap* map, Coord c, PWArea* area, Match mFunc)
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
void ScanAndFillLine(PWAreaMap* map, LineSeg seg, PWArea* area, Match mFunc)
{
    uint16 dy = seg.y + seg.dy;
    if (ValidateY(map, seg.y + seg.dy) == -1)
        return;

    uint16 odd = dy % 2;
    uint16 notOdd = seg.y % 2;
    int32 xStop = map->base.wrapX ? -(map->base.dim.w * 30) : -1;

    int32 lineFound = 0;
    int32 leftExtreme = INT32_MAX;

    for (int32 leftExt = seg.xLeft - odd; leftExt > xStop; --leftExt)
    {
        leftExtreme = leftExt;

        Coord c;
        c.x = ValidateX(map, leftExtreme);
        c.y = ValidateY(map, dy);
        uint32 i = GetIndex(&map->base, c);

        if (map->base.data[i] == 0 || area->trueMatch == mFunc(c))
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
        c.y = ValidateY(map, dy);
        uint32 i = GetIndex(&map->base, c);

        if (map->base.data[i] == 0 || area->trueMatch == mFunc(c))
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

uint16 ValidateY(PWAreaMap* map, uint16 y)
{
    if (map->base.wrapY)
        return y % map->base.dim.h;
    else if (y > map->base.dim.h)
        return -1;

    return y;
}

uint16 ValidateX(PWAreaMap* map, uint16 x)
{
    if (map->base.wrapX)
        return x % map->base.dim.w;
    else if (x > map->base.dim.w)
        return -1;

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

void InitLineSeg(LineSeg* seg, uint16 y, uint16 xLeft, uint16 xRight, int16 dy)
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
        (junc->altitude < map->eMap->seaLevelThreshold))
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

    free(lakeList);
    free(onQueueMapNorth);
    free(onQueueMapSouth);
}

float64* rainfallMap; // TODO: move

void RecreateNewLakes(RiverMap* map, PW6Settings * settings)
{
    LakeDataUtil ldu;
    ldu.lakesToAdd = (uint32)(map->eMap->base.length * settings->landPercent * settings->lakePercent);
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
        if (*it > map->eMap->seaLevelThreshold)
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
    std::vector<uint32> neighbors = GetRadiusAroundCell(lakeHex->coord);
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
    if (plot->feature != fFEATURE_NONE)
        return false;

    std::vector<uint32> cellList = GetRadiusAroundCell(lakeHex->coord);
    for (uint32 i : cellList)
    {
        RiverHex* nHex = map->riverData + i;

        if (map->eMap->base.data[i] < map->eMap->seaLevelThreshold)
            return false;
        else if (nHex->lakeID != -1 && nHex->lakeID != ldu->currentLakeSize)
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

void SetRiverSizes(RiverMap* map, PW6Settings* settings, float64 * locRainfallMap)
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

    uint32 riverIndex = (uint32)(floor(settings->riverPercent * size));
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
    if (river.junctions.size() >= gSettings.minRiverLength || river.sourceJunc->isOutflow)
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
    if (gSettings.minRiverLength)
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
        WOfRiver = tfdFLOWDIRECTION_SOUTH;
        WID = map->riverData[ii].southJunction.id;
    }

    GetNeighbor(&map->eMap->base, c, dSE, &coord);
    ii = GetIndex(&map->eMap->base, coord);

    if (ii != UINT32_MAX &&
        map->riverData[ii].northJunction.flow == fdVert &&
        map->riverData[ii].northJunction.size > map->riverThreshold &&
        map->riverData[ii].northJunction.id != UINT32_MAX)
    {
        WOfRiver = tfdFLOWDIRECTION_NORTH;
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
        NWOfRiver = tfdFLOWDIRECTION_SOUTHWEST;
        NWID = map->riverData[ii].northJunction.id;
    }

    if (map->riverData[i].southJunction.flow == fdEast &&
        map->riverData[i].southJunction.size > map->riverThreshold &&
        map->riverData[i].southJunction.id != UINT32_MAX)
    {
        NWOfRiver = tfdFLOWDIRECTION_NORTHEAST;
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
        NEOfRiver = tfdFLOWDIRECTION_SOUTHEAST;
        NEID = map->riverData[ii].northJunction.id;
    }

    if (map->riverData[i].southJunction.flow == fdWest &&
        map->riverData[i].southJunction.size > map->riverThreshold &&
        map->riverData[i].southJunction.id != UINT32_MAX)
    {
        NEOfRiver = tfdFLOWDIRECTION_NORTHWEST;
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
    hex->lakeID = -1;
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


// --- Generation Functions ---------------------------------------------------

void CountLand() {}
void GenerateMap() {}

void GenerateTwistedPerlinMap(Dim dim, bool xWrap, bool yWrap,
    float64 minFreq, float64 maxFreq, float64 varFreq,
    FloatMap* out)
{
    FloatMap inputNoise;
    InitFloatMap(&inputNoise, dim, xWrap, yWrap);
    GenerateNoise(&inputNoise);
    Normalize(&inputNoise);

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

    FloatMap inputNoise2;
    InitFloatMap(&inputNoise2, dim, xWrap, yWrap);
    GenerateBinaryNoise(&inputNoise2);
    Normalize(&inputNoise2);

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
    memcpy(stdDevMap.data, mountainMap->data, dim.w * dim.h);
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

    FloatMap moundMap;
    InitFloatMap(&moundMap, dim, xWrap, yWrap);
    float64* mtnIt = mountainMap->data;
    float64* mndIns = moundMap.data;
    for (c.y = 0; c.y < dim.h; ++c.y)
    {
        for (c.x = 0; c.x < dim.w; ++c.x, ++mtnIt, ++mndIns)
        {
            float64 val = *mtnIt;
            *mndIns = (sin(val * M_PI * 2 - M_PI_2) * 0.5 + 0.5) * GetAttenuationFactor(mountainMap, val, c);
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

    float64 stdDevThreshold = FindThresholdFromPercent(&stdDevMap, 1.0 - gSettings.landPercent, false);
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
}

void WaterMatch() {}
float64 GetAttenuationFactor(FloatMap* map, float64 val, Coord c) { return 0.0; }
void GenerateElevationMap() {}
void FillInLakes() {}
void GenerateTempMaps() {}
void GenerateRainfallMap() {}
void DistributeRain() {}
void GetRainCost() {}
void GetDifferenceAroundHex() {}
void PlacePossibleOasis() {}
void PlacePossibleIce() {}
void PlacePossibleReef() {}
void AddTerrainFromContinents() {}
void ApplyVolcanoBump() {}
void ApplyTerrain() {}
void GeneratePlotTypes() {}
void GenerateTerrain() {}
void FinalAlterations() {}
void GenerateCoasts() {}
void IsAdjacentToCoast() {}


// --- Generation Functions ---------------------------------------------------

void AddFeatures() {}
void GetDirectionString() {}
void AddRivers() {}
void ClearFloodPlains() {}
void GetRiverSidesForJunction() {}
void AddLakes() {}
std::vector<uint32> GetRadiusAroundCell(Coord c) { return {}; }
void GetRingAroundCell() {}


// --- Generation Functions ---------------------------------------------------

void isNonCoastWaterMatch() {}

struct PangaeaBreaker
{

};

void InitPangaeaBreaker(PangaeaBreaker * brk) {}
void BreakPangaeas(PangaeaBreaker* brk) {}
void IsPangea(PangaeaBreaker* brk) {}
void GetMeteorStrike(PangaeaBreaker* brk) {}
void CastMeteorUponTheEarth(PangaeaBreaker* brk) {}
void CreateDistanceMap(PangaeaBreaker* brk) {}
void GetHighestCentrality(PangaeaBreaker* brk) {}
void CreateContinentList(PangaeaBreaker* brk) {}
void CreateCentralityList(PangaeaBreaker* brk) {}
void CreateNewWorldMap(PangaeaBreaker* brk) {}
void IsTileNewWorld(PangaeaBreaker* brk) {}
void GetOldWorldPlots(PangaeaBreaker* brk) {}

struct CentralityScore
{

};

void InitCentralityScore(CentralityScore * score) {}
void IsCityRealEstateMatch(CentralityScore * score) {}
void GetOldWorldPlots(CentralityScore * score) {}
void GetNewWorldPlots(CentralityScore * score) {}
void FilterBadStarts(CentralityScore * score) {}


// --- AssignStartingPlots ----------------------------------------------------

void __InitStartingData() {}
void __SetStartMajor() {}
void AddCliffs() {}
void SetCliff() {}


// --- FeatureGenerator ----------------------------------------------------

void AddFeaturesFromContinents() {}

// TODO: add expansion natural wonders list

void GetNaturalWonderString() {}
void NW_IsMountain() {}
void NW_IsPassableLand() {}
void NW_IsDesert() {}


// --- NaturalWonderGenerator ----------------------------------------------------

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
