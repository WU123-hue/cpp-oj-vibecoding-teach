#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <mysql/mysql.h>

#include <string>
#include <thread>

#include "db/connection_pool.h"
#include "handler/auth_handler.h"
#include "utils/httplib.h"
#include "utils/logger.h"

namespace {

constexpr const char* kHost     = "localhost";
constexpr const int   kPort     = 3306;
constexpr const char* kUser     = "root";
constexpr const char* kPassword = "";
constexpr const char* kDatabase = "oj_db";

constexpr int kTestPort = 18081;

oj::ConnectionPool& Pool() { return oj::ConnectionPool::Instance(); }

using json = nlohmann::json;

std::string QueryScalar(MYSQL* c, const std::string& sql) {
  if (::mysql_query(c, sql.c_str()) != 0) return "";
  MYSQL_RES* res = ::mysql_store_result(c);
  if (res == nullptr) return "";
  MYSQL_ROW row = ::mysql_fetch_row(res);
  std::string val = (row && row[0]) ? row[0] : "";
  ::mysql_free_result(res);
  return val;
}

void CleanTestUsers() {
  oj::ConnectionGuard g;
  if (!g.Valid()) return;
  ::mysql_query(g.Get(), "DELETE FROM users WHERE username LIKE 'UT_APIAUTH_%'");
}

}  // namespace

class AuthApiTest : public ::testing::Test {
 protected:
  static httplib::Server                          server_;
  static std::thread                              server_thread_;
  static std::unique_ptr<httplib::Client>         cli_;

  static void SetUpTestSuite() {
    oj::Logger::Instance().Init("error", "/tmp/oj_authapi_ut", "authapi.log");
    ASSERT_TRUE(Pool().Init(kHost, kPort, kUser, kPassword, kDatabase, 2));

    oj::RegisterAuthHandlers(server_);

    server_thread_ = std::thread([]() {
      server_.listen("127.0.0.1", kTestPort);
    });

    cli_ = std::make_unique<httplib::Client>("127.0.0.1", kTestPort);
    cli_->set_connection_timeout(5);
    cli_->set_read_timeout(5);

    for (int i = 0; i < 50; ++i) {
      auto res = cli_->Post("/api/register", "{}", "application/json");
      if (res) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  static void TearDownTestSuite() {
    server_.stop();
    if (server_thread_.joinable()) server_thread_.join();
    Pool().Destroy();
  }

  void SetUp() override { CleanTestUsers(); }
  void TearDown() override { CleanTestUsers(); }
};

httplib::Server                          AuthApiTest::server_;
std::thread                              AuthApiTest::server_thread_;
std::unique_ptr<httplib::Client>         AuthApiTest::cli_;

// =============================================================================
// POST /api/register — 正常注册
// =============================================================================
TEST_F(AuthApiTest, RegisterSuccess) {
  json body = {{"username", "UT_APIAUTH_success"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 200);
  EXPECT_EQ(resp["message"], "registered");
  EXPECT_TRUE(resp["data"].contains("id"));
  EXPECT_GT(resp["data"]["id"].get<int>(), 0);
  EXPECT_EQ(resp["data"]["username"], "UT_APIAUTH_success");
}

// =============================================================================
// POST /api/register — 重复用户名返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterDuplicateUsername) {
  json body = {{"username", "UT_APIAUTH_dup"}, {"password", "pass123456"}};
  auto res1 = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res1);
  EXPECT_EQ(res1->status, 200);

  auto res2 = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res2);
  EXPECT_EQ(res2->status, 400);

  json resp = json::parse(res2->body);
  EXPECT_EQ(resp["code"], 400);
  EXPECT_EQ(resp["message"], "username already exists");
}

// =============================================================================
// POST /api/register — 空 username 返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterEmptyUsername) {
  json body = {{"username", ""}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 400);
  EXPECT_EQ(resp["message"], "username is required");
}

// =============================================================================
// POST /api/register — 缺少 username 字段返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterMissingUsername) {
  json body = {{"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 400);
  EXPECT_EQ(resp["message"], "username is required");
}

// =============================================================================
// POST /api/register — username 非字符串返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterUsernameNotString) {
  json body = {{"username", 123}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "username is required");
}

// =============================================================================
// POST /api/register — 空 password 返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterEmptyPassword) {
  json body = {{"username", "UT_APIAUTH_emptypwd"}, {"password", ""}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "password is required");
}

// =============================================================================
// POST /api/register — 缺少 password 字段返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterMissingPassword) {
  json body = {{"username", "UT_APIAUTH_nopwd"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "password is required");
}

// =============================================================================
// POST /api/register — password 非字符串返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterPasswordNotString) {
  json body = {{"username", "UT_APIAUTH_pwdnum"}, {"password", 123456}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "password is required");
}

// =============================================================================
// POST /api/register — username 过短返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterShortUsername) {
  json body = {{"username", "ab"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "username must be 3-64 characters");
}

// =============================================================================
// POST /api/register — username 过长返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterLongUsername) {
  json body = {{"username", std::string(65, 'a')}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "username must be 3-64 characters");
}

// =============================================================================
// POST /api/register — password 过短返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterShortPassword) {
  json body = {{"username", "UT_APIAUTH_shortpwd"}, {"password", "12345"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "password must be 6-64 characters");
}

// =============================================================================
// POST /api/register — password 过长返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterLongPassword) {
  json body = {{"username", "UT_APIAUTH_longpwd"}, {"password", std::string(65, 'p')}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "password must be 6-64 characters");
}

// =============================================================================
// POST /api/register — 非法 JSON 返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterInvalidJson) {
  auto res = cli_->Post("/api/register", "{invalid json", "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 400);
  EXPECT_TRUE(resp["message"].get<std::string>().find("invalid JSON") != std::string::npos);
}

// =============================================================================
// POST /api/register — 空 body 返回 400
// =============================================================================
TEST_F(AuthApiTest, RegisterEmptyBody) {
  auto res = cli_->Post("/api/register", "", "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["code"], 400);
}

// =============================================================================
// POST /api/register — 注册后密码在数据库中为 bcrypt 哈希
// =============================================================================
TEST_F(AuthApiTest, RegisterPasswordStoredAsBcrypt) {
  json body = {{"username", "UT_APIAUTH_bcrypt"}, {"password", "secret123"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  ASSERT_EQ(res->status, 200);

  int uid = json::parse(res->body)["data"]["id"];

  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  std::string stored = QueryScalar(g.Get(),
      "SELECT password FROM users WHERE id=" + std::to_string(uid));
  EXPECT_TRUE(stored.rfind("$2a$", 0) == 0);
  EXPECT_NE(stored, "secret123");
}

// =============================================================================
// POST /api/register — 注册后角色为 user
// =============================================================================
TEST_F(AuthApiTest, RegisterRoleIsUser) {
  json body = {{"username", "UT_APIAUTH_role"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  ASSERT_EQ(res->status, 200);

  int uid = json::parse(res->body)["data"]["id"];

  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT role FROM users WHERE id=" + std::to_string(uid)), "user");
}

// =============================================================================
// POST /api/register — 响应 Content-Type 为 application/json
// =============================================================================
TEST_F(AuthApiTest, RegisterResponseContentType) {
  json body = {{"username", "UT_APIAUTH_ctype"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  auto ct = res->get_header_value("Content-Type");
  EXPECT_NE(ct.find("application/json"), std::string::npos);
}

// =============================================================================
// POST /api/register — 多次注册不同用户均成功
// =============================================================================
TEST_F(AuthApiTest, RegisterMultipleUsers) {
  const char* names[] = {"UT_APIAUTH_multi1", "UT_APIAUTH_multi2", "UT_APIAUTH_multi3"};
  for (const char* name : names) {
    json body = {{"username", name}, {"password", "pass123456"}};
    auto res = cli_->Post("/api/register", body.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(json::parse(res->body)["data"]["username"], name);
  }

  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM users WHERE username LIKE 'UT_APIAUTH_multi%'"), "3");
}

// =============================================================================
// POST /api/register — 用户名含特殊字符
// =============================================================================
TEST_F(AuthApiTest, RegisterUsernameWithSpecialChars) {
  json body = {{"username", "UT_APIAUTH_q'Name"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  int uid = json::parse(res->body)["data"]["id"];

  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT username FROM users WHERE id=" + std::to_string(uid)),
      "UT_APIAUTH_q'Name");
}

// =============================================================================
// POST /api/register — 额外字段不干扰注册
// =============================================================================
TEST_F(AuthApiTest, RegisterExtraFieldsIgnored) {
  json body = {
    {"username", "UT_APIAUTH_extra"},
    {"password", "pass123456"},
    {"extra_field", "ignored"},
    {"role", "admin"}
  };
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  int uid = json::parse(res->body)["data"]["id"];

  // role 应为 user，不受请求中 role 字段影响
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT role FROM users WHERE id=" + std::to_string(uid)), "user");
}

// =============================================================================
// POST /api/register — 响应体结构完整 {code, message, data{id, username}}
// =============================================================================
TEST_F(AuthApiTest, RegisterResponseStructure) {
  json body = {{"username", "UT_APIAUTH_struct"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  ASSERT_EQ(res->status, 200);

  json resp = json::parse(res->body);
  EXPECT_TRUE(resp.contains("code"));
  EXPECT_TRUE(resp.contains("message"));
  EXPECT_TRUE(resp.contains("data"));
  EXPECT_TRUE(resp["data"].contains("id"));
  EXPECT_TRUE(resp["data"].contains("username"));
  EXPECT_TRUE(resp["data"]["id"].is_number());
  EXPECT_TRUE(resp["data"]["username"].is_string());
}

// =============================================================================
// POST /api/register — 3 字符用户名（边界值）成功
// =============================================================================
TEST_F(AuthApiTest, RegisterMinUsernameBoundary) {
  json body = {{"username", "UTz"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  // 清理（无 UT_APIAUTH_ 前缀）
  oj::ConnectionGuard g;
  if (g.Valid()) ::mysql_query(g.Get(), "DELETE FROM users WHERE username='UTz'");
}

// =============================================================================
// POST /api/register — 6 字符密码（边界值）成功
// =============================================================================
TEST_F(AuthApiTest, RegisterMinPasswordBoundary) {
  json body = {{"username", "UT_APIAUTH_minpwd"}, {"password", "123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);
}

// =============================================================================
// POST /api/register — 64 字符用户名（边界值）成功
// =============================================================================
TEST_F(AuthApiTest, RegisterMaxUsernameBoundary) {
  json body = {{"username", std::string("UT_APIAUTH_") + std::string(53, 'x')},
               {"password", "pass123456"}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);
}

// =============================================================================
// POST /api/register — 64 字符密码（边界值）成功
// =============================================================================
TEST_F(AuthApiTest, RegisterMaxPasswordBoundary) {
  json body = {{"username", "UT_APIAUTH_maxpwd"}, {"password", std::string(64, 'p')}};
  auto res = cli_->Post("/api/register", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);
}
