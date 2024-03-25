cmake_minimum_required(VERSION 3.10)
project(AF)

set(SOURCE_FILES af_interface.cpp af_impl.cpp sampler.cpp)

add_compile_options(-fPIC)
add_library(AF STATIC ${SOURCE_FILES})

target_link_libraries(AF PUBLIC ${OpenCV_LIBS})
