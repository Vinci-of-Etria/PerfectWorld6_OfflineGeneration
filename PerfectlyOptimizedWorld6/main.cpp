#include "PerfectOptWorld6.h"

#pragma comment(lib, "sqlite3-static.lib")

int main(int argc, char* argv[])
{
    LoadDefaultSettings("standard_pw6_settings.txt");
    GenerateMap();
}
