#ifndef BASE_MATH_H
#define BASE_MATH_H

#include "base_defs.h"

typedef struct { f32 x, y;       } vec2f;
typedef struct { f32 x, y, z;    } vec3f;
typedef struct { f32 x, y, z, w; } vec4f;

typedef struct { f32 x, y, w, h; } rectf;

typedef struct { f32 m[4];  } mat2f;
typedef struct { f32 m[9];  } mat3f;
typedef struct { f32 m[16]; } mat4f;

typedef struct {
    vec2f center;
    f32 aspect_ratio;
    f32 width;
    f32 rotation;
} viewf;

vec2f vec2f_add(vec2f a, vec2f b);
vec2f vec2f_sub(vec2f a, vec2f b);
vec2f vec2f_scl(vec2f v, f32 s);
f32 vec2f_dot(vec2f a, vec2f b);
// Computes the length of the z vector of the cross product 
f32 vec2f_crs(vec2f a, vec2f b);
f32 vec2f_sqr_len(vec2f v);
f32 vec2f_len(vec2f v);
f32 vec2f_sqr_dist(vec2f a, vec2f b);
f32 vec2f_dist(vec2f a, vec2f b);
vec2f vec2f_nrm(vec2f v);
vec2f vec2f_prp(vec2f v);
b32 vec2f_eq(vec2f a, vec2f b);

void mat3f_transform(mat3f* mat, vec2f scale, vec2f offset, f32 rotation);
void mat3f_from_view(mat3f* mat, viewf v);
void mat3f_inverse(mat3f* out, const mat3f* mat);
vec2f mat3f_mul_vec2f(const mat3f* mat, vec2f v);

#endif // BASE_MATH_H

