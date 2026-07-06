cmake_minimum_required(VERSION 3.24)
project(zazaki_scp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZSCP_BUILD_TESTS "Build zazaki_scp tests" ON)

set(ZSSH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../zazaki_ssh")
set(ZSSH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${ZSSH_ROOT} ${CMAKE_BINARY_DIR}/zazaki_ssh)

add_executable(zazaki_scp
    src/main.cpp
    src/scp_cli.cpp
    src/progress_renderer.cpp
)
target_include_directories(zazaki_scp PRIVATE src)
target_link_libraries(zazaki_scp PRIVATE zssh_copy zssh_core zssh_protocol_libssh)

if(ZSCP_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.15.2)
    FetchContent_MakeAvailable(googletest)

    add_executable(
        zscp_unit_tests
        tests/unit/scp_cli_test.cpp
        tests/unit/progress_renderer_test.cpp
        src/scp_cli.cpp
        src/progress_renderer.cpp
    )
    target_compile_definitions(zscp_unit_tests PRIVATE ZSCP_TEST_BUILD)
    target_include_directories(zscp_unit_tests PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(zscp_unit_tests PRIVATE GTest::gtest_main zssh_copy zssh_core zssh_protocol_libssh)
    include(GoogleTest)
    gtest_discover_tests(zscp_unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(
        zscp_integration_tests
        tests/integration/end_to_end_test.cpp
        src/main.cpp
        src/scp_cli.cpp
        src/progress_renderer.cpp
    )
    target_compile_definitions(zscp_integration_tests PRIVATE ZSCP_TEST_BUILD)
    target_include_directories(zscp_integration_tests PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(zscp_integration_tests PRIVATE GTest::gtest_main zssh_copy zssh_core zssh_protocol_libssh)
    gtest_discover_tests(zscp_integration_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
