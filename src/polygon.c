#define VERTEX_DRAG_REGION_SIZE 8
#define VERTEX_THRESHOLD 16

typedef struct Vertex Vertex;
struct Vertex {
    HM_V2 pos;
    bool is_ear;

    Vertex *prev;
    Vertex *next;
};

typedef struct EditingPolygon EditingPolygon;
struct EditingPolygon {
    u32 vertex_count;
    Vertex *first;

    Vertex *selected;
    HM_V2 drag_pos;
    bool is_dragging;

    EditingPolygon *next;
};

typedef struct {
    HM_MemoryArena arena;

    EditingPolygon *first_free_editing_polygon;
    Vertex *first_free_vertex;
} PolygonPool;

typedef struct {
    u32 triangle_count;
    HM_Triangle2 *triangles;
} TriangulatedPolygon;

static TriangulatedPolygon *triangulated_polygon;

static PolygonPool *
make_polygon_pool(HM_MemoryArena *arena, usize size) {
    PolygonPool *result = hm_push_struct(arena, PolygonPool);

    hm_clear_memory(result);

    result->arena = hm_sub_memory_arena(arena, size);

    return result;
}

static EditingPolygon *
make_polygon(HM_MemoryArena *arena) {
    EditingPolygon *result = hm_push_struct(arena, EditingPolygon);

    hm_clear_memory(result);

    return result;
}

static void
close_polygon(EditingPolygon *polygon) {
    HM_ASSERT(polygon->vertex_count >= 3);
}

static Vertex *
get_free_or_alloc_vertex(PolygonPool *pool) {
    Vertex *result = pool->first_free_vertex;
    if (result) {
        pool->first_free_vertex = result->next;
    } else {
        result = hm_push_struct(&pool->arena, Vertex);
    }

    return result;
}

static Vertex *
insert_vertex_after(PolygonPool *pool, EditingPolygon *polygon, Vertex *vertex, HM_V2 pos) {
    Vertex *result = get_free_or_alloc_vertex(pool);
    result->pos = pos;

    result->prev = vertex;
    result->next = vertex->next;

    result->prev->next = result;
    result->next->prev = result;

    ++polygon->vertex_count;

    return result;
}

static void
push_vertex(PolygonPool *pool, EditingPolygon *polygon, HM_V2 pos) {
    if (polygon->first) {
        insert_vertex_after(pool, polygon, polygon->first->prev, pos);
    } else {
        polygon->first = get_free_or_alloc_vertex(pool);
        polygon->first->pos = pos;
        polygon->first->prev = polygon->first;
        polygon->first->next = polygon->first;

        ++polygon->vertex_count;
    }
}

static void
remove_vertex(PolygonPool *pool, EditingPolygon *polygon, Vertex *freed) {
    freed->next->prev = freed->prev;
    freed->prev->next = freed->next;

    if (freed == polygon->first) {
        if (polygon->vertex_count > 1) {
            polygon->first = freed->next;
        } else {
            polygon->first = 0;
        }
    }

    freed->next = pool->first_free_vertex;
    pool->first_free_vertex = freed;
    --polygon->vertex_count;
}

static void
remove_first_vertex(PolygonPool *pool, EditingPolygon *polygon) {
    remove_vertex(pool, polygon, polygon->first);
}

static EditingPolygon *
copy_polygon(PolygonPool *pool, EditingPolygon *polygon) {
    EditingPolygon *result = pool->first_free_editing_polygon;
    if (result) {
        pool->first_free_editing_polygon = result->next;
        hm_clear_memory(result);
    } else {
        result = make_polygon(&pool->arena);
    }

    Vertex *a = polygon->first;
    for (u32 i = 0; i < polygon->vertex_count; ++i) {
        push_vertex(pool, result, a->pos);
        a = a->next;
    }
    close_polygon(result);

    return result;
}

static void
free_polygon(PolygonPool *pool, EditingPolygon *polygon) {
    while (polygon->vertex_count) {
        remove_first_vertex(pool, polygon);
    }
    polygon->next = pool->first_free_editing_polygon;
    pool->first_free_editing_polygon = polygon;
}

static void
free_triangulated_polygon(PolygonPool *pool, TriangulatedPolygon *triangulated_polygon) {
    // TODO
}

static bool
is_diagonalie(EditingPolygon *polygon, Vertex *s1, Vertex *s2) {
    HM_Line2 test = hm_line2(s1->pos, s2->pos);
    Vertex *a = polygon->first;
    for (u32 i = 0; i < polygon->vertex_count; ++i) {
        Vertex *b = a->next;

        if (a != s1 && a != s2 && b != s1 && b != s2 &&
            hm_is_line2_intersect(hm_line2(a->pos, b->pos), test))
        {
            return false;
        }

        a = a->next;
    }

    return true;
}

static bool
is_in_cone(EditingPolygon *polygon, Vertex *a, Vertex *b) {
    (void)polygon;

    Vertex *a0 = a->prev;
    Vertex *a1 = a->next;

    if (hm_is_line2_left_on(hm_line2(a->pos, a1->pos), a0->pos)) {
        // convex vertex
        return (hm_is_line2_left(hm_line2(a->pos, b->pos), a0->pos) &&
                hm_is_line2_left(hm_line2(b->pos, a->pos), a1->pos));
    } else {
        // reflex vertex
        return !(hm_is_line2_left_on(hm_line2(a->pos, b->pos), a1->pos) &&
                 hm_is_line2_left_on(hm_line2(b->pos, a->pos), a0->pos));
    }
}

static bool
is_diagonal(EditingPolygon *polygon, Vertex *a, Vertex *b) {
    bool result = (is_in_cone(polygon, a, b) &&
                   is_in_cone(polygon, b, a) &&
                   is_diagonalie(polygon, a, b));

    return result;
}

static void
init_polygon_ear(EditingPolygon *polygon) {
    Vertex *v1 = polygon->first;
    for (u32 i = 0; i < polygon->vertex_count; ++i) {
        v1->is_ear = is_diagonal(polygon, v1->prev, v1->next);

        v1 = v1->next;
    }
}

static TriangulatedPolygon *
triangulate_polygon(PolygonPool *pool, EditingPolygon *polygon) {
    TriangulatedPolygon *result = hm_push_struct(&pool->arena, TriangulatedPolygon);

    result->triangle_count = 0;
    result->triangles = hm_push_array(&pool->arena, HM_Triangle2, polygon->vertex_count - 2);

    init_polygon_ear(polygon);
    while (polygon->vertex_count > 3) {
        u32 n = polygon->vertex_count;

        Vertex *v2 = polygon->first;
        do {
            if (v2->is_ear) {
                Vertex *v1 = v2->prev;
                Vertex *v3 = v2->next;

                Vertex *v0 = v1->prev;
                Vertex *v4 = v3->next;

                HM_Triangle2 *triangle = result->triangles + result->triangle_count++;
                triangle->a = v1->pos;
                triangle->b = v2->pos;
                triangle->c = v3->pos;

                v1->is_ear = is_diagonal(polygon, v0, v3);
                v3->is_ear = is_diagonal(polygon, v1, v4);

                remove_vertex(pool, polygon, v2);

                break;
            }

            v2 = v2->next;
        } while (v2 != polygon->first);

        HM_ASSERT(n > polygon->vertex_count);
    }

    HM_Triangle2 *triangle = result->triangles + result->triangle_count++;
    triangle->a = polygon->first->prev->pos;
    triangle->b = polygon->first->pos;
    triangle->c = polygon->first->next->pos;

    return result;
}

static void
update_polygon(EditingPolygon *polygon, PolygonPool *pool, Hammer *hammer) {
    HM_Input *input = hammer->input;
    HM_Texture2 *framebuffer = hammer->framebuffer;

    HM_V2 mouse_pos = hm_v2(input->mouse.x, framebuffer->height - input->mouse.y);

    if (!input->mouse.left.is_down) {
        polygon->is_dragging = false;
    }

    if (polygon->is_dragging) {
        polygon->drag_pos = mouse_pos;
        polygon->selected->pos = polygon->drag_pos;
    } else {
        // Check if the mouse have been moved
        if (input->mouse.is_moved) {
            // Find closest vertex
            f32 min_distance = HM_F32_MAX;
            Vertex *min_vertex = 0;

            Vertex *a = polygon->first;
            for (u32 i = 0; i < polygon->vertex_count; ++i) {
                Vertex *b = a->next;

                HM_V2 ac = hm_v2_sub(mouse_pos, a->pos);
                f32 distance = hm_get_v2_len(ac);

                HM_Line2 line = hm_line2(a->pos, b->pos);
                f32 proj = hm_get_line2_proj_p(line, mouse_pos);
                if (proj >= 0.0f && proj < 1.0f) {
                    distance = HM_MIN(hm_get_line2_distance(line, mouse_pos), distance);
                }

                if (distance < min_distance) {
                    min_distance = distance;
                    min_vertex = a;
                }

                a = a->next;
            }

            // Move drag point along edge
            if (min_vertex) {
                polygon->selected = min_vertex;
                polygon->drag_pos = polygon->selected->pos;

                Vertex *a = polygon->selected->prev;
                Vertex *b = polygon->selected;

                f32 min_distance = HM_F32_MAX;

                for (int i = 0; i < 2; ++i) {
                    f32 distance = hm_get_line2_distance(hm_line2(a->pos, b->pos),
                                                         mouse_pos);
                    if (distance < min_distance) {
                        min_distance = distance;

                        f32 proj = hm_get_line2_proj_p(hm_line2(a->pos, b->pos), mouse_pos);
                        if (proj >= 0.0f && proj < 1.0f) {
                            polygon->drag_pos = hm_v2_add(a->pos, hm_v2_mul(proj, hm_v2_sub(b->pos, a->pos)));
                        }
                    }

                    a = a->next;
                    b = b->next;
                }
            }

            // Snap drag point to vertex
            a = polygon->first;
            for (u32 i = 0; i < polygon->vertex_count; ++i) {
                f32 distance = hm_get_v2_len(hm_v2_sub(polygon->drag_pos, a->pos));
                if (distance < VERTEX_THRESHOLD) {
                    polygon->selected = a;
                    polygon->drag_pos = a->pos;
                    break;
                }
                a = a->next;
            }
        }

        if (polygon->selected && input->mouse.left.is_pressed) {
            HM_BBox2 bbox = hm_bbox2_cen_size(
                polygon->drag_pos,
                hm_v2(VERTEX_DRAG_REGION_SIZE, VERTEX_DRAG_REGION_SIZE)
            );

            if (hm_is_bbox2_contains_point(bbox, mouse_pos)) {
                // Add new vertex at drag point
                if (!hm_is_v2_equal(polygon->selected->pos, polygon->drag_pos)) {
                    polygon->selected = insert_vertex_after(pool, polygon,
                                                            polygon->selected,
                                                            polygon->drag_pos);
                }

                polygon->is_dragging = true;
            }
        }
    }

    EditingPolygon *copied = copy_polygon(pool, polygon);
    if (triangulated_polygon != 0) {
        free_triangulated_polygon(pool, triangulated_polygon);
    }
    triangulated_polygon = triangulate_polygon(pool, copied);
    free_polygon(pool, copied);

    printf("%lu\n", pool->arena.used);
}

static void
render_polygon(EditingPolygon *polygon, HM_RenderContext *context) {
    hm_render_push(context);

    hm_set_render_trans2(context, hm_trans2_identity());

    // Draw triangulated polygon
    {
        hm_set_render_color(context, hm_v4(0.7f, 0.7f, 0.7f, 1.0f));
        for (u32 triangle_index = 0; triangle_index < triangulated_polygon->triangle_count; ++triangle_index) {
            HM_Triangle2 *triangle = triangulated_polygon->triangles + triangle_index;

            hm_render_line2(context, hm_line2(triangle->a, triangle->b), 1.5f);
            hm_render_line2(context, hm_line2(triangle->b, triangle->c), 1.5f);
            hm_render_line2(context, hm_line2(triangle->c, triangle->a), 1.5f);
        }
    }

    HM_V4 selected_color = hm_v4(0.0f, 1.0f, 0.0f, 1.0f);

    Vertex *a = polygon->first;
    for (u32 i = 0; i < polygon->vertex_count; ++i) {
        Vertex *b = a->next;

        HM_V4 color = hm_v4(1.0f, 1.0f, 1.0f, 1.0f);

        if (hm_is_v2_equal(a->pos, polygon->drag_pos) ||
            hm_is_v2_equal(b->pos, polygon->drag_pos))
        {
            color = selected_color;
        } else {
            HM_V2 ab = hm_v2_sub(b->pos, a->pos);
            HM_V2 ac = hm_v2_sub(polygon->drag_pos, a->pos);

            HM_Line2 line = hm_line2(a->pos, b->pos);
            f32 proj = hm_get_line2_proj_p(line, polygon->drag_pos);
            if (proj >= 0.0f && proj < 1.0f &&
                hm_get_v2_rad_between(ab, ac) < 0.001f)
            {
                color = selected_color;
            }
        }

        hm_set_render_color(context, color);
        hm_render_line2(context, hm_line2(a->pos, b->pos), 2.0f);

        a = a->next;
    }

    if (polygon->selected) {
        hm_set_render_color(context, selected_color);
        hm_render_bbox2(context,
                        hm_bbox2_cen_size(polygon->drag_pos,
                                          hm_v2(VERTEX_DRAG_REGION_SIZE,
                                                VERTEX_DRAG_REGION_SIZE)));
    }

#if 0
    // Draw diagonal
    {
        hm_set_render_color(context, hm_v4(0.5f, 0.5f, 0.5f, 1.0f));

        Vertex *a = polygon->first;
        for (u32 i = 0; i < polygon->vertex_count; ++i) {
            Vertex *b = a->next->next;
            for (u32 j = 3; j < polygon->vertex_count; ++j) {
                if (is_diagonal(polygon, a, b)) {
                    hm_render_line2(context, hm_line2(a->pos, b->pos), 2.0f);
                }
                b = b->next;
            }
            a = a->next;
        }
    }
#endif

    hm_render_pop(context);
}
