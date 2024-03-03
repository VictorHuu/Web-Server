#include "util_timer.h"
static int epollfd = 0;
void m_cb_func( client_data* user_data )
{
    epoll_ctl( epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0 );
    assert( user_data );
    close( user_data->sockfd );
    char ip[INET_ADDRSTRLEN]="";
    inet_ntop(AF_INET,&user_data->address.sin_addr.s_addr,ip,INET6_ADDRSTRLEN);
    printf( "close fd %d from %s\n", user_data->sockfd,ip);
}
size_t heap_util_timer::getIndex(util_timer_node *node)
{
    int len=heap_.size();
    for(int i=0;i<len;i++){
        if(heap_[i]->id==node->id){
            return i;
        }
    }
    return 0;
}
void heap_util_timer::shiftup_(size_t i)
{
    if(i >= heap_.size()){
        LOG_ERROR("Index i %lu out of range %lu",i,heap_.size());
        return;
    }
    size_t j = (i - 1) / 2;
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void heap_util_timer::SwapNode_(size_t i, size_t j) {
    if(i >= heap_.size()){
        LOG_ERROR("Index i %lu out of range %lu",i,heap_.size());
        return;
    }
    if(j >= heap_.size()){
        LOG_ERROR("Index j %lu out of range %lu",j,heap_.size());
        return;
    }
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i]->id] = i;
    ref_[heap_[j]->id] = j;
} 

bool heap_util_timer::siftdown_(size_t index, size_t n) {
    if(index >= heap_.size()){
        LOG_ERROR("Index index %lu out of range %lu",index,heap_.size());
        return false;
    }
    if(n > heap_.size()){
        LOG_ERROR("Index n %lu out of range %lu",n,heap_.size());
        return false;
    }
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

void heap_util_timer::doWork(int id) {
    if(heap_.empty()){
        LOG_ERROR("Can't remove timer from an empty heap");
        return;
    }
    if(ref_.count(id)==0){
        LOG_ERROR("The node you want to adjust doesn't exist in the node");
        return;
    }
    size_t i = ref_[id];
    util_timer_node* node = heap_[i];
    node->cb_func(node->user_data);
    del_timer(node);
}

void heap_util_timer::del_timer(util_timer_node* node) {
    if(node==NULL){
        LOG_FATAL("The node is NULL hence there're no id");
        return;
    }
    int index=getIndex(node);
    if(heap_.empty()){
        LOG_ERROR("Can't remove timer from an empty heap");
        return;
    }
    if(index>=heap_.size()){
        LOG_ERROR("Index %d out of range %d",index,heap_.size());
        return;
    }
    size_t i = index;
    size_t n = heap_.size() - 1;

    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            shiftup_(i);
        }
    }
    heap_.pop_back();
}

void heap_util_timer::adjust_timer(util_timer_node* node, int timeout) {

    if(node==NULL){
        LOG_FATAL("The node is NULL hence there're no id");
        return;
    }

    /* 调整指定id的结点 */
    int id=node->id;
    if(heap_.empty()){
        LOG_ERROR("Can't remove timer from an empty heap");
        return;
    }
    if(ref_.count(id)==0){
        LOG_ERROR("The node %d you want to adjust doesn't exist in the node",id);
        return;
    }

    time_t cur=time(NULL);
    heap_[ref_[id]]->expire = node->expire;

    siftdown_(ref_[id], heap_.size());

}

void heap_util_timer::add_timer(util_timer_node *node)
{
    LOG_DEBUG("the new id is %d,there're %d nodes",node->id,heap_.size());
    size_t i;
    i = heap_.size();
    ref_[node->id]=i;
    heap_.push_back(node);
    shiftup_(i);
}

void heap_util_timer::tick() {
    if(heap_.empty()) {
        return;
    }
    printf( "timer tick\n" );
    while(!heap_.empty()) {
        util_timer_node* node = heap_.front();
        time_t cur=time(NULL);
        if(node->expire-cur>0)
            break;
        node->cb_func(node->user_data);
        pop();
    }
}

void heap_util_timer::pop() {
    if(heap_.empty()){
        LOG_ERROR("Can't pop timer from an empty heap");
        return;
    }
    del_timer(heap_[0]);
}

void heap_util_timer::clear() {
    ref_.clear();
    heap_.clear();
}

int heap_util_timer::GetNextTick() {
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        int cur=time(NULL);
        res=heap_.front()->expire-cur;
        if(res < 0) { res = 0; }
    }
    return res;
}