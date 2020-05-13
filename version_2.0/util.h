#ifndef UTIL_H
#define UTIL_H

#include "http_conn.h"

class Utils{
public:
    Utils(){};
    ~Utils(){};

    // void init(sort_timer_lit timer_lst, int timeslot);
    
    int setnonblocking(int fd);
    void addfd(int epollfd, int fd, bool one_shot, int TRIG_MODE);
    static void sig_handler(int sig);
    void addsig(int sig, void(handler)(int), bool restart = true);
    void time_handler();
    void show_error(int connfd, const char *info);
public:
    static int *u_pipefd;
    // sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};


#endif