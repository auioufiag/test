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
using namespace std;

enum TaskType
{
    A,
    B
};

//用来存储configure信息的结构体
struct TaskInfo
{
    TaskType taskType;
    int seed;
    string ip;
    int port;
};

int SolveA(const int &seed)
{
    srand(seed);
    return (seed - 100 + rand() % 200);
}

int SolveB(const int &seed)
{
    srand(seed);
    return (-100 + rand() % 200) / seed;
}

class Task
{
public:
    Task(const TaskInfo &info);
    //分别启动三个线程，分别是计算线程，定时线程，和发送线程
    int Run();

private:
    TaskInfo mInfo;
    void init();

    //用来标志线程结束
    void Stop();

    //计算线程
    void StartCal();

    //发送结果线程
    void StartSend();

    //定时线程
    void StartTime();
    std::mutex mMutex;
    std::function<int(TaskInfo)>
        f;

    //存储计算结果的buffer
    std::vector<int> mBuffer;

    //用来标志线程是不是已经结束
    std::atomic<bool> mStop;
};