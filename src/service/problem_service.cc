#include "problem_service.h"

#include <mysql/mysql.h>

#include <string>

#include "db/connection_pool.h"
#include "model/mapper.h"
#include "utils/logger.h"

namespace oj {

namespace {

std::string Esc(MYSQL* conn, const std::string& s) {
  std::string buf(s.size() * 2 + 1, '\0');
  unsigned long len = ::mysql_real_escape_string(conn, &buf[0], s.c_str(),
                                                 static_cast<unsigned long>(s.size()));
  buf.resize(len);
  return "'" + buf + "'";
}

}  // namespace

bool ProblemService::Create(const CreateProblemRequest& req, int* new_id,
                            std::string* reason) {
  // 参数校验
  if (req.title.empty()) {
    if (reason) *reason = "title is required";
    return false;
  }
  if (req.content.empty()) {
    if (reason) *reason = "content is required";
    return false;
  }
  if (req.difficulty != "Easy" && req.difficulty != "Medium" &&
      req.difficulty != "Hard") {
    if (reason) *reason = "invalid difficulty";
    return false;
  }

  ConnectionGuard g;
  if (!g.Valid()) {
    if (reason) *reason = "database connection unavailable";
    return false;
  }
  MYSQL* c = g.Get();

  // 开启事务
  if (::mysql_query(c, "BEGIN") != 0) {
    if (reason) *reason = "begin transaction failed";
    return false;
  }

  // 插入题目
  std::string sql =
      "INSERT INTO problems(title,difficulty,content,template) VALUES(" +
      Esc(c, req.title) + ",'" + req.difficulty + "'," +
      Esc(c, req.content) + "," + Esc(c, req.tpl) + ")";
  if (::mysql_query(c, sql.c_str()) != 0) {
    if (reason) *reason = std::string("insert problem failed: ") + ::mysql_error(c);
    ::mysql_query(c, "ROLLBACK");
    return false;
  }
  int pid = static_cast<int>(::mysql_insert_id(c));

  // 插入测试用例
  for (size_t i = 0; i < req.test_cases.size(); ++i) {
    const auto& tc = req.test_cases[i];
    sql = "INSERT INTO test_cases(problem_id,input,expected,position) VALUES(" +
          std::to_string(pid) + "," + Esc(c, tc.input()) + "," +
          Esc(c, tc.expected()) + "," + std::to_string(i) + ")";
    if (::mysql_query(c, sql.c_str()) != 0) {
      if (reason) *reason = std::string("insert test_case failed: ") + ::mysql_error(c);
      ::mysql_query(c, "ROLLBACK");
      return false;
    }
  }

  if (::mysql_query(c, "COMMIT") != 0) {
    if (reason) *reason = "commit failed";
    return false;
  }

  if (new_id) *new_id = pid;
  LOG_INFO_FMT("problem created: id=%d title=%s", pid, req.title.c_str());
  return true;
}

bool ProblemService::Delete(int id, std::string* reason) {
  if (id <= 0) {
    if (reason) *reason = "invalid id";
    return false;
  }

  ConnectionGuard g;
  if (!g.Valid()) {
    if (reason) *reason = "database connection unavailable";
    return false;
  }

  // 外键 ON DELETE CASCADE 会自动删除关联测试用例
  if (!Problem::DeleteFromDB(g.Get(), id)) {
    if (reason) *reason = "delete problem failed";
    return false;
  }
  LOG_INFO_FMT("problem deleted: id=%d", id);
  return true;
}

bool ProblemService::List(std::vector<ProblemSummary>* out, std::string* reason) {
  ConnectionGuard g;
  if (!g.Valid()) {
    if (reason) *reason = "database connection unavailable";
    return false;
  }

  std::string sql = "SELECT id,title,difficulty FROM problems ORDER BY id";
  if (::mysql_query(g.Get(), sql.c_str()) != 0) {
    if (reason) *reason = "query failed";
    return false;
  }
  MYSQL_RES* res = ::mysql_store_result(g.Get());
  if (res == nullptr) {
    if (reason) *reason = "store result failed";
    return false;
  }

  out->clear();
  MYSQL_ROW row;
  while ((row = ::mysql_fetch_row(res)) != nullptr) {
    ProblemSummary s;
    s.id = row[0] ? std::atoi(row[0]) : 0;
    s.title = row[1] ? row[1] : "";
    s.difficulty = row[2] ? row[2] : "";
    out->push_back(std::move(s));
  }
  ::mysql_free_result(res);
  return true;
}

bool ProblemService::Get(int id, Problem* problem,
                         std::vector<TestCase>* test_cases, std::string* reason) {
  ConnectionGuard g;
  if (!g.Valid()) {
    if (reason) *reason = "database connection unavailable";
    return false;
  }
  MYSQL* c = g.Get();

  // 加载题目
  if (!problem->LoadFromDB(c, id)) {
    if (reason) *reason = "problem not found";
    return false;
  }

  // 加载测试用例
  std::string sql = "SELECT id,problem_id,input,expected,position "
                    "FROM test_cases WHERE problem_id=" + std::to_string(id) +
                    " ORDER BY position";
  if (::mysql_query(c, sql.c_str()) != 0) {
    if (reason) *reason = "query test_cases failed";
    return false;
  }
  MYSQL_RES* res = ::mysql_store_result(c);
  if (res == nullptr) {
    if (reason) *reason = "store result failed";
    return false;
  }
  *test_cases = MapTestCases(res);
  ::mysql_free_result(res);
  return true;
}

}  // namespace oj
