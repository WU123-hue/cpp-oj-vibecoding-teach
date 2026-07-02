#include "auth_handler.h"

#include <nlohmann/json.hpp>

#include <string>

#include "service/auth_service.h"
#include "service/session_manager.h"
#include "utils/logger.h"

namespace oj {

using json = nlohmann::json;

namespace {

void JsonResp(httplib::Response& res, int status, const std::string& msg,
              const json& data = nullptr) {
  json body;
  body["code"] = status;
  body["message"] = msg;
  if (!data.is_null()) {
    body["data"] = data;
  }
  res.status = status;
  res.set_content(body.dump(), "application/json");
}

// 从请求 Cookie 头中提取指定 Cookie 值
std::string ExtractCookie(const std::string& cookie_header,
                          const std::string& name) {
  std::string key = name + "=";
  size_t pos = cookie_header.find(key);
  if (pos == std::string::npos) return "";

  pos += key.size();
  size_t end = cookie_header.find(';', pos);
  if (end == std::string::npos) end = cookie_header.size();
  return cookie_header.substr(pos, end - pos);
}

}  // namespace

void RegisterAuthHandlers(httplib::Server& server) {
  // POST /api/register — 注册新用户
  // 请求体: { "username": "alice", "password": "secret123" }
  server.Post("/api/register", [](const httplib::Request& req,
                                    httplib::Response& res) {
    json j;
    try {
      j = json::parse(req.body);
    } catch (const std::exception& e) {
      JsonResp(res, 400, "invalid JSON: " + std::string(e.what()));
      return;
    }

    if (!j.contains("username") || !j["username"].is_string()) {
      JsonResp(res, 400, "username is required");
      return;
    }
    if (!j.contains("password") || !j["password"].is_string()) {
      JsonResp(res, 400, "password is required");
      return;
    }

    RegisterRequest reg_req;
    reg_req.username = j["username"].get<std::string>();
    reg_req.password = j["password"].get<std::string>();

    AuthService svc;
    int new_id = 0;
    std::string reason;
    if (!svc.Register(reg_req, &new_id, &reason)) {
      JsonResp(res, 400, reason);
      return;
    }

    json data;
    data["id"] = new_id;
    data["username"] = reg_req.username;
    JsonResp(res, 200, "registered", data);
  });

  // POST /api/login — 用户登录
  // 请求体: { "username": "alice", "password": "secret123" }
  // 成功响应: Set-Cookie: oj_session=<sid>; Path=/; HttpOnly
  server.Post("/api/login", [](const httplib::Request& req,
                                 httplib::Response& res) {
    json j;
    try {
      j = json::parse(req.body);
    } catch (const std::exception& e) {
      JsonResp(res, 400, "invalid JSON: " + std::string(e.what()));
      return;
    }

    if (!j.contains("username") || !j["username"].is_string() ||
        j["username"].get<std::string>().empty()) {
      JsonResp(res, 400, "username is required");
      return;
    }
    if (!j.contains("password") || !j["password"].is_string() ||
        j["password"].get<std::string>().empty()) {
      JsonResp(res, 400, "password is required");
      return;
    }

    LoginRequest login_req;
    login_req.username = j["username"].get<std::string>();
    login_req.password = j["password"].get<std::string>();

    AuthService svc;
    LoginResult result;
    std::string reason;
    if (!svc.Login(login_req, &result, &reason)) {
      JsonResp(res, 401, reason);
      return;
    }

    // 设置 Set-Cookie 响应头
    std::string cookie = std::string(SessionManager::CookieName()) + "=" +
                         result.session_id + "; Path=/; HttpOnly";
    res.set_header("Set-Cookie", cookie);

    json data;
    data["id"]       = result.user_id;
    data["username"] = result.username;
    data["role"]     = result.role;
    JsonResp(res, 200, "login successful", data);
  });

  // POST /api/logout — 用户登出
  // 从 Cookie 中提取 session_id 并销毁会话
  server.Post("/api/logout", [](const httplib::Request& req,
                                  httplib::Response& res) {
    std::string cookie_header = req.get_header_value("Cookie");
    std::string sid = ExtractCookie(cookie_header,
                                    SessionManager::CookieName());

    if (!sid.empty()) {
      SessionManager::Instance().DestroySession(sid);
    }

    // 清除客户端 Cookie
    std::string cookie = std::string(SessionManager::CookieName()) +
                         "=; Path=/; HttpOnly; Max-Age=0";
    res.set_header("Set-Cookie", cookie);

    JsonResp(res, 200, "logged out");
  });
}

}  // namespace oj
