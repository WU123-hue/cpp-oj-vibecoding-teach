#ifndef OJ_LOGGER_H
#define OJ_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>

namespace oj {

enum class LogLevel {
  DEBUG = 0,
  INFO  = 1,
  WARN  = 2,
  ERROR = 3
};

// 线程安全的日志器，支持级别过滤、格式化、控制台着色、文件按大小轮转
class Logger {
 public:
  static Logger& Instance();

  // 初始化：设置日志级别、输出目录与文件名
  void Init(const std::string& level,
            const std::string& dir,
            const std::string& filename,
            size_t max_file_size = 10 * 1024 * 1024);  // 默认 10MB 轮转

  // 字符串日志接口（带源文件定位）
  void Debug(const std::string& msg, const char* file = nullptr, int line = 0);
  void Info(const std::string& msg, const char* file = nullptr, int line = 0);
  void Warn(const std::string& msg, const char* file = nullptr, int line = 0);
  void Error(const std::string& msg, const char* file = nullptr, int line = 0);

  // printf 风格格式化日志接口
  void DebugFmt(const char* fmt, ...);
  void InfoFmt(const char* fmt, ...);
  void WarnFmt(const char* fmt, ...);
  void ErrorFmt(const char* fmt, ...);

 private:
  Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void Log(LogLevel lvl, const std::string& msg,
           const char* file = nullptr, int line = 0);
  static const char* LevelToStr(LogLevel lvl);
  static const char* LevelToColor(LogLevel lvl);  // 终端颜色码
  void RotateIfNeeded();                          // 日志文件轮转
  std::string FormatHeader(LogLevel lvl,
                           const char* file, int line);

  std::mutex   mtx_;
  std::ofstream ofs_;
  std::string  dir_;
  std::string  filename_;
  size_t       max_file_size_ = 10 * 1024 * 1024;
  LogLevel     level_ = LogLevel::INFO;
  bool         initialized_ = false;
};

// 便捷宏：自动捕获 __FILE__ / __LINE__
#define LOG_DEBUG(msg) \
  oj::Logger::Instance().Debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg)  \
  oj::Logger::Instance().Info(msg, __FILE__, __LINE__)
#define LOG_WARN(msg)  \
  oj::Logger::Instance().Warn(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) \
  oj::Logger::Instance().Error(msg, __FILE__, __LINE__)

// printf 风格便捷宏
#define LOG_DEBUG_FMT(fmt, ...) oj::Logger::Instance().DebugFmt(fmt, ##__VA_ARGS__)
#define LOG_INFO_FMT(fmt, ...)  oj::Logger::Instance().InfoFmt(fmt, ##__VA_ARGS__)
#define LOG_WARN_FMT(fmt, ...)  oj::Logger::Instance().WarnFmt(fmt, ##__VA_ARGS__)
#define LOG_ERROR_FMT(fmt, ...) oj::Logger::Instance().ErrorFmt(fmt, ##__VA_ARGS__)

}  // namespace oj

#endif  // OJ_LOGGER_H
