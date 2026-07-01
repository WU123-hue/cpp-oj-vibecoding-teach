#include "connection_pool.h"

#include <chrono>
#include "utils/logger.h"

namespace oj {

ConnectionPool& ConnectionPool::Instance() {
  static ConnectionPool instance;
  return instance;
}

MYSQL* ConnectionPool::CreateConnection() {
  MYSQL* conn = ::mysql_init(nullptr);
  if (conn == nullptr) {
    LOG_ERROR("mysql_init failed");
    return nullptr;
  }

  // 设置连接超时，避免阻塞
  unsigned int timeout = 5;
  ::mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
  // 启用自动重连
  bool reconnect = true;
  ::mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);
  // 字符集 utf8mb4
  ::mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

  if (!::mysql_real_connect(conn, host_.c_str(), user_.c_str(),
                            password_.empty() ? nullptr : password_.c_str(),
                            database_.c_str(), port_, nullptr, 0)) {
    LOG_ERROR_FMT("mysql_real_connect failed: %s", ::mysql_error(conn));
    ::mysql_close(conn);
    return nullptr;
  }
  return conn;
}

bool ConnectionPool::Init(const std::string& host, int port,
                          const std::string& user, const std::string& password,
                          const std::string& database, int pool_size) {
  std::lock_guard<std::mutex> lock(mtx_);

  if (inited_) {
    LOG_WARN("connection pool already initialized");
    return true;
  }

  host_ = host;
  port_ = port;
  user_ = user;
  password_ = password;
  database_ = database;
  pool_size_ = pool_size > 0 ? pool_size : 4;

  ::mysql_library_init(0, nullptr, nullptr);

  int ok = 0;
  for (int i = 0; i < pool_size_; ++i) {
    MYSQL* conn = CreateConnection();
    if (conn) {
      free_conns_.push(conn);
      ++ok;
    }
  }
  total_ = ok;
  inited_ = (ok > 0);

  LOG_INFO_FMT("connection pool init: %d/%d connections established", ok, pool_size_);
  return ok > 0;
}

ConnectionPool::~ConnectionPool() {
  Destroy();
  ::mysql_library_end();
}

void ConnectionPool::Destroy() {
  std::lock_guard<std::mutex> lock(mtx_);
  while (!free_conns_.empty()) {
    ::mysql_close(free_conns_.front());
    free_conns_.pop();
  }
  total_ = 0;
  inited_ = false;
  LOG_INFO("connection pool destroyed");
}

MYSQL* ConnectionPool::GetConnection(int timeout_ms) {
  std::unique_lock<std::mutex> lock(mtx_);

  if (!inited_) {
    LOG_ERROR("connection pool not initialized");
    return nullptr;
  }

  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::milliseconds(timeout_ms);

  // 等待直到有空闲连接或超时
  while (free_conns_.empty()) {
    if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
      LOG_WARN("get connection timeout");
      return nullptr;
    }
  }

  // 取出空闲连接
  MYSQL* conn = free_conns_.front();
  free_conns_.pop();

  // 探活：若连接已断开则尝试重建
  if (::mysql_ping(conn) != 0) {
    LOG_WARN_FMT("connection lost, recreating: %s", ::mysql_error(conn));
    ::mysql_close(conn);
    conn = CreateConnection();
    if (conn == nullptr) {
      --total_;  // 重建失败，总数减一
      return nullptr;
    }
  }

  return conn;
}

void ConnectionPool::ReleaseConnection(MYSQL* conn) {
  if (conn == nullptr) return;

  std::lock_guard<std::mutex> lock(mtx_);
  free_conns_.push(conn);
  cv_.notify_one();
}

int ConnectionPool::TotalCount() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return total_;
}

int ConnectionPool::FreeCount() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return static_cast<int>(free_conns_.size());
}

// ---------------------------------------------------------------------------
// ConnectionGuard
// ---------------------------------------------------------------------------
ConnectionGuard::ConnectionGuard(int timeout_ms) {
  conn_ = ConnectionPool::Instance().GetConnection(timeout_ms);
}

ConnectionGuard::~ConnectionGuard() {
  if (conn_) {
    ConnectionPool::Instance().ReleaseConnection(conn_);
  }
}

ConnectionGuard::ConnectionGuard(ConnectionGuard&& other) noexcept
    : conn_(other.conn_) {
  other.conn_ = nullptr;
}

ConnectionGuard& ConnectionGuard::operator=(ConnectionGuard&& other) noexcept {
  if (this != &other) {
    if (conn_) {
      ConnectionPool::Instance().ReleaseConnection(conn_);
    }
    conn_ = other.conn_;
    other.conn_ = nullptr;
  }
  return *this;
}

}  // namespace oj
