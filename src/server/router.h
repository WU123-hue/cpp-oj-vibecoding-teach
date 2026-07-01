#ifndef OJ_ROUTER_H
#define OJ_ROUTER_H

#include "utils/httplib.h"

namespace oj {

// 注册所有 API 路由到 server
void RegisterRoutes(httplib::Server& server);

}  // namespace oj

#endif  // OJ_ROUTER_H
