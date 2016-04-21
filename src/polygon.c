typedef struct Vertex Vertex;
struct Vertex {
    f32 x, y;

    Vertex *prev;
    Vertex *next;
};

typedef struct {
    HM_MemoryArena arena;
    Vertex *first_free_vertex;
} PolygonPool;

typedef struct {
    Vertex *first;
    Vertex *last;
} EditingPolygon;

static PolygonPool *
make_polygon_pool(HM_MemoryArena *arena, usize size) {
    PolygonPool *result = HM_PUSH_STRUCT(arena, PolygonPool);

    result->arena = HM_SUB_MEMORY_ARENA(arena, size);
    result->first_free_vertex = 0;

    return result;
}

static EditingPolygon *
make_polygon(HM_MemoryArena *arena) {
    EditingPolygon *result = HM_PUSH_STRUCT(arena, EditingPolygon);

    result->first = 0;
    result->last = 0;

    return result;
}

static void
push_vertex(PolygonPool *pool, EditingPolygon *polygon, f32 x, f32 y) {
    Vertex *vertex = HM_PUSH_STRUCT(&pool->arena, Vertex);

    vertex->x = x;
    vertex->y = y;
    vertex->next = 0;

    vertex->prev = polygon->last;

    if (polygon->last) {
        polygon->last->next = vertex;
    }

    polygon->last = vertex;

    if (!polygon->first) {
        polygon->first = vertex;
    }
}

static void
update_polygon(EditingPolygon *polygon, PolygonPool *pool) {
    (void)polygon;
    (void)pool;
}

static void
render_polygon(EditingPolygon *polygon, HM_RenderCommandBuffer *buffer) {
    for (Vertex *a = polygon->first; a; a = a->next) {
        Vertex *b = a->next;
        if (b == 0) {
            b = polygon->first;
        }

        hm_render_line2(buffer, hm_transform2_identity(),
                        hm_line2(hm_v2(a->x, a->y), hm_v2(b->x, b->y)),
                        2.0f,
                        hm_v4(1.0f, 1.0f, 1.0f, 1.0f));
    }
}
