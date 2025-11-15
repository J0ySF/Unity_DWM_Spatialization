cmake_minimum_required(VERSION 3.10)
project(Unity_DWM_Spatialization VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# When adding compiler support, configure the compiler specific flags
# so that the shared library is aggressively optimized for the host target
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -march=native -mtune=native") # TODO: try -flto on non-MINGW targets
    if (MINGW)
        # Link statically on MinGW, otherwise Unity cannot load the dependencies
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -static -flto")
    endif ()
    set(DISABLE_WARNINGS_FLAG "-w")
    set(DISABLE_FAST_FP_MATH "-fno-fast-math")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ot /GL /fp:fast")
    set(DISABLE_WARNINGS_FLAG "/w")
    set(DISABLE_FAST_FP_MATH "/fp:precise")
else ()
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported yet!")
endif ()

# Generate configuration file
if (NOT DEFINED DWM_SAMPLE_RATE)
    set(DWM_SAMPLE_RATE 16000)
elseif (DWM_SAMPLE_RATE LESS_EQUAL 0)
    message(FATAL_ERROR "Invalid DWM sample rate ${DWM_SAMPLE_RATE} specified!")
endif ()
if (NOT DEFINED DWM_BUFFER_SIZE)
    set(DWM_BUFFER_SIZE 128)
elseif (DWM_BUFFER_SIZE LESS_EQUAL 0)
    message(FATAL_ERROR "Invalid DWM buffer size ${DWM_BUFFER_SIZE} specified!")
endif ()
if (NOT DEFINED DWM_MAX_SOURCE_COUNT)
    set(DWM_MAX_SOURCE_COUNT 16)
elseif (DWM_MAX_SOURCE_COUNT LESS_EQUAL 0)
    message(FATAL_ERROR "Invalid DWM source count ${DWM_MAX_SOURCE_COUNT} specified!")
endif ()
if (NOT DEFINED DWM_MESH_WIDTH)
    set(DWM_MESH_WIDTH 1.0)
elseif (DWM_MESH_WIDTH LESS_EQUAL 0)
    message(FATAL_ERROR "Invalid DWM mesh width ${DWM_MESH_WIDTH} specified!")
endif ()
if (NOT DEFINED DWM_MESH_HEIGHT)
    set(DWM_MESH_HEIGHT 1.0)
elseif (DWM_MESH_HEIGHT LESS_EQUAL 0)
    message(FATAL_ERROR "Invalid DWM mesh height ${DWM_MESH_HEIGHT} specified!")
endif ()
if (NOT DEFINED DWM_MESH_DEPTH)
    set(DWM_MESH_DEPTH 1.0)
elseif (DWM_MESH_DEPTH LESS_EQUAL 0)
    message(FATAL_ERROR "Invalid DWM mesh depth ${DWM_MESH_DEPTH} specified!")
endif ()
if (NOT DEFINED DWM_EARS_DISTANCE)
    set(DWM_EARS_DISTANCE 0.15)
elseif (DWM_EARS_DISTANCE LESS_EQUAL 0)
    message(FATAL_ERROR "Invalid DWM listener ears distance ${DWM_EARS_DISTANCE} specified!")
endif ()
configure_file(plugin_config.h.in ${CMAKE_BINARY_DIR}/plugin_config.h)

# Use Unity Native Audio Plugin sources
set(UNITY_NATIVE_AUDIO_PLUGIN_SOURCE_DIR third-party/NativeAudioPlugins/NativeCode)
# Copy the relevant Unity Native Audio Plugin sources to the build directory, to avoid wrong local directory includes
configure_file(${UNITY_NATIVE_AUDIO_PLUGIN_SOURCE_DIR}/AudioPluginUtil.cpp ${CMAKE_BINARY_DIR}/AudioPluginUtil.cpp COPYONLY)
configure_file(${UNITY_NATIVE_AUDIO_PLUGIN_SOURCE_DIR}/AudioPluginInterface.h ${CMAKE_BINARY_DIR}/AudioPluginInterface.h COPYONLY)
configure_file(${UNITY_NATIVE_AUDIO_PLUGIN_SOURCE_DIR}/AudioPluginUtil.h ${CMAKE_BINARY_DIR}/AudioPluginUtil.h COPYONLY)
# Disable warnings in Unity Native Audio Plugin sources
set(UNITY_NATIVE_AUDIO_PLUGIN_SOURCES
        ${CMAKE_BINARY_DIR}/AudioPluginUtil.cpp
        ${CMAKE_BINARY_DIR}/AudioPluginInterface.h
        ${CMAKE_BINARY_DIR}/AudioPluginUtil.h)
set_source_files_properties(${UNITY_NATIVE_AUDIO_PLUGIN_SOURCES} PROPERTIES COMPILE_FLAGS ${DISABLE_WARNINGS_FLAG})
# Disable /fp:fast on this file on MSVC, otherwise Unity fails to load the library (should not cause performance loss)
if (MSVC)
    set_source_files_properties(${CMAKE_BINARY_DIR}/AudioPluginUtil.cpp PROPERTIES COMPILE_FLAGS ${DISABLE_FAST_FP_MATH})
endif ()

# Compile the library
add_library(Unity_DWM_Spatializer SHARED plugin.cpp ${UNITY_NATIVE_AUDIO_PLUGIN_SOURCES})
target_include_directories(Unity_DWM_Spatializer PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

# Link libraries and configure the Plugins folder target prefix
add_subdirectory(third-party/Spatial_Audio_Framework)
target_link_libraries(Unity_DWM_Spatializer PUBLIC saf_example_binauraliser)
if (UNIX)
    # Suppose that UNIX -> Linux
    # TODO: improve target detection for other "non-Linux" UNIX targets
    target_link_libraries(Unity_DWM_Spatializer INTERFACE m)
    set(INSTALL_TARGET_PREFIX "Linux_x86_64")
elseif (WIN32)
    set(INSTALL_TARGET_PREFIX "Windows_x86_64")
else ()
    message(FATAL_ERROR "Detected target platform is not supported yet!")
endif ()

set(UNITY_ASSETS_PLUGIN_DIR "${PROJECT_SOURCE_DIR}/../Assets/Plugins")

# Install the built shared library to the Plugins folder in the Unity project
set_target_properties(Unity_DWM_Spatializer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${UNITY_ASSETS_PLUGIN_DIR}/${INSTALL_TARGET_PREFIX}")
set_target_properties(Unity_DWM_Spatializer PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${UNITY_ASSETS_PLUGIN_DIR}/${INSTALL_TARGET_PREFIX}")
# MSVC fix to avoid Debug or Release prefix in the output path
if (MSVC)
    set_target_properties(Unity_DWM_Spatializer PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${UNITY_ASSETS_PLUGIN_DIR}/${INSTALL_TARGET_PREFIX}")
    set_target_properties(Unity_DWM_Spatializer PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${UNITY_ASSETS_PLUGIN_DIR}/${INSTALL_TARGET_PREFIX}")
endif ()