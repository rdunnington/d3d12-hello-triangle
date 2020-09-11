#pragma once

#include <math.h>
#include <xmmintrin.h>

#define INLINE   __forceinline
#define M_PI        3.14159265358979323846f
#define DEG2RAD(_a) ((_a)*M_PI/180.0f)
#define RAD2DEG(_a) ((_a)*180.0f/M_PI)
#define INT_MIN     (-2147483647 - 1)
#define INT_MAX     2147483647
#define FLT_MAX     3.402823466e+38F

struct vec4f {
	union {
		float v[4];
		struct { float x; float y; float z; float w; };
	};
};

struct vec3f {
	union {
		float v[3];
		struct { float x; float y; float z; };
	};
};

struct vec2f {
	union {
		float v[2];
		struct { float x; float y; };
	};
};

typedef __m128 vec;

#define VCONST extern const __dclspec(selectany)
struct vconstu {
	union { uint32_t u[4]; vec v; };
};
struct vconstu VEC_SIGNBITS = { 0x80000000, 0x80000000, 0x80000000, 0x80000000, };

INLINE vec vec_xyzw(float x, float y, float z, float w) {
	return _mm_set_ps(z, y, x, w);
}

INLINE vec vec_xyz(float x, float y, float z) {
	return vec_xyzw(x, y, z, 0);
}

INLINE vec vec_xy(float x, float y) {
	return vec_xyzw(x, y, 0, 0);
}

INLINE vec vec_zero() {
	return vec_xyzw(0, 0, 0, 0);
}

INLINE vec vec_splat(float s) {
	return _mm_set1_ps(s);
}

INLINE float vec_x(vec v) {
	return _mm_cvtss_f32(v);
}

INLINE float vec_y(vec v) {
	return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1,1,1,1)));
}

INLINE float vec_z(vec v) {
	return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(2,2,2,2)));
}

INLINE float vec_w(vec v) {
	return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3,3,3,3)));
}

INLINE vec vec_add(vec v1, vec v2) {
	return _mm_add_ps(v1, v2);
}

INLINE vec vec_sub(vec v1, vec v2) {
	return _mm_sub_ps(v1, v2);
}

INLINE vec vec_mul(vec v1, vec v2) {
	return _mm_mul_ps(v1, v2);
}

INLINE vec vec_div(vec v1, vec v2) {
	return _mm_div_ps(v1, v2);
}

INLINE vec vec_scale(vec v, float s) {
	return vec_mul(v, vec_splat(s));
}

INLINE vec vec_scalediv(vec v, float s) {
	return vec_div(v, vec_splat(s));
}

INLINE float vec_sum(vec v) {
	return vec_x(v) + vec_y(v) + vec_z(v);
}

INLINE float vec_dot(vec v1, vec v2) {
	return vec_sum(vec_mul(v1, v2));
}

INLINE float vec_lengthsq(vec v) {
	return vec_dot(v, v);
}

INLINE float vec_length(vec v) {
	return sqrtf(vec_lengthsq(v));
}

INLINE vec vec_normalize(vec v) {
	return vec_scalediv(v, vec_length(v));
}

INLINE vec vec_abs(vec v) {
	return _mm_andnot_ps(VEC_SIGNBITS.v, v);
}

INLINE vec vec_lerp(vec a, vec b, float t) {
	vec c = vec_sub(b, a);
	c = vec_scale(c, t);
	return vec_add(a, c);
}

INLINE struct vec2f vec_vec2f(vec v) {
	struct vec2f copy = {
		.x = vec_x(v),
		.y = vec_y(v),
	};
	return copy;
}

INLINE struct vec3f vec_vec3f(vec v) {
	struct vec3f copy = {
		.x = vec_x(v),
		.y = vec_y(v),
		.z = vec_z(v),
	};
	return copy;
}

INLINE struct vec4f vec_vec4f(vec v) {
	struct vec4f copy = {
		.x = vec_x(v),
		.y = vec_y(v),
		.z = vec_z(v),
		.w = vec_w(v),
	};
	return copy;
}

INLINE struct vec2f vec2f(float x, float y) {
	return (struct vec2f){ x, y };
}

INLINE struct vec3f vec3f(float x, float y, float z) {
	return (struct vec3f){ x, y, z };
}

INLINE struct vec4f vec4f(float x, float y, float z, float w) {
	return (struct vec4f){ x, y, z, w };
}

const struct vec2f VEC2F_ZERO = {0,0};
const struct vec3f VEC3F_ZERO = {0,0,0};
const struct vec4f VEC4F_ZERO = {0,0,0,0};

