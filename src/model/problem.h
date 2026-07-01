#ifndef OJ_MODEL_PROBLEM_H
#define OJ_MODEL_PROBLEM_H

#include <mysql/mysql.h>

#include <string>
#include <vector>

namespace oj {

// 题目难度枚举（对应 MySQL ENUM('Easy','Medium','Hard')）
enum class Difficulty {
  Easy   = 0,
  Medium = 1,
  Hard   = 2
};

// 难度枚举与字符串互转
Difficulty   ParseDifficulty(const std::string& s);
std::string  DifficultyToStr(Difficulty d);

// 题目模型（对应 problems 表）
// 字段映射：
//   id         -> id_
//   title      -> title_
//   difficulty -> difficulty_
//   content    -> content_      (Markdown)
//   template   -> template_     (代码模板)
//   created_at -> created_at_
class Problem {
 public:
  int         id()         const { return id_; }
  std::string title()      const { return title_; }
  Difficulty  difficulty() const { return difficulty_; }
  std::string content()    const { return content_; }
  std::string tpl()        const { return template_; }
  std::string created_at() const { return created_at_; }

  void set_id(int v)                    { id_ = v; }
  void set_title(const std::string& v)  { title_ = v; }
  void set_difficulty(Difficulty v)     { difficulty_ = v; }
  void set_content(const std::string& v){ content_ = v; }
  void set_tpl(const std::string& v)    { template_ = v; }
  void set_created_at(const std::string& v) { created_at_ = v; }

  // 按 id 从数据库加载，成功返回 true
  bool LoadFromDB(MYSQL* conn, int id);
  // 写入数据库：id_==0 执行 INSERT（并回填自增 id），否则 UPDATE
  bool SaveToDB(MYSQL* conn);
  // 按 id 从数据库删除
  static bool DeleteFromDB(MYSQL* conn, int id);

 private:
  int         id_ = 0;
  std::string title_;
  Difficulty  difficulty_ = Difficulty::Easy;
  std::string content_;
  std::string template_;
  std::string created_at_;
};

}  // namespace oj

#endif  // OJ_MODEL_PROBLEM_H
