# csv_parse.py
#
# ESP32-optimized graph exporter.
#
# Major optimizations:
#
# 1. NO node IDs stored
#    Node index IS the ID
#
# 2. Local Cartesian coordinates
#    x/y in millimeters relative to map center
#
# 3. Smaller edge cost
#    uint16 length_dm (decimeters)
#
# Result:
#
# Node size:
#   OLD = 12 bytes
#   NEW = 8 bytes
#
# Edge size:
#   OLD = 16 bytes
#   NEW = 12 bytes
#
# Very large RAM savings on ESP32.

import osmnx as ox
import struct
import os
import math
from collections import defaultdict

EARTH_RADIUS_M = 6371000.0

# ---------------------------------------------------------------------------
# Highway encoding
# ---------------------------------------------------------------------------

HIGHWAY_MAP = {
    'cycleway':     0,
    'residential':  1,
    'primary':      2,
    'secondary':    3,
    'tertiary':     4,
    'footway':      5,
    'path':         6,
    'service':      7,
    'unclassified': 8,
}

# ---------------------------------------------------------------------------
# Coordinate projection
# ---------------------------------------------------------------------------

def latlon_to_xy(lat, lon, origin_lat, origin_lon):
    """
    Convert lat/lon to local Cartesian meters.

    Accurate for small map regions.
    """

    lat_rad = math.radians(lat)
    origin_lat_rad = math.radians(origin_lat)

    dlat = math.radians(lat - origin_lat)
    dlon = math.radians(lon - origin_lon)

    x = dlon * math.cos((lat_rad + origin_lat_rad) * 0.5) * EARTH_RADIUS_M
    y = dlat * EARTH_RADIUS_M

    return x, y


# ---------------------------------------------------------------------------
# Fetch graph
# ---------------------------------------------------------------------------

def fetch_nodes_edges(
    center_point=(42.056871, -87.679689),
    radius=3000,
    dist_type='bbox',
    net_type='bike',
    simplify=False,
    retain_all=True,
    truncate_edge=True,
    custom_filter=None,
):

    user_input = input("Use default settings? yes/no: ").strip().lower()

    use_defaults = user_input in (
        "yes", "y", "true", "t"
    )

    if not use_defaults:

        lat = float(input("Center point latitude:  "))
        lon = float(input("Center point longitude: "))

        center_point = (lat, lon)

        radius = float(input("Radius in meters: "))

        dist_type = input(
            "Distance type (bbox/network): "
        )

        net_type = input(
            "Network type (bike/walk/drive/all): "
        )

        retain_all = input(
            "Retain all nodes? yes/no: "
        ).strip().lower() in (
            "yes", "y", "true", "t"
        )

        truncate_edge = input(
            "Truncate by edge? yes/no: "
        ).strip().lower() in (
            "yes", "y", "true", "t"
        )

    G = ox.graph_from_point(
        center_point,
        dist=radius,
        dist_type=dist_type,
        network_type=net_type,
        simplify=simplify,
        retain_all=retain_all,
        truncate_by_edge=truncate_edge,
        custom_filter=custom_filter,
    )

    origin_lat, origin_lon = center_point

    # -----------------------------------------------------------------------
    # Nodes
    #
    # Stored as:
    #   int32 x_mm
    #   int32 y_mm
    # -----------------------------------------------------------------------

    nodes = {}

    for node_id, data in G.nodes(data=True):

        lat = data['y']
        lon = data['x']

        x_m, y_m = latlon_to_xy(
            lat,
            lon,
            origin_lat,
            origin_lon
        )

        nodes[node_id] = {
            'x_mm': int(round(x_m * 1000.0)),
            'y_mm': int(round(y_m * 1000.0)),
        }

    # -----------------------------------------------------------------------
    # Build node remap
    #
    # Index in array = local node ID
    # -----------------------------------------------------------------------

    node_ids = list(nodes.keys())

    id_map = {
        old: new
        for new, old in enumerate(node_ids)
    }

    # -----------------------------------------------------------------------
    # Edges
    #
    # length_dm:
    #   uint16 decimeters
    #
    # Max:
    #   65535 dm = 6.5 km
    #
    # More than enough for local graph.
    # -----------------------------------------------------------------------

    edges = []

    for u, v, data in G.edges(data=True):

        highway = data.get(
            'highway',
            'unclassified'
        )

        if isinstance(highway, list):
            highway = highway[0]

        length_m = data.get('length', 0.0)

        length_dm = int(round(length_m * 10.0))

        # clamp for uint16 safety
        length_dm = max(
            0,
            min(length_dm, 65535)
        )

        edges.append({
            'u':          id_map[u],
            'v':          id_map[v],
            'length_dm':  length_dm,
            'highway':    highway,
            'oneway':     data.get('oneway', False),
        })

    return nodes, edges


# ---------------------------------------------------------------------------
# Adjacency helper
# ---------------------------------------------------------------------------

def build_adjacency(edges):

    adjacency = defaultdict(list)

    for edge in edges:

        u = edge['u']
        v = edge['v']

        cost = edge['length_dm']

        adjacency[u].append((v, cost, edge))

        if not edge['oneway']:
            adjacency[v].append((u, cost, edge))

    return adjacency


# ---------------------------------------------------------------------------
# Binary export
# ---------------------------------------------------------------------------

def export_binary_esp32(
    nodes,
    edges,
    output_file='graph.bin'
):
    """
    Binary format:

    Header:
        uint32 node_count
        uint32 edge_count

    Nodes:
        int32 x_mm
        int32 y_mm

    Edges:
        uint32 u
        uint32 v
        uint16 length_dm
        uint8  highway
        uint8  oneway
    """

    node_ids = list(nodes.keys())

    node_count = len(nodes)
    edge_count = len(edges)

    with open(output_file, 'wb') as f:

        # -------------------------------------------------------------------
        # Header
        # -------------------------------------------------------------------

        f.write(struct.pack(
            '<II',
            node_count,
            edge_count
        ))

        # -------------------------------------------------------------------
        # Nodes
        #
        # int32 x_mm
        # int32 y_mm
        #
        # 8 bytes/node
        # -------------------------------------------------------------------

        for node_id in node_ids:

            data = nodes[node_id]

            f.write(struct.pack(
                '<ii',
                data['x_mm'],
                data['y_mm']
            ))

        # -------------------------------------------------------------------
        # Edges
        #
        # uint32 u
        # uint32 v
        # uint16 length_dm
        # uint8  highway
        # uint8  oneway
        #
        # 12 bytes/edge
        # -------------------------------------------------------------------

        for edge in edges:

            highway = HIGHWAY_MAP.get(
                edge['highway'],
                8
            )

            oneway = (
                1 if edge['oneway']
                else 0
            )

            f.write(struct.pack(
                '<IIHBB',
                edge['u'],
                edge['v'],
                edge['length_dm'],
                highway,
                oneway
            ))

    size = os.path.getsize(output_file)

    print(f"\n── ESP32 binary: {output_file} ──")

    print(
        f"  Nodes : {node_count:,}  "
        f"({node_count * 8:,} bytes)"
    )

    print(
        f"  Edges : {edge_count:,}  "
        f"({edge_count * 12:,} bytes)"
    )

    print(
        f"  Total : {size:,} bytes  "
        f"({size / 1024:.1f} KB)\n"
    )

    return {
        'node_count': node_count,
        'edge_count': edge_count,
        'size_bytes': size,
    }


# ---------------------------------------------------------------------------
# Example usage
# ---------------------------------------------------------------------------

if __name__ == "__main__":

    nodes, edges = fetch_nodes_edges(
        radius=3000,
        net_type='bike'
    )

    export_binary_esp32(
        nodes,
        edges,
        "graph.bin"
    )