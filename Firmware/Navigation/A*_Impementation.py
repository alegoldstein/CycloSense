#actual A* implementation, output route in edges and nodes
import osmnx as ox
import heapq
from heuristic import haversine_heuristic



def astar(nodes, adjacency, start, end, G):
    #initialize dicts and lists
    open_list = []
    closed_list = set()
    f_scores = {}
    g_scores = {}
    came_from = {}


    #find starting and ending nodes
    start_lon = start['lon']
    start_lat = start['lat']     
    end_lon = end['lon']
    end_lat = end['lat']   
    cord2 = (end_lon,end_lat)
    start_node = ox.nearest_nodes(G, X=start_lon, Y=start_lat)
    end_node   = ox.nearest_nodes(G, X=end_lon,   Y=end_lat)

    #add start node to open list and set its f score to 0
    #use heap to keep smallest f always at front of open list
    f_scores[start_node] = 0
    g_scores[start_node] = 0
    heapq.heappush(open_list, (f_scores[start_node], start_node))
    

    #main loop

    #while open list isnt empty
    while open_list:

        #get node in open list with smallest f
        current_f, q = heapq.heappop(open_list)  #pop smallest f

        #check if popped q is end
        if q == end_node:
            return reconstruct_path(came_from, q)

        #get all neighbors from adjacency
        for node, cost, edge in adjacency[q]:
            if node in closed_list:
                continue
            
            #calculate g score
            g_scores[node] = g_scores[q] + edge

            #calculate h score
            cord1 = (G.nodes[node]['y'], G.nodes[node]['x'])
            h_score = haversine_heuristic(cord1, cord2)
            #get f score and add to list
            f_scores[node] = g_scores[node] + h_score

            #add neighbor node to open list sorted by f score
            heapq.heappush(open_list, (f_scores[node], node))

        #not add q to closed list, open list is ready for next iteration
        closed_list.add(q)

    # If we reach here, no path was found
    return None

def reconstruct_path(came_from, current):
    total_path = [current]
    while current in came_from:
        current = came_from[current]
        total_path.append(current)
    return total_path[::-1]

            



