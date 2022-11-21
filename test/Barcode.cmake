set(Barcode_INCLUDE_DIRS "" CACHE STRING INTERNAL)
set(Barcode_LIBRARIES "" CACHE STRING INTERNAL)

function(load_tengine tengine_local_path)
    set(Tengine_URL "http://artifactory.smoa.cloud/artifactory/SMScanner/nxp_tengine_inference/tengine_lib.tar.gz")
    if(NOT Tengine_URL)
        message(FATAL_ERROR "Could Not Found Tenginee_URL!")
    endif()
    include(FetchContent)
    FetchContent_Declare(Tengine
        URL ${Tengine_URL}
        SOURCE_DIR ${tengine_local_path}
    )
    message(STATUS "Fetch Tengine from ${Tengine_URL}.")
    FetchContent_MakeAvailable(Tengine)
endfunction()

function(load_barcode barcode_dir)
    if(NOT EXISTS ${barcode_dir})
        message(FATAL_ERROR  "can not load barcode sdk in ${barcode_dir}")
    endif()

    file(GLOB Barcode_LIBRARY_FILES "${barcode_dir}/lib/*.so*")
    string(FIND "${Barcode_LIBRARY_FILES}" "tengine-lite" POS)
#    message("Barcode_LIBRARY_FILES is ${Barcode_LIBRARY_FILES}")

    set(${Barcode_DEP_LIBRARY_FILES} "")
    if((0 LESS ${POS}))
        set(TENGINE_LOCAL_PATH  ${barcode_dir}/tengine_lib)
        if(NOT EXISTS ${barcode_dir}/tengine_lib)
            load_tengine(${TENGINE_LOCAL_PATH})     
        endif()
        file(GLOB  Barcode_DEP_LIBRARY_FILES "${TENGINE_LOCAL_PATH}/aarch64_nxp/lib/*.so")
    endif()
    set(Barcode_LIBRARIES  ${Barcode_LIBRARY_FILES} ${Barcode_DEP_LIBRARY_FILES} PARENT_SCOPE)
    set(Barcode_INCLUDE_DIRS ${barcode_dir}/include PARENT_SCOPE)
endfunction()
