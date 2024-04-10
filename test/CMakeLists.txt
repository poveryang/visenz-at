cmake_minimum_required(VERSION 3.10)
project(TEST_AT)

# Load Barcode SDK
set(Barcode_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libBarcode")
set(Barcode_INC "${Barcode_ROOT}/${DEVICE}/include")
file(GLOB Barcode_LIBS "${Barcode_ROOT}/${DEVICE}/lib/*.*")

# Load Smvc(prebuilt-lib) or V4L2(source-code)
if (${DEVICE} MATCHES "vs1000p")
    add_definitions(-DUSE_V4L2)
    set(V4L2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/v4l2Src/${DEVICE}")
    file(GLOB V4L2_SRC "${V4L2_ROOT}/*.c*")
    set(CAP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/cap_v4l2.cpp")
elseif (${DEVICE} MATCHES "vs2000p")
    add_definitions(-DUSE_SMVC)
    set(Smvc_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libSmvc")
    set(Smvc_INC "${Smvc_ROOT}/${DEVICE}/inc")
    file(GLOB Smvc_LIBS "${Smvc_ROOT}/${DEVICE}/lib/*.so")
    set(CAP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/cap_smvc.cpp")
endif()

# Build AT_TEST
add_executable(TEST_AT
        ${V4L2_SRC}
        ${CAP_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_at.cpp
)

target_link_libraries(TEST_AT PUBLIC
        AT
        ${OpenCV_LIBS}
        ${Barcode_LIBS}
        ${Smvc_LIBS}
)

target_include_directories(TEST_AT PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${Barcode_INC}
        ${Smvc_INC}
        ${V4L2_ROOT}
        ${OpenCV_INCLUDE_DIRS}
)



# Install
INSTALL(TARGETS TEST_AT
        DESTINATION bin
)