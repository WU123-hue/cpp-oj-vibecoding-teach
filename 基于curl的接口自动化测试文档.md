# 基于 curl 的接口自动化测试文档

> 本文档用于对 OJ 系统的全部 HTTP 接口进行自动化测试验证。
> 测试脚本依赖 `curl`、`python3`、`mysql` 命令行工具。

---

## 1. 环境准备

### 1.1 编译服务器与工具

```bash
# 编译 OJ 服务器
g++ -std=c++17 -Wall -Wextra -I src -I/usr/include/mysql \
  src/main.cc \
  src/server/server.cc src/server/router.cc \
  src/handler/admin_handler.cc src/handler/problem_handler.cc \
  src/handler/submit_handler.cc src/handler/auth_handler.cc \
  src/service/problem_service.cc src/service/executor_service.cc \
  src/service/auth_service.cc src/service/session_manager.cc \
  src/model/problem.cc src/model/test_case.cc src/model/user.cc src/model/mapper.cc \
  src/db/connection_pool.cc \
  src/utils/config.cc src/utils/logger.cc \
  -o /tmp/oj_build/oj_server \
  -lmysqlclient -lpthread -lyaml-cpp -lssl -lcrypto -lcrypt

# 编译 init_admin 工具
g++ -std=c++17 -Wall -Wextra -I src -I/usr/include/mysql \
  src/init_admin.cc \
  src/service/auth_service.cc src/service/session_manager.cc \
  src/model/user.cc src/model/mapper.cc src/model/problem.cc src/model/test_case.cc \
  src/db/connection_pool.cc src/utils/logger.cc \
  -o /tmp/oj_build/init_admin \
  -lmysqlclient -lpthread -lyaml-cpp -lcrypt
```

### 1.2 初始化管理员账户

```bash
/tmp/oj_build/init_admin
```

预期输出：
```
[OK] 管理员账户已创建: id=xxx username=admin role=admin
[OK] 密码验证测试通过
```

### 1.3 清理残留测试数据

```bash
mysql -u root oj_db -e "
  DELETE FROM users WHERE username LIKE 'UT_CURL_%';
  DELETE FROM problems WHERE title LIKE 'UT_CURL_%';
" 2>/dev/null
```

### 1.4 启动服务器

```bash
# 杀掉可能残留的服务器进程
ps aux | grep oj_server | grep -v grep | awk '{print $2}' | xargs -r kill -9 2>/dev/null

# 启动服务器
/tmp/oj_build/oj_server > /tmp/oj_build/server.log 2>&1 &
sleep 2

# 验证服务器就绪
curl -s http://localhost:8080/api/problems | python3 -c "import sys,json; print('server up, code=', json.load(sys.stdin)['code'])"
```

预期输出：
```
server up, code= 200
```

---

## 2. 测试用例

> 以下按接口分组，每组包含正常流程和异常场景。
> 所有测试使用 `BASE="http://localhost:8080"` 作为基础地址。

### 2.1 用户注册

#### 2.1.1 正常注册

```bash
curl -s -X POST $BASE/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"secret123"}'
```

预期：
```json
{"code":200,"data":{"id":<数字>,"username":"alice"},"message":"registered"}
```

#### 2.1.2 重复用户名

```bash
curl -s -X POST $BASE/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"secret123"}'
```

预期：
```json
{"code":400,"message":"username already exists"}
```

#### 2.1.3 用户名过短（<3）

```bash
curl -s -X POST $BASE/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"ab","password":"secret123"}'
```

预期：
```json
{"code":400,"message":"username must be 3-64 characters"}
```

#### 2.1.4 密码过短（<6）

```bash
curl -s -X POST $BASE/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"bob","password":"12345"}'
```

预期：
```json
{"code":400,"message":"password must be 6-64 characters"}
```

#### 2.1.5 非法 JSON

```bash
curl -s -X POST $BASE/api/register \
  -H "Content-Type: application/json" \
  -d '{invalid'
```

预期：
```json
{"code":400,"message":"invalid JSON: ..."}
```

---

### 2.2 用户登录

#### 2.2.1 普通用户登录

```bash
curl -s -D - -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"secret123"}'
```

预期响应头包含：
```
Set-Cookie: oj_session=<32位十六进制>; Path=/; HttpOnly
```

预期响应体：
```json
{"code":200,"data":{"id":<数字>,"username":"alice","role":"user"},"message":"login successful"}
```

#### 2.2.2 管理员登录

```bash
curl -s -D - -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}'
```

预期响应体：
```json
{"code":200,"data":{"id":<数字>,"username":"admin","role":"admin"},"message":"login successful"}
```

#### 2.2.3 错误密码

```bash
curl -s -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"wrongpassword"}'
```

预期：
```json
{"code":401,"message":"invalid username or password"}
```

#### 2.2.4 不存在的用户

```bash
curl -s -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"nosuchuser","password":"secret123"}'
```

预期：
```json
{"code":401,"message":"invalid username or password"}
```

#### 2.2.5 空密码

```bash
curl -s -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":""}'
```

预期：
```json
{"code":400,"message":"password is required"}
```

#### 2.2.6 缺少 username 字段

```bash
curl -s -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"password":"admin123"}'
```

预期：
```json
{"code":400,"message":"username is required"}
```

---

### 2.3 用户登出

#### 2.3.1 已登录用户登出

```bash
# 先登录获取 session_id
SID=$(curl -s -D - -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}' 2>&1 | grep -oP 'oj_session=\K[a-f0-9]+')

# 带 Cookie 登出
curl -s -D - -X POST $BASE/api/logout \
  -H "Cookie: oj_session=$SID"
```

预期响应头：
```
Set-Cookie: oj_session=; Path=/; HttpOnly; Max-Age=0
```

预期响应体：
```json
{"code":200,"message":"logged out"}
```

#### 2.3.2 无 Cookie 登出

```bash
curl -s -X POST $BASE/api/logout
```

预期：
```json
{"code":200,"message":"logged out"}
```

#### 2.3.3 无效 session_id 登出

```bash
curl -s -X POST $BASE/api/logout \
  -H "Cookie: oj_session=invalid_sid_12345"
```

预期：
```json
{"code":200,"message":"logged out"}
```

---

### 2.4 新增题目（管理员）

#### 2.4.1 正常创建 — 含测试用例

```bash
RESP=$(curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{
    "title":"UT_CURL_AB",
    "difficulty":"Easy",
    "content":"输入两个整数 a 和 b，输出它们的和。",
    "template":"#include <iostream>\nint main(){}",
    "test_cases":[
      {"input":"1 2","expected":"3"},
      {"input":"10 20","expected":"30"}
    ]
  }')
echo $RESP
PID1=$(echo $RESP | python3 -c "import sys,json; print(json.load(sys.stdin)['data']['id'])")
```

预期：
```json
{"code":200,"data":{"id":<数字>},"message":"created"}
```

#### 2.4.2 正常创建 — 无测试用例

```bash
RESP=$(curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"UT_CURL_NoTc","difficulty":"Medium","content":"无测试用例的题目"}')
PID2=$(echo $RESP | python3 -c "import sys,json; print(json.load(sys.stdin)['data']['id'])")
```

预期：
```json
{"code":200,"data":{"id":<数字>},"message":"created"}
```

#### 2.4.3 正常创建 — Hard 难度

```bash
RESP=$(curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"UT_CURL_Hard","difficulty":"Hard","content":"困难题目","test_cases":[{"input":"5","expected":"120"}]}')
PID3=$(echo $RESP | python3 -c "import sys,json; print(json.load(sys.stdin)['data']['id'])")
```

预期：
```json
{"code":200,"data":{"id":<数字>},"message":"created"}
```

#### 2.4.4 缺少 title

```bash
curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"difficulty":"Easy","content":"缺标题"}'
```

预期：
```json
{"code":400,"message":"title is required"}
```

#### 2.4.5 缺少 content

```bash
curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"UT_CURL_NoContent","difficulty":"Easy"}'
```

预期：
```json
{"code":400,"message":"content is required"}
```

#### 2.4.6 无效 difficulty

```bash
curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"UT_CURL_BadDiff","difficulty":"SuperHard","content":"无效难度"}'
```

预期：
```json
{"code":400,"message":"invalid difficulty"}
```

#### 2.4.7 非法 JSON

```bash
curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{bad json'
```

预期：
```json
{"code":400,"message":"invalid JSON: ..."}
```

---

### 2.5 题目列表

```bash
curl -s $BASE/api/problems
```

预期：
```json
{
  "code":200,
  "message":"ok",
  "data":[
    {"id":<数字>,"title":"UT_CURL_AB","difficulty":"Easy"},
    {"id":<数字>,"title":"UT_CURL_NoTc","difficulty":"Medium"},
    {"id":<数字>,"title":"UT_CURL_Hard","difficulty":"Hard"}
  ]
}
```

---

### 2.6 题目详情

#### 2.6.1 存在的题目 — 含测试用例

```bash
curl -s $BASE/api/problems/$PID1
```

预期：
```json
{
  "code":200,
  "message":"ok",
  "data":{
    "id":<数字>,
    "title":"UT_CURL_AB",
    "difficulty":"Easy",
    "content":"输入两个整数 a 和 b，输出它们的和。",
    "template":"#include <iostream>\nint main(){}",
    "created_at":"<时间戳>",
    "test_cases":[
      {"id":<数字>,"input":"1 2","expected":"3","position":0},
      {"id":<数字>,"input":"10 20","expected":"30","position":1}
    ]
  }
}
```

#### 2.6.2 存在的题目 — 无测试用例

```bash
curl -s $BASE/api/problems/$PID2
```

预期：
```json
{
  "code":200,
  "message":"ok",
  "data":{
    "id":<数字>,
    "title":"UT_CURL_NoTc",
    "difficulty":"Medium",
    "content":"无测试用例的题目",
    "template":"",
    "created_at":"<时间戳>",
    "test_cases":[]
  }
}
```

#### 2.6.3 不存在的题目

```bash
curl -s $BASE/api/problems/9999999
```

预期：
```json
{"code":404,"message":"problem not found"}
```

---

### 2.7 提交代码执行

#### 2.7.1 AC — 正确代码

```bash
curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1,\"code\":\"#include <iostream>\\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b<<std::endl;return 0;}\"}"
```

预期：
```json
{
  "code":200,
  "message":"ok",
  "data":{
    "status":"AC",
    "passed":2,
    "total":2,
    "max_time_ms":<数字>,
    "cases":[
      {"status":"AC","exit_code":0,"time_ms":<数字>,"actual":"3\n"},
      {"status":"AC","exit_code":0,"time_ms":<数字>,"actual":"30\n"}
    ]
  }
}
```

#### 2.7.2 WA — 错误输出

```bash
curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1,\"code\":\"#include <iostream>\\nint main(){int a,b;std::cin>>a>>b;std::cout<<a*b<<std::endl;return 0;}\"}"
```

预期：
```json
{
  "code":200,
  "data":{
    "status":"WA",
    "passed":0,
    "total":2,
    ...
  }
}
```

#### 2.7.3 CE — 编译错误

```bash
curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1,\"code\":\"int main(){ syntax error }\"}"
```

预期：
```json
{
  "code":200,
  "data":{
    "status":"CE",
    "passed":0,
    "total":2,
    "compile_output":"<编译器错误信息>",
    "cases":[]
  }
}
```

#### 2.7.4 TLE — 超时

```bash
curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1,\"code\":\"int main(){while(1){}return 0;}\"}"
```

预期：
```json
{
  "code":200,
  "data":{
    "status":"TLE",
    "passed":0,
    "total":2,
    "cases":[
      {"status":"TLE","exit_code":-1,"time_ms":<数字>,"actual":"","error":"CPU time limit exceeded (SIGXCPU)"}
    ]
  }
}
```

#### 2.7.5 RE — 运行时错误

```bash
curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1,\"code\":\"int main(){int*p=nullptr;*p=42;return 0;}\"}"
```

预期：
```json
{
  "code":200,
  "data":{
    "status":"RE",
    "passed":0,
    "total":2,
    "cases":[
      {"status":"RE","exit_code":-1,"time_ms":<数字>,"actual":"","error":"segmentation fault"}
    ]
  }
}
```

#### 2.7.6 题目不存在

```bash
curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d '{"problem_id":9999999,"code":"int main(){}"}'
```

预期：
```json
{"code":404,"message":"problem not found"}
```

#### 2.7.7 缺少 code 字段

```bash
curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1}"
```

预期：
```json
{"code":400,"message":"code is required"}
```

---

### 2.8 删除题目（管理员）

#### 2.8.1 正常删除

```bash
curl -s -X DELETE $BASE/api/admin/problems/$PID1
```

预期：
```json
{"code":200,"message":"deleted"}
```

#### 2.8.2 验证已删除 — 查询详情返回 404

```bash
curl -s $BASE/api/problems/$PID1
```

预期：
```json
{"code":404,"message":"problem not found"}
```

#### 2.8.3 验证已删除 — 列表中不再包含

```bash
curl -s $BASE/api/problems | python3 -c "
import sys,json
data = json.load(sys.stdin)['data']
found = [p for p in data if p['title'] == 'UT_CURL_AB']
print('已删除题目在列表中:', len(found) > 0)
"
```

预期：
```
已删除题目在列表中: False
```

#### 2.8.4 删除不存在的题目

```bash
curl -s -X DELETE $BASE/api/admin/problems/9999999
```

预期：
```json
{"code":200,"message":"deleted"}
```

#### 2.8.5 清理剩余测试题目

```bash
curl -s -X DELETE $BASE/api/admin/problems/$PID2
curl -s -X DELETE $BASE/api/admin/problems/$PID3
```

预期均返回：
```json
{"code":200,"message":"deleted"}
```

---

## 3. 完整自动化测试脚本

将以下内容保存为脚本并执行，可一次性运行全部测试：

```bash
#!/bin/bash
# oj_api_test.sh — OJ 系统 curl 接口自动化测试
# 用法: bash oj_api_test.sh

set -e
BASE="http://localhost:8080"
PASS=0
FAIL=0

# 测试断言函数
# 用法: assert_eq "描述" "实际值" "预期值"
assert_eq() {
  local desc="$1" actual="$2" expected="$3"
  if [ "$actual" = "$expected" ]; then
    echo "  [PASS] $desc"
    PASS=$((PASS + 1))
  else
    echo "  [FAIL] $desc"
    echo "         预期: $expected"
    echo "         实际: $actual"
    FAIL=$((FAIL + 1))
  fi
}

# 用法: assert_contains "描述" "实际值" "应包含的子串"
assert_contains() {
  local desc="$1" actual="$2" substr="$3"
  if echo "$actual" | grep -qF "$substr"; then
    echo "  [PASS] $desc"
    PASS=$((PASS + 1))
  else
    echo "  [FAIL] $desc"
    echo "         应包含: $substr"
    echo "         实际: $actual"
    FAIL=$((FAIL + 1))
  fi
}

echo "=========================================="
echo "  OJ 系统 curl 接口自动化测试"
echo "=========================================="

# ===================================================================
echo ""
echo "--- 1. 用户注册 ---"

# 1.1 正常注册
RESP=$(curl -s -X POST $BASE/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testcurl","password":"testpass123"}')
assert_contains "1.1 正常注册" "$RESP" '"code":200'
assert_contains "1.1 正常注册" "$RESP" '"username":"testcurl"'

# 1.2 重复用户名
RESP=$(curl -s -X POST $BASE/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testcurl","password":"testpass123"}')
assert_contains "1.2 重复用户名" "$RESP" '"code":400'
assert_contains "1.2 重复用户名" "$RESP" 'username already exists'

# 1.3 用户名过短
RESP=$(curl -s -X POST $BASE/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"ab","password":"testpass123"}')
assert_contains "1.3 用户名过短" "$RESP" '"code":400'
assert_contains "1.3 用户名过短" "$RESP" 'username must be 3-64 characters'

# 1.4 密码过短
RESP=$(curl -s -X POST $BASE/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testcurl2","password":"12345"}')
assert_contains "1.4 密码过短" "$RESP" '"code":400'
assert_contains "1.4 密码过短" "$RESP" 'password must be 6-64 characters'

# 1.5 非法 JSON
RESP=$(curl -s -X POST $BASE/api/register \
  -H "Content-Type: application/json" \
  -d '{invalid')
assert_contains "1.5 非法 JSON" "$RESP" '"code":400'
assert_contains "1.5 非法 JSON" "$RESP" 'invalid JSON'

# ===================================================================
echo ""
echo "--- 2. 用户登录 ---"

# 2.1 管理员登录
RESP=$(curl -s -D /tmp/oj_test_headers -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123"}')
assert_contains "2.1 管理员登录" "$RESP" '"code":200'
assert_contains "2.1 管理员登录" "$RESP" '"role":"admin"'
SID=$(cat /tmp/oj_test_headers | grep -oP 'oj_session=\K[a-f0-9]+')
assert_contains "2.1 管理员登录 Set-Cookie" "$(cat /tmp/oj_test_headers)" 'Set-Cookie: oj_session='

# 2.2 普通用户登录
RESP=$(curl -s -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testcurl","password":"testpass123"}')
assert_contains "2.2 普通用户登录" "$RESP" '"code":200'
assert_contains "2.2 普通用户登录" "$RESP" '"role":"user"'

# 2.3 错误密码
RESP=$(curl -s -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"wrongpassword"}')
assert_contains "2.3 错误密码" "$RESP" '"code":401'
assert_contains "2.3 错误密码" "$RESP" 'invalid username or password'

# 2.4 不存在的用户
RESP=$(curl -s -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"nosuchuser","password":"testpass123"}')
assert_contains "2.4 不存在的用户" "$RESP" '"code":401'
assert_contains "2.4 不存在的用户" "$RESP" 'invalid username or password'

# 2.5 空密码
RESP=$(curl -s -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":""}')
assert_contains "2.5 空密码" "$RESP" '"code":400'
assert_contains "2.5 空密码" "$RESP" 'password is required'

# 2.6 缺少 username
RESP=$(curl -s -X POST $BASE/api/login \
  -H "Content-Type: application/json" \
  -d '{"password":"admin123"}')
assert_contains "2.6 缺少 username" "$RESP" '"code":400'
assert_contains "2.6 缺少 username" "$RESP" 'username is required'

# ===================================================================
echo ""
echo "--- 3. 用户登出 ---"

# 3.1 带 Cookie 登出
RESP=$(curl -s -D /tmp/oj_test_headers2 -X POST $BASE/api/logout \
  -H "Cookie: oj_session=$SID")
assert_contains "3.1 带 Cookie 登出" "$RESP" '"code":200'
assert_contains "3.1 带 Cookie 登出" "$RESP" 'logged out'
assert_contains "3.1 登出 Set-Cookie 清除" "$(cat /tmp/oj_test_headers2)" 'Max-Age=0'

# 3.2 无 Cookie 登出
RESP=$(curl -s -X POST $BASE/api/logout)
assert_contains "3.2 无 Cookie 登出" "$RESP" '"code":200'
assert_contains "3.2 无 Cookie 登出" "$RESP" 'logged out'

# 3.3 无效 session 登出
RESP=$(curl -s -X POST $BASE/api/logout \
  -H "Cookie: oj_session=invalid_sid_12345")
assert_contains "3.3 无效 session 登出" "$RESP" '"code":200'
assert_contains "3.3 无效 session 登出" "$RESP" 'logged out'

# ===================================================================
echo ""
echo "--- 4. 新增题目 ---"

# 4.1 正常创建 — 含测试用例
RESP=$(curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"UT_CURL_AB","difficulty":"Easy","content":"输入两个整数 a 和 b，输出它们的和。","template":"#include <iostream>\nint main(){}","test_cases":[{"input":"1 2","expected":"3"},{"input":"10 20","expected":"30"}]}')
assert_contains "4.1 正常创建含测试用例" "$RESP" '"code":200'
assert_contains "4.1 正常创建含测试用例" "$RESP" '"message":"created"'
PID1=$(echo $RESP | python3 -c "import sys,json; print(json.load(sys.stdin)['data']['id'])")

# 4.2 正常创建 — 无测试用例
RESP=$(curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"UT_CURL_NoTc","difficulty":"Medium","content":"无测试用例的题目"}')
assert_contains "4.2 正常创建无测试用例" "$RESP" '"code":200'
PID2=$(echo $RESP | python3 -c "import sys,json; print(json.load(sys.stdin)['data']['id'])")

# 4.3 正常创建 — Hard 难度
RESP=$(curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"UT_CURL_Hard","difficulty":"Hard","content":"困难题目","test_cases":[{"input":"5","expected":"120"}]}')
assert_contains "4.3 正常创建 Hard 难度" "$RESP" '"code":200'
PID3=$(echo $RESP | python3 -c "import sys,json; print(json.load(sys.stdin)['data']['id'])")

# 4.4 缺少 title
RESP=$(curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"difficulty":"Easy","content":"缺标题"}')
assert_contains "4.4 缺少 title" "$RESP" '"code":400'
assert_contains "4.4 缺少 title" "$RESP" 'title is required'

# 4.5 缺少 content
RESP=$(curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"UT_CURL_NoContent","difficulty":"Easy"}')
assert_contains "4.5 缺少 content" "$RESP" '"code":400'
assert_contains "4.5 缺少 content" "$RESP" 'content is required'

# 4.6 无效 difficulty
RESP=$(curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{"title":"UT_CURL_BadDiff","difficulty":"SuperHard","content":"无效难度"}')
assert_contains "4.6 无效 difficulty" "$RESP" '"code":400'
assert_contains "4.6 无效 difficulty" "$RESP" 'invalid difficulty'

# 4.7 非法 JSON
RESP=$(curl -s -X POST $BASE/api/admin/problems \
  -H "Content-Type: application/json" \
  -d '{bad json')
assert_contains "4.7 非法 JSON" "$RESP" '"code":400'
assert_contains "4.7 非法 JSON" "$RESP" 'invalid JSON'

# ===================================================================
echo ""
echo "--- 5. 题目列表 ---"

RESP=$(curl -s $BASE/api/problems)
assert_contains "5.1 题目列表" "$RESP" '"code":200'
assert_contains "5.1 题目列表" "$RESP" '"message":"ok"'
assert_contains "5.1 列表含 UT_CURL_AB" "$RESP" 'UT_CURL_AB'
assert_contains "5.1 列表含 UT_CURL_NoTc" "$RESP" 'UT_CURL_NoTc'
assert_contains "5.1 列表含 UT_CURL_Hard" "$RESP" 'UT_CURL_Hard'

# ===================================================================
echo ""
echo "--- 6. 题目详情 ---"

# 6.1 存在的题目 — 含测试用例
RESP=$(curl -s $BASE/api/problems/$PID1)
assert_contains "6.1 题目详情" "$RESP" '"code":200'
assert_contains "6.1 题目详情" "$RESP" 'UT_CURL_AB'
assert_contains "6.1 题目详情含测试用例" "$RESP" '"expected":"3"'

# 6.2 存在的题目 — 无测试用例
RESP=$(curl -s $BASE/api/problems/$PID2)
assert_contains "6.2 无测试用例详情" "$RESP" '"code":200'
assert_contains "6.2 无测试用例详情" "$RESP" 'UT_CURL_NoTc'

# 6.3 不存在的题目
RESP=$(curl -s $BASE/api/problems/9999999)
assert_contains "6.3 不存在的题目" "$RESP" '"code":404'
assert_contains "6.3 不存在的题目" "$RESP" 'problem not found'

# ===================================================================
echo ""
echo "--- 7. 提交代码执行 ---"

# 7.1 AC
RESP=$(curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1,\"code\":\"#include <iostream>\\nint main(){int a,b;std::cin>>a>>b;std::cout<<a+b<<std::endl;return 0;}\"}")
assert_contains "7.1 提交 AC" "$RESP" '"status":"AC"'
assert_contains "7.1 提交 AC" "$RESP" '"passed":2'

# 7.2 WA
RESP=$(curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1,\"code\":\"#include <iostream>\\nint main(){int a,b;std::cin>>a>>b;std::cout<<a*b<<std::endl;return 0;}\"}")
assert_contains "7.2 提交 WA" "$RESP" '"status":"WA"'
assert_contains "7.2 提交 WA" "$RESP" '"passed":0'

# 7.3 CE
RESP=$(curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1,\"code\":\"int main(){ syntax error }\"}")
assert_contains "7.3 提交 CE" "$RESP" '"status":"CE"'
assert_contains "7.3 提交 CE" "$RESP" 'compile_output'

# 7.4 TLE
RESP=$(curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1,\"code\":\"int main(){while(1){}return 0;}\"}")
assert_contains "7.4 提交 TLE" "$RESP" '"status":"TLE"'

# 7.5 RE
RESP=$(curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1,\"code\":\"int main(){int*p=nullptr;*p=42;return 0;}\"}")
assert_contains "7.5 提交 RE" "$RESP" '"status":"RE"'
assert_contains "7.5 提交 RE" "$RESP" 'segmentation fault'

# 7.6 题目不存在
RESP=$(curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d '{"problem_id":9999999,"code":"int main(){}"}')
assert_contains "7.6 题目不存在" "$RESP" '"code":404'
assert_contains "7.6 题目不存在" "$RESP" 'problem not found'

# 7.7 缺少 code
RESP=$(curl -s -X POST $BASE/api/submit \
  -H "Content-Type: application/json" \
  -d "{\"problem_id\":$PID1}")
assert_contains "7.7 缺少 code" "$RESP" '"code":400'
assert_contains "7.7 缺少 code" "$RESP" 'code is required'

# ===================================================================
echo ""
echo "--- 8. 删除题目 ---"

# 8.1 正常删除
RESP=$(curl -s -X DELETE $BASE/api/admin/problems/$PID1)
assert_contains "8.1 正常删除" "$RESP" '"code":200'
assert_contains "8.1 正常删除" "$RESP" 'deleted'

# 8.2 验证已删除
RESP=$(curl -s $BASE/api/problems/$PID1)
assert_contains "8.2 删除后查详情 404" "$RESP" '"code":404'
assert_contains "8.2 删除后查详情 404" "$RESP" 'problem not found'

# 8.3 删除不存在的题目
RESP=$(curl -s -X DELETE $BASE/api/admin/problems/9999999)
assert_contains "8.3 删除不存在的题目" "$RESP" '"code":200'

# 8.4 清理剩余测试题目
curl -s -X DELETE $BASE/api/admin/problems/$PID2 > /dev/null
curl -s -X DELETE $BASE/api/admin/problems/$PID3 > /dev/null

# ===================================================================
echo ""
echo "=========================================="
echo "  测试结果: PASS=$PASS  FAIL=$FAIL"
echo "=========================================="
```

---

## 4. 运行自动化测试

```bash
# 1. 初始化管理员
/tmp/oj_build/init_admin

# 2. 清理残留数据
mysql -u root oj_db -e "DELETE FROM users WHERE username LIKE 'UT_CURL_%' OR username='testcurl' OR username='alice' OR username='bob'; DELETE FROM problems WHERE title LIKE 'UT_CURL_%';" 2>/dev/null

# 3. 启动服务器
ps aux | grep oj_server | grep -v grep | awk '{print $2}' | xargs -r kill -9 2>/dev/null
/tmp/oj_build/oj_server > /tmp/oj_build/server.log 2>&1 &
sleep 2

# 4. 运行测试脚本
bash oj_api_test.sh

# 5. 测试完成后清理
ps aux | grep oj_server | grep -v grep | awk '{print $2}' | xargs -r kill -9 2>/dev/null
mysql -u root oj_db -e "DELETE FROM users WHERE username LIKE 'UT_CURL_%' OR username='testcurl' OR username='alice' OR username='bob'; DELETE FROM problems WHERE title LIKE 'UT_CURL_%';" 2>/dev/null
```

---

## 5. 测试用例总览

| # | 接口 | 场景 | 预期状态码 |
|---|------|------|-----------|
| 1.1 | POST /api/register | 正常注册 | 200 |
| 1.2 | POST /api/register | 重复用户名 | 400 |
| 1.3 | POST /api/register | 用户名过短 | 400 |
| 1.4 | POST /api/register | 密码过短 | 400 |
| 1.5 | POST /api/register | 非法 JSON | 400 |
| 2.1 | POST /api/login | 管理员登录 | 200 + Set-Cookie |
| 2.2 | POST /api/login | 普通用户登录 | 200 + Set-Cookie |
| 2.3 | POST /api/login | 错误密码 | 401 |
| 2.4 | POST /api/login | 不存在的用户 | 401 |
| 2.5 | POST /api/login | 空密码 | 400 |
| 2.6 | POST /api/login | 缺少 username | 400 |
| 3.1 | POST /api/logout | 带 Cookie 登出 | 200 + 清除 Cookie |
| 3.2 | POST /api/logout | 无 Cookie 登出 | 200 |
| 3.3 | POST /api/logout | 无效 session 登出 | 200 |
| 4.1 | POST /api/admin/problems | 正常创建含测试用例 | 200 |
| 4.2 | POST /api/admin/problems | 正常创建无测试用例 | 200 |
| 4.3 | POST /api/admin/problems | Hard 难度 | 200 |
| 4.4 | POST /api/admin/problems | 缺少 title | 400 |
| 4.5 | POST /api/admin/problems | 缺少 content | 400 |
| 4.6 | POST /api/admin/problems | 无效 difficulty | 400 |
| 4.7 | POST /api/admin/problems | 非法 JSON | 400 |
| 5.1 | GET /api/problems | 题目列表 | 200 |
| 6.1 | GET /api/problems/:id | 详情含测试用例 | 200 |
| 6.2 | GET /api/problems/:id | 详情无测试用例 | 200 |
| 6.3 | GET /api/problems/:id | 不存在的题目 | 404 |
| 7.1 | POST /api/submit | AC | 200 + status=AC |
| 7.2 | POST /api/submit | WA | 200 + status=WA |
| 7.3 | POST /api/submit | CE | 200 + status=CE |
| 7.4 | POST /api/submit | TLE | 200 + status=TLE |
| 7.5 | POST /api/submit | RE | 200 + status=RE |
| 7.6 | POST /api/submit | 题目不存在 | 404 |
| 7.7 | POST /api/submit | 缺少 code | 400 |
| 8.1 | DELETE /api/admin/problems/:id | 正常删除 | 200 |
| 8.2 | GET /api/problems/:id | 删除后查询 | 404 |
| 8.3 | DELETE /api/admin/problems/:id | 删除不存在的题目 | 200 |
```
