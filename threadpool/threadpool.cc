#include <stdexcept>
#include"threadpool.h"
#ifndef VICTOR_THREADPOOL_CC
#define VICTOR_THREADPOOL_CC
template <typename T>
threadpool<T>::threadpool(int thread_num, int max_requests)
{
	
	if(thread_num<0||max_requests<0)
		throw std::invalid_argument("The thread_num and max_requests must be greater than 0");
	m_thread_num=thread_num;
	m_max_requests=max_requests;
	
	m_threads.reserve(m_thread_num);
	
	if (pthread_attr_init(&m_attr) != 0) {
        perror("pthread_attr_init");
        throw std::runtime_error("pthread_attr_init failed");
    }

    // 设置线程为分离状态
    if (pthread_attr_setdetachstate(&m_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("pthread_attr_setdetachstate");
        pthread_attr_destroy(&m_attr);
        throw std::runtime_error("pthread_attr_setdetachstate failed");
    }
	for(int i=0;i<m_thread_num;i++){
		printf( "create the %dth thread\n", i);
		pthread_t threadId;
		if(pthread_create(&threadId,&m_attr,worker,this)){
			perror("pthread_create");
			throw std::runtime_error("pthread_create");
		}
		m_threads.emplace_back(threadId);
	}
    m_threads.shrink_to_fit();
	
	pthread_mutex_init(&m_queuelocker,nullptr);
	sem_init(&m_queuestat,1,8);
	
}

template <typename T>
threadpool<T>::~threadpool()
{
	m_threads.clear();
	pthread_mutex_destroy(&m_queuelocker);
	sem_destroy(&m_queuestat);
	pthread_attr_destroy(&m_attr);
}

template <typename T>
bool threadpool<T>::append(T *request,[[ maybe_unused ]] int state)
{
	pthread_mutex_lock(&m_queuelocker);
    if(m_workqueue.size()>m_max_requests){
		pthread_mutex_unlock(&m_queuelocker);
		return false;
	}


	m_workqueue.push(request);
	pthread_mutex_unlock(&m_queuelocker);
	sem_post(&m_queuestat);
	return true;
}

template <typename T>
inline void *threadpool<T>::worker(void *arg)
{

    auto pool=(threadpool*)arg;
	pool->run();
	return pool;
}

template <typename T>
inline void threadpool<T>::run()
{

	while(true){
		sem_wait(&m_queuestat);
		pthread_mutex_lock(&m_queuelocker);
		if(m_workqueue.empty()){
			pthread_mutex_unlock(&m_queuelocker);
			continue;
		}

		T* request=m_workqueue.front();
		m_workqueue.pop();
		pthread_mutex_unlock(&m_queuelocker);
		if(!request)
			continue;
		request->process();
	}
}
#endif
