// init_admin — 初始化管理员账户工具
// 用法: ./init_admin
// 功能: 向数据库插入 admin 账户（用户名 admin，密码 admin123）
//       密码通过 AuthService::HashPassword 进行 bcrypt 哈希后存储
//       若 admin 已存在则先删除旧记录，再重新插入

#include <iostream>
#include <string>

#include "db/connection_pool.h"
#include "model/user.h"
#include "service/auth_service.h"
#include "utils/logger.h"

int main() {
  oj::Logger::Instance().Init("info", "logs", "init_admin.log");

  auto& pool = oj::ConnectionPool::Instance();
  if (!pool.Init("localhost", 3306, "root", "", "oj_db", 1)) {
    std::cerr << "[ERROR] 数据库连接失败" << std::endl;
    return 1;
  }

  oj::ConnectionGuard g;
  if (!g.Valid()) {
    std::cerr << "[ERROR] 获取数据库连接失败" << std::endl;
    pool.Destroy();
    return 1;
  }

  std::string username = "admin";
  std::string password = "admin123";

  std::string hash = oj::AuthService::HashPassword(password);
  if (hash.empty()) {
    std::cerr << "[ERROR] 密码哈希失败" << std::endl;
    pool.Destroy();
    return 1;
  }

  std::cout << "生成的 bcrypt 哈希: " << hash << std::endl;

  // 若 admin 已存在，先删除旧记录
  oj::User existing;
  if (existing.LoadFromDBByUsername(g.Get(), username)) {
    std::cout << "发现已存在的 admin 账户 (id=" << existing.id()
              << ")，先删除旧记录..." << std::endl;
    if (!oj::User::DeleteFromDB(g.Get(), existing.id())) {
      std::cerr << "[ERROR] 删除旧 admin 记录失败" << std::endl;
      pool.Destroy();
      return 1;
    }
    std::cout << "[OK] 旧记录已删除" << std::endl;
  }

  // 重新插入 admin 账户
  oj::User user;
  user.set_username(username);
  user.set_password(hash);
  user.set_role(oj::Role::Admin);
  if (!user.SaveToDB(g.Get())) {
    std::cerr << "[ERROR] 创建管理员账户失败" << std::endl;
    pool.Destroy();
    return 1;
  }

  std::cout << "[OK] 管理员账户已创建: id=" << user.id()
            << " username=" << user.username()
            << " role=" << oj::RoleToStr(user.role()) << std::endl;

  // 验证密码
  if (!oj::AuthService::VerifyPassword(password, hash)) {
    std::cerr << "[WARN] 密码验证测试失败" << std::endl;
  } else {
    std::cout << "[OK] 密码验证测试通过" << std::endl;
  }

  pool.Destroy();
  std::cout << "完成。" << std::endl;
  return 0;
}
