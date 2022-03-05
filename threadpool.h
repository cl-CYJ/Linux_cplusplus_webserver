#ifndef _THREADPOOL_H__ 
#define _THREADPOOL_H__

#include <pthread.h>
#include <queue>
#include <exception>
#include <cstdio>
#include "locker.h"

//线程池类，定义为模板类方便代码复用，模板参数T为任务类
template<typename T>
class threadpool {
public:
    threadpool(int thread_num = 8, int max_requests = 1000) :
    m_thread_number(thread_num), m_max_requests(max_requests),
    m_stop(false), m_threads(NULL) {
        if (thread_num <= 0 || max_requests <= 0) {
            throw std::exception();
        }

        m_threads = new pthread_t[m_thread_number];
        if (!m_threads) {
            throw std::exception();
        }

        //创建thread_number个线程，并设置线程分离
        for (int i = 0; i < thread_num; i ++) {
            if (pthread_create(m_threads + i, NULL, worker, this)) {
                delete[] m_threads;
                throw std::exception(); 
            }
            printf("create the %dth thread\n", i + 1);

            if (pthread_detach(m_threads[i])) {
                delete[] m_threads;
                throw std::exception();
            }
        }
    }
    ~threadpool() {
        delete[] m_threads;
        m_stop = true;
    }
    bool append(T* request) {
        m_queuelocker.lock();
        if (m_workqueue.size() >= m_max_requests) {
            m_queuelocker.unlock();
            return false;
        }

        m_workqueue.push(request);
        m_queuelocker.unlock();
        m_queuestat.post();
        return true;
    }

private:
    void run() {
        while (!m_stop) {
            m_queuestat.wait();
            m_queuelocker.lock();
            if (m_workqueue.empty()) {
                m_queuelocker.unlock();
                m_queuestat.post();
                continue;
            }

            T* request = m_workqueue.front();
            m_workqueue.pop();
            m_queuelocker.unlock();
            if (!request) {
                continue;
            }
            request->process();
        }
    }

    static void* worker(void* arg) {
        threadpool * pool = (threadpool*) arg;
        pool->run();
        return pool;
    }

private:
    //线程数量
    int m_thread_number;
    
    //线程池数组，大小为m_thread_number;
    pthread_t * m_threads;

    //请求队列中至多允许的等待处理的请求数量
    int m_max_requests;

    //请求队列
    std::queue<T*> m_workqueue;

    //互斥锁
    locker m_queuelocker;

    //信号量用来判断是否有任务需要处理
    sem m_queuestat;

    //是否结束线程
    bool m_stop;
};

#endif //_THREADPOOL_H__