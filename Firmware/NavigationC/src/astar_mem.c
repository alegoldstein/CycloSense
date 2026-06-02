/*
 * astar_mem.c — load Graph from RAM buffer (testing without SPIFFS)
 * Updated for new binary format: header | nodes | row_ptr | adj
 */

#include "astar_mem.h"
#include "astar.h"
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

int astar_load_graph_from_mem(Graph *g, const uint8_t *buf, uint32_t len)
{
    memset(g, 0, sizeof(Graph));
    strncpy(g->path, "mem", sizeof(g->path) - 1);

    Cursor c = { .p = buf, .end = buf + len };

    if (cur_read(&c, &g->node_count, 4) != 0 ||
        cur_read(&c, &g->adj_total,  4) != 0) {
        printf("astar_mem: header read failed\n"); return -1;
    }
    if (g->node_count == 0 || g->node_count > 65535) {
        printf("astar_mem: bad node_count\n"); return -1;
    }

    uint32_t nb  = g->node_count * (uint32_t)sizeof(Node);
    uint32_t rpb = (g->node_count + 1) * (uint32_t)sizeof(uint32_t);

    g->nodes = (Node *)malloc(nb);
    if (!g->nodes || cur_read(&c, g->nodes, nb) != 0) {
        printf("astar_mem: nodes failed\n"); goto err;
    }
    g->row_ptr = (uint32_t *)malloc(rpb);
    if (!g->row_ptr || cur_read(&c, g->row_ptr, rpb) != 0) {
        printf("astar_mem: row_ptr failed\n"); goto err;
    }

    /* For mem-based graph, adj_file_offset marks where adj starts in buf.
     * astar_find() uses the file path, so mem-based testing still needs
     * a real file. For unit tests, just use astar_load_graph() directly. */
    g->adj_file_offset = (long)(c.p - buf);

    printf("astar_mem: loaded %lu nodes  adj_total=%lu\n",
           (unsigned long)g->node_count, (unsigned long)g->adj_total);
    return 0;
err:
    astar_free_graph(g);
    return -1;
}