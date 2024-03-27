#ifndef VICTOR_LOCK_H
#define VICTOR_LOCK_H
#include<exception>
#include<semaphore.h>
#include<pthread.h>
#include <stdexcept>
class sem{
private:
	sem_t m_sem;
public:
	sem(){
		if(sem_init(&m_sem,0,0)){
			throw std::runtime_error("pshared is nonzero, but the system does not support process-shared semaphores");
		}
	}
	explicit sem(int num){
		if(sem_init(&m_sem,0,num)){
			throw std::exception();
		}
	}
    sem(const sem& other) = delete;

    sem& operator=(const sem& other) = delete;
	~sem(){
		sem_destroy(&m_sem);
	}
	//P/V Semaphore
	bool wait(){
		return sem_wait(&m_sem)==0;
	}
	bool post(){
		return sem_post(&m_sem)==0;
	}
};
class locker{
private:
	pthread_mutex_t m_locker;
public:
	locker(){
		if(pthread_mutex_init(&m_locker,nullptr)){
			throw std::exception();
		}
	}
    locker(const locker& other) = delete;

    // 赋值运算符重载
    locker& operator=(const locker& other) = delete;
	~locker() noexcept {
		pthread_mutex_destroy(&m_locker);
	}
	bool lock(){
		return pthread_mutex_lock(&m_locker)==0;
	}
	bool unlock(){
		return pthread_mutex_unlock(&m_locker)==0;
	}
	pthread_mutex_t *get(){
		return &m_locker;
	}
};

class cond{
private:
	pthread_cond_t m_cond;
public:
	cond(){
		if(pthread_cond_init(&m_cond,nullptr)){
			throw std::exception();
		}
	}
    cond(const cond& other) = delete;

    // 赋值运算符重载
    cond& operator=(const cond& other) = delete;
	~cond() noexcept{
		pthread_cond_destroy(&m_cond);
	}

	bool wait(pthread_mutex_t* m_mutex){
		return pthread_cond_wait(&m_cond,m_mutex)==0;
	}
	
	bool timedwait(pthread_mutex_t* m_mutex,struct timespec t){
		return pthread_cond_timedwait(&m_cond,m_mutex,&t)==0;
	}

	bool signal(){
		return pthread_cond_signal(&m_cond)==0;
	}
	bool broadcast(){
		return pthread_cond_broadcast(&m_cond)==0;
	}
};
#endif
