#include "util.h"
#include <execinfo.h> // backtrace, backtrace_symbols
#include <filesystem>

namespace lsh {
    pid_t GetThreadId() {
        // 获取 Linux 下的内核线程 id，全局唯一
        return syscall(SYS_gettid);
    }

    u_int32_t GetFiberId() {
        return 0;
    }

    // 辅助函数：将绝对路径转换为相对路径并返回 const char*
    const char *getRelativePath(const std::string &absolutePath) {
        static std::string relativePathStr;
        std::filesystem::path currentPath = std::filesystem::current_path();
        std::filesystem::path absPath(absolutePath);
        relativePathStr = std::filesystem::relative(absPath, currentPath).string();
        return relativePathStr.c_str();
    }

    /**
     * @brief 获取当前调用栈（Backtrace）并存储到 backtrace 数组中。
     *
     * 该函数使用 GNU 提供的 backtrace() 和 backtrace_symbols() 来获取调用栈。
     *
     * @param backtrace 存储调用栈信息的 std::vector<std::string>
     * @param size 最大调用栈深度（最多记录多少个栈帧）
     * @param skip 跳过最前面的 skip 个栈帧（通常用于跳过 Backtrace() 本身及其调用者）
     */
    void Backtrace(std::vector<std::string> &backtrace, int size, int skip) {
        // 1. 分配 void* 数组，用于存储调用栈地址
        void **array = (void **)malloc(sizeof(void *) * size);
        if (!array) {
            return; // 申请内存失败，直接返回
        }

        // 2. 获取调用栈地址，最多存储 size 个栈帧
        size_t s = ::backtrace(array, size);

        // 3. 将地址转换为可读的函数符号信息
        char **strings = backtrace_symbols(array, s);
        if (strings == nullptr) {
            free(array);
            return; // 解析失败，释放内存并返回
        }

        // 4. 存储解析后的栈信息，跳过 skip 个栈帧
        for (size_t i = skip; i < s; i++) {
            backtrace.push_back(strings[i]);
        }

        // 5. 释放动态分配的内存，避免内存泄漏
        free(strings);
        free(array);
    }

    /**
     * @brief 获取当前调用栈（Backtrace）并格式化为字符串。
     *
     * 该函数调用 Backtrace() 获取调用栈信息，并将其转换为带有前缀的字符串格式。
     *
     * @param size 最大调用栈深度（最多记录多少个栈帧）
     * @param skip 跳过最前面的 skip 个栈帧（通常用于跳过 BacktraceToString() 本身及其调用者）
     * @param prefix 每行前缀（通常用于日志格式化）
     * @return std::string 格式化后的调用栈字符串
     */
    std::string BacktraceToString(int size, int skip, const std::string &prefix) {
        std::vector<std::string> backtrace;
        Backtrace(backtrace, size, skip);
        std::stringstream ss;
        for (int i = 0; i < backtrace.size(); i++) {
            ss << prefix << backtrace[i] << std::endl;
        }
        return ss.str();
    }

}