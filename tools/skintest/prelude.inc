// Generated harness: runs a translated vertex shader (GLSL subset) on the CPU with dumped draw inputs.
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
typedef uint32_t uint;
struct vec4 { float x, y, z, w; vec4(float a = 0, float b = 0, float c = 0, float d = 0) : x(a), y(b), z(c), w(d) {}
  float &operator[](int i) { return (&x)[i]; } };
struct uvec4 { uint x, y, z, w; uint &operator[](int i) { return (&x)[i]; } };
static inline uint as_u(float f) { uint u; memcpy(&u, &f, 4); return u; }
static inline uint as_u(uint u) { return u; }
static inline uint as_u(int i) { return (uint)i; }
static inline float as_f(uint u) { float f; memcpy(&f, &u, 4); return f; }
static inline float as_f(float f) { return f; }
static inline float as_f(int i) { return as_f((uint)i); }
static inline float fma(float a, float b, float c) { return std::fmaf(a, b, c); }
static inline float inversesqrt(float x) { return 1.0f / std::sqrt(x); }
#define cbr(cb, off) cb[std::min((uint)((off) >> 4), (uint)(sizeof(cb) / sizeof(cb[0]) - 1))][((off) >> 2) & 3u]
static uvec4 cb3[16], cb4[4096];
static vec4 a0, a1, a3, a4, a12; static uvec4 a7, a11;
static vec4 gl_Position, v1, v2, v3, v4, v5, v6, v7, v8;
