CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(ViSenz-AT)

# 1. Set sdk version and cmake options
SET(AT_VERSION_MAJOR 4)
SET(AT_VERSION_MINOR 0)
SET(AT_VERSION_PATCH 0)
CONFIGURE_FILE("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h")
OPTION(ENABLE_AT_TEST "Build AT UnitTest" ON)
OPTION(ENABLE_SUB_TEST "Build Submodule UnitTest" OFF)

# 2. Load dependent library
SET(TARGET_PLATFORM "imx8plus" CACHE STRING "Target platform")
IF (${TARGET_PLATFORM} MATCHES "imx8mini")
#    SET(Eigen3_DIR "/opt/libs_aarch64/eigen3/share/eigen3/cmake")
    SET(OpenCV_DIR "/opt/libs_aarch64/mier_opencv/lib/cmake/opencv4")
ELSEIF (${TARGET_PLATFORM} MATCHES "imx8plus")
#    SET(Eigen3_DIR "/opt/libs_aarch64/eigen3/share/eigen3/cmake")
    SET(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
ENDIF ()
#FIND_PACKAGE(Eigen3 REQUIRED PATHS ${Eigen3_DIR})
FIND_PACKAGE(OpenCV REQUIRED PATHS ${OpenCV_DIR})

# 2. Build AT submodules
ADD_SUBDIRECTORY(modules/auto_lights)
ADD_SUBDIRECTORY(modules/auto_exposure)
ADD_SUBDIRECTORY(modules/auto_focus)
IF (${TARGET_PLATFORM} MATCHES "imx8plus")  # Build heatmap generator on i.MX8Plus platform
    ADD_SUBDIRECTORY(modules/hmap_generator)
ENDIF ()

# 3. Build AT library
FILE(GLOB AT_SRC "src/*.cpp")
ADD_LIBRARY(AT SHARED ${AT_SRC})
TARGET_COMPILE_DEFINITIONS(AT PRIVATE AT4VS)

TARGET_INCLUDE_DIRECTORIES(AT PUBLIC "./include" ${PROJECT_BINARY_DIR})
TARGET_LINK_LIBRARIES(AT PUBLIC AL AF AE
        ${OpenCV_LIBS})
IF (${TARGET_PLATFORM} MATCHES "imx8plus")
    TARGET_LINK_LIBRARIES(AT PUBLIC HMAP)
ENDIF ()

# 4. Build AT test(optional)
IF (${ENABLE_AT_TEST})
    ADD_SUBDIRECTORY(test)
ENDIF ()

# 5. Install AT library
SET(AT_PUBLIC_HEADER
        "${CMAKE_SOURCE_DIR}/include/at_interface.h"
        "${CMAKE_SOURCE_DIR}/include/at_base.h"
        "${CMAKE_SOURCE_DIR}/include/ar_base.h"
        )
INSTALL(TARGETS AT
        LIBRARY DESTINATION lib)
INSTALL(FILES ${AT_PUBLIC_HEADER}
        DESTINATION include)
