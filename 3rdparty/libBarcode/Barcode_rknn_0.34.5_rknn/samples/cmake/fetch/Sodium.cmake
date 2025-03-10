set(Sodium_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/libsodium)
message("Sodium: " ${CMAKE_SYSTEM_NAME})
if(EXISTS ${Sodium_LOCAL_PATH})
    set(Sodium_DIR ${Sodium_LOCAL_PATH})
else()
    if(NOT DEFINED Sodium_URL)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
        set(Sodium_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/android/libsodium-android.zip?version=latest")
        set(Sodium_MD5 "c6b331d80df27a7eadd082fbef3b39b6")
    else()
        set(Sodium_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/libsodium-20220104.zip")
        set(Sodium_MD5 "162add7064159de9528cf4db21e0ef4b")
    endif()
endif()

    include(FetchContent)

    FetchContent_Declare(Sodium
        URL ${Sodium_URL}
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/libsodium
        URL_MD5 ${Sodium_MD5}
    )

    message(STATUS "Fetch Sodium")
    FetchContent_MakeAvailable(Sodium)
    message(STATUS "Fetch Sodium done")

    # get Sodium_DIR
    FetchContent_GetProperties(Sodium SOURCE_DIR Sodium_DIR)
endif()