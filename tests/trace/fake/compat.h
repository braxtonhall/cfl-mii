// Force-included when building cfl_mii.c for tests/trace: its file reads of CFL_Res.dat go to the file given to the
// harness, and what it prints goes into the trace instead of the terminal.
#pragma once
#include <stdarg.h>
#include <stdio.h>
FILE* fake_fopen(const char* path, const char* mode);
int fake_vprintf(const char* fmt, va_list ap);
int fake_printf(const char* fmt, ...);
#define fopen(p, m) fake_fopen((p), (m))
#define vprintf(fmt, ap) fake_vprintf((fmt), (ap))
#define printf(...) fake_printf(__VA_ARGS__)
