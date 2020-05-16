#ifndef CONFIG_H
#define CONFIG_H

using namespace std;


class Config{
public:
    Config(){
        PORT = 6666;
        SQLVerify = 0;
        LOGWrite = 0;
        TRIGMode = 1;
        OPT_LINGER = 0;
        sql_num = 8;
        thread_num = 8;
        close_log = 0;
    }
    
    ~Config(){

    }

    //端口号
    int PORT;

    //数据库校验方式：0同步，1异步
    int SQLVerify;

    //日志写入方式：0同步，1异步
    int LOGWrite;

    //触发模式：0 LT，1 ET
    int TRIGMode;

    //优雅关闭链接，0为不使用
    int OPT_LINGER;

    //数据库连接池数量
    int sql_num;

    //线程池内的线程数量
    int thread_num;

    //是否关闭日志，1关闭
    int close_log;
};

#endif