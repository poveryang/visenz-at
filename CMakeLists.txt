cmake_minimum_required (VERSION 3.10)
project(ViSenz_AT)

if (${TARGET_PLATFORM} MATCHES "imx8mini")
    set(TARGET_ARCHITECTURE "arm64-v8a")
    # Eigen3
    set(Eigen3_DIR "/opt/libs_aarch64/eigen3/share/eigen3/cmake")
    find_package(Eigen3 REQUIRED PATHS ${Eigen3_DIR})
    # OpenCV
    set(OpenCV_DIR "/opt/libs_aarch64/mier_opencv/lib/cmake/opencv4")
    find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})
    # Barcode
    include(modules/ViSenzProd_AR/Barcode.cmake)
    load_barcode("/opt/libs_aarch64/Barcode_mini8_0.21.0")
elseif (${TARGET_PLATFORM} MATCHES "imx8plus")
    set(TARGET_ARCHITECTURE "arm64-v8a")
    # Eigen3
    set(Eigen3_DIR "/opt/libs_aarch64/eigen3/share/eigen3/cmake")
    find_package(Eigen3 REQUIRED PATHS ${Eigen3_DIR})
    # OpenCV
    set(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
    find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})
    # Barcode
    include(modules/auto_recognition/Barcode.cmake)
    load_barcode("/opt/libs_aarch64/Barcode_8plus_dl_0.23.0_rc2")
else()
    find_package(OpenCV REQUIRED)
    find_package(Eigen3 REQUIRED)
endif()

option(MODULE_TEST OFF)
add_subdirectory(modules/auto_lights)
add_subdirectory(modules/auto_exposure)
add_subdirectory(modules/auto_focus)
add_subdirectory(modules/auto_recognition)

file(GLOB AT_SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB AT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(AT SHARED ${AT_SRC_LIST})
target_include_directories(AT PUBLIC ${AT_INCLUDE_DIRS})
target_link_libraries(AT PUBLIC AL AF AE AR)
target_link_libraries(AT PRIVATE ${OpenCV_LIBS} Eigen3::Eigen)

add_subdirectory(test)