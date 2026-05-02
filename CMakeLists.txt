cmake_minimum_required(VERSION 3.16)
project(GoldMiner VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ==================== Qt 6 依赖 ====================
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# ==================== OpenCV MinGW 预编译版 ====================
set(OpenCV_DIR "F:/toolbox/opencv/OpenCV-MinGW-Build/x64/mingw/lib")
find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs videoio)

if(OpenCV_FOUND)
    add_definitions(-DHAS_OPENCV)
    message(STATUS "OpenCV found: ${OpenCV_VERSION} (MinGW)")
else()
    message(WARNING "OpenCV not found - 手势识别功能将不可用，仅保留键盘操作模式")
endif()

# ==================== 源文件 ====================
set(SOURCES
    main.cpp
    src/data/UserDataManager.cpp
    src/data/LevelConfig.cpp
    src/core/Mineral.cpp
    src/core/Hook.cpp
    src/scene/GameScene.cpp
    src/scene/ShopScene.cpp
    src/scene/HomeScene.cpp
    src/scene/SceneManager.cpp
    src/ui/MainWindow.cpp
    src/gesture/HandTracker.cpp
    src/network/AIVisionClient.cpp
)

# ==================== 可执行文件 ====================
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/config
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
)

if(OpenCV_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        opencv_core opencv_imgproc opencv_imgcodecs opencv_videoio
    )
endif()

# ==================== 复制配置文件和 OpenCV DLL 到构建目录 ====================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/config
)

if(OpenCV_FOUND)
    foreach(dll core imgproc imgcodecs videoio)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "F:/toolbox/opencv/OpenCV-MinGW-Build/x64/mingw/bin/libopencv_${dll}452.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endforeach()
endif()

# ==================== Windows 设置 ====================
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
