# Black_Hole_Simulator_C
Black Hole Simulation
Black Hole Simulation

This project is a real-time black hole visualization written in C using OpenGL.

It uses a GPU compute shader to simulate light rays around a simplified Schwarzschild black hole and visualize gravitational lensing, an accretion disk, objects, and a warped space grid.

Technologies

The project uses C, OpenGL 4.3, GLFW, GLEW, and GLSL compute shaders.

Project Structure

The main C program is in src/blackhole.c and the GPU ray-tracing shader is in geodesic.comp.

How to Run

Install a C compiler such as MinGW GCC, along with GLFW, GLEW, and OpenGL development libraries.

Open a terminal in the project root folder.

Compile the program with the command: gcc src/blackhole.c -o blackhole.exe -lglfw3 -lglew32 -lopengl32 -lm

Run the program with the command: .\blackhole.exe

Keep geodesic.comp in the project root folder when running the program.

Controls

Use the left mouse button to rotate the camera.

Use the mouse wheel to zoom.

Use the right mouse button to apply gravitational interaction while it is held.

Press G to toggle the gravitational interaction.

About the Simulation

The simulation calculates a Schwarzschild radius from the black hole mass and uses numerical geodesic integration to approximate the path of light near the black hole.

The goal of this project is to provide a visual and interactive demonstration of black hole lensing and related effects rather than a research-grade physical simulation.



License

This project is released under the MIT License. See LICENSE for details.

Author

Arghyajit Kar
ECE Student | Digital Hardware, VLSI & Computer Architecture 
