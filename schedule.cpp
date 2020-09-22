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
#include "schedule.h"
using namespace std;

Schedule::Schedule()
{
    //初始化第一台服务器上A和B的任务分配比例
    mP1a = 2;
    mP1b = 3;

    mTask = mTaska = mTaskb = 0;
    mMap[0] = "xxx.xxx.xxx.0";
    mMap[1] = "xxx.xxx.xxx.1";
    mMap[2] = "xxx.xxx.xxx.2";
}

void Schedule::Run()
{
    //一个线程运行30s创建conf的函数
    std::thread createConf(&Schedule::CreateConf, this);

    //一个线程运行发送conf的函数
    std::thread sendConf(&Schedule::SendConf, this);
    createConf.join();
    sendConf.join();
}

void Schedule::CreateConf()
{
    while (1)
    {
        sleep(30);
        string temp;
        temp.clear();
        srand((unsigned)time(NULL));
        int seed = (rand() % 200) - 100;

        //根据百分比生成AB任务
        int type = rand() % 10;
        temp += "type=";
        if (type <= 5)
            temp += 'A;';
        else
            temp += 'B;';
        temp += "seed=" + to_string(seed) + ";clientIp=";
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mTask > 20)
                printf("Too Many Buffers Here !!!!!!!!!!!!!\n");
            else
            {
                mTask++;
                mBuffer.push(temp);
                if (type <= 5)
                    mTaska++;
                else
                    mTaskb++;
            }
        }
    }
}

//用来解决当buffer中的AB任务比例偏离6:4的情况下的解决方案，优化A和B的服务器分配比例
//计算公式：mbuffer中A任务的个数*A任务的期望运行时间/运行A任务的服务器个数≈≈mbuffer中B任务的个数*B任务的期望运行时间/运行B任务的服务器个数
void Schedule::TaskSchedule()
{
    //只针对一定数量的buffer进行动态调优
    if (mTask > 10)
    {
        int temp = (180 * mTaska * 8 - 325 * mTaskb * 3) / (180 * mTaska + 325 * mTaskb);
        if (temp < 0)
            mP1a = 0, mP1b = 5;
        if (temp >= 5)
            mP1a = 5, mP1b = 0;
        if (temp >= 0 && temp < 5)
        {
            double diff1 = abs(1.0 * mTaska * 180 / (temp + 3) - 1.0 * mTaskb * 325 / (8 - temp));
            double diff2 = abs(1.0 * mTaska * 180 / (temp + 4) - 1.0 * mTaskb * 325 / (7 - temp));
            if ((diff1 - diff2) < 1e-6)
                mP1a = temp, mP1b = 5 - temp;
            else
                mP1a = temp + 1, mP1b = 4 - temp;
        }
    }
}

void Schedule::SendServer(std::string message, int id)
{
    //初始化socket套接字，发送message
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("socket failed!\n");
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(9999);
    serv_addr.sin_addr.s_addr = inet_addr(map[id]);

    int ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret == -1)
        printf("connect error!\n");

    int len = message.length();
    write(sockfd, message.c_str(), len);
    while (sockfd != -1)
    {

    } //等待
    close(sockfd);
}

void Schedule::SendConf()
{
    vector<vector<int>> temp;
    temp.reserve(3);
    temp[0].reserve(5);
    temp[1].reserve(3);
    temp[2].reserve(3);
    //获得各个服务器上运行任务的信息
    while (1)
    {
        GetServerInfo(temp);

        int p[3][2];
        memset(p, 0, sizeof(p));
        for (int i = 0; i < temp.size(); i++)
        {
            for (int j = 0; j < temp[i].size(); j++)
            {
                p[i][1]++;
                p[i][0]++;
            }
        }
        //根据buffer中的任务类型进行conf生成和相关策略发送到相对应的服务器
        MakeConf(p);
        //根据buffer中的A和B的任务比例，对服务器进行优化分配比例
        TaskSchedule();
        sleep(5);
    }
}

void Schedule::MakeConf(int a[][])
{
    while (mTask)
    {
        std::string temp = mBuffer.top();
        //按照负载均衡进行分配configure分配给server
        if (temp[5] == 'A')
        {
            if (p[0][0] + p[0][1] < p[1][0])
            {
                if (mP1a - p[0][0] > 0)
                {
                    temp += mMap[0] + ":10000;";
                    SendServer(temp, 0);
                }
                else if (p[1][0] < 3)
                {
                    temp += mMap[1] + ":10000;";
                    SendServer(temp, 1);
                }
                else
                    break;
            }
            else if (p[1][0] < 3)
            {
                temp += mMap[1] + ":10000;";
                SendServer(temp, 1);
            }
            else
                break;
            mTaska--;
        }
        else
        {
            if (p[0][0] + p[0][1] < p[2][0])
            {
                if (mP1b - p[0][1] > 0)
                {
                    temp += mMap[0] + ":10000;";
                    SendServer(temp, 0);
                }
                else if (p[2][0] < 3)
                {
                    temp += mMap[2] + ":10000;";
                    SendServer(temp, 2);
                }
                else
                    break;
            }
            else if (p[2][0] < 3)
            {
                temp += mMap[2] + ":10000;";
                SendServer(temp, 2);
            }
            else
                break;
            mTaskb--;
        }
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mBuffer.pop();
            mTask--;
        }
    }
}

void Schedule::GetServerInfo(vector<vector<int>> &server)
{
    //server用来获取各个服务器上任务是否已经结束的函数，暂时没有实现
    //vector记录的是各个服务器中任务开始的时间以及任务是否已经结束,如果已经结束那么就只有-1没有任务开始的时间
    //里面int存储的数据格式为，高位存储A||B的信息，低位存储的是task已经在该服务器上运行了多少时间
}