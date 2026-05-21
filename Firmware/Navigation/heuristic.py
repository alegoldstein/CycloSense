# heuristic.py
# distance heuristics for A*

import math

def haversine_heuristic(coord1, coord2):
    """
    Haversine distance between two (lat, lon) points in meters.
    Admissible heuristic for geographic A* — never overestimates.
    """
    lat1, lon1 = coord1
    lat2, lon2 = coord2

    R = 6371000  # earth radius in meters

    phi1   = math.radians(lat1)
    phi2   = math.radians(lat2)
    dphi   = math.radians(lat2 - lat1)
    dlambda = math.radians(lon2 - lon1)

    a = math.sin(dphi / 2)**2 + \
        math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2)**2

    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))


def fast_heuristic(coord1, coord2):
    """
    Equirectangular approximation — faster than haversine, accurate enough
    for distances under ~50km. Uses 1 cos and 1 sqrt vs haversine's 2+2.
    """
    lat1, lon1 = math.radians(coord1[0]), math.radians(coord1[1])
    lat2, lon2 = math.radians(coord2[0]), math.radians(coord2[1])

    x = (lon2 - lon1) * math.cos((lat1 + lat2) / 2)
    y = (lat2 - lat1)
    return 6371000 * math.sqrt(x * x + y * y)