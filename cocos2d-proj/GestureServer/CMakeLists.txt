cmake_minimum_required(VERSION 3.6)
project(GestureServer)

set(OpenCV_DIR "F:/tools/opencv/build/x64/vc16/lib")
find_package(OpenCV REQUIRED)

add_executable(GestureServer main.cpp)
target_link_libraries(GestureServer ${OpenCV_LIBS})
target_include_directories(GestureServer PRIVATE ${OpenCV_INCLUDE_DIRS})

if(MSVC)
    target_compile_options(GestureServer PRIVATE /utf-8)
endif()

set_target_properties(GestureServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
