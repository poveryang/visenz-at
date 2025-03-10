
if("${Barcode_DIR}" STREQUAL "")
    message(FATAL_ERROR "No Barcode directory found ,please set Barcode_DIR before find Barcode first")
endif()

set(TengineLite_PLATFORM_DIR ${TengineLite_DIR}/${PLATFORM_DIR})

find_path(Barcode_INCLUDE_DIR
    NAMES Barcode.h
    PATHS ${Barcode_DIR}/include/
    CMAKE_FIND_ROOT_PATH_BOTH)

find_library(Barcode_LIBRARY
    NAMES Barcode
    PATHS ${Barcode_DIR}/lib
    CMAKE_FIND_ROOT_PATH_BOTH)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(Barcode DEFAULT_MSG Barcode_INCLUDE_DIR Barcode_LIBRARY)

if(Barcode_FOUND)
    set(Barcode_INCLUDE_DIRS ${Barcode_INCLUDE_DIR})
    set(Barcode_LIBRARIES ${Barcode_LIBRARY})
endif()

message("Barcode_INCLUDE_DIRS: ${Barcode_INCLUDE_DIRS}")
message("Barcode_LIBRARIES: ${Barcode_LIBRARIES}")