#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include"../log/log.h"
int setnonblocking( int fd );
void addfd( int epollfd, int fd, bool one_shot );
void removefd( int epollfd, int fd );
void modfd(int epollfd, int fd, int ev);
class http_conn
{
public:
    static const int FILENAME_LEN = 200;        
    static const int READ_BUFFER_SIZE = 2048;   
    static const int WRITE_BUFFER_SIZE = 1024;  
    
    
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    
    /*
        解析客户端请求时，主状态机的状态
        CHECK_STATE_REQUESTLINE:当前正在分析请求行
        CHECK_STATE_HEADER:当前正在分析头部字段
        CHECK_STATE_CONTENT:当前正在解析请求体
    */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    
    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求,获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    
    
    
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
public:
    http_conn(){}
    ~http_conn(){}
public:
    void init(int sockfd, const sockaddr_in& addr); 
    void close_conn();  
    void process(); 
    bool read();
    bool write();
private:
    void init();    
    HTTP_CODE process_read();    
    bool process_write( HTTP_CODE ret );    

    
    HTTP_CODE parse_request_line( char* text );
    HTTP_CODE parse_headers( char* text );
    HTTP_CODE parse_content( char* text );
    HTTP_CODE do_request();
    char* get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    
    void unmap();
    bool add_response( const char* format, ... );
    bool add_content( const char* content );
    bool add_content_type(const char* content_type);
    bool add_status_line( int status, const char* title );
    bool add_headers( int content_length,const char* content_type="text/html");
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line();
    bool add_ua();
    bool add_mime();

    const char* find_value(const char *filename, const char *key);
public:
    static int m_epollfd;       
    static int m_user_count;    

private:
    int m_sockfd;           
    sockaddr_in m_address;
    
    char m_read_buf[ READ_BUFFER_SIZE ];    
    int m_read_idx;                         
    int m_checked_idx;                      
    int m_start_line;                       

    CHECK_STATE m_check_state;              
    METHOD m_method;                        

    char m_real_file[ FILENAME_LEN ];       
    char* m_url;                        
    char* m_version;                        
    char* m_host;                           
    int m_content_length;                   
    bool m_linger;                          
    char* m_ua;
    char* m_mime;
    char* m_lang;
    char* m_encode;
    char* m_ref;
    char* m_upgrade_insecure_requests;
    
    char m_write_buf[ WRITE_BUFFER_SIZE ];  
    int m_write_idx;                        
    char* m_file_address;                   
    struct stat m_file_stat;                
    struct iovec m_iv[2];                   
    int m_iv_count;
};

#endif
