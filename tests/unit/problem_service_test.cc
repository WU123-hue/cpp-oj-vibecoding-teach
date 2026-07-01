#include "service/problem_service.h"

#include <gtest/gtest.h>
#include <mysql/mysql.h>

#include <string>

#include "db/connection_pool.h"
#include "model/problem.h"
#include "model/test_case.h"
#include "utils/logger.h"

namespace {

constexpr const char* kHost     = "localhost";
constexpr const int   kPort     = 3306;
constexpr const char* kUser     = "root";
constexpr const char* kPassword = "";
constexpr const char* kDatabase = "oj_db";

oj::ConnectionPool& Pool() { return oj::ConnectionPool::Instance(); }

std::string QueryScalar(MYSQL* c, const std::string& sql) {
  if (::mysql_query(c, sql.c_str()) != 0) return "";
  MYSQL_RES* res = ::mysql_store_result(c);
  if (res == nullptr) return "";
  MYSQL_ROW row = ::mysql_fetch_row(res);
  std::string val = (row && row[0]) ? row[0] : "";
  ::mysql_free_result(res);
  return val;
}

}  // namespace

// =============================================================================
// 全局环境
// =============================================================================
class ProblemServiceTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    oj::Logger::Instance().Init("error", "/tmp/oj_svc_ut", "svc.log");
    ASSERT_TRUE(Pool().Init(kHost, kPort, kUser, kPassword, kDatabase, 2));
  }
  static void TearDownTestSuite() { Pool().Destroy(); }

  // 清理所有 UT_ 前缀的测试题目
  void CleanUp() {
    oj::ConnectionGuard g;
    if (!g.Valid()) return;
    ::mysql_query(g.Get(),
                  "DELETE FROM problems WHERE title LIKE 'UT_SVC_%'");
  }
};

// =============================================================================
// Create：正常创建题目（含测试用例）
// =============================================================================
TEST_F(ProblemServiceTest, CreateSuccess) {
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_CreateSuccess";
  req.difficulty = "Easy";
  req.content    = "求两个整数之和";
  req.tpl        = "#include <iostream>\nint main(){}";

  oj::TestCase tc1;
  tc1.set_input("1 2");
  tc1.set_expected("3");
  req.test_cases.push_back(tc1);

  oj::TestCase tc2;
  tc2.set_input("10 20");
  tc2.set_expected("30");
  req.test_cases.push_back(tc2);

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Create(req, &new_id, &reason)) << reason;
  EXPECT_GT(new_id, 0);

  // 验证题目已写入
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM problems WHERE id=" + std::to_string(new_id)), "1");
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT title FROM problems WHERE id=" + std::to_string(new_id)),
      "UT_SVC_CreateSuccess");
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT difficulty FROM problems WHERE id=" + std::to_string(new_id)),
      "Easy");

  // 验证测试用例已写入
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM test_cases WHERE problem_id=" +
          std::to_string(new_id)), "2");

  // 验证 position 按插入顺序递增
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT input FROM test_cases WHERE problem_id=" +
          std::to_string(new_id) + " ORDER BY position LIMIT 1"), "1 2");
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT input FROM test_cases WHERE problem_id=" +
          std::to_string(new_id) + " ORDER BY position LIMIT 1,1"), "10 20");

  CleanUp();
}

// =============================================================================
// Create：无测试用例也能创建
// =============================================================================
TEST_F(ProblemServiceTest, CreateNoTestCases) {
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_NoTestCases";
  req.difficulty = "Medium";
  req.content    = "无测试用例的题目";

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Create(req, &new_id, &reason)) << reason;
  EXPECT_GT(new_id, 0);

  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM test_cases WHERE problem_id=" +
          std::to_string(new_id)), "0");

  CleanUp();
}

// =============================================================================
// Create：缺少 title 返回失败
// =============================================================================
TEST_F(ProblemServiceTest, CreateMissingTitle) {
  oj::CreateProblemRequest req;
  req.difficulty = "Easy";
  req.content    = "缺少标题";

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Create(req, &new_id, &reason));
  EXPECT_EQ(reason, "title is required");
  EXPECT_EQ(new_id, 0);
}

// =============================================================================
// Create：空 title 返回失败
// =============================================================================
TEST_F(ProblemServiceTest, CreateEmptyTitle) {
  oj::CreateProblemRequest req;
  req.title      = "";
  req.difficulty = "Easy";
  req.content    = "空标题";

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Create(req, &new_id, &reason));
  EXPECT_EQ(reason, "title is required");
}

// =============================================================================
// Create：缺少 content 返回失败
// =============================================================================
TEST_F(ProblemServiceTest, CreateMissingContent) {
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_NoContent";
  req.difficulty = "Easy";

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Create(req, &new_id, &reason));
  EXPECT_EQ(reason, "content is required");
}

// =============================================================================
// Create：空 content 返回失败
// =============================================================================
TEST_F(ProblemServiceTest, CreateEmptyContent) {
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_EmptyContent";
  req.difficulty = "Easy";
  req.content    = "";

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Create(req, &new_id, &reason));
  EXPECT_EQ(reason, "content is required");
}

// =============================================================================
// Create：无效 difficulty 返回失败
// =============================================================================
TEST_F(ProblemServiceTest, CreateInvalidDifficulty) {
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_InvalidDiff";
  req.difficulty = "SuperHard";
  req.content    = "无效难度";

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Create(req, &new_id, &reason));
  EXPECT_EQ(reason, "invalid difficulty");
}

// =============================================================================
// Create：空字符串 difficulty 返回失败
// =============================================================================
TEST_F(ProblemServiceTest, CreateEmptyDifficulty) {
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_EmptyDiff";
  req.difficulty = "";
  req.content    = "空难度";

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  EXPECT_FALSE(svc.Create(req, &new_id, &reason));
  EXPECT_EQ(reason, "invalid difficulty");
}

// =============================================================================
// Create：三种合法难度均可创建
// =============================================================================
TEST_F(ProblemServiceTest, CreateAllDifficulties) {
  const char* diffs[] = {"Easy", "Medium", "Hard"};

  for (const char* d : diffs) {
    oj::CreateProblemRequest req;
    req.title      = std::string("UT_SVC_Diff_") + d;
    req.difficulty = d;
    req.content    = "难度测试";

    oj::ProblemService svc;
    int new_id = 0;
    std::string reason;
    ASSERT_TRUE(svc.Create(req, &new_id, &reason)) << d << ": " << reason;

    oj::ConnectionGuard g;
    ASSERT_TRUE(g.Valid());
    EXPECT_EQ(QueryScalar(g.Get(),
        "SELECT difficulty FROM problems WHERE id=" +
            std::to_string(new_id)), d);
  }

  CleanUp();
}

// =============================================================================
// Create：含特殊字符的内容能正确存储（SQL 转义）
// =============================================================================
TEST_F(ProblemServiceTest, CreateWithSpecialChars) {
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_Special'\"Chars";
  req.difficulty = "Easy";
  req.content    = "包含 '单引号' 和 \"双引号\" 及反斜杠 \\ 和换行\n第二行";
  req.tpl        = "cout << \"hello\";";

  oj::TestCase tc;
  tc.set_input("in'with\"quotes");
  tc.set_expected("out'with\"quotes");
  req.test_cases.push_back(tc);

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Create(req, &new_id, &reason)) << reason;

  // 验证内容完整
  oj::ProblemService svc2;
  oj::Problem problem;
  std::vector<oj::TestCase> tcs;
  ASSERT_TRUE(svc2.Get(new_id, &problem, &tcs, &reason));

  EXPECT_EQ(problem.title(), "UT_SVC_Special'\"Chars");
  EXPECT_EQ(problem.content(), "包含 '单引号' 和 \"双引号\" 及反斜杠 \\ 和换行\n第二行");
  EXPECT_EQ(problem.tpl(), "cout << \"hello\";");
  ASSERT_EQ(tcs.size(), 1u);
  EXPECT_EQ(tcs[0].input(), "in'with\"quotes");
  EXPECT_EQ(tcs[0].expected(), "out'with\"quotes");

  CleanUp();
}

// =============================================================================
// Create：空 template 字段合法
// =============================================================================
TEST_F(ProblemServiceTest, CreateEmptyTemplate) {
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_EmptyTpl";
  req.difficulty = "Easy";
  req.content    = "空模板";
  req.tpl        = "";

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Create(req, &new_id, &reason)) << reason;

  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM problems WHERE id=" +
          std::to_string(new_id)), "1");

  CleanUp();
}

// =============================================================================
// Delete：正常删除题目
// =============================================================================
TEST_F(ProblemServiceTest, DeleteSuccess) {
  // 先创建
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_DeleteSuccess";
  req.difficulty = "Easy";
  req.content    = "待删除";

  oj::TestCase tc;
  tc.set_input("1");
  tc.set_expected("2");
  req.test_cases.push_back(tc);

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Create(req, &new_id, &reason));

  // 删除
  EXPECT_TRUE(svc.Delete(new_id, &reason));

  // 验证已删除
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM problems WHERE id=" +
          std::to_string(new_id)), "0");
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM test_cases WHERE problem_id=" +
          std::to_string(new_id)), "0");
}

// =============================================================================
// Delete：级联删除测试用例
// =============================================================================
TEST_F(ProblemServiceTest, DeleteCascadesTestCases) {
  // 创建题目 + 多个测试用例
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_DeleteCascade";
  req.difficulty = "Hard";
  req.content    = "级联删除";

  for (int i = 0; i < 5; ++i) {
    oj::TestCase tc;
    tc.set_input(std::to_string(i));
    tc.set_expected(std::to_string(i + 1));
    req.test_cases.push_back(tc);
  }

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Create(req, &new_id, &reason));

  // 确认有 5 个测试用例
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM test_cases WHERE problem_id=" +
          std::to_string(new_id)), "5");

  // 删除题目，验证级联
  EXPECT_TRUE(svc.Delete(new_id, &reason));
  EXPECT_EQ(QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM test_cases WHERE problem_id=" +
          std::to_string(new_id)), "0");
}

// =============================================================================
// Delete：删除不存在的 id（MySQL DELETE 成功，返回 true）
// =============================================================================
TEST_F(ProblemServiceTest, DeleteNonExistentId) {
  oj::ProblemService svc;
  std::string reason;
  // 删除不存在的 id，DELETE FROM ... WHERE id=9999999 影响行数为0但SQL成功
  EXPECT_TRUE(svc.Delete(9999999, &reason));
}

// =============================================================================
// Delete：非法 id (id <= 0) 返回失败
// =============================================================================
TEST_F(ProblemServiceTest, DeleteInvalidId) {
  oj::ProblemService svc;
  std::string reason;

  EXPECT_FALSE(svc.Delete(0, &reason));
  EXPECT_EQ(reason, "invalid id");

  EXPECT_FALSE(svc.Delete(-1, &reason));
  EXPECT_EQ(reason, "invalid id");
}

// =============================================================================
// List：返回题目列表
// =============================================================================
TEST_F(ProblemServiceTest, ListReturnsProblems) {
  // 创建若干题目
  oj::CreateProblemRequest r1;
  r1.title = "UT_SVC_ListA";
  r1.difficulty = "Easy";
  r1.content = "a";
  oj::CreateProblemRequest r2;
  r2.title = "UT_SVC_ListB";
  r2.difficulty = "Hard";
  r2.content = "b";

  oj::ProblemService svc;
  int id1 = 0, id2 = 0;
  std::string reason;
  ASSERT_TRUE(svc.Create(r1, &id1, &reason));
  ASSERT_TRUE(svc.Create(r2, &id2, &reason));

  // 查询列表
  std::vector<oj::ProblemSummary> list;
  ASSERT_TRUE(svc.List(&list, &reason));

  // 确保列表中包含刚创建的两个
  bool found1 = false, found2 = false;
  for (const auto& p : list) {
    if (p.id == id1) {
      EXPECT_EQ(p.title, "UT_SVC_ListA");
      EXPECT_EQ(p.difficulty, "Easy");
      found1 = true;
    }
    if (p.id == id2) {
      EXPECT_EQ(p.title, "UT_SVC_ListB");
      EXPECT_EQ(p.difficulty, "Hard");
      found2 = true;
    }
  }
  EXPECT_TRUE(found1);
  EXPECT_TRUE(found2);

  CleanUp();
}

// =============================================================================
// List：空数据库也能返回空列表
// =============================================================================
TEST_F(ProblemServiceTest, ListReturnsNonEmpty) {
  std::vector<oj::ProblemSummary> list;
  oj::ProblemService svc;
  std::string reason;
  EXPECT_TRUE(svc.List(&list, &reason));
  // 只要能查询成功即可，列表可能不为空（admin默认题目等）
}

// =============================================================================
// Get：获取题目详情（含测试用例）
// =============================================================================
TEST_F(ProblemServiceTest, GetReturnsProblemDetail) {
  // 创建带测试用例的题目
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_GetDetail";
  req.difficulty = "Medium";
  req.content    = "详情测试";
  req.tpl        = "int main(){}";

  oj::TestCase tc1;
  tc1.set_input("1 2");
  tc1.set_expected("3");
  req.test_cases.push_back(tc1);

  oj::TestCase tc2;
  tc2.set_input("3 4");
  tc2.set_expected("7");
  req.test_cases.push_back(tc2);

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Create(req, &new_id, &reason));

  // Get
  oj::Problem problem;
  std::vector<oj::TestCase> tcs;
  ASSERT_TRUE(svc.Get(new_id, &problem, &tcs, &reason));

  EXPECT_EQ(problem.id(), new_id);
  EXPECT_EQ(problem.title(), "UT_SVC_GetDetail");
  EXPECT_EQ(problem.difficulty(), oj::Difficulty::Medium);
  EXPECT_EQ(problem.content(), "详情测试");
  EXPECT_EQ(problem.tpl(), "int main(){}");
  EXPECT_FALSE(problem.created_at().empty());

  // 验证测试用例按 position 排序
  ASSERT_EQ(tcs.size(), 2u);
  EXPECT_EQ(tcs[0].input(), "1 2");
  EXPECT_EQ(tcs[0].expected(), "3");
  EXPECT_EQ(tcs[0].position(), 0);
  EXPECT_EQ(tcs[1].input(), "3 4");
  EXPECT_EQ(tcs[1].expected(), "7");
  EXPECT_EQ(tcs[1].position(), 1);

  CleanUp();
}

// =============================================================================
// Get：获取不存在的题目返回失败
// =============================================================================
TEST_F(ProblemServiceTest, GetNonExistentReturnsFalse) {
  oj::ProblemService svc;
  oj::Problem problem;
  std::vector<oj::TestCase> tcs;
  std::string reason;
  EXPECT_FALSE(svc.Get(9999999, &problem, &tcs, &reason));
  EXPECT_EQ(reason, "problem not found");
}

// =============================================================================
// Get：无测试用例的题目也能获取
// =============================================================================
TEST_F(ProblemServiceTest, GetProblemWithoutTestCases) {
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_GetNoTc";
  req.difficulty = "Easy";
  req.content    = "无用例题目";

  oj::ProblemService svc;
  int new_id = 0;
  std::string reason;
  ASSERT_TRUE(svc.Create(req, &new_id, &reason));

  oj::Problem problem;
  std::vector<oj::TestCase> tcs;
  ASSERT_TRUE(svc.Get(new_id, &problem, &tcs, &reason));
  EXPECT_EQ(problem.title(), "UT_SVC_GetNoTc");
  EXPECT_TRUE(tcs.empty());

  CleanUp();
}

// =============================================================================
// 完整生命周期：Create -> Get -> List -> Delete -> Get 失败
// =============================================================================
TEST_F(ProblemServiceTest, FullLifecycle) {
  oj::ProblemService svc;
  std::string reason;

  // Create
  oj::CreateProblemRequest req;
  req.title      = "UT_SVC_Lifecycle";
  req.difficulty = "Hard";
  req.content    = "完整生命周期";
  req.tpl        = "// template";

  oj::TestCase tc;
  tc.set_input("42");
  tc.set_expected("42");
  req.test_cases.push_back(tc);

  int pid = 0;
  ASSERT_TRUE(svc.Create(req, &pid, &reason));

  // Get
  oj::Problem problem;
  std::vector<oj::TestCase> tcs;
  ASSERT_TRUE(svc.Get(pid, &problem, &tcs, &reason));
  EXPECT_EQ(problem.title(), "UT_SVC_Lifecycle");

  // List 中能找到
  std::vector<oj::ProblemSummary> list;
  ASSERT_TRUE(svc.List(&list, &reason));
  bool found = false;
  for (const auto& p : list) {
    if (p.id == pid) { found = true; break; }
  }
  EXPECT_TRUE(found);

  // Delete
  EXPECT_TRUE(svc.Delete(pid, &reason));

  // Get 失败
  oj::Problem problem2;
  std::vector<oj::TestCase> tcs2;
  EXPECT_FALSE(svc.Get(pid, &problem2, &tcs2, &reason));
  EXPECT_EQ(reason, "problem not found");
}
