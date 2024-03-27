#ifndef VICTOR_LOG_LEVEL_H
#define VICTOR_LOG_LEVEL_H
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
enum Level{
		DEBUG,
		INFO,
		WARN,
		ERROR,
		FATAL,
		UNKNOWN
};
#define COLOR_DEBUG     "\x1b[97m"
#define COLOR_INFO   "\x1b[34m"
#define COLOR_WARN  "\x1b[35m"
#define COLOR_ERROR    "\033[33m"
#define COLOR_FATAL    "\033[31m"
#define COLOR_RESET   "\x1b[0m"
#define COLOR_GRAY "\033[90m"
#define BOLD "\033[1m"
class LogLevel{
	Level m_level;
public:
	const char* toColoredString();
	void FromString(const char* str);
	void setLevel(const Level& level);
	Level getLevel() const;
};
#endif