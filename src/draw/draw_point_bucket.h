#ifndef DRAW_POINT_BUCKET_H
#define DRAW_POINT_BUCKET_H

#include "base/base.h"

// Right now, this value is arbitrary
#define DRAW_POINT_BUCKET_SIZE 64

typedef struct draw_point_bucket {
    u32 size;
    vec2f points[DRAW_POINT_BUCKET_SIZE];
    struct draw_point_bucket* next;
} draw_point_bucket;

typedef struct {
    b32 owned_arena;
    mg_arena* backing_arena;

    // Free list
    draw_point_bucket* free_first;
    draw_point_bucket* free_last;
} draw_point_allocator;

// backing_arena can be NULL
draw_point_allocator* draw_point_alloc_create(mg_arena* backing_arena);
void draw_point_alloc_destroy(draw_point_allocator* point_alloc);
draw_point_bucket* draw_point_alloc_alloc(draw_point_allocator* point_alloc);
void draw_point_alloc_free(draw_point_allocator* point_alloc, draw_point_bucket* bucket);

#endif // DRAW_POINT_BUCKET_H
