# 黄金矿工（手势识别版）—— C++ 课程设计学习报告

> 项目类型：C++ 课程设计 / 桌面游戏
>
> 核心技术栈：C++17、Cocos2d-x 4.0、OpenCV、RapidJSON、libcurl、豆包大模型 API
>
> 代码仓库：`goldminer-game/Classes/` 及 `GestureServer/main.cpp`

---

## 目录

1. [项目概述](#1-项目概述)
2. [用到的 C++ 知识（按主题分组）](#2-用到的-c-知识按主题分组)
   - 2.1 面向对象基础
   - 2.2 C++11/14/17 现代特性
   - 2.3 模板、STL 容器与算法
   - 2.4 内存管理与资源生命周期
   - 2.5 多线程并发编程
   - 2.6 设计模式
   - 2.7 网络与外部库集成
   - 2.8 游戏引擎（Cocos2d-x）专用知识
   - 2.9 跨模块编程实践
3. [总体架构与模块连接](#3-总体架构与模块连接)
4. [核心算法流程](#4-核心算法流程)
5. [踩过的坑与解决方法](#5-踩过的坑与解决方法)
6. [如果重新开始会怎么做](#6-如果重新开始会怎么做)
7. [结语：C++ 学习心得](#7结语c-学习心得)

---

## 1. 项目概述

黄金矿工（手势识别版）是经典"黄金矿工"游戏的现代化重制版。玩家可以选择三种控制方式：

- **触摸/键鼠模式**（传统）
- **OpenCV 本地手势识别模式**（摄像头 + OpenCV 做手部检测）
- **AI 大模型手势识别模式**（摄像头 + 豆包视觉大模型）

游戏主体基于 Cocos2d-x 4.0 引擎用 C++17 开发，大约有 30 个 .cpp/.hpp 文件，约 6000 行 C++ 代码（不含引擎）。手势识别部分封装成一套完整的**本地+云端双通道混合识别架构**，用纯 C++17 实现，是项目最有技术含量的部分。

项目源码所在目录：

```
goldminer-game/
├── Classes/
│   ├── MainScene/        场景层：Game / MainRoot / Shop / Pause / StageTipsLayer
│   ├── gameobjects/      实体层：Mineral
│   ├── services/         服务层：手势识别 + 存档 + 关卡加载
│   ├── Tool/             工具层：MusicPlayer / SoundTool
│   ├── utils/            工具层：MatToTexture
│   └── Other/            基础设施：AppDelegate / Const / UserDataManager
├── GestureServer/        独立进程：OpenCV 摄像头采集服务
└── Resources/            资源文件（.csb / .plist / .json / .mp3 / .png）
```

---

## 2. 用到的 C++ 知识（按主题分组）

### 2.1 面向对象基础

#### 类与对象

项目中的模块几乎全部以类的形式组织。代表性文件：

- [Game.hpp](file:///d:/GOLD%20MINER/goldminer-game/Classes/MainScene/Game.hpp) — 游戏主场景类
- [Mineral.hpp](file:///d:/GOLD%20MINER/goldminer-game/Classes/gameobjects/Mineral.hpp) — 矿物实体类
- [PlayerManager.hpp](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/PlayerManager.hpp) — 多玩家管理器

每个类都用 `public / protected / private` 分区封装数据和行为。例如 `Mineral` 把价格、回收速度、钩子转角作为公共成员（其他类需要读取），而商店加成系数 `_power`、`_diamondsCoe`、`_stoneCoe` 作为 protected 成员（Mineral 内部逻辑使用）。

```cpp
class Mineral : public cocos2d::Sprite {
public:
    enum class Type { GOLD_SMALL, GOLD_MEDIUM, ... };
    int  score = 0;
    float hookRote = 0;
    int  backSpeed = 10;
    Type mineralType;
protected:
    int _power = 1;
    int _stoneCoe = 1;
    int _diamondsCoe = 1;
};
```

#### 继承与多态

- `Mineral : public cocos2d::Sprite` — 继承 Cocos 精灵，获得渲染能力
- **手势识别体系**（最典型）：`IGestureRecognizer` 是纯虚基类，三个实现类继承并 override 全部接口：

```
IGestureRecognizer（抽象接口）
  ├─ LocalGestureRecognizer   本地 OpenCV 高频识别
  ├─ CloudGestureRecognizer   云端大模型低频校验
  └─ AIGestureRecognizer      AI 独占模式
```

参考 [IGestureRecognizer.hpp](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/IGestureRecognizer.hpp)：

```cpp
class IGestureRecognizer {
public:
    using ResultCallback = std::function<void(const GestureResult&)>;
    virtual ~IGestureRecognizer() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void setCallback(ResultCallback cb) = 0;
    virtual void pushFrame(const std::vector<uint8_t>& jpegData) = 0;
    virtual bool isRunning() const = 0;
    virtual const char* name() const = 0;
};
```

`GestureFusion` 融合器在运行时根据玩家选择的模式切换识别器指针，实现**运行时多态**。

#### 访问控制、友元、静态成员

- 所有 Manager 类都把**构造函数设为私有**，配合静态单例指针和 `getInstance()` 实现单例模式（见 §2.6）。
- `static InputMode _defaultInputMode`（[Game.hpp:61](file:///d:/GOLD%20MINER/goldminer-game/Classes/MainScene/Game.hpp#L61)）静态类成员在 .cpp 文件中单独定义。

### 2.2 C++11/14/17 现代特性

项目全面使用了 C++11 及之后的新语法。

#### 强类型枚举 `enum class`

三处关键枚举：

```cpp
enum class InputMode { TOUCH, OPENCV, AI };          // Game.hpp
enum class GestureType { OPEN_PALM, FIST, UNKNOWN }; // GestureData.hpp
enum class Mineral::Type { GOLD_SMALL, ... };        // Mineral.hpp
```

避免了传统 `enum` 的命名空间污染，编译器帮助检查类型错误。

#### `auto` 与范围 for

```cpp
auto* levelLoader = LevelLoader::getInstance();
for (const auto& minDef : levelDef->minerals) { ... }
for (Node* subNode : goldsLayout->getChildren()) { ... }
```

#### Lambda 表达式 + 捕获

Cocos 事件系统里大量使用 `[=]` 捕获。例子见 [Game.cpp:88-92](file:///d:/GOLD%20MINER/goldminer-game/Classes/MainScene/Game.cpp#L88-L92)：

```cpp
bompButton->addTouchEventListener([=](Ref* ref, Widget::TouchEventType type) {
    if (type == Widget::TouchEventType::ENDED) this->detonateBomb();
});
```

手势指令回调 [Game.cpp:545-558](file:///d:/GOLD%20MINER/goldminer-game/Classes/MainScene/Game.cpp#L545-L558)：

```cpp
fusion->setCommandCallback([this](const GestureCommand& cmd) {
    if (cmd.shouldReleaseHook && !ropeChangeing && !isOpenHook) { ... }
    if (cmd.shouldDetonateBomb) { this->detonateBomb(); }
});
```

#### 右值语义与 `std::move`

```cpp
jpegFrame = std::move(data.jpegFrame);   // 转移帧数据所有权
setCommandCallback(std::move(cb));       // 转移回调所有权
std::move(response)                      // 闭包捕获时移动（AIGestureRecognizer）
```

避免不必要的深拷贝，提升多线程环境下的内存效率。

#### `std::nothrow` 布局 new

[Game.cpp:603-614](file:///d:/GOLD%20MINER/goldminer-game/Classes/MainScene/Game.cpp#L603-L614) 中创建 `Image` / `Texture2D`：

```cpp
Image* img = new (std::nothrow) Image();
if (!img || !img->initWithImageData(jpegFrame.data(), jpegFrame.size())) {
    delete img;
    return;
}
```

#### `= default` / `= delete`

```cpp
virtual ~IGestureRecognizer() = default;   // 合成默认析构
GestureFusion() = default;                 // 合成默认构造
```

### 2.3 模板、STL 容器与算法

#### 容器

| 容器 | 用途 | 例子 |
|---|---|---|
| `std::vector<T>` | 列表（矿物、关卡、历史、玩家名） | `std::vector<MineralDef> minerals`、`std::vector<HistoryEntry> history`、`std::vector<std::string> players`、`std::vector<uint8_t> jpegFrame` |
| `std::string` | 字符串 | 解析 .csb 帧名、split 逗号列表 |
| `std::function<T>` | 函数包装器 | 结果回调、指令回调 |

#### STL 算法

- `std::string::find / substr / npos` — 拆分逗号分隔玩家列表（[PlayerManager.cpp:29-36](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/PlayerManager.cpp#L29-L36)）、拆分 `.csb` 文件名。
- `std::chrono::steady_clock::now()` — 记录 FIST 触发时间（[GestureFusion.hpp:83](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/GestureFusion.hpp#L83)）。
- `std::put_time + std::tm + localtime_s` — 生成 ISO 时间戳（[PlayerManager.cpp:124-129](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/PlayerManager.cpp#L124-L129)）。

### 2.4 内存管理与资源生命周期

#### Cocos 的引用计数

`new + init + autorelease` 两阶段构造：

```cpp
Game* Game::create(...) {
    Game* pRet = new Game();
    if (pRet && pRet->init(...)) {
        pRet->autorelease();
        return pRet;
    } else {
        delete pRet;
        return nullptr;
    }
}
```

#### C++ 原生手动管理

`std::nothrow new` + 错误路径上 `delete`；跨线程共享的 `std::vector<uint8_t>` 通过 `std::move` 转移所有权。

#### scope guard（`std::shared_ptr` deleter 技巧）

[AIGestureService.cpp:123-125](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/AIGestureService.cpp#L123-L125) 中，无论线程正常结束还是中途 return，都能把原子标志位重置：

```cpp
std::shared_ptr<void> guard(nullptr, [this](void*) {
    _requestInFlight = false;
});
```

这是一种**RAII 模式**：把"清理动作"塞进 `shared_ptr` 的删除器，生命周期绑定到 `guard` 变量本身，离开作用域时自动执行。

#### 引擎资源释放

Texture / Image 使用 `->release()` 手动递减引用计数（[Game.cpp:632-633](file:///d:/GOLD%20MINER/goldminer-game/Classes/MainScene/Game.cpp#L632-L633)）。

### 2.5 多线程并发编程

这是项目**另一个重点收获**。手势识别全部在独立线程运行，主线程只负责游戏渲染。

#### `std::thread`

三个识别器都是 `std::thread` + 成员函数：

```cpp
// LocalGestureRecognizer 轮询线程
_thread = std::thread(&LocalGestureRecognizer::pollThread, this);

// AIGestureRecognizer 推理线程
_thread = std::thread(&AIGestureRecognizer::inferenceLoop, this);

// AIGestureService 单次异步请求（detach）
std::thread([...]{ ... }).detach();
```

#### `std::atomic`

跨线程共享的布尔/标志位必须用原子变量，避免数据竞争：

```cpp
std::atomic<bool> _running{false};
std::atomic<bool> _fistPendingCloud{false};
std::atomic<bool> _requestInFlight{false};
```

`GestureFusion` 用 `std::atomic<bool> _fistPendingCloud` 标记"FIST 已锁定、正等待云端确认"，由 UI 线程写、识别线程读，无锁即可安全。

#### `std::mutex` + `std::lock_guard`

保护更复杂的共享状态（结构体、帧数据）：

```cpp
mutable std::mutex _localMutex;
mutable std::mutex _cloudMutex;
mutable std::mutex _pendingMutex;

// 写入侧
{
    std::lock_guard<std::mutex> lock(_localMutex);
    _latestLocal = result;
}
// 读取侧
GestureResult local;
{
    std::lock_guard<std::mutex> lock(_localMutex);
    local = _latestLocal;
}
```

三个识别器 + GestureFusion 一共有 **7 个 mutex**，都是用来保护同一份数据在"识别线程写 + 游戏线程读"时不冲突。

#### 生产者-消费者模型

识别器线程是**生产者**（产出 `GestureResult`），GestureFusion 和 Game 是**消费者**。融合器再做一层 EMA 平滑和状态机，最终产出 `GestureCommand` 交给游戏逻辑。

#### Cocos 主线程切换

识别器在 worker thread 拿到 AI 响应后，用 `Director::getInstance()->getScheduler()->performFunctionInCocosThread(...)` 切回 Cocos 主线程再操作 UI/回调（[AIGestureRecognizer.cpp:242-300](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/AIGestureRecognizer.cpp#L242-L300)）。这是**"任何涉及引擎对象的操作都必须在主线程"**的铁则。

### 2.6 设计模式

#### 策略模式（核心）

`IGestureRecognizer` 抽象接口 + 三种实现类。`GestureFusion::initialize()` 里根据参数挑选识别器，`Game::switchInputMode()` 里切换策略。

优点：新增第四种识别器只需实现接口，不修改任何现有代码（**开闭原则**）。

#### 单例模式

几乎所有 Manager 类都是"私有构造 + 静态 getInstance"：

```cpp
static GestureFusion* s_fusionInstance = nullptr;
GestureFusion* GestureFusion::getInstance() {
    if (!s_fusionInstance) s_fusionInstance = new GestureFusion();
    return s_fusionInstance;
}
```

包括：GestureFusion、PlayerManager、LevelLoader、LocalGestureRecognizer、CloudGestureRecognizer、AIGestureRecognizer、UserDataManager、MusicPlayer、SoundTool。

#### 工厂模式（两阶段构造）

Cocos 推荐的 `static xxx* create(...)` + `bool init(...)` + `autorelease()`。`Game::create`、`Mineral::create`、`Scene::createScene` 都遵循这个模式。

#### 观察者模式 / 回调

- Cocos 事件系统（`EventListenerPhysicsContact`、`EventListenerTouchOneByOne`、`EventMouse`、`EventCustom("nextStage")`）
- 识别器的 `ResultCallback`（识别器 → 融合器）
- 融合器的 `CommandCallback`（融合器 → Game）

#### 双通道混合架构（自主设计）

```
LocalGestureRecognizer（高频瞄准，≈30Hz）
  + CloudGestureRecognizer（低频 FIST 确认）
         ↓
      GestureFusion（混合 + EMA 平滑 + 状态机）
         ↓
       GestureCommand（释放 / 角度 / 引爆）
```

### 2.7 网络与外部库集成

#### libcurl

手写 HTTP POST 请求豆包大模型。完整流程见 [AIGestureRecognizer.cpp:172-239](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/AIGestureRecognizer.cpp#L172-L239)：

```cpp
CURL* curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_URL, frameUrl.c_str());
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, &frameResp);
curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 500L);
curl_easy_perform(curl);
curl_easy_cleanup(curl);
```

第二跳请求豆包 API 时还要拼 `Content-Type: application/json` 和 `Authorization: Bearer xxx` 头。

#### RapidJSON

项目里 JSON 出现三次：

- **关卡数据**：`Resources/level_data.json` → LevelLoader
- **玩家档案持久化**：PlayerManager（RapidJSON 序列化 + UserDefault）
- **AI 配置**：`ai_config.json` → Game::switchInputMode 读取模型名和 API Key
- **AI 请求体构建**：AIGestureRecognizer / AIGestureService 构建豆包 ChatCompletion 请求
- **AI 响应解析**：C++ 字符串里找第一个 `{` 和最后一个 `}` 截取 JSON 片段再 parse（因为大模型有时会包 markdown 代码块）

#### Base64 手写实现

[AIGestureRecognizer.cpp:24-39](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/AIGestureRecognizer.cpp#L24-L39) 用纯 C++ 写了 Base64 编码（位运算 + 查表），没有依赖第三方库。

#### OpenCV

- `cv::VideoCapture` 开摄像头（在独立的 `GestureServer/main.cpp` 中）
- `cv::Mat` 颜色空间转换（[MatToTexture.cpp](file:///d:/GOLD%20MINER/goldminer-game/Classes/utils/MatToTexture.cpp)，BGR→RGB→RGBA）
- `findContours / convexHull / moments` 做轮廓与凸包分析（在 Python server.py 中）

### 2.8 游戏引擎（Cocos2d-x）专用知识

这部分不是标准 C++，但对项目来说是必须掌握的：

| 主题 | 用到的能力 |
|---|---|
| 场景/节点/层级 | `Scene::createWithPhysics()`、`Layer`、`Node`、`addChild` |
| 物理引擎 | `PhysicsWorld`、`PhysicsBody::createCircle/createEdgeBox`、`PhysicsContact`、ContactTestBitmask、EventListenerPhysicsContact |
| UI 系统 | `Button`、`Text`、`ImageView`、`Layout`、`Helper::seekWidgetByName`、`TouchEventType` |
| Action 系统 | `RotateTo`、`Sequence`、`RepeatForever`、`Spawn`、`MoveTo`、`ScaleTo`、`CallFuncN` |
| 调度器 | `schedule(selector, interval, repeat, delay)`、`unschedule`、`Director::getInstance()->getScheduler()->performFunctionInCocosThread` |
| CSLoader | 读取 .csb 编辑器二进制：`CSLoader::createNode`、`createTimeline` |
| 资源 | `SpriteFrameCache`、`FileUtils::fullPathForFilename`、`FileUtils::getStringFromFile`、`UserDefault` |
| 音频 | `CocosDenshion::SimpleAudioEngine`（SoundTool / MusicPlayer） |

### 2.9 跨模块编程实践

#### 数据结构协议

三个核心 struct 是整个服务层的"语言"：

- `GestureType` / `GestureResult` / `GestureCommand` — [GestureData.hpp](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/GestureData.hpp)
- `LevelDef` / `MineralDef` — [LevelLoader.hpp](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/LevelLoader.hpp)
- `PlayerProfile` / `HistoryEntry` — [PlayerManager.hpp](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/PlayerManager.hpp)

#### 依赖方向

```
Game.cpp
  ├─ GestureFusion ─┬─ LocalGestureRecognizer ─┐
  │                 ├─ CloudGestureRecognizer  ├─ libcurl + RapidJSON
  │                 └─ AIGestureRecognizer ─────┘
  ├─ GestureClient (HTTP 客户端)
  ├─ LevelLoader (RapidJSON + FileUtils)
  ├─ PlayerManager (RapidJSON + UserDefault)
  └─ Cocos 引擎模块
```

上层 include 下层，下层不 include 上层，**没有循环依赖**。

#### 全局常量

统一放在 [Const.hpp](file:///d:/GOLD%20MINER/goldminer-game/Classes/Other/Const.hpp)（如 `kWinSizeWidth`、`kWinSizeHeight`）。

#### 配置驱动

- 关卡：`level_data.json`（可随时加关卡）
- AI 凭据：`ai_config.json`（不硬编码）
- UI/动画：`.csb` 二进制（Cocos Studio 可视化编辑器导出）
- 资源：`.plist` 图集 + `.mp3 / .wav` 音频

#### 调试日志

每个模块都加了 `[ModuleName]` 前缀的 `CCLOG`，例如：

```cpp
CCLOG("[GestureFusion] Dual-channel hybrid architecture");
CCLOG("[AIGesture] Sending frame to %s (image=%d bytes b64)", ...);
CCLOG("[LevelLoader] Loaded %d levels from JSON", ...);
```

---

## 3. 总体架构与模块连接

### 3.1 架构分层

```
┌──────────────────────────────────────────────────────────────┐
│                      Game 场景层                              │
│  Game / MainRoot / Shop / Pause / StageTipsLayer             │
│  （管理游戏状态、UI、输入分发）                                │
└─────────────────────┬────────────────────────────────────────┘
                      │ GestureCommand / setCommandCallback
┌─────────────────────┴────────────────────────────────────────┐
│                    GestureFusion 融合器                       │
│  （EMA 平滑 + FIST 状态机 + 双通道混合 + 冷却）                │
└──────┬──────────────────────────┬───────────────────────────┘
       │ ResultCallback            │ ResultCallback
┌──────┴──────────────────┐  ┌───┴─────────────────────┐
│ LocalGestureRecognizer   │  │ CloudGestureRecognizer   │
│ （OpenCV 本地高频检测）  │  │（豆包大模型低频校验）     │
└──────┬───────────────────┘  └──┬───────────────────────┘
       │ HTTP                    │ HTTP (curl)
┌──────┴───────────────────┐  ┌──┴───────────────────────┐
│ GestureServer.exe (独立)  │  │ 豆包 API (云端)           │
│（OpenCV 摄像头 + 手势）  │  └───────────────────────────┘
└──────────────────────────┘
```

AI 模式是另一条独立管线：

```
GestureFusion.initializeAI()
  └─ AIGestureRecognizer
      ├─ GestureServer.exe (仅采集 JPEG 帧)
      └─ 推理线程 (curl → 豆包 Vision API → 角度/手势 → 回调)
```

### 3.2 模块职责清单

| 模块 | 单例 | 线程 | 职责 |
|---|---|---|---|
| Game | 否（场景实例） | 主线程 | 游戏主循环、输入分发、物理碰撞、关卡流程 |
| Mineral | 否（实体） | 主线程 | 矿物属性（分值、速度、转角） |
| IGestureRecognizer | 接口 | — | 统一识别器接口 |
| LocalGestureRecognizer | 是 | worker | 轮询本地 GestureServer 的 /gesture 端点 |
| CloudGestureRecognizer | 是 | worker | 把帧发给豆包 API 做二次校验 |
| AIGestureRecognizer | 是 | worker | 直连豆包 Vision（采集 + 识别合一） |
| GestureFusion | 是 | 主线程 tick | 混合双通道结果 → GestureCommand |
| GestureClient | 是 | 内部线程 | HTTP 通信客户端 |
| AIGestureService | 是 | 单次 detach | 封装 Base64 + curl + RapidJSON 的通用服务 |
| PlayerManager | 是 | 主线程 | 多玩家档案（RapidJSON + UserDefault） |
| LevelLoader | 是 | 主线程 | 关卡 JSON 加载 |
| MusicPlayer / SoundTool | 是 | 主线程（音频在后台线程） | 播放 BGM / 音效 |
| MatToTexture | 静态方法 | 任何线程都可 | cv::Mat → Cocos Texture2D |

### 3.3 数据流向

```
摄像头 → JPEG 帧
         │
    ┌────┴──────┐
    │           │
    ▼           ▼
GestureServer  AI 模式（直连大模型）
    │                    │
    ▼                    ▼
GestureClient      AIGestureRecognizer
    │                    │
    ▼                    ▼
LocalRecognizer    AIGestureRecognizer (不同实例)
    │           │        │
    └─────┬─────┘        │
          ▼              │
     GestureFusion ◄─────┘
          │ tick(dt)
          ▼
    GestureCommand (shouldReleaseHook, targetAngle, ...)
          │ setCommandCallback
          ▼
       Game.cpp
    ├─ OPEN_PALM → rope->setRotation(targetAngle)
    ├─ FIST 短按(<1.5s) → 释放钩子
    └─ FIST 长按(≥1.5s) → 引爆炸药
          │
          ▼ (碰撞)
    Mineral（查表判定类型/分值/速度）
          │
          ▼
    加分动画 → 更新金币 → PlayerManager 存档 → Shop → 下一关
```

---

## 4. 核心算法流程

### 4.1 游戏主循环（TOUCH 模式）

```
Game::startGame()
  ├─ EventListenerTouchOneByOne → touchCallBack()
  │   └─ 点屏幕 → 停止摇摆动画 → schedule(addRopeHeight, 0.025)
  ├─ EventListenerMouse → onRightMouseClick() → detonateBomb()
  └─ schedule(updateTime, 1s, repeat=59, delay=0)  // 60 秒倒计时

每 25ms:
  addRopeHeight(dt) → ropeHeight += 10 → rope->setContentSize(3, ropeHeight)
  （向下放绳）

PhysicsContact（钩子触碰到矿物）:
  physicsBegin() → pullGold()
    ├─ 创建 Mineral（name+scale 查表 → score / backSpeed / hookRote）
    ├─ 把 Mineral 加到 middleCircle（跟随钩子移动）
    └─ 切换为 schedule(subRopeHeight, 0.025)  // 开始收绳

每 25ms:
  subRopeHeight(dt) → ropeHeight -= backSpeed
    └─ ropeHeight ≤ 20 时:
        ├─ 停收绳
        ├─ 恢复摇摆动画
        └─ 如果钓着东西:
            ├─ 加分动画（Spawn(MoveTo + ScaleTo) + CallFuncN 回调）
            ├─ curStageScore += Mineral.score
            └─ Mineral->removeFromParent()

倒计时到 0:
  stopGame() → 比较 passScroe
    ├─ 够分 → StageTipsLayer(SUCCESS) → Shop → 下一关
    └─ 不够 → StageTipsLayer(FAIL) → 重玩
```

目标分数公式（旧版兜底）：

```cpp
passScroe = 650 + 275 * (stage - 1) + 410 * (stage - 1);
```

JSON 关卡则直接读取 `level_data.json` 里的 `targetMoney`。

### 4.2 手势识别流程（OpenCV 双通道模式）

```
Game::switchInputMode(OPENCV)
  └─ GestureFusion::initialize("")
      ├─ LocalGestureRecognizer::start()
      │   └─ std::thread pollThread()
      │       └─ 每 ~30ms: GET localhost:5000/gesture
      │           └─ 得到 { gesture, angle, stable, locked }
      │               └─ 回调 → GestureFusion::onLocalResult()
      └─ CloudGestureRecognizer::start()  // endpointId/key 空则跳过

Game::updateGestureAngle(dt) 每 50ms:
  GestureFusion::tick(dt):
    ├─ 读 _latestLocal（mutex 保护）
    ├─ EMA 平滑: _gameAngle = 0.7 * _gameAngle + 0.3 * local.angle
    ├─ FIST 上升沿 → _fistHoldTime = 0
    ├─ FIST 持续   → _fistHoldTime += dt
    ├─ FIST 下降沿（手张开）:
    │   ├─ 0.8s < t < 1.5s 且（云端置信度>0.85 或云端不可用）→ shouldReleaseHook
    │   └─ 否则忽略
    ├─ FIST 持续 ≥ 1.5s → shouldDetonateBomb
    └─ 冷却 0.8s 防连发

Game 收到 GestureCommand:
  shouldReleaseHook → 触发 touchCallBack 同款逻辑（放绳）
  shouldDetonateBomb → detonateBomb()（右键同款）
```

### 4.3 角度映射（AI 模式）

```
大模型返回:
  {
    "gesture": "open_palm" | "fist" | "unknown",
    "position": -1.0 ~ +1.0,       ← 手部 X 位置（归一化）
    "confidence": 0.0 ~ 1.0
  }

转换为物理角度:
  angle = clamp(position * 65.0f, -65, +65)

判定:
  open_palm + confidence > 0.7  → isStable = true，持续瞄准
  fist      + confidence > 0.7  → isLocked  = true，握拳计时
  else                          → 维持当前状态（防抖）
```

### 4.4 矿物类型判定表（Mineral::init）

`Mineral::init` 是一张由 `sprite帧名 + scale百分比` 组成的查表：

| sprite | scale | type | score | backSpeed | hookRote |
|---|---|---|---|---|---|
| gold-0-0.png | 40 | GOLD_SMALL | 100 × _power | 3 | 16° |
| gold-0-0.png | 65 | GOLD_MEDIUM | 200 × _power | 2 | 36° |
| pulled-gold-0-0.png | 90 | GOLD_LARGE | 400 × _power | 1.5 | 35° |
| stone-0.png | 80 | STONE_SMALL | 25 × _stoneCoe | 3 | 15° |
| stone-1.png | 100 | STONE_MEDIUM | 50 × _stoneCoe | 2 | 30° |
| stone-0.png | 150 | STONE_LARGE | 75 × _stoneCoe | 1.5 | 30° |
| diamond.png | — | DIAMOND | 500 × _diamondsCoe | 3 | 6° |
| treasure-bag.png | — | TREASURE_BAG | rand()%200 + 50 | 3 | 5° |

商店道具效果：

- 力量药水（200金）→ `_power = 1.2`，所有物品回收速度提升
- 钻石升值书（300金）→ `_diamondsCoe = 3`，钻石价值三倍
- 矿石收藏书（100金）→ `_stoneCoe = 3`，石头也能卖钱
- 炸药（150金）→ 抓到重物可引爆

### 4.5 关卡加载（双通道）

```
Game::loadStageInfo():
  1. 优先 JSON (LevelLoader)
     └─ level_data.json → 遍历 minerals[] → 每个矿物 create Sprite + PhysicsBody
  2. 兜底 CSB 关卡文件
     └─ level1.csb ~ level5.csb 循环 → 遍历 goldPanel 子节点加 PhysicsBody
```

### 4.6 存档系统

```
PlayerManager（RapidJSON + UserDefault）:
  档案 = { name, allMoney, stageNum, history[] }
  saveProfile()  → UserDefault.setStringForKey("player_profile_X", JSON).flush()
  loadProfile()  → UserDefault.getStringForKey() → RapidJSON parse
  玩家列表 = UserDefault.getStringForKey("player_index")  （逗号分隔）
  history 只保留最近 50 条（erase begin 清理最旧）

UserDataManager（旧系统，UserDefault 直接存取标量）:
  UserDefault::getInstance()->getIntegerForKey("allMoney") / getStageNum()
```

### 4.7 抓钩摇摆动画

```cpp
rope->runAction(RepeatForever::create(
    Sequence(
        RotateTo::create(1, +65),
        RotateTo::create(1, 0),
        RotateTo::create(1, -65),
        RotateTo::create(1, 0),
        NULL
    )
));
```

周期 4 秒，最大摆角 ±65°。手势模式用 `rope->setRotation(_gestureAngle)` 逐帧平滑替代。

---

## 5. 踩过的坑与解决方法

### 坑 1：跨线程访问 Cocos 对象崩溃

**现象**：在识别器 worker thread 里直接 `Sprite::create()` 或 `setTexture()`，偶发崩溃。

**原因**：Cocos 不是线程安全的，引擎对象必须在主操作。

**解决**：所有 UI 操作切回主线程：

```cpp
Director::getInstance()->getScheduler()
    ->performFunctionInCocosThread([this, response = std::move(response)]() {
        // 在主线程里 parse JSON、回调结果
    });
```

### 坑 2：识别频率过高触发 API 限流 + 游戏卡顿

**现象**：每帧都想发给云端，API 调用爆 429，游戏渲染也因主线程等 curl 响应卡顿。

**解决**：
- curl 放在 **worker thread**，主线程不阻塞
- 云端只在 **FIST 锁定时** 才请求（OPEN_PALM 完全不走云端）
- 推理间隔 400ms（`std::this_thread::sleep_for(std::chrono::milliseconds(400))`）
- curl 超时短（500ms / 3s / 2s），避免网络卡整个游戏

### 坑 3：竞态条件——两个 FIST 触发重叠

**现象**：玩家快速握拳-张开-握拳，偶发钩子没放或连放两次。

**解决**：
- FIST 短按区间 0.8s < t < 1.5s，防抖动
- **冷却 0.8s**（`COOLDOWN_DURATION = 0.8f`），两次释放钩子必须间隔
- 用 `std::atomic<bool> _fistPendingCloud` + `std::mutex _pendingMutex` 保护"待云端确认"状态

### 坑 4：大模型返回 markdown 包裹的 JSON

**现象**：`{"choices":[{"message":{"content":"```json {...} ```"}}]}`，直接 parse 失败。

**解决**：手动 find 第一个 `{` 和最后一个 `}` 截取纯 JSON 再 parse（[AIGestureRecognizer.cpp:253-257](file:///d:/GOLD%20MINER/goldminer-game/Classes/services/AIGestureRecognizer.cpp#L253-L257)）。

### 坑 5：多线程共享 `_latestLocal` 不加锁

**现象**：偶发崩溃或读到半更新的 `GestureResult` 结构体。

**解决**：每个 recognizer 一个 mutex，write 侧和 read 侧都必须 `std::lock_guard<std::mutex>`。

### 坑 6：`curl_easy_init` / `curl_easy_cleanup` 泄漏

**现象**：长时间运行后内存上涨。

**解决**：每次请求结束必 `curl_easy_cleanup(curl)` + `curl_slist_free_all(headers)`；RAII scope guard（shared_ptr deleter）保证异常/提前 return 时也执行。

### 坑 7：Mineral 查表里 scale 是浮点数不能精确比较

**现象**：`name == "gold-0-0.png" && scale == 40` 有些情况下永远进不去。

**解决**：取整 `int scale = (int)(scaleX * 100);` 再比较整数。

---

## 6. 如果重新开始会怎么做

如果再写一次，我会做这些改进：

1. **Mineral 查表**：现在是一个巨型 `if/else`（[Mineral.cpp:19-93](file:///d:/GOLD%20MINER/goldminer-game/Classes/gameobjects/Mineral.cpp#L19-L93)，30+ 分支）。应该改成：

   ```cpp
   struct MineralStats { Type type; int score; float backSpeed; float hookRote; Vec2 offset; };
   std::unordered_map<std::string, MineralStats> table = { ... };
   ```

   这样加新矿物类型只要加一行。

2. **减少单例**：现在 9 个 getInstance() 单例其实可以注入。比如 Game 构造时传一个 ServiceContext 引用，里面持有 fusion、levelLoader、playerManager 指针，更容易写单元测试。

3. **手势识别用回调链**：现在每个识别器都手动 thread + mutex + atomic，可以封装一个"线程安全的异步识别器基类"，三个实现只关心"如何识别"，不关心线程模型。

4. **关卡数据驱动矿物布局**：现在 Mineral 有个兜底 CSB 关卡 + 硬编码 scale 的查表。改成 level_data.json 里直接定义每个矿物的 type/x/y/scale，Mineral 不做任何判断只负责渲染和返回 stats。

5. **手势融合器拆成独立模块**：GestureFusion 现在有 AI 模式 + 双通道模式两套逻辑。可以用"装饰器模式"——对 LocalRecognizer 套一个 CloudVerifierDecorator，或直接用 RecognizerSelector 做策略切换。

6. **存档改成二进制**：RapidJSON + UserDefault 够用但升级难。可以换 SQLite 或直接 `fwrite` 自定义格式。

7. **手势识别测试**：应该有单元测试（mock 掉摄像头，构造 GestureResult 序列）来验证 FIST 状态机、冷却、EMA 系数的正确性。

---

## 7. 结语：C++ 学习心得

做完这个项目，C++ 学习最大的收获不是某个 API，而是**对"怎么把东西搭起来"的整体感觉**。

具体地说：

1. **C++ 真的没有魔法**。STL 容器、mutex、atomic、thread、shared_ptr 这些东西全是"让你自己声明意图，编译器/运行时帮你一点点"。不像 Python/JS 自动帮你做这么多。代价是写的时候想多写几行，好处是你对"每一份内存、每一个线程、每一次锁"都有明确的直觉。

2. **多线程不是 `#include <thread>` 就完了**。线程只是"执行的并行"，"状态的一致"全靠你自己 mutex + atomic + 生命周期管理。踩过"忘记加锁崩溃"和"忘记 reset atomic 第二次 FIST 无效"之后，对"每个共享变量都要有明确所有权"这件事会非常警惕。

3. **现代 C++（C++11 及之后）真的好用**。`enum class`、`auto`、lambda、`std::function`、`std::move`、`= default` 让代码简洁很多。但代价是要理解"右值是什么"、"shared_ptr deleter 能做 scope guard"这种高级技巧，需要持续学习。

4. **设计模式不是玄学**。策略模式解决了"三种识别器怎么切换"；单例模式解决了"全局唯一状态"；观察者模式/回调解决了"识别器怎么通知上层"；工厂模式解决了"Cocos 的两阶段构造"。这些都在项目里有实际代码可对照。

5. **外部库集成是一门学问**。libcurl + RapidJSON + OpenCV + Cocos 引擎全混在一起时，编译错误会让人崩溃。理解 `CMakeLists.txt`、include 路径、链接顺序、不同库的生命周期（Cocos autorelease vs C++ 原生 delete vs libcurl curl_easy_cleanup）是必须掌握的。

6. **"能跑起来"和"长期维护"是两件事**。Mineral 那个巨型 if/else、3 种手势识别器 200 行 Fusion 逻辑、PlayerManager 里 RapidJSON 手动读写——每个都能跑，但每次改都要非常小心。加注释、加日志、抽象出小模块会让自己少掉很多头发。

7. **做"和硬件互动 + 网络请求 + 游戏循环"的项目最爽**。单纯写算法题看不到 mutex/thread、看不到 libcurl、看不到 RapidJSON。而当你把"摄像头 → 本地识别 → 云端确认 → 融合器 → 游戏指令 → 物理碰撞 → 加分动画"这一整条链路用 C++ 串起来跑通时，那种成就感是写 LeetCode 无法比拟的。

C++ 学习没有捷径。最有效的方式就是**选一个真有内容的小项目，全程用 C++ 实现，不逃避任何一个困难**。

---

> 报告整理于 2026 年 6 月，基于项目 `goldminer-game/` 当前代码。
