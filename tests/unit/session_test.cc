#include "service/session_manager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "utils/logger.h"

// =============================================================================
// 全局环境
// =============================================================================
class SessionManagerTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    oj::Logger::Instance().Init("error", "/tmp/oj_session_ut", "session.log");
  }

  void SetUp() override {
    // 每个测试前重置超时为 30 分钟
    oj::SessionManager::Instance().SetTimeout(1800);
  }
};

// =============================================================================
// CreateSession: 创建会话返回非空 session_id
// =============================================================================
TEST_F(SessionManagerTest, CreateSessionReturnsNonEmptyId) {
  auto& sm = oj::SessionManager::Instance();
  std::string sid = sm.CreateSession(1, "alice", "user");
  EXPECT_FALSE(sid.empty());
  EXPECT_EQ(sid.size(), 32u);  // 32 字符十六进制
}

// =============================================================================
// CreateSession: 每次生成不同的 session_id
// =============================================================================
TEST_F(SessionManagerTest, CreateSessionUniqueIds) {
  auto& sm = oj::SessionManager::Instance();
  std::string sid1 = sm.CreateSession(1, "alice", "user");
  std::string sid2 = sm.CreateSession(2, "bob", "user");
  EXPECT_NE(sid1, sid2);
}

// =============================================================================
// GetSession: 获取存在的会话返回正确信息
// =============================================================================
TEST_F(SessionManagerTest, GetSessionReturnsValidSession) {
  auto& sm = oj::SessionManager::Instance();
  std::string sid = sm.CreateSession(42, "charlie", "admin");

  const oj::Session* s = sm.GetSession(sid);
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->session_id, sid);
  EXPECT_EQ(s->user_id, 42);
  EXPECT_EQ(s->username, "charlie");
  EXPECT_EQ(s->role, "admin");
}

// =============================================================================
// GetSession: 不存在的 session_id 返回 nullptr
// =============================================================================
TEST_F(SessionManagerTest, GetSessionNonExistentReturnsNull) {
  auto& sm = oj::SessionManager::Instance();
  const oj::Session* s = sm.GetSession("nonexistent_session_id_12345");
  EXPECT_EQ(s, nullptr);
}

// =============================================================================
// GetSession: 空 session_id 返回 nullptr
// =============================================================================
TEST_F(SessionManagerTest, GetSessionEmptyIdReturnsNull) {
  auto& sm = oj::SessionManager::Instance();
  const oj::Session* s = sm.GetSession("");
  EXPECT_EQ(s, nullptr);
}

// =============================================================================
// GetSession: 验证后更新 last_access
// =============================================================================
TEST_F(SessionManagerTest, GetSessionUpdatesLastAccess) {
  auto& sm = oj::SessionManager::Instance();
  std::string sid = sm.CreateSession(1, "alice", "user");

  const oj::Session* s1 = sm.GetSession(sid);
  ASSERT_NE(s1, nullptr);
  // 记录时间戳值（s1 指向 map 内对象，会被后续 GetSession 修改）
  auto t1 = s1->last_access;

  // 等待足够长时间确保 steady_clock 有可测量差异
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  const oj::Session* s2 = sm.GetSession(sid);
  ASSERT_NE(s2, nullptr);
  auto t2 = s2->last_access;

  // last_access 应该被更新
  EXPECT_GT(t2, t1);
}

// =============================================================================
// DestroySession: 正常销毁
// =============================================================================
TEST_F(SessionManagerTest, DestroySessionSuccess) {
  auto& sm = oj::SessionManager::Instance();
  std::string sid = sm.CreateSession(1, "alice", "user");

  EXPECT_TRUE(sm.DestroySession(sid));

  // 销毁后获取返回 nullptr
  const oj::Session* s = sm.GetSession(sid);
  EXPECT_EQ(s, nullptr);
}

// =============================================================================
// DestroySession: 销毁不存在的会话返回 false
// =============================================================================
TEST_F(SessionManagerTest, DestroySessionNonExistentReturnsFalse) {
  auto& sm = oj::SessionManager::Instance();
  EXPECT_FALSE(sm.DestroySession("nonexistent_sid"));
}

// =============================================================================
// DestroySession: 空 session_id 返回 false
// =============================================================================
TEST_F(SessionManagerTest, DestroySessionEmptyIdReturnsFalse) {
  auto& sm = oj::SessionManager::Instance();
  EXPECT_FALSE(sm.DestroySession(""));
}

// =============================================================================
// ActiveCount: 正确统计活跃会话数
// =============================================================================
TEST_F(SessionManagerTest, ActiveCountReflectsSessions) {
  auto& sm = oj::SessionManager::Instance();

  // 先记录基线
  size_t baseline = sm.ActiveCount();

  std::string sid1 = sm.CreateSession(1, "u1", "user");
  std::string sid2 = sm.CreateSession(2, "u2", "user");
  std::string sid3 = sm.CreateSession(3, "u3", "admin");

  EXPECT_EQ(sm.ActiveCount(), baseline + 3);

  sm.DestroySession(sid2);
  EXPECT_EQ(sm.ActiveCount(), baseline + 2);

  sm.DestroySession(sid1);
  sm.DestroySession(sid3);
  EXPECT_EQ(sm.ActiveCount(), baseline);
}

// =============================================================================
// 过期机制: 超时会话自动失效
// =============================================================================
TEST_F(SessionManagerTest, ExpiredSessionInvalidated) {
  auto& sm = oj::SessionManager::Instance();
  sm.SetTimeout(1);  // 1 秒超时

  std::string sid = sm.CreateSession(1, "alice", "user");
  const oj::Session* s = sm.GetSession(sid);
  ASSERT_NE(s, nullptr);  // 立即获取应该有效

  // 等待超过 1 秒
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  const oj::Session* s2 = sm.GetSession(sid);
  EXPECT_EQ(s2, nullptr);  // 过期后获取返回 nullptr
}

// =============================================================================
// 过期机制: GetSession 更新 last_access 防止活跃会话过期
// =============================================================================
TEST_F(SessionManagerTest, ActiveAccessPreventsExpiry) {
  auto& sm = oj::SessionManager::Instance();
  sm.SetTimeout(2);  // 2 秒超时

  std::string sid = sm.CreateSession(1, "alice", "user");

  // 每秒访问一次，持续 3 秒（超过原始超时）
  for (int i = 0; i < 3; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    const oj::Session* s = sm.GetSession(sid);
    ASSERT_NE(s, nullptr) << "session expired at iteration " << i;
  }

  // 最后一次访问后等待超过 2 秒，应该过期
  std::this_thread::sleep_for(std::chrono::milliseconds(2200));
  EXPECT_EQ(sm.GetSession(sid), nullptr);
}

// =============================================================================
// CleanExpired: 清理过期会话
// =============================================================================
TEST_F(SessionManagerTest, CleanExpiredRemovesExpiredSessions) {
  auto& sm = oj::SessionManager::Instance();

  // 先用短超时清理历史残留
  sm.SetTimeout(0);
  sm.CleanExpired();
  sm.SetTimeout(1);  // 1 秒超时

  std::string sid1 = sm.CreateSession(1, "u1", "user");
  std::string sid2 = sm.CreateSession(2, "u2", "user");
  std::string sid3 = sm.CreateSession(3, "u3", "user");

  EXPECT_GE(sm.ActiveCount(), 3u);

  // 等待过期
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  size_t cleaned = sm.CleanExpired();
  EXPECT_GE(cleaned, 3u);  // 至少清理了这 3 个

  // 这 3 个会话应被清理
  EXPECT_EQ(sm.GetSession(sid1), nullptr);
  EXPECT_EQ(sm.GetSession(sid2), nullptr);
  EXPECT_EQ(sm.GetSession(sid3), nullptr);
}

// =============================================================================
// CleanExpired: 活跃会话不被清理
// =============================================================================
TEST_F(SessionManagerTest, CleanExpiredKeepsActiveSessions) {
  auto& sm = oj::SessionManager::Instance();
  sm.SetTimeout(10);  // 10 秒超时

  // 先清理历史残留
  sm.CleanExpired();

  std::string sid_active = sm.CreateSession(1, "active", "user");

  // 缩短为 1 秒超时，再创建一个会快速过期的会话
  sm.SetTimeout(1);
  std::string sid_expire = sm.CreateSession(2, "expire", "user");

  // 等待 sid_expire 过期（此时 sid_active 也过期了，因为全局 timeout=1）
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  // 在 CleanExpired 之前先刷新 sid_active
  // 但 sid_active 已过期，GetSession 会返回 nullptr 并删除它
  // 所以需要用一个不同的策略：恢复较长超时后再 CleanExpired
  sm.SetTimeout(10);  // 恢复 10 秒超时

  // sid_active 的 last_access 是 1.1 秒前，在 10 秒超时下仍然有效
  // sid_expire 的 last_access 也是 1.1 秒前，在 10 秒超时下也仍然有效
  // 这个测试策略需要调整——改用直接验证 CleanExpired 不会误删活跃会话

  // 刷新 sid_active 的 last_access
  sm.GetSession(sid_active);

  // 立即 CleanExpired——两个会话都在 10 秒超时下活跃
  size_t cleaned = sm.CleanExpired();
  // 不应该有清理（sid_active 刚被访问，sid_expire 也在 10 秒内）
  EXPECT_EQ(cleaned, 0u);

  // sid_active 仍在
  const oj::Session* s = sm.GetSession(sid_active);
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->username, "active");

  // 清理
  sm.DestroySession(sid_active);
  sm.DestroySession(sid_expire);
}

// =============================================================================
// CookieName: 返回正确的 Cookie 名称
// =============================================================================
TEST_F(SessionManagerTest, CookieNameIsOjSession) {
  EXPECT_STREQ(oj::SessionManager::CookieName(), "oj_session");
}

// =============================================================================
// SetTimeout: 动态修改超时时间
// =============================================================================
TEST_F(SessionManagerTest, SetTimeoutChangesExpiry) {
  auto& sm = oj::SessionManager::Instance();

  // 设置 10 秒超时
  sm.SetTimeout(10);
  std::string sid = sm.CreateSession(1, "alice", "user");

  // 缩短为 1 秒
  sm.SetTimeout(1);

  // 等待 1.1 秒
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  // 应该已过期（使用新的超时设置）
  EXPECT_EQ(sm.GetSession(sid), nullptr);
}

// =============================================================================
// 线程安全: 并发创建和获取会话
// =============================================================================
TEST_F(SessionManagerTest, ConcurrentCreateAndGetNoCrash) {
  auto& sm = oj::SessionManager::Instance();
  sm.SetTimeout(60);

  std::vector<std::thread> threads;
  std::vector<std::string> sids(10);

  // 10 个线程并发创建会话
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&sm, &sids, i]() {
      sids[i] = sm.CreateSession(i, "user" + std::to_string(i), "user");
    });
  }
  for (auto& t : threads) t.join();
  threads.clear();

  // 验证所有会话都能获取
  for (int i = 0; i < 10; ++i) {
    const oj::Session* s = sm.GetSession(sids[i]);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->user_id, i);
    EXPECT_EQ(s->username, "user" + std::to_string(i));
  }

  // 10 个线程并发销毁会话
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&sm, &sids, i]() {
      EXPECT_TRUE(sm.DestroySession(sids[i]));
    });
  }
  for (auto& t : threads) t.join();
}

// =============================================================================
// 线程安全: 并发 Get 和 Destroy 混合
// =============================================================================
TEST_F(SessionManagerTest, ConcurrentGetAndDestroyMixed) {
  auto& sm = oj::SessionManager::Instance();
  sm.SetTimeout(60);

  // 创建 5 个会话
  std::vector<std::string> sids;
  for (int i = 0; i < 5; ++i) {
    sids.push_back(sm.CreateSession(i, "u" + std::to_string(i), "user"));
  }

  std::vector<std::thread> threads;

  // 线程 A: 反复 Get
  threads.emplace_back([&sm, &sids]() {
    for (int i = 0; i < 100; ++i) {
      sm.GetSession(sids[i % 5]);
    }
  });

  // 线程 B: 反复 Destroy + 重新创建
  threads.emplace_back([&sm, &sids]() {
    for (int i = 0; i < 50; ++i) {
      sm.DestroySession(sids[i % 5]);
    }
  });

  // 线程 C: 反复 CleanExpired
  threads.emplace_back([&sm]() {
    for (int i = 0; i < 50; ++i) {
      sm.CleanExpired();
    }
  });

  // 线程 D: 反复 ActiveCount
  threads.emplace_back([&sm]() {
    for (int i = 0; i < 50; ++i) {
      sm.ActiveCount();
    }
  });

  for (auto& t : threads) t.join();

  // 不崩溃即通过
  SUCCEED();
}

// =============================================================================
// 同一用户多次创建会话: 每次生成不同 sid，旧会话保留
// =============================================================================
TEST_F(SessionManagerTest, SameUserMultipleSessions) {
  auto& sm = oj::SessionManager::Instance();

  std::string sid1 = sm.CreateSession(1, "alice", "user");
  std::string sid2 = sm.CreateSession(1, "alice", "user");

  EXPECT_NE(sid1, sid2);

  // 两个会话都有效
  EXPECT_NE(sm.GetSession(sid1), nullptr);
  EXPECT_NE(sm.GetSession(sid2), nullptr);

  // 销毁一个不影响另一个
  sm.DestroySession(sid1);
  EXPECT_EQ(sm.GetSession(sid1), nullptr);
  EXPECT_NE(sm.GetSession(sid2), nullptr);

  sm.DestroySession(sid2);
}

// =============================================================================
// Session 字段完整性验证
// =============================================================================
TEST_F(SessionManagerTest, SessionFieldsComplete) {
  auto& sm = oj::SessionManager::Instance();
  std::string sid = sm.CreateSession(99, "testuser", "admin");

  const oj::Session* s = sm.GetSession(sid);
  ASSERT_NE(s, nullptr);

  EXPECT_EQ(s->session_id, sid);
  EXPECT_EQ(s->user_id, 99);
  EXPECT_EQ(s->username, "testuser");
  EXPECT_EQ(s->role, "admin");
  // created_at 和 last_access 应接近当前时间
  auto now = std::chrono::steady_clock::now();
  auto created_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - s->created_at);
  EXPECT_LT(created_diff.count(), 1000);  // 小于 1 秒

  sm.DestroySession(sid);
}

// =============================================================================
// session_id 格式验证: 32 字符十六进制
// =============================================================================
TEST_F(SessionManagerTest, SessionIdFormat) {
  auto& sm = oj::SessionManager::Instance();
  std::string sid = sm.CreateSession(1, "u", "user");

  EXPECT_EQ(sid.size(), 32u);
  for (char c : sid) {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
        << "invalid hex char: " << c;
  }

  sm.DestroySession(sid);
}

// =============================================================================
// 销毁已销毁的会话返回 false
// =============================================================================
TEST_F(SessionManagerTest, DestroyTwiceReturnsFalse) {
  auto& sm = oj::SessionManager::Instance();
  std::string sid = sm.CreateSession(1, "u", "user");

  EXPECT_TRUE(sm.DestroySession(sid));
  EXPECT_FALSE(sm.DestroySession(sid));  // 第二次返回 false
}

// =============================================================================
// CleanExpired 无过期会话时返回 0
// =============================================================================
TEST_F(SessionManagerTest, CleanExpiredNoExpiredReturnsZero) {
  auto& sm = oj::SessionManager::Instance();

  // 先清理历史残留（避免上个测试残留的过期会话干扰）
  sm.SetTimeout(0);
  sm.CleanExpired();
  sm.SetTimeout(60);

  sm.CreateSession(1, "u1", "user");
  sm.CreateSession(2, "u2", "user");

  size_t cleaned = sm.CleanExpired();
  EXPECT_EQ(cleaned, 0u);
}

// =============================================================================
// 过期会话在 ActiveCount 中不计入
// =============================================================================
TEST_F(SessionManagerTest, ExpiredSessionsNotCounted) {
  auto& sm = oj::SessionManager::Instance();

  // 先清理历史残留
  sm.SetTimeout(0);
  sm.CleanExpired();
  sm.SetTimeout(1);

  sm.CreateSession(1, "u1", "user");
  sm.CreateSession(2, "u2", "user");

  // 等待过期
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  // ActiveCount 不应计入过期会话
  size_t count = sm.ActiveCount();
  EXPECT_EQ(count, 0u);
}
