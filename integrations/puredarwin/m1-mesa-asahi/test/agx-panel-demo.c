// SPDX-License-Identifier: MIT
// agx-panel-demo: GLES3 scenes rendered off-screen on the GPU at panel resolution and shown on
// the built-in panel through the CPU framebuffer mapping (PureDarwin PDGOP), with timings.
// The same source builds on Linux (no panel: the present step writes to a heap buffer of the
// panel's layout, or is skipped with --no-present).
//
// Scenes:  plasma  full-screen procedural fragment shader
//          quads   10000 textured, alpha-blended quads in one draw call
//          mesh    nine lit, rotating tori with a depth buffer
// Modes:   render    draw + glFinish
//          readback  draw + glReadPixels
//          present   draw + glReadPixels + rows into the panel mapping
// Paths:   rgba8   RGBA8 target, canonical orientation; present converts to x2r10g10b10 and flips
//          rgb10   RGB10_A2 target drawn y-flipped with red/blue swapped, so that a
//                  GL_UNSIGNED_INT_2_10_10_10_REV readback is already the panel's word; present
//                  is one copy
//          direct  as rgb10, glReadPixels straight into the panel mapping (present mode only)
//
// Output (stdout), one line per measurement and two hashes per scene:
//   AGXBENCH scene=S mode=M frames=N median_us=.. min_us=.. max_us=.. fps=.. path=P sink=K read_us=.. copy_us=..
//   AGXHASH scene=S target=rgba8|rgb10 frame=7 sha256=...
//   AGXCHECK scene=S differ_px=.. of=.. max_diff=.. flat=0|1 result=ok|FAIL
// AGXCHECK needs no reference: it converts the rgba8 frame the way the present step does and
// compares it with the rgb10 frame of the same scene (flip, channel swap and conversion must all
// agree; a few edge pixels and 8- versus 10-bit blending may differ, at most 2% over 16/1023;
// a frame that is one flat colour fails).
// Frames are a function of the frame index only, never of the clock, so hashes are repeatable.
// Whatever was on the panel (the text console) is saved first and put back after every present
// run and at exit; lines are printed only while the saved image is on the panel.
//
// usage: agx-panel-demo [--size WxH] [--frames N] [--budget seconds] [--scenes plasma,quads,mesh]
//                       [--no-present] [--hold-ms N] [--require-renderer substr] [--dump prefix]

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#ifdef HAVE_PDGOP
#include <PDGOP.h>
#endif

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

#define T "agx-panel-demo: "
#define HASH_FRAME 7
#define NQUADS 10000

// ---- SHA-256 ----
static const uint32_t K[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha256_block(uint32_t h[8], const uint8_t *p)
{
	uint32_t w[64], a[8];
	for (int i = 0; i < 16; i++)
		w[i] = (uint32_t)p[4*i] << 24 | (uint32_t)p[4*i+1] << 16 | (uint32_t)p[4*i+2] << 8 | p[4*i+3];
	for (int i = 16; i < 64; i++)
		w[i] = w[i-16] + (ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3)) + w[i-7] +
		    (ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10));
	memcpy(a, h, sizeof(a));
	for (int i = 0; i < 64; i++) {
		uint32_t t1 = a[7] + (ROR(a[4], 6) ^ ROR(a[4], 11) ^ ROR(a[4], 25)) +
		    ((a[4] & a[5]) ^ (~a[4] & a[6])) + K[i] + w[i];
		uint32_t t2 = (ROR(a[0], 2) ^ ROR(a[0], 13) ^ ROR(a[0], 22)) +
		    ((a[0] & a[1]) ^ (a[0] & a[2]) ^ (a[1] & a[2]));
		memmove(a + 1, a, 7 * sizeof(a[0]));
		a[4] += t1;
		a[0] = t1 + t2;
	}
	for (int i = 0; i < 8; i++)
		h[i] += a[i];
}
static void sha256_hex(const uint8_t *data, size_t n, char out[65])
{
	uint32_t h[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
	uint8_t tail[128] = { 0 };
	size_t full = n / 64, rem = n % 64, tl = rem < 56 ? 64 : 128;
	for (size_t i = 0; i < full; i++)
		sha256_block(h, data + 64 * i);
	memcpy(tail, data + 64 * full, rem);
	tail[rem] = 0x80;
	for (int i = 0; i < 8; i++)
		tail[tl - 1 - i] = (uint8_t)(((uint64_t)n * 8) >> (8 * i));
	sha256_block(h, tail);
	if (tl == 128)
		sha256_block(h, tail + 64);
	for (int i = 0; i < 8; i++)
		sprintf(out + 8 * i, "%08x", h[i]);
}

// ---- small helpers ----
static uint64_t now_us(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

static uint32_t rng_state = 0x1234567u;
static uint32_t rng(void)	// fixed sequence: the scenes must be identical on every run
{
	rng_state = rng_state * 1664525u + 1013904223u;
	return rng_state >> 8;
}
static float frand(void) { return (float)(rng() & 0xffff) / 65535.0f; }

static volatile sig_atomic_t stop_requested;
static void on_signal(int sig) { (void)sig; stop_requested = 1; }

typedef struct { float m[16]; } mat4;	// column-major
static mat4 mat_mul(mat4 a, mat4 b)
{
	mat4 r;
	for (int c = 0; c < 4; c++)
		for (int w = 0; w < 4; w++) {
			float s = 0;
			for (int k = 0; k < 4; k++)
				s += a.m[k * 4 + w] * b.m[c * 4 + k];
			r.m[c * 4 + w] = s;
		}
	return r;
}
static mat4 mat_identity(void) { mat4 r = { { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 } }; return r; }
static mat4 mat_translate(float x, float y, float z) { mat4 r = mat_identity(); r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
static mat4 mat_rot(float ax, float ay, float az, float angle)
{
	float c = cosf(angle), s = sinf(angle), l = sqrtf(ax * ax + ay * ay + az * az), t = 1 - c;
	mat4 r = mat_identity();
	ax /= l; ay /= l; az /= l;
	r.m[0] = t*ax*ax + c;    r.m[1] = t*ax*ay + s*az; r.m[2] = t*ax*az - s*ay;
	r.m[4] = t*ax*ay - s*az; r.m[5] = t*ay*ay + c;    r.m[6] = t*ay*az + s*ax;
	r.m[8] = t*ax*az + s*ay; r.m[9] = t*ay*az - s*ax; r.m[10] = t*az*az + c;
	return r;
}
static mat4 mat_perspective(float fovy, float aspect, float n, float f)
{
	float t = 1.0f / tanf(fovy / 2);
	mat4 r = { { 0 } };
	r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
	return r;
}

// ---- GL objects ----
// Every vertex shader ends with gl_Position.y *= u_flip and every fragment shader with
// mix(c, c.bgra, u_bgr): the rgb10 path uses both to produce panel-ready words.
static const char VS_PLASMA[] =
	"#version 300 es\nlayout(location=0) in vec2 a_pos; out vec2 v_uv; uniform float u_flip;\n"
	"void main() { v_uv = a_pos * 0.5 + 0.5; gl_Position = vec4(a_pos.x, a_pos.y * u_flip, 0.0, 1.0); }\n";
static const char FS_PLASMA[] =
	"#version 300 es\nprecision highp float;\nin vec2 v_uv; out vec4 frag; uniform float u_t; uniform float u_bgr; uniform float u_aspect;\n"
	"void main() {\n"
	"  vec2 p = vec2(v_uv.x * u_aspect, v_uv.y);\n"
	"  float v = sin(p.x * 10.0 + u_t) + sin(10.0 * (p.x * sin(u_t * 0.5) + p.y * cos(u_t * 0.33)) + u_t);\n"
	"  vec2 c = p - vec2(0.8 + 0.5 * sin(u_t * 0.2), 0.5 + 0.5 * cos(u_t * 0.33));\n"
	"  v += sin(sqrt(100.0 * dot(c, c) + 1.0) + u_t);\n"
	"  vec3 col = 0.5 + 0.5 * sin(v * 3.14159265 * 0.5 + vec3(0.0, 2.0943951, 4.1887902));\n"
	"  vec4 o = vec4(col * (0.35 + 0.65 * v_uv.y), 1.0);\n"
	"  frag = mix(o, o.bgra, u_bgr);\n}\n";
static const char VS_QUADS[] =
	"#version 300 es\nlayout(location=0) in vec2 a_corner; layout(location=1) in vec2 a_center;\n"
	"layout(location=2) in vec3 a_param; layout(location=3) in vec4 a_color;\n"
	"out vec2 v_uv; out vec4 v_color; uniform float u_t; uniform float u_flip; uniform float u_aspect;\n"
	"void main() {\n"
	"  float ph = a_param.y;\n"
	"  vec2 drift = 0.06 * vec2(cos(u_t * 0.7 + ph), sin(u_t * 0.9 + ph * 1.7));\n"
	"  vec2 p = a_center + drift + (a_corner - 0.5) * a_param.x * vec2(1.0, u_aspect);\n"
	"  float tile = a_param.z;\n"
	"  v_uv = (vec2(mod(tile, 4.0), floor(tile / 4.0)) + a_corner) * 0.25;\n"
	"  v_color = a_color;\n"
	"  gl_Position = vec4(p.x, p.y * u_flip, 0.0, 1.0);\n}\n";
static const char FS_QUADS[] =
	"#version 300 es\nprecision mediump float;\nin vec2 v_uv; in vec4 v_color; out vec4 frag; uniform sampler2D u_tex; uniform float u_bgr;\n"
	"void main() { vec4 o = texture(u_tex, v_uv) * v_color; frag = mix(o, o.bgra, u_bgr); }\n";
static const char VS_MESH[] =
	"#version 300 es\nlayout(location=0) in vec3 a_pos; layout(location=1) in vec3 a_normal;\n"
	"out vec3 v_normal; out vec3 v_eye; uniform mat4 u_mv; uniform mat4 u_proj; uniform float u_flip;\n"
	"void main() {\n"
	"  vec4 e = u_mv * vec4(a_pos, 1.0);\n"
	"  v_eye = e.xyz; v_normal = mat3(u_mv) * a_normal;\n"
	"  gl_Position = u_proj * e; gl_Position.y *= u_flip;\n}\n";
static const char FS_MESH[] =
	"#version 300 es\nprecision highp float;\nin vec3 v_normal; in vec3 v_eye; out vec4 frag; uniform vec3 u_albedo; uniform float u_bgr;\n"
	"void main() {\n"
	"  vec3 n = normalize(v_normal), l = normalize(vec3(0.4, 0.7, 0.6)), v = normalize(-v_eye);\n"
	"  float d = max(dot(n, l), 0.0), s = pow(max(dot(n, normalize(l + v)), 0.0), 48.0);\n"
	"  vec4 o = vec4(u_albedo * (0.12 + 0.88 * d) + vec3(0.6) * s, 1.0);\n"
	"  frag = mix(o, o.bgra, u_bgr);\n}\n";

static GLuint compile(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	GLint ok = 0;
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024];
		glGetShaderInfoLog(s, sizeof(log), NULL, log);
		printf(T "shader compile failed: %s\n", log);
		exit(2);
	}
	return s;
}
static GLuint program(const char *vs, const char *fs)
{
	GLuint p = glCreateProgram();
	GLint ok = 0;
	glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
	glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
	glLinkProgram(p);
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[1024];
		glGetProgramInfoLog(p, sizeof(log), NULL, log);
		printf(T "program link failed: %s\n", log);
		exit(2);
	}
	return p;
}

static int W = 2560, H = 1600;
static GLuint prog_plasma, prog_quads, prog_mesh;
static GLuint vao_plasma, vao_quads, vao_mesh, tex_atlas;
static GLsizei mesh_indices;

static void setup_plasma(void)
{
	static const float tri[] = { -1, -1, 3, -1, -1, 3 };
	GLuint vbo;
	prog_plasma = program(VS_PLASMA, FS_PLASMA);
	glGenVertexArrays(1, &vao_plasma);
	glBindVertexArray(vao_plasma);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
}

static void setup_quads(void)
{
	// 4x4 atlas of 64x64 tiles: soft discs, rings, bars and frames, premade with straight alpha
	enum { TS = 64, AS = 256 };
	uint8_t *atlas = malloc(AS * AS * 4);
	for (int ty = 0; ty < 4; ty++)
		for (int tx = 0; tx < 4; tx++) {
			int kind = (ty * 4 + tx) % 4;
			for (int y = 0; y < TS; y++)
				for (int x = 0; x < TS; x++) {
					float u = (x + 0.5f) / TS - 0.5f, v = (y + 0.5f) / TS - 0.5f, r = sqrtf(u * u + v * v), a;
					if (kind == 0) a = r < 0.45f ? 1.0f - r / 0.45f * 0.5f : 0.0f;
					else if (kind == 1) a = fabsf(r - 0.33f) < 0.09f ? 0.9f : 0.0f;
					else if (kind == 2) a = (fabsf(u) < 0.46f && fabsf(v) < 0.46f) ? 0.55f : 0.0f;
					else a = (fabsf(u) < 0.46f && fabsf(v) < 0.46f && (fabsf(u) > 0.36f || fabsf(v) > 0.36f || ((x / 8 + y / 8) & 1))) ? 0.8f : 0.0f;
					uint8_t *p = atlas + 4 * ((ty * TS + y) * AS + tx * TS + x);
					p[0] = (uint8_t)(160 + 95 * (tx / 3.0f));
					p[1] = (uint8_t)(160 + 95 * (ty / 3.0f));
					p[2] = (uint8_t)(255 - 60 * ((tx + ty) / 6.0f));
					p[3] = (uint8_t)(a * 255.0f + 0.5f);
				}
		}
	glGenTextures(1, &tex_atlas);
	glBindTexture(GL_TEXTURE_2D, tex_atlas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, AS, AS, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	free(atlas);

	// per vertex: corner(2) centre(2) size,phase,tile(3) colour(4)
	enum { STRIDE = 11 };
	float *v = malloc(sizeof(float) * STRIDE * 4 * NQUADS);
	uint16_t *idx = malloc(sizeof(uint16_t) * 6 * NQUADS);
	for (int q = 0; q < NQUADS; q++) {
		float cx = frand() * 2.2f - 1.1f, cy = frand() * 2.2f - 1.1f;
		float size = 0.03f + 0.07f * frand(), phase = frand() * 6.2831853f, tile = (float)(rng() % 16);
		float col[4] = { 0.4f + 0.6f * frand(), 0.4f + 0.6f * frand(), 0.4f + 0.6f * frand(), 0.35f + 0.5f * frand() };
		for (int k = 0; k < 4; k++) {
			float *o = v + STRIDE * (4 * q + k);
			o[0] = (float)(k & 1); o[1] = (float)(k >> 1);
			o[2] = cx; o[3] = cy; o[4] = size; o[5] = phase; o[6] = tile;
			memcpy(o + 7, col, sizeof(col));
		}
		uint16_t b = (uint16_t)(4 * q), *i = idx + 6 * q;
		i[0] = b; i[1] = b + 1; i[2] = b + 2; i[3] = b + 2; i[4] = b + 1; i[5] = b + 3;
	}
	GLuint vbo, ibo;
	prog_quads = program(VS_QUADS, FS_QUADS);
	glGenVertexArrays(1, &vao_quads);
	glBindVertexArray(vao_quads);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * STRIDE * 4 * NQUADS, v, GL_STATIC_DRAW);
	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * 6 * NQUADS, idx, GL_STATIC_DRAW);
	const GLsizei st = STRIDE * sizeof(float);
	for (int a = 0; a < 4; a++)
		glEnableVertexAttribArray(a);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, st, (void *)0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, st, (void *)(2 * sizeof(float)));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, st, (void *)(4 * sizeof(float)));
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, st, (void *)(7 * sizeof(float)));
	free(v);
	free(idx);
}

static void setup_mesh(void)
{
	enum { NU = 128, NV = 64 };	// torus: 8192 quads, 16384 triangles
	const float R = 1.0f, r = 0.42f;
	int nverts = (NU + 1) * (NV + 1);
	float *v = malloc(sizeof(float) * 6 * nverts);
	uint16_t *idx = malloc(sizeof(uint16_t) * 6 * NU * NV);
	for (int i = 0; i <= NU; i++)
		for (int j = 0; j <= NV; j++) {
			float a = 6.2831853f * i / NU, b = 6.2831853f * j / NV, *o = v + 6 * (i * (NV + 1) + j);
			o[0] = (R + r * cosf(b)) * cosf(a); o[1] = (R + r * cosf(b)) * sinf(a); o[2] = r * sinf(b);
			o[3] = cosf(b) * cosf(a); o[4] = cosf(b) * sinf(a); o[5] = sinf(b);
		}
	mesh_indices = 0;
	for (int i = 0; i < NU; i++)
		for (int j = 0; j < NV; j++) {
			uint16_t p = (uint16_t)(i * (NV + 1) + j), q = (uint16_t)(p + NV + 1);
			uint16_t *o = idx + mesh_indices;
			o[0] = p; o[1] = q; o[2] = p + 1; o[3] = p + 1; o[4] = q; o[5] = q + 1;
			mesh_indices += 6;
		}
	GLuint vbo, ibo;
	prog_mesh = program(VS_MESH, FS_MESH);
	glGenVertexArrays(1, &vao_mesh);
	glBindVertexArray(vao_mesh);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * nverts, v, GL_STATIC_DRAW);
	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * mesh_indices, idx, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
	free(v);
	free(idx);
}

enum { SCENE_PLASMA, SCENE_QUADS, SCENE_MESH, NSCENES };
static const char *const scene_names[NSCENES] = { "plasma", "quads", "mesh" };

// Draw frame `frame` of a scene into the bound framebuffer. panel != 0 selects the y-flipped,
// red/blue-swapped output of the rgb10 path.
static void draw_scene(int scene, int frame, int panel)
{
	const float t = (float)frame / 60.0f, flip = panel ? -1.0f : 1.0f, bgr = panel ? 1.0f : 0.0f;
	const float aspect = (float)W / (float)H;
	float bg[3] = { 0.05f, 0.07f, 0.12f };
	if (panel) { float s = bg[0]; bg[0] = bg[2]; bg[2] = s; }

	glViewport(0, 0, W, H);
	if (scene == SCENE_PLASMA) {
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glUseProgram(prog_plasma);
		glUniform1f(glGetUniformLocation(prog_plasma, "u_t"), t * 2.0f);
		glUniform1f(glGetUniformLocation(prog_plasma, "u_flip"), flip);
		glUniform1f(glGetUniformLocation(prog_plasma, "u_bgr"), bgr);
		glUniform1f(glGetUniformLocation(prog_plasma, "u_aspect"), aspect);
		glBindVertexArray(vao_plasma);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	} else if (scene == SCENE_QUADS) {
		glClearColor(bg[0], bg[1], bg[2], 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram(prog_quads);
		glUniform1f(glGetUniformLocation(prog_quads, "u_t"), t * 3.0f);
		glUniform1f(glGetUniformLocation(prog_quads, "u_flip"), flip);
		glUniform1f(glGetUniformLocation(prog_quads, "u_bgr"), bgr);
		glUniform1f(glGetUniformLocation(prog_quads, "u_aspect"), aspect);
		glUniform1i(glGetUniformLocation(prog_quads, "u_tex"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex_atlas);
		glBindVertexArray(vao_quads);
		glDrawElements(GL_TRIANGLES, 6 * NQUADS, GL_UNSIGNED_SHORT, 0);
	} else {
		glClearColor(bg[0], bg[1], bg[2], 1.0f);
		glClearDepthf(1.0f);
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(prog_mesh);
		mat4 proj = mat_perspective(0.9f, aspect, 0.5f, 50.0f);
		glUniformMatrix4fv(glGetUniformLocation(prog_mesh, "u_proj"), 1, GL_FALSE, proj.m);
		glUniform1f(glGetUniformLocation(prog_mesh, "u_flip"), flip);
		glUniform1f(glGetUniformLocation(prog_mesh, "u_bgr"), bgr);
		glBindVertexArray(vao_mesh);
		for (int i = 0; i < 9; i++) {
			int gx = i % 3 - 1, gy = i / 3 - 1;
			mat4 mv = mat_mul(mat_translate(gx * 3.1f, gy * 2.0f, -7.5f + 0.6f * (float)((i * 5) % 3)),
			    mat_mul(mat_rot(0.3f + gx, 1.0f, 0.2f * gy, t * 1.5f + i), mat_rot(1, 0, 0, t * 0.9f + 0.7f * i)));
			glUniformMatrix4fv(glGetUniformLocation(prog_mesh, "u_mv"), 1, GL_FALSE, mv.m);
			glUniform3f(glGetUniformLocation(prog_mesh, "u_albedo"), 0.35f + 0.2f * (i % 3), 0.3f + 0.2f * ((i + 1) % 3),
			    0.3f + 0.2f * ((i + 2) % 3));
			glDrawElements(GL_TRIANGLES, mesh_indices, GL_UNSIGNED_SHORT, 0);
		}
		glDisable(GL_DEPTH_TEST);
	}
}

// ---- the panel (or a heap stand-in with the same layout) ----
struct sink {
	uint8_t *base;		// first visible pixel of row 0
	uint32_t stride;	// bytes
	const char *kind;	// "panel" or "heap"
	uint8_t *saved;		// console image, visible pixels only (panel)
	uint8_t *row;		// one converted row, so the mapping is only ever written
#ifdef HAVE_PDGOP
	PDGOPFramebuffer fb;
#endif
	int open;
};

static void sink_restore(struct sink *s)
{
	if (!s->saved)
		return;
	for (int y = 0; y < H; y++)
		memcpy(s->base + (size_t)y * s->stride, s->saved + (size_t)y * W * 4, (size_t)W * 4);
}

// RGBA8, bottom row first -> x2r10g10b10, top row first
static void sink_present_rgba8(struct sink *s, const uint8_t *rgba)
{
	uint32_t *row = (uint32_t *)s->row;
	for (int y = 0; y < H; y++) {
		const uint8_t *src = rgba + (size_t)(H - 1 - y) * W * 4;
		for (int x = 0; x < W; x++) {
			uint32_t r = src[4 * x], g = src[4 * x + 1], b = src[4 * x + 2];
			row[x] = ((r << 2 | r >> 6) << 20) | ((g << 2 | g >> 6) << 10) | (b << 2 | b >> 6);
		}
		memcpy(s->base + (size_t)y * s->stride, row, (size_t)W * 4);
	}
}

// already panel words, top row first
static void sink_present_words(struct sink *s, const uint8_t *words)
{
	if (s->stride == (uint32_t)W * 4) {
		memcpy(s->base, words, (size_t)W * 4 * H);
		return;
	}
	for (int y = 0; y < H; y++)
		memcpy(s->base + (size_t)y * s->stride, words + (size_t)y * W * 4, (size_t)W * 4);
}

// ---- measurement ----
enum { MODE_RENDER, MODE_READBACK, MODE_PRESENT };
enum { PATH_RGBA8, PATH_RGB10, PATH_DIRECT };
static const char *const mode_names[] = { "render", "readback", "present" };
static const char *const path_names[] = { "rgba8", "rgb10", "direct" };

static int cmp_u64(const void *a, const void *b)
{
	uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
	return x < y ? -1 : x > y;
}
static uint64_t median(uint64_t *v, int n)
{
	qsort(v, n, sizeof(*v), cmp_u64);
	return n & 1 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}

static GLuint fbo8, fbo10;
static uint8_t *pixels;		// W*H*4 readback buffer

static void run(int scene, int mode, int path, int frames, uint64_t cap_us, struct sink *s, int hold_ms)
{
	uint64_t *total = calloc(frames, sizeof(uint64_t)), *rd = calloc(frames, sizeof(uint64_t)), *cp = calloc(frames, sizeof(uint64_t));
	const int panel = path != PATH_RGBA8;
	const GLenum type = panel ? GL_UNSIGNED_INT_2_10_10_10_REV : GL_UNSIGNED_BYTE;
	const uint64_t begin = now_us();
	int n = 0;

	glBindFramebuffer(GL_FRAMEBUFFER, panel ? fbo10 : fbo8);
	// one untimed frame: first use of a program/target pair compiles and allocates
	draw_scene(scene, 0, panel);
	glFinish();
	for (int f = 0; f < frames && !stop_requested; f++) {
		uint64_t t0 = now_us(), t1, t2;
		draw_scene(scene, f, panel);
		if (mode == MODE_RENDER) {
			glFinish();
			t1 = t2 = now_us();
		} else if (mode == MODE_PRESENT && path == PATH_DIRECT) {
			glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(s->stride / 4));
			glReadPixels(0, 0, W, H, GL_RGBA, type, s->base);
			glPixelStorei(GL_PACK_ROW_LENGTH, 0);
			t1 = t2 = now_us();
		} else {
			glReadPixels(0, 0, W, H, GL_RGBA, type, pixels);
			t1 = t2 = now_us();
			if (mode == MODE_PRESENT) {
				if (panel)
					sink_present_words(s, pixels);
				else
					sink_present_rgba8(s, pixels);
				t2 = now_us();
			}
		}
		total[n] = t2 - t0; rd[n] = t1 - t0; cp[n] = t2 - t1;
		n++;
		if (now_us() - begin > cap_us && n >= 5)
			break;
	}
	GLenum err = glGetError();
	if (mode == MODE_PRESENT && s->saved) {
		if (hold_ms > 0)
			usleep((useconds_t)hold_ms * 1000);
		sink_restore(s);	// console back before anything is printed
	}
	if (n > 0) {
		uint64_t mn = total[0], mx = total[0];
		for (int i = 1; i < n; i++) {
			if (total[i] < mn) mn = total[i];
			if (total[i] > mx) mx = total[i];
		}
		uint64_t med = median(total, n);
		printf("AGXBENCH scene=%s mode=%s frames=%d median_us=%llu min_us=%llu max_us=%llu fps=%.1f path=%s sink=%s read_us=%llu copy_us=%llu%s\n",
		    scene_names[scene], mode_names[mode], n, (unsigned long long)med, (unsigned long long)mn,
		    (unsigned long long)mx, med ? 1e6 / (double)med : 0.0, path_names[path],
		    mode == MODE_PRESENT ? s->kind : "none", (unsigned long long)median(rd, n),
		    (unsigned long long)median(cp, n), err != GL_NO_ERROR ? " gl_error=1" : "");
	}
	free(total); free(rd); free(cp);
}

// rgba8 frame (bottom row first) against the panel words of the rgb10 frame (top row first)
static int check_paths(int scene, const uint8_t *rgba, const uint8_t *words)
{
	size_t differ = 0, flat = 1;
	unsigned max_diff = 0;
	// a frame of one single value is a failed render, however well the two paths agree
	for (size_t i = 1; i < (size_t)W * H && flat; i++)
		flat = memcmp(rgba, rgba + 4 * i, 4) == 0;
	for (int y = 0; y < H; y++) {
		const uint8_t *src = rgba + (size_t)(H - 1 - y) * W * 4;
		const uint32_t *w = (const uint32_t *)(words + (size_t)y * W * 4);
		for (int x = 0; x < W; x++) {
			unsigned worst = 0;
			for (int ch = 0; ch < 3; ch++) {
				unsigned v8 = src[4 * x + ch], a = v8 << 2 | v8 >> 6, b = (w[x] >> (20 - 10 * ch)) & 0x3ff;
				unsigned d = a > b ? a - b : b - a;
				if (d > worst) worst = d;
			}
			if (worst > max_diff) max_diff = worst;
			differ += worst > 16;
		}
	}
	int ok = differ * 50 <= (size_t)W * H && !flat;
	printf("AGXCHECK scene=%s differ_px=%zu of=%d max_diff=%u flat=%d result=%s\n", scene_names[scene], differ,
	    W * H, max_diff, (int)flat, ok ? "ok" : "FAIL");
	return ok;
}

static GLuint make_fbo(GLenum internal, GLuint depth)
{
	GLuint fbo, tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexStorage2D(GL_TEXTURE_2D, 1, internal, W, H);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf(T "framebuffer 0x%x incomplete: 0x%x\n", internal, glCheckFramebufferStatus(GL_FRAMEBUFFER));
		return 0;
	}
	return fbo;
}

int main(int argc, char **argv)
{
	const char *require = NULL, *scenes = "plasma,quads,mesh", *dump = NULL;
	int frames = 30, budget_s = 50, no_present = 0, hold_ms = 0, size_given = 0;
	struct sink sink = { 0 };

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--size") && i + 1 < argc && sscanf(argv[i + 1], "%dx%d", &W, &H) == 2) { size_given = 1; i++; }
		else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--budget") && i + 1 < argc) budget_s = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--scenes") && i + 1 < argc) scenes = argv[++i];
		else if (!strcmp(argv[i], "--hold-ms") && i + 1 < argc) hold_ms = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--require-renderer") && i + 1 < argc) require = argv[++i];
		else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump = argv[++i];
		else if (!strcmp(argv[i], "--no-present")) no_present = 1;
		else {
			printf("usage: %s [--size WxH] [--frames N] [--budget s] [--scenes plasma,quads,mesh] "
			    "[--no-present] [--hold-ms N] [--require-renderer substr] [--dump prefix]\n", argv[0]);
			return 2;
		}
	}
	if (frames < 5 || frames > 10000 || W < 16 || H < 16 || W > 8192 || H > 8192) {
		printf(T "bad --frames or --size\n");
		return 2;
	}
	(void)size_given;
	setvbuf(stdout, NULL, _IOLBF, 0);
	const uint64_t start = now_us(), deadline = start + (uint64_t)budget_s * 1000000u;

#ifdef HAVE_PDGOP
	if (!no_present) {
		kern_return_t kr = PDGOPOpen(&sink.fb);
		if (kr != KERN_SUCCESS) {
			printf(T "PDGOPOpen failed: 0x%x at %s; presenting to a heap buffer\n", kr, PDGOPLastErrorStage());
		} else if (PDGOPGetServiceKind(&sink.fb) != kPDGOPServiceDirectMemory || sink.fb.bpp != 32 ||
		    sink.fb.componentMasks[0] != 0x3ff00000 || sink.fb.componentMasks[1] != 0x000ffc00 ||
		    sink.fb.componentMasks[2] != 0x000003ff || sink.fb.address == 0 || (sink.fb.address & 3) ||
		    sink.fb.stride < sink.fb.width * 4 || (sink.fb.stride & 3) ||
		    (uint64_t)sink.fb.stride * sink.fb.height > sink.fb.size ||
		    (size_given && ((int)sink.fb.width != W || (int)sink.fb.height != H))) {
			printf(T "framebuffer %ux%u stride %u bpp %u masks %08x/%08x/%08x is not a directly mapped "
			    "x2r10g10b10 panel of the requested size; presenting to a heap buffer\n", sink.fb.width,
			    sink.fb.height, sink.fb.stride, sink.fb.bpp, sink.fb.componentMasks[0],
			    sink.fb.componentMasks[1], sink.fb.componentMasks[2]);
			PDGOPClose(&sink.fb);
		} else {
			W = (int)sink.fb.width;
			H = (int)sink.fb.height;
			sink.base = (uint8_t *)(uintptr_t)sink.fb.address;
			sink.stride = sink.fb.stride;
			sink.kind = "panel";
			sink.open = 1;
		}
	}
#endif
	if (!sink.base && !no_present) {
		sink.stride = (uint32_t)W * 4;
		sink.base = calloc((size_t)sink.stride, H);
		sink.kind = "heap";
	}
	pixels = malloc((size_t)W * H * 4);
	uint8_t *pixels8 = malloc((size_t)W * H * 4);
	sink.row = malloc((size_t)W * 4);
	if (!pixels || !pixels8 || !sink.row || (!no_present && !sink.base)) {
		printf(T "out of memory\n");
		return 2;
	}
	if (sink.open) {
		// the console image, read once; every later access to the mapping is a write
		sink.saved = malloc((size_t)W * H * 4);
		if (!sink.saved) {
			printf(T "out of memory\n");
			return 2;
		}
		for (int y = 0; y < H; y++)
			memcpy(sink.saved + (size_t)y * W * 4, sink.base + (size_t)y * sink.stride, (size_t)W * 4);
	}
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	PFNEGLGETPLATFORMDISPLAYEXTPROC get_display =
	    (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	static const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	EGLint major, minor, ncfg = 0;
	EGLConfig cfg;
	EGLDisplay dpy = get_display ? get_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL) : EGL_NO_DISPLAY;
	if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) {
		printf(T "eglInitialize failed: 0x%x\n", eglGetError());
		return 2;
	}
	eglBindAPI(EGL_OPENGL_ES_API);
	eglChooseConfig(dpy, (EGLint[]){ EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE }, &cfg, 1, &ncfg);
	EGLContext ctx = eglCreateContext(dpy, ncfg > 0 ? cfg : EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, ctx_attribs);
	if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
		printf(T "GLES3 context failed: 0x%x\n", eglGetError());
		return 2;
	}
	const char *renderer = (const char *)glGetString(GL_RENDERER);
	printf(T "GL_RENDERER=%s\n", renderer);
	printf(T "GL_VERSION=%s\n", (const char *)glGetString(GL_VERSION));
	if (require && (!renderer || !strstr(renderer, require))) {
		printf(T "FAIL: renderer does not contain \"%s\"\n", require);
		return 1;
	}

	GLuint depth;
	glGenRenderbuffers(1, &depth);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, H);
	fbo8 = make_fbo(GL_RGBA8, depth);
	fbo10 = make_fbo(GL_RGB10_A2, depth);
	if (!fbo8)
		return 2;
	// the 10-bit path needs the driver to hand RGB10_A2 back as packed 2:10:10:10 words
	int have10 = 0;
	if (fbo10) {
		GLint fmt = 0, type = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, fbo10);
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &fmt);
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);
		have10 = fmt == GL_RGBA && type == GL_UNSIGNED_INT_2_10_10_10_REV;
		printf(T "RGB10_A2 read format 0x%x type 0x%x: 10-bit path %s\n", fmt, type, have10 ? "on" : "off");
	}
	glDisable(GL_DITHER);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	setup_plasma();
	setup_quads();
	setup_mesh();
	printf(T "size=%dx%d frames=%d budget_s=%d sink=%s stride=%u setup_ms=%llu\n", W, H, frames, budget_s,
	    no_present ? "none" : sink.kind, sink.stride, (unsigned long long)(now_us() - start) / 1000);

	static const struct { int mode, path; } plan[] = {
		{ MODE_RENDER, PATH_RGBA8 }, { MODE_READBACK, PATH_RGBA8 }, { MODE_PRESENT, PATH_RGBA8 },
		{ MODE_RENDER, PATH_RGB10 }, { MODE_READBACK, PATH_RGB10 }, { MODE_PRESENT, PATH_RGB10 },
		{ MODE_PRESENT, PATH_DIRECT },
	};
	int skipped = 0, checks_failed = 0;
	for (int scene = 0; scene < NSCENES && !stop_requested; scene++) {
		char hex[65];
		if (!strstr(scenes, scene_names[scene]))
			continue;
		// regression hashes: frame HASH_FRAME on both targets, before any timing
		glBindFramebuffer(GL_FRAMEBUFFER, fbo8);
		draw_scene(scene, HASH_FRAME, 0);
		glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels8);
		sha256_hex(pixels8, (size_t)W * H * 4, hex);
		if (dump) {
			char name[512];
			snprintf(name, sizeof(name), "%s-%s.ppm", dump, scene_names[scene]);
			FILE *f = fopen(name, "wb");
			if (f) {
				fprintf(f, "P6\n%d %d\n255\n", W, H);
				for (int y = H - 1; y >= 0; y--)
					for (int x = 0; x < W; x++)
						fwrite(pixels8 + ((size_t)y * W + x) * 4, 1, 3, f);
				fclose(f);
			}
		}
		printf("AGXHASH scene=%s target=rgba8 frame=%d size=%dx%d sha256=%s\n", scene_names[scene], HASH_FRAME, W, H, hex);
		if (have10) {
			glBindFramebuffer(GL_FRAMEBUFFER, fbo10);
			draw_scene(scene, HASH_FRAME, 1);
			glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, pixels);
			sha256_hex(pixels, (size_t)W * H * 4, hex);
			printf("AGXHASH scene=%s target=rgb10 frame=%d size=%dx%d sha256=%s\n", scene_names[scene], HASH_FRAME, W, H, hex);
			checks_failed += !check_paths(scene, pixels8, pixels);
		}
		for (unsigned k = 0; k < sizeof(plan) / sizeof(plan[0]) && !stop_requested; k++) {
			if (plan[k].path != PATH_RGBA8 && !have10)
				continue;
			if (plan[k].mode == MODE_PRESENT && no_present)
				continue;
			if (now_us() > deadline) {
				printf("AGXBENCH_SKIP scene=%s mode=%s path=%s reason=budget\n", scene_names[scene],
				    mode_names[plan[k].mode], path_names[plan[k].path]);
				skipped++;
				continue;
			}
			run(scene, plan[k].mode, plan[k].path, frames, 2000000, &sink, hold_ms);
		}
	}
	GLenum err = glGetError();
	sink_restore(&sink);
#ifdef HAVE_PDGOP
	if (sink.open)
		PDGOPClose(&sink.fb);
#endif
	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(dpy, ctx);
	eglTerminate(dpy);
	printf("AGXBENCH_DONE status=%s skipped=%d seconds=%.1f\n",
	    stop_requested ? "interrupted" : err != GL_NO_ERROR ? "gl-error" : checks_failed ? "check-failed" : "ok",
	    skipped, (double)(now_us() - start) / 1e6);
	return stop_requested || err != GL_NO_ERROR || checks_failed ? 1 : 0;
}
