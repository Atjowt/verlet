# Verlet Integration

![Alt text](/.github/preview.gif?raw=true "preview")

A multi-threaded particle simulation using verlet integration.

Tests go up to ~64k particles before dropping below 60 FPS on my Intel Core i5-7400 (4) @ 3.50 GHz.

Rendering is done using point sprites, which requires GPU support for the GL_ARB_POINT_SPRITE OpenGL extension. Most GPUs should have this, but if it fails to run or looks broken this may be why. For reference I used an NVIDIA GeForce GTX 1060 6GB GPU.

You can change the number of threads used by changing the "#define SUBDIVISIONS" macro. This represents the number of spatial subdivisions, alternating between vertical and horizontal splits. Since each thread is assigned its own split, the total thread count is `2^SUBDIVISIONS`.
e.g.
- 0 subdivision(s) = 1 thread(s)
- 1 subdivision(s) = 2 thread(s)
- 2 subdivision(s) = 4 thread(s)
- ...

Use this to choose an appropriate number based on your CPU cores.

You can of course also modify other parameters like `NUM_PARTICLES` or `INV_RADIUS`.

# Build and Run
Currently only supports POSIX compliant operating systems (so no Windows).
Running `make run` should build and run the program using clang.
