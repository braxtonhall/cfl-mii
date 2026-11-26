// tests/trace: builds Miis with cfl-mii on a PC against the stand-in GPU (fake/), and prints one line per Mii:
//   index  icon  draws  files  prints  ndraws
// icon: what the icon texture holds (every draw into it, and through the textures it samples, every draw into those);
// draws: every draw in order with its state; files: the writes to CFL_DB.dat; prints: what cfl-mii printed.
//
//   cfl-trace CFL_Res.dat [--count N] [--seed S] [--frames] [--db CFL_DB.dat] [--ubll ubll.lst] [--real CFL_DB.dat]
//             [--dump I]
//
// The Miis come from a seeded random generator, so two builds given the same --seed and --count get the same Miis.
// Each also gets random build settings: expressions (sometimes all of them), resolution (valid and invalid), hair
// mode, icon size, icon expression and background. --real adds the Miis of a console's CFL_DB.dat (its own and its
// recently seen) before the random ones. --frames builds with the per-frame API (CFL_BeginCharModel /
// CFL_StepCharModel / CFL_DrawModelIcon) instead of CFL_InitCharModel and CFL_CommandMakeModelIcon; it needs a
// cfl-mii that has it (CFL_HAS_FRAME_STEPS). --dump I prints every event of Mii I instead of the summary lines.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cfl_mii.h"
#include "trace.h"
#undef printf   // fake/compat.h routes cfl-mii's printing into the trace; the harness prints for real

static u64 rng_state;
static u64 rng(void)
{
	u64 x = rng_state;   // xorshift64*
	x ^= x >> 12;
	x ^= x << 25;
	x ^= x >> 27;
	rng_state = x;
	return x * 0x2545F4914F6CDD1Dull;
}
static u32 pick(u32 n) { return (u32)(rng() % n); }

static unsigned char* read_all(const char* path, size_t* len)
{
	FILE* f = fopen(path, "rb");
	if (!f) { fprintf(stderr, "cfl-trace: cannot read %s\n", path); exit(2); }
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	rewind(f);
	unsigned char* d = malloc((size_t)n + 1);
	if (fread(d, 1, (size_t)n, f) != (size_t)n) { fprintf(stderr, "cfl-trace: short read on %s\n", path); exit(2); }
	fclose(f);
	*len = (size_t)n;
	return d;
}

static u16 crc16(const u8* data, size_t size)
{
	u32 crc = 0;
	for (size_t i = 0; i < size; i++) {
		for (int bit = 0; bit < 8; bit++)
			crc = (crc & 0x8000) ? ((0x1021 ^ (crc << 1)) & 0xFFFF) : ((crc << 1) & 0xFFFF);
		crc ^= data[i];
	}
	return (u16)crc;
}

// A CFL_DB.dat as cfl-mii reads and writes it: "CFOG", no Miis of its own, and an empty recently-seen section
// ("CFRA", count 0, CRC) at 0xC820, so every Mii built goes through the recently-seen write.
#define DB_SIZE 0xE4C0
#define RECENT_AT 0xC820
#define RECENT_SIZE 0x1CA0
static unsigned char* make_db(void)
{
	unsigned char* db = calloc(1, DB_SIZE);
	memcpy(db, "CFOG", 4);
	unsigned char* r = db + RECENT_AT;
	memcpy(r, "CFRA", 4);
	u16 c = crc16(r, RECENT_SIZE);
	r[RECENT_SIZE - 2] = (u8)(c >> 8);
	r[RECENT_SIZE - 1] = (u8)c;
	return db;
}

typedef struct { MiiData mii; } Entry;

static void random_mii(MiiData* m)
{
	u8* b = (u8*)m;
	for (size_t i = 0; i < sizeof(*m); i++) b[i] = (u8)rng();
	b[0] = 3;   // version
	if (pick(4) == 0) memset(b + 0x0C, 0, 10);   // now and then a null Mii ID (cfl-mii skips the write for those)
	// mostly real faces and hair (shapes 0..11, styles 0..131); the rest stay out of range, which must fail the same way
	if (pick(8) != 0) m->face_style.shape = pick(12);
	if (pick(8) != 0) m->hair_style = (u8)pick(132);
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: cfl-trace CFL_Res.dat [--count N] [--seed S] [--frames] [--db F] [--ubll F] [--real F] [--dump I]\n");
		return 2;
	}
	const char* res = argv[1];
	long count = 1000, dump = -1;
	u64 seed = 1;
	int frames = 0;
	const char *db_path = NULL, *ubll_path = NULL, *real_path = NULL;
	for (int i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "--count") && i + 1 < argc) count = atol(argv[++i]);
		else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--frames")) frames = 1;
		else if (!strcmp(argv[i], "--db") && i + 1 < argc) db_path = argv[++i];
		else if (!strcmp(argv[i], "--ubll") && i + 1 < argc) ubll_path = argv[++i];
		else if (!strcmp(argv[i], "--real") && i + 1 < argc) real_path = argv[++i];
		else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump = atol(argv[++i]);
		else { fprintf(stderr, "cfl-trace: unknown option %s\n", argv[i]); return 2; }
	}
#ifndef CFL_HAS_FRAME_STEPS
	if (frames) { fprintf(stderr, "cfl-trace: --frames needs a cfl-mii with the per-frame API\n"); return 2; }
#endif
	size_t db_len = DB_SIZE, ubll_len = 0;
	unsigned char* db = db_path ? read_all(db_path, &db_len) : make_db();
	unsigned char* ubll = ubll_path ? read_all(ubll_path, &ubll_len) : NULL;
	trace_files(res, db, db_len, ubll, ubll_len);

	// the Miis: a console's own and recently seen (--real), then random ones
	size_t nreal = 0;
	Entry* real = NULL;
	if (real_path) {
		size_t len;
		unsigned char* d = read_all(real_path, &len);
		real = calloc(200, sizeof(Entry));
		for (int i = 0; i < 100 && 8 + (i + 1) * 92 <= (int)len; i++) {
			const u8* rec = d + 8 + i * 92;
			int empty = 1;
			for (int k = 0x0C; k < 0x16; k++) if (rec[k]) empty = 0;
			if (!empty) memcpy(&real[nreal++].mii, rec, 92);
		}
		for (int i = 0; i < 100 && RECENT_AT + 0x6C + (i + 1) * 72 <= (int)len; i++) {
			const u8* rec = d + RECENT_AT + 0x6C + i * 72;
			int empty = 1;
			for (int k = 0x0C; k < 0x16; k++) if (rec[k]) empty = 0;
			if (!empty) memcpy(&real[nreal++].mii, rec, 72);   // the recently-seen records stop before the author
		}
		free(d);
	}

	if (!CFL_Initialize()) { fprintf(stderr, "cfl-trace: CFL_Initialize failed (CFL_Res.dat?)\n"); return 1; }
#ifdef CFL_HAS_FRAME_STEPS
	u32 epoch = 0;
#endif
	long total = (long)nreal + count;
	for (long i = 0; i < total; i++) {
		rng_state = (seed * 0x9E3779B97F4A7C15ull) ^ ((u64)(i + 1) * 0xD1B54A32D192ED03ull);
		if (!rng_state) rng_state = 1;
		for (int k = 0; k < 4; k++) rng();
		MiiData mii;
		if ((size_t)i < nreal) mii = real[i].mii;
		else random_mii(&mii);

		static const int resolutions[] = { 64, 96, 128, 224, 256, 480, 512, 1024, 100 };
		CFLExpressionFlag flags;
		switch (pick(4)) {
		case 0: flags = CFL_EXPRESSION_FLAG_ALL; break;
		case 1: flags = (CFLExpressionFlag)(rng() & CFL_EXPRESSION_FLAG_ALL); break;   // any subset, maybe none
		default: flags = CFL_EXPRESSION_FLAG(CFL_EXPRESSION_NORMAL); break;
		}
		CFLResolution resolution = (size_t)i < nreal ? CFL_RESOLUTION_128 : resolutions[pick(9)];
		if (resolution >= 512 && flags == CFL_EXPRESSION_FLAG_ALL) resolution = 256;   // keep memory and time sane
		CFLHairMode hair = (size_t)i < nreal ? CFL_HAIR_NORMAL : (CFLHairMode)pick(3);
		static const int icon_sizes[] = { 32, 64, 128, 256 };
		int icon_size = (size_t)i < nreal ? 64 : icon_sizes[pick(4)];
		CFLExpression expr = (CFLExpression)pick(CFL_EXPRESSION_COUNT);
		CFLIconSetting setting;
		memset(&setting, 0, sizeof(setting));
		setting.bgType = (CFLIconBGType)pick(3);
		for (int k = 0; k < 4; k++) setting.bgColor[k] = (float)pick(256) / 255.0f;

		trace_begin(i == dump);
		if (i == dump) printf("mii %ld: resolution %d flags %08x hair %d icon %d expression %d bg %d\n", i, resolution,
			flags, hair, icon_size, expr, setting.bgType);
		CFLCharModel model;
		memset(&model, 0, sizeof(model));
		C3D_Tex icon;
		memset(&icon, 0, sizeof(icon));
		int built = 0, drawn = 0;
		if (!frames) {
			built = CFL_InitCharModelWithHairMode(&model, &mii, resolution, flags, hair);
			if (built) {
				CFL_RebindShader();
				drawn = CFL_CommandMakeModelIcon(&model, expr, icon_size, &setting, &icon);
			}
		}
#ifdef CFL_HAS_FRAME_STEPS
		else {
			CFLModelBuild b;
			CFL_SetFrameEpoch(++epoch);
			if (CFL_BeginCharModel(&b, &model, &mii, resolution, flags, hair)) {
				CFLBuildResult r;
				do {
					CFL_SetFrameEpoch(++epoch);
					r = CFL_StepCharModel(&b);
				} while (r == CFL_BUILD_MORE);
				built = r == CFL_BUILD_DONE;
			}
			if (built) {
				CFL_SetFrameEpoch(++epoch);
				drawn = CFL_DrawModelIcon(&model, expr, icon_size, &setting, &icon);
			}
		}
#endif
		u64 icon_hash = drawn ? trace_texture(&icon) : 0;
		TraceSums s;
		trace_end(&s);
		if (icon.data) C3D_TexDelete(&icon);
		CFL_DeleteModel(&model);
#ifdef CFL_HAS_FRAME_STEPS
		if (frames) CFL_FreePending(epoch);
#endif
		if (dump < 0)
			printf("%ld %d%d %016llx %016llx %016llx %016llx %d\n", i, built, drawn, (unsigned long long)icon_hash,
				(unsigned long long)s.draws, (unsigned long long)s.files, (unsigned long long)s.errs, s.ndraws);
		else if (i == dump)
			printf("mii %ld: built %d drawn %d icon %016llx\n", i, built, drawn, (unsigned long long)icon_hash);
	}
	CFL_Finalize();
	free(real);
	return 0;
}
