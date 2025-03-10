set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
# TODO: Make sure the cross-compilers are installed and added to PATH
set(CMAKE_C_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++)
set(CMAKE_CXX_FLAGS -fPIC)
set(CMAKE_FIND_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# Set this to empty, otherwise cmake will not expand the executable
# path properly when invoking add_custom_command()
set(CMAKE_CROSSCOMPILING_EMULATOR "")
