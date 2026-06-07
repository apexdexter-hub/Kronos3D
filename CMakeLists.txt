cmake_minimum_required(VERSION 3.22.1)
project(Kronos3DEngine)

# Configure compiler flags with optimization options for ARM64 v8a
add_compile_options(
    -O3
    -march=armv8-a
    -mtune=cortex-a55
    -ffast-math
    -funroll-loops
    -fvectorize
    -fomit-frame-pointer
    -fno-exceptions
    -fno-rtti
)
add_compile_definitions(
    USE_NEON=1
    KRONOS_BENCHMARK=1
)

# OpenGL ES 3.2 system dependency
find_library(log-lib log)
find_library(android-lib android)
find_library(gles3-lib GLESv3)

# Include directories
include_directories(
    .
    app/src/main/cpp
    third_party/glm
    third_party/imgui
    third_party/mobileglues/include
)

# Define native engine library target
add_library(kronos3d_engine SHARED
    app/src/main/cpp/native-lib.cpp
    engine/mesh/mesh.cpp
    engine/render/viewport.cpp
    engine/render/shader.cpp
    engine/render/overlay.cpp
)

# Link log, android, and openGL GLESv3 libraries
target_link_libraries(kronos3d_engine
    ${log-lib}
    ${android-lib}
    ${gles3-lib}
)
