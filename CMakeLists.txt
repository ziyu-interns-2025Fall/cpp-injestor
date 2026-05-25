cmake_minimum_required(VERSION 3.14)
project(cpp_injestor CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER REQUIRED poppler-cpp)
pkg_check_modules(HIREDIS REQUIRED hiredis)
pkg_check_modules(JSON REQUIRED nlohmann_json)
pkg_check_modules(CURL REQUIRED libcurl)

link_directories(${POPPLER_LIBRARY_DIRS} ${HIREDIS_LIBRARY_DIRS} ${JSON_LIBRARY_DIRS} ${CURL_LIBRARY_DIRS})

# GTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(injestor src/main.cpp src/RedisConsumer.cpp src/PdfParser.cpp src/Chunker.cpp src/MilvusClient.cpp)
target_include_directories(injestor PRIVATE ${POPPLER_INCLUDE_DIRS} ${HIREDIS_INCLUDE_DIRS} ${JSON_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
target_link_libraries(injestor PRIVATE ${POPPLER_LIBRARIES} ${HIREDIS_LIBRARIES} ${JSON_LIBRARIES} ${CURL_LIBRARIES})

add_executable(injestor_tests tests/main_test.cpp tests/RedisConsumer_test.cpp tests/PdfParser_test.cpp tests/Chunker_test.cpp src/RedisConsumer.cpp src/PdfParser.cpp src/Chunker.cpp src/MilvusClient.cpp)
target_include_directories(injestor_tests PRIVATE ${POPPLER_INCLUDE_DIRS} ${HIREDIS_INCLUDE_DIRS} ${JSON_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
target_link_libraries(injestor_tests gtest_main ${POPPLER_LIBRARIES} ${HIREDIS_LIBRARIES} ${JSON_LIBRARIES} ${CURL_LIBRARIES})
