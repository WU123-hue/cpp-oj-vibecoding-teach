#include "server.h"

#include "utils/httplib.h"

#include <string>

#include "router.h"
#include "utils/config.h"
#include "utils/logger.h"

namespace oj {

bool StartServer() {
  auto& cfg = Config::Instance();

  httplib::Server server;

  // 静态文件服务：public 目录映射到根路径
  server.set_base_dir("./public");

  // 注册 API 路由
  RegisterRoutes(server);

  std::string host = cfg.server().host;
  int port = cfg.server().port;

  LOG_INFO_FMT("HTTP server starting on %s:%d", host.c_str(), port);

  if (!server.listen(host.c_str(), port)) {
    LOG_ERROR_FMT("server listen failed on %s:%d", host.c_str(), port);
    return false;
  }
  return true;
}

}  // namespace oj
