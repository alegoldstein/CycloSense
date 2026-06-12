/*
 * astar.h — public interface for ESP32 A* pathfinder
 *
 * Adjacency data stays in SPIFFS — never loaded into RAM.
 * Only nodes[] and row_ptr[] are kept in RAM (~92 KB total).
 * A* reads neighbor lists directly from the file during search.
 */

#ifndef ASTAR_H
#define ASTAR_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)

typedef struct {
    int16_t x_m;
    int16_t y_m;
} Node;

typedef struct {
    uint32_t u;
    uint32_t v;
    uint16_t length_dm;
    uint8_t  highway;
    uint8_t  oneway;
} Edge;

typedef struct {
    uint16_t node;
    uint16_t cost;
} AdjEntry;

#pragma pack(pop)

/*
 * Graph RAM layout (~92 KB for 12k-node Evanston graph):
 *   nodes[]    46 KB   int16 x/y metres
 *   row_ptr[]  46 KB   uint32 offsets into adj section
 *
 * adj data stays in the open SPIFFS file.
 * adj_file_offset records where AdjEntry data begins in the file.
 * A* seeks + reads one AdjEntry at a time during search.
 */
typedef struct {
    uint32_t  node_count;
    uint32_t  edge_count;
    Node     *nodes;
    Edge     *edges;         /* always NULL after load */
    uint32_t *row_ptr;       /* [node_count + 1] — offsets into adj on disk */
    uint32_t  adj_total;     /* total directed edges */
    long      adj_file_offset; /* byte offset in graph.bin where adj begins */
    char      path[64];      /* kept so astar_find can open the file */
} Graph;

/* -------------------------------------------------------------------------
 * Return codes
 * ---------------------------------------------------------------------- */
#define ASTAR_ERR_OOM            (-1)
#define ASTAR_ERR_NO_PATH        (-2)
#define ASTAR_ERR_INVALID_NODE   (-3)
#define ASTAR_ERR_PATH_TOO_LONG  (-4)
#define ASTAR_ERR_IO             (-5)

/* -------------------------------------------------------------------------
 * API
 * ---------------------------------------------------------------------- */
int  astar_load_graph(Graph *g, const char *path);
void astar_free_graph(Graph *g);
int  astar_find(const Graph *g,
                uint32_t     start,
                uint32_t     end,
                uint32_t    *out_path,
                uint32_t     max_path);

#ifdef __cplusplus
}
#endif

#endif /* ASTAR_H */