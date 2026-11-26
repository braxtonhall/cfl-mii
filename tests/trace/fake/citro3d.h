// A stand-in for the parts of citro3d cfl_mii.c uses (tests/trace). Nothing is drawn: fake.c keeps the GPU state these
// calls set and, at every draw, folds a description of everything that decides its pixels into the hash of the
// target being drawn to (see fake.c). Names and signatures follow citro3d; values only need to be consistent.
#pragma once
#include "3ds.h"
#include <math.h>

// ---- GPU enums (values as in libctru's gpu/enums.h where it matters to nobody: any distinct values do) ----
typedef enum {
	GPU_RGBA8 = 0x0, GPU_RGB8 = 0x1, GPU_RGBA5551 = 0x2, GPU_RGB565 = 0x3, GPU_RGBA4 = 0x4, GPU_LA8 = 0x5,
	GPU_HILO8 = 0x6, GPU_L8 = 0x7, GPU_A8 = 0x8, GPU_LA4 = 0x9, GPU_L4 = 0xA, GPU_A4 = 0xB, GPU_ETC1 = 0xC,
	GPU_ETC1A4 = 0xD,
} GPU_TEXCOLOR;
typedef enum { GPU_NEAREST = 0, GPU_LINEAR = 1 } GPU_TEXTURE_FILTER_PARAM;
typedef enum { GPU_CLAMP_TO_EDGE = 0, GPU_CLAMP_TO_BORDER = 1, GPU_REPEAT = 2, GPU_MIRRORED_REPEAT = 3 } GPU_TEXTURE_WRAP_PARAM;
typedef enum { GPU_TEXFACE_2D = 0 } GPU_TEXFACE;
typedef enum { GPU_RB_DEPTH16 = 0, GPU_RB_DEPTH24 = 2, GPU_RB_DEPTH24_STENCIL8 = 3 } GPU_DEPTHBUF;
typedef enum {
	GPU_NEVER = 0, GPU_ALWAYS, GPU_EQUAL, GPU_NOTEQUAL, GPU_LESS, GPU_LEQUAL, GPU_GREATER, GPU_GEQUAL,
} GPU_TESTFUNC;
typedef enum {
	GPU_WRITE_RED = 1, GPU_WRITE_GREEN = 2, GPU_WRITE_BLUE = 4, GPU_WRITE_ALPHA = 8, GPU_WRITE_DEPTH = 16,
	GPU_WRITE_COLOR = 15, GPU_WRITE_ALL = 31,
} GPU_WRITEMASK;
typedef enum { GPU_BLEND_ADD = 0, GPU_BLEND_SUBTRACT, GPU_BLEND_REVERSE_SUBTRACT, GPU_BLEND_MIN, GPU_BLEND_MAX } GPU_BLENDEQUATION;
typedef enum {
	GPU_ZERO = 0, GPU_ONE, GPU_SRC_COLOR, GPU_ONE_MINUS_SRC_COLOR, GPU_DST_COLOR, GPU_ONE_MINUS_DST_COLOR,
	GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_DST_ALPHA, GPU_ONE_MINUS_DST_ALPHA, GPU_CONSTANT_COLOR,
	GPU_ONE_MINUS_CONSTANT_COLOR, GPU_CONSTANT_ALPHA, GPU_ONE_MINUS_CONSTANT_ALPHA, GPU_SRC_ALPHA_SATURATE,
} GPU_BLENDFACTOR;
typedef enum { GPU_CULL_NONE = 0, GPU_CULL_FRONT_CCW, GPU_CULL_BACK_CCW } GPU_CULLMODE;
typedef enum {
	GPU_PRIMARY_COLOR = 0x00, GPU_FRAGMENT_PRIMARY_COLOR = 0x01, GPU_FRAGMENT_SECONDARY_COLOR = 0x02,
	GPU_TEXTURE0 = 0x03, GPU_TEXTURE1 = 0x04, GPU_TEXTURE2 = 0x05, GPU_TEXTURE3 = 0x06,
	GPU_PREVIOUS_BUFFER = 0x0D, GPU_CONSTANT = 0x0E, GPU_PREVIOUS = 0x0F,
} GPU_TEVSRC;
typedef enum {
	GPU_TEVOP_RGB_SRC_COLOR = 0x00, GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR = 0x01, GPU_TEVOP_RGB_SRC_ALPHA = 0x02,
	GPU_TEVOP_RGB_ONE_MINUS_SRC_ALPHA = 0x03, GPU_TEVOP_RGB_SRC_R = 0x04, GPU_TEVOP_RGB_ONE_MINUS_SRC_R = 0x05,
	GPU_TEVOP_RGB_SRC_G = 0x08, GPU_TEVOP_RGB_ONE_MINUS_SRC_G = 0x09, GPU_TEVOP_RGB_SRC_B = 0x0C,
	GPU_TEVOP_RGB_ONE_MINUS_SRC_B = 0x0D,
} GPU_TEVOP_RGB;
typedef enum {
	GPU_REPLACE = 0x00, GPU_MODULATE = 0x01, GPU_ADD = 0x02, GPU_ADD_SIGNED = 0x03, GPU_INTERPOLATE = 0x04,
	GPU_SUBTRACT = 0x05, GPU_DOT3_RGB = 0x06, GPU_DOT3_RGBA = 0x07, GPU_MULTIPLY_ADD = 0x08, GPU_ADD_MULTIPLY = 0x09,
} GPU_COMBINEFUNC;
typedef enum { GPU_BYTE = 0, GPU_UNSIGNED_BYTE = 1, GPU_SHORT = 2, GPU_FLOAT = 3 } GPU_FORMATS;
typedef enum { GPU_TRIANGLES = 0x0000, GPU_TRIANGLE_STRIP = 0x0100, GPU_TRIANGLE_FAN = 0x0200, GPU_GEOMETRY_PRIM = 0x0300 } GPU_Primitive_t;
typedef enum { GPU_VERTEX_SHADER = 0x0, GPU_GEOMETRY_SHADER = 0x1 } GPU_SHADER_TYPE;
typedef enum { GPU_LUT_D0 = 0, GPU_LUT_D1, GPU_LUT_SP, GPU_LUT_FR, GPU_LUT_RB, GPU_LUT_RG, GPU_LUT_RR } GPU_LIGHTLUTID;
typedef enum { GPU_LUTINPUT_NH = 0, GPU_LUTINPUT_VH, GPU_LUTINPUT_NV, GPU_LUTINPUT_LN, GPU_LUTINPUT_SP, GPU_LUTINPUT_CP } GPU_LIGHTLUTINPUT;
enum { GPUREG_FRAMEBUFFER_INVALIDATE = 0x0110, GPUREG_FRAMEBUFFER_FLUSH = 0x0111 };
void GPUCMD_AddWrite(u32 reg, u32 val);

// ---- maths (citro3d maths.h) ----
typedef union { struct { float w, z, y, x; }; float c[4]; } C3D_FVec;
typedef union { C3D_FVec r[4]; float m[16]; } C3D_Mtx;
#define C3D_AngleFromDegrees(_angle) ((_angle) * (float)M_PI / 180.0f)
static inline C3D_FVec FVec4_New(float x, float y, float z, float w) { C3D_FVec v; v.x = x; v.y = y; v.z = z; v.w = w; return v; }
void Mtx_Identity(C3D_Mtx* out);
void Mtx_Persp(C3D_Mtx* mtx, float fovy, float aspect, float near, float far, bool isLeftHanded);
void Mtx_Translate(C3D_Mtx* mtx, float x, float y, float z, bool bRightSide);

// ---- shaders ----
typedef struct { int index; } DVLE_s;
typedef struct { u32 numDVLE; DVLE_s* DVLE; } DVLB_s;
typedef struct { int dummy; } shaderInstance_s;
typedef struct { shaderInstance_s* vertexShader; shaderInstance_s* geometryShader; } shaderProgram_s;
DVLB_s* DVLB_ParseFile(u32* shbinData, u32 shbinSize);
void DVLB_Free(DVLB_s* dvlb);
Result shaderProgramInit(shaderProgram_s* sp);
Result shaderProgramFree(shaderProgram_s* sp);
Result shaderProgramSetVsh(shaderProgram_s* sp, DVLE_s* dvle);
s8 shaderInstanceGetUniformLocation(shaderInstance_s* si, const char* name);
void C3D_BindProgram(shaderProgram_s* program);
void C3D_FVUnifMtx4x4(GPU_SHADER_TYPE type, int id, const C3D_Mtx* mtx);

// ---- attributes and buffers ----
typedef struct { int count; struct { int reg; int format; int n; } loader[12]; } C3D_AttrInfo;
typedef struct { int count; struct { const void* data; int stride; int attribs; u64 permutation; } buf[12]; } C3D_BufInfo;
C3D_AttrInfo* C3D_GetAttrInfo(void);
void AttrInfo_Init(C3D_AttrInfo* info);
int AttrInfo_AddLoader(C3D_AttrInfo* info, int regId, GPU_FORMATS format, int count);
C3D_BufInfo* C3D_GetBufInfo(void);
void BufInfo_Init(C3D_BufInfo* info);
int BufInfo_Add(C3D_BufInfo* info, const void* data, ptrdiff_t stride, int attribCount, u64 permutation);

// ---- textures and render targets ----
typedef struct {
	void* data;
	u16 width, height;
	GPU_TEXCOLOR fmt;
	u32 size;
	int vram;
	u32 magFilter, minFilter, wrapS, wrapT;
} C3D_Tex;
bool C3D_TexInit(C3D_Tex* tex, u16 width, u16 height, GPU_TEXCOLOR format);
bool C3D_TexInitVRAM(C3D_Tex* tex, u16 width, u16 height, GPU_TEXCOLOR format);
void C3D_TexUpload(C3D_Tex* tex, const void* data);
void C3D_TexFlush(C3D_Tex* tex);
void C3D_TexDelete(C3D_Tex* tex);
void C3D_TexSetFilter(C3D_Tex* tex, GPU_TEXTURE_FILTER_PARAM magFilter, GPU_TEXTURE_FILTER_PARAM minFilter);
void C3D_TexSetWrap(C3D_Tex* tex, GPU_TEXTURE_WRAP_PARAM wrapS, GPU_TEXTURE_WRAP_PARAM wrapT);
void C3D_TexBind(int unitId, C3D_Tex* tex);

typedef struct C3D_RenderTarget_tag { void* color; int width, height; int depth; void* depthBuf; } C3D_RenderTarget;
typedef enum { C3D_CLEAR_COLOR = 1, C3D_CLEAR_DEPTH = 2, C3D_CLEAR_ALL = 3 } C3D_ClearBits;
C3D_RenderTarget* C3D_RenderTargetCreateFromTex(C3D_Tex* tex, GPU_TEXFACE face, int level, int depthFmt);
void C3D_RenderTargetDelete(C3D_RenderTarget* target);
void C3D_RenderTargetClear(C3D_RenderTarget* target, C3D_ClearBits clearBits, u32 clearColor, u32 clearDepth);

// ---- frames ----
#define C3D_FRAME_SYNCDRAW 1
#define C3D_FRAME_NONBLOCK 2
bool C3D_FrameBegin(u8 flags);
bool C3D_FrameDrawOn(C3D_RenderTarget* target);
void C3D_FrameEnd(u8 flags);
float C3D_FrameSync(void);

// ---- fixed-function state ----
void C3D_SetViewport(u32 x, u32 y, u32 w, u32 h);
void C3D_CullFace(GPU_CULLMODE mode);
void C3D_DepthTest(bool enable, GPU_TESTFUNC function, GPU_WRITEMASK writemask);
void C3D_AlphaBlend(GPU_BLENDEQUATION colorEq, GPU_BLENDEQUATION alphaEq, GPU_BLENDFACTOR srcClr, GPU_BLENDFACTOR dstClr,
	GPU_BLENDFACTOR srcAlpha, GPU_BLENDFACTOR dstAlpha);

typedef enum { C3D_RGB = 1, C3D_Alpha = 2, C3D_Both = 3 } C3D_TexEnvMode;
typedef struct { u16 srcRgb, srcAlpha; u32 opAll; u16 funcRgb, funcAlpha; u32 color; u16 scaleRgb, scaleAlpha; } C3D_TexEnv;
C3D_TexEnv* C3D_GetTexEnv(int id);
void C3D_TexEnvInit(C3D_TexEnv* env);
void C3D_TexEnvSrc(C3D_TexEnv* env, C3D_TexEnvMode mode, int s1, int s2, int s3);
void C3D_TexEnvOpRgb(C3D_TexEnv* env, int o1, int o2, int o3);
void C3D_TexEnvFunc(C3D_TexEnv* env, C3D_TexEnvMode mode, GPU_COMBINEFUNC param);
void C3D_DirtyTexEnv(C3D_TexEnv* env);

// ---- fragment lighting ----
typedef struct { float ambient[3], diffuse[3], specular0[3], specular1[3], emission[3]; } C3D_Material;
typedef struct { u32 data[256]; } C3D_LightLut;
typedef struct C3D_LightEnv_t C3D_LightEnv;
typedef struct {
	C3D_LightEnv* parent;
	bool enabled;
	float ambient[3], diffuse[3], specular0[3], specular1[3];
	float position[4];
} C3D_Light;
struct C3D_LightEnv_t {
	C3D_Material material;
	float ambient[3];
	struct { int input; bool negative; const C3D_LightLut* lut; } lut[7];
	C3D_Light* lights[8];
};
void C3D_LightEnvInit(C3D_LightEnv* env);
void C3D_LightEnvBind(C3D_LightEnv* env);
void C3D_LightEnvMaterial(C3D_LightEnv* env, const C3D_Material* mtl);
void C3D_LightEnvAmbient(C3D_LightEnv* env, float r, float g, float b);
void C3D_LightEnvLut(C3D_LightEnv* env, GPU_LIGHTLUTID lutId, GPU_LIGHTLUTINPUT input, bool negative, C3D_LightLut* lut);
int C3D_LightInit(C3D_Light* light, C3D_LightEnv* env);
void C3D_LightEnable(C3D_Light* light, bool enable);
void C3D_LightAmbient(C3D_Light* light, float r, float g, float b);
void C3D_LightDiffuse(C3D_Light* light, float r, float g, float b);
void C3D_LightSpecular0(C3D_Light* light, float r, float g, float b);
void C3D_LightSpecular1(C3D_Light* light, float r, float g, float b);
void C3D_LightPosition(C3D_Light* light, C3D_FVec* pos);
void LightLut_Phong(C3D_LightLut* lut, float shininess);

// ---- drawing ----
enum { C3D_UNSIGNED_BYTE = 0, C3D_UNSIGNED_SHORT = 1 };
void C3D_DrawArrays(GPU_Primitive_t primitive, int first, int size);
void C3D_DrawElements(GPU_Primitive_t primitive, int count, int type, const void* indices);
