#ifndef OJ_AUTH_HANDLER_H
#define OJ_AUTH_HANDLER_H

#include "utils/httplib.h"

namespace oj {

// 注册认证接口路由
//   POST /api/register  注册新用户
//   POST /api/login     登录（后续实现）
//   POST /api/logout    登出（后续实现）
void RegisterAuthHandlers(httplib::Server& server);

}  // namespace oj

#endif  // OJ_AUTH_HANDLER_H
