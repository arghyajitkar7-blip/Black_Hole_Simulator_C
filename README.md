# Black_Hole_Simulator_C
Black Hole Simulation

A real-time black-hole visualization written in C using OpenGL 4.3, GLFW, GLEW, and a GPU compute shader.

The program visualizes light rays around a simplified Schwarzschild black hole and renders effects such as gravitational lensing, an accretion disk, scene objects, and a warped 3D grid. Ray integration is performed on the GPU using a Runge-Kutta stepper inside geodesic.comp.

Important: This is an educational/visual simulation, not a research-grade general-relativistic solver. The implementation uses a simplified numerical model and fixed simulation parameters for real-time rendering.

Features

Real-time black-hole visualization

GPU ray tracing with an OpenGL compute shader

Numerical integration of photon trajectories using RK4-style stepping

Simplified Schwarzschild black-hole model

Gravitational lensing around the black hole

Accretion-disk rendering

Additional scene objects that can be hit by traced rays

Optional Newtonian gravity interaction between scene objects

Interactive orbit camera and zoom

3D warped grid visualization around the black hole

Technology Stack

C

OpenGL 4.3+

GLSL 4.30 compute shader

GLFW - window creation and input handling

GLEW - OpenGL function loading

MinGW GCC / GCC - compilation

Project Structure

black-hole-simulation/
│
├── src/
│   └── blackhole.c
│
├── geodesic.comp
├── README.md
├── LICENSE
└── screenshots/
    └── black-hole.png

Why is geodesic.comp in the repository root?

The current C source loads the compute shader with:

CreateComputeProgram("geodesic.comp");

Therefore, geodesic.comp must be available in the current working directory when the program starts. The easiest way is to keep it in the repository root and run the executable from the repository root.

The other vertex/fragment shaders used for the fullscreen display and grid are embedded directly inside blackhole.c, so they do not need separate files.

Requirements

Before building, make sure you have:

A C compiler such as GCC/MinGW

GLFW

GLEW

An OpenGL 4.3+ capable GPU and driver

Windows, Linux, or another platform with working GLFW/OpenGL support

The instructions below use MSYS2 UCRT64 on Windows, which is a convenient way to obtain GCC, GLFW, and GLEW together.

Windows Setup (MSYS2 + UCRT64)

1. Install MSYS2

Download and install MSYS2 from:

https://www.msys2.org/

After installation, open the MSYS2 UCRT64 terminal.

2. Update MSYS2

Run:

pacman -Syu

If MSYS2 asks you to close and reopen the terminal, do that, then run the update command again if required.

3. Install GCC, GLFW, and GLEW

pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-glew mingw-w64-ucrt-x86_64-pkgconf

Check that GCC is available:

gcc --version

Check that pkg-config is available:

pkg-config --version

Clone the Repository

From the MSYS2 UCRT64 terminal:

git clone https://github.com/YOUR-USERNAME/YOUR-REPOSITORY.git
cd YOUR-REPOSITORY

Replace YOUR-USERNAME/YOUR-REPOSITORY with the actual GitHub repository path.

You should now have:

src/blackhole.c
geodesic.comp
README.md

Build

From the repository root, run:

gcc src/blackhole.c -O2 -std=c11 -Wall -Wextra -o blackhole.exe $(pkg-config --cflags --libs glfw3 glew) -lopengl32 -lm

If compilation succeeds, you should get:

blackhole.exe

in the repository root.

What the command does

src/blackhole.c - main C source file

-O2 - compiler optimization

-std=c11 - compile as C11

-Wall -Wextra - enable useful compiler warnings

pkg-config --cflags --libs glfw3 glew - adds GLFW/GLEW include and library flags

-lopengl32 - links the Windows OpenGL library

-lm - links the C math library

-o blackhole.exe - creates the executable named blackhole.exe

Run

Still from the repository root:

./blackhole.exe

You can also run it from a Windows Command Prompt after building:

blackhole.exe

Do not move blackhole.exe away from geodesic.comp unless you also change the shader path in blackhole.c.

Controls

Input

Action

Left Mouse Drag

Orbit around the black hole

Middle Mouse Drag

Orbit around the black hole

Mouse Wheel

Zoom in / out

Right Mouse Button

Enable gravity while held

G

Toggle gravity ON/OFF

When gravity is enabled, the program applies pairwise Newtonian gravitational interaction between the simulated scene objects and updates their positions over time.

How It Works

The project has two main parts: the C/OpenGL host program and the GPU compute shader.

1. CPU side (blackhole.c)

The C program:

Creates an OpenGL 4.3 core-profile window using GLFW

Initializes OpenGL functions with GLEW

Creates the black-hole and scene-object data

Updates the camera

Generates the warped grid

Uploads camera, disk, and object data to OpenGL uniform buffer objects

Dispatches the compute shader

Displays the generated image on a fullscreen quad

The window is created at 800 × 600 pixels, while the ray-tracing texture is generated at 200 × 150 and then displayed on the larger window.

2. GPU side (geodesic.comp)

geodesic.comp is an OpenGL compute shader using:

#version 430

Each compute invocation corresponds to a pixel in the ray-traced image.

For each pixel, the shader:

Builds a camera ray.

Converts the ray into spherical-coordinate state variables.

Numerically integrates the trajectory using a Runge-Kutta step.

Checks whether the ray reaches the black hole.

Checks whether it crosses the accretion disk.

Checks whether it intersects one of the scene objects.

Stops if the ray escapes far from the system.

Writes the resulting color into an OpenGL image texture.

The shader uses a fixed integration step and a fixed maximum of 60,000 steps per ray.

Black Hole Model

The C program calculates the Schwarzschild radius using:

Rs = 2GM / c²

where:

G is the gravitational constant

M is the black-hole mass

c is the speed of light

The current source initializes the black hole with a mass of approximately:

8.54 × 10^36 kg

The compute shader uses the corresponding Schwarzschild-radius value for its ray calculations.

Troubleshooting

Failed to open shader: geodesic.comp

This means the program cannot find the compute shader in its current working directory.

Make sure you are inside the repository root before running:

cd YOUR-REPOSITORY
./blackhole.exe

Also verify that this file exists:

geodesic.comp

GLFW init failed

GLFW was not installed correctly or the required runtime/library cannot be found.

Verify the package is installed in the MSYS2 UCRT64 environment:

pacman -Qs mingw-w64-ucrt-x86_64-glfw

GLEW initialization fails

Verify that GLEW is installed:

pacman -Qs mingw-w64-ucrt-x86_64-glew

Also make sure you are running the program from the same MSYS2 UCRT64 environment used to build it.

OpenGL / shader version error

The program requests an OpenGL 4.3 core profile, and the compute shader requires GLSL 4.30.

If your GPU/driver does not provide OpenGL 4.3 or newer, the program will not work correctly.

Update your graphics driver and verify the OpenGL version reported by the application.

The program runs slowly

The ray tracer performs a large number of numerical integration steps per pixel. The current shader uses up to 60,000 steps for every ray.

Performance therefore depends heavily on the GPU and driver.

The current renderer intentionally uses a relatively low 200 × 150 compute resolution and scales the result to the 800 × 600 window.

Building with an Existing GCC/MinGW Installation

If you already have GCC, GLFW, and GLEW installed and available to your compiler, you can use a command similar to:

gcc src/blackhole.c -O2 -std=c11 -Wall -Wextra -o blackhole.exe -lglfw3 -lglew32 -lopengl32 -lm

The exact library names and search paths can vary depending on how GLFW and GLEW were installed. If the linker reports errors such as cannot find -lglfw3 or cannot find -lglew32, use the MSYS2 setup above or provide the correct -I and -L paths for your installation.

Linux (Ubuntu/Debian)

Install the required development packages:

sudo apt update
sudo apt install build-essential libglfw3-dev libglew-dev libgl1-mesa-dev

Build:

gcc src/blackhole.c -O2 -std=c11 -Wall -Wextra -o blackhole $(pkg-config --cflags --libs glfw3 glew) -lGL -lm

Run from the repository root:

./blackhole

Your graphics driver must still provide OpenGL 4.3+.

Limitations

This project is primarily intended for learning, experimentation, and visualization.

The relativistic ray tracing is a simplified numerical implementation.

It is not intended as a validated scientific simulation.

Many simulation constants are fixed in the source/shader.

The ray-tracing resolution is intentionally low for real-time performance.

The optional object gravity system uses a simplified Newtonian interaction.

The application currently expects geodesic.comp in the working directory.

Possible Future Improvements

Add higher-quality adaptive ray integration

Add adjustable ray-tracing resolution

Add configurable black-hole mass and disk parameters

Improve accretion-disk shading

Add star-field/background rendering

Move shader paths and simulation parameters into configuration files

Add camera presets and reset controls

Add performance/FPS display

Provide prebuilt binaries for supported platforms

License

This project is released under the MIT License. See LICENSE for details.

Author

Arghyajit Kar
ECE Student | Digital Hardware, VLSI & Computer Architecture 
