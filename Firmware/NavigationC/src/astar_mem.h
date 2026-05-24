/*
 * astar_mem.h — load a Graph from a RAM buffer instead of a file.
 *
 * Binary layout is identical to the .bin written by csv_parse.py.
 * Use this during bringup before SD card / SPIFFS is wired up.
 * Replace the call with astar_load_graph() once storage is ready.
 *
 * astar_free_graph() works the same to release all heap memory.
 */

#ifndef ASTAR_MEM_H
#define ASTAR_MEM_H

#include <stdint.h>
#include "astar.h"

/**
 * Load a Graph from `buf` (length `len` bytes).
 * Returns 0 on success, -1 on truncated buffer or OOM.
 * Call astar_free_graph() when done.
 */
int astar_load_graph_from_mem(Graph *g, const uint8_t *buf, uint32_t len);

#endif /* ASTAR_MEM_H */