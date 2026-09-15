# AOIProject build setup

The project is a CMake superproject. `TaskControl` is the shared task/algorithm
library; `Shrimp` is the Qt GUI. Public dependencies are locked by
`conanfile.py`; no machine-local include or library directory is used.

## Toolchain contract

Use exactly one configuration for every binary that is linked together:

- Windows x64
- Visual Studio 2022, MSVC v143 (compiler version 194)
- C++20
- dynamic MSVC CRT: `/MD` for Release and `/MDd` for Debug

Conan is installed as `C:\Users\GACII\AppData\Roaming\Python\Python313\Scripts\conan.exe` on this machine. Add that Scripts directory to `PATH` before using the short `conan` command.

## Build TaskControl

```powershell
conan install . --lockfile=conan.lock --build=missing -pr:h=conan/profiles/windows-msvc-v143-x64 -pr:b=conan/profiles/windows-msvc-v143-x64 -s:h build_type=Debug -of=out/conan/debug
cmake --preset debug
cmake --build --preset build-debug

# RelWithDebInfo reuses the Release Conan packages.
cmake --preset relwithdebuginfo
cmake --build --preset build-relwithdebuginfo

conan install . --lockfile=conan.lock --build=missing -pr:h=conan/profiles/windows-msvc-v143-x64 -pr:b=conan/profiles/windows-msvc-v143-x64 -s:h build_type=Release -of=out/conan/release
cmake --preset release
cmake --build --preset build-release
```

The Debug and Release commands intentionally use separate Conan folders and
separate CMake configurations. Never build Debug from the Release preset or
the inverse.

## Dependency policy

The core is resolved from ConanCenter: Qt 6.8.3, OpenCV 4.10.0, PCL 1.14.1,
Eigen, fmt 11.1.3, spdlog and Taskflow. fmt is fixed to the version required by spdlog. PCL 1.15.1 was requested by
the legacy project but has no ConanCenter recipe; 1.14.1 is the newest
available ConanCenter recipe.

ConanCenter does not publish the required VTK configuration. `conan/recipes/vtk`
is therefore the private wrapper for the Conan-built VTK 9.5.0 package. It
keeps VTK in the same dependency graph and must be built with the toolchain
contract above. Export it after the VTK source archive is available:

```powershell
conan create conan/recipes/vtk --user=aoi --channel=stable -pr:h=conan/profiles/windows-msvc-v143-x64 -pr:b=conan/profiles/windows-msvc-v143-x64 -s:h build_type=Debug
```

Create matching Release packages with `-s:h build_type=Release`. Do not add
`D:\PublicLibraries`, vcpkg, a Qt kit, PCL, VTK, or OpenCV paths back into a
project property sheet.

## Build Shrimp

`Shrimp` uses the checked-in `TaskControl` API. Its ROI measurement recipe
injects an input point, filters the point cloud, fits a plane, measures the
distance, then aggregates the tolerance result. `AOI_BUILD_SHRIMP` remains OFF
by default because Shrimp needs the private VTK package.

`TaskControl` still exposes a C++ API. Cross-process or independently built
plug-ins should use `TaskControl_C.h` until its C++ boundary is redesigned.




