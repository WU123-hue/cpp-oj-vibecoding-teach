#include "db/connection_pool.h"
#include "model/problem.h"
#include "model/test_case.h"
#include "model/user.h"
#include "utils/logger.h"

#include <gtest/gtest.h>
#include <mysql/mysql.h>

#include <string>

namespace {

constexpr const char* kHost     = "localhost";
constexpr const int   kPort     = 3306;
constexpr const char* kUser     = "root";
constexpr const char* kPassword = "";
constexpr const char* kDatabase = "oj_db";

oj::ConnectionPool& Pool() {
  return oj::ConnectionPool::Instance();
}

bool ExecSql(MYSQL* c, const std::string& sql) {
  return ::mysql_query(c, sql.c_str()) == 0;
}

// 查询单值
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
// 全局环境：套件级初始化连接池
// =============================================================================
class ModelCrudTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    oj::Logger::Instance().Init("error", "/tmp/oj_crud_ut", "crud.log");
    ASSERT_TRUE(Pool().Init(kHost, kPort, kUser, kPassword, kDatabase, 2));
  }
  static void TearDownTestSuite() {
    Pool().Destroy();
  }

  // 每个用例获取独立连接
  oj::ConnectionGuard Conn() { return oj::ConnectionGuard(); }
};

// =============================================================================
// Problem::SaveToDB (INSERT)：插入新题目并回填 id
// =============================================================================
TEST_F(ModelCrudTest, ProblemSaveInsert) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::Problem p;
  p.set_title("UT_CRUD_ProblemInsert");
  p.set_difficulty(oj::Difficulty::Medium);
  p.set_content("测试插入题目内容");
  p.set_tpl("int main(){}");

  ASSERT_TRUE(p.SaveToDB(c));
  int new_id = p.id();
  EXPECT_GT(new_id, 0);

  // 验证已写入
  EXPECT_EQ(QueryScalar(c,
      "SELECT COUNT(*) FROM problems WHERE id=" + std::to_string(new_id)), "1");

  // 清理
  EXPECT_TRUE(oj::Problem::DeleteFromDB(c, new_id));
}

// =============================================================================
// Problem::SaveToDB (UPDATE)：更新已有题目
// =============================================================================
TEST_F(ModelCrudTest, ProblemSaveUpdate) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  // 先插入
  oj::Problem p;
  p.set_title("UT_CRUD_ProblemUpdate");
  p.set_difficulty(oj::Difficulty::Easy);
  p.set_content("原始内容");
  p.set_tpl("原始模板");
  ASSERT_TRUE(p.SaveToDB(c));
  int pid = p.id();
  ASSERT_GT(pid, 0);

  // 修改并更新
  p.set_title("UT_CRUD_ProblemUpdated");
  p.set_difficulty(oj::Difficulty::Hard);
  p.set_content("修改后内容");
  p.set_tpl("修改后模板");
  EXPECT_TRUE(p.SaveToDB(c));
  EXPECT_EQ(p.id(), pid);  // id 不变

  // 重新加载验证
  oj::Problem p2;
  ASSERT_TRUE(p2.LoadFromDB(c, pid));
  EXPECT_EQ(p2.title(), "UT_CRUD_ProblemUpdated");
  EXPECT_EQ(p2.difficulty(), oj::Difficulty::Hard);
  EXPECT_EQ(p2.content(), "修改后内容");
  EXPECT_EQ(p2.tpl(), "修改后模板");

  EXPECT_TRUE(oj::Problem::DeleteFromDB(c, pid));
}

// =============================================================================
// Problem::LoadFromDB：加载存在的题目
// =============================================================================
TEST_F(ModelCrudTest, ProblemLoadExisting) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::Problem p;
  p.set_title("UT_CRUD_ProblemLoad");
  p.set_difficulty(oj::Difficulty::Hard);
  p.set_content("加载测试内容");
  p.set_tpl("加载模板");
  ASSERT_TRUE(p.SaveToDB(c));
  int pid = p.id();

  oj::Problem loaded;
  EXPECT_TRUE(loaded.LoadFromDB(c, pid));
  EXPECT_EQ(loaded.id(), pid);
  EXPECT_EQ(loaded.title(), "UT_CRUD_ProblemLoad");
  EXPECT_EQ(loaded.difficulty(), oj::Difficulty::Hard);
  EXPECT_EQ(loaded.content(), "加载测试内容");
  EXPECT_EQ(loaded.tpl(), "加载模板");
  EXPECT_FALSE(loaded.created_at().empty());

  EXPECT_TRUE(oj::Problem::DeleteFromDB(c, pid));
}

// =============================================================================
// Problem::LoadFromDB：加载不存在的 id 返回 false
// =============================================================================
TEST_F(ModelCrudTest, ProblemLoadNonExistentReturnsFalse) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());

  oj::Problem p;
  EXPECT_FALSE(p.LoadFromDB(g.Get(), 9999999));
}

// =============================================================================
// Problem::DeleteFromDB：删除题目，级联删除测试用例
// =============================================================================
TEST_F(ModelCrudTest, ProblemDeleteCascadesTestCases) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::Problem p;
  p.set_title("UT_CRUD_ProblemDeleteCascade");
  p.set_content("x");
  ASSERT_TRUE(p.SaveToDB(c));
  int pid = p.id();

  // 插入关联的测试用例
  ASSERT_TRUE(ExecSql(c,
      "INSERT INTO test_cases(problem_id,input,expected,position) "
      "VALUES(" + std::to_string(pid) + ",'1','2',0)"));
  EXPECT_EQ(QueryScalar(c,
      "SELECT COUNT(*) FROM test_cases WHERE problem_id=" + std::to_string(pid)), "1");

  // 删除题目，测试用例应级联删除
  EXPECT_TRUE(oj::Problem::DeleteFromDB(c, pid));
  EXPECT_EQ(QueryScalar(c,
      "SELECT COUNT(*) FROM problems WHERE id=" + std::to_string(pid)), "0");
  EXPECT_EQ(QueryScalar(c,
      "SELECT COUNT(*) FROM test_cases WHERE problem_id=" + std::to_string(pid)), "0");
}

// =============================================================================
// Problem::SaveToDB：含特殊字符的内容（SQL 转义）
// =============================================================================
TEST_F(ModelCrudTest, ProblemSaveWithSpecialChars) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::Problem p;
  p.set_title("UT_CRUD_Special'\"Chars");
  p.set_content("包含 '单引号' 和 \"双引号\" 及反斜杠 \\ 和换行\n第二行");
  p.set_tpl("cout << \"hello\";");
  ASSERT_TRUE(p.SaveToDB(c));
  int pid = p.id();

  oj::Problem loaded;
  ASSERT_TRUE(loaded.LoadFromDB(c, pid));
  EXPECT_EQ(loaded.title(), "UT_CRUD_Special'\"Chars");
  EXPECT_EQ(loaded.content(), "包含 '单引号' 和 \"双引号\" 及反斜杠 \\ 和换行\n第二行");
  EXPECT_EQ(loaded.tpl(), "cout << \"hello\";");

  EXPECT_TRUE(oj::Problem::DeleteFromDB(c, pid));
}

// =============================================================================
// TestCase::SaveToDB (INSERT)：插入测试用例
// =============================================================================
TEST_F(ModelCrudTest, TestCaseSaveInsert) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  // 先建父题目
  oj::Problem p;
  p.set_title("UT_CRUD_TcInsert");
  p.set_content("x");
  ASSERT_TRUE(p.SaveToDB(c));
  int pid = p.id();

  oj::TestCase tc;
  tc.set_problem_id(pid);
  tc.set_input("3\n1 2 3\n");
  tc.set_expected("6\n");
  tc.set_position(0);
  ASSERT_TRUE(tc.SaveToDB(c));
  EXPECT_GT(tc.id(), 0);

  // 清理（删除题目会级联删除用例）
  EXPECT_TRUE(oj::Problem::DeleteFromDB(c, pid));
}

// =============================================================================
// TestCase::LoadFromDB / SaveToDB (UPDATE)
// =============================================================================
TEST_F(ModelCrudTest, TestCaseLoadAndUpdate) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::Problem p;
  p.set_title("UT_CRUD_TcUpdate");
  p.set_content("x");
  ASSERT_TRUE(p.SaveToDB(c));
  int pid = p.id();

  oj::TestCase tc;
  tc.set_problem_id(pid);
  tc.set_input("1 2");
  tc.set_expected("3");
  tc.set_position(0);
  ASSERT_TRUE(tc.SaveToDB(c));
  int tcid = tc.id();

  // 加载验证
  oj::TestCase loaded;
  ASSERT_TRUE(loaded.LoadFromDB(c, tcid));
  EXPECT_EQ(loaded.id(), tcid);
  EXPECT_EQ(loaded.problem_id(), pid);
  EXPECT_EQ(loaded.input(), "1 2");
  EXPECT_EQ(loaded.expected(), "3");
  EXPECT_EQ(loaded.position(), 0);

  // 更新
  loaded.set_input("10 20");
  loaded.set_expected("30");
  loaded.set_position(5);
  EXPECT_TRUE(loaded.SaveToDB(c));

  oj::TestCase loaded2;
  ASSERT_TRUE(loaded2.LoadFromDB(c, tcid));
  EXPECT_EQ(loaded2.input(), "10 20");
  EXPECT_EQ(loaded2.expected(), "30");
  EXPECT_EQ(loaded2.position(), 5);

  EXPECT_TRUE(oj::Problem::DeleteFromDB(c, pid));
}

// =============================================================================
// TestCase::LoadFromDB：不存在返回 false
// =============================================================================
TEST_F(ModelCrudTest, TestCaseLoadNonExistentReturnsFalse) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());

  oj::TestCase tc;
  EXPECT_FALSE(tc.LoadFromDB(g.Get(), 9999999));
}

// =============================================================================
// TestCase::DeleteFromDB
// =============================================================================
TEST_F(ModelCrudTest, TestCaseDelete) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::Problem p;
  p.set_title("UT_CRUD_TcDelete");
  p.set_content("x");
  ASSERT_TRUE(p.SaveToDB(c));
  int pid = p.id();

  oj::TestCase tc;
  tc.set_problem_id(pid);
  tc.set_input("in");
  tc.set_expected("out");
  tc.set_position(0);
  ASSERT_TRUE(tc.SaveToDB(c));
  int tcid = tc.id();

  EXPECT_TRUE(oj::TestCase::DeleteFromDB(c, tcid));
  EXPECT_EQ(QueryScalar(c,
      "SELECT COUNT(*) FROM test_cases WHERE id=" + std::to_string(tcid)), "0");

  EXPECT_TRUE(oj::Problem::DeleteFromDB(c, pid));
}

// =============================================================================
// User::SaveToDB (INSERT)：插入新用户
// =============================================================================
TEST_F(ModelCrudTest, UserSaveInsert) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::User u;
  u.set_username("UT_CRUD_UserInsert");
  u.set_password("hashed_pwd_123");
  u.set_role(oj::Role::User);
  ASSERT_TRUE(u.SaveToDB(c));
  EXPECT_GT(u.id(), 0);

  // 清理
  EXPECT_TRUE(oj::User::DeleteFromDB(c, u.id()));
}

// =============================================================================
// User::LoadFromDB：加载存在的用户
// =============================================================================
TEST_F(ModelCrudTest, UserLoadExisting) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::User u;
  u.set_username("UT_CRUD_UserLoad");
  u.set_password("pwd_hash");
  u.set_role(oj::Role::Admin);
  ASSERT_TRUE(u.SaveToDB(c));
  int uid = u.id();

  oj::User loaded;
  EXPECT_TRUE(loaded.LoadFromDB(c, uid));
  EXPECT_EQ(loaded.id(), uid);
  EXPECT_EQ(loaded.username(), "UT_CRUD_UserLoad");
  EXPECT_EQ(loaded.password(), "pwd_hash");
  EXPECT_EQ(loaded.role(), oj::Role::Admin);
  EXPECT_TRUE(loaded.IsAdmin());
  EXPECT_FALSE(loaded.created_at().empty());

  EXPECT_TRUE(oj::User::DeleteFromDB(c, uid));
}

// =============================================================================
// User::SaveToDB (UPDATE)：更新用户信息
// =============================================================================
TEST_F(ModelCrudTest, UserSaveUpdate) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::User u;
  u.set_username("UT_CRUD_UserUpdate");
  u.set_password("old_pwd");
  u.set_role(oj::Role::User);
  ASSERT_TRUE(u.SaveToDB(c));
  int uid = u.id();

  u.set_password("new_pwd");
  u.set_role(oj::Role::Admin);
  EXPECT_TRUE(u.SaveToDB(c));

  oj::User loaded;
  ASSERT_TRUE(loaded.LoadFromDB(c, uid));
  EXPECT_EQ(loaded.password(), "new_pwd");
  EXPECT_EQ(loaded.role(), oj::Role::Admin);
  EXPECT_TRUE(loaded.IsAdmin());

  EXPECT_TRUE(oj::User::DeleteFromDB(c, uid));
}

// =============================================================================
// User::LoadFromDB：不存在返回 false
// =============================================================================
TEST_F(ModelCrudTest, UserLoadNonExistentReturnsFalse) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());

  oj::User u;
  EXPECT_FALSE(u.LoadFromDB(g.Get(), 9999999));
}

// =============================================================================
// User::DeleteFromDB
// =============================================================================
TEST_F(ModelCrudTest, UserDelete) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::User u;
  u.set_username("UT_CRUD_UserDelete");
  u.set_password("x");
  ASSERT_TRUE(u.SaveToDB(c));
  int uid = u.id();

  EXPECT_TRUE(oj::User::DeleteFromDB(c, uid));
  EXPECT_EQ(QueryScalar(c,
      "SELECT COUNT(*) FROM users WHERE id=" + std::to_string(uid)), "0");
}

// =============================================================================
// User::SaveToDB：含特殊字符的用户名（SQL 转义）
// =============================================================================
TEST_F(ModelCrudTest, UserSaveWithSpecialChars) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  oj::User u;
  u.set_username("UT_CRUD_Quote'Name");
  u.set_password("p'\"wd");
  u.set_role(oj::Role::User);
  ASSERT_TRUE(u.SaveToDB(c));
  int uid = u.id();

  oj::User loaded;
  ASSERT_TRUE(loaded.LoadFromDB(c, uid));
  EXPECT_EQ(loaded.username(), "UT_CRUD_Quote'Name");
  EXPECT_EQ(loaded.password(), "p'\"wd");

  EXPECT_TRUE(oj::User::DeleteFromDB(c, uid));
}

// =============================================================================
// 完整 CRUD 循环：插入 -> 加载 -> 更新 -> 加载 -> 删除 -> 加载失败
// =============================================================================
TEST_F(ModelCrudTest, ProblemFullCrudCycle) {
  auto g = Conn();
  ASSERT_TRUE(g.Valid());
  MYSQL* c = g.Get();

  // INSERT
  oj::Problem p;
  p.set_title("UT_CRUD_FullCycle");
  p.set_difficulty(oj::Difficulty::Easy);
  p.set_content("完整循环测试");
  p.set_tpl("template");
  ASSERT_TRUE(p.SaveToDB(c));
  int pid = p.id();
  ASSERT_GT(pid, 0);

  // LOAD
  oj::Problem loaded1;
  ASSERT_TRUE(loaded1.LoadFromDB(c, pid));
  EXPECT_EQ(loaded1.title(), "UT_CRUD_FullCycle");

  // UPDATE
  loaded1.set_title("UT_CRUD_FullCycle_Updated");
  loaded1.set_difficulty(oj::Difficulty::Hard);
  EXPECT_TRUE(loaded1.SaveToDB(c));

  // LOAD 验证更新
  oj::Problem loaded2;
  ASSERT_TRUE(loaded2.LoadFromDB(c, pid));
  EXPECT_EQ(loaded2.title(), "UT_CRUD_FullCycle_Updated");
  EXPECT_EQ(loaded2.difficulty(), oj::Difficulty::Hard);

  // DELETE
  EXPECT_TRUE(oj::Problem::DeleteFromDB(c, pid));

  // LOAD 失败
  oj::Problem loaded3;
  EXPECT_FALSE(loaded3.LoadFromDB(c, pid));
}
