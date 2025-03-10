find_path(TensorRT_INCLUDE_DIR 
    NAMES NvInfer.h
    PATHS ${TensorRT_DIR}/include)

find_library(TensorRT_LIBRARY_INFER
    NAMES nvinfer
    PATHS ${TensorRT_DIR}/lib)

find_library(TensorRT_LIBRARY_ONNXPARSER
    NAMES nvonnxparser
    PATHS ${TensorRT_DIR}/lib)

find_library(TensorRT_LIBRARY_NVINFER_PLUGIN
    NAMES nvinfer_plugin
    PATHS ${TensorRT_DIR}/lib)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(TensorRT DEFAULT_MSG
    TensorRT_INCLUDE_DIR 
    TensorRT_LIBRARY_INFER TensorRT_LIBRARY_ONNXPARSER TensorRT_LIBRARY_NVINFER_PLUGIN)

if(TensorRT_FOUND)
    set(TensorRT_INCLUDE_DIRS ${TensorRT_INCLUDE_DIR})
    set(TensorRT_LIBRARIES ${TensorRT_LIBRARY_INFER} ${TensorRT_LIBRARY_ONNXPARSER} ${TensorRT_LIBRARY_NVINFER_PLUGIN})
    message(STATUS "Found TensorRT: (include: ${TensorRT_INCLUDE_DIRS}, library: ${TensorRT_LIBRARIES})")

    if(WIN32)
        set(TensorRT_IMPORTED_LOCATION_INFER ${TensorRT_DIR}/lib/nvinfer.dll)
        set(TensorRT_IMPORTED_LOCATION_ONNXPARSER ${TensorRT_DIR}/lib/nvonnxparser.dll)
        set(TensorRT_IMPORTED_LOCATION_PLUGIN ${TensorRT_DIR}/lib/nvinfer_plugin.dll)
    else()
        set(TensorRT_IMPORTED_LOCATION_INFER ${TensorRT_LIBRARY_INFER})
        set(TensorRT_IMPORTED_LOCATION_ONNXPARSER ${TensorRT_LIBRARY_ONNXPARSER})
        set(TensorRT_IMPORTED_LOCATION_PLUGIN ${TensorRT_LIBRARY_NVINFER_PLUGIN})
    endif()

    if(NOT TARGET TensorRT::Infer)
        add_library(TensorRT::Infer SHARED IMPORTED)
        set_target_properties(TensorRT::Infer
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${TensorRT_INCLUDE_DIRS}
            IMPORTED_LOCATION ${TensorRT_IMPORTED_LOCATION_INFER}
            IMPORTED_IMPLIB ${TensorRT_LIBRARY_INFER}
        )
    endif()

    if(NOT TARGET TensorRT::ONNXParser)
        add_library(TensorRT::ONNXParser SHARED IMPORTED)
        set_target_properties(TensorRT::ONNXParser
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${TensorRT_INCLUDE_DIRS}
            IMPORTED_LOCATION ${TensorRT_IMPORTED_LOCATION_ONNXPARSER}
            IMPORTED_IMPLIB ${TensorRT_LIBRARY_ONNXPARSER}
        )
    endif()

    if(NOT TARGET TensorRT::Plugin)
        add_library(TensorRT::Plugin SHARED IMPORTED)
        set_target_properties(TensorRT::Plugin
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${TensorRT_INCLUDE_DIRS}
            IMPORTED_LOCATION ${TensorRT_IMPORTED_LOCATION_PLUGIN}
            IMPORTED_IMPLIB ${TensorRT_LIBRARY_NVINFER_PLUGIN}
        )
    endif()
endif()