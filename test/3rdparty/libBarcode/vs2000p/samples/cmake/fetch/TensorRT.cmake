set(TENSORRT_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/TensorRT-8)

if(EXISTS ${TENSORRT_LOCAL_PATH})
    set(TensorRT_DIR ${TENSORRT_LOCAL_PATH})
else()
    if(NOT DEFINED TensorRT_URL)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64 bits
            if(CMAKE_SYSTEM_NAME MATCHES "Linux")
                # message(FATAL_ERROR "Don't Support TensorRT!")
                set(TensorRT_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/TensorRT-8.2.4.2.Linux.x86_64-gnu.cuda-11.4.cudnn8.2.tar.gz?version=latest")
            elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
                set(TensorRT_URL "https://devops-generic.pkg.coding.smoa.cloud/artifacts/tensorrt/tensorrt-8.2.4.2-win10-x64-cuda11.4-cudnn8.2.zip?version=latest")
                set(TensorRT_MD5 "ac7252b92cb76084f875554a5f8f759a")
                endif()
        endif()
    endif()

    if(NOT TensorRT_URL)
        message(FATAL_ERROR "Could Not Found TensorRT_URL!")
    endif()

    include(FetchContent)


    FetchContent_Declare(TensorRT
        URL ${TensorRT_URL}
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/TensorRT-8
        URL_MD5 ${TensorRT_MD5}
    )


    message(STATUS "Fetch TensorRT")
    FetchContent_MakeAvailable(TensorRT)
    message(STATUS "Fetch TensorRT done")

    # get TENSORRT_DIR
    FetchContent_GetProperties(TensorRT SOURCE_DIR TensorRT_DIR)

endif()