cmake_minimum_required(VERSION 3.20)  # VERSION 4.2 doesn't exist yet!

project(cgit LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Collect source files
file(GLOB_RECURSE SOURCE_FILES 
    src/*.c 
    src/*.h
)

# Find dependencies
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Link libraries - FIXED: use ${PROJECT_NAME} not "git"
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::Crypto)
target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
