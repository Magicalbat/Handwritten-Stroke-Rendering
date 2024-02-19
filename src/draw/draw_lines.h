#ifndef DRAW_LINES_H
#define DRAW_LINES_H

#include "base/base.h"
#include "draw_point_bucket.h"
#include "gfx/gfx.h"

// Contents defined in draw backends
typedef struct draw_lines_shaders draw_lines_shaders;

draw_lines_shaders* draw_lines_shaders_create(mg_arena* arena);
void draw_lines_shaders_destroy(draw_lines_shaders* shaders);

typedef struct {
    vec4f color;
    f32 width;

    draw_point_allocator* allocator;

    // Number of total points
    u32 num_points;
    draw_point_bucket* points_first;
    draw_point_bucket* points_last;

    u32 num_verts;
    u32 num_indices;
    u32 num_corners;

    struct _draw_lines_backend* backend;
} draw_lines;

// Creates lines with the specified points
draw_lines* draw_lines_from_points(mg_arena* arena, draw_point_allocator* allocator, vec2f* points, u32 num_points, vec4f col, f32 line_width);
// Creates an empty lines object
draw_lines* draw_lines_create(mg_arena* arena, draw_point_allocator* allocator, vec4f col, f32 line_width);
void draw_lines_destroy(draw_lines* lines);

void draw_lines_draw(const draw_lines* lines, const draw_lines_shaders* shaders, const gfx_window* win, viewf view);
// Updates the geometry of the lines with the new color and width
void draw_lines_update(draw_lines* lines, vec4f col, f32 line_width);

#endif // DRAW_LINES_H

