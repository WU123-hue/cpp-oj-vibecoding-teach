# OJ System — C++ 在线判题系统

> 一个基于 C++ 后端 + 原生前端实现的在线代码判题系统，支持 C++ 代码在线编辑、编译、运行与自动判定，适用于教学训练与算法练习。

---

## 功能概览

- **用户注册/登录**：基于 Cookie + Session 认证，密码 bcrypt 哈希存储
- **题目浏览**：按难度（简单/中等/困难）筛选，查看题目描述与测试用例
- **在线代码编辑**：内置 Ace 编辑器，支持 C++ 语法高亮与自动补全
- **自动判题**：提交代码后自动编译运行，判定 AC / WA / CE / TLE / RE 五种状态
- **管理后台**：管理员可新增题目（含测试用例）、删除题目
- **深色科技风 UI**：统一的深色主题，响应式布局，支持移动端访问

---

## 技术栈

| 层级 | 技术 |
|------|------|
| 后端语言 | C++17 |
| HTTP 框架 | cpp-httplib（header-only） |
| 数据库 | MySQL 8.0（utf8mb4） |
| 配置管理 | yaml-cpp |
| JSON 处理 | nlohmann/json |
| 密码哈希 | bcrypt（libcrypt） |
| 前端 | 原生 HTML + CSS + JavaScript |
| 代码编辑器 | Ace Editor（CDN） |
| 测试框架 | Google Test（gtest） |

---

## 系统架构

```
┌─────────────────────────────────────────┐
│            前端 (Browser)                │
│     HTML + CSS + JS (原生，无框架)        │
└──────────────────┬──────────────────────┘
                   │ HTTP REST
┌──────────────────▼──────────────────────┐
│            C++ Backend                   │
│          cpp-httplib (HTTP)              │
│  ┌──────────┬────────────┬────────────┐ │
│  │ 题目服务  │ 代码执行服务 │ 认证服务   │ │
│  │ (MySQL)  │ (fork+g++) │ (Session)  │ │
│  └──────────┴────────────┴────────────┘ │
└─────────────────────────────────────────┘
```

---

## 项目结构

```
cpp-oj-vibecoding-teach/
├── SPEC.md                        # 系统规格说明书
├── API.md                         # API 接口文档
├── DEPLOY.md                      # 部署文档
├── config/
│   └── config.yaml                # 运行时配置
├── database/
│   └── init.sql                   # 数据库初始化脚本
├── src/
│   ├── main.cc                    # 程序入口
│   ├── init_admin.cc              # 管理员初始化工具
│   ├── server/                    # HTTP 服务器与路由
│   ├── handler/                   # 请求处理器（auth/problem/submit/admin）
│   ├── service/                   # 业务逻辑层（auth/problem/executor/session）
│   ├── model/                     # 数据模型（problem/test_case/user/mapper）
│   ├── db/                        # MySQL 连接池
│   └── utils/                     # 工具（config/logger/httplib）
├── public/                        # 前端静态文件
│   ├── index.html                 # 大屏落地页
│   ├── login.html                 # 登录页
│   ├── register.html              # 注册页
│   ├── problem_list.html          # 题目列表页
│   ├── problem.html               # 题目详情页
│   ├── admin.html                 # 管理后台页
│   ├── css/style.css              # 全局样式
│   └── js/                        # 前端逻辑
└── tests/
    ├── unit/                      # 单元测试（12 个测试文件）
    └── python/                    # Python 接口自动化测试
```

---

## 核心特性

### 判题流程

1. 用户在题目详情页编写 C++ 代码
2. 点击提交（或 Ctrl+Enter），代码发送到后端
3. 后端 fork 子进程调用 `g++ -std=c++17 -O2` 编译
4. 编译成功后逐个测试用例运行，设置 CPU/内存/输出限制
5. 比较 stdout 与期望输出，判定 AC/WA/TLE/RE
6. 前端无刷新展示结果

### 执行限制

| 参数 | 默认值 | 说明 |
|------|--------|------|
| 运行超时 | 5 秒 | wall-clock 超时 |
| CPU 限制 | 2 秒 | RLIMIT_CPU，超出触发 SIGXCPU |
| 内存限制 | 256 MB | RLIMIT_AS |
| 输出限制 | 16 MB | RLIMIT_FSIZE |

### 认证机制

- 登录成功后服务端通过 `Set-Cookie: oj_session=<sid>; Path=/; HttpOnly` 下发会话
- 会话存储在内存中（SessionManager 单例），30 分钟超时，活跃访问自动续期
- 登出时销毁会话并清除客户端 Cookie

---

## API 接口

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/register` | 用户注册 |
| POST | `/api/login` | 用户登录 |
| POST | `/api/logout` | 用户登出 |
| GET | `/api/problems` | 题目列表 |
| GET | `/api/problems/:id` | 题目详情 |
| POST | `/api/submit` | 提交代码执行 |
| POST | `/api/admin/problems` | 新增题目（管理员） |
| DELETE | `/api/admin/problems/:id` | 删除题目（管理员） |

详见 [API.md](API.md)。

---

## 默认管理员

| 用户名 | 密码 | 角色 |
|--------|------|------|
| `admin` | `admin123` | `admin` |

> 建议首次登录后尽快修改密码。

---

## 快速开始

```bash
# 1. 初始化数据库
mysql -u root < database/init.sql

# 2. 编译后端
g++ -std=c++17 -I src -I/usr/include/mysql \
  src/main.cc src/server/*.cc src/handler/*.cc src/service/*.cc \
  src/model/*.cc src/db/*.cc src/utils/*.cc \
  -o oj_server \
  -lmysqlclient -lpthread -lyaml-cpp -lssl -lcrypto -lcrypt

# 3. 启动服务器
./oj_server

# 4. 访问
# 浏览器打开 http://localhost:8080
```

详细部署步骤见 [DEPLOY.md](DEPLOY.md)。

---

## 测试

```bash
# 单元测试（需 gtest）
g++ -std=c++17 -I src -I/usr/include/mysql \
  tests/unit/executor_test.cc src/service/executor_service.cc src/utils/logger.cc \
  -o executor_test -lpthread -lgtest -lgtest_main
./executor_test

# Python 接口自动化测试（需 requests）
python3 tests/python/test_api.py
```

---

## 许可

本项目仅用于教学目的。
