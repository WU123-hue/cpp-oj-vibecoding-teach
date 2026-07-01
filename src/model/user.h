#ifndef OJ_MODEL_USER_H
#define OJ_MODEL_USER_H

#include <mysql/mysql.h>

#include <string>

namespace oj {

// 用户角色枚举（对应 MySQL ENUM('user','admin')）
enum class Role {
  User  = 0,
  Admin = 1
};

Role        ParseRole(const std::string& s);
std::string RoleToStr(Role r);

// 用户模型（对应 users 表）
// 字段映射：
//   id         -> id_
//   username   -> username_
//   password   -> password_    (bcrypt 哈希)
//   role       -> role_
//   created_at -> created_at_
class User {
 public:
  int         id()         const { return id_; }
  std::string username()   const { return username_; }
  std::string password()   const { return password_; }
  Role        role()       const { return role_; }
  std::string created_at() const { return created_at_; }

  void set_id(int v)                    { id_ = v; }
  void set_username(const std::string& v){ username_ = v; }
  void set_password(const std::string& v){ password_ = v; }
  void set_role(Role v)                 { role_ = v; }
  void set_created_at(const std::string& v) { created_at_ = v; }

  // 便捷判断
  bool IsAdmin() const { return role_ == Role::Admin; }

  // 按 id 从数据库加载，成功返回 true
  bool LoadFromDB(MYSQL* conn, int id);
  // 写入数据库：id_==0 执行 INSERT（并回填自增 id），否则 UPDATE
  bool SaveToDB(MYSQL* conn);
  // 按 id 从数据库删除
  static bool DeleteFromDB(MYSQL* conn, int id);

 private:
  int         id_ = 0;
  std::string username_;
  std::string password_;
  Role        role_ = Role::User;
  std::string created_at_;
};

}  // namespace oj

#endif  // OJ_MODEL_USER_H
