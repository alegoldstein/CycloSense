/*
 * astar.h — public interface for ESP32 A* pathfinder
 */

#ifndef ASTAR_H
#define ASTAR_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Structs — layout must match the binary written by csv_parse.py
 * ---------------------------------------------------------------------- */

#pragma pack(push, 1)

typedef struct {
    int32_t x_mm;
    int32_t y_mm;
} Node;

typedef struct {
    uint32_t u;
    uint32_t v;
    uint16_t length_dm;
    uint8_t highway;
    uint8_t oneway;
} Edge;

#pragma pack(pop)
/* Graph holds all data + CSR adjacency built at load time */
typedef struct {
    uint32_t  node_count;
    uint32_t  edge_count;
    Node     *nodes;      /* [node_count] */
    Edge     *edges;      /* [edge_count] */
    /* CSR adjacency */
    uint32_t *row_ptr;    /* [node_count + 1] */
    uint32_t *adj_node;   /* [total directed edges] */
    uint32_t *adj_cost;   /* [total directed edges] (cost in decimetres)
                            stored as integer to avoid floating math */
} Graph;

/* -------------------------------------------------------------------------
 * Return codes for astar_find()
 * ---------------------------------------------------------------------- */
#define ASTAR_ERR_OOM            (-1)
#define ASTAR_ERR_NO_PATH        (-2)
#define ASTAR_ERR_INVALID_NODE   (-3)
#define ASTAR_ERR_PATH_TOO_LONG  (-4)

/* -------------------------------------------------------------------------
 * API
 * ---------------------------------------------------------------------- */

/**
 * Load graph binary from `path` (e.g. "/sdcard/graph.bin") into `g`.
 * Allocates all memory; call astar_free_graph() when done.
 * Returns 0 on success, -1 on error.
 */
int astar_load_graph(Graph *g, const char *path);

/**
 * Release all memory allocated by astar_load_graph().
 */
void astar_free_graph(Graph *g);

/**
 * Run A* from `start` to `end` (local 0-based node IDs).
 *
 * On success: writes the path into out_path[0..len-1] and returns len.
 * On failure: returns a negative ASTAR_ERR_* code.
 *
 * `max_path` is the size of the out_path buffer.
 */
int astar_find(const Graph *g,
               uint32_t     start,
               uint32_t     end,
               uint32_t    *out_path,
               uint32_t     max_path);

#endif /* ASTAR_H */