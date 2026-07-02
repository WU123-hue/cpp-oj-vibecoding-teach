#!/usr/bin/env python3
"""
OJ 系统 API 接口自动化测试
基于「基于curl的接口自动化测试文档.md」实现

用法:
  python3 tests/python/test_api.py [--host HOST] [--port PORT]

前置条件:
  1. 已编译 oj_server 并启动
  2. 已运行 init_admin 初始化管理员账户
  3. 数据库 oj_db 可连接
"""

import argparse
import json
import re
import subprocess
import sys
import time

import requests

# =====================================================================
# 全局状态
# =====================================================================
BASE_URL = "http://localhost:8080"
PASS_COUNT = 0
FAIL_COUNT = 0

# 测试中创建的资源，用于清理
CREATED_PROBLEM_IDS = []
CREATED_USERNAMES = []


# =====================================================================
# 断言工具
# =====================================================================
def assert_eq(desc, actual, expected):
    global PASS_COUNT, FAIL_COUNT
    if actual == expected:
        print(f"  [PASS] {desc}")
        PASS_COUNT += 1
    else:
        print(f"  [FAIL] {desc}")
        print(f"         预期: {expected}")
        print(f"         实际: {actual}")
        FAIL_COUNT += 1


def assert_contains(desc, actual, substr):
    global PASS_COUNT, FAIL_COUNT
    if substr in actual:
        print(f"  [PASS] {desc}")
        PASS_COUNT += 1
    else:
        print(f"  [FAIL] {desc}")
        print(f"         应包含: {substr}")
        print(f"         实际: {actual}")
        FAIL_COUNT += 1


def assert_json_field(desc, resp_json, field, expected):
    global PASS_COUNT, FAIL_COUNT
    actual = resp_json.get(field) if isinstance(resp_json, dict) else None
    if actual == expected:
        print(f"  [PASS] {desc}")
        PASS_COUNT += 1
    else:
        print(f"  [FAIL] {desc}")
        print(f"         预期 {field}={expected}")
        print(f"         实际 {field}={actual}")
        FAIL_COUNT += 1


# =====================================================================
# HTTP 请求封装
# =====================================================================
def post_json(path, body, cookies=None):
    url = BASE_URL + path
    resp = requests.post(url, json=body, cookies=cookies, timeout=30)
    return resp


def post_raw(path, raw_body, cookies=None):
    url = BASE_URL + path
    resp = requests.post(url, data=raw_body,
                         headers={"Content-Type": "application/json"},
                         cookies=cookies, timeout=30)
    return resp


def get_json(path, cookies=None):
    url = BASE_URL + path
    resp = requests.get(url, cookies=cookies, timeout=10)
    return resp


def delete_json(path, cookies=None):
    url = BASE_URL + path
    resp = requests.delete(url, cookies=cookies, timeout=10)
    return resp


# =====================================================================
# 辅助函数
# =====================================================================
def extract_session_id(resp):
    """从 Set-Cookie 响应头中提取 session_id"""
    cookie_header = resp.headers.get("Set-Cookie", "")
    match = re.search(r'oj_session=([a-f0-9]+)', cookie_header)
    return match.group(1) if match else ""


def login_and_get_sid(username, password):
    """登录并返回 session_id"""
    resp = post_json("/api/login", {"username": username, "password": password})
    return extract_session_id(resp)


# =====================================================================
# 1. 用户注册测试
# =====================================================================
def test_register():
    print("\n--- 1. 用户注册 ---")

    # 1.1 正常注册
    resp = post_json("/api/register",
                     {"username": "testcurl", "password": "testpass123"})
    body = resp.json()
    assert_eq("1.1 正常注册 code", body.get("code"), 200)
    assert_eq("1.1 正常注册 username",
              body.get("data", {}).get("username"), "testcurl")
    CREATED_USERNAMES.append("testcurl")

    # 1.2 重复用户名
    resp = post_json("/api/register",
                     {"username": "testcurl", "password": "testpass123"})
    body = resp.json()
    assert_eq("1.2 重复用户名 code", body.get("code"), 400)
    assert_eq("1.2 重复用户名 message",
              body.get("message"), "username already exists")

    # 1.3 用户名过短
    resp = post_json("/api/register",
                     {"username": "ab", "password": "testpass123"})
    body = resp.json()
    assert_eq("1.3 用户名过短 code", body.get("code"), 400)
    assert_eq("1.3 用户名过短 message",
              body.get("message"), "username must be 3-64 characters")

    # 1.4 密码过短
    resp = post_json("/api/register",
                     {"username": "testcurl2", "password": "12345"})
    body = resp.json()
    assert_eq("1.4 密码过短 code", body.get("code"), 400)
    assert_eq("1.4 密码过短 message",
              body.get("message"), "password must be 6-64 characters")

    # 1.5 非法 JSON
    resp = post_raw("/api/register", "{invalid")
    body = resp.json()
    assert_eq("1.5 非法 JSON code", body.get("code"), 400)
    assert_contains("1.5 非法 JSON message",
                    body.get("message", ""), "invalid JSON")


# =====================================================================
# 2. 用户登录测试
# =====================================================================
def test_login():
    print("\n--- 2. 用户登录 ---")

    # 2.1 管理员登录
    resp = post_json("/api/login",
                     {"username": "admin", "password": "admin123"})
    body = resp.json()
    assert_eq("2.1 管理员登录 code", body.get("code"), 200)
    assert_eq("2.1 管理员登录 role",
              body.get("data", {}).get("role"), "admin")
    sid = extract_session_id(resp)
    assert_contains("2.1 管理员登录 Set-Cookie",
                    resp.headers.get("Set-Cookie", ""),
                    "oj_session=")
    # 保存 sid 供后续测试使用
    test_login.admin_sid = sid

    # 2.2 普通用户登录
    resp = post_json("/api/login",
                     {"username": "testcurl", "password": "testpass123"})
    body = resp.json()
    assert_eq("2.2 普通用户登录 code", body.get("code"), 200)
    assert_eq("2.2 普通用户登录 role",
              body.get("data", {}).get("role"), "user")

    # 2.3 错误密码
    resp = post_json("/api/login",
                     {"username": "admin", "password": "wrongpassword"})
    body = resp.json()
    assert_eq("2.3 错误密码 code", body.get("code"), 401)
    assert_eq("2.3 错误密码 message",
              body.get("message"), "invalid username or password")

    # 2.4 不存在的用户
    resp = post_json("/api/login",
                     {"username": "nosuchuser", "password": "testpass123"})
    body = resp.json()
    assert_eq("2.4 不存在的用户 code", body.get("code"), 401)
    assert_eq("2.4 不存在的用户 message",
              body.get("message"), "invalid username or password")

    # 2.5 空密码
    resp = post_json("/api/login",
                     {"username": "admin", "password": ""})
    body = resp.json()
    assert_eq("2.5 空密码 code", body.get("code"), 400)
    assert_eq("2.5 空密码 message",
              body.get("message"), "password is required")

    # 2.6 缺少 username
    resp = post_json("/api/login", {"password": "admin123"})
    body = resp.json()
    assert_eq("2.6 缺少 username code", body.get("code"), 400)
    assert_eq("2.6 缺少 username message",
              body.get("message"), "username is required")


# =====================================================================
# 3. 用户登出测试
# =====================================================================
def test_logout():
    print("\n--- 3. 用户登出 ---")

    sid = getattr(test_login, "admin_sid", "")

    # 3.1 带 Cookie 登出
    cookies = {"oj_session": sid}
    resp = post_json("/api/logout", {}, cookies=cookies)
    body = resp.json()
    assert_eq("3.1 带 Cookie 登出 code", body.get("code"), 200)
    assert_eq("3.1 带 Cookie 登出 message",
              body.get("message"), "logged out")
    assert_contains("3.1 登出 Set-Cookie 清除",
                    resp.headers.get("Set-Cookie", ""), "Max-Age=0")

    # 3.2 无 Cookie 登出
    resp = post_json("/api/logout", {})
    body = resp.json()
    assert_eq("3.2 无 Cookie 登出 code", body.get("code"), 200)
    assert_eq("3.2 无 Cookie 登出 message",
              body.get("message"), "logged out")

    # 3.3 无效 session 登出
    resp = post_json("/api/logout", {},
                     cookies={"oj_session": "invalid_sid_12345"})
    body = resp.json()
    assert_eq("3.3 无效 session 登出 code", body.get("code"), 200)
    assert_eq("3.3 无效 session 登出 message",
              body.get("message"), "logged out")


# =====================================================================
# 4. 新增题目测试
# =====================================================================
def test_create_problem():
    print("\n--- 4. 新增题目 ---")

    # 4.1 正常创建 — 含测试用例
    resp = post_json("/api/admin/problems", {
        "title": "UT_CURL_AB",
        "difficulty": "Easy",
        "content": "输入两个整数 a 和 b，输出它们的和。",
        "template": "#include <iostream>\nint main(){}",
        "test_cases": [
            {"input": "1 2", "expected": "3"},
            {"input": "10 20", "expected": "30"}
        ]
    })
    body = resp.json()
    assert_eq("4.1 正常创建含测试用例 code", body.get("code"), 200)
    assert_eq("4.1 正常创建含测试用例 message",
              body.get("message"), "created")
    test_create_problem.pid1 = body["data"]["id"]
    CREATED_PROBLEM_IDS.append(test_create_problem.pid1)

    # 4.2 正常创建 — 无测试用例
    resp = post_json("/api/admin/problems", {
        "title": "UT_CURL_NoTc",
        "difficulty": "Medium",
        "content": "无测试用例的题目"
    })
    body = resp.json()
    assert_eq("4.2 正常创建无测试用例 code", body.get("code"), 200)
    test_create_problem.pid2 = body["data"]["id"]
    CREATED_PROBLEM_IDS.append(test_create_problem.pid2)

    # 4.3 正常创建 — Hard 难度
    resp = post_json("/api/admin/problems", {
        "title": "UT_CURL_Hard",
        "difficulty": "Hard",
        "content": "困难题目",
        "test_cases": [{"input": "5", "expected": "120"}]
    })
    body = resp.json()
    assert_eq("4.3 正常创建 Hard 难度 code", body.get("code"), 200)
    test_create_problem.pid3 = body["data"]["id"]
    CREATED_PROBLEM_IDS.append(test_create_problem.pid3)

    # 4.4 缺少 title
    resp = post_json("/api/admin/problems", {
        "difficulty": "Easy",
        "content": "缺标题"
    })
    body = resp.json()
    assert_eq("4.4 缺少 title code", body.get("code"), 400)
    assert_eq("4.4 缺少 title message",
              body.get("message"), "title is required")

    # 4.5 缺少 content
    resp = post_json("/api/admin/problems", {
        "title": "UT_CURL_NoContent",
        "difficulty": "Easy"
    })
    body = resp.json()
    assert_eq("4.5 缺少 content code", body.get("code"), 400)
    assert_eq("4.5 缺少 content message",
              body.get("message"), "content is required")

    # 4.6 无效 difficulty
    resp = post_json("/api/admin/problems", {
        "title": "UT_CURL_BadDiff",
        "difficulty": "SuperHard",
        "content": "无效难度"
    })
    body = resp.json()
    assert_eq("4.6 无效 difficulty code", body.get("code"), 400)
    assert_eq("4.6 无效 difficulty message",
              body.get("message"), "invalid difficulty")

    # 4.7 非法 JSON
    resp = post_raw("/api/admin/problems", "{bad json")
    body = resp.json()
    assert_eq("4.7 非法 JSON code", body.get("code"), 400)
    assert_contains("4.7 非法 JSON message",
                    body.get("message", ""), "invalid JSON")


# =====================================================================
# 5. 题目列表测试
# =====================================================================
def test_list_problems():
    print("\n--- 5. 题目列表 ---")

    resp = get_json("/api/problems")
    body = resp.json()
    assert_eq("5.1 题目列表 code", body.get("code"), 200)
    assert_eq("5.1 题目列表 message", body.get("message"), "ok")

    titles = [p["title"] for p in body["data"]]
    assert_contains("5.1 列表含 UT_CURL_AB",
                    " ".join(titles), "UT_CURL_AB")
    assert_contains("5.1 列表含 UT_CURL_NoTc",
                    " ".join(titles), "UT_CURL_NoTc")
    assert_contains("5.1 列表含 UT_CURL_Hard",
                    " ".join(titles), "UT_CURL_Hard")


# =====================================================================
# 6. 题目详情测试
# =====================================================================
def test_get_problem():
    print("\n--- 6. 题目详情 ---")

    pid1 = test_create_problem.pid1
    pid2 = test_create_problem.pid2

    # 6.1 存在的题目 — 含测试用例
    resp = get_json(f"/api/problems/{pid1}")
    body = resp.json()
    assert_eq("6.1 题目详情 code", body.get("code"), 200)
    data = body.get("data", {})
    assert_eq("6.1 题目详情 title", data.get("title"), "UT_CURL_AB")
    test_cases = data.get("test_cases", [])
    assert_eq("6.1 题目详情测试用例数", len(test_cases), 2)
    if test_cases:
        assert_eq("6.1 测试用例0 expected",
                  test_cases[0].get("expected"), "3")

    # 6.2 存在的题目 — 无测试用例
    resp = get_json(f"/api/problems/{pid2}")
    body = resp.json()
    assert_eq("6.2 无测试用例详情 code", body.get("code"), 200)
    data = body.get("data", {})
    assert_eq("6.2 无测试用例详情 title",
              data.get("title"), "UT_CURL_NoTc")
    assert_eq("6.2 无测试用例详情 test_cases为空",
              len(data.get("test_cases", [])), 0)

    # 6.3 不存在的题目
    resp = get_json("/api/problems/9999999")
    body = resp.json()
    assert_eq("6.3 不存在的题目 code", body.get("code"), 404)
    assert_eq("6.3 不存在的题目 message",
              body.get("message"), "problem not found")


# =====================================================================
# 7. 提交代码执行测试
# =====================================================================
def test_submit():
    print("\n--- 7. 提交代码执行 ---")

    pid1 = test_create_problem.pid1

    # 7.1 AC
    resp = post_json("/api/submit", {
        "problem_id": pid1,
        "code": "#include <iostream>\n"
                "int main(){int a,b;std::cin>>a>>b;"
                "std::cout<<a+b<<std::endl;return 0;}"
    })
    body = resp.json()
    data = body.get("data", {})
    assert_eq("7.1 提交 AC status", data.get("status"), "AC")
    assert_eq("7.1 提交 AC passed", data.get("passed"), 2)

    # 7.2 WA
    resp = post_json("/api/submit", {
        "problem_id": pid1,
        "code": "#include <iostream>\n"
                "int main(){int a,b;std::cin>>a>>b;"
                "std::cout<<a*b<<std::endl;return 0;}"
    })
    body = resp.json()
    data = body.get("data", {})
    assert_eq("7.2 提交 WA status", data.get("status"), "WA")
    assert_eq("7.2 提交 WA passed", data.get("passed"), 0)

    # 7.3 CE
    resp = post_json("/api/submit", {
        "problem_id": pid1,
        "code": "int main(){ syntax error }"
    })
    body = resp.json()
    data = body.get("data", {})
    assert_eq("7.3 提交 CE status", data.get("status"), "CE")
    assert_contains("7.3 提交 CE compile_output",
                    data.get("compile_output", ""), "error")

    # 7.4 TLE
    resp = post_json("/api/submit", {
        "problem_id": pid1,
        "code": "int main(){while(1){}return 0;}"
    })
    body = resp.json()
    data = body.get("data", {})
    assert_eq("7.4 提交 TLE status", data.get("status"), "TLE")

    # 7.5 RE
    resp = post_json("/api/submit", {
        "problem_id": pid1,
        "code": "int main(){int*p=nullptr;*p=42;return 0;}"
    })
    body = resp.json()
    data = body.get("data", {})
    assert_eq("7.5 提交 RE status", data.get("status"), "RE")
    cases = data.get("cases", [])
    if cases:
        assert_contains("7.5 提交 RE error",
                        cases[0].get("error", ""), "segmentation fault")

    # 7.6 题目不存在
    resp = post_json("/api/submit", {
        "problem_id": 9999999,
        "code": "int main(){}"
    })
    body = resp.json()
    assert_eq("7.6 题目不存在 code", body.get("code"), 404)
    assert_eq("7.6 题目不存在 message",
              body.get("message"), "problem not found")

    # 7.7 缺少 code
    resp = post_json("/api/submit", {"problem_id": pid1})
    body = resp.json()
    assert_eq("7.7 缺少 code code", body.get("code"), 400)
    assert_eq("7.7 缺少 code message",
              body.get("message"), "code is required")


# =====================================================================
# 8. 删除题目测试
# =====================================================================
def test_delete_problem():
    print("\n--- 8. 删除题目 ---")

    pid1 = test_create_problem.pid1
    pid2 = test_create_problem.pid2
    pid3 = test_create_problem.pid3

    # 8.1 正常删除
    resp = delete_json(f"/api/admin/problems/{pid1}")
    body = resp.json()
    assert_eq("8.1 正常删除 code", body.get("code"), 200)
    assert_eq("8.1 正常删除 message", body.get("message"), "deleted")

    # 8.2 验证已删除 — 查询详情返回 404
    resp = get_json(f"/api/problems/{pid1}")
    body = resp.json()
    assert_eq("8.2 删除后查详情 404 code", body.get("code"), 404)
    assert_eq("8.2 删除后查详情 404 message",
              body.get("message"), "problem not found")

    # 8.3 删除不存在的题目
    resp = delete_json("/api/admin/problems/9999999")
    body = resp.json()
    assert_eq("8.3 删除不存在的题目 code", body.get("code"), 200)

    # 8.4 清理剩余测试题目
    delete_json(f"/api/admin/problems/{pid2}")
    delete_json(f"/api/admin/problems/{pid3}")
    print("  [PASS] 8.5 清理剩余测试题目")


# =====================================================================
# 主函数
# =====================================================================
def main():
    parser = argparse.ArgumentParser(
        description="OJ 系统 API 接口自动化测试")
    parser.add_argument("--host", default="localhost",
                        help="服务器地址 (默认: localhost)")
    parser.add_argument("--port", type=int, default=8080,
                        help="服务器端口 (默认: 8080)")
    args = parser.parse_args()

    global BASE_URL
    BASE_URL = f"http://{args.host}:{args.port}"

    print("=" * 42)
    print("  OJ 系统 Python 接口自动化测试")
    print(f"  目标: {BASE_URL}")
    print("=" * 42)

    # 等待服务器就绪
    print("\n等待服务器就绪...", end=" ")
    for i in range(30):
        try:
            resp = get_json("/api/problems")
            if resp.status_code == 200:
                print("OK")
                break
        except requests.exceptions.ConnectionError:
            pass
        time.sleep(0.5)
    else:
        print("FAILED")
        print("无法连接服务器，请确认服务器已启动")
        sys.exit(1)

    # 执行测试
    test_register()
    test_login()
    test_logout()
    test_create_problem()
    test_list_problems()
    test_get_problem()
    test_submit()
    test_delete_problem()

    # 输出结果
    print()
    print("=" * 42)
    print(f"  测试结果: PASS={PASS_COUNT}  FAIL={FAIL_COUNT}")
    print("=" * 42)

    if FAIL_COUNT > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
