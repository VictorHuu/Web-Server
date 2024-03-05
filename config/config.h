#ifndef VICTOR_CONFIG_H
#define VICTOR_CONFIG_H
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include"../log/log_level.h"
class Config{
private:
    int m_port;//-p
    int m_threadnum;//-t
    char* m_logname;//-n
    Level m_loglevel;//-l
    bool m_closelog;//-o
    bool m_help;//-h
    Config();
    
public:
    ~Config();
    int get_port() const{return m_port;}
    int get_threadnum() const{return m_threadnum;}
    bool get_help() const{return m_help;}
    char* get_logname() const{return strdup(m_logname);}
    bool get_closelog() const{return m_closelog;}
    Level get_level() const{return m_loglevel;}
    Config& getInstance();
    void parse_arg(int argc, char*argv[]);
};
#endif