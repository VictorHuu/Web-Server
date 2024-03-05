#include"log_level.h"
const char* LogLevel::toColoredString(){
		switch(m_level){
#define _CONCAT(a,b) a ## b 
#define CONCAT(a,b) _CONCAT(a,b)
#define BRACKET(name) STRINGIFY([name]:)
#define STRINGIFY(x) _STRINGIFY(x)
#define _STRINGIFY(x) #x
#define XX(name) \
		case name: \
			return  CONCAT(COLOR_, name) BRACKET(name) COLOR_RESET;
		XX(DEBUG);
		XX(INFO);
		XX(WARN);
		XX(ERROR);
		XX(FATAL);
#undef XX
#undef BRACKET
#undef CONCAT
#undef _CONCAT
#undef STRINGIFY
#undef _STRINGIFY
		default:
			return "UNKNOWN";
		}
		return "UNKNOWN";
}

void LogLevel::FromString(const char* str){
#define XX(level,v) \
		if(strcasecmp(str,#v)==0){\
			m_level=level;\
		}
		XX(DEBUG,debug)
		else XX(INFO,info)
		else XX(WARN,warn)
		else XX(ERROR,error)
		else XX(FATAL,fatal)
		else
			m_level=UNKNOWN;
#undef XX
}

void LogLevel::setLevel(const Level& level)
{
	m_level=level;
}

Level LogLevel::getLevel() const
{
    return m_level;
}
