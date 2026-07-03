# OJ 系统部署文档

> 本文档描述如何从零开始在一台 Linux 服务器上部署 OJ 在线判题系统。

---

## 1. 环境要求

### 1.1 操作系统

- Ubuntu 22.04 / 24.04 LTS（推荐）
- 其他 Linux 发行版（需适配包管理器命令）

### 1.2 软件依赖

| 软件 | 最低版本 | 用途 |
|------|---------|------|
| g++ | 9.0 | 编译 C++17 后端 |
| cmake | 3.10 | 可选，构建辅助 |
| MySQL Server | 8.0 | 数据存储 |
| libmysqlclient-dev | 8.0 | MySQL C 客户端库 |
| libyaml-cpp-dev | 0.6 | YAML 配置解析 |
| libssl-dev | 1.1 | HTTPS 支持 |
| libgtest-dev | 1.12 | 单元测试（可选） |
| libcrypt-dev | 4.4 | bcrypt 密码哈希 |

---

## 2. 安装依赖

### 2.1 Ubuntu / Debian

```bash
sudo apt update
sudo apt install -y g++ make mysql-server libmysqlclient-dev \
    libyaml-cpp-dev libssl-dev libcrypt-dev
```

### 2.2 安装 Google Test（可选，用于单元测试）

```bash
sudo apt install -y libgtest-dev
cd /usr/src/gtest && sudo cmake . && sudo make && sudo make install
```

---

## 3. 数据库初始化

### 3.1 启动 MySQL

```bash
sudo systemctl start mysql
sudo systemctl enable mysql
```

### 3.2 初始化数据库与表结构

```bash
mysql -u root < database/init.sql
```

此脚本会：
- 创建数据库 `oj_db`（utf8mb4）
- 创建 `problems`、`test_cases`、`users` 三张表
- 插入默认管理员账户（`admin` / `admin123`，bcrypt 哈希）

### 3.3 验证数据库

```bash
mysql -u root oj_db -e "SHOW TABLES;"
# 预期输出：problems, test_cases, users

mysql -u root oj_db -e "SELECT username, role FROM users;"
# 预期输出：admin, admin
```

---

## 4. 编译后端

### 4.1 编译 OJ 服务器

```bash
g++ -std=c++17 -Wall -Wextra -I src -I/usr/include/mysql \
  src/main.cc \
  src/server/server.cc src/server/router.cc \
  src/handler/admin_handler.cc src/handler/problem_handler.cc \
  src/handler/submit_handler.cc src/handler/auth_handler.cc \
  src/service/problem_service.cc src/service/executor_service.cc \
  src/service/auth_service.cc src/service/session_manager.cc \
  src/model/problem.cc src/model/test_case.cc src/model/user.cc src/model/mapper.cc \
  src/db/connection_pool.cc \
  src/utils/config.cc src/utils/logger.cc \
  -o oj_server \
  -lmysqlclient -lpthread -lyaml-cpp -lssl -lcrypto -lcrypt
```

### 4.2 编译管理员初始化工具

```bash
g++ -std=c++17 -Wall -Wextra -I src -I/usr/include/mysql \
  src/init_admin.cc \
  src/service/auth_service.cc src/service/session_manager.cc \
  src/model/user.cc src/model/mapper.cc src/model/problem.cc src/model/test_case.cc \
  src/db/connection_pool.cc src/utils/logger.cc \
  -o init_admin \
  -lmysqlclient -lpthread -lyaml-cpp -lcrypt
```

### 4.3 初始化管理员账户

```bash
./init_admin
```

预期输出：
```
[OK] 管理员账户已创建: id=1 username=admin role=admin
[OK] 密码验证测试通过
```

> 如果 `init.sql` 已插入 admin 账户，此步骤会先删除旧记录再重新插入，确保密码哈希正确。

---

## 5. 配置文件

编辑 `config/config.yaml`：

```yaml
# HTTP 服务器配置
server:
  host: "0.0.0.0"        # 监听地址，0.0.0.0 表示所有网卡
  port: 8080              # 监听端口
  thread_count: 4

# 数据库配置
database:
  host: "localhost"
  port: 3306
  user: "root"
  password: ""            # 本地免密为空，远程部署需设置密码
  database: "oj_db"
  pool_size: 4

# 日志配置
log:
  level: "info"           # debug / info / warn / error
  dir: "logs"
  filename: "oj.log"

# 代码执行配置
executor:
  timeout_sec: 5          # 运行超时（秒）
  cpu_limit_sec: 2        # CPU 时间限制（秒）
  mem_limit_mb: 256       # 内存限制（MB）
```

### 关键配置说明

| 配置项 | 说明 |
|--------|------|
| `server.host` | 设为 `0.0.0.0` 以允许外部访问 |
| `server.port` | 默认 8080，如被占用可修改 |
| `database.password` | 如果 MySQL 设置了 root 密码，需在此填写 |
| `executor.timeout_sec` | 用户代码运行超时上限 |
| `executor.cpu_limit_sec` | CPU 时间限制，超出触发 SIGXCPU → TLE |
| `executor.mem_limit_mb` | 内存限制，超出触发 RE |

---

## 6. 启动服务

### 6.1 前台启动（调试用）

```bash
./oj_server
```

服务器启动后输出：
```
[INFO] ===== OJ System starting =====
[INFO] db pool ready: localhost/oj_db pool_size=4
[INFO] HTTP server starting on 0.0.0.0:8080
```

### 6.2 后台启动（生产用）

```bash
nohup ./oj_server > logs/server.log 2>&1 &
```

### 6.3 验证服务

```bash
# 检查端口监听
ss -tlnp | grep 8080

# 测试 API
curl http://localhost:8080/api/problems
# 预期: {"code":200,"message":"ok","data":[...]}

# 浏览器访问
# http://<服务器IP>:8080
```

---

## 7. 防火墙配置

### 7.1 系统防火墙

```bash
# ufw (Ubuntu)
sudo ufw allow 8080/tcp
sudo ufw reload

# 或 iptables
sudo iptables -A INPUT -p tcp --dport 8080 -j ACCEPT
```

### 7.2 云服务商安全组

在云服务器控制台（腾讯云/阿里云/AWS）的安全组规则中添加：

| 方向 | 协议 | 端口 | 来源 |
|------|------|------|------|
| 入站 | TCP | 8080 | 0.0.0.0/0 |

---

## 8. 日志查看

日志文件位于 `logs/` 目录下：

```bash
# 实时查看日志
tail -f logs/oj.log

# 查看最近 100 行
tail -100 logs/oj.log

# 搜索错误
grep "ERROR" logs/oj.log
```

日志支持自动轮转（默认 10MB 单文件，保留 5 个历史文件）。

---

## 9. 停止服务

```bash
# 查找进程
ps aux | grep oj_server | grep -v grep

# 停止
kill <PID>

# 强制停止
kill -9 <PID>
```

---

## 10. 常见问题

### Q1: 启动报错 `database connection pool init failed`

**原因**：MySQL 未启动或连接参数错误。

**解决**：
```bash
sudo systemctl start mysql
mysql -u root -e "SELECT 1;"  # 验证 MySQL 可连接
```

检查 `config/config.yaml` 中的 database 配置是否正确。

### Q2: 编译报错 `cannot find -lyaml-cpp`

**原因**：未安装 yaml-cpp 开发库。

**解决**：
```bash
sudo apt install libyaml-cpp-dev
```

### Q3: 登录 admin 提示 `invalid username or password`

**原因**：数据库中的 admin 密码哈希不正确。

**解决**：重新运行管理员初始化工具：
```bash
./init_admin
```

### Q4: 提交代码后一直等待无响应

**原因**：g++ 未安装或不在 PATH 中。

**解决**：
```bash
which g++
# 如未安装
sudo apt install g++
```

### Q5: 外部无法访问 8080 端口

**原因**：防火墙或云安全组未放行。

**解决**：参照第 7 节配置防火墙和云安全组。

### Q6: 提交代码报 TLE 但代码本身不超时

**原因**：`cpu_limit_sec` 设置过小，或代码确实有性能问题。

**解决**：检查 `config/config.yaml` 中 `executor.cpu_limit_sec`，默认 2 秒。如需调整修改后重启服务。

---

## 11. 部署检查清单

- [ ] MySQL 已启动且 `oj_db` 数据库已创建
- [ ] `oj_server` 编译成功无警告
- [ ] `init_admin` 运行成功，管理员可登录
- [ ] `config/config.yaml` 配置正确
- [ ] 服务器启动后 `curl localhost:8080/api/problems` 返回 200
- [ ] 防火墙/安全组已放行 8080 端口
- [ ] 浏览器可访问 `http://<IP>:8080` 看到落地页
- [ ] 管理员可登录并在管理后台创建题目
- [ ] 用户可注册、登录、刷题、提交代码获得判定结果
- [ ] 日志文件正常写入 `logs/oj.log`
