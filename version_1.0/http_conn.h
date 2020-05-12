#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<sys/stat.h>
#include<string.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/mman.h>
#include<stdarg.h>
#include<errno.h>
#include<sys/wait.h>
#include<sys/uio.h>
#include<map>
using namespace std;

#include "locker.h"

class http_conn{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;

public:
    http_conn() {}
    ~http_conn() {}
    void init(int sockfd, const sockaddr_in &addr, char *root, map<string, string> &users, int SQLverify,
int TRIGMode, int close_log, string user, string passwd, string sqlname);
    void close_conn(bool real_close = true);
    bool write();
    bool read();
    void process();

public:
    static int m_epollfd;
    static int m_user_count;

private:
    void init();
    void process_read();
    void process_write();

private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_readbuf[READ_BUFFER_SIZE];
    char m_writebuf[WRITE_BUFFER_SIZE];
    int m_read_idx;
    int m_write_idx;
    int m_TRIGMode;
    int m_close_log;
};



#endif