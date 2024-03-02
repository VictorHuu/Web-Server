#include "log.h"

bool Log::init(const char *filename, bool close, int bufsize, int line_thresh, int max_queue_size)
{
    if(max_queue_size>=1){
        m_async=true;
        m_block_q=new block_queue(max_queue_size);

        pthread_t tid;
        pthread_create(&tid,NULL,flush_log_thread,NULL);
    }

    m_close=close;
    m_buffer_size=bufsize;
    m_buf=new char[m_buffer_size];
    bzero(m_buf,m_buffer_size);
    m_line_threshold=line_thresh;

    time_t t=time(NULL);
    struct tm* now_tm=localtime(&t);
    
    const char* relative_name=strrchr(filename,'/');
    char log_full_name[MAX_TOTAL_LOG_NAME]="";

    if(relative_name==NULL){
        snprintf(log_full_name,MAX_TOTAL_LOG_NAME,"%d-%02d-%02d-%s",now_tm->tm_year+1900,now_tm->tm_mon+1,now_tm->tm_mday,filename);
    }else{
        relative_name++;
        strcpy(log_name,relative_name);
        strncpy(dir_name,filename,relative_name-filename);
        snprintf(log_full_name,MAX_TOTAL_LOG_NAME,"%s-%d-%02d-%02d-%s",dir_name,now_tm->tm_year+1900,now_tm->tm_mon+1,now_tm->tm_mday,filename);
    }
    m_today=now_tm->tm_mday;

    m_fd=open(log_full_name,O_APPEND);
    if(m_fd==-1){
        perror("open");
        return false;
    }
    return true;
}

void *Log::flush_log_thread(void *args)
{
    Log::getInstance()->async_write_log();
}

void Log::write_log(LogLevel level, const char *format, ...)
{
    struct timeval now={0,0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *now_tm = localtime(&t);
    char s[32]={};
    strcpy(s,level.toColoredString());
    m_mutex.lock();
    m_count++;

    //A new page
    if(now_tm->tm_mday>m_today||m_count%m_line_threshold==0){
        if(fsync(m_fd)==-1){
            perror("fsync");
            close(m_fd);
            return;
        }
        close(m_fd);
        char new_log[MAX_TOTAL_LOG_NAME]="";
        char date[16]="";
        snprintf(date,16,"%d-%02d-%02d",now_tm->tm_year,now_tm->tm_mon,now_tm->tm_mday);
        if(now_tm->tm_mday>m_today){
            snprintf(new_log,MAX_TOTAL_LOG_NAME,"%s%s%s",dir_name,date,log_name);
            m_today=now_tm->tm_mday;
            m_count=0;
        }else{
            snprintf(new_log,MAX_TOTAL_LOG_NAME,"%s%s%s.%ju",dir_name,date,log_name,m_count/m_line_threshold);
        }
        m_fd=open(new_log,O_CREAT);
    }
    m_mutex.unlock();

    va_list arg;
    va_start(arg,format);
    char * log_buf;
    m_mutex.lock();
    int n=snprintf(m_buf,60, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     now_tm->tm_year + 1900, now_tm->tm_mon + 1, now_tm->tm_mday,
                     now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec, now.tv_usec,s);
    int m=vsnprintf(m_buf+n,m_buffer_size-n-1,format,arg);
    m_buf[n+m]='\n';
    m_buf[n+m+1]='\0';
    log_buf=m_buf;

    if(m_async&&!m_block_q->full()){
        m_block_q->push(log_buf);
    }else{
        m_mutex.lock();
        write(m_fd,log_buf,strlen(log_buf)+1);
        m_mutex.unlock();
    }
    va_end(arg);


}

void Log::flush()
{
    m_mutex.lock();
    fsync(m_fd);
    m_mutex.unlock();
}

Log::~Log()
{
    close(m_fd);
    delete [] m_buf;
}

void Log::async_write_log()
{
    string single_log;
	while(m_block_q->pop(single_log)){
		m_mutex.lock();
		write(m_fd,single_log.c_str(),strlen(single_log.c_str())+1);
		m_mutex.unlock();
	}
}


