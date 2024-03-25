if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(DnnModel_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/dnnmodel_${CMAKE_CONFIGURATION_TYPES})
else()
    set(DnnModel_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/dnnmodel)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64 bits
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(DnnModel_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/dnnmodel_linux_0214.zip")
        set(DnnModel_MD5 "db8720a08cd48fdf7def5968527d4bc0")
    elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
        if(CMAKE_CONFIGURATION_TYPES MATCHES "Release")
            set(DnnModel_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/dnnmodel/dnnmodel_windows_x64_static.zip")
            set(DnnModel_MD5 "767d73a184348ec9ec860ca69f15c8ed")
        elseif(CMAKE_CONFIGURATION_TYPES MATCHES "Debug")
            set(DnnModel_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/dnnmodel/dnnmodel_windows_x64_debug_static.zip")
            set(DnnModel_MD5 "9bac29c59ec8583c17b7d71f9c9f4364")
        endif()
    endif()
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4) #32bits
    if(INFERENCE_TYPE MATCHES "armv7_novaic")
        set(DnnModel_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/dnnmodel_linux_0919.zip")
        set(DnnModel_MD5 "3430f891f8520e2feada29ab433e2292")
    endif()
endif()


if(NOT DnnModel_URL)
    message(FATAL_ERROR "Could Not Found DnnModel_URL!")
endif()

include(FetchContent)

FetchContent_Declare(DnnModel
    URL ${DnnModel_URL}
    SOURCE_DIR ${DnnModel_LOCAL_PATH}
    URL_MD5 ${DnnModel_MD5}
)


message(STATUS "Fetch DnnModel")
FetchContent_MakeAvailable(DnnModel)
message(STATUS "Fetch DnnModel done")

FetchContent_GetProperties(DnnModel SOURCE_DIR DnnModel_DIR)

