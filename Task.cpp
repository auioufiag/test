#include "Task.h"
using namespace std;

void Task::init()
{
    if (mInfo.taskType == A)
        f = SolveA;
    else
        f = SolveB;
}

Task::Task(const TaskInfo &info) : mInfo(info), mStop(false)
{
    init();
}

void Task::StartSend()
{

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("socket failed!\n");
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(mInfo.port);
    serv_addr.sin_addr.s_addr = inet_addr(mInfo.ip);

    int ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret == -1)
        printf("connect error!\n");

    std::vector<int> temp;
    while (!mStop)
    {
        temp.clear();
        {
            std::lock_guard<std::mutex> lock(mMutex);
            temp.swap(mBuffer);
        }
        string t;
        t.clear();
        for (int i = 0; i < temp.size(); i++)
        {
            t += to_string(temp[i]);
            t += ';'
        }
        int len = t.length;
        write(sockfd, t.c_str(), len);
        while (sockfd != -1)
        {

        } //等待
    }
    close(sockfd);
}

void Task::StartCal()
{
    while (!mStop)
    {
        int result = f(mInfo.seed);
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mBuffer.emplace_back(result);
        }
    }
    return;
}

void Task::Run()
{
    cout << "Begin To Calculate................" << endl;
    std::thread threadCal(&Task::StartCal, this);
    std::thread threadSend(&Task::StartSend, this);
    std::thread threadTime(&Task::StartTime, this);
    threadTime.join();
    threadCal.join();
    threadSend.join();
    return;
}

void Task::Stop()
{
    mStop = true;
    return;
}

void Task::StartTime()
{
    int time;
    srand(mInfo.seed);
    if (mInfo.taskType == A)
        time = 150 + rand() % 60;
    else
        time = 250 + rand() % 150;
    sleep(time);
    mStop = true;
    return;
}