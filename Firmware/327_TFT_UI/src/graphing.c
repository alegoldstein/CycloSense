#include "graphing.h"
#include "tft_wrapper.h"
#include "astar.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#define MAP_ORIGIN_LAT   42.056871
#define MAP_ORIGIN_LON  -87.679689
#define EARTH_RADIUS_M   6371000.0

#define SCALE_FP  ((MAP_PX * 256) / (VIEW_RANGE_M * 2))

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

/* clamp a y screen coordinate to stay within the map area */
static inline int16_t clamp_y(int16_t y)
{
    if (y < 0)           return 0;
    if (y >= MAP_HEIGHT) return MAP_HEIGHT - 1;
    return y;
}

void draw_background(const Graph *g)
{
    tft_fill_rect(0, 0, MAP_PX, MAP_HEIGHT, COLOR_BLACK);

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

            int16_t x0 = to_screen_x(a->x_m);
            int16_t y0 = clamp_y(to_screen_y(a->y_m));
            int16_t x1 = to_screen_x(b->x_m);
            int16_t y1 = clamp_y(to_screen_y(b->y_m));

            if (y0 >= MAP_HEIGHT && y1 >= MAP_HEIGHT) continue;

            tft_draw_line(x0, y0, x1, y1, COLOR_WHITE);
            tft_draw_line(x0+1, y0+1, x1+1, y1+1, COLOR_WHITE);
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
        int16_t y0 = clamp_y(to_screen_y(a->y_m));
        int16_t x1 = to_screen_x(b->x_m);
        int16_t y1 = clamp_y(to_screen_y(b->y_m));

        if (y0 >= MAP_HEIGHT && y1 >= MAP_HEIGHT) continue;

        tft_draw_line(x0,   y0,   x1,   y1,   COLOR_GREEN);
        tft_draw_line(x0+1, y0+1,   x1+1, y1+1,   COLOR_GREEN);
        tft_draw_line(x0-1, y0-1,   x1-1, y1-1,   COLOR_GREEN);
        tft_draw_line(x0+2,   y0+2, x1+2,   y1+2, COLOR_GREEN);
        tft_draw_line(x0-2,   y0-2, x1-2,   y1-2, COLOR_GREEN);
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

    int16_t sx = to_screen_x(user_x);
    int16_t sy = clamp_y(to_screen_y(user_y));
    tft_fill_circle(sx, sy, 5, COLOR_RED);
    return redraw;
}

void draw_metrics(uint32_t speed_kmh, uint8_t hours, uint8_t minutes)
{
    char buf[20];

    /* speed — always shows "Speed: X km/h" */
    snprintf(buf, sizeof(buf), "Speed:%3lu km/h", (unsigned long)speed_kmh);
    tft_draw_string(5, METRICS_Y + 10, buf, COLOR_RED, 5);
    tft_fill_circle(5,280,5,COLOR_RED);
    printf("drawing metrics\n");

    /* time — shows label always, value only when GPS valid */
    if (hours != 0 || minutes != 0) {
        snprintf(buf, sizeof(buf), "Time: %02u:%02u   ", hours, minutes);
    } else {
        snprintf(buf, sizeof(buf), "Time: --:--   ");
    }
    tft_draw_string(5, METRICS_Y + 40, buf, COLOR_WHITE, 2);
}