#include "mapper.h"

#include <cstdlib>

namespace oj {

namespace {

// 安全读取行字段，越界或 NULL 返回空串
std::string Cell(MYSQL_ROW row, int n, int idx) {
  if (idx < 0 || idx >= n) return "";
  if (row[idx] == nullptr) return "";
  return std::string(row[idx]);
}

}  // namespace

std::vector<Problem> MapProblems(MYSQL_RES* res) {
  std::vector<Problem> out;
  if (res == nullptr) return out;

  int n = ::mysql_num_fields(res);
  ::mysql_field_seek(res, 0);
  MYSQL_FIELD* fields = ::mysql_fetch_fields(res);

  MYSQL_ROW row;
  while ((row = ::mysql_fetch_row(res)) != nullptr) {
    Problem p;
    // 按列名匹配，兼容不同 SELECT 顺序
    for (int i = 0; i < n; ++i) {
      std::string name = fields[i].name;
      if (name == "id") {
        p.set_id(std::atoi(Cell(row, n, i).c_str()));
      } else if (name == "title") {
        p.set_title(Cell(row, n, i));
      } else if (name == "difficulty") {
        p.set_difficulty(ParseDifficulty(Cell(row, n, i)));
      } else if (name == "content") {
        p.set_content(Cell(row, n, i));
      } else if (name == "template") {
        p.set_tpl(Cell(row, n, i));
      } else if (name == "created_at") {
        p.set_created_at(Cell(row, n, i));
      }
    }
    out.push_back(std::move(p));
  }
  return out;
}

std::vector<TestCase> MapTestCases(MYSQL_RES* res) {
  std::vector<TestCase> out;
  if (res == nullptr) return out;

  int n = ::mysql_num_fields(res);
  MYSQL_FIELD* fields = ::mysql_fetch_fields(res);

  MYSQL_ROW row;
  while ((row = ::mysql_fetch_row(res)) != nullptr) {
    TestCase tc;
    for (int i = 0; i < n; ++i) {
      std::string name = fields[i].name;
      if (name == "id") {
        tc.set_id(std::atoi(Cell(row, n, i).c_str()));
      } else if (name == "problem_id") {
        tc.set_problem_id(std::atoi(Cell(row, n, i).c_str()));
      } else if (name == "input") {
        tc.set_input(Cell(row, n, i));
      } else if (name == "expected") {
        tc.set_expected(Cell(row, n, i));
      } else if (name == "position") {
        tc.set_position(std::atoi(Cell(row, n, i).c_str()));
      }
    }
    out.push_back(std::move(tc));
  }
  return out;
}

std::vector<User> MapUsers(MYSQL_RES* res) {
  std::vector<User> out;
  if (res == nullptr) return out;

  int n = ::mysql_num_fields(res);
  MYSQL_FIELD* fields = ::mysql_fetch_fields(res);

  MYSQL_ROW row;
  while ((row = ::mysql_fetch_row(res)) != nullptr) {
    User u;
    for (int i = 0; i < n; ++i) {
      std::string name = fields[i].name;
      if (name == "id") {
        u.set_id(std::atoi(Cell(row, n, i).c_str()));
      } else if (name == "username") {
        u.set_username(Cell(row, n, i));
      } else if (name == "password") {
        u.set_password(Cell(row, n, i));
      } else if (name == "role") {
        u.set_role(ParseRole(Cell(row, n, i)));
      } else if (name == "created_at") {
        u.set_created_at(Cell(row, n, i));
      }
    }
    out.push_back(std::move(u));
  }
  return out;
}

long MapScalar(MYSQL_RES* res, long fallback) {
  if (res == nullptr) return fallback;
  MYSQL_ROW row = ::mysql_fetch_row(res);
  if (row == nullptr || row[0] == nullptr) return fallback;
  return std::atol(row[0]);
}

}  // namespace oj
