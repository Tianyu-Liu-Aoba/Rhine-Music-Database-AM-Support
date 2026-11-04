# RHINE MUSIC DATABASE

**Apple Music 遥控终端 · 正在播放可视化 · 本地曲库播放**

> 本项目由 **[MikeQu223/Rhine-Music-Database](https://github.com/MikeQu223/Rhine-Music-Database)** 修改而来。
> 原始设计、界面与绝大部分代码均来自原作者，本分支只增加了 Apple Music 相关能力与遥控形态的界面调整。
> 请优先支持原作者的项目。

这是对《明日方舟》特别映像「莱茵生命：访问」终端界面的非官方复刻，现在被改造成一台**面向"正在播放"的播放终端**：
在 Apple Music 桌面应用里选曲，这个面板负责显示、遥控、歌词与视觉。

使用 **Qt 6 / C++ / QRhi（Vulkan · D3D12 · Metal）** 构建。

![正在播放](Assets/previews/panel.png)

## 它是什么

```
┌──────────────────────────────────────────────────────────┐
│  RHINE LAB                 打开 Apple Music ↗   设置      │
│                                                          │
│   [ 3D 唱片场景 ]              NOW PLAYING / 正在播放     │
│                                 BOW AND ARROW            │
│                                 Kenshi Yonezu            │
│                                 BOW AND ARROW            │
│                                                          │
│            ⏮ ⏯ ⏭  ─────●────  01:00 / 02:55   词  队列   │
└──────────────────────────────────────────────────────────┘
```

- **主页**只回答一个问题：现在在放什么
- **词页**给出大封面与逐行滚动歌词（当前行高亮，可显示翻译）
- **底栏**只有播放控制、进度、「词」与「队列」

## 功能

### Apple Music

- [x] 公开目录检索：歌曲、专辑、艺人
- [x] 专辑与艺人详情、发行日期、封面
- [x] **30 秒试听**（Apple 公开预览片段）
- [x] 公开榜单（热门歌曲 / 热门专辑）
- [x] 商店地区跟随系统区域，可在设置中覆盖
- [x] **镜像并遥控** Apple Music 桌面应用：播放 / 暂停 / 上一首 / 下一首 / 拖动进度
- [x] 读取官方应用的「播放中队列」，点击条目跳转
- [x] 唱片封面跟随正在播放的曲目
- [x] 一键跳转 Apple Music 应用

### 歌词

- [x] 三源并行：网易云 → QQ 音乐 → LRCLIB，先到先用
- [x] 译文与原文按时间戳对齐，独立字号排版
- [x] 罗马音标题反查原文标题
- [x] 以 Apple Music 播放器显示的时间为基准自动校准偏移
- [x] 手动「歌词偏移」微调

### 通用

- [x] 本地曲库播放（FFmpeg 音频后端）
- [x] 独立后台播放服务：界面退出或崩溃时停止播放
- [x] Vulkan / D3D12 / Metal 三套 3D 后端，含 FSR / 时域抗锯齿
- [x] 面板设置：渲染后端、帧生成、地区、歌词偏移

### 明确不做

- [ ] 整曲播放：Apple 音频受 FairPlay 保护，第三方无法解密
- [ ] 个人资料库 / 收藏 / 推荐：Apple 未向第三方开放免费接口
- [ ] MV、评论、电台、音乐下载

## 运行要求

| 项目 | 要求 |
| --- | --- |
| 系统 | Windows 10/11 x64（主要目标）；macOS、Linux 可构建 |
| 图形 | Vulkan 或 D3D12（Windows），Metal（macOS），OpenGL 回退 |
| Apple Music | 需要安装 **Apple Music 桌面应用**并登录，遥控功能依赖它 |
| 网络 | 公开目录与歌词需要联网；试听需要访问 `*.apple.com` |

> 遥控功能读取的是 **Windows 系统媒体会话（SMTC）** 与 **UI Automation**，
> 不登录 Apple 账号、不读取凭据、不修改你的音乐资料库。

## 构建

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物位于 `output/`。Windows 上若使用 MSYS2 工具链，可用 UCRT64 环境：

```sh
pacman -S mingw-w64-ucrt-x86_64-qt6 mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

## 测试

```sh
ctest --test-dir build --output-on-failure
```

离线回归覆盖 Apple 目录映射、分页边界、播放动作路由与歌词匹配规则，不访问网络：

```sh
./build/RhineAppleMusicRegression
# Apple Music offline regression passed: 30 checks
```

## 排障

| 现象 | 说明 |
| --- | --- |
| 面板显示「未在播放」 | Apple Music 应用没在播放，或已被系统媒体会话移除 |
| 队列为空 | 需要打开 Apple Music 的「播放中」面板（迷你播放器优先） |
| 歌词缺失 | 三个歌词源都没有收录；可点「重新查找歌词」 |
| 歌词偏移 | 用底栏「歌词偏移」微调，偏移会全局保存 |
| 没有声音 | 试听只播放 30 秒片段；整曲请在 Apple Music 中播放 |

启动日志里的 `Display: <平台插件> backend: <后端>` 与 `media-session` / `apple-op` 行可用于定位问题，
设置环境变量 `RHINE_MEDIA_SESSION_LOG=1` 可打开详细诊断。

## 致谢

- **原始项目：[MikeQu223/Rhine-Music-Database](https://github.com/MikeQu223/Rhine-Music-Database)**
  —— 本仓库是它的修改版本，界面设计、渲染架构与全部基础功能都归原作者所有
- 界面原型：《明日方舟》「莱茵生命：访问」特别映像（非官方复刻）
- 歌词：[LRCLIB](https://lrclib.net)、网易云音乐、QQ 音乐
- 音频：FFmpeg；图形：Qt QRhi

## 许可

本项目为个人学习用非官方复刻，与应用内涉及的音乐服务无隶属关系。
Apple Music、Apple、iTunes 是 Apple Inc. 的商标。所有音乐版权归各自权利人所有。
