/*
 * astar_mem.c — astar_load_graph_from_mem()
 *
 * Mirrors astar_load_graph() but reads from a const uint8_t* buffer.
 * astar.c is left completely unmodified.
 */

#include "astar_mem.h"
#include "Astar.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { const uint8_t *p; const uint8_t *end; } Cursor;

static int cur_read(Cursor *c, void *dst, uint32_t n)
{
    if (c->p + n > c->end) return -1;
    memcpy(dst, c->p, n);
    c->p += n;
    return 0;
}

/* CSR build — identical to the private function in astar.c */
static int build_adjacency(Graph *g)
{
    uint32_t N = g->node_count;
    uint32_t *deg = (uint32_t *)calloc(N + 1, sizeof(uint32_t));
    if (!deg) return -1;

    for (uint32_t i = 0; i < g->edge_count; i++) {
        deg[g->edges[i].u]++;
        if (!g->edges[i].oneway) deg[g->edges[i].v]++;
    }

    g->row_ptr = (uint32_t *)malloc((N + 1) * sizeof(uint32_t));
    if (!g->row_ptr) { free(deg); return -1; }
    g->row_ptr[0] = 0;
    for (uint32_t i = 0; i < N; i++)
        g->row_ptr[i + 1] = g->row_ptr[i] + deg[i];

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

int astar_load_graph_from_mem(Graph *g, const uint8_t *buf, uint32_t len)
{
    memset(g, 0, sizeof(Graph));
    Cursor c = { .p = buf, .end = buf + len };

    if (cur_read(&c, &g->node_count, 4) != 0 ||
        cur_read(&c, &g->edge_count, 4) != 0) {
        printf("astar_mem: header read failed\n");
        return -1;
    }

    if (g->node_count == 0 || g->node_count > 500000 ||
        g->edge_count == 0 || g->edge_count > 2000000) {
        printf("astar_mem: bad counts n=%lu e=%lu\n",
               (unsigned long)g->node_count, (unsigned long)g->edge_count);
        return -1;
    }

    uint32_t nb = g->node_count * (uint32_t)sizeof(Node);
    uint32_t eb = g->edge_count * (uint32_t)sizeof(Edge);

    g->nodes = (Node *)malloc(nb);
    if (!g->nodes || cur_read(&c, g->nodes, nb) != 0) {
        printf("astar_mem: nodes alloc/read failed\n"); goto err;
    }
    g->edges = (Edge *)malloc(eb);
    if (!g->edges || cur_read(&c, g->edges, eb) != 0) {
        printf("astar_mem: edges alloc/read failed\n"); goto err;
    }
    if (build_adjacency(g) != 0) {
        printf("astar_mem: adjacency OOM\n"); goto err;
    }

    free(g->edges);
    g->edges = NULL;

    printf("astar_mem: loaded %lu nodes %lu edges from buffer\n",
           (unsigned long)g->node_count, (unsigned long)g->edge_count);
    return 0;
err:
    astar_free_graph(g);
    return -1;
}