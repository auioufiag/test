#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "Task.h"

#define MAX_LINK_NUM 128
#define SERV_PORT 8888
#define BUFF_LENGTH 320
#define MAX_EVENTS 5
using namespace std;
int count = 0;
int threadNum = 0;
std::vector<std::thread> thread;

//解析configure的日志数据
/*
解析数据示例：
    type=A;seed=45;clientIp=192.168.1.10:10000;
*/
void Parase(TaskInfo &info, char *buff)
{
    if (buff[5] == 'A')
        info.taskType = A;
    else
        info.taskType = B;
    info.seed = 0;
    int pre = 7;
    for (int i = pre;; i++)
    {
        if (pre == -1 && buff[i] != ';')
            info.seed = info.seed * 10 + buff[i] - '0';
        if (buff[i] == '=')
            pre = -1;
        if (buff[i] == ';')
        {
            pre = i + 1;
            break;
        }
    }
    info.ip.clear();
    for (int i = pre;; i++)
    {
        if (pre == -1 && buff[i] != ':')
            info.port += buff[i];
        if (buff[i] == '=')
            pre = -1;
        if (buff[i] == ':')
        {
            pre = i + 1;
            break;
        }
    }

    info.port = 0;
    for (int i = pre;; i++)
    {
        if (buff[i] == ';')
            break;
        info.port = info.port * 10 + buff[i] - '0';
    }
}

int tcp_epoll_server_init()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("socket error!\n");
        return -1;
    }

    struct sockaddr_in serv_addr;
    struct sockaddr_in clit_addr;
    socklen_t clit_len;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 任意本地ip

    int ret = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret == -1)
    {
        printf("bind error!\n");
        return -2;
    }

    listen(sockfd, MAX_LINK_NUM);

    //创建epoll
    int epoll_fd = epoll_create(MAX_EVENTS);
    if (epoll_fd == -1)
    {
        printf("epoll_create error!\n");
        return -3;
    }

    //向epoll注册sockfd监听事件
    struct epoll_event ev;                 //epoll事件结构体
    struct epoll_event events[MAX_EVENTS]; //事件监听队列

    ev.events = EPOLLIN;
    ev.data.fd = sockfd;

    int ret2 = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev);
    if (ret2 == -1)
    {
        printf("epoll_ctl error!\n");
        return -4;
    }
    int connfd = 0;
    while (1)
    {

        //epoll等待事件
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            printf("epoll_wait error!\n");
            return -5;
        }
        printf("nfds: %d\n", nfds);
        //检测
        for (int i = 0; i < nfds; ++i)
        {
            //客服端有新的请求
            if (events[i].data.fd == sockfd)
            {
                connfd = accept(sockfd, (struct sockaddr *)&clit_addr, &clit_len);
                if (connfd == -1)
                {
                    printf("accept error!\n");
                    return -6;
                }

                ev.events = EPOLLIN;
                ev.data.fd = connfd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connfd, &ev) == -1)
                {
                    printf("epoll_ctl add error!\n");
                    return -7;
                }

                printf("accept client: %s\n", inet_ntoa(clit_addr.sin_addr));
                printf("client %d\n", ++count);
            }
            //客户端有数据发送过来
            else
            {
                char buff[BUFF_LENGTH];

                //读取调度器发过来的configure
                int ret1 = read(connfd, buff, sizeof(buff));
                printf("Recved From Client:%s\n", buff);
                int sig = threadNum % 3;
                if (thread[sig].joinable())
                    thread[sig].join();
                TaskInfo info;

                //解析调度器schedule发过来的configure
                Parase(info, buff);
                Task temp(info);

                //启动一个线程运行对应的A或者B任务
                thread[sig] = std::thread(&Task::Run, temp);
                threadNum++;
            }
        }
    }
    close(connfd);
    return 0;
}

int main()
{
    //初始化该服务器上可以最大运行的线程任务个数
    thread.reserve(3);
    tcp_epoll_server_init();
}