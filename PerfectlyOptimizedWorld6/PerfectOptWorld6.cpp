#include "PerfectOptWorld6.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

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

	printf("Random seed for this map is: %i", seed);
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
		assert(0);
	else
		x = coord.x;

	if (map->wrapY)
		y = coord.y % map->dim.h;
	else if (coord.y > map->dim.h - 1)
		assert(0);
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
	//	return 1.01;
	//if (percent <= 0.0)
	//	return -0.01;
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
	return floor((((lat - settings->bottomLatitude) / range) * map->dim.h) + .5);
}

void GetZone(FloatMap* map) {}
void GetYFromZone(FloatMap* map) {}
void GetGeostrophicWindDirections(FloatMap* map) {}
void GetGeostrophicPressure(FloatMap* map) {}
void ApplyFunction(FloatMap* map) {}
void GetRadiusAroundHex(FloatMap* map) {}
void GetAverageInHex(FloatMap* map) {}
void GetStdDevInHex(FloatMap* map) {}
void Smooth(FloatMap* map) {}
void Deviate(FloatMap* map) {}
void IsOnMap(FloatMap* map) {}
void Save(FloatMap* map) {}

// --- Generation Functions ---------------------------------------------------

void GenerateElevationMap(uint32 width, uint32 height, bool xWrap, bool yWrap, float32 * elevationMap)
{

}
