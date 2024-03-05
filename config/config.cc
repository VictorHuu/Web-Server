#include "config.h"
Config::Config(){
    m_port=0;
    m_threadnum=8;
    m_loglevel=DEBUG;
    m_closelog=false;
    m_help=false;
    m_logname=(char*)malloc(100);
    strcpy(m_logname,"fransics");
}
Config& Config::getInstance(){
    static Config handler;
    return handler;
}
Config::~Config()
{
    free(m_logname);
}
void Config::parse_arg(int argc, char *argv[])
{
    int opt;
    const char *str = "p:t:n:l:o:h";
     while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            m_port = atoi(optarg);
            break;
        }
        case 't':
        {
            m_threadnum = atoi(optarg);
            break;
        }
        case 'n':
        {
            strcpy(m_logname,optarg);
            break;
        }
        case 'l':
        {
            m_loglevel=(Level)atoi(optarg);
            break;
        }
        case 'o':
        {
            m_closelog = (strcasestr(optarg,"y")!=NULL);
            break;
        }
        case 'h':
        {
            m_help=true;
            break;
        }
        default:
            break;
        }
    }
}