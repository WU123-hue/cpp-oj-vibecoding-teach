#include "executor_service.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "utils/logger.h"

namespace oj {

std::string JudgeStatusToStr(JudgeStatus s) {
  switch (s) {
    case JudgeStatus::CE:  return "CE";
    case JudgeStatus::AC:  return "AC";
    case JudgeStatus::WA:  return "WA";
    case JudgeStatus::TLE: return "TLE";
    case JudgeStatus::RE:  return "RE";
    case JudgeStatus::SE:  return "SE";
  }
  return "SE";
}

namespace {

// 将字符串写入文件
bool WriteFile(const std::string& path, const std::string& content) {
  std::ofstream ofs(path);
  if (!ofs.is_open()) return false;
  ofs << content;
  return ofs.good();
}

// 读取文件全部内容
std::string ReadFile(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) return "";
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return oss.str();
}

// 去除行末空白字符（空格、\r、\t）
void RTrim(std::string& s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\r' ||
                        s.back() == '\t')) {
    s.pop_back();
  }
}

// 获取当前时间戳（毫秒）
long long NowMs() {
  auto tp = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             tp.time_since_epoch()).count();
}

}  // namespace

// =============================================================================
// 创建临时工作目录
// =============================================================================
std::string ExecutorService::MakeTempDir() {
  char tmpl[] = "/tmp/oj_exec_XXXXXX";
  char* dir = ::mkdtemp(tmpl);
  if (dir == nullptr) return "";
  return std::string(dir);
}

// =============================================================================
// 递归删除目录（仅文件，不递归子目录——我们的目录里只有源码和可执行文件）
// =============================================================================
void ExecutorService::RemoveDir(const std::string& path) {
  // 删除目录内可能存在的文件
  ::unlink((path + "/main.cpp").c_str());
  ::unlink((path + "/main").c_str());
  ::unlink((path + "/compile_err.txt").c_str());
  ::unlink((path + "/stdin.txt").c_str());
  ::unlink((path + "/stdout.txt").c_str());
  ::unlink((path + "/stderr.txt").c_str());
  ::rmdir(path.c_str());
}

// =============================================================================
// 编译：fork + g++
// =============================================================================
bool ExecutorService::Compile(const std::string& code,
                              const std::string& src_path,
                              const std::string& exe_path,
                              std::string& compile_output) {
  // 写入源代码文件
  if (!WriteFile(src_path, code)) {
    compile_output = "failed to write source file";
    return false;
  }

  // 创建文件接收编译错误输出
  std::string err_path = src_path.substr(0, src_path.find_last_of('/')) +
                         "/compile_err.txt";
  int err_fd = ::open(err_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (err_fd < 0) {
    compile_output = "failed to create compile error file";
    return false;
  }

  pid_t pid = ::fork();
  if (pid < 0) {
    ::close(err_fd);
    compile_output = "fork failed";
    return false;
  }

  if (pid == 0) {
    // 子进程：重定向 stderr 到文件
    ::dup2(err_fd, STDERR_FILENO);
    ::close(err_fd);

    // g++ -std=c++17 -O2 -o <exe> <src>
    ::execlp("g++", "g++", "-std=c++17", "-O2", "-o",
             exe_path.c_str(), src_path.c_str(), nullptr);
    // execlp 失败
    ::_exit(127);
  }

  // 父进程：等待编译完成
  ::close(err_fd);
  int status = 0;
  ::waitpid(pid, &status, 0);

  compile_output = ReadFile(err_path);
  ::unlink(err_path.c_str());

  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    return true;  // 编译成功
  }

  if (compile_output.empty()) {
    compile_output = "compilation failed (no error output)";
  }
  return false;
}

// =============================================================================
// 运行单个测试用例：fork + exec + 超时控制 + 资源限制
// =============================================================================
CaseResult ExecutorService::RunSingleCase(const std::string& exe_path,
                                          const ExecTestCase& tc,
                                          int timeout_sec,
                                          int cpu_limit_sec,
                                          int mem_limit_mb) {
  CaseResult result;
  result.input = tc.input;
  result.expected = tc.expected;
  result.status = JudgeStatus::RE;
  result.exit_code = -1;
  result.elapsed_ms = 0;

  // 准备输入文件
  std::string dir = exe_path.substr(0, exe_path.find_last_of('/'));
  std::string stdin_path  = dir + "/stdin.txt";
  std::string stdout_path = dir + "/stdout.txt";
  std::string stderr_path = dir + "/stderr.txt";

  WriteFile(stdin_path, tc.input);

  int in_fd = ::open(stdin_path.c_str(), O_RDONLY);
  if (in_fd < 0) {
    result.status = JudgeStatus::SE;
    result.error_msg = "failed to open stdin file";
    return result;
  }

  int out_fd = ::open(stdout_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out_fd < 0) {
    ::close(in_fd);
    result.status = JudgeStatus::SE;
    result.error_msg = "failed to open stdout file";
    return result;
  }

  int err_fd = ::open(stderr_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (err_fd < 0) {
    ::close(in_fd);
    ::close(out_fd);
    result.status = JudgeStatus::SE;
    result.error_msg = "failed to open stderr file";
    return result;
  }

  long long start = NowMs();

  pid_t pid = ::fork();
  if (pid < 0) {
    ::close(in_fd);
    ::close(out_fd);
    ::close(err_fd);
    result.status = JudgeStatus::SE;
    result.error_msg = "fork failed";
    return result;
  }

  if (pid == 0) {
    // === 子进程 ===
    ::dup2(in_fd, STDIN_FILENO);
    ::dup2(out_fd, STDOUT_FILENO);
    ::dup2(err_fd, STDERR_FILENO);
    ::close(in_fd);
    ::close(out_fd);
    ::close(err_fd);

    // 设置 CPU 时间限制（RLIMIT_CPU）
    struct rlimit cpu_rl;
    cpu_rl.rlim_cur = static_cast<rlim_t>(cpu_limit_sec);
    cpu_rl.rlim_max = static_cast<rlim_t>(cpu_limit_sec + 1);
    ::setrlimit(RLIMIT_CPU, &cpu_rl);

    // 设置内存限制（RLIMIT_AS）
    struct rlimit mem_rl;
    mem_rl.rlim_cur = static_cast<rlim_t>(mem_limit_mb) * 1024 * 1024;
    mem_rl.rlim_max = static_cast<rlim_t>(mem_limit_mb) * 1024 * 1024;
    ::setrlimit(RLIMIT_AS, &mem_rl);

    // 设置输出文件大小限制，防止恶意刷屏
    struct rlimit fsize_rl;
    fsize_rl.rlim_cur = 16 * 1024 * 1024;  // 16MB
    fsize_rl.rlim_max = 16 * 1024 * 1024;
    ::setrlimit(RLIMIT_FSIZE, &fsize_rl);

    ::execl(exe_path.c_str(), exe_path.c_str(), nullptr);
    ::_exit(127);
  }

  // === 父进程：带超时等待 ===
  ::close(in_fd);
  ::close(out_fd);
  ::close(err_fd);

  int status = 0;
  bool timed_out = false;
  int elapsed = 0;

  for (;;) {
    pid_t ret = ::waitpid(pid, &status, WNOHANG);
    if (ret == pid) break;  // 子进程已退出

    elapsed = static_cast<int>(NowMs() - start);
    if (elapsed > timeout_sec * 1000) {
      timed_out = true;
      ::kill(pid, SIGKILL);
      ::waitpid(pid, &status, 0);
      break;
    }
    ::usleep(10000);  // 10ms 轮询
  }

  result.elapsed_ms = static_cast<int>(NowMs() - start);

  if (timed_out) {
    result.status = JudgeStatus::TLE;
    result.actual = ReadFile(stdout_path);
    result.exit_code = -1;
    return result;
  }

  // 读取子进程输出
  result.actual = ReadFile(stdout_path);
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

  // 检查是否被信号终止（RE）
  if (WIFSIGNALED(status)) {
    int sig = WTERMSIG(status);
    result.status = JudgeStatus::RE;
    if (sig == SIGXCPU) {
      // CPU 时间超限 → TLE
      result.status = JudgeStatus::TLE;
      result.error_msg = "CPU time limit exceeded (SIGXCPU)";
    } else if (sig == SIGSEGV) {
      result.error_msg = "segmentation fault";
    } else if (sig == SIGFPE) {
      result.error_msg = "floating point exception";
    } else if (sig == SIGABRT) {
      result.error_msg = "aborted";
    } else {
      result.error_msg = "killed by signal " + std::to_string(sig);
    }
    return result;
  }

  // 进程正常退出但返回非零 → RE
  if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
    result.status = JudgeStatus::RE;
    result.error_msg = "exit code " + std::to_string(WEXITSTATUS(status));
    return result;
  }

  // 比较输出
  if (CompareOutput(result.actual, tc.expected)) {
    result.status = JudgeStatus::AC;
  } else {
    result.status = JudgeStatus::WA;
  }

  return result;
}

// =============================================================================
// 比较输出：去除行末空白，忽略文件末尾多余空行
// =============================================================================
bool ExecutorService::CompareOutput(const std::string& actual,
                                    const std::string& expected) {
  std::istringstream act_stream(actual);
  std::istringstream exp_stream(expected);

  std::string act_line, exp_line;

  while (true) {
    bool act_has = static_cast<bool>(std::getline(act_stream, act_line));
    bool exp_has = static_cast<bool>(std::getline(exp_stream, exp_line));

    if (act_has && exp_has) {
      RTrim(act_line);
      RTrim(exp_line);
      if (act_line != exp_line) return false;
    } else if (act_has && !exp_has) {
      // expected 已结束，actual 还有行——如果剩余行全空则 AC
      RTrim(act_line);
      if (!act_line.empty()) return false;
      // 继续检查后续行
      while (std::getline(act_stream, act_line)) {
        RTrim(act_line);
        if (!act_line.empty()) return false;
      }
      return true;
    } else if (!act_has && exp_has) {
      // actual 已结束，expected 还有行——不能 AC
      RTrim(exp_line);
      if (exp_line.empty()) {
        // expected 剩余行全空也算 AC
        while (std::getline(exp_stream, exp_line)) {
          RTrim(exp_line);
          if (!exp_line.empty()) return false;
        }
        return true;
      }
      return false;
    } else {
      // 两个都结束了
      break;
    }
  }
  return true;
}

// =============================================================================
// 完整执行流程：编译 → 逐个运行测试用例 → 汇总
// =============================================================================
ExecutorResult ExecutorService::Execute(const std::string& code,
                                        const std::vector<ExecTestCase>& test_cases,
                                        int timeout_sec,
                                        int cpu_limit_sec,
                                        int mem_limit_mb) {
  ExecutorResult result;
  result.overall = JudgeStatus::CE;
  result.passed = 0;
  result.total = static_cast<int>(test_cases.size());
  result.max_elapsed_ms = 0;

  // 1. 创建临时工作目录
  std::string work_dir = MakeTempDir();
  if (work_dir.empty()) {
    result.overall = JudgeStatus::SE;
    result.compile_output = "failed to create temp directory";
    return result;
  }

  std::string src_path = work_dir + "/main.cpp";
  std::string exe_path = work_dir + "/main";

  // 2. 编译
  std::string compile_output;
  if (!Compile(code, src_path, exe_path, compile_output)) {
    result.overall = JudgeStatus::CE;
    result.compile_output = compile_output;
    RemoveDir(work_dir);
    LOG_INFO_FMT("compile error: %s", compile_output.c_str());
    return result;
  }

  // 3. 逐个运行测试用例
  result.overall = JudgeStatus::AC;  // 默认 AC，遇到失败更新
  bool any_failed = false;

  for (const auto& tc : test_cases) {
    CaseResult cr = RunSingleCase(exe_path, tc, timeout_sec,
                                  cpu_limit_sec, mem_limit_mb);
    result.cases.push_back(cr);

    if (cr.elapsed_ms > result.max_elapsed_ms) {
      result.max_elapsed_ms = cr.elapsed_ms;
    }

    if (cr.status == JudgeStatus::AC) {
      result.passed++;
    } else {
      any_failed = true;
      // 总体状态：取第一个失败用例的状态
      if (result.overall == JudgeStatus::AC) {
        result.overall = cr.status;
      }
    }
  }

  if (!any_failed && result.total > 0) {
    result.overall = JudgeStatus::AC;
  } else if (result.total == 0) {
    result.overall = JudgeStatus::AC;  // 无测试用例视为通过
  }

  // 4. 清理临时目录
  RemoveDir(work_dir);

  LOG_INFO_FMT("execute done: overall=%s passed=%d/%d max_ms=%d",
               JudgeStatusToStr(result.overall).c_str(),
               result.passed, result.total, result.max_elapsed_ms);
  return result;
}

}  // namespace oj
