#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "base/base.h"
#include "os/os.h"
#include "gfx/gfx.h"
#include "gfx/opengl/opengl.h"
#include "gfx/opengl/opengl_helpers.h"

#include "draw/draw.h"

#define WIDTH 640
#define HEIGHT 360

#define INTERP_MARGIN 0.02f

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
    gfx_win_make_current(win);

    u32 basic_program = glh_create_shader(basic_vert, basic_frag);

    glUseProgram(basic_program);
    u32 basic_view_mat_loc = glGetUniformLocation(basic_program, "u_view_mat");
    u32 basic_col_loc = glGetUniformLocation(basic_program, "u_col");

    draw_lines_shaders* shaders = draw_lines_shaders_create(perm_arena);
    draw_point_allocator* point_allocator = draw_point_alloc_create(perm_arena);

    /*u32 w = 500;
    u32 h = 400;
    vec2f* points = MGA_PUSH_ZERO_ARRAY(perm_arena, vec2f, w * h);

    srand(time(NULL));

    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            f32 v_y = -500.0f + ((f32)y / h) * 1000.0f;
            //v_y += ((f32)rand() / (f32)RAND_MAX) * 3.0f - 1.5f;
            v_y += (x % 2) * 4.0f - 2.0f;
            f32 v_x = -500.0f + ((f32)x / w) * 1000.0f;
            if ((y % 2) == 1) {
                v_x = -v_x;
            }

            points[x + y * w] = (vec2f){ v_x, v_y };
        }
    }*/

    u32 num_lines = 0;
    draw_lines* lines[1024] = { 0 };

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

    u32 vertex_buffer = glh_create_buffer(GL_ARRAY_BUFFER, sizeof(rect_verts), rect_verts, GL_DYNAMIC_DRAW);
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

    vec2f prev_mouse_pos = win->mouse_pos;
    vec2f prev_point = prev_mouse_pos;
    vec2f prev_prev_point = prev_mouse_pos;

    b32 erase = false;

    os_time_init();

    u64 prev_frame = os_now_usec();
    while (!win->should_close) {
        u64 cur_frame = os_now_usec();
        f32 delta = (f32)(cur_frame - prev_frame) / 1e6;
        prev_frame = cur_frame;

#ifndef PLATFORM_WASM
        gfx_win_process_events(win);
#endif

        // Update

        f32 move_speed = view.width;

        view.aspect_ratio = (f32)win->width / win->height;
        view.width *= 1.0f + (-10.0f * win->mouse_scroll * delta);

        if (GFX_IS_KEY_DOWN(win, GFX_KEY_W)) {
            view.center.y -= move_speed * delta;
        }
        if (GFX_IS_KEY_DOWN(win, GFX_KEY_S)) {
            view.center.y += move_speed * delta;
        }
        if (GFX_IS_KEY_DOWN(win, GFX_KEY_A)) {
            view.center.x -= move_speed * delta;
        }
        if (GFX_IS_KEY_DOWN(win, GFX_KEY_D)) {
            view.center.x += move_speed * delta;
        }

        mat3f_from_view(&view_mat, view);

        mat3f_inverse(&inv_view_mat, &view_mat);

        vec2f mouse_pos = (vec2f){
            2.0f * win->mouse_pos.x / win->width - 1.0f,
            -(2.0f * win->mouse_pos.y / win->height - 1.0f),
        };
        mouse_pos = mat3f_mul_vec2f(&inv_view_mat, mouse_pos);

        if (GFX_IS_MOUSE_JUST_DOWN(win, GFX_MB_LEFT)) {
            if (GFX_IS_KEY_DOWN(win, GFX_KEY_E)) {
                erase = true;
            } else {
                erase = false;

                num_lines++;

                if (lines[num_lines - 1] == NULL) {
                    lines[num_lines - 1] = draw_lines_create(perm_arena, point_allocator, (vec4f){ 1.0f, 1.0f, 1.0f, 1.0f }, 5.0f);
                } else {
                    draw_lines_reinit(lines[num_lines - 1], (vec4f){ 1, 1, 1, 1}, 5.0f);
                }

                draw_lines_add_point(lines[num_lines - 1], mouse_pos);

                prev_point = mouse_pos;
                prev_prev_point = prev_point;
            }
        } else if (!erase &&
            (GFX_IS_MOUSE_DOWN(win, GFX_MB_LEFT) || GFX_IS_MOUSE_JUST_UP(win, GFX_MB_LEFT)) &&
            !vec2f_eq(mouse_pos, prev_mouse_pos)) {

            if (!vec2f_eq(mouse_pos, prev_point)) {
                f32 prev_dist = vec2f_dist(prev_point, mouse_pos);
                u32 num_points = (u32)roundf(prev_dist / (view.width * INTERP_MARGIN)) + 1;

                const f32 smoothing = 0.25f;
                vec2f control_offset = vec2f_scl(vec2f_sub(prev_point, prev_prev_point), smoothing);
                vec2f norm = vec2f_prp(vec2f_nrm(vec2f_sub(mouse_pos, prev_point)));

                // Defining bezier control points based on hermite spline
                vec2f p0 = prev_point;
                vec2f p1 = vec2f_add(prev_point, control_offset);
                vec2f p2 = vec2f_sub(mouse_pos, vec2f_ref(control_offset, norm));
                vec2f p3 = mouse_pos;

                cubic_bezier bez = cbezier_create(p0, p1, p2, p3);

                f32 t_interval = 1.0f / (f32)(num_points + 1);
                f32 t = t_interval;

                for (u32 i = 0; i < num_points; i++) {
                    vec2f p = cbezier_calc(&bez, t);

                    draw_lines_add_point(lines[num_lines - 1], p);

                    t += t_interval;
                }

                prev_prev_point = prev_point;
                prev_point = mouse_pos;
            }
        }
        prev_mouse_pos = mouse_pos;

        for (i64 i = 0; i < num_lines; i++) {
            if (erase && GFX_IS_MOUSE_DOWN(win, GFX_MB_LEFT) && draw_lines_collide_circle(lines[i], (circlef){ mouse_pos, 25 })) {
                draw_lines_clear(lines[i]);
                draw_lines* cleared_line = lines[i];

                num_lines--;

                for (i64 j = i; j < num_lines; j++) {
                    lines[j] = lines[j + 1];
                }
                lines[num_lines] = cleared_line;

                i--;
            }
        }

        gfx_win_clear(win);

        // Draw

        // Rect draw
        {
            glUseProgram(basic_program);
            glUniformMatrix3fv(basic_view_mat_loc, 1, GL_FALSE, view_mat.m);

            glUniform4f(basic_col_loc, 0.0f, 0.0f, 0.0f, 1.0f);

            glBindVertexArray(vertex_array);
            glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

            vec2f rect_verts[] = {
                { -250.0f,  250.0f },
                { -250.0f, -250.0f },
                {  250.0f, -250.0f },
                {  250.0f,  250.0f }
            };

            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(rect_verts), rect_verts);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2f), NULL);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);

            glDisableVertexAttribArray(0);
        }

        for (u32 i = 0; i < num_lines; i++) {
            draw_lines_draw(lines[i], shaders, win, view);
        }

        if (erase && GFX_IS_MOUSE_DOWN(win, GFX_MB_LEFT)) {
            glUseProgram(basic_program);
            glUniform4f(basic_col_loc, 0.0f, 1.0f, 0.0f, 0.5f);
            glBindVertexArray(vertex_array);
            glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

            rectf bb = { mouse_pos.x - 25.0f, mouse_pos.y - 25.0f, 50.0f, 50.0f };
            vec2f verts[] = {
                { bb.x, bb.y + bb.h },
                { bb.x, bb.y },
                { bb.x + bb.w, bb.y },
                { bb.x + bb.w, bb.y + bb.h },
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2f), NULL);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);

            glDisableVertexAttribArray(0);
        }

        gfx_win_swap_buffers(win);

#ifdef PLATFORM_WASM
        gfx_win_process_events(win);
#endif

        os_sleep_ms(2);
    }

    for (u32 i = 0; i < num_lines; i++) {
        draw_lines_destroy(lines[i]);
    }

    draw_lines_shaders_destroy(shaders);
    draw_point_alloc_destroy(point_allocator);

    glDeleteBuffers(1, &vertex_buffer);
    glDeleteBuffers(1, &index_buffer);
    glDeleteVertexArrays(1, &vertex_array);

    glDeleteProgram(basic_program);

    gfx_win_destroy(win);

    mga_destroy(perm_arena);

    return 0;
}
