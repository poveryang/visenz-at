cmake_minimum_required (VERSION 3.10)
project(AT_TEST)

if (${TARGET_PLATFORM} MATCHES "imx8mini")
    set(DEVICE VS800)
    # Barcode
    include(modules/auto_recognition/Barcode.cmake)
    load_barcode("/opt/libs_aarch64/Barcode_mini8_0.24.0")
elseif (${TARGET_PLATFORM} MATCHES "imx8plus")
    # Barcode
    include(modules/auto_recognition/Barcode.cmake)
    load_barcode("/opt/libs_aarch64/Barcode_8plus_dl_0.24.0")
    set(DEVICE VS1000Pro)
endif ()

set(INC_DIR "./drivers/${DEVICE}/" "./")
file(GLOB AT_TEST_SRC "./*.cpp")
file(GLOB VCAP_SRC "./drivers/${DEVICE}/*.c*")

add_executable(${PROJECT_NAME} ${AT_TEST_SRC} ${VCAP_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC ${INC_DIR} ${Barcode_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC AT ${OpenCV_LIBS} ${Barcode_LIBRARIES})