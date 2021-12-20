#pragma once

#include "typedefs.h"

enum JungleCoversion
{
    jcNone,      // Leave them as grassland
    jcHillsOnly, // Change only the hills to plains
    jcAll,       // Change all jungle tiles to have underlying plains
};

struct PW6Settings
{
    // Percent of land tiles on the map.
    float32 landPercent = 0.28f;
    // Percent of dry land that is below the hill elevation deviance threshold.
    float32 hillsPercent = 0.54f;
    // Percent of dry land that is below the mountain elevation deviance threshold.
    float32 mountainsPercent = 0.86f;
    // Percent of land that is below the desert rainfall threshold.
    float32 desertPercent = 0.37f;
    // Coldest absolute temperature allowed to be desert, plains if colder.
    float32 desertMinTemperature = 0.34f;
    // Percent of land that is below the plains rainfall threshold.
    float32 plainsPercent = 0.64f;

    // Percent of land that is below the rainfall threshold where no trees can appear.
    float32 zeroTreesPercent = 0.30f;
    // Coldest absolute temperature where trees appear.
    float32 treesMinTemperature = 0.27f;

    // Percent of land below the jungle rainfall threshold.
    float32 junglePercent = 0.78f;
    // Coldest absolute temperature allowed to be jungle, forest if colder.
    float32 jungleMinTemperature = 0.70f;

    // Percent of land below the marsh rainfall threshold.
    float32 marshPercent = 0.92f;

    // Absolute temperature below which is snow.
    float32 snowTemperature = 0.27f;

    // Absolute temperature below which is tundra.
    float32 tundraTemperature = 0.32f;

    // North and south ice latitude limits. Used for pre-GS
    int32 iceNorthLatitudeLimit = 60;
    int32 iceSouthLatitudeLimit = -60;

    // Percent of land tiles made to be lakes
    float32 lakePercent = 0.04f;

    // Percent of river junctions that are large enough to become rivers.
    float32 riverPercent = 0.55f;

    // Minumum river length measured in hex sides. Shorter rivers that are not lake outflows will be culled.
    uint32 minRiverLength = 5;

    // This is the percent of rivers that have floodplains that flood largest rivers always have priority for floodability
    float32 percentRiversFloodplains = 0.25f;

    // Maximum chance for reef at highest temperature
    float32 maxReefChance = 0.15f;

    // normally, jungle has plains underlying them. I personally don't like it because I want 
    // jungles to look wet, and I like that it makes settling in jungle more challenging, 
    // however, it's a subjective thing. Here is an option to change it back.
    JungleCoversion JungleToPlains = jcAll;

    // These attenuation factors lower the altitude of the map edges. This is currently
    // used to prevent large continents in the uninhabitable polar regions.
    float32 northAttenuationFactor = 0.75f;
    float32 northAttenuationRange = 0.15f;  // Percent of the map height
    float32 southAttenuationFactor = 0.75f;
    float32 southAttenuationRange = 0.15f;

    // East west attenuation. Civ 6 creates a rather ugly seam when continents
    // straddle the map edge. It still plays well but I have decided to avoid
    // the map edges for aesthetic reasons.
    float32 eastAttenuationFactor = 0.75f;
    float32 eastAttenuationRange = 0.10f;  // Percent of the map height
    float32 westAttenuationFactor = 0.75f;
    float32 westAttenuationRange = 0.10f;

    // These set the water temperature compression that creates the land/sea
    // seasonal temperature differences that cause monsoon winds.
    float32 minWaterTemp = 0.10f;
    float32 maxWaterTemp = 0.60f;

    // Top and bottom map latitudes.
    int32 topLatitude = 70;
    int32 bottomLatitude = -70;

    // Important latitude markers used for generating climate.
    int32 polarFrontLatitude = 60;
    int32 tropicLatitudes = 23;
    int32 horseLatitudes = 28; // I shrunk these a bit to emphasize temperate lattitudes

    // Strength of geostrophic climate generation versus monsoon climate generation.
    float32 geostrophicFactor = 3.0f;

    float32 geostrophicLateralWindStrength = 0.6f;

    // Fill in any lakes smaller than this. It looks bad to have large
    // river systems flowing into a tiny lake.
    uint32 minOceanSize = 50;

    // Weight of the mountain elevation map versus the coastline elevation map.
    float32 mountainWeight = 0.8f;

    // Crazy rain tweaking variables. I wouldn't touch these if I were you.
    float32 minimumRainCost = 0.0001f;
    int32 upLiftExponent = 4;
    float32 polarRainBoost = 0.00f;

    // Default frequencies for map of width 128. Adjusting these frequences
    // will generate larger or smaller map features.
    float32 twistMinFreq = 0.05f;
    float32 twistMaxFreq = 0.12f;
    float32 twistVar = 0.042f;
    float32 mountainFreq = 0.078f;

    bool AllowPangeas = false;
    // A continent with more land tiles than this percentage of total landtiles is
    // considered a pangaea and broken up if allowed.
    float32 PangaeaSize = 0.70f;
    // Maximum percentage of land tiles that will be designated as new world continents.
    float32 maxNewWorldSize = 0.35f;

    bool OldWorldStart = true; //MapConfiguration.GetValue("oldworld") == "OLD_WORLD"
    // If OldWorldStart == true, setting this to false will force all minor civs to start
    // in the old world also. Setting this to true will spread minor civs proportional to 
    // the land mass of the old and new worlds. If OldWorldStart == false, then this does nothing
    bool ProportionalMinors = true;
    // This is the minimum contiguous passable non water landmass that can be considered
    // a major civ capital. Full 3 radius city area could have 37 tiles maximum
    int32 realEstateMin = 15;
    // PerfectWorld maps are a bit bigger that normal maps, and it may be appropriate to
    // have slightly more natural wonders. This variable sets how many extra wonders are set
    // according to map size. I'm told that 'less is more' when it comes to natural wonders,
    // and I interpret that as a little more is maybe more than less.
    int32 naturalWonderExtra = 1;
};


