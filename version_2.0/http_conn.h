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
    enum METHOD{
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE{
        NO_REQUEST,         //请求不完整，需要继续等待
        GET_REQUEST,        //获得了完整请求，调用do_request()完成资源映射
        BAD_REQUEST,        //请求报文有错误
        NO_RESOURCE,        //请求资源不存在
        FORBIDDEN_REQUEST,  //请求资源禁止访问
        FILE_REQUEST,       //请求资源可以访问，调用process_write()完成响应报文
        INTERNAL_ERROR,     //服务器内部错误
        CLOSED_CONNECTION
    };
    enum LINE_STATUS{
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

    void init(int sockfd, const sockaddr_in &addr, char *root, map<string, string> &users, int SQLverify,
int TRIGMode, int close_log, string user, string passwd, string sqlname);
    void close_conn(bool real_close = true);
    bool write();
    bool read();
    void process();
    sockaddr_in *get_address(){
        return &m_address;
    }
    // void initmysql_result(connection_pool *connPool, map<string, string> &);
    // void initresultFile(connection_pool *connPool, map<string, string> &);


public:
    static int m_epollfd;
    static int m_user_count;
    //MYSQL *mysql;

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() {return m_readbuf + m_start_line; }
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();


private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_readbuf[READ_BUFFER_SIZE];
    int m_read_idx;
    int m_check_idx;
    int m_start_line;
    char m_writebuf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        //是否启用POST
    char *m_string; //存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;

    map<string, string> m_users;
    int m_SQLVerify;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};



#endif