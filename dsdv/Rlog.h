#include <stdio.h>
#include <time.h>

#define OFF 7
#define FATAL 6
#define ERROR 5
#define WARNING 4
#define INFO 3
#define DEBUG 2
#define TRACE 1

#define LOG_LEVEL INFO

void print_in_format(int level, char* msg)
{
	time_t rawtime;
	struct tm * timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	char logT[20];	
	sprintf(logT,"%4d-%02d-%02d %02d:%02d:%02d",1900+timeinfo->tm_year, 1+timeinfo->tm_mon,
	timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
	
	char logL[8];
	switch(level)
	{
		case 1:
			sprintf(logL, "TRACE");
			break;
		case 2:
			sprintf(logL, "DEBUG");
			break;
		case 3:
			sprintf(logL, "INFO");
			break;
		case 4:
			sprintf(logL, "WARNING");
			break;
		case 5: 
			sprintf(logL, "ERROR");
			break;
		case 6:
			sprintf(logL, "FATAL");
			break;
		case 7:
			sprintf(logL, "OFF");
			break;
		default:
			break;
	}

	//按照 [Time] [level] -msg 的格式打印
	printf("[%s] [%s] -%s\n",logT,logL,msg);
}

void Rlog(int level, char* msg)
{
	if( level > LOG_LEVEL )
		print_in_format(level, msg);

}