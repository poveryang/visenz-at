cmake_minimum_required (VERSION 3.10)
project(AT_TEST)

set(DEVICE VS1000Pro)
set(INC_DIR "./drivers/${DEVICE}/" "./")
file(GLOB AT_TEST_SRC "./*.cpp")
file(GLOB VCAP_SRC "./drivers/${DEVICE}/*.c*")

add_executable(${PROJECT_NAME} ${AT_TEST_SRC} ${VCAP_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC ${INC_DIR} ${Barcode_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC AT ${OpenCV_LIBS} ${Barcode_LIBRARIES})