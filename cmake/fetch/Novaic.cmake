set(Novaic_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/novaic_lib/)
set(Novaic_URL "https://devops-generic.pkg.coding.smoa.cloud/x-barcode/package/novaic/novaic_lib.zip?version=v4_0409")
set(Novaic_MD5 "4233c5b00a27398c8a19ccfd17074c3e")

message("novaic: " ${CMAKE_SYSTEM_NAME})

if(EXISTS ${Novaic_LOCAL_PATH})
    set(Novaic_DIR ${Novaic_LOCAL_PATH})
else()
    include(FetchContent)

    FetchContent_Declare(Novaic
        URL ${Novaic_URL}
        URL_MD5 ${Novaic_MD5}
        SOURCE_DIR ${Novaic_LOCAL_PATH}
    )
    message(STATUS "Fetch Novaic")
    FetchContent_MakeAvailable(Novaic)
    message(STATUS "Fetch Novaic done")

    FetchContent_GetProperties(Novaic SOURCE_DIR Novaic_DIR)

    # get Novaiclib dir
    set(Novaic_DIR ${Novaic_DIR})

endif()




