#include "http_conn.h"

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";


int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;
locker m_lock;

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

void http_conn::close_conn(bool real_close){
    if (real_close && (m_sockfd != -1)){
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
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
            m_check_state = CHECK_STATE_CONTENT;
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
    else if (strncasecmp(text, "Content-Length:", 15) == 0){
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
        //printf("others header not parse\n");
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

//处理url中的请求资源
http_conn::HTTP_CODE http_conn::do_request(){
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    const char *p = strrchr(m_url, '/');
    //cgi部分后面再处理


    if (*(p + 1) == '0'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);        
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '1'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '5'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '6'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '7'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else{
        //表示"/"，即欢迎界面
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    }
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    
    int fd = open(m_real_file, O_RDONLY);
    //将m_real_file中的文件读到m_file_stat
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}


bool http_conn::process_write(HTTP_CODE ret){
    switch (ret)
    {
    case INTERNAL_ERROR:
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    case BAD_REQUEST:
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    case FORBIDDEN_REQUEST:
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    case FILE_REQUEST:
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0){
            add_headers(m_file_stat.st_size);
            //m_writebuf记录响应头内容
            //m_file_address记录返回的html页面
            m_iv[0].iov_base = m_writebuf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else{
            const char *ok_string = "<html><body>empty string!</body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;  
        }
    default:
        return false;
    }
    m_iv[0].iov_base = m_writebuf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

bool http_conn::add_response(const char *format, ...){
    if (m_write_idx >= WRITE_BUFFER_SIZE){
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_writebuf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)){
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status, const char *title){
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_length){
    return add_content_length(content_length) && add_content_type() && add_linger() && add_blank_line();
}

bool http_conn::add_content_type(){
    return add_response("Content-Type:%s\r\n", "text/html");
}

bool http_conn::add_content_length(int content_length){
    return add_response("Content-Type:%d\r\n", content_length);
}

bool http_conn::add_linger(){
    return add_response("Connection:%s\r\n", (m_linger == true? "keep-alive" : "close"));
}

bool http_conn::add_blank_line(){
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char *content){
    return add_response("%s", content);
}

//释放内存
void http_conn::unmap(){
    if (m_file_address){
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
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
    int temp = 0;
    int newadd = 0;
    if (bytes_to_send <= 0){
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return false;
    }
    while(1){
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp >= 0){
            bytes_have_send += temp;
            newadd = bytes_have_send - m_write_idx;
        }
        else{
            if (errno == EAGAIN){
                //表示响应头部分已经write完毕，正在write返回的html
                if (m_iv[0].iov_len <= bytes_have_send){
                    m_iv[0].iov_len = 0;
                    m_iv[1].iov_base = m_file_address + newadd;
                    m_iv[1].iov_len = bytes_to_send;
                }
                //响应头部分还没有write完毕
                else{
                    m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
                    m_iv[0].iov_base = m_writebuf + bytes_have_send;
                }
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            //发生其他错误，释放内存
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        if (bytes_to_send <= 0){
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
            if (m_linger){
                init();
                return true;
            }
            else{
                return false;
            }
        }
    }
}