#include "submit_handler.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include "service/executor_service.h"
#include "service/problem_service.h"
#include "utils/config.h"
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

void RegisterSubmitHandlers(httplib::Server& server) {
  // POST /api/submit — 提交代码执行
  // 请求体: { "problem_id": 1, "code": "#include ..." }
  server.Post("/api/submit", [](const httplib::Request& req,
                                 httplib::Response& res) {
    // 1. 解析请求
    json j;
    try {
      j = json::parse(req.body);
    } catch (const std::exception& e) {
      JsonResp(res, 400, "invalid JSON: " + std::string(e.what()));
      return;
    }

    if (!j.contains("problem_id") || !j["problem_id"].is_number()) {
      JsonResp(res, 400, "problem_id is required");
      return;
    }
    if (!j.contains("code") || !j["code"].is_string()) {
      JsonResp(res, 400, "code is required");
      return;
    }

    int problem_id = j["problem_id"].get<int>();
    std::string code = j["code"].get<std::string>();

    if (code.empty()) {
      JsonResp(res, 400, "code is empty");
      return;
    }

    // 2. 获取题目详情（含测试用例）
    ProblemService psvc;
    Problem problem;
    std::vector<TestCase> tcs;
    std::string reason;
    if (!psvc.Get(problem_id, &problem, &tcs, &reason)) {
      JsonResp(res, 404, reason);
      return;
    }

    // 3. 组装执行用例
    std::vector<ExecTestCase> exec_cases;
    for (const auto& tc : tcs) {
      exec_cases.push_back({tc.input(), tc.expected()});
    }

    // 4. 执行
    auto& cfg = Config::Instance();
    ExecutorService esvc;
    ExecutorResult eresult = esvc.Execute(
        code, exec_cases,
        cfg.executor().timeout_sec,
        cfg.executor().cpu_limit_sec,
        cfg.executor().mem_limit_mb);

    // 5. 构造响应
    json data;
    data["status"] = JudgeStatusToStr(eresult.overall);
    data["passed"] = eresult.passed;
    data["total"]  = eresult.total;
    data["max_time_ms"] = eresult.max_elapsed_ms;

    if (eresult.overall == JudgeStatus::CE) {
      data["compile_output"] = eresult.compile_output;
    }

    json cases_arr = json::array();
    for (const auto& cr : eresult.cases) {
      json cr_obj;
      cr_obj["status"] = JudgeStatusToStr(cr.status);
      cr_obj["exit_code"] = cr.exit_code;
      cr_obj["time_ms"] = cr.elapsed_ms;
      cr_obj["actual"] = cr.actual;
      if (cr.status == JudgeStatus::RE ||
          cr.status == JudgeStatus::TLE) {
        cr_obj["error"] = cr.error_msg;
      }
      cases_arr.push_back(cr_obj);
    }
    data["cases"] = cases_arr;

    JsonResp(res, 200, "ok", data);
    LOG_INFO_FMT("submit: problem_id=%d status=%s passed=%d/%d",
                 problem_id, JudgeStatusToStr(eresult.overall).c_str(),
                 eresult.passed, eresult.total);
  });
}

}  // namespace oj
