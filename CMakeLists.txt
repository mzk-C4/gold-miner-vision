cmake_minimum_required(VERSION 3.16)
project(GoldMiner VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ==================== Qt 6 依赖 ====================
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# ==================== OpenCV 配置（MinGW 导入库） ====================
set(OPENCV_DIR "F:/toolbox/opencv/build")
set(OPENCV_INCLUDE_DIR "${OPENCV_DIR}/include")
set(OPENCV_LIB_DIR "${OPENCV_DIR}/x64/vc16/bin")

if(EXISTS "${OPENCV_INCLUDE_DIR}/opencv2/core.hpp" AND
   EXISTS "${OPENCV_LIB_DIR}/libopencv_world4120.a")
    add_definitions(-DHAS_OPENCV)
    set(OPENCV_LIBS "${OPENCV_LIB_DIR}/libopencv_world4120.a")
    message(STATUS "OpenCV configured (MinGW import lib): ${OPENCV_LIBS}")
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

set(HEADERS
    src/data/UserDataManager.h
    src/data/LevelConfig.h
    src/core/Mineral.h
    src/core/Hook.h
    src/scene/GameScene.h
    src/scene/ShopScene.h
    src/scene/HomeScene.h
    src/scene/SceneManager.h
    src/ui/MainWindow.h
    src/gesture/HandTracker.h
    src/network/AIVisionClient.h
)

# ==================== 可执行文件 ====================
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/config
)

if(DEFINED OPENCV_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${OPENCV_INCLUDE_DIR})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
)

if(DEFINED OPENCV_LIBS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENCV_LIBS})
endif()

# ==================== 复制配置文件和 DLL 到构建目录 ====================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/config
)

if(DEFINED OPENCV_LIB_DIR)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENCV_LIB_DIR}/opencv_world4120.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

# ==================== Windows 特殊设置 ====================
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
