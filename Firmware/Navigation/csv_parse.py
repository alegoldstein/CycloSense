# csv_parse.py
# fetch OSM data, build node/edge dicts, export binary tiles

import osmnx as ox
import struct
import os
from collections import defaultdict
from tiling import export_tiles

def fetch_nodes_edges(center_point=(42.056871, -87.679689),
                      radius=3000,
                      dist_type='bbox',
                      net_type='bike',
                      simplify=False,
                      retain_all=True,
                      truncate_edge=True,
                      custom_filter=None,
                      use_defaults=True):

    user_input = input("Use default settings? yes/no: ").strip().lower()
    use_defaults = user_input in ("yes", "y", "true", "t")

    if not use_defaults:
        lat = float(input("center point latitude: "))
        lon = float(input("center point longitude: "))
        center_point = (lat, lon)
        radius    = float(input("radius in meters: "))
        dist_type = input("distance type (bbox/network): ")
        net_type  = input("network type (bike/walk/drive/all): ")

        user_input = input("retain all nodes? yes/no: ").strip().lower()
        retain_all = user_input in ("yes", "y", "true", "t")

        user_input = input("truncate by edge? yes/no: ").strip().lower()
        truncate_edge = user_input in ("yes", "y", "true", "t")

    G = ox.graph_from_point(
        center_point,
        dist=radius,
        dist_type=dist_type,
        network_type=net_type,
        simplify=simplify,
        retain_all=retain_all,
        truncate_by_edge=truncate_edge,
        custom_filter=custom_filter
    )

    nodes = {}
    edges = []

# build node and edge dicts from graph
    for node_id, data in G.nodes(data=True):
        nodes[node_id] = {
            'lat': data['y'],
            'lon': data['x']
        }
#go through all edges and get start + end nodes, length, name, highway type, oneway
    for u, v, data in G.edges(data=True):
        name = data.get('name', '')
        if isinstance(name, list):
            name = name[0]
        highway = data.get('highway', 'unclassified')
        if isinstance(highway, list):
            highway = highway[0]

        edges.append({
            'u':      u,
            'v':      v,
            'length': data.get('length', 0),
            'name':   name,
            'highway': highway,
            'oneway': data.get('oneway', False)
        })

    return nodes, edges, G

#create list of neighbors for each node from edge list, used for A* when not using tiles
def build_adjacency(edges):
    adjacency = defaultdict(list)
    for edge in edges:
        u    = edge['u']
        v    = edge['v']
        cost = edge['length']
        adjacency[u].append((v, cost, edge))
        if not edge['oneway']:
            adjacency[v].append((u, cost, edge))
    return adjacency


def export_binary(nodes, edges, output_file='graph.bin'):
    """Export entire graph as a single binary file (non-tiled, for reference)."""
    node_ids = list(nodes.keys())
    id_map   = {old: new for new, old in enumerate(node_ids)}

    name_set   = ['']
    name_index = {'': 0}
    for edge in edges:
        name = edge['name']
        if name not in name_index:
            name_index[name] = len(name_set)
            name_set.append(name)

    highway_map = {
        'cycleway': 0, 'residential': 1, 'primary': 2,
        'secondary': 3, 'tertiary': 4,  'footway': 5,
        'path': 6,     'service': 7,    'unclassified': 8
    }

    with open(output_file, 'wb') as f:
        f.write(struct.pack('III', len(nodes), len(edges), len(name_set)))
        for name in name_set:
            enc = name.encode('utf-8')
            f.write(struct.pack('H', len(enc)))
            f.write(enc)
        for old_id, data in nodes.items():
            f.write(struct.pack('Iff', id_map[old_id], data['lat'], data['lon']))
        for edge in edges:
            u       = id_map.get(edge['u'], 0)
            v       = id_map.get(edge['v'], 0)
            cost    = float(edge['length'])
            nidx    = name_index.get(edge['name'], 0)
            highway = highway_map.get(edge['highway'], 8)
            oneway  = 1 if edge['oneway'] else 0
            f.write(struct.pack('IIfHBB', u, v, cost, nidx, highway, oneway))

    size = os.path.getsize(output_file)
    print(f"\n── Single binary: {output_file} ──")
    print(f"  Nodes: {len(nodes)}, Edges: {len(edges)}")
    print(f"  Size:  {size} bytes ({size/1024:.1f} KB)\n")


def export_graph_tiles(nodes, edges, tile_dir='tiles'):
    """Export graph as tiled binary files for SD card simulation."""
    print(f"\n── Exporting tiles to '{tile_dir}/' ──")
    export_tiles(nodes, edges, tile_dir)