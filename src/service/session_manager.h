#ifndef OJ_SESSION_MANAGER_H
#define OJ_SESSION_MANAGER_H

#include <chrono>
#include <map>
#include <mutex>
#include <string>

namespace oj {

// 会话信息
struct Session {
  std::string  session_id;    // 唯一会话 ID
  int          user_id;       // 关联用户 ID
  std::string  username;      // 用户名（冗余，避免每次查库）
  std::string  role;          // 角色: "user" / "admin"
  std::chrono::steady_clock::time_point created_at;   // 创建时间
  std::chrono::steady_clock::time_point last_access;  // 最后访问时间
};

// 会话管理器（线程安全单例）
// 管理 sid → Session 映射，支持创建、验证、销毁、过期清理
class SessionManager {
 public:
  static SessionManager& Instance();

  // 创建新会话，返回 session_id
  //   user_id   : 用户 ID
  //   username  : 用户名
  //   role      : 角色
  std::string CreateSession(int user_id, const std::string& username,
                            const std::string& role);

  // 根据 session_id 获取会话，不存在或已过期返回 nullptr
  // 验证成功时更新 last_access
  const Session* GetSession(const std::string& session_id);

  // 销毁指定会话，返回是否成功销毁
  bool DestroySession(const std::string& session_id);

  // 清理所有过期会话
  size_t CleanExpired();

  // 当前活跃会话数（未过期）
  size_t ActiveCount();

  // 设置会话超时时间（秒），默认 1800（30 分钟）
  void SetTimeout(int seconds);

  // 获取 Cookie 名称
  static const char* CookieName() { return "oj_session"; }

 private:
  SessionManager();
  ~SessionManager() = default;
  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;

  // 生成随机 session_id（32 字符十六进制）
  std::string GenerateSessionId() const;

  // 判断会话是否已过期
  bool IsExpired(const Session& s) const;

  mutable std::mutex              mtx_;
  std::map<std::string, Session>  sessions_;
  int                             timeout_sec_ = 1800;  // 30 分钟
};

}  // namespace oj

#endif  // OJ_SESSION_MANAGER_H
