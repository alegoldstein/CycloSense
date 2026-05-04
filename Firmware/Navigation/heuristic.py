#heuristic calculation, get weight of path from current node to end goal
import math

#haversine for gps coordinates, accounts curvature of eartch
def haversine_heuristic(coord1, coord2):
    # Coordinates in decimal degrees (e.g., 28.6139, 77.2090)
    lat1, lon1 = coord1
    lat2, lon2 = coord2
    
    R = 6371.0  # Earth radius in kilometers
    
    # Convert degrees to radians
    phi1, phi2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlambda = math.radians(lon2 - lon1)
    
    # Haversine calculation
    a = math.sin(dphi / 2)**2 + \
        math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2)**2
    
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    
    return R * c

def fast_heuristic(coord1, coord2):
    # R is Earth radius, lat/lon converted to radians
    # This uses only 1 cos and 1 sqrt, much faster than Haversine
    lat1, lon1 = math.radians(coord1[0]), math.radians(coord1[1])
    lat2, lon2 = math.radians(coord2[0]), math.radians(coord2[1])
    
    x = (lon2 - lon1) * math.cos((lat1 + lat2) / 2)
    y = (lat2 - lat1)
    return 6371.0 * math.sqrt(x*x + y*y)