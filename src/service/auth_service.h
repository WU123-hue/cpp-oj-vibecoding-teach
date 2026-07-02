#ifndef OJ_AUTH_SERVICE_H
#define OJ_AUTH_SERVICE_H

#include <string>

namespace oj {

// 注册请求
struct RegisterRequest {
  std::string username;
  std::string password;
};

// 登录请求
struct LoginRequest {
  std::string username;
  std::string password;
};

// 登录结果
struct LoginResult {
  std::string session_id;  // 会话 ID
  int         user_id;     // 用户 ID
  std::string username;    // 用户名
  std::string role;        // 角色: "user" / "admin"
};

// 认证业务服务
class AuthService {
 public:
  // 用户注册
  //   req    : 注册请求（username + password 明文）
  //   new_id : 成功时填充新用户 ID
  //   reason : 失败时填充错误信息
  // 校验规则：
  //   - username 非空，长度 3~64
  //   - password 非空，长度 6~64
  //   - username 唯一（数据库 UNIQUE 约束）
  // 密码使用 bcrypt 哈希后存储
  bool Register(const RegisterRequest& req, int* new_id, std::string* reason);

  // 用户登录
  //   req    : 登录请求（username + password 明文）
  //   result : 成功时填充登录结果（含 session_id）
  //   reason : 失败时填充错误信息
  // 流程：按 username 查库 → bcrypt 验证密码 → 创建会话
  bool Login(const LoginRequest& req, LoginResult* result, std::string* reason);

  // 验证密码是否匹配存储的 bcrypt 哈希
  //   password : 用户输入的明文密码
  //   stored   : 数据库中的 bcrypt 哈希
  static bool VerifyPassword(const std::string& password,
                             const std::string& stored);

  // 生成 bcrypt 哈希
  static std::string HashPassword(const std::string& password);

  // 检查用户名是否存在
  static bool UsernameExists(const std::string& username);
};

}  // namespace oj

#endif  // OJ_AUTH_SERVICE_H
