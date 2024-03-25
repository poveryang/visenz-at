
set(Rknn_PLATFORM_DIR ${Rknn_DIR}/aarch64/)
message("Rknn_PLATFORM_DIR ${Rknn_PLATFORM_DIR}")

set(Rknn_INCLUDE_DIR ${Rknn_PLATFORM_DIR}/include)

file(GLOB  Rknn_LIBRARIES "${Rknn_PLATFORM_DIR}/lib/*.so")
file(GLOB  Rknn_Install_LIBRARIES "${Rknn_PLATFORM_DIR}/lib/*.so")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(Novaic DEFAULT_MSG
    Rknn_INCLUDE_DIR Rknn_LIBRARIES)

if(Rknn_FOUND)
    set(Rknn_INCLUDE_DIR ${Rknn_INCLUDE_DIR})
    set(Rknn_LIBRARIES ${Rknn_LIBRARIES})
    set(Rknn_Install_LIBRARIES ${Rknn_Install_LIBRARIES})
endif()

message("Rknn_INCLUDE_DIR: ${Rknn_INCLUDE_DIR}")
message("Rknn_LIBRARIES: ${Rknn_LIBRARIES}")
message("Rknn_Install_LIBRARIES: ${Rknn_Install_LIBRARIES}")