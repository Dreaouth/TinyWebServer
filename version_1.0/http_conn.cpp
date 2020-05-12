#include "http_conn.h"

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int sockfd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = sockfd;
    if(TRIGMode == 1){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else{
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);
    setnonblocking(sockfd);
}

void removefd(int epollfd, int sockfd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, 0);
    close(sockfd);
}

void modfd(int epollfd, int sockfd, int ev, int TRIGMode){
    epoll_event event;
    event.data.fd = sockfd;
    if (TRIGMode == 1){
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    }
    else {
         event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &event);
}

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, map<string, string> &users, int SQLverify,
int TRIGMode, int close_log, string user, string passwd, string sqlname){
    m_sockfd = sockfd;
    m_address = addr;
    m_user_count++;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    addfd(m_epollfd, m_sockfd, true, m_TRIGMode);
    //数据库等

    init();
}

void http_conn::init(){
    m_write_idx = 0;
    m_read_idx = 0;
    memset(m_readbuf, '\0', READ_BUFFER_SIZE);
    memset(m_writebuf, '\0', WRITE_BUFFER_SIZE);
}

bool http_conn::read(){
    if (m_read_idx >= READ_BUFFER_SIZE){
        printf("READ_BUFFER FULL!\n");
        return false;
    }
    int bytes_read = 0;
    while(true){
        bytes_read = recv(m_sockfd, m_readbuf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1){
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if(bytes_read == 0){
            return false;
        }
        m_read_idx += bytes_read;
        printf("get %d bytes of client data from sockfd %d\n", bytes_read, m_sockfd);
    }
    return true;
}

bool http_conn::write(){
    //先不考虑长连接短链接问题了
    if (!m_writebuf){
        printf("WRITE_BUFFER is NULL!\n");
        return false;
    }
    int ret = send(m_sockfd, m_writebuf, strlen(m_writebuf), 0);
    if (ret != strlen(m_writebuf)){
        printf("write is not over!\n");
    }
    modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
    init();
    return true;
}

void http_conn::process(){
    strcpy(m_writebuf, m_readbuf);
    char rec_str[] = " from server,haha";
    strcat(m_writebuf, rec_str);
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}