#include"log_level.h"
#include<string.h>
#include<string>
#include<stdint.h>
#include<unistd.h>
#include<../lock/lock.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<time.h>
#include<sys/time.h>
#include<stdarg.h>
#include<stdatomic.h>
const char* LogLevel::toColoredString(){
		switch(m_level){
#define _CONCAT(a,b,c) a##b##c
#define CONCAT(a,b,c) _CONCAT(a,b,c)
#define BRACKET(name) "["##name##"]:"
#define XX(name) \
		case LogLevel::name: \
			return CONCAT(COLOR_##name, BRACKET(#name) ,COLOR_RESET);
		XX(DEBUG);
		XX(INFO);
		XX(WARN);
		XX(ERROR);
		XX(FATAL);
#undef XX
#undef BRACKET
#undef CONCAT
#undef _CONCAT
		default:
			return "UNKNOWN";
		}
		return "UNKNOWN";
}

void LogLevel::FromString(const char* str){
#define XX(level,v) \
		if(strcasecmp(str,#v)==0){\
			m_level=LogLevel::level;\
		}
		XX(DEBUG,debug);
		XX(INFO,info);
		XX(WARN,warn);
		XX(ERROR,error);
		XX(FATAL,fatal);
		m_level=LogLevel::UNKNOWN;
#undef XX
}