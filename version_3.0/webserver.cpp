#include "webserver.h"


WebServer::WebServer(){
    users = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/www";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    //定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete[] users;
    delete[] users_timer;
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
    // LT触发模式
    if (m_TRIGMode == 0){   
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
        timer(connfd, client_address);
    }
    //ET触发模式
    else{   
        while(1){
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0){
                // 非阻塞ET模式正常现象
                break;
            }
            if (http_conn::m_user_count >= MAX_FD){
                printf("server busy!\n");
                break;
            }
            users[connfd].init(connfd, client_address, m_root, m_users_passwd, m_SQLVerify, m_TRIGMode, m_close_log, m_user, m_passWord, m_databaseName);
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

void WebServer::dealwith_read(int sockfd){
    util_timer *timer = users_timer[sockfd].timer;
    //proactor模式，即主线程先读数据，然后工作线程处理逻辑
    if (users[sockfd].read()){
        m_pool->append(users + sockfd);
        if (timer){
            adjust_timer(timer);
        }
    }
    else{
        delete_timer(timer, sockfd);
    }
}

void WebServer::dealwith_write(int sockfd){
    util_timer *timer = users_timer[sockfd].timer;
    if (users[sockfd].write()){
        if (timer){
            adjust_timer(timer);
        }
    }
    else{
        delete_timer(timer, sockfd);
    }
}

void WebServer::thread_pool(){
    //先使用无数据库版本threadpool
    m_pool = new threadpool<http_conn>(m_thread_num, MAXEVENT_NUMBER);
}


//创建定时器，初始化client_data
void WebServer::timer(int connfd, struct sockaddr_in client_address){
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->cb_func = cb_func;
    timer->user_data = &users_timer[connfd];
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    timer_lst.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer){
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;  
    timer_lst.adjust_timer(timer);
}

//删除该定时器（指发生读或写错误是删除定时器）
void WebServer::delete_timer(util_timer *timer, int sockfd){
    timer->cb_func(&users_timer[sockfd]);
    if (timer){
        timer_lst.del_timer(timer);
        printf("delete timer when read or write wrong\n");
    }
}

bool WebServer::dealwith_signal(bool &timeout, bool &stop_server){
    int ret = 0;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1 || ret == 0){
        return false;
    }
    else {
        for (int i = 0; i < ret; ++i){
            switch (signals[i])
            {
            case SIGALRM:
                timeout = true;
                break;
            case SIGTERM:
                stop_server = true;
                break;
            }
        }
    }
    return true;
}


void WebServer::eventListen(){
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    //优雅关闭连接
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

    //创建epoll内核事件表
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listenfd, false, m_TRIGMode);
    http_conn::m_epollfd = m_epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);

    //工具类，信号和描述符
    Utils::u_epollfd = m_epollfd;
    Utils::u_pipefd = m_pipefd;
    utils.init(TIMESLOT);

    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, m_TRIGMode);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);
}

void WebServer::eventLoop(){
    //timeout变量表示有定时任务需要处理，但不是立即处理，因为定时任务的优先级低于其他任务
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
            if (sockfd == m_listenfd){
                int flag = deal_clientData();
                if(!flag){
                    continue;
                }
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                util_timer *timer = users_timer[sockfd].timer;
                delete_timer(timer, sockfd);
            }
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)){
                bool flag = dealwith_signal(timeout, stop_server);
                if (!flag){
                    continue;
                }
            }
            else if (events[i].events & EPOLLIN){
                dealwith_read(sockfd);
            }
            else if (events[i].events & EPOLLOUT){
                dealwith_write(sockfd);
            }
        }
        //有信号到来
        if (timeout){
            timer_lst.tick();
            alarm(TIMESLOT);
            timeout = false;
        }
    }
}