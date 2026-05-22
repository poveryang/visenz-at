cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 发版时只改此处版本号；安装目录与库内版本 API 均由此派生
project(AT VERSION 5.2.0)

# 跨平台下限: imx8mini GCC 7.3 + CMake 3.10
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wno-psabi")

option(ENABLE_CORE_TEST "Build AT core unit tests" OFF)
option(ENABLE_AT_RUNNER "Build device-side AT runner skeleton" OFF)
option(HMAP_INSTALL_RUNTIME_LIBS "Install heatmap/Tengine runtime libraries into AT release" OFF)
set(HMAP_SDK_ROOT "" CACHE PATH "Optional heatmap-model/cpp root for AT Tengine heatmap provider")

if (NOT DEFINED PLATFORM)
    if (APPLE)
        set(PLATFORM "imx8plus")
    else()
        set(PLATFORM "imx8plus")
    endif()
endif()

# 默认安装到 release/AT_v<version>/<platform>/
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX
        "${CMAKE_SOURCE_DIR}/release/AT_v${PROJECT_VERSION}/${PLATFORM}"
        CACHE PATH "AT install root" FORCE)
endif()

message(STATUS "Platform: ${PLATFORM}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "AT library version: ${PROJECT_VERSION}")

if (APPLE)
    set(OpenCV_DIR "/opt/homebrew/Cellar/opencv/4.11.0_1/lib/cmake/opencv4")
elseif (PLATFORM STREQUAL "imx8mini")
    set(OpenCV_DIR "/opt/libs_aarch64/mier_opencv/lib/cmake/opencv4")
elseif (PLATFORM STREQUAL "nova")
    set(OpenCV_DIR "/opt/libs_aarch32/opencv-4.5.2-contrib-linux-aarch32/lib/cmake/opencv4")
elseif (PLATFORM STREQUAL "rknn" OR PLATFORM STREQUAL "imx8plus")
    set(OpenCV_DIR "/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4")
else()
    message(FATAL_ERROR "Unsupported PLATFORM=${PLATFORM}")
endif()

find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})

set(AT_SRC
        "src/core/at_session.cpp"
        "src/core/candidate_store.cpp"
        "src/core/vision/image_quality.cpp"
        "src/core/vision/roi_tracker.cpp"
        "src/orchestrator/at_orchestrator.cpp"
        "src/providers/null_providers.cpp"
        "src/modules/focus/focus_module.cpp"
        "src/modules/exposure/exposure_module.cpp"
        "src/modules/lighting/lighting_module.cpp")

add_library(AT STATIC ${AT_SRC})
set_target_properties(AT PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(AT PRIVATE AT_VERSION_STR=\"${PROJECT_VERSION}\")
target_include_directories(AT PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        PRIVATE
        "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(AT PUBLIC ${OpenCV_LIBS})

if (HMAP_SDK_ROOT)
  set(_hmap_root "${HMAP_SDK_ROOT}")
  if (NOT EXISTS "${_hmap_root}/include/hmap_generator.h")
    message(FATAL_ERROR "hmap SDK header not found under ${_hmap_root}")
  endif()
  if (EXISTS "${_hmap_root}/build/${PLATFORM}/libHMAP.a")
    set(_hmap_library "${_hmap_root}/build/${PLATFORM}/libHMAP.a")
  elseif (EXISTS "${_hmap_root}/build/imx8plus/libHMAP.a")
    set(_hmap_library "${_hmap_root}/build/imx8plus/libHMAP.a")
  else()
    message(FATAL_ERROR "libHMAP.a not found under ${_hmap_root}/build/${PLATFORM} or ${_hmap_root}/build/imx8plus")
  endif()

  set(_hmap_tengine_inc "${_hmap_root}/thirdparty/tengine/include")
  set(_hmap_tengine_lib_dir "${_hmap_root}/thirdparty/tengine/lib/aarch64")
  set(_hmap_tengine_core "${_hmap_tengine_lib_dir}/libtengine-lite.so")
  file(GLOB _hmap_tengine_libs "${_hmap_tengine_lib_dir}/*.so")
  if (NOT EXISTS "${_hmap_tengine_inc}/c_api.h")
    message(FATAL_ERROR "Tengine c_api.h not found under ${_hmap_tengine_inc}")
  endif()
  if (NOT EXISTS "${_hmap_tengine_core}")
    message(FATAL_ERROR "libtengine-lite.so not found under ${_hmap_tengine_lib_dir}")
  endif()
  if (NOT _hmap_tengine_libs)
    message(FATAL_ERROR "Tengine runtime libraries not found under ${_hmap_tengine_lib_dir}")
  endif()

  add_library(HMAP::hmap STATIC IMPORTED GLOBAL)
  set_target_properties(HMAP::hmap PROPERTIES
          IMPORTED_LOCATION "${_hmap_library}"
          INTERFACE_INCLUDE_DIRECTORIES "${_hmap_root}/include;${_hmap_tengine_inc}")

  add_library(AT_HMAP_TENGINE STATIC "${CMAKE_SOURCE_DIR}/src/providers/tengine_heatmap_provider.cpp")
  set_target_properties(AT_HMAP_TENGINE PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_include_directories(AT_HMAP_TENGINE PUBLIC
          "${CMAKE_SOURCE_DIR}/include"
          PRIVATE
          "${CMAKE_SOURCE_DIR}/src")
  target_link_libraries(AT_HMAP_TENGINE PUBLIC ${OpenCV_LIBS} HMAP::hmap ${_hmap_tengine_libs})

  target_link_libraries(AT PRIVATE AT_HMAP_TENGINE)
  target_compile_definitions(AT PRIVATE AT_WITH_HMAP_TENGINE=1)
  install(TARGETS AT_HMAP_TENGINE ARCHIVE DESTINATION lib)
  if (HMAP_INSTALL_RUNTIME_LIBS)
    install(FILES ${_hmap_tengine_libs} DESTINATION lib)
  endif()
  if (EXISTS "${_hmap_root}/artifacts/tmfile/model-uint8.tmfile")
    install(FILES "${_hmap_root}/artifacts/tmfile/model-uint8.tmfile" DESTINATION model)
  endif()
endif()

if (ENABLE_CORE_TEST)
    enable_testing()
    add_executable(at_core_unit_test "${CMAKE_SOURCE_DIR}/test/unit/at_core_unit_test.cpp")
    target_link_libraries(at_core_unit_test PRIVATE AT ${OpenCV_LIBS})
    add_test(NAME at_core_unit_test COMMAND at_core_unit_test)
endif()

if (ENABLE_AT_RUNNER)
    if (NOT SMORE_CAM_CAP_SDK_ROOT)
        message(FATAL_ERROR "ENABLE_AT_RUNNER requires -DSMORE_CAM_CAP_SDK_ROOT=<sdk>")
    endif()
  set(_camcap_root "${SMORE_CAM_CAP_SDK_ROOT}")
  if (NOT EXISTS "${_camcap_root}/include/camcap/camcap.h")
    message(FATAL_ERROR "camcap SDK not found under ${_camcap_root}")
  endif()

  add_executable(at_device_runner "${CMAKE_SOURCE_DIR}/apps/at_runner/main.cpp")
  target_include_directories(at_device_runner PRIVATE "${_camcap_root}/include")
  target_link_directories(at_device_runner PRIVATE "${_camcap_root}/lib")
  target_link_libraries(at_device_runner PRIVATE
          AT
          ${OpenCV_LIBS}
          camcap
          camcap_opencv_adapter
          pthread
          dl)
  install(TARGETS at_device_runner RUNTIME DESTINATION bin)
endif()

set(AT_PUBLIC_HEADERS
        "${CMAKE_SOURCE_DIR}/include/at_types.h"
        "${CMAKE_SOURCE_DIR}/include/at_trace.h"
        "${CMAKE_SOURCE_DIR}/include/at_session.h"
        "${CMAKE_SOURCE_DIR}/include/at_orchestrator.h"
        "${CMAKE_SOURCE_DIR}/include/at_providers.h"
        "${CMAKE_SOURCE_DIR}/include/at_heatmap_tengine.h")
install(FILES ${AT_PUBLIC_HEADERS} DESTINATION include)
install(TARGETS AT ARCHIVE DESTINATION lib)
