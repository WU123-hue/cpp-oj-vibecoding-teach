#include "user.h"

#include <mysql/mysql.h>

#include <algorithm>
#include <cctype>
#include <string>

#include "mapper.h"

namespace oj {

Role ParseRole(const std::string& s) {
  std::string t;
  t.reserve(s.size());
  for (char c : s) t.push_back(static_cast<char>(std::tolower(c)));
  if (t == "admin") return Role::Admin;
  return Role::User;  // 默认 user
}

std::string RoleToStr(Role r) {
  return r == Role::Admin ? "admin" : "user";
}

namespace {

std::string Esc(MYSQL* conn, const std::string& s) {
  std::string buf(s.size() * 2 + 1, '\0');
  unsigned long len = ::mysql_real_escape_string(conn, &buf[0], s.c_str(),
                                                 static_cast<unsigned long>(s.size()));
  buf.resize(len);
  return "'" + buf + "'";
}

}  // namespace

bool User::LoadFromDB(MYSQL* conn, int id) {
  std::string sql = "SELECT id,username,password,role,created_at "
                    "FROM users WHERE id=" + std::to_string(id);
  if (::mysql_query(conn, sql.c_str()) != 0) return false;
  MYSQL_RES* res = ::mysql_store_result(conn);
  if (res == nullptr) return false;

  auto users = MapUsers(res);
  ::mysql_free_result(res);

  if (users.empty()) return false;
  *this = std::move(users[0]);
  return true;
}

bool User::LoadFromDBByUsername(MYSQL* conn, const std::string& username) {
  std::string sql = "SELECT id,username,password,role,created_at "
                    "FROM users WHERE username=" + Esc(conn, username);
  if (::mysql_query(conn, sql.c_str()) != 0) return false;
  MYSQL_RES* res = ::mysql_store_result(conn);
  if (res == nullptr) return false;

  auto users = MapUsers(res);
  ::mysql_free_result(res);

  if (users.empty()) return false;
  *this = std::move(users[0]);
  return true;
}

bool User::SaveToDB(MYSQL* conn) {
  if (id_ == 0) {
    std::string sql = "INSERT INTO users(username,password,role) "
                      "VALUES(" + Esc(conn, username_) + "," +
                      Esc(conn, password_) + "," +
                      "'" + RoleToStr(role_) + "')";
    if (::mysql_query(conn, sql.c_str()) != 0) return false;
    id_ = static_cast<int>(::mysql_insert_id(conn));
    return true;
  }
  std::string sql = "UPDATE users SET username=" + Esc(conn, username_) +
                    ",password=" + Esc(conn, password_) +
                    ",role='" + RoleToStr(role_) + "'" +
                    " WHERE id=" + std::to_string(id_);
  return ::mysql_query(conn, sql.c_str()) == 0;
}

bool User::DeleteFromDB(MYSQL* conn, int id) {
  std::string sql = "DELETE FROM users WHERE id=" + std::to_string(id);
  return ::mysql_query(conn, sql.c_str()) == 0;
}

}  // namespace oj
