// A stand-in for the parts of libctru cfl_mii.c uses, so it builds and runs on a PC (tests/trace). Types and names
// follow libctru; the values only need to be consistent within one build. MiiData and CFLStoreData are libctru's own
// (include/3ds/mii.h, zlib licence, (c) the libctru authors).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef s32 Result;
typedef u32 Handle;
#define R_FAILED(r) (((Result)(r)) < 0)
#define R_SUCCEEDED(r) (((Result)(r)) >= 0)
#define CTR_PACKED __attribute__((packed))

// ---- Mii (libctru include/3ds/mii.h) ----
typedef struct
{
	u8 version;
	struct
	{
		bool allow_copying : 1;
		bool is_private_name : 1;
		u8 region_lock : 2;
		u8 char_set : 2;
		u8 _pad : 2;
	} mii_options;
	struct
	{
		u8 page_index : 4;
		u8 slot_index : 4;
	} mii_pos;
	struct
	{
		u8 unknown0 : 4;
		u8 origin_console : 3;
		u8 _pad : 1;
	} console_identity;
	u64 system_id;
	u32 mii_id;
	u8 mac[6];
	u8 pad[2];
	struct {
		bool sex : 1;
		u16 bday_month : 4;
		u16 bday_day : 5;
		u16 shirt_color : 4;
		u16 favorite : 1;
		u16 _pad : 1;
	} CTR_PACKED mii_details;
	u16 mii_name[10];
	u8 height;
	u8 width;
	struct
	{
		u16 disable_sharing : 1;
		u16 shape : 4;
		u16 skinColor : 3;
	} CTR_PACKED face_style;
	struct
	{
		u16 wrinkles : 4;
		u16 makeup : 4;
	} CTR_PACKED face_details;
	u8 hair_style;
	struct
	{
		u16 color : 3;
		u16 flip : 1;
		u16 _pad : 4;
	} CTR_PACKED hair_details;
	struct
	{
		u16 style : 6;
		u16 color : 3;
		u16 scale : 4;
		u16 yscale : 3;
		u16 rotation : 5;
		u16 xspacing : 4;
		u16 yposition : 5;
		u16 _pad : 2;
	} CTR_PACKED eye_details;
	struct
	{
		u16 style : 5;
		u16 color : 3;
		u16 scale : 4;
		u16 yscale : 3;
		u16 _pad : 1;
		u16 rotation : 4;
		u16 xspacing : 4;
		u16 yposition : 5;
		u16 _pad2 : 3;
	} CTR_PACKED eyebrow_details;
	struct
	{
		u16 style : 5;
		u16 scale : 4;
		u16 yposition : 5;
		u16 _pad : 2;
	} CTR_PACKED nose_details;
	struct
	{
		u16 style : 6;
		u16 color : 3;
		u16 scale : 4;
		u16 yscale : 3;
	} CTR_PACKED mouth_details;
	struct
	{
		u16 mouth_yposition : 5;
		u16 mustache_style : 3;
		u16 _pad : 8;
	} CTR_PACKED mustache_details;
	struct
	{
		u16 style : 3;
		u16 color : 3;
		u16 scale : 4;
		u16 ypos : 5;
		u16 _pad : 1;
	} CTR_PACKED beard_details;
	struct
	{
		u16 style : 4;
		u16 color : 3;
		u16 scale : 4;
		u16 ypos : 5;
	} CTR_PACKED glasses_details;
	struct
	{
		bool enable : 1;
		u16 scale : 4;
		u16 xpos : 5;
		u16 ypos : 5;
		u16 _pad : 1;
	} CTR_PACKED mole_details;
	u16 author_name[10];
} CTR_PACKED MiiData;

typedef struct
{
	MiiData miiData;
	u8 pad[2];
	u16 crc16;
} CFLStoreData;

// ---- memory ----
void* linearAlloc(size_t size);
void linearFree(void* mem);
u32 linearSpaceFree(void);
u32 vramSpaceFree(void);

// ---- the file system: shared extdata F000000B (CFL_DB.dat, ubll.lst), served from memory by fake.c ----
typedef u64 FS_Archive;
typedef enum { PATH_INVALID = 0, PATH_EMPTY = 1, PATH_BINARY = 2, PATH_ASCII = 3, PATH_UTF16 = 4 } FS_PathType;
typedef struct { FS_PathType type; u32 size; const void* data; } FS_Path;
enum { ARCHIVE_SHARED_EXTDATA = 7 };
enum { MEDIATYPE_NAND = 0, MEDIATYPE_SD = 1 };
enum { FS_OPEN_READ = 1, FS_OPEN_WRITE = 2, FS_OPEN_CREATE = 4 };
enum { FS_WRITE_FLUSH = 1 };
FS_Path fsMakePath(FS_PathType type, const void* path);
Result FSUSER_OpenArchive(FS_Archive* archive, int id, FS_Path path);
Result FSUSER_CloseArchive(FS_Archive archive);
Result FSUSER_OpenFile(Handle* out, FS_Archive archive, FS_Path path, u32 openFlags, u32 attributes);
Result FSFILE_Read(Handle handle, u32* bytesRead, u64 offset, void* buffer, u32 size);
Result FSFILE_Write(Handle handle, u32* bytesWritten, u64 offset, const void* buffer, u32 size, u32 flags);
Result FSFILE_Close(Handle handle);

// ---- romfs (CFL_Res.dat): fake.c's fopen serves "$CFLRES:/CFL_Res.dat" from the file given to the harness ----
Result romfsMountFromTitle(u64 tid, int mediatype, const char* name);
Result romfsUnmount(const char* name);

// ---- cfg ----
Result CFGU_GenHashConsoleUnique(u32 appIdSalt, u64* hash);
