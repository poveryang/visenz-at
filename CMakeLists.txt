cmake_minimum_required(VERSION 3.10)

project(ViSenz-AT)
set(CMAKE_CXX_STANDARD 17)

# Set AT version
set(AT_VERSION_MAJOR 5)
set(AT_VERSION_MINOR 1)
set(AT_VERSION_PATCH 4)
configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h")
message(STATUS "AT version: ${AT_VERSION_MAJOR}.${AT_VERSION_MINOR}.${AT_VERSION_PATCH}")

# Build options(enable at test by default)
option(ENABLE_AT_TEST "Build AT UnitTest" ON)
option(ENABLE_SUB_TEST "Build Submodule UnitTest" OFF)
option(BUILD_WITH_LOG "Build With Log" ON)

# Build setting
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

# Load OpenCV
if (${DEVICE} MATCHES "vs1000p|vs2000")
    set(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
elseif (${DEVICE} MATCHES "vs600|vs800p")
     SET(OpenCV_DIR "/opt/libs_aarch32/opencv-4.5.2-contrib-linux-aarch32/lib/cmake/opencv4")
elseif (${DEVICE} MATCHES "vs2000p")
    set(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
endif ()
find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})

if (BUILD_WITH_LOG)
    add_definitions(-DBUILD_WITH_LOG)
endif()

# Load Barcode SDK
set(Barcode_ROOT "${CMAKE_SOURCE_DIR}/3rdparty/libBarcode")
set(Barcode_INC "${Barcode_ROOT}/${DEVICE}/include")
file(GLOB Barcode_LIBS "${Barcode_ROOT}/${DEVICE}/lib/*.*")

# Build AT submodules
add_subdirectory("./src/auto_exposure/")
add_subdirectory("./src/auto_focus/")

# Build AT library
file(GLOB AT_SRC "src/*.cpp")
add_library(AT SHARED ${AT_SRC})

target_include_directories(AT PUBLIC
    "./include" 
    "src/auto_exposure" 
    "src/auto_focus" 
    ${Barcode_INC}
    ${PROJECT_BINARY_DIR})

target_link_libraries(AT PUBLIC
    AE 
    AF 
    ${OpenCV_LIBS}
    ${Barcode_LIBS}
)

if(${DEVICE} MATCHES "vs600|vs800p")
    LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    include(fetch/Novaic)
    find_package(Novaic REQUIRED)
    install(FILES ${Novaic_Install_LIBRARIES}  DESTINATION ./${TARGET_ARCHITECTURE}/lib)

    target_include_directories(
        AT PUBLIC 
        ${Novaic_INCLUDE_DIR}
    )

    target_link_libraries(
        AT PUBLIC 
        ${Novaic_LIBRARIES}
    )
endif()

# Build AT test(optional)
if (${ENABLE_AT_TEST})
    add_subdirectory("./test/")
endif ()


# Install AT library
set(install_dir "${CMAKE_SOURCE_DIR}/release/${DEVICE}")
set(AT_PUBLIC_HEADER
        "${CMAKE_SOURCE_DIR}/include/at_interface.h"
        "${CMAKE_SOURCE_DIR}/include/at_base.h"
        "${CMAKE_SOURCE_DIR}/include/ar_base.h"
)

install(FILES ${AT_PUBLIC_HEADER}
        DESTINATION include)

install(TARGETS AT
        LIBRARY DESTINATION lib)
