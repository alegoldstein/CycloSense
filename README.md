# CycloSense
# EE 327 Spring 2026

This repository contains the files for the firmware, PCB design, and 3D models used to create our bike computer named CycloSense. CycloSense was created to be a bike computer for the everyday bike user, affordable, easy to use, and includes predictive maintenance alerts and offline navigation. 

Objective:
Create a bike computer to provide local navigation, predict maintenence, and display metrics


Steps Taken:
- Load nodes and edges from OpenStreetMap to perform A* algorithm on to find optimal route between two points. 
- Convert node and edge data to a 2D grid based on offset from the map origin to save memory and shorten computing time.
- Process audio from brake and classify as squeaky or normal with a ML model trained on Edge Impulse to alert user of necessary maintenance.
- Display speed, time, brake status, and directions overlayed on a map.
- Mount main system to handlebar and microphone + hall sensor to front fork with custom 3D printed parts.


Outcome:
A bike computer prototype that successfully tracks speed and provides navigation while showcasing predictive maintenance using edge AI.

<p align="center">
  <img src="Photos/PCB-Schematic.png" width="800">
  <br>
  <em>PCB Schematic</em>
</p>

<p align="center">
  <img src="Photos/PCB-Render.png" width="600">
  <br>
  <em>PCB Rendering</em>
</p>


<table align="center">
  <tr>
    <td align="center">
      <img src="Photos/Full-System.png" width="500"><br>
      <em>Full system mounted to handlebar</em>
    </td>
    <td align="center">
      <img src="Photos/3D-Printed-Enclosure.png" width="500"><br>
      <em>Model of 3D printed enclosure</em>
    </td>
  </tr>
</table>


