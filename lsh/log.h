#ifndef __LSH_LOG_H__
#define __LSH_LOG_H__

#include "singleton.h"
#include "thread.h"
#include "util.h"
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief 使用流式方式将日志级别level的日志事件写入到logger
 *
 * 流式输入日志：使用 << 操作符简化日志记录。
 * 自动日志写入：通过 LogEventWrap 在析构时写入 logger,并调用 logger->log() 输出日志
 */
#define LSH_LOG_LEVEL(logger, level)                                                              \
    if (logger->getLevel() <= level)                                                              \
    lsh::LogEventWrap(std::make_shared<lsh::LogEvent>(/*相对路径 lsh::getRelativePath(__FILE__)*/ \
                                                      __FILE__, __LINE__, 0,                      \
                                                      lsh::GetThreadId(),                         \
                                                      lsh::GetFiberId(), time(0), logger, level)) \
        .getSS()

#define LSH_LOG_DEBUG(logger) LSH_LOG_LEVEL(logger, lsh::LogLevel::DEBUG)
#define LSH_LOG_INFO(logger) LSH_LOG_LEVEL(logger, lsh::LogLevel::INFO)
#define LSH_LOG_WARN(logger) LSH_LOG_LEVEL(logger, lsh::LogLevel::WARN)
#define LSH_LOG_ERROR(logger) LSH_LOG_LEVEL(logger, lsh::LogLevel::ERROR)
#define LSH_LOG_FATAL(logger) LSH_LOG_LEVEL(logger, lsh::LogLevel::FATAL)

/**
 * @brief 使用格式化方式将日志级别level的日志事件写入到logger
 */
#define LSH_LOG_FMT_LEVEL(logger, level, fmt, ...)                                                \
    if (logger->getLevel() <= level)                                                              \
    lsh::LogEventWrap(std::make_shared<lsh::LogEvent>(__FILE__, __LINE__, 0,                      \
                                                      lsh::GetThreadId(),                         \
                                                      lsh::GetFiberId(), time(0), logger, level)) \
        .getEvent()                                                                               \
        ->format(fmt, __VA_ARGS__)

#define LSH_LOG_FMT_DEBUG(logger, fmt, ...) LSH_LOG_FMT_LEVEL(logger, lsh::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define LSH_LOG_FMT_INFO(logger, fmt, ...) LSH_LOG_FMT_LEVEL(logger, lsh::LogLevel::INFO, fmt, __VA_ARGS__)
#define LSH_LOG_FMT_WARN(logger, fmt, ...) LSH_LOG_FMT_LEVEL(logger, lsh::LogLevel::WARN, fmt, __VA_ARGS__)
#define LSH_LOG_FMT_ERROR(logger, fmt, ...) LSH_LOG_FMT_LEVEL(logger, lsh::LogLevel::ERROR, fmt, __VA_ARGS__)
#define LSH_LOG_FMT_FATAL(logger, fmt, ...) LSH_LOG_FMT_LEVEL(logger, lsh::LogLevel::FATAL, fmt, __VA_ARGS__)

// 得到默认的 logger
#define LSH_LOG_ROOT lsh::LoggerMgr::GetInstance()->getRoot()

#define LSH_LOG_NAME(name) lsh::LoggerMgr::GetInstance()->getLogger(name)

namespace lsh {
    class Logger;
    class LogAppender;
    //=============== LogLevel =================
    /**
     * @brief 定义日志级别
     */
    class LogLevel {
    public:
        enum Level {
            UNKNOWN = 0, // 未知级别
            DEBUG = 1,   // 调试信息
            INFO = 2,    // 普通信息
            WARN = 3,    // 警告信息
            ERROR = 4,   // 错误信息
            FATAL = 5    // 严重错误
        };
        /**
         * @brief 将日志级别转换为字符串
         */
        static std::string toString(Level level);
        static LogLevel::Level fromString(const std::string &str);
    };

    //=============== LogEvent =================
    /**
     * @brief 表示单个日志事件，存储日志的详细信息
     */
    class LogEvent {
    public:
        LogEvent(const char *file, int32_t line, uint32_t elapse,
                 uint32_t threadId, uint32_t fiberId, uint64_t time,
                 std::shared_ptr<Logger> logger, LogLevel::Level level);

        auto getFile() const -> const char * { return m_file; }
        auto getLine() const -> int32_t { return m_line; }
        auto getElapse() const -> uint32_t { return m_elapse; }
        auto getThreadId() const -> uint32_t { return m_threadId; }
        auto getFiberId() const -> uint32_t { return m_fiberId; }
        auto getTime() const -> uint64_t { return m_time; }
        std::stringstream &getSS() { return m_stringStream; }
        std::string getContent() const { return m_stringStream.str(); }
        std::shared_ptr<Logger> getLogger() const { return m_logger; };
        LogLevel::Level getLevel() const { return m_level; }

        /**
         * @brief 格式化写入日志内容
         *  ... 代表 可变参数，可以传递多个值给 fmt 进行格式化。
         *  这些参数对应 fmt 中的占位符，类似 printf 的使用方式
         */
        void format(const char *fmt, ...);

        void format(const char *fmt, va_list al);

    private:
        const char *m_file;               // 文件名
        int32_t m_line;                   // 行号
        uint32_t m_elapse;                // 运行毫秒数
        uint32_t m_threadId;              // 线程 ID
        uint32_t m_fiberId;               // 协程 ID
        uint64_t m_time;                  // 时间戳
        std::stringstream m_stringStream; // 日志内容流
        std::shared_ptr<Logger> m_logger;
        LogLevel::Level m_level;
    };

    class LogEventWrap {
    public:
        LogEventWrap(std::shared_ptr<LogEvent> event);
        ~LogEventWrap();

        std::stringstream &getSS();
        std::shared_ptr<LogEvent> getEvent();

    private:
        std::shared_ptr<LogEvent> m_event;
    };
    //=============== LogFormatter =================
    /**
     * @brief 负责日志格式化
     */
    /*
     *  %m 消息
     *  %p 日志级别
     *  %r 累计毫秒数
     *  %c 日志名称
     *  %t 线程id
     *  %n 换行
     *  %d 时间
     *  %f 文件名
     *  %l 行号
     *  %T 制表符
     *  %F 协程id
     *  %N 线程名称
     *
     *  默认格式 "%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"
     */
    class LogFormatter {
    public:
        LogFormatter(const std::string &pattern);
        // 将日志输出对象格式化
        std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level,
                           std::shared_ptr<LogEvent> event);

    public:
        class FormatItem {
        public:
            FormatItem(const std::string &format = "") {};

            virtual ~FormatItem() {}

            virtual void format(std::ostream &os, std::shared_ptr<Logger> logger,
                                LogLevel::Level level, std::shared_ptr<LogEvent> event) = 0;
        };

        // 解析日志格式
        void init();

        auto getFormatterSize() -> size_t { return m_items.size(); }

        bool isError() const { return m_error; }

        const std::string getPattern() const { return m_pattern; }

    private:
        std::string m_pattern;                            // 日志格式化字符串
        bool m_error{false};                              // 用于标记是否有解析错误
        std::vector<std::shared_ptr<FormatItem>> m_items; // 格式化字符串解析后的解析器列表
    };

    class MessageFormatItem : public LogFormatter::FormatItem {
    public:
        MessageFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
    };

    class ElapseFormatItem : public LogFormatter::FormatItem {
    public:
        ElapseFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
    };

    class ThreadIdFormatItem : public LogFormatter::FormatItem {
    public:
        ThreadIdFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
    };

    class FiberIdFormatItem : public LogFormatter::FormatItem {
    public:
        FiberIdFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
    };

    class DateTimeFormatItem : public LogFormatter::FormatItem {
    public:
        DateTimeFormatItem(const std::string &format = "") {
            if (format.empty()) {
                m_format = "%Y-%m-%d %H:%M:%S";
            } else {
                m_format = format;
            }
        }
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;

    private:
        std::string m_format;
    };

    class LineFormatItem : public LogFormatter::FormatItem {
    public:
        LineFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
    };

    class FileNameFormatItem : public LogFormatter::FormatItem {
    public:
        FileNameFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
    };

    class LevelFormatItem : public LogFormatter::FormatItem {
    public:
        LevelFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
    };

    class LoggerNameFormatItem : public LogFormatter::FormatItem {
    public:
        LoggerNameFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
    };

    class NewLineFormatItem : public LogFormatter::FormatItem {
    public:
        NewLineFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
    };

    class TabFormatItem : public LogFormatter::FormatItem {
    public:
        TabFormatItem(const std::string &str = "") {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
    };

    class StringFormatItem : public LogFormatter::FormatItem {
    public:
        StringFormatItem(const std::string &str) : m_string(str) {}
        void format(std::ostream &os, std::shared_ptr<Logger> logger,
                    LogLevel::Level level, std::shared_ptr<LogEvent> event) override;

    private:
        std::string m_string;
    };

    //=============== LogAppender =================
    /**
     * @brief 日志输出地（Appender），可以是控制台或文件
     */
    class LogAppender {
        friend class Logger;

    public:
        typedef Spinlock MutexType;
        LogAppender(LogLevel::Level level = LogLevel::DEBUG) : m_level(level) {}
        virtual ~LogAppender() {}
        virtual void log(std::shared_ptr<class Logger> logger, LogLevel::Level level,
                         std::shared_ptr<LogEvent> event) = 0;
        void setFormatter(std::shared_ptr<LogFormatter> formatter) {
            MutexType::Lock lock(m_mutex);
            m_formatter = formatter;
            if (m_formatter) {
                m_hasFormatter = true;
            } else {
                m_hasFormatter = false;
            }
        }

        auto getFormatter() -> std::shared_ptr<LogFormatter> {
            MutexType::Lock lock(m_mutex);
            return m_formatter;
        }

        LogLevel::Level getLevel() const { return m_level; }
        void setLevel(LogLevel::Level level) { m_level = level; }

        virtual std::string toYamlString() = 0;

    protected:
        LogLevel::Level m_level;
        bool m_hasFormatter{false};
        std::shared_ptr<LogFormatter> m_formatter;
        MutexType m_mutex;
    };

    /**
     * @brief 控制台日志输出
     */
    class StdoutLogAppender : public LogAppender {
    public:
        void log(std::shared_ptr<class Logger> logger, LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
        std::string toYamlString() override;
    };

    /**
     * @brief 文件日志输出
     */
    class FileLogAppender : public LogAppender {
    public:
        FileLogAppender(const std::string &filename);
        void log(std::shared_ptr<class Logger> logger, LogLevel::Level level, std::shared_ptr<LogEvent> event) override;
        bool reopen();

        std::string toYamlString() override;

    private:
        std::string m_file_name;
        std::ofstream m_file_stream;
    };

    //=============== Logger =================
    /**
     * @brief 负责管理日志的记录和分发
     */
    class Logger : public std::enable_shared_from_this<Logger> {
        friend class LoggerManager;
        // friend class LogAppender;

    public:
        typedef Spinlock MutexType;
        Logger(const std::string &name = "root");

        void log(LogLevel::Level level, std::shared_ptr<LogEvent> event);
        void debug(std::shared_ptr<LogEvent> event);
        void info(std::shared_ptr<LogEvent> event);
        void warn(std::shared_ptr<LogEvent> event);
        void error(std::shared_ptr<LogEvent> event);
        void fatal(std::shared_ptr<LogEvent> event);

        void addAppender(std::shared_ptr<LogAppender> appender);
        void deleteAppender(std::shared_ptr<LogAppender> appender);
        void clearAppenders();

        auto getLevel() const -> LogLevel::Level { return m_level; }
        void setLevel(LogLevel::Level level) { m_level = level; }
        auto getName() -> std::string { return m_name; }
        void setFormatter(std::shared_ptr<LogFormatter> val);
        void setFormatter(const std::string &val);

        std::shared_ptr<LogFormatter> getFormatter() {
            MutexType::Lock lock(m_mutex);
            return m_formatter;
        }

        std::string toYamlString();

    private:
        std::string m_name; // 日志器名称
        LogLevel::Level m_level{LogLevel::DEBUG};
        std::list<std::shared_ptr<LogAppender>> m_appenders;
        std::shared_ptr<LogFormatter> m_formatter;
        std::shared_ptr<Logger> m_root;
        MutexType m_mutex;
    };

    // 这样使用 logger
    // lsh::Logger g_logger = LSH_LOG_NAME("system");
    // 当 system 的 appenders 为空时，使用 m_root 打印日志

    class LoggerManager {
    public:
        typedef Spinlock MutexType;
        LoggerManager();
        std::shared_ptr<Logger> getLogger(const std::string &name);

        void init();

        std::shared_ptr<Logger> getRoot() const { return m_root; }

        std::string toYamlString();

    private:
        std::map<std::string, std::shared_ptr<Logger>> m_loggers;
        std::shared_ptr<Logger> m_root; // 主 logger
        MutexType m_mutex;
    };

    // 确保 LoggerManager 是全局唯一的：
    typedef Singleton<LoggerManager> LoggerMgr;
}

#endif