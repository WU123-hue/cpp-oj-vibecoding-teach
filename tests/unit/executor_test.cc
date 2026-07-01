#include "service/executor_service.h"

#include <gtest/gtest.h>

#include <dirent.h>
#include <sys/stat.h>

#include <chrono>
#include <cstring>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "utils/logger.h"

namespace {

constexpr int kTimeoutSec  = 5;
constexpr int kCpuLimitSec = 3;
constexpr int kMemLimitMb  = 256;

// 基本测试参数
oj::ExecutorService& Svc() {
  static oj::ExecutorService svc;
  return svc;
}

// 统计 /tmp/oj_exec_XXXXXX 目录数量（mkdtemp 生成的 6 位随机后缀）
// 排除 /tmp/oj_exec_ut（logger 日志目录）
int CountTempDirs() {
  DIR* d = ::opendir("/tmp");
  if (!d) return -1;
  int count = 0;
  struct dirent* ent;
  while ((ent = ::readdir(d)) != nullptr) {
    // 匹配 oj_exec_ 后跟至少 6 个字符的随机后缀
    if (std::strncmp(ent->d_name, "oj_exec_", 8) == 0) {
      std::string suffix(ent->d_name + 8);
      if (suffix != "ut" && suffix.size() >= 6) {
        ++count;
      }
    }
  }
  ::closedir(d);
  return count;
}

}  // namespace

// =============================================================================
// 全局环境
// =============================================================================
class ExecutorTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    oj::Logger::Instance().Init("error", "/tmp/oj_exec_ut", "exec.log");
  }
};

// =============================================================================
// Compile + Run: 正确的 A+B 程序，全部 AC
// =============================================================================
TEST_F(ExecutorTest, HelloWorldAC) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "Hello, World!" << std::endl;
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {
    {"", "Hello, World!\n"}
  };

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
  EXPECT_EQ(result.passed, 1);
  EXPECT_EQ(result.total, 1);
  ASSERT_EQ(result.cases.size(), 1u);
  EXPECT_EQ(result.cases[0].status, oj::JudgeStatus::AC);
}

// =============================================================================
// A+B 多组测试用例，全部 AC
// =============================================================================
TEST_F(ExecutorTest, AddMultipleCasesAllAC) {
  std::string code = R"(
#include <iostream>
int main() {
  int a, b;
  std::cin >> a >> b;
  std::cout << a + b << std::endl;
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {
    {"1 2\n", "3\n"},
    {"10 20\n", "30\n"},
    {"100 200\n", "300\n"},
    {"-5 5\n", "0\n"}
  };

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
  EXPECT_EQ(result.passed, 4);
  EXPECT_EQ(result.total, 4);
}

// =============================================================================
// A+B 部分用例 WA
// =============================================================================
TEST_F(ExecutorTest, PartialWA) {
  std::string code = R"(
#include <iostream>
int main() {
  int a, b;
  std::cin >> a >> b;
  std::cout << a + b << std::endl;
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {
    {"1 2\n", "3\n"},       // AC
    {"10 20\n", "99\n"},    // WA
    {"100 200\n", "300\n"}  // AC
  };

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::WA);
  EXPECT_EQ(result.passed, 2);
  EXPECT_EQ(result.total, 3);
  EXPECT_EQ(result.cases[0].status, oj::JudgeStatus::AC);
  EXPECT_EQ(result.cases[1].status, oj::JudgeStatus::WA);
  EXPECT_EQ(result.cases[2].status, oj::JudgeStatus::AC);
}

// =============================================================================
// 编译错误 (CE)
// =============================================================================
TEST_F(ExecutorTest, CompileError) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "missing semicolon"  // 缺分号
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", "Hello\n"}};

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::CE);
  EXPECT_FALSE(result.compile_output.empty());
  EXPECT_EQ(result.passed, 0);
  EXPECT_TRUE(result.cases.empty());
}

// =============================================================================
// 运行时错误 (RE) - 除零
// =============================================================================
TEST_F(ExecutorTest, RuntimeErrorDivByZero) {
  std::string code = R"(
#include <iostream>
int main() {
  int a = 1, b = 0;
  std::cout << a / b << std::endl;
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", "anything\n"}};

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::RE);
  EXPECT_EQ(result.passed, 0);
  ASSERT_EQ(result.cases.size(), 1u);
  EXPECT_EQ(result.cases[0].status, oj::JudgeStatus::RE);
  EXPECT_FALSE(result.cases[0].error_msg.empty());
}

// =============================================================================
// 运行时错误 (RE) - 段错误
// =============================================================================
TEST_F(ExecutorTest, RuntimeErrorSegfault) {
  std::string code = R"(
#include <iostream>
int main() {
  int* p = nullptr;
  *p = 42;
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", "42\n"}};

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::RE);
  ASSERT_EQ(result.cases.size(), 1u);
  EXPECT_EQ(result.cases[0].status, oj::JudgeStatus::RE);
  EXPECT_NE(result.cases[0].error_msg.find("segmentation"), std::string::npos);
}

// =============================================================================
// 运行时错误 (RE) - abort
// =============================================================================
TEST_F(ExecutorTest, RuntimeErrorAbort) {
  std::string code = R"(
#include <cstdlib>
int main() {
  std::abort();
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", ""}};

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::RE);
  EXPECT_EQ(result.cases[0].status, oj::JudgeStatus::RE);
}

// =============================================================================
// 超时 (TLE) - 死循环（CPU 限制或 wall-clock 超时均触发 TLE）
// =============================================================================
TEST_F(ExecutorTest, TimeoutInfiniteLoop) {
  std::string code = R"(
int main() {
  while (1) {}
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", "anything\n"}};

  // 使用 2 秒超时、1 秒 CPU 限制加速测试
  // CPU 限制会先触发 SIGXCPU（约1秒）或 wall-clock 超时（约2秒）
  auto result = Svc().Execute(code, cases, 2, 1, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::TLE);
  EXPECT_EQ(result.passed, 0);
  ASSERT_EQ(result.cases.size(), 1u);
  EXPECT_EQ(result.cases[0].status, oj::JudgeStatus::TLE);
  // CPU 限制或 wall-clock 超时，耗时应在 1~3 秒之间
  EXPECT_GE(result.cases[0].elapsed_ms, 900);
  EXPECT_LE(result.cases[0].elapsed_ms, 3500);
}

// =============================================================================
// 无测试用例视为 AC
// =============================================================================
TEST_F(ExecutorTest, NoTestCasesIsAC) {
  std::string code = R"(
int main() { return 0; }
)";

  std::vector<oj::ExecTestCase> cases;

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
  EXPECT_EQ(result.passed, 0);
  EXPECT_EQ(result.total, 0);
}

// =============================================================================
// 输出带行末空格仍 AC（CompareOutput 容错）
// =============================================================================
TEST_F(ExecutorTest, TrailingWhitespaceTolerated) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "hello  " << std::endl;
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", "hello\n"}};

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// =============================================================================
// 输出末尾多空行仍 AC
// =============================================================================
TEST_F(ExecutorTest, TrailingEmptyLinesTolerated) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "42\n\n\n" << std::endl;
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", "42\n"}};

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// =============================================================================
// 期望输出为空，实际输出有内容 → WA
// =============================================================================
TEST_F(ExecutorTest, ExpectedEmptyButActualHasContent) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "unexpected" << std::endl;
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", ""}};

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::WA);
}

// =============================================================================
// 多行输出完全匹配
// =============================================================================
TEST_F(ExecutorTest, MultilineOutputMatch) {
  std::string code = R"(
#include <iostream>
int main() {
  int n;
  std::cin >> n;
  for (int i = 1; i <= n; ++i) {
    std::cout << i << std::endl;
  }
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {
    {"3\n", "1\n2\n3\n"},
    {"5\n", "1\n2\n3\n4\n5\n"}
  };

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
  EXPECT_EQ(result.passed, 2);
}

// =============================================================================
// 多行输出有一行不匹配 → WA
// =============================================================================
TEST_F(ExecutorTest, MultilineOutputMismatch) {
  std::string code = R"(
#include <iostream>
int main() {
  int n;
  std::cin >> n;
  for (int i = 1; i <= n; ++i) {
    std::cout << i * 2 << std::endl;
  }
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {
    {"3\n", "1\n2\n3\n"}  // 实际输出 2 4 6
  };

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::WA);
}

// =============================================================================
// 正确返回 exit_code
// =============================================================================
TEST_F(ExecutorTest, ExitCodeCaptured) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "ok" << std::endl;
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", "ok\n"}};

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  ASSERT_EQ(result.cases.size(), 1u);
  EXPECT_EQ(result.cases[0].exit_code, 0);
}

// =============================================================================
// 非0退出码视为 RE
// =============================================================================
TEST_F(ExecutorTest, NonZeroExitIsRE) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "hello" << std::endl;
  return 42;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", "hello\n"}};

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::RE);
  ASSERT_EQ(result.cases.size(), 1u);
  EXPECT_EQ(result.cases[0].exit_code, 42);
}

// =============================================================================
// 首个失败用例决定 overall 状态
// =============================================================================
TEST_F(ExecutorTest, FirstFailureDeterminesOverall) {
  std::string code = R"(
#include <iostream>
int main() {
  int a, b;
  std::cin >> a >> b;
  std::cout << a * b << std::endl;
  return 0;
}
)";

  // case 0 应输出 2 但实际输出 2（AC），case 1 应输出 6 但实际 6（AC），
  // case 2 应输出 20 但实际 20（AC）—— 这里改一下让 case 1 WA
  std::vector<oj::ExecTestCase> cases = {
    {"1 2\n", "2\n"},        // AC
    {"2 3\n", "999\n"},      // WA (实际 6)
    {"4 5\n", "20\n"}        // AC
  };

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::WA);
  EXPECT_EQ(result.passed, 2);
}

// =============================================================================
// 读取多组输入（循环处理）
// =============================================================================
TEST_F(ExecutorTest, MultipleInputLines) {
  std::string code = R"(
#include <iostream>
int main() {
  int a, b;
  while (std::cin >> a >> b) {
    std::cout << a + b << std::endl;
  }
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {
    {"1 2\n3 4\n5 6\n", "3\n7\n11\n"}
  };

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// =============================================================================
// 内存超限 → RE 或 TLE（取决于系统行为）
// 使用持续分配 + 写入来触发 RLIMIT_AS
// =============================================================================
TEST_F(ExecutorTest, MemoryLimitExceeded) {
  std::string code = R"(
#include <cstdlib>
#include <cstring>
int main() {
  // 持续分配并写入内存，直到触发 RLIMIT_AS
  for (int i = 0; i < 1000; ++i) {
    char* p = (char*)malloc(8 * 1024 * 1024);  // 每次 8MB
    if (p == nullptr) {
      return 1;  // malloc 失败 → RE (exit code 1)
    }
    memset(p, 1, 8 * 1024 * 1024);  // 实际写入触发物理内存
  }
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", ""}};

  // 内存限制 32MB
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, 32);
  // 应该 RE（内存分配失败）或 TLE（如果系统行为不同）
  EXPECT_NE(result.overall, oj::JudgeStatus::AC);
  ASSERT_EQ(result.cases.size(), 1u);
  EXPECT_TRUE(result.cases[0].status == oj::JudgeStatus::RE ||
              result.cases[0].status == oj::JudgeStatus::TLE);
}

// =============================================================================
// 编译警告不导致 CE（编译成功但有 warning）
// =============================================================================
TEST_F(ExecutorTest, CompileWarningNotCE) {
  std::string code = R"(
#include <iostream>
int main() {
  int x;  // 未使用变量，会产生 warning
  std::cout << "ok" << std::endl;
  return 0;
}
)";

  std::vector<oj::ExecTestCase> cases = {{"", "ok\n"}};

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// =============================================================================
// JudgeStatusToStr 枚举转换
// =============================================================================
TEST(ExecutorEnumTest, JudgeStatusToStr) {
  EXPECT_EQ(oj::JudgeStatusToStr(oj::JudgeStatus::CE), "CE");
  EXPECT_EQ(oj::JudgeStatusToStr(oj::JudgeStatus::AC), "AC");
  EXPECT_EQ(oj::JudgeStatusToStr(oj::JudgeStatus::WA), "WA");
  EXPECT_EQ(oj::JudgeStatusToStr(oj::JudgeStatus::TLE), "TLE");
  EXPECT_EQ(oj::JudgeStatusToStr(oj::JudgeStatus::RE), "RE");
  EXPECT_EQ(oj::JudgeStatusToStr(oj::JudgeStatus::SE), "SE");
}

// =============================================================================
// CompareOutput 单元测试（通过 Execute 间接调用）
// 以下用最小程序测试各种输出比较边界情况
// =============================================================================

// 完全相同的空输出 → AC
TEST_F(ExecutorTest, CompareBothEmpty) {
  std::string code = "int main(){return 0;}";
  std::vector<oj::ExecTestCase> cases = {{"", ""}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// 期望有内容但实际为空 → WA
TEST_F(ExecutorTest, CompareActualEmptyExpectedNonEmpty) {
  std::string code = "int main(){return 0;}";
  std::vector<oj::ExecTestCase> cases = {{"", "hello\n"}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::WA);
}

// 行末 \r\n 容忍
TEST_F(ExecutorTest, CompareCRLF) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "line1\r\nline2\r\n";
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"", "line1\nline2\n"}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// 行末 tab 容忍
TEST_F(ExecutorTest, CompareTrailingTab) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "ok\t" << std::endl;
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"", "ok\n"}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// 中间空行匹配
TEST_F(ExecutorTest, CompareMiddleEmptyLine) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "a\n\nb\n";
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"", "a\n\nb\n"}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// 缺少中间空行 → WA
TEST_F(ExecutorTest, CompareMissingMiddleEmptyLine) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "a\nb\n";
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"", "a\n\nb\n"}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::WA);
}

// 末尾缺少换行 → 仍然 AC（getline 不要求末尾换行）
TEST_F(ExecutorTest, CompareMissingTrailingNewline) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "hello";
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"", "hello\n"}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// =============================================================================
// 临时目录清理验证
// =============================================================================

// AC 后临时目录被清理
TEST_F(ExecutorTest, TempDirCleanedAfterAC) {
  int before = CountTempDirs();
  std::string code = R"(
#include <iostream>
int main() { std::cout << "ok" << std::endl; return 0; }
)";
  std::vector<oj::ExecTestCase> cases = {{"", "ok\n"}};
  Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  int after = CountTempDirs();
  EXPECT_EQ(after, before);
}

// CE 后临时目录被清理
TEST_F(ExecutorTest, TempDirCleanedAfterCE) {
  int before = CountTempDirs();
  std::string code = "int main(){ syntax error }";
  std::vector<oj::ExecTestCase> cases = {{"", ""}};
  Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  int after = CountTempDirs();
  EXPECT_EQ(after, before);
}

// RE 后临时目录被清理
TEST_F(ExecutorTest, TempDirCleanedAfterRE) {
  int before = CountTempDirs();
  std::string code = R"(
#include <cstdlib>
int main() { std::abort(); return 0; }
)";
  std::vector<oj::ExecTestCase> cases = {{"", ""}};
  Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  int after = CountTempDirs();
  EXPECT_EQ(after, before);
}

// TLE 后临时目录被清理
TEST_F(ExecutorTest, TempDirCleanedAfterTLE) {
  int before = CountTempDirs();
  std::string code = "int main(){while(1){}return 0;}";
  std::vector<oj::ExecTestCase> cases = {{"", ""}};
  Svc().Execute(code, cases, 2, 1, kMemLimitMb);
  int after = CountTempDirs();
  EXPECT_EQ(after, before);
}

// =============================================================================
// 并发安全性：多个 Execute 同时运行不会冲突
// =============================================================================

// 并发执行相同代码，各自获得独立结果
TEST_F(ExecutorTest, ConcurrentExecutionNoConflict) {
  std::string code = R"(
#include <iostream>
int main() {
  int a, b;
  std::cin >> a >> b;
  std::cout << a + b << std::endl;
  return 0;
}
)";

  auto run = [&code](int a, int b) -> oj::ExecutorResult {
    oj::ExecutorService svc;
    std::vector<oj::ExecTestCase> cases = {
      {std::to_string(a) + " " + std::to_string(b) + "\n",
       std::to_string(a + b) + "\n"}
    };
    return svc.Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  };

  // 启动 8 个并发任务
  std::vector<std::future<oj::ExecutorResult>> futures;
  for (int i = 0; i < 8; ++i) {
    futures.push_back(std::async(std::launch::async, run, i * 10, i * 10 + 1));
  }

  // 等待全部完成
  for (int i = 0; i < 8; ++i) {
    auto result = futures[i].get();
    EXPECT_EQ(result.overall, oj::JudgeStatus::AC)
        << "task " << i << " failed";
    EXPECT_EQ(result.passed, 1);
    EXPECT_EQ(result.total, 1);
  }

  // 确认无残留临时目录
  EXPECT_EQ(CountTempDirs(), 0);
}

// 并发执行不同判题结果（AC + CE + RE 混合）
TEST_F(ExecutorTest, ConcurrentMixedResults) {
  std::string ac_code = R"(
#include <iostream>
int main() { std::cout << "ac" << std::endl; return 0; }
)";
  std::string ce_code = "int main(){ broken }";
  std::string re_code = R"(
#include <cstdlib>
int main() { std::abort(); return 0; }
)";

  auto run_ac = [&ac_code]() {
    oj::ExecutorService svc;
    return svc.Execute(ac_code, {{"", "ac\n"}}, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  };
  auto run_ce = [&ce_code]() {
    oj::ExecutorService svc;
    return svc.Execute(ce_code, {{"", ""}}, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  };
  auto run_re = [&re_code]() {
    oj::ExecutorService svc;
    return svc.Execute(re_code, {{"", ""}}, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  };

  auto f1 = std::async(std::launch::async, run_ac);
  auto f2 = std::async(std::launch::async, run_ce);
  auto f3 = std::async(std::launch::async, run_re);
  auto f4 = std::async(std::launch::async, run_ac);
  auto f5 = std::async(std::launch::async, run_ce);

  EXPECT_EQ(f1.get().overall, oj::JudgeStatus::AC);
  EXPECT_EQ(f2.get().overall, oj::JudgeStatus::CE);
  EXPECT_EQ(f3.get().overall, oj::JudgeStatus::RE);
  EXPECT_EQ(f4.get().overall, oj::JudgeStatus::AC);
  EXPECT_EQ(f5.get().overall, oj::JudgeStatus::CE);

  EXPECT_EQ(CountTempDirs(), 0);
}

// =============================================================================
// 耗时测量准确性
// =============================================================================

// elapsed_ms 对快速程序应很小
TEST_F(ExecutorTest, ElapsedMsForFastProgram) {
  std::string code = "int main(){return 0;}";
  std::vector<oj::ExecTestCase> cases = {{"", ""}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  ASSERT_EQ(result.cases.size(), 1u);
  EXPECT_GE(result.cases[0].elapsed_ms, 0);
  EXPECT_LT(result.cases[0].elapsed_ms, 2000);
}

// max_elapsed_ms 取所有用例最大值
TEST_F(ExecutorTest, MaxElapsedMsIsMaximum) {
  // 第二个用例输入更多数据，程序处理稍慢
  std::string code = R"(
#include <iostream>
int main() {
  int n, sum = 0;
  std::cin >> n;
  for (int i = 0; i < n; ++i) { int x; std::cin >> x; sum += x; }
  std::cout << sum << std::endl;
  return 0;
}
)";
  // 构造两个用例：一个少量数据，一个大量数据
  std::string small_input = "3\n1 2 3\n";
  std::string large_input = "100000\n";
  for (int i = 0; i < 100000; ++i) large_input += "1 ";

  std::vector<oj::ExecTestCase> cases = {
    {small_input, "6\n"},
    {large_input, "100000\n"}
  };

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.max_elapsed_ms, std::max(result.cases[0].elapsed_ms,
                                             result.cases[1].elapsed_ms));
}

// =============================================================================
// 编译器边界情况
// =============================================================================

// 仅包含注释的空程序合法
TEST_F(ExecutorTest, EmptyProgramWithComments) {
  std::string code = R"(
// This is a comment
int main() { return 0; }
)";
  std::vector<oj::ExecTestCase> cases = {{"", ""}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// 使用 C++17 特性编译成功
TEST_F(ExecutorTest, Cpp17FeatureCompiles) {
  std::string code = R"(
#include <iostream>
#include <optional>
std::optional<int> compute() { return 42; }
int main() {
  auto v = compute();
  if (v) std::cout << *v << std::endl;
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"", "42\n"}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// 包含 <bits/stdc++.h>（GCC 扩展头文件）编译成功
TEST_F(ExecutorTest, BitsStdCppHeader) {
  std::string code = R"(
#include <bits/stdc++.h>
using namespace std;
int main() {
  int a, b; cin >> a >> b;
  cout << a + b << endl;
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"1 2\n", "3\n"}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// 程序输出包含中文
TEST_F(ExecutorTest, ChineseOutput) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "你好世界" << std::endl;
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"", "你好世界\n"}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
}

// =============================================================================
// ExecutorResult 结构完整性
// =============================================================================

// AC 结果的 ExecutorResult 各字段正确
TEST_F(ExecutorTest, ResultFieldsComplete) {
  std::string code = R"(
#include <iostream>
int main() {
  int a, b;
  std::cin >> a >> b;
  std::cout << a + b << std::endl;
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {
    {"1 2\n", "3\n"},
    {"5 7\n", "12\n"}
  };

  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);

  EXPECT_EQ(result.overall, oj::JudgeStatus::AC);
  EXPECT_EQ(result.passed, 2);
  EXPECT_EQ(result.total, 2);
  EXPECT_GE(result.max_elapsed_ms, 0);
  EXPECT_TRUE(result.compile_output.empty());
  ASSERT_EQ(result.cases.size(), 2u);

  // case 0
  EXPECT_EQ(result.cases[0].status, oj::JudgeStatus::AC);
  EXPECT_EQ(result.cases[0].input, "1 2\n");
  EXPECT_EQ(result.cases[0].expected, "3\n");
  EXPECT_EQ(result.cases[0].actual, "3\n");
  EXPECT_EQ(result.cases[0].exit_code, 0);
  EXPECT_GE(result.cases[0].elapsed_ms, 0);
  EXPECT_TRUE(result.cases[0].error_msg.empty());

  // case 1
  EXPECT_EQ(result.cases[1].status, oj::JudgeStatus::AC);
  EXPECT_EQ(result.cases[1].actual, "12\n");
}

// WA 结果的 actual 字段保存了实际输出
TEST_F(ExecutorTest, WAActualFieldPopulated) {
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "wrong answer" << std::endl;
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"", "correct\n"}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  ASSERT_EQ(result.cases.size(), 1u);
  EXPECT_EQ(result.cases[0].status, oj::JudgeStatus::WA);
  EXPECT_EQ(result.cases[0].actual, "wrong answer\n");
}

// CE 结果的 compile_output 包含编译器错误
TEST_F(ExecutorTest, CECompileOutputContainsError) {
  std::string code = R"(
int main() {
  int x = ;  // 语法错误
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"", ""}};
  auto result = Svc().Execute(code, cases, kTimeoutSec, kCpuLimitSec, kMemLimitMb);
  EXPECT_EQ(result.overall, oj::JudgeStatus::CE);
  EXPECT_NE(result.compile_output.find("error"), std::string::npos);
  EXPECT_TRUE(result.cases.empty());
}

// TLE 结果的 actual 字段保存了部分输出
TEST_F(ExecutorTest, TLEActualFieldPopulated) {
  // 先输出再死循环，验证 actual 捕获了部分输出
  std::string code = R"(
#include <iostream>
int main() {
  std::cout << "partial" << std::endl;
  std::cout.flush();
  while (1) {}
  return 0;
}
)";
  std::vector<oj::ExecTestCase> cases = {{"", "complete\n"}};
  auto result = Svc().Execute(code, cases, 2, 1, kMemLimitMb);
  ASSERT_EQ(result.cases.size(), 1u);
  EXPECT_EQ(result.cases[0].status, oj::JudgeStatus::TLE);
  EXPECT_EQ(result.cases[0].actual, "partial\n");
}
