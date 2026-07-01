#include "router.h"

#include "handler/admin_handler.h"
#include "handler/problem_handler.h"
#include "handler/submit_handler.h"

namespace oj {

void RegisterRoutes(httplib::Server& server) {
  // 管理接口
  RegisterAdminHandlers(server);

  // 用户接口
  RegisterProblemHandlers(server);

  // 代码提交执行接口
  RegisterSubmitHandlers(server);
}

}  // namespace oj
