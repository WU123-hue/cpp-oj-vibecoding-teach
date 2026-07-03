# OJ 系统 Web 自动化测试文档 — Playwright 篇

> **测试目标服务器**：`http://101.42.13.135:8080`
>
> **管理员账号**：用户名 `admin`，密码 `admin123`
>
> **测试工具**：Playwright CLI（`playwright-cli` v0.1.15）
>
> **运行模式**：有头模式（`--headed`），每步操作间停顿 1 秒，便于肉眼观察
>
> **文档用途**：记录使用 Playwright CLI 对 OJ 系统前端页面及端到端业务流程的自动化测试过程，重点描述操作步骤与验证方法。

---

## 目录

- [1. 测试环境与前置条件](#1-测试环境与前置条件)
- [2. 测试用例总览](#2-测试用例总览)
- [3. 前端页面 UI 测试](#3-前端页面-ui-测试)
  - [3.1 落地页测试（TC-UI-01 ~ TC-UI-03）](#31-落地页测试tc-ui-01--tc-ui-03)
  - [3.2 登录页测试（TC-UI-04 ~ TC-UI-05）](#32-登录页测试tc-ui-04--tc-ui-05)
  - [3.3 题目列表页测试（TC-UI-06 ~ TC-UI-08）](#33-题目列表页测试tc-ui-06--tc-ui-08)
  - [3.4 题目详情页测试（TC-UI-09 ~ TC-UI-10）](#34-题目详情页测试tc-ui-09--tc-ui-10)
- [4. 端到端业务流程测试](#4-端到端业务流程测试)
  - [4.1 TC-E2E-01：新用户注册→登录→登出](#41-tc-e2e-01新用户注册登录登出)
  - [4.2 TC-E2E-02：管理员创建题目→列表可见→详情正确](#42-tc-e2e-02管理员创建题目列表可见详情正确)
  - [4.3 TC-E2E-03/05：完整刷题流程与判题状态全覆盖](#43-tc-e2e-0305完整刷题流程与判题状态全覆盖)
  - [4.4 TC-E2E-04：管理员删除题目→列表中消失→404](#44-tc-e2e-04管理员删除题目列表中消失404)
- [5. 测试结果汇总](#5-测试结果汇总)
- [6. 技术备注](#6-技术备注)

---

## 1. 测试环境与前置条件

### 1.1 服务器信息

| 项 | 值 |
|------|------|
| 服务器地址 | `http://101.42.13.135:8080` |
| 管理员用户名 | `admin` |
| 管理员密码 | `admin123` |
| 管理员角色 | `admin` |

### 1.2 工具与环境

| 项 | 值 |
|------|------|
| 测试工具 | Playwright CLI（`@playwright/cli` v0.1.15） |
| 浏览器 | Chrome（`--browser=chrome`） |
| 运行模式 | 有头模式（`--headed`），可见浏览器窗口 |
| 操作系统 | Windows |
| 命令调用 | 通过 `cmd.exe /c "playwright-cli ..."` 执行（绕过 PowerShell 执行策略限制） |
| 操作节奏 | 每个操作之间停顿 1 秒（`Start-Sleep -Seconds 1`），便于肉眼观察 |

### 1.3 前置条件

1. 服务器已启动且可通过 `http://101.42.13.135:8080` 访问
2. 管理员账户已初始化（`admin` / `admin123`）
3. Playwright CLI 已全局安装：`npm install -g @playwright/cli@latest`
4. 浏览器已以有头模式启动并导航到目标地址

### 1.4 测试数据命名约定

| 类型 | 前缀 | 示例 |
|------|------|------|
| 测试用户名 | `webtest_e2e_` | `webtest_e2e_1783077326401`（附时间戳避免重复） |
| 测试题目标题 | `WebTest_E2E_` | `WebTest_E2E_UI` |
| 测试密码 | `webtest123` | 固定使用 |

---

## 2. 测试用例总览

| 编号 | 模块 | 用例数 | 说明 |
|------|------|--------|------|
| TC-UI-01~03 | 落地页 | 3 | 页面加载、导航按钮、统计数据 |
| TC-UI-04~05 | 登录页 | 2 | 管理员登录流程、错误密码登录 |
| TC-UI-06~08 | 题目列表页 | 3 | 列表加载、难度筛选、管理员标识 |
| TC-UI-09~10 | 题目详情页 | 2 | 编辑器加载、提交代码 |
| TC-E2E-01 | 端到端 | 1 | 注册→登录→登出完整流程 |
| TC-E2E-02 | 端到端 | 1 | 管理员创建题目→列表可见→详情正确 |
| TC-E2E-03/05 | 端到端 | 5 | AC/WA/CE/TLE/RE 五种判题状态 |
| TC-E2E-04 | 端到端 | 1 | 删除题目→列表消失→404 |
| **合计** | | **18** | **全部通过** |

---

## 3. 前端页面 UI 测试

### 3.1 落地页测试（TC-UI-01 ~ TC-UI-03）

**页面地址**：`http://101.42.13.135:8080/index.html`

#### 操作步骤

```bash
# 步骤1：以有头模式打开 Chrome 并导航到落地页
playwright-cli open --browser=chrome --headed http://101.42.13.135:8080/index.html

# 步骤2：等待1秒后获取页面快照，验证页面元素
Start-Sleep -Seconds 1
playwright-cli snapshot
```

#### 验证结果

| 编号 | 场景 | 验证内容 | 预期结果 | 实际结果 |
|------|------|---------|---------|---------|
| TC-UI-01 | 页面加载 | 页面标题、Hero 区域 | 标题含"在线判题"，Hero 区域可见 | ✅ 标题"OJ — 在线判题系统"，Hero 区域"在线代码判题 实时编译运行" |
| TC-UI-02 | 未登录导航 | 右上角按钮 | 显示"登录"和"注册"按钮 | ✅ 导航栏含"登录"链接和"注册"链接 |
| TC-UI-03 | 统计数据 | 题目数量统计 | 显示为数字（非 0 或加载中） | ✅ 题目数量显示为"3" |

#### 关键快照元素

```yaml
- heading "在线代码判题 实时编译运行" [level=1]    # Hero 区域
- link "登录"                                       # 未登录导航按钮
- link "注册"                                       # 未登录导航按钮
- generic: "3"                                      # 题目数量统计
- generic: 题目数量
```

---

### 3.2 登录页测试（TC-UI-04 ~ TC-UI-05）

**页面地址**：`http://101.42.13.135:8080/login.html`

#### TC-UI-04：管理员登录流程

```bash
# 步骤1：导航到登录页
Start-Sleep -Seconds 1
playwright-cli goto http://101.42.13.135:8080/login.html

# 步骤2：获取快照，定位用户名输入框 ref
playwright-cli snapshot
# 快照显示：textbox "用户名" [ref=f1e16]

# 步骤3：填写用户名
Start-Sleep -Seconds 1
playwright-cli fill f1e16 admin

# 步骤4：填写密码
Start-Sleep -Seconds 1
playwright-cli fill f1e20 admin123

# 步骤5：点击登录按钮
Start-Sleep -Seconds 1
playwright-cli click f1e21

# 步骤6：验证跳转
playwright-cli snapshot
# 页面 URL 应从 /login.html 跳转到 /problem_list.html
```

**验证结果**：

| 验证点 | 预期 | 实际 |
|--------|------|------|
| 页面跳转 | 跳转到 `/problem_list.html` | ✅ URL 变为 `http://101.42.13.135:8080/problem_list.html` |
| 页面标题 | `OJ — 题目列表` | ✅ 匹配 |

#### TC-UI-05：错误密码登录

```bash
# 前置：先退出当前登录状态（点击导航栏"退出"链接）
Start-Sleep -Seconds 1
playwright-cli click f2e19
# 页面跳转回 /login.html

# 步骤1：获取登录页快照，定位输入框 ref
playwright-cli snapshot
# 快照显示：textbox "用户名" [ref=f3e16], textbox "密码" [ref=f3e20], button "登录" [ref=f3e21]

# 步骤2：填写用户名
Start-Sleep -Seconds 1
playwright-cli fill f3e16 admin

# 步骤3：填写错误密码
Start-Sleep -Seconds 1
playwright-cli fill f3e20 wrongpwd

# 步骤4：点击登录
Start-Sleep -Seconds 1
playwright-cli click f3e21

# 步骤5：验证错误提示
playwright-cli snapshot
```

**验证结果**：

| 验证点 | 预期 | 实际 |
|--------|------|------|
| 页面不跳转 | 保持在 `/login.html` | ✅ URL 仍为 `/login.html` |
| 错误提示 | 红色提示 `invalid username or password` | ✅ 快照中出现 `alert` 元素，文本为 `invalid username or password` |

**关键快照元素**：

```yaml
- alert [ref=f3e26]:                    # 错误提示为 alert 类型
  - img [ref=f3e27]
  - generic [ref=f3e29]: invalid username or password
```

---

### 3.3 题目列表页测试（TC-UI-06 ~ TC-UI-08）

**页面地址**：`http://101.42.13.135:8080/problem_list.html`

> 前置条件：已用管理员账号登录。

#### TC-UI-06：题目列表加载

```bash
# 管理员登录后自动跳转到题目列表页，直接获取快照
playwright-cli snapshot
```

**验证结果**：

| 验证点 | 预期 | 实际 |
|--------|------|------|
| 题目行显示 | 每行含编号、标题、难度 | ✅ 显示 #85、#86、#87 三道题，每行含编号、标题、难度标签 |
| 计数 | "共 N 题" | ✅ "共 3 题" |

#### TC-UI-07：难度筛选

```bash
# 步骤1：点击"简单"筛选按钮
Start-Sleep -Seconds 1
playwright-cli click f2e29

# 步骤2：验证筛选结果
playwright-cli snapshot
```

**验证结果**：

| 验证点 | 预期 | 实际 |
|--------|------|------|
| 按钮状态 | "简单"按钮变为 active | ✅ `button "简单" [active]` |
| 列表内容 | 仅显示 Easy 难度题目 | ✅ 列表仍显示 3 道"简单"题目（均为 Easy） |

#### TC-UI-08：管理员标识

```bash
# 直接在题目列表页快照中验证导航栏
playwright-cli snapshot
```

**验证结果**：

| 验证点 | 预期 | 实际 |
|--------|------|------|
| admin 徽章 | 显示 `admin` 文字徽章 | ✅ `generic: admin` |
| 管理后台链接 | 显示"管理后台"链接 | ✅ `link "管理后台" -> /admin.html` |

**关键快照元素**：

```yaml
- link "管理后台" [ref=f2e11]:          # 管理员专属链接
  - /url: /admin.html
- generic [ref=f2e15]:
  - generic [ref=f2e16]: admin          # admin 徽章
  - generic [ref=f2e17]: A              # 头像首字母
  - generic [ref=f2e18]: admin          # 用户名
```

---

### 3.4 题目详情页测试（TC-UI-09 ~ TC-UI-10）

**页面地址**：`http://101.42.13.135:8080/problem.html?id=85`

#### TC-UI-09：页面加载与编辑器

```bash
# 步骤1：在题目列表页点击第一道题
Start-Sleep -Seconds 1
playwright-cli click f2e42
# 跳转到 /problem.html?id=85

# 步骤2：等待页面加载（初次显示"加载中..."，需等待2秒）
Start-Sleep -Seconds 2
playwright-cli snapshot
```

**验证结果**：

| 验证点 | 预期 | 实际 |
|--------|------|------|
| 题目标题 | 显示标题和编号 | ✅ `#85 A+B Problem` |
| 题目描述 | 左侧显示描述内容 | ✅ 显示"求和" |
| 代码编辑器 | 右侧显示 Ace 编辑器 | ✅ 含行号、代码模板、语法高亮 |
| 测试用例数 | 显示数量 | ✅ "1" |
| 创建时间 | YYYY-MM-DD HH:MM:SS 格式 | ✅ `2026-07-02 20:14:44` |

#### TC-UI-10：提交代码

```bash
# 步骤1：通过 Ace Editor API 写入 AC 代码
# （Ace 编辑器的 textarea 被遮挡，无法直接 fill，需用 evaluate 调用 Ace API）
Start-Sleep -Seconds 1

# 将以下代码写入文件 set_ac_code.js，然后执行：
# async page => {
#   await page.evaluate(() => {
#     const editor = ace.edit(document.querySelector('.ace_editor'));
#     editor.setValue('#include <iostream>\nusing namespace std;\nint main() {\n    int a, b;\n    cin >> a >> b;\n    cout << a + b << endl;\n    return 0;\n}', -1);
#   });
# }

playwright-cli run-code --filename=set_ac_code.js

# 步骤2：点击"提交运行"按钮
Start-Sleep -Seconds 1
playwright-cli click f5e52

# 步骤3：等待判题结果（3秒）
Start-Sleep -Seconds 3
playwright-cli snapshot
```

**验证结果**：

| 验证点 | 预期 | 实际 |
|--------|------|------|
| 判题状态 | Accepted | ✅ `text: Accepted` |
| 通过数量 | 1/1 | ✅ `1/1 通过` |
| 耗时 | 显示毫秒数 | ✅ `11ms 最大耗时` |
| 用例结果 | 用例 #1 AC | ✅ `用例 #1 AC 11ms` |

---

## 4. 端到端业务流程测试

### 4.1 TC-E2E-01：新用户注册→登录→登出

**目标**：通过浏览器 UI 完成完整的用户生命周期流程

#### 步骤1：注册新用户

```bash
# 导航到注册页
Start-Sleep -Seconds 1
playwright-cli goto http://101.42.13.135:8080/register.html

# 获取快照，定位输入框
playwright-cli snapshot
# textbox "用户名" [ref=f6e16], textbox "密码" [ref=f6e20], textbox "确认密码" [ref=f6e26], button "注册" [ref=f6e27]

# 填写用户名（使用时间戳避免重复）
Start-Sleep -Seconds 1
playwright-cli fill f6e16 webtest_e2e_1783077326401

# 填写密码
Start-Sleep -Seconds 1
playwright-cli fill f6e20 webtest123

# 填写确认密码
Start-Sleep -Seconds 1
playwright-cli fill f6e26 webtest123

# 点击注册按钮
Start-Sleep -Seconds 1
playwright-cli click f6e27

# 验证：注册成功后自动跳转到登录页
Start-Sleep -Seconds 2
playwright-cli snapshot
# URL 从 /register.html 跳转到 /login.html
```

#### 步骤2：用新用户登录

```bash
# 在跳转后的登录页填写用户名
Start-Sleep -Seconds 1
playwright-cli fill f7e16 webtest_e2e_1783077326401

# 填写密码
Start-Sleep -Seconds 1
playwright-cli fill f7e20 webtest123

# 点击登录
Start-Sleep -Seconds 1
playwright-cli click f7e21

# 验证：登录成功，跳转到题目列表页
playwright-cli snapshot
# URL 跳转到 /problem_list.html
```

#### 步骤3：验证普通用户身份

```yaml
# 快照验证：导航栏不显示"管理后台"链接（普通用户无管理权限）
- generic [ref=f8e11]:
  - generic [ref=f8e12]: W                                   # 头像首字母
  - generic [ref=f8e13]: webtest_e2e_1783077326401           # 用户名
- link "退出" [ref=f8e14]                                     # 仅有退出链接，无管理后台
```

#### 步骤4：登出

```bash
# 点击"退出"链接
Start-Sleep -Seconds 1
playwright-cli click f8e14

# 验证：跳转回登录页
playwright-cli snapshot
# URL 跳转到 /login.html
```

**验证结果**：

| 步骤 | 验证点 | 预期 | 实际 |
|------|--------|------|------|
| 注册 | 跳转到登录页 | 注册成功后自动跳转 | ✅ URL 变为 `/login.html` |
| 登录 | 跳转到题目列表 | 登录成功后跳转 | ✅ URL 变为 `/problem_list.html` |
| 角色 | 无管理后台链接 | 普通用户无管理权限 | ✅ 导航栏仅显示用户名和"退出"，无"管理后台" |
| 登出 | 跳转回登录页 | 登出成功 | ✅ URL 变为 `/login.html` |

---

### 4.2 TC-E2E-02：管理员创建题目→列表可见→详情正确

**目标**：通过管理后台 UI 创建题目，验证列表和详情

#### 步骤1：管理员登录

```bash
# 在登录页填写管理员账号
Start-Sleep -Seconds 1
playwright-cli fill f9e16 admin
Start-Sleep -Seconds 1
playwright-cli fill f9e20 admin123
Start-Sleep -Seconds 1
playwright-cli click f9e21
# 跳转到 /problem_list.html
```

#### 步骤2：进入管理后台

```bash
# 导航到管理后台
Start-Sleep -Seconds 1
playwright-cli goto http://101.42.13.135:8080/admin.html

# 获取快照，定位表单元素
playwright-cli snapshot
# textbox "题目标题" [ref=f11e32]
# combobox "难度" [ref=f11e35]
# textbox "题目描述" [ref=f11e38]
# textbox "代码模板（可选）" [ref=f11e41]
# textbox "输入数据" [ref=f11e47], textbox "期望输出" [ref=f11e48]
# button "添加测试用例" [ref=f11e52]
# button "创建题目" [ref=f11e56]
```

#### 步骤3：填写题目信息

```bash
# 填写标题
Start-Sleep -Seconds 1
playwright-cli fill f11e32 WebTest_E2E_UI

# 填写描述（含空格/中文，需通过 run-code 方式填写）
# 将以下代码写入文件 fill_desc.js：
# async page => {
#   await page.getByRole('textbox', {name: '题目描述'}).fill('输入两个整数 a 和 b，输出它们的和。');
# }
Start-Sleep -Seconds 1
playwright-cli run-code --filename=fill_desc.js

# 填写第一个测试用例（含空格，需通过 run-code 方式填写）
# 将以下代码写入文件 fill_tc1.js：
# async page => {
#   await page.locator('aria-ref=f11e47').fill('1 2');
#   await page.locator('aria-ref=f11e48').fill('3');
# }
Start-Sleep -Seconds 1
playwright-cli run-code --filename=fill_tc1.js

# 点击"添加测试用例"按钮
Start-Sleep -Seconds 1
playwright-cli click f11e52

# 获取新测试用例的 ref
playwright-cli snapshot
# 新增：textbox "输入数据" [ref=f11e133], textbox "期望输出" [ref=f11e134]

# 填写第二个测试用例
# 将以下代码写入文件 fill_tc2.js：
# async page => {
#   await page.locator('aria-ref=f11e133').fill('10 20');
#   await page.locator('aria-ref=f11e134').fill('30');
# }
Start-Sleep -Seconds 1
playwright-cli run-code --filename=fill_tc2.js
```

#### 步骤4：点击"创建题目"

```bash
Start-Sleep -Seconds 1
playwright-cli click f11e56

# 验证：显示"题目创建成功"提示，题目管理列表出现新题目
playwright-cli snapshot
```

#### 步骤5：验证题目列表

```bash
# 导航到题目列表页
Start-Sleep -Seconds 1
playwright-cli goto http://101.42.13.135:8080/problem_list.html

# 验证列表中包含新创建的题目
playwright-cli snapshot
# "共 5 题"，列表中包含 #89 WebTest_E2E_UI
```

#### 步骤6：验证题目详情

```bash
# 点击新创建的题目进入详情页
Start-Sleep -Seconds 1
playwright-cli click f12e94
# 跳转到 /problem.html?id=89

# 等待页面加载
Start-Sleep -Seconds 2
playwright-cli snapshot
```

**验证结果**：

| 步骤 | 验证点 | 预期 | 实际 |
|------|--------|------|------|
| 创建 | 成功提示 | "题目创建成功" | ✅ 快照底部显示 `generic: 题目创建成功` |
| 创建 | 管理列表 | 出现新题目 #89 | ✅ 题目管理列表中出现 `#89 WebTest_E2E_UI` |
| 列表 | 题目可见 | 列表包含新题目 | ✅ "共 5 题"，包含 `#89 WebTest_E2E_UI` |
| 详情 | 标题 | WebTest_E2E_UI | ✅ `heading "WebTest_E2E_UI"` |
| 详情 | 描述 | 输入两个整数 a 和 b，输出它们的和。 | ✅ 匹配 |
| 详情 | 测试用例数 | 2 | ✅ `"2"` |

---

### 4.3 TC-E2E-03/05：完整刷题流程与判题状态全覆盖

**目标**：在同一题目详情页，依次提交 AC/WA/CE/TLE/RE 五种代码，验证判题状态

**前置条件**：已创建题目 #89（含 2 个测试用例：`1 2`→`3`，`10 20`→`30`），当前在详情页

#### 测试代码

| 状态 | 代码 | 说明 |
|------|------|------|
| AC | `#include <iostream>\nusing namespace std;\nint main() {\n    int a, b;\n    cin >> a >> b;\n    cout << a + b << endl;\n    return 0;\n}` | A+B 正确实现 |
| WA | `#include <iostream>\nusing namespace std;\nint main() {\n    int a, b;\n    cin >> a >> b;\n    cout << a * b << endl;\n    return 0;\n}` | A*B 错误输出 |
| CE | `int main(){ syntax error }` | 编译错误 |
| TLE | `int main(){while(1){}return 0;}` | 死循环超时 |
| RE | `int main(){int*p=nullptr;*p=42;return 0;}` | 空指针解引用 |

#### 操作步骤（以 AC 为例，其余重复相同流程）

```bash
# 步骤1：通过 Ace Editor API 写入代码
# 将代码写入文件 set_ac_code.js：
# async page => {
#   await page.evaluate(() => {
#     const editor = ace.edit(document.querySelector('.ace_editor'));
#     editor.setValue('#include <iostream>\nusing namespace std;\nint main() { ... }', -1);
#   });
# }
Start-Sleep -Seconds 1
playwright-cli run-code --filename=set_ac_code.js

# 步骤2：点击"提交运行"
Start-Sleep -Seconds 1
playwright-cli click f13e67

# 步骤3：等待判题结果
# AC/WA/CE/RE 等待 3 秒；TLE 需等待 8 秒（5 秒超时 + 网络延迟）
Start-Sleep -Seconds 3    # TLE 时改为 Start-Sleep -Seconds 8
playwright-cli snapshot
```

#### 五种状态验证结果

| 状态 | 预期显示 | 通过数 | 实际结果 | 耗时 |
|------|---------|--------|---------|------|
| AC | Accepted | 2/2 | ✅ `text: Accepted`，`2/2 通过` | 10ms |
| WA | Wrong Answer | 0/2 | ✅ `text: Wrong Answer`，`0/2 通过` | 11ms |
| CE | Compile Error | 0/2 | ✅ `text: Compile Error`，含编译错误信息 | 0ms |
| TLE | Time Limit Exceeded | 0/2 | ✅ `text: Time Limit Exceeded`，`0/2 通过` | 2503ms |
| RE | Runtime Error | 0/2 | ✅ `text: Runtime Error`，`0/2 通过` | 172ms |

#### 各状态快照关键元素

**AC 结果**：
```yaml
- generic [ref=f13e76]:
  - text: Accepted
- generic [ref=f13e80]: 2/2
- generic [ref=f13e81]: 通过
- generic [ref=f13e83]: 10ms
- generic [ref=f13e84]: 最大耗时
- generic [ref=f13e88]: "用例 #1 AC"
- generic [ref=f13e89]: 10ms
- generic [ref=f13e92]: "用例 #2 AC"
- generic [ref=f13e93]: 10ms
```

**CE 结果**（含编译错误信息）：
```yaml
- generic [ref=f13e76]:
  - text: Compile Error
- generic [ref=f13e111]: 0/2
- generic [ref=f13e114]: 0ms
- generic [ref=f13e116]: "/tmp/oj_exec_BV8pou/main.cpp: In function 'int main()':
    /tmp/oj_exec_BV8pou/main.cpp:1:13: error: 'syntax' was not declared in this scope
    1 | int main(){ syntax error }
      |             ^~~~~~"
```

**TLE 结果**：
```yaml
- generic [ref=f13e76]:
  - text: Time Limit Exceeded
- generic [ref=f13e119]: 0/2
- generic [ref=f13e122]: 2503ms
- generic [ref=f13e126]: "用例 #1 TLE"
- generic [ref=f13e127]: 2503ms
- generic [ref=f13e130]: "用例 #2 TLE"
- generic [ref=f13e131]: 2160ms
```

---

### 4.4 TC-E2E-04：管理员删除题目→列表中消失→404

**目标**：通过管理后台 UI 删除题目，验证列表和详情页

#### 步骤1：进入管理后台

```bash
Start-Sleep -Seconds 1
playwright-cli goto http://101.42.13.135:8080/admin.html

# 等待题目管理列表加载
Start-Sleep -Seconds 2
playwright-cli snapshot
# 定位 #89 的删除按钮 [ref=f14e139]
```

#### 步骤2：点击删除按钮

```bash
Start-Sleep -Seconds 1
playwright-cli click f14e139

# 弹出确认对话框
playwright-cli snapshot
# heading "确认删除" [ref=f14e147]
# button "取消" [ref=f14e150]
# button "删除" [ref=f14e151]
```

#### 步骤3：确认删除

```bash
Start-Sleep -Seconds 1
playwright-cli click f14e151

# 验证：显示"删除成功"提示，#89 从管理列表消失
Start-Sleep -Seconds 1
playwright-cli snapshot
```

#### 步骤4：验证题目列表

```bash
# 导航到题目列表页
Start-Sleep -Seconds 1
playwright-cli goto http://101.42.13.135:8080/problem_list.html

playwright-cli snapshot
# "共 4 题"，#89 不在列表中
```

#### 步骤5：验证详情页 404

```bash
# 直接访问已删除题目的详情页
Start-Sleep -Seconds 1
playwright-cli goto http://101.42.13.135:8080/problem.html?id=89

# 等待页面加载
Start-Sleep -Seconds 2
playwright-cli snapshot
```

**验证结果**：

| 步骤 | 验证点 | 预期 | 实际 |
|------|--------|------|------|
| 删除 | 成功提示 | "删除成功" | ✅ `generic: 删除成功` |
| 管理列表 | #89 消失 | 不在列表中 | ✅ 列表仅剩 #85~#88 |
| 题目列表 | 共 4 题 | #89 不在列表 | ✅ "共 4 题"，无 #89 |
| 详情页 | problem not found | 显示 404 信息 | ✅ `generic: problem not found` |

---

## 5. 测试结果汇总

### 5.1 前端页面 UI 测试

| 编号 | 场景 | 结果 |
|------|------|------|
| TC-UI-01 | 落地页加载，标题含"在线判题" | ✅ PASS |
| TC-UI-02 | 未登录显示"登录""注册"按钮 | ✅ PASS |
| TC-UI-03 | 题目数量统计为数字 | ✅ PASS |
| TC-UI-04 | 管理员登录成功跳转 problem_list | ✅ PASS |
| TC-UI-05 | 错误密码显示 `invalid username or password` | ✅ PASS |
| TC-UI-06 | 题目列表显示编号/标题/难度 | ✅ PASS |
| TC-UI-07 | "简单"筛选按钮 active，仅显示 Easy 题 | ✅ PASS |
| TC-UI-08 | 管理员显示 admin 徽章和"管理后台"链接 | ✅ PASS |
| TC-UI-09 | 详情页左侧描述+右侧 Ace 编辑器 | ✅ PASS |
| TC-UI-10 | 提交代码显示 Accepted，1/1 通过 | ✅ PASS |

### 5.2 端到端业务流程测试

| 编号 | 场景 | 结果 |
|------|------|------|
| TC-E2E-01 | 注册→登录（role=user，无管理后台）→登出 | ✅ PASS |
| TC-E2E-02 | 管理后台创建题目→列表可见→详情2个测试用例 | ✅ PASS |
| TC-E2E-03 | 提交 AC 代码（2/2）+ WA 代码（0/2） | ✅ PASS |
| TC-E2E-04 | 删除题目→列表消失→详情显示 "problem not found" | ✅ PASS |
| TC-E2E-05 | 五种判题状态：AC / WA / CE / TLE / RE | ✅ PASS |

### 5.3 判题状态详情

| 状态 | 显示文本 | 通过数 | 耗时 |
|------|---------|--------|------|
| AC | Accepted | 2/2 | ~10ms |
| WA | Wrong Answer | 0/2 | ~11ms |
| CE | Compile Error | 0/2 | 0ms（含编译错误信息） |
| TLE | Time Limit Exceeded | 0/2 | ~2503ms |
| RE | Runtime Error | 0/2 | ~172ms |

### 5.4 总计

**18 个测试用例全部通过，0 失败。**

---

## 6. 技术备注

### 6.1 有头模式启动

Playwright CLI 默认以无头模式（`--headless`）启动浏览器，窗口不可见。必须添加 `--headed` 参数才能显示浏览器窗口：

```bash
playwright-cli open --browser=chrome --headed http://101.42.13.135:8080/index.html
```

> **注意**：`--config=headed-config.json`（`{"headless": false}`）方式无效，启动参数中仍会包含 `--headless`。必须使用 `--headed` 命令行选项。

### 6.2 Windows 下通过 cmd.exe 调用

PowerShell 执行策略可能阻止 `playwright-cli.ps1` 脚本运行。解决方式是通过 `cmd.exe` 调用：

```bash
cmd.exe /c "playwright-cli open --browser=chrome --headed http://101.42.13.135:8080/index.html"
```

### 6.3 Ace 编辑器代码输入

Ace 编辑器的 `textarea` 被 `ace_content` 层遮挡，无法通过 `playwright-cli fill` 或 `click` 直接操作。需要通过 `run-code` 调用 Ace JavaScript API 写入代码：

```javascript
// 写入文件 set_code.js
async page => {
  await page.evaluate(() => {
    const editor = ace.edit(document.querySelector('.ace_editor'));
    editor.setValue('你的代码内容', -1);  // -1 表示光标移到开头
  });
}
```

执行：`playwright-cli run-code --filename=set_code.js`

### 6.4 含空格/中文的文本填写

`playwright-cli fill <ref> <text>` 命令中，如果文本包含空格或中文，通过 `cmd.exe` 传递时会被拆分为多个参数。解决方式：

- **方法一**：用双引号包裹（简单文本）
  ```bash
  playwright-cli fill f11e32 WebTest_E2E_UI
  ```

- **方法二**：通过 `run-code` 使用 Playwright API（含空格/中文）
  ```javascript
  // 写入文件 fill_desc.js
  async page => {
    await page.getByRole('textbox', {name: '题目描述'}).fill('输入两个整数 a 和 b，输出它们的和。');
  }
  ```

### 6.5 页面加载等待

部分页面（如题目详情页）初次加载时显示"加载中..."，需要等待异步请求完成后才显示实际内容。建议在导航后等待 2 秒再获取快照：

```bash
Start-Sleep -Seconds 2
playwright-cli snapshot
```

### 6.6 TLE 测试等待时间

TLE（超时）测试代码 `while(1){}` 需要等待服务器 5 秒超时限制触发，加上网络延迟，建议等待 8 秒：

```bash
Start-Sleep -Seconds 8
playwright-cli snapshot
```

### 6.7 删除确认对话框

管理后台删除题目时会弹出确认对话框（非浏览器原生 `confirm`，而是页面内 modal），需要先点击删除按钮，等待对话框出现后再点击"删除"确认：

```bash
# 步骤1：点击删除图标
playwright-cli click f14e139

# 步骤2：等待确认对话框出现，点击"删除"按钮
Start-Sleep -Seconds 1
playwright-cli click f14e151
```

### 6.8 时间戳避免用户名重复

注册测试时，多次运行可能因用户名已存在而返回 400 错误。使用时间戳生成唯一用户名：

```bash
# 通过 run-code 获取时间戳
playwright-cli run-code --filename=get_ts.js
# 返回："1783077326401"

# 使用时间戳作为用户名后缀
playwright-cli fill f6e16 webtest_e2e_1783077326401
```

### 6.9 快照文件位置

所有快照文件保存在工作目录下的 `.playwright-cli/` 文件夹中，文件名格式为 `page-<timestamp>.yml`。可通过 `--raw` 选项直接在输出中获取快照内容：

```bash
playwright-cli --raw snapshot
```
