
#pragma once

#include <3ds.h>
#include <citro3d.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	CFL_EXPRESSION_NORMAL = 0,
	CFL_EXPRESSION_SMILE,
	CFL_EXPRESSION_ANGER,
	CFL_EXPRESSION_SORROW,
	CFL_EXPRESSION_SURPRISE,
	CFL_EXPRESSION_BLINK,
	CFL_EXPRESSION_OPENMOUTH,
	CFL_EXPRESSION_SMILE_OM,
	CFL_EXPRESSION_ANGER_OM,
	CFL_EXPRESSION_SORROW_OM,
	CFL_EXPRESSION_SURPRISE_OM,
	CFL_EXPRESSION_BLINK_OM,
	CFL_EXPRESSION_WINK_L,
	CFL_EXPRESSION_WINK_R,
	CFL_EXPRESSION_WINK_L_OM,
	CFL_EXPRESSION_WINK_R_OM,
	CFL_EXPRESSION_LIKE_WINK_L,
	CFL_EXPRESSION_LIKE_WINK_R,
	CFL_EXPRESSION_FRUSTRATED,
	CFL_EXPRESSION_COUNT
} CFLExpression;

typedef u32 CFLExpressionFlag;
#define CFL_EXPRESSION_FLAG(e) (1u << (u32)(e))
#define CFL_EXPRESSION_FLAG_ALL ((1u << CFL_EXPRESSION_COUNT) - 1u)

typedef int CFLResolution;
#define CFL_RESOLUTION_64   64
#define CFL_RESOLUTION_128  128
#define CFL_RESOLUTION_256  256
#define CFL_RESOLUTION_512  512
#define CFL_RESOLUTION_1024 1024

typedef struct {
	void* vbo;
	void* ibo;
	u32 vertexCount;
	u32 indexCount;
	bool useIndices;
	
	float color[3];
	bool hasTexture;
	C3D_Tex tex;
	
	bool needsTint;
	
	bool isAlphaOnly;
	
	bool depthWrite;
	
	bool noSpecular;
	
	bool capBlend;
} CFLPart;

#define CFL_MAX_PARTS 12

// Low two model-flag bits recovered from CFLi_InitResCharModel and the
// matching Miiverse FUN_00187458. Names here are reimplementation API names.
typedef enum {
	CFL_HAIR_NORMAL = 0,
	CFL_HAIR_HAT = 1, // alternate meshes fitted beneath an app-supplied hat
	CFL_HAIR_HIDDEN = 2, // omit cap, hair and forehead together
} CFLHairMode;

typedef struct {
	CFLPart parts[CFL_MAX_PARTS];
	int partCount;
	int maskPartIndex;
	C3D_Tex maskTexForExpr[CFL_EXPRESSION_COUNT];
	bool maskTexBaked[CFL_EXPRESSION_COUNT];
	CFLExpressionFlag expressionFlags;
	CFLExpression expression;
	MiiData mii;
	bool valid;
} CFLCharModel;

bool CFL_IsAvailable(void);

bool CFL_Initialize(void);

void CFL_Finalize(void);

// CFL_Initialize and CFL_Finalize in two halves, so the file work can run on
// another thread than the one that owns citro3d:
// - CFL_LoadResource / CFL_FreeResource: CFL_Res.dat and the CFL_DB.dat cache.
//   File I/O and memory only, no GPU calls; any one thread at a time.
// - CFL_InitGpu / CFL_FiniGpu: the shader and light environments, on the
//   citro3d thread. Neither binds anything (CFL_RebindShader does); call
//   CFL_FiniGpu between frames, once nothing drawn with the shader is queued.
// CFL_Initialize is CFL_LoadResource + CFL_InitGpu + CFL_RebindShader, and
// CFL_Finalize is CFL_FiniGpu + CFL_FreeResource.
bool CFL_LoadResource(void);
void CFL_FreeResource(void);
bool CFL_InitGpu(void);
void CFL_FiniGpu(void);

// The recently-seen write: CFL_InitCharModel adds every Mii it builds to the
// console's recently-seen list in CFL_DB.dat (unless its author is on the
// console's blocklist, ubll.lst). Both are NAND file I/O. With
// CFL_SetAutoAddRecent(false), model building does neither and the caller
// calls CFL_AddRecent itself, on the thread that loaded the resource.
void CFL_SetAutoAddRecent(bool enable);
bool CFL_AddRecent(const MiiData* mii);

void CFL_EnableSDDebug(bool enable);

void CFL_FlushSDDebug(void);

void dbglog(const char* fmt, ...);

void dbglogErr(const char* fmt, ...);

void dbglogVramStats(const char* context, bool onScreen);

bool CFL_MakeStoreData(const MiiData* mii, CFLStoreData* out);
bool CFL_IsStoreDataValid(const CFLStoreData* storeData);

bool CFL_SearchOfficialData(const MiiData* mii, u16* outIndex);

bool CFL_IsAvailableOfficialData(u16 index);
bool CFL_GetOfficialData(u16 index, MiiData* outMii);
int CFL_GetAvailableOfficialDataNum(void);

bool CFL_GetMyMiiIndex(u16* outIndex);

int CFL_GetWorkSize(bool hdModeEnabled);

const float* CFL_GetFavoriteColor(u8 index);

bool CFL_InitCharModel(CFLCharModel* model, const MiiData* mii, CFLResolution resolution, CFLExpressionFlag expressionFlags);

// Explicit hair/hat mode; the original entry point defaults to NORMAL.
// Rebuild the model to change mode. This does not invent/load a hat mesh.
bool CFL_InitCharModelWithHairMode(CFLCharModel* model, const MiiData* mii, CFLResolution resolution, CFLExpressionFlag expressionFlags, CFLHairMode hairMode);

void CFL_DeleteModel(CFLCharModel* model);

bool CFL_HasCharModel(const CFLCharModel* model);

bool CFL_SetExpression(CFLCharModel* model, CFLExpression expression);
CFLExpression CFL_GetExpression(const CFLCharModel* model);
const char* CFL_GetExpressionName(CFLExpression expression);

bool CFL_IsAvailableExpression(const CFLCharModel* model, CFLExpression expression);

int CFL_GetPartCount(const CFLCharModel* model);
const CFLPart* CFL_GetPart(const CFLCharModel* model, int index);

typedef struct {
	int projection;
	int modelView;
} CFLShaderLocations;

CFLShaderLocations CFL_GetShaderLocations(void);

void CFL_BindDefaultShader(void);
void CFL_SetDefaultMaterial(const float color[3], bool noSpecular);

void CFL_RebindShader(void);

typedef enum {
	CFL_ICON_BG_FAVORITE = 0,
	CFL_ICON_BG_DIRECT = 1,
	CFL_ICON_BG_NO_CLEAR = 2,
} CFLIconBGType;

typedef void (*CFLIconCustomCallback)(void* customArgument, const CFLPart* part, const C3D_Mtx* projection, const C3D_Mtx* modelView);

typedef struct {
	CFLIconBGType bgType;
	float bgColor[4];
	CFLIconCustomCallback customCallback;
	void* customArgument;
} CFLIconSetting;

// Reimplementation extension: draw the head into the caller's active icon
// target, allowing custom bodies/cameras without duplicating CFL's renderer.
// Call inside a C3D frame with the CFL shader, lighting and attributes set up.
// Does not allocate, begin/end frames, or own the supplied model/matrices.
void CFL_DrawIconHead(CFLCharModel* model, CFLExpression expression, const CFLIconSetting* setting, const C3D_Mtx* projection, const C3D_Mtx* modelView);

bool CFL_CommandMakeModelIcon(CFLCharModel* model, CFLExpression expression, int iconSize, const CFLIconSetting* setting, C3D_Tex* outIcon);

// ---- Building inside the caller's frames ----
#define CFL_HAS_FRAME_STEPS 1   // this API is here (tests/trace builds against versions with and without it)
// CFL_InitCharModel and CFL_CommandMakeModelIcon each run whole SYNCDRAW frames
// of their own and wait for them. These run the same work a step at a time,
// each pass drawn into the C3D frame the caller already has open (between its
// C3D_FrameBegin and C3D_FrameEnd), with no waits:
// - CFL_BeginCharModel sets a build up (no GPU work). Then call
//   CFL_StepCharModel once per frame until it returns CFL_BUILD_DONE or
//   CFL_BUILD_FAILED: the face bake, one step per mask expression, then the
//   other parts (CPU only). Each step binds the CFL shader; the caller restores
//   its own GPU state afterwards. On failure, CFL_DeleteModel frees what was
//   built (once the GPU is done with it, as below).
// - CFL_DrawModelIcon draws the icon into outIcon (zeroed or holding nothing)
//   in the open frame. Its pixels are there once that frame has finished on
//   the GPU.
// The render targets, decal textures and buffers these passes use are tagged
// with the epoch set by CFL_SetFrameEpoch. Free them with CFL_FreePending(e),
// outside a frame, once every frame of epochs <= e has finished on the GPU;
// it deletes render targets, which C3D forbids inside a frame. The model's own
// textures (face, mask) and the icon belong to the caller as usual.
typedef enum {
	CFL_BUILD_FAILED = -1,
	CFL_BUILD_MORE = 0,
	CFL_BUILD_DONE = 1,
} CFLBuildResult;

enum {
	CFL_BUILD_STEP_FACE = 1,
	CFL_BUILD_STEP_MASK,
	CFL_BUILD_STEP_PARTS,
	CFL_BUILD_STEP_DONE,
	CFL_BUILD_STEP_FAILED,
};

typedef struct {
	CFLCharModel* model;
	MiiData mii;
	u32 canvasSize;
	CFLExpressionFlag expressionFlags;
	CFLExpression startExpr;
	CFLHairMode hairMode;
	int step;
	int maskExpr, maskRequested, maskBaked;
	u32 vramBeforeBakes;
	float anchors[9];
	bool ownFrames;    // internal: CFL_InitCharModel's own SYNCDRAW frames
} CFLModelBuild;

bool CFL_BeginCharModel(CFLModelBuild* build, CFLCharModel* model, const MiiData* mii, CFLResolution resolution, CFLExpressionFlag expressionFlags, CFLHairMode hairMode);
CFLBuildResult CFL_StepCharModel(CFLModelBuild* build);
bool CFL_DrawModelIcon(CFLCharModel* model, CFLExpression expression, int iconSize, const CFLIconSetting* setting, C3D_Tex* outIcon);
void CFL_SetFrameEpoch(u32 epoch);
int CFL_FreePending(u32 doneEpoch);   // the number freed
int CFL_PendingCount(void);

#ifdef __cplusplus
}
#endif
