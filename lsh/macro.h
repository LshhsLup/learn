#ifndef __LSH_MACRO_H__
#define __LSH_MACRO_H__

#include "util.h"
#include <string>

#define LSH_ASSERT(x)                                                          \
    if (!(x)) {                                                                \
        LSH_LOG_ERROR(LSH_LOG_ROOT) << "ASSERTION: " #x                        \
                                    << "\nbacktrace:\n"                        \
                                    << lsh::BacktraceToString(100, 2, "    "); \
        assert(x);                                                             \
    }

#define LSH_ASSERT_MSG(x, w)                                                   \
    if (!(x)) {                                                                \
        LSH_LOG_ERROR(LSH_LOG_ROOT) << "ASSERTION: " #x                        \
                                    << "\n"                                    \
                                    << #w                                      \
                                    << "\nbacktrace:\n"                        \
                                    << lsh::BacktraceToString(100, 2, "    "); \
        assert(x);                                                             \
    }

#endif