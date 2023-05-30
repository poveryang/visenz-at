CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(AT-TEST)

# Set device
SET(BAR_WRAP_DIR "3rdparty/barcode")
INCLUDE(${BAR_WRAP_DIR}/barcode.cmake)
IF (${TARGET_PLATFORM} MATCHES "imx8mini")
    # Load barcode library
    LOAD_BARCODE("/opt/libs_aarch64/Barcode_mini8_0.25.8")
ELSEIF (${TARGET_PLATFORM} MATCHES "imx8plus")
    # Load barcode library
    LOAD_BARCODE("/opt/libs_aarch64/Barcode_8plus_dl_0.25.16")
ENDIF ()

# Set cap library
SET(CAP_INC_DIR "3rdparty/driver_cap/VS1000P/inc")
SET(CAP_LIB_DIR "3rdparty/driver_cap/VS1000P/lib")
FILE(GLOB CAP_LIB "${CAP_LIB_DIR}/*.*")

# Build AT_TEST
ADD_EXECUTABLE(AT_TEST "test_at.cpp")


TARGET_INCLUDE_DIRECTORIES(AT_TEST PUBLIC
        ${CAP_INC_DIR}
        ${BAR_WRAP_DIR}
        ${Barcode_INCLUDE_DIRS})

TARGET_LINK_LIBRARIES(AT_TEST PUBLIC
        AT
        ${CAP_LIB}
        ${OpenCV_LIBS}
        ${Barcode_LIBRARIES})

# Install
INSTALL(TARGETS AT_TEST
        DESTINATION bin)