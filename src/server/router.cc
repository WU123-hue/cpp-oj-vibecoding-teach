#include "router.h"

#include "handler/admin_handler.h"
#include "handler/problem_handler.h"

namespace oj {

void RegisterRoutes(httplib::Server& server) {
  // 管理接口
  RegisterAdminHandlers(server);

  // 用户接口
  RegisterProblemHandlers(server);
}

}  // namespace oj
