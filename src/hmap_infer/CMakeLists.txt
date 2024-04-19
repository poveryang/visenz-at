cmake_minimum_required(VERSION 3.10)
project(HmapInfer)

# Build options
option(BUILD_UNIT_TEST "Build unit test" ON)
option(BUILD_EVAL "Build eval to go through dataset" ON)

# Configure npu backend
 if (${DEVICE} MATCHES "vs1000p|vs2000")
     message(STATUS "Using tengine backend")
     add_definitions(-DUSE_TENGINE)
     set(NPU_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libtengine_api")
 elseif (${DEVICE} MATCHES "vs600|vs800p")
     message(STATUS "Using nvtai backend")
     add_definitions(-DUSE_NVTAI)
     # TODO: add nvtai backend
 elseif (${DEVICE} MATCHES "vs2000p")
     message(STATUS "Using rknn backend")
     add_definitions(-DUSE_RKNN)
     set(NPU_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/librknn_api")
 endif ()
file(GLOB NPU_LIBS ${NPU_API_DIR}/lib/*.so)

# Build hmap_infer library
add_compile_options(-fPIC)
file(GLOB SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(HmapInfer STATIC ${SRC_FILE})

target_link_libraries(HmapInfer PUBLIC
        ${NPU_LIBS}
        ${OpenCV_LIBS})

target_include_directories(HmapInfer PUBLIC
        ${NPU_API_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Build test_hmap_infer
if (BUILD_UNIT_TEST)
    add_executable(TEST-HMAP-INFER "${CMAKE_CURRENT_SOURCE_DIR}/test/test_hmap_infer.cpp")
    target_link_libraries(TEST-HMAP-INFER PUBLIC HmapInfer)
endif ()

if (BUILD_EVAL)
    add_executable(TEST-HMAP-EVAL ${CMAKE_CURRENT_SOURCE_DIR}/test/test_hmap_eval.cpp ${CMAKE_CURRENT_SOURCE_DIR}/test/json.hpp)
    target_link_libraries(TEST-HMAP-EVAL PUBLIC HmapInfer)
endif()