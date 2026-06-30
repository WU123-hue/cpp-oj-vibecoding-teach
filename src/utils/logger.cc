#include "logger.h"

#include <sys/stat.h>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace oj {

Logger& Logger::Instance() {
  static Logger instance;
  return instance;
}

void Logger::Init(const std::string& level, const std::string& dir, const std::string& filename) {
  // 解析日志级别
  std::string lvl = level;
  std::transform(lvl.begin(), lvl.end(), lvl.begin(), ::tolower);
  if (lvl == "debug")      level_ = LogLevel::DEBUG;
  else if (lvl == "info")  level_ = LogLevel::INFO;
  else if (lvl == "warn")  level_ = LogLevel::WARN;
  else if (lvl == "error") level_ = LogLevel::ERROR;
  else                     level_ = LogLevel::INFO;

  // 创建日志目录（权限 0755）
  ::mkdir(dir.c_str(), 0755);

  // 若已打开旧文件，先关闭再重新打开
  if (ofs_.is_open()) {
    ofs_.close();
  }

  // 打开日志文件
  std::string path = dir + "/" + filename;
  ofs_.open(path, std::ios::app);
  if (!ofs_.is_open()) {
    std::cerr << "[Logger] failed to open log file: " << path << std::endl;
  }
  initialized_ = true;

  Log(LogLevel::INFO, "logger initialized: level=" + level + ", file=" + path);
}

const char* Logger::LevelToStr(LogLevel lvl) {
  switch (lvl) {
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO:  return "INFO";
    case LogLevel::WARN:  return "WARN";
    case LogLevel::ERROR: return "ERROR";
  }
  return "UNKNOWN";
}

void Logger::Log(LogLevel lvl, const std::string& msg) {
  if (static_cast<int>(lvl) < static_cast<int>(level_)) {
    return;
  }

  // 时间戳
  std::time_t now = std::time(nullptr);
  std::tm tm_buf;
  ::localtime_r(&now, &tm_buf);

  // 格式: [2026-06-30 12:00:00] [INFO] message
  std::ostringstream oss;
  oss << '[' << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] "
      << '[' << LevelToStr(lvl) << "] "
      << msg;

  std::string line = oss.str();

  std::lock_guard<std::mutex> lock(mtx_);
  // 写文件
  if (ofs_.is_open()) {
    ofs_ << line << '\n';
    ofs_.flush();
  }
  // 同时输出到 stderr（DEBUG/WARN 以上）
  if (lvl >= LogLevel::WARN) {
    std::cerr << line << std::endl;
  }
}

void Logger::Debug(const std::string& msg) { Log(LogLevel::DEBUG, msg); }
void Logger::Info(const std::string& msg)  { Log(LogLevel::INFO, msg); }
void Logger::Warn(const std::string& msg)  { Log(LogLevel::WARN, msg); }
void Logger::Error(const std::string& msg) { Log(LogLevel::ERROR, msg); }

}  // namespace oj
