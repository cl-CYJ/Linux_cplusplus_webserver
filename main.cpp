#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <signal.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 66535 // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 // 监听文件描述符的最大数量
 
 //添加信号捕捉
void addsig(int sig, void(handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

//添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);
//从epoll中删除文件描述符
extern void removefd(int epollfd, int fd);
//修改文件描述符
extern void modfd(int epollfd, int fd, epoll_event event);
//设置文件描述符非堵塞
extern void setnonblocking(int fd);

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printf("请按如下格式运行：%s port_number\n", basename(argv[0]));
        exit(-1);
    }

    //获取端口号
    int port = atoi(argv[1]);
    
    //对SIGPIPE信号进行处理
    addsig(SIGPIPE, SIG_IGN);

    //创建线程池，初始化线程池
    threadpool<http_conn>* pool = NULL;
    try{
        pool = new threadpool<http_conn>;
    } catch(...) {
        printf("error here\n");
        throw std::exception();
        exit(-1);
    }

    //创建一个数组用于保存所有客户端信息
    http_conn* users = new http_conn[MAX_FD];

    //创建监听的套接字
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    //设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

    //绑定   
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(listenfd, (sockaddr*)&addr, sizeof(addr));

    //监听   
    listen(listenfd, 5);

    //创建epoll对象，事件数组，添加
    epoll_event event[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(10);

    //将监听的文件描述符添加到epoll对象中,监听文件描述符单独使用LT模式  
    {
        epoll_event levent;
        levent.data.fd = listenfd;
        levent.events = EPOLLIN | EPOLLRDHUP;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &levent);
        setnonblocking(listenfd);
    }
    http_conn::m_epollfd = epollfd;

    while (1) {
        int num = epoll_wait(epollfd, event, MAX_EVENT_NUMBER, -1);
        if ((num < 0) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }

        //循环遍历事件数组
        for (int i = 0; i < num; i ++) {
            int sockfd = event[i].data.fd;
            if (sockfd == listenfd) {
                //有客户端连接
                sockaddr_in cliaddr;
                socklen_t cliaddrlen = sizeof(cliaddr);
                int connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &cliaddrlen);

                if (http_conn::m_user_count >= MAX_FD) {
                    //目前连接数满了
                    close(connfd);
                    continue;
                }

                 //将新的客户的数据初始化，放到数组中
                users[connfd].init(connfd, cliaddr);
            } else if (event[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //对方异常断开或错误等事件
                users[sockfd].close_conn();
            } else if (event[i].events &  EPOLLIN) {
                //一次性把所有数据都读完
                if (users[sockfd].read()) {                   
                    pool->append(users + sockfd);
                } else {
                    users[sockfd].close_conn();
                }
            } else if (event[i].events & EPOLLOUT) {
                //一次性把所有数据写完
                if (!users[sockfd].write()) {
                    users[sockfd].close_conn();
                }
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;

    return 0;
}