#include "log.h"

bool Log::init(const char *filename, bool close, int bufsize, int line_thresh, int max_queue_size)
{
    if(max_queue_size>=1){
        m_async=true;
        m_block_q=new block_queue<char*>(max_queue_size);

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

    m_fps[0]=fopen(log_full_name,"a");
    if(m_fps[0]==NULL){
        perror("fopen");
        return false;
    }
    char error_full_name[MAX_TOTAL_LOG_NAME+6]="";
    snprintf(error_full_name,MAX_TOTAL_LOG_NAME+6,"error-%s",log_full_name);
    m_fps[1]=fopen(error_full_name,"a");
    if(m_fps[1]==NULL){
        perror("fopen");
        return false;
    }
    return true;
}

void *Log::flush_log_thread(void *args)
{
    Log::getInstance()->async_write_log();
    return NULL;
}

void Log::write_log(Level level_, const char *format, ...)
{
    struct timeval now={0,0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *now_tm = localtime(&t);
    char s[32]={};
    LogLevel level;
    level.setLevel(level_);
    strcpy(s,level.toColoredString());
    
    m_mutex.lock();
    m_count++;
    //A new page
    if(now_tm->tm_mday>m_today||m_count%m_line_threshold==0){
        if(fflush(m_fps[0])==-1){
            perror("fsync");
            fclose(m_fps[0]);
            return;
        }
        fclose(m_fps[0]);
        if(fflush(m_fps[1])==-1){
            perror("fsync");
            fclose(m_fps[1]);
            return;
        }
        fclose(m_fps[1]);
        char new_log[MAX_TOTAL_LOG_NAME+24]="";
        char date[16]="";
        snprintf(date,16,"%d-%02d-%02d",now_tm->tm_year,now_tm->tm_mon,now_tm->tm_mday);
        if(now_tm->tm_mday>m_today){
            snprintf(new_log,MAX_TOTAL_LOG_NAME+sizeof(date),"%s%s%s",dir_name,date,log_name);
            m_today=now_tm->tm_mday;
            m_count=0;
        }else{
            snprintf(new_log,MAX_TOTAL_LOG_NAME+sizeof(date)+sizeof(intmax_t),"%s%s%s.%ju",dir_name,date,log_name,(intmax_t)(m_count/m_line_threshold));
        }
        m_fps[0]=fopen(new_log,"a");
        char new_err_log[MAX_TOTAL_LOG_NAME+29]="";
        snprintf(new_err_log,MAX_TOTAL_LOG_NAME+sizeof(date)+sizeof(intmax_t)+6,"error-%s",new_log);
        m_fps[1]=fopen(new_err_log,"a");
    }
    m_mutex.unlock();
    va_list arg;
    va_start(arg,format);
    char * log_buf;
    m_mutex.lock();
    int n=snprintf(m_buf,120, "\033[1m%s %s[%d-%02d-%02d %02d:%02d:%02d.%06ld]-",s,COLOR_GRAY,
                     now_tm->tm_year + 1900, now_tm->tm_mon + 1, now_tm->tm_mday,
                     now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec, now.tv_usec);
    int m=vsnprintf(m_buf+n,m_buffer_size-n-1,format,arg);
    m_buf[n+m]='\n';
    m_buf[n+m+1]='\0';
    log_buf=m_buf;
    m_mutex.unlock();
    if(m_async&&!m_block_q->full()){
        m_block_q->push(log_buf);
    }else{
        m_mutex.lock();
        
        if(level.getLevel()==FATAL||level.getLevel()==ERROR){
            fputs(log_buf,stdout);
            fputs(log_buf,m_fps[1]);
        }else
            fputs(log_buf,m_fps[0]);
        m_mutex.unlock();
    }
    va_end(arg);


}

void Log::flush()
{
    m_mutex.lock();
    fflush(m_fps[0]);
    fflush(m_fps[1]);
    m_mutex.unlock();
}

Log::~Log()
{
    fclose(m_fps[0]);
    fclose(m_fps[1]);
    delete [] m_buf;
}

void Log::async_write_log()
{
    char* single_log;
	while(m_block_q->pop(single_log)){
		m_mutex.lock();
		fflush(m_fps[0]);
        fflush(m_fps[1]);
		m_mutex.unlock();
	}
}


