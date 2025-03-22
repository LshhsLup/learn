#ifndef __LSH_IOMANAGER_H__
#define __LSH_IOMANAGER_H__

#include "scheduler.h"

namespace lsh {

    class IOManager : public Scheduler {
    public:
        typedef std::shared_ptr<IOManager> ptr;
        typedef RWMutex RWMutexType;

        enum Event {
            NONE = 0,
            READ = 0x1,
            WRITE = 0x2
        };

    private:
        struct FdContext {
            typedef Mutex MutexType;
            struct EventContext {
                Scheduler *scheduler{nullptr}; // 事件执行的 scheduler
                Fiber::ptr fiber;              // 事件协程
                std::function<void()> cb;      // 事件的回调函数
            };

            int fd;             // 事件关联的句柄
            EventContext read;  // 读事件
            EventContext write; // 写事件
            Event event = NONE; // 已注册的事件
            MutexType mutex;
        };

    public:
        IOManager(size_t threads = 1, bool use_caller = true, const std::string &name);
        ~IOManager();

        // 1 success 0 retry -1 error
        int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
        bool delEvent(int fd, Event event);
        bool cnacelEvent(int fd, Event event);
        bool cancelAll(int fd);

        static IOManager *GetThis();

    protected:
        void tickle() override;
        bool stopping() override;
        void idle() override;

    private:
        int m_epoll_fd = 0;
        int m_tickle_fds[2];

        std::atomic<size_t> m_pendingEventCount{0};
        RWMutexType m_mutex;
        std::vector<FdContext *> m_fdContext;
    };

} // namespace lsh

#endif