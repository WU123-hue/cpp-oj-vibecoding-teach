#include "db/connection_pool.h"
#include "server/server.h"
#include "utils/config.h"
#include "utils/logger.h"

#include <iostream>

int main() {
  // 1. 加载配置
  auto& cfg = oj::Config::Instance();
  if (!cfg.Load("config/config.yaml")) {
    std::cerr << "load config failed, using defaults" << std::endl;
    cfg.SetDefault();
  }

  // 2. 初始化日志
  oj::Logger::Instance().Init(cfg.log().level, cfg.log().dir, cfg.log().filename);

  LOG_INFO("===== OJ System starting =====");

  // 3. 初始化数据库连接池
  if (!oj::ConnectionPool::Instance().Init(
          cfg.db().host, cfg.db().port, cfg.db().user,
          cfg.db().password, cfg.db().database, cfg.db().pool_size)) {
    LOG_ERROR("database connection pool init failed");
    return 1;
  }
  LOG_INFO_FMT("db pool ready: %s/%s pool_size=%d",
               cfg.db().host.c_str(), cfg.db().database.c_str(), cfg.db().pool_size);

  // 4. 启动 HTTP 服务器（阻塞）
  if (!oj::StartServer()) {
    LOG_ERROR("server start failed");
    return 1;
  }

  // 5. 清理
  oj::ConnectionPool::Instance().Destroy();
  LOG_INFO("===== OJ System stopped =====");
  return 0;
}
