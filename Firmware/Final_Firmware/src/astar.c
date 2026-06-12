/*
 * astar.c — A* pathfinding for ESP32
 *
 * Binary format (NEW — produced by find_nodes.py):
 *
 *   Header  : uint32 node_count, uint32 adj_total           (8 bytes)
 *   Nodes   : node_count × { int16 x_m, int16 y_m }         (4 B each)
 *   row_ptr : (node_count+1) × uint32                        (4 B each)
 *   AdjData : adj_total × { uint16 node, uint16 cost }       (4 B each)
 *
 * RAM usage (~92 KB for 12k-node Evanston graph):
 *   nodes[]    46 KB
 *   row_ptr[]  46 KB
 *
 * AdjData stays in the SPIFFS file. A* opens the file and seeks
 * to each node's neighbor list on demand. Slower than RAM but
 * uses ~200 KB less heap — fits comfortably on ESP32.
 */

#include "astar.h"
#include "esp_system.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Heuristic — octagonal approximation
 * Nodes in metres (int16); costs in dm → multiply by 10
 * ---------------------------------------------------------------------- */
static inline uint32_t heuristic(const Node *a, const Node *b)
{
    int32_t  raw_dx = (int32_t)a->x_m - (int32_t)b->x_m;
    int32_t  raw_dy = (int32_t)a->y_m - (int32_t)b->y_m;
    uint32_t dx = (uint32_t)(raw_dx < 0 ? -raw_dx : raw_dx);
    uint32_t dy = (uint32_t)(raw_dy < 0 ? -raw_dy : raw_dy);
    uint32_t mn = dx < dy ? dx : dy;
    uint32_t mx = dx > dy ? dx : dy;
    return (mx + ((mn * 3u) >> 3)) * 10u;
}

/* -------------------------------------------------------------------------
 * Min-heap
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
 * Public API
 * ---------------------------------------------------------------------- */

int astar_load_graph(Graph *g, const char *path)
{
    memset(g, 0, sizeof(Graph));
    strncpy(g->path, path, sizeof(g->path) - 1);

    FILE *f = fopen(path, "rb");
    if (!f) { printf("astar: cannot open %s\n", path); return -1; }

    /* Header: node_count, adj_total */
    if (fread(&g->node_count, 4, 1, f) != 1 ||
        fread(&g->adj_total,  4, 1, f) != 1) goto err;

    printf("astar: %lu nodes  adj_total=%lu  heap=%lu\n",
           (unsigned long)g->node_count,
           (unsigned long)g->adj_total,
           (unsigned long)esp_get_free_heap_size());

    if (g->node_count > 65535) {
        printf("astar: node_count exceeds uint16 limit\n"); goto err;
    }

    /* Read nodes */
    g->nodes = (Node *)malloc(g->node_count * sizeof(Node));
    if (!g->nodes) { printf("astar: OOM nodes\n"); goto err; }
    {
        uint32_t rem = g->node_count;
        Node *ptr = g->nodes;
        while (rem > 0) {
            uint32_t chunk = rem > 64 ? 64 : rem;
            if (fread(ptr, sizeof(Node), chunk, f) != chunk) goto err;
            ptr += chunk; rem -= chunk;
        }
    }
    printf("astar: nodes loaded  heap=%lu\n",
           (unsigned long)esp_get_free_heap_size());

    /* Read row_ptr */
    g->row_ptr = (uint32_t *)malloc((g->node_count + 1) * sizeof(uint32_t));
    if (!g->row_ptr) { printf("astar: OOM row_ptr\n"); goto err; }
    {
        uint32_t rem = g->node_count + 1;
        uint32_t *ptr = g->row_ptr;
        while (rem > 0) {
            uint32_t chunk = rem > 64 ? 64 : rem;
            if (fread(ptr, sizeof(uint32_t), chunk, f) != chunk) goto err;
            ptr += chunk; rem -= chunk;
        }
    }
    printf("astar: row_ptr loaded  heap=%lu\n",
           (unsigned long)esp_get_free_heap_size());

    /* Record where AdjData begins in the file */
    g->adj_file_offset = ftell(f);

    fclose(f);
    printf("astar: loaded OK  adj at file offset %ld\n", g->adj_file_offset);
    return 0;

err:
    printf("astar: load failed\n");
    if (f) fclose(f);
    astar_free_graph(g);
    return -1;
}

void astar_free_graph(Graph *g)
{
    free(g->nodes);   g->nodes   = NULL;
    free(g->edges);   g->edges   = NULL;
    free(g->row_ptr); g->row_ptr = NULL;
}

int astar_find(const Graph *g,
               uint32_t     start,
               uint32_t     end,
               uint32_t    *out_path,
               uint32_t     max_path)
{
    uint32_t N = g->node_count;
    if (start >= N || end >= N) return ASTAR_ERR_INVALID_NODE;

    /* Open file for adj reads during search */
    FILE *f = fopen(g->path, "rb");
    if (!f) return ASTAR_ERR_IO;

    uint32_t *g_score   = (uint32_t *)malloc(N * sizeof(uint32_t));
    uint32_t *came_from = (uint32_t *)malloc(N * sizeof(uint32_t));
    uint8_t  *closed    = (uint8_t  *)calloc(N, 1);

    if (!g_score || !came_from || !closed) {
        free(g_score); free(came_from); free(closed);
        fclose(f);
        return ASTAR_ERR_OOM;
    }

    for (uint32_t i = 0; i < N; i++) {
        g_score[i]   = UINT32_MAX;
        came_from[i] = UINT32_MAX;
    }
    g_score[start] = 0u;

    MinHeap heap;
    if (heap_init(&heap, 512) != 0) {
        free(g_score); free(came_from); free(closed);
        fclose(f);
        return ASTAR_ERR_OOM;
    }

    heap_push(&heap, heuristic(&g->nodes[start], &g->nodes[end]), start);

    int result = ASTAR_ERR_NO_PATH;

    /* Small stack buffer for reading a node's neighbors from file.
     * Most nodes have < 8 neighbors; 32 is generous headroom. */
    AdjEntry nbuf[32];

    while (heap.size > 0) {
        HeapItem cur = heap_pop(&heap);
        uint32_t q   = cur.node;

        if (q == end)  { result = 0; break; }
        if (closed[q]) continue;
        closed[q] = 1;

        uint32_t ei_start = g->row_ptr[q];
        uint32_t ei_end   = g->row_ptr[q + 1];
        uint32_t count    = ei_end - ei_start;
        if (count == 0) continue;

        /* Seek to this node's adjacency entries in the file */
        long seek_pos = g->adj_file_offset
                        + (long)ei_start * (long)sizeof(AdjEntry);
        if (fseek(f, seek_pos, SEEK_SET) != 0) {
            result = ASTAR_ERR_IO; break;
        }

        /* Read all neighbors of q in one shot (count is small, ~2-8) */
        uint32_t to_read = count < 32 ? count : 32;
        if (fread(nbuf, sizeof(AdjEntry), to_read, f) != to_read) {
            result = ASTAR_ERR_IO; break;
        }

        for (uint32_t i = 0; i < to_read; i++) {
            uint32_t nb   = nbuf[i].node;
            uint32_t cost = nbuf[i].cost;

            if (closed[nb]) continue;
            if (g_score[q] > UINT32_MAX - cost) continue;

            uint32_t tg = g_score[q] + cost;
            if (tg < g_score[nb]) {
                g_score[nb]   = tg;
                came_from[nb] = q;
                uint32_t h = heuristic(&g->nodes[nb], &g->nodes[end]);
                uint32_t f_score = (tg > UINT32_MAX - h) ? UINT32_MAX : tg + h;
                heap_push(&heap, f_score, nb);
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
            for (uint32_t i = len; i-- > 0;) {
                out_path[i] = cur;
                cur = came_from[cur];
            }
            result = (int)len;
        }
    }

    fclose(f);
    heap_free(&heap);
    free(g_score);
    free(came_from);
    free(closed);
    return result;
}