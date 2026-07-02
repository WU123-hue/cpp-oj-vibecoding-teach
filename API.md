# OJ 系统 API 接口文档

> 基础地址：`http://<host>:8080`（默认端口 8080，见 `config/config.yaml`）
>
> 所有接口均使用 JSON 格式进行请求与响应，请求体需设置 `Content-Type: application/json`。

---

## 目录

- [通用约定](#通用约定)
- [认证机制](#认证机制)
- [1. 用户注册](#1-用户注册)
- [2. 用户登录](#2-用户登录)
- [3. 用户登出](#3-用户登出)
- [4. 题目列表](#4-题目列表)
- [5. 题目详情](#5-题目详情)
- [6. 提交代码执行](#6-提交代码执行)
- [7. 新增题目（管理员）](#7-新增题目管理员)
- [8. 删除题目（管理员）](#8-删除题目管理员)

---

## 通用约定

### 统一响应格式

所有接口返回统一的 JSON 响应体：

```json
{
  "code": 200,
  "message": "ok",
  "data": { ... }    // 成功时包含业务数据；失败时 data 字段不存在
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `code` | `int` | HTTP 状态码，与 `res.status` 一致 |
| `message` | `string` | 状态描述信息 |
| `data` | `object\|array` | 业务数据（仅成功响应包含） |

### HTTP 状态码

| 状态码 | 含义 |
|--------|------|
| `200` | 请求成功 |
| `400` | 请求参数错误 / 校验失败 |
| `401` | 认证失败（用户名或密码错误） |
| `404` | 资源不存在 |
| `500` | 服务器内部错误 |

---

## 认证机制

系统使用基于 Cookie 的 Session 认证。

- **登录成功**后，服务端通过 `Set-Cookie` 响应头下发会话 ID：
  ```
  Set-Cookie: oj_session=<session_id>; Path=/; HttpOnly
  ```
- **后续请求**需在请求头中携带 Cookie：
  ```
  Cookie: oj_session=<session_id>
  ```
- **登出**后，服务端销毁会话并通过 `Set-Cookie` 清除客户端 Cookie：
  ```
  Set-Cookie: oj_session=; Path=/; HttpOnly; Max-Age=0
  ```

| 属性 | 值 |
|------|------|
| Cookie 名称 | `oj_session` |
| session_id 格式 | 32 字符十六进制字符串 |
| 会话超时 | 1800 秒（30 分钟），活跃访问自动续期 |
| 存储方式 | 内存（服务端单例 SessionManager，重启后丢失） |

---

## 1. 用户注册

### 基本信息

| 项 | 值 |
|------|------|
| 方法 | `POST` |
| 路径 | `/api/register` |
| 权限 | 公开（所有人可访问） |
| 作用 | 注册新用户，密码使用 bcrypt 哈希存储 |

### 请求参数

请求体（JSON）：

| 字段 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `username` | `string` | 是 | 长度 3~64 | 用户名，需唯一 |
| `password` | `string` | 是 | 长度 6~64 | 明文密码，服务端哈希后存储 |

请求示例：

```bash
curl -X POST http://localhost:8080/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"secret123"}'
```

### 响应

**成功（200）：**

```json
{
  "code": 200,
  "message": "registered",
  "data": {
    "id": 10,
    "username": "alice"
  }
}
```

**失败（400）：**

| 场景 | message |
|------|---------|
| username 为空 | `username is required` |
| username 长度 < 3 或 > 64 | `username must be 3-64 characters` |
| password 为空 | `password is required` |
| password 长度 < 6 或 > 64 | `password must be 6-64 characters` |
| 用户名已存在 | `username already exists` |
| 非法 JSON | `invalid JSON: <详情>` |
| 缺少 username 字段 | `username is required` |
| 缺少 password 字段 | `password is required` |

```json
{
  "code": 400,
  "message": "username already exists"
}
```

---

## 2. 用户登录

### 基本信息

| 项 | 值 |
|------|------|
| 方法 | `POST` |
| 路径 | `/api/login` |
| 权限 | 公开（所有人可访问） |
| 作用 | 验证用户名密码，创建会话，下发 Set-Cookie |

### 请求参数

请求体（JSON）：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `username` | `string` | 是 | 用户名，非空 |
| `password` | `string` | 是 | 明文密码，非空 |

请求示例：

```bash
curl -X POST http://localhost:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"secret123"}'
```

### 响应

**成功（200）：**

响应头：

```
Set-Cookie: oj_session=a1b2c3d4e5f6...; Path=/; HttpOnly
```

响应体：

```json
{
  "code": 200,
  "message": "login successful",
  "data": {
    "id": 10,
    "username": "alice",
    "role": "user"
  }
}
```

| data 字段 | 类型 | 说明 |
|-----------|------|------|
| `id` | `int` | 用户 ID |
| `username` | `string` | 用户名 |
| `role` | `string` | 角色：`"user"` 或 `"admin"` |

**失败（401）：**

用户名不存在或密码错误均返回相同信息（不泄露用户是否存在）：

```json
{
  "code": 401,
  "message": "invalid username or password"
}
```

**失败（400）：**

| 场景 | message |
|------|---------|
| username 为空/缺失/非字符串 | `username is required` |
| password 为空/缺失/非字符串 | `password is required` |
| 非法 JSON | `invalid JSON: <详情>` |

---

## 3. 用户登出

### 基本信息

| 项 | 值 |
|------|------|
| 方法 | `POST` |
| 路径 | `/api/logout` |
| 权限 | 公开（即使未登录也返回 200） |
| 作用 | 销毁当前会话，清除客户端 Cookie |

### 请求参数

无需请求体。需在请求头中携带 Cookie（可选）：

```
Cookie: oj_session=<session_id>
```

请求示例：

```bash
curl -X POST http://localhost:8080/api/logout \
  -H "Cookie: oj_session=a1b2c3d4e5f6..."
```

### 响应

**始终返回 200：**

响应头：

```
Set-Cookie: oj_session=; Path=/; HttpOnly; Max-Age=0
```

响应体：

```json
{
  "code": 200,
  "message": "logged out"
}
```

> 无论是否携带 Cookie、Cookie 中 session 是否有效，均返回 200。服务端在 session 存在时销毁它，并始终通过 `Set-Cookie` 清除客户端 Cookie。

---

## 4. 题目列表

### 基本信息

| 项 | 值 |
|------|------|
| 方法 | `GET` |
| 路径 | `/api/problems` |
| 权限 | 公开 |
| 作用 | 获取所有题目列表（仅含摘要信息，不含题目详情） |

### 请求参数

无。

请求示例：

```bash
curl http://localhost:8080/api/problems
```

### 响应

**成功（200）：**

```json
{
  "code": 200,
  "message": "ok",
  "data": [
    {
      "id": 1,
      "title": "A+B Problem",
      "difficulty": "Easy"
    },
    {
      "id": 2,
      "title": "两数之和",
      "difficulty": "Medium"
    }
  ]
}
```

| data 数组元素字段 | 类型 | 说明 |
|-------------------|------|------|
| `id` | `int` | 题目 ID |
| `title` | `string` | 题目标题 |
| `difficulty` | `string` | 难度：`"Easy"` / `"Medium"` / `"Hard"` |

**失败（500）：**

数据库查询失败时返回：

```json
{
  "code": 500,
  "message": "query failed"
}
```

---

## 5. 题目详情

### 基本信息

| 项 | 值 |
|------|------|
| 方法 | `GET` |
| 路径 | `/api/problems/:id` |
| 权限 | 公开 |
| 作用 | 获取指定题目的完整详情（含描述、代码模板、测试用例） |

### 请求参数

路径参数：

| 参数 | 类型 | 说明 |
|------|------|------|
| `id` | `int` | 题目 ID（正整数） |

请求示例：

```bash
curl http://localhost:8080/api/problems/1
```

### 响应

**成功（200）：**

```json
{
  "code": 200,
  "message": "ok",
  "data": {
    "id": 1,
    "title": "A+B Problem",
    "difficulty": "Easy",
    "content": "输入两个整数 a 和 b，输出它们的和。",
    "template": "#include <iostream>\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b;return 0;}",
    "created_at": "2026-07-01 13:45:22",
    "test_cases": [
      {
        "id": 1,
        "input": "1 2",
        "expected": "3",
        "position": 0
      },
      {
        "id": 2,
        "input": "10 20",
        "expected": "30",
        "position": 1
      }
    ]
  }
}
```

| data 字段 | 类型 | 说明 |
|-----------|------|------|
| `id` | `int` | 题目 ID |
| `title` | `string` | 题目标题 |
| `difficulty` | `string` | 难度：`"Easy"` / `"Medium"` / `"Hard"` |
| `content` | `string` | 题目描述（Markdown 格式） |
| `template` | `string` | 代码模板 |
| `created_at` | `string` | 创建时间（`YYYY-MM-DD HH:MM:SS`） |
| `test_cases` | `array` | 测试用例列表，按 `position` 升序排列 |

| test_cases 元素字段 | 类型 | 说明 |
|---------------------|------|------|
| `id` | `int` | 测试用例 ID |
| `input` | `string` | 输入数据 |
| `expected` | `string` | 期望输出 |
| `position` | `int` | 排序序号（从 0 开始） |

**失败（404）：**

题目不存在：

```json
{
  "code": 404,
  "message": "problem not found"
}
```

---

## 6. 提交代码执行

### 基本信息

| 项 | 值 |
|------|------|
| 方法 | `POST` |
| 路径 | `/api/submit` |
| 权限 | 公开（后续需登录） |
| 作用 | 提交 C++ 源代码，编译并逐个测试用例运行，返回判题结果 |

### 执行限制

以下参数由 `config/config.yaml` 的 `executor` 节配置：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `timeout_sec` | 5 | 单个测试用例运行超时（秒，wall-clock） |
| `cpu_limit_sec` | 2 | CPU 时间限制（秒，超出触发 SIGXCPU） |
| `mem_limit_mb` | 256 | 内存限制（MB，RLIMIT_AS） |

编译命令：`g++ -std=c++17 -O2 -o <exe> <src>`

### 请求参数

请求体（JSON）：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `problem_id` | `int` | 是 | 题目 ID |
| `code` | `string` | 是 | C++ 源代码，不能为空 |

请求示例：

```bash
curl -X POST http://localhost:8080/api/submit \
  -H "Content-Type: application/json" \
  -d '{"problem_id":1,"code":"#include <iostream>\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b;return 0;}"}'
```

### 判题状态

| 状态码 | 含义 | 说明 |
|--------|------|------|
| `AC` | Accepted | 全部测试用例通过 |
| `WA` | Wrong Answer | 输出与期望不匹配 |
| `CE` | Compile Error | 编译失败 |
| `TLE` | Time Limit Exceeded | 运行超时（wall-clock 或 CPU 限制） |
| `RE` | Runtime Error | 运行时错误（段错误、除零、abort 等） |
| `SE` | System Error | 系统错误（fork 失败等） |

### 响应

**成功（200）— 编译并通过测试用例：**

```json
{
  "code": 200,
  "message": "ok",
  "data": {
    "status": "AC",
    "passed": 2,
    "total": 2,
    "max_time_ms": 13,
    "cases": [
      {
        "status": "AC",
        "exit_code": 0,
        "time_ms": 11,
        "actual": "3\n"
      },
      {
        "status": "AC",
        "exit_code": 0,
        "time_ms": 13,
        "actual": "30\n"
      }
    ]
  }
}
```

**成功（200）— 编译错误（CE）：**

```json
{
  "code": 200,
  "message": "ok",
  "data": {
    "status": "CE",
    "passed": 0,
    "total": 2,
    "max_time_ms": 0,
    "compile_output": "main.cpp: In function 'int main()':\nmain.cpp:1:13: error: ...\n",
    "cases": []
  }
}
```

**成功（200）— 运行时错误（RE）：**

```json
{
  "code": 200,
  "message": "ok",
  "data": {
    "status": "RE",
    "passed": 0,
    "total": 2,
    "max_time_ms": 251,
    "cases": [
      {
        "status": "RE",
        "exit_code": -1,
        "time_ms": 251,
        "actual": "",
        "error": "segmentation fault"
      }
    ]
  }
}
```

**成功（200）— 超时（TLE）：**

```json
{
  "code": 200,
  "message": "ok",
  "data": {
    "status": "TLE",
    "passed": 0,
    "total": 2,
    "max_time_ms": 2398,
    "cases": [
      {
        "status": "TLE",
        "exit_code": -1,
        "time_ms": 2398,
        "actual": "",
        "error": "CPU time limit exceeded (SIGXCPU)"
      }
    ]
  }
}
```

| data 字段 | 类型 | 说明 |
|-----------|------|------|
| `status` | `string` | 总体判题状态（首个失败用例的状态决定） |
| `passed` | `int` | 通过的测试用例数 |
| `total` | `int` | 测试用例总数 |
| `max_time_ms` | `int` | 所有用例中最大耗时（毫秒） |
| `compile_output` | `string` | 编译错误信息（仅 `status=CE` 时存在） |
| `cases` | `array` | 各测试用例的运行结果（CE 时为空数组） |

| cases 元素字段 | 类型 | 说明 |
|----------------|------|------|
| `status` | `string` | 该用例的判题状态 |
| `exit_code` | `int` | 子进程退出码（被信号终止时为 -1） |
| `time_ms` | `int` | 该用例运行耗时（毫秒） |
| `actual` | `string` | 子进程实际 stdout 输出 |
| `error` | `string` | 错误描述（仅 `RE` 和 `TLE` 时存在） |

**失败（400）：**

| 场景 | message |
|------|---------|
| 缺少 problem_id 或非数字 | `problem_id is required` |
| 缺少 code 或非字符串 | `code is required` |
| code 为空字符串 | `code is empty` |
| 非法 JSON | `invalid JSON: <详情>` |

**失败（404）：**

题目不存在：

```json
{
  "code": 404,
  "message": "problem not found"
}
```

---

## 7. 新增题目（管理员）

### 基本信息

| 项 | 值 |
|------|------|
| 方法 | `POST` |
| 路径 | `/api/admin/problems` |
| 权限 | 管理员 |
| 作用 | 创建新题目，同时插入关联的测试用例（事务保证原子性） |

### 请求参数

请求体（JSON）：

| 字段 | 类型 | 必填 | 约束 | 说明 |
|------|------|------|------|------|
| `title` | `string` | 是 | 非空 | 题目标题 |
| `difficulty` | `string` | 是 | `"Easy"` / `"Medium"` / `"Hard"` | 难度 |
| `content` | `string` | 是 | 非空 | 题目描述（Markdown） |
| `template` | `string` | 否 | 可为空 | 代码模板 |
| `test_cases` | `array` | 否 | 可为空数组 | 测试用例列表 |

`test_cases` 数组元素：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `input` | `string` | 是 | 输入数据 |
| `expected` | `string` | 是 | 期望输出 |

> 测试用例的 `position` 按数组顺序自动赋值（0, 1, 2, ...）。

请求示例：

```bash
curl -X POST http://localhost:8080/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{
    "title": "A+B Problem",
    "difficulty": "Easy",
    "content": "输入两个整数 a 和 b，输出它们的和。",
    "template": "#include <iostream>\nint main(){}",
    "test_cases": [
      {"input": "1 2", "expected": "3"},
      {"input": "10 20", "expected": "30"}
    ]
  }'
```

### 响应

**成功（200）：**

```json
{
  "code": 200,
  "message": "created",
  "data": {
    "id": 3
  }
}
```

| data 字段 | 类型 | 说明 |
|-----------|------|------|
| `id` | `int` | 新创建的题目 ID |

**失败（400）：**

| 场景 | message |
|------|---------|
| title 为空 | `title is required` |
| content 为空 | `content is required` |
| difficulty 非法 | `invalid difficulty` |
| 非法 JSON | `invalid JSON: <详情>` |

---

## 8. 删除题目（管理员）

### 基本信息

| 项 | 值 |
|------|------|
| 方法 | `DELETE` |
| 路径 | `/api/admin/problems/:id` |
| 权限 | 管理员 |
| 作用 | 删除指定题目，外键级联删除关联的测试用例 |

### 请求参数

路径参数：

| 参数 | 类型 | 说明 |
|------|------|------|
| `id` | `int` | 题目 ID（正整数） |

请求示例：

```bash
curl -X DELETE http://localhost:8080/api/admin/problems/3
```

### 响应

**成功（200）：**

```json
{
  "code": 200,
  "message": "deleted"
}
```

**失败（400）：**

| 场景 | message |
|------|---------|
| id <= 0 | `invalid id` |

---

## 接口总览

| # | 方法 | 路径 | 权限 | 作用 |
|---|------|------|------|------|
| 1 | POST | `/api/register` | 公开 | 用户注册 |
| 2 | POST | `/api/login` | 公开 | 用户登录 |
| 3 | POST | `/api/logout` | 公开 | 用户登出 |
| 4 | GET | `/api/problems` | 公开 | 题目列表 |
| 5 | GET | `/api/problems/:id` | 公开 | 题目详情 |
| 6 | POST | `/api/submit` | 公开 | 提交代码执行 |
| 7 | POST | `/api/admin/problems` | 管理员 | 新增题目 |
| 8 | DELETE | `/api/admin/problems/:id` | 管理员 | 删除题目 |
