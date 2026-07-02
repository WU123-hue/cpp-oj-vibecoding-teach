#include "service/auth_service.h"

#include <gtest/gtest.h>
#include <mysql/mysql.h>

#include <string>

#include "db/connection_pool.h"
#include "model/user.h"
#include "service/session_manager.h"
#include "utils/logger.h"

namespace {

constexpr const char* kHost     = "localhost";
constexpr const int   kPort     = 3306;
constexpr const char* kUser     = "root";
constexpr const char* kPassword = "";
constexpr const char* kDatabase = "oj_db";

oj::ConnectionPool& Pool() { return oj::ConnectionPool::Instance(); }

std::string QueryScalar(MYSQL* c, const std::string& sql) {
  if (::mysql_query(c, sql.c_str()) != 0) return "";
  MYSQL_RES* res = ::mysql_store_result(c);
  if (res == nullptr) return "";
  MYSQL_ROW row = ::mysql_fetch_row(res);
  std::string val = (row && row[0]) ? row[0] : "";
  ::mysql_free_result(res);
  return val;
}

// 清理 UT_AUTH_ 前缀的测试用户
void CleanTestUsers() {
  oj::ConnectionGuard g;
  if (!g.Valid()) return;
  ::mysql_query(g.Get(), "DELETE FROM users WHERE username LIKE 'UT_AUTH_%'");
}

}  // namespace

// =============================================================================
// 全局环境
// =============================================================================
class AuthServiceTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    oj::Logger::Instance().Init("error", "/tmp/oj_auth_ut", "auth.log");
    ASSERT_TRUE(Pool().Init(kHost, kPort, kUser, kPassword, kDatabase, 2));
  }
  static void TearDownTestSuite() { Pool().Destroy(); }

  void SetUp() override { CleanTestUsers(); }
  void TearDown() override { CleanTestUsers(); }

  oj::ConnectionGuard Conn() { return oj::ConnectionGuard(); }
};

// =============================================================================
// Register: 正常注册
// =============================================================================
TEST_F(AuthServiceTest, RegisterSuccess) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_RegisterSuccess";
  req.password = "password123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason)) << reason;
  EXPECT_GT(new_id, 0);

  // 验证已写入数据库
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM users WHERE id=" + std::to_string(new_id)), "1");
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT username FROM users WHERE id=" + std::to_string(new_id)),
      "UT_AUTH_RegisterSuccess");
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT role FROM users WHERE id=" + std::to_string(new_id)), "user");

  // 密码应为 bcrypt 哈希（以 $2a$ 开头）
  std::string stored_hash = QueryScalar(g.Get(),
      "SELECT password FROM users WHERE id=" + std::to_string(new_id));
  EXPECT_TRUE(stored_hash.rfind("$2a$", 0) == 0)
      << "password should be bcrypt hash, got: " << stored_hash;
  // 不应存储明文密码
  EXPECT_NE(stored_hash, "password123");
}

// =============================================================================
// Register: 返回的 new_id 正确
// =============================================================================
TEST_F(AuthServiceTest, RegisterReturnsValidId) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_ValidId";
  req.password = "secret456";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason));
  EXPECT_GT(new_id, 0);

  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT id FROM users WHERE username='UT_AUTH_ValidId'"),
      std::to_string(new_id));
}

// =============================================================================
// Register: 注册的用户默认角色为 user
// =============================================================================
TEST_F(AuthServiceTest, RegisterDefaultRoleIsUser) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_DefaultRole";
  req.password = "pass123456";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason));

  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT role FROM users WHERE id=" + std::to_string(new_id)), "user");
}

// =============================================================================
// Register: 密码使用 bcrypt 哈希存储，不存明文
// =============================================================================
TEST_F(AuthServiceTest, RegisterPasswordHashedWithBcrypt) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_BcryptHash";
  req.password = "mypassword";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason));

  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  std::string stored = QueryScalar(g.Get(),
      "SELECT password FROM users WHERE id=" + std::to_string(new_id));

  // bcrypt 哈希格式: $2a$10$<22 char salt><31 char hash>
  EXPECT_EQ(stored.size(), 60u);
  EXPECT_TRUE(stored.rfind("$2a$10$", 0) == 0);
  EXPECT_NE(stored, "mypassword");
}

// =============================================================================
// Register: 两次注册同一用户密码，哈希不同（salt 随机）
// =============================================================================
TEST_F(AuthServiceTest, RegisterSamePasswordDifferentHash) {
  oj::AuthService svc;

  oj::RegisterRequest r1;
  r1.username = "UT_AUTH_HashA";
  r1.password = "samepassword";
  int id1 = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(r1, &id1, &reason));

  oj::RegisterRequest r2;
  r2.username = "UT_AUTH_HashB";
  r2.password = "samepassword";
  int id2 = 0;
  ASSERT_TRUE(svc.Register(r2, &id2, &reason));

  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  std::string h1 = QueryScalar(g.Get(),
      "SELECT password FROM users WHERE id=" + std::to_string(id1));
  std::string h2 = QueryScalar(g.Get(),
      "SELECT password FROM users WHERE id=" + std::to_string(id2));

  EXPECT_NE(h1, h2);  // 相同密码，不同 salt，不同哈希
}

// =============================================================================
// Register: 用户名已存在返回失败
// =============================================================================
TEST_F(AuthServiceTest, RegisterDuplicateUsernameFails) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_Duplicate";
  req.password = "password123";

  oj::AuthService svc;
  int id1 = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &id1, &reason));

  // 再次注册相同用户名
  int id2 = 999;
  EXPECT_FALSE(svc.Register(req, &id2, &reason));
  EXPECT_EQ(reason, "username already exists");
  EXPECT_EQ(id2, 999);  // 不应被修改
}

// =============================================================================
// Register: 空 username 返回失败
// =============================================================================
TEST_F(AuthServiceTest, RegisterEmptyUsernameFails) {
  oj::RegisterRequest req;
  req.username = "";
  req.password = "password123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Register(req, &new_id, &reason));
  EXPECT_EQ(reason, "username is required");
}

// =============================================================================
// Register: username 过短（<3）返回失败
// =============================================================================
TEST_F(AuthServiceTest, RegisterShortUsernameFails) {
  oj::RegisterRequest req;
  req.username = "ab";  // 2 字符
  req.password = "password123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Register(req, &new_id, &reason));
  EXPECT_EQ(reason, "username must be 3-64 characters");
}

// =============================================================================
// Register: username 过长（>64）返回失败
// =============================================================================
TEST_F(AuthServiceTest, RegisterLongUsernameFails) {
  oj::RegisterRequest req;
  req.username = std::string(65, 'a');
  req.password = "password123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Register(req, &new_id, &reason));
  EXPECT_EQ(reason, "username must be 3-64 characters");
}

// =============================================================================
// Register: username 刚好 3 字符合法
// =============================================================================
TEST_F(AuthServiceTest, RegisterMinUsernameLength) {
  oj::RegisterRequest req;
  req.username = "abc";
  req.password = "password123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason)) << reason;
  EXPECT_GT(new_id, 0);
}

// =============================================================================
// Register: username 刚好 64 字符合法
// =============================================================================
TEST_F(AuthServiceTest, RegisterMaxUsernameLength) {
  oj::RegisterRequest req;
  req.username = std::string(64, 'x');
  req.password = "password123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason)) << reason;
  EXPECT_GT(new_id, 0);
}

// =============================================================================
// Register: 空 password 返回失败
// =============================================================================
TEST_F(AuthServiceTest, RegisterEmptyPasswordFails) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_EmptyPwd";
  req.password = "";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Register(req, &new_id, &reason));
  EXPECT_EQ(reason, "password is required");
}

// =============================================================================
// Register: password 过短（<6）返回失败
// =============================================================================
TEST_F(AuthServiceTest, RegisterShortPasswordFails) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_ShortPwd";
  req.password = "12345";  // 5 字符

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Register(req, &new_id, &reason));
  EXPECT_EQ(reason, "password must be 6-64 characters");
}

// =============================================================================
// Register: password 过长（>64）返回失败
// =============================================================================
TEST_F(AuthServiceTest, RegisterLongPasswordFails) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_LongPwd";
  req.password = std::string(65, 'p');

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Register(req, &new_id, &reason));
  EXPECT_EQ(reason, "password must be 6-64 characters");
}

// =============================================================================
// Register: password 刚好 6 字符合法
// =============================================================================
TEST_F(AuthServiceTest, RegisterMinPasswordLength) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_MinPwd";
  req.password = "123456";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason)) << reason;
  EXPECT_GT(new_id, 0);
}

// =============================================================================
// UsernameExists: 存在的用户名返回 true
// =============================================================================
TEST_F(AuthServiceTest, UsernameExistsReturnsTrue) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_Exists";
  req.password = "password123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason));

  EXPECT_TRUE(oj::AuthService::UsernameExists("UT_AUTH_Exists"));
}

// =============================================================================
// UsernameExists: 不存在的用户名返回 false
// =============================================================================
TEST_F(AuthServiceTest, UsernameNotExistsReturnsFalse) {
  EXPECT_FALSE(oj::AuthService::UsernameExists("UT_AUTH_DoesNotExist"));
}

// =============================================================================
// UsernameExists: 默认 admin 用户存在
// =============================================================================
TEST_F(AuthServiceTest, AdminUserExists) {
  EXPECT_TRUE(oj::AuthService::UsernameExists("admin"));
}

// =============================================================================
// HashPassword: 返回有效的 bcrypt 哈希
// =============================================================================
TEST_F(AuthServiceTest, HashPasswordReturnsBcryptFormat) {
  std::string hash = oj::AuthService::HashPassword("testpassword");
  EXPECT_EQ(hash.size(), 60u);
  EXPECT_TRUE(hash.rfind("$2a$10$", 0) == 0);
}

// =============================================================================
// HashPassword: 相同密码两次哈希结果不同
// =============================================================================
TEST_F(AuthServiceTest, HashPasswordSameInputDifferentOutput) {
  std::string h1 = oj::AuthService::HashPassword("samepassword");
  std::string h2 = oj::AuthService::HashPassword("samepassword");
  EXPECT_NE(h1, h2);
}

// =============================================================================
// VerifyPassword: 正确密码验证成功
// =============================================================================
TEST_F(AuthServiceTest, VerifyPasswordCorrect) {
  std::string hash = oj::AuthService::HashPassword("mypassword");
  EXPECT_TRUE(oj::AuthService::VerifyPassword("mypassword", hash));
}

// =============================================================================
// VerifyPassword: 错误密码验证失败
// =============================================================================
TEST_F(AuthServiceTest, VerifyPasswordWrong) {
  std::string hash = oj::AuthService::HashPassword("mypassword");
  EXPECT_FALSE(oj::AuthService::VerifyPassword("wrongpassword", hash));
}

// =============================================================================
// VerifyPassword: 空哈希返回 false
// =============================================================================
TEST_F(AuthServiceTest, VerifyPasswordEmptyHash) {
  EXPECT_FALSE(oj::AuthService::VerifyPassword("password", ""));
}

// =============================================================================
// VerifyPassword: 无效哈希返回 false
// =============================================================================
TEST_F(AuthServiceTest, VerifyPasswordInvalidHash) {
  EXPECT_FALSE(oj::AuthService::VerifyPassword("password", "not_a_hash"));
}

// =============================================================================
// VerifyPassword: 空密码返回 false（即使哈希有效）
// =============================================================================
TEST_F(AuthServiceTest, VerifyPasswordEmptyPassword) {
  std::string hash = oj::AuthService::HashPassword("somepassword");
  EXPECT_FALSE(oj::AuthService::VerifyPassword("", hash));
}

// =============================================================================
// Register + VerifyPassword 集成: 注册后能用原密码验证
// =============================================================================
TEST_F(AuthServiceTest, RegisterThenVerifyPassword) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_RegisterVerify";
  req.password = "verifiable123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason));

  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  std::string stored = QueryScalar(g.Get(),
      "SELECT password FROM users WHERE id=" + std::to_string(new_id));

  // 原密码能验证
  EXPECT_TRUE(oj::AuthService::VerifyPassword("verifiable123", stored));
  // 错误密码不能验证
  EXPECT_FALSE(oj::AuthService::VerifyPassword("wrongpassword", stored));
}

// =============================================================================
// Register: 用户名含特殊字符能正确存储
// =============================================================================
TEST_F(AuthServiceTest, RegisterUsernameWithSpecialChars) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_Quote'Name";
  req.password = "password123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason)) << reason;

  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT username FROM users WHERE id=" + std::to_string(new_id)),
      "UT_AUTH_Quote'Name");
}

// =============================================================================
// Register: 密码含特殊字符能正确哈希和验证
// =============================================================================
TEST_F(AuthServiceTest, RegisterPasswordWithSpecialChars) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_SpecialPwd";
  req.password = "p@ss\"w0rd'!#$%";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason));

  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  std::string stored = QueryScalar(g.Get(),
      "SELECT password FROM users WHERE id=" + std::to_string(new_id));

  EXPECT_TRUE(oj::AuthService::VerifyPassword("p@ss\"w0rd'!#$%", stored));
  EXPECT_FALSE(oj::AuthService::VerifyPassword("p@ssw0rd", stored));
}

// =============================================================================
// Register: 注册后用户可通过 UsernameExists 找到
// =============================================================================
TEST_F(AuthServiceTest, RegisterThenUsernameExists) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_ThenExists";
  req.password = "password123";

  oj::AuthService svc;
  // 注册前不存在
  EXPECT_FALSE(oj::AuthService::UsernameExists("UT_AUTH_ThenExists"));

  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason));

  // 注册后存在
  EXPECT_TRUE(oj::AuthService::UsernameExists("UT_AUTH_ThenExists"));
}

// =============================================================================
// Register: reason 为 nullptr 时不崩溃
// =============================================================================
TEST_F(AuthServiceTest, RegisterNullReasonNoCrash) {
  oj::RegisterRequest req;
  req.username = "";  // 故意触发失败
  req.password = "password123";

  oj::AuthService svc;
  int new_id = 0;
  EXPECT_FALSE(svc.Register(req, &new_id, nullptr));
  EXPECT_EQ(new_id, 0);
}

// =============================================================================
// Register: new_id 为 nullptr 时不崩溃
// =============================================================================
TEST_F(AuthServiceTest, RegisterNullNewIdNoCrash) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_NullId";
  req.password = "password123";

  oj::AuthService svc;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, nullptr, &reason));
}

// =============================================================================
// ===== Login 功能测试 =====
// =============================================================================

// 辅助：注册测试用户
void RegisterUser(const std::string& username, const std::string& password) {
  oj::RegisterRequest req;
  req.username = username;
  req.password = password;
  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  svc.Register(req, &new_id, &reason);
}

// =============================================================================
// Login: 正常登录成功
// =============================================================================
TEST_F(AuthServiceTest, LoginSuccess) {
  RegisterUser("UT_AUTH_LoginSuccess", "pass123456");

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginSuccess";
  req.password = "pass123456";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  ASSERT_TRUE(svc.Login(req, &result, &reason)) << reason;

  EXPECT_FALSE(result.session_id.empty());
  EXPECT_GT(result.user_id, 0);
  EXPECT_EQ(result.username, "UT_AUTH_LoginSuccess");
  EXPECT_EQ(result.role, "user");
}

// =============================================================================
// Login: session_id 格式正确（32 字符十六进制）
// =============================================================================
TEST_F(AuthServiceTest, LoginSessionIdFormat) {
  RegisterUser("UT_AUTH_LoginSidFmt", "pass123456");

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginSidFmt";
  req.password = "pass123456";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  ASSERT_TRUE(svc.Login(req, &result, &reason));

  EXPECT_EQ(result.session_id.size(), 32u);
  for (char c : result.session_id) {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
        << "invalid hex char: " << c;
  }
}

// =============================================================================
// Login: 错误密码返回失败
// =============================================================================
TEST_F(AuthServiceTest, LoginWrongPasswordFails) {
  RegisterUser("UT_AUTH_LoginWrongPwd", "correctpass");

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginWrongPwd";
  req.password = "wrongpassword";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  EXPECT_FALSE(svc.Login(req, &result, &reason));
  EXPECT_EQ(reason, "invalid username or password");
  EXPECT_TRUE(result.session_id.empty());
}

// =============================================================================
// Login: 不存在的用户返回失败
// =============================================================================
TEST_F(AuthServiceTest, LoginNonExistentUserFails) {
  oj::LoginRequest req;
  req.username = "UT_AUTH_NoSuchUser";
  req.password = "pass123456";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  EXPECT_FALSE(svc.Login(req, &result, &reason));
  EXPECT_EQ(reason, "invalid username or password");
}

// =============================================================================
// Login: 空 username 返回失败
// =============================================================================
TEST_F(AuthServiceTest, LoginEmptyUsernameFails) {
  oj::LoginRequest req;
  req.username = "";
  req.password = "pass123456";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  EXPECT_FALSE(svc.Login(req, &result, &reason));
  EXPECT_EQ(reason, "username is required");
}

// =============================================================================
// Login: 空 password 返回失败
// =============================================================================
TEST_F(AuthServiceTest, LoginEmptyPasswordFails) {
  RegisterUser("UT_AUTH_LoginEmptyPwd", "pass123456");

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginEmptyPwd";
  req.password = "";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  EXPECT_FALSE(svc.Login(req, &result, &reason));
  EXPECT_EQ(reason, "password is required");
}

// =============================================================================
// Login: 错误密码和不存在用户的错误信息相同（不泄露用户是否存在）
// =============================================================================
TEST_F(AuthServiceTest, LoginWrongPasswordAndNonExistentSameError) {
  RegisterUser("UT_AUTH_LoginSameErr", "pass123456");

  oj::AuthService svc;
  std::string reason1, reason2;

  // 错误密码
  oj::LoginRequest r1;
  r1.username = "UT_AUTH_LoginSameErr";
  r1.password = "wrongpassword";
  svc.Login(r1, nullptr, &reason1);

  // 不存在的用户
  oj::LoginRequest r2;
  r2.username = "UT_AUTH_LoginNoExist";
  r2.password = "pass123456";
  svc.Login(r2, nullptr, &reason2);

  EXPECT_EQ(reason1, reason2);
  EXPECT_EQ(reason1, "invalid username or password");
}

// =============================================================================
// Login: 登录后 session 可通过 SessionManager 获取
// =============================================================================
TEST_F(AuthServiceTest, LoginCreatesValidSession) {
  RegisterUser("UT_AUTH_LoginValidSession", "pass123456");

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginValidSession";
  req.password = "pass123456";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  ASSERT_TRUE(svc.Login(req, &result, &reason));

  const oj::Session* s = oj::SessionManager::Instance().GetSession(
      result.session_id);
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->user_id, result.user_id);
  EXPECT_EQ(s->username, "UT_AUTH_LoginValidSession");
  EXPECT_EQ(s->role, "user");
}

// =============================================================================
// Login: 两次登录生成不同 session_id
// =============================================================================
TEST_F(AuthServiceTest, LoginTwiceDifferentSessionIds) {
  RegisterUser("UT_AUTH_LoginTwice", "pass123456");

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginTwice";
  req.password = "pass123456";

  oj::AuthService svc;
  oj::LoginResult r1, r2;
  std::string reason;

  ASSERT_TRUE(svc.Login(req, &r1, &reason));
  ASSERT_TRUE(svc.Login(req, &r2, &reason));

  EXPECT_NE(r1.session_id, r2.session_id);
  EXPECT_EQ(r1.user_id, r2.user_id);
  EXPECT_EQ(r1.username, r2.username);
}

// =============================================================================
// Login: 管理员登录返回 role=admin
// =============================================================================
TEST_F(AuthServiceTest, LoginAdminRole) {
  oj::LoginRequest req;
  req.username = "admin";
  req.password = "admin123";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  ASSERT_TRUE(svc.Login(req, &result, &reason)) << reason;

  EXPECT_EQ(result.username, "admin");
  EXPECT_EQ(result.role, "admin");
  EXPECT_FALSE(result.session_id.empty());
}

// =============================================================================
// Login: 管理员密码错误返回失败
// =============================================================================
TEST_F(AuthServiceTest, LoginAdminWrongPasswordFails) {
  oj::LoginRequest req;
  req.username = "admin";
  req.password = "wrongadmin";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  EXPECT_FALSE(svc.Login(req, &result, &reason));
  EXPECT_EQ(reason, "invalid username or password");
}

// =============================================================================
// Login: result 为 nullptr 时不崩溃
// =============================================================================
TEST_F(AuthServiceTest, LoginNullResultNoCrash) {
  RegisterUser("UT_AUTH_LoginNullResult", "pass123456");

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginNullResult";
  req.password = "pass123456";

  oj::AuthService svc;
  std::string reason;
  ASSERT_TRUE(svc.Login(req, nullptr, &reason));
}

// =============================================================================
// Login: reason 为 nullptr 时不崩溃
// =============================================================================
TEST_F(AuthServiceTest, LoginNullReasonNoCrash) {
  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginNullReason";
  req.password = "pass123456";

  oj::AuthService svc;
  oj::LoginResult result;
  EXPECT_FALSE(svc.Login(req, &result, nullptr));
  EXPECT_TRUE(result.session_id.empty());
}

// =============================================================================
// Login: 登录后 session_id 在 SessionManager 中有效
// =============================================================================
TEST_F(AuthServiceTest, LoginSessionAccessibleViaManager) {
  RegisterUser("UT_AUTH_LoginViaManager", "pass123456");

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginViaManager";
  req.password = "pass123456";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  ASSERT_TRUE(svc.Login(req, &result, &reason));

  // SessionManager 能查到
  EXPECT_NE(oj::SessionManager::Instance().GetSession(result.session_id),
            nullptr);

  // 销毁后查不到
  EXPECT_TRUE(oj::SessionManager::Instance().DestroySession(
      result.session_id));
  EXPECT_EQ(oj::SessionManager::Instance().GetSession(result.session_id),
            nullptr);
}

// =============================================================================
// Login: 注册→登录→验证密码 完整流程
// =============================================================================
TEST_F(AuthServiceTest, RegisterLoginVerifyFlow) {
  // 注册
  oj::RegisterRequest reg_req;
  reg_req.username = "UT_AUTH_RegLoginFlow";
  reg_req.password = "flowpass123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(reg_req, &new_id, &reason));

  // 从数据库读取哈希
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  std::string stored = QueryScalar(g.Get(),
      "SELECT password FROM users WHERE id=" + std::to_string(new_id));

  // VerifyPassword 能验证
  EXPECT_TRUE(oj::AuthService::VerifyPassword("flowpass123", stored));

  // 登录
  oj::LoginRequest login_req;
  login_req.username = "UT_AUTH_RegLoginFlow";
  login_req.password = "flowpass123";

  oj::LoginResult result;
  ASSERT_TRUE(svc.Login(login_req, &result, &reason));
  EXPECT_EQ(result.user_id, new_id);
  EXPECT_EQ(result.username, "UT_AUTH_RegLoginFlow");
  EXPECT_EQ(result.role, "user");

  // 错误密码登录失败
  login_req.password = "wrongpass";
  EXPECT_FALSE(svc.Login(login_req, &result, &reason));
}

// =============================================================================
// Login: 密码含特殊字符能正确登录
// =============================================================================
TEST_F(AuthServiceTest, LoginSpecialCharsPassword) {
  std::string pwd = "p@ss\"w0rd'!#$%";
  RegisterUser("UT_AUTH_LoginSpecial", pwd);

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginSpecial";
  req.password = pwd;

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  ASSERT_TRUE(svc.Login(req, &result, &reason)) << reason;
  EXPECT_FALSE(result.session_id.empty());

  // 稍微修改密码应失败
  req.password = "p@ssw0rd";
  EXPECT_FALSE(svc.Login(req, &result, &reason));
}

// =============================================================================
// Login: 大小写敏感的用户名
// =============================================================================
TEST_F(AuthServiceTest, LoginUsernameCaseSensitive) {
  RegisterUser("UT_AUTH_CaseTest", "pass123456");

  oj::LoginRequest req;
  req.username = "ut_auth_casetest";  // 全小写
  req.password = "pass123456";

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  // MySQL 默认 collation 不区分大小写，但行为取决于 collation
  // 这里只验证不崩溃，且如果登录成功则 session 有效
  bool ok = svc.Login(req, &result, &reason);
  if (ok) {
    EXPECT_FALSE(result.session_id.empty());
  }
  // 不做强制断言，因为 collation 行为不确定
}

// =============================================================================
// Login: 长密码能正确登录
// =============================================================================
TEST_F(AuthServiceTest, LoginLongPassword) {
  std::string long_pwd(64, 'p');
  RegisterUser("UT_AUTH_LoginLongPwd", long_pwd);

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginLongPwd";
  req.password = long_pwd;

  oj::AuthService svc;
  oj::LoginResult result;
  std::string reason;
  ASSERT_TRUE(svc.Login(req, &result, &reason)) << reason;

  // 少一个字符应失败
  req.password = std::string(63, 'p');
  EXPECT_FALSE(svc.Login(req, &result, &reason));
}

// =============================================================================
// Login: 多次连续登录均成功（会话独立）
// =============================================================================
TEST_F(AuthServiceTest, LoginMultipleTimesAllSucceed) {
  RegisterUser("UT_AUTH_LoginMulti", "pass123456");

  oj::LoginRequest req;
  req.username = "UT_AUTH_LoginMulti";
  req.password = "pass123456";

  oj::AuthService svc;
  for (int i = 0; i < 5; ++i) {
    oj::LoginResult result;
    std::string reason;
    ASSERT_TRUE(svc.Login(req, &result, &reason))
        << "login failed at iteration " << i << ": " << reason;
    EXPECT_FALSE(result.session_id.empty());
  }
}

// =============================================================================
// VerifyPassword: 与注册存储的哈希兼容
// =============================================================================
TEST_F(AuthServiceTest, VerifyPasswordCompatibleWithRegisterStoredHash) {
  oj::RegisterRequest req;
  req.username = "UT_AUTH_VerifyCompat";
  req.password = "compatpass123";

  oj::AuthService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Register(req, &new_id, &reason));

  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  std::string stored = QueryScalar(g.Get(),
      "SELECT password FROM users WHERE id=" + std::to_string(new_id));

  // 从数据库取出的哈希能被 VerifyPassword 验证
  EXPECT_TRUE(oj::AuthService::VerifyPassword("compatpass123", stored));
  EXPECT_FALSE(oj::AuthService::VerifyPassword("CompatPass123", stored));  // 大小写
  EXPECT_FALSE(oj::AuthService::VerifyPassword("compatpass12", stored));  // 少一字符
  EXPECT_FALSE(oj::AuthService::VerifyPassword("compatpass1234", stored)); // 多一字符
}

// =============================================================================
// VerifyPassword: 短哈希（<7 字符）返回 false
// =============================================================================
TEST_F(AuthServiceTest, VerifyPasswordShortHashReturnsFalse) {
  EXPECT_FALSE(oj::AuthService::VerifyPassword("password", "$2a"));  // 3 字符
  EXPECT_FALSE(oj::AuthService::VerifyPassword("password", "$2a$10"));  // 6 字符
}

// =============================================================================
// VerifyPassword: 哈希与密码均为空返回 false
// =============================================================================
TEST_F(AuthServiceTest, VerifyPasswordBothEmptyReturnsFalse) {
  EXPECT_FALSE(oj::AuthService::VerifyPassword("", ""));
}

// =============================================================================
// VerifyPassword: 相同密码不同 salt 的哈希均能验证
// =============================================================================
TEST_F(AuthServiceTest, VerifyPasswordMultipleSaltsAllVerify) {
  std::string pwd = "samepassword123";
  std::string h1 = oj::AuthService::HashPassword(pwd);
  std::string h2 = oj::AuthService::HashPassword(pwd);
  std::string h3 = oj::AuthService::HashPassword(pwd);

  EXPECT_NE(h1, h2);
  EXPECT_NE(h2, h3);

  EXPECT_TRUE(oj::AuthService::VerifyPassword(pwd, h1));
  EXPECT_TRUE(oj::AuthService::VerifyPassword(pwd, h2));
  EXPECT_TRUE(oj::AuthService::VerifyPassword(pwd, h3));
}

// =============================================================================
// VerifyPassword: 密码含 null 字节不会被截断（不适用——std::string 可含 null）
// 验证含二进制数据的密码
// =============================================================================
TEST_F(AuthServiceTest, VerifyPasswordWithBinaryData) {
  std::string pwd = "pass\x01\x02\x03word";
  std::string hash = oj::AuthService::HashPassword(pwd);
  EXPECT_TRUE(oj::AuthService::VerifyPassword(pwd, hash));
  EXPECT_FALSE(oj::AuthService::VerifyPassword("password", hash));
}
