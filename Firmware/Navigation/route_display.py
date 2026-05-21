# route_display.py
# fetch graph, run A* in flat or tiled mode, display route

import osmnx as ox
import matplotlib.pyplot as plt
from csv_parse import fetch_nodes_edges, build_adjacency, export_binary, export_graph_tiles
from A_star_implementation import astar
import time

def calculate_and_display_route(start_coord, end_coord,
                                 use_tiles=False, tile_dir='tiles'):
    """
    Calculate A* route and display on map.

    start_coord : (lat, lon)
    end_coord   : (lat, lon)
    use_tiles   : False = load full graph into RAM (fast)
                  True  = simulate SD card tile loading (slower, embedded-like)
    """

    # ── fetch graph ──────────────────────────────────────────────────────────
    nodes, edges, G = fetch_nodes_edges(use_defaults=True)

    # ── export binary files ──────────────────────────────────────────────────
    export_binary(nodes, edges, 'graph.bin')       # single file for reference
    export_graph_tiles(nodes, edges, tile_dir)     # tiled files for SD sim

    # ── build in-memory adjacency (used when use_tiles=False) ────────────────
    adjacency = build_adjacency(edges) if not use_tiles else {}

    # ── snap coords to nearest graph nodes ───────────────────────────────────
    start_node = ox.nearest_nodes(G, X=start_coord[1], Y=start_coord[0])
    end_node   = ox.nearest_nodes(G, X=end_coord[1],   Y=end_coord[0])

    print(f"Start node ID : {start_node}")
    print(f"End node ID   : {end_node}")
    print(f"Total nodes   : {len(nodes)}")
    print(f"Total edges   : {len(edges)}")
    print(f"Mode          : {'tiled SD simulation' if use_tiles else 'full RAM'}")

    # ── run A* ───────────────────────────────────────────────────────────────
    t0    = time.perf_counter()
    route = astar(nodes, adjacency, start_node, end_node, G,
                  use_tiles=use_tiles, tile_dir=tile_dir)
    elapsed = time.perf_counter() - t0

    if route is None:
        print("No route found")
        return

    print(f"Route found   : {len(route)} nodes")
    print(f"Wall time     : {elapsed*1000:.1f} ms")

    # ── display ───────────────────────────────────────────────────────────────
    fig, ax = ox.plot_graph_route(G, route, route_linewidth=6,
                                  node_size=0, bgcolor='k')
    out = 'route_tiled.png' if use_tiles else 'route.png'
    plt.savefig(out, dpi=300, bbox_inches='tight')
    print(f"Saved: {out}")


if __name__ == "__main__":
    start = (42.052225, -87.677267)
    end   = (42.059128, -87.674679)

    print("\n=== Mode 1: Full graph in RAM ===")
    calculate_and_display_route(start, end, use_tiles=False)

    print("\n=== Mode 2: Tiled SD card simulation ===")
    calculate_and_display_route(start, end, use_tiles=True)