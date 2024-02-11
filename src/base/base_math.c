#include "base_math.h"
#include <math.h>
#include <stdio.h>

vec2f vec2f_add(vec2f a, vec2f b) {
    return (vec2f){ a.x + b.x, a.y + b.y };
}
vec2f vec2f_sub(vec2f a, vec2f b) {
    return (vec2f){ a.x - b.x, a.y - b.y };
}
vec2f vec2f_scl(vec2f v, f32 s) {
    return (vec2f){ v.x * s, v.y * s };
}
f32 vec2f_dot(vec2f a, vec2f b) {
    return a.x * b.x + a.y * b.y;
}
f32 vec2f_crs(vec2f a, vec2f b) {
    return a.x * b.y - a.y * b.x;
}
f32 vec2f_sqr_len(vec2f v) {
    return v.x * v.x + v.y * v.y;
}
f32 vec2f_len(vec2f v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}
f32 vec2f_sqr_dist(vec2f a, vec2f b) {
    return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}
f32 vec2f_dist(vec2f a, vec2f b) {
    return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}
vec2f vec2f_prp(vec2f v) {
    return (vec2f){ -v.y, v.x };
}
vec2f vec2f_nrm(vec2f v) {
    f32 r = 1.0f / sqrtf(v.x * v.x + v.y * v.y);
    return (vec2f){ v.x * r, v.y * r };
}
b32 vec2f_eq(vec2f a, vec2f b) {
    return (a.x == b.x && a.y == b.y);
}

void mat3f_transform(mat3f* mat, vec2f scale, vec2f offset, f32 rotation) {
    f32 r_sin = sinf(rotation);
    f32 r_cos = cosf(rotation);

    mat->m[0] = r_cos * scale.x;
    mat->m[1] = -r_sin * scale.y;
    mat->m[2] = 0.0f;
    mat->m[3] = r_sin * scale.x;
    mat->m[4] = r_cos * scale.y;
    mat->m[5] = 0.0f;
    mat->m[6] = offset.x;
    mat->m[7] = offset.y;
    mat->m[8] = 1.0f;
}
void mat3f_from_view(mat3f* mat, viewf v) {
    vec2f scale = {
         2.0f / v.size.x,
        -2.0f / v.size.y,
    };

    f32 r_sin = sinf(v.rotation);
    f32 r_cos = cosf(v.rotation);

    vec2f offset = {
        -(v.center.x * (r_cos * scale.x) + v.center.y * (r_sin * scale.x)),
        -(v.center.x * (-r_sin * scale.y) + v.center.y * (r_cos * scale.y))
    };

    mat3f_transform(mat, scale, offset, v.rotation);
}
void mat3f_inverse(mat3f* out, const mat3f* mat) {
    // Renaming so it is easier to type
    const f32* m = mat->m;
    f32 mat_det =
        m[0] * (m[4] * m[8] - m[5] * m[7]) -
        m[1] * (m[3] * m[8] - m[5] * m[6]) +
        m[2] * (m[3] * m[7] - m[4] * m[6]);

    if (fabsf(mat_det) < 1e-8f) {
        fprintf(stderr, "Cannot inverse mat3: determinant of matrix is near zero\n");
        return;
    }

    f32 inv_det = 1.0f / mat_det;

    out->m[0] = (m[4] * m[8] - m[5] * m[7]) * inv_det;
    out->m[1] = (m[2] * m[7] - m[1] * m[8]) * inv_det;
    out->m[2] = (m[1] * m[5] - m[2] * m[4]) * inv_det;
    out->m[3] = (m[5] * m[6] - m[3] * m[8]) * inv_det;
    out->m[4] = (m[0] * m[8] - m[2] * m[6]) * inv_det;
    out->m[5] = (m[2] * m[3] - m[0] * m[5]) * inv_det;
    out->m[6] = (m[3] * m[7] - m[4] * m[6]) * inv_det;
    out->m[7] = (m[1] * m[6] - m[0] * m[7]) * inv_det;
    out->m[8] = (m[0] * m[4] - m[1] * m[3]) * inv_det;
}
vec2f mat3f_mul_vec2f(const mat3f* mat, vec2f vec) {
    vec2f out = { 0 };

    out.x = vec.x * mat->m[0] + vec.y * mat->m[3] + mat->m[6];
    out.y = vec.x * mat->m[1] + vec.y * mat->m[4] + mat->m[7];

    return out;
}

