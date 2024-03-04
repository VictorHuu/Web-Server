# Web-Server
涉及领域：Linux系统编程、Linux网络编程、Makefile、C++11、HTML协议

## 特点
1. 使用 线程池 + 非阻塞socket + epoll(ET) + 事件处理(Proactor) 的并发模型
2. 使用状态机解析HTTP请求报文，支持解析GET,HEAD,DELETE,OPTIONS请求
3. 可以请求服务器PDF，XML文档，图片和视频文件等多种格式
4. 实现同步/异步日志系统，记录服务器运行状态
5. 使用Makefile一键生成可执行文件

## 使用方法
```bash
make
./webserver [PORT]
```

