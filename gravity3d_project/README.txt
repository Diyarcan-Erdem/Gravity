Gravity 3D - Free Camera OpenGL (Windows only)

What changed:
- full 3D scene
- free camera with mouse look
- WASD + Space + Ctrl movement
- smaller sun
- rebuilt gravity well with smoother shape and normals
- OpenGL rendering instead of old 2D GDI drawing
- trails, orbit guides, Saturn ring, stars

Controls:
- Mouse = rotate camera
- W A S D = move
- Space / Ctrl = up / down
- Shift = move faster
- M = lock/unlock mouse
- P = pause simulation
- R = relativity on/off
- + / - = simulation speed
- Esc = exit

Build:
1. Make sure g++ works in terminal
2. Double click compile.bat

Manual build:
g++ Gravity3DWinOpenGL.cpp -O2 -std=c++17 -o Gravity3D.exe -lopengl32 -lglu32 -lgdi32 -lwinmm
