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
#include <iostream>
#include <assert.h>
#include "/home/cyj/linux/Linux_cplusplus_webserver/lock/locker.h"
#include "/home/cyj/linux/Linux_cplusplus_webserver/threadpool/threadpool.h"
#include "/home/cyj/linux/Linux_cplusplus_webserver/http/http_conn.h"
#include "/home/cyj/linux/Linux_cplusplus_webserver/timer/time_heap.h"

#define MAX_FD 66535 // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 11000 // epoll监听的文件描述符的最大数量
#define TIMESLOT 5 //最小超时单位
 
//设置定时器相关参数
static int pipefd[2];
static int epollfd = 0;
static time_heap client_time_heap(2048);

//添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);
//从epoll中删除文件描述符
extern void removefd(int epollfd, int fd);
//修改文件描述符
extern void modfd(int epollfd, int fd, epoll_event event);
//设置文件描述符非堵塞
extern void setnonblocking(int fd);

//信号处理函数
void sig_handler(int sig) {

    printf("收到了一次信号并写入管道\n");
    
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

 //添加信号捕捉
void addsig(int sig, void(handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

//定时处理任务: 每TIMESLOT执行一次tick并重新定时
void timer_handler() {
    client_time_heap.tick();
    alarm(TIMESLOT);
}

//定时器回调函数，删除非活动连接在socket上的注册事件，并关闭
void cb_func(client_data *user_data) {
    #ifdef DEBUG
    
        printf("%d : 执行了回调函数\n", user_data->sockfd);
    
    #endif 

    removefd(epollfd, user_data->sockfd);
    http_conn::m_user_count--;
    printf("close fd %d\n", user_data->sockfd);
}

int main(int argc, char* argv[]) {

    if (argc <= 1) {
        printf("请按如下格式运行: %s port_number\n", basename(argv[0]));
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
        printf("error on creating threadpool\n");
        throw std::exception();
        exit(-1);
    }

    //创建一个数组用于保存所有客户端信息
    http_conn* users = new http_conn[MAX_FD];
    
    //创建用户数据结构，包含客户端socket地址，socket文件描述符与定时器指针
    client_data *users_data = new client_data[MAX_FD];

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
    epollfd = epoll_create(10);

    //将监听的文件描述符添加到epoll对象中,监听文件描述符单独使用LT模式  
    {
        epoll_event levent;
        levent.data.fd = listenfd;
        levent.events = EPOLLIN | EPOLLRDHUP;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &levent);
        setnonblocking(listenfd);
    }
    http_conn::m_epollfd = epollfd;

    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    //设置管道读端为ET非阻塞模式
    addfd(epollfd, pipefd[0], false);


    addsig(SIGALRM, sig_handler);
    addsig(SIGTERM, sig_handler);
    bool stop_server = false;

    //每隔TIMESLOT时间触发SIGALRM信号
    alarm(TIMESLOT);
    //标记有定时任务需要处理
    bool timeout = false;

    while (!stop_server) {
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

                #ifdef DEBUG
                
                    printf("来了新用户, 他的sockfd为%d\n", connfd);
                
                #endif 

                //将新的客户的数据初始化，放到数组中
                users[connfd].init(connfd, cliaddr);

                //初始化client_data数据
                //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到小根堆中
                users_data[connfd].address = cliaddr;
                users_data[connfd].sockfd = connfd;
                heap_timer* timer = new heap_timer(3 * TIMESLOT);
                if (timer) {
                    timer->cb_func = cb_func;
                    timer->user_data = &users_data[connfd];
                    ret = client_time_heap.add_timer(timer);
                    #ifdef DEBUG
                    
                        printf("ret: %d\n", ret);
                    
                    #endif 
                    if (ret == -1) {
                        fprintf(stderr, "client: %d add heap_timer failed.\n", connfd);
                    } else {
                        users_data[connfd].timer = timer;
                    }            
                } else {
                    fprintf(stderr, "client: %d add heap_timer failed.\n", connfd);
                }

            } else if ((sockfd == pipefd[0]) && (event[i].events & EPOLLIN)) {
                //处理信号
                int sig;
                char signals[1024];
                //这里只会有SIGALRM与SIGTERM信号
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1)
                {
                    continue;
                }
                else if (!ret)
                {
                    continue;
                } else {
                    for (int i = 0; i < ret; i ++) {
                        if (signals[i] == SIGALRM) {
                            //用timeout变量标记有定时任务需要处理，但并不立即处理定时任务
                            //定时任务的优先级不是很高，优先处理其他更重要的任务。
                            timeout = true;
                            break;
                        } else if (signals[i] == SIGTERM) {
                            stop_server = true;
                        }
                    }
                }
            } else if (event[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //对方异常断开或错误等事件
                users[sockfd].close_conn();
            } else if (event[i].events &  EPOLLIN) {
                heap_timer *timer = users_data[sockfd].timer;
                //处理客户连接上接收到的数据，一次性把所有数据都读完
                if (users[sockfd].read()) {    
                    //若监测到读事件，将该事件放入请求队列               
                    pool->append(users + sockfd);

                    //若有数据传输，则将定时器往后延迟3个单位
                    //并对新的定时器在堆上的位置进行调整
                    if (timer) {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        client_time_heap.adjust_node(timer);
                        #ifdef DEBUG
                        
                            std::cout << "读数据后cd_fucn : " << timer->cb_func << std::endl;
                        
                        #endif 
                    }
                    #ifdef DEBUG
                    
                        printf("读到新数据, 执行了定时器的更新\n");
                    
                    #endif 

                } else {
                    if (timer) {
                        client_time_heap.del_timer(timer);
                    }
                    users[sockfd].close_conn();
                }
            } else if (event[i].events & EPOLLOUT) {
                //一次性把所有数据写完
                heap_timer *timer = users_data[sockfd].timer;
                if (!users[sockfd].write()) {
                    if (timer) {
                        client_time_heap.del_timer(timer);
                    }
                    users[sockfd].close_conn();
                }
                //若有数据传输，则将定时器往后延迟3个单位
                //并对新的定时器在堆上的位置进行调整
                if (timer) {
                    time_t cur = time(NULL);
                    timer->expire = cur + 3 * TIMESLOT;
                    client_time_heap.adjust_node(timer);
                    #ifdef DEBUG
                    
                        printf("有数据写到客户端, 执行了定时器的更新\n");  

                    #endif 
                }
            }
        }

        if (timeout) {
            timer_handler();
            timeout = false;
        }
    }

    close(epollfd);
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete[] users;
    delete[] users_data;
    delete pool;

    return 0;
}