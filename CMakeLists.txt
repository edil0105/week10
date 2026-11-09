cmake_minimum_required(VERSION 3.15)
project(cg_week10 CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# glad пен stb осы репозиторийдің external/ бумасынан алынады
set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

add_executable(cg main.cpp ${EXTERNAL_DIR}/glad/src/gl.c)

target_include_directories(cg PRIVATE ${EXTERNAL_DIR}/glad/include ${EXTERNAL_DIR}/stb)

target_link_libraries(cg PRIVATE glfw OpenGL::GL ${CMAKE_DL_LIBS})

# F12 скриншоттары осы бумаға сақталады
target_compile_definitions(cg PRIVATE SCREENSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/screenshots")

set_target_properties(cg PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
