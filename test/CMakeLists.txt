CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(AT-TEST)

# Load barcode and driver sdk
SET(BARCODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/barcode")
SET(DRIVER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/driver_cap")
INCLUDE(${BARCODE_DIR}/barcode.cmake)
INCLUDE(${DRIVER_DIR}/driver.cmake)

IF (${TARGET_PLATFORM} STREQUAL "imx8mini")
    SET(barcode_sdk_dir "/opt/libs_aarch64/Barcode_mini8_0.25.8")
    SET(driver_sdk_dir "../3rdparty/libs_aarch64/drivers_aarch64/VS800")
ELSEIF (${TARGET_PLATFORM} STREQUAL "imx8plus")
    SET(barcode_sdk_dir "/opt/libs_aarch64/Barcode_plus8_0.25.8")
    SET(driver_sdk_dir "../3rdparty/libs_aarch64/drivers_aarch64/VS1000P")
ELSEIF (${TARGET_PLATFORM} STREQUAL "nvt-52x")
    SET(barcode_sdk_dir "${BARCODE_DIR}/barcode_sdk_vs800p")
    SET(driver_sdk_dir "${DRIVER_DIR}/aarch32")
ELSEIF (${TARGET_PLATFORM} STREQUAL "nvt-56x")
    SET(barcode_sdk_dir "${BARCODE_DIR}/barcode_sdk_vs600")
    SET(driver_sdk_dir "${DRIVER_DIR}/aarch32")
ENDIF ()

LOAD_BARCODE(${barcode_sdk_dir} ${TARGET_PLATFORM})
LOAD_DRIVER(${driver_sdk_dir} ${TARGET_PLATFORM})

# Build AT_TEST
ADD_EXECUTABLE(AT_TEST "test_at.cpp" ${CAP_SRC})

TARGET_INCLUDE_DIRECTORIES(AT_TEST PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${Barcode_INC}
        ${Driver_INC})

TARGET_LINK_LIBRARIES(AT_TEST PUBLIC
        AT
        ${Barcode_LIBS}
        ${Driver_LIBS}
        ${OpenCV_LIBS})

# Install
INSTALL(TARGETS AT_TEST
        DESTINATION bin)