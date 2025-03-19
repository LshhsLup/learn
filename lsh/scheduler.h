#ifndef __LSH_SCHEDULER_H__
#define __LSH_SCHEDULER_H__

//         1 - N     1 - M
// scheduler--->thread--->fiber
// 1 线程池，分配一组线程
// 2 协程调度器，将协程指定到相应的线程去执行
//   (1) 创建一个协程，随机选择一个线程上执行
//   (2) 或者到指定的线程上执行

#include "fiber.h"
#include "mutex"
#include "thread.h"
#include <list>
#include <memory>
#include <vector>

namespace lsh {
    class Scheduler {
    public:
        typedef std::shared_ptr<Scheduler> ptr;
        typedef Mutex MutexType;

        // 若某个线程执行了 Scheduler 构造函数，use_caller = true 意味着这个线程也要加入这个 Scheduler
        Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name);
        virtual ~Scheduler();

        const std::string &getName() const { return m_name; }

        static Scheduler *GetThis();
        static Fiber *GetMainFiber();

        void start();
        void stop();

    private:
        // 支持的执行类型
        struct FiberAndThread {
            Fiber::ptr fiber;
            std::function<void()> callback;
            int threadId;

            FiberAndThread(Fiber::ptr f, int thread) : fiber(f), threadId(thread) {}

            FiberAndThread(Fiber::ptr *f, int thread) : threadId(thread) {
                fiber.swap(*f);
            }

            FiberAndThread(std::function<void()> f, int t) : callback(f), threadId(t) {}

            FiberAndThread(std::function<void()> *f, int t) : threadId(t) {
                callback.swap(*f);
            }

            FiberAndThread() : threadId(-1) {}

            void reset() {
                fiber = nullptr;
                callback = nullptr;
                threadId = -1;
            }
        };

    private:
        MutexType m_mutex;
        std::vector<Thread::ptr> m_threads;
        std::list<FiberAndThread> m_fibers;
        std::string m_name;
    };
}

#endif