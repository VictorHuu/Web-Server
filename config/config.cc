#include "config.h"
Config::Config(){
    strncpy(m_logname,"fransics",MAX_LOGNAME_LEN);
}
Config& Config::getInstance(){
    static Config handler;
    return handler;
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
            LogLevel _level;
            _level.FromString(optarg);
            m_loglevel=_level.getLevel();
            break;
        }
        case 'o':
        {
            m_closelog = (strcasestr(optarg,"y")!=nullptr);
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