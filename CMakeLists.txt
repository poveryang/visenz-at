cmake_minimum_required (VERSION 3.10)
project(ViSenz_AT)

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

option(BUILD_AT_TEST "Build AT UnitTest" ON)
option(SUBMODULE_TEST "Build AT Submodule's UnitTest" OFF)

# 1. Load dependent library
if (${TARGET_PLATFORM} MATCHES "imx8mini")
    # Eigen3
    set(Eigen3_DIR "/opt/libs_aarch64/eigen3/share/eigen3/cmake")
    find_package(Eigen3 REQUIRED PATHS ${Eigen3_DIR})
    # OpenCV
    set(OpenCV_DIR "/opt/libs_aarch64/mier_opencv/lib/cmake/opencv4")
    find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})
elseif (${TARGET_PLATFORM} MATCHES "imx8plus")
    # Eigen3
    set(Eigen3_DIR "/opt/libs_aarch64/eigen3/share/eigen3/cmake")
    find_package(Eigen3 REQUIRED PATHS ${Eigen3_DIR})
    # OpenCV
    set(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
    find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})
endif()

# 2. Build AT submodule
add_subdirectory(modules/auto_lights)
add_subdirectory(modules/auto_exposure)
add_subdirectory(modules/auto_focus)

# 3. Build AT library
file(GLOB AT_SRC_LIST "src/*.cpp")
add_library(AT SHARED ${AT_SRC_LIST})
target_include_directories(AT PUBLIC "./include")
target_link_libraries(AT PUBLIC AL AF AE ${OpenCV_LIBS})

set(AT_PUBLIC_HEADER
        "${CMAKE_SOURCE_DIR}/include/at_interface.h"
        "${CMAKE_SOURCE_DIR}/include/at_params.h"
        )

install(TARGETS AT
        LIBRARY DESTINATION lib)

install(FILES ${AT_PUBLIC_HEADER}
        DESTINATION include)

if (${BUILD_AT_TEST})
    add_subdirectory(test)
    install(TARGETS AT_TEST
            DESTINATION bin)
endif()

