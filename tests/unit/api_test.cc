#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <mysql/mysql.h>

#include <string>
#include <thread>

#include "db/connection_pool.h"
#include "handler/admin_handler.h"
#include "handler/problem_handler.h"
#include "utils/config.h"
#include "utils/httplib.h"
#include "utils/logger.h"

namespace {

constexpr const char* kHost     = "localhost";
constexpr const int   kPort     = 3306;
constexpr const char* kUser     = "root";
constexpr const char* kPassword = "";
constexpr const char* kDatabase = "oj_db";

// 测试用 HTTP 端口（避免与正式 8080 冲突）
constexpr int kTestPort = 18080;

oj::ConnectionPool& Pool() { return oj::ConnectionPool::Instance(); }

using json = nlohmann::json;

// 辅助：执行 SQL 返回标量
std::string QueryScalar(MYSQL* c, const std::string& sql) {
  if (::mysql_query(c, sql.c_str()) != 0) return "";
  MYSQL_RES* res = ::mysql_store_result(c);
  if (res == nullptr) return "";
  MYSQL_ROW row = ::mysql_fetch_row(res);
  std::string val = (row && row[0]) ? row[0] : "";
  ::mysql_free_result(res);
  return val;
}

// 辅助：清理 UT_API_ 前缀的测试数据
void CleanTestData() {
  oj::ConnectionGuard g;
  if (!g.Valid()) return;
  ::mysql_query(g.Get(), "DELETE FROM problems WHERE title LIKE 'UT_API_%'");
}

}  // namespace

// =============================================================================
// 全局环境：启动 HTTP 服务器 + 初始化连接池
// =============================================================================
class ApiTest : public ::testing::Test {
 protected:
  static httplib::Server server_;
  static std::thread     server_thread_;
  static std::unique_ptr<httplib::Client> cli_;

  static void SetUpTestSuite() {
    oj::Logger::Instance().Init("error", "/tmp/oj_api_ut", "api.log");
    ASSERT_TRUE(Pool().Init(kHost, kPort, kUser, kPassword, kDatabase, 2));

    // 注册路由
    oj::RegisterAdminHandlers(server_);
    oj::RegisterProblemHandlers(server_);

    // 启动服务器（非阻塞）
    server_thread_ = std::thread([]() {
      server_.listen("127.0.0.1", kTestPort);
    });

    // 等待服务器就绪
    cli_ = std::make_unique<httplib::Client>("127.0.0.1", kTestPort);
    cli_->set_connection_timeout(5);
    cli_->set_read_timeout(5);

    // 轮询等待
    for (int i = 0; i < 50; ++i) {
      auto res = cli_->Get("/api/problems");
      if (res) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  static void TearDownTestSuite() {
    server_.stop();
    if (server_thread_.joinable()) server_thread_.join();
    Pool().Destroy();
  }

  void SetUp() override { CleanTestData(); }
  void TearDown() override { CleanTestData(); }
};

httplib::Server                          ApiTest::server_;
std::thread                              ApiTest::server_thread_;
std::unique_ptr<httplib::Client>         ApiTest::cli_;

// =============================================================================
// POST /api/admin/problems — 正常创建题目
// =============================================================================
TEST_F(ApiTest, CreateProblemSuccess) {
  json body = {
    {"title", "UT_API_CreateSuccess"},
    {"difficulty", "Easy"},
    {"content", "求两个整数之和"},
    {"template", "#include <iostream>\nint main(){}"},
    {"test_cases", {
      {{"input", "1 2"}, {"expected", "3"}},
      {{"input", "10 20"}, {"expected", "30"}}
    }}
  };

  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 200);
  EXPECT_EQ(resp["message"], "created");
  EXPECT_TRUE(resp["data"].contains("id"));
  int new_id = resp["data"]["id"];
  EXPECT_GT(new_id, 0);
}

// =============================================================================
// POST /api/admin/problems — 创建不含测试用例
// =============================================================================
TEST_F(ApiTest, CreateProblemNoTestCases) {
  json body = {
    {"title", "UT_API_NoTestCases"},
    {"difficulty", "Medium"},
    {"content", "无测试用例"}
  };

  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 200);
  EXPECT_EQ(resp["message"], "created");
  EXPECT_GT(resp["data"]["id"].get<int>(), 0);
}

// =============================================================================
// POST /api/admin/problems — 缺少 title 返回 400
// =============================================================================
TEST_F(ApiTest, CreateProblemMissingTitle) {
  json body = {
    {"difficulty", "Easy"},
    {"content", "缺少标题"}
  };

  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 400);
  EXPECT_EQ(resp["message"], "title is required");
}

// =============================================================================
// POST /api/admin/problems — 空 title 返回 400
// =============================================================================
TEST_F(ApiTest, CreateProblemEmptyTitle) {
  json body = {
    {"title", ""},
    {"difficulty", "Easy"},
    {"content", "空标题"}
  };

  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 400);
  EXPECT_EQ(resp["message"], "title is required");
}

// =============================================================================
// POST /api/admin/problems — 缺少 content 返回 400
// =============================================================================
TEST_F(ApiTest, CreateProblemMissingContent) {
  json body = {
    {"title", "UT_API_NoContent"},
    {"difficulty", "Easy"}
  };

  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 400);
  EXPECT_EQ(resp["message"], "content is required");
}

// =============================================================================
// POST /api/admin/problems — 无效 difficulty 返回 400
// =============================================================================
TEST_F(ApiTest, CreateProblemInvalidDifficulty) {
  json body = {
    {"title", "UT_API_InvalidDiff"},
    {"difficulty", "SuperHard"},
    {"content", "无效难度"}
  };

  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 400);
  EXPECT_EQ(resp["message"], "invalid difficulty");
}

// =============================================================================
// POST /api/admin/problems — 空 difficulty 返回 400
// =============================================================================
TEST_F(ApiTest, CreateProblemEmptyDifficulty) {
  json body = {
    {"title", "UT_API_EmptyDiff"},
    {"difficulty", ""},
    {"content", "空难度"}
  };

  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 400);
  EXPECT_EQ(resp["message"], "invalid difficulty");
}

// =============================================================================
// POST /api/admin/problems — 非法 JSON 返回 400
// =============================================================================
TEST_F(ApiTest, CreateProblemInvalidJson) {
  auto res = cli_->Post("/api/admin/problems", "{invalid json", "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 400);
  EXPECT_TRUE(resp["message"].get<std::string>().find("invalid JSON") != std::string::npos);
}

// =============================================================================
// POST /api/admin/problems — 三种合法难度
// =============================================================================
TEST_F(ApiTest, CreateProblemAllDifficulties) {
  const char* diffs[] = {"Easy", "Medium", "Hard"};
  for (const char* d : diffs) {
    json body = {
      {"title", std::string("UT_API_Diff_") + d},
      {"difficulty", d},
      {"content", "难度测试"}
    };

    auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json resp = json::parse(res->body);
    EXPECT_EQ(resp["code"], 200);
  }
}

// =============================================================================
// POST /api/admin/problems — 含特殊字符的内容
// =============================================================================
TEST_F(ApiTest, CreateProblemWithSpecialChars) {
  json body = {
    {"title", "UT_API_Special'\"Chars"},
    {"difficulty", "Easy"},
    {"content", "包含 '单引号' 和 \"双引号\" 及反斜杠 \\ 和换行\n第二行"},
    {"template", "cout << \"hello\";"},
    {"test_cases", {
      {{"input", "in'with\"quotes"}, {"expected", "out'with\"quotes"}}
    }}
  };

  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);
  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 200);
  int new_id = resp["data"]["id"];

  // 验证详情
  auto res2 = cli_->Get("/api/problems/" + std::to_string(new_id));
  ASSERT_TRUE(res2);
  json detail = json::parse(res2->body);
  EXPECT_EQ(detail["data"]["title"], "UT_API_Special'\"Chars");
  EXPECT_EQ(detail["data"]["content"], "包含 '单引号' 和 \"双引号\" 及反斜杠 \\ 和换行\n第二行");
  EXPECT_EQ(detail["data"]["test_cases"][0]["input"], "in'with\"quotes");
  EXPECT_EQ(detail["data"]["test_cases"][0]["expected"], "out'with\"quotes");
}

// =============================================================================
// GET /api/problems — 返回列表
// =============================================================================
TEST_F(ApiTest, ListProblems) {
  // 创建两个题目
  json b1 = {{"title", "UT_API_ListA"}, {"difficulty", "Easy"}, {"content", "a"}};
  json b2 = {{"title", "UT_API_ListB"}, {"difficulty", "Hard"}, {"content", "b"}};
  cli_->Post("/api/admin/problems", b1.dump(), "application/json");
  cli_->Post("/api/admin/problems", b2.dump(), "application/json");

  auto res = cli_->Get("/api/problems");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 200);
  EXPECT_EQ(resp["message"], "ok");
  EXPECT_TRUE(resp["data"].is_array());
  EXPECT_FALSE(resp["data"].empty());

  // 验证每项结构 {id, title, difficulty}
  for (const auto& item : resp["data"]) {
    EXPECT_TRUE(item.contains("id"));
    EXPECT_TRUE(item.contains("title"));
    EXPECT_TRUE(item.contains("difficulty"));
  }

  // 确保包含刚创建的题目
  bool foundA = false, foundB = false;
  for (const auto& item : resp["data"]) {
    if (item["title"] == "UT_API_ListA") {
      EXPECT_EQ(item["difficulty"], "Easy");
      foundA = true;
    }
    if (item["title"] == "UT_API_ListB") {
      EXPECT_EQ(item["difficulty"], "Hard");
      foundB = true;
    }
  }
  EXPECT_TRUE(foundA);
  EXPECT_TRUE(foundB);
}

// =============================================================================
// GET /api/problems/:id — 返回题目详情
// =============================================================================
TEST_F(ApiTest, GetProblemDetail) {
  // 先创建
  json body = {
    {"title", "UT_API_GetDetail"},
    {"difficulty", "Medium"},
    {"content", "详情测试"},
    {"template", "int main(){}"},
    {"test_cases", {
      {{"input", "1 2"}, {"expected", "3"}},
      {{"input", "3 4"}, {"expected", "7"}}
    }}
  };
  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  json resp = json::parse(res->body);
  int new_id = resp["data"]["id"];

  // 获取详情
  auto res2 = cli_->Get("/api/problems/" + std::to_string(new_id));
  ASSERT_TRUE(res2);
  EXPECT_EQ(res2->status, 200);

  json detail = json::parse(res2->body);
  EXPECT_EQ(detail["code"], 200);
  EXPECT_EQ(detail["message"], "ok");

  EXPECT_EQ(detail["data"]["id"], new_id);
  EXPECT_EQ(detail["data"]["title"], "UT_API_GetDetail");
  EXPECT_EQ(detail["data"]["difficulty"], "Medium");
  EXPECT_EQ(detail["data"]["content"], "详情测试");
  EXPECT_EQ(detail["data"]["template"], "int main(){}");
  EXPECT_FALSE(detail["data"]["created_at"].get<std::string>().empty());

  // 验证测试用例
  EXPECT_EQ(detail["data"]["test_cases"].size(), 2);
  EXPECT_EQ(detail["data"]["test_cases"][0]["input"], "1 2");
  EXPECT_EQ(detail["data"]["test_cases"][0]["expected"], "3");
  EXPECT_EQ(detail["data"]["test_cases"][1]["input"], "3 4");
  EXPECT_EQ(detail["data"]["test_cases"][1]["expected"], "7");
}

// =============================================================================
// GET /api/problems/:id — 不存在的 id 返回 404
// =============================================================================
TEST_F(ApiTest, GetProblemNotFound) {
  auto res = cli_->Get("/api/problems/9999999");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 404);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 404);
  EXPECT_EQ(resp["message"], "problem not found");
}

// =============================================================================
// GET /api/problems/:id — 无测试用例的题目详情
// =============================================================================
TEST_F(ApiTest, GetProblemDetailNoTestCases) {
  json body = {
    {"title", "UT_API_GetNoTc"},
    {"difficulty", "Easy"},
    {"content", "无用例题目"}
  };
  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  json resp = json::parse(res->body);
  int new_id = resp["data"]["id"];

  auto res2 = cli_->Get("/api/problems/" + std::to_string(new_id));
  ASSERT_TRUE(res2);
  EXPECT_EQ(res2->status, 200);

  json detail = json::parse(res2->body);
  EXPECT_EQ(detail["data"]["title"], "UT_API_GetNoTc");
  EXPECT_TRUE(detail["data"]["test_cases"].empty());
}

// =============================================================================
// DELETE /api/admin/problems/:id — 正常删除
// =============================================================================
TEST_F(ApiTest, DeleteProblemSuccess) {
  // 先创建
  json body = {
    {"title", "UT_API_Delete"},
    {"difficulty", "Easy"},
    {"content", "待删除"},
    {"test_cases", {{{"input", "1"}, {"expected", "2"}}}}
  };
  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  json resp = json::parse(res->body);
  int new_id = resp["data"]["id"];

  // 删除
  auto res2 = cli_->Delete("/api/admin/problems/" + std::to_string(new_id));
  ASSERT_TRUE(res2);
  EXPECT_EQ(res2->status, 200);

  json resp2 = json::parse(res2->body);
  EXPECT_EQ(resp2["code"], 200);
  EXPECT_EQ(resp2["message"], "deleted");

  // 验证已删除 — GET 返回 404
  auto res3 = cli_->Get("/api/problems/" + std::to_string(new_id));
  ASSERT_TRUE(res3);
  EXPECT_EQ(res3->status, 404);
}

// =============================================================================
// DELETE /api/admin/problems/:id — 级联删除测试用例
// =============================================================================
TEST_F(ApiTest, DeleteProblemCascadesTestCases) {
  // 创建带多个测试用例的题目
  json body = {
    {"title", "UT_API_DeleteCascade"},
    {"difficulty", "Hard"},
    {"content", "级联删除"},
    {"test_cases", {
      {{"input", "0"}, {"expected", "1"}},
      {{"input", "1"}, {"expected", "2"}},
      {{"input", "2"}, {"expected", "3"}}
    }}
  };
  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  json resp = json::parse(res->body);
  int new_id = resp["data"]["id"];

  // 确认有 3 个测试用例
  auto res2 = cli_->Get("/api/problems/" + std::to_string(new_id));
  ASSERT_TRUE(res2);
  json detail = json::parse(res2->body);
  EXPECT_EQ(detail["data"]["test_cases"].size(), 3);

  // 删除
  auto res3 = cli_->Delete("/api/admin/problems/" + std::to_string(new_id));
  ASSERT_TRUE(res3);
  EXPECT_EQ(res3->status, 200);

  // 验证 DB 中测试用例也被删除
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM test_cases WHERE problem_id=" +
          std::to_string(new_id)), "0");
}

// =============================================================================
// 完整 API 生命周期：Create -> List -> Get -> Delete -> Get 404
// =============================================================================
TEST_F(ApiTest, FullApiLifecycle) {
  // Create
  json body = {
    {"title", "UT_API_Lifecycle"},
    {"difficulty", "Hard"},
    {"content", "完整生命周期"},
    {"template", "// tpl"},
    {"test_cases", {{{"input", "42"}, {"expected", "42"}}}}
  };
  auto res = cli_->Post("/api/admin/problems", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);
  json resp = json::parse(res->body);
  int pid = resp["data"]["id"];
  EXPECT_GT(pid, 0);

  // List 中能找到
  auto res2 = cli_->Get("/api/problems");
  ASSERT_TRUE(res2);
  json list = json::parse(res2->body);
  bool found = false;
  for (const auto& item : list["data"]) {
    if (item["id"] == pid) { found = true; break; }
  }
  EXPECT_TRUE(found);

  // Get 详情
  auto res3 = cli_->Get("/api/problems/" + std::to_string(pid));
  ASSERT_TRUE(res3);
  EXPECT_EQ(res3->status, 200);
  json detail = json::parse(res3->body);
  EXPECT_EQ(detail["data"]["title"], "UT_API_Lifecycle");
  EXPECT_EQ(detail["data"]["difficulty"], "Hard");

  // Delete
  auto res4 = cli_->Delete("/api/admin/problems/" + std::to_string(pid));
  ASSERT_TRUE(res4);
  EXPECT_EQ(res4->status, 200);

  // Get 返回 404
  auto res5 = cli_->Get("/api/problems/" + std::to_string(pid));
  ASSERT_TRUE(res5);
  EXPECT_EQ(res5->status, 404);
}

// =============================================================================
// POST /api/admin/problems — 空 body 返回 400
// =============================================================================
TEST_F(ApiTest, CreateProblemEmptyBody) {
  auto res = cli_->Post("/api/admin/problems", "", "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 400);
}
