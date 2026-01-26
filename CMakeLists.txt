cmake_minimum_required(VERSION 3.14)
project(rsjfw VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(CURL REQUIRED)
find_package(LibArchive REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.90
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

FetchContent_Declare(
    ImGuiFileDialog
    GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog
    GIT_TAG v0.6.6
)
FetchContent_Populate(ImGuiFileDialog)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(${json_SOURCE_DIR}/include)
include_directories(include)
include_directories(${imgui_SOURCE_DIR})
include_directories(${imgui_SOURCE_DIR}/backends)
include_directories(${stb_SOURCE_DIR})
include_directories(${imguifiledialog_SOURCE_DIR})

add_subdirectory(src/layer)

file(GLOB_RECURSE MAIN_SOURCES "src/*.cpp")
list(REMOVE_ITEM MAIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/layer/layer.cpp")

add_executable(rsjfw ${MAIN_SOURCES} ${IMGUI_SOURCES} ${imguifiledialog_SOURCE_DIR}/ImGuiFileDialog.cpp)

target_link_libraries(rsjfw
    PRIVATE
    nlohmann_json::nlohmann_json
    ${CURL_LIBRARIES}
    ${LibArchive_LIBRARIES}
    glfw
    OpenGL::GL
    Vulkan::Vulkan
    pthread
    dl
)

install(TARGETS rsjfw DESTINATION bin)

enable_testing()
add_executable(registry_test tests/registry_test.cpp src/registry.cpp src/logger.cpp)
target_link_libraries(registry_test GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(registry_test)

add_executable(registry_verify tests/registry_verify.cpp src/registry.cpp src/logger.cpp)
target_link_libraries(registry_verify GTest::gtest_main)
gtest_discover_tests(registry_verify)

add_executable(reg_convert tests/reg_convert.cpp src/registry.cpp src/logger.cpp)
