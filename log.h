#ifndef VICTOR_LOG_H
#define VICTOR_LOG_H
#define MAX_PATH_LEN 128
#define MAX_LOGNAME_LEN 128
#define MAX_TOTAL_LOG_NAME MAX_PATH_LEN+MAX_LOGNAME_LEN
#include<string.h>
#include<string>
#include<stdint.h>
#include<unistd.h>
#include"../lock/lock.h"
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<time.h>
#include<sys/time.h>
#include<stdarg.h>
#include<stdatomic.h>
#include"log_level.h"
#include"block_queue.h"

using namespace std;
class Log{
private:
	char dir_name[MAX_PATH_LEN];
	char log_name[MAX_LOGNAME_LEN];
	int32_t m_line_threshold;//the Maximum No. of Lines in a log
	int32_t m_buffer_size;//the length of the buffer
	atomic_intmax_t m_count;
    short m_today;
	int m_fd;//the fd for the log
	char* m_buf;
	block_queue<string>* m_block_q;
	bool m_async;
	locker m_mutex;
	bool m_close;
public:
	static Log* getInstance(){
		static Log instance;
		return &instance;
	}
	bool init(const char* filename,bool close,int bufsize,int line_thresh,int max_queue_size);
	static void *flush_log_thread(void *args);
	void write_log(LogLevel level,const char* format,...);
	void flush();
private:
	Log()=default;
	//Copy and Move are strictly forbidden
	Log(const Log&)=delete;
	Log& operator=(const Log&)=delete;
	Log(Log&&)=delete;
	Log& operator=(Log&&)=delete;

	virtual ~Log();
	void async_write_log();


};

#endif
