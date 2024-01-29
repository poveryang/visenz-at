FUNCTION(load_barcode_aarch64 barcode_dir)
    # Load Barcode libs
    FILE(GLOB Barcode_LIBS "${barcode_dir}/lib/*.so*")
    IF (Barcode_LIBS)
        MESSAGE(STATUS "Barcode libs is founded.")
    ELSE ()
        MESSAGE(FATAL_ERROR "Barcode libs is not founded.")
    ENDIF ()

    # Load Tengine libs(optional)
    STRING(FIND "${Barcode_LIBS}" "${barcode_dir}/lib/libtengine-lite.so" TENGINE_INDEX)
    IF(TENGINE_INDEX GREATER -1)
        FILE(GLOB Tengine_LIBS "${barcode_dir}/tengine_lib/aarch64_nxp/lib/*.so")
        IF (Tengine_LIBS)
            MESSAGE(STATUS "Tengine libs is founded.")
            LIST(APPEND Barcode_LIBS ${Tengine_LIBS})
        ELSE ()
            MESSAGE(FATAL_ERROR "Tengine libs is not founded.")
        ENDIF ()
    ENDIF()

    SET(Barcode_LIBS ${Barcode_LIBS} PARENT_SCOPE)
    set(Barcode_INC ${barcode_dir}/include PARENT_SCOPE)
ENDFUNCTION()


FUNCTION(load_barcode_nvt56x barcode_dir)
    # Load Barcode libs
    FILE(GLOB Barcode_LIBS "${barcode_dir}/lib/*.so*")
    IF (Barcode_LIBS)
        MESSAGE(STATUS "Barcode libs is founded.")
    ELSE ()
        MESSAGE(FATAL_ERROR "Barcode libs is not founded.")
    ENDIF ()

    # Load dnn libs
    FILE(GLOB DNN_LIBS "${barcode_dir}/novaic_lib/armv7-a/vs600_56x/lib/*.so")
    IF (DNN_LIBS)
        MESSAGE(STATUS "Novaic DNN libs is founded.")
        LIST(APPEND Barcode_LIBS ${DNN_LIBS})
    ELSE ()
        MESSAGE(FATAL_ERROR "Novaic DNN libs is not founded.")
    ENDIF ()

    SET(Barcode_LIBS ${Barcode_LIBS} PARENT_SCOPE)
    set(Barcode_INC ${barcode_dir}/include PARENT_SCOPE)
ENDFUNCTION()


FUNCTION(load_barcode_nvt52x barcode_dir)
    # Load Barcode libs
    FILE(GLOB Barcode_LIBS "${barcode_dir}/lib/*.so*")
    IF (Barcode_LIBS)
        MESSAGE(STATUS "Barcode libs is founded.")
    ELSE ()
        MESSAGE(FATAL_ERROR "Barcode libs is not founded.")
    ENDIF ()

    # Load dnn libs
    FILE(GLOB DNN_LIBS "${barcode_dir}/novaic_lib/armv7-a/vs800_52x/lib/*.so")
    IF (DNN_LIBS)
        MESSAGE(STATUS "Novaic DNN libs is founded.")
        LIST(APPEND Barcode_LIBS ${DNN_LIBS})
    ELSE ()
        MESSAGE(FATAL_ERROR "Novaic DNN libs is not founded.")
    ENDIF ()

    SET(Barcode_LIBS ${Barcode_LIBS} PARENT_SCOPE)
    SET(Barcode_INC "${barcode_dir}/include" PARENT_SCOPE)

ENDFUNCTION()


FUNCTION(load_barcode barcode_dir platform)
    IF (${platform} STREQUAL "imx8mini")
        load_barcode_aarch64(${barcode_dir})
    ELSEIF (${platform} STREQUAL "imx8plus")
        load_barcode_aarch64(${barcode_dir})
    ELSEIF (${platform} STREQUAL "nvt-56x")
        load_barcode_nvt56x(${barcode_dir})
    ELSEIF (${platform} STREQUAL "nvt-52x")
        load_barcode_nvt52x(${barcode_dir})
    ELSE ()
        MESSAGE(FATAL_ERROR "Platform is not supported.")
    ENDIF ()

    SET(Barcode_LIBS ${Barcode_LIBS} PARENT_SCOPE)
    SET(Barcode_INC ${Barcode_INC} PARENT_SCOPE)
ENDFUNCTION()