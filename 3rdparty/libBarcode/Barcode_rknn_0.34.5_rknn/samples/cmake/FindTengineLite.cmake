
set(TengineLite_LINUX_NXP8P "aarch64_nxp")
set(TengineLite_LINUX_X86_64 "x86_64_default")
message("CMAKE_SYSTEM_PROCESSOR " ${CMAKE_SYSTEM_PROCESSOR})
if(NOT "${TengineLite_DIR}" STREQUAL "")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64 bits
        if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
                set(PLATFORM_DIR ${TengineLite_LINUX_NXP8P})
            elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
                set(PLATFORM_DIR ${TengineLite_LINUX_X86_64})
            else()
                message(FATAL_ERROR "Tengine linux only support 8pus and x86")
            endif()
        elseif()
            message(FATAL_ERROR "Tengine only support linux os")
        endif()
    endif()
else()
    message(FATAL_ERROR "No tengine directory,please fetch first")
endif()

set(TengineLite_PLATFORM_DIR ${TengineLite_DIR}/${PLATFORM_DIR})
# message("TengineLite_PLATFORM_DIR ${TengineLite_PLATFORM_DIR}")

find_path(TengineLite_INCLUDE_DIR
    NAMES c_api.h
    PATHS ${TengineLite_PLATFORM_DIR}/include/tengine
    CMAKE_FIND_ROOT_PATH_BOTH)

file(GLOB  TengineLite_LIBRARY "${TengineLite_PLATFORM_DIR}/lib/*.so")
file(GLOB  TengineLite_Install_LIBRARY "${TengineLite_PLATFORM_DIR}/lib/libtengine-lite.so")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(TengineLite DEFAULT_MSG
    TengineLite_INCLUDE_DIR TengineLite_LIBRARY)

if(TengineLite_FOUND)
    set(TengineLite_INCLUDE_DIRS ${TengineLite_INCLUDE_DIR})
    set(TengineLite_LIBRARIES ${TengineLite_LIBRARY})
    set(TengineLite_Install_LIBRARIES ${TengineLite_Install_LIBRARY})
endif()

message("TengineLite_INCLUDE_DIRS: ${TengineLite_INCLUDE_DIRS}")
message("TengineLite_LIBRARIES: ${TengineLite_LIBRARIES}")
message("TengineLite_Install_LIBRARIES: ${TengineLite_Install_LIBRARIES}")