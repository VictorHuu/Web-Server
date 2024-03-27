#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
//#include <pthread.h>
#include "timer/util_timer.h"
#include"http_conn/http_conn.h"
#include"threadpool/threadpool.h"
#include"config/config.h"
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5
class fd_util;


extern void addfd( int epollfd, int fd, bool one_shot );
extern void removefd( int epollfd, int fd );


#define ERROR_HANDLE(func) do{ \
    LOG_FATAL("%s:%s",#func,strerror(errno));\
    sleep(1);\
    exit(EXIT_FAILURE);\
}while(0)

class epoll_util;
class fd_util{
public:
    static int pipefd[2];
    static heap_util_timer timer_lst;
    static int epollfd;
    static int cnt;
    static void addfd( int epfd, int fd )
    {
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET |EPOLLOUT;
        epoll_ctl( epfd, EPOLL_CTL_ADD, fd, &event );
        setnonblocking( fd );
    }
    static void sig_handler( int sig )
    {
        int save_errno = errno;
        int msg = sig;
        send( fd_util::pipefd[1], ( char* )&msg, 1, 0 );
        errno = save_errno;
    }
    static void addsig(int sig, void( handler )(int)=sig_handler){
        struct sigaction sa;
        memset( &sa, '\0', sizeof( sa ) );
        sa.sa_handler = handler;
        sigfillset( &sa.sa_mask );
        assert( sigaction( sig, &sa, nullptr ) != -1 );
    }
    static void timer_handler()
    {
        timer_lst.tick();
        alarm(TIMESLOT);
    }
    static void cb_func( const client_data* user_data )
    {
        epoll_ctl( fd_util::epollfd, EPOLL_CTL_DEL, user_data->sockfd, nullptr );
        assert( user_data );
        close( user_data->sockfd );
        char ip[INET_ADDRSTRLEN]="";
        inet_ntop(AF_INET,&user_data->address.sin_addr.s_addr,ip,INET6_ADDRSTRLEN);
        printf( "close fd %d from %s\n", user_data->sockfd,ip);
    }
};
int fd_util::pipefd[2]={0,0};
heap_util_timer fd_util::timer_lst;
int fd_util::epollfd=0;
int fd_util::cnt=0;

class epoll_util{
private:


    int sockfd;
    int listenfd;
    client_data* users;
    http_conn* userconns;
    bool timeout;
    int ret;
    epoll_event events[ MAX_EVENT_NUMBER ];
    threadpool<http_conn>* pool;
    bool stop_server;
    bool init_state;

    int handle_listener(){
        struct sockaddr_in client_address;
        socklen_t client_addrlength = sizeof( client_address );
        int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );

        if ( connfd < 0 ) {
            printf( "errno is: %d\n", errno );
            return -1;
        }

        if( http_conn::m_user_count >= FD_LIMIT ) {
            close(connfd);
            return -1;
        }

        users[connfd].address = client_address;
        users[connfd].sockfd = connfd;


        auto timer = new util_timer_node(fd_util::cnt++,&users[connfd],3 * TIMESLOT,fd_util::cb_func);
        users[connfd].timer = timer;
        fd_util::timer_lst.add_timer( timer );

        userconns[connfd].init( connfd, client_address);
        return 0;
    }
    int handle_sig(){
        char signals[1024];
        ret = static_cast<int> (recv( fd_util::pipefd[0], signals, sizeof( signals ), 0 ));
        if( ret == -1 ) {
            return -1;
        } else if( ret == 0 ) {
            return -1;
        } else  {
            for( int j = 0; j < ret; ++j ) {
                switch( signals[j] )  {
                    case SIGALRM:
                    {

                        timeout = true;
                        break;
                    }
                    case SIGTERM:
                    {
                        stop_server = true;
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        return 0;
    }
    void handle_in(){

        if(userconns[sockfd].read()) {
            pool->append(userconns + sockfd,0);
        } else {
            userconns[sockfd].close_conn();
        }

        util_timer_node* timer = users[sockfd].timer;
        if( ret < 0 )
        {

            if( errno != EAGAIN )
            {
                fd_util::cb_func( &users[sockfd] );
                if( timer )
                {
                    fd_util::timer_lst.del_timer( timer );
                }
            }
        }
        else if( ret == 0 )
        {

            fd_util::cb_func( &users[sockfd] );
            if( timer )
            {
                fd_util::timer_lst.del_timer( timer );
            }
        }
        else
        {

            if( timer ) {
                printf( "adjust timer once\n" );
                fd_util::timer_lst.adjust_timer( timer,3 * TIMESLOT );
                LOG_DEBUG("Here!")
            }
        }
    }

    int configure(int argc, char* argv[]) const{
        Config::getInstance().parse_arg(argc,argv);

        if(const Config& config_=Config::getInstance();
                config_.get_help()||config_.get_port()<=1023||config_.get_threadnum()<=0
                ||config_.get_logname()==nullptr||strlen(config_.get_logname())==0||
                config_.get_level()==UNKNOWN){
            struct stat statbuf;
            int manual_fd = open( "manual.txt", O_RDONLY );
            if(manual_fd==-1)
                ERROR_HANDLE(open);
            if(fstat(manual_fd,&statbuf)==-1)
                ERROR_HANDLE(fstat);
            //map then write to the stdout
            auto manual_address = ( char* )mmap( nullptr, statbuf.st_size, PROT_READ, MAP_PRIVATE, manual_fd, 0 );
            if(manual_address==MAP_FAILED)
                ERROR_HANDLE(mmap);
            struct iovec iov[2];
            char errmsg[100]="";
            if(config_.get_threadnum()<=0)
                snprintf(errmsg,100,"%sThe threadnum must be greater than 0,but it's %d now%s\n",COLOR_FATAL,config_.get_threadnum(),COLOR_RESET);
            else if(config_.get_port()<=1023)
                snprintf(errmsg,100,"%sThe port must be set and greater than 1023,but it's %d now%s\n",COLOR_FATAL,config_.get_port(),COLOR_RESET);
            else if(config_.get_logname()==nullptr||strlen(config_.get_logname())==0)
                snprintf(errmsg,100,"%sThe name of the log can't be nullptr%s\n",COLOR_FATAL,COLOR_RESET);
            else if(config_.get_level()==UNKNOWN)
                snprintf(errmsg,100,"%sThe Log Level must be one of the following %s[DEBUG/INFO/WARN/ERROR/FATAL]%s\n",COLOR_FATAL,BOLD,COLOR_RESET);
            iov[1].iov_base=manual_address;
            iov[1].iov_len=statbuf.st_size;
            iov[0].iov_base=errmsg;
            iov[0].iov_len=strlen(errmsg);
            if(writev(STDOUT_FILENO,iov,2)==-1)
                ERROR_HANDLE(writev);
            //Free the resources
            munmap(manual_address,statbuf.st_size);
            close(manual_fd);
            return -1;
        }
        return 0;
    }
    void init(int argc, char* argv[]){
        if(configure(argc,argv)==-1) {
            init_state=false;
        }
#define config_ Config::getInstance()
        Log::getInstance()->init(config_.get_logname(),config_.get_closelog(),1024,1000,2);
        Log::getInstance()->setLevel(config_.get_level());
        printf( "usage: %d port_number\n", config_.get_port());
        int port = config_.get_port();

        fd_util::addsig( SIGPIPE, SIG_IGN );

        pool = nullptr;
        try {
            pool = new threadpool<http_conn>(config_.get_threadnum());
        } catch( std::invalid_argument&) {
            init_state=false;
        }

        userconns = new http_conn[ FD_LIMIT ];
        users = new client_data[FD_LIMIT];

        listenfd = socket( PF_INET, SOCK_STREAM, 0 );

        ret = 0;
        struct sockaddr_in address;
        memset(&address,0,sizeof (address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons( static_cast<uint16_t>(port) );

        // 端口复用
        int reuse = 1;
        setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
        ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
        if(ret==-1){
            ERROR_HANDLE(bind);
        }
        ret = listen( listenfd, 5 );
        if(ret==-1){
            ERROR_HANDLE(listen);
        }
        fd_util::epollfd = epoll_create( 5 );

        addfd( fd_util::epollfd, listenfd, false );
        http_conn::m_epollfd = fd_util::epollfd;

        ret = socketpair(PF_UNIX, SOCK_STREAM, 0, fd_util::pipefd);
        setnonblocking( fd_util::pipefd[1] );
        fd_util::addfd( fd_util::epollfd, fd_util::pipefd[0]);
        fd_util::addsig( SIGALRM );
        fd_util::addsig( SIGTERM );
        stop_server = false;
        timeout = false;
        alarm(TIMESLOT);
        init_state=true;
    }
    void process(const int& i){
        if( sockfd == listenfd )
        {
            if(handle_listener()==-1)
                return;
        }else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) ) {

            userconns[sockfd].close_conn();

        }else if( ( sockfd == fd_util::pipefd[0] ) && ( events[i].events & EPOLLIN ) ) {
            int lret=handle_sig();
            if(lret==-1)
                return;
        }
        else if(  events[i].events & EPOLLIN )
        {
            handle_in();
        }else if(( events[i].events & EPOLLOUT ) &&!userconns[sockfd].write()){
            userconns[sockfd].close_conn();
        }
    }
public:
    epoll_util(const epoll_util& other)=delete;
    epoll_util& operator=(const epoll_util& other)=delete;
    epoll_util(int argc, char* argv[] ){

        init(argc,argv);
    }


    [[ nodiscard ]] bool get_stopserver() const{return stop_server;}
    [[ nodiscard ]] bool get_init_state() const{return init_state;}

    void run(){
        int number = epoll_wait( fd_util::epollfd, events, MAX_EVENT_NUMBER, -1 );
        if ( ( number < 0 ) && ( errno != EINTR ) ) {
            printf( "epoll failure\n" );
            return;
        }

        for ( int i = 0; i < number; i++ ) {
            sockfd = events[i].data.fd;
            process(i);
        }
        if( timeout ) {
            fd_util::timer_handler();
            timeout = false;
        }
    }
    void clear(){
        close( listenfd );
        close( fd_util::pipefd[1] );
        close( fd_util::pipefd[0] );
        delete [] users;
        delete [] userconns;
        delete pool;
    }
};
int main( int argc, char* argv[] ) {
    epoll_util util(argc,argv);
    if(!util.get_init_state()) {
        util.clear();
        return 0;
    }
    //Configuration
    while( !util.get_stopserver())
    {
        util.run();
    }
    util.clear();
    return 0;
}