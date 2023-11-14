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

# Unit Test.
if (NOT SINGLETON_SKIP_TEST)
    enable_testing()

    add_executable(
        singleton_test
        test/unit/singleton_test.cpp
    )
    set_target_properties(
        singleton_test PROPERTIES
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED TRUE
    )
  
    # Use access_private for the unit test
    target_include_directories(singleton_test PRIVATE test/thirdparty/access_private/include)
  
    # Use Google Test framework for the unit test
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(test/thirdparty/googletest EXCLUDE_FROM_ALL)
    target_link_libraries(singleton_test gtest_main gmock)

    # Register in ctest
    add_test(NAME singleton_test COMMAND "$<TARGET_FILE:singleton_test>")
endif()
