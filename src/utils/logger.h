#ifndef OJ_LOGGER_H
#define OJ_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>

namespace oj {

enum class LogLevel {
  DEBUG,
  INFO,
  WARN,
  ERROR
};

// 简单的线程安全日志器
class Logger {
 public:
  static Logger& Instance();

  // 初始化：设置日志级别与输出目录/文件
  void Init(const std::string& level, const std::string& dir, const std::string& filename);

  // 日志接口
  void Debug(const std::string& msg);
  void Info(const std::string& msg);
  void Warn(const std::string& msg);
  void Error(const std::string& msg);

 private:
  Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void Log(LogLevel lvl, const std::string& msg);
  static const char* LevelToStr(LogLevel lvl);

  std::mutex   mtx_;
  std::ofstream ofs_;
  LogLevel     level_ = LogLevel::INFO;
  bool         initialized_ = false;
};

// 便捷宏
#define LOG_DEBUG(msg) oj::Logger::Instance().Debug(msg)
#define LOG_INFO(msg)  oj::Logger::Instance().Info(msg)
#define LOG_WARN(msg)  oj::Logger::Instance().Warn(msg)
#define LOG_ERROR(msg) oj::Logger::Instance().Error(msg)

}  // namespace oj

#endif  // OJ_LOGGER_H
