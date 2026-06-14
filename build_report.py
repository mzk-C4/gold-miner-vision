import markdown
import re

# Read the C++ report
with open(r'd:\GOLD MINER\docs\C++学习报告.md', 'r', encoding='utf-8') as f:
    md = f.read()

# Add emoji prefixes to sections for visual appeal
emoji_map = {
    '## 1. 项目概述': '## 🎮 1. 项目概述',
    '## 2. 用到的 C++ 知识': '## 🧠 2. 用到的 C++ 知识',
    '### 2.1 面向对象基础': '### 🏗️ 2.1 面向对象基础',
    '### 2.2 C\\+\\+11/14/17 现代特性': '### ⚡ 2.2 C++11/14/17 现代特性',
    '### 2.3 模板、STL 容器与算法': '### 📦 2.3 模板、STL 容器与算法',
    '### 2.4 内存管理与资源生命周期': '### 💾 2.4 内存管理与资源生命周期',
    '### 2.5 多线程并发编程': '### 🔀 2.5 多线程并发编程',
    '### 2.6 设计模式': '### 🎯 2.6 设计模式',
    '### 2.7 网络与外部库集成': '### 🌐 2.7 网络与外部库集成',
    '### 2.8 游戏引擎': '### 🕹️ 2.8 游戏引擎（Cocos2d-x）专用知识',
    '### 2.9 跨模块编程实践': '### 🔗 2.9 跨模块编程实践',
    '## 3. 总体架构与模块连接': '## 🏛️ 3. 总体架构与模块连接',
    '## 4. 核心算法流程': '## ⚙️ 4. 核心算法流程',
    '## 5. 踩过的坑与解决方法': '## 🐛 5. 踩过的坑与解决方法',
    '## 6. 如果重新开始会怎么做': '## 🔄 6. 如果重新开始会怎么做',
    '## 7. 结语：C\\+\\+ 学习心得': '## ✨ 7. 结语：C++ 学习心得',
    '#### 类与对象': '#### 🔷 类与对象',
    '#### 继承与多态': '#### 🔶 继承与多态',
    '#### 访问控制、友元、静态成员': '#### 🔐 访问控制、友元、静态成员',
    '#### 强类型枚举': '#### 🏷️ 强类型枚举 `enum class`',
    '#### `auto` 与范围 for': '#### 🔄 `auto` 与范围 for',
    '#### Lambda 表达式': '#### 🎣 Lambda 表达式 + 捕获',
    '#### 右值语义与': '#### 📤 右值语义与 `std::move`',
    '#### `std::nothrow`': '#### 🛡️ `std::nothrow` 布局 new',
    '#### `= default` / `= delete`': '#### ✂️ `= default` / `= delete`',
    '#### 容器': '#### 🗃️ 容器',
    '#### STL 算法': '#### 🔍 STL 算法',
    '#### Cocos 的引用计数': '#### ♻️ Cocos 的引用计数',
    '#### C\\+\\+ 原生手动管理': '#### 📐 C++ 原生手动管理',
    '#### scope guard': '#### 🔒 Scope Guard（RAII 模式）',
    '#### 引擎资源释放': '#### 🗑️ 引擎资源释放',
    '#### `std::thread`': '#### 🧵 `std::thread`',
    '#### `std::atomic`': '#### ⚛️ `std::atomic`',
    '#### `std::mutex`': '#### 🔐 `std::mutex` + `std::lock_guard`',
    '#### 生产者-消费者模型': '#### 🏭 生产者-消费者模型',
    '#### Cocos 主线程切换': '#### 🔄 Cocos 主线程切换',
    '#### 策略模式': '#### ♟️ 策略模式（核心）',
    '#### 单例模式': '#### 1️⃣ 单例模式',
    '#### 工厂模式': '#### 🏭 工厂模式（两阶段构造）',
    '#### 观察者模式': '#### 👀 观察者模式 / 回调',
    '#### 双通道混合架构': '#### 🌀 双通道混合架构（自主设计）',
    '#### libcurl': '#### 🌊 libcurl',
    '#### RapidJSON': '#### 📋 RapidJSON',
    '#### Base64 手写实现': '#### 🔢 Base64 手写实现',
    '#### OpenCV': '#### 👁️ OpenCV',
    '#### 数据结构协议': '#### 📜 数据结构协议',
    '#### 依赖方向': '#### 🧭 依赖方向',
    '#### 全局常量': '#### 📌 全局常量',
    '#### 配置驱动': '#### ⚡ 配置驱动',
    '#### 调试日志': '#### 📝 调试日志',
}

for old, new in emoji_map.items():
    md = md.replace(old, new)

# Convert to HTML
html_body = markdown.markdown(md, extensions=['tables', 'fenced_code', 'codehilite', 'toc', 'nl2br'])

# Read main page for CSS
with open(r'd:\GOLD MINER\web-landing\index.html', 'r', encoding='utf-8') as f:
    main_html = f.read()

style_match = re.search(r'<style>(.*?)</style>', main_html, re.DOTALL)
css = style_match.group(1) if style_match else ''

extra_css = '''
        .report-container { max-width: 900px; margin: 0 auto; padding: 120px 24px 100px; }
        .report-container h1 { font-size: 2.2em; background: linear-gradient(180deg,#FFD700,#B8860B); -webkit-background-clip:text; -webkit-text-fill-color:transparent; margin-bottom: 12px; text-align:center; }
        .report-container h2 { font-size:1.5em; color:#FFD700; margin-top:48px; margin-bottom:16px; border-bottom:1px solid rgba(255,215,0,0.15); padding-bottom:10px; }
        .report-container h3 { font-size:1.2em; color:#e8c860; margin-top:32px; margin-bottom:12px; }
        .report-container h4 { color:#c0a040; margin-top:22px; font-size:1.05em; }
        .report-container p { margin-bottom:14px; line-height:1.9; font-size:0.98em; }
        .report-container ul, .report-container ol { margin:12px 0 22px 24px; }
        .report-container li { margin-bottom:8px; line-height:1.8; }
        .report-container table { width:100%; border-collapse:collapse; margin:18px 0; background:rgba(15,21,32,0.6); border:1px solid rgba(255,215,0,0.12); border-radius:8px; overflow:hidden; font-size:0.94em; }
        .report-container th { background:rgba(255,215,0,0.08); color:#FFD700; padding:12px 16px; text-align:left; font-weight:500; }
        .report-container td { padding:10px 16px; border-top:1px solid rgba(255,215,0,0.06); color:#c0b898; }
        .report-container code { background:rgba(255,215,0,0.06); color:#e8c860; padding:2px 7px; border-radius:4px; font-size:0.9em; font-family:'Cascadia Code','Fira Code','Consolas',monospace; }
        .report-container pre { background:#0a0f1a; border:1px solid rgba(255,215,0,0.12); border-radius:8px; padding:18px 22px; overflow-x:auto; margin:18px 0; font-size:0.85em; line-height:1.65; }
        .report-container pre code { background:none; color:#c8c0a0; padding:0; }
        .report-container blockquote { border-left:3px solid #B8860B; padding:10px 18px; margin:18px 0; background:rgba(255,215,0,0.03); color:#a09880; border-radius:0 6px 6px 0; }
        .report-container a { color:#4a90d9; text-decoration:none; }
        .report-container a:hover { text-decoration:underline; }
        .report-container hr { border:none; border-top:1px solid rgba(255,215,0,0.08); margin:40px 0; }
        .report-container strong { color:#e8d080; }
        .report-container em { color:#c0a060; }
        .report-container img { max-width:100%; border-radius:8px; }
        .report-container .highlight { background:linear-gradient(90deg, rgba(255,215,0,0.12), transparent); padding:2px 0; }
        /* Hero subtitle */
        .report-hero { text-align:center; margin-bottom:40px; padding-bottom:30px; border-bottom:1px solid rgba(255,215,0,0.08); }
        .report-hero p { color:#8a8070; font-size:1.05em; max-width:600px; margin:0 auto; }
        .report-hero .badge-row { display:flex; gap:12px; justify-content:center; flex-wrap:wrap; margin-top:16px; }
        .report-hero .badge { display:inline-flex; align-items:center; gap:6px; padding:6px 14px; background:rgba(255,153,51,0.08); border:1px solid rgba(255,153,51,0.2); border-radius:50px; font-size:0.85em; color:#f0a040; }
        /* Nav */
        .nav-bar { position:fixed; top:0; left:0; right:0; z-index:100; background:rgba(8,12,20,0.92); backdrop-filter:blur(12px); border-bottom:1px solid rgba(255,215,0,0.1); padding:14px 24px; display:flex; align-items:center; gap:24px; }
        .nav-bar a { color:#e8e0d0; text-decoration:none; font-size:0.95em; transition:color 0.2s; }
        .nav-bar a:hover { color:#FFD700; }
        .nav-bar .nav-title { font-weight:700; color:#FFD700; font-size:1.1em; }
        .nav-bar .nav-spacer { flex:1; }
        /* Back to top */
        .back-top { position:fixed; bottom:30px; right:30px; width:44px; height:44px; background:rgba(255,215,0,0.15); border:1px solid rgba(255,215,0,0.3); border-radius:50%; display:flex; align-items:center; justify-content:center; cursor:pointer; font-size:1.2em; transition:all 0.2s; z-index:50; color:#FFD700; }
        .back-top:hover { background:rgba(255,215,0,0.25); transform:translateY(-3px); }
'''

hero_html = '''
        <div class="report-hero">
            <div class="badge-row">
                <span class="badge">🖥️ C++17</span>
                <span class="badge">🎮 Cocos2d-x 4.0</span>
                <span class="badge">👁️ OpenCV</span>
                <span class="badge">🤖 AI 大模型</span>
                <span class="badge">🧵 多线程</span>
                <span class="badge">📐 设计模式</span>
            </div>
        </div>
'''

html = f'''<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>C++ 学习报告 — 黄金矿工手势识别版</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Noto+Sans+SC:wght@300;400;500;700;900&display=swap');
        {css}
        {extra_css}
    </style>
</head>
<body>
    <div class="cave-bg"><img src="images/hero-cave.jpg" alt=""></div>
    <div class="cave-bg-overlay"></div>
    <div class="particles" id="particles"></div>
    <nav class="nav-bar">
        <a href="/" class="nav-title">⛏️ 黄金矿工</a>
        <a href="/">← 返回主页</a>
        <span class="nav-spacer"></span>
        <span style="color:#8a8070;font-size:0.85em">C++ 课程设计学习报告</span>
    </nav>
    <div class="report-container">
        {hero_html}
        {html_body}
    </div>
    <a href="#" class="back-top" title="回到顶部">↑</a>
    <script>
        const pc=document.getElementById('particles');
        for(let i=0;i<30;i++){{const p=document.createElement('div');p.className=Math.random()>0.3?'particle':'particle spark';p.style.left=Math.random()*100+'%';p.style.animationDuration=(8+Math.random()*15)+'s';p.style.animationDelay=Math.random()*10+'s';pc.appendChild(p);}}
        document.querySelector('.back-top').addEventListener('click',e=>{{e.preventDefault();window.scrollTo({{top:0,behavior:'smooth'}});}});
    </script>
</body>
</html>'''

with open(r'd:\GOLD MINER\web-landing\cpp-report.html', 'w', encoding='utf-8') as f:
    f.write(html)
print(f'Generated: {len(html):,} chars')
