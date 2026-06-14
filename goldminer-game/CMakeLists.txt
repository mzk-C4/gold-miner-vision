cmake_minimum_required(VERSION 3.6)

set(APP_NAME GoldMiner)
project(${APP_NAME})

set(COCOS2DX_ROOT_PATH "D:/cocos2d-x")
set(CMAKE_MODULE_PATH ${COCOS2DX_ROOT_PATH}/cmake/Modules/)

include(CocosBuildSet)
add_subdirectory(${COCOS2DX_ROOT_PATH}/cocos ${ENGINE_BINARY_PATH}/cocos/core)

# OpenCV - MinGW build
set(OpenCV_DIR "F:/toolbox/opencv/OpenCV-MinGW-Build/x64/mingw/lib")
find_package(OpenCV REQUIRED)

set(GAME_RES_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/Resources")

if(WINDOWS)
    cocos_mark_multi_resources(common_res_files RES_TO "Resources" FOLDERS ${GAME_RES_FOLDER})
endif()

list(APPEND GAME_SOURCE
    Classes/Other/AppDelegate.cpp
    Classes/MainScene/MainRoot.cpp
    Classes/MainScene/Game.cpp
    Classes/MainScene/Shop.cpp
    Classes/MainScene/Pause.cpp
    Classes/MainScene/StageTipsLayer.cpp
    Classes/Other/UserDataManager.cpp
    Classes/Tool/SoundTool.cpp
    Classes/Tool/MusicPlayer.cpp
    Classes/gameobjects/Mineral.cpp
    Classes/services/LevelLoader.cpp
    Classes/services/GestureClient.cpp
    Classes/services/PlayerManager.cpp
    Classes/services/AIGestureService.cpp
    Classes/services/LocalGestureRecognizer.cpp
    Classes/services/CloudGestureRecognizer.cpp
    Classes/services/GestureFusion.cpp
    Classes/services/AIGestureRecognizer.cpp
    Classes/MainScene/Gold.cpp
    Classes/services/GestureRecognizer.cpp
    Classes/utils/MatToTexture.cpp
)

list(APPEND GAME_HEADER
    Classes/Other/AppDelegate.h
    Classes/MainScene/MainRoot.hpp
    Classes/MainScene/Game.hpp
    Classes/MainScene/Shop.hpp
    Classes/MainScene/Pause.hpp
    Classes/MainScene/StageTipsLayer.hpp
    Classes/Other/Const.hpp
    Classes/Other/UserDataManager.hpp
    Classes/Tool/SoundTool.hpp
    Classes/Tool/MusicPlayer.hpp
    Classes/gameobjects/Mineral.hpp
    Classes/services/LevelLoader.hpp
    Classes/services/GestureClient.hpp
    Classes/services/PlayerManager.hpp
    Classes/services/AIGestureService.hpp
    Classes/services/GestureData.hpp
    Classes/services/IGestureRecognizer.hpp
    Classes/services/LocalGestureRecognizer.hpp
    Classes/services/CloudGestureRecognizer.hpp
    Classes/services/GestureFusion.hpp
    Classes/services/AIGestureRecognizer.hpp
    Classes/MainScene/Gold.hpp
    Classes/services/GestureRecognizer.hpp
    Classes/utils/MatToTexture.hpp
)

if(WINDOWS)
    list(APPEND GAME_HEADER
        proj.win32/main.h
        proj.win32/resource.h
    )
    list(APPEND GAME_SOURCE
        proj.win32/main.cpp
        ${common_res_files}
    )
endif()

set(all_code_files ${GAME_HEADER} ${GAME_SOURCE})
add_executable(${APP_NAME} ${all_code_files})

target_link_libraries(${APP_NAME} cocos2d ${OpenCV_LIBS})
target_include_directories(${APP_NAME}
    PRIVATE Classes
    PRIVATE ${OpenCV_INCLUDE_DIRS}
    PRIVATE Classes/Other
    PRIVATE Classes/Tool
    PRIVATE Classes/MainScene
    PRIVATE Classes/gameobjects
    PRIVATE Classes/services
    PRIVATE Classes/utils
    PRIVATE Classes/utils
    PRIVATE ${COCOS2DX_ROOT_PATH}/cocos/audio/include
    PRIVATE ${COCOS2DX_ROOT_PATH}/cocos/editor-support/cocostudio
    PRIVATE ${COCOS2DX_ROOT_PATH}/external
)

setup_cocos_app_config(${APP_NAME})

if(MSVC)
    target_compile_options(${APP_NAME} PRIVATE /utf-8)
else()
    target_compile_options(${APP_NAME} PRIVATE -std=c++17 -Wall)
endif()

if(WINDOWS)
    cocos_copy_target_dll(${APP_NAME})
    cocos_get_resource_path(APP_RES_DIR ${APP_NAME})
    cocos_copy_target_res(${APP_NAME} LINK_TO ${APP_RES_DIR} FOLDERS ${GAME_RES_FOLDER})

    # Copy GestureServer.exe and OpenCV DLL to game output directory
    set(GESTURE_SERVER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/GestureServer/build/bin/GestureServer.exe")
    set(OPENCV_DLL_DIR "F:/toolbox/opencv/OpenCV-MinGW-Build/x64/mingw/bin")
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GESTURE_SERVER_SRC}" "$<TARGET_FILE_DIR:${APP_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPENCV_DLL_DIR}/libopencv_core452.dll" "$<TARGET_FILE_DIR:${APP_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPENCV_DLL_DIR}/libopencv_imgproc452.dll" "$<TARGET_FILE_DIR:${APP_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPENCV_DLL_DIR}/libopencv_highgui452.dll" "$<TARGET_FILE_DIR:${APP_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPENCV_DLL_DIR}/libopencv_videoio452.dll" "$<TARGET_FILE_DIR:${APP_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPENCV_DLL_DIR}/libopencv_imgcodecs452.dll" "$<TARGET_FILE_DIR:${APP_NAME}>"
        COMMENT "Copying GestureServer.exe and OpenCV DLLs to game output"
    )
endif()
