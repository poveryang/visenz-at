cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

project(Barcode)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++17")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flax-vector-conversions")

set(SRCFILE test_barcode test_barcode_multi)

add_definitions(-DDEFINE_PROFILE_AS_EMTPY)

add_library(Barcode SHARED IMPORTED)
set_target_properties(Barcode PROPERTIES
  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/../lib/libBarcode.so"
)

add_library(imgproc_base SHARED IMPORTED)
set_target_properties(imgproc_base PROPERTIES
  IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/../lib/libimgproc_base.so"
)

find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})


foreach(target IN LISTS SRCFILE)

    if(BUILD_WITH_DL)
        if(INFERENCE_TYPE MATCHES "aarch64_nxp_tengine")
            message("System is Linux, on aarch64_nxp_tengine")
            link_directories(${PROJECT_SOURCE_DIR}/3rdparty/tengine_lib/aarch64_nxp/lib/)
        elseif(INFERENCE_TYPE MATCHES "x86_64_default_tengine")
            message("System is Linux, on x86_64_default_tengine")
            link_directories(${PROJECT_SOURCE_DIR}/3rdparty/tengine_lib/x86_64_default/lib)
        elseif(INFERENCE_TYPE MATCHES "aarch64_android")
            message("System is Android, on aarch64")
            link_directories(${PROJECT_SOURCE_DIR}/3rdparty/tengine_lib/aarch64_android/lib)
        elseif(INFERENCE_TYPE MATCHES "armv7_android")
            message("System is Android, on armv7")
            link_directories(${PROJECT_SOURCE_DIR}/3rdparty/tengine_lib/armv7_android/lib)
        elseif(INFERENCE_TYPE MATCHES "armv7_novaic")
            message("system is Linux, armv7_novaic")	          
            link_directories(${PROJECT_SOURCE_DIR}/3rdparty/novaic_lib/armv7-a/vs600_56x/lib/)
        else()
            message("System is Linux, on x86_64_default_tengine")
            link_directories(${PROJECT_SOURCE_DIR}/3rdparty/tengine_lib/x86_64_default/lib)
        endif()
    endif()

    add_executable(${target} "${target}.cpp")

    target_include_directories(
        ${target}
        PRIVATE
        ${PROJECT_SOURCE_DIR}/../include
        ${PROJECT_SOURCE_DIR}
    )

    target_link_libraries(
        ${target}
        PRIVATE 
        Barcode
        ${OpenCV_LIBS}
        imgproc_base
    )

    if(BUILD_WITH_DL)
        target_link_libraries(
            ${target}
            PRIVATE
            tengine-lite
        )
        if (INFERENCE_TYPE MATCHES "aarch64_nxp_tengine")
            target_link_libraries(${target} PRIVATE CLC)
            target_link_libraries(${target} PRIVATE GAL)
            target_link_libraries(${target} PRIVATE OpenVX)
            target_link_libraries(${target} PRIVATE OpenVXU)
            target_link_libraries(${target} PRIVATE VSC)
            target_link_libraries(${target} PRIVATE ArchModelSw)
            target_link_libraries(${target} PRIVATE NNArchPerf)
        endif()
    endif()

    install(
        TARGETS ${target}
        RUNTIME DESTINATION ./${TARGET_ARCHITECTURE}/bin
    )
endforeach()