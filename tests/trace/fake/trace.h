// tests/trace: what the harness reads from the stand-in (fake.c).
#pragma once
#include "citro3d.h"

// CFL_Res.dat's path; CFL_DB.dat and ubll.lst as the shared extdata holds them (NULL: the file is not there). The
// database is kept in memory and changed by cfl-mii's writes; nothing is written to disk.
void trace_files(const char* res_path, unsigned char* db, size_t db_len, const unsigned char* ubll, size_t ubll_len);

// One Mii: trace_begin, cfl-mii's calls, trace_end. With dump, every event is printed as it is hashed.
void trace_begin(int dump);
typedef struct {
	u64 draws;   // every draw in order, each with all the state that decides its pixels
	u64 files;   // the writes to CFL_DB.dat
	u64 errs;    // what cfl-mii printed
	int ndraws;
} TraceSums;
void trace_end(TraceSums* out);

// What a texture holds: its uploads, or everything drawn into it since its last clear.
u64 trace_texture(const C3D_Tex* tex);
