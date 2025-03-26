#ifndef __LSH_MACRO_H__
#define __LSH_MACRO_H__

#include "log.h"
#include "util.h"
#include <assert.h>
#include <string>

// 这里使用 c++20 的 [[likely]] 和 [[unlikely]] 优化分支预测
// 不使用这些宏，为啥？看不懂！！
// #ifdef __GUNC__ || defined __llvm__
// #define LSH_LIKELY(x) __builtin_expect(!!(x), 1)
// #define LSH_UNLIKELY(x) __builtin_expect(!!(x), 0)
// #else
// #define LSH_LIKELY(x) (x)
// #define LSH_UNLIKELY(x) (x)
// #endif

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
                                    << w                                       \
                                    << "\nbacktrace:\n"                        \
                                    << lsh::BacktraceToString(100, 2, "    "); \
        assert(x);                                                             \
    }

#endif