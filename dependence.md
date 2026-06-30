# OJ 系统依赖说明

## 系统级依赖

```bash
# 构建工具
sudo apt update
sudo apt install -y cmake g++ make

# MySQL 服务器
sudo apt install -y mysql-server
```

## C++ 库依赖

```bash

# MySQL Connector/C++
sudo apt install -y libmysqlclient-dev

# YAML 配置文件解析
sudo apt install -y libyaml-cpp-dev

```

## 一键安装命令

```bash
sudo apt update && sudo apt install -y \
  cmake g++ make \
  mysql-server \
  libmysqlclient-dev \
  libyaml-cpp-dev \
```

## 验证安装

```bash
g++ --version
cmake --version
mysql --version
```

## 依赖说明

| 依赖 | 用途 |
|------|------|
| cmake | C++ 项目构建工具 |
| g++ | C++ 编译器，用于编译后端代码和用户提交的代码 |
| make | 构建辅助工具 |
| mysql-server | 数据库服务，存储题目和用户数据 |
| libhttplib-dev | C++ HTTP 库，用于构建 REST API 服务 |
| libmysqlclient-dev | MySQL 客户端库，用于连接数据库 |
| libyaml-cpp-dev | YAML 配置文件解析库 |
| libbcrypt-dev | bcrypt 密码哈希库，用于用户密码加密 |