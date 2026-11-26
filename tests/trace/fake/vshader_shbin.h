// tests/trace: the vertex shader is never run; DVLB_ParseFile (fake.c) only needs something to point at.
static const unsigned char vshader_shbin[4] __attribute__((aligned(4))) = {0};
static const unsigned int vshader_shbin_size = sizeof(vshader_shbin);
