#include "problem.h"

#include <mysql/mysql.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#include "mapper.h"

namespace oj {

Difficulty ParseDifficulty(const std::string& s) {
  std::string t;
  t.reserve(s.size());
  for (char c : s) t.push_back(static_cast<char>(std::tolower(c)));
  if (t == "easy")   return Difficulty::Easy;
  if (t == "medium") return Difficulty::Medium;
  if (t == "hard")   return Difficulty::Hard;
  return Difficulty::Easy;  // 默认值
}

std::string DifficultyToStr(Difficulty d) {
  switch (d) {
    case Difficulty::Easy:   return "Easy";
    case Difficulty::Medium: return "Medium";
    case Difficulty::Hard:   return "Hard";
  }
  return "Easy";
}

namespace {

// SQL 转义：将字符串转为 '...' 形式，避免注入与语法错误
std::string Esc(MYSQL* conn, const std::string& s) {
  std::string buf(s.size() * 2 + 1, '\0');
  unsigned long len = ::mysql_real_escape_string(conn, &buf[0], s.c_str(),
                                                 static_cast<unsigned long>(s.size()));
  buf.resize(len);
  return "'" + buf + "'";
}

}  // namespace

bool Problem::LoadFromDB(MYSQL* conn, int id) {
  std::string sql = "SELECT id,title,difficulty,content,template,created_at "
                    "FROM problems WHERE id=" + std::to_string(id);
  if (::mysql_query(conn, sql.c_str()) != 0) return false;
  MYSQL_RES* res = ::mysql_store_result(conn);
  if (res == nullptr) return false;

  auto probs = MapProblems(res);
  ::mysql_free_result(res);

  if (probs.empty()) return false;
  *this = std::move(probs[0]);
  return true;
}

bool Problem::SaveToDB(MYSQL* conn) {
  if (id_ == 0) {
    // INSERT
    std::string sql = "INSERT INTO problems(title,difficulty,content,template) "
                      "VALUES(" + Esc(conn, title_) + "," +
                      "'" + DifficultyToStr(difficulty_) + "'," +
                      Esc(conn, content_) + "," +
                      Esc(conn, template_) + ")";
    if (::mysql_query(conn, sql.c_str()) != 0) return false;
    id_ = static_cast<int>(::mysql_insert_id(conn));
    return true;
  }
  // UPDATE
  std::string sql = "UPDATE problems SET title=" + Esc(conn, title_) +
                    ",difficulty='" + DifficultyToStr(difficulty_) + "'" +
                    ",content=" + Esc(conn, content_) +
                    ",template=" + Esc(conn, template_) +
                    " WHERE id=" + std::to_string(id_);
  return ::mysql_query(conn, sql.c_str()) == 0;
}

bool Problem::DeleteFromDB(MYSQL* conn, int id) {
  std::string sql = "DELETE FROM problems WHERE id=" + std::to_string(id);
  return ::mysql_query(conn, sql.c_str()) == 0;
}

}  // namespace oj
