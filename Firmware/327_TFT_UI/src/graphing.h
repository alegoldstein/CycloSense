#ifndef GRAPHING_H
#define GRAPHING_H

#include <stdint.h>
#include "astar.h"

#define MAP_PX        240
#define MAP_HEIGHT    240
#define METRICS_Y     241
#define VIEW_RANGE_M  150

#ifdef __cplusplus
extern "C" {
#endif

void draw_background(const Graph *g);
void draw_route(const Graph *g, const uint32_t *path, int path_len);
int  draw_user(const Graph *g, const uint32_t *path, int path_len,
               double cur_lat, double cur_lon);
void draw_metrics(uint32_t speed_kmh, uint8_t hours, uint8_t minutes);

#ifdef __cplusplus
}
#endif

#endif