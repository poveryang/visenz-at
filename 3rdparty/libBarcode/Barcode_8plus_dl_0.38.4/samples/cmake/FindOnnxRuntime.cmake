find_path(ONNXRUNTIME_INCLUDE_DIR 
    NAMES onnxruntime_cxx_api.h
    PATHS ${ONNXRUNTIME_DIR}/include)

find_library(ONNXRUNTIME_LIBRARY
    NAMES libonnxruntime.so
    PATHS ${ONNXRUNTIME_DIR}/lib)

find_library(ONNXRUNTIME_LIBRARY_STUB
    NAMES libonnxruntime.so.1.8.1
    PATHS ${ONNXRUNTIME_DIR}/lib)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(OnnxRuntime DEFAULT_MSG
    ONNXRUNTIME_INCLUDE_DIR ONNXRUNTIME_LIBRARY)

if(OnnxRuntime_FOUND)
  set(ONNXRUNTIME_INCLUDE_DIRS ${ONNXRUNTIME_INCLUDE_DIR})
  set(ONNXRUNTIME_LIBRARIES ${ONNXRUNTIME_LIBRARY} ${ONNXRUNTIME_LIBRARY_STUB})
  message(STATUS "Found OnnxRuntime: (include: ${ONNXRUNTIME_INCLUDE_DIRS}, library: ${ONNXRUNTIME_LIBRARIES})")
endif()
