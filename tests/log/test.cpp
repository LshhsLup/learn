#include "log.h"
#include "singleton.h"
#include "util.h"
#include <memory>
#include <sstream>
#include <thread>

int main(int argc, char **argv) {
    std::shared_ptr<lsh::Logger> logger(new lsh::Logger);
    std::shared_ptr<lsh::LogAppender> appender(new lsh::StdoutLogAppender);
    // std::shared_ptr<lsh::LogAppender> appender(new lsh::FileLogAppender("./log.txt"));
    std::string pattern = "%d{%H:%M:%S} [%p] %m%n";
    auto formatter = std::make_shared<lsh::LogFormatter>(pattern);
    std::cout << formatter->getFormatterSize() << std::endl;
    appender->setFormatter(formatter);
    std::cout << lsh::LogLevel::toString(logger->getLevel()) << std::endl;
    appender->setLevel(lsh::LogLevel::ERROR);
    logger->addAppender(appender);

    // std::shared_ptr<lsh::LogEvent> event =
    //     std::make_shared<lsh::LogEvent>(__FILE__, __LINE__, 0, lsh::GetThreadId(), lsh::GetFiberId(), time(0));
    // event->getSS() << "hello nihao";
    // logger->log(lsh::LogLevel::DEBUG, event);

    // 使用日志宏来记录日志
    LSH_LOG_LEVEL(logger, lsh::LogLevel::DEBUG) << "hello nihao";
    LSH_LOG_LEVEL(logger, lsh::LogLevel::INFO) << "This is an info log.";
    LSH_LOG_LEVEL(logger, lsh::LogLevel::ERROR) << "An error occurred!";
    LSH_LOG_ERROR(logger) << "hahahhmish";

    // 使用 LSH_LOG_FMT_LEVEL 宏 (printf 风格)
    LSH_LOG_FMT_LEVEL(logger, lsh::LogLevel::DEBUG, "User %s logged in %d times.", "Alice", 5);
    LSH_LOG_FMT_LEVEL(logger, lsh::LogLevel::INFO, "System started at %ld", time(0));
    LSH_LOG_FMT_LEVEL(logger, lsh::LogLevel::ERROR, "Fatal error: code=%d", -1);
    LSH_LOG_FMT_LEVEL(logger, lsh::LogLevel::ERROR, "Fatal error: code=%d", -1);

    auto l = lsh::LoggerMgr::GetInstance()->getLogger("xxx");
    LSH_LOG_INFO(l) << "xxxxx";
    std::cout << "hello,world" << std::endl;
    return 0;
}
// using namespace lsh;
// namespace fs = std::filesystem;

// // 测试固件
// class LoggerTest : public ::testing::Test {
// protected:
//     void SetUp() override {
//         testFile = "test.log";
//         // 确保每次测试前删除旧日志文件
//         if (fs::exists(testFile)) {
//             fs::remove(testFile);
//         }
//     }

//     void TearDown() override {
//         if (fs::exists(testFile)) {
//             fs::remove(testFile);
//         }
//     }

//     std::string testFile;
// };

// // 测试日志级别转换
// TEST(LogLevelTest, LevelToString) {
//     EXPECT_EQ(LogLevel::toString(LogLevel::DEBUG), "DEBUG");
//     EXPECT_EQ(LogLevel::toString(LogLevel::INFO), "INFO");
//     EXPECT_EQ(LogLevel::toString(LogLevel::WARN), "WARN");
//     EXPECT_EQ(LogLevel::toString(LogLevel::ERROR), "ERROR");
//     EXPECT_EQ(LogLevel::toString(LogLevel::FATAL), "FATAL");
//     EXPECT_EQ(LogLevel::toString(static_cast<LogLevel::Level>(99)), "UNKNOWN");
// }

// // 测试LogEvent基本功能
// TEST(LogEventTest, BasicProperties) {
//     LogEvent event(__FILE__, __LINE__, 100, 2222, 5678, 1630454400, "Test message");

//     EXPECT_STREQ(event.getFile(), __FILE__);
//     EXPECT_EQ(event.getLine(), __LINE__ - 3); // 注意行号偏移
//     EXPECT_EQ(event.getElapse(), 100);
//     EXPECT_EQ(event.getThreadId(), 2222);
//     EXPECT_EQ(event.getFiberId(), 5678);
//     EXPECT_EQ(event.getTime(), 1630454400);
//     EXPECT_EQ(event.getContent(), "Test message");
// }

// // 测试控制台输出Appender
// TEST_F(LoggerTest, StdoutAppenderTest) {
//     testing::internal::CaptureStdout();

//     auto logger = std::make_shared<Logger>("test_logger");
//     auto appender = std::make_shared<StdoutLogAppender>();
//     logger->addAppender(appender);

//     auto event = std::make_shared<LogEvent>(__FILE__, __LINE__, 0,
//                                             std::hash<std::thread::id>{}(std::this_thread::get_id()), 0, 0, "Test message");

//     logger->info(event);

//     std::string output = testing::internal::GetCapturedStdout();
//     EXPECT_TRUE(output.find("Test message") != std::string::npos);
// }

// // 测试文件输出Appender
// TEST_F(LoggerTest, FileAppenderTest) {
//     auto logger = std::make_shared<Logger>("file_logger");
//     auto appender = std::make_shared<FileLogAppender>(testFile);
//     logger->addAppender(appender);

//     auto event = std::make_shared<LogEvent>(__FILE__, __LINE__, 0,
//                                             std::hash<std::thread::id>{}(std::this_thread::get_id()), 0, 0, "File message");

//     logger->warn(event);

//     // 确保文件已写入
//     logger.reset();

//     std::ifstream in(testFile);
//     std::string content((std::istreambuf_iterator<char>(in)),
//                         std::istreambuf_iterator<char>());
//     EXPECT_TRUE(content.find("File message") != std::string::npos);
// }

// // 测试日志级别过滤
// TEST_F(LoggerTest, LogLevelFilter) {
//     testing::internal::CaptureStdout();

//     auto logger = std::make_shared<Logger>("level_logger");
//     logger->setLevel(LogLevel::WARN);
//     auto appender = std::make_shared<StdoutLogAppender>();
//     logger->addAppender(appender);

//     auto event = std::make_shared<LogEvent>(__FILE__, __LINE__, 0, 0, 0, 0, "Should not appear");

//     logger->debug(event); // 应被过滤
//     logger->error(event); // 应显示

//     std::string output = testing::internal::GetCapturedStdout();
//     EXPECT_TRUE(output.find("DEBUG") == std::string::npos);
//     EXPECT_TRUE(output.find("ERROR") != std::string::npos);
// }

// // 测试日志格式器
// TEST(LogFormatterTest, PatternFormatting) {
//     LogFormatter formatter("%% [%p] %m %n");
//     auto logger = std::make_shared<Logger>("formatter_test");
//     auto event = std::make_shared<LogEvent>("test.cpp", 42, 100, 123, 456, 1630454400, "Formatted message");

//     std::string result = formatter.format(logger, LogLevel::INFO, event);

//     EXPECT_TRUE(result.find("[INFO] Formatted message") != std::string::npos);
//     EXPECT_TRUE(result.find("%") != std::string::npos); // 测试转义字符
// }

// // 测试完整的日志流程
// TEST_F(LoggerTest, CompleteWorkflow) {
//     auto logger = std::make_shared<Logger>("workflow_test");

//     // 创建两个appender
//     auto stdoutAppender = std::make_shared<StdoutLogAppender>();
//     auto fileAppender = std::make_shared<FileLogAppender>(testFile);

//     // 设置格式器
//     auto formatter = std::make_shared<LogFormatter>("%d %t [%p] %f:%l %m%n");
//     stdoutAppender->setFormatter(formatter);
//     fileAppender->setFormatter(formatter);

//     logger->addAppender(stdoutAppender);
//     logger->addAppender(fileAppender);

//     auto event = std::make_shared<LogEvent>(__FILE__, __LINE__, 0,
//                                             std::hash<std::thread::id>{}(std::this_thread::get_id()), 0, 0, "Complete test");

//     logger->info(event);

//     // 验证文件输出
//     std::ifstream in(testFile);
//     std::string content((std::istreambuf_iterator<char>(in)),
//                         std::istreambuf_iterator<char>());
//     EXPECT_TRUE(content.find(__FILE__) != std::string::npos);
//     EXPECT_TRUE(content.find("[INFO]") != std::string::npos);
// }

// // 测试Appender管理
// TEST_F(LoggerTest, AppenderManagement) {
//     auto logger = std::make_shared<Logger>("appender_test");
//     auto appender = std::make_shared<StdoutLogAppender>();

//     // 测试添加/删除
//     EXPECT_EQ(logger->getLevel(), LogLevel::DEBUG);
//     logger->addAppender(appender);
//     logger->deleteAppender(appender);

//     // 测试日志不会输出
//     testing::internal::CaptureStdout();
//     auto event = std::make_shared<LogEvent>(__FILE__, __LINE__, 0, 0, 0, 0, "No output");
//     logger->info(event);
//     std::string output = testing::internal::GetCapturedStdout();
//     EXPECT_TRUE(output.empty());
// }

// // 测试文件Appender重新打开
// TEST_F(LoggerTest, FileReopen) {
//     auto appender = std::make_shared<FileLogAppender>(testFile);

//     // 第一次写入
//     auto event1 = std::make_shared<LogEvent>(__FILE__, __LINE__, 0, 0, 0, 0, "First write");
//     appender->log(nullptr, LogLevel::INFO, event1);

//     // 重命名文件
//     fs::rename(testFile, "temp.log");

//     // 再次写入应该重新打开
//     auto event2 = std::make_shared<LogEvent>(__FILE__, __LINE__, 0, 0, 0, 0, "Second write");
//     appender->log(nullptr, LogLevel::INFO, event2);

//     EXPECT_TRUE(fs::exists(testFile));
//     fs::remove("temp.log");
// }

// int main(int argc, char **argv) {
//     testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
