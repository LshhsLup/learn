#ifndef __LSH_SCHEDULER_H__
#define __LSH_SCHEDULER_H__

//          1 —— N     1 —— M
// scheduler ---> thread ---> fiber
// ​
// N : M 协程可以在线程间自由切换
// 1. 线程池， 分配一组线程
// 2. 协程调度器，将协程指定到相应线程上执行
//     a）随机选择空闲的线程执行
//     b）协程指定必须在某个线程上执行

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

        // use_caller 为 true 表示当前线程也会参与调度
        Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "");
        virtual ~Scheduler();

        const std::string &getName() const { return m_name; }

        static Scheduler *GetThis();
        static Fiber *GetMainFiber();

        void start();
        void stop();

        template <class FiberOrCb>
        void schedule(FiberOrCb fc, int thread = -1) {
            bool need_tickle = false;

            {
                MutexType::Lock lock(m_mutex);
                need_tickle = scheduleNoLock(fc, thread);
            }

            if (need_tickle) {
                tickle();
            }
        }

        template <class InputIterator>
        void schedule(InputIterator begin, InputIterator end) {
            bool need_tickle = false;
            {
                MutexType::Lock lock(m_mutex);
                while (begin != end) {
                    need_tickle = scheduleNoLock(&*begin, -1) || need_tickle;
                    begin++;
                }
            }
            if (need_tickle) {
                tickle();
            }
        }

    protected:
        virtual void tickle();

        void run();

        virtual bool stopping();

        virtual void idle();

        void setThis();

        bool hasIdleThread() {
            return m_idle_thread_count > 0;
        }

    private:
        template <class FiberOrCb>
        bool scheduleNoLock(FiberOrCb fc, int thread) {

            bool need_tickle = m_fibers.empty();

            FiberAndThread ft(fc, thread);

            if (ft.fiber || ft.callback) {
                m_fibers.push_back(ft);
            }

            return need_tickle;
        }

    private:
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
        std::list<FiberAndThread> m_fibers; // 任务队列
        std::string m_name;
        Fiber::ptr m_root_fiber; // use_caller为true时有效，调度协程

    protected:
        std::vector<int> m_threadIds;
        size_t m_thread_count{0};
        std::atomic<size_t> m_active_thread_count{0};
        std::atomic<size_t> m_idle_thread_count{0};
        bool m_stopping{true};
        bool m_autoStop{false};
        int m_root_threadId{0};
    };
}

#endif