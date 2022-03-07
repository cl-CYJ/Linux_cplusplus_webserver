# Linux_cplusplus_webserver

使用语言：C++

相关技能：Linux系统编程、网络编程、计算机网络

项目地址：https://github.com/cl-CYJ/Linux_cplusplus_webserver

参考资料：阅读《Linux高性能服务器编程》，《UNIX网络编程》部分章节

项目描述：用C++实现的高性能WEB服务器，经过webbench压力测试可实现上万的并发连接, 42000的QPS

1. 使用**RAII机制**封装锁，让线程更安全
2. 使用多线程充分利用多核CPU，并使用**线程池**避免线程频繁创建销毁的开销
3. 使用**EPOLL边沿触发**的IO多路复用技术，非阻塞IO，使用同步IO模拟Proactor模式
4. 利用状态机思想解析HTTP报文，支持**GET请求**
5. 使用**小根堆**实现定时器以关闭超时连接，解决超时连接占有系统资源的问题