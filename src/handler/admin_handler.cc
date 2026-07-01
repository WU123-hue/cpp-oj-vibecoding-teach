#include "admin_handler.h"

#include <nlohmann/json.hpp>

#include <string>

#include "service/problem_service.h"
#include "utils/logger.h"

namespace oj {

using json = nlohmann::json;

namespace {

// 统一 JSON 响应
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

// 解析新增题目请求 JSON
bool ParseCreateRequest(const std::string& body, CreateProblemRequest& req,
                        std::string& err) {
  try {
    json j = json::parse(body);
    req.title = j.value("title", "");
    req.difficulty = j.value("difficulty", "");
    req.content = j.value("content", "");
    req.tpl = j.value("template", "");

    if (j.contains("test_cases")) {
      for (auto& tc : j["test_cases"]) {
        TestCase t;
        t.set_input(tc.value("input", ""));
        t.set_expected(tc.value("expected", ""));
        req.test_cases.push_back(std::move(t));
      }
    }
    return true;
  } catch (const std::exception& e) {
    err = e.what();
    return false;
  }
}

}  // namespace

void RegisterAdminHandlers(httplib::Server& server) {
  // POST /api/admin/problems — 新增题目
  server.Post("/api/admin/problems", [](const httplib::Request& req,
                                         httplib::Response& res) {
    std::string err;
    CreateProblemRequest create_req;
    if (!ParseCreateRequest(req.body, create_req, err)) {
      JsonResp(res, 400, "invalid JSON: " + err);
      return;
    }

    ProblemService svc;
    int new_id = 0;
    std::string reason;
    if (!svc.Create(create_req, &new_id, &reason)) {
      JsonResp(res, 400, reason);
      return;
    }

    json data;
    data["id"] = new_id;
    JsonResp(res, 200, "created", data);
  });

  // DELETE /api/admin/problems/:id — 删除题目
  server.Delete(R"(/api/admin/problems/(\d+))",
                [](const httplib::Request& req, httplib::Response& res) {
    int id = std::stoi(req.matches[1]);
    ProblemService svc;
    std::string reason;
    if (!svc.Delete(id, &reason)) {
      JsonResp(res, 400, reason);
      return;
    }
    JsonResp(res, 200, "deleted");
  });
}

}  // namespace oj
