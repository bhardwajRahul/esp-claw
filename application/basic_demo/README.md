# ESP-Clawgent

**事件驱动的 OpenClaw，为嵌入式场景量身定制。**

`ESP-Clawgent` 是一套运行于 ESP32 上的事件驱动 AI assistant，适用于需要长期运行、可持续扩展、功能持续迭代的设备侧 AI 场景

- 事件驱动：不局限于消息输入，多种外部事件都可以触发 Agent Loop
- 组件化：可按需裁切模块
- 离线自动化：在无网络条件下执行本地规则
- 自编程：内嵌 Lua 解释器，可由 AI 自主规划实现功能
- 渐进式工具披露：当前 agent 能做什么，由已加载和已激活的 skills 决定
- 超低资源占用：ESP32-C 系列上也能流畅运行

## 1. What is ESP-Clawgent

项目包含示例与基础功能组件。

- `application/basic_demo/`：当前主应用，用于将这些模块装配成可运行固件

`components/` 包含以下核心部分：

- `claw_core`：负责完整的 agent 执行流
- `claw_cap`：负责能力注册、调度和工具调用
- `claw_memory`：负责会话历史和长期记忆
- `claw_skill`：负责技能加载、技能文档和激活态技能上下文
- `claw_event_router`：负责标准事件接入、规则路由、Agent/脚本分发和出站路由
- `cap_*`：负责拓展具体能力，比如 QQ、Telegram、文件、时间、Web Search、MCP、Lua 等

## 2. How it works

主启动入口位于 `application/basic_demo/main/main.c`。

设备启动后，整体流程如下：

1. 初始化 NVS，加载设备配置
2. 挂载 FATFS 到 `/fatfs/data`
3. 初始化 Wi-Fi 和本地 HTTP 配置服务
4. 进入 `app_clawgent_start()`
5. 初始化 event router、memory、skills、capabilities
6. 初始化并启动 `claw_core`
7. 启动 CLI，开始响应请求和事件

当前运行时依赖以下本地目录：

- `/fatfs/data/sessions`：会话历史
- `/fatfs/data/memory/MEMORY.md`：长期记忆
- `/fatfs/data/skills`：skills 文档和清单
- `/fatfs/data/lua`：Lua 脚本
- `/fatfs/data/automation/automations.json`：自动化规则
- `/fatfs/data/inbox`：消息附件存储目录

### Event-driven

`ESP-Clawgent` 的核心是“收到事件，触发行为”。

事件可以来自：

- 即时通讯入口
- 配置界面
- 本地事件规则
- 文件或附件输入
- 后续扩展的 cap 事件源

行为可以是：

- 调模型
- 调工具
- 读写本地文件
- 执行 Lua
- 触发事件路由链路
- 给外部 IM 回消息

### Progressive tool exposure

工具不会默认一次性全部暴露给模型。

`claw_core` 在运行时会接入这些上下文提供器：

- 长期记忆
- 会话历史
- skills list
- 已激活 skill 的文档
- 当前 cap 工具描述

初始情况下，仅暴露由 `skills_list.json` 声明的基础能力。

- skill 没加载，模型看不到对应能力的说明
- skill 没激活，模型拿不到对应文档上下文
- 当前会话能做什么，是逐步展开的


### Self-programming with Lua

项目内嵌 Lua 解释器，支持 Lua 脚本编辑与运行。

这使得以下能力成为可能：

- 把设备逻辑写成 Lua
- 把某些 agent 行为抽成脚本
- 让 assistant 在现有能力之上继续组合出新的功能

许多扩展可以先在 Lua 层完成，无需重新烧录固件。只要向 LLM 提出具体需求，系统即可直接生成并实现对应功能。

## 3. Project architecture

仓库结构如下：

```text
esp-clawgent-master/
├── components/
│   ├── claw_core/
│   ├── claw_cap/
│   ├── claw_event_router/
│   ├── claw_memory/
│   ├── claw_skill/
│   └── cap_*/
└── application/
    └── basic_demo/
        ├── main/
        └── build/
```

### Runtime layers

- Application layer
  负责启动、配置、文件系统、Wi-Fi、HTTP 配置页和整机装配
- Core layer
  `claw_core` 负责 agent 执行流
- cap layer
  `claw_cap` 和各类 `cap_*` 负责工具和具体能力实现
- Event routing layer
  `claw_event_router` 负责统一事件入口、规则匹配、脚本/agent 调度和出站分发
- Memory layer
  `claw_memory` 负责持久化上下文
- Skill layer
  `claw_skill` 负责按 skill 控制上下文与功能暴露

### Current capabilities

当前 `basic_demo` 已接入的能力包括：

- `cap_im_qq`
- `cap_im_tg`
- `cap_files`
- `cap_lua`
- `cap_mcp_client`
- `cap_mcp_server`
- `cap_skill`
- `cap_time`
- `cap_llm_inspect`
- `cap_web_search`

### Design style

项目的架构关键词包括：

- event-driven
- componentized
- local-first

联网并不是系统运行的唯一前提。无网络时，本地事件路由、Lua、文件系统和既有记忆仍可继续工作。

## 4. Quick Start

### Prerequisites

- ESP-IDF 环境已安装并导出
- 建议使用 `ESP-IDF v5.5.1`
- 默认目标芯片为 `esp32s3`

```bash
. /esp-idf/export.sh
```

### Build

所有 ESP-IDF 命令都在 `application/basic_demo/` 下执行：

```bash
cd application/basic_demo
idf.py set-target esp32s3
idf.py build
```

### Configure

当前 Demo 的关键配置包括：

- Wi-Fi SSID / Password
- LLM API Key / Provider / Model
- QQ App ID / App Secret
- Telegram Bot Token
- Brave / Tavily Search Key
- Timezone

可以通过 `menuconfig` 调整编译期默认值：

```bash
cd application/basic_demo
idf.py menuconfig
```

设备运行后，配置也会通过 NVS 持久化。

### Notes on Keys

- IM bot token：可通过 Telegram 的 [@BotFather](https://t.me/BotFather) 或 [QQ Bot](https://q.qq.com/qqbot/openclaw/login.html) 获取
- LLM API key：可使用 [Anthropic Console](https://console.anthropic.com)、[OpenAI Platform](https://platform.openai.com) 或 [阿里云百炼](https://bailian.console.aliyun.com/#/api-key) 提供的 Key


### Flash

```bash
cd application/basic_demo
idf.py flash monitor
```

如果串口不是默认值：

```bash
cd application/basic_demo
idf.py flash monitor -p /dev/ttyUSB0
```

### First boot

首次启动后，通常会看到以下阶段：

- NVS 初始化
- FATFS 挂载
- 设置加载
- Wi-Fi 和本地配置服务启动
- memory / skills / capabilities 初始化
- `claw_core` 启动
- CLI 启动
