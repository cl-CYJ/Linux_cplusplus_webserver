# Linux_cplusplus_webserver

## 项目概述

使用语言：C++

相关技能：Linux系统编程、网络编程、计算机网络

项目地址：https://github.com/cl-CYJ/Linux_cplusplus_webserver

参考资料：阅读《Linux高性能服务器编程》，《UNIX网络编程》部分章节

项目描述：用C++实现的高性能WEB服务器，经过webbench压力测试可实现上万的并发连接, 42000的QPS

1. 使用**RAII机制**封装锁，让线程更安全
2. 使用多线程充分利用多核CPU，并手写**线程池**避免线程频繁创建销毁的开销
3. 使用**EPOLL边沿触发**的IO多路复用技术，非阻塞IO，使用同步IO模拟Proactor模式
4. 利用状态机思想解析HTTP报文，支持**GET请求**
5. 基于**小根堆**实现定时器容器以关闭超时连接，解决超时连接占有系统资源的问题

## 项目结构

```shell
.
├── http
│   ├── http_conn.cpp
│   └── http_conn.h
├── lock
│   └── locker.h
├── main.cpp
├── Makefile
├── ProjectSchedule.md
├── README.md
├── resources
│   ├── test.txt
│   └── t.txt
├── server
├── test_tool
│   └── webbench-1.5
│       ├── webbench
│       └── webbench.c
├── threadpool
│   └── threadpool.h
└── timer
    ├── time_heap.cpp
    └── time_heap.h

7 directories, 15 files

```

## 项目启动

```shell
修改http_conn.cpp中的网站根目录 "doc_root"
执行: 
make server
./server 端口号
```

## 压力测试

[![b61tRU.png](https://s1.ax1x.com/2022/03/07/b61tRU.png)](https://imgtu.com/i/b61tRU)
[![b61yi6.png](https://s1.ax1x.com/2022/03/07/b61yi6.png)](https://imgtu.com/i/b61yi6)

[![b61gzD.png](https://s1.ax1x.com/2022/03/07/b61gzD.png)](https://imgtu.com/i/b61gzD)

[![b63KfK.png](https://s1.ax1x.com/2022/03/07/b63KfK.png)](https://imgtu.com/i/b63KfK)

```
./webbench-1.5/webbench -c 客户端数量 -t 访问时间 http://ip:port/  （注意网址格式需要严格按照示例要求）
若报权限不够的错误，执行 chmod +x webbench 即可
```

### 测试环境：

- 操作系统：Ubuntu18.04
- cpu：i5-8265U

### 测试结果：
支持上万用户并发，QPS   40000+ ，但由于webbench也是在本地虚拟机上运行，当webbench指定的用户数过多后测试结果明显变差（fork( )子进程太消耗资源）

