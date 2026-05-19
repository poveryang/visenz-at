cmake_minimum_required(VERSION 3.10)
project(AE)

set(SOURCE_FILES ae_interface.cpp ae_interface.h ae_impl.cpp ae_impl.h)

add_compile_options(-fPIC)
add_library(AE STATIC ${SOURCE_FILES})

target_link_libraries(AE PUBLIC ${OpenCV_LIBS})