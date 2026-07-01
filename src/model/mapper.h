#ifndef OJ_MODEL_MAPPER_H
#define OJ_MODEL_MAPPER_H

#include <mysql/mysql.h>

#include <string>
#include <vector>

#include "problem.h"
#include "test_case.h"
#include "user.h"

namespace oj {

// 从已执行查询的结果集 (MYSQL_RES) 映射为模型对象。
// 调用方需先 mysql_query 成功，再传入 mysql_store_result 的结果。

// 将 problems 表查询结果映射为 Problem 列表
// 列顺序要求: id, title, difficulty, content, template, created_at
std::vector<Problem> MapProblems(MYSQL_RES* res);

// 将 test_cases 表查询结果映射为 TestCase 列表
// 列顺序要求: id, problem_id, input, expected, position
std::vector<TestCase> MapTestCases(MYSQL_RES* res);

// 将 users 表查询结果映射为 User 列表
// 列顺序要求: id, username, password, role, created_at
std::vector<User> MapUsers(MYSQL_RES* res);

// 取单行单列的整数值（如 COUNT(*)），失败返回 fallback
long MapScalar(MYSQL_RES* res, long fallback = 0);

}  // namespace oj

#endif  // OJ_MODEL_MAPPER_H
