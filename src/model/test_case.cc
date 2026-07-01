#include "test_case.h"

#include <mysql/mysql.h>

#include <string>

#include "mapper.h"

namespace oj {

namespace {

std::string Esc(MYSQL* conn, const std::string& s) {
  std::string buf(s.size() * 2 + 1, '\0');
  unsigned long len = ::mysql_real_escape_string(conn, &buf[0], s.c_str(),
                                                 static_cast<unsigned long>(s.size()));
  buf.resize(len);
  return "'" + buf + "'";
}

}  // namespace

bool TestCase::LoadFromDB(MYSQL* conn, int id) {
  std::string sql = "SELECT id,problem_id,input,expected,position "
                    "FROM test_cases WHERE id=" + std::to_string(id);
  if (::mysql_query(conn, sql.c_str()) != 0) return false;
  MYSQL_RES* res = ::mysql_store_result(conn);
  if (res == nullptr) return false;

  auto tcs = MapTestCases(res);
  ::mysql_free_result(res);

  if (tcs.empty()) return false;
  *this = std::move(tcs[0]);
  return true;
}

bool TestCase::SaveToDB(MYSQL* conn) {
  if (id_ == 0) {
    std::string sql = "INSERT INTO test_cases(problem_id,input,expected,position) "
                      "VALUES(" + std::to_string(problem_id_) + "," +
                      Esc(conn, input_) + "," +
                      Esc(conn, expected_) + "," +
                      std::to_string(position_) + ")";
    if (::mysql_query(conn, sql.c_str()) != 0) return false;
    id_ = static_cast<int>(::mysql_insert_id(conn));
    return true;
  }
  std::string sql = "UPDATE test_cases SET problem_id=" + std::to_string(problem_id_) +
                    ",input=" + Esc(conn, input_) +
                    ",expected=" + Esc(conn, expected_) +
                    ",position=" + std::to_string(position_) +
                    " WHERE id=" + std::to_string(id_);
  return ::mysql_query(conn, sql.c_str()) == 0;
}

bool TestCase::DeleteFromDB(MYSQL* conn, int id) {
  std::string sql = "DELETE FROM test_cases WHERE id=" + std::to_string(id);
  return ::mysql_query(conn, sql.c_str()) == 0;
}

}  // namespace oj
