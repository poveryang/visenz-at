set(Tengine_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/tengine_lib)
set(TengineLite_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/tengine/tengine_v1.5_nvdia_cachesave.tar.gz")
set(TengineLite_MD5 "52adeb4cc12440239fb23ac17d57b7f9")

if(EXISTS ${Tengine_LOCAL_PATH})
    set(TengineLite_DIR ${Tengine_LOCAL_PATH})
else()
include(FetchContent)

FetchContent_Declare(TengineLite
    URL ${TengineLite_URL}
    URL_MD5 ${TengineLite_MD5}
    SOURCE_DIR ${Tengine_LOCAL_PATH}
)

FetchContent_MakeAvailable(TengineLite)

FetchContent_GetProperties(TengineLite SOURCE_DIR TengineLite_DIR)

# get TengineLite_DIR
set(TengineLite_DIR ${TengineLite_DIR})

endif()


