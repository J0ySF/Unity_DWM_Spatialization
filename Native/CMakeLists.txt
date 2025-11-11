cmake_minimum_required(VERSION 3.5)
project(Unity_DWM_Spatialization VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# When adding compiler support, configure the compiler specific flags
# so that the shared library is aggressively optimized for the host target
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -march=native -mtune=native") #  -flto
    set(COMPILER_DISABLE_FILE_WARNINGS_FLAG "-w")
else ()
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported yet!")
endif ()

# Generate configuration file
if (NOT DEFINED DWM_SAMPLE_RATE)
    set(DWM_SAMPLE_RATE 16000)
elseif (DWM_SAMPLE_RATE LESS_EQUAL 0)
    message(FATAL_ERROR "Invalid DWM sample rate specified!")
endif ()
if (NOT DEFINED DWM_BUFFER_SIZE)
    set(DWM_BUFFER_SIZE 128)
elseif (DWM_BUFFER_SIZE LESS_EQUAL 0)
    message(FATAL_ERROR "Invalid DWM buffer size specified!")
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
set_source_files_properties(${UNITY_NATIVE_AUDIO_PLUGIN_SOURCES} PROPERTIES COMPILE_FLAGS ${COMPILER_DISABLE_FILE_WARNINGS_FLAG})

# Compile the library
add_library(Unity_DWM_Spatializer SHARED plugin.cpp ${UNITY_NATIVE_AUDIO_PLUGIN_SOURCES})
target_include_directories(Unity_DWM_Spatializer PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

# Link libraries and configure the Plugins folder target prefix
IF (UNIX)
    # Suppose that UNIX -> Linux
    # TODO: improve target detection for other "non-Linux" UNIX targets
    target_link_libraries(Unity_DWM_Spatializer INTERFACE m)
    set(INSTALL_TARGET_PREFIX "Linux_x86_64")
else ()
    message(FATAL_ERROR "Detected target platform is not supported yet!")
endif ()

# Install the built shared library to the Plugins folder in the Unity project
set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/../Assets/Plugins)
install(TARGETS Unity_DWM_Spatializer LIBRARY DESTINATION ${INSTALL_TARGET_PREFIX})
