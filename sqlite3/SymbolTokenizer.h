#pragma once

struct sqlite3;
struct sqlite3_api_routines;

extern "C" __declspec(dllimport) int sqlite3_symboltokenizer_init(
    sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);