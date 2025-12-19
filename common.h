/**
 * @file common.h
 * @brief common functional implementation
 * @author Qvafir <qvafir@outlook.com>
 * @copyright MIT License, see repository LICENSE file
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdarg.h>

void _log(const char *file_name, int line, const char *funct, const char *format, ...);
uint64_t time_now_ns();

#define log(msg, ...) _log(__FILE_NAME__, __LINE__, __func__, msg, ##__VA_ARGS__)
#define ASSERT(expression)                      \
if (!(expression)) {                        \
        log("FAILED: %s", #expression );  \
        return;                                 \
}



#endif
