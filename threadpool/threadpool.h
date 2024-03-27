#ifndef VICTOR_THREADPOOL_H
#define VICTOR_THREADPOOL_H
#include<queue>
#include<cstdio>
#include<vector>
#include<exception>
#include<pthread.h>
#include<semaphore.h>

template <typename T>
class threadpool{
public:
	threadpool(int thread_num=8,int max_requests=10000);
    threadpool& operator=(const threadpool& other)=delete;
    threadpool(const threadpool& other)=delete;
	~threadpool();
	bool append(T *request, int state);

    static void *worker(void *arg);
    void run();
private:
	int m_thread_num;
	int m_max_requests;
	std::vector<pthread_t> m_threads;
	std::queue<T*> m_workqueue;
	pthread_mutex_t  m_queuelocker;
	pthread_attr_t m_attr;
	sem_t  m_queuestat;
	bool m_actor_model;

};
#include"threadpool.cc"
#endif

