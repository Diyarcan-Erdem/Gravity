@echo off
setlocal

echo Compiling Gravity 3D OpenGL project...
g++ Gravity3DWinOpenGL.cpp -O2 -std=c++17 -o Gravity3D.exe -lopengl32 -lglu32 -lgdi32 -lwinmm
if errorlevel 1 (
    echo.
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo Build complete. Running...
start "" Gravity3D.exe
