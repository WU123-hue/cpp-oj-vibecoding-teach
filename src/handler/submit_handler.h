#ifndef OJ_SUBMIT_HANDLER_H
#define OJ_SUBMIT_HANDLER_H

#include "utils/httplib.h"

namespace oj {

// 注册代码提交执行接口路由
//   POST /api/submit  提交代码执行
void RegisterSubmitHandlers(httplib::Server& server);

}  // namespace oj

#endif  // OJ_SUBMIT_HANDLER_H
