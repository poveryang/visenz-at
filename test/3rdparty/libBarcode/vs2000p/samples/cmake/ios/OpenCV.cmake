if(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
    set(OPENCV_LOCAL_PATH ${CMAKE_SOURCE_DIR}/3rdparty/OpenCV_iOS/opencv2.framework)
    get_filename_component(OPENCV_LOCAL_CMAKE_PATH ${OPENCV_LOCAL_PATH} DIRECTORY)
else()
    message(FATAL_ERROR "Only Support on iOS!")
endif()

if(EXISTS ${OPENCV_LOCAL_CMAKE_PATH})
    message(STATUS "OpenCV_DIR exists at ${OPENCV_LOCAL_CMAKE_PATH}, fetch skipped.")
    set(OpenCV_DIR ${OPENCV_LOCAL_CMAKE_PATH})
else()
    set(OpenCV_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/opencv/opencv-4.5.2-ios-framework.zip")
    set(OpenCV_MD5 "7682c7bc246a5990b259104343a1fffa")

    include(FetchContent)

    FetchContent_Declare(OpenCV
        URL ${OpenCV_URL}
        SOURCE_DIR ${OPENCV_LOCAL_PATH}
        URL_MD5 ${OpenCV_MD5}
    )

    message(STATUS "Fetch OpenCV from ${OpenCV_URL}.")
    # get OpenCV
    FetchContent_MakeAvailable(OpenCV)
    message(STATUS "Fetch OpenCV finished.")

    set(OpenCV_DIR ${OPENCV_LOCAL_CMAKE_PATH})
    message(STATUS "OpenCV_DIR: ${OpenCV_DIR}.")
endif()

# temp set CMAKE_FIND_ROOT_PATH_MODE_INCLUDE & CMAKE_FIND_ROOT_PATH_MODE_LIBRARY
set(TMP_CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE})
set(TMP_CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY})
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)

set(CMAKE_FIND_FRAMEWORK FIRST)

find_library(OpenCV_LIBS
    NAMES opencv2
    PATHS ${OpenCV_DIR}
    REQUIRED
    NO_DEFAULT_PATH)

find_path(OpenCV_INCLUDE_DIRS
    NAMES opencv.hpp
    PATHS ${OpenCV_LIBS}/Headers
    REQUIRED
    NO_DEFAULT_PATH)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCV DEFAULT_MSG OpenCV_INCLUDE_DIRS OpenCV_LIBS)

if(OPENCV_FOUND)
    message(STATUS "Found OpenCV: (include: ${OpenCV_INCLUDE_DIRS}, library: ${OpenCV_LIBS})")
endif()
# reset CMAKE_FIND_ROOT_PATH_MODE_INCLUDE & CMAKE_FIND_ROOT_PATH_MODE_LIBRARY
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ${TMP_CMAKE_FIND_ROOT_PATH_MODE_INCLUDE})
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ${TMP_CMAKE_FIND_ROOT_PATH_MODE_LIBRARY})
unset(TMP_CMAKE_FIND_ROOT_PATH_MODE_INCLUDE)
unset(TMP_CMAKE_FIND_ROOT_PATH_MODE_LIBRARY)
