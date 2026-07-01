#include "db/connection_pool.h"
#include "model/mapper.h"
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
  if (::mysql_query(c, sql.c_str()) != 0) {
    return false;
  }
  return true;
}

// 执行查询并返回结果集（调用方负责 mysql_free_result）
MYSQL_RES* RunQuery(MYSQL* c, const std::string& sql) {
  if (::mysql_query(c, sql.c_str()) != 0) return nullptr;
  return ::mysql_store_result(c);
}

}  // namespace

// =============================================================================
// 全局环境：套件级初始化连接池，并准备测试数据
// =============================================================================
class MapperTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    oj::Logger::Instance().Init("error", "/tmp/oj_mapper_ut", "m.log");
    ASSERT_TRUE(Pool().Init(kHost, kPort, kUser, kPassword, kDatabase, 2));
    PrepareTestData();
  }

  static void TearDownTestSuite() {
    CleanupTestData();
    Pool().Destroy();
  }

  static oj::ConnectionGuard GetConn() {
    return oj::ConnectionGuard();
  }

  static void PrepareTestData() {
    oj::ConnectionGuard g;
    ASSERT_TRUE(g.Valid());
    MYSQL* c = g.Get();

    // 清理可能的残留
    ExecSql(c, "DELETE FROM problems WHERE title LIKE 'UT_%'");

    // 插入测试题目
    ASSERT_TRUE(ExecSql(c, "BEGIN"));
    ASSERT_TRUE(ExecSql(c,
        "INSERT INTO problems(title,difficulty,content,template) "
        "VALUES('UT_MapTest1','Easy','输入两个整数求和','template1')"));
    test_pid_ = ::mysql_insert_id(c);
    ASSERT_TRUE(ExecSql(c,
        "INSERT INTO problems(title,difficulty,content,template) "
        "VALUES('UT_MapTest2','Hard','反转链表','template2')"));

    // 插入测试用例
    ASSERT_TRUE(ExecSql(c,
        "INSERT INTO test_cases(problem_id,input,expected,position) "
        "VALUES(" + std::to_string(test_pid_) + ",'1 2','3',0),"
        "(" + std::to_string(test_pid_) + ",'10 20','30',1)"));
    ASSERT_TRUE(ExecSql(c, "COMMIT"));
  }

  static void CleanupTestData() {
    oj::ConnectionGuard g;
    if (!g.Valid()) return;
    ExecSql(g.Get(), "DELETE FROM problems WHERE title LIKE 'UT_%'");
  }

  static long test_pid_;
};

long MapperTest::test_pid_ = 0;

// =============================================================================
// MapProblems：正确映射多条题目
// =============================================================================
TEST_F(MapperTest, MapProblemsReturnsAllRows) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT id,title,difficulty,content,template,created_at "
      "FROM problems WHERE title LIKE 'UT_%' ORDER BY id");
  ASSERT_NE(res, nullptr);

  auto probs = oj::MapProblems(res);
  ::mysql_free_result(res);

  ASSERT_EQ(probs.size(), 2u);
  EXPECT_EQ(probs[0].title(), "UT_MapTest1");
  EXPECT_EQ(probs[1].title(), "UT_MapTest2");
}

// =============================================================================
// MapProblems：字段值正确
// =============================================================================
TEST_F(MapperTest, MapProblemsFieldValues) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT id,title,difficulty,content,template,created_at "
      "FROM problems WHERE id=" + std::to_string(test_pid_));
  ASSERT_NE(res, nullptr);

  auto probs = oj::MapProblems(res);
  ::mysql_free_result(res);

  ASSERT_EQ(probs.size(), 1u);
  auto& p = probs[0];
  EXPECT_EQ(p.id(), test_pid_);
  EXPECT_EQ(p.title(), "UT_MapTest1");
  EXPECT_EQ(p.difficulty(), oj::Difficulty::Easy);
  EXPECT_EQ(p.content(), "输入两个整数求和");
  EXPECT_EQ(p.tpl(), "template1");
  EXPECT_FALSE(p.created_at().empty());
}

// =============================================================================
// MapProblems：难度枚举映射（Hard）
// =============================================================================
TEST_F(MapperTest, MapProblemsDifficultyEnum) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT id,title,difficulty,content,template,created_at "
      "FROM problems WHERE title='UT_MapTest2'");
  ASSERT_NE(res, nullptr);

  auto probs = oj::MapProblems(res);
  ::mysql_free_result(res);

  ASSERT_EQ(probs.size(), 1u);
  EXPECT_EQ(probs[0].difficulty(), oj::Difficulty::Hard);
  EXPECT_EQ(oj::DifficultyToStr(probs[0].difficulty()), "Hard");
}

// =============================================================================
// MapProblems：列顺序无关（按列名匹配）
// =============================================================================
TEST_F(MapperTest, MapProblemsColumnOrderIndependent) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  // 打乱列顺序
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT created_at,template,content,difficulty,title,id "
      "FROM problems WHERE id=" + std::to_string(test_pid_));
  ASSERT_NE(res, nullptr);

  auto probs = oj::MapProblems(res);
  ::mysql_free_result(res);

  ASSERT_EQ(probs.size(), 1u);
  EXPECT_EQ(probs[0].id(), test_pid_);
  EXPECT_EQ(probs[0].title(), "UT_MapTest1");
  EXPECT_EQ(probs[0].difficulty(), oj::Difficulty::Easy);
  EXPECT_EQ(probs[0].content(), "输入两个整数求和");
}

// =============================================================================
// MapProblems：空结果集返回空 vector
// =============================================================================
TEST_F(MapperTest, MapProblemsEmptyResultSet) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT id,title,difficulty,content,template,created_at "
      "FROM problems WHERE id=9999999");
  ASSERT_NE(res, nullptr);

  auto probs = oj::MapProblems(res);
  ::mysql_free_result(res);

  EXPECT_TRUE(probs.empty());
}

// =============================================================================
// MapProblems：nullptr 入参返回空
// =============================================================================
TEST_F(MapperTest, MapProblemsNullptrReturnsEmpty) {
  auto probs = oj::MapProblems(nullptr);
  EXPECT_TRUE(probs.empty());
}

// =============================================================================
// MapTestCases：正确映射并按 position 排序
// =============================================================================
TEST_F(MapperTest, MapTestCasesByPosition) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT id,problem_id,input,expected,position "
      "FROM test_cases WHERE problem_id=" + std::to_string(test_pid_) +
      " ORDER BY position");
  ASSERT_NE(res, nullptr);

  auto tcs = oj::MapTestCases(res);
  ::mysql_free_result(res);

  ASSERT_EQ(tcs.size(), 2u);
  EXPECT_EQ(tcs[0].position(), 0);
  EXPECT_EQ(tcs[0].input(), "1 2");
  EXPECT_EQ(tcs[0].expected(), "3");
  EXPECT_EQ(tcs[1].position(), 1);
  EXPECT_EQ(tcs[1].input(), "10 20");
  EXPECT_EQ(tcs[1].expected(), "30");
}

// =============================================================================
// MapTestCases：problem_id 正确
// =============================================================================
TEST_F(MapperTest, MapTestCasesProblemId) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT id,problem_id,input,expected,position "
      "FROM test_cases WHERE problem_id=" + std::to_string(test_pid_));
  ASSERT_NE(res, nullptr);

  auto tcs = oj::MapTestCases(res);
  ::mysql_free_result(res);

  for (auto& tc : tcs) {
    EXPECT_EQ(tc.problem_id(), test_pid_);
  }
}

// =============================================================================
// MapTestCases：nullptr 入参返回空
// =============================================================================
TEST_F(MapperTest, MapTestCasesNullptrReturnsEmpty) {
  auto tcs = oj::MapTestCases(nullptr);
  EXPECT_TRUE(tcs.empty());
}

// =============================================================================
// MapUsers：映射 admin 用户
// =============================================================================
TEST_F(MapperTest, MapUsersAdmin) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT id,username,password,role,created_at "
      "FROM users WHERE username='admin'");
  ASSERT_NE(res, nullptr);

  auto users = oj::MapUsers(res);
  ::mysql_free_result(res);

  ASSERT_EQ(users.size(), 1u);
  auto& u = users[0];
  EXPECT_EQ(u.username(), "admin");
  EXPECT_EQ(u.role(), oj::Role::Admin);
  EXPECT_TRUE(u.IsAdmin());
  EXPECT_FALSE(u.password().empty());
  EXPECT_FALSE(u.created_at().empty());
}

// =============================================================================
// MapUsers：role 列名匹配
// =============================================================================
TEST_F(MapperTest, MapUsersColumnOrderIndependent) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT role,created_at,password,username,id "
      "FROM users WHERE username='admin'");
  ASSERT_NE(res, nullptr);

  auto users = oj::MapUsers(res);
  ::mysql_free_result(res);

  ASSERT_EQ(users.size(), 1u);
  EXPECT_EQ(users[0].username(), "admin");
  EXPECT_EQ(users[0].role(), oj::Role::Admin);
}

// =============================================================================
// MapUsers：空结果返回空 vector
// =============================================================================
TEST_F(MapperTest, MapUsersEmptyResultSet) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT id,username,password,role,created_at "
      "FROM users WHERE username='nonexistent_user_xyz'");
  ASSERT_NE(res, nullptr);

  auto users = oj::MapUsers(res);
  ::mysql_free_result(res);

  EXPECT_TRUE(users.empty());
}

// =============================================================================
// MapScalar：COUNT 查询返回正确数值
// =============================================================================
TEST_F(MapperTest, MapScalarCount) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  MYSQL_RES* res = RunQuery(g.Get(),
      "SELECT COUNT(*) FROM test_cases WHERE problem_id=" +
      std::to_string(test_pid_));
  ASSERT_NE(res, nullptr);

  long cnt = oj::MapScalar(res, -1);
  ::mysql_free_result(res);

  EXPECT_EQ(cnt, 2);
}

// =============================================================================
// MapScalar：nullptr 返回 fallback
// =============================================================================
TEST_F(MapperTest, MapScalarNullptrReturnsFallback) {
  EXPECT_EQ(oj::MapScalar(nullptr, -1), -1);
  EXPECT_EQ(oj::MapScalar(nullptr, 0), 0);
  EXPECT_EQ(oj::MapScalar(nullptr, 99), 99);
}
