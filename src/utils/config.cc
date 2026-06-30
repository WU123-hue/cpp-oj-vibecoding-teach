#include "config.h"

#include <yaml-cpp/yaml.h>
#include <iostream>

namespace oj {

Config& Config::Instance() {
  static Config instance;
  return instance;
}

bool Config::Load(const std::string& path) {
  try {
    YAML::Node root = YAML::LoadFile(path);

    if (auto n = root["database"]) {
      if (n["host"])      db_.host      = n["host"].as<std::string>();
      if (n["port"])      db_.port      = n["port"].as<int>();
      if (n["user"])      db_.user      = n["user"].as<std::string>();
      if (n["password"])  db_.password  = n["password"].as<std::string>();
      if (n["database"])  db_.database  = n["database"].as<std::string>();
      if (n["pool_size"]) db_.pool_size = n["pool_size"].as<int>();
    }

    if (auto n = root["log"]) {
      if (n["level"])    log_.level    = n["level"].as<std::string>();
      if (n["dir"])      log_.dir      = n["dir"].as<std::string>();
      if (n["filename"]) log_.filename = n["filename"].as<std::string>();
    }

    if (auto n = root["server"]) {
      if (n["host"])        server_.host         = n["host"].as<std::string>();
      if (n["port"])        server_.port         = n["port"].as<int>();
      if (n["thread_count"]) server_.thread_count = n["thread_count"].as<int>();
    }

    if (auto n = root["executor"]) {
      if (n["timeout_sec"])   executor_.timeout_sec   = n["timeout_sec"].as<int>();
      if (n["cpu_limit_sec"]) executor_.cpu_limit_sec = n["cpu_limit_sec"].as<int>();
      if (n["mem_limit_mb"])  executor_.mem_limit_mb  = n["mem_limit_mb"].as<int>();
    }

    return true;
  } catch (const std::exception& e) {
    std::cerr << "[Config] load failed: " << e.what()
              << ", path=" << path << std::endl;
    return false;
  }
}

void Config::SetDefault() {
  db_       = DbConfig{};
  log_      = LogConfig{};
  server_   = ServerConfig{};
  executor_ = ExecutorConfig{};
}

}  // namespace oj
