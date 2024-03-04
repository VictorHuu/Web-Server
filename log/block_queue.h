#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/lock.h"
using namespace std;

template <class T>
class block_queue
{
public:
    block_queue(int max_size = 1000);

    void clear();

    ~block_queue();
    //判断队列是否满了
    bool full();
    //判断队列是否为空
    bool empty();
    //返回队首元素
    bool front(T &value);
    //返回队尾元素
    bool back(T &value);

    int size();

    int max_size();

    bool push(const T &item);
    //pop时,如果当前队列没有元素,将会等待条件变量
    bool pop(T &item);

    //增加了超时处理
    bool pop(T &item, int ms_timeout);


private:
    locker m_mutex;
    cond m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};

#endif