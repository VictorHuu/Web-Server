#include <sys/types.h>
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
#include <pthread.h>
#include "timer/util_timer.h"
#include"http_conn/http_conn.h"
#include"threadpool/threadpool.h"
#include"config/config.h"
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];
static heap_util_timer timer_lst;
static int epollfd = 0;

extern void addfd( int epollfd, int fd, bool one_shot );
extern void removefd( int epollfd, int fd );
void addfd( int epollfd, int fd )
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET |EPOLLOUT;
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}
void sig_handler( int sig )
{
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}
void addsig(int sig, void( handler )(int)=sig_handler){
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}
void timer_handler()
{
    timer_lst.tick();   
    alarm(TIMESLOT);
}


void cb_func( client_data* user_data )
{
    epoll_ctl( epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0 );
    assert( user_data );
    close( user_data->sockfd );
    char ip[INET_ADDRSTRLEN]="";
    inet_ntop(AF_INET,&user_data->address.sin_addr.s_addr,ip,INET6_ADDRSTRLEN);
    printf( "close fd %d from %s\n", user_data->sockfd,ip);
}
static int cnt=0;
#define ERROR_HANDLE(func) do{ \
    LOG_FATAL("%s:%s",#func,strerror(errno));\
    sleep(1);\
    exit(EXIT_FAILURE);\
}while(0)

int main( int argc, char* argv[] ) {
    //Configuration
    Config* config;
    config->getInstance().parse_arg(argc,argv);
    Config& config_=config->getInstance();
    
    if(config_.get_help()||config_.get_port()<=1023||config_.get_threadnum()<=0
    ||config_.get_logname()==NULL||strlen(config_.get_logname())==0||
    config_.get_level()==UNKNOWN){
        struct stat statbuf;
        int manual_fd = open( "manual.txt", O_RDONLY );
        if(manual_fd==-1)
            ERROR_HANDLE(open);
        int ret=fstat(manual_fd,&statbuf);
        if(ret==-1)
            ERROR_HANDLE(fstat);
        //map then write to the stdout
        char* manual_address = ( char* )mmap( 0, statbuf.st_size, PROT_READ, MAP_PRIVATE, manual_fd, 0 );
        if(manual_address==MAP_FAILED)
            ERROR_HANDLE(mmap);
        struct iovec iov[2];
        char errmsg[100]="";
        if(config_.get_threadnum()<=0)
            snprintf(errmsg,100,"%sThe threadnum must be greater than 0,but it's %d now%s\n",COLOR_FATAL,config_.get_threadnum(),COLOR_RESET);
        else if(config_.get_port()<=1023)
            snprintf(errmsg,100,"%sThe port must be set and greater than 1023,but it's %d now%s\n",COLOR_FATAL,config_.get_port(),COLOR_RESET);
        else if(config_.get_logname()==NULL||strlen(config_.get_logname())==0)
            snprintf(errmsg,100,"%sThe name of the log can't be NULL%s\n",COLOR_FATAL,COLOR_RESET);
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
        return 0;
    }
    Log *log;
    log->getInstance()->init(config_.get_logname(),config_.get_closelog(),1024,1000,2);
    log->getInstance()->setLevel(config_.get_level());
    printf( "usage: %d port_number\n", config_.get_port());
    int port = config_.get_port();

    addsig( SIGPIPE, SIG_IGN );

    threadpool< http_conn >* pool = NULL;
    try {
        pool = new threadpool<http_conn>(config_.get_threadnum());
    } catch( ... ) {
        return 1;
    }

    http_conn* userconns = new http_conn[ FD_LIMIT ];
    client_data* users = new client_data[FD_LIMIT]; 

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );

    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( port );

    // 端口复用
    int reuse = 1;
    setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    ret = listen( listenfd, 5 );

    epoll_event events[ MAX_EVENT_NUMBER ];
    int epollfd = epoll_create( 5 );

    addfd( epollfd, listenfd, false );
    http_conn::m_epollfd = epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    setnonblocking( pipefd[1] );
    addfd( epollfd, pipefd[0]);
    addsig( SIGALRM );
    addsig( SIGTERM );
    bool stop_server = false;

    
    bool timeout = false;
    alarm(TIMESLOT);  

    while( !stop_server )
    {
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        if ( ( number < 0 ) && ( errno != EINTR ) ) {
            printf( "epoll failure\n" );
            break;
        }
    
        for ( int i = 0; i < number; i++ ) {
            int sockfd = events[i].data.fd;
            if( sockfd == listenfd )
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );

                if ( connfd < 0 ) {
                    printf( "errno is: %d\n", errno );
                    continue;
                } 

                if( http_conn::m_user_count >= FD_LIMIT ) {
                    close(connfd);
                    continue;
                }

                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;
                
                
                util_timer_node* timer = new util_timer_node(cnt++,&users[connfd],3 * TIMESLOT,cb_func);
                users[connfd].timer = timer;
                timer_lst.add_timer( timer );
                
                userconns[connfd].init( connfd, client_address);
            }else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) ) {

                userconns[sockfd].close_conn();

            }else if( ( sockfd == pipefd[0] ) && ( events[i].events & EPOLLIN ) ) {
                
                int sig;
                char signals[1024];
                ret = recv( pipefd[0], signals, sizeof( signals ), 0 );
                if( ret == -1 ) {
                    continue;
                } else if( ret == 0 ) {
                    continue;
                } else  {
                    for( int i = 0; i < ret; ++i ) {
                        switch( signals[i] )  {
                            case SIGALRM:
                            {

                                timeout = true;
                                break;
                            }
                            case SIGTERM:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            else if(  events[i].events & EPOLLIN )
            {

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
                        cb_func( &users[sockfd] );
                        if( timer )
                        {
                            timer_lst.del_timer( timer );
                        }
                    }
                }
                else if( ret == 0 )
                {
                    
                    cb_func( &users[sockfd] );
                    if( timer )
                    {
                        timer_lst.del_timer( timer );
                    }
                }
                else
                {
                    
                    if( timer ) {
                        printf( "adjust timer once\n" );
                        timer_lst.adjust_timer( timer,3 * TIMESLOT );
                        LOG_DEBUG("Here!");
                    }
                }
            }else if( events[i].events & EPOLLOUT ) {

                if( !userconns[sockfd].write() ) {
                    userconns[sockfd].close_conn();
                }
            }

           
        }

        
        if( timeout ) {
            timer_handler();
            timeout = false;
        }
    }

    close( listenfd );
    close( pipefd[1] );
    close( pipefd[0] );
    delete [] users;
    delete [] userconns;
    delete pool;
    return 0;
}