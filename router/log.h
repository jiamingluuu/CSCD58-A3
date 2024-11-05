#ifndef __LOG_H
#define __LOG_H

#define DEBUG

#include <stdio.h>
#include <string.h>

#ifdef DEBUG
#define LOG_DEBUG(format, ...) printf("DEBUG [-:-] " format "\n");
#else
#define LOG_DEBUG(format, ...)
#endif

#define LOG_INFO(format, ...) printf("DEBUG [-:-] " format "\n");

#define LOG_WARN(format, ...) printf("DEBUG [-:-] " format "\n");

#define LOG_ERROR(format, ...) printf("DEBUG [-:-] " format "\n");

#endif /* __LOG_h */