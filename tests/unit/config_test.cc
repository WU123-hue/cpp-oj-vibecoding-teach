#include "utils/config.h"
#include "utils/logger.h"

#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <cstdio>

namespace {

// 写入临时 yaml 文件，返回路径
std::string WriteYaml(const std::string& content) {
  std::string path = "/tmp/oj_config_test_" +
                     std::to_string(getpid()) + "_" +
                     std::to_string(clock()) + ".yaml";
  std::ofstream ofs(path);
  ofs << content;
  ofs.close();
  return path;
}

void RemoveFile(const std::string& path) {
  ::remove(path.c_str());
}

}  // namespace

// -----------------------------------------------------------------------------
// Config 单例
// -----------------------------------------------------------------------------
TEST(ConfigTest, SingletonIsSameInstance) {
  oj::Config& a = oj::Config::Instance();
  oj::Config& b = oj::Config::Instance();
  ASSERT_EQ(&a, &b);
}

// -----------------------------------------------------------------------------
// 默认值
// -----------------------------------------------------------------------------
TEST(ConfigTest, SetDefaultUsesBuiltinValues) {
  oj::Config& cfg = oj::Config::Instance();
  cfg.SetDefault();

  EXPECT_EQ(cfg.db().host, "localhost");
  EXPECT_EQ(cfg.db().port, 3306);
  EXPECT_EQ(cfg.db().user, "root");
  EXPECT_EQ(cfg.db().password, "");
  EXPECT_EQ(cfg.db().database, "oj_db");
  EXPECT_EQ(cfg.db().pool_size, 4);

  EXPECT_EQ(cfg.log().level, "info");
  EXPECT_EQ(cfg.log().dir, "logs");
  EXPECT_EQ(cfg.log().filename, "oj.log");

  EXPECT_EQ(cfg.server().host, "0.0.0.0");
  EXPECT_EQ(cfg.server().port, 8080);
  EXPECT_EQ(cfg.server().thread_count, 4);

  EXPECT_EQ(cfg.executor().timeout_sec, 5);
  EXPECT_EQ(cfg.executor().cpu_limit_sec, 2);
  EXPECT_EQ(cfg.executor().mem_limit_mb, 256);
}

// -----------------------------------------------------------------------------
// 正常加载完整配置
// -----------------------------------------------------------------------------
TEST(ConfigTest, LoadFullConfig) {
  std::string path = WriteYaml(R"(
server:
  host: "127.0.0.1"
  port: 9090
  thread_count: 8
database:
  host: "db.example.com"
  port: 3307
  user: "ojuser"
  password: "secret"
  database: "oj_prod"
  pool_size: 16
log:
  level: "debug"
  dir: "/var/log/oj"
  filename: "server.log"
executor:
  timeout_sec: 10
  cpu_limit_sec: 3
  mem_limit_mb: 512
)");

  oj::Config& cfg = oj::Config::Instance();
  ASSERT_TRUE(cfg.Load(path));

  EXPECT_EQ(cfg.server().host, "127.0.0.1");
  EXPECT_EQ(cfg.server().port, 9090);
  EXPECT_EQ(cfg.server().thread_count, 8);

  EXPECT_EQ(cfg.db().host, "db.example.com");
  EXPECT_EQ(cfg.db().port, 3307);
  EXPECT_EQ(cfg.db().user, "ojuser");
  EXPECT_EQ(cfg.db().password, "secret");
  EXPECT_EQ(cfg.db().database, "oj_prod");
  EXPECT_EQ(cfg.db().pool_size, 16);

  EXPECT_EQ(cfg.log().level, "debug");
  EXPECT_EQ(cfg.log().dir, "/var/log/oj");
  EXPECT_EQ(cfg.log().filename, "server.log");

  EXPECT_EQ(cfg.executor().timeout_sec, 10);
  EXPECT_EQ(cfg.executor().cpu_limit_sec, 3);
  EXPECT_EQ(cfg.executor().mem_limit_mb, 512);

  RemoveFile(path);
}

// -----------------------------------------------------------------------------
// 部分字段缺失时保留默认值
// -----------------------------------------------------------------------------
TEST(ConfigTest, LoadPartialConfigKeepsDefaults) {
  std::string path = WriteYaml(R"(
database:
  user: "custom_user"
  database: "custom_db"
)");

  oj::Config& cfg = oj::Config::Instance();
  cfg.SetDefault();  // 先恢复默认值
  ASSERT_TRUE(cfg.Load(path));

  // 指定的字段被覆盖
  EXPECT_EQ(cfg.db().user, "custom_user");
  EXPECT_EQ(cfg.db().database, "custom_db");

  // 未指定的字段保留默认值
  EXPECT_EQ(cfg.db().host, "localhost");
  EXPECT_EQ(cfg.db().port, 3306);
  EXPECT_EQ(cfg.db().password, "");
  EXPECT_EQ(cfg.db().pool_size, 4);

  // 其他节保留默认值
  EXPECT_EQ(cfg.server().port, 8080);
  EXPECT_EQ(cfg.log().level, "info");
  EXPECT_EQ(cfg.executor().timeout_sec, 5);

  RemoveFile(path);
}

// -----------------------------------------------------------------------------
// 空文件加载（合法 yaml，但无内容）应保留默认值
// -----------------------------------------------------------------------------
TEST(ConfigTest, LoadEmptyYamlKeepsDefaults) {
  std::string path = WriteYaml("");

  oj::Config& cfg = oj::Config::Instance();
  cfg.SetDefault();
  ASSERT_TRUE(cfg.Load(path));

  EXPECT_EQ(cfg.db().host, "localhost");
  EXPECT_EQ(cfg.server().port, 8080);

  RemoveFile(path);
}

// -----------------------------------------------------------------------------
// 文件不存在应返回 false
// -----------------------------------------------------------------------------
TEST(ConfigTest, LoadNonExistentFileReturnsFalse) {
  oj::Config& cfg = oj::Config::Instance();
  bool ok = cfg.Load("/tmp/oj_definitely_not_exist_12345.yaml");
  EXPECT_FALSE(ok);
}

// -----------------------------------------------------------------------------
// 非法 yaml 语法应返回 false（不抛异常）
// -----------------------------------------------------------------------------
TEST(ConfigTest, LoadMalformedYamlReturnsFalse) {
  std::string path = WriteYaml(R"(
server:
  host: "ok"
  - this is invalid yaml
  : : :
)");

  oj::Config& cfg = oj::Config::Instance();
  bool ok = cfg.Load(path);
  EXPECT_FALSE(ok);

  RemoveFile(path);
}

// -----------------------------------------------------------------------------
// 重复加载（热更新场景）应覆盖旧值
// -----------------------------------------------------------------------------
TEST(ConfigTest, ReloadOverwritesPreviousValues) {
  std::string path1 = WriteYaml(R"(
server:
  port: 1111
database:
  database: "db_one"
)");
  std::string path2 = WriteYaml(R"(
server:
  port: 2222
database:
  database: "db_two"
)");

  oj::Config& cfg = oj::Config::Instance();
  ASSERT_TRUE(cfg.Load(path1));
  EXPECT_EQ(cfg.server().port, 1111);
  EXPECT_EQ(cfg.db().database, "db_one");

  ASSERT_TRUE(cfg.Load(path2));
  EXPECT_EQ(cfg.server().port, 2222);
  EXPECT_EQ(cfg.db().database, "db_two");

  RemoveFile(path1);
  RemoveFile(path2);
}

// -----------------------------------------------------------------------------
// Logger 测试
// -----------------------------------------------------------------------------
class LoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = "/tmp/oj_logger_test_" + std::to_string(getpid());
    ::mkdir(dir_.c_str(), 0755);
  }
  void TearDown() override {
    // 清理测试目录
    std::string cmd = "rm -rf " + dir_;
    int ret = ::system(cmd.c_str());
    (void)ret;
  }
  std::string Dir() const { return dir_; }

 private:
  std::string dir_;
};

// 初始化后日志文件应被创建
TEST_F(LoggerTest, InitCreatesLogFile) {
  std::string file = Dir() + "/test.log";
  oj::Logger::Instance().Init("info", Dir(), "test.log");

  std::ifstream ifs(file);
  EXPECT_TRUE(ifs.good()) << "log file should exist after Init";
}

// 不同级别过滤：level=warn 时 debug/info 不写入
TEST_F(LoggerTest, LevelFiltering) {
  std::string file = Dir() + "/filter.log";
  oj::Logger::Instance().Init("warn", Dir(), "filter.log");

  oj::Logger::Instance().Debug("debug-msg-xyz");
  oj::Logger::Instance().Info("info-msg-xyz");
  oj::Logger::Instance().Warn("warn-msg-xyz");
  oj::Logger::Instance().Error("error-msg-xyz");

  std::ifstream ifs(file);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());

  EXPECT_EQ(content.find("debug-msg-xyz"), std::string::npos);
  EXPECT_EQ(content.find("info-msg-xyz"), std::string::npos);
  EXPECT_NE(content.find("warn-msg-xyz"), std::string::npos);
  EXPECT_NE(content.find("error-msg-xyz"), std::string::npos);
}

// 日志格式应包含时间戳和级别标记
TEST_F(LoggerTest, LogFormatContainsTimestampAndLevel) {
  std::string file = Dir() + "/fmt.log";
  oj::Logger::Instance().Init("debug", Dir(), "fmt.log");

  oj::Logger::Instance().Info("format-check");

  std::ifstream ifs(file);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());

  EXPECT_NE(content.find("[INFO]"), std::string::npos);
  EXPECT_NE(content.find("format-check"), std::string::npos);
  // 时间戳格式 [YYYY-MM-DD HH:MM:SS]
  EXPECT_NE(content.find("2026"), std::string::npos);
}

// debug 级别应记录所有日志
TEST_F(LoggerTest, DebugLevelLogsAll) {
  std::string file = Dir() + "/all.log";
  oj::Logger::Instance().Init("debug", Dir(), "all.log");

  oj::Logger::Instance().Debug("d");
  oj::Logger::Instance().Info("i");
  oj::Logger::Instance().Warn("w");
  oj::Logger::Instance().Error("e");

  std::ifstream ifs(file);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());

  EXPECT_NE(content.find("[DEBUG]"), std::string::npos);
  EXPECT_NE(content.find("[INFO]"), std::string::npos);
  EXPECT_NE(content.find("[WARN]"), std::string::npos);
  EXPECT_NE(content.find("[ERROR]"), std::string::npos);
}

// 无效级别字符串应回退到 INFO
TEST_F(LoggerTest, InvalidLevelDefaultsToInfo) {
  std::string file = Dir() + "/invalid.log";
  oj::Logger::Instance().Init("nonsense_level", Dir(), "invalid.log");

  // INFO 应被记录
  oj::Logger::Instance().Info("info-visible");
  // DEBUG 不应被记录
  oj::Logger::Instance().Debug("debug-hidden");

  std::ifstream ifs(file);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());

  EXPECT_NE(content.find("info-visible"), std::string::npos);
  EXPECT_EQ(content.find("debug-hidden"), std::string::npos);
}
