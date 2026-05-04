# C++大作业讲解视频录制计划

## 黄金矿工游戏项目 - 讲解大纲

---

## 一、项目背景与目标（建议时长：2-3分钟）

### 1.1 项目简介
- 项目名称：黄金矿工（Cocos2d-x + OpenCV手势识别版）
- 项目类型：C++面向对象编程大作业
- 开发环境：Visual Studio + Cocos2d-x 3.x + OpenCV

### 1.2 项目目标
- 实现经典黄金矿工游戏的核心玩法
- 集成OpenCV手势识别，实现手势控制
- 应用C++面向对象特性设计模块化架构

### 1.3 技术栈概览
| 技术 | 说明 |
|------|------|
| Cocos2d-x | 游戏引擎，处理渲染与场景管理 |
| OpenCV | 摄像头捕获、手势识别 |
| C++17 | 现代C++特性（智能指针、lambda、thread） |
| 单例模式 | 管理器类设计 |

---

## 二、项目架构与文件结构（建议时长：2-3分钟）

### 2.1 目录结构
```
Classes/
├── MainScene/           # 游戏主场景
│   ├── Game.hpp/cpp     # 游戏核心逻辑
│   ├── MainRoot.hpp/cpp # 主菜单场景
│   ├── Shop.hpp/cpp     # 商店系统
│   └── StageTipsLayer   # 关卡提示
├── gameobjects/          # 游戏对象
│   └── Mineral.hpp/cpp  # 矿物类（矿石、钻石等）
├── services/            # 服务层
│   ├── GestureClient.hpp/cpp   # 手势客户端
│   └── GestureRecognizer.hpp/cpp # OpenCV手势识别
└── Other/               # 工具类
    ├── UserDataManager.hpp/cpp # 用户数据管理
    └── AppDelegate.hpp/cpp     # 应用程序入口
```

### 2.2 核心类关系图
```
AppDelegate
    └── MainRootScene
            ├── GameScene (游戏主逻辑)
            │       ├── Mineral (矿物对象)
            │       ├── GestureClient (手势输入)
            │       └── Hook (钩子物理)
            └── ShopScene (商店系统)
                    └── UserDataManager (数据持久化)
```

---

## 三、核心功能模块讲解（建议时长：10-12分钟）

### 3.1 游戏核心逻辑 - Game类（约4分钟）

**关键代码片段1：输入模式枚举**
```cpp
enum class InputMode { TOUCH, OPENCV, AI };
```
**讲解要点**：强类型枚举的优势、避免魔法字符串

**关键代码片段2：状态机设计**
```cpp
// 钩子状态管理
bool isOpenHook;  // 是否正在抓取
bool ropeChangeing; // 绳索是否在变化
int ropeHeight;     // 绳索长度
```
**讲解要点**：状态机的概念、游戏对象的状态管理

**关键代码片段3：物理碰撞检测**
```cpp
bool Game::physicsBegin(PhysicsContact &contact) {
    if (contact.getShapeB()->getBody()->getNode()->getTag() != kWorldTag) {
        this->pullGold(contact);
    }
}
```
**讲解要点**：Cocos2d-x物理引擎、碰撞检测机制

### 3.2 矿物对象系统 - Mineral类（约3分钟）

**关键代码片段：枚举类型定义**
```cpp
enum class Type {
    GOLD_SMALL,
    GOLD_MEDIUM,
    GOLD_LARGE,
    STONE_SMALL,
    STONE_MEDIUM,
    STONE_LARGE,
    DIAMOND,
    TREASURE_BAG
};
```
**讲解要点**：
- 强类型枚举的命名约定
- 工厂模式与create方法
- 对象属性设计（score, power, backSpeed）

**关键代码片段：属性修饰**
```cpp
bool potion;      // 力量药水效果
bool diamonds;    // 钻石升值效果
bool stoneBook;   // 矿石收藏书效果
int _stoneCoe;    // 石头系数
int _diamondsCoe; // 钻石系数
```
**讲解要点**：策略模式在实际游戏中的应用

### 3.3 手势识别系统（约3分钟）

**关键代码片段1：多线程设计**
```cpp
std::thread _thread;
std::atomic<bool> _running{false};
std::mutex _frameMutex;
```
**讲解要点**：
- C++11多线程编程
- atomic与mutex的区别
- 线程安全的临界区保护

**关键代码片段2：回调函数设计**
```cpp
using GestureCallback = std::function<void(Gesture)>;
void setCallback(GestureCallback cb) { _callback = std::move(cb); }
```
**讲解要点**：
- std::function的灵活性
- std::move避免拷贝开销
- 回调模式解耦模块

**关键代码片段3：手势枚举**
```cpp
enum class Gesture {
    NONE,
    OPEN_PALM,  // 张开手掌 - 放钩子
    FIST        // 握拳 - 准备
};
```
**讲解要点**：强类型枚举的优势

---

## 四、C++语言特性应用（建议时长：5-6分钟）

### 4.1 面向对象特性

**封装与类设计**
- 私有成员变量的命名约定（下划线前缀）
- getter/setter方法的简洁写法
- 构造函数的初始化列表

### 4.2 智能指针与内存管理
```cpp
// Cocos2d-x的autorelease机制
static Game* create(bool isBuyBomb, ...) {
    auto game = new Game();
    if (game->init(...)) {
        game->autorelease();
        return game;
    }
    return nullptr;
}
```
**讲解要点**：Cocos2d-x的内存管理机制、new/delete vs autorelease

### 4.3 Lambda表达式
```cpp
startBtn->addTouchEventListener([=](Ref *sender, Widget::TouchEventType type) {
    if (type == Widget::TouchEventType::ENDED) {
        // 处理点击事件
    }
});
```
**讲解要点**：Lambda捕获列表、简洁的回调写法

### 4.4 命名空间与作用域
```cpp
namespace cocos2d {
    class Sprite;
    class Scene;
}
```
**讲解要点**：避免命名冲突、模块化代码组织

---

## 五、设计模式应用（建议时长：3-4分钟）

### 5.1 单例模式
**UserDataManager单例实现**
```cpp
static UserDataManager *_instance = nullptr;

UserDataManager* UserDataManager::getInstance() {
    if (_instance == nullptr) {
        _instance = new UserDataManager();
    }
    return _instance;
}
```
**讲解要点**：
- 延迟初始化
- 线程安全问题（后续可扩展）
- 全局访问点

### 5.2 工厂模式
**Mineral创建工厂**
```cpp
static Mineral* create(const std::string& name, float scaleX, float scaleY,
                       float rotate, bool potion, bool diamonds, bool stoneBook);
```
**讲解要点**：对象创建的封装、参数化配置

### 5.3 观察者模式
**手势回调机制**
```cpp
class GestureRecognizer {
public:
    using GestureCallback = std::function<void(Gesture)>;
    void setCallback(GestureCallback cb) { _callback = std::move(cb); }
};
```
**讲解要点**：解耦事件源与处理者

---

## 六、关键代码解析（建议时长：4-5分钟）

### 6.1 游戏主循环
```cpp
void Game::updateTime(float dt) {
    timeCount--;
    time->setString(StringUtils::toString(timeCount));
    if (timeCount <= 0) {
        stopGame();
    }
}
```
**讲解要点**：定时器机制、游戏结束判定

### 6.2 钩子摇摆动画
```cpp
void Game::startShakeHookAnimation() {
    float duration = 1;
    float angle = 65;
    rope->runAction(RepeatForever::create(
        Sequence::create(
            RotateTo::create(duration, angle),
            RotateTo::create(duration, 0),
            RotateTo::create(duration, -angle),
            RotateTo::create(duration, 0),
            nullptr
        )
    ));
}
```
**讲解要点**：Cocos2d-x动画系统、Action序列组合

### 6.3 矿物抓取逻辑
```cpp
void Game::pullGold(PhysicsContact &contact) {
    if (!isOpenHook) {
        this->isOpenHook = true;
        // 计算抓取后的速度和位置
        this->schedule(SEL_SCHEDULE(&Game::subRopeHeight), 0.025);
    }
}
```
**讲解要点**：物理引擎回调、状态转换

---

## 七、测试与验证（建议时长：2-3分钟）

### 7.1 功能测试点
- [ ] 钩子左右摆动正常
- [ ] 点击屏幕释放钩子
- [ ] 碰撞检测正确识别矿物
- [ ] 分数累加正确
- [ ] 关卡切换逻辑
- [ ] 商店购买流程

### 7.2 手势识别测试
- [ ] 摄像头正常打开
- [ ] 手掌张开识别
- [ ] 握拳识别
- [ ] 模式切换

### 7.3 测试用例示例
```cpp
void testMineralCreation() {
    auto mineral = Mineral::create("gold_small", 1.0f, 1.0f, 0, false, false, false);
    CCASSERT(mineral != nullptr, "Mineral creation failed");
    CCASSERT(mineral->score == 200, "Score mismatch");
}
```

---

## 八、遇到的问题与解决方案（建议时长：2-3分钟）

### 8.1 问题1：内存泄漏
**问题描述**：频繁切换场景导致内存持续增长
**解决方案**：遵循Cocos2d-x的autorelease机制，避免手动new/delete

### 8.2 问题2：多线程数据竞争
**问题描述**：OpenCV线程与主线程同时访问帧数据
**解决方案**：使用mutex保护临界区，使用atomic标记状态

### 8.3 问题3：跨平台兼容性
**问题描述**：Windows命名管道与Linux不兼容
**解决方案**：抽象出GestureClient接口，支持PIPE/HTTP两种模式

### 8.4 问题4：OpenCV头文件编译
**问题描述**：OpenCV 4.x与旧版本API差异
**解决方案**：条件编译 `#ifdef HAS_OPENCV`

---

## 九、代码规范与工程实践（建议时长：1-2分钟）

### 9.1 命名规范
- 类名：首字母大写，驼峰命名（GameScene）
- 方法名：首字母小写，驼峰命名（initGame）
- 成员变量：下划线前缀（_instance, _running）
- 常量：全大写，下划线分隔（MAX_SCORE）

### 9.2 头文件保护
```cpp
#ifndef Game_hpp
#define Game_hpp
// ...
#endif
```

### 9.3 注释规范
- 类说明注释
- 关键算法注释
- TODO/FIXME标记

---

## 十、总结与扩展（建议时长：1-2分钟）

### 10.1 项目亮点总结
1. 模块化设计，代码结构清晰
2. 多种输入模式支持（触摸/手势/AI）
3. C++11/17新特性的应用
4. 跨平台架构设计

### 10.2 可扩展方向
- AI模式：接入云端手势识别API
- 多人模式：网络对战
- 更多关卡：关卡编辑器
- 音效系统：背景音乐与音效控制

### 10.3 学习收获
- 面向对象设计思想
- 游戏引擎架构理解
- C++工程实践经验
- 跨学科整合能力（CV+游戏）

---

## 十一、视频录制建议

### 11.1 技术准备
- 屏幕录制软件（OBS/OBS Studio）
- 代码高亮插件
- 演示PPT/思维导图

### 11.2 讲解技巧
| 要点 | 建议 |
|------|------|
| 语速 | 中等偏慢，重点处停顿 |
| 重点 | 关键代码高亮+放大 |
| 节奏 | 每10-15分钟休息一次 |
| 互动 | 结尾留Q&A时间 |

### 11.3 分段录制
建议将视频分为4-5段：
1. 开场与项目介绍（5分钟）
2. 核心代码讲解（15分钟）
3. C++特性分析（10分钟）
4. 问题与解决方案（5分钟）
5. 总结与演示（5分钟）

---

## 十二、参考资源

- Cocos2d-x官方文档：https://docs.cocos.com/cocos2d-x/
- OpenCV官方教程：https://docs.opencv.org/
- C++11标准参考：https://en.cppreference.com/
- 设计模式：GoF《设计模式》

---

**文档版本**：v1.0
**创建日期**：2026-05-03
**适用项目**：黄金矿工（Cocos2d-x + OpenCV手势识别版）
