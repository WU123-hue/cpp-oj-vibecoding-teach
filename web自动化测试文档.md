# OJ 系统 Web 自动化测试文档

> **测试目标服务器**：`http://101.42.13.135:8080`
>
> **管理员账号**：用户名 `admin`，密码 `admin123`
>
> **测试工具**：Python + requests 库（API 层测试），Selenium / Playwright（UI 层测试）
>
> **文档用途**：针对 OJ 系统全部 8 个 API 接口和 6 个前端页面，设计完整的自动化测试用例，覆盖正常流程、边界值、异常场景。

---

## 目录

- [1. 测试环境与前置条件](#1-测试环境与前置条件)
- [2. 测试用例总览](#2-测试用例总览)
- [3. 用户注册接口测试](#3-用户注册接口测试)
- [4. 用户登录接口测试](#4-用户登录接口测试)
- [5. 用户登出接口测试](#5-用户登出接口测试)
- [6. 题目列表接口测试](#6-题目列表接口测试)
- [7. 题目详情接口测试](#7-题目详情接口测试)
- [8. 提交代码执行接口测试](#8-提交代码执行接口测试)
- [9. 新增题目接口测试（管理员）](#9-新增题目接口测试管理员)
- [10. 删除题目接口测试（管理员）](#10-删除题目接口测试管理员)
- [11. 前端页面 UI 测试](#11-前端页面-ui-测试)
- [12. 端到端业务流程测试](#12-端到端业务流程测试)
- [附录：Python 自动化测试脚本](#附录python-自动化测试脚本)

---

## 1. 测试环境与前置条件

### 1.1 服务器信息

| 项 | 值 |
|------|------|
| 服务器地址 | `http://101.42.13.135:8080` |
| 管理员用户名 | `admin` |
| 管理员密码 | `admin123` |
| 管理员角色 | `admin` |

### 1.2 前置条件

1. 服务器已启动且可通过 `http://101.42.13.135:8080` 访问
2. 管理员账户已初始化（`admin` / `admin123`）
3. 数据库 `oj_db` 已创建且表结构已初始化
4. 测试前清理残留测试数据（用户名以 `webtest_` 为前缀，题目标题以 `WebTest_` 为前缀）

### 1.3 测试数据命名约定

| 类型 | 前缀 | 示例 |
|------|------|------|
| 测试用户名 | `webtest_` | `webtest_alice` |
| 测试题目标题 | `WebTest_` | `WebTest_ABProblem` |
| 测试密码 | `webtest123` | 固定使用 |

### 1.4 统一响应格式

```json
{
  "code": 200,
  "message": "ok",
  "data": { ... }
}
```

---

## 2. 测试用例总览

| 编号 | 模块 | 用例数 | 说明 |
|------|------|--------|------|
| TC-REG | 用户注册 | 7 | 正常注册 + 校验规则 + 重复用户名 |
| TC-LOGIN | 用户登录 | 8 | 管理员/普通用户登录 + 错误密码 + 缺字段 |
| TC-LOGOUT | 用户登出 | 4 | 正常登出 + 无Cookie + 无效session |
| TC-LIST | 题目列表 | 3 | 获取列表 + 空列表 + 数据结构验证 |
| TC-DETAIL | 题目详情 | 4 | 存在/不存在 + 含/不含测试用例 |
| TC-SUBMIT | 代码提交 | 9 | AC/WA/CE/TLE/RE + 缺字段 + 题目不存在 |
| TC-CREATE | 新增题目 | 7 | 正常创建 + 校验规则 + 非法JSON |
| TC-DELETE | 删除题目 | 4 | 正常删除 + 验证删除 + 不存在题目 |
| TC-UI | 前端页面 | 10 | 落地页/登录/注册/列表/详情/管理后台 |
| TC-E2E | 端到端流程 | 5 | 注册→登录→刷题→登出等完整流程 |
| **合计** | | **61** | |

---

## 3. 用户注册接口测试

**接口**：`POST /api/register`
**请求体**：`{ "username": "string", "password": "string" }`

| 编号 | 场景 | 请求参数 | 预期状态码 | 预期 message | 预期 data |
|------|------|---------|-----------|-------------|-----------|
| TC-REG-01 | 正常注册 | `{"username":"webtest_alice","password":"webtest123"}` | 200 | `registered` | 含 `id`（正整数）和 `username` |
| TC-REG-02 | 重复用户名 | 同 TC-REG-01 的用户名 | 400 | `username already exists` | 无 |
| TC-REG-03 | 用户名为空 | `{"username":"","password":"webtest123"}` | 400 | `username is required` | 无 |
| TC-REG-04 | 用户名过短（2字符） | `{"username":"ab","password":"webtest123"}` | 400 | `username must be 3-64 characters` | 无 |
| TC-REG-05 | 密码为空 | `{"username":"webtest_empty","password":""}` | 400 | `password is required` | 无 |
| TC-REG-06 | 密码过短（5字符） | `{"username":"webtest_short","password":"12345"}` | 400 | `password must be 6-64 characters` | 无 |
| TC-REG-07 | 非法JSON | 原始文本 `{invalid` | 400 | 含 `invalid JSON` | 无 |

### 验证要点

- TC-REG-01：响应 `data.id` 应为正整数，`data.username` 与请求一致
- TC-REG-02~06：响应体不含 `data` 字段
- TC-REG-01：注册后用登录接口验证密码正确（间接验证 bcrypt 哈希）

---

## 4. 用户登录接口测试

**接口**：`POST /api/login`
**请求体**：`{ "username": "string", "password": "string" }`

| 编号 | 场景 | 请求参数 | 预期状态码 | 预期 message | 预期 data / 响应头 |
|------|------|---------|-----------|-------------|-------------------|
| TC-LOGIN-01 | 管理员登录 | `{"username":"admin","password":"admin123"}` | 200 | `login successful` | `data.role`=`admin`，响应头含 `Set-Cookie: oj_session=...` |
| TC-LOGIN-02 | 普通用户登录 | `{"username":"webtest_alice","password":"webtest123"}` | 200 | `login successful` | `data.role`=`user`，响应头含 `Set-Cookie` |
| TC-LOGIN-03 | 错误密码 | `{"username":"admin","password":"wrongpwd"}` | 401 | `invalid username or password` | 无 data |
| TC-LOGIN-04 | 不存在的用户 | `{"username":"nosuchuser","password":"webtest123"}` | 401 | `invalid username or password` | 无 data |
| TC-LOGIN-05 | 空用户名 | `{"username":"","password":"admin123"}` | 400 | `username is required` | 无 data |
| TC-LOGIN-06 | 空密码 | `{"username":"admin","password":""}` | 400 | `password is required` | 无 data |
| TC-LOGIN-07 | 缺少username字段 | `{"password":"admin123"}` | 400 | `username is required` | 无 data |
| TC-LOGIN-08 | 非法JSON | 原始文本 `{bad` | 400 | 含 `invalid JSON` | 无 data |

### 验证要点

- TC-LOGIN-01/02：`Set-Cookie` 头格式为 `oj_session=<32位十六进制>; Path=/; HttpOnly`
- TC-LOGIN-03/04：错误密码与不存在用户返回**相同**错误信息（不泄露用户是否存在）
- TC-LOGIN-01/02：`data` 含 `id`、`username`、`role` 三个字段

---

## 5. 用户登出接口测试

**接口**：`POST /api/logout`
**请求头**：`Cookie: oj_session=<session_id>`（可选）

| 编号 | 场景 | 请求参数 | 预期状态码 | 预期 message | 预期响应头 |
|------|------|---------|-----------|-------------|-----------|
| TC-LOGOUT-01 | 已登录用户登出 | 先登录获取 Cookie，再带 Cookie 登出 | 200 | `logged out` | `Set-Cookie` 含 `Max-Age=0` |
| TC-LOGOUT-02 | 无Cookie登出 | 不带任何 Cookie | 200 | `logged out` | `Set-Cookie` 含 `Max-Age=0` |
| TC-LOGOUT-03 | 无效session登出 | `Cookie: oj_session=invalid123` | 200 | `logged out` | `Set-Cookie` 含 `Max-Age=0` |
| TC-LOGOUT-04 | 重复登出同一session | TC-LOGOUT-01 后再次用同一 Cookie 登出 | 200 | `logged out` | `Set-Cookie` 含 `Max-Age=0` |

### 验证要点

- 所有场景均返回 200（登出永远成功）
- 响应头 `Set-Cookie` 应清除 `oj_session`（值为空 + `Max-Age=0`）

---

## 6. 题目列表接口测试

**接口**：`GET /api/problems`

| 编号 | 场景 | 前置条件 | 预期状态码 | 预期验证 |
|------|------|---------|-----------|---------|
| TC-LIST-01 | 获取题目列表 | 数据库中已有题目 | 200 | `data` 为数组，每项含 `id`、`title`、`difficulty` |
| TC-LIST-02 | 验证难度字段值 | 数据库中有 Easy/Medium/Hard 题目 | 200 | `difficulty` 值仅为 `Easy`/`Medium`/`Hard` |
| TC-LIST-03 | 数据结构完整性 | 有题目数据 | 200 | 数组每项仅含 3 个字段，不含 `content`/`template` 等详情字段 |

### 验证要点

- `data` 为 JSON 数组
- 每项元素字段：`id`（int）、`title`（string）、`difficulty`（string）
- 列表按 `id` 升序排列

---

## 7. 题目详情接口测试

**接口**：`GET /api/problems/:id`

| 编号 | 场景 | 前置条件 | 预期状态码 | 预期验证 |
|------|------|---------|-----------|---------|
| TC-DETAIL-01 | 获取含测试用例的题目 | 存在题目且有测试用例 | 200 | `data` 含 `id`/`title`/`difficulty`/`content`/`template`/`created_at`/`test_cases` |
| TC-DETAIL-02 | 获取无测试用例的题目 | 存在题目但无测试用例 | 200 | `data.test_cases` 为空数组 `[]` |
| TC-DETAIL-03 | 获取不存在的题目 | `id=9999999` | 404 | `message` = `problem not found`，无 `data` |
| TC-DETAIL-04 | 测试用例排序验证 | 题目有多个测试用例 | 200 | `test_cases` 按 `position` 升序排列 |

### 验证要点

- `test_cases` 每项含 `id`、`input`、`expected`、`position`
- `created_at` 格式为 `YYYY-MM-DD HH:MM:SS`
- `difficulty` 值为 `Easy`/`Medium`/`Hard`

---

## 8. 提交代码执行接口测试

**接口**：`POST /api/submit`
**请求体**：`{ "problem_id": int, "code": "string" }`

> 前置条件：通过管理员创建一个测试题目（含 2 个测试用例：`1 2`→`3`，`10 20`→`30`），记录其 `problem_id`。

| 编号 | 场景 | 请求参数 | 预期状态码 | 预期 data.status | 预期 data.passed |
|------|------|---------|-----------|-----------------|-----------------|
| TC-SUBMIT-01 | AC — 正确代码 | A+B 正确实现 | 200 | `AC` | `2`（等于 total） |
| TC-SUBMIT-02 | WA — 错误输出 | A*B 实现 | 200 | `WA` | `0` |
| TC-SUBMIT-03 | CE — 编译错误 | `int main(){ syntax error }` | 200 | `CE` | `0`，`cases` 为空数组，含 `compile_output` |
| TC-SUBMIT-04 | TLE — 超时 | `int main(){while(1){}return 0;}` | 200 | `TLE` | `0`，cases 含 `error` 字段 |
| TC-SUBMIT-05 | RE — 运行时错误 | `int main(){int*p=nullptr;*p=42;return 0;}` | 200 | `RE` | `0`，cases 含 `error: segmentation fault` |
| TC-SUBMIT-06 | 题目不存在 | `{"problem_id":9999999,"code":"int main(){}"}` | 404 | — | — |
| TC-SUBMIT-07 | 缺少code字段 | `{"problem_id":<id>}` | 400 | — | — |
| TC-SUBMIT-08 | 缺少problem_id字段 | `{"code":"int main(){}"}` | 400 | — | — |
| TC-SUBMIT-09 | 非法JSON | 原始文本 `{bad` | 400 | — | — |

### 验证要点

- AC/WA：`cases` 数组每项含 `status`、`exit_code`、`time_ms`、`actual`
- CE：`data` 含 `compile_output` 字段（编译器错误信息），`cases` 为空数组
- TLE：`cases` 每项 `error` 含 `CPU time limit exceeded` 或超时描述
- RE：`cases` 每项 `error` 含 `segmentation fault` 或对应错误描述
- `data.max_time_ms` 为所有用例最大耗时（毫秒）

---

## 9. 新增题目接口测试（管理员）

**接口**：`POST /api/admin/problems`
**请求体**：

```json
{
  "title": "string",
  "difficulty": "Easy|Medium|Hard",
  "content": "string",
  "template": "string (可选)",
  "test_cases": [{"input": "string", "expected": "string"}]
}
```

| 编号 | 场景 | 请求参数 | 预期状态码 | 预期 message | 预期 data |
|------|------|---------|-----------|-------------|-----------|
| TC-CREATE-01 | 正常创建（含测试用例） | 完整数据 + 2个测试用例 | 200 | `created` | `data.id` 为正整数 |
| TC-CREATE-02 | 正常创建（无测试用例） | 不含 `test_cases` 字段 | 200 | `created` | `data.id` 为正整数 |
| TC-CREATE-03 | 缺少title | 不含 `title` 字段 | 400 | `title is required` | 无 |
| TC-CREATE-04 | 缺少content | 不含 `content` 字段 | 400 | `content is required` | 无 |
| TC-CREATE-05 | 无效difficulty | `difficulty: "SuperHard"` | 400 | `invalid difficulty` | 无 |
| TC-CREATE-06 | 非法JSON | 原始文本 `{bad json` | 400 | 含 `invalid JSON` | 无 |
| TC-CREATE-07 | 三种难度均合法 | 分别用 Easy/Medium/Hard 创建 | 200 | `created` | 每次返回不同 `id` |

### 验证要点

- TC-CREATE-01：创建后用 `GET /api/problems/:id` 验证题目详情和测试用例数量正确
- TC-CREATE-01：测试用例的 `position` 按数组顺序自动赋值（0, 1, 2...）
- `template` 字段可选，不传时为空字符串

---

## 10. 删除题目接口测试（管理员）

**接口**：`DELETE /api/admin/problems/:id`

| 编号 | 场景 | 前置条件 | 预期状态码 | 预期 message | 后续验证 |
|------|------|---------|-----------|-------------|---------|
| TC-DELETE-01 | 正常删除 | 先创建一个题目获取 `id` | 200 | `deleted` | — |
| TC-DELETE-02 | 验证已删除 | TC-DELETE-01 后 `GET /api/problems/:id` | 404 | `problem not found` | — |
| TC-DELETE-03 | 验证列表中不存在 | TC-DELETE-01 后 `GET /api/problems` | 200 | `ok` | 列表中不含已删除的 `id` |
| TC-DELETE-04 | 删除不存在的题目 | `id=9999999` | 200 | `deleted` | — |

### 验证要点

- 删除后测试用例应级联删除（外键 `ON DELETE CASCADE`）
- 删除不存在的题目也返回 200（MySQL DELETE 对 0 行影响也成功）

---

## 11. 前端页面 UI 测试

> UI 测试基于 Selenium 或 Playwright，通过浏览器访问页面并验证 DOM 元素和交互行为。

### 11.1 落地页（`/index.html`）

| 编号 | 场景 | 操作 | 预期结果 |
|------|------|------|---------|
| TC-UI-01 | 页面加载 | 访问 `http://101.42.13.135:8080/index.html` | 页面正常加载，标题含"在线判题"，Hero 区域可见 |
| TC-UI-02 | 未登录状态导航 | 检查右上角按钮 | 显示"登录"和"注册"两个按钮 |
| TC-UI-03 | 统计数据加载 | 等待页面加载完成 | 题目数量统计显示为数字（非 0 或加载中） |

### 11.2 登录页（`/login.html`）

| 编号 | 场景 | 操作 | 预期结果 |
|------|------|------|---------|
| TC-UI-04 | 管理员登录流程 | 输入 `admin` / `admin123`，点击登录 | 显示"登录成功"提示，1秒后跳转到 `/problem_list.html` |
| TC-UI-05 | 错误密码登录 | 输入 `admin` / `wrongpwd`，点击登录 | 显示红色错误提示 `invalid username or password`，不跳转 |

### 11.3 题目列表页（`/problem_list.html`）

| 编号 | 场景 | 操作 | 预期结果 |
|------|------|------|---------|
| TC-UI-06 | 题目列表加载 | 登录后访问题目列表 | 表格显示题目行，每行含编号、标题、难度标签 |
| TC-UI-07 | 难度筛选 | 点击"简单"筛选标签 | 仅显示难度为 Easy 的题目，计数更新 |
| TC-UI-08 | 管理员标识 | 管理员登录后查看导航栏 | 显示 `admin` 徽章和"管理后台"链接 |

### 11.4 题目详情页（`/problem.html?id=<id>`）

| 编号 | 场景 | 操作 | 预期结果 |
|------|------|------|---------|
| TC-UI-09 | 页面加载与编辑器 | 访问详情页 | 左侧显示题目描述，右侧显示 Ace 代码编辑器 |
| TC-UI-10 | 提交代码 | 在编辑器中输入正确代码，点击"提交运行" | 显示加载遮罩，随后显示结果面板，状态为 AC |

---

## 12. 端到端业务流程测试

> 模拟真实用户操作流程，覆盖验收标准 1~9。

| 编号 | 场景 | 操作步骤 | 验证点 |
|------|------|---------|--------|
| TC-E2E-01 | 新用户注册→登录→登出 | 1. 注册新用户 `webtest_e2e`<br>2. 用该用户登录<br>3. 验证返回 role=user<br>4. 登出 | 注册成功→登录成功→role 正确→登出成功（验收标准 9） |
| TC-E2E-02 | 管理员创建题目→列表可见→详情正确 | 1. 管理员创建题目（含测试用例）<br>2. 查询题目列表，验证新题目出现<br>3. 查询题目详情，验证测试用例数量 | 创建→列表可见→详情完整（验收标准 1） |
| TC-E2E-03 | 完整刷题流程 | 1. 管理员创建题目<br>2. 用户提交 AC 代码<br>3. 验证 status=AC, passed=2/2<br>4. 用户提交 WA 代码<br>5. 验证 status=WA | 提交→AC 判定→WA 判定（验收标准 2,4,8） |
| TC-E2E-04 | 管理员删除题目→列表中消失 | 1. 创建题目<br>2. 删除该题目<br>3. 查询列表验证已消失<br>4. 查询详情返回 404 | 删除成功→列表消失→详情 404（验收标准 5） |
| TC-E2E-05 | 判题状态全覆盖 | 1. 创建含测试用例的题目<br>2. 分别提交 AC/WA/CE/TLE/RE 五种代码<br>3. 验证每种 status 正确 | 五种判题状态正确（验收标准 4） |

---

## 附录：Python 自动化测试脚本

以下脚本基于 `requests` 库实现全部 API 层测试，可直接运行：

```python
#!/usr/bin/env python3
"""
OJ 系统 Web 自动化测试
目标服务器: http://101.42.13.135:8080
用法: python3 web_auto_test.py
"""

import requests
import json
import sys
import time

BASE_URL = "http://101.42.13.135:8080"
ADMIN_USER = "admin"
ADMIN_PASS = "admin123"
TEST_USER = "webtest_auto"
TEST_PASS = "webtest123"
PASS = 0
FAIL = 0
CREATED_IDS = []


def assert_eq(desc, actual, expected):
    global PASS, FAIL
    if actual == expected:
        print(f"  [PASS] {desc}")
        PASS += 1
    else:
        print(f"  [FAIL] {desc}")
        print(f"         预期: {expected}")
        print(f"         实际: {actual}")
        FAIL += 1


def assert_contains(desc, text, substr):
    global PASS, FAIL
    if substr in str(text):
        print(f"  [PASS] {desc}")
        PASS += 1
    else:
        print(f"  [FAIL] {desc}")
        print(f"         应包含: {substr}")
        print(f"         实际: {text}")
        FAIL += 1


def assert_true(desc, condition):
    global PASS, FAIL
    if condition:
        print(f"  [PASS] {desc}")
        PASS += 1
    else:
        print(f"  [FAIL] {desc}")
        FAIL += 1


def post(path, body, cookies=None):
    try:
        resp = requests.post(f"{BASE_URL}{path}", json=body, cookies=cookies, timeout=30)
        return resp
    except Exception as e:
        print(f"  [ERROR] 请求异常: {e}")
        return None


def get(path, cookies=None):
    try:
        resp = requests.get(f"{BASE_URL}{path}", cookies=cookies, timeout=10)
        return resp
    except Exception as e:
        print(f"  [ERROR] 请求异常: {e}")
        return None


def delete(path, cookies=None):
    try:
        resp = requests.delete(f"{BASE_URL}{path}", cookies=cookies, timeout=10)
        return resp
    except Exception as e:
        print(f"  [ERROR] 请求异常: {e}")
        return None


def extract_sid(resp):
    cookie = resp.headers.get("Set-Cookie", "")
    import re
    m = re.search(r'oj_session=([a-f0-9]+)', cookie)
    return m.group(1) if m else ""


# =====================================================================
print("=" * 50)
print("  OJ 系统 Web 自动化测试")
print(f"  目标: {BASE_URL}")
print("=" * 50)

# 等待服务器就绪
print("\n等待服务器就绪...", end=" ")
for i in range(30):
    resp = get("/api/problems")
    if resp and resp.status_code == 200:
        print("OK")
        break
    time.sleep(1)
else:
    print("FAILED")
    sys.exit(1)

# =====================================================================
# 3. 用户注册
print("\n--- 3. 用户注册 ---")

resp = post("/api/register", {"username": TEST_USER, "password": TEST_PASS})
body = resp.json()
assert_eq("TC-REG-01 正常注册 code", body.get("code"), 200)
assert_eq("TC-REG-01 正常注册 message", body.get("message"), "registered")
assert_true("TC-REG-01 返回id为正整数", body.get("data", {}).get("id", 0) > 0)

resp = post("/api/register", {"username": TEST_USER, "password": TEST_PASS})
body = resp.json()
assert_eq("TC-REG-02 重复用户名 code", body.get("code"), 400)
assert_eq("TC-REG-02 重复用户名 message", body.get("message"), "username already exists")

resp = post("/api/register", {"username": "", "password": TEST_PASS})
body = resp.json()
assert_eq("TC-REG-03 用户名为空", body.get("message"), "username is required")

resp = post("/api/register", {"username": "ab", "password": TEST_PASS})
body = resp.json()
assert_eq("TC-REG-04 用户名过短", body.get("message"), "username must be 3-64 characters")

resp = post("/api/register", {"username": "webtest_empty", "password": ""})
body = resp.json()
assert_eq("TC-REG-05 密码为空", body.get("message"), "password is required")

resp = post("/api/register", {"username": "webtest_short", "password": "12345"})
body = resp.json()
assert_eq("TC-REG-06 密码过短", body.get("message"), "password must be 6-64 characters")

resp = requests.post(f"{BASE_URL}/api/register", data="{invalid", headers={"Content-Type": "application/json"}, timeout=10)
body = resp.json()
assert_contains("TC-REG-07 非法JSON", body.get("message", ""), "invalid JSON")

# =====================================================================
# 4. 用户登录
print("\n--- 4. 用户登录 ---")

resp = post("/api/login", {"username": ADMIN_USER, "password": ADMIN_PASS})
body = resp.json()
admin_sid = extract_sid(resp)
assert_eq("TC-LOGIN-01 管理员登录 code", body.get("code"), 200)
assert_eq("TC-LOGIN-01 管理员 role", body.get("data", {}).get("role"), "admin")
assert_true("TC-LOGIN-01 Set-Cookie", len(admin_sid) == 32)

resp = post("/api/login", {"username": TEST_USER, "password": TEST_PASS})
body = resp.json()
assert_eq("TC-LOGIN-02 普通用户登录 code", body.get("code"), 200)
assert_eq("TC-LOGIN-02 普通用户 role", body.get("data", {}).get("role"), "user")

resp = post("/api/login", {"username": ADMIN_USER, "password": "wrongpwd"})
body = resp.json()
assert_eq("TC-LOGIN-03 错误密码 code", body.get("code"), 401)
assert_eq("TC-LOGIN-03 错误密码 message", body.get("message"), "invalid username or password")

resp = post("/api/login", {"username": "nosuchuser", "password": TEST_PASS})
body = resp.json()
assert_eq("TC-LOGIN-04 不存在用户 code", body.get("code"), 401)
assert_eq("TC-LOGIN-04 不存在用户 message", body.get("message"), "invalid username or password")

resp = post("/api/login", {"username": "", "password": ADMIN_PASS})
body = resp.json()
assert_eq("TC-LOGIN-05 空用户名", body.get("message"), "username is required")

resp = post("/api/login", {"username": ADMIN_USER, "password": ""})
body = resp.json()
assert_eq("TC-LOGIN-06 空密码", body.get("message"), "password is required")

resp = post("/api/login", {"password": ADMIN_PASS})
body = resp.json()
assert_eq("TC-LOGIN-07 缺少username", body.get("message"), "username is required")

resp = requests.post(f"{BASE_URL}/api/login", data="{bad", headers={"Content-Type": "application/json"}, timeout=10)
body = resp.json()
assert_contains("TC-LOGIN-08 非法JSON", body.get("message", ""), "invalid JSON")

# =====================================================================
# 5. 用户登出
print("\n--- 5. 用户登出 ---")

cookies = {"oj_session": admin_sid}
resp = post("/api/logout", {}, cookies=cookies)
body = resp.json()
assert_eq("TC-LOGOUT-01 带 Cookie 登出 code", body.get("code"), 200)
assert_eq("TC-LOGOUT-01 带 Cookie 登出 message", body.get("message"), "logged out")
assert_contains("TC-LOGOUT-01 Set-Cookie 清除", resp.headers.get("Set-Cookie", ""), "Max-Age=0")

resp = post("/api/logout", {})
body = resp.json()
assert_eq("TC-LOGOUT-02 无 Cookie 登出", body.get("message"), "logged out")

resp = post("/api/logout", {}, cookies={"oj_session": "invalid123"})
body = resp.json()
assert_eq("TC-LOGOUT-03 无效 session 登出", body.get("message"), "logged out")

resp = post("/api/logout", {}, cookies=cookies)
body = resp.json()
assert_eq("TC-LOGOUT-04 重复登出", body.get("message"), "logged out")

# =====================================================================
# 6. 题目列表
print("\n--- 6. 题目列表 ---")

resp = get("/api/problems")
body = resp.json()
assert_eq("TC-LIST-01 题目列表 code", body.get("code"), 200)
assert_true("TC-LIST-01 data为数组", isinstance(body.get("data"), list))

if body["data"]:
    first = body["data"][0]
    assert_true("TC-LIST-01 含id字段", "id" in first)
    assert_true("TC-LIST-01 含title字段", "title" in first)
    assert_true("TC-LIST-01 含difficulty字段", "difficulty" in first)
    assert_true("TC-LIST-03 不含content字段", "content" not in first)

# =====================================================================
# 7. 新增题目（管理员）
print("\n--- 7. 新增题目 ---")

resp = post("/api/admin/problems", {
    "title": "WebTest_ABProblem",
    "difficulty": "Easy",
    "content": "输入两个整数 a 和 b，输出它们的和。",
    "template": "#include <iostream>\nint main(){}",
    "test_cases": [
        {"input": "1 2", "expected": "3"},
        {"input": "10 20", "expected": "30"}
    ]
})
body = resp.json()
assert_eq("TC-CREATE-01 正常创建 code", body.get("code"), 200)
assert_eq("TC-CREATE-01 正常创建 message", body.get("message"), "created")
test_pid = body.get("data", {}).get("id", 0)
assert_true("TC-CREATE-01 返回id为正整数", test_pid > 0)
CREATED_IDS.append(test_pid)

resp = post("/api/admin/problems", {
    "title": "WebTest_NoTc",
    "difficulty": "Medium",
    "content": "无测试用例题目"
})
body = resp.json()
assert_eq("TC-CREATE-02 无测试用例创建 code", body.get("code"), 200)
no_tc_pid = body.get("data", {}).get("id", 0)
CREATED_IDS.append(no_tc_pid)

resp = post("/api/admin/problems", {"difficulty": "Easy", "content": "缺标题"})
body = resp.json()
assert_eq("TC-CREATE-03 缺少title", body.get("message"), "title is required")

resp = post("/api/admin/problems", {"title": "WebTest_NoContent", "difficulty": "Easy"})
body = resp.json()
assert_eq("TC-CREATE-04 缺少content", body.get("message"), "content is required")

resp = post("/api/admin/problems", {"title": "WebTest_BadDiff", "difficulty": "SuperHard", "content": "x"})
body = resp.json()
assert_eq("TC-CREATE-05 无效difficulty", body.get("message"), "invalid difficulty")

resp = requests.post(f"{BASE_URL}/api/admin/problems", data="{bad json", headers={"Content-Type": "application/json"}, timeout=10)
body = resp.json()
assert_contains("TC-CREATE-06 非法JSON", body.get("message", ""), "invalid JSON")

for diff in ["Easy", "Medium", "Hard"]:
    resp = post("/api/admin/problems", {"title": f"WebTest_Diff_{diff}", "difficulty": diff, "content": "x"})
    body = resp.json()
    assert_eq(f"TC-CREATE-07 难度{diff}创建", body.get("code"), 200)
    if body.get("data", {}).get("id"):
        CREATED_IDS.append(body["data"]["id"])

# =====================================================================
# 8. 题目详情
print("\n--- 8. 题目详情 ---")

resp = get(f"/api/problems/{test_pid}")
body = resp.json()
data = body.get("data", {})
assert_eq("TC-DETAIL-01 详情 code", body.get("code"), 200)
assert_eq("TC-DETAIL-01 title", data.get("title"), "WebTest_ABProblem")
assert_true("TC-DETAIL-01 含test_cases", "test_cases" in data)
assert_eq("TC-DETAIL-01 测试用例数", len(data.get("test_cases", [])), 2)

resp = get(f"/api/problems/{no_tc_pid}")
body = resp.json()
data = body.get("data", {})
assert_eq("TC-DETAIL-02 无测试用例详情 code", body.get("code"), 200)
assert_eq("TC-DETAIL-02 test_cases为空", len(data.get("test_cases", [])), 0)

resp = get("/api/problems/9999999")
body = resp.json()
assert_eq("TC-DETAIL-03 不存在 code", body.get("code"), 404)
assert_eq("TC-DETAIL-03 不存在 message", body.get("message"), "problem not found")

# 验证测试用例排序
resp = get(f"/api/problems/{test_pid}")
body = resp.json()
cases = body.get("data", {}).get("test_cases", [])
if len(cases) >= 2:
    assert_true("TC-DETAIL-04 用例按position排序", cases[0]["position"] <= cases[1]["position"])

# =====================================================================
# 9. 提交代码执行
print("\n--- 9. 提交代码执行 ---")

ac_code = "#include <iostream>\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b<<std::endl;return 0;}"
wa_code = "#include <iostream>\nint main(){int a,b;std::cin>>a>>b;std::cout<<a*b<<std::endl;return 0;}"
ce_code = "int main(){ syntax error }"
tle_code = "int main(){while(1){}return 0;}"
re_code = "int main(){int*p=nullptr;*p=42;return 0;}"

resp = post("/api/submit", {"problem_id": test_pid, "code": ac_code})
body = resp.json()
data = body.get("data", {})
assert_eq("TC-SUBMIT-01 AC status", data.get("status"), "AC")
assert_eq("TC-SUBMIT-01 AC passed", data.get("passed"), 2)

resp = post("/api/submit", {"problem_id": test_pid, "code": wa_code})
body = resp.json()
data = body.get("data", {})
assert_eq("TC-SUBMIT-02 WA status", data.get("status"), "WA")
assert_eq("TC-SUBMIT-02 WA passed", data.get("passed"), 0)

resp = post("/api/submit", {"problem_id": test_pid, "code": ce_code})
body = resp.json()
data = body.get("data", {})
assert_eq("TC-SUBMIT-03 CE status", data.get("status"), "CE")
assert_true("TC-SUBMIT-03 CE 含compile_output", len(data.get("compile_output", "")) > 0)
assert_eq("TC-SUBMIT-03 CE cases为空", len(data.get("cases", [])), 0)

resp = post("/api/submit", {"problem_id": test_pid, "code": tle_code})
body = resp.json()
data = body.get("data", {})
assert_eq("TC-SUBMIT-04 TLE status", data.get("status"), "TLE")

resp = post("/api/submit", {"problem_id": test_pid, "code": re_code})
body = resp.json()
data = body.get("data", {})
assert_eq("TC-SUBMIT-05 RE status", data.get("status"), "RE")
if data.get("cases"):
    assert_contains("TC-SUBMIT-05 RE error", data["cases"][0].get("error", ""), "segmentation fault")

resp = post("/api/submit", {"problem_id": 9999999, "code": "int main(){}"})
body = resp.json()
assert_eq("TC-SUBMIT-06 题目不存在 code", body.get("code"), 404)
assert_eq("TC-SUBMIT-06 题目不存在 message", body.get("message"), "problem not found")

resp = post("/api/submit", {"problem_id": test_pid})
body = resp.json()
assert_eq("TC-SUBMIT-07 缺少code code", body.get("code"), 400)
assert_eq("TC-SUBMIT-07 缺少code message", body.get("message"), "code is required")

resp = post("/api/submit", {"code": "int main(){}"})
body = resp.json()
assert_eq("TC-SUBMIT-08 缺少problem_id code", body.get("code"), 400)
assert_eq("TC-SUBMIT-08 缺少problem_id message", body.get("message"), "problem_id is required")

resp = requests.post(f"{BASE_URL}/api/submit", data="{bad", headers={"Content-Type": "application/json"}, timeout=10)
body = resp.json()
assert_contains("TC-SUBMIT-09 非法JSON", body.get("message", ""), "invalid JSON")

# =====================================================================
# 10. 删除题目
print("\n--- 10. 删除题目 ---")

resp = delete(f"/api/admin/problems/{test_pid}")
body = resp.json()
assert_eq("TC-DELETE-01 正常删除 code", body.get("code"), 200)
assert_eq("TC-DELETE-01 正常删除 message", body.get("message"), "deleted")

resp = get(f"/api/problems/{test_pid}")
body = resp.json()
assert_eq("TC-DELETE-02 删除后查询 code", body.get("code"), 404)
assert_eq("TC-DELETE-02 删除后查询 message", body.get("message"), "problem not found")

resp = get("/api/problems")
body = resp.json()
ids = [p["id"] for p in body.get("data", [])]
assert_true("TC-DELETE-03 列表中不存在", test_pid not in ids)

resp = delete("/api/admin/problems/9999999")
body = resp.json()
assert_eq("TC-DELETE-04 删除不存在 code", body.get("code"), 200)

# 清理测试数据
for pid in CREATED_IDS:
    if pid != test_pid:
        delete(f"/api/admin/problems/{pid}")

# =====================================================================
# 12. 端到端流程
print("\n--- 12. 端到端流程 ---")

# E2E-01: 注册→登录→登出
e2e_user = "webtest_e2e_flow"
resp = post("/api/register", {"username": e2e_user, "password": TEST_PASS})
assert_eq("TC-E2E-01 注册成功", resp.json().get("code"), 200)

resp = post("/api/login", {"username": e2e_user, "password": TEST_PASS})
body = resp.json()
assert_eq("TC-E2E-01 登录成功", body.get("code"), 200)
assert_eq("TC-E2E-01 role正确", body.get("data", {}).get("role"), "user")

e2e_sid = extract_sid(resp)
resp = post("/api/logout", {}, cookies={"oj_session": e2e_sid})
assert_eq("TC-E2E-01 登出成功", resp.json().get("message"), "logged out")

# E2E-02: 管理员创建→列表可见→详情正确
resp = post("/api/admin/problems", {
    "title": "WebTest_E2E_Create",
    "difficulty": "Easy",
    "content": "E2E测试题目",
    "test_cases": [{"input": "1", "expected": "2"}]
})
e2e_pid = resp.json().get("data", {}).get("id", 0)
assert_true("TC-E2E-02 创建成功", e2e_pid > 0)

resp = get("/api/problems")
titles = [p["title"] for p in resp.json().get("data", [])]
assert_true("TC-E2E-02 列表可见", "WebTest_E2E_Create" in titles)

resp = get(f"/api/problems/{e2e_pid}")
cases = resp.json().get("data", {}).get("test_cases", [])
assert_eq("TC-E2E-02 详情测试用例数", len(cases), 1)

# E2E-04: 删除→列表消失→404
resp = delete(f"/api/admin/problems/{e2e_pid}")
assert_eq("TC-E2E-04 删除成功", resp.json().get("code"), 200)

resp = get(f"/api/problems/{e2e_pid}")
assert_eq("TC-E2E-04 详情404", resp.json().get("code"), 404)

# E2E-05: 判题状态全覆盖
resp = post("/api/admin/problems", {
    "title": "WebTest_E2E_Judge",
    "difficulty": "Medium",
    "content": "判题测试",
    "test_cases": [{"input": "1 2", "expected": "3"}]
})
judge_pid = resp.json().get("data", {}).get("id", 0)

for desc, code, expected_status in [
    ("AC", ac_code, "AC"),
    ("WA", wa_code, "WA"),
    ("CE", ce_code, "CE"),
    ("TLE", tle_code, "TLE"),
    ("RE", re_code, "RE"),
]:
    resp = post("/api/submit", {"problem_id": judge_pid, "code": code})
    status = resp.json().get("data", {}).get("status")
    assert_eq(f"TC-E2E-05 判题{desc}", status, expected_status)

delete(f"/api/admin/problems/{judge_pid}")

# =====================================================================
print()
print("=" * 50)
print(f"  测试结果: PASS={PASS}  FAIL={FAIL}")
print("=" * 50)

if FAIL > 0:
    sys.exit(1)
```

---

## 测试执行方式

### 方式一：直接运行 Python 脚本

```bash
# 安装依赖
pip install requests

# 运行测试
python3 web_auto_test.py
```

### 方式二：保存脚本到文件后运行

```bash
# 将附录中的脚本保存为 web_auto_test.py
python3 web_auto_test.py
```

### 预期输出

```
==================================================
  OJ 系统 Web 自动化测试
  目标: http://101.42.13.135:8080
==================================================

等待服务器就绪... OK

--- 3. 用户注册 ---
  [PASS] TC-REG-01 正常注册 code
  [PASS] TC-REG-01 正常注册 message
  ...

--- 12. 端到端流程 ---
  [PASS] TC-E2E-01 注册成功
  ...

==================================================
  测试结果: PASS=XX  FAIL=0
==================================================
```
