#turn csv data into a dictionary with info for A*

import osmnx as ox

#inputs to graph extraction

#set defaults
center_point = (42.0411, 87.6901)
radius = 3000
dist_type = 
net_type = 
retain_all = True
truncate_edge = True

#ask if they want to input their own data
user_input = input("Enter yes/no: ").strip().lower()
set_own = user_input in ("yes", "y", "true", "t")

if (set_own){
    #center point
    center_point_lat = float(input("center point latitude: \n"))
    center_point_long = float(input("center point longitude: \n"))
    center_point = (center_point_lat, center_point_long)

    #radius in meters
    radius = float(input("radius from center point in meters\n"))

    # not sure
    dist_type = input("distance type: \n")

    #what type of street networks to retrieve
    net_type = input("network types: \n")


    #retain entire graph or only connected network
    user_input = input("retain all? yes/no: ").strip().lower()
    retain_all = user_input in ("yes", "y", "true", "t")

    #get outside nodes if theyre neighbors with an inside node (if true)
    user_input = input("retain all? yes/no: ").strip().lower()
    truncate_edge = user_input in ("yes", "y", "true", "t")
}




ox.graph_from_point()