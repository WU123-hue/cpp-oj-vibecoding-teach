# 依赖安装说明

> 目标系统：空白 Ubuntu 24.04
> 对应文档：[SPEC.md](./SPEC.md)

---

## 一、一键安装命令

```bash
sudo apt update && sudo apt install -y \
    build-essential cmake pkg-config \
    libssl-dev \
    nlohmann-json3-dev \
    libcrypt-dev \
    libzip-dev
```

---

## 二、依赖清单

| 依赖 | 安装包 | 对应 SPEC 用途 |
|---|---|---|
| C++ 编译器 / make | `build-essential` | 后端 C++ 编译；判题模块 `g++ -std=c++17` 编译用户代码（§3、§4.1） |
| 构建系统 | `cmake` | `CMakeLists.txt`（cpp-httplib + MySQL + pthread，§项目结构） |
| 库发现工具 | `pkg-config` | CMake 中查找 MySQL/zip 等库 |
| MySQL 服务端 | `mysql-server` | 数据库（users/problems/test_cases/submissions 四张表，§数据模型） |
| MySQL 客户端开发库 | `libmysqlclient-dev` | `dao/mysql_pool` 连接池、各 DAO 层访问 MySQL（`mysql.h`） |
| OpenSSL | `libssl-dev` | cpp-httplib 可选依赖（MVP 用 HTTP，一并安装以便后续 HTTPS） |
| JSON 库 | `nlohmann-json3-dev` | `common/json` 封装、读取 `config.json`、API JSON 序列化（§5 API 契约） |
| crypt（含 bcrypt） | `libcrypt-dev` | `utils/crypto` 密码 bcrypt 加盐哈希（`crypt()` 的 `$2y$` 前缀，§4.6.3） |
| libzip | `libzip-dev` | `utils/file_utils` 解压测试用例 zip 包（§4.5 打包上传） |

---

## 三、无需安装（项目中 vendored）

- **cpp-httplib**：单头文件，置于 `third_party/cpp-httplib/`（§3 技术栈）
- **CodeMirror**：前端编辑器，置于 `static/vendor/codemirror/`（§6 P4 做题页）
- **pthread**：随 glibc 提供，`build-essential` 已含

---

## 四、补充说明

1. **bcrypt 实现选择**：若不想依赖 `libcrypt-dev`，可改为 vendor 一个独立的 bcrypt 库（如 `libbcrypt`）放入 `third_party/`。SPEC 强调"单头文件依赖、零额外依赖"的轻量风格，两种方式均符合。
2. **g++ 版本**：Ubuntu 24.04 默认 GCC 13，完整支持 C++17，满足 SPEC 中 `-std=c++17` 要求。
3. **MySQL 初始化**：安装后需运行 `sudo mysql_secure_installation` 并执行 `sql/schema.sql` 建库建表（含预置管理员，§4.6.6）。
