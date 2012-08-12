#ifndef DEBUG_H
#define DEBUG_H

// The debugging levels
#define ERROR 1
#define WARN 2
#define SOMEINFO 3
#define INFO 4
#define MOREINFO 5

// The level to debug at
extern unsigned int debug_level;

#define debugLevelError() (((ERROR <= debug_level) ? 1 : 0))
#define debugLevelWarn() (((WARN <= debug_level) ? 1 : 0))
#define debugLevelSomeInfo() (((SOMEINFO <= debug_level) ? 1 : 0))
#define debugLevelInfo() (((INFO <= debug_level) ? 1 : 0))
#define debugLevelMoreInfo() (((MOREINFO <= debug_level) ? 1 : 0))

void debug(unsigned int level, const char *fmt, ...);
void debug_if(unsigned int condition, unsigned int level, const char *fmt, ...);

#endif
