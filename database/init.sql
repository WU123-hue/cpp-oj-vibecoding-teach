-- =============================================================================
-- OJ 系统数据库初始化脚本
-- 用法: mysql -u root -p < database/init.sql
-- =============================================================================

CREATE DATABASE IF NOT EXISTS oj_db
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

USE oj_db;

-- -----------------------------------------------------------------------------
-- 题目表
-- -----------------------------------------------------------------------------
DROP TABLE IF EXISTS test_cases;
DROP TABLE IF EXISTS problems;

CREATE TABLE problems (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  title      VARCHAR(255) NOT NULL,
  difficulty ENUM('Easy','Medium','Hard') NOT NULL,
  content    TEXT NOT NULL,                     -- 题目描述 (Markdown)
  template   TEXT,                              -- 代码模板
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- -----------------------------------------------------------------------------
-- 测试用例表 (与题目 1:N 关联)
-- -----------------------------------------------------------------------------
CREATE TABLE test_cases (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  problem_id INT NOT NULL,
  input      TEXT NOT NULL,                     -- 输入数据
  expected   TEXT NOT NULL,                     -- 期望输出
  position   INT NOT NULL DEFAULT 0,            -- 排序序号
  FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE INDEX idx_test_cases_problem_id ON test_cases(problem_id);

-- -----------------------------------------------------------------------------
-- 用户表
-- -----------------------------------------------------------------------------
DROP TABLE IF EXISTS users;

CREATE TABLE users (
  id         INT PRIMARY KEY AUTO_INCREMENT,
  username   VARCHAR(64) UNIQUE NOT NULL,
  password   VARCHAR(128) NOT NULL,             -- bcrypt 哈希
  role       ENUM('user','admin') DEFAULT 'user',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- -----------------------------------------------------------------------------
-- 初始管理员账号 (密码: admin123 的 bcrypt 哈希)
-- 如需修改请在首次启动后通过管理接口更新密码
-- -----------------------------------------------------------------------------
INSERT INTO users (username, password, role) VALUES
  ('admin', '$2a$10$N9qo8uLOickgx2ZMRZoMyeIjZAgcfl7p92ldGxad68LJZdL17lhWy', 'admin');
