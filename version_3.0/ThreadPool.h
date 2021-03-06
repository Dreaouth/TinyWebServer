#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<pthread.h>
#include<list>
#include<cstdio>
#include<stdio.h>
#include<exception>
#include "locker.h"

template<typename T>
class threadpool{
public:
    threadpool() {}
    threadpool(int thread_number = 8, int max_request = 10000);
    // threadpool(connection_pool *connPool = NULL, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request);

private:
    //工作线程运行的函数
    static void *worker(void *arg);
    void run();

    int m_thread_number;
    int m_max_request;
    pthread_t *m_threads;
    std::list<T *> m_workqueue;
    locker m_queuelocker;
    sem m_queuestat;
    // connection_pool *m_connPool;   //数据库连接池
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_request) : m_thread_number(thread_number), m_max_request(max_request){
    if(thread_number <= 0 || max_request <= 0){
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    for(int i = 0; i < thread_number; ++i){
        if(pthread_create(m_threads+i, NULL, worker, this) != 0){
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw std::exception();
        }
    }
}

/*
template<typename T>
threadpool<T>::threadpool(connection_pool *connPool = NULL, int thread_number = 8, int max_request = 10000) : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL),m_connPool(connPool){
    if(thread_number <= 0 || max_request <= 0){
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    for(int i = 0; i < thread_number; i++){
        if(pthread_create(pthreads+i, NULL, worker, NULL) != 0){
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw std::execption();
        }
    }
}
*/


template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template<typename T>
bool threadpool<T>::append(T *request){
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_request){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void *threadpool<T>::worker(void *arg){
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    while(true){
        m_queuestat.wait();
        m_queuelocker.lock();
        if( m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            continue;
        }
        // connectionRAII mysqlcon(&request->mysql, m_connPool);
        request->process();     //调用http_conn中的process函数
    }
}

#endif