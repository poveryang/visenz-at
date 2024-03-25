cmake_minimum_required(VERSION 3.10)
project(TEST_AT)

# Load Barcode SDK
set(Barcode_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libBarcode")
set(Barcode_INC "${Barcode_ROOT}/${DEVICE}/include")
file(GLOB Barcode_LIBS "${Barcode_ROOT}/${DEVICE}/lib/*.*")
message(STATUS "Barcode_LIBS: ${Barcode_LIBS}")

# Load Smvc SDK
if (${DEVICE} MATCHES "vs2000p")
    add_definitions(-DUSE_SMVC)
    set(Smvc_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libSmvc")
    set(Smvc_INC "${Smvc_ROOT}/${DEVICE}/inc")
    file(GLOB Smvc_LIBS "${Smvc_ROOT}/${DEVICE}/lib/*.so")
    set(CAP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/cap_smvc.cpp")
endif()

# Build AT_TEST
add_executable(TEST_AT "${CMAKE_CURRENT_SOURCE_DIR}/test_at.cpp" ${CAP_SRC})

target_include_directories(TEST_AT PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${Barcode_INC}
        ${Smvc_INC})

target_link_libraries(TEST_AT PUBLIC
        AT
        ${OpenCV_LIBS}
        ${Barcode_LIBS}
        ${Smvc_LIBS})

# Install
INSTALL(TARGETS TEST_AT
        DESTINATION bin)