#ifndef OJ_MODEL_TEST_CASE_H
#define OJ_MODEL_TEST_CASE_H

#include <mysql/mysql.h>

#include <string>

namespace oj {

// 测试用例模型（对应 test_cases 表，与 Problem 1:N 关联）
// 字段映射：
//   id         -> id_
//   problem_id -> problem_id_
//   input      -> input_
//   expected   -> expected_
//   position   -> position_   (排序序号)
class TestCase {
 public:
  int         id()         const { return id_; }
  int         problem_id() const { return problem_id_; }
  std::string input()      const { return input_; }
  std::string expected()   const { return expected_; }
  int         position()   const { return position_; }

  void set_id(int v)                    { id_ = v; }
  void set_problem_id(int v)            { problem_id_ = v; }
  void set_input(const std::string& v)  { input_ = v; }
  void set_expected(const std::string& v){ expected_ = v; }
  void set_position(int v)              { position_ = v; }

  // 按 id 从数据库加载，成功返回 true
  bool LoadFromDB(MYSQL* conn, int id);
  // 写入数据库：id_==0 执行 INSERT（并回填自增 id），否则 UPDATE
  bool SaveToDB(MYSQL* conn);
  // 按 id 从数据库删除
  static bool DeleteFromDB(MYSQL* conn, int id);

 private:
  int         id_ = 0;
  int         problem_id_ = 0;
  std::string input_;
  std::string expected_;
  int         position_ = 0;
};

}  // namespace oj

#endif  // OJ_MODEL_TEST_CASE_H
