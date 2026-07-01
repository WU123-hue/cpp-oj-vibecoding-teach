#include "auth_handler.h"

#include <nlohmann/json.hpp>

#include <string>

#include "service/auth_service.h"
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
}

}  // namespace oj
