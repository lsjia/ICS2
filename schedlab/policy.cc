#include "policy.h"
#include <algorithm>
#include <utility>
std::vector<std::pair<Event, int>> eventlist; // 存放已知事件

bool comp(const std::pair<Event, int> &a, const std::pair<Event, int> &b) // 比较优先级的函数
{
    if (a.second != b.second)
        return a.second > b.second;
    else
        return (a.first.task.deadline - a.first.task.arrivalTime) < (b.first.task.deadline - b.first.task.arrivalTime);
}

Action policy(const std::vector<Event> &events, int current_cpu,
              int current_io)
{

    int size = events.size();
    int next_cpu = 0, next_io = current_io;
    Action action;
    bool flag_ioend = false;
    int highestPrior = 7;
    for (int i = 0; i < size; i++)
    {
        if (events[i].type == Event::Type::kTaskFinish)
        {
            for (auto iter = eventlist.begin(); iter != eventlist.end(); iter++)
            {
                if (iter->first.task.taskId == events[i].task.taskId)
                {
                    if (!eventlist.empty())
                    {
                        eventlist.erase(iter);
                    }
                    break;
                }
            }
        }
        else if (events[i].type == Event::Type::kTaskArrival)
        {
            eventlist.push_back(std::make_pair(events[i], highestPrior)); // 加入列表中
        }
        else if (events[i].type == Event::Type::kIoRequest)
        {
            for (auto iter = eventlist.begin(); iter != eventlist.end(); iter++)
            {
                if (iter->first.task.taskId == events[i].task.taskId)
                {
                    iter->first.type = Event::Type::kIoRequest;
                    break;
                }
            }
        }
        else if (events[i].type == Event::Type::kIoEnd)
        {
            if (events[i].task.taskId == current_io)
                flag_ioend = true;
            for (auto iter = eventlist.begin(); iter != eventlist.end(); iter++)
            {
                if (iter->first.task.taskId == events[i].task.taskId)
                {
                    iter->first.type = Event::Type::kIoEnd;
                    break;
                }
            }
        }
    }
    if (events[0].time % 10 == 0) // 周期性
    {
        for (auto iter = eventlist.begin(); iter != eventlist.end(); iter++)
        {
            iter->second = highestPrior;
        }
    }

    sort(eventlist.begin(), eventlist.end(), comp); // 降序排序

    bool flag_cpu = false;
    bool flag_io = false;

    for (auto iter = eventlist.begin(); iter != eventlist.end(); iter++) // 遍历先前存储的事件
    {

        if (iter->first.type != Event::Type::kIoRequest && !flag_cpu) // 不需要io
        {
            next_cpu = iter->first.task.taskId;
            flag_cpu = true;
        }

        if ((current_io == 0 || flag_ioend) && iter->first.type == Event::Type::kIoRequest && !flag_io)
        {
            next_io = iter->first.task.taskId;
            flag_io = true;
        }
        if (next_cpu && next_io) // 及时退出
            break;
    }
    if (current_io != 0)
        next_io = current_io;

    action.cpuTask = next_cpu;
    action.ioTask = next_io;

    for (auto iter = eventlist.begin(); iter != eventlist.end(); iter++)
    {
        if (iter->first.task.taskId == next_cpu && iter->second > 0)
        {
            iter->second--;
            break;
        }
    }

    return action;
}