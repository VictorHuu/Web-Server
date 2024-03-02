class LogLevel{
	enum Level{
		DEBUG,
		INFO,
		WARN,
		ERROR,
		FATAL,
		UNKNOWN
	};
	Level m_level;
#define COLOR_DEBUG     "\x1b[31m"
#define COLOR_INFO   "\x1b[32m"
#define COLOR_WARN  "\x1b[33m"
#define COLOR_ERROR    "\x1b[34m"
#define COLOR_FATAL    "\x1b[35m"
#define COLOR_RESET   "\x1b[0m"
public:
	const char* toColoredString();
	void FromString(const char* str);
};