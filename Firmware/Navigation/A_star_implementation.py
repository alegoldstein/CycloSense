# A_star_implementation.py
# A* using tiled map loading — simulates SD card access on embedded

import heapq
from heuristic import haversine_heuristic
from tiling import TileCache, lat_lon_to_tile

def astar(nodes_all, adjacency_all, start_node, end_node, G,
          use_tiles=False, tile_dir='tiles'):
    """
    A* pathfinding.

    Two modes:
      use_tiles=False  — use preloaded adjacency dict (Pi mode, fast)
      use_tiles=True   — load tiles on demand from disk (SD card simulation)

    Parameters:
      nodes_all    : full node dict {node_id: {'lat':..., 'lon':...}}
      adjacency_all: full adjacency dict (ignored when use_tiles=True)
      start_node   : integer OSM node ID
      end_node     : integer OSM node ID
      G            : OSMnx graph (used for node coordinates)
      use_tiles    : if True, simulate SD card tile loading
      tile_dir     : directory containing tile binary files
    """

    # initialize
    open_list   = []
    closed_list = set()
    f_scores    = {}
    g_scores    = {}
    came_from   = {}
    binary_reads = 0

    # tile cache (only used when use_tiles=True)
    tile_cache = TileCache(tile_dir=tile_dir, max_tiles=6) if use_tiles else None

    # end node coordinates for heuristic
    end_lat = G.nodes[end_node]['y']
    end_lon = G.nodes[end_node]['x']
    cord2   = (end_lat, end_lon)

    # initialize start
    g_scores[start_node] = 0
    f_scores[start_node] = 0
    heapq.heappush(open_list, (0, start_node))

    while open_list:
        current_f, q = heapq.heappop(open_list)

        # found goal
        if q == end_node:
            _print_stats(binary_reads, len(closed_list), tile_cache)
            return reconstruct_path(came_from, q)

        # skip if already finalized
        if q in closed_list:
            continue
        closed_list.add(q)

        # get neighbors — either from preloaded adjacency or tile cache
        if use_tiles:
            q_lat = G.nodes[q]['y']
            q_lon = G.nodes[q]['x']
            neighbors = tile_cache.get_neighbors(q, q_lat, q_lon)
        else:
            neighbors = adjacency_all.get(q, [])

        for node, cost, edge in neighbors:
            binary_reads += 1  # each neighbor = one edge record read

            if node in closed_list:
                continue

            tentative_g = g_scores[q] + cost
            if tentative_g < g_scores.get(node, float('inf')):
                came_from[node] = q
                g_scores[node]  = tentative_g

                cord1   = (G.nodes[node]['y'], G.nodes[node]['x'])
                h_score = haversine_heuristic(cord1, cord2)
                f_scores[node] = tentative_g + h_score
                heapq.heappush(open_list, (f_scores[node], node))

    _print_stats(binary_reads, len(closed_list), tile_cache)
    return None


def _print_stats(binary_reads, nodes_explored, tile_cache=None):
    print(f"\n── A* Stats ──────────────────────────")
    print(f"  Edge reads (binary):  {binary_reads}")
    print(f"  Nodes explored:       {nodes_explored}")
    print(f"  SD latency estimate:  {binary_reads * 0.001:.2f}s (@ 1ms/read)")
    if tile_cache:
        tile_cache.stats()
    print(f"──────────────────────────────────────\n")


def reconstruct_path(came_from, current):
    total_path = [current]
    while current in came_from:
        current = came_from[current]
        total_path.append(current)
    return total_path[::-1]