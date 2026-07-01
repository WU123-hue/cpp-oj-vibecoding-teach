#ifndef OJ_CONNECTION_POOL_H
#define OJ_CONNECTION_POOL_H

#include <mysql/mysql.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace oj {

// 数据库连接配置
struct DbConfig;

// RAII 封装的 MySQL 连接：析构时自动归还连接池
class ConnectionGuard;

// 线程安全的 MySQL 连接池
class ConnectionPool {
 public:
  static ConnectionPool& Instance();

  // 初始化连接池：创建 pool_size 条到 MySQL 的连接
  bool Init(const std::string& host, int port,
            const std::string& user, const std::string& password,
            const std::string& database, int pool_size);

  // 从池中获取一条连接（阻塞等待直到有可用连接），失败返回 nullptr
  MYSQL* GetConnection(int timeout_ms = 5000);

  // 归还连接到池中
  void ReleaseConnection(MYSQL* conn);

  // 关闭并销毁所有连接
  void Destroy();

  // 状态查询
  int  TotalCount() const;
  int  FreeCount() const;

 private:
  friend class ConnectionGuard;

  ConnectionPool() = default;
  ~ConnectionPool();
  ConnectionPool(const ConnectionPool&) = delete;
  ConnectionPool& operator=(const ConnectionPool&) = delete;

  // 创建一条新的 MySQL 连接，失败返回 nullptr
  MYSQL* CreateConnection();

  mutable std::mutex           mtx_;
  std::condition_variable      cv_;
  std::queue<MYSQL*>           free_conns_;
  int                          total_ = 0;   // 已创建连接总数（含被借出的）
  int                          pool_size_ = 0;
  bool                         inited_ = false;

  // 连接参数（用于重建连接）
  std::string host_;
  int         port_ = 3306;
  std::string user_;
  std::string password_;
  std::string database_;
};

// RAII 连接守卫：构造时借出，析构时归还
class ConnectionGuard {
 public:
  explicit ConnectionGuard(int timeout_ms = 5000);
  ~ConnectionGuard();

  ConnectionGuard(const ConnectionGuard&) = delete;
  ConnectionGuard& operator=(const ConnectionGuard&) = delete;

  // 支持移动语义（便于存入容器）
  ConnectionGuard(ConnectionGuard&& other) noexcept;
  ConnectionGuard& operator=(ConnectionGuard&& other) noexcept;

  // 是否成功持有可用连接
  bool Valid() const { return conn_ != nullptr; }
  explicit operator bool() const { return Valid(); }

  // 获取底层 MYSQL 句柄
  MYSQL* Get() { return conn_; }
  MYSQL* operator->() { return conn_; }

 private:
  MYSQL* conn_ = nullptr;
};

}  // namespace oj

#endif  // OJ_CONNECTION_POOL_H
