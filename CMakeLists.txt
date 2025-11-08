cmake_minimum_required(VERSION 3.16)
project(tyco-cpp VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Enable testing
enable_testing()

# Include directories
include_directories(include)

# Fetch nlohmann/json for JSON support
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# Source files - using new parser implementation
set(SOURCES 
    src/parser_new.cpp
)
file(GLOB_RECURSE HEADERS "include/*.h")

# Create library
add_library(tyco STATIC ${SOURCES} ${HEADERS})
target_include_directories(tyco PUBLIC include)
target_link_libraries(tyco PUBLIC nlohmann_json::nlohmann_json)

# CLI executable
add_executable(tyco-cli src/main.cpp)
target_link_libraries(tyco-cli tyco)

# Tests with Google Test (optional)
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
    add_executable(tyco-tests ${TEST_SOURCES})
    target_link_libraries(tyco-tests tyco gtest_main)
    
    include(GoogleTest)
    gtest_discover_tests(tyco-tests)
endif()

# Install targets
install(TARGETS tyco
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin)

install(DIRECTORY include/tyco DESTINATION include)