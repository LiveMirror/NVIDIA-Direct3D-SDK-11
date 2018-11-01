N-Body Simulation with DirectCompute Shaders
--------------------------------------------

This program performs a parallel simulation of an "N-body" system of stars.  That is, it simulates the evolution over time of the positions and velocities of a number of massive bodies, such as stars, that all influence each other via gravitational attraction.  Because all bodies interact with all others, there are O(N^2) interactions to compute at each time step (each frame).  This is a lot of computation, but it is highly parallel and is thus amenable to GPU implementation using DirectCompute shaders.

The simulation utilizes DirectCompute groupshared memory to cache body positions and share them among all threads of a thread group.  This greatly reduces off-chip bandwidth use.
