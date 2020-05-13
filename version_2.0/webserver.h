#ifndef WEBSERVER_H
#define WEBSERVER_H

#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<stdlib.h>
#include<cassert>
#include<sys/epoll.h>


#include "ThreadPool.h"
#include "http_conn.h"
#include "util.h"
#include "locker.h"

using namespace std;

const int MAX_FD = 65536;        //最大文件描述符
const int MAXEVENT_NUMBER = 10000;  //最大事件数
const int TIMESLOT = 5;         //最小超时时间

class WebServer{
public:
    WebServer();
    ~WebServer();

    void init(int port, string user, string password, string databaseName, int sqlverify, int logwrite,
    int trigmode, int opt_linger, int sql_num, int thread_num, int close_log);

    void thread_pool();
    void sql_pool();
    void log_write();

    void eventListen();
    void eventLoop();
    void timer(int connfd, struct socketaddr_in client_address);
    // void adjust_timer(util_timer *timer);
    // void deal_timer(util_timer *timer, int sockfd);
    bool deal_clientData();
    bool dealwith_signal(bool timeout, bool &stop_server);
    void dealwith_read(int socketfd);
    void dealwith_write(int socketfd);

private:
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    
    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    //数据库相关
    // connection_pool *m_connPool;
    string m_user;         //登陆数据库用户名
    string m_passWord;     //登陆数据库密码
    string m_databaseName; //使用数据库名
    int m_sql_num;
    int m_SQLVerify;
    map<string, string> m_users_passwd;

    //线程池相关
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAXEVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;

    //定时器相关
    // sort_timer_lst timer_lst;
    // client_data *users_timer;
    Utils utils;
};



#endif