#include "log.h"
#include "config.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <functional>
#include <map>
#include <sstream>

namespace lsh {
    std::string LogLevel::toString(Level level) {
        constexpr std::array<const char *, 6> levelNames = {
            "UNKNOWN",
            "DEBUG",
            "INFO",
            "WARN",
            "ERROR",
            "FATAL"};
        return (level >= UNKNOWN && level <= FATAL) ? levelNames[level] : "UNKNOWN";
    }

    LogLevel::Level LogLevel::fromString(const std::string &str) {
        const std::unordered_map<std::string, LogLevel::Level> strToLevel = {
            {"UNKONWN", UNKNOWN},
            {"DEBUG", DEBUG},
            {"INFO", INFO},
            {"WARN", WARN},
            {"ERROR", ERROR},
            {"FATAL", FATAL}};
        std::string s = str;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        auto it = strToLevel.find(s);
        return it == strToLevel.end() ? UNKNOWN : it->second;
    }

    LogEvent::LogEvent(const char *file, int32_t line, uint32_t elapse,
                       uint32_t threadId, uint32_t fiberId, uint64_t time,
                       std::shared_ptr<Logger> logger, LogLevel::Level level)
        : m_file(file), m_line(line), m_elapse(elapse),
          m_threadId(threadId), m_fiberId(fiberId), m_time(time), m_logger(logger), m_level(level) {}

    void LogEvent::format(const char *fmt, ...) {
        va_list al;
        va_start(al, fmt);
        format(fmt, al);
        va_end(al);
    }

    void LogEvent::format(const char *fmt, va_list al) {
        char *buf = nullptr;
        int len = vasprintf(&buf, fmt, al);
        if (len != -1) {
            m_stringStream << std::string(buf, len);
            free(buf);
        }
    }

    LogEventWrap::LogEventWrap(std::shared_ptr<LogEvent> event) { m_event = event; }

    LogEventWrap::~LogEventWrap() {
        m_event->getLogger()->log(m_event->getLevel(), m_event);
    }

    std::stringstream &LogEventWrap::getSS() {
        return m_event->getSS();
    }

    std::shared_ptr<LogEvent> LogEventWrap::getEvent() {
        return m_event;
    }

    LogFormatter::LogFormatter(const std::string &pattern) : m_pattern(pattern) {
        this->init(); // 初始化
    }

    // 将日志输出对象格式化
    auto LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level,
                              std::shared_ptr<LogEvent> event) -> std::string {
        std::ostringstream str_stream;
        for (auto &item : m_items) {
            item->format(str_stream, logger, level, event);
        }
        return str_stream.str();
    }

    void LogFormatter::init() {
        std::vector<std::tuple<std::string, std::string, int>> vec;
        std::string nstr;
        for (size_t i = 0; i < m_pattern.size(); ++i) {
            if (m_pattern[i] != '%') {
                nstr.append(1, m_pattern[i]);
                continue;
            }

            if ((i + 1) < m_pattern.size()) {
                if (m_pattern[i + 1] == '%') {
                    nstr.append(1, '%');
                    continue;
                }
            }

            size_t n = i + 1;
            int fmt_status = 0;
            size_t fmt_begin = 0;

            std::string str;
            std::string fmt;
            while (n < m_pattern.size()) {
                if (!fmt_status && (!isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[n] != '}')) {
                    str = m_pattern.substr(i + 1, n - i - 1);
                    break;
                }
                if (fmt_status == 0) {
                    if (m_pattern[n] == '{') {
                        str = m_pattern.substr(i + 1, n - i - 1);
                        // std::cout << "*" << str << std::endl;
                        fmt_status = 1; // 解析格式
                        fmt_begin = n;
                        ++n;
                        continue;
                    }
                } else if (fmt_status == 1) {
                    if (m_pattern[n] == '}') {
                        fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                        // std::cout << "#" << fmt << std::endl;
                        fmt_status = 0;
                        ++n;
                        break;
                    }
                }
                ++n;
                if (n == m_pattern.size()) {
                    if (str.empty()) {
                        str = m_pattern.substr(i + 1);
                    }
                }
            }

            if (fmt_status == 0) {
                if (!nstr.empty()) {
                    vec.push_back(std::make_tuple(nstr, std::string(), 0));
                    nstr.clear();
                }
                vec.push_back(std::make_tuple(str, fmt, 1));
                i = n - 1;
            } else if (fmt_status == 1) {
                std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
                m_error = true;
                vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
            }
        }

        if (!nstr.empty()) {
            vec.push_back(std::make_tuple(nstr, "", 0));
        }
        static std::map<std::string, std::function<std::shared_ptr<FormatItem>(const std::string &str)>> s_format_items = {
#define XX(str, C) \
    {#str, [](const std::string &fmt) { return std::shared_ptr<FormatItem>(new C(fmt)); }}

            XX(m, MessageFormatItem),    // m:消息
            XX(p, LevelFormatItem),      // p:日志级别
            XX(r, ElapseFormatItem),     // r:累计毫秒数
            XX(c, LoggerNameFormatItem), // c:日志名称
            XX(t, ThreadIdFormatItem),   // t:线程id
            XX(n, NewLineFormatItem),    // n:换行
            XX(d, DateTimeFormatItem),   // d:时间
            XX(f, FileNameFormatItem),   // f:文件名
            XX(l, LineFormatItem),       // l:行号
            XX(T, TabFormatItem),        // T:Tab
            XX(F, FiberIdFormatItem),    // F:协程id
            XX(N, ThreadIdFormatItem),   // N:线程名称
#undef XX
        };

        for (auto &i : vec) {
            if (std::get<2>(i) == 0) {
                m_items.push_back(std::shared_ptr<FormatItem>(new StringFormatItem(std::get<0>(i))));
            } else {
                auto it = s_format_items.find(std::get<0>(i));
                if (it == s_format_items.end()) {
                    m_items.push_back(std::shared_ptr<FormatItem>(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
                    m_error = true;
                } else {
                    m_items.push_back(it->second(std::get<1>(i)));
                }
            }
        }
    }

    void MessageFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                   LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << event->getContent();
    }

    void ElapseFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                  LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << event->getElapse();
    }

    void ThreadIdFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                    LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << event->getThreadId();
    }

    void FiberIdFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                   LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << event->getFiberId();
    }

    void DateTimeFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                    LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        time_t timestamp = event->getTime();
        struct tm localTime;
        localtime_r(&timestamp, &localTime);

        std::ostringstream oss;
        char buffer[256];
        size_t result = strftime(buffer, sizeof(buffer), m_format.c_str(), &localTime);
        if (result == 0) {
            // 处理格式化失败的情况
            oss << "<<time_format_error>>";
        } else {
            oss << buffer;
        }
        // oss << buffer;
        os << oss.str();
    }

    void LineFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << event->getLine();
    }

    void FileNameFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                    LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << event->getFile();
    }

    void LevelFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                 LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << LogLevel::toString(level);
    }

    void LoggerNameFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                      LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << event->getLogger()->getName();
    }

    void NewLineFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                   LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << std::endl;
    }

    void TabFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                               LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << "  ";
    }

    void StringFormatItem::format(std::ostream &os, std::shared_ptr<Logger> logger,
                                  LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        os << m_string;
    }

    FileLogAppender::FileLogAppender(const std::string &file_name) : m_file_name(file_name) {
        reopen();
    }

    void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                              std::shared_ptr<LogEvent> event) {
        if (m_file_stream) {
            if (level >= m_level) {
                MutexType::Lock lock(m_mutex);
                m_file_stream << m_formatter->format(logger, level, event);
            }
        }
    }

    auto FileLogAppender::reopen() -> bool {
        MutexType::Lock lock(m_mutex);
        if (m_file_stream) {
            m_file_stream.close();
        }
        // 指定 std::ios::app 模式以追加写入
        m_file_stream.open(m_file_name, std::ios::app);
        return m_file_stream.is_open();
    }

    std::string FileLogAppender::toYamlString() {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        node["type"] = "FileLogAppender";
        node["file"] = m_file_name;
        if (m_level != LogLevel::UNKNOWN) {
            node["level"] = LogLevel::toString(m_level);
        }
        if (m_formatter && m_hasFormatter) {
            node["formatter"] = m_formatter->getPattern();
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

    std::string StdoutLogAppender::toYamlString() {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        node["type"] = "StdoutLogAppender";
        if (m_level != LogLevel::UNKNOWN) {
            node["level"] = LogLevel::toString(m_level);
        }
        if (m_formatter && m_hasFormatter) {
            node["formatter"] = m_formatter->getPattern();
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

    void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                                std::shared_ptr<LogEvent> event) {
        if (level >= m_level) {
            MutexType::Lock lock(m_mutex);
            std::cout << m_formatter->format(logger, level, event);
        }
    }

    Logger::Logger(const std::string &name) : m_name(name) {
        m_formatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
    }

    void Logger::log(LogLevel::Level level, std::shared_ptr<LogEvent> event) {
        if (level >= m_level) {
            MutexType::Lock lock(m_mutex);
            if (!m_appenders.empty()) {
                for (auto &appender : m_appenders) {
                    appender->log(shared_from_this(), level, event);
                }
            } else if (m_root) {
                m_root->log(level, event);
            }
        }
    }

    void Logger::debug(std::shared_ptr<LogEvent> event) { log(LogLevel::DEBUG, event); }
    void Logger::info(std::shared_ptr<LogEvent> event) { log(LogLevel::INFO, event); }
    void Logger::warn(std::shared_ptr<LogEvent> event) { log(LogLevel::WARN, event); }
    void Logger::error(std::shared_ptr<LogEvent> event) { log(LogLevel::ERROR, event); }
    void Logger::fatal(std::shared_ptr<LogEvent> event) { log(LogLevel::FATAL, event); }

    void Logger::addAppender(std::shared_ptr<LogAppender> appender) {
        MutexType::Lock lock(m_mutex);
        if (!appender->getFormatter()) {
            MutexType::Lock lock(appender->m_mutex);
            appender->m_formatter = this->m_formatter;
        }
        m_appenders.push_back(appender);
    }

    void Logger::deleteAppender(std::shared_ptr<LogAppender> appender) {
        MutexType::Lock lock(m_mutex);
        std::erase(m_appenders, appender); // c++20
    }

    void Logger::clearAppenders() {
        MutexType::Lock lock(m_mutex);
        m_appenders.clear();
    }

    void Logger::setFormatter(std::shared_ptr<LogFormatter> val) {
        MutexType::Lock lock(m_mutex);
        m_formatter = val;

        // 如果 appender 继承 logger 的 formatter,需要改动 appender 的 formatter
        for (auto &i : m_appenders) {
            MutexType::Lock l(i->m_mutex);
            if (!i->m_hasFormatter) {
                i->m_formatter = m_formatter;
            }
        }
    }
    void Logger::setFormatter(const std::string &val) {
        std::shared_ptr<LogFormatter> new_val = std::make_shared<LogFormatter>(val);
        if (new_val->isError()) {
            std::cout << "Logger setFormatter name=" << m_name
                      << " value=" << val << " is invalid" << std::endl;
            return;
        }
        this->setFormatter(new_val);
    }

    std::string Logger::toYamlString() {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        node["name"] = m_name;
        node["level"] = LogLevel::toString(m_level);
        if (m_formatter) {
            node["formatter"] = m_formatter->getPattern();
        }
        for (auto &i : m_appenders) {
            node["appender"].push_back(YAML::Load(i->toYamlString()));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

    LoggerManager::LoggerManager() {
        // 默认 Logger
        m_root.reset(new Logger);
        m_root->addAppender(std::make_shared<StdoutLogAppender>());

        m_loggers[m_root->getName()] = m_root;
        init();
    }

    std::shared_ptr<Logger> LoggerManager::getLogger(const std::string &name) {
        MutexType::Lock lock(m_mutex);
        auto it = m_loggers.find(name);
        if (it != m_loggers.end()) {
            return it->second;
        }
        std::shared_ptr<Logger> logger = std::make_shared<Logger>(name);
        logger->m_root = m_root; // 保证 logger 的 appender 为空时，可以使用 root 打印
        m_loggers[name] = logger;
        return logger;
    }

    // 跟配置模块结合起来

    struct LogAppenderDefine {
        int type = 0; // 1 file 2 stdout
        LogLevel::Level level = LogLevel::UNKNOWN;
        std::string formatter;
        std::string file;

        bool operator==(const LogAppenderDefine &other) const {
            return type == other.type && level == other.level && formatter == other.formatter && file == other.file;
        }
    };

    struct LogDefine {
        std::string name;
        LogLevel::Level level = LogLevel::UNKNOWN;
        std::string formatter;
        std::vector<LogAppenderDefine> appenders;

        bool operator==(const LogDefine &other) const {
            return name == other.name && level == other.level && formatter == other.formatter && appenders == other.appenders;
        }

        bool operator<(const LogDefine &other) const {
            return name < other.name;
        }
    };

    // 对 LogDefine 进行偏特化，支持 string -> LogDefine
    template <>
    class LexicalCast<std::string, LogDefine> {
    public:
        LogDefine operator()(const std::string &str) {
            YAML::Node node = YAML::Load(str);
            LogDefine log;
            if (!node["name"].IsDefined()) {
                std::cout << "log config ERROR: name is NULL" << std::endl;
                return {};
            }
            log.name = node["name"].as<std::string>();
            log.level = LogLevel::fromString(node["level"].IsDefined() ? node["level"].as<std::string>() : "");
            if (node["formatter"].IsDefined()) {
                log.formatter = node["formatter"].as<std::string>();
            }
            if (node["appender"].IsDefined()) {
                for (const auto logappender : node["appender"]) {
                    LogAppenderDefine log_appender_define;
                    if (!logappender["type"].IsDefined()) {
                        std::cout << "log config ERROR: appender type is NULL" << std::endl;
                        continue;
                    }
                    std::string type = logappender["type"].as<std::string>();
                    if (type == "FileLogAppender") {
                        log_appender_define.type = 1;
                        if (!logappender["file"].IsDefined()) {
                            std::cout << "log config ERROR: FileLogAppender file is NULL" << std::endl;
                            continue;
                        }
                        log_appender_define.file = logappender["file"].as<std::string>();
                    } else if (type == "StdoutLogAppender") {
                        log_appender_define.type = 2;
                    } else {
                        std::cout << "log config ERROR: appender type is invalid" << std::endl;
                        continue;
                    }
                    if (logappender["formatter"].IsDefined()) {
                        log_appender_define.formatter = logappender["formatter"].as<std::string>();
                    }
                    log_appender_define.level = LogLevel::fromString(logappender["level"].IsDefined() ? logappender["level"].as<std::string>() : "");
                    log.appenders.push_back(log_appender_define);
                }
            }
            return log;
        }
    };

    // 对 LogDefine 进行偏特化，支持 LogDefine -> string
    template <>
    class LexicalCast<LogDefine, std::string> {
    public:
        std::string operator()(const LogDefine &v) {
            YAML::Node node;
            node["name"] = v.name;
            node["level"] = LogLevel::toString(v.level);
            if (!v.formatter.empty()) {
                node["formatter"] = v.formatter;
            }
            for (const auto &a : v.appenders) {
                YAML::Node n;
                if (a.type == 1) {
                    n["type"] = "FileLogAppender";
                    n["file"] = a.file;
                } else if (a.type == 2) {
                    n["type"] = "StdoutAppender";
                }
                n["level"] = LogLevel::toString(a.level);
                if (!a.formatter.empty()) {
                    n["formatter"] = a.formatter;
                }
                node["appender"].push_back(n);
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // 这里创建了全局名字为 logs, 值为空的配置,
    // YAML::Node root = YAML::LoadFile("/home/lsh/server_framework/bin/conf/logs.yml");
    // lsh::Config::LoadFromYaml(root);
    // 当执行这条语句时候就是从 yaml 文件读取 logs 的配置信息并修改
    // 由于静态全局变量 __log_init 会在 main 函数之前（也就是加载配置之前）调用构造函数
    // 此时我们又在构造函数中增加回调函数，这样再修改配置信息时候会触发回调函数做的内容
    // 这里回调函数做的内容就是把从 yaml 记载出来并成 std::set<LogDefine> 后的数据进行处理，并生成对应配置信息的 Logger
    // ------------------------------------------------------------------------------------------------------
    // 从 Yaml 文件加载数据经过 string->logDeinfe 解析后修改已经存在的配置项
    // 修改配置项时触发回调函数，根据原来的logDefine和从 Yaml 转换后的logDefine对loggers处理
    lsh::ConfigVar<std::set<LogDefine>>::ptr g_logs_defines = lsh::Config::Creat("logs", std::set<LogDefine>(), "logs config");

    struct LogIniter {
        LogIniter() {
            g_logs_defines->addListener(0xF1E231, [](const std::set<LogDefine> &old_value, const std::set<LogDefine> &new_value) {
                LSH_LOG_INFO(LSH_LOG_ROOT) << "on_logger_config_changed.";
                // 新增，新的里边有，老的里边没有
                for (auto &i : new_value) {
                    // 仅按名字查找，因为重载了 operator<
                    auto it = old_value.find(i);
                    std::shared_ptr<lsh::Logger> logger;
                    if (it == old_value.end()) {
                        // 新增 logger
                        logger = LSH_LOG_NAME(i.name);
                    } else {
                        // 新的里边有，老的里边也有，需要判断是否变化
                        if (!(i == *it)) {
                            // 变化了，修改logger
                            logger = LSH_LOG_NAME(i.name);
                        }
                    }
                    logger->setLevel(i.level);
                    if (!i.formatter.empty()) {
                        logger->setFormatter(i.formatter);
                    }
                    logger->clearAppenders();
                    for (auto &a : i.appenders) {
                        std::shared_ptr<LogAppender> appender;
                        if (a.type == 1) {
                            appender.reset(new FileLogAppender(a.file));
                        } else if (a.type == 2) {
                            appender.reset(new StdoutLogAppender);
                        }
                        appender->setLevel(a.level);
                        // std::cout << a.formatter << std::endl;
                        if (!a.formatter.empty()) {
                            appender->setFormatter(std::make_shared<LogFormatter>(a.formatter));
                        }
                        logger->addAppender(appender);
                    }
                }

                for (auto &i : old_value) {
                    auto it = new_value.find(i);
                    // 老的里边有，新的里边没有
                    if (it == new_value.end()) {
                        // 删除 Logger
                        auto logger = LSH_LOG_NAME(i.name);
                        logger->setLevel(static_cast<LogLevel::Level>(10000));
                        logger->clearAppenders();
                    }
                }
            });
        }
    };

    static LogIniter __log_init;

    std::string LoggerManager::toYamlString() {
        MutexType::Lock lock(m_mutex);
        YAML::Node node;
        for (auto &i : m_loggers) {
            node.push_back(YAML::Load(i.second->toYamlString()));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

    void LoggerManager::init() {
    }
}