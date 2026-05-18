cmake_minimum_required(VERSION 3.10)

# compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
option(ENABLE_CORE_TEST "Build AT core unit tests" OFF)
option(ENABLE_AT_DEVICE_RUNNER "Build device-side AT runner with smore-cam-cap SDK" OFF)
option(BUILD_WITH_LOG "Build With Log" ON)
set(SMORE_CAM_CAP_SDK_ROOT "" CACHE PATH "Installed smore-cam-cap SDK root")

# Build setting
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wno-psabi")

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
if (NOT "${PLATFORM}" STREQUAL "imx8plus")
    message(FATAL_ERROR "This refactor branch currently supports only imx8plus. Other platforms will be added later.")
endif ()
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
    # set(Barcode_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libBarcode/Barcode_rknn_0.34.5_rknn")
    set(Barcode_DIR "${CMAKE_SOURCE_DIR}/3rdparty/libBarcode/Barcode_rknn_0.39.15")
endif ()

# Load OpenCV
find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})

# Load Barcode
set(Barcode_INC "${Barcode_DIR}/include")
if (APPLE)
    # imx8plus Barcode ships Linux aarch64 artifacts only (.so / .a). The static
    # archive is not Mach-O and must not be linked on macOS host builds.
    set(Barcode_LIBS "")
else()
    file(GLOB Barcode_LIBS "${Barcode_DIR}/lib/*.so")
endif()

# Build new AT core. This target intentionally avoids Barcode linkage so unit
# tests can run on the host machine before device integration.
set(AT_CORE_SRC
        "src/core/session_controller.cpp"
        "src/core/candidate_pool.cpp"
        "src/vision/image_quality.cpp"
        "src/vision/roi_tracker.cpp")

add_library(AT_CORE STATIC ${AT_CORE_SRC})
set_target_properties(AT_CORE PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(AT_CORE PUBLIC
        "./include"
        ${PROJECT_BINARY_DIR}
        PRIVATE
        "./src")
target_link_libraries(AT_CORE PUBLIC ${OpenCV_LIBS})

if (ENABLE_AT_DEVICE_RUNNER)
    if (NOT SMORE_CAM_CAP_SDK_ROOT)
        message(FATAL_ERROR "ENABLE_AT_DEVICE_RUNNER requires -DSMORE_CAM_CAP_SDK_ROOT=/path/to/smore-cam-cap/release/<device>")
    endif ()

    set(SMORE_CAM_CAP_INCLUDE_DIR "${SMORE_CAM_CAP_SDK_ROOT}/include")
    set(SMORE_CAM_CAP_LIBRARY "${SMORE_CAM_CAP_SDK_ROOT}/lib/libcamcap.a")
    set(SMORE_CAM_CAP_OPENCV_ADAPTER_LIBRARY "${SMORE_CAM_CAP_SDK_ROOT}/lib/libcamcap_opencv_adapter.a")

    if (NOT EXISTS "${SMORE_CAM_CAP_INCLUDE_DIR}/camcap/camcap.h")
        message(FATAL_ERROR "smore-cam-cap header not found: ${SMORE_CAM_CAP_INCLUDE_DIR}/camcap/camcap.h")
    endif ()
    if (NOT EXISTS "${SMORE_CAM_CAP_LIBRARY}")
        message(FATAL_ERROR "smore-cam-cap library not found: ${SMORE_CAM_CAP_LIBRARY}")
    endif ()
    if (NOT EXISTS "${SMORE_CAM_CAP_OPENCV_ADAPTER_LIBRARY}")
        message(FATAL_ERROR "smore-cam-cap OpenCV adapter library not found: ${SMORE_CAM_CAP_OPENCV_ADAPTER_LIBRARY}")
    endif ()

    add_library(SMORE_CAM_CAP::camcap STATIC IMPORTED GLOBAL)
    set_target_properties(SMORE_CAM_CAP::camcap PROPERTIES
            IMPORTED_LOCATION "${SMORE_CAM_CAP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SMORE_CAM_CAP_INCLUDE_DIR}")

    add_library(SMORE_CAM_CAP::opencv_adapter STATIC IMPORTED GLOBAL)
    set_target_properties(SMORE_CAM_CAP::opencv_adapter PROPERTIES
            IMPORTED_LOCATION "${SMORE_CAM_CAP_OPENCV_ADAPTER_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SMORE_CAM_CAP_INCLUDE_DIR}")

    add_executable(at_device_runner
            "apps/at_device_runner/main.cpp")
    target_include_directories(at_device_runner PRIVATE
            "./include"
            "./src")
    target_link_libraries(at_device_runner PRIVATE
            AT_CORE
            SMORE_CAM_CAP::opencv_adapter
            SMORE_CAM_CAP::camcap
            ${OpenCV_LIBS})
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
        target_link_libraries(at_device_runner PRIVATE stdc++fs)
    endif ()
    install(TARGETS at_device_runner
            RUNTIME DESTINATION bin)
endif ()

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
        AT_CORE
        AE
        AF
        ${OpenCV_LIBS}
        ${Barcode_LIBS}
)

# Build AT test(optional)
if (ENABLE_AT_TEST OR ENABLE_SUB_TEST OR ENABLE_CORE_TEST)
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
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/at"
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")

install(TARGETS AT
        LIBRARY DESTINATION lib)
