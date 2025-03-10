
message("VeriCode_DIR ${VeriCode_DIR}")
set(VeriCode_INCLUDE_DIR ${VeriCode_DIR}/include)

file(GLOB  VeriCode_LIBRARIES "${VeriCode_DIR}/lib/*.a")
file(GLOB  VeriCode_Install_LIBRARIES "${VeriCode_DIR}/lib/vcread.a")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(VeriCode DEFAULT_MSG
    VeriCode_INCLUDE_DIR VeriCode_LIBRARIES)

if(VeriCode_FOUND)
    set(VeriCode_INCLUDE_DIR ${VeriCode_INCLUDE_DIR})
    set(VeriCode_LIBRARIES ${VeriCode_LIBRARIES})
    set(VeriCode_Install_LIBRARIES ${VeriCode_Install_LIBRARIES})
endif()

message("VeriCode_INCLUDE_DIR: ${VeriCode_INCLUDE_DIR}")
message("VeriCode_LIBRARIES: ${VeriCode_LIBRARIES}")
message("VeriCode_Install_LIBRARIES: ${VeriCode_Install_LIBRARIES}")