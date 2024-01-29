CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(ViSenz-AT)

# Set AT version
SET(AT_VERSION_MAJOR 4)
SET(AT_VERSION_MINOR 0)
SET(AT_VERSION_PATCH 0)
CONFIGURE_FILE("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h")
MESSAGE(STATUS "AT version: ${AT_VERSION_MAJOR}.${AT_VERSION_MINOR}.${AT_VERSION_PATCH}")

# Set build options
OPTION(ENABLE_AT_TEST "Build AT UnitTest" ON)
OPTION(ENABLE_SUB_TEST "Build Submodule UnitTest" OFF)

# Set target platform
IF (NOT DEFINED TARGET_PLATFORM)
    SET(TARGET_PLATFORM "nvt-52x" CACHE STRING "Target platform")
ELSE ()
    MESSAGE(STATUS "Target platform: ${TARGET_PLATFORM}")
ENDIF ()


# Load dependent library
IF (${TARGET_PLATFORM} STREQUAL "imx8mini")
    SET(OpenCV_DIR "/opt/libs_aarch64/mier_opencv/lib/cmake/opencv4")
ELSEIF (${TARGET_PLATFORM} STREQUAL "imx8plus")
    ADD_DEFINITIONS(-DTENGINE_INFER)
    SET(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
ELSEIF (${TARGET_PLATFORM} MATCHES "^nvt.*")
    ADD_DEFINITIONS(-DNVTAI_INFER)
    SET(OpenCV_DIR "/opt/libs_aarch32/opencv-4.5.2-contrib-linux-aarch32/lib/cmake/opencv4")
ENDIF ()

FIND_PACKAGE(OpenCV REQUIRED PATHS ${OpenCV_DIR})

# Build AT submodules
ADD_SUBDIRECTORY(modules/auto_lights)
ADD_SUBDIRECTORY(modules/auto_exposure)
ADD_SUBDIRECTORY(modules/auto_focus)
ADD_SUBDIRECTORY(modules/hmap_generator)

# Build AT library
FILE(GLOB AT_SRC "src/*.cpp")
ADD_LIBRARY(AT SHARED ${AT_SRC})
TARGET_COMPILE_DEFINITIONS(AT PRIVATE AT4VS)

TARGET_INCLUDE_DIRECTORIES(AT PUBLIC "./include" ${PROJECT_BINARY_DIR})
TARGET_LINK_LIBRARIES(AT PUBLIC AL AF AE
        ${OpenCV_LIBS})
TARGET_LINK_LIBRARIES(AT PUBLIC HMAP)

# Build AT test(optional)
IF (${ENABLE_AT_TEST})
    ADD_SUBDIRECTORY(test)
ENDIF ()

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
