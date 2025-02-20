cmake_minimum_required(VERSION 3.10)
project(test_at)

# External dependencies
file(GLOB CamCap_LIBS "${CMAKE_SOURCE_DIR}/3rdparty/cam_cap/${DEVICE}/lib/*.*")
set(CamCap_INC "${CMAKE_SOURCE_DIR}/3rdparty/cam_cap/${DEVICE}/inc")

# Executable definition
add_executable(test_at "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

target_link_libraries(test_at PUBLIC
    AT
    ${OpenCV_LIBS}
    ${CamCap_LIBS}
#    ${Barcode_LIBS}
)

target_include_directories(test_at PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CamCap_INC}
)

# Add a general macro for DEVICE
target_compile_definitions(test_at PUBLIC DEVICE_NAME="${DEVICE}")

# Installation
install(TARGETS test_at DESTINATION bin)
