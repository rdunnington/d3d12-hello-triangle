# d3d12-hello-triangle
Simple, linear C99 code that initializes a Win32 window, D3D12, and draws a triangle.

## Build and run with build.bat
Open a x64 Visual Studio command prompt and enter these commands:
```
cd d3d12-hello-triangle
build.bat
demo.exe
```

## Build and run with cmake
This assumes cmake is setup to generate Visual Studio projects by default.
```
cd d3d12-hello-triangle
mkdir cmake_build
cd cmake_build
cmake.exe ..\.
cmake.exe --build .
bin\Debug\demo.exe
```
