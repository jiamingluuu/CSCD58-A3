#ifndef __LOG_H
#define __LOG_H

#define DEBUG

/* #include <cstring> */
#include <stdio.h>
#include <string.h>

#ifdef DEBUG
#define LOG_DEBUG(format, ...) \
  { fprintf(stdout, "DEBUG [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1, __LINE__, ##__VA_ARGS__); }
#else
#define LOG_DEBUG(format, ...)
#endif

#define LOG_INFO(format, ...)                                                                          \
  do {                                                                                                 \
    fprintf(stdout, "INFO [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1, __LINE__, ##__VA_ARGS__); \
  } while (0)

#define LOG_WARN(format, ...)                                                                          \
  do {                                                                                                 \
    fprintf(stdout, "WARN [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1, __LINE__, ##__VA_ARGS__); \
  } while (0)

#define LOG_ERROR(format, ...)                                                                          \
  do {                                                                                                  \
    fprintf(stdout, "ERROR [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1, __LINE__, ##__VA_ARGS__); \
  } while (0)

#endif /* __LOG_h */