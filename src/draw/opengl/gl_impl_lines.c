#include "draw/draw.h"

#ifdef DRAW_BACKEND_OPENGL

#include <stdio.h>

#include "gfx/opengl/opengl.h"
#include "gfx/opengl/opengl_helpers.h"

typedef struct draw_lines_shaders {
    u32 line_program;
    u32 line_view_mat_loc;
    u32 line_display_width_loc;
    u32 line_col_loc;

    u32 corner_program;
    u32 corner_view_mat_loc;
    u32 corner_screen_loc;
    u32 corner_line_width_loc;
    u32 corner_col_loc;
} draw_lines_shaders;

typedef struct _draw_lines_backend {
    // OpenGL objects

    u32 segment_array;
    u32 corner_array;

    u32 vert_buffer;
    u32 index_buffer;
    u32 corner_buffer;
} draw_lines_backend;

// Line vertex data
typedef struct {
    vec2f pos;
} line_vert;

// Line corner instance data
typedef struct {
    vec2f p0;
    vec2f p1;
    vec2f p2;
} line_corner;

#define AA_SMOOTHING 3
#define TANGENT_EPSILON 1e-5
#define MITER_LIMIT 1.5f

static const char* line_seg_vert;
static const char* line_seg_frag;
static const char* corner_vert;
static const char* corner_frag;

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
    shaders->corner_line_width_loc = glGetUniformLocation(shaders->corner_program, "u_line_width");
    shaders->corner_col_loc = glGetUniformLocation(shaders->corner_program, "u_col");

    glUseProgram(0);

    return shaders;

}
void draw_lines_shaders_destroy(draw_lines_shaders* shaders) {
    if (shaders == NULL) {
        fprintf(stderr, "Cannot destroy lines shaders: shaders is NULL\n");
        return;
    }

    glDeleteProgram(shaders->line_program);
    glDeleteProgram(shaders->corner_program);
}

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

    return miter_scale >= MITER_LIMIT || vec2f_sqr_len(vec2f_add(l1, l2)) <= TANGENT_EPSILON;
}

draw_lines* draw_lines_from_points(mg_arena* arena, draw_point_allocator* allocator, vec2f* points, u32 num_points, vec4f col, f32 line_width) {
    if (num_points == 0) {
        fprintf(stderr, "Cannot create lines with zero points\n");
        return NULL;
    }

    draw_lines* lines = MGA_PUSH_ZERO_STRUCT(arena, draw_lines);
    lines->backend = MGA_PUSH_ZERO_STRUCT(arena, draw_lines_backend);

    lines->color = col;
    lines->width = line_width;

    lines->allocator = allocator;

    lines->num_points = num_points;
    u32 num_buckets = (num_points + DRAW_POINT_BUCKET_SIZE - 1) / DRAW_POINT_BUCKET_SIZE;
    for (u32 i = 0; i < num_buckets; i++) {
        draw_point_bucket* bucket = draw_point_alloc_alloc(allocator);

        u32 size = i == num_buckets - 1 ? 
            num_points - (DRAW_POINT_BUCKET_SIZE * (num_buckets - 1)) : DRAW_POINT_BUCKET_SIZE;

        bucket->size = size;
        memcpy(bucket->points, points + i * DRAW_POINT_BUCKET_SIZE, sizeof(vec2f) * size);

        SLL_PUSH_BACK(lines->points_first, lines->points_last, bucket);
    }

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
    glGenVertexArrays(1, &lines->backend->segment_array);
    glBindVertexArray(lines->backend->segment_array);

    lines->backend->vert_buffer = glh_create_buffer(GL_ARRAY_BUFFER, sizeof(line_vert) * lines->num_verts, NULL, GL_DYNAMIC_DRAW);
    lines->backend->index_buffer = glh_create_buffer(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * lines->num_indices, indices, GL_STATIC_DRAW);

    mga_scratch_release(scratch);

    glGenVertexArrays(1, &lines->backend->corner_array);
    glBindVertexArray(lines->backend->corner_array);

    lines->backend->corner_buffer = glh_create_buffer(GL_ARRAY_BUFFER, sizeof(line_corner) * lines->num_corners, NULL, GL_DYNAMIC_DRAW);

    // Computing the initial geometry
    draw_lines_update(lines, col, line_width);

    return lines;

}
draw_lines* draw_lines_create(mg_arena* arena, draw_point_allocator* allocator, vec4f col, f32 line_width);
void draw_lines_destroy(draw_lines* lines) {
    if (lines == NULL) {
        fprintf(stderr, "Cannot destroy lines: lines is NULL\n");
        return;
    }

    glDeleteVertexArrays(1, &lines->backend->segment_array);
    glDeleteVertexArrays(1, &lines->backend->corner_array);

    glDeleteBuffers(1, &lines->backend->vert_buffer);
    glDeleteBuffers(1, &lines->backend->index_buffer);
    glDeleteBuffers(1, &lines->backend->corner_buffer);
}

void draw_lines_draw(const draw_lines* lines, const draw_lines_shaders* shaders, const gfx_window* win, viewf view) {
    if (lines == NULL) {
        fprintf(stderr, "Cannot draw lines: lines is NULL\n");
        return;
    }

    mat3f view_mat = { 0 };
    mat3f_from_view(&view_mat, view);

    // Drawing line segments
    glUseProgram(shaders->line_program);
    glUniformMatrix3fv(shaders->line_view_mat_loc, 1, GL_FALSE, view_mat.m);
    glUniform4f(shaders->line_col_loc, lines->color.x, lines->color.y, lines->color.z, lines->color.w);
    glUniform1f(shaders->line_display_width_loc, lines->width * ((f32)win->width) / view.width);

    glBindVertexArray(lines->backend->segment_array);
    glBindBuffer(GL_ARRAY_BUFFER, lines->backend->vert_buffer);

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(line_vert), (void*)(offsetof(line_vert, pos)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lines->backend->index_buffer);
    glDrawElements(GL_TRIANGLES, lines->num_indices, GL_UNSIGNED_INT, NULL);

    glDisableVertexAttribArray(0);

    // Drawing corners
    glUseProgram(shaders->corner_program);
    glUniformMatrix3fv(shaders->corner_view_mat_loc, 1, GL_FALSE, view_mat.m);
    glUniform4f(shaders->corner_col_loc, lines->color.x, lines->color.y, lines->color.z, lines->color.w);
    glUniform2f(shaders->corner_screen_loc, win->width, win->height);
    glUniform1f(shaders->corner_line_width_loc, lines->width);

    glBindVertexArray(lines->backend->corner_array);
    glBindBuffer(GL_ARRAY_BUFFER, lines->backend->corner_buffer);

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

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 5, lines->num_corners);

    glVertexAttribDivisor(0, 0);
    glVertexAttribDivisor(1, 0);
    glVertexAttribDivisor(2, 0);
    glVertexAttribDivisor(3, 0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);

    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
void draw_lines_update(draw_lines* lines, vec4f col, f32 line_width) {
    if (lines == NULL || lines->num_points == 0) {
        fprintf(stderr, "Cannot update lines: invalid lines object\n");
        return;
    }

    lines->color = col;
    lines->width = line_width;

    mga_temp scratch = mga_scratch_get(NULL, 0);

    line_vert* verts = MGA_PUSH_ZERO_ARRAY(scratch.arena, line_vert, lines->num_verts);
    line_corner* corners = MGA_PUSH_ZERO_ARRAY(scratch.arena, line_corner, lines->num_corners);
    
    u32 num_verts = 0;
    u32 num_corners = 0;

    if (lines->num_points == 1) {
        vec2f point = lines->points_first->points[0];

        // Two corners form a circle here
        corners[num_corners++] = (line_corner){
            vec2f_add(point, (vec2f){ line_width * 1.1f, 0.0f }),
            point,
            vec2f_add(point, (vec2f){ line_width * 1.1f, 0.0f }),
        };
        corners[num_corners++] = (line_corner){
            vec2f_sub(point, (vec2f){ line_width * 1.1f, 0.0f }),
            point,
            vec2f_sub(point, (vec2f){ line_width * 1.1f, 0.0f }),
        };
    } else {
        // Points
        vec2f p0, p1, p2;
        // Lines and normals
        vec2f l1, n1, l2, n2;

        f32 half_w = lines->width * 0.5f;

        draw_point_bucket* cur_bucket = lines->points_first;
        u32 cur_num_buckets = 0;

        p0 = cur_bucket->points[0];
        p1 = cur_bucket->points[1];

        l1 = vec2f_nrm(vec2f_sub(p1, p0));
        n1 = vec2f_prp(l1);

        // Corner for rounded line cap
        corners[num_corners++] = (line_corner){ p1, p0, p1 };

        verts[num_verts++] = (line_vert){ vec2f_sub(p0, vec2f_scl(n1, half_w)) };
        verts[num_verts++] = (line_vert){ vec2f_add(p0, vec2f_scl(n1, half_w)) };

        // This is to get the correct p0 and p1 values in the first iteration of the for loop
        p1 = cur_bucket->points[0];
        p2 = cur_bucket->points[1];
        for (u32 i = 1; i < lines->num_points - 1; i++) {
            p0 = p1;
            p1 = p2;

            if (i + 1 >= (cur_num_buckets + 1) * DRAW_POINT_BUCKET_SIZE) {
                if (cur_bucket->next == NULL) {
                    fprintf(stderr, "Cannot update lines, not enough point buckets\n");

                    // So that scratch arena gets released
                    goto end;
                }

                cur_bucket = cur_bucket->next;
                cur_num_buckets++;
            }
            p2 = cur_bucket->points[i + 1 - cur_num_buckets * DRAW_POINT_BUCKET_SIZE];

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

            if (miter_scale < MITER_LIMIT && vec2f_sqr_len(line_sum) > TANGENT_EPSILON) {
                verts[num_verts++] = (line_vert){ vec2f_sub(p1, vec2f_scl(miter, half_w * miter_scale)) };
                verts[num_verts++] = (line_vert){ vec2f_add(p1, vec2f_scl(miter, half_w * miter_scale)) };
            } else {
                corners[num_corners++] = (line_corner){ p0, p1, p2 };

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
        l2 = vec2f_nrm(vec2f_sub(p2, p1));
        n2 = vec2f_prp(l2);

        corners[num_corners++] = (line_corner){ p1, p2, p1 };

        verts[num_verts++] = (line_vert){ vec2f_sub(p2, vec2f_scl(n2, half_w)) };
        verts[num_verts++] = (line_vert){ vec2f_add(p2, vec2f_scl(n2, half_w)) };
    }

    glBindBuffer(GL_ARRAY_BUFFER, lines->backend->vert_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(line_vert) * lines->num_verts, verts);
    glBindBuffer(GL_ARRAY_BUFFER, lines->backend->corner_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(line_corner) * lines->num_corners, corners);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

end: 

    mga_scratch_release(scratch);
}


#define GLSL_SOURCE(version, shader) "#version " #version " core \n" STRINGIFY(shader)

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
        float b = smoothstep(0.0, AA_SMOOTHING / u_display_width, 1.0 - abs(side));
        out_col = vec4(u_col.xyz, u_col.w * b);
    }
);

static const char* corner_vert = GLSL_SOURCE(
    330,
    layout (location = 0) in vec2 a_p0;
    layout (location = 1) in vec2 a_p1;
    layout (location = 2) in vec2 a_p2;

    uniform float u_line_width;
    uniform mat3 u_view_mat;
    uniform vec2 u_screen;

    out vec2 pos;
    flat out vec2 p0;
    flat out vec2 p1;
    flat out vec2 p2;
    flat out float display_width;

    // Returns length of z component
    float crs(vec2 a, vec2 b) {
        return a.x * b.y - a.y * b.x;
    }

    void main() {
        p0 = a_p0;
        p1 = a_p1;
        p2 = a_p2;

        vec2 l1 = normalize(p1 - p0);
        vec2 n1 = vec2(-l1.y, l1.x);
        vec2 l2 = normalize(p2 - p1);
        vec2 n2 = vec2(-l2.y, l2.x);

        display_width = length((u_view_mat * vec3(n1 * u_line_width, 0.0)).xy * (u_screen * 0.5));

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


        float half_w = u_line_width * 0.5;
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
                    p1 + n1 * s * half_w + l1 * u_line_width :
                    p1 - n1 * s * half_w + l1 * u_line_width;
            } else {
                // Points for line cap calculations
                // (i1, i2) and (i3, i4) define two lines
                // These verts sit at the intersection of those lines
                vec2 i1 = p1 + miter * (s * half_w);
                vec2 i2 = i1 - tangent;
                vec2 i3 = (p1 - miter * (s * half_w * miter_scale)) + (n1 * s * u_line_width);
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

    uniform float u_line_width;
    uniform vec4 u_col;

    in vec2 pos;
    flat in vec2 p0;
    flat in vec2 p1;
    flat in vec2 p2;
    flat in float display_width;

    float line_seg_sdf(vec2 p, vec2 a, vec2 b) {
        vec2 ba = b - a;
        vec2 pa = p - a;
        float t = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        return length(pa - t * ba);
    }

    void main() {
        float dist = min(line_seg_sdf(pos, p0, p1), line_seg_sdf(pos, p1, p2)) - u_line_width * 0.5;
        dist /= u_line_width;
        float alpha = smoothstep(0.0, -(AA_SMOOTHING * 0.5f) / display_width, dist);
        out_col = vec4(u_col.xyz, u_col.w * alpha);
    }
);

#endif // DRAW_BACKEND_OPENGL

