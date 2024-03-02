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
#include"log_level.h"
#include"block_queue.h"

using namespace std;
class Log{
private:
	char dir_name[MAX_PATH_LEN];
	char log_name[MAX_LOGNAME_LEN];
	int32_t m_line_threshold;//the Maximum No. of Lines in a log
	int32_t m_buffer_size;//the length of the buffer
	intmax_t m_count;
    short m_today;
	FILE* m_fps[2];//0 for regular,1 for error
	#define m_fp &m_fps[0]
	
	char* m_buf;
	block_queue<char*>* m_block_q;
	bool m_async;
	locker m_mutex;
	bool m_close;
public:
	bool getCloseFlag()const{
		return m_close;
	}
	static Log* getInstance(){
		static Log instance;
		return &instance;
	}
	bool init(const char* filename,bool close,int bufsize,int line_thresh,int max_queue_size);
	static void *flush_log_thread(void *args);
	void write_log(Level level,const char* format,...);
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
#define META(LEVEL,format,...) if(0 == Log::getInstance()->getCloseFlag()) {Log::getInstance()->write_log(LEVEL,"(PID=%d) %s%s%s:%d%s " format,getpid(),COLOR_RESET,BOLD,__FILE__,__LINE__,COLOR_RESET, ##__VA_ARGS__); Log::getInstance()->flush();}
#define LOG_DEBUG(format, ...) META(DEBUG,format,##__VA_ARGS__)
#define LOG_INFO(format, ...) META(INFO,format,##__VA_ARGS__)
#define LOG_WARN(format, ...) META(WARN,format,##__VA_ARGS__)
#define LOG_ERROR(format, ...) META(ERROR,format,##__VA_ARGS__)
#define LOG_FATAL(format, ...) META(FATAL,format,##__VA_ARGS__)
#endif
