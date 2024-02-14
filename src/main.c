#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "base/base.h"
#include "gfx/gfx.h"
#include "gfx/opengl/opengl.h"
#include "gfx/opengl/opengl_helpers.h"

#define WIDTH 1280
#define HEIGHT 720

#define STRINGIFY(s) #s
#define GLSL_SOURCE(version, shader) "#version " #version " core \n" STRINGIFY(shader)

static const char* basic_vert = GLSL_SOURCE(
    330,
    layout (location = 0) in vec2 a_pos;

    uniform mat3 u_view_mat;

    void main() {
       vec2 pos = (u_view_mat * vec3(a_pos, 1.0)).xy;
       gl_Position = vec4(pos, 0.0, 1.0);
    }
);

static const char* basic_frag = GLSL_SOURCE(
    330,
    layout (location = 0) out vec4 out_col;

    uniform vec4 u_col;

    void main() {
       out_col = u_col;
    }
);

#define LINE_AA_SMOOTHING 3

static const char* line_seg_vert = GLSL_SOURCE(
    330,
    layout (location = 0) in vec2 a_pos;

    uniform mat3 u_view_mat;

    out float side;

    void main() {
        side = ((gl_VertexID % 2) - 0.5) * 2.0;

        vec2 pos = (u_view_mat * vec3(a_pos, 1.0)).xy;
        gl_Position = vec4(pos, 0.0, 1.0);
    }
);

static const char* line_seg_frag = GLSL_SOURCE(
    330,
    layout (location = 0) out vec4 out_col;

    uniform vec4 u_col;
    uniform float u_display_width;

    in float side;

    void main() {
        float b = smoothstep(0.0, LINE_AA_SMOOTHING / u_display_width, 1.0 - abs(side));
        out_col = vec4(u_col.xyz, u_col.w * b);
    }
);

#define TANGENT_EPSILON 1e-5

static const char* corner_vert = GLSL_SOURCE(
    330,
    layout (location = 0) in vec2 a_p0;
    layout (location = 1) in vec2 a_p1;
    layout (location = 2) in vec2 a_p2;

    layout (location = 3) in float a_line_width;

    uniform mat3 u_view_mat;
    uniform vec2 u_screen;

    out vec2 pos;
    flat out vec2 p0;
    flat out vec2 p1;
    flat out vec2 p2;
    flat out float line_width;
    flat out float display_width;

    // Returns length of z component
    float crs(vec2 a, vec2 b) {
        return a.x * b.y - a.y * b.x;
    }

    void main() {
        p0 = a_p0;
        p1 = a_p1;
        p2 = a_p2;
        line_width = a_line_width;

        vec2 l1 = normalize(p1 - p0);
        vec2 n1 = vec2(-l1.y, l1.x);
        vec2 l2 = normalize(p2 - p1);
        vec2 n2 = vec2(-l2.y, l2.x);

        display_width = length((u_view_mat * vec3(n1 * line_width, 0.0)).xy * (u_screen * 0.5));

        vec2 line_sum = l1 + l2;
        vec2 tangent;
        vec2 miter;
        float miter_scale;
        if (dot(line_sum, line_sum) < TANGENT_EPSILON) {
            tangent = l1;
            miter = n1;
            miter_scale = 1.0;
        } else {
            tangent = normalize(l1 + l2);
            miter = vec2(-tangent.y, tangent.x);
            miter_scale = 1.0 / dot(miter, n1);
        }


        float half_w = a_line_width * 0.5;
        float s = -sign(crs(p1 - p0, p2 - p1));
        // TODO: Is this line necessary?
        if (s == 0.0) { s = 1.0; }

        // Points on the middle of the lines
        vec2 l1_p = (p1 - miter * (s * half_w * miter_scale)) + n1 * (s * half_w);
        vec2 l2_p = (p1 - miter * (s * half_w * miter_scale)) + n2 * (s * half_w);

        // Getting parametric t values for the bottom of the corner triangles
        vec2 l1_vec = p1 - p0;
        vec2 l2_vec = p1 - p2;

        float t1_unclamped = dot(l1_p - p0, l1_vec) / dot(l1_vec, l1_vec);
        float t2_unclamped = dot(l2_p - p2, l2_vec) / dot(l2_vec, l2_vec);

        float t1 = clamp(t1_unclamped, 0.0, 1.0);
        float t2 = clamp(t2_unclamped, 0.0, 1.0);

        l1_p = p0 + l1_vec * t1;
        l2_p = p2 + l2_vec * t2;

        // gl_VertexID == 1 || gl_VertexID == 3
        if ((gl_VertexID % 2) == 1) {
            if (dot(line_sum, line_sum) < TANGENT_EPSILON) {
                pos = gl_VertexID == 1 ?
                    p1 + n1 * s * half_w + l1 * line_width :
                    p1 - n1 * s * half_w + l1 * line_width;
            } else {
                // Points for line cap calculations
                // (i1, i2) and (i3, i4) define two lines
                // These verts sit at the intersection of those lines
                vec2 i1 = p1 + miter * (s * half_w);
                vec2 i2 = i1 - tangent;
                vec2 i3 = (p1 - miter * (s * half_w * miter_scale)) + (n1 * s * a_line_width);
                vec2 i4 = i3 + l1;

                vec2 c1 = (l1 * -crs(i1, i2) - tangent * crs(i3, i4)) / crs(tangent, -l1);
                vec2 c2 = c1 + 2.0 * (i1 - c1);

                pos = gl_VertexID == 1 ? c1 : c2;
            }
        } else if (gl_VertexID == 2) {
            pos = t1_unclamped > t2_unclamped ?
                l1_p - n1 * s * half_w : l2_p - n2 * s * half_w;
        } else {
            pos = gl_VertexID == 0 ? 
                l1_p + n1 * s * half_w : l2_p + n2 * s * half_w;
        }

        vec2 screen_pos = (u_view_mat * vec3(pos, 1.0)).xy;
        gl_Position = vec4(screen_pos, 0.0, 1.0);
    }
);

static const char* corner_frag = GLSL_SOURCE(
    330,
    layout (location = 0) out vec4 out_col;

    uniform vec4 u_col;

    in vec2 pos;
    flat in vec2 p0;
    flat in vec2 p1;
    flat in vec2 p2;
    flat in float line_width;
    flat in float display_width;

    float line_seg_sdf(vec2 p, vec2 a, vec2 b) {
        vec2 ba = b - a;
        vec2 pa = p - a;
        float t = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        return length(pa - t * ba);
    }

    void main() {
        float dist = min(line_seg_sdf(pos, p0, p1), line_seg_sdf(pos, p1, p2)) - line_width * 0.5;
        dist /= line_width;
        float alpha = smoothstep(0.0, -(LINE_AA_SMOOTHING * 0.5f) / display_width, dist);
        out_col = vec4(u_col.xyz, u_col.w * alpha);
    }
);

#if defined(PLATFORM_LINUX)

void os_time_init(void) { }
u64 os_now_usec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
void os_sleep_ms(u64 ms) {
    usleep(ms * 1000);
}

#elif defined(PLATFORM_WIN32)

static u64 w32_ticks_per_sec = 1;
void os_time_init(void) {
    LARGE_INTEGER perf_freq;
    if (QueryPerformanceFrequency(&perf_freq)) {
        w32_ticks_per_sec = ((u64)perf_freq.HighPart << 32) | perf_freq.LowPart;
    } else {
        fprintf(stderr, "Failed to initialize time: could not get performance frequency\n");
    }
}
u64 os_now_usec(void) {
    u64 out = 0;
    LARGE_INTEGER perf_count;
    if (QueryPerformanceCounter(&perf_count)) {
        u64 ticks = ((u64)perf_count.HighPart << 32) | perf_count.LowPart;
        out = ticks * 1000000 / w32_ticks_per_sec;
    } else {
        fprintf(stderr, "Failed to retrive time in micro seconds\n");
    }
    return out;
}
void os_sleep_ms(u64 ms) {
    Sleep(ms);
}
#endif

// Line vertex data
typedef struct {
    vec2f pos;
} line_vert;

// Line corner instance data
typedef struct {
    vec2f p0;
    vec2f p1;
    vec2f p2;

    f32 line_width;
} line_corner;

/*
// Right now, this value is arbitrary
#define LINE_POINT_BUCKET_SIZE 64

typedef struct line_point_bucket {
    u32 size;
    vec2f points[LINE_POINT_BUCKET_SIZE];
    struct line_point_bucket* next;
} line_point_bucket;
*/

// TODO: make free list allocator

typedef struct {
    u32 line_program;
    u32 line_view_mat_loc;
    u32 line_display_width_loc;
    u32 line_col_loc;

    u32 corner_program;
    u32 corner_view_mat_loc;
    u32 corner_screen_loc;
    u32 corner_col_loc;
} draw_lines_shaders;

draw_lines_shaders* draw_lines_shaders_create(mg_arena* arena) {
    draw_lines_shaders* shaders = MGA_PUSH_ZERO_STRUCT(arena, draw_lines_shaders);

    shaders->line_program = glh_create_shader(line_seg_vert, line_seg_frag);
    shaders->corner_program = glh_create_shader(corner_vert, corner_frag);

    glUseProgram(shaders->line_program);
    shaders->line_view_mat_loc = glGetUniformLocation(shaders->line_program, "u_view_mat");
    shaders->line_display_width_loc = glGetUniformLocation(shaders->line_program, "u_display_width");
    shaders->line_col_loc = glGetUniformLocation(shaders->line_program, "u_col");

    glUseProgram(shaders->corner_program);
    shaders->corner_view_mat_loc = glGetUniformLocation(shaders->corner_program, "u_view_mat");
    shaders->corner_screen_loc = glGetUniformLocation(shaders->corner_program, "u_screen");
    shaders->corner_col_loc = glGetUniformLocation(shaders->corner_program, "u_col");

    glUseProgram(0);

    return shaders;
}

void draw_lines_shaders_destroy(draw_lines_shaders* shaders) {
    glDeleteProgram(shaders->line_program);
    glDeleteProgram(shaders->corner_program);
}

// Static lines right now
// TODO: dynamically add points
typedef struct {
    vec4f color;
    f32 width;

    // Number of total points
    u32 num_points;
    vec2f* points;

    u32 num_verts;
    u32 num_indices;
    u32 num_corners;
    
    // GL Stuff
    u32 segment_array;
    u32 corner_array;

    u32 vert_buffer;
    u32 index_buffer;
    u32 corner_buffer;
} draw_lines;

#define LINE_MITER_LIMIT 1.5f

b32 _is_corner(vec2f p0, vec2f p1, vec2f p2) {
    vec2f l1 = vec2f_nrm(vec2f_sub(p1, p0));
    vec2f n1 = vec2f_prp(l1);
    vec2f l2 = vec2f_nrm(vec2f_sub(p2, p1));

    // Avoiding issues with infinite miter projection
    vec2f line_sum = vec2f_add(l1, l2);
    vec2f tangent;
    f32 miter_scale;
    if (vec2f_sqr_len(line_sum) < TANGENT_EPSILON) {
        tangent = l1;
        miter_scale = 1.0f;
    } else {
        tangent = vec2f_nrm(vec2f_add(l1, l2));
        vec2f miter = vec2f_prp(tangent);
        miter_scale = 1.0f / vec2f_dot(miter, n1);
    }

    return miter_scale >= LINE_MITER_LIMIT || vec2f_sqr_len(vec2f_add(l1, l2)) <= TANGENT_EPSILON;
}

// Creates the OpenGL geometry with the new width and color
void draw_lines_update(draw_lines* lines, f32 line_width) {
    if (lines == NULL || lines->num_points == 0) {
        fprintf(stderr, "Cannot update lines: invalid lines object\n");
        return;
    }

    lines->width = line_width;

    mga_temp scratch = mga_scratch_get(NULL, 0);

    line_vert* verts = MGA_PUSH_ZERO_ARRAY(scratch.arena, line_vert, lines->num_verts);
    line_corner* corners = MGA_PUSH_ZERO_ARRAY(scratch.arena, line_corner, lines->num_corners);
    
    u32 num_verts = 0;
    u32 num_corners = 0;

    if (lines->num_points == 1) {
        // Two corners form a circle here
        corners[num_corners++] = (line_corner){
            vec2f_add(lines->points[0], (vec2f){ line_width * 1.1f, 0.0f }),
            lines->points[0],
            vec2f_add(lines->points[0], (vec2f){ line_width * 1.1f, 0.0f }),
            lines->width
        };
        corners[num_corners++] = (line_corner){
            vec2f_sub(lines->points[0], (vec2f){ line_width * 1.1f, 0.0f }),
            lines->points[0],
            vec2f_sub(lines->points[0], (vec2f){ line_width * 1.1f, 0.0f }),
            lines->width
        };
    } else {
        // Points
        vec2f p0, p1, p2;
        // Lines and normals
        vec2f l1, n1, l2, n2;

        f32 half_w = lines->width * 0.5f;
        
        p0 = lines->points[0];
        p1 = lines->points[1];

        l1 = vec2f_nrm(vec2f_sub(p1, p0));
        n1 = vec2f_prp(l1);

        // Corner for rounded line cap
        corners[num_corners++] = (line_corner){ p1, p0, p1, lines->width };

        verts[num_verts++] = (line_vert){ vec2f_sub(p0, vec2f_scl(n1, half_w)) };
        verts[num_verts++] = (line_vert){ vec2f_add(p0, vec2f_scl(n1, half_w)) };

        for (u32 i = 1; i < lines->num_points - 1; i++) {
            p0 = lines->points[i - 1];
            p1 = lines->points[i];
            p2 = lines->points[i + 1];

            l1 = vec2f_nrm(vec2f_sub(p1, p0));
            n1 = vec2f_prp(l1);
            l2 = vec2f_nrm(vec2f_sub(p2, p1));
            n2 = vec2f_prp(l2);

            // Avoiding issues with infinite miter projection
            vec2f line_sum = vec2f_add(l1, l2);
            vec2f tangent, miter;
            f32 miter_scale;
            if (vec2f_sqr_len(line_sum) < TANGENT_EPSILON) {
                tangent = l1;
                miter = n1;
                miter_scale = 1.0f;
            } else {
                tangent = vec2f_nrm(vec2f_add(l1, l2));
                miter = vec2f_prp(tangent);
                miter_scale = 1.0f / vec2f_dot(miter, n1);
            }

            f32 line_cross = vec2f_crs(vec2f_sub(p1, p0), vec2f_sub(p2, p1));
            // Some corner operations depend on which side of the points p1 is on
            f32 s = -SIGN(line_cross);

            if (miter_scale < LINE_MITER_LIMIT && vec2f_sqr_len(line_sum) > TANGENT_EPSILON) {
                verts[num_verts++] = (line_vert){ vec2f_sub(p1, vec2f_scl(miter, half_w * miter_scale)) };
                verts[num_verts++] = (line_vert){ vec2f_add(p1, vec2f_scl(miter, half_w * miter_scale)) };
            } else {
                corners[num_corners++] = (line_corner){ 
                    p0, p1, p2, lines->width
                };

                // Point in the middle of line 1
                vec2f l1_p = vec2f_add(
                    vec2f_sub(p1, vec2f_scl(miter, s * half_w * miter_scale)),
                    vec2f_scl(n1, s * half_w)
                );
                // Point in the middle of line 2
                vec2f l2_p = vec2f_add(
                    vec2f_sub(p1, vec2f_scl(miter, s * half_w * miter_scale)),
                    vec2f_scl(n2, s * half_w)
                );

                // Getting parametric values for the line points
                vec2f l1_vec = vec2f_sub(p1, p0);
                f32 t1_unclamped = vec2f_dot(vec2f_sub(l1_p, p0), l1_vec) / vec2f_dot(l1_vec, l1_vec);
                f32 t1 = CLAMP(t1_unclamped, 0, 1);

                vec2f l2_vec = vec2f_sub(p1, p2);
                f32 t2_unclamped = vec2f_dot(vec2f_sub(l2_p, p2), l2_vec) / vec2f_dot(l2_vec, l2_vec);
                f32 t2 = CLAMP(t2_unclamped, 0, 1);

                l1_p = vec2f_add(vec2f_scl(l1_vec, t1), p0);
                l2_p = vec2f_add(vec2f_scl(l2_vec, t2), p2);

                if (s == 1.0f) {
                    verts[num_verts++] = (line_vert){ vec2f_sub(l1_p, vec2f_scl(n1, s * half_w)) };
                    verts[num_verts++] = (line_vert){ vec2f_add(l1_p, vec2f_scl(n1, s * half_w)) };
                    verts[num_verts++] = (line_vert){ vec2f_sub(l2_p, vec2f_scl(n2, s * half_w)) };
                    verts[num_verts++] = (line_vert){ vec2f_add(l2_p, vec2f_scl(n2, s * half_w)) };
                } else {
                    verts[num_verts++] = (line_vert){ vec2f_add(l1_p, vec2f_scl(n1, s * half_w)) };
                    verts[num_verts++] = (line_vert){ vec2f_sub(l1_p, vec2f_scl(n1, s * half_w)) };
                    verts[num_verts++] = (line_vert){ vec2f_add(l2_p, vec2f_scl(n2, s * half_w)) };
                    verts[num_verts++] = (line_vert){ vec2f_sub(l2_p, vec2f_scl(n2, s * half_w)) };
                }
            }
        }

        p1 = lines->points[lines->num_points - 2];
        p2 = lines->points[lines->num_points - 1];

        l2 = vec2f_nrm(vec2f_sub(p2, p1));
        n2 = vec2f_prp(l2);

        corners[num_corners++] = (line_corner){ p1, p2, p1, lines->width };

        verts[num_verts++] = (line_vert){ vec2f_sub(p2, vec2f_scl(n2, half_w)) };
        verts[num_verts++] = (line_vert){ vec2f_add(p2, vec2f_scl(n2, half_w)) };
    }

    glBindBuffer(GL_ARRAY_BUFFER, lines->vert_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(line_vert) * lines->num_verts, verts);
    glBindBuffer(GL_ARRAY_BUFFER, lines->corner_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(line_corner) * lines->num_corners, corners);

    mga_scratch_release(scratch);
}
draw_lines* draw_lines_from_points(mg_arena* arena, vec2f* points, u32 num_points, f32 line_width) {
    if (num_points == 0) {
        fprintf(stderr, "Cannot create lines with zero points\n");
        return NULL;
    }

    draw_lines* lines = MGA_PUSH_ZERO_STRUCT(arena, draw_lines);

    lines->color = (vec4f){ 1.0f, 1.0f, 1.0f, 1.0f };
    lines->width = line_width;

    lines->num_points = num_points;
    // TODO: bad, but I am just testing now
    lines->points = points;

    lines->num_indices = (num_points - 1) * 6;
    if (num_points == 1) {
        // Two corners will make a circle
        lines->num_corners = 2;
    } else {
        // At least two for end caps
        lines->num_corners = 2;
        // At least two for first segment
        lines->num_verts = 2;

        for (u32 i = 1; i < num_points - 1; i++) {
            vec2f p0 = points[i - 1];
            vec2f p1 = points[i];
            vec2f p2 = points[i + 1];

            if (_is_corner(p0, p1, p2)) {
                lines->num_corners++;
                lines->num_verts += 4;
            } else {
                lines->num_verts += 2;
            }
        }

        // End of last line segment
        lines->num_verts += 2;
    }

    // Indices do not change here, so they can be computed beforehand
    mga_temp scratch = mga_scratch_get(NULL, 0);
    u32* indices = MGA_PUSH_ZERO_ARRAY(scratch.arena, u32, lines->num_indices);

    if (num_points > 1) {
        u32 num_indices = 0;
        u32 num_verts = 2;

        for (u32 i = 1; i < num_points - 1; i++) {
            vec2f p0 = points[i - 1];
            vec2f p1 = points[i];
            vec2f p2 = points[i + 1];

            if (_is_corner(p0, p1, p2)) {
                num_verts += 4;

                indices[num_indices++] = num_verts - 6;
                indices[num_indices++] = num_verts - 5;
                indices[num_indices++] = num_verts - 4;

                indices[num_indices++] = num_verts - 5;
                indices[num_indices++] = num_verts - 3;
                indices[num_indices++] = num_verts - 4;
            } else {
                num_verts += 2;

                indices[num_indices++] = num_verts - 4;
                indices[num_indices++] = num_verts - 3;
                indices[num_indices++] = num_verts - 2;

                indices[num_indices++] = num_verts - 3;
                indices[num_indices++] = num_verts - 1;
                indices[num_indices++] = num_verts - 2;
            }
        }

        num_verts += 2;

        indices[num_indices++] = num_verts - 4;
        indices[num_indices++] = num_verts - 3;
        indices[num_indices++] = num_verts - 2;

        indices[num_indices++] = num_verts - 3;
        indices[num_indices++] = num_verts - 1;
        indices[num_indices++] = num_verts - 2;
    }

    // OpenGL stuff

    glGenVertexArrays(1, &lines->segment_array);
    glBindVertexArray(lines->segment_array);

    lines->vert_buffer = glh_create_buffer(GL_ARRAY_BUFFER, sizeof(line_vert) * lines->num_verts, NULL, GL_DYNAMIC_DRAW);
    lines->index_buffer = glh_create_buffer(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * lines->num_indices, indices, GL_STATIC_DRAW);

    mga_scratch_release(scratch);

    glGenVertexArrays(1, &lines->corner_array);
    glBindVertexArray(lines->corner_array);

    lines->corner_buffer = glh_create_buffer(GL_ARRAY_BUFFER, sizeof(line_corner) * lines->num_corners, NULL, GL_DYNAMIC_DRAW);

    // Computing the initial geometry
    draw_lines_update(lines, line_width);

    return lines;
}
void draw_lines_draw(const draw_lines* lines, const draw_lines_shaders* shaders, const gfx_window* win, viewf view) {
    mat3f view_mat = { 0 };
    mat3f_from_view(&view_mat, view);

    // Drawing line segments
    glUseProgram(shaders->line_program);
    glUniformMatrix3fv(shaders->line_view_mat_loc, 1, GL_FALSE, view_mat.m);
    glUniform4f(shaders->line_col_loc, 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform1f(shaders->line_display_width_loc, lines->width * ((f32)win->width) / view.width);

    glBindVertexArray(lines->segment_array);
    glBindBuffer(GL_ARRAY_BUFFER, lines->vert_buffer);

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(line_vert), (void*)(offsetof(line_vert, pos)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lines->index_buffer);
    glDrawElements(GL_TRIANGLES, lines->num_indices, GL_UNSIGNED_INT, NULL);

    glDisableVertexAttribArray(0);

    // Drawing corners
    glUseProgram(shaders->corner_program);
    glUniformMatrix3fv(shaders->corner_view_mat_loc, 1, GL_FALSE, view_mat.m);
    glUniform4f(shaders->corner_col_loc, 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform2f(shaders->corner_screen_loc, win->width, win->height);

    glBindVertexArray(lines->corner_array);
    glBindBuffer(GL_ARRAY_BUFFER, lines->corner_buffer);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    glVertexAttribDivisor(0, 1);
    glVertexAttribDivisor(1, 1);
    glVertexAttribDivisor(2, 1);
    glVertexAttribDivisor(3, 1);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(line_corner), (void*)offsetof(line_corner, p0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(line_corner), (void*)offsetof(line_corner, p1));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(line_corner), (void*)offsetof(line_corner, p2));
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(line_corner), (void*)offsetof(line_corner, line_width));

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 5, lines->num_corners);

    glVertexAttribDivisor(0, 0);
    glVertexAttribDivisor(1, 0);
    glVertexAttribDivisor(2, 0);
    glVertexAttribDivisor(3, 0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
}
void draw_lines_destroy(draw_lines* lines) {
    glDeleteVertexArrays(1, &lines->segment_array);
    glDeleteVertexArrays(1, &lines->corner_array);

    glDeleteBuffers(1, &lines->vert_buffer);
    glDeleteBuffers(1, &lines->index_buffer);
    glDeleteBuffers(1, &lines->corner_buffer);
}

void mga_err(mga_error err) {
    printf("MGA ERROR %d: %s", err.code, err.msg);
}
int main(void) {
    mga_desc desc = {
        .desired_max_size = MGA_MiB(16),
        .desired_block_size = MGA_KiB(256),
        .error_callback = mga_err
    };
    mg_arena* perm_arena = mga_create(&desc);

    gfx_window* win = gfx_win_create(perm_arena, WIDTH, HEIGHT, STR8("Line Render Test"));

    u32 basic_program = glh_create_shader(basic_vert, basic_frag);

    glUseProgram(basic_program);
    u32 basic_view_mat_loc = glGetUniformLocation(basic_program, "u_view_mat");
    u32 basic_col_loc = glGetUniformLocation(basic_program, "u_col");

    draw_lines_shaders* shaders = draw_lines_shaders_create(perm_arena);

    vec2f points[] = {
        { -250.0f,  250.0f },
        { -125.0f, -250.0f },
        {    0.0f,  250.0f },
        {  125.0f, -250.0f },
        {  250.0f,  250.0f },
    };

    draw_lines* lines = draw_lines_from_points(perm_arena, points, sizeof(points) / sizeof(points[0]), 20.0f);

    vec2f rect_verts[] = {
        { -250.0f,  250.0f },
        { -250.0f, -250.0f },
        {  250.0f, -250.0f },
        {  250.0f,  250.0f }
    };

    u32 rect_indices[] = {
        0, 1, 2, 
        0, 2, 3
    };

    u32 vertex_array = 0;
    glGenVertexArrays(1, &vertex_array);
    glBindVertexArray(vertex_array);

    u32 vertex_buffer = glh_create_buffer(GL_ARRAY_BUFFER, sizeof(rect_verts), rect_verts, GL_STATIC_DRAW);
    u32 index_buffer = glh_create_buffer(GL_ELEMENT_ARRAY_BUFFER, sizeof(rect_indices), rect_indices, GL_STATIC_DRAW);

    glClearColor(0.2f, 0.2f, 0.4f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  

    viewf view = {
        .center = { 0, 0 },
        .aspect_ratio = (f32)win->width / win->height,
        .width = win->width,
        .rotation = 0.0f
    };
    mat3f view_mat = { 0 };
    mat3f inv_view_mat = { 0 };
    mat3f_from_view(&view_mat, view);
    mat3f_inverse(&inv_view_mat, &view_mat);

    gfx_win_process_events(win);
    vec2f prev_mouse_pos = mat3f_mul_vec2f(&inv_view_mat, win->mouse_pos);

    os_time_init();

    u64 prev_frame = os_now_usec();
    while (!win->should_close) {
        u64 cur_frame = os_now_usec();
        f32 delta = (f32)(cur_frame - prev_frame) / 1e6;
        prev_frame = cur_frame;

        gfx_win_process_events(win);

        // Update

        f32 move_speed = 2.0f;

        view.aspect_ratio = (f32)win->width / win->height;
        view.width *= 1.0f + (-0.03f * win->mouse_scroll);

        if (GFX_IS_KEY_DOWN(win, GFX_KEY_W)) {
            view.center.y -= move_speed;
        }
        if (GFX_IS_KEY_DOWN(win, GFX_KEY_S)) {
            view.center.y += move_speed;
        }
        if (GFX_IS_KEY_DOWN(win, GFX_KEY_A)) {
            view.center.x -= move_speed;
        }
        if (GFX_IS_KEY_DOWN(win, GFX_KEY_D)) {
            view.center.x += move_speed;
        }

        if (GFX_IS_KEY_DOWN(win, GFX_KEY_Q)) {
            view.rotation += 0.01f;
        }
        if (GFX_IS_KEY_DOWN(win, GFX_KEY_E)) {
            view.rotation -= 0.01f;
        }

        mat3f_from_view(&view_mat, view);
        mat3f_inverse(&inv_view_mat, &view_mat);

        vec2f mouse_pos = (vec2f){
            2.0f * win->mouse_pos.x / win->width - 1.0f,
            -(2.0f * win->mouse_pos.y / win->height - 1.0f),
        };
        mouse_pos = mat3f_mul_vec2f(&inv_view_mat, mouse_pos);

        if (GFX_IS_MOUSE_DOWN(win, GFX_MB_LEFT)) {
            if (vec2f_sqr_dist(mouse_pos, prev_mouse_pos) > 2.0f) {
            }
        }
        prev_mouse_pos = mouse_pos;

        gfx_win_clear(win);

        // Draw

        // Rect draw
        {
            glUseProgram(basic_program);
            glUniformMatrix3fv(basic_view_mat_loc, 1, GL_FALSE, view_mat.m);

            glUniform4f(basic_col_loc, 0.0f, 0.0f, 0.0f, 1.0f);

            glBindVertexArray(vertex_array);
            glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2f), NULL);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);

            glDisableVertexAttribArray(0);
        }

        draw_lines_draw(lines, shaders, win, view);

        gfx_win_swap_buffers(win);

        os_sleep_ms(8);
    }

    draw_lines_shaders_destroy(shaders);
    draw_lines_destroy(lines);

    glDeleteBuffers(1, &vertex_buffer);
    glDeleteBuffers(1, &index_buffer);
    glDeleteVertexArrays(1, &vertex_array);

    glDeleteProgram(basic_program);

    gfx_win_destroy(win);

    mga_destroy(perm_arena);

    return 0;
}
