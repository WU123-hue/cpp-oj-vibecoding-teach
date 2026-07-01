#ifndef OJ_PROBLEM_HANDLER_H
#define OJ_PROBLEM_HANDLER_H

#include "utils/httplib.h"

namespace oj {

// 注册题目查询接口路由
//   GET /api/problems       题目列表
//   GET /api/problems/:id   题目详情（含描述/用例）
void RegisterProblemHandlers(httplib::Server& server);

}  // namespace oj

#endif  // OJ_PROBLEM_HANDLER_H
