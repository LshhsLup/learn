#ifndef __LSH_IOMANAGER_H__
#define __LSH_IOMANAGER_H__

#include "macro.h"
#include "scheduler.h"
#include "timer.h"

namespace lsh {

    class IOManager : public Scheduler, public TimerManager {
    public:
        typedef std::shared_ptr<IOManager> ptr;
        typedef RWMutex RWMutexType;

        enum Event {
            NONE = 0x0,
            READ = 0x1,  // EPOLLIN
            WRITE = 0x4, // EPOLLOUT
        };

    private:
        // 维护一个 fd 的读/写事件、调度信息和回调
        struct FdContext {
            typedef Mutex MutexType;

            //  存储事件的协程、调度器和回调
            struct EventContext {
                Scheduler *scheduler{nullptr}; // 事件执行的 scheduler
                Fiber::ptr fiber;              // 事件协程
                std::function<void()> cb;      // 事件的回调函数
            };

            int fd = 0;         // 事件关联的句柄
            EventContext read;  // 读事件
            EventContext write; // 写事件
            /*
             * 一个整数类型，用来存储多个事件的位标志，可以通过按位运算来进行事件的组合和检查
             * 按位“或”（|） 用于组合多个事件，按位“与”（&） 用于检查特定的事件是否已经设置
             *
             * fd_ctx->events = READ | WRITE;  // 组合了 READ 和 WRITE 事件，结果是 0x3（二进制 0011）
             * 检查是否包含特定的事件（比如 READ 事件）
             * 使用按位与运算符（&）来检查特定的位标志是否存在
             * if (fd_ctx->events & READ) {
             *    如果 fd_ctx->events 中包含 READ 事件（即按位与结果非零）
             *   std::cout << "READ event is registered!" << std::endl;
             * }
             */
            Event events = NONE; // 已注册的事件
            MutexType mutex;

            EventContext &getContext(Event event) {
                switch (event) {
                case READ:
                    return read;
                case WRITE:
                    return write;
                default:
                    LSH_ASSERT_MSG(false, "getContext");
                }
            }

            void resetContext(EventContext &context) {
                context.scheduler = nullptr;
                context.fiber.reset();
                context.cb = nullptr;
            }

            void triggerEvent(Event event) {
                LSH_ASSERT(events & event);
                events = (Event)(events & ~event);
                EventContext &event_ctx = getContext(event);
                if (event_ctx.cb) {
                    event_ctx.scheduler->schedule(&event_ctx.cb);
                } else {
                    event_ctx.scheduler->schedule(&event_ctx.fiber);
                }
                event_ctx.scheduler = nullptr;
                return;
            }
        };

    public:
        IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "");
        ~IOManager();

        // 1 success 0 retry -1 error
        // 将文件描述符 fd 上的事件（如读取、写入事件）添加到 epoll 的事件监听中
        int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
        bool delEvent(int fd, Event event);
        bool cnacelEvent(int fd, Event event);
        bool cancelAll(int fd);

        static IOManager *GetThis();

    protected:
        void tickle() override;
        bool stopping() override;
        void idle() override;
        void contextResize(size_t size);
        bool stopping(uint64_t &timeout);
        void onTimerInsertedAtFront() override;

    private:
        int m_epoll_fd = 0;                         // epoll 文件句柄
        int m_tickle_fds[2];                        // pipe 文件句柄，其中 fd[0] 表示读端，fd[1] 表示写端
        std::atomic<size_t> m_pendingEventCount{0}; // 等待执行的事件数量
        RWMutexType m_mutex;                        // 读写锁
        std::vector<FdContext *> m_fdContext;       // 事件上下文容器
    };

} // namespace lsh

#endif