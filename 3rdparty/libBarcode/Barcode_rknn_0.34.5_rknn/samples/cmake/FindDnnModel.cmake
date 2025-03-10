find_path(DnnModel_INCLUDE_DIR
    NAMES dnnmodel.h
    PATHS ${DnnModel_DIR}/include
    CMAKE_FIND_ROOT_PATH_BOTH)

if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64 bits
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        if(INFERENCE_TYPE MATCHES "aarch64_nxp_tengine")
            find_library(DnnModel_LIBRARY
                        NAMES smdnnmodel
                        PATHS ${DnnModel_DIR}/lib/aarch64
                        CMAKE_FIND_ROOT_PATH_BOTH)
        elseif(INFERENCE_TYPE MATCHES "aarch64_nxp_hailo")
            find_library(DnnModel_LIBRARY
                NAMES smdnnmodel
                PATHS ${DnnModel_DIR}/lib/aarch64
                CMAKE_FIND_ROOT_PATH_BOTH)
        elseif(INFERENCE_TYPE MATCHES "aarch64_rknn")
            find_library(DnnModel_LIBRARY
                NAMES smdnnmodel
                PATHS ${DnnModel_DIR}/lib/aarch64
                CMAKE_FIND_ROOT_PATH_BOTH)
        else()
            find_library(DnnModel_LIBRARY
                NAMES smdnnmodel
                PATHS ${DnnModel_DIR}/lib/x86_64
                CMAKE_FIND_ROOT_PATH_BOTH)
        endif()
    elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
       find_library(DnnModel_LIBRARY
                    NAMES smdnnmodel
                    PATHS ${DnnModel_DIR}/lib)
    endif()
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4) #32bits
    if(INFERENCE_TYPE MATCHES "armv7_novaic")
        # find_library(DnnModel_LIBRARY
        #     NAMES smdnnmodel
        #     PATHS ${DnnModel_DIR}/lib/armv7-a
        #     CMAKE_FIND_ROOT_PATH_BOTH)
        file(GLOB  DnnModel_LIBRARY "${DnnModel_DIR}/lib/armv7-a/*.a")
        message(STATUS "Found DnnModel:  library: ${DnnModel_LIBRARY})")
    endif()
endif()


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DnnModel DEFAULT_MSG
    DnnModel_INCLUDE_DIR DnnModel_LIBRARY)

if(DnnModel_FOUND)
    set(DnnModel_INCLUDE_DIRS ${DnnModel_INCLUDE_DIR})
    message(STATUS "Found DnnModel: (include: ${DnnModel_INCLUDE_DIRS}, library: ${DnnModel_LIBRARY})")
endif()