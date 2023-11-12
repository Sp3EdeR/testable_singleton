cmake_minimum_required(VERSION 3.12)

project(singleton VERSION 1.0 LANGUAGES CXX)

# The library target
add_library(singleton INTERFACE)
target_include_directories(
    singleton INTERFACE 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(
    singleton PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED TRUE
)
# Standard C++ mutex depends on pthread on Unix.
if (UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(singleton INTERFACE Threads::Threads)
endif()
