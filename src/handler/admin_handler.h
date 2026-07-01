#ifndef OJ_ADMIN_HANDLER_H
#define OJ_ADMIN_HANDLER_H

#include "utils/httplib.h"

namespace oj {

// 注册管理接口路由
//   POST   /api/admin/problems      新增题目
//   DELETE /api/admin/problems/:id  删除题目
void RegisterAdminHandlers(httplib::Server& server);

}  // namespace oj

#endif  // OJ_ADMIN_HANDLER_H
