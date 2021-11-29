#include "PerfectOptWorld6.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <vector>

#define _USE_MATH_DEFINES
#include <math.h>



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
    uint32 rectX;
    uint32 rectY;
    uint32 rectWidth;
    uint32 rectHeight;
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
    void* outflow;
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
    uint32 rainfall;
};

struct River
{
    RiverJunction sourceJunc;
    uint32 riverID;
    std::vector<RiverJunction*> junctions;
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
    fNo,
    fWest,
    fEast,
    fVert,
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



// --- Util Functions ---------------------------------------------------------

static Dir FlipDir(Dir dir)
{
    return (Dir)((dir + 3) % 6);
}

static float32 BellCurve(float32 value)
{
    return sin(value * M_PI * 2.0f - M_PI_2) * 0.5f + 0.5f;
}

static float64 PWRandSeed(uint32 fixed_seed = 0)
{
    uint32 seed = fixed_seed;
    //seed = 394527185;
    if (seed == 0)
    {
        seed = rand() % 256;
        seed = seed << 8 + rand() % 256;
        seed = seed << 8 + rand() % 256;
        seed = seed << 4 + rand() % 256;
    }

    printf("Random seed for this map is: %i\n", seed);
    srand(seed);
}

static float64 PWRand()
{
    return rand() / (float64)RAND_MAX;
}

static float64 PWRandInt(int32 min, int32 max)
{
    assert(max - min <= RAND_MAX);
    return (rand() % (max - min)) + min;
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
            noiseMap->rectX = floor(noiseMap->dim.w / 2 - (destMapWidth * freq) / 2);
            noiseMap->rectWidth = std::max(floor(destMapWidth * freq), 1.0);
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
            noiseMap->rectY = floor(noiseMap->dim.h / 2 - (destMapHeight * freq) / 2);
            noiseMap->rectHeight = std::max(floor(destMapHeight * freq), 1.0);
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
            noiseMap->rectX = floor(noiseMap->dim.w / 2 - (destMapWidth * freq) / 2);
            noiseMap->rectWidth = floor(destMapWidth * freq);
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
            noiseMap->rectY = floor(noiseMap->dim.h / 2 - (destMapHeight * freq) / 2);
            noiseMap->rectHeight = floor(destMapHeight * freq);
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

// --- FloatMap

void InitFloatMap(FloatMap * map, uint16 width, uint16 height, bool wrapX, bool wrapY)
{
    assert(map);
    assert(!map->data);

    map->dim.w = width;
    map->dim.h = height;
    map->length = width * height;
    map->wrapX = wrapX;
    map->wrapY = wrapY;

    map->data = (float64*)calloc(map->length, sizeof(*map->data));

    // TODO: check member size
    map->rectX = 0;
    map->rectY = 0;
    map->rectWidth = width;
    map->rectHeight = height;
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
        return map->length;// TODO: assert(0);
    else
        x = coord.x;

    if (map->wrapY)
        y = coord.y % map->dim.h;
    else if (coord.y > map->dim.h - 1)
        return map->length;// TODO: assert(0);
    else
        y = coord.y;

    return y * map->dim.w + x;
}

// TODO: Don't use
Coord GetXYFromIndex(FloatMap* map, uint32 ind)
{
    return { ind % map->dim.w, ind / map->dim.w };
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
    Coord coord = { map->rectX + (coord.x % map->rectWidth),
                    map->rectY + (coord.y % map->rectHeight) };

    return GetIndex(map, coord);
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
    float64* maplist = (float64 *)alloca(map->length * sizeof(float64));

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
    return floor((((lat - settings->bottomLatitude) / range) * map->dim.h) + 0.5);
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
    FILE* fp = fopen("map_data.csv", "w");

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

void InitElevationMap(ElevationMap* map, uint32 width, uint32 height, bool xWrap, bool yWrap)
{
    InitFloatMap(&map->base, width, height, xWrap, yWrap);

    map->seaLevelThreshold = 0.0;
}

bool IsBelowSeaLevel(ElevationMap* map, Coord c)
{
    uint32 i = GetIndex(&map->base, c);
    return map->base.data[i] < map->seaLevelThreshold;
}

// --- AreaMap

void InitPWAreaMap(PWAreaMap* map, uint32 width, uint32 height, bool xWrap, bool yWrap)
{
    InitFloatMap(&map->base, width, height, xWrap, yWrap);

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

typedef bool (*Match)(Coord);

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

void InitLineSeg(LineSeg* seg, uint16 y, uint16 xLeft, uint16 xRight, float64 dy)
{
    seg->y = y;
    seg->xLeft = xLeft;
    seg->xRight = xRight;
    seg->dy = dy;
}


// --- RiverMap

struct RiverMap
{
    ElevationMap* elevationMap;

    RiverHex * riverData;
    std::vector<River> rivers;
};

void InitRiverMap(RiverMap* map, ElevationMap * elevMap)
{
    map->elevationMap = elevMap;
    map->riverData = (RiverHex *)malloc(elevMap->base.length * sizeof(RiverHex));

    Coord c;
    RiverHex* it = map->riverData;
    for (c.y = 0; c.y < map->elevationMap->base.dim.h; ++c.y)
        for (c.x = 0; c.x < map->elevationMap->base.dim.w; ++c.x, ++it)
            InitRiverHex(it, c);
}

RiverJunction* GetJunction(RiverMap* map, Coord c, bool isNorth)
{
    uint32 i = GetIndex(&map->elevationMap->base, c);
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
    case fWest:
        c.x = junc->coord.x + odd - 1;
        c.y = junc->isNorth ? junc->coord.y + 1 : junc->coord.y - 1;
        break;
    case fEast:
        c.x = junc->coord.x + odd;
        c.y = junc->isNorth ? junc->coord.y + 1 : junc->coord.y - 1;
        break;
    case fVert:
        c.x = junc->coord.x;
        c.y = junc->isNorth ? junc->coord.y + 2 : junc->coord.y - 2;
        break;
    case fNo:
    default:
        assert(0 && "can't get junction neighbor in direction NOFLOW");
        break;
    }

    if (GetIndex(&map->elevationMap->base, c) != -1)
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

    uint32 i = GetIndex(&map->elevationMap->base, c);
    if (i != -1)
        return &map->riverData[i];
    return NULL;
}

uint8 GetJunctionsAroundHex(RiverMap* map, RiverHex* hex, RiverJunction* out[6])
{
    out[0] = &hex->northJunction;
    out[1] = &hex->southJunction;

    uint8 ind = 2;

    RiverJunction* junc = GetJunctionNeighbor(map, fWest, &hex->northJunction);
    if (junc)
    {
        out[ind] = junc;
        ++ind;
    }

    RiverJunction* junc = GetJunctionNeighbor(map, fEast, &hex->northJunction);
    if (junc)
    {
        out[ind] = junc;
        ++ind;
    }

    RiverJunction* junc = GetJunctionNeighbor(map, fWest, &hex->southJunction);
    if (junc)
    {
        out[ind] = junc;
        ++ind;
    }

    RiverJunction* junc = GetJunctionNeighbor(map, fEast, &hex->southJunction);
    if (junc)
    {
        out[ind] = junc;
        ++ind;
    }

    return ind;
}

void SetJunctionAltitudes(RiverMap* map) {}
void isLake(RiverMap* map) {}
void GetNeighborAverage(RiverMap* map) {}
void SiltifyLakes(RiverMap* map) {}
void RecreateNewLakes(RiverMap* map) {}
void GrowLake(RiverMap* map) {}
void GetLowestJunctionAroundHex(RiverMap* map) {}
void SetOutflowForLakeHex(RiverMap* map) {}
void GetRandomLakeSize(RiverMap* map) {}
void ValidLakeHex(RiverMap* map) {}
void GetInitialLake(RiverMap* map) {}
void SetFlowDestinations(RiverMap* map) {}
void GetValidFlows(RiverMap* map) {}
void IsTouchingOcean(RiverMap* map) {}
void SetRiverSizes(RiverMap* map) {}
void GetNextJunctionInFlow(RiverMap* map) {}
void IsRiverSource(RiverMap* map) {}
void CreateRiverList(RiverMap* map) {}
void LongEnough(RiverMap* map) {}
void AssignRiverIDs(RiverMap* map) {}
void GetRiverHexForJunction(RiverMap* map) {}
void GetFlowDirections(RiverMap* map) {}


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
    junc->flow = fNo;
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
    printf("junction at %d, %d isNorth=%d, flow=%s, size=%f, submerged=%d, outflow=%x, isOutflow=%d riverID = %d",
        junc->coord.x, junc->coord.y, junc->isNorth, flowStr[junc->flow], junc->size, junc->submerged, junc->outflow, junc->isOutflow, junc->id);
}


// --- River

void InitRiver(River * river, RiverJunction * sourceJunc, uint32 rawID)
{
    river->sourceJunc = *sourceJunc;
    river->riverID = UINT32_MAX;

    river->sourceJunc.rawID = rawID;
    river->junctions.push_back(&river->sourceJunc);
}

void Add(River* river, RiverJunction* junc)
{
    river->junctions.push_back(junc);
}

uint32 GetLength(River* river)
{
    return river->junctions.size();
}


// --- Generation Functions ---------------------------------------------------

void CountLand() {}
void GenerateMap() {}

void GenerateTwistedPerlinMap() {}
void ArrayRemove() {}
void ShuffleList() {}
void GenerateMountainMap() {}
void WaterMatch() {}
void GetAttenuationFactor() {}
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
void GetRadiusAroundCell() {}
void GetRingAroundCell() {}


// --- Generation Functions ---------------------------------------------------

void isNonCoastWaterMatch() {}
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
