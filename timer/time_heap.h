#ifndef _TIME_HEAP_H__
#define _TIME_HEAP_H__
 
#include <stdio.h>
#include <netinet/in.h>
#include <time.h>
 
class heap_timer;
 
//用户数据结构
struct client_data {
    sockaddr_in address; //客户端地址
    int sockfd; //通信socket
    heap_timer *timer; //定时器
};
 
//定时器
class heap_timer {
    
public:
    heap_timer(int delay) {
        expire = time(NULL) + delay;
    }
 
public:
    int position;                   //定时器在堆中的位置
    time_t expire;                  //定时器生效的绝对时间
    void (*cb_func)(client_data*);  //定时器的回调函数
    client_data *user_data;         //客户端数据
};
 
//时间堆，用数组模拟实现
class time_heap {

public:
    //构造之一：初始化一个大小为cap的空堆
    time_heap(int cap);
 
    //构造之二：用数组来初始化堆
    time_heap(heap_timer **init_array, int size, int capacity);
    
    //销毁时间堆
    ~time_heap();
 
    //添加定时器timer
    int add_timer(heap_timer *timer);
    
    //删除定时器timer
    void del_timer(heap_timer *timer);
 
    //获得堆顶部的定时器
    heap_timer* top() const;
 
    //删除堆顶部的定时器
    void pop_timer();
 
    //心跳函数
    void tick();
 
    //堆是否为空
    bool empty()const { return cur_size == 0; }
 
    //调整更新了expire的定时器在堆中的位置
    void adjust_node(heap_timer* node);

private:    
    //最小堆的下潜操作，
    //确保堆数组中认第hole个节点作为根的子树拥有最小堆性质
    void percolate_down(int hole);
    //将堆数组容量扩大1倍
    void resize();
    //交换堆中的两个heap_timer
    void heap_swap(int pos1, int pos2);

private:
    heap_timer **array;     //堆数组
    int capacity;           //堆数组的容量
    int cur_size;           //堆数组当前包含元素个数
 
};
 
#endif //_TIME_HEAP_H__