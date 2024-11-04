#ifndef __LOG_H
#define __LOG_H

#define DEBUG

/* #include <cstring> */
#include <stdio.h>
#include <string.h>

#ifdef DEBUG
#define LOG_DEBUG(format, ...) printf("DEBUG [%s:%d] " format "\n");
// fprintf(stdout, "DEBUG [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1, __LINE__, ##__VA_ARGS__)
#else
#define LOG_DEBUG(format, ...)
#endif

#define LOG_INFO(format, ...) \
  fprintf(stdout, "INFO [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1, __LINE__, ##__VA_ARGS__);

#define LOG_WARN(format, ...) \
  fprintf(stdout, "WARN [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1, __LINE__, ##__VA_ARGS__);

#define LOG_ERROR(format, ...) \
  fprintf(stdout, "ERROR [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1, __LINE__, ##__VA_ARGS__);

#endif /* __LOG_h */