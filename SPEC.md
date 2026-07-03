# OJ 系统规格说明书

## 1. 需求概述

| 维度 | 规格 |
|------|------|
| **业务目标** | 教学/训练平台 |
| **目标用户规模** | 1-20 人同时在线 |
| **核心功能** | 题目列表、题目描述、测试用例、在线编辑/编译/运行/返回结果、题目管理后台 |
| **角色** | 普通用户（做题）、管理员（增删题目） |
| **语言支持** | 仅 C++ |
| **题目来源** | 管理员手动导入 |
| **执行隔离** | 进程级隔离（基础） |
| **部署形态** | 单机部署 |
| **性能要求** | <500ms 响应 |
| **持久化** | 题目存 MySQL，提交记录/代码暂不持久化 |

---

## 2. 系统架构

```
┌─────────────────────────────────────────────────┐
│                   前端 (Browser)                  │
│     HTML + CSS + JS (原生，无框架)                │
└──────────────────┬──────────────────────────────┘
                   │ HTTP REST
                    │ HTTP REST
┌──────────────────▼──────────────────────────────┐
│                C++ Backend                        │
│             cpp-httplib (HTTP)                   │
│  ┌─────────────┬─────────────┬────────────────┐ │
│  │ 题目服务    │ 代码执行服务 │ 认证/权限服务   │ │
│  │ (MySQL)     │ (fork/popen)│ (Session)      │ │
│  └─────────────┴─────────────┴────────────────┘ │
└─────────────────────────────────────────────────┘
```

---

## 2.1 项目目录结构

```
cpp-oj-vibecoding-teach/
├── SPEC.md
├── README.md
├── CMakeLists.txt                    # 后端构建配置
├── config/
│   └── config.yaml                   # 配置文件
├── database/
│   └── init.sql                      # 数据库初始化脚本
├── src/
│   ├── main.cc                      # 程序入口
│   ├── server/
│   │   ├── server.cc                # HTTP 服务器
│   │   ├── router.cc                # 路由处理
│   │   └── router.h
│   ├── handler/
│   │   ├── problem_handler.cc       # 题目相关 API
│   │   ├── submit_handler.cc        # 代码提交执行
│   │   ├── auth_handler.cc          # 登录注册
│   │   └── admin_handler.cc         # 管理接口
│   ├── service/
│   │   ├── problem_service.cc       # 题目业务逻辑
│   │   ├── executor_service.cc      # 代码执行服务
│   │   └── auth_service.cc          # 认证业务逻辑
│   ├── model/
│   │   ├── problem.cc               # 题目数据模型
│   │   ├── test_case.cc             # 测试用例模型
│   │   └── user.cc                  # 用户模型
│   ├── db/
│   │   ├── connection_pool.cc       # MySQL 连接池
│   │   └── connection_pool.h
│   └── utils/
│       ├── logger.cc                # 日志工具
│       ├── logger.h
│       ├── config.cc                # 配置加载
│       └── config.h
├── public/
│   ├── index.html                   # 题目列表页
│   ├── login.html                   # 登录页
│   ├── register.html                # 注册页
│   ├── problem.html                 # 题目详情页
│   ├── admin.html                   # 管理后台页
│   ├── css/
│   │   └── style.css                # 样式文件
│   └── js/
│       ├── api.js                   # API 调用封装
│       ├── auth.js                  # 认证状态管理
│       ├── problem.js               # 题目列表逻辑
│       ├── problem_detail.js        # 题目详情逻辑
│       ├── submit.js                # 提交执行逻辑
│       └── admin.js                 # 管理后台逻辑
└── tests/
    ├── unit/
    │   ├── problem_test.cc
    │   └── executor_test.cc
    └── integration/
        └── api_test.cc
```

---

## 3. API 边界

### 3.1 公开接口（普通用户）

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/problems` | 题目列表 |
| GET | `/api/problems/:id` | 题目详情（含描述/用例） |
| POST | `/api/submit` | 提交代码执行 |
| POST | `/api/login` | 登录 |
| POST | `/api/logout` | 登出 |
| POST | `/api/register` | 注册新用户 |

### 3.2 管理接口（管理员）

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/admin/problems` | 新增题目 |
| DELETE | `/api/admin/problems/:id` | 删除题目 |

---

## 4. 前端页面

| 页面 | 路径 | 说明 | 访问权限 |
|------|------|------|----------|
| 登录页 | `/login.html` | 用户名 + 密码登录，登录成功跳转题目列表 | 所有人 |
| 注册页 | `/register.html` | 用户名 + 密码 + 确认密码，注册成功跳转登录页 | 所有人 |
| 题目列表页 | `/index.html` | 展示所有题目（编号、标题、难度），点击进入详情 | 已登录用户 |
| 题目详情页 | `/problem.html?id=:id` | 题目描述 + 在线代码编辑器 + 提交按钮 + 结果展示 | 已登录用户 |
| 后台管理页 | `/admin.html` | 新增题目表单、题目列表（含删除操作） | 管理员 |

---

## 5. 数据模型 (MySQL)

```sql
-- 题目表
CREATE TABLE problems (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  title      VARCHAR(255) NOT NULL,
  difficulty ENUM('Easy','Medium','Hard') NOT NULL,
  content    TEXT NOT NULL,          -- 题目描述 (Markdown)
  template   TEXT,                   -- 代码模板
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 测试用例表 (与题目 1:N 关联)
CREATE TABLE test_cases (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  problem_id INT NOT NULL,
  input      TEXT NOT NULL,          -- 输入数据
  expected   TEXT NOT NULL,          -- 期望输出
  position   INT NOT NULL DEFAULT 0, -- 排序序号
  FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
);

-- 用户表
CREATE TABLE users (
  id       INT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(64) UNIQUE NOT NULL,
  password VARCHAR(128) NOT NULL,    -- bcrypt 哈希
  role     ENUM('user','admin') DEFAULT 'user',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

---

## 5.1 数据库访问方式

> 以下为当前开发/部署环境实际的数据库连接信息。

| 项 | 值 |
|------|------|
| **数据库地址** | `localhost` (本地 Unix socket) |
| **端口** | `3306`（默认） |
| **数据库名** | `oj_db` |
| **用户名** | `root` |
| **密码** | （空，使用本地 socket 免密认证） |
| **字符集** | `utf8mb4` |

**连接命令示例：**

```bash
# 命令行连接
mysql -u root oj_db

# 后端代码连接参数 (对应 config/config.yaml)
#   host: localhost
#   port: 3306
#   user: root
#   password: ""
#   database: oj_db
```

**默认管理员账号（已随 init.sql 初始化）：**

| 用户名 | 密码 | 角色 |
|--------|------|------|
| `admin` | `admin123` | `admin` |

> 注：`admin123` 为 bcrypt 哈希前的明文，仅用于首次登录，建议登录后尽快修改。

---

## 6. TODO 清单

### Phase 1 - 基础设施
- [x] 项目目录结构搭建
- [x] MySQL 数据库初始化脚本
- [x] cpp-httplib 基础 HTTP 服务
- [x] 配置管理
- [x] 日志封装
- [x] 数据库连接池实现

### Phase 2 - 题目模块
- [x] 题目数据模型映射
- [x] 题目 CRUD API（管理员）
- [x] 题目列表/详情 API（用户）

### Phase 3 - 代码执行模块
- [x] C++ 代码编译（fork + g++）
- [x] 代码运行 + 超时控制
- [x] 结果比较（stdout vs expected）
- [x] 进程级资源限制（CPU/内存）

### Phase 4 - 登录注册模块
- [x] Session/Cookie 认证机制
- [x] 用户注册 API（用户名唯一性校验）
- [x] 用户登录 API
- [x] 用户退出登录 


### Phase 5 - 前端
- [x] 登录页面 (`/login.html`)
- [x] 注册页面 (`/register.html`)
- [x] 题目列表页面
- [x] 题目详情页面（描述 + 在线编辑器）
- [x] 提交结果展示
- [x] 管理后台（新增/删除题目）
- [x] 大屏落地页

### Phase 6 - 安全与部署
- [ ] 管理员权限校验
- [ ] 用户认证（Session/Cookie）
- [ ] 基础输入校验
- [ ] 部署文档

---

## 7. 验收标准

| # | 标准 |
|---|------|
| 1 | 管理员可成功新增题目并在前端列表看到 |
| 2 | 普通用户可查看题目、在线编辑 C++ 代码并提交 |
| 3 | 代码在 <5s 超时限制内执行并返回结果 |
| 4 | 正确判断 AC/WA/TLE/RE 并反馈给用户 |
| 5 | 管理员可删除题目 |
| 6 | 普通用户无法访问管理接口 |
| 7 | 部署文档完整，单机可运行 |
| 8 | 页面无需刷新可完成一次完整提交 |
| 9 | 新用户可注册账号并登录 |

---
 
## 8. 潜在风险与权衡

| 风险 | 权衡 |
|------|------|
| 进程级隔离安全性低（恶意代码可能影响系统） | 小规模教学场景可接受，建议后期升级容器级隔离 |
| 代码不持久化，重启丢失 | 当前阶段明确知晓，后续按需加 Redis/DB |
| <500ms 在代码编译时难以保证 | 放宽至 5s 内返回，或预编译缓存优化 |
| MySQL 单机部署无主从 | 小规模可接受，注意备份 |