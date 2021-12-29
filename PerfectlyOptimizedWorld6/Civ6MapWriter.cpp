
#include "MapData.h"

#include "sqlite3.h"

#include "stdio.h"
#include "string.h"
#include "assert.h"

#define MAX_FILENAME_LEN 100
#define EXTENSION_LEN 9
static char const extension[EXTENSION_LEN] = ".Civ6Map";

// Sample DB for reference

static void CreateSampleEmptyMap()
{
	sqlite3* map = nullptr;

	int error = sqlite3_open("sample.Civ6Map", &map);
	if (error != SQLITE_OK)
	{
		printf("ERROR: can't open database: %s\n", sqlite3_errmsg(map));
		assert(0);
	}

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
		error = sqlite3_exec(map, query, NULL, NULL, &err_msg);
		if (error != SQLITE_OK)
		{
			printf("SQL error: %s\n", err_msg);
			sqlite3_free(err_msg);
			assert(0);
		}
		else
		{
			printf("Tables created!");
		}
	}

	sqlite3_close(map);
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
	char * str = strncpy(filename, name, len);
	str = strncpy(str, extension, EXTENSION_LEN);
	*str = '\0';

	sqlite3* mapDB = nullptr;
	int error = sqlite3_open(filename, &mapDB);
	if (error != SQLITE_OK)
	{
		printf("ERROR: can't open database: %s\n", sqlite3_errmsg(mapDB));
		assert(0);
		sqlite3_close(mapDB);
		return NULL;
	}

	printf("%s successfully opened!\n", filename);

	return mapDB;
}

static void CloseMap(sqlite3* mapDB)
{
	sqlite3_close(mapDB);
}


// Table filling



// Interface

void SaveToCiv6Map(char const* name, MapTile* map)
{
	sqlite3* mapDB = OpenMap(name);


	CloseMap(mapDB);
}
