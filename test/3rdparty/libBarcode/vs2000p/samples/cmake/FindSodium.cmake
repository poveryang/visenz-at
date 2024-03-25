if(WIN32)
    set(os windows)
elseif(APPLE)
    set(os mac)
else()
    set(os linux)
endif()
message("Sodium_DIR ${Sodium_DIR}")
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(architecture x64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(architecture x86)
    endif()

    set(Sodium_LIBRARY_DIR ${Sodium_DIR}/${os}/${architecture}/dynamic)
    set(Sodium_IMPORTED_LOCATION ${Sodium_LIBRARY_DIR}/libsodium.dll)
    find_library(Sodium_LIBRARY sodium NAMES libsodium PATHS ${Sodium_LIBRARY_DIR})

elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    if(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "armeabi-v7a")
        set(Sodium_INCLUDE_DIR ${Sodium_DIR}/include)
        set(Sodium_LIBRARY_DIR ${Sodium_DIR}/lib/armeabi-v7a)
    elseif(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a")
        set(Sodium_LIBRARY_DIR ${Sodium_DIR}/lib/arm64-v8a/)
        set(Sodium_IMPORTED_LOCATION ${Sodium_LIBRARY_DIR}/libsodium.a)
    else()
        message(FATAL_ERROR "Android Sodium only support ABI ")
    endif()
    set(Sodium_LIBRARY ${Sodium_LIBRARY_DIR}/libsodium.a)

else()
    set(architecture ${CMAKE_SYSTEM_PROCESSOR})
    set(Sodium_LIBRARY_DIR ${Sodium_DIR}/${os}/${architecture})
    if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)|(x86_64)")
        set(Sodium_IMPORTED_LOCATION ${Sodium_LIBRARY_DIR}/libsodium.so.23)
        find_library(Sodium_LIBRARY libsodium.so.23 HINTS ${Sodium_LIBRARY_DIR})
    else()
        set(Sodium_IMPORTED_LOCATION ${Sodium_LIBRARY_DIR}/libsodium.so)
        find_library(Sodium_LIBRARY sodium NAMES libsodium.so HINTS ${Sodium_LIBRARY_DIR})
    endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(Sodium_INCLUDE_DIR ${Sodium_DIR}/include)
else()
    find_path(Sodium_INCLUDE_DIR 
        NAMES sodium.h  
        HINTS ${Sodium_DIR}/include)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sodium DEFAULT_MSG Sodium_INCLUDE_DIR Sodium_LIBRARY)

if(Sodium_FOUND)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
        add_library(Sodium SHARED IMPORTED)
        set_target_properties(Sodium
            PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${Sodium_INCLUDE_DIR})
    else()
        add_library(Sodium SHARED IMPORTED)
        set_target_properties(Sodium
            PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${Sodium_INCLUDE_DIR}
            IMPORTED_LOCATION ${Sodium_IMPORTED_LOCATION}
            IMPORTED_IMPLIB ${Sodium_LIBRARY})
    endif()
endif()

message("Sodium_INCLUDE_DIR ${Sodium_INCLUDE_DIR}")
message("Sodium_LIBRARY ${Sodium_LIBRARY}")