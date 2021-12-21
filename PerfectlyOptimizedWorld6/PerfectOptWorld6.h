#pragma once

#include "typedefs.h"

enum JungleCoversion
{
    jcNone,      // Leave them as grassland
    jcHillsOnly, // Change only the hills to plains
    jcAll,       // Change all jungle tiles to have underlying plains
};

enum ResourceSpread
{
    rsSparse,
    rsStandard,
    rsAbundant,

    rsRandom,
};

enum StartPosition
{
    spBalanced,
    spStandard,
    spLegendary,
};

struct PW6Settings
{
    /// General

    uint16 width = 98;
    uint16 height = 62;
    bool wrapX = true;
    bool wrapY = false;

    uint32 fixedSeed = 0;



    /// Generation

    // Top and bottom map latitudes.
    int32 topLatitude = 70;
    int32 bottomLatitude = -70;

    // Default frequencies for map of width 128. Adjusting these frequences
    // will generate larger or smaller map features.
    float64 twistMinFreq = 0.05;
    float64 twistMaxFreq = 0.12;
    float64 twistVar = 0.042;
    float64 mountainFreq = 0.078;



    /// Land/Water Division

    // Percent of land tiles on the map.
    float64 landPercent = 0.28;

    bool allowPangeas = false;
    // A continent with more land tiles than this percentage of total landtiles is
    // considered a pangaea and broken up if allowed.
    float64 pangaeaSize = 0.70;
    // Maximum percentage of land tiles that will be designated as new world continents.
    float64 maxNewWorldSize = 0.35;

    // These attenuation factors lower the altitude of the map edges. This is currently
    // used to prevent large continents in the uninhabitable polar regions.
    float64 northAttenuationFactor = 0.75;
    float64 northAttenuationRange = 0.15;  // Percent of the map height
    float64 southAttenuationFactor = 0.75;
    float64 southAttenuationRange = 0.15;

    // East west attenuation. Civ 6 creates a rather ugly seam when continents
    // straddle the map edge. It still plays well but I have decided to avoid
    // the map edges for aesthetic reasons.
    float64 eastAttenuationFactor = 0.75;
    float64 eastAttenuationRange = 0.10;  // Percent of the map height
    float64 westAttenuationFactor = 0.75;
    float64 westAttenuationRange = 0.10;



    /// Terrain

    // Percent of dry land that is below the hill elevation deviance threshold.
    float64 hillsPercent = 0.54;
    // Percent of dry land that is below the mountain elevation deviance threshold.
    float64 mountainsPercent = 0.86;

    // Percent of land that is below the desert rainfall threshold.
    float64 desertPercent = 0.37;
    // Percent of land that is below the plains rainfall threshold.
    float64 plainsPercent = 0.64;

    // Coldest absolute temperature allowed to be desert, plains if colder.
    float64 desertMinTemperature = 0.34;
    // Absolute temperature below which is tundra.
    float64 tundraTemperature = 0.32;
    // Absolute temperature below which is snow.
    float64 snowTemperature = 0.27;

    // Percent of land tiles made to be lakes
    float64 lakePercent = 0.04;

    // Fill in any lakes smaller than this. It looks bad to have large
    // river systems flowing into a tiny lake.
    uint32 minOceanSize = 50;

    // Weight of the mountain elevation map versus the coastline elevation map.
    float64 mountainWeight = 0.8;



    /// Rain

    // Strength of geostrophic climate generation versus monsoon climate generation.
    float64 geostrophicFactor = 3.0;

    float64 geostrophicLateralWindStrength = 0.6;

    // Important latitude markers used for generating climate.
    int32 polarFrontLatitude = 60;
    int32 tropicLatitudes = 23;
    int32 horseLatitudes = 28; // I shrunk these a bit to emphasize temperate lattitudes

    // Crazy rain tweaking variables. I wouldn't touch these if I were you.
    float64 minimumRainCost = 0.0001;
    int32 upLiftExponent = 4;
    float64 polarRainBoost = 0.00;



    /// Rivers

    // Percent of river junctions that are large enough to become rivers.
    float64 riverPercent = 0.55;
    // Minumum river length measured in hex sides. Shorter rivers that are not lake outflows will be culled.
    uint32 minRiverLength = 5;



    /// Features

    // Percent of land that is below the rainfall threshold where no trees can appear.
    float64 zeroTreesPercent = 0.30;
    // Percent of land below the jungle rainfall threshold.
    float64 junglePercent = 0.78;
    // Percent of land below the marsh rainfall threshold.
    float64 marshPercent = 0.92;

    // Coldest absolute temperature where trees appear.
    float64 treesMinTemperature = 0.27;
    // Coldest absolute temperature allowed to be jungle, forest if colder.
    float64 jungleMinTemperature = 0.70;

    // North and south ice latitude limits. Used for pre-GS
    int32 iceNorthLatitudeLimit = 60;
    int32 iceSouthLatitudeLimit = -60;

    // This is the percent of rivers that have floodplains that flood largest rivers always have priority for floodability
    float64 percentRiversFloodplains = 0.25;

    // Maximum chance for reef at highest temperature
    float64 maxReefChance = 0.15;

    // normally, jungle has plains underlying them. I personally don't like it because I want 
    // jungles to look wet, and I like that it makes settling in jungle more challenging, 
    // however, it's a subjective thing. Here is an option to change it back.
    JungleCoversion jungleToPlains = jcAll;

    // PerfectWorld maps are a bit bigger that normal maps, and it may be appropriate to
    // have slightly more natural wonders. This variable sets how many extra wonders are set
    // according to map size. I'm told that 'less is more' when it comes to natural wonders,
    // and I interpret that as a little more is maybe more than less.
    int32 naturalWonderExtra = 1;

    // These set the water temperature compression that creates the land/sea
    // seasonal temperature differences that cause monsoon winds.
    float64 minWaterTemp = 0.10;
    float64 maxWaterTemp = 0.60;

    // TODO: Natural Wonder whitelist/blacklist



    /// Assign Starting Plots

    uint32 numPlayers = 8;
    uint32 numCityStates = 12;
    ResourceSpread resources = rsStandard;
    StartPosition start = spStandard;
    bool oldWorldStart = true;

    // If OldWorldStart == true, setting this to false will force all minor civs to start
    // in the old world also. Setting this to true will spread minor civs proportional to 
    // the land mass of the old and new worlds. If OldWorldStart == false, then this does nothing
    bool proportionalMinors = true;
    // This is the minimum contiguous passable non water landmass that can be considered
    // a major civ capital. Full 3 radius city area could have 37 tiles maximum
    int32 realEstateMin = 15;
};

struct Dim
{
    uint16 w;
    uint16 h;
};

void LoadDefaultSettings(char const* settingsFile);
void GenerateMap(Dim dim);
