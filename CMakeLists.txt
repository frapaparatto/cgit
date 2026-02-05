cmake_minimum_required(VERSION 4.2)

project(cgit LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCE_FILES 
    src/*.c 
    src/*.h
)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::Crypto)
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
