#ifndef __LOG_H
#define __LOG_H

/* #include <cstring> */ 
#include <string.h>
#include <stdio.h>

#ifdef DEBUG
#define LOG_DEBUG(format, ...)                                                 \
  {                                                                            \
    fprintf(stderr, "DEBUG [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1,  \
            __LINE__, ##__VA_ARGS__);                                          \
  }
#else
#define LOG_DEBUG(format, ...)
#endif

#define LOG_INFO(format, ...)                                                  \
  do {                                                                         \
    fprintf(stderr, "INFO [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1,   \
            __LINE__, ##__VA_ARGS__);                                          \
  } while (0)

#define LOG_WARN(format, ...)                                                  \
  do {                                                                         \
    fprintf(stderr, "WARN [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1,   \
            __LINE__, ##__VA_ARGS__);                                          \
  } while (0)

#define LOG_ERROR(format, ...)                                                 \
  do {                                                                         \
    fprintf(stderr, "ERROR [%s:%d] " format "\n", strrchr(__FILE__, '/') + 1,  \
            __LINE__, ##__VA_ARGS__);                                          \
  } while (0)

#endif /* __LOG_h */ 