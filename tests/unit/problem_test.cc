#include "model/problem.h"
#include "model/test_case.h"
#include "model/user.h"

#include <gtest/gtest.h>

// =============================================================================
// Difficulty 枚举转换
// =============================================================================
TEST(DifficultyTest, ParseFromString) {
  EXPECT_EQ(oj::ParseDifficulty("Easy"), oj::Difficulty::Easy);
  EXPECT_EQ(oj::ParseDifficulty("Medium"), oj::Difficulty::Medium);
  EXPECT_EQ(oj::ParseDifficulty("Hard"), oj::Difficulty::Hard);
}

TEST(DifficultyTest, ParseCaseInsensitive) {
  EXPECT_EQ(oj::ParseDifficulty("easy"), oj::Difficulty::Easy);
  EXPECT_EQ(oj::ParseDifficulty("MEDIUM"), oj::Difficulty::Medium);
  EXPECT_EQ(oj::ParseDifficulty("HaRd"), oj::Difficulty::Hard);
}

TEST(DifficultyTest, ParseInvalidDefaultsToEasy) {
  EXPECT_EQ(oj::ParseDifficulty("unknown"), oj::Difficulty::Easy);
  EXPECT_EQ(oj::ParseDifficulty(""), oj::Difficulty::Easy);
  EXPECT_EQ(oj::ParseDifficulty("abc"), oj::Difficulty::Easy);
}

TEST(DifficultyTest, ToString) {
  EXPECT_EQ(oj::DifficultyToStr(oj::Difficulty::Easy), "Easy");
  EXPECT_EQ(oj::DifficultyToStr(oj::Difficulty::Medium), "Medium");
  EXPECT_EQ(oj::DifficultyToStr(oj::Difficulty::Hard), "Hard");
}

// =============================================================================
// Problem 模型 getter/setter
// =============================================================================
TEST(ProblemTest, DefaultValues) {
  oj::Problem p;
  EXPECT_EQ(p.id(), 0);
  EXPECT_EQ(p.title(), "");
  EXPECT_EQ(p.difficulty(), oj::Difficulty::Easy);
  EXPECT_EQ(p.content(), "");
  EXPECT_EQ(p.tpl(), "");
  EXPECT_EQ(p.created_at(), "");
}

TEST(ProblemTest, SetAndGet) {
  oj::Problem p;
  p.set_id(42);
  p.set_title("A+B Problem");
  p.set_difficulty(oj::Difficulty::Medium);
  p.set_content("输入两个整数求和");
  p.set_tpl("#include <iostream>\nint main(){}");
  p.set_created_at("2026-07-01 12:00:00");

  EXPECT_EQ(p.id(), 42);
  EXPECT_EQ(p.title(), "A+B Problem");
  EXPECT_EQ(p.difficulty(), oj::Difficulty::Medium);
  EXPECT_EQ(p.content(), "输入两个整数求和");
  EXPECT_EQ(p.tpl(), "#include <iostream>\nint main(){}");
  EXPECT_EQ(p.created_at(), "2026-07-01 12:00:00");
}

TEST(ProblemTest, SetAllDifficulties) {
  oj::Problem p;
  p.set_difficulty(oj::Difficulty::Easy);
  EXPECT_EQ(oj::DifficultyToStr(p.difficulty()), "Easy");
  p.set_difficulty(oj::Difficulty::Medium);
  EXPECT_EQ(oj::DifficultyToStr(p.difficulty()), "Medium");
  p.set_difficulty(oj::Difficulty::Hard);
  EXPECT_EQ(oj::DifficultyToStr(p.difficulty()), "Hard");
}

TEST(ProblemTest, ChineseContent) {
  oj::Problem p;
  p.set_title("两数之和");
  p.set_content("## 题目描述\n给定两个整数 $a, b$，输出 $a+b$。\n\n## 输入格式\n一行两个整数。");
  EXPECT_EQ(p.title(), "两数之和");
  EXPECT_NE(p.content().find("题目描述"), std::string::npos);
  EXPECT_NE(p.content().find("输入格式"), std::string::npos);
}

// =============================================================================
// TestCase 模型 getter/setter
// =============================================================================
TEST(TestCaseTest, DefaultValues) {
  oj::TestCase tc;
  EXPECT_EQ(tc.id(), 0);
  EXPECT_EQ(tc.problem_id(), 0);
  EXPECT_EQ(tc.input(), "");
  EXPECT_EQ(tc.expected(), "");
  EXPECT_EQ(tc.position(), 0);
}

TEST(TestCaseTest, SetAndGet) {
  oj::TestCase tc;
  tc.set_id(7);
  tc.set_problem_id(3);
  tc.set_input("1 2\n");
  tc.set_expected("3\n");
  tc.set_position(2);

  EXPECT_EQ(tc.id(), 7);
  EXPECT_EQ(tc.problem_id(), 3);
  EXPECT_EQ(tc.input(), "1 2\n");
  EXPECT_EQ(tc.expected(), "3\n");
  EXPECT_EQ(tc.position(), 2);
}

TEST(TestCaseTest, MultilineInput) {
  oj::TestCase tc;
  tc.set_input("3\n1 2 3\n4 5 6\n");
  tc.set_expected("6\n");
  EXPECT_EQ(tc.input(), "3\n1 2 3\n4 5 6\n");
  EXPECT_EQ(tc.expected(), "6\n");
}

TEST(TestCaseTest, EmptyStringFields) {
  oj::TestCase tc;
  tc.set_input("");
  tc.set_expected("");
  EXPECT_TRUE(tc.input().empty());
  EXPECT_TRUE(tc.expected().empty());
}

// =============================================================================
// Role 枚举转换
// =============================================================================
TEST(RoleTest, ParseFromString) {
  EXPECT_EQ(oj::ParseRole("user"), oj::Role::User);
  EXPECT_EQ(oj::ParseRole("admin"), oj::Role::Admin);
}

TEST(RoleTest, ParseCaseInsensitive) {
  EXPECT_EQ(oj::ParseRole("USER"), oj::Role::User);
  EXPECT_EQ(oj::ParseRole("Admin"), oj::Role::Admin);
}

TEST(RoleTest, ParseInvalidDefaultsToUser) {
  EXPECT_EQ(oj::ParseRole("unknown"), oj::Role::User);
  EXPECT_EQ(oj::ParseRole(""), oj::Role::User);
  EXPECT_EQ(oj::ParseRole("root"), oj::Role::User);
}

TEST(RoleTest, ToString) {
  EXPECT_EQ(oj::RoleToStr(oj::Role::User), "user");
  EXPECT_EQ(oj::RoleToStr(oj::Role::Admin), "admin");
}

// =============================================================================
// User 模型 getter/setter
// =============================================================================
TEST(UserTest, DefaultValues) {
  oj::User u;
  EXPECT_EQ(u.id(), 0);
  EXPECT_EQ(u.username(), "");
  EXPECT_EQ(u.password(), "");
  EXPECT_EQ(u.role(), oj::Role::User);
  EXPECT_EQ(u.created_at(), "");
  EXPECT_FALSE(u.IsAdmin());
}

TEST(UserTest, SetAndGet) {
  oj::User u;
  u.set_id(10);
  u.set_username("alice");
  u.set_password("$2a$10$hashedvalue");
  u.set_role(oj::Role::Admin);
  u.set_created_at("2026-07-01 08:30:00");

  EXPECT_EQ(u.id(), 10);
  EXPECT_EQ(u.username(), "alice");
  EXPECT_EQ(u.password(), "$2a$10$hashedvalue");
  EXPECT_EQ(u.role(), oj::Role::Admin);
  EXPECT_EQ(u.created_at(), "2026-07-01 08:30:00");
  EXPECT_TRUE(u.IsAdmin());
}

TEST(UserTest, IsAdminFlag) {
  oj::User u;
  u.set_role(oj::Role::User);
  EXPECT_FALSE(u.IsAdmin());

  u.set_role(oj::Role::Admin);
  EXPECT_TRUE(u.IsAdmin());

  u.set_role(oj::Role::User);
  EXPECT_FALSE(u.IsAdmin());
}

TEST(UserTest, DefaultRoleIsUser) {
  oj::User u;
  EXPECT_EQ(u.role(), oj::Role::User);
  EXPECT_FALSE(u.IsAdmin());
}
