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

  // 辅助：注册一个测试用户
  static void RegisterTestUser(const std::string& username,
                               const std::string& password) {
    json body = {{"username", username}, {"password", password}};
    cli_->Post("/api/register", body.dump(), "application/json");
  }
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

// =============================================================================
// ===== Login API 测试 =====
// =============================================================================

// =============================================================================
// POST /api/login — 正常登录成功
// =============================================================================
TEST_F(AuthApiTest, LoginSuccess) {
  RegisterTestUser("UT_APIAUTH_loginok", "pass123456");

  json body = {{"username", "UT_APIAUTH_loginok"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 200);
  EXPECT_EQ(resp["message"], "login successful");
  EXPECT_EQ(resp["data"]["username"], "UT_APIAUTH_loginok");
  EXPECT_EQ(resp["data"]["role"], "user");
  EXPECT_GT(resp["data"]["id"].get<int>(), 0);
}

// =============================================================================
// POST /api/login — 登录成功返回 Set-Cookie 头
// =============================================================================
TEST_F(AuthApiTest, LoginSetsCookie) {
  RegisterTestUser("UT_APIAUTH_cookie", "pass123456");

  json body = {{"username", "UT_APIAUTH_cookie"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  auto cookie = res->get_header_value("Set-Cookie");
  EXPECT_NE(cookie.find("oj_session="), std::string::npos);
  EXPECT_NE(cookie.find("Path=/"), std::string::npos);
  EXPECT_NE(cookie.find("HttpOnly"), std::string::npos);
}

// =============================================================================
// POST /api/login — Cookie 中的 session_id 非空
// =============================================================================
TEST_F(AuthApiTest, LoginCookieHasSessionId) {
  RegisterTestUser("UT_APIAUTH_sid", "pass123456");

  json body = {{"username", "UT_APIAUTH_sid"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  ASSERT_EQ(res->status, 200);

  auto cookie = res->get_header_value("Set-Cookie");
  // oj_session= 后面应紧跟非空的 session_id
  size_t pos = cookie.find("oj_session=");
  ASSERT_NE(pos, std::string::npos);
  pos += std::string("oj_session=").size();
  EXPECT_LT(pos, cookie.size());
  // session_id 不应为空（下一个字符不是 ; 或空）
  EXPECT_NE(cookie[pos], ';');
  EXPECT_NE(cookie[pos], '\0');
}

// =============================================================================
// POST /api/login — 错误密码返回 401
// =============================================================================
TEST_F(AuthApiTest, LoginWrongPassword) {
  RegisterTestUser("UT_APIAUTH_wrongpwd", "pass123456");

  json body = {{"username", "UT_APIAUTH_wrongpwd"}, {"password", "wrongpassword"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 401);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 401);
  EXPECT_EQ(resp["message"], "invalid username or password");
}

// =============================================================================
// POST /api/login — 不存在的用户返回 401
// =============================================================================
TEST_F(AuthApiTest, LoginNonExistentUser) {
  json body = {{"username", "UT_APIAUTH_nosuchuser"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 401);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 401);
  EXPECT_EQ(resp["message"], "invalid username or password");
}

// =============================================================================
// POST /api/login — 空 username 返回 400
// =============================================================================
TEST_F(AuthApiTest, LoginEmptyUsername) {
  json body = {{"username", ""}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "username is required");
}

// =============================================================================
// POST /api/login — 缺少 username 返回 400
// =============================================================================
TEST_F(AuthApiTest, LoginMissingUsername) {
  json body = {{"password", "pass123456"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "username is required");
}

// =============================================================================
// POST /api/login — 空 password 返回 400
// =============================================================================
TEST_F(AuthApiTest, LoginEmptyPassword) {
  json body = {{"username", "UT_APIAUTH_emptypwd2"}, {"password", ""}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "password is required");
}

// =============================================================================
// POST /api/login — 缺少 password 返回 400
// =============================================================================
TEST_F(AuthApiTest, LoginMissingPassword) {
  json body = {{"username", "UT_APIAUTH_nopwd2"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "password is required");
}

// =============================================================================
// POST /api/login — 非法 JSON 返回 400
// =============================================================================
TEST_F(AuthApiTest, LoginInvalidJson) {
  auto res = cli_->Post("/api/login", "{bad json", "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_NE(json::parse(res->body)["message"].get<std::string>().find("invalid JSON"),
            std::string::npos);
}

// =============================================================================
// POST /api/login — 空 body 返回 400
// =============================================================================
TEST_F(AuthApiTest, LoginEmptyBody) {
  auto res = cli_->Post("/api/login", "", "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
}

// =============================================================================
// POST /api/login — 两次登录生成不同 session_id
// =============================================================================
TEST_F(AuthApiTest, LoginTwiceDifferentSessions) {
  RegisterTestUser("UT_APIAUTH_twice", "pass123456");

  json body = {{"username", "UT_APIAUTH_twice"}, {"password", "pass123456"}};

  auto res1 = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res1);
  ASSERT_EQ(res1->status, 200);
  std::string cookie1 = res1->get_header_value("Set-Cookie");

  auto res2 = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res2);
  ASSERT_EQ(res2->status, 200);
  std::string cookie2 = res2->get_header_value("Set-Cookie");

  // 两次登录的 session_id 应不同
  EXPECT_NE(cookie1, cookie2);
}

// =============================================================================
// POST /api/login — 管理员登录返回 role=admin
// =============================================================================
TEST_F(AuthApiTest, LoginAdminRole) {
  json body = {{"username", "admin"}, {"password", "admin123"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["data"]["role"], "admin");
  EXPECT_EQ(resp["data"]["username"], "admin");
}

// =============================================================================
// POST /api/login — 响应 Content-Type 为 application/json
// =============================================================================
TEST_F(AuthApiTest, LoginResponseContentType) {
  RegisterTestUser("UT_APIAUTH_loginct", "pass123456");

  json body = {{"username", "UT_APIAUTH_loginct"}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  ASSERT_EQ(res->status, 200);

  auto ct = res->get_header_value("Content-Type");
  EXPECT_NE(ct.find("application/json"), std::string::npos);
}

// =============================================================================
// POST /api/login — username 非字符串返回 400
// =============================================================================
TEST_F(AuthApiTest, LoginUsernameNotString) {
  json body = {{"username", 123}, {"password", "pass123456"}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "username is required");
}

// =============================================================================
// POST /api/login — password 非字符串返回 400
// =============================================================================
TEST_F(AuthApiTest, LoginPasswordNotString) {
  json body = {{"username", "UT_APIAUTH_pwdnum2"}, {"password", 123456}};
  auto res = cli_->Post("/api/login", body.dump(), "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 400);
  EXPECT_EQ(json::parse(res->body)["message"], "password is required");
}

// =============================================================================
// ===== Logout API 测试 =====
// =============================================================================

// =============================================================================
// POST /api/logout — 已登录用户登出成功
// =============================================================================
TEST_F(AuthApiTest, LogoutSuccess) {
  RegisterTestUser("UT_APIAUTH_logout", "pass123456");

  // 先登录获取 Cookie
  json login_body = {{"username", "UT_APIAUTH_logout"}, {"password", "pass123456"}};
  auto login_res = cli_->Post("/api/login", login_body.dump(), "application/json");
  ASSERT_TRUE(login_res);
  ASSERT_EQ(login_res->status, 200);
  std::string cookie = login_res->get_header_value("Set-Cookie");

  // 提取 session_id 部分
  size_t eq = cookie.find('=');
  size_t semi = cookie.find(';');
  std::string sid = cookie.substr(eq + 1, semi - eq - 1);

  // 带 Cookie 登出
  httplib::Headers headers = {{"Cookie", "oj_session=" + sid}};
  auto res = cli_->Post("/api/logout", headers, "", "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);

  json resp = json::parse(res->body);
  EXPECT_EQ(resp["code"], 200);
  EXPECT_EQ(resp["message"], "logged out");
}

// =============================================================================
// POST /api/logout — 登出后 Set-Cookie 清除 Cookie（Max-Age=0）
// =============================================================================
TEST_F(AuthApiTest, LogoutClearsCookie) {
  RegisterTestUser("UT_APIAUTH_clearcookie", "pass123456");

  json login_body = {{"username", "UT_APIAUTH_clearcookie"}, {"password", "pass123456"}};
  auto login_res = cli_->Post("/api/login", login_body.dump(), "application/json");
  ASSERT_TRUE(login_res);
  ASSERT_EQ(login_res->status, 200);
  std::string cookie = login_res->get_header_value("Set-Cookie");

  size_t eq = cookie.find('=');
  size_t semi = cookie.find(';');
  std::string sid = cookie.substr(eq + 1, semi - eq - 1);

  httplib::Headers headers = {{"Cookie", "oj_session=" + sid}};
  auto res = cli_->Post("/api/logout", headers, "", "application/json");
  ASSERT_TRUE(res);
  ASSERT_EQ(res->status, 200);

  auto resp_cookie = res->get_header_value("Set-Cookie");
  EXPECT_NE(resp_cookie.find("oj_session="), std::string::npos);
  EXPECT_NE(resp_cookie.find("Max-Age=0"), std::string::npos);
}

// =============================================================================
// POST /api/logout — 无 Cookie 登出仍返回 200
// =============================================================================
TEST_F(AuthApiTest, LogoutNoCookie) {
  auto res = cli_->Post("/api/logout", "", "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);
  EXPECT_EQ(json::parse(res->body)["message"], "logged out");
}

// =============================================================================
// POST /api/logout — 无效 session_id 登出仍返回 200
// =============================================================================
TEST_F(AuthApiTest, LogoutInvalidSession) {
  httplib::Headers headers = {{"Cookie", "oj_session=invalid_sid_12345"}};
  auto res = cli_->Post("/api/logout", headers, "", "application/json");
  ASSERT_TRUE(res);
  EXPECT_EQ(res->status, 200);
  EXPECT_EQ(json::parse(res->body)["message"], "logged out");
}

// =============================================================================
// ===== 登录 → 登出 完整流程 =====
// =============================================================================

// =============================================================================
// 注册 → 登录 → 登出 完整流程
// =============================================================================
TEST_F(AuthApiTest, FullRegisterLoginLogoutFlow) {
  // 1. 注册
  json reg_body = {{"username", "UT_APIAUTH_flow"}, {"password", "flowpass123"}};
  auto reg_res = cli_->Post("/api/register", reg_body.dump(), "application/json");
  ASSERT_TRUE(reg_res);
  ASSERT_EQ(reg_res->status, 200);

  // 2. 登录
  json login_body = {{"username", "UT_APIAUTH_flow"}, {"password", "flowpass123"}};
  auto login_res = cli_->Post("/api/login", login_body.dump(), "application/json");
  ASSERT_TRUE(login_res);
  ASSERT_EQ(login_res->status, 200);
  EXPECT_EQ(json::parse(login_res->body)["data"]["username"], "UT_APIAUTH_flow");

  std::string cookie = login_res->get_header_value("Set-Cookie");
  EXPECT_FALSE(cookie.empty());

  // 提取 session_id
  size_t eq = cookie.find('=');
  size_t semi = cookie.find(';');
  std::string sid = cookie.substr(eq + 1, semi - eq - 1);
  EXPECT_FALSE(sid.empty());

  // 3. 登出
  httplib::Headers headers = {{"Cookie", "oj_session=" + sid}};
  auto logout_res = cli_->Post("/api/logout", headers, "", "application/json");
  ASSERT_TRUE(logout_res);
  ASSERT_EQ(logout_res->status, 200);
  EXPECT_EQ(json::parse(logout_res->body)["message"], "logged out");

  // 4. 登出后再次登录仍成功（会话已被销毁但用户记录仍在）
  auto login2_res = cli_->Post("/api/login", login_body.dump(), "application/json");
  ASSERT_TRUE(login2_res);
  ASSERT_EQ(login2_res->status, 200);
}
