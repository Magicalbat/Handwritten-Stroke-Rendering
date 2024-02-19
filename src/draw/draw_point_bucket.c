#include "draw_point_bucket.h"

#include <string.h>

draw_point_allocator* draw_point_alloc_create(mg_arena* backing_arena) {
    mg_arena* arena = backing_arena;

    b32 owned_arena = false;

    if (arena == NULL) {
        owned_arena = true;

        mga_desc desc = {
            // This should allow for a little over 2 million points
            .desired_max_size = MGA_MiB(16),
            .desired_block_size = MGA_MiB(1),
        };
        arena = mga_create(&desc);
    }

    draw_point_allocator* point_alloc = MGA_PUSH_ZERO_STRUCT(arena, draw_point_allocator);

    point_alloc->owned_arena = owned_arena;
    point_alloc->backing_arena = arena;

    return point_alloc;
}
void draw_point_alloc_destroy(draw_point_allocator* point_alloc) {
    if (point_alloc->owned_arena) {
        mga_destroy(point_alloc->backing_arena);
    }
}

draw_point_bucket* draw_point_alloc_alloc(draw_point_allocator* point_alloc) {
    // Free node in list exists
    if (point_alloc->free_first != NULL) {
        draw_point_bucket* out = point_alloc->free_first;

        SLL_POP_FRONT(point_alloc->free_first, point_alloc->free_last);

        // This should work because the array is a part of the struct, not a pointer
        memset(out, 0, sizeof(draw_point_bucket) * DRAW_POINT_BUCKET_SIZE);

        return out;
    }

    draw_point_bucket* out = MGA_PUSH_ZERO_STRUCT(point_alloc->backing_arena, draw_point_bucket);

    return out;
}
void draw_point_alloc_free(draw_point_allocator* point_alloc, draw_point_bucket* bucket) {
    SLL_PUSH_FRONT(point_alloc->free_first, point_alloc->free_last, bucket);
}
