# Build the driver from source code
FUNCTION(load_driver_aarch64 driver_dir)
    SET(Driver_INC ${driver_dir} PARENT_SCOPE)

    FILE(GLOB Driver_SRC ${driver_dir}/*.c*)
    SET(Driver_SRC ${Driver_SRC} PARENT_SCOPE)

    # Set Driver_LIBS to empty to avoid error when compiling
    SET(Driver_LIBS "" PARENT_SCOPE)
ENDFUNCTION()

# Use the pre-built driver library
FUNCTION(load_driver_aarch32 driver_dir)
    SET(Driver_INC "${driver_dir}/inc" PARENT_SCOPE)

    FILE(GLOB Driver_LIBS ${driver_dir}/lib/*.so)
    SET(Driver_LIBS ${Driver_LIBS} PARENT_SCOPE)

    # Set Driver_LIBS to empty to avoid error when compiling
    SET(Driver_SRC "" PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(load_driver driver_dir platform)
    IF(${platform} MATCHES "^imx.*")
        load_driver_aarch64(${driver_dir})
        SET(CAP_SRC "src/cap_aarch64.cpp" PARENT_SCOPE)
        ADD_DEFINITIONS(-DAARCH64)
    ELSEIF(${platform} MATCHES "^nvt.*")
        load_driver_aarch32(${driver_dir})
        SET(CAP_SRC "src/cap_aarch32.cpp" PARENT_SCOPE)
        ADD_DEFINITIONS(-DAARCH32)
    ELSE()
        MESSAGE(FATAL_ERROR "Unsupported platform: ${platform}")
    ENDIF()

    SET(Driver_SRC ${Driver_SRC} PARENT_SCOPE)
    SET(Driver_LIBS ${Driver_LIBS} PARENT_SCOPE)
ENDFUNCTION()