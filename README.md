### Branch information

Technique used: many points are sampled on the top side of the mesh, with attenuation based on distance from the listener

# Unity DWM Spatialization

Experimental spatialization implementation for Unity.

Developed and tested on Unity 6.0 LTS.

## Introduction

TODO: describe usage.

### Supported platforms/versions

* Linux with GCC
* Windows with MinGW64 GCC
* Windows with MSVC (not recommended, read below for more)

### Disclaimers

The algorithm employed is very resource hungry, with running time depending on both spatialized volume size and audio
sample rate.

To alleviate the heavy costs of the algorithm, the CMakeLists.txt file sets up compiler flags which optimize the native
code build for the target machine: for this reason, the plugins are not included in the repository and must be compiled
as explained in the following (before opening the Unity project).

#### Why MSVC is **not** recommended (for now)

For reasons that are not clearly understood at the moment, MSVC is not able to optimize the DWM implementation as much
as GCC, which in tests yields code that is *at least* twice as fast. For this reason is *highly encouraged* to compile
with MinGW64 GCC on Windows.

If someone is more knowledgeable of MSVC and can help improve MSVC performance, help is welcome!

### How to build

#### Requirements

* One of the supported compilers for your target platform (at least C++20 compatible)
* CMake (version >= 3.10)

#### Build instructions

##### IDE build

The easiest way to build is to open the build folder with any IDE which supports CMake projects (such as CLion or Visual
Studio, to then **build** the `Unity_DWM_Spatializer` target using the IDE's functionalities.

##### Manual build

TODO: describe procedure

#### Compile options

TODO: describe

## Assets attributions

The following third party assets are used:

* "02_Orchestral.mp3" by www.openairlib.net is licensed under CC BY 4.0