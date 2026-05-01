# 黄金矿工 — 手势控制版

基于经典黄金矿工游戏的重构版本，使用 Qt 6 + QGraphicsScene 实现，支持本地 CV 手势识别和 AI 视觉模式。

## 技术栈

- **核心框架**: Qt 6 (Widgets, Core, Gui, Network)
- **图形引擎**: QGraphicsScene / QGraphicsView
- **手势识别**: OpenCV 4.12 (本地 CV) / 远程 AI 视觉接口
- **构建系统**: CMake 3.16+
- **编程语言**: C++17

## 功能特性

### 双模操作系统

| 模式 | 说明 |
|------|------|
| **本地 CV 模式** | 基于 OpenCV 颜色阈值识别，使用蓝色/红色手套 |
| **AI 视觉模式** | 调用远程 AI 接口进行高精度手势识别 |
| **键盘兜底** | 传统键盘控制，确保所有环境都能游玩 |

### 手势控制

| 手势 | 功能 |
|------|------|
| 手部左右倾斜 | 控制钩子摇摆方向 |
| 手掌张开 | 释放钩子 |
| 握拳 | 加速回收钩子 |
| 点赞 | 触发炸弹功能（需购买） |

### 游戏系统

- **5个关卡**，难度递增
- **4种矿物**: 小金块、大金块、钻石、石头
- **商店系统**: 购买炸弹、力量药水、钻石升值书、矿石收藏书
- **数据持久化**: 使用 QSettings 保存用户金币和关卡进度

### 矿物属性

| 矿物 | 价值 | 重量 | 说明 |
|------|------|------|------|
| 小金块 | 200 | 5 | 性价比适中 |
| 大金块 | 500 | 10 | 价值高，重量中等 |
| 钻石 | 600 | 1 | 高价值轻量 |
| 石头 | 20 | 15 | 低价值高重量 |

## 项目结构

```
GoldMiner/
├── CMakeLists.txt          # CMake 构建配置
├── main.cpp                # 程序入口
├── README.md               # 项目说明
├── config/                 # 关卡配置文件
│   ├── level1.json
│   ├── level2.json
│   ├── level3.json
│   ├── level4.json
│   └── level5.json
├── src/                    # 源代码
│   ├── core/               # 核心游戏逻辑
│   │   ├── Mineral.h/cpp   # 矿物类体系
│   │   └── Hook.h/cpp      # 钩子类
│   ├── scene/              # 场景管理
│   │   ├── HomeScene.h/cpp # 首页
│   │   ├── GameScene.h/cpp # 游戏场景
│   │   ├── ShopScene.h/cpp # 商店场景
│   │   └── SceneManager.h/cpp
│   ├── gesture/            # 手势识别
│   │   └── HandTracker.h/cpp
│   ├── network/            # 网络通信
│   │   └── AIVisionClient.h/cpp
│   ├── data/               # 数据管理
│   │   ├── UserDataManager.h/cpp
│   │   └── LevelConfig.h/cpp
│   └── ui/                 # UI 层
│       └── MainWindow.h/cpp
├── res/                    # 资源文件（来自原 Cocos2d 项目）
│   ├── Resources/          # 图片资源
│   ├── music/              # 音效
│   └── *.csb               # CocosStudio 布局文件
└── Classes/                # 原 Cocos2d 项目源码（保留参考）
    ├── MainScene/
    ├── Other/
    └── Tool/
```

## 环境要求

### 必需

- Qt 6.x
- CMake 3.16+
- C++17 兼容编译器 (MinGW / MSVC)

### 可选（手势识别）

- OpenCV 4.12
- 摄像头设备

## 构建步骤

### 1. 配置 OpenCV（可选）

在 `CMakeLists.txt` 中修改 OpenCV 路径：

```cmake
set(OPENCV_DIR "F:/toolbox/opencv/build")
```

### 2. 构建项目

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 3. 运行

构建产物位于 `build/GoldMiner.exe`，配置文件会自动复制到构建目录。

## 键盘控制（兜底方案）

| 按键 | 功能 |
|------|------|
| 空格 | 释放钩子 |
| ↑ / ↓ | 加速回收 / 减速 |
| B | 使用炸弹 |
| Esc | 暂停/返回 |

## 开发说明

### 关卡配置

关卡数据使用 JSON 格式存储在 `config/level*.json`：

```json
{
  "level": 1,
  "target_money": 650,
  "time_limit": 60,
  "minerals": [
    {"type": "small_gold", "x": 300, "y": 350, "value": 200, "weight": 5, "size": 25}
  ]
}
```

### 手势调试

本地 CV 模式支持调整 HSV 颜色阈值以适应不同颜色的手套，相关参数在 `HandTracker.h` 中定义。

## 历史版本

- **v1.0 (当前)**: Qt 6 + 手势识别重构版
- **v0.1**: 原始 Cocos2d-X 3.12 版本（保留在 Classes/ 目录）

## 致谢

- 原始项目: [ZhongTaoTian/GoldMiner](https://github.com/ZhongTaoTian/GoldMiner)
- Cocos2d-X 游戏引擎
