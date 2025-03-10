if(${PLATFORM} MATCHES "vs600")
    set(Novaic_PLATFORM_DIR ${Novaic_DIR}/armv7-a/vs600_56x/)
elseif(${PLATFORM} MATCHES "vs800")
    set(Novaic_PLATFORM_DIR ${Novaic_DIR}/armv7-a/vs800_52x/)
else()
    set(Novaic_PLATFORM_DIR ${Novaic_DIR}/armv7-a/vs600_56x/)
endif()

message("Novaic_PLATFORM_DIR ${Novaic_PLATFORM_DIR}")

# find_path(Novaic_INCLUDE_DIR
#     NAMES network_api.h
#     PATHS ${Novaic_PLATFORM_DIR}/include/)

set(Novaic_INCLUDE_DIR ${Novaic_PLATFORM_DIR}/include)

file(GLOB  Novaic_LIBRARIES "${Novaic_PLATFORM_DIR}/lib/*.so")
file(GLOB  Novaic_Install_LIBRARIES "${Novaic_PLATFORM_DIR}/lib/libai_demo.so")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(Novaic DEFAULT_MSG
    Novaic_INCLUDE_DIR Novaic_LIBRARIES)

if(Novaic_FOUND)
    set(Novaic_INCLUDE_DIR ${Novaic_INCLUDE_DIR})
    set(Novaic_LIBRARIES ${Novaic_LIBRARIES})
    set(Novaic_Install_LIBRARIES ${Novaic_Install_LIBRARIES})
endif()

message("Novaic_INCLUDE_DIR: ${Novaic_INCLUDE_DIR}")
message("Novaic_LIBRARIES: ${Novaic_LIBRARIES}")
message("Novaic_Install_LIBRARIES: ${Novaic_Install_LIBRARIES}")