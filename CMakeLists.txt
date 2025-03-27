cmake_minimum_required(VERSION 3.10)

project(AT4VS VERSION 5.1.4)
add_definitions(
        -DVERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        -DVERSION_MINOR=${PROJECT_VERSION_MINOR}
        -DVERSION_PATCH=${PROJECT_VERSION_PATCH}
)
message(STATUS "AT version: ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(CMAKE_CXX_STANDARD 17)

# Build options(enable at test by default)
option(ENABLE_AT_TEST "Build AT UnitTest" OFF)
option(ENABLE_SUB_TEST "Build Submodule UnitTest" OFF)
option(BUILD_WITH_LOG "Build With Log" ON)

# Build setting
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

if (BUILD_WITH_LOG)
    add_definitions(-DBUILD_WITH_LOG)
endif ()

# Set platform according to device(if not defined platform)
if (NOT DEFINED PLATFORM)
    # Set default device for Mac environment
    if (NOT DEFINED DEVICE)
        if (APPLE)
            set(DEVICE "vs1000p")
        else()
            message(FATAL_ERROR "Please specify the device name by -DDEVICE=xxx")
        endif()
    endif ()

    # Set platform according to device
    if (${DEVICE} MATCHES "vs1000p.*|vs2000")
        set(PLATFORM "imx8plus")
    elseif (${DEVICE} MATCHES "vs800")
        set(PLATFORM "imx8mini")
    elseif (${DEVICE} MATCHES "vs600|vs800p")
        set(PLATFORM "nova")
    elseif (${DEVICE} MATCHES "vs2000p")
        set(PLATFORM "rknn")
    endif ()
endif ()

# Set install path
message(STATUS "Platform: ${PLATFORM}")
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/release/AT4VS_v${PROJECT_VERSION}/${PLATFORM}")

# Set OpenCV and Barcode library path according to platform
if (APPLE)
    # Use system OpenCV on Mac
    set(OpenCV_DIR "/opt/homebrew/Cellar/opencv/4.11.0_1/lib/cmake/opencv4")
    set(Barcode_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libBarcode/Barcode_8plus_dl_0.38.4")
elseif (${PLATFORM} STREQUAL "imx8plus")  # NXP i.MX8M Plus
    set(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
    set(Barcode_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libBarcode/Barcode_8plus_dl_0.38.4")
elseif (${PLATFORM} STREQUAL "imx8mini") # NXP i.MX8M Mini
    set(OpenCV_DIR "/opt/libs_aarch64/mier_opencv/lib/cmake/opencv4")
    set(Barcode_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libBarcode/Barcode_mini8_0.38.4")
elseif (${PLATFORM} STREQUAL "nova")  # Novatek NT985xx
    set(OpenCV_DIR "/opt/libs_aarch32/opencv-4.5.2-contrib-linux-aarch32/lib/cmake/opencv4")
    set(Barcode_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libBarcode/Barcode_novaic_0.38.4")
elseif (${PLATFORM} STREQUAL "rknn") # NXP i.MX8M Plus with RKNN
    set(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
    set(Barcode_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libBarcode/Barcode_rknn_0.34.5_rknn")
endif ()

# Load OpenCV
find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})

# Load Barcode
set(Barcode_INC "${Barcode_DIR}/include")
file(GLOB Barcode_LIBS "${Barcode_ROOT}/${DEVICE}/lib/*.so")

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

# Build AT test(optional)
if (${ENABLE_AT_TEST})
    add_subdirectory("./test/")
endif ()

# Install AT library
set(AT_PUBLIC_HEADER
        "${CMAKE_SOURCE_DIR}/include/at_interface.h"
        "${CMAKE_SOURCE_DIR}/include/at_base.h"
        "${CMAKE_SOURCE_DIR}/include/ar_base.h"
)

install(FILES ${AT_PUBLIC_HEADER}
        DESTINATION include)

install(TARGETS AT
        LIBRARY DESTINATION lib)
