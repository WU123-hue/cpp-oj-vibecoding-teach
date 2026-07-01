#ifndef OJ_EXECUTOR_SERVICE_H
#define OJ_EXECUTOR_SERVICE_H

#include <string>
#include <vector>

namespace oj {

// 判题结果状态
enum class JudgeStatus {
  CE,   // Compile Error
  AC,   // Accepted
  WA,   // Wrong Answer
  TLE,  // Time Limit Exceeded
  RE,   // Runtime Error
  SE    // System Error
};

// 状态转字符串（用于 JSON 响应）
std::string JudgeStatusToStr(JudgeStatus s);

// 单个测试用例的运行结果
struct CaseResult {
  JudgeStatus  status;
  std::string  input;
  std::string  expected;
  std::string  actual;        // 实际 stdout
  int          exit_code;     // 子进程退出码
  int          elapsed_ms;    // 耗时（毫秒）
  std::string  error_msg;     // RE 时的错误描述
};

// 整体判题结果
struct ExecutorResult {
  JudgeStatus              overall;       // 总体判定
  std::string              compile_output; // 编译错误信息（CE 时填充）
  std::vector<CaseResult>  cases;         // 各测试用例结果
  int                      passed;        // 通过数
  int                      total;         // 总数
  int                      max_elapsed_ms; // 最大耗时
};

// 测试用例数据（执行时使用）
struct ExecTestCase {
  std::string input;
  std::string expected;
};

// 代码执行服务
class ExecutorService {
 public:
  // 编译并运行代码，对每个测试用例判定结果
  //   code        : 用户提交的 C++ 源代码
  //   test_cases  : 测试用例列表
  //   timeout_sec : 单个用例运行超时（秒）
  //   cpu_limit   : CPU 时间限制（秒）
  //   mem_limit_mb: 内存限制（MB）
  ExecutorResult Execute(const std::string& code,
                         const std::vector<ExecTestCase>& test_cases,
                         int timeout_sec,
                         int cpu_limit_sec,
                         int mem_limit_mb);

 private:
  // 编译代码到 exe_path，成功返回 true
  // 失败时 compile_output 填充编译器输出
  bool Compile(const std::string& code,
               const std::string& src_path,
               const std::string& exe_path,
               std::string& compile_output);

  // 运行单个测试用例
  CaseResult RunSingleCase(const std::string& exe_path,
                           const ExecTestCase& tc,
                           int timeout_sec,
                           int cpu_limit_sec,
                           int mem_limit_mb);

  // 比较实际输出与期望输出（去除行末空白后逐行比较）
  static bool CompareOutput(const std::string& actual,
                            const std::string& expected);

  // 创建临时工作目录
  static std::string MakeTempDir();

  // 递归删除目录
  static void RemoveDir(const std::string& path);
};

}  // namespace oj

#endif  // OJ_EXECUTOR_SERVICE_H
