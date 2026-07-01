#include "auth_service.h"

#include <crypt.h>

#include <mysql/mysql.h>

#include <cstring>
#include <cstdlib>
#include <string>

#include "db/connection_pool.h"
#include "model/user.h"
#include "utils/logger.h"

namespace oj {

namespace {

// 生成 22 字符的 bcrypt salt 前缀（$2a$10$ + 22 字符随机串）
// 使用 /dev/urandom 生成，回退到 rand()
std::string GenerateBcryptSalt() {
  const char charset[] =
      "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const int charset_size = sizeof(charset) - 1;

  unsigned char rand_bytes[16];
  FILE* fp = ::fopen("/dev/urandom", "rb");
  if (fp) {
    if (::fread(rand_bytes, 1, sizeof(rand_bytes), fp) != sizeof(rand_bytes)) {
      for (size_t i = 0; i < sizeof(rand_bytes); ++i) {
        rand_bytes[i] = static_cast<unsigned char>(::rand());
      }
    }
    ::fclose(fp);
  } else {
    for (size_t i = 0; i < sizeof(rand_bytes); ++i) {
      rand_bytes[i] = static_cast<unsigned char>(::rand());
    }
  }

  std::string salt = "$2a$10$";
  for (size_t i = 0; i < 22; ++i) {
    salt += charset[rand_bytes[i] % charset_size];
  }
  return salt;
}

// SQL 转义
std::string Esc(MYSQL* conn, const std::string& s) {
  std::string buf(s.size() * 2 + 1, '\0');
  unsigned long len = ::mysql_real_escape_string(conn, &buf[0], s.c_str(),
                                                 static_cast<unsigned long>(s.size()));
  buf.resize(len);
  return "'" + buf + "'";
}

}  // namespace

// =============================================================================
// 生成 bcrypt 哈希
// =============================================================================
std::string AuthService::HashPassword(const std::string& password) {
  std::string salt = GenerateBcryptSalt();

  struct crypt_data data;
  ::memset(&data, 0, sizeof(data));

  char* hash = ::crypt_r(password.c_str(), salt.c_str(), &data);
  if (hash == nullptr) return "";

  return std::string(hash);
}

// =============================================================================
// 验证密码
// =============================================================================
bool AuthService::VerifyPassword(const std::string& password,
                                 const std::string& stored) {
  if (stored.size() < 7) return false;

  struct crypt_data data;
  ::memset(&data, 0, sizeof(data));

  char* hash = ::crypt_r(password.c_str(), stored.c_str(), &data);
  if (hash == nullptr) return false;

  return ::strcmp(hash, stored.c_str()) == 0;
}

// =============================================================================
// 检查用户名是否存在
// =============================================================================
bool AuthService::UsernameExists(const std::string& username) {
  ConnectionGuard g;
  if (!g.Valid()) return false;

  std::string sql = "SELECT COUNT(*) FROM users WHERE username=" +
                    Esc(g.Get(), username);
  if (::mysql_query(g.Get(), sql.c_str()) != 0) return false;
  MYSQL_RES* res = ::mysql_store_result(g.Get());
  if (res == nullptr) return false;

  MYSQL_ROW row = ::mysql_fetch_row(res);
  bool exists = false;
  if (row && row[0]) {
    exists = std::atoi(row[0]) > 0;
  }
  ::mysql_free_result(res);
  return exists;
}

// =============================================================================
// 用户注册
// =============================================================================
bool AuthService::Register(const RegisterRequest& req, int* new_id,
                           std::string* reason) {
  // 参数校验
  if (req.username.empty()) {
    if (reason) *reason = "username is required";
    return false;
  }
  if (req.username.size() < 3 || req.username.size() > 64) {
    if (reason) *reason = "username must be 3-64 characters";
    return false;
  }
  if (req.password.empty()) {
    if (reason) *reason = "password is required";
    return false;
  }
  if (req.password.size() < 6 || req.password.size() > 64) {
    if (reason) *reason = "password must be 6-64 characters";
    return false;
  }

  // 用户名唯一性校验
  if (UsernameExists(req.username)) {
    if (reason) *reason = "username already exists";
    return false;
  }

  // 生成 bcrypt 哈希
  std::string hash = HashPassword(req.password);
  if (hash.empty()) {
    if (reason) *reason = "password hashing failed";
    return false;
  }

  // 写入数据库
  ConnectionGuard g;
  if (!g.Valid()) {
    if (reason) *reason = "database connection unavailable";
    return false;
  }

  User user;
  user.set_username(req.username);
  user.set_password(hash);
  user.set_role(Role::User);

  if (!user.SaveToDB(g.Get())) {
    if (reason) *reason = "registration failed";
    return false;
  }

  if (new_id) *new_id = user.id();
  LOG_INFO_FMT("user registered: id=%d username=%s", user.id(),
               req.username.c_str());
  return true;
}

}  // namespace oj
