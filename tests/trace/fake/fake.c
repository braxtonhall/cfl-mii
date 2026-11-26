// tests/trace: libctru and citro3d as far as cfl_mii.c uses them, on a PC. Nothing is rasterised. Instead:
//   - every texture has a content hash: an upload sets it to the hash of the data; a clear of a render target sets it
//     from the clear's values; every draw into it folds in a description of the draw;
//   - a draw's description holds everything that decides its pixels on the 3DS: the target's size, the viewport,
//     culling, depth test, blending, the program, attribute loaders, the vertex and index data it reads, the six
//     texture combiners, the textures bound (their size, format, filters, wraps and content hash), the fragment
//     lighting environment (material, ambient, LUTs, lights) and the matrices in the shader's uniforms;
//   - what was drawn when, and frame boundaries, are not part of it: begin, end, sync, splits, when targets are made
//     and deleted, and the order of state changes between draws make no difference to the pixels.
// Two builds whose draws (and so their icons' content hashes) match draw the same pixels on the console.
#include "trace.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- hashing (FNV-1a, 64 bit) ----
#define FNV_BASIS 0xcbf29ce484222325ull
static u64 hbytes(u64 h, const void* data, size_t n)
{
	const u8* p = (const u8*)data;
	size_t i = 0;
	for (; i + 8 <= n; i += 8) {   // a word at a time, then the tail a byte at a time
		u64 w;
		memcpy(&w, p + i, 8);
		h = (h ^ w) * 0x100000001b3ull;
		h ^= h >> 29;
	}
	for (; i < n; i++) h = (h ^ p[i]) * 0x100000001b3ull;
	return h;
}
static u64 hstr(u64 h, const char* s) { return hbytes(h, s, strlen(s)); }

static struct {
	int dump;
	u64 draws, files, errs;
	int ndraws;
} T;

static void event(u64* sum, const char* text)
{
	*sum = hstr(*sum, text);
	if (T.dump) fputs(text, stdout);
}

void trace_begin(int dump)
{
	T.dump = dump;
	T.draws = T.files = T.errs = FNV_BASIS;
	T.ndraws = 0;
}

void trace_end(TraceSums* out)
{
	out->draws = T.draws;
	out->files = T.files;
	out->errs = T.errs;
	out->ndraws = T.ndraws;
	T.dump = 0;
}

// ---- what textures hold, by their memory ----
typedef struct { const void* key; u64 content; } Slot;
static Slot* slots;
static size_t nslots, used;

static size_t slot_index(const void* key)
{
	size_t h = (size_t)((uintptr_t)key * 0x9E3779B97F4A7C15ull) & (nslots - 1);
	while (slots[h].key && slots[h].key != key) h = (h + 1) & (nslots - 1);
	return h;
}

static void content_set(const void* key, u64 v)
{
	if (!key) return;
	if (!nslots || (used + 1) * 2 > nslots) {
		Slot* old = slots;
		size_t n = nslots;
		nslots = n ? n * 2 : 1024;
		slots = calloc(nslots, sizeof(Slot));
		used = 0;
		for (size_t i = 0; i < n; i++)
			if (old[i].key) { slots[slot_index(old[i].key)] = old[i]; used++; }
		free(old);
	}
	size_t i = slot_index(key);
	if (!slots[i].key) used++;
	slots[i].key = key;
	slots[i].content = v;
}

static u64 content_get(const void* key)
{
	if (!key || !nslots) return 0;
	size_t i = slot_index(key);
	return slots[i].key ? slots[i].content : 0;
}

static void content_drop(const void* key)
{
	if (!key || !nslots) return;
	size_t i = slot_index(key);
	if (!slots[i].key) return;
	// backward-shift deletion keeps the probe chains whole
	size_t j = i;
	for (;;) {
		slots[i].key = NULL;
		for (;;) {
			j = (j + 1) & (nslots - 1);
			if (!slots[j].key) { used--; return; }
			size_t k = (size_t)((uintptr_t)slots[j].key * 0x9E3779B97F4A7C15ull) & (nslots - 1);
			if ((i <= j) ? (i < k && k <= j) : (i < k || k <= j)) continue;
			break;
		}
		slots[i] = slots[j];
		i = j;
	}
}

u64 trace_texture(const C3D_Tex* tex) { return tex ? content_get(tex->data) : 0; }

// ---- memory ----
void* linearAlloc(size_t size) { return malloc(size ? size : 1); }
void linearFree(void* mem) { content_drop(mem); free(mem); }
u32 linearSpaceFree(void) { return 32u << 20; }
u32 vramSpaceFree(void) { return 4u << 20; }

// ---- files ----
static const char* res_path;
static unsigned char* db_data;
static size_t db_size;
static const unsigned char* ubll_data;
static size_t ubll_size;

void trace_files(const char* res, unsigned char* db, size_t db_len, const unsigned char* ubll, size_t ubll_len)
{
	res_path = res;
	db_data = db;
	db_size = db_len;
	ubll_data = ubll;
	ubll_size = ubll_len;
}

#undef fopen
FILE* fake_fopen(const char* path, const char* mode)
{
	if (!strcmp(path, "$CFLRES:/CFL_Res.dat")) return fopen(res_path, mode);
	return fopen(path, mode);
}

int fake_vprintf(const char* fmt, va_list ap)
{
	char line[512];
	vsnprintf(line, sizeof(line), fmt, ap);
	char text[560];
	snprintf(text, sizeof(text), "print %s%s", line, line[0] && line[strlen(line) - 1] == '\n' ? "" : "\n");
	event(&T.errs, text);
	return 0;
}

int fake_printf(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fake_vprintf(fmt, ap);
	va_end(ap);
	return 0;
}

FS_Path fsMakePath(FS_PathType type, const void* path)
{
	FS_Path p = { type, 0, path };
	return p;
}
Result FSUSER_OpenArchive(FS_Archive* archive, int id, FS_Path path) { (void)id; (void)path; *archive = 1; return 0; }
Result FSUSER_CloseArchive(FS_Archive archive) { (void)archive; return 0; }

enum { FILE_DB = 1, FILE_UBLL = 2 };
Result FSUSER_OpenFile(Handle* out, FS_Archive archive, FS_Path path, u32 openFlags, u32 attributes)
{
	(void)archive; (void)openFlags; (void)attributes;
	char name[64];
	int n = 0;
	const u16* p = (const u16*)path.data;
	while (p && p[n] && n < 63) { name[n] = (char)p[n]; n++; }
	name[n] = 0;
	if (!strcmp(name, "/CFL_DB.dat") && db_data) { *out = FILE_DB; return 0; }
	if (!strcmp(name, "/ubll.lst") && ubll_data) { *out = FILE_UBLL; return 0; }
	return (Result)0xC8804478;   // not found
}

Result FSFILE_Read(Handle handle, u32* bytesRead, u64 offset, void* buffer, u32 size)
{
	const unsigned char* d = handle == FILE_DB ? db_data : ubll_data;
	size_t len = handle == FILE_DB ? db_size : ubll_size;
	u32 n = offset >= len ? 0 : (u32)(len - offset < size ? len - offset : size);
	if (n) memcpy(buffer, d + offset, n);
	*bytesRead = n;
	return 0;
}

Result FSFILE_Write(Handle handle, u32* bytesWritten, u64 offset, const void* buffer, u32 size, u32 flags)
{
	(void)flags;
	*bytesWritten = 0;
	if (handle != FILE_DB || offset + size > db_size) return (Result)0xC8A04554;
	memcpy(db_data + offset, buffer, size);
	*bytesWritten = size;
	char text[96];
	snprintf(text, sizeof(text), "write CFL_DB.dat at %llu, %u bytes, %016llx\n", (unsigned long long)offset, size,
		(unsigned long long)hbytes(FNV_BASIS, buffer, size));
	event(&T.files, text);
	return 0;
}

Result FSFILE_Close(Handle handle) { (void)handle; return 0; }
Result romfsMountFromTitle(u64 tid, int mediatype, const char* name) { (void)tid; (void)mediatype; (void)name; return 0; }
Result romfsUnmount(const char* name) { (void)name; return 0; }
Result CFGU_GenHashConsoleUnique(u32 salt, u64* hash) { (void)salt; *hash = 0x0123456789ABCDEFull; return 0; }

// ---- maths (citro3d's, so the matrices are the console's) ----
void Mtx_Identity(C3D_Mtx* out)
{
	memset(out, 0, sizeof(*out));
	out->r[0].x = out->r[1].y = out->r[2].z = out->r[3].w = 1.0f;
}

void Mtx_Persp(C3D_Mtx* mtx, float fovy, float aspect, float near, float far, bool isLeftHanded)
{
	float fovy_tan = tanf(fovy / 2.0f);
	memset(mtx, 0, sizeof(*mtx));
	mtx->r[0].x = 1.0f / (aspect * fovy_tan);
	mtx->r[1].y = 1.0f / fovy_tan;
	mtx->r[2].w = far * near / (near - far);
	mtx->r[3].z = isLeftHanded ? 1.0f : -1.0f;
	mtx->r[2].z = -mtx->r[3].z * near / (near - far);
}

void Mtx_Translate(C3D_Mtx* mtx, float x, float y, float z, bool bRightSide)
{
	float v[4] = { x, y, z, 1.0f };
	if (bRightSide) {
		for (int i = 0; i < 4; i++)
			mtx->r[i].w = mtx->r[i].x * v[0] + mtx->r[i].y * v[1] + mtx->r[i].z * v[2] + mtx->r[i].w;
	} else {
		mtx->r[0].w += x;
		mtx->r[1].w += y;
		mtx->r[2].w += z;
	}
}

// ---- shaders ----
static shaderProgram_s* bound_program;
static float uniforms[96][4];

DVLB_s* DVLB_ParseFile(u32* shbinData, u32 shbinSize)
{
	(void)shbinData; (void)shbinSize;
	DVLB_s* d = calloc(1, sizeof(DVLB_s));
	d->numDVLE = 1;
	d->DVLE = calloc(1, sizeof(DVLE_s));
	return d;
}
void DVLB_Free(DVLB_s* dvlb) { if (dvlb) { free(dvlb->DVLE); free(dvlb); } }
static shaderInstance_s the_vsh;
Result shaderProgramInit(shaderProgram_s* sp) { memset(sp, 0, sizeof(*sp)); return 0; }
Result shaderProgramFree(shaderProgram_s* sp) { if (bound_program == sp) bound_program = NULL; memset(sp, 0, sizeof(*sp)); return 0; }
Result shaderProgramSetVsh(shaderProgram_s* sp, DVLE_s* dvle) { (void)dvle; sp->vertexShader = &the_vsh; return 0; }
s8 shaderInstanceGetUniformLocation(shaderInstance_s* si, const char* name)
{
	(void)si;
	if (!strcmp(name, "projection")) return 0;
	if (!strcmp(name, "modelView")) return 4;
	return -1;
}
void C3D_BindProgram(shaderProgram_s* program) { bound_program = program; }
void C3D_FVUnifMtx4x4(GPU_SHADER_TYPE type, int id, const C3D_Mtx* mtx)
{
	if (type != GPU_VERTEX_SHADER || id < 0 || id + 4 > 96) return;
	for (int i = 0; i < 4; i++) {   // as citro3d stores a row: w, z, y, x
		uniforms[id + i][0] = mtx->r[i].c[0];
		uniforms[id + i][1] = mtx->r[i].c[1];
		uniforms[id + i][2] = mtx->r[i].c[2];
		uniforms[id + i][3] = mtx->r[i].c[3];
	}
}

// ---- attributes and buffers ----
static C3D_AttrInfo attr;
static C3D_BufInfo bufs;
C3D_AttrInfo* C3D_GetAttrInfo(void) { return &attr; }
void AttrInfo_Init(C3D_AttrInfo* info) { memset(info, 0, sizeof(*info)); }
int AttrInfo_AddLoader(C3D_AttrInfo* info, int regId, GPU_FORMATS format, int count)
{
	if (info->count >= 12) return -1;
	info->loader[info->count].reg = regId;
	info->loader[info->count].format = format;
	info->loader[info->count].n = count;
	return info->count++;
}
C3D_BufInfo* C3D_GetBufInfo(void) { return &bufs; }
void BufInfo_Init(C3D_BufInfo* info) { memset(info, 0, sizeof(*info)); }
int BufInfo_Add(C3D_BufInfo* info, const void* data, ptrdiff_t stride, int attribCount, u64 permutation)
{
	if (info->count >= 12) return -1;
	info->buf[info->count].data = data;
	info->buf[info->count].stride = (int)stride;
	info->buf[info->count].attribs = attribCount;
	info->buf[info->count].permutation = permutation;
	return info->count++;
}

// ---- textures and targets ----
static const u8 bits_per_pixel[14] = { 32, 24, 16, 16, 16, 16, 16, 8, 8, 8, 4, 4, 4, 8 };

static bool tex_init(C3D_Tex* tex, u16 width, u16 height, GPU_TEXCOLOR format, int vram)
{
	memset(tex, 0, sizeof(*tex));
	if ((unsigned)format >= 14 || width < 8 || height < 8 || (width & (width - 1)) || (height & (height - 1))) return false;
	tex->width = width;
	tex->height = height;
	tex->fmt = format;
	tex->size = (u32)width * height * bits_per_pixel[format] / 8;
	tex->vram = vram;
	tex->data = malloc(tex->size);
	if (!tex->data) return false;
	char text[64];
	snprintf(text, sizeof(text), "new %ux%u %d", width, height, format);
	content_set(tex->data, hstr(FNV_BASIS, text));
	return true;
}
bool C3D_TexInit(C3D_Tex* tex, u16 width, u16 height, GPU_TEXCOLOR format) { return tex_init(tex, width, height, format, 0); }
bool C3D_TexInitVRAM(C3D_Tex* tex, u16 width, u16 height, GPU_TEXCOLOR format) { return tex_init(tex, width, height, format, 1); }
void C3D_TexUpload(C3D_Tex* tex, const void* data)
{
	if (!tex->data) return;
	memcpy(tex->data, data, tex->size);
	content_set(tex->data, hbytes(FNV_BASIS, data, tex->size));
}
void C3D_TexFlush(C3D_Tex* tex) { (void)tex; }
void C3D_TexDelete(C3D_Tex* tex)
{
	if (!tex || !tex->data) return;
	content_drop(tex->data);
	free(tex->data);
	tex->data = NULL;
}
void C3D_TexSetFilter(C3D_Tex* tex, GPU_TEXTURE_FILTER_PARAM magFilter, GPU_TEXTURE_FILTER_PARAM minFilter)
{
	tex->magFilter = magFilter;
	tex->minFilter = minFilter;
}
void C3D_TexSetWrap(C3D_Tex* tex, GPU_TEXTURE_WRAP_PARAM wrapS, GPU_TEXTURE_WRAP_PARAM wrapT)
{
	tex->wrapS = wrapS;
	tex->wrapT = wrapT;
}
static C3D_Tex units[3];   // what was bound, as the GPU's registers hold it (the struct may be gone since)
void C3D_TexBind(int unitId, C3D_Tex* tex)
{
	if (unitId < 0 || unitId >= 3) return;
	if (tex) units[unitId] = *tex;
	else memset(&units[unitId], 0, sizeof(units[unitId]));
}

C3D_RenderTarget* C3D_RenderTargetCreateFromTex(C3D_Tex* tex, GPU_TEXFACE face, int level, int depthFmt)
{
	(void)face; (void)level;
	C3D_RenderTarget* t = calloc(1, sizeof(C3D_RenderTarget));
	t->color = tex->data;
	t->width = tex->width;
	t->height = tex->height;
	t->depth = depthFmt;
	if (depthFmt >= 0) {
		t->depthBuf = malloc(1);
		content_set(t->depthBuf, hstr(FNV_BASIS, "new depth"));
	}
	return t;
}
void C3D_RenderTargetDelete(C3D_RenderTarget* target)
{
	if (!target) return;
	content_drop(target->depthBuf);
	free(target->depthBuf);
	free(target);
}
void C3D_RenderTargetClear(C3D_RenderTarget* target, C3D_ClearBits clearBits, u32 clearColor, u32 clearDepth)
{
	char text[96];
	if ((clearBits & C3D_CLEAR_COLOR) && target->color) {
		snprintf(text, sizeof(text), "clear color %08x\n", clearColor);
		content_set(target->color, hstr(FNV_BASIS, text));
		event(&T.draws, text);
	}
	if ((clearBits & C3D_CLEAR_DEPTH) && target->depthBuf) {
		snprintf(text, sizeof(text), "clear depth %08x\n", clearDepth);
		content_set(target->depthBuf, hstr(FNV_BASIS, text));
		event(&T.draws, text);
	}
}

static C3D_RenderTarget* current_target;
bool C3D_FrameBegin(u8 flags) { (void)flags; return true; }
bool C3D_FrameDrawOn(C3D_RenderTarget* target) { current_target = target; return true; }
void C3D_FrameEnd(u8 flags) { (void)flags; }
float C3D_FrameSync(void) { return 0.0f; }
void GPUCMD_AddWrite(u32 reg, u32 val) { (void)reg; (void)val; }

// ---- fixed-function state ----
static struct { u32 x, y, w, h; } viewport;
static int cull;
static struct { int enable, func, mask; } depth;
static struct { int ceq, aeq, sc, dc, sa, da; } blend;
static C3D_TexEnv envs[6];
static C3D_LightEnv* light_env;

void C3D_SetViewport(u32 x, u32 y, u32 w, u32 h) { viewport.x = x; viewport.y = y; viewport.w = w; viewport.h = h; }
void C3D_CullFace(GPU_CULLMODE mode) { cull = mode; }
void C3D_DepthTest(bool enable, GPU_TESTFUNC function, GPU_WRITEMASK writemask)
{
	depth.enable = enable;
	depth.func = function;
	depth.mask = writemask;
}
void C3D_AlphaBlend(GPU_BLENDEQUATION colorEq, GPU_BLENDEQUATION alphaEq, GPU_BLENDFACTOR srcClr, GPU_BLENDFACTOR dstClr,
	GPU_BLENDFACTOR srcAlpha, GPU_BLENDFACTOR dstAlpha)
{
	blend.ceq = colorEq; blend.aeq = alphaEq;
	blend.sc = srcClr; blend.dc = dstClr; blend.sa = srcAlpha; blend.da = dstAlpha;
}

C3D_TexEnv* C3D_GetTexEnv(int id) { return &envs[id]; }
void C3D_TexEnvInit(C3D_TexEnv* env)
{
	env->srcRgb = env->srcAlpha = GPU_PREVIOUS | GPU_PREVIOUS << 4 | GPU_PREVIOUS << 8;
	env->opAll = 0;
	env->funcRgb = env->funcAlpha = GPU_REPLACE;
	env->color = 0xFFFFFFFF;
	env->scaleRgb = env->scaleAlpha = 0;
}
void C3D_TexEnvSrc(C3D_TexEnv* env, C3D_TexEnvMode mode, int s1, int s2, int s3)
{
	u16 v = (u16)(s1 | s2 << 4 | s3 << 8);
	if (mode & C3D_RGB) env->srcRgb = v;
	if (mode & C3D_Alpha) env->srcAlpha = v;
}
void C3D_TexEnvOpRgb(C3D_TexEnv* env, int o1, int o2, int o3) { env->opAll = (env->opAll & ~0xFFFu) | (u32)(o1 | o2 << 4 | o3 << 8); }
void C3D_TexEnvFunc(C3D_TexEnv* env, C3D_TexEnvMode mode, GPU_COMBINEFUNC param)
{
	if (mode & C3D_RGB) env->funcRgb = param;
	if (mode & C3D_Alpha) env->funcAlpha = param;
}
void C3D_DirtyTexEnv(C3D_TexEnv* env) { (void)env; }

void C3D_LightEnvInit(C3D_LightEnv* env) { memset(env, 0, sizeof(*env)); }
void C3D_LightEnvBind(C3D_LightEnv* env) { light_env = env; }
void C3D_LightEnvMaterial(C3D_LightEnv* env, const C3D_Material* mtl) { env->material = *mtl; }
void C3D_LightEnvAmbient(C3D_LightEnv* env, float r, float g, float b) { env->ambient[0] = r; env->ambient[1] = g; env->ambient[2] = b; }
void C3D_LightEnvLut(C3D_LightEnv* env, GPU_LIGHTLUTID lutId, GPU_LIGHTLUTINPUT input, bool negative, C3D_LightLut* lut)
{
	env->lut[lutId].input = input;
	env->lut[lutId].negative = negative;
	env->lut[lutId].lut = lut;
}
int C3D_LightInit(C3D_Light* light, C3D_LightEnv* env)
{
	memset(light, 0, sizeof(*light));
	for (int i = 0; i < 8; i++)
		if (!env->lights[i]) { env->lights[i] = light; light->parent = env; return i; }
	return -1;
}
void C3D_LightEnable(C3D_Light* light, bool enable) { light->enabled = enable; }
void C3D_LightAmbient(C3D_Light* light, float r, float g, float b) { light->ambient[0] = r; light->ambient[1] = g; light->ambient[2] = b; }
void C3D_LightDiffuse(C3D_Light* light, float r, float g, float b) { light->diffuse[0] = r; light->diffuse[1] = g; light->diffuse[2] = b; }
void C3D_LightSpecular0(C3D_Light* light, float r, float g, float b) { light->specular0[0] = r; light->specular0[1] = g; light->specular0[2] = b; }
void C3D_LightSpecular1(C3D_Light* light, float r, float g, float b) { light->specular1[0] = r; light->specular1[1] = g; light->specular1[2] = b; }
void C3D_LightPosition(C3D_Light* light, C3D_FVec* pos)
{
	light->position[0] = pos->x; light->position[1] = pos->y; light->position[2] = pos->z; light->position[3] = pos->w;
}
void LightLut_Phong(C3D_LightLut* lut, float shininess)
{
	for (int i = 0; i < 256; i++) {
		float v = powf((float)i / 255.0f, shininess);
		u32 q = (u32)(v * 4095.0f + 0.5f);
		lut->data[i] = q;
	}
}

static u64 lut_hash(const C3D_LightLut* lut)
{
	static const C3D_LightLut* last;
	static u32 last_data[256];
	static u64 last_hash;
	if (lut != last || memcmp(lut->data, last_data, sizeof(last_data))) {
		last = lut;
		memcpy(last_data, lut->data, sizeof(last_data));
		last_hash = hbytes(FNV_BASIS, lut->data, sizeof(lut->data));
	}
	return last_hash;
}

// ---- drawing ----
// A draw's description, as a hash of the state that decides its pixels; with --dump, as text too.
#define CAT(...) do { if (len < (int)cap) len += snprintf(buf + len, cap - (size_t)len, __VA_ARGS__); } while (0)
#define HV(x) (h = hbytes(h, &(x), sizeof(x)))

// The texture units the combiners read: a unit no combiner samples does not change the pixels.
static int units_read(void)
{
	int mask = 0;
	for (int i = 0; i < 6; i++)
		for (int k = 0; k < 3; k++) {
			int a = envs[i].srcRgb >> (4 * k) & 15, b = envs[i].srcAlpha >> (4 * k) & 15;
			if (a >= GPU_TEXTURE0 && a <= GPU_TEXTURE2) mask |= 1 << (a - GPU_TEXTURE0);
			if (b >= GPU_TEXTURE0 && b <= GPU_TEXTURE2) mask |= 1 << (b - GPU_TEXTURE0);
		}
	return mask;
}

static u64 hash_state(int vertices, char* buf, size_t cap)
{
	int len = 0;
	u64 h = FNV_BASIS;
	int tw = current_target ? current_target->width : 0, th = current_target ? current_target->height : 0;
	int td = current_target ? current_target->depth : -9, prog = bound_program != NULL;
	HV(tw); HV(th); HV(td); HV(viewport); HV(cull); HV(depth); HV(blend); HV(prog);
	if (buf) CAT("  target %dx%d depth %d viewport %u,%u %ux%u cull %d depth test %d func %d mask %d\n"
		"  blend %d %d %d %d %d %d program %s\n", tw, th, td, viewport.x, viewport.y, viewport.w, viewport.h, cull,
		depth.enable, depth.func, depth.mask, blend.ceq, blend.aeq, blend.sc, blend.dc, blend.sa, blend.da, prog ? "cfl" : "none");
	HV(attr.count);
	for (int i = 0; i < attr.count; i++) {
		HV(attr.loader[i]);
		if (buf) CAT("  attr %d: reg %d format %d x%d\n", i, attr.loader[i].reg, attr.loader[i].format, attr.loader[i].n);
	}
	HV(bufs.count);
	for (int i = 0; i < bufs.count; i++) {
		u64 data = bufs.buf[i].data ? hbytes(FNV_BASIS, bufs.buf[i].data, (size_t)bufs.buf[i].stride * (size_t)vertices) : 0;
		HV(bufs.buf[i].stride); HV(bufs.buf[i].attribs); HV(bufs.buf[i].permutation); HV(data);
		if (buf) CAT("  buffer %d: stride %d attribs %d permutation %llx data %016llx\n", i, bufs.buf[i].stride,
			bufs.buf[i].attribs, (unsigned long long)bufs.buf[i].permutation, (unsigned long long)data);
	}
	h = hbytes(h, envs, sizeof(envs));
	if (buf) for (int i = 0; i < 6; i++)
		CAT("  combiner %d: src %03x/%03x op %x func %d/%d color %08x scale %d/%d\n", i, envs[i].srcRgb, envs[i].srcAlpha,
			envs[i].opAll, envs[i].funcRgb, envs[i].funcAlpha, envs[i].color, envs[i].scaleRgb, envs[i].scaleAlpha);
	int read = units_read();
	for (int i = 0; i < 3; i++) {
		if (!(read >> i & 1)) continue;
		const C3D_Tex* t = &units[i];
		u64 content = t->data ? content_get(t->data) : 0;
		HV(i); HV(t->width); HV(t->height); HV(t->fmt); HV(t->magFilter); HV(t->minFilter); HV(t->wrapS); HV(t->wrapT); HV(content);
		if (buf) CAT("  texture %d: %ux%u format %d filter %u/%u wrap %u/%u content %016llx\n", i, t->width, t->height,
			t->fmt, t->magFilter, t->minFilter, t->wrapS, t->wrapT, (unsigned long long)content);
	}
	if (!light_env) {
		int none = -1;
		HV(none);
		if (buf) CAT("  lighting none\n");
	} else {
		const C3D_LightEnv* e = light_env;
		const C3D_Material* m = &e->material;
		HV(e->ambient); HV(*m);
		if (buf) CAT("  lighting ambient %a %a %a material %a %a %a / %a %a %a / %a %a %a / %a %a %a / %a %a %a\n",
			e->ambient[0], e->ambient[1], e->ambient[2], m->ambient[0], m->ambient[1], m->ambient[2], m->diffuse[0],
			m->diffuse[1], m->diffuse[2], m->specular0[0], m->specular0[1], m->specular0[2], m->specular1[0],
			m->specular1[1], m->specular1[2], m->emission[0], m->emission[1], m->emission[2]);
		for (int i = 0; i < 7; i++) {
			if (!e->lut[i].lut) continue;
			u64 lh = lut_hash(e->lut[i].lut);
			HV(i); HV(e->lut[i].input); HV(e->lut[i].negative); HV(lh);
			if (buf) CAT("  lut %d input %d negative %d %016llx\n", i, e->lut[i].input, e->lut[i].negative, (unsigned long long)lh);
		}
		for (int i = 0; i < 8; i++) {
			const C3D_Light* l = e->lights[i];
			if (!l) continue;
			HV(i); HV(l->enabled); HV(l->ambient); HV(l->diffuse); HV(l->specular0); HV(l->specular1); HV(l->position);
			if (buf) CAT("  light %d on %d ambient %a %a %a diffuse %a %a %a spec %a %a %a / %a %a %a position %a %a %a %a\n", i,
				l->enabled, l->ambient[0], l->ambient[1], l->ambient[2], l->diffuse[0], l->diffuse[1], l->diffuse[2],
				l->specular0[0], l->specular0[1], l->specular0[2], l->specular1[0], l->specular1[1], l->specular1[2],
				l->position[0], l->position[1], l->position[2], l->position[3]);
		}
	}
	h = hbytes(h, uniforms, 8 * sizeof(uniforms[0]));
	if (buf) for (int u = 0; u < 8; u += 4)
		CAT("  uniform %d: %a %a %a %a / %a %a %a %a / %a %a %a %a / %a %a %a %a\n", u,
			uniforms[u][0], uniforms[u][1], uniforms[u][2], uniforms[u][3], uniforms[u + 1][0], uniforms[u + 1][1],
			uniforms[u + 1][2], uniforms[u + 1][3], uniforms[u + 2][0], uniforms[u + 2][1], uniforms[u + 2][2],
			uniforms[u + 2][3], uniforms[u + 3][0], uniforms[u + 3][1], uniforms[u + 3][2], uniforms[u + 3][3]);
	return h;
}

static void draw(u64 head, const char* head_text, int vertices)
{
	static char buf[16384];
	char* text = T.dump ? buf : NULL;
	size_t cap = sizeof(buf);
	int len = 0;
	if (text) len = snprintf(buf, cap, "%s", head_text);
	u64 d = hash_state(vertices, text ? buf + len : NULL, cap - (size_t)len);
	d = hbytes(d, &head, sizeof(head));
	if (current_target) {
		// the target before the draw matters (blending, and the depth test against what is there)
		u64 color_before = content_get(current_target->color), depth_before = content_get(current_target->depthBuf);
		d = hbytes(d, &color_before, sizeof(color_before));
		d = hbytes(d, &depth_before, sizeof(depth_before));
		if (text) {
			len = (int)strlen(buf);
			snprintf(buf + len, cap - (size_t)len, "  onto %016llx depth %016llx\n", (unsigned long long)color_before,
				(unsigned long long)depth_before);
		}
		content_set(current_target->color, hbytes(color_before, &d, sizeof(d)));
		if (current_target->depthBuf && depth.enable && (depth.mask & GPU_WRITE_DEPTH))
			content_set(current_target->depthBuf, hbytes(depth_before, &d, sizeof(d)));
	}
	T.ndraws++;
	T.draws = hbytes(T.draws, &d, sizeof(d));
	if (text) fputs(buf, stdout);
}

void C3D_DrawArrays(GPU_Primitive_t primitive, int first, int size)
{
	u64 head[3] = { 1, (u64)primitive << 32 | (u32)first, (u64)(u32)size };
	char text[96] = "";
	if (T.dump) snprintf(text, sizeof(text), "draw arrays prim %d first %d count %d\n", primitive, first, size);
	// the buffers are hashed from their first vertex: shift them to `first` for the description
	C3D_BufInfo saved = bufs;
	for (int i = 0; i < bufs.count; i++)
		if (bufs.buf[i].data) bufs.buf[i].data = (const u8*)bufs.buf[i].data + (size_t)first * (size_t)bufs.buf[i].stride;
	draw(hbytes(FNV_BASIS, head, sizeof(head)), text, size);
	bufs = saved;
}

void C3D_DrawElements(GPU_Primitive_t primitive, int count, int type, const void* indices)
{
	int top = -1;
	for (int i = 0; i < count; i++) {
		int v = type == C3D_UNSIGNED_SHORT ? ((const u16*)indices)[i] : ((const u8*)indices)[i];
		if (v > top) top = v;
	}
	u64 ih = hbytes(FNV_BASIS, indices, (size_t)count * (type == C3D_UNSIGNED_SHORT ? 2 : 1));
	u64 head[4] = { 2, (u64)primitive, (u64)count << 8 | (u64)type, ih };
	char text[128] = "";
	if (T.dump) snprintf(text, sizeof(text), "draw elements prim %d count %d type %d indices %016llx\n", primitive, count, type,
		(unsigned long long)ih);
	draw(hbytes(FNV_BASIS, head, sizeof(head)), text, top + 1);
}
