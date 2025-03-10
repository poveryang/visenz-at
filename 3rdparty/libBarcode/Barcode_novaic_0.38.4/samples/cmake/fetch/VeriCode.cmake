set(VeriCode_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/vericode_lib)
set(VeriCode_URL "https://devops-generic.pkg.coding.smoa.cloud/x-barcode/package/vericode/vericode_lib.zip?version=v0_0410")
set(VeriCode_MD5 "40225f6f9b18ee60e6a51bad44f53e54")

message("vericode: " ${CMAKE_SYSTEM_NAME})

if(EXISTS ${VeriCode_LOCAL_PATH})
    set(VeriCode_DIR ${VeriCode_LOCAL_PATH})
else()
    include(FetchContent)

    FetchContent_Declare(VeriCode
        URL ${VeriCode_URL}
        URL_MD5 ${VeriCode_MD5}
        SOURCE_DIR ${VeriCode_LOCAL_PATH}
    )
    message(STATUS "Fetch VeriCode")
    FetchContent_MakeAvailable(VeriCode)
    message(STATUS "Fetch VeriCode done")

    FetchContent_GetProperties(VeriCode SOURCE_DIR VeriCode_DIR)

    # get VeriCodelib dir
    set(VeriCode_DIR ${VeriCode_DIR})

endif()




