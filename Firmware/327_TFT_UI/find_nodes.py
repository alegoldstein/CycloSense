# find_nodes.py
#
# Fetches OSM bike graph and writes data/graph.bin in the format
# expected by astar.c:
#
#   Header   : uint32 node_count, uint32 adj_total        (8 bytes)
#   Nodes    : node_count × { int16 x_m, int16 y_m }      (4 bytes each)
#   row_ptr  : (node_count+1) × uint32                    (4 bytes each)
#   AdjData  : adj_total × { uint16 node, uint16 cost }   (4 bytes each)
#
# AdjData is pre-built CSR so the ESP32 can seek directly to any
# node's neighbor list without loading it all into RAM.

import osmnx as ox
import struct
import math
import os
from collections import defaultdict

EARTH_RADIUS_M = 6371000.0

CENTER      = (42.056871, -87.679689)
RADIUS_M    = 3000
NET_TYPE    = 'bike'

START_COORD = (42.056871, -87.679689)
END_COORD   = (42.059106, -87.674741)

OUTPUT_BIN  = 'data/graph.bin'

HIGHWAY_MAP = {
    'cycleway': 0, 'residential': 1, 'primary': 2,
    'secondary': 3, 'tertiary': 4, 'footway': 5,
    'path': 6, 'service': 7, 'unclassified': 8,
}

def latlon_to_xy(lat, lon, origin_lat, origin_lon):
    lat_rad        = math.radians(lat)
    origin_lat_rad = math.radians(origin_lat)
    dlat = math.radians(lat - origin_lat)
    dlon = math.radians(lon - origin_lon)
    x = dlon * math.cos((lat_rad + origin_lat_rad) * 0.5) * EARTH_RADIUS_M
    y = dlat * EARTH_RADIUS_M
    return x, y

def find_nearest(target_lat, target_lon, nodes_raw, id_map):
    best_osm  = None
    best_dist = float('inf')
    for osm_id, d in nodes_raw.items():
        dlat = d['lat'] - target_lat
        dlon = d['lon'] - target_lon
        dist = dlat*dlat + dlon*dlon
        if dist < best_dist:
            best_dist = dist
            best_osm  = osm_id
    return id_map[best_osm], best_osm, nodes_raw[best_osm]

def main():
    origin_lat, origin_lon = CENTER

    print(f"Fetching OSM graph — centre {CENTER}, radius {RADIUS_M}m ...")
    G = ox.graph_from_point(
        CENTER, dist=RADIUS_M, dist_type='bbox',
        network_type=NET_TYPE, simplify=False,
        retain_all=True, truncate_by_edge=True,
    )

    # Build node list
    nodes_raw = {}
    for osm_id, data in G.nodes(data=True):
        x_m, y_m = latlon_to_xy(data['y'], data['x'], origin_lat, origin_lon)
        xi = max(-32767, min(32767, int(round(x_m))))
        yi = max(-32767, min(32767, int(round(y_m))))
        nodes_raw[osm_id] = {
            'lat': data['y'], 'lon': data['x'],
            'x_m': xi, 'y_m': yi,
        }

    node_ids = list(nodes_raw.keys())
    id_map   = {old: new for new, old in enumerate(node_ids)}
    N = len(node_ids)

    # Build edge list
    edges = []
    for u, v, data in G.edges(data=True):
        highway   = data.get('highway', 'unclassified')
        if isinstance(highway, list): highway = highway[0]
        length_dm = int(round(data.get('length', 0.0) * 10.0))
        length_dm = max(0, min(length_dm, 65535))
        edges.append({
            'u': id_map[u], 'v': id_map[v],
            'length_dm': length_dm,
            'oneway': data.get('oneway', False),
        })

    # Build CSR adjacency in Python
    deg = [0] * N
    for e in edges:
        deg[e['u']] += 1
        if not e['oneway']:
            deg[e['v']] += 1

    row_ptr = [0] * (N + 1)
    for i in range(N):
        row_ptr[i + 1] = row_ptr[i] + deg[i]

    adj_total = row_ptr[N]
    adj_node  = [0] * adj_total
    adj_cost  = [0] * adj_total
    cur       = [0] * N

    for e in edges:
        pu = row_ptr[e['u']] + cur[e['u']]; cur[e['u']] += 1
        adj_node[pu] = e['v']
        adj_cost[pu] = e['length_dm']
        if not e['oneway']:
            pv = row_ptr[e['v']] + cur[e['v']]; cur[e['v']] += 1
            adj_node[pv] = e['u']
            adj_cost[pv] = e['length_dm']

    # Find nearest nodes
    s_local, s_osm, s_data = find_nearest(*START_COORD, nodes_raw, id_map)
    e_local, e_osm, e_data = find_nearest(*END_COORD,   nodes_raw, id_map)

    # Write binary
    os.makedirs(os.path.dirname(OUTPUT_BIN), exist_ok=True)
    with open(OUTPUT_BIN, 'wb') as f:
        # Header
        f.write(struct.pack('<II', N, adj_total))
        # Nodes (int16 metres)
        for osm_id in node_ids:
            d = nodes_raw[osm_id]
            f.write(struct.pack('<hh', d['x_m'], d['y_m']))
        # row_ptr
        for rp in row_ptr:
            f.write(struct.pack('<I', rp))
        # AdjData (uint16 node, uint16 cost)
        for i in range(adj_total):
            f.write(struct.pack('<HH', adj_node[i], adj_cost[i]))

    size = os.path.getsize(OUTPUT_BIN)
    expected = 8 + N*4 + (N+1)*4 + adj_total*4
    assert size == expected, f"Size mismatch: {size} vs {expected}"

    print()
    print("─" * 52)
    print(f"  Nodes       : {N:,}")
    print(f"  Directed adj: {adj_total:,}")
    print(f"  File size   : {size:,} bytes ({size/1024:.1f} KB)")
    print(f"  ESP32 RAM   : ~{(N*4 + (N+1)*4)//1024} KB (nodes+row_ptr only)")
    print("─" * 52)
    print(f"  START  local ID : {s_local}")
    print(f"         lat/lon  : {s_data['lat']:.6f}, {s_data['lon']:.6f}")
    print()
    print(f"  END    local ID : {e_local}")
    print(f"         lat/lon  : {e_data['lat']:.6f}, {e_data['lon']:.6f}")
    print("─" * 52)
    print()
    print("Paste into main.c:")
    print(f"  #define ROUTE_START_NODE  {s_local}u")
    print(f"  #define ROUTE_END_NODE    {e_local}u")
    print()
    print(f"Binary written → {OUTPUT_BIN}")
    print("Next: pio run --target uploadfs  then  pio run --target upload")

if __name__ == '__main__':
    main()