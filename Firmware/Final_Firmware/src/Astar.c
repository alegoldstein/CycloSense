/*
 * astar.c — A* pathfinding for ESP32
 *
 * Binary format produced by csv_parse.py:
 *
 *   Header : uint32 node_count, uint32 edge_count          (8 bytes)
 *   Nodes  : node_count × { int32 x_mm, int32 y_mm }       (8 B each)
 *   Edges  : edge_count × { uint32 u, uint32 v,
 *                            uint16 length_dm,
 *                            uint8 highway, uint8 oneway }  (12 B each)
 *
 * All integers little-endian (ESP32 native).
 * No floating-point math anywhere in this file.
 *
 * Usage
 * -----
 *   Graph g;
 *   astar_load_graph(&g, "/sdcard/graph.bin");
 *   uint32_t path[MAX_PATH];
 *   int len = astar_find(&g, start_id, end_id, path, MAX_PATH);
 *   astar_free_graph(&g);
 *
 * Dependencies: stdlib.h, stdio.h, string.h, stdint.h
 * No FreeRTOS calls; safe to run from any task.
 */

#include "Astar.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Heuristic — integer Euclidean distance in decimetres
 *
 * Nodes are stored in mm; costs are in dm (decimetres).
 * Convert mm → dm by dividing by 100, then use the octagonal approximation
 * to avoid an integer sqrt:
 *
 *   oct(dx, dy) = max + 3/8 * min   (~4 % overestimate max)
 *
 * This keeps the heuristic admissible enough for navigation at this scale
 * while using only integer multiply and shift — no FPU needed.
 * ---------------------------------------------------------------------- */
static inline uint32_t heuristic(const Node *a, const Node *b)
{
    /* mm → dm: divide by 100.  Use abs via conditional to stay branchless. */
    int32_t raw_dx = a->x_mm - b->x_mm;
    int32_t raw_dy = a->y_mm - b->y_mm;
    uint32_t dx = (uint32_t)(raw_dx < 0 ? -raw_dx : raw_dx) / 100u;
    uint32_t dy = (uint32_t)(raw_dy < 0 ? -raw_dy : raw_dy) / 100u;

    uint32_t mn = dx < dy ? dx : dy;
    uint32_t mx = dx > dy ? dx : dy;
    /* 3/8 ≈ 0.375; use (mn * 3) >> 3 for integer multiply + shift */
    return mx + ((mn * 3u) >> 3);
}

/* -------------------------------------------------------------------------
 * Min-heap — stores (f_score, node_id), all uint32
 * ---------------------------------------------------------------------- */
typedef struct { uint32_t f; uint32_t node; } HeapItem;
typedef struct { HeapItem *data; uint32_t size; uint32_t cap; } MinHeap;

static int heap_init(MinHeap *h, uint32_t cap)
{
    h->data = (HeapItem *)malloc(cap * sizeof(HeapItem));
    if (!h->data) return -1;
    h->size = 0; h->cap = cap;
    return 0;
}

static void heap_free(MinHeap *h)
{ free(h->data); h->data = NULL; h->size = 0; }

static void heap_swap(HeapItem *a, HeapItem *b)
{ HeapItem t = *a; *a = *b; *b = t; }

static int heap_push(MinHeap *h, uint32_t f, uint32_t node)
{
    if (h->size == h->cap) {
        uint32_t nc = h->cap * 2;
        HeapItem *tmp = (HeapItem *)realloc(h->data, nc * sizeof(HeapItem));
        if (!tmp) return -1;
        h->data = tmp; h->cap = nc;
    }
    uint32_t i = h->size++;
    h->data[i] = (HeapItem){ .f = f, .node = node };
    while (i > 0) {
        uint32_t p = (i - 1) / 2;
        if (h->data[p].f <= h->data[i].f) break;
        heap_swap(&h->data[p], &h->data[i]);
        i = p;
    }
    return 0;
}

static HeapItem heap_pop(MinHeap *h)
{
    HeapItem top = h->data[0];
    h->data[0] = h->data[--h->size];
    uint32_t i = 0;
    for (;;) {
        uint32_t l = 2*i+1, r = 2*i+2, s = i;
        if (l < h->size && h->data[l].f < h->data[s].f) s = l;
        if (r < h->size && h->data[r].f < h->data[s].f) s = r;
        if (s == i) break;
        heap_swap(&h->data[i], &h->data[s]);
        i = s;
    }
    return top;
}

/* -------------------------------------------------------------------------
 * Build CSR adjacency
 * ---------------------------------------------------------------------- */
static int build_adjacency(Graph *g)
{
    uint32_t N = g->node_count;

    uint32_t *deg = (uint32_t *)calloc(N + 1, sizeof(uint32_t));
    if (!deg) return -1;

    for (uint32_t i = 0; i < g->edge_count; i++) {
        Edge *e = &g->edges[i];
        deg[e->u]++;
        if (!e->oneway) deg[e->v]++;
    }

    g->row_ptr = (uint32_t *)malloc((N + 1) * sizeof(uint32_t));
    if (!g->row_ptr) { free(deg); return -1; }
    g->row_ptr[0] = 0;
    for (uint32_t i = 0; i < N; i++)
        g->row_ptr[i+1] = g->row_ptr[i] + deg[i];

    uint32_t total = g->row_ptr[N];
    g->adj_node = (uint32_t *)malloc(total * sizeof(uint32_t));
    g->adj_cost = (uint32_t *)malloc(total * sizeof(uint32_t));
    if (!g->adj_node || !g->adj_cost) { free(deg); return -1; }

    memset(deg, 0, (N + 1) * sizeof(uint32_t));
    for (uint32_t i = 0; i < g->edge_count; i++) {
        Edge *e = &g->edges[i];
        uint32_t pu = g->row_ptr[e->u] + deg[e->u]++;
        g->adj_node[pu] = e->v;
        g->adj_cost[pu] = e->length_dm;
        if (!e->oneway) {
            uint32_t pv = g->row_ptr[e->v] + deg[e->v]++;
            g->adj_node[pv] = e->u;
            g->adj_cost[pv] = e->length_dm;
        }
    }

    free(deg);
    return 0;
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

int astar_load_graph(Graph *g, const char *path)
{
    memset(g, 0, sizeof(Graph));

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "astar: cannot open %s\n", path); return -1; }

    if (fread(&g->node_count, 4, 1, f) != 1 ||
        fread(&g->edge_count, 4, 1, f) != 1) goto err;

    g->nodes = (Node *)malloc(g->node_count * sizeof(Node));
    if (!g->nodes) goto err;
    if (fread(g->nodes, sizeof(Node), g->node_count, f) != g->node_count) goto err;

    g->edges = (Edge *)malloc(g->edge_count * sizeof(Edge));
    if (!g->edges) goto err;
    if (fread(g->edges, sizeof(Edge), g->edge_count, f) != g->edge_count) goto err;

    fclose(f);

    if (build_adjacency(g) != 0) {
        fprintf(stderr, "astar: adjacency build OOM\n");
        astar_free_graph(g);
        return -1;
    }

    /* Free edge array — only needed for CSR build, not for A* or rendering */
    free(g->edges);
    g->edges = NULL;

    printf("astar: loaded %lu nodes, %lu edges\n",
           (unsigned long)g->node_count, (unsigned long)g->edge_count);
    return 0;

err:
    fprintf(stderr, "astar: read error\n");
    fclose(f);
    astar_free_graph(g);
    return -1;
}

void astar_free_graph(Graph *g)
{
    free(g->nodes);    g->nodes    = NULL;
    free(g->edges);    g->edges    = NULL;
    free(g->row_ptr);  g->row_ptr  = NULL;
    free(g->adj_node); g->adj_node = NULL;
    free(g->adj_cost); g->adj_cost = NULL;
}

int astar_find(const Graph *g,
               uint32_t     start,
               uint32_t     end,
               uint32_t    *out_path,
               uint32_t     max_path)
{
    uint32_t N = g->node_count;
    if (start >= N || end >= N) return ASTAR_ERR_INVALID_NODE;

    uint32_t *g_score   = (uint32_t *)malloc(N * sizeof(uint32_t));
    uint32_t *came_from = (uint32_t *)malloc(N * sizeof(uint32_t));
    uint8_t  *closed    = (uint8_t  *)calloc(N, 1);

    if (!g_score || !came_from || !closed) {
        free(g_score); free(came_from); free(closed);
        return ASTAR_ERR_OOM;
    }

    for (uint32_t i = 0; i < N; i++) {
        g_score[i]   = UINT32_MAX;
        came_from[i] = UINT32_MAX;
    }
    g_score[start] = 0u;

    MinHeap heap;
    if (heap_init(&heap, 256) != 0) {
        free(g_score); free(came_from); free(closed);
        return ASTAR_ERR_OOM;
    }

    heap_push(&heap, heuristic(&g->nodes[start], &g->nodes[end]), start);

    int result = ASTAR_ERR_NO_PATH;

    while (heap.size > 0) {
        HeapItem cur = heap_pop(&heap);
        uint32_t q   = cur.node;

        if (q == end)    { result = 0; break; }
        if (closed[q])     continue;
        closed[q] = 1;

        for (uint32_t ei = g->row_ptr[q]; ei < g->row_ptr[q+1]; ei++) {
            uint32_t nb   = g->adj_node[ei];
            uint32_t cost = g->adj_cost[ei];

            if (closed[nb]) continue;

            /* guard against overflow before adding */
            if (g_score[q] > UINT32_MAX - cost) continue;
            uint32_t tg = g_score[q] + cost;

            if (tg < g_score[nb]) {
                g_score[nb]   = tg;
                came_from[nb] = q;
                uint32_t h    = heuristic(&g->nodes[nb], &g->nodes[end]);
                /* guard f overflow */
                uint32_t f    = (tg > UINT32_MAX - h) ? UINT32_MAX : tg + h;
                heap_push(&heap, f, nb);
            }
        }
    }

    if (result == 0) {
        uint32_t len = 0;
        uint32_t cur = end;
        while (cur != UINT32_MAX) { len++; cur = came_from[cur]; }

        if (len > max_path) {
            result = ASTAR_ERR_PATH_TOO_LONG;
        } else {
            cur = end;
            for (uint32_t i = len; i-- > 0; ) {
                out_path[i] = cur;
                cur = came_from[cur];
            }
            result = (int)len;
        }
    }

    heap_free(&heap);
    free(g_score);
    free(came_from);
    free(closed);
    return result;
}