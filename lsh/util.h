#ifndef __LSH_UTIL_H__
#define __LSH_UTIL_H__

#include <string>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace lsh {
    // 获取线程id
    pid_t GetThreadId();

    // 获取协程 id
    u_int32_t GetFiberId();

    // 绝对路径转换为相对路径 c++17
    const char *getRelativePath(const std::string &absolutePath);

    // 获取当前调用栈（Backtrace）并存储到 backtrace 数组中
    void Backtrace(std::vector<std::string> &backtrace, int size = 64, int skip = 1);
    // 获取当前调用栈（Backtrace）并格式化为字符串
    std::string BacktraceToString(int size = 64, int skip = 2, const std::string &prefix = "");
}

#endif