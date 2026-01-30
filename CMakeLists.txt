cmake_minimum_required(VERSION 3.24)
project(nickbot CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(libtouchstone GIT_REPOSITORY https://github.com/2ndwest/libtouchstone.git GIT_TAG main)
FetchContent_Declare(json_cpp GIT_REPOSITORY https://github.com/jart/json.cpp GIT_TAG main)
FetchContent_MakeAvailable(libtouchstone json_cpp)

add_executable(bot bot.cc)

target_link_libraries(bot libtouchstone::libtouchstone json)
target_include_directories(bot PRIVATE ${json_cpp_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
target_compile_features(bot PRIVATE cxx_std_17)
