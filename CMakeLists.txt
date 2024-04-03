cmake_minimum_required(VERSION 3.10)

# SET(CMAKE_C_COMPILER /home/lsa/project/barcode-sdk/3rdparty/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc)
# SET(CMAKE_CXX_COMPILER /home/lsa/project/barcode-sdk/3rdparty/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++)

project(ViSenz-AT)

# Set AT version
set(AT_VERSION_MAJOR 4)
set(AT_VERSION_MINOR 0)
set(AT_VERSION_PATCH 0)
configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h")
message(STATUS "AT version: ${AT_VERSION_MAJOR}.${AT_VERSION_MINOR}.${AT_VERSION_PATCH}")

# Build options
option(ENABLE_AT_TEST "Build AT UnitTest" OFF)
option(ENABLE_SUB_TEST "Build Submodule UnitTest" OFF)

# Build setting
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

# Load dependent library
if (${DEVICE} MATCHES "vs1000p")
    message(STATUS "Using tengine backend")
    add_definitions(-DUSE_TENGINE)
    set(NPU_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libtengine_api")
    set(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
    # set(OpenCV_DIR "/home/lsa/project/barcode-sdk/3rdparty/opencv_aarch64/opencv_aarch64/lib/cmake/opencv4")
elseif (${DEVICE} MATCHES "vs600|vs800p")
    message(STATUS "Using nvtai backend")
    add_definitions(-DUSE_NVTAI)
    # TODO: add nvtai backend
    SET(OpenCV_DIR "/opt/libs_aarch32/opencv-4.5.2-contrib-linux-aarch32/lib/cmake/opencv4")
elseif (${DEVICE} MATCHES "vs2000p")
    message(STATUS "Using rknn backend")
    add_definitions(-DUSE_RKNN)
    set(NPU_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/librknn_api")
    set(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
endif ()

find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})

# Build AT submodules
add_subdirectory("./src/auto_exposure/")
add_subdirectory("./src/auto_focus/")
add_subdirectory("./src/hmap_infer/")

# Build AT library
file(GLOB AT_SRC "src/*.cpp")
add_library(AT SHARED ${AT_SRC})
#target_compile_definitions(AT PRIVATE AT4VS)

target_include_directories(AT PUBLIC "./include" "src/auto_exposure" "src/auto_focus" "src/infer_hmap" ${PROJECT_BINARY_DIR})
target_link_libraries(AT PUBLIC AE AF HmapInfer ${OpenCV_LIBS})
#target_link_libraries(AT PUBLIC AE AF ${OpenCV_LIBS})

# Build AT test(optional)
if (${ENABLE_AT_TEST})
    add_subdirectory("./test/")
endif ()

# Install AT library
SET(AT_PUBLIC_HEADER
        "${CMAKE_SOURCE_DIR}/include/at_interface.h"
        "${CMAKE_SOURCE_DIR}/include/at_base.h"
        "${CMAKE_SOURCE_DIR}/include/ar_base.h"
        )
INSTALL(TARGETS AT
        LIBRARY DESTINATION lib)
INSTALL(FILES ${AT_PUBLIC_HEADER}
        DESTINATION include)
