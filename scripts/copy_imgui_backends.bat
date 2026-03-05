@echo off
REM Copy ImGui backend files from Conan cache to project

echo Copying ImGui SDL3/OpenGL3 backend files...

set SRC_DIR=C:\Users\Alexa\.conan2\p\imgui0780de9dd4e6e\s\src\backends
set DEST_DIR=C:\Users\Alexa\Documents\repo\sudoku-cpp\src\imgui_backends

if not exist "%DEST_DIR%" mkdir "%DEST_DIR%"

copy "%SRC_DIR%\imgui_impl_sdl3.cpp" "%DEST_DIR%\" /Y
copy "%SRC_DIR%\imgui_impl_sdl3.h" "%DEST_DIR%\" /Y
copy "%SRC_DIR%\imgui_impl_opengl3.cpp" "%DEST_DIR%\" /Y
copy "%SRC_DIR%\imgui_impl_opengl3.h" "%DEST_DIR%\" /Y
copy "%SRC_DIR%\imgui_impl_opengl3_loader.h" "%DEST_DIR%\" /Y

echo Backend files copied successfully!
