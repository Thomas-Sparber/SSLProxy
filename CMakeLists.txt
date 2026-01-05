cmake_minimum_required(VERSION 3.14)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(SSLProxy VERSION 1.0.0 LANGUAGES CXX)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_library(SSLProxy INTERFACE)

target_include_directories(SSLProxy INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(SSLProxy INTERFACE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

if(WIN32)
    target_link_libraries(SSLProxy INTERFACE ws2_32 wsock32)
endif()

target_compile_features(SSLProxy INTERFACE cxx_std_17)

option(SSLPROXY_BUILD_EXAMPLES "Build the example applications" ON)

if(SSLPROXY_BUILD_EXAMPLES)
    message(STATUS "Building SSLProxy examples...")

    function(add_proxy_example target_name source_file)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/${source_file}")
            add_executable(${target_name} "src/${source_file}")
            target_link_libraries(${target_name} PRIVATE SSLProxy)
            if(MSVC)
                target_compile_options(${target_name} PRIVATE /W4)
            else()
                target_compile_options(${target_name} PRIVATE -Wall -Wextra)
            endif()
        endif()
    endfunction()

    add_proxy_example(test_blocking "test_blocking.cpp")
    add_proxy_example(test_nonblocking "test_nonblocking.cpp")

    find_package(Boost 1.66 QUIET) 
    if(Boost_FOUND)
        message(STATUS "Boost found. Building test_boost.")
        add_proxy_example(test_boost "test_boost.cpp")
        target_link_libraries(test_boost PRIVATE Boost::headers) 
    endif()

    find_package(Crow QUIET)
    find_path(CROW_INCLUDE_DIR "crow.h")
    if(Crow_FOUND OR CROW_INCLUDE_DIR)
        message(STATUS "Crow found. Building test-crow.")
        add_proxy_example(test-crow "test-crow.cpp")
        if(CROW_INCLUDE_DIR)
            target_include_directories(test-crow PRIVATE ${CROW_INCLUDE_DIR})
        endif()
    endif()
endif()