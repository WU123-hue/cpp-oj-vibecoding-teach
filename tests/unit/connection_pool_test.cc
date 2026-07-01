#include "db/connection_pool.h"
#include "utils/logger.h"

#include <gtest/gtest.h>
#include <mysql/mysql.h>

#include <atomic>
#include <thread>
#include <vector>

namespace {

// 连接参数（对应 SPEC.md 5.1 数据库访问方式）
constexpr const char* kHost     = "localhost";
constexpr const int   kPort     = 3306;
constexpr const char* kUser     = "root";
constexpr const char* kPassword = "";
constexpr const char* kDatabase = "oj_db";
constexpr const int   kPoolSize = 4;

oj::ConnectionPool& Pool() {
  return oj::ConnectionPool::Instance();
}

// 执行一条 SQL，返回是否成功
bool ExecSql(MYSQL* conn, const std::string& sql) {
  return ::mysql_query(conn, sql.c_str()) == 0;
}

// 查询单值（第一条记录第一列），失败返回空串
std::string QueryScalar(MYSQL* conn, const std::string& sql) {
  if (::mysql_query(conn, sql.c_str()) != 0) return "";
  MYSQL_RES* res = ::mysql_store_result(conn);
  if (res == nullptr) return "";
  MYSQL_ROW row = ::mysql_fetch_row(res);
  std::string val = (row && row[0]) ? row[0] : "";
  ::mysql_free_result(res);
  return val;
}

}  // namespace

// =============================================================================
// 全局环境：每个测试套件前后初始化/销毁连接池
// =============================================================================
class ConnectionPoolTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    oj::Logger::Instance().Init("warn", "/tmp/oj_pool_ut", "pool.log");
    ASSERT_TRUE(Pool().Init(kHost, kPort, kUser, kPassword, kDatabase, kPoolSize));
  }
  static void TearDownTestSuite() {
    Pool().Destroy();
  }

  void SetUp() override {
    // 每个用例开始前确认所有连接已归还
    ASSERT_EQ(Pool().FreeCount(), Pool().TotalCount());
  }
};

// =============================================================================
// 初始化：连接数正确
// =============================================================================
TEST_F(ConnectionPoolTest, InitCreatesCorrectConnections) {
  EXPECT_EQ(Pool().TotalCount(), kPoolSize);
  EXPECT_EQ(Pool().FreeCount(), kPoolSize);
}

// =============================================================================
// 重复 Init 不崩溃且保持原状态
// =============================================================================
TEST_F(ConnectionPoolTest, DoubleInitIsSafe) {
  bool ok = Pool().Init(kHost, kPort, kUser, kPassword, kDatabase, kPoolSize);
  EXPECT_TRUE(ok);  // 已初始化直接返回 true
  EXPECT_EQ(Pool().TotalCount(), kPoolSize);
}

// =============================================================================
// 借出后 free 减一，归还后恢复
// =============================================================================
TEST_F(ConnectionPoolTest, BorrowAndRelease) {
  int initial_free = Pool().FreeCount();

  MYSQL* conn = Pool().GetConnection();
  ASSERT_NE(conn, nullptr);
  EXPECT_EQ(Pool().FreeCount(), initial_free - 1);

  Pool().ReleaseConnection(conn);
  EXPECT_EQ(Pool().FreeCount(), initial_free);
}

// =============================================================================
// ConnectionGuard RAII：析构自动归还
// =============================================================================
TEST_F(ConnectionPoolTest, GuardAutoReleaseOnDestruct) {
  int initial_free = Pool().FreeCount();
  {
    oj::ConnectionGuard g;
    EXPECT_TRUE(g.Valid());
    EXPECT_EQ(Pool().FreeCount(), initial_free - 1);
  }
  EXPECT_EQ(Pool().FreeCount(), initial_free);
}

// =============================================================================
// ConnectionGuard 的 bool 转换与 Get
// =============================================================================
TEST_F(ConnectionPoolTest, GuardBoolConversionAndGet) {
  oj::ConnectionGuard g;
  EXPECT_TRUE(g.Valid());
  EXPECT_TRUE(static_cast<bool>(g));
  EXPECT_NE(g.Get(), nullptr);
}

// =============================================================================
// ConnectionGuard 移动构造：所有权转移，原对象失效
// =============================================================================
TEST_F(ConnectionPoolTest, GuardMoveConstructor) {
  int initial_free = Pool().FreeCount();
  oj::ConnectionGuard g1;
  ASSERT_TRUE(g1.Valid());
  MYSQL* raw = g1.Get();

  oj::ConnectionGuard g2(std::move(g1));
  EXPECT_FALSE(g1.Valid());          // 原对象失效
  EXPECT_TRUE(g2.Valid());           // 新对象持有
  EXPECT_EQ(g2.Get(), raw);          // 同一句柄
  EXPECT_EQ(Pool().FreeCount(), initial_free - 1);  // 仍借出一条
}

// =============================================================================
// ConnectionGuard 移动赋值
// =============================================================================
TEST_F(ConnectionPoolTest, GuardMoveAssignment) {
  oj::ConnectionGuard g1;
  oj::ConnectionGuard g2;
  ASSERT_TRUE(g1.Valid());
  ASSERT_TRUE(g2.Valid());
  int after_borrow = Pool().FreeCount();

  g2 = std::move(g1);
  EXPECT_FALSE(g1.Valid());
  EXPECT_TRUE(g2.Valid());
  // g2 原连接被归还 (free+1)，g1 连接转移到 g2
  EXPECT_EQ(Pool().FreeCount(), after_borrow + 1);
}

// =============================================================================
// 借出连接可执行真实 SQL 查询
// =============================================================================
TEST_F(ConnectionPoolTest, BorrowedConnectionCanQuery) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  std::string val = QueryScalar(g.Get(), "SELECT COUNT(*) FROM users");
  EXPECT_FALSE(val.empty());
  // init.sql 预置了 admin 账号
  int count = std::stoi(val);
  EXPECT_GE(count, 1);
}

// =============================================================================
// 借出连接可执行 DML 并回滚
// =============================================================================
TEST_F(ConnectionPoolTest, BorrowedConnectionCanExecuteDml) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());
  ASSERT_TRUE(ExecSql(g.Get(), "BEGIN"));
  ASSERT_TRUE(ExecSql(g.Get(),
      "INSERT INTO users(username,password,role) VALUES('ut_tmp','x','user')"));
  std::string val = QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM users WHERE username='ut_tmp'");
  EXPECT_EQ(val, "1");
  ASSERT_TRUE(ExecSql(g.Get(), "ROLLBACK"));
  val = QueryScalar(g.Get(),
      "SELECT COUNT(*) FROM users WHERE username='ut_tmp'");
  EXPECT_EQ(val, "0");
}

// =============================================================================
// 池空时阻塞等待：其他线程归还后唤醒
// =============================================================================
TEST_F(ConnectionPoolTest, BlocksWhenPoolEmpty) {
  // 借空所有连接
  std::vector<MYSQL*> held;
  for (int i = 0; i < kPoolSize; ++i) {
    held.push_back(Pool().GetConnection());
  }
  EXPECT_EQ(Pool().FreeCount(), 0);

  std::atomic<bool> got{false};
  // 另一线程尝试借出应阻塞
  std::thread t([&got]() {
    MYSQL* conn = Pool().GetConnection(3000);
    if (conn) {
      got = true;
      Pool().ReleaseConnection(conn);
    }
  });

  // 等待确保另一线程已阻塞
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_FALSE(got.load());

  // 归还一条，唤醒等待线程
  Pool().ReleaseConnection(held.back());
  held.pop_back();
  t.join();
  EXPECT_TRUE(got.load());

  // 清理剩余
  for (MYSQL* c : held) Pool().ReleaseConnection(c);
}

// =============================================================================
// 池空且超时应返回 nullptr
// =============================================================================
TEST_F(ConnectionPoolTest, ReturnsNullptrOnTimeout) {
  std::vector<MYSQL*> held;
  for (int i = 0; i < kPoolSize; ++i) {
    held.push_back(Pool().GetConnection());
  }
  EXPECT_EQ(Pool().FreeCount(), 0);

  auto start = std::chrono::steady_clock::now();
  MYSQL* conn = Pool().GetConnection(300);  // 300ms 超时
  auto end = std::chrono::steady_clock::now();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        end - start).count();

  EXPECT_EQ(conn, nullptr);
  EXPECT_GE(elapsed_ms, 250);   // 至少等待了 ~300ms
  EXPECT_LE(elapsed_ms, 1500);  // 但不应远超超时

  for (MYSQL* c : held) Pool().ReleaseConnection(c);
}

// =============================================================================
// 多线程并发借出归还：总数守恒，无错乱
// =============================================================================
TEST_F(ConnectionPoolTest, ConcurrentBorrowRelease) {
  const int thread_count = 8;
  const int per_thread = 20;
  std::atomic<int> success{0};

  std::vector<std::thread> threads;
  for (int t = 0; t < thread_count; ++t) {
    threads.emplace_back([&success]() {
      for (int i = 0; i < per_thread; ++i) {
        oj::ConnectionGuard g(5000);
        if (g.Valid()) {
          // 执行一次轻量查询确认连接可用
          if (::mysql_query(g.Get(), "SELECT 1") == 0) {
            MYSQL_RES* res = ::mysql_store_result(g.Get());
            if (res) ::mysql_free_result(res);
            success.fetch_add(1);
          }
        }
      }
    });
  }
  for (auto& th : threads) th.join();

  EXPECT_EQ(success.load(), thread_count * per_thread);
  EXPECT_EQ(Pool().FreeCount(), Pool().TotalCount());
}

// =============================================================================
// 连接可复用：同一连接多次查询
// =============================================================================
TEST_F(ConnectionPoolTest, ConnectionReusableAcrossQueries) {
  oj::ConnectionGuard g;
  ASSERT_TRUE(g.Valid());

  for (int i = 0; i < 5; ++i) {
    std::string val = QueryScalar(g.Get(), "SELECT 1");
    EXPECT_EQ(val, "1");
  }
}

// =============================================================================
// 归还 nullptr 不崩溃、不影响计数
// =============================================================================
TEST_F(ConnectionPoolTest, ReleaseNullptrIsSafe) {
  int initial_free = Pool().FreeCount();
  Pool().ReleaseConnection(nullptr);
  EXPECT_EQ(Pool().FreeCount(), initial_free);
}

// =============================================================================
// Destroy 后状态清零（独立测试，避免影响 fixture 套件状态）
// =============================================================================
TEST(ConnectionPoolDestroyTest, DestroyClearsAll) {
  auto& pool = oj::ConnectionPool::Instance();
  ASSERT_TRUE(pool.Init(kHost, kPort, kUser, kPassword, kDatabase, 2));
  EXPECT_EQ(pool.TotalCount(), 2);
  pool.Destroy();
  EXPECT_EQ(pool.TotalCount(), 0);
  EXPECT_EQ(pool.FreeCount(), 0);
}

// =============================================================================
// Destroy 后可重新 Init
// =============================================================================
TEST(ConnectionPoolDestroyTest, ReinitAfterDestroy) {
  auto& pool = oj::ConnectionPool::Instance();
  pool.Destroy();
  ASSERT_TRUE(pool.Init(kHost, kPort, kUser, kPassword, kDatabase, 3));
  EXPECT_EQ(pool.TotalCount(), 3);
  EXPECT_EQ(pool.FreeCount(), 3);
  pool.Destroy();
}
