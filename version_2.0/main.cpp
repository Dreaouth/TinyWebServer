#include<iostream>

#include "config.h"
#include "webserver.h"

using namespace std;

int main(int argc, char *argv[]){
    
    string user = "root";
    string passwd = "root";
    string databasename = "liujun";

    Config config;
    WebServer server;
    server.init(config.PORT, user, passwd, databasename, config.SQLVerify, config.LOGWrite, config.TRIGMode, config.OPT_LINGER, config.sql_num, config.thread_num, config.close_log);
    cout << "config is ok" << endl;
    server.thread_pool();
    cout << "threadpool is start" << endl;
    server.eventListen();
    cout << "listen is ok" << endl;
    server.eventLoop();

    return 0;
}