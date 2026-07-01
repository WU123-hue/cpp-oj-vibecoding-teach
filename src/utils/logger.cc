#include "logger.h"

#include <sys/stat.h>
#include <unistd.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace oj {

namespace {

// 提取文件名（去掉目录路径），仅保留 basename
const char* Basename(const char* path) {
  if (path == nullptr) return nullptr;
  const char* p = path;
  for (const char* q = path; *q; ++q) {
    if (*q == '/' || *q == '\\') p = q + 1;
  }
  return p;
}

}  // namespace

Logger& Logger::Instance() {
  static Logger instance;
  return instance;
}

void Logger::Init(const std::string& level,
                  const std::string& dir,
                  const std::string& filename,
                  size_t max_file_size) {
  // 解析日志级别
  std::string lvl = level;
  std::transform(lvl.begin(), lvl.end(), lvl.begin(), ::tolower);
  if (lvl == "debug")      level_ = LogLevel::DEBUG;
  else if (lvl == "info")  level_ = LogLevel::INFO;
  else if (lvl == "warn")  level_ = LogLevel::WARN;
  else if (lvl == "error") level_ = LogLevel::ERROR;
  else                     level_ = LogLevel::INFO;

  dir_ = dir;
  filename_ = filename;
  max_file_size_ = max_file_size;

  // 创建日志目录（权限 0755）
  ::mkdir(dir_.c_str(), 0755);

  std::lock_guard<std::mutex> lock(mtx_);

  // 若已打开旧文件，先关闭再重新打开
  if (ofs_.is_open()) {
    ofs_.close();
  }

  // 打开日志文件
  std::string path = dir_ + "/" + filename_;
  ofs_.open(path, std::ios::app);
  if (!ofs_.is_open()) {
    std::cerr << "[Logger] failed to open log file: " << path << std::endl;
  }
  initialized_ = true;

  // Init 日志（直接写，避免递归加锁，此时已持锁）
  std::string header = FormatHeader(LogLevel::INFO, nullptr, 0);
  std::string line = header + "logger initialized: level=" + level +
                     ", file=" + path;
  if (ofs_.is_open()) {
    ofs_ << line << '\n';
    ofs_.flush();
  }
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

const char* Logger::LevelToColor(LogLevel lvl) {
  switch (lvl) {
    case LogLevel::DEBUG: return "\033[37m";   // 灰
    case LogLevel::INFO:  return "\033[32m";   // 绿
    case LogLevel::WARN:  return "\033[33m";   // 黄
    case LogLevel::ERROR: return "\033[31m";   // 红
  }
  return "\033[0m";
}

std::string Logger::FormatHeader(LogLevel lvl, const char* file, int line) {
  std::time_t now = std::time(nullptr);
  std::tm tm_buf;
  ::localtime_r(&now, &tm_buf);

  std::ostringstream oss;
  // 格式: [2026-06-30 12:00:00] [INFO] [file.cc:42]
  oss << '[' << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] "
      << '[' << LevelToStr(lvl) << ']';
  if (file != nullptr) {
    oss << " [" << Basename(file);
    if (line > 0) oss << ':' << line;
    oss << ']';
  }
  oss << ' ';
  return oss.str();
}

void Logger::RotateIfNeeded() {
  // 注意：调用方需已持锁
  if (!ofs_.is_open() || max_file_size_ == 0) return;

  std::string path = dir_ + "/" + filename_;
  struct stat st;
  if (::stat(path.c_str(), &st) != 0) return;
  if (static_cast<size_t>(st.st_size) < max_file_size_) return;

  // 轮转：当前文件 -> filename.1，旧 filename.1 -> filename.2，依此类推
  ofs_.close();

  // 最多保留 5 个历史文件
  for (int i = 4; i >= 1; --i) {
    std::string src = path + "." + std::to_string(i);
    std::string dst = path + "." + std::to_string(i + 1);
    ::rename(src.c_str(), dst.c_str());
  }
  ::rename(path.c_str(), (path + ".1").c_str());

  // 重新打开新文件
  ofs_.open(path, std::ios::app);
}

void Logger::Log(LogLevel lvl, const std::string& msg,
                 const char* file, int line) {
  if (static_cast<int>(lvl) < static_cast<int>(level_)) {
    return;
  }

  std::string header = FormatHeader(lvl, file, line);
  std::string line_str = header + msg;

  std::lock_guard<std::mutex> lock(mtx_);

  // 写文件
  if (ofs_.is_open()) {
    RotateIfNeeded();
    ofs_ << line_str << '\n';
    ofs_.flush();
  }

  // 同时输出到 stderr，带颜色
  std::cerr << LevelToColor(lvl) << line_str << "\033[0m" << std::endl;
}

// printf 风格格式化的内部辅助
namespace {

std::string FormatVaList(const char* fmt, va_list ap) {
  va_list ap2;
  va_copy(ap2, ap);
  int len = std::vsnprintf(nullptr, 0, fmt, ap2);
  va_end(ap2);
  if (len < 0) return "";

  std::string buf(static_cast<size_t>(len) + 1, '\0');
  std::vsnprintf(&buf[0], buf.size(), fmt, ap);
  buf.resize(static_cast<size_t>(len));
  return buf;
}

}  // namespace

void Logger::Debug(const std::string& msg, const char* file, int line) {
  Log(LogLevel::DEBUG, msg, file, line);
}
void Logger::Info(const std::string& msg, const char* file, int line) {
  Log(LogLevel::INFO, msg, file, line);
}
void Logger::Warn(const std::string& msg, const char* file, int line) {
  Log(LogLevel::WARN, msg, file, line);
}
void Logger::Error(const std::string& msg, const char* file, int line) {
  Log(LogLevel::ERROR, msg, file, line);
}

void Logger::DebugFmt(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  Log(LogLevel::DEBUG, FormatVaList(fmt, ap));
  va_end(ap);
}
void Logger::InfoFmt(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  Log(LogLevel::INFO, FormatVaList(fmt, ap));
  va_end(ap);
}
void Logger::WarnFmt(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  Log(LogLevel::WARN, FormatVaList(fmt, ap));
  va_end(ap);
}
void Logger::ErrorFmt(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  Log(LogLevel::ERROR, FormatVaList(fmt, ap));
  va_end(ap);
}

}  // namespace oj
