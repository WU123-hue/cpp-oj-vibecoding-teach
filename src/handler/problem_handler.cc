#include "problem_handler.h"

#include <nlohmann/json.hpp>

#include <string>

#include "service/problem_service.h"

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

void RegisterProblemHandlers(httplib::Server& server) {
  // GET /api/problems — 题目列表
  server.Get("/api/problems", [](const httplib::Request&,
                                  httplib::Response& res) {
    ProblemService svc;
    std::vector<ProblemSummary> list;
    std::string reason;
    if (!svc.List(&list, &reason)) {
      JsonResp(res, 500, reason);
      return;
    }

    json data = json::array();
    for (const auto& p : list) {
      data.push_back({{"id", p.id}, {"title", p.title}, {"difficulty", p.difficulty}});
    }
    JsonResp(res, 200, "ok", data);
  });

  // GET /api/problems/:id — 题目详情
  server.Get(R"(/api/problems/(\d+))",
             [](const httplib::Request& req, httplib::Response& res) {
    int id = std::stoi(req.matches[1]);
    ProblemService svc;
    Problem problem;
    std::vector<TestCase> tcs;
    std::string reason;
    if (!svc.Get(id, &problem, &tcs, &reason)) {
      JsonResp(res, 404, reason);
      return;
    }

    json data;
    data["id"] = problem.id();
    data["title"] = problem.title();
    data["difficulty"] = DifficultyToStr(problem.difficulty());
    data["content"] = problem.content();
    data["template"] = problem.tpl();
    data["created_at"] = problem.created_at();

    json tc_arr = json::array();
    for (const auto& tc : tcs) {
      tc_arr.push_back({
        {"id", tc.id()},
        {"input", tc.input()},
        {"expected", tc.expected()},
        {"position", tc.position()},
      });
    }
    data["test_cases"] = tc_arr;

    JsonResp(res, 200, "ok", data);
  });
}

}  // namespace oj
