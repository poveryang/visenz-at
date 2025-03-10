set(Rknn_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/rknn_lib/)
set(Rknn_URL "https://devops-generic.pkg.coding.smoa.cloud/x-barcode/package/rknn/rknn_lib.zip?version=v0_0304")
set(Rknn_MD5 "0a5a9d06f25296cd901c7ae7da24db09")

message("rknn: " ${CMAKE_SYSTEM_NAME})

if(EXISTS ${Rknn_LOCAL_PATH})
    set(Rknn_DIR ${Rknn_LOCAL_PATH})
else()
    include(FetchContent)

    FetchContent_Declare(Rknn
        URL ${Rknn_URL}
        URL_MD5 ${Rknn_MD5}
        SOURCE_DIR ${Rknn_LOCAL_PATH}
    )
    message(STATUS "Fetch Rknn")
    FetchContent_MakeAvailable(Rknn)
    message(STATUS "Fetch Rknn done")

    FetchContent_GetProperties(Rknn SOURCE_DIR Rknn_DIR)

    # get Rknnlib dir
    set(Rknn_DIR ${Rknn_DIR})

endif()




