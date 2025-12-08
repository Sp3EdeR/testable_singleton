cmake_minimum_required(VERSION 3.12)

project(singleton VERSION 1.0 LANGUAGES CXX)

include(GNUInstallDirs)

# The library target
add_library(singleton INTERFACE)
target_include_directories(
    singleton INTERFACE 
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(singleton INTERFACE cxx_std_11)

# Standard C++ mutex depends on pthread on Unix.
if (UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(singleton INTERFACE Threads::Threads)
endif()

# Installation of the header and cmake module.
option(SINGLETON_DO_INSTALL "Enable the installation of the singleton headers" ON)
if (SINGLETON_DO_INSTALL)
    # Install the headers
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    # Install the interface target
    install(
        TARGETS singleton
        EXPORT ${CMAKE_PROJECT_NAME}Config
    )
    # Install the export
    install(
        EXPORT ${CMAKE_PROJECT_NAME}Config
        NAMESPACE Singleton::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME}"
    )
endif()

# Documentation generation using Doxygen
option(SINGLETON_BUILD_DOC "Disable the generation of singleton documentation" ON)
if (SINGLETON_BUILD_DOC)
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "README.md")
        set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc")
        doxygen_add_docs(singleton_doc "README.md" include)
    endif()

    if (SINGLETON_DO_INSTALL)
        install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc/html/" TYPE DOC OPTIONAL)
    endif()
endif()

# Unit Test.
# Legacy compatibility: SINGLETON_SKIP_TEST overrides and forces tests off.
if ("${SINGLETON_SKIP_TEST}")
    cmake_policy(SET CMP0077 NEW)
    set(BUILD_TESTING OFF)
endif()
# Loads the BUILD_TESTING via CTest's inclusion.
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()
# Project-scoped test option to change test building compared to the global BUILD_TESTING.
option(SINGLETON_BUILD_TESTING "Build the singleton's tests" ${BUILD_TESTING})
if (SINGLETON_BUILD_TESTING)
    # If parent did not include CTest, ensure testing is enabled locally.
    enable_testing()

    add_executable(singleton_test test/unit/singleton_test.cpp)
    set_target_properties(
        singleton_test PROPERTIES
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED TRUE
    )

    # Use Google Test framework for the unit test
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/thirdparty/googletest/CMakeLists.txt")
        message(FATAL_ERROR "Google Test submodule not found. Please run 'git submodule update --init --recursive'")
    endif()
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(test/thirdparty/googletest EXCLUDE_FROM_ALL)
    target_link_libraries(singleton_test gtest_main gmock)

    # Register in ctest
    add_test(NAME singleton_test COMMAND "$<TARGET_FILE:singleton_test>")

    # Build examples, and register as tests in ctest
    add_executable(example_base_usage doc/example_base_usage.cpp)
    target_link_libraries(example_base_usage singleton)
    add_test(NAME example_base_usage COMMAND "$<TARGET_FILE:example_base_usage>")
    add_executable(example_constructor doc/example_constructor.cpp)
    target_link_libraries(example_constructor singleton)
    add_test(NAME example_constructor COMMAND "$<TARGET_FILE:example_constructor>")
    add_executable(example_full_usage doc/example_full_usage.cpp)
    target_link_libraries(example_full_usage singleton)
    add_test(NAME example_full_usage COMMAND "$<TARGET_FILE:example_full_usage>")
endif()
