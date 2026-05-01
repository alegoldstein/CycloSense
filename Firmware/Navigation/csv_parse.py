#turn csv data into a dictionary with info for A*

import osmnx as ox

#inputs to graph extraction

#set defaults
# center_point = (42.0411, 87.6901)
# radius = 3000
# dist_type = 
# net_type = 
# simplify = False
# retain_all = True
# truncate_edge = True
# custom_filter = None

def fetch_nodes_edges(center_point=(42.0411, -87.6901),
                      radius=3000,
                      dist_type='bbox',
                      net_type='bike',
                      simplify=False,
                      retain_all=True,
                      truncate_edge=True,
                      custom_filter=None):

    user_input = input("Use default settings? yes/no: ").strip().lower()
    use_defaults = user_input in ("yes", "y", "true", "t")

    if not use_defaults:
        lat = float(input("center point latitude: "))
        lon = float(input("center point longitude: "))
        center_point = (lat, lon)

        radius = float(input("radius in meters: "))
        dist_type = input("distance type (bbox/network): ")
        net_type = input("network type (bike/walk/drive/all): ")

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

    for node_id, data in G.nodes(data=True):
        nodes[node_id] = {
            'lat': data['y'],
            'lon': data['x']
        }

    for u, v, data in G.edges(data=True):
        name = data.get('name', '')
        if isinstance(name, list):
            name = name[0]
        highway = data.get('highway', 'unclassified')
        if isinstance(highway, list):
            highway = highway[0]

        edges.append({
            'u': u,
            'v': v,
            'length': data.get('length', 0),
            'name':    name,
            'highway': highway,
            'oneway':  data.get('oneway', False)
        })

    return nodes, edges