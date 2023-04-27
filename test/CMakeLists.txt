CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(AT_TEST)

# Set device
INCLUDE(Barcode.cmake)
IF (${TARGET_PLATFORM} MATCHES "imx8mini")
    LOAD_BARCODE("/opt/libs_aarch64/Barcode_mini8_0.25.8")
ELSEIF (${TARGET_PLATFORM} MATCHES "imx8plus")
    SET(DEVICE VS1000Pro)
    LOAD_BARCODE("/opt/libs_aarch64/Barcode_8plus_dl_0.25.5")
ENDIF ()

IF (NOT ${ENABLE_SUB_TEST})
    ADD_SUBDIRECTORY("./driver_cap")
ENDIF ()

# Build AT_TEST
ADD_EXECUTABLE(AT_TEST "./main.cpp")

# Determine whether to load barcode and link it
IF (${TARGET_SERIES} MATCHES "VS")
    TARGET_INCLUDE_DIRECTORIES(AT_TEST PUBLIC
            ${Barcode_INCLUDE_DIRS})
    TARGET_LINK_LIBRARIES(AT_TEST PUBLIC
            AT
            DCAP
            ${OpenCV_LIBS}
            ${Barcode_LIBRARIES})
ELSEIF (${TARGET_SERIES} MATCHES "VN")
    TARGET_LINK_LIBRARIES(AT_TEST PUBLIC
            AT
            ${OpenCV_LIBS}
            DCAP
            )
ENDIF ()

# Install
INSTALL(TARGETS AT_TEST
        DESTINATION bin)