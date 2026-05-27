cmake_minimum_required(VERSION 3.14)
project(cpp_ingestor CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(PkgConfig REQUIRED)

# Use IMPORTED_TARGET to get modern CMake targets (PkgConfig::*)
pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-cpp)
pkg_check_modules(HIREDIS REQUIRED IMPORTED_TARGET hiredis)
pkg_check_modules(JSON REQUIRED IMPORTED_TARGET nlohmann_json)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl)

# Create a library for core logic to avoid compiling the same files multiple times
add_library(ingestor_core STATIC
    src/RedisConsumer.cpp
    src/PdfParser.cpp
    src/Chunker.cpp
    src/MilvusClient.cpp
    src/Embedder.cpp
)

# Link dependencies to the core library
target_link_libraries(ingestor_core PUBLIC
    PkgConfig::POPPLER
    PkgConfig::HIREDIS
    PkgConfig::JSON
    PkgConfig::CURL
    PkgConfig::OPENSSL
)

# Ensure the library knows where its own headers are
target_include_directories(ingestor_core PUBLIC src)

# Main executable
add_executable(ingestor src/main.cpp)
target_link_libraries(ingestor PRIVATE ingestor_core)

# GTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(ingestor_tests 
    tests/main_test.cpp 
    tests/RedisConsumer_test.cpp 
    tests/PdfParser_test.cpp 
    tests/Chunker_test.cpp
)
target_link_libraries(ingestor_tests 
    PRIVATE 
    ingestor_core 
    gtest_main
)
