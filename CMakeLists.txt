cmake_minimum_required(VERSION 3.24)
project(hfpe CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(libtouchstone GIT_REPOSITORY https://github.com/2ndwest/libtouchstone.git GIT_TAG main)
FetchContent_Declare(json_cpp GIT_REPOSITORY https://github.com/jart/json.cpp GIT_TAG main)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr GIT_TAG 1.10.5)
FetchContent_MakeAvailable(libtouchstone json_cpp cpr)

add_executable(bot bot.cc)

enable_testing()
file(GLOB TEST_FILES "${CMAKE_SOURCE_DIR}/test/*.cc")

foreach(test_src ${TEST_FILES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src} sectionlist.cc)
    target_include_directories(${test_name} PRIVATE "${CMAKE_SOURCE_DIR}")
    add_test(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()

# Convenience target: build + run all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS bot
)

target_link_libraries(bot libtouchstone::libtouchstone json cpr::cpr)
target_include_directories(bot PRIVATE ${json_cpp_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
target_compile_features(bot PRIVATE cxx_std_17)
