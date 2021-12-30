
#include "MapData.h"

#include "sqlite3.h"

#include "stdio.h"
#include "string.h"
#include "assert.h"

#define MAX_FILENAME_LEN 100
#define EXTENSION_LEN 9
static char const extension[EXTENSION_LEN] = ".Civ6Map";

#define STATEMENT_BUF_SIZE 1024


// Util

static inline bool HandleSQLError(int32 error, sqlite3* db)
{
    if (error != SQLITE_OK)
    {
        printf("SQL error: %s\n", sqlite3_errmsg(db));
        assert(0);
        sqlite3_close(db);
        return true;
    }

    return false;
}


// Sample DB for reference

static void CreateSampleEmptyMap()
{
    sqlite3* mapDB = nullptr;

    int error = sqlite3_open("sample.Civ6Map", &mapDB);
    if (HandleSQLError(error, mapDB))
        return;

    printf("Success!\n");

    {
        char const* query =
            "DROP TABLE IF EXISTS Buildings;"
            "CREATE TABLE Buildings ( "
            "BuildingType TEXT NOT NULL, "
            "Plot INTEGER NOT NULL, "
            "PRIMARY KEY(BuildingType, Plot)"
            ");"
            "DROP TABLE IF EXISTS Cities;"
            "CREATE TABLE Cities ( "
            "Owner INTEGER NOT NULL, -- The owner ID in the database, not the game \n"
            "Plot INTEGER NOT NULL, "
            "Name TEXT, "
            "PRIMARY KEY(Plot)"
            ");"
            "DROP TABLE IF EXISTS CityAttributes;"
            "CREATE TABLE CityAttributes ( "
            "ID INTEGER NOT NULL, "
            "Type TEXT NOT NULL, "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(ID, Type, Name)"
            ");"
            "DROP TABLE IF EXISTS DistrictAttributes;"
            "CREATE TABLE DistrictAttributes ( "
            "ID INTEGER NOT NULL, "
            "Type TEXT NOT NULL, "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(ID, Type, Name)"
            ");"
            "DROP TABLE IF EXISTS Districts;"
            "CREATE TABLE Districts ( "
            "DistrictType TEXT NOT NULL, "
            "CityID INTEGER NOT NULL, -- The city ID in the database, not the game \n"
            "Plot INTEGER NOT NULL, "
            "PRIMARY KEY(Plot)"
            ");"
            "DROP TABLE IF EXISTS GameConfig;"
            "CREATE TABLE GameConfig ( "
            "ID TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS Map;"
            "CREATE TABLE Map ( "
            "ID TEXT NOT NULL, "
            "Width INTEGER, "
            "Height INTEGER, "
            "TopLatitude INTEGER, "
            "BottomLatitude INTEGER, "
            "WrapX BOOLEAN, "
            "WrapY BOOLEAN, "
            "MapSizeType TEXT, "
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS MapAttributes;"
            "CREATE TABLE MapAttributes ( "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(Name)"
            ");"
            "DROP TABLE IF EXISTS MetaData;"
            "CREATE TABLE MetaData ( "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(Name)"
            ");"
            "DROP TABLE IF EXISTS ModComponent_Items;"
            "CREATE TABLE ModComponent_Items ( "
            "Type TEXT NOT NULL, "
            "ID TEXT NOT NULL, "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(Type, ID, Name, Value)"
            ");"
            "DROP TABLE IF EXISTS ModComponent_Properties;"
            "CREATE TABLE ModComponent_Properties ( "
            "Type TEXT NOT NULL, "
            "ID TEXT NOT NULL, "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(Type, ID, Name, Value)"
            ");"
            "DROP TABLE IF EXISTS ModComponents;"
            "CREATE TABLE ModComponents ( "
            "Type TEXT NOT NULL, "
            "ID TEXT NOT NULL, "
            "PRIMARY KEY(Type, ID)"
            ");"
            "DROP TABLE IF EXISTS ModDependencies;"
            "CREATE TABLE ModDependencies ( "
            "ID TEXT NOT NULL, "
            "Title TEXT NOT NULL, "
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS ModProperties;"
            "CREATE TABLE ModProperties ( "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(Name, Value)"
            ");"
            "DROP TABLE IF EXISTS ModSettings;"
            "CREATE TABLE ModSettings ( "
            "Type TEXT NOT NULL, "
            "ID TEXT NOT NULL, "
            "PRIMARY KEY(Type, ID)"
            ");"
            "DROP TABLE IF EXISTS ModSettings_Items;"
            "CREATE TABLE ModSettings_Items ( "
            "Type TEXT NOT NULL, "
            "ID TEXT NOT NULL, "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(Type, ID, Name, Value)"
            ");"
            "DROP TABLE IF EXISTS ModSettings_Properties;"
            "CREATE TABLE ModSettings_Properties ( "
            "Type TEXT NOT NULL, "
            "ID TEXT NOT NULL, "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(Type, ID, Name, Value)"
            ");"
            "DROP TABLE IF EXISTS ModText;"
            "CREATE TABLE ModText ( "
            "Language TEXT NOT NULL, "
            "ID TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(Language, ID)"
            ");"
            "DROP TABLE IF EXISTS PlayerAttributes;"
            "CREATE TABLE PlayerAttributes ( "
            "ID INTEGER NOT NULL, "
            "Type TEXT NOT NULL, "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(ID, Type, Name)"
            ");"
            "DROP TABLE IF EXISTS Players;"
            "CREATE TABLE Players ( "
            "ID INTEGER NOT NULL, "
            "CivilizationType TEXT, "
            "LeaderType TEXT, "
            "CivilizationLevelType TEXT, "
            "AgendaType TEXT, "
            "Status TEXT, "
            "Handicap TEXT, "
            "StartingPosition TEXT, "
            "Color TEXT, "
            "Initialized BOOLEAN, "
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS PlotAttributes;"
            "CREATE TABLE PlotAttributes ( "
            "ID INTEGER NOT NULL, "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(ID, Name)"
            ");"
            "DROP TABLE IF EXISTS PlotCliffs;"
            "CREATE TABLE PlotCliffs ( "
            "ID INTEGER NOT NULL, "
            "IsNEOfCliff BOOLEAN, "
            "IsWOfCliff BOOLEAN, "
            "IsNWOfCliff BOOLEAN, "
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS PlotFeatures;"
            "CREATE TABLE PlotFeatures ( "
            "ID INTEGER NOT NULL, "
            "FeatureType TEXT, "
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS PlotImprovements;"
            "CREATE TABLE PlotImprovements ( "
            "ID INTEGER NOT NULL, "
            "ImprovementType TEXT, "
            "ImprovementOwner INTEGER, -- The owner ID in the database, not the game \n"
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS PlotOwners;"
            "CREATE TABLE PlotOwners ( "
            "ID INTEGER NOT NULL, "
            "Owner INTEGER, -- The owner ID in the database, not the game \n"
            "CityOwner INTEGER, -- The city ID in the database, not the game \n"
            "CityWorking INTEGER, -- The city ID in the database, not the game \n"
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS PlotResources;"
            "CREATE TABLE PlotResources ( "
            "ID INTEGER NOT NULL, "
            "ResourceType TEXT, "
            "ResourceCount INTEGER, "
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS PlotRivers;"
            "CREATE TABLE PlotRivers ( "
            "ID INTEGER NOT NULL, "
            "IsNEOfRiver BOOLEAN, "
            "IsWOfRiver BOOLEAN, "
            "IsNWOfRiver BOOLEAN, "
            "EFlowDirection INTEGER, "
            "SEFlowDirection INTEGER, "
            "SWFlowDirection INTEGER, "
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS PlotRoutes;"
            "CREATE TABLE PlotRoutes ( "
            "ID INTEGER NOT NULL, "
            "RouteType TEXT, "
            "RouteEra TEXT, "
            "IsRoutePillaged BOOLEAN, "
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS Plots;"
            "CREATE TABLE Plots ( "
            "ID INTEGER NOT NULL, "
            "TerrainType TEXT NOT NULL, "
            "ContinentType TEXT, "
            "IsImpassable BOOLEAN, "
            "PRIMARY KEY(ID)"
            ");"
            "DROP TABLE IF EXISTS RevealedPlots;"
            "CREATE TABLE RevealedPlots ( "
            "ID INTEGER NOT NULL, "
            "Player INTEGER, "
            "PRIMARY KEY(ID, Player)"
            ");"
            "DROP TABLE IF EXISTS StartPositions;"
            "CREATE TABLE StartPositions ( "
            "Plot INTEGER NOT NULL, "
            "Type STRING NOT NULL, "
            "Value STRING NOT NULL, "
            "PRIMARY KEY(Plot)"
            ");"
            "DROP TABLE IF EXISTS UnitAttributes;"
            "CREATE TABLE UnitAttributes ( "
            "ID INTEGER NOT NULL, "
            "Type TEXT NOT NULL, "
            "Name TEXT NOT NULL, "
            "Value TEXT, "
            "PRIMARY KEY(ID, Type, Name)"
            ");"
            "DROP TABLE IF EXISTS Units;"
            "CREATE TABLE Units ( "
            "ID INTEGER NOT NULL, "
            "UnitType TEXT NOT NULL, "
            "Owner INTEGER NOT NULL, -- The owner ID in the database, not the game \n"
            "Plot INTEGER NOT NULL, "
            "Name TEXT, "
            "PRIMARY KEY(ID)"
            ");"
            ;

        char* err_msg;
        error = sqlite3_exec(mapDB, query, NULL, NULL, &err_msg);
        if (HandleSQLError(error, mapDB))
            return;

        printf("Tables created!");
    }

    sqlite3_close(mapDB);
}


// DB management

static sqlite3* OpenMap(char const* name)
{
    uint32 len = strlen(name);
    if (len > MAX_FILENAME_LEN - EXTENSION_LEN)
    {
        printf("Output map name '%s' too long!\n", name);
        return NULL;
    }

    char filename[MAX_FILENAME_LEN];
    strncpy(filename, name, len);
    // extension includes nul terminator
    strncpy(filename + len, extension, EXTENSION_LEN);

    sqlite3* mapDB = nullptr;
    int32 error = sqlite3_open(filename, &mapDB);
    if (HandleSQLError(error, mapDB))
        return NULL;

    printf("%s successfully opened!\n", filename);

    return mapDB;
}

static void CloseMap(sqlite3* mapDB)
{
    sqlite3_close(mapDB);
}


// Table filling

static void AddInUnusedTables(sqlite3* mapDB)
{
    char const* query =
        "DROP TABLE IF EXISTS Buildings;"
        "CREATE TABLE Buildings ( "
        "BuildingType TEXT NOT NULL, "
        "Plot INTEGER NOT NULL, "
        "PRIMARY KEY(BuildingType, Plot)"
        ");"
        "DROP TABLE IF EXISTS Cities;"
        "CREATE TABLE Cities ( "
        "Owner INTEGER NOT NULL, -- The owner ID in the database, not the game \n"
        "Plot INTEGER NOT NULL, "
        "Name TEXT, "
        "PRIMARY KEY(Plot)"
        ");"
        "DROP TABLE IF EXISTS CityAttributes;"
        "CREATE TABLE CityAttributes ( "
        "ID INTEGER NOT NULL, "
        "Type TEXT NOT NULL, "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(ID, Type, Name)"
        ");"
        "DROP TABLE IF EXISTS DistrictAttributes;"
        "CREATE TABLE DistrictAttributes ( "
        "ID INTEGER NOT NULL, "
        "Type TEXT NOT NULL, "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(ID, Type, Name)"
        ");"
        "DROP TABLE IF EXISTS Districts;"
        "CREATE TABLE Districts ( "
        "DistrictType TEXT NOT NULL, "
        "CityID INTEGER NOT NULL, -- The city ID in the database, not the game \n"
        "Plot INTEGER NOT NULL, "
        "PRIMARY KEY(Plot)"
        ");"
        "DROP TABLE IF EXISTS GameConfig;"
        "CREATE TABLE GameConfig ( "
        "ID TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(ID)"
        ");"
        // Map, MapAttributes, and MetaData need to be filled
        "DROP TABLE IF EXISTS ModComponent_Items;"
        "CREATE TABLE ModComponent_Items ( "
        "Type TEXT NOT NULL, "
        "ID TEXT NOT NULL, "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(Type, ID, Name, Value)"
        ");"
        "DROP TABLE IF EXISTS ModComponent_Properties;"
        "CREATE TABLE ModComponent_Properties ( "
        "Type TEXT NOT NULL, "
        "ID TEXT NOT NULL, "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(Type, ID, Name, Value)"
        ");"
        "DROP TABLE IF EXISTS ModComponents;"
        "CREATE TABLE ModComponents ( "
        "Type TEXT NOT NULL, "
        "ID TEXT NOT NULL, "
        "PRIMARY KEY(Type, ID)"
        ");"
        "DROP TABLE IF EXISTS ModDependencies;"
        "CREATE TABLE ModDependencies ( "
        "ID TEXT NOT NULL, "
        "Title TEXT NOT NULL, "
        "PRIMARY KEY(ID)"
        ");"
        "DROP TABLE IF EXISTS ModProperties;"
        "CREATE TABLE ModProperties ( "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(Name, Value)"
        ");"
        "DROP TABLE IF EXISTS ModSettings;"
        "CREATE TABLE ModSettings ( "
        "Type TEXT NOT NULL, "
        "ID TEXT NOT NULL, "
        "PRIMARY KEY(Type, ID)"
        ");"
        "DROP TABLE IF EXISTS ModSettings_Items;"
        "CREATE TABLE ModSettings_Items ( "
        "Type TEXT NOT NULL, "
        "ID TEXT NOT NULL, "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(Type, ID, Name, Value)"
        ");"
        "DROP TABLE IF EXISTS ModSettings_Properties;"
        "CREATE TABLE ModSettings_Properties ( "
        "Type TEXT NOT NULL, "
        "ID TEXT NOT NULL, "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(Type, ID, Name, Value)"
        ");"
        "DROP TABLE IF EXISTS ModText;"
        "CREATE TABLE ModText ( "
        "Language TEXT NOT NULL, "
        "ID TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(Language, ID)"
        ");"
        "DROP TABLE IF EXISTS PlayerAttributes;"
        "CREATE TABLE PlayerAttributes ( "
        "ID INTEGER NOT NULL, "
        "Type TEXT NOT NULL, "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(ID, Type, Name)"
        ");"
        "DROP TABLE IF EXISTS Players;"
        "CREATE TABLE Players ( "
        "ID INTEGER NOT NULL, "
        "CivilizationType TEXT, "
        "LeaderType TEXT, "
        "CivilizationLevelType TEXT, "
        "AgendaType TEXT, "
        "Status TEXT, "
        "Handicap TEXT, "
        "StartingPosition TEXT, "
        "Color TEXT, "
        "Initialized BOOLEAN, "
        "PRIMARY KEY(ID)"
        ");"
        "DROP TABLE IF EXISTS PlotAttributes;"
        "CREATE TABLE PlotAttributes ( "
        "ID INTEGER NOT NULL, "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(ID, Name)"
        ");"
        // PlotCliffs and PlotFeatures need to be filled
        "DROP TABLE IF EXISTS PlotImprovements;"
        "CREATE TABLE PlotImprovements ( "
        "ID INTEGER NOT NULL, "
        "ImprovementType TEXT, "
        "ImprovementOwner INTEGER, -- The owner ID in the database, not the game \n"
        "PRIMARY KEY(ID)"
        ");"
        "DROP TABLE IF EXISTS PlotOwners;"
        "CREATE TABLE PlotOwners ( "
        "ID INTEGER NOT NULL, "
        "Owner INTEGER, -- The owner ID in the database, not the game \n"
        "CityOwner INTEGER, -- The city ID in the database, not the game \n"
        "CityWorking INTEGER, -- The city ID in the database, not the game \n"
        "PRIMARY KEY(ID)"
        ");"
        // PlotResources and PlotRivers need to be filled
        "DROP TABLE IF EXISTS PlotRoutes;"
        "CREATE TABLE PlotRoutes ( "
        "ID INTEGER NOT NULL, "
        "RouteType TEXT, "
        "RouteEra TEXT, "
        "IsRoutePillaged BOOLEAN, "
        "PRIMARY KEY(ID)"
        ");"
        // Plots needs to be filled
        "DROP TABLE IF EXISTS RevealedPlots;"
        "CREATE TABLE RevealedPlots ( "
        "ID INTEGER NOT NULL, "
        "Player INTEGER, "
        "PRIMARY KEY(ID, Player)"
        ");"
        "DROP TABLE IF EXISTS StartPositions;"
        "CREATE TABLE StartPositions ( "
        "Plot INTEGER NOT NULL, "
        "Type STRING NOT NULL, "
        "Value STRING NOT NULL, "
        "PRIMARY KEY(Plot)"
        ");"
        "DROP TABLE IF EXISTS UnitAttributes;"
        "CREATE TABLE UnitAttributes ( "
        "ID INTEGER NOT NULL, "
        "Type TEXT NOT NULL, "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(ID, Type, Name)"
        ");"
        "DROP TABLE IF EXISTS Units;"
        "CREATE TABLE Units ( "
        "ID INTEGER NOT NULL, "
        "UnitType TEXT NOT NULL, "
        "Owner INTEGER NOT NULL, -- The owner ID in the database, not the game \n"
        "Plot INTEGER NOT NULL, "
        "Name TEXT, "
        "PRIMARY KEY(ID)"
        ");"
        ;

    char* err_msg;
    int32 error = sqlite3_exec(mapDB, query, NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;

    printf("Unused tables added in.\n");
}

static void AddMapDetails(sqlite3* mapDB, MapDetails* details)
{
#if 1
    char const* query =
        "DROP TABLE IF EXISTS Map;"
        "CREATE TABLE Map ( "
        "ID TEXT NOT NULL, "
        "Width INTEGER, "
        "Height INTEGER, "
        "TopLatitude INTEGER, "
        "BottomLatitude INTEGER, "
        "WrapX BOOLEAN, "
        "WrapY BOOLEAN, "
        "MapSizeType TEXT, "
        "PRIMARY KEY(ID)"
        ");"
        ;
#else
    // just wipe rows instead of recreating
    char const* query =
        "DELETE FROM Map;"
        ;
#endif

    char* err_msg;
    int32 error = sqlite3_exec(mapDB, query, NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;

    printf("Map table created!\n");


    // Add in the settings data

    char statement[STATEMENT_BUF_SIZE];
    sprintf(statement, "INSERT INTO Map VALUES (?, ?, ?, ?, ?, ?, ?, ?);");

    sqlite3_stmt* stmt;
    error = sqlite3_prepare_v2(mapDB, statement, STATEMENT_BUF_SIZE, &stmt, NULL);
    if (HandleSQLError(error, mapDB))
        return;


    error = sqlite3_exec(mapDB, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;
    {
        sqlite3_bind_text(stmt, 1, "DEFAULT", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, details->width);
        sqlite3_bind_int(stmt, 3, details->height);
        sqlite3_bind_int(stmt, 4, details->topLatitude);
        sqlite3_bind_int(stmt, 5, details->bottomLatitude);
        sqlite3_bind_int(stmt, 6, details->wrapX ? 1 : 0);
        sqlite3_bind_int(stmt, 7, details->wrapY ? 1 : 0);
        sqlite3_bind_text(stmt, 8, mapSizeStrs[details->size], -1, SQLITE_STATIC);

        error = sqlite3_step(stmt);
        if (error != SQLITE_DONE)
            assert(0);

        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }
    error = sqlite3_exec(mapDB, "END TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_finalize(stmt);
    if (HandleSQLError(error, mapDB))
        return;

    printf("   Map row added!\n");
}

static void AddMapAttributes(sqlite3* mapDB)
{
    char const* query =
        "DROP TABLE IF EXISTS MapAttributes;"
        "CREATE TABLE MapAttributes ( "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(Name)"
        ");"
        ;

    char* err_msg;
    int32 error = sqlite3_exec(mapDB, query, NULL, NULL, &err_msg);
    if (error != SQLITE_OK)
    {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        assert(0);
        return;
    }

    printf("MapAttributes table created!\n");


    // Add in the attributes

    char statement[STATEMENT_BUF_SIZE];
    sprintf(statement, "INSERT INTO MapAttributes VALUES (?, ?);");

    sqlite3_stmt* stmt;
    error = sqlite3_prepare_v2(mapDB, statement, STATEMENT_BUF_SIZE, &stmt, NULL);
    if (HandleSQLError(error, mapDB))
        return;


    error = sqlite3_exec(mapDB, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;
    {
        sqlite3_bind_text(stmt, 1, "MapScript", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, MAP_SCRIPT, -1, SQLITE_STATIC);

        error = sqlite3_step(stmt);
        if (error != SQLITE_DONE)
            assert(0);

        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);

        sqlite3_bind_text(stmt, 1, "Ruleset", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, RULESET, -1, SQLITE_STATIC);

        error = sqlite3_step(stmt);
        if (error != SQLITE_DONE)
            assert(0);

        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }
    error = sqlite3_exec(mapDB, "END TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_finalize(stmt);
    if (HandleSQLError(error, mapDB))
        return;

    printf("   MapAttributes rows added!\n");
}

static void AddMetaData(sqlite3* mapDB)
{
    char const* query =
        "DROP TABLE IF EXISTS MetaData;"
        "CREATE TABLE MetaData ( "
        "Name TEXT NOT NULL, "
        "Value TEXT, "
        "PRIMARY KEY(Name)"
        ");"
        ;

    char* err_msg;
    int32 error = sqlite3_exec(mapDB, query, NULL, NULL, &err_msg);
    if (error != SQLITE_OK)
    {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        assert(0);
        return;
    }

    printf("MetaData table created!\n");


    // Add in the metadata

    char const* rowNames[] =
    {
        "MetaDataVersion",
        "AppVersion",
        "DisplayName",
        "ID",
        "IsMod",
    };
    static uint32 const len = sizeof rowNames / sizeof * rowNames;

    char const* rowValues[] =
    {
        METADATA_VERSION,
        APP_VERSION,
        DISPLAY_NAME,
        METADATA_ID,
        IS_MOD,
    };

    char statement[STATEMENT_BUF_SIZE];
    sprintf(statement, "INSERT INTO MetaData VALUES (?, ?);");

    sqlite3_stmt* stmt;
    error = sqlite3_prepare_v2(mapDB, statement, STATEMENT_BUF_SIZE, &stmt, NULL);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_exec(mapDB, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;
    for (uint32 i = 0; i < len; ++i)
    {
        sqlite3_bind_text(stmt, 1, rowNames[i], -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, rowValues[i], -1, SQLITE_STATIC);

        error = sqlite3_step(stmt);
        if (error != SQLITE_DONE)
            assert(0);

        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }
    error = sqlite3_exec(mapDB, "END TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_finalize(stmt);
    if (HandleSQLError(error, mapDB))
        return;

    printf("   MetaData rows added!\n");
}

static void AddCliffs(sqlite3* mapDB, MapTile* start, MapTile* end)
{
    char const* query =
        "DROP TABLE IF EXISTS PlotCliffs;"
        "CREATE TABLE PlotCliffs ( "
        "ID INTEGER NOT NULL, "
        "IsNEOfCliff BOOLEAN, "
        "IsWOfCliff BOOLEAN, "
        "IsNWOfCliff BOOLEAN, "
        "PRIMARY KEY(ID)"
        ");"
        ;

    char* err_msg;
    int32 error = sqlite3_exec(mapDB, query, NULL, NULL, &err_msg);
    if (error != SQLITE_OK)
    {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        assert(0);
        return;
    }

    printf("PlotCliffs table created!\n");


    // Add in cliffs

    char statement[STATEMENT_BUF_SIZE];
    sprintf(statement, "INSERT INTO PlotCliffs VALUES (?, ?, ?, ?);");

    sqlite3_stmt* stmt;
    error = sqlite3_prepare_v2(mapDB, statement, STATEMENT_BUF_SIZE, &stmt, NULL);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_exec(mapDB, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;
    uint32 id = 0;
    uint32 cnt = 0;
    for (MapTile* plot = start; plot < end; ++plot, ++id)
    {
        if (!plot->isWOfCliff && !plot->isNWOfCliff && !plot->isNEOfCliff)
            continue;

        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_int(stmt, 2, plot->isNEOfCliff);
        sqlite3_bind_int(stmt, 3, plot->isWOfCliff);
        sqlite3_bind_int(stmt, 4, plot->isNWOfCliff);

        error = sqlite3_step(stmt);
        if (error != SQLITE_DONE)
            assert(0);

        ++cnt;
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }
    error = sqlite3_exec(mapDB, "END TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_finalize(stmt);
    if (HandleSQLError(error, mapDB))
        return;

    printf("   %d PlotCliffs rows added!\n", cnt);
}

static void AddFeatures(sqlite3* mapDB, MapTile* start, MapTile* end)
{
    char const* query =
        "DROP TABLE IF EXISTS PlotFeatures;"
        "CREATE TABLE PlotFeatures ( "
        "ID INTEGER NOT NULL, "
        "FeatureType TEXT, "
        "PRIMARY KEY(ID)"
        ");"
        ;

    char* err_msg;
    int32 error = sqlite3_exec(mapDB, query, NULL, NULL, &err_msg);
    if (error != SQLITE_OK)
    {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        assert(0);
        return;
    }

    printf("PlotFeatures table created!\n");


    // Add in Features

    char statement[STATEMENT_BUF_SIZE];
    sprintf(statement, "INSERT INTO PlotFeatures VALUES (?, ?);");

    sqlite3_stmt* stmt;
    error = sqlite3_prepare_v2(mapDB, statement, STATEMENT_BUF_SIZE, &stmt, NULL);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_exec(mapDB, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;
    uint32 id = 0;
    uint32 cnt = 0;
    for (MapTile* plot = start; plot < end; ++plot, ++id)
    {
        if (plot->feature == fNONE)
            continue;

        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, featureStrs[plot->feature], -1, SQLITE_STATIC);

        error = sqlite3_step(stmt);
        if (error != SQLITE_DONE)
            assert(0);

        ++cnt;
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }
    error = sqlite3_exec(mapDB, "END TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_finalize(stmt);
    if (HandleSQLError(error, mapDB))
        return;

    printf("   %d PlotFeatures rows added!\n", cnt);
}

static void AddResources(sqlite3* mapDB, MapTile* start, MapTile* end)
{
    char const* query =
        "DROP TABLE IF EXISTS PlotResources;"
        "CREATE TABLE PlotResources ( "
        "ID INTEGER NOT NULL, "
        "ResourceType TEXT, "
        "ResourceCount INTEGER, "
        "PRIMARY KEY(ID)"
        ");"
        ;

    char* err_msg;
    int32 error = sqlite3_exec(mapDB, query, NULL, NULL, &err_msg);
    if (error != SQLITE_OK)
    {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        assert(0);
        return;
    }

    printf("PlotResources table created!\n");


    // Add in Resources

    char statement[STATEMENT_BUF_SIZE];
    sprintf(statement, "INSERT INTO PlotResources VALUES (?, ?, 1);");

    sqlite3_stmt* stmt;
    error = sqlite3_prepare_v2(mapDB, statement, STATEMENT_BUF_SIZE, &stmt, NULL);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_exec(mapDB, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;
    uint32 id = 0;
    uint32 cnt = 0;
    for (MapTile* plot = start; plot < end; ++plot, ++id)
    {
        if (plot->resource == rNONE)
            continue;

        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, resourceStrs[plot->resource], -1, SQLITE_STATIC);

        error = sqlite3_step(stmt);
        if (error != SQLITE_DONE)
            assert(0);

        ++cnt;
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }
    error = sqlite3_exec(mapDB, "END TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_finalize(stmt);
    if (HandleSQLError(error, mapDB))
        return;

    printf("   %d PlotResources rows added!\n", cnt);
}

static void AddRivers(sqlite3* mapDB, MapTile* start, MapTile* end)
{
    char const* query =
        "DROP TABLE IF EXISTS PlotRivers;"
        "CREATE TABLE PlotRivers ( "
        "ID INTEGER NOT NULL, "
        "IsNEOfRiver BOOLEAN, "
        "IsWOfRiver BOOLEAN, "
        "IsNWOfRiver BOOLEAN, "
        "EFlowDirection INTEGER, "
        "SEFlowDirection INTEGER, "
        "SWFlowDirection INTEGER, "
        "PRIMARY KEY(ID)"
        ");"
        ;

    char* err_msg;
    int32 error = sqlite3_exec(mapDB, query, NULL, NULL, &err_msg);
    if (error != SQLITE_OK)
    {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        assert(0);
        return;
    }

    printf("PlotRivers table created!\n");


    // Add in Rivers

    char statement[STATEMENT_BUF_SIZE];
    sprintf(statement, "INSERT INTO PlotRivers VALUES (?, ?, ?, ?, ?, ?, ?);");

    sqlite3_stmt* stmt;
    error = sqlite3_prepare_v2(mapDB, statement, STATEMENT_BUF_SIZE, &stmt, NULL);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_exec(mapDB, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;
    uint32 id = 0;
    uint32 cnt = 0;
    for (MapTile* plot = start; plot < end; ++plot, ++id)
    {
        if (!plot->isWOfRiver && !plot->isNWOfRiver && !plot->isNEOfRiver)
            continue;

        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_int(stmt, 2, plot->isNEOfRiver);
        sqlite3_bind_int(stmt, 3, plot->isWOfRiver);
        sqlite3_bind_int(stmt, 4, plot->isNWOfRiver);
        sqlite3_bind_int(stmt, 5, (int32)plot->flowDirE - 1);
        sqlite3_bind_int(stmt, 6, (int32)plot->flowDirSE - 1);
        sqlite3_bind_int(stmt, 7, (int32)plot->flowDirSW - 1);

        error = sqlite3_step(stmt);
        if (error != SQLITE_DONE)
            assert(0);

        ++cnt;
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }
    error = sqlite3_exec(mapDB, "END TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_finalize(stmt);
    if (HandleSQLError(error, mapDB))
        return;

    printf("   %d PlotRivers rows added!\n", cnt);
}

static void AddTerrain(sqlite3* mapDB, MapTile* start, MapTile* end)
{
    char const* query =
        "DROP TABLE IF EXISTS Plots;"
        "CREATE TABLE Plots ( "
        "ID INTEGER NOT NULL, "
        "TerrainType TEXT NOT NULL, "
        "ContinentType TEXT, "
        "IsImpassable BOOLEAN, "
        "PRIMARY KEY(ID)"
        ");"
        ;

    char* err_msg;
    int32 error = sqlite3_exec(mapDB, query, NULL, NULL, &err_msg);
    if (error != SQLITE_OK)
    {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        assert(0);
        return;
    }

    printf("Plots table created!\n");


    // Add in Terrain

    char statement[STATEMENT_BUF_SIZE];
    sprintf(statement, "INSERT INTO Plots VALUES (?, ?, ?, ?);");

    sqlite3_stmt* stmt;
    error = sqlite3_prepare_v2(mapDB, statement, STATEMENT_BUF_SIZE, &stmt, NULL);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_exec(mapDB, "BEGIN TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;
    uint32 id = 0;
    uint32 cnt = 0;
    for (MapTile* plot = start; plot < end; ++plot, ++id)
    {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, terrainStrs[plot->terrain], -1, SQLITE_STATIC);
        // TODO: add in continent naming
        sqlite3_bind_text(stmt, 3, "", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, plot->isImpassable);

        error = sqlite3_step(stmt);
        if (error != SQLITE_DONE)
            assert(0);

        ++cnt;
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }
    error = sqlite3_exec(mapDB, "END TRANSACTION", NULL, NULL, &err_msg);
    if (HandleSQLError(error, mapDB))
        return;

    error = sqlite3_finalize(stmt);
    if (HandleSQLError(error, mapDB))
        return;

    printf("   %d Plots rows added!\n", cnt);
}


// Interface

void SaveToCiv6Map(char const* name, MapDetails* details, MapTile* map)
{
    uint32 len = details->width * details->height;
    MapTile* end = map + len;

    sqlite3* mapDB = OpenMap(name);

    AddInUnusedTables(mapDB);
    AddMapDetails(mapDB, details);
    AddMapAttributes(mapDB);
    AddMetaData(mapDB);
    AddCliffs(mapDB, map, end);
    AddFeatures(mapDB, map, end);
    AddResources(mapDB, map, end);
    AddRivers(mapDB, map, end);
    AddTerrain(mapDB, map, end);

    CloseMap(mapDB);
}
