#include "log.h"
#include <time.h>

void print_in_format(int level, std::string msg)
{
	time_t rawtime;
	struct tm * timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	char logT[20];	
	sprintf(logT,"%4d-%02d-%02d %02d:%02d:%02d",1900+timeinfo->tm_year, 1+timeinfo->tm_mon,
	timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	
	std::string logL;
	switch(level)
	{
		case 1:
			logL = "TRACE";
			break;
		case 2:
			logL = "DEBUG";
			break;
		case 3:
			logL = "INFO";
			break;
		case 4:
			logL = "WARNING";
			break;
		case 5: 
			logL = "ERROR";
			break;
		case 6:
			logL = "FATAL";
			break;
		case 7:
			logL = "OFF";
			break;
		default:
			break;
	}

	//按照 [Time] [level] -msg 的格式打印
	printf("[%s] [%s] -%s,%s\n",logT,logL.c_str(),msg.c_str(),__PRETTY_FUNCTION__);
}

void log(int level, std::string msg)
{
	if( level > LOG_LEVEL )
		print_in_format(level, msg);

}