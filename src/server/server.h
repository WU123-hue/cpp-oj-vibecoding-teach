#ifndef OJ_SERVER_H
#define OJ_SERVER_H

namespace oj {

// 初始化并启动 HTTP 服务器（阻塞）
// 返回时表示服务器已停止
bool StartServer();

}  // namespace oj

#endif  // OJ_SERVER_H
