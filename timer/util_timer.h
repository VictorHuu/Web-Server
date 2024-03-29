#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <assert.h> 
#include <chrono>
#include<sys/epoll.h>
#include "../log/log.h"
constexpr int TIMESLOT=5;
constexpr int BUFFER_SIZE=64;
struct util_timer_node;
struct client_data
{
    sockaddr_in address;

    int sockfd;
	char buf[ BUFFER_SIZE ];
    util_timer_node* timer;
    ~client_data()=default;
};
void m_cb_func( client_data* user_data );
struct util_timer_node {
	int id;
    client_data* user_data;
    time_t expire;
	void (*cb_func)( const client_data* );
    bool operator<(const util_timer_node& t) const{
        return expire < t.expire;
    }
	util_timer_node(int id_,client_data* ud,int timeout,void(*cb_)(const client_data*)):id(id_),user_data(ud),
    cb_func(cb_){
		time_t cur = time( nullptr );
		expire=cur+timeout;
	}
	util_timer_node():user_data(nullptr),cb_func(nullptr){
		time_t cur = time( nullptr );
		expire=cur+3*TIMESLOT;
	}
};
class heap_util_timer {
public:
    heap_util_timer() { heap_.reserve(m_capacity); }
    heap_util_timer& operator=(const heap_util_timer& other)=delete;
    heap_util_timer(const heap_util_timer& other)=delete;
    ~heap_util_timer() { clear(); }
    
    void adjust_timer(util_timer_node* node, int newExpires=3*TIMESLOT);

    void add_timer(util_timer_node* node);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int GetNextTick();

	void del_timer(util_timer_node* node);
private:
    
    size_t getIndex(util_timer_node* node) ;
    void shiftup_(size_t i);

    bool siftdown_(size_t index, size_t n);

    void SwapNode_(size_t i, size_t j);

    std::vector<util_timer_node*> heap_;

    std::unordered_map<int, size_t> ref_;
	size_t m_capacity;
	size_t m_size;
};

#endif //HEAP_TIMER_H
