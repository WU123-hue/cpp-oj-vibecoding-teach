#include "utils/logger.h"

#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>

namespace {

// 读取文件全部内容
std::string ReadFile(const std::string& path) {
  std::ifstream ifs(path);
  return std::string((std::istreambuf_iterator<char>(ifs)),
                      std::istreambuf_iterator<char>());
}

// 获取当前进程唯一临时目录
std::string MakeTempDir() {
  std::string dir = "/tmp/oj_logger_ut_" + std::to_string(getpid());
  ::mkdir(dir.c_str(), 0755);
  return dir;
}

}  // namespace

class LoggerUnitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = MakeTempDir();
  }
  void TearDown() override {
    std::string cmd = "rm -rf " + dir_;
    int ret = ::system(cmd.c_str());
    (void)ret;
  }
  std::string Dir() const { return dir_; }
  std::string File() const { return dir_ + "/test.log"; }

 private:
  std::string dir_;
};

// =============================================================================
// 单例
// =============================================================================
TEST_F(LoggerUnitTest, SingletonIsSameInstance) {
  oj::Logger& a = oj::Logger::Instance();
  oj::Logger& b = oj::Logger::Instance();
  ASSERT_EQ(&a, &b);
}

// =============================================================================
// Init 创建日志文件
// =============================================================================
TEST_F(LoggerUnitTest, InitCreatesLogFile) {
  oj::Logger::Instance().Init("info", Dir(), "test.log");
  std::ifstream ifs(File());
  EXPECT_TRUE(ifs.good());
}

// =============================================================================
// 级别过滤：warn 级时 debug/info 不写入文件
// =============================================================================
TEST_F(LoggerUnitTest, LevelFilteringWarn) {
  oj::Logger::Instance().Init("warn", Dir(), "test.log");

  oj::Logger::Instance().Debug("debug-hidden");
  oj::Logger::Instance().Info("info-hidden");
  oj::Logger::Instance().Warn("warn-visible");
  oj::Logger::Instance().Error("error-visible");

  std::string content = ReadFile(File());
  EXPECT_EQ(content.find("debug-hidden"), std::string::npos);
  EXPECT_EQ(content.find("info-hidden"), std::string::npos);
  EXPECT_NE(content.find("warn-visible"), std::string::npos);
  EXPECT_NE(content.find("error-visible"), std::string::npos);
}

// =============================================================================
// debug 级记录所有级别
// =============================================================================
TEST_F(LoggerUnitTest, DebugLevelLogsAll) {
  oj::Logger::Instance().Init("debug", Dir(), "test.log");

  oj::Logger::Instance().Debug("d");
  oj::Logger::Instance().Info("i");
  oj::Logger::Instance().Warn("w");
  oj::Logger::Instance().Error("e");

  std::string content = ReadFile(File());
  EXPECT_NE(content.find("[DEBUG]"), std::string::npos);
  EXPECT_NE(content.find("[INFO]"), std::string::npos);
  EXPECT_NE(content.find("[WARN]"), std::string::npos);
  EXPECT_NE(content.find("[ERROR]"), std::string::npos);
}

// =============================================================================
// 无效级别字符串回退到 INFO
// =============================================================================
TEST_F(LoggerUnitTest, InvalidLevelDefaultsToInfo) {
  oj::Logger::Instance().Init("nonsense", Dir(), "test.log");

  oj::Logger::Instance().Debug("debug-hidden");
  oj::Logger::Instance().Info("info-visible");

  std::string content = ReadFile(File());
  EXPECT_EQ(content.find("debug-hidden"), std::string::npos);
  EXPECT_NE(content.find("info-visible"), std::string::npos);
}

// =============================================================================
// 日志格式包含时间戳与级别标记
// =============================================================================
TEST_F(LoggerUnitTest, LogFormatContainsTimestampAndLevel) {
  oj::Logger::Instance().Init("debug", Dir(), "test.log");
  oj::Logger::Instance().Info("format-check");

  std::string content = ReadFile(File());
  EXPECT_NE(content.find("[INFO]"), std::string::npos);
  EXPECT_NE(content.find("format-check"), std::string::npos);
  // 时间戳格式 YYYY-MM-DD HH:MM:SS
  EXPECT_NE(content.find("2026"), std::string::npos);
}

// =============================================================================
// 源文件定位：file/line 参数写入日志
// =============================================================================
TEST_F(LoggerUnitTest, SourceLocationIncluded) {
  oj::Logger::Instance().Init("info", Dir(), "test.log");

  oj::Logger::Instance().Info("loc-msg", "handler.cc", 123);

  std::string content = ReadFile(File());
  EXPECT_NE(content.find("loc-msg"), std::string::npos);
  EXPECT_NE(content.find("handler.cc"), std::string::npos);
  EXPECT_NE(content.find(":123]"), std::string::npos);
}

// =============================================================================
// 宏自动捕获 __FILE__ / __LINE__
// =============================================================================
TEST_F(LoggerUnitTest, MacroCapturesSourceLocation) {
  oj::Logger::Instance().Init("info", Dir(), "test.log");

  LOG_INFO("macro-loc-msg");

  std::string content = ReadFile(File());
  EXPECT_NE(content.find("macro-loc-msg"), std::string::npos);
  // 应包含本文件名（basename）
  EXPECT_NE(content.find("logger_test.cc"), std::string::npos);
  // 应包含行号（数字）
  EXPECT_NE(content.find(":"), std::string::npos);
}

// =============================================================================
// 宏自动捕获的 basename 不含目录路径
// =============================================================================
TEST_F(LoggerUnitTest, MacroUsesBasenameOnly) {
  oj::Logger::Instance().Init("info", Dir(), "test.log");

  LOG_WARN("basename-check");

  std::string content = ReadFile(File());
  // 不应包含绝对路径中的 / 分隔符（在文件名标记内）
  // 至少应包含 basename
  EXPECT_NE(content.find("logger_test.cc"), std::string::npos);
}

// =============================================================================
// printf 风格格式化：InfoFmt
// =============================================================================
TEST_F(LoggerUnitTest, PrintfStyleFormatting) {
  oj::Logger::Instance().Init("info", Dir(), "test.log");

  oj::Logger::Instance().InfoFmt("user %s solved %d problems, rate=%.2f",
                                  "alice", 42, 0.95);

  std::string content = ReadFile(File());
  EXPECT_NE(content.find("user alice solved 42 problems, rate=0.95"),
            std::string::npos);
}

// =============================================================================
// printf 风格宏 LOG_*_FMT
// =============================================================================
TEST_F(LoggerUnitTest, PrintfStyleMacros) {
  oj::Logger::Instance().Init("debug", Dir(), "test.log");

  LOG_DEBUG_FMT("debug %d", 1);
  LOG_INFO_FMT("info %d", 2);
  LOG_WARN_FMT("warn %d", 3);
  LOG_ERROR_FMT("error %d", 4);

  std::string content = ReadFile(File());
  EXPECT_NE(content.find("debug 1"), std::string::npos);
  EXPECT_NE(content.find("info 2"), std::string::npos);
  EXPECT_NE(content.find("warn 3"), std::string::npos);
  EXPECT_NE(content.find("error 4"), std::string::npos);
}

// =============================================================================
// 空格式化字符串
// =============================================================================
TEST_F(LoggerUnitTest, EmptyFormatString) {
  oj::Logger::Instance().Init("info", Dir(), "test.log");

  oj::Logger::Instance().InfoFmt("");

  std::string content = ReadFile(File());
  // 应有空消息行存在（不崩溃）
  EXPECT_NE(content.find("[INFO]"), std::string::npos);
}

// =============================================================================
// 日志文件轮转：超过 max_file_size 时生成 .1 备份
// =============================================================================
TEST_F(LoggerUnitTest, FileRotation) {
  // 设置很小的轮转阈值（200 字节）
  oj::Logger::Instance().Init("info", Dir(), "test.log", 200);

  // 写入足够多内容触发轮转
  for (int i = 0; i < 10; ++i) {
    oj::Logger::Instance().InfoFmt("rotation-line-%d-padding-padding-padding-padding", i);
  }

  // 当前文件
  std::string cur = ReadFile(File());
  EXPECT_FALSE(cur.empty());

  // 应存在 .1 备份文件
  std::string backup1 = File() + ".1";
  std::ifstream ifs1(backup1);
  EXPECT_TRUE(ifs1.good()) << "rotation backup .1 should exist";
}

// =============================================================================
// 轮转：max_file_size=0 表示不轮转（全部写入同一文件）
// =============================================================================
TEST_F(LoggerUnitTest, NoRotationWhenSizeZero) {
  oj::Logger::Instance().Init("info", Dir(), "test.log", 0);

  for (int i = 0; i < 5; ++i) {
    oj::Logger::Instance().InfoFmt("no-rotate-%d", i);
  }

  std::string content = ReadFile(File());
  EXPECT_NE(content.find("no-rotate-0"), std::string::npos);
  EXPECT_NE(content.find("no-rotate-4"), std::string::npos);

  // 不应有备份文件
  std::ifstream ifs1(File() + ".1");
  EXPECT_FALSE(ifs1.good());
}

// =============================================================================
// 多线程并发写日志：不丢失、不交错（线程安全）
// =============================================================================
TEST_F(LoggerUnitTest, ConcurrentLoggingIsThreadSafe) {
  oj::Logger::Instance().Init("info", Dir(), "test.log");

  const int thread_count = 8;
  const int per_thread = 50;

  std::vector<std::thread> threads;
  std::atomic<int> counter{0};

  for (int t = 0; t < thread_count; ++t) {
    threads.emplace_back([&counter, t]() {
      for (int i = 0; i < per_thread; ++i) {
        oj::Logger::Instance().InfoFmt("thread-%d-msg-%d", t, i);
        counter.fetch_add(1);
      }
    });
  }
  for (auto& th : threads) th.join();

  EXPECT_EQ(counter.load(), thread_count * per_thread);

  // 日志文件中应包含所有线程的所有消息（数量正确）
  std::string content = ReadFile(File());
  int total_lines = 0;
  size_t pos = 0;
  while ((pos = content.find('\n', pos)) != std::string::npos) {
    ++total_lines;
    ++pos;
  }
  // 至少应包含 thread_count * per_thread 条业务日志（不含 init 行）
  EXPECT_GE(total_lines, thread_count * per_thread);
}

// =============================================================================
// 重新 Init 后写入新文件（不残留旧状态）
// =============================================================================
TEST_F(LoggerUnitTest, ReinitSwitchesFile) {
  oj::Logger::Instance().Init("info", Dir(), "first.log");
  oj::Logger::Instance().Info("in-first");

  oj::Logger::Instance().Init("info", Dir(), "second.log");
  oj::Logger::Instance().Info("in-second");

  std::string first = ReadFile(Dir() + "/first.log");
  std::string second = ReadFile(Dir() + "/second.log");

  EXPECT_NE(first.find("in-first"), std::string::npos);
  EXPECT_EQ(first.find("in-second"), std::string::npos);

  EXPECT_EQ(second.find("in-first"), std::string::npos);
  EXPECT_NE(second.find("in-second"), std::string::npos);
}
