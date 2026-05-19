cmake_minimum_required(VERSION 3.10)
project(test_at)

if (ENABLE_AT_TEST)
    # External dependencies
    file(GLOB CamCap_LIBS "${CMAKE_SOURCE_DIR}/3rdparty/cam_cap/${DEVICE}/lib/*.*")
    set(CamCap_INC "${CMAKE_SOURCE_DIR}/3rdparty/cam_cap/${DEVICE}/inc")

    # Executable definition
    add_executable(test_at "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

    target_link_libraries(test_at PUBLIC
        AT
        ${OpenCV_LIBS}
        ${CamCap_LIBS}
    )

    target_include_directories(test_at PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CamCap_INC}
    )

    # Add a general macro for DEVICE
    target_compile_definitions(test_at PUBLIC DEVICE_NAME="${DEVICE}")
    if (${DEVICE} MATCHES "vs1000p.*|vs800")
        add_definitions(-DUSE_V4L2)
    endif()
    # Installation
    install(TARGETS test_at DESTINATION bin)
endif()

if (ENABLE_SUB_TEST)
    message(STATUS "ENABLE_SUB_TEST is reserved for AE/AF submodule tests.")
endif()

if (ENABLE_CORE_TEST)
    add_executable(at_core_unit_test "${CMAKE_CURRENT_SOURCE_DIR}/unit/core_unit_test.cpp")
    target_link_libraries(at_core_unit_test PUBLIC AT_CORE ${OpenCV_LIBS})
    target_include_directories(at_core_unit_test PUBLIC "${CMAKE_SOURCE_DIR}/include")
endif()
