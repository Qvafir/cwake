/**
 * @file common.c
 * @brief common functional implementation
 * @author Qvafir <qvafir@outlook.com>
 * @copyright MIT License, see repository LICENSE file
 */


#define _POSIX_C_SOURCE 199309L //force enable RT functional for C99 standard
#include <time.h>
#include <stdio.h>

#include "common.h"

void _log(const char *file_name, int line, const char *funct, const char *format, ...)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);

    char data[256];
    snprintf(data, 256, "[%5lu.%06lu] [%7s:%4d:%-22s] %s\n",
             time.tv_sec,
             time.tv_nsec / 1000,
             file_name,
             line,
             funct,
             format);

    va_list args;
    va_start(args, format);
    vprintf(data, args);
    va_end(args);
}

uint64_t time_now_ns()
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000000000 + time.tv_nsec;
}

