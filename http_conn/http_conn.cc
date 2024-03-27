#include "http_conn.h"
#include<string.h>
// 定义HTTP响应的一些状态信息
const char* const ok_200_title = "OK";
const char* const ok_204_title = "No Content";
const char* const error_400_title = "Bad Request";
const char* const error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* const error_403_title = "Forbidden";
const char* const error_403_form = "You do not have permission to get file from this server.\n";
const char* const error_404_title = "Not Found";
const char* const error_404_form = "The requested file was not found on this server.\n";
const char* const error_500_title = "Internal Error";
const char* const error_500_form = "There was an unusual problem serving the requested file.\n";

// 网站的根目录
const char* const doc_root = "/home/victor/sylar/resources";

int setnonblocking( int fd ) {
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

// 向epoll中添加需要监听的文件描述符
void addfd( int epollfd, int fd, bool one_shot ) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;
    if(one_shot) 
    {
        // 防止同一个通信被不同的线程处理
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    // 设置文件描述符非阻塞
    setnonblocking(fd);  
}

// 从epoll中移除监听的文件描述符
void removefd( int epollfd, int fd ) {
    epoll_ctl( epollfd, EPOLL_CTL_DEL, fd, nullptr );
    close(fd);
}

// 修改文件描述符，重置socket上的EPOLLONESHOT事件，以确保下一次可读时，EPOLLIN事件能被触发
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl( epollfd, EPOLL_CTL_MOD, fd, &event );
}

// 所有的客户数
int http_conn::m_user_count = 0;
// 所有socket上的事件都被注册到同一个epoll内核事件中，所以设置成静态的
int http_conn::m_epollfd = -1;

// 关闭连接
void http_conn::close_conn() {
    if(m_sockfd != -1) {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--; // 关闭一个连接，将客户总数量-1
    }
}

// 初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in& addr){
    m_sockfd = sockfd;
    m_address = addr;
    
    // 端口复用
    int reuse = 1;
    setsockopt( m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
    addfd( m_epollfd, sockfd, true );
    m_user_count++;
    init();
}

void http_conn::init()
{
    m_check_state = CHECK_STATE::CHECK_STATE_REQUESTLINE;    // 初始状态为检查请求行
    m_linger = false;       // 默认不保持链接  Connection : keep-alive保持连接

    m_method = METHOD::GET;         // 默认请求方式为GET
    m_url = nullptr;
    m_version = nullptr;
    m_content_length = 0;
    m_host = nullptr;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, WRITE_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);
}

// 循环读取客户数据，直到无数据可读或者对方关闭连接
bool http_conn::read() {
    if( m_read_idx >= READ_BUFFER_SIZE ) {
        return false;
    }
    ssize_t bytes_read = 0;
    while(true) {
        // 从m_read_buf + m_read_idx索引出开始保存数据，大小是READ_BUFFER_SIZE - m_read_idx
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, 
        READ_BUFFER_SIZE - m_read_idx, 0 );
        if (bytes_read == -1) {
            if( errno == EAGAIN || errno == EWOULDBLOCK ) {
                // 没有数据
                break;
            }
            return false;   
        } else if (bytes_read == 0) {   // 对方关闭连接
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

// 解析一行，判断依据\r\n
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for ( ; m_checked_idx < m_read_idx; ++m_checked_idx ) {
        temp = m_read_buf[ m_checked_idx ];

        if ( temp == '\r' ) {
            if ( ( m_checked_idx + 1 ) == m_read_idx ) {
                return LINE_STATUS::LINE_OPEN;
            } else if ( m_read_buf[ m_checked_idx + 1 ] == '\n' ) {
                m_read_buf[ m_checked_idx++ ] = '\0';
                m_read_buf[ m_checked_idx++ ] = '\0';
                return LINE_STATUS::LINE_OK;
            }
            return LINE_STATUS::LINE_BAD;
        } else if( temp == '\n' )  {
            if( ( m_checked_idx > 1) && ( m_read_buf[ m_checked_idx - 1 ] == '\r' ) ) {
                m_read_buf[ m_checked_idx-1 ] = '\0';
                m_read_buf[ m_checked_idx++ ] = '\0';
                return LINE_STATUS::LINE_OK;
            }
            return LINE_STATUS::LINE_BAD;
        }
    }
    
    return LINE_STATUS::LINE_OPEN;
}
// 解析HTTP请求行，获得请求方法，目标URL,以及HTTP版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
    // GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t"); // 判断第二个参数中的字符哪个在text中最先出现
    if (! m_url) { 
        return HTTP_CODE::BAD_REQUEST;
    }
    // GET\0/index.html HTTP/1.1
    *m_url++ = '\0';    // 置位空字符，字符串结束符
    const char* method = text;
#define XX(str) if( strcasecmp(method, #str) == 0 ) { \
        m_method = METHOD::str; \
    } \

    XX(GET)
    else XX(HEAD)
    else XX(POST)
    else XX(DELETE)
    else XX(PUT)
    else XX(OPTIONS)
    else XX(TRACE)
    else
        return HTTP_CODE::BAD_REQUEST;
#undef XX
    // /index.html HTTP/1.1
    // 检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
    m_version = strpbrk( m_url, " \t" );
    if (!m_version) {
        return HTTP_CODE::BAD_REQUEST;
    }
    *m_version++ = '\0';
    if (strcasecmp( m_version, "HTTP/1.1") != 0 ) {
        return HTTP_CODE::BAD_REQUEST;
    }
    /**
     * http://192.168.110.129:10000/index.html
    */
    if (strncasecmp(m_url, "https://", 8) == 0 ) {   
        m_url += 8;
        // 在参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置。
        m_url = strchr( m_url, '/' );
    }
    if ( !m_url || m_url[0] != '/' ) {
        return HTTP_CODE::BAD_REQUEST;
    }
    m_check_state = CHECK_STATE::CHECK_STATE_HEADER; // 检查状态变成检查头
    return HTTP_CODE::NO_REQUEST;
}

// 解析HTTP请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char* text) {   
    // 遇到空行，表示头部字段解析完毕
    if( text[0] == '\0' ) {
        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
        // 状态机转移到CHECK_STATE_CONTENT状态
        if ( m_content_length != 0 ) {
            m_check_state = CHECK_STATE::CHECK_STATE_CONTENT;
            return HTTP_CODE::NO_REQUEST;
        }
        // 否则说明我们已经得到了一个完整的HTTP请求
        return HTTP_CODE::GET_REQUEST;
    } else if ( strncasecmp( text, "Connection:", 11 ) == 0 ) {
        // 处理Connection 头部字段  Connection: keep-alive
        text += 11;
        text += strspn( text, " \t" );
        if ( strcasecmp( text, "keep-alive" ) == 0 ) {
            m_linger = true;
        }
    } else if ( strncasecmp( text, "Content-Length:", 15 ) == 0 ) {
        // 处理Content-Length头部字段
        text += 15;
        text += strspn( text, " \t" );
        m_content_length = atoi(text);
    } else if ( strncasecmp( text, "Host:", 5 ) == 0 ) {
        // 处理Host头部字段
        text += 5;
        text += strspn( text, " \t" );
        m_host = text;
    } else if(strncasecmp(text,"User-Agent:",11)==0){
        text += 11;
        text += strspn( text, " \t" );
        m_ua = text;
    }else if(strncasecmp(text,"Accept:",7)==0){
        text += 7;
        text += strspn( text, " \t" );
        m_mime = text;
    }else if(strncasecmp(text,"Accept-Language:",16)==0){
        text += 16;
        text += strspn( text, " \t" );
        m_lang = text;
    }else if(strncasecmp(text,"Accept-Encoding:",16)==0){
        text += 16;
        text += strspn( text, " \t" );
        m_encode = text;
    }else if(strncasecmp(text,"Referer:",8)==0){
        text += 8;
        text += strspn( text, " \t" );
        m_ref = text;
    }else if(strncasecmp(text,"Upgrade-Insecure-Requests:",26)==0){
        text += 27;
        text += strspn( text, " \t" );
        m_upgrade_insecure_requests = text;
    }
    else{
        LOG_FATAL( "oop! unknow header %s\n", text )
    }
    return HTTP_CODE::NO_REQUEST;
}

// 我们没有真正解析HTTP请求的消息体，只是判断它是否被完整的读入了
http_conn::HTTP_CODE http_conn::parse_content( char* text ) const{
    if ( m_read_idx >= ( m_content_length + m_checked_idx ) )
    {
        text[ m_content_length ] = '\0';
        return HTTP_CODE::GET_REQUEST;
    }
    return HTTP_CODE::NO_REQUEST;
}

// 主状态机，解析请求
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_STATUS::LINE_OK;
    HTTP_CODE ret = HTTP_CODE::NO_REQUEST;
    char* text = nullptr;
    while (((m_check_state == CHECK_STATE::CHECK_STATE_CONTENT) && (line_status == LINE_STATUS::LINE_OK))
                || ((line_status = parse_line()) == LINE_STATUS::LINE_OK)) {
        // 获取一行数据
        text = get_line();
        m_start_line = m_checked_idx;
        switch ( m_check_state ) {
            case CHECK_STATE::CHECK_STATE_REQUESTLINE: {
                ret = parse_request_line( text );
                if ( ret == HTTP_CODE::BAD_REQUEST ) {
                    return HTTP_CODE::BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE::CHECK_STATE_HEADER: {
                ret = parse_headers( text );
                if ( ret == HTTP_CODE::BAD_REQUEST ) {
                    return HTTP_CODE::BAD_REQUEST;
                } else if ( ret == HTTP_CODE::GET_REQUEST ) {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE::CHECK_STATE_CONTENT: {
                ret = parse_content( text );
                if ( ret == HTTP_CODE::GET_REQUEST ) {
                    return do_request();
                }
                line_status = LINE_STATUS::LINE_OPEN;
                break;
            }
            default: {
                return HTTP_CODE::INTERNAL_ERROR;
            }
        }
    }
    return HTTP_CODE::NO_REQUEST;
}

// 当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性，
// 如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其
// 映射到内存地址m_file_address处，并告诉调用者获取文件成功
http_conn::HTTP_CODE http_conn::do_request()
{
    // "/home/nowcoder/webserver/resources"
    strcpy( m_real_file, doc_root );
    int len = strlen( doc_root );
    strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if ( stat( m_real_file, &m_file_stat ) < 0 ) {
        return HTTP_CODE::NO_RESOURCE;
    }

    // 判断访问权限
    if ( ! ( m_file_stat.st_mode & S_IROTH ) ) {
        return HTTP_CODE::FORBIDDEN_REQUEST;
    }

    // 判断是否是目录
    if ( S_ISDIR( m_file_stat.st_mode ) ) {
        return HTTP_CODE::BAD_REQUEST;
    }

    // 以只读方式打开文件
    int fd = open( m_real_file, O_RDONLY );
    // 创建内存映射
    m_file_address = ( char* )mmap( nullptr, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    return HTTP_CODE::FILE_REQUEST;
}

// 对内存映射区执行munmap操作
void http_conn::unmap() {
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = nullptr;
    }
}

// 写HTTP响应
bool http_conn::write()
{
    ssize_t temp = 0;
    int bytes_have_send = 0;    // 已经发送的字节
    int bytes_to_send = m_write_idx;// 将要发送的字节 （m_write_idx）写缓冲区中待发送的字节数
    
    if ( bytes_to_send == 0 ) {
        // 将要发送的字节为0，这一次响应结束。
        modfd( m_epollfd, m_sockfd, EPOLLIN ); 
        init();
        return true;
    }

    while(true) {
        // 分散写
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if ( temp <= -1 ) {
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if( errno == EAGAIN ) {
                modfd( m_epollfd, m_sockfd, EPOLLOUT );
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if ( bytes_to_send <= bytes_have_send ) {
            // 发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接
            unmap();
            if(m_linger) {
                init();
                modfd( m_epollfd, m_sockfd, EPOLLIN );
                return true;
            } else {
                modfd( m_epollfd, m_sockfd, EPOLLIN );
                return false;
            } 
        }
    }
}

// 往写缓冲中写入待发送的数据
bool http_conn::add_response( const char* format, ... ) {
    if( m_write_idx >= WRITE_BUFFER_SIZE ) {
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list );
    if( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx ) ) {
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );
    return true;
}

bool http_conn::add_status_line( int status, const char* title ) {
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

bool http_conn::add_headers(int content_len,const char* content_type) {
    bool flag1=add_content_length(content_len);
    bool flag2=add_content_type(content_type);
    bool flag3=add_linger();
    bool flag4=add_blank_line();
    return flag1&&flag2&&flag3&&flag4;
}

bool http_conn::add_content_length(int content_len) {
    return add_response( "Content-Length: %d\r\n", content_len );
}

bool http_conn::add_linger()
{
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}
bool http_conn::add_mime()
{
    return add_response( "Accept: %s\r\n",m_mime);
}
bool http_conn::add_ua()
{
    return add_response( "User-Agent: %s\r\n",m_ua);
}
bool http_conn::add_blank_line()
{
    return add_response( "%s", "\r\n" );
}

bool http_conn::add_content( const char* content )
{
    return add_response( "%s", content );
}

bool http_conn::add_content_type(const char* content_type) {
    return add_response("Content-Type:%s\r\n", content_type);
}

extern "C" const char* http_conn::find_value(const char *filename, const char *key) const {
constexpr int MAX_LINE_LENGTH=25;
    static char value[MAX_LINE_LENGTH];
    FILE *file = fopen(filename, "r");
    if (file == nullptr) {
        printf("Failed to open config file\n");
        return nullptr;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, file) != nullptr) {
        char * saveptr=nullptr;
        // 使用 strtok 函数分割行
        char *token = strtok_r(line, ":",&saveptr);
        if (token != nullptr&&strstr(key,token)) {
            // 如果 key 匹配，则返回对应的 value
            token = strtok_r(nullptr, ":",&saveptr);
            if (token != nullptr) {
                char *newline = strchr(token, '\n');
                if (newline != nullptr) {
                    *newline = '\0';
                }
                newline = strchr(line, '\r');
                if (newline != nullptr) {
                    *newline = '\0';
                }
                strcpy(value, token);
                fclose(file);
                return value;
            }
        }
    }

    fclose(file);
    return nullptr;
}
// 根据服务器处理HTTP请求的结果，决定返回给客户端的内容
bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret)
    {
        case HTTP_CODE::INTERNAL_ERROR:
            add_status_line( 500, error_500_title );
            add_headers( strlen( error_500_form ) );
            if ( ! add_content( error_500_form ) ) {
                return false;
            }
            break;
        case HTTP_CODE::BAD_REQUEST:
            add_status_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) ) {
                return false;
            }
            break;
        case HTTP_CODE::NO_RESOURCE:
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) ) {
                return false;
            }
            break;
        case HTTP_CODE::FORBIDDEN_REQUEST:
            add_status_line( 403, error_403_title );
            add_headers(strlen( error_403_form));
            if ( ! add_content( error_403_form ) ) {
                return false;
            }
            break;
        case HTTP_CODE::FILE_REQUEST:{
            if(m_method==METHOD::DELETE)
                add_status_line(204, ok_204_title );
            else{
                add_status_line(200, ok_200_title );
            }
            //DONE:optimize by ENUM or File I/O
            //Judge if it's image or not
            if(m_method!=METHOD::OPTIONS){
                const char* mime=find_value("formatlist.txt",m_url);
                bool not_image=(mime==nullptr);
                //Judge if
                if(not_image){
                    LOG_ERROR("URL is just a text html\n")
                    add_headers(static_cast<int>(m_file_stat.st_size));
                }else{
                    LOG_INFO("URL contains *%s*\n", mime)
                    add_headers(static_cast<int>(m_file_stat.st_size),mime);
                }
            }
            switch(m_method){
                case METHOD::GET:{
                    m_iv[0].iov_base = m_write_buf;
                    m_iv[0].iov_len = m_write_idx;
                    m_iv[1].iov_base = m_file_address;
                    m_iv[1].iov_len = m_file_stat.st_size;
                    m_iv_count = 2;
                    return true;
                }
                case METHOD::DELETE:
                    remove(m_real_file);
                case METHOD::HEAD:{
                    m_iv[0].iov_base = m_write_buf;
                    m_iv[0].iov_len = m_write_idx;
                    m_iv_count =1;
                    return true;
                }
                case METHOD::OPTIONS:
                    add_response("Allow: %s,%s,%s","HEAD","GET","OPTIONS");
                    m_iv[0].iov_base = m_write_buf;
                    m_iv[0].iov_len = m_write_idx;
                    m_iv_count =1;
                    break;
                default:
                    break;
            }
            return true;
            
            
            
        }
        default:
            return false;
    }

    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

// 由线程池中的工作线程调用，这是处理HTTP请求的入口函数
void http_conn::process() {
    // 解析HTTP请求
    HTTP_CODE read_ret = process_read();
    if ( read_ret == HTTP_CODE::NO_REQUEST ) {
        modfd( m_epollfd, m_sockfd, EPOLLIN );
        return;
    }
    
    // 生成响应
    bool write_ret = process_write( read_ret );
    if ( !write_ret ) {
        close_conn();
    }
    modfd( m_epollfd, m_sockfd, EPOLLOUT);
}