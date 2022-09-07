cmake_minimum_required (VERSION 3.10)
project(AT_TEST)

set(DEVICE VS1000Pro)
set(INC_DIR "./drivers/${DEVICE}/" "./")
file(GLOB AT_TEST_SRC "./*.cpp")
file(GLOB VCAP_SRC "./drivers/${DEVICE}/*.c*")

add_executable(${PROJECT_NAME} ${AT_TEST_SRC} ${VCAP_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE ${INC_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE AT ${OpenCV_LIBS})
