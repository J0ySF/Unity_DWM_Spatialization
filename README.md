# Unity DWM Spatialization

Experimental spatialization implementation for Unity.

Developed and tested on Unity 6.0 LTS.

## Introduction

TODO: describe usage.

### Disclaimers

The algorithm employed is very resource hungry, with running time depending on both spatialized volume size and audio
sample rate.

To alleviate the heavy costs of the algorithm, the CMakeLists.txt file sets up compiler flags which optimize the native
code build for the target machine: for this reason, the plugins are not included in the repository and must be compiled
as explained in the following (before opening the Unity project).

### Supported platforms/versions

* Linux with GCC

### How to build

#### Requirements

* One of the supported compilers for your target platform (at least C++20 compatible)
* CMake (version >= 3.5)

#### Build instructions

##### IDE build

The easiest way to build is to open the build folder with any IDE which supports CMake projects (such as Visual Studio
or CLion, to then **build** and **install** the `Unity_DWM_Spatializer` target using the IDE's functionalities.

##### Manual build

TODO: describe procedure

## Assets attributions

The following third party assets are used:

* "02_Orchestral.mp3" by www.openairlib.net is licensed under CC BY 4.0