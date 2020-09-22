#include <function>
#include <ctime>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <arpa/inet.h>
#include "Task.h"
using namespace std;

class Schedule
{
public:
    Schedule();

    //生成两个线程：1个是CreateConf函数，用来创建conf；另外1个是SendConf函数，用来发送conf
    void Run();

private:
    std::mutex mMutex;
    //用来存储服务器ip对应标号
    std::map<int, string> mMap;
    std::atomic<int> mTask;
    //用来缓存服务器都负载时候的configure
    std::queue<std::string> mBuffer;

    //由于会根据mBuffer中的ab任务比例进行服务器动态分配，用来标志目前第一台服务器最优分配方案
    int mP1a, mP1b;

    //标志mBuffer中A和B任务的个数
    std::atomic<int> mTaska, mTaskb;

    //每隔30s进行configure文件建立
    void CreateConf();

    //根据服务器运行情况，相关调度策略发送conf，并且动态调整服务器的任务分配
    void SendConf();

    //server用来获取各个服务器上任务是否已经结束的函数，暂时没有实现
    //vector记录的是各个服务器中任务开始的时间以及任务是否已经结束,如果已经结束那么就只有-1没有任务开始的时间
    //里面int存储的数据格式为，高位存储A||B的信息，低位存储的是task已经在该服务器上运行了多少时间
    void GetServerInfo(vector<vector<int>> &server);

    //用来解决当mBuffer中的AB任务比例偏离6:4的情况下的解决方案，动态优化A和B的服务器分配比例
    void TaskSchedule();

    //发送message消息到id对应的服务器上
    void SendServer(std::string message, int id);

    //根据mBuffer中的任务类型进行conf生成和相关负载均衡的策略发送到相对应的服务器
    void MakeConf(int a[][]);
};