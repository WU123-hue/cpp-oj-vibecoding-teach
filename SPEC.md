# 仿 LeetCode OJ 项目 — SPEC.md

> 由 Socratic 深度访谈生成。MVP 目标：单机跑通完整判题闭环，代码质量可展示，安全边界达标。

---

## 1. 项目定位与目标

| 维度 | 决策 |
|---|---|
| 定位 | 个人学习 / 技术练手项目 |
| 规模 | 单机够用，尽快跑通 MVP |
| 环境 | Linux 原生部署（rlimit/fork/exec 依赖 Linux 系统调用） |
| 用户角色 | 普通用户（看题+做题）、管理员（含题目增删改） |
| 语言 | MVP 仅 C++ |

### 成功标准（验收门槛）

1. **全链路可跑通**：注册登录 → 管理员加题 → 用户提交 → 看到 AC/WA 等五件套结果
2. **安全边界达标**：fork 炸弹 / 无限循环 / 超内存被正确拦截，不拖垮系统
3. **性能达标**：判题延迟 < 3s，前端响应 < 500ms，单机 10 并发不崩
4. **代码质量可展示**：分层清晰、可读，可作面试/作品展示

---

## 2. 功能范围（MVP）

### 必做
- 用户注册 / 登录（Session + Cookie）
- 角色区分：普通用户 / 管理员
- 题目列表页（含难度筛选、状态标签）
- 题目详情 + 做题页（左侧描述、右侧 CodeMirror、底部用例/结果面板）
- 提交判题（同步阻塞返回，五件套结果逐条可见）
- 自定义测试运行（用户自填输入，不计入判题，仅返回程序输出）
- 提交记录列表页（全局流，可点进查看代码与结果）
- 管理员后台（新增/编辑/删除题目 + 测试用例管理）
- 测试用例录入：表单逐条 + 打包文件上传 两种方式

### 下一阶段（非 MVP）
- seccomp 严格白名单
- 异步判题 + 轮询
- 多语言（C / Python / Java）
- 排行榜 / 竞赛 / 讨论区 / 题解

---

## 3. 技术栈与架构

### 技术栈
| 层 | 选型 | 理由 |
|---|---|---|
| 后端 | C++ + cpp-httplib | 单头文件依赖，轻量；贴合学习目标 |
| 数据库 | MySQL | 题目/用户/提交/测试用例结构化存储 |
| 判题沙箱 | fork/exec + rlimit | 原生 Linux 系统调用，零额外依赖；**MVP 不加 seccomp** |
| 前端 | 原生 HTML + CSS + JS + CodeMirror 5/6 | 轻量（~200KB），CDN 引入，支持 C++ 高亮/行号 |
| 认证 | Session + Cookie | 服务端 session，简单直接 |
| 日志 | 仅控制台输出 | MVP 不做持久化 |

### 架构图

```
┌─────────────────────────────────────────────────────────────┐
│                         浏览器（前端）                         │
│  原生 HTML/CSS/JS                     │
│  页面：登录 · 注册 · 题目列表 · 题目详情+做题 · 提交记录 · 后台   │
└───────────────────────────┬─────────────────────────────────┘
                            │ HTTP RESTful + JSON
                            │ 
┌───────────────────────────▼─────────────────────────────────┐
│              单一 C++ 服务进程（cpp-httplib）                  │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐ │
│  │  路由/中间件   │  │  业务逻辑层    │  │   判题模块（核心）  │ │
│  │  Session 鉴权  │  │  用户/题目/    │  │  Web 进程内         │ │
│  │  角色守卫      │  │  提交 CRUD    │  │  fork/exec + rlimit │ │
│  └──────────────┘  └──────────────┘  └─────────┬──────────┘ │
│         │                │                      │            │
│         └────────────────┴──────────────────────┘            │
│                            │                                 │
└────────────────────────────┼─────────────────────────────────┘
                             │
              ┌──────────────▼──────────────┐
              │          MySQL              │
              │  users / problems /         │
              │  submissions / test_cases   │
              └─────────────────────────────┘

判题时序（同步阻塞）：
  浏览器 POST /api/submissions
    → 服务端在 Web 进程内 fork 子进程
      → 子进程：setrlimit(CPU/MEM/FSIZE/NPROC) + exec 用户程序
      → 父进程：waitpid + 收集输出 + 比对
    → 同步返回每个用例的 {status, time, memory}
```

### 数据模型（MySQL）

```sql
-- 用户表
users(
  id BIGINT PK AUTO_INCREMENT,
  username VARCHAR(64) UNIQUE NOT NULL,
  password_hash VARCHAR(128) NOT NULL,   -- bcrypt/加盐哈希
  role ENUM('user','admin') DEFAULT 'user',
  created_at DATETIME
)

-- 题目表
problems(
  id BIGINT PK AUTO_INCREMENT,
  title VARCHAR(128) NOT NULL,
  description TEXT NOT NULL,             -- 支持 Markdown/HTML
  input_format TEXT,
  output_format TEXT,
  sample_input TEXT,                     -- 示例（展示用）
  sample_output TEXT,
  time_limit INT DEFAULT 1000,           -- ms，保守档 1000
  memory_limit INT DEFAULT 64,           -- MB，保守档 64
  difficulty ENUM('easy','medium','hard') DEFAULT 'easy',
  is_deleted TINYINT DEFAULT 0,
  created_at DATETIME
)

-- 测试用例表
test_cases(
  id BIGINT PK AUTO_INCREMENT,
  problem_id BIGINT NOT NULL,
  input TEXT NOT NULL,
  expected_output TEXT NOT NULL,
  is_sample TINYINT DEFAULT 0,           -- 是否示例用例（前端展示）
  created_at DATETIME,
  INDEX(problem_id)
)

-- 提交记录表
submissions(
  id BIGINT PK AUTO_INCREMENT,
  user_id BIGINT NOT NULL,
  problem_id BIGINT NOT NULL,
  code TEXT NOT NULL,
  language VARCHAR(16) DEFAULT 'cpp',
  status ENUM('AC','WA','TLE','RE','CE') NOT NULL,
  -- 逐用例详情：JSON 数组 [{case_id, status, time_ms, memory_kb}]
  case_results JSON,
  is_custom TINYINT DEFAULT 0,           -- 1=自定义测试，不统计
  error_message TEXT,                    -- CE 原文 / RE 信号名
  stdout TEXT,                           -- 自定义测试时的程序输出
  created_at DATETIME,
  INDEX(user_id), INDEX(problem_id), INDEX(created_at)
)
```

### 项目目录结构

按"分层 + 模块化"组织，后端 `src/` 内分 routes / middleware / controllers / models / dao / judge / utils / common 八层；前端 `static/` + `views/` 分离；配置与脚本置于根目录独立子目录。

```
cpp-oj-vibecoding-teach/
├── CMakeLists.txt                  # 顶层构建（cpp-httplib + MySQL client + pthread）
├── SPEC.md                         # 本规格说明
├── README.md                       # 部署、配置、使用说明
├── .gitignore                      # 忽略 build/ temp/ 配置敏感项
│
├── config/
│   └── config.json                 # 数据库连接、端口、判题资源限制等
│
├── sql/
│   └── schema.sql                  # 建库建表 + 预置初始管理员账号
│
├── third_party/
│   └── cpp-httplib/                # 第三方单头文件依赖（vendored）
│
├── src/                            # 后端 C++ 源码
│   ├── main.cpp                    # 程序入口：加载配置 → 初始化 DB → 启动服务
│   │
│   ├── server/                     # 服务启动与路由注册
│   │   ├── server.h
│   │   └── server.cpp
│   │
│   ├── routes/                     # 路由层（HTTP 端点绑定）
│   │   ├── auth_routes.{h,cpp}             # /api/auth/*  注册/登录/登出
│   │   ├── problem_routes.{h,cpp}          # /api/problems/*
│   │   ├── submission_routes.{h,cpp}       # /api/submissions、/api/run
│   │   └── admin_routes.{h,cpp}            # /api/admin/*
│   │
│   ├── middleware/                 # 中间件
│   │   ├── session.{h,cpp}                 # 解析 Cookie → 加载 session → 注入 user
│   │   └── auth_guard.{h,cpp}              # 角色守卫（admin 才能访问 /api/admin/*）
│   │
│   ├── controllers/                # 业务逻辑层
│   │   ├── user_controller.{h,cpp}         # 注册校验、密码哈希、登录 session
│   │   ├── problem_controller.{h,cpp}      # 题目 CRUD + 软删除
│   │   ├── submission_controller.{h,cpp}   # 提交判题编排、自定义测试
│   │   └── testcase_controller.{h,cpp}     # 用例表单录入 + 打包上传解析
│   │
│   ├── models/                     # 数据模型（POJO/结构体）
│   │   ├── user.{h,cpp}
│   │   ├── problem.{h,cpp}
│   │   ├── submission.{h,cpp}
│   │   └── test_case.{h,cpp}
│   │
│   ├── dao/                        # 数据访问层（MySQL）
│   │   ├── mysql_pool.{h,cpp}              # 连接获取/释放
│   │   ├── user_dao.{h,cpp}
│   │   ├── problem_dao.{h,cpp}
│   │   ├── submission_dao.{h,cpp}
│   │   └── test_case_dao.{h,cpp}
│   │
│   ├── judge/                      # 判题核心模块
│   │   ├── compiler.{h,cpp}                # 编译子进程（g++ + rlimit）
│   │   ├── runner.{h,cpp}                  # 运行子进程（fork + rlimit + 管道）
│   │   ├── sandbox.{h,cpp}                 # setrlimit 封装（CPU/MEM/FSIZE/NPROC/CORE）
│   │   ├── comparator.{h,cpp}              # 输出比对（去尾空白 + 精确匹配）
│   │   └── judge.{h,cpp}                   # 判题入口：编译→逐用例运行→归类五件套
│   │
│   ├── utils/                      # 工具
│   │   ├── crypto.{h,cpp}                  # bcrypt 加盐哈希
│   │   ├── file_utils.{h,cpp}              # 临时目录管理、zip/tar 解压与安全校验
│   │   ├── string_utils.{h,cpp}            # 去尾空白、JSON 转义等
│   │   └── uuid.{h,cpp}                    # /tmp/judge_<uuid> 命名
│   │
│   └── common/                     # 公共定义
│       ├── types.h                         # 状态枚举（AC/WA/TLE/RE/CE）、角色枚举
│       ├── config.{h,cpp}                  # 读取 config.json
│       └── json.{h,cpp}                    # JSON 序列化/反序列化封装
│
├── static/                         # 前端静态资源（由服务直接托管）
│   ├── css/
│   │   └── style.css                       # 全局样式 + 导航栏 + 各页面
│   ├── js/
│   │   ├── api.js                          # 公共 API 客户端（fetch 封装、401/403 处理）
│   │   ├── pages/
│   │   │   ├── login.js                    # P1 登录页逻辑
│   │   │   ├── register.js                 # P2 注册页逻辑（前端字段校验）
│   │   │   ├── problem_list.js             # P3 列表页（筛选、状态标签）
│   │   │   ├── problem_detail.js           # P4 做题页（CodeMirror + 结果面板）
│   │   │   └── admin.js                    # P5 后台（CRUD + 用例管理）
│   │   └── components/
│   │       └── editor.js                   # CodeMirror 封装（C++ 高亮/行号/缩进/重置）
│   └── vendor/
│       └── codemirror/                     # CodeMirror 5/6 本地化（避免 CDN 依赖）
│
├── views/                          # HTML 页面模板
│   ├── login.html                          # P1
│   ├── register.html                       # P2
│   ├── index.html                          # P3 题目列表
│   ├── problem.html                        # P4 题目详情+做题
│   └── admin.html                          # P5 管理员后台
│
└── tests/                          # 测试
    ├── judge_test.cpp                      # 判题模块单元测试
    └── data/                               # 测试用例样例（AC/WA/TLE/RE/CE 各类）
```

> 说明：
> - **routes** 仅负责 HTTP 收发与参数提取；**controllers** 承载业务规则；**dao** 隔离 SQL；**judge** 与业务层解耦，可独立测试。
> - `static/vendor/` 将 CodeMirror 本地化，便于离线/内网部署，不依赖公网 CDN。
> - `src/common/types.h` 统一定义五件套状态与角色枚举，供各层引用避免魔法字符串。

---

## 4. 核心机制详细规格

### 4.1 判题沙箱（fork/exec + rlimit）

```
编译阶段：
  g++ -O2 -w -std=c++17 -o /tmp/judge_<uuid>/solution solution.cpp
    （限 CPU 5s / 内存 256MB / 输出 16MB，防止编译指令/模板爆炸）

运行阶段（每个测试用例）：
  1. pipe() 建立 stdin/stdout/stderr 管道
  2. fork()
  3. 子进程：
       chdir 到 /tmp/judge_<uuid>/ 工作目录
       setrlimit(RLIMIT_CPU, 1)            -- CPU 1s（保守档）
       setrlimit(RLIMIT_AS, 64MB)          -- 内存 64MB（保守档）
       setrlimit(RLIMIT_FSIZE, 16MB)       -- 输出文件 16MB
       setrlimit(RLIMIT_NPROC, 0)          -- 禁止 fork（进程数 0）
       setrlimit(RLIMIT_CORE, 0)           -- 禁止 core dump
       dup2 重定向 stdin/stdout/stderr
       execve(solution, ...)
  4. 父进程：
       写入测试用例 input 到子进程 stdin
       waitpid()，记录 wall-clock 与子进程 status
       读子进程 stdout
  5. 判定（按优先级）：
       超时(>1.5x limit)           → TLE
       被信号杀死(SIGSEGV/SIGFPE…)  → RE + 信号名
       输出超过 RLIMIT_FSIZE        → RE（输出超限合并）
       输出对比不匹配              → WA
       否则                        → AC
```

### 4.2 判题结果五件套

| 状态 | 含义 | 触发条件 | 详情字段 |
|---|---|---|---|
| AC | Accepted | 全部用例输出匹配 | 无 |
| WA | Wrong Answer | 输出不匹配（去尾空白后精确比对） | 无 |
| TLE | Time Limit Exceeded | CPU 超 1s 或 wall 超 1.5s | 无 |
| RE | Runtime Error | 信号杀死 / 输出超限 | 信号名（如 SIGSEGV） |
| CE | Compile Error | g++ 非零退出 | 编译器原始 stderr 原文 |

### 4.3 输出比对规则
- 全量精确匹配
- 预处理：去除每行末尾空白、去除文末多余换行
- 处理后逐字节比对

### 4.4 自定义测试运行
- 用户在做题页填入自定义输入 → `POST /api/run`
- 走与判题相同的沙箱（1s/64MB 限制），但**不做输出比对**
- 返回：程序 stdout（截断至 64KB）+ 状态（AC/RE/TLE）
- 不写入 `submissions` 表或标记 `is_custom=1`

### 4.5 测试用例录入
- **表单逐条**：后台页面每条用例一对多行文本框（input / expected_output）
- **打包上传**：上传 zip/tar，服务端解压后按约定目录读取（`1.in`/`1.out`/`2.in`/`2.out`…）
  - 解压前校验：限制文件数 ≤ 100、单文件 ≤ 1MB、总大小 ≤ 10MB、禁止路径穿越（`..`）

### 4.6 注册与认证机制

#### 4.6.1 注册流程
1. 用户在注册页填写用户名、密码、确认密码
2. 前端做字段校验（长度、字符集、两次密码一致）
3. `POST /api/auth/register` 提交
4. 服务端校验：用户名唯一、长度/字符集合法、密码长度达标
5. 密码加盐哈希（bcrypt）后写入 `users` 表，`role` 默认 `user`
6. 注册成功返回用户信息（**不含密码**），前端引导跳转登录页
7. 注册接口**不创建管理员账号**；管理员由 `schema.sql` 预置数据初始化

#### 4.6.2 字段校验规则
| 字段 | 规则 |
|---|---|
| username | 3-32 字符；仅字母/数字/下划线；大小写敏感；全局唯一 |
| password | 6-64 字符；MVP 不强制复杂度（建议含字母+数字） |

#### 4.6.3 密码安全
- 存储：bcrypt 加盐哈希，**禁止明文存储**
- 日志：禁止打印密码明文
- 传输：MVP 走明文 HTTP（文档标注"仅学习环境，勿公网暴露"），下一阶段引入 HTTPS
- 注册/登录接口均加速率限制，防暴力枚举（MVP 可选，下一阶段必做）

#### 4.6.4 注册错误处理
| 场景 | HTTP | 响应 |
|---|---|---|
| 用户名已存在 | 409 | `{error: "username_taken"}` |
| 字段不合法 | 400 | `{error: "invalid_field", detail: ...}` |
| 注册成功 | 201 | `{user: {id, username, role}}` |

#### 4.6.5 登录与 Session
- `POST /api/auth/login` 校验密码哈希 → 创建 session → `Set-Cookie: session_id`（HttpOnly）
- Session 存内存（MVP），key=`session_id`，value=`{user_id, role, created_at}`
- 登出 `POST /api/auth/logout` 删除 session 并清除 Cookie
- Session 中间件：解析 Cookie → 加载 session → 注入 `user` 到请求上下文
- 角色守卫：`/api/admin/*` 校验 `role=admin`，否则 403

#### 4.6.6 初始管理员
- 通过 `schema.sql` 预置一个 admin 账号（默认凭据写入部署文档，首次登录可改）
- 注册接口不得创建 admin；`role` 字段仅管理员可在后台改（MVP 不开放此功能）

---

## 5. API 契约（RESTful + JSON，同步判题）

### 认证
| Method | Path | 说明 |
|---|---|---|
| POST | `/api/auth/register` | 注册 {username, password} → {user} |
| POST | `/api/auth/login` | 登录 → Set-Cookie: session_id |
| POST | `/api/auth/logout` | 登出，清除 session |

### 题目
| Method | Path | 权限 | 说明 |
|---|---|---|---|
| GET | `/api/problems` | 公开 | 列表（支持 ?difficulty=&status= 筛选） |
| GET | `/api/problems/:id` | 公开 | 题目详情（不含隐藏用例） |

### 提交与判题
| Method | Path | 权限 | 说明 |
|---|---|---|---|
| POST | `/api/submissions` | 登录 | 提交判题，**同步阻塞返回** {case_results[], status, error_message} |
| POST | `/api/run` | 登录 | 自定义测试运行，返回 {stdout, status, error_message} |
| GET | `/api/submissions` | 登录 | 提交记录列表（?user_id=&problem_id= 分页） |
| GET | `/api/submissions/:id` | 登录 | 提交详情（含代码） |

### 管理员后台
| Method | Path | 权限 | 说明 |
|---|---|---|---|
| POST | `/api/admin/problems` | admin | 新增题目 |
| PUT | `/api/admin/problems/:id` | admin | 编辑题目 |
| DELETE | `/api/admin/problems/:id` | admin | 软删除（is_deleted=1） |
| POST | `/api/admin/problems/:id/test_cases` | admin | 新增测试用例（表单） |
| POST | `/api/admin/problems/:id/test_cases/upload` | admin | 打包上传用例 |
| DELETE | `/api/admin/test_cases/:id` | admin | 删除用例 |

---

## 6. 前端页面

前端采用原生 HTML + CSS + JS，无框架；每个页面一个独立路由，按"页面模块 + 公共 API 客户端"组织。MVP 共需 5 类页面：

| # | 页面 | 路由 | 权限 | 要点 |
|---|---|---|---|---|
| P1 | **登录页** | `/login` | 公开 | 用户名+密码表单；提交 `POST /api/auth/login`；成功跳转 `/`；含"去注册"链接 |
| P2 | **注册页** | `/register` | 公开 | 用户名+密码+确认密码表单；前端字段校验；提交 `POST /api/auth/register`；成功跳转 `/login`；含"去登录"链接 |
| P3 | **题目列表页** | `/` | 公开（登录可选） | 表格列出题目（编号/标题/难度）；难度筛选（easy/medium/hard/all）；登录后显示该用户对该题的最近状态标签（AC/WA/未提交）；点击进入详情页；顶部导航含登录/注册或用户名+登出 |
| P4 | **题目详情 + 做题页** | `/problems/:id` | 登录 | 左侧题目描述（Markdown 渲染，含示例输入输出）；右侧 CodeMirror 编辑器（C++ 高亮/行号/缩进，可重置/默认模板）；底部用例与结果面板；含"提交（判题）"与"运行（自定义测试）"两按钮；提交后逐用例显示 {状态, 耗时, 内存}；CE/TLE/RE 显示对应信息 |
| P5 | **管理员后台页** | `/admin` | admin | 题目 CRUD（列表 + 新增/编辑表单，含标题/描述/输入输出格式/示例/时间内存限制/难度）；测试用例管理（表单逐条录入 + 打包 zip 上传切换）；软删除确认交互；仅 `role=admin` 可访问，普通用户访问跳转 `/` 并提示无权限 |

### 6.1 公共布局与导航
- 顶部导航栏（所有页面复用）：左 Logo/站名，中间题目列表入口，右侧根据登录状态显示「登录/注册」或「用户名 + 登出」；管理员额外显示「后台」入口
- 统一 API 客户端模块：封装 fetch，自动携带 Cookie，统一错误处理（401 跳登录、403 提示无权限）

### 6.2 路由与鉴权前置
- 前端路由：纯前端 hash 路由或 server 端返回对应 HTML 均可（MVP 选其一）
- 需登录页面（P4、P5）在进入时先校验登录态：未登录跳 `/login` 并带 `?redirect=` 回跳
- P5 额外校验 `role=admin`，否则跳 `/`

### 6.3 提交记录页（可选，MVP 可合并）
- 路由 `/submissions`，登录可见，全局提交流，分页 + 筛选；点击进入提交详情查看代码与逐用例结果
- MVP 可与题目详情页的结果面板合并展示，独立列表页为下一阶段增强

---

## 7. 风险与缓解策略

| 风险 | 严重度 | 缓解策略（MVP） |
|---|---|---|
| **安全风险（无 seccomp）** | 高 | rlimit 禁 fork（NPROC=0）+ 输出限 16MB + CPU/内存限；接受"可读任意文件/可发起网络"的已知缺口，文档标注"仅限学习环境，勿公网暴露"；下一阶段加 seccomp |
| **并发阻塞风险** | 高 | 同步判题会阻塞 Web 线程；cpp-httplib 多线程模式下每请求一线程兜底；判题设 3s 硬超时；后续可演进为独立判题进程/异步 |
| **资源耗尽风险** | 中 | rlimit FSIZE=16MB 限输出；工作目录用 tmpfs 或定时清理 /tmp/judge_*；打包上传严格限制文件数与大小 |
| **前端可维护性** | 中 | 原生 JS 按"页面模块 + 公共 API 客户端"组织；CodeMirror 单独封装；避免全局状态污染；下一阶段可引入构建工具 |
| **编译指令/模板爆炸** | 中 | 编译阶段单独 rlimit（CPU 5s / 内存 256MB / 输出 16MB） |

---

## 8. TODO 清单

### 阶段 0：基础设施
- [ ] 0.1 初始化 Git 仓库结构与 .gitignore（build/、temp/、配置敏感项）
- [ ] 0.2 CMakeLists.txt（cpp-httplib + MySQL client + pthread）
- [ ] 0.3 MySQL 建库建表脚本（schema.sql，含上述 4 张表）
- [ ] 0.4 配置文件读取（数据库连接、端口、判题资源限制等）

### 阶段 1：认证与用户
- [ ] 1.1 用户注册接口（密码 bcrypt 加盐哈希）
- [ ] 1.2 注册字段校验（用户名 3-32 字符/字母数字下划线、唯一；密码 6-64）
- [ ] 1.3 注册错误处理（用户名已存在 409、字段非法 400、成功 201）
- [ ] 1.4 用户登录接口（校验哈希 + 创建 session + Set-Cookie HttpOnly）
- [ ] 1.5 登出接口（删除 session + 清除 Cookie）
- [ ] 1.6 Session 中间件（解析 Cookie → 加载 session → 注入 user）
- [ ] 1.7 角色守卫中间件（admin 才能访问 /api/admin/*，否则 403）
- [ ] 1.8 schema.sql 预置初始管理员账号
- [ ] 1.9 登录页前端（表单 + 提交 + 跳转 + 错误提示）
- [ ] 1.10 注册页前端（表单 + 前端字段校验 + 提交 + 跳转登录）

### 阶段 2：题目模块
- [ ] 2.1 题目 CRUD 接口（管理员写、所有人读）
- [ ] 2.2 测试用例 CRUD 接口（表单逐条）
- [ ] 2.3 打包上传用例接口（zip/tar 解压 + 安全校验）
- [ ] 2.4 题目列表页前端（筛选、状态标签）
- [ ] 2.5 题目详情页前端骨架

### 阶段 3：判题核心
- [ ] 3.1 判题模块：编译子进程（g++ + rlimit）
- [ ] 3.2 判题模块：运行子进程（fork + rlimit + 管道重定向）
- [ ] 3.3 判题模块：waitpid + 信号/超时判定
- [ ] 3.4 判题模块：输出比对（去尾空白 + 精确匹配）
- [ ] 3.5 判题模块：五件套状态归类 + error_message 生成
- [ ] 3.6 提交判题接口（同步阻塞，逐用例结果）
- [ ] 3.7 自定义测试运行接口（/api/run）
- [ ] 3.8 临时工作目录管理（/tmp/judge_<uuid>，判后清理）

### 阶段 4：提交记录
- [ ] 4.1 提交记录列表接口（分页、筛选）
- [ ] 4.2 提交详情接口（含代码、逐用例结果）
- [ ] 4.3 提交记录列表页前端
- [ ] 4.4 做题页结果面板（逐用例状态、耗时、内存）

### 阶段 5：管理员后台前端
- [ ] 5.1 后台题目管理页（列表 + 新增/编辑表单）
- [ ] 5.2 测试用例管理（表单逐条 + 打包上传切换）
- [ ] 5.3 软删除确认交互

### 阶段 6：集成与验收
- [ ] 6.1 CodeMirror 集成（C++ 高亮、行号、缩进）
- [ ] 6.2 全链路联调（注册→加题→提交→判题→记录）
- [ ] 6.3 安全边界测试（fork 炸弹 / 死循环 / 超内存 / 大输出）
- [ ] 6.4 性能测试（10 并发提交，判题延迟 < 3s）
- [ ] 6.5 README（部署、配置、使用说明）

---

## 9. 验收标准

| # | 场景 | 预期 |
|---|---|---|
| V1 | 新用户注册→登录→看到题目列表 | 成功，状态正确 |
| V2 | 管理员新增题目 + 录入 3 条测试用例（表单） | 题目可见，用例入库 |
| V3 | 管理员打包上传用例 zip（含 5.in/5.out） | 解压入库，无路径穿越 |
| V4 | 普通用户提交 AC 代码 | 同步返回 AC，逐用例全绿，记录可查 |
| V5 | 提交 WA 代码 | 返回首个失败用例 WA + 该用例详情 |
| V6 | 提交编译错误代码 | 返回 CE + g++ 原始报错文本 |
| V7 | 提交死循环代码 | 返回 TLE，耗时 ≈ 1s，不阻塞后续请求 |
| V8 | 提交 fork 炸弹代码 | 返回 RE，系统不崩 |
| V9 | 提交超内存代码 | 返回 RE（信号 SIGSEGV 或内存超限） |
| V10 | 自定义测试：填入输入→运行 | 返回程序 stdout（≤64KB），不记入提交 |
| V11 | 普通用户访问 /api/admin/* | 403 |
| V12 | 10 并发提交 | 全部在 3s 内返回，Web 服务不阻塞 |
| V13 | 判题完成后 /tmp/judge_* | 目录已清理 |

---

# 10. 明确的非目标（Out of Scope for MVP）

- seccomp 严格白名单（下一阶段）
- 异步判题 + 前端轮询（下一阶段）
- 多语言支持（仅 C++）
- 排行榜 / 竞赛 / 讨论区 / 题解
- 分布式 / 多判题节点
- 持久化日志（仅控制台）
- 前端构建工具链 / 框架（纯原生）
- seccomp 严格白名单（下一阶段）
- 异步判题 + 前端轮询（下一阶段）
- 多语言支持（仅 C++）
- 排行榜 / 竞赛 / 讨论区 / 题解
- 分布式 / 多判题节点
- 持久化日志（仅控制台）
- 前端构建工具链 / 框架（纯原生）
