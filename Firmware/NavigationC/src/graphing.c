#include "graphing.h"
#include "display.h"
#include "astar.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAP_ORIGIN_LAT   42.056871
#define MAP_ORIGIN_LON  -87.679689
#define EARTH_RADIUS_M   6371000.0

#define SCALE_FP  ((MAP_PX * 256) / (VIEW_RANGE_M * 2))

#define METRICS_Y    240

static int16_t curr_center_x = 0;
static int16_t curr_center_y = 0;

static void latlon_to_xy(double lat, double lon, int16_t *x_m, int16_t *y_m)
{
    double lat_rad        = lat            * M_PI / 180.0;
    double origin_lat_rad = MAP_ORIGIN_LAT * M_PI / 180.0;
    double dlat = (lat - MAP_ORIGIN_LAT) * M_PI / 180.0;
    double dlon = (lon - MAP_ORIGIN_LON) * M_PI / 180.0;
    *x_m = (int16_t)(dlon * cos((lat_rad + origin_lat_rad) * 0.5) * EARTH_RADIUS_M);
    *y_m = (int16_t)(dlat * EARTH_RADIUS_M);
}

static inline int16_t to_screen_x(int16_t x_m)
{
    return (int16_t)(MAP_PX / 2 + ((int32_t)(x_m - curr_center_x) * SCALE_FP >> 8));
}

static inline int16_t to_screen_y(int16_t y_m)
{
    return (int16_t)(MAP_PX / 2 - ((int32_t)(y_m - curr_center_y) * SCALE_FP >> 8));
}

static inline int in_view(int16_t x_m, int16_t y_m)
{
    return (x_m >= curr_center_x - VIEW_RANGE_M &&
            x_m <= curr_center_x + VIEW_RANGE_M &&
            y_m >= curr_center_y - VIEW_RANGE_M &&
            y_m <= curr_center_y + VIEW_RANGE_M);
}

void draw_background(const Graph *g)
{
    display_fill_rect(0, 0, MAP_PX, MAP_PX, COLOR_BLACK);

    FILE *f = fopen("/spiffs/graph.bin", "rb");
    if (!f) return;

    fseek(f, g->adj_file_offset, SEEK_SET);

    for (uint32_t i = 0; i < g->node_count; i++) {
        uint32_t start = g->row_ptr[i];
        uint32_t end   = g->row_ptr[i + 1];
        const Node *a  = &g->nodes[i];

        for (uint32_t ei = start; ei < end; ei++) {
            AdjEntry ae;
            fread(&ae, sizeof(AdjEntry), 1, f);

            if (ae.node < i) continue;

            const Node *b = &g->nodes[ae.node];
            if (!in_view(a->x_m, a->y_m) && !in_view(b->x_m, b->y_m))
                continue;

            display_draw_line(
                to_screen_x(a->x_m), to_screen_y(a->y_m),
                to_screen_x(b->x_m), to_screen_y(b->y_m),
                COLOR_DARKGREY);
        }
    }

    fclose(f);
}

void draw_route(const Graph *g, const uint32_t *path, int path_len)
{
    for (int i = 0; i + 1 < path_len; i++) {
        const Node *a = &g->nodes[path[i]];
        const Node *b = &g->nodes[path[i + 1]];

        if (!in_view(a->x_m, a->y_m) && !in_view(b->x_m, b->y_m))
            continue;

        int16_t x0 = to_screen_x(a->x_m);
        int16_t y0 = to_screen_y(a->y_m);
        int16_t x1 = to_screen_x(b->x_m);
        int16_t y1 = to_screen_y(b->y_m);

        display_draw_line(x0,   y0,   x1,   y1,   COLOR_BLUE);
        display_draw_line(x0+1, y0,   x1+1, y1,   COLOR_BLUE);
        display_draw_line(x0,   y0+1, x1,   y1+1, COLOR_BLUE);
    }
}

int draw_user(const Graph *g, const uint32_t *path, int path_len,
              double cur_lat, double cur_lon)
{
    int16_t user_x, user_y;
    latlon_to_xy(cur_lat, cur_lon, &user_x, &user_y);

    int redraw = 0;
    int16_t threshold = VIEW_RANGE_M * 6 / 10;
    if (abs(user_x - curr_center_x) > threshold ||
        abs(user_y - curr_center_y) > threshold) {
        curr_center_x = user_x;
        curr_center_y = user_y;
        redraw = 1;
        draw_background(g);
        draw_route(g, path, path_len);
    }

    display_fill_circle(to_screen_x(user_x), to_screen_y(user_y), 5, COLOR_RED);
    return redraw;
}

void draw_metrics(uint32_t speed_kmh, uint8_t hours, uint8_t minutes)
{
    char buf[16];
    display_fill_rect(0, METRICS_Y, 240, 80, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "%lu km/h", (unsigned long)speed_kmh);
    display_draw_string(10, METRICS_Y + 20, buf, COLOR_WHITE, 3);

    snprintf(buf, sizeof(buf), "%02u:%02u", hours, minutes);
    display_draw_string(150, METRICS_Y + 20, buf, COLOR_WHITE, 3);
}