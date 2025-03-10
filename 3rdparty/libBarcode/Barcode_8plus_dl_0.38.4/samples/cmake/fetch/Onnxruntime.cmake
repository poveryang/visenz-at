set(OnnxRuntime_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/onnxruntime)

if(EXISTS ${OnnxRuntime_LOCAL_PATH})
    set(ONNXRUNTIME_DIR ${OnnxRuntime_LOCAL_PATH})
else()
    if(NOT DEFINED OnnxRuntime_URL)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64 bits
            if(CMAKE_SYSTEM_NAME MATCHES "Linux")
                set(OnnxRuntime_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/onnxruntime/onnxruntime-linux-x64-cpu-1.8.1.tgz")
                set(OnnxRuntime_MD5 "58e825b819173cb0b0a6d3669914ebd7")
            elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
                set(OnnxRuntime_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/onnxruntime/onnxruntime-windows-x64-cpu-1.8.1.zip")
                set(OnnxRuntime_MD5 "bf429c1778c53ec7304c414e157a8d67")
            endif()
        endif()
    endif()

    if(NOT OnnxRuntime_URL)
        message(FATAL_ERROR "Could Not Found OnnxRuntime_URL!")
    endif()

    include(FetchContent)

    FetchContent_Declare(OnnxRuntime
        URL ${OnnxRuntime_URL}
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/onnxruntime
        URL_MD5 ${OnnxRuntime_MD5}
    )
    message(STATUS "Fetch OnnxRuntime")
    FetchContent_MakeAvailable(OnnxRuntime)
    message(STATUS "Fetch OnnxRuntime done")

    # get OnnxRuntime_DIR
    FetchContent_GetProperties(OnnxRuntime SOURCE_DIR ONNXRUNTIME_DIR)
endif()