#ifndef OJ_PROBLEM_SERVICE_H
#define OJ_PROBLEM_SERVICE_H

#include "model/problem.h"
#include "model/test_case.h"

#include <string>
#include <vector>

namespace oj {

// 题目创建请求（含测试用例）
struct CreateProblemRequest {
  std::string              title;
  std::string              difficulty;   // "Easy"/"Medium"/"Hard"
  std::string              content;      // Markdown 描述
  std::string              tpl;          // 代码模板
  std::vector<TestCase>    test_cases;   // 测试用例
};

// 题目列表项（列表页只需部分字段）
struct ProblemSummary {
  int         id;
  std::string title;
  std::string difficulty;
};

// 题目业务服务
class ProblemService {
 public:
  // 新增题目（含测试用例），成功返回 true，失败 reason 填充错误信息
  bool Create(const CreateProblemRequest& req, int* new_id, std::string* reason);

  // 删除题目（级联删除测试用例）
  bool Delete(int id, std::string* reason);

  // 获取题目列表（仅 id/title/difficulty）
  bool List(std::vector<ProblemSummary>* out, std::string* reason);

  // 获取题目详情（含测试用例）
  bool Get(int id, Problem* problem, std::vector<TestCase>* test_cases,
           std::string* reason);
};

}  // namespace oj

#endif  // OJ_PROBLEM_SERVICE_H
