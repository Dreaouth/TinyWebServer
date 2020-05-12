#include "webserver.h"


WebServer::WebServer(){
    users = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    //定时器
    // users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete[] users;
    // delete[] users_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string password, string databaseName, int sqlverify, int logwrite,
    int trigmode, int opt_linger, int sql_num, int thread_num, int close_log){
        m_port = port;
        m_user = user;
        m_passWord = password;
        m_databaseName = databaseName;
        m_SQLVerify = sqlverify;
        m_sql_num = sql_num;
        m_log_write = logwrite;
        m_TRIGMode = trigmode;
        m_close_log = close_log;
        m_OPT_LINGER = opt_linger;
        m_thread_num = thread_num;
}

bool WebServer::deal_clientData(){
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (m_TRIGMode == 0){   // LT触发模式
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0){
            printf("epoll error is %d!\n", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD){
            printf("server busy!\n");
            return false;
        }
        printf("get an client,connfd is %d\n", connfd);
        users[connfd].init(connfd, client_address, m_root, m_users_passwd, m_SQLVerify, m_TRIGMode, m_close_log, m_user, m_passWord, m_databaseName);
    }
    else{   //ET触发模式
        while(1){
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0){
                // printf("epoll errno2 is %d!\n", errno);  //正常现象
                break;
            }
            if (http_conn::m_user_count >= MAX_FD){
                printf("server busy!\n");
                break;
            }
            users[connfd].init(connfd, client_address, m_root, m_users_passwd, m_SQLVerify, m_TRIGMode, m_close_log, m_user, m_passWord, m_databaseName);
        }
        return false;
    }
    return true;
}

void WebServer::dealwith_read(int sockfd){
    //
    //proactor模式，即主线程先读数据，然后工作线程处理
    if (users[sockfd].read()){
        m_pool->append(users + sockfd);
    }
    else{
        printf("get an error when deal with read\n");
    }
}

void WebServer::dealwith_write(int sockfd){
    //
    if (users[sockfd].write()){
        printf("success send data to the client!\n");
    }
}

void WebServer::thread_pool(){
    //先使用无数据库版本threadpool
    m_pool = new threadpool<http_conn>(m_thread_num, MAXEVENT_NUMBER);
}

void WebServer::eventListen(){
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    if(m_OPT_LINGER == 0){
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if(m_OPT_LINGER == 1){
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = PF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int ret = bind(m_listenfd, (struct sockaddr*) &address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    //工具类，信号和描述符（先不写）
    Utils::u_epollfd = m_epollfd;
    Utils::u_pipefd = m_pipefd;
    // utils.init(timer_list, TIMESLOT);

    epoll_event events[MAXEVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listenfd, false, m_TRIGMode);
    http_conn::m_epollfd = m_epollfd;
}


void WebServer::eventLoop(){ 
    bool timeout = false;
    bool stop_server = false;
    while(!stop_server){
        int number = epoll_wait(m_epollfd, events, MAXEVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            printf("epoll failure!\n");
            break;
        }
        for(int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;
            if(sockfd == m_listenfd){
                int flag = deal_clientData();
                if(!flag){
                    continue;
                }
            }
            else if(events[i].events & EPOLLIN){
                dealwith_read(sockfd);
            }
            else if(events[i].events & EPOLLOUT){
                dealwith_write(sockfd);
            }
        }
    }
}