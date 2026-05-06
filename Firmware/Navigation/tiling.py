# tiling.py
# divides the map into geographic tiles and handles loading them on demand
# tile size is in degrees, ~1km x 1km at mid-latitudes

import os
import struct
import math

TILE_SIZE_DEG = 0.01  # ~1.1km per tile at 42 deg latitude

# ── Binary tile format ───────────────────────────────────────────────────────
#
# Header:   4 bytes node_count + 4 bytes edge_count + 4 bytes string_count
# Strings:  each entry: 2 bytes length + N bytes UTF-8
# Nodes:    each: 8 bytes osm_id (int64) + 4 bytes lat (f32) + 4 bytes lon (f32) = 16 bytes
# Edges:    each: 8 bytes u (int64) + 8 bytes v (int64) + 4 bytes cost (f32) +
#                 2 bytes name_idx + 1 byte highway + 1 byte oneway = 24 bytes
#
# Using int64 (Q) for node IDs because OSM IDs exceed int32 range

HIGHWAY_MAP = {
    'cycleway': 0, 'residential': 1, 'primary': 2,
    'secondary': 3, 'tertiary': 4,  'footway': 5,
    'path': 6,     'service': 7,    'unclassified': 8
}

# ── Tile coordinate math ─────────────────────────────────────────────────────

def lat_lon_to_tile(lat, lon):
    """Convert a lat/lon coordinate to a tile (row, col) index."""
    row = int(math.floor(lat / TILE_SIZE_DEG))
    col = int(math.floor(lon / TILE_SIZE_DEG))
    return (row, col)

def tile_bounds(row, col):
    """Return (min_lat, min_lon, max_lat, max_lon) for a tile."""
    min_lat = row * TILE_SIZE_DEG
    min_lon = col * TILE_SIZE_DEG
    return (min_lat, min_lon, min_lat + TILE_SIZE_DEG, min_lon + TILE_SIZE_DEG)

def tiles_for_bbox(min_lat, min_lon, max_lat, max_lon):
    """Return all tile (row, col) pairs that overlap a bounding box."""
    tiles = []
    for r in range(int(math.floor(min_lat / TILE_SIZE_DEG)),
                   int(math.floor(max_lat / TILE_SIZE_DEG)) + 1):
        for c in range(int(math.floor(min_lon / TILE_SIZE_DEG)),
                       int(math.floor(max_lon / TILE_SIZE_DEG)) + 1):
            tiles.append((r, c))
    return tiles

def tile_filename(row, col, tile_dir='tiles'):
    return os.path.join(tile_dir, f"tile_{row}_{col}.bin")

# ── Export ───────────────────────────────────────────────────────────────────

def export_tiles(nodes, edges, tile_dir='tiles'):
    """
    Split nodes and edges into geographic tiles, one binary file per tile.
    Cross-boundary edges are written to both tiles so A* never loses a
    connection at tile borders. Original OSM IDs are preserved throughout.
    """
    os.makedirs(tile_dir, exist_ok=True)

    # assign every node to its tile
    node_to_tile = {}
    tiles = {}  # (row, col) -> {'nodes': {}, 'edges': []}

    for node_id, data in nodes.items():
        tile = lat_lon_to_tile(data['lat'], data['lon'])
        node_to_tile[node_id] = tile
        if tile not in tiles:
            tiles[tile] = {'nodes': {}, 'edges': []}
        tiles[tile]['nodes'][node_id] = data

    # assign edges — cross-boundary edges go into both tiles
    for edge in edges:
        u_tile = node_to_tile.get(edge['u'])
        v_tile = node_to_tile.get(edge['v'])
        if u_tile is None or v_tile is None:
            continue

        if u_tile not in tiles:
            tiles[u_tile] = {'nodes': {}, 'edges': []}
        tiles[u_tile]['edges'].append(edge)

        if v_tile != u_tile:
            if v_tile not in tiles:
                tiles[v_tile] = {'nodes': {}, 'edges': []}
            tiles[v_tile]['edges'].append(edge)
            # ensure both endpoints exist in both tiles
            tiles[u_tile]['nodes'][edge['v']] = nodes[edge['v']]
            tiles[v_tile]['nodes'][edge['u']] = nodes[edge['u']]

    # write each tile
    total_bytes = 0
    for (row, col), tile_data in tiles.items():
        tile_nodes = tile_data['nodes']
        tile_edges = tile_data['edges']

        # build string table
        name_set   = ['']
        name_index = {'': 0}
        for edge in tile_edges:
            name = edge.get('name', '')
            if name not in name_index:
                name_index[name] = len(name_set)
                name_set.append(name)

        filepath = tile_filename(row, col, tile_dir)
        with open(filepath, 'wb') as f:
            # header
            f.write(struct.pack('III', len(tile_nodes), len(tile_edges), len(name_set)))

            # string table
            for name in name_set:
                enc = name.encode('utf-8')
                f.write(struct.pack('H', len(enc)))
                f.write(enc)

            # nodes: int64 osm_id, float32 lat, float32 lon  (16 bytes each)
            for node_id, data in tile_nodes.items():
                f.write(struct.pack('Qff', node_id, data['lat'], data['lon']))

            # edges: int64 u, int64 v, float32 cost, uint16 name_idx,
            #        uint8 highway, uint8 oneway  (24 bytes each)
            for edge in tile_edges:
                cost     = float(edge['length'])
                name_idx = name_index.get(edge.get('name', ''), 0)
                highway  = HIGHWAY_MAP.get(edge.get('highway', 'unclassified'), 8)
                oneway   = 1 if edge.get('oneway', False) else 0
                f.write(struct.pack('QQfHBB', edge['u'], edge['v'],
                                    cost, name_idx, highway, oneway))

        total_bytes += os.path.getsize(filepath)

    print(f"Exported {len(tiles)} tiles to '{tile_dir}/'")
    print(f"  Total size: {total_bytes} bytes ({total_bytes/1024:.1f} KB)")
    print(f"  Avg tile:   {total_bytes//len(tiles)} bytes ({total_bytes/len(tiles)/1024:.1f} KB)")
    return list(tiles.keys())

# ── Load ─────────────────────────────────────────────────────────────────────

def load_tile(row, col, tile_dir='tiles'):
    """
    Load one tile binary file.
    Returns (nodes dict, adjacency dict) keyed by original OSM IDs.
    Returns (None, None) if tile file does not exist.
    """
    filepath = tile_filename(row, col, tile_dir)
    if not os.path.exists(filepath):
        return None, None

    nodes     = {}
    adjacency = {}

    with open(filepath, 'rb') as f:
        node_count, edge_count, string_count = struct.unpack('III', f.read(12))

        # string table
        name_table = []
        for _ in range(string_count):
            length = struct.unpack('H', f.read(2))[0]
            name_table.append(f.read(length).decode('utf-8'))

        # nodes: int64 osm_id, float32 lat, float32 lon
        for _ in range(node_count):
            node_id, lat, lon = struct.unpack('Qff', f.read(16))
            nodes[node_id] = {'lat': lat, 'lon': lon}

        # edges: int64 u, int64 v, float32 cost, uint16 name_idx,
        #        uint8 highway, uint8 oneway
        for _ in range(edge_count):
            u, v, cost, name_idx, highway, oneway = struct.unpack('QQfHBB', f.read(24))
            name = name_table[name_idx] if name_idx < len(name_table) else ''
            edge = {'cost': cost, 'name': name, 'highway': highway, 'oneway': oneway}

            if u not in adjacency:
                adjacency[u] = []
            adjacency[u].append((v, cost, edge))

            if not oneway:
                if v not in adjacency:
                    adjacency[v] = []
                adjacency[v].append((u, cost, edge))

    return nodes, adjacency

# ── Tile cache ───────────────────────────────────────────────────────────────

class TileCache:
    """
    LRU tile cache — loads tiles from disk on demand, evicts least-recently-used
    when full. Simulates embedded system behavior with limited RAM.
    """
    def __init__(self, tile_dir='tiles', max_tiles=6):
        self.tile_dir     = tile_dir
        self.max_tiles    = max_tiles
        self.cache        = {}        # (row,col) -> (nodes, adjacency)
        self.access_order = []
        self.load_count   = 0         # total SD reads for benchmarking

    def get(self, row, col):
        key = (row, col)
        if key in self.cache:
            self.access_order.remove(key)
            self.access_order.append(key)
            return self.cache[key]

        nodes, adjacency = load_tile(row, col, self.tile_dir)
        if nodes is None:
            return None, None

        self.load_count += 1

        if len(self.cache) >= self.max_tiles:
            lru = self.access_order.pop(0)
            del self.cache[lru]

        self.cache[key] = (nodes, adjacency)
        self.access_order.append(key)
        return nodes, adjacency

    def get_neighbors(self, node_id, node_lat, node_lon):
        """Get neighbors of a node, loading its tile from disk if needed."""
        row, col = lat_lon_to_tile(node_lat, node_lon)
        _, adjacency = self.get(row, col)
        if adjacency is None:
            return []
        return adjacency.get(node_id, [])

    def stats(self):
        print(f"  Tile loads from disk: {self.load_count}")
        print(f"  Tiles in cache:       {len(self.cache)}")