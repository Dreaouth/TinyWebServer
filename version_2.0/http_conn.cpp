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
    m_SQLVerify = SQLverify;
    doc_root = root;
    m_users = users;
    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());
    

    addfd(m_epollfd, m_sockfd, true, m_TRIGMode);
    //数据库等

    init();
}

void http_conn::init(){

    m_read_idx = 0;
    m_check_idx = 0;
    m_start_line = 0;
    m_write_idx = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_host = 0;
    m_content_length = 0;
    m_linger = false;
    m_file_address = 0;
    bytes_to_send = 0;
    bytes_have_send = 0;
    
    memset(m_real_file, '\0', FILENAME_LEN);
    memset(m_readbuf, '\0', READ_BUFFER_SIZE);
    memset(m_writebuf, '\0', WRITE_BUFFER_SIZE);
}

//从状态机，用于分析出一行内容
//返回 LINE_STATUS 分别有LINE_OPEN、LINE_OK、LINE_BAD
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    for (; m_check_idx < m_read_idx; ++m_check_idx){
        temp = m_readbuf[m_check_idx];
        if (temp == '\r'){
            if ((m_check_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_readbuf[m_check_idx + 1] == '\n'){
                m_readbuf[m_check_idx++] = '\0';
                m_readbuf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            else{
                return LINE_BAD;
            }
        }
        else if (temp == '\n'){
            if (m_check_idx > 1 && m_readbuf[m_read_idx - 1] == '\r'){
                m_readbuf[m_check_idx - 1] = '\0';
                m_readbuf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//解析请求行
http_conn::HTTP_CODE http_conn::parse_request_line(char *text){
    // m_url是包含" \t"中任意一个字符后面的字符串
    m_url = strpbrk(text, " \t");
    printf("m_url = %s\n", m_url);
    if (!m_url){
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0){
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0){
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析请求头
http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    //如果是空行，代表请求头结束
    if (text[0] == '\0'){
        if (m_content_length != 0){
            m_check_state == CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0){
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
            m_linger = true;
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0){
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else {
        //其他头部先不解析
        printf("others header not parse\n");
    }
    return NO_REQUEST;
}

//解析请求消息体
http_conn::HTTP_CODE http_conn::parse_content(char *text){
    if (m_read_idx >= (m_content_length + m_check_idx)){
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read(){
    char *text = 0;
    HTTP_CODE ret = NO_REQUEST;
    LINE_STATUS line_status = LINE_OK;
    // 按行来解析，读一行解析一行
    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || (line_status = parse_line()) == LINE_OK){
        text = get_line();
        printf("text is %s\n", text);
        m_start_line = m_check_idx;
        switch (m_check_state){
        case CHECK_STATE_REQUESTLINE:
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
        case CHECK_STATE_HEADER:
            ret = parse_headers(text);
            if (ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            else if (ret == GET_REQUEST){
                return do_request();
            }
            break;
        case CHECK_STATE_CONTENT:
            ret = parse_content(text);
            if (ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            else if (ret == GET_REQUEST){
                return do_request();
            }
            line_status = LINE_OPEN;
            break;
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
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
        else if (bytes_read == 0){
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
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST){
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret){
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
    // strcpy(m_writebuf, m_readbuf);
    // char rec_str[] = " from server,haha";
    // strcat(m_writebuf, rec_str);
    // modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}