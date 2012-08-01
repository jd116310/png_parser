#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "debug.h"

unsigned int debug_level = ERROR;

// Prints the message if level <= debug_level. 
// If level == ERROR, exit(1) is called
void vdebug(unsigned int level, const char *fmt, va_list args)
{	
	if(level <= debug_level)
	{
		switch(level)
		{
			case ERROR:
				printf("Error: ");
				vprintf(fmt, args);
				system("pause");
				exit(1);
			case WARN:
				printf("Warning: ");
				vprintf(fmt, args);
				break;
			case SOMEINFO:
				vprintf(fmt, args);
				break;
			case INFO:
				vprintf(fmt, args);
				break;
			case MOREINFO:
				vprintf(fmt, args);
				break;
			default:
				vprintf(fmt, args);
				break;
		}	
	}
}
void debug(unsigned int level, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vdebug(level, fmt, args);
	va_end(args);
}
void debug_if(unsigned int condition, unsigned int level, const char *fmt, ...)
{
	if(!condition) return;
	
	va_list args;
	va_start(args, fmt);
	vdebug(level, fmt, args);
	va_end(args);
}