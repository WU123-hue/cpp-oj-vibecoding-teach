#include "session_manager.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

#include "utils/logger.h"

namespace oj {

SessionManager& SessionManager::Instance() {
  static SessionManager instance;
  return instance;
}

SessionManager::SessionManager() {
  // 初始化随机种子
  ::srand(static_cast<unsigned>(::time(nullptr)));
}

// =============================================================================
// 生成 32 字符十六进制 session_id
// 使用 /dev/urandom 生成密码学安全的随机字节，回退到 rand()
// =============================================================================
std::string SessionManager::GenerateSessionId() const {
  unsigned char buf[16];
  FILE* fp = ::fopen("/dev/urandom", "rb");
  if (fp) {
    size_t n = ::fread(buf, 1, sizeof(buf), fp);
    ::fclose(fp);
    if (n != sizeof(buf)) {
      // 回退到 rand
      for (int i = 0; i < 16; ++i) {
        buf[i] = static_cast<unsigned char>(::rand() & 0xFF);
      }
    }
  } else {
    for (int i = 0; i < 16; ++i) {
      buf[i] = static_cast<unsigned char>(::rand() & 0xFF);
    }
  }

  char hex[33];
  for (int i = 0; i < 16; ++i) {
    ::snprintf(hex + i * 2, 3, "%02x", buf[i]);
  }
  hex[32] = '\0';
  return std::string(hex, 32);
}

// =============================================================================
// 判断会话是否过期
// =============================================================================
bool SessionManager::IsExpired(const Session& s) const {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      now - s.last_access);
  return elapsed.count() >= timeout_sec_;
}

// =============================================================================
// 创建新会话
// =============================================================================
std::string SessionManager::CreateSession(int user_id,
                                          const std::string& username,
                                          const std::string& role) {
  Session s;
  s.session_id = GenerateSessionId();
  s.user_id    = user_id;
  s.username   = username;
  s.role       = role;
  s.created_at = std::chrono::steady_clock::now();
  s.last_access = s.created_at;

  std::lock_guard<std::mutex> lock(mtx_);
  sessions_[s.session_id] = s;

  LOG_INFO_FMT("session created: sid=%s user=%s role=%s",
               s.session_id.c_str(), username.c_str(), role.c_str());
  return s.session_id;
}

// =============================================================================
// 获取会话（不存在或已过期返回 nullptr，验证成功更新 last_access）
// =============================================================================
const Session* SessionManager::GetSession(const std::string& session_id) {
  if (session_id.empty()) return nullptr;

  std::lock_guard<std::mutex> lock(mtx_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) return nullptr;

  if (IsExpired(it->second)) {
    sessions_.erase(it);
    LOG_INFO_FMT("session expired on access: sid=%s", session_id.c_str());
    return nullptr;
  }

  it->second.last_access = std::chrono::steady_clock::now();
  return &(it->second);
}

// =============================================================================
// 销毁会话
// =============================================================================
bool SessionManager::DestroySession(const std::string& session_id) {
  if (session_id.empty()) return false;

  std::lock_guard<std::mutex> lock(mtx_);
  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) return false;

  sessions_.erase(it);
  LOG_INFO_FMT("session destroyed: sid=%s", session_id.c_str());
  return true;
}

// =============================================================================
// 清理所有过期会话
// =============================================================================
size_t SessionManager::CleanExpired() {
  std::lock_guard<std::mutex> lock(mtx_);
  size_t removed = 0;
  for (auto it = sessions_.begin(); it != sessions_.end(); ) {
    if (IsExpired(it->second)) {
      it = sessions_.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }
  if (removed > 0) {
    LOG_INFO_FMT("cleaned %zu expired sessions", removed);
  }
  return removed;
}

// =============================================================================
// 当前活跃会话数
// =============================================================================
size_t SessionManager::ActiveCount() {
  std::lock_guard<std::mutex> lock(mtx_);
  size_t count = 0;
  for (const auto& kv : sessions_) {
    if (!IsExpired(kv.second)) ++count;
  }
  return count;
}

// =============================================================================
// 设置会话超时时间
// =============================================================================
void SessionManager::SetTimeout(int seconds) {
  std::lock_guard<std::mutex> lock(mtx_);
  timeout_sec_ = seconds;
}

}  // namespace oj
