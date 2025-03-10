if(WIN32)
    set(Boost_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/boost_1_79_0.zip")
else()
    set(Boost_URL "https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/boost_1_79_0.tar.gz")
endif()

FetchContent_Declare(Boost
    URL ${Boost_URL}
)
message(STATUS "Fetching opencv : ${Boost_URL}")
FetchContent_MakeAvailable(Boost)
message(STATUS "Fetching done")

FetchContent_GetProperties(Boost SOURCE_DIR Boost_SOURCE_DIR)
message(STATUS "Boost src dir : ${Boost_SOURCE_DIR}")

set(Boost_INCLUDE_DIRS ${Boost_SOURCE_DIR})
# get OpenCV_DIR
# if(WIN32)
# set(OpenCV_DIR ${OpenCV_SOURCE_DIR}/x64/vc15/lib)
# else()
# set(OpenCV_DIR ${OpenCV_SOURCE_DIR}/lib/cmake/opencv4)
# endif()

# find_package(Boost REQUIRED)