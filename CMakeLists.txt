cmake_minimum_required(VERSION 3.16)
project(rubcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(GLEW REQUIRED)

add_executable(rubcs
    src/main.cpp
    src/cube.cpp
    src/renderer.cpp
    src/solver.cpp
    src/font.cpp
)

target_include_directories(rubcs PRIVATE src)
target_link_libraries(rubcs PRIVATE OpenGL::GL glfw GLEW::GLEW pthread)

# Copy shaders to build directory
file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})

# Install target
install(TARGETS rubcs DESTINATION bin)
install(DIRECTORY shaders/ DESTINATION share/rubcs/shaders)

# ============================================================
# Tests
# ============================================================
enable_testing()

add_executable(rubcs_tests
    tests/test_main.cpp
    src/cube.cpp
    src/solver.cpp
)
target_include_directories(rubcs_tests PRIVATE src)

add_test(NAME rubcs_tests COMMAND rubcs_tests)
