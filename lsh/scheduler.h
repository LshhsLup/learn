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
        Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "");
        virtual ~Scheduler();

        const std::string &getName() const { return m_name; }

        static Scheduler *GetThis();
        static Fiber *GetMainFiber();

        void start();
        void stop();

        /**
         * @brief 调度一个单一的 Fiber 或回调函数到指定的线程执行
         *
         * 该方法将一个 `Fiber` 或 `std::function<void()>` 类型的任务添加到任务队列中
         * 并根据任务队列的状态决定是否需要唤醒调度器。
         *
         * @tparam FiberOrCb   任务类型，可以是 `Fiber::ptr` 或 `std::function<void()>` 类型
         * @param fc           要调度的任务（`Fiber` 或回调函数）
         * @param thread       目标线程的 ID，默认为 -1，表示任务可以在任何线程上执行
         */
        template <class FiberOrCb>
        void schedule(FiberOrCb fc, int thread = -1) {
            bool need_tickle = false;

            {
                // 使用互斥锁来确保线程安全，防止多线程同时访问共享的资源
                MutexType::Lock lock(m_mutex);

                // 调用 scheduleNoLock 调度任务，并获取是否需要唤醒调度器的信息
                need_tickle = scheduleNoLock(fc, thread);
            }

            // 如果任务队列为空，需要唤醒调度器来处理任务
            if (need_tickle) {
                tickle();
            }
        }

        /**
         * @brief 调度一个任务范围（多个任务）到指定的线程执行
         */
        template <class InputIterator>
        void schedule(InputIterator begin, InputIterator end) {
            bool need_tickle = false;
            {
                MutexType::Lock lock(m_mutex);
                while (begin != end) {
                    need_tickle = scheduleNoLock(&*begin) || need_tickle;
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

    private:
        /**
         * @brief 将 Fiber 或 callback 添加到任务队列中（无锁版本）
         *
         * 该函数是调度器的核心方法之一，可将一个 `Fiber` 或 `std::function<void()>` 任务
         * 绑定到一个指定的线程上，并将其添加到 `m_fibers` 任务队列中。
         *
         * @tparam FiberOrCb   任务类型，可以是 Fiber::ptr 或 std::function<void()>
         * @param fc           需要调度的 Fiber 或回调函数
         * @param thread       目标线程 ID，-1 表示交由任意线程执行
         * @return true        如果任务队列原本为空，需要唤醒调度器
         * @return false       如果任务队列已有任务，无需唤醒
         */
        template <class FiberOrCb>
        bool scheduleNoLock(FiberOrCb fc, int thread) {
            // 如果当前任务队列为空，说明需要唤醒调度器
            bool need_tickle = m_fibers.empty();

            // 创建一个 FiberAndThread 实例，将 fc 和线程信息 thread 进行绑定
            FiberAndThread ft(fc, thread);

            // 如果 Fiber 或 callback 存在，将其加入 m_fibers
            if (ft.fiber || ft.callback) {
                m_fibers.push_back(ft);
            }

            // 返回是否需要唤醒调度器
            return need_tickle;
        }

    private:
        /**
         * @brief 支持的执行类型，表示一个 Fiber 或一个可调用对象（callback）及其关联的线程信息
         *
         * 该结构体包含一个 Fiber、一个回调函数（`std::function<void()>`）和一个线程 ID
         * 根据传入的类型，构造函数将会初始化 `fiber` 或 `callback` 并将其与目标线程关联
         */
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
        Fiber::ptr m_root_fiber;

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