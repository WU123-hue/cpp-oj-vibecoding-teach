#ifndef OJ_CONFIG_H
#define OJ_CONFIG_H

#include <string>

namespace oj {

// 数据库连接配置
struct DbConfig {
  std::string host     = "localhost";
  int         port     = 3306;
  std::string user     = "root";
  std::string password = "";
  std::string database = "oj_db";
  int         pool_size = 4;          // 连接池大小
};

// 日志配置
struct LogConfig {
  std::string level    = "info";      // debug/info/warn/error
  std::string dir      = "logs";      // 日志目录
  std::string filename = "oj.log";    // 日志文件名
};

// HTTP 服务器配置
struct ServerConfig {
  std::string host = "0.0.0.0";
  int         port = 8080;
  int         thread_count = 4;
};

// 代码执行配置
struct ExecutorConfig {
  int timeout_sec    = 5;             // 运行超时（秒）
  int cpu_limit_sec  = 2;             // CPU 时间限制（秒）
  int mem_limit_mb   = 256;           // 内存限制（MB）
};

// 全局配置单例
class Config {
 public:
  static Config& Instance();

  // 从 yaml 文件加载配置，成功返回 true
  bool Load(const std::string& path);

  // 加载失败时调用，使用内置默认值
  void SetDefault();

  // 各子配置访问器
  const DbConfig&       db()       const { return db_; }
  const LogConfig&      log()      const { return log_; }
  const ServerConfig&   server()   const { return server_; }
  const ExecutorConfig& executor() const { return executor_; }

 private:
  Config() = default;
  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;

  DbConfig       db_;
  LogConfig      log_;
  ServerConfig   server_;
  ExecutorConfig executor_;
};

}  // namespace oj

#endif  // OJ_CONFIG_H
