#ifndef VICTOR_CONFIG_H
#define VICTOR_CONFIG_H
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include"../log/log_level.h"
#include "../log/log.h"

class Config{
private:
    int m_port=0;//-p
    int m_threadnum=8;//-t
    char m_logname[MAX_LOGNAME_LEN];//-n
    Level m_loglevel=DEBUG;//-l
    bool m_closelog=false;//-o
    bool m_help=false;//-h
    Config();
    
public:
    ~Config()=default;
    int get_port() const{return m_port;}
    int get_threadnum() const{return m_threadnum;}
    bool get_help() const{return m_help;}
    const char* get_logname() const{return m_logname;}
    bool get_closelog() const{return m_closelog;}
    Level get_level() const{return m_loglevel;}
    static Config& getInstance();
    void parse_arg(int argc, char*argv[]);
};
#endif