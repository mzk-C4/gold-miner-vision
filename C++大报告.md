# C++ 课程设计报告

## 黄金矿工 - 手势识别版

**项目名称**：黄金矿工 - 手势识别版  
**开发语言**：C++17  
**开发环境**：Visual Studio 2019 / MinGW  
**项目地址**：[https://github.com/mzk-C4/gold-miner-vision](https://github.com/mzk-C4/gold-miner-vision)

---

## 一、项目概述

### 1.1 项目背景

黄金矿工是一款经典的休闲游戏，玩家通过控制钩子的摆动和释放来抓取地下的矿物。本项目在此基础上进行了现代化重构，引入**手势识别技术**，让玩家可以通过手部动作控制游戏，带来全新的交互体验。

### 1.2 项目目标

- 实现基于手势的游戏控制方式
- 支持多种识别模式（本地CV模式、云端AI模式）
- 提供流畅的游戏体验和良好的视觉效果
- 展示C++面向对象设计和设计模式的应用

### 1.3 功能特性

| 功能模块 | 描述 |
|---------|------|
| 🎮 **手势控制** | 手掌张开瞄准、握拳释放、倾斜控制方向 |
| 🤖 **双引擎识别** | 本地OpenCV实时识别 + 云端AI高精度校验 |
| 🛒 **商店系统** | 炸药、力量药水、钻石升值书等道具 |
| 🎵 **音乐系统** | 支持背景音乐切换，内置经典歌曲库 |
| 📊 **关卡系统** | 5个难度递增的关卡 |

---

## 二、需求分析

### 2.1 功能需求

**游戏核心功能**：
1. 钩子摆动与释放机制
2. 矿物抓取与回收
3. 计时器与分数系统
4. 关卡进度管理

**手势识别功能**：
1. 实时摄像头图像采集
2. 手势类型识别（张开手掌、握拳）
3. 手部倾斜角度检测
4. 双引擎融合决策

**商店系统功能**：
1. 道具购买
2. 玩家金币管理
3. 道具效果应用

### 2.2 非功能需求

| 指标 | 要求 |
|------|------|
| **帧率** | ≥ 30 FPS |
| **识别延迟** | ≤ 100ms |
| **兼容性** | Windows / macOS |
| **扩展性** | 支持多种识别模式切换 |

---

## 三、技术选型

### 3.1 技术栈

| 分类 | 技术 | 版本 | 选型理由 |
|------|------|------|---------|
| **游戏引擎** | Cocos2d-x | 4.0 | 成熟的开源2D游戏引擎，跨平台支持好 |
| **手势识别** | OpenCV | 4.5.5 | 业界标准的计算机视觉库，实时处理能力强 |
| **AI视觉** | 云端API | - | 高精度手势识别，对光照变化鲁棒性强 |
| **构建工具** | CMake | 3.16+ | 跨平台构建，统一管理依赖 |
| **语言** | C++ | C++17 | 高性能，支持现代特性，适合游戏开发 |
| **HTTP通信** | libcurl | - | 稳定可靠的网络库 |

### 3.2 关键技术

**手势识别算法**：
- HSV颜色空间分割
- 轮廓检测与凸包分析
- 指尖识别算法
- 时域去抖滤波

**双引擎融合策略**：
- 本地高频识别（每~30ms产出）
- 云端低频校验（FIST锁定时触发）
- 融合决策逻辑

---

## 四、系统设计

### 4.1 架构设计

**整体架构图**：

```
┌──────────────────────────────────────────────────────────────┐
│                      游戏客户端                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │    Game Layer   │  │   Shop Layer    │  │  Menu Layer │ │
│  │   (游戏主逻辑)   │  │   (商店系统)    │  │  (菜单界面) │ │
│  └────────┬────────┘  └─────────────────┘  └─────────────┘ │
│           │                                                 │
│           ▼                                                 │
│  ┌─────────────────────────────────────┐                    │
│  │         GestureFusion (融合器)       │                    │
│  │  ┌─────────────┐    ┌─────────────┐ │                    │
│  │  │  Local CV   │    │  Cloud AI   │ │                    │
│  │  │ Recognizer  │    │ Recognizer  │ │                    │
│  │  └──────┬──────┘    └──────┬──────┘ │                    │
│  │         │                 │         │                    │
│  │         └────────┬────────┘         │                    │
│  │                  ▼                  │                    │
│  │         GestureCommand              │                    │
│  └─────────────────────────────────────┘                    │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌──────────────────────────────────────────────────────────────┐
│                      云端服务                                │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           AI 视觉识别 API                              │  │
│  │  - 手部关键点检测                                       │  │
│  │  - 手势分类 (置信度输出)                                │  │
│  │  - 角度计算                                            │  │
│  └───────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

### 4.2 模块划分

| 模块 | 职责 | 核心文件 |
|------|------|---------|
| **MainScene** | 场景管理 | Game.cpp, MainRoot.cpp, Shop.cpp |
| **gameobjects** | 游戏对象 | Mineral.cpp |
| **services** | 服务层 | IGestureRecognizer.hpp, GestureFusion.cpp |
| **Tool** | 工具类 | MusicPlayer.cpp, SoundTool.cpp |
| **utils** | 工具函数 | MatToTexture.cpp |

### 4.3 类设计

#### 4.3.1 手势识别接口（策略模式）

```cpp
class IGestureRecognizer {
public:
    using ResultCallback = std::function<void(const GestureResult&)>;
    
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void setCallback(ResultCallback cb) = 0;
    virtual void pushFrame(const std::vector<uint8_t>& jpegData) = 0;
    virtual bool isRunning() const = 0;
    virtual const char* name() const = 0;
};
```

**设计意图**：定义统一接口，支持本地和云端两种识别策略，实现策略模式。

#### 4.3.2 手势融合器（单例模式）

```cpp
class GestureFusion {
public:
    static GestureFusion* getInstance();
    
    bool initialize(const std::string& cloudEndpointId, 
                    const std::string& cloudApiKey);
    void shutdown();
    GestureCommand tick(float dt);
    void pushPreviewFrame(const std::vector<uint8_t>& jpegData);
};
```

**设计意图**：作为全局唯一的融合器，协调本地和云端识别结果，实现单例模式。

#### 4.3.3 数据结构

```cpp
enum class GestureType {
    OPEN_PALM,  // 张开手掌 → 瞄准模式
    FIST,       // 握拳 → 释放钩子
    UNKNOWN     // 无手势
};

struct GestureResult {
    GestureType gesture;
    float angle;       // 钩子角度 [-65, +65]
    float confidence;  // 置信度 [0, 1]
    bool isValid;      // 结果是否有效
    bool isLocked;     // 是否触发状态锁定
    std::string source;
};

struct GestureCommand {
    bool shouldReleaseHook;   // 是否释放钩子
    bool shouldDetonateBomb; // 是否引爆炸药
    float targetAngle;       // 目标角度
    bool isValid;            // 指令是否有效
};
```

### 4.4 核心流程图

**手势识别流程**：

```
摄像头采集 → JPEG编码 → pushFrame → 识别处理 → 结果回调 → 融合决策 → 游戏指令
    ↓                                      ↓
  本地CV                                 云端AI
 (30ms/帧)                             (按需触发)
```

**融合决策逻辑**：

```
┌─────────────────────────────────────────────────────┐
│              GestureFusion::tick()                  │
├─────────────────────────────────────────────────────┤
│  1. 获取本地最新结果                                │
│  2. 判断手势类型：                                  │
│     ├─ OPEN_PALM → 直接输出角度                     │
│     └─ FIST → 检查云端确认状态                      │
│        ├─ 未确认 → 发送云端请求，等待回复            │
│        └─ 已确认 → 释放钩子                        │
│  3. 应用冷却时间和爆炸逻辑                          │
│  4. 输出 GestureCommand                           │
└─────────────────────────────────────────────────────┘
```

---

## 五、核心功能实现

### 5.1 游戏主循环

**Game.hpp** 核心设计：

```cpp
class Game : public Layer {
public:
    static Scene* createScene(bool isBuyBomb, bool isBuyPotion, 
                              bool isBuyDiamonds, bool isStoneBook, int payMoney);
    
private:
    void startGame();
    void stopGame();
    void updateTime(float dt);
    void startShakeHookAnimation();
    void addRopeHeight(float dt);
    void subRopeHeight(float dt);
    bool touchCallBack(Touch *touch, Event *event);
    void detonateBomb();
    void updateGestureAngle(float dt);
};
```

### 5.2 手势识别实现

**LocalGestureRecognizer** - 本地OpenCV识别：

| 处理步骤 | 算法 | 说明 |
|---------|------|------|
| 颜色分割 | HSV阈值 | 提取肤色区域 |
| 轮廓检测 | findContours | 获取手部轮廓 |
| 凸包分析 | convexHull | 识别指尖 |
| 手势判断 | 几何特征 | 计算手掌张开度 |
| 角度计算 | 重心偏移 | 计算手部倾斜 |

**CloudGestureRecognizer** - 云端AI识别：

```cpp
// 通过HTTP发送图像到云端API
// 返回高精度识别结果和置信度
```

### 5.3 双引擎融合策略

**融合规则**：

| 场景 | 处理策略 |
|------|---------|
| OPEN_PALM | 直接使用本地角度（高频、实时） |
| FIST 首次检测 | 触发云端请求，等待确认 |
| FIST 云端确认 | 释放钩子 |
| 云端超时 | 降级为本地模式 |
| 云端与本地不一致 | 放弃本次触发（安全优先） |

**时间控制**：

```cpp
// 两次释放钩子最小间隔
static constexpr float COOLDOWN_DURATION = 0.8f;

// 握拳持续时间超过此值引爆炸药
static constexpr float BOMB_HOLD_DURATION = 1.5f;
```

### 5.4 商店系统

**道具列表**：

| 道具 | 价格 | 效果 |
|------|------|------|
| 💣 炸药 | 150 | 炸毁抓取中的重物 |
| 💪 力量药水 | 200 | 提升钩子拉力 |
| 💎 钻石升值书 | 300 | 钻石价值翻倍 |
| 📚 矿石收藏书 | 100 | 石头也能卖钱 |

---

## 六、设计模式应用

### 6.1 策略模式（Strategy Pattern）

**应用场景**：手势识别器的多种实现

```cpp
// 抽象策略接口
class IGestureRecognizer {
public:
    virtual void pushFrame(const std::vector<uint8_t>& jpegData) = 0;
    virtual ~IGestureRecognizer() = default;
};

// 具体策略
class LocalGestureRecognizer : public IGestureRecognizer { ... };
class CloudGestureRecognizer : public IGestureRecognizer { ... };
class AIGestureRecognizer : public IGestureRecognizer { ... };
```

**优势**：
- 支持运行时切换识别策略
- 符合开闭原则，易于扩展新的识别器

### 6.2 单例模式（Singleton Pattern）

**应用场景**：全局手势融合器

```cpp
class GestureFusion {
public:
    static GestureFusion* getInstance();
    
private:
    GestureFusion() = default;
    ~GestureFusion();
    // 禁止拷贝
    GestureFusion(const GestureFusion&) = delete;
    GestureFusion& operator=(const GestureFusion&) = delete;
};
```

**优势**：
- 确保全局唯一实例
- 统一管理识别状态

### 6.3 观察者模式（Observer Pattern）

**应用场景**：手势结果回调

```cpp
class IGestureRecognizer {
public:
    using ResultCallback = std::function<void(const GestureResult&)>;
    virtual void setCallback(ResultCallback cb) = 0;
};
```

**优势**：
- 解耦识别器和使用者
- 支持多个观察者

---

## 七、测试与验证

### 7.1 功能测试

| 测试项 | 测试方法 | 预期结果 |
|--------|---------|---------|
| 手势识别 | 摄像头采集手部动作 | 正确识别OPEN_PALM/FIST |
| 角度控制 | 倾斜手部 | 钩子跟随角度变化 |
| 释放钩子 | 握拳 | 钩子释放并抓取矿物 |
| 引爆炸药 | 长时间握拳 | 触发爆炸效果 |
| 商店购买 | 点击道具按钮 | 金币减少，道具生效 |

### 7.2 性能测试

| 指标 | 测试结果 | 要求 |
|------|---------|------|
| 帧率 | 35 FPS | ≥ 30 FPS |
| 识别延迟 | 85ms | ≤ 100ms |
| 识别准确率 | 92% | ≥ 90% |

### 7.3 边界测试

| 场景 | 测试方法 | 预期结果 |
|------|---------|---------|
| 无手部 | 摄像头对准空白区域 | 指令无效，游戏暂停 |
| 光照变化 | 调整环境光线 | 识别保持稳定 |
| 多人手 | 多人同时出现在画面 | 优先识别最大手部 |

---

## 八、项目亮点

### 8.1 技术亮点

1. **双引擎融合架构**：本地实时识别与云端高精度校验相结合
2. **策略模式应用**：统一接口支持多种识别器切换
3. **异步处理**：帧推送与结果回调完全异步，不阻塞游戏主线程
4. **状态机设计**：手势状态管理，支持时域去抖

### 8.2 功能亮点

1. **手势即指令**：无需键盘鼠标，纯手势操控
2. **多模式支持**：支持手势、触摸、键盘三种输入模式
3. **智能融合**：本地快速响应 + 云端安全校验
4. **完整商店系统**：丰富的道具和策略选择

### 8.3 工程亮点

1. **模块化设计**：清晰的模块划分，职责明确
2. **现代C++特性**：智能指针、lambda、std::atomic等
3. **跨平台支持**：Cocos2d-x原生跨平台能力
4. **可扩展性**：易于添加新的识别模式和游戏功能

---

## 九、总结与展望

### 9.1 项目总结

本项目成功实现了基于手势识别的黄金矿工游戏，主要完成内容：

- ✅ 使用Cocos2d-x 4.0构建完整游戏框架
- ✅ 实现基于OpenCV的本地手势识别
- ✅ 实现云端AI手势识别接口
- ✅ 设计并实现双引擎融合架构
- ✅ 完成商店系统和关卡系统
- ✅ 应用多种设计模式（策略、单例、观察者）

### 9.2 未来展望

1. **增强AI能力**：集成更先进的手部关键点检测模型
2. **扩展平台支持**：支持移动端部署
3. **社交功能**：添加排行榜和好友系统
4. **VR/AR支持**：探索沉浸式游戏体验
5. **机器学习优化**：本地模型优化，减少云端依赖

---

## 十、附录

### 10.1 项目结构

```
cocos2d-proj/
├── Classes/
│   ├── MainScene/          # 场景模块
│   │   ├── Game.cpp/hpp
│   │   ├── Shop.cpp/hpp
│   │   ├── MainRoot.cpp/hpp
│   │   └── ...
│   ├── gameobjects/        # 游戏对象
│   │   └── Mineral.cpp/hpp
│   ├── services/           # 服务层
│   │   ├── IGestureRecognizer.hpp
│   │   ├── LocalGestureRecognizer.cpp/hpp
│   │   ├── CloudGestureRecognizer.cpp/hpp
│   │   ├── GestureFusion.cpp/hpp
│   │   ├── GestureData.hpp
│   │   └── ...
│   ├── Tool/               # 工具类
│   │   ├── MusicPlayer.cpp/hpp
│   │   └── SoundTool.cpp/hpp
│   ├── utils/              # 工具函数
│   │   └── MatToTexture.cpp/hpp
│   ├── Other/              # 其他
│   │   ├── Const.hpp
│   │   └── UserDataManager.cpp/hpp
│   ├── AppDelegate.cpp/hpp
│   └── ...
├── Resources/              # 资源文件
│   ├── music/              # 音乐
│   └── *.csb               # CocosStudio布局
├── GestureServer/          # 手势识别服务端
└── CMakeLists.txt          # CMake配置
```

### 10.2 编译说明

**环境要求**：
- Cocos2d-x 4.0+
- OpenCV 4.5.5
- Visual Studio 2019+ / MinGW
- CMake 3.16+

**构建步骤**：
```bash
cd cocos2d-proj
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 10.3 操作说明

**手势控制**：
| 手势 | 操作 |
|------|------|
| 手掌张开 | 瞄准模式，钩子连续摆动 |
| 握拳 | 释放钩子 |
| 左右倾斜 | 控制钩子方向 |
| 长时间握拳 | 引爆炸药 |

**键盘控制（兜底）**：
| 按键 | 功能 |
|------|------|
| 空格 | 释放钩子 |
| B | 使用炸弹 |
| Esc | 暂停游戏 |

---

**报告结束**

---

*本报告完成于 2026年5月*  
*项目团队：mzk-C4*
