#ifndef UTIL_H
#define UTIL_H

#include "http_conn.h"
#include "timer.h"

class Utils{
public:
    Utils(){};
    ~Utils(){};

    void init(int timeslot);
    
    int setnonblocking(int fd);
    void addfd(int epollfd, int fd, bool one_shot, int TRIG_MODE);
    static void sig_handler(int sig);
    void addsig(int sig, void(handler)(int), bool restart = true);
    void time_handler();
    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    static int u_epollfd;
    int m_TIMESLOT;
};


#endif