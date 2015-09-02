#include <stdio.h>
#include <string>

#define OFF 7
#define FATAL 6
#define ERROR 5
#define WARNING 4
#define INFO 3
#define DEBUG 2
#define TRACE 1

#define LOG_LEVEL INFO

void log(int level, std::string msg);