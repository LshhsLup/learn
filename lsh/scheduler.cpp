#include "scheduler.h"
#include "hook.h"
#include "log.h"
#include "macro.h"
#include <cassert>

namespace lsh {
    static std::shared_ptr<Logger> g_logger = LSH_LOG_NAME("system");

    static thread_local Scheduler *t_schedeluer = nullptr;
    static thread_local Fiber *t_schedeluer_fiber = nullptr;

    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name) {
        m_name = name;
        LSH_ASSERT(threads > 0);

        // use_caller 为 true 表示当前线程也会参与调度
        // 这个时候初始化 scheduler 会有两个协程，
        // t_schedeluer_fiber 表示当前调度器的调度协程，其他协程跟这个协程进行切换
        // 同时还有一个属于当前线程的主协程 t_threadFiber ，通过 Fiber::GetThis() 构造
        // 这时候它们两个是不同的，t_schedeluer_fiber 需要与 t_threadFiber 进行切换

        // use_caller 为 true 表示当前线程不会参与调度
        // 这时候只有一个负责切换的协程，t_threadFiber
        // 同时 t_schedeluer_fiber 等于 t_threadFiber，其他协程与这个协程进行切换
        if (use_caller) {
            Thread::setName(m_name);          // 设置线程名称
            Fiber::GetThis();                 // 初始化构造 scheduler 的线程的主协程 t_threadFiber
            --threads;                        // 当前线程参与了调度，需要创建的线程 -1
            LSH_ASSERT(GetThis() == nullptr); // 这时候没有初始化 t_schedeluer_fiber
            t_schedeluer = this;

            // 构造 m_root_fiber，也就是 t_schedeluer_fiber，用于其他工作协程的切换
            m_root_fiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
            t_schedeluer_fiber = m_root_fiber.get();

            // 得到当前线程 id
            m_root_threadId = GetThreadId();
            m_threadIds.push_back(m_root_threadId);
        } else {
            m_root_threadId = -1;
        }
        m_thread_count = threads; // 线程数量
    }

    Scheduler::~Scheduler() {
        LSH_ASSERT(m_stopping);
        if (GetThis() == nullptr) {
            t_schedeluer = nullptr;
        }
    }

    Scheduler *Scheduler::GetThis() {
        return t_schedeluer;
    }
    Fiber *Scheduler::GetMainFiber() {
        return t_schedeluer_fiber;
    }

    void Scheduler::start() {
        MutexType::Lock lock(m_mutex);
        if (!m_stopping) {
            return; // 启动了
        }
        m_stopping = false;
        LSH_ASSERT(m_threads.empty());
        m_threads.resize(m_thread_count);
        // 创建线程
        for (size_t i = 0; i < m_thread_count; i++) {
            // 线程执行 run 方法
            m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
            m_threadIds.push_back(m_threads[i]->getId());
        }

        lock.unlock();
        /**
         * ---------------------------------------------------------------
         * 在 use_caller 为 true 的情况下
         * 如果在这里进行 m_root_fiber(也就是 t_scheduler_fiber) 与 t_thread_fiber 进行切换的话
         * m_root_fiber 开始执行 run() 方法，而 run() 方法负责任务的处理
         * 那么再进行任务加入的话，就会漏掉任务队列中的任务
         * 所以再 stop() 方法中进行切换
         * ----------------------------------------------------------------
         */
    }

    void Scheduler::stop() {
        // 手动停止
        m_autoStop = true;
        // 使用use_caller,并且只有一个线程，并且主协程的状态为结束或者初始化
        if (m_root_fiber && m_thread_count == 0 &&
            (m_root_fiber->getState() == Fiber::TERM || m_root_fiber->getState() == Fiber::INIT)) {
            LSH_LOG_INFO(g_logger) << this->m_name << " sheduler stopped";
            // 停止状态为true
            m_stopping = true;

            // 若达到停止条件则直接return
            if (stopping()) {
                return;
            }
        }

        // use_caller线程
        // 当前调度器和t_secheduler相同
        if (m_root_threadId != -1) {
            LSH_ASSERT(GetThis() == this);
        }
        // 非use_caller，此时的t_secheduler应该为nullptr
        else {
            LSH_ASSERT(GetThis() != this);
        }

        // 停止状态为true
        m_stopping = true;

        // 每个线程都tickle一下
        for (size_t i = 0; i < m_thread_count; ++i) {
            tickle();
        }

        // 使用use_caller多tickle一下
        if (m_root_fiber) {
            tickle();
        }

        // 使用use_caller，只要没达到停止条件，当前线程主协程(t_thread_fiber)交出执行权，执行run
        // call() 方法是 swapcontext(&t_threadFiber->m_ucontext, &m_ucontext)
        // 这时候在执行的时候是 t_scheduler_fiber,任务队列中的协程需要与 t_scheduler_fiber 切换，
        // 也就是 swapIn() 方法。其它协程执行完之后，会调用 swapOut() 方法回到 t_scheduler_fiber
        // 当 run 方法执行完后，会调用 back() 回到 t_thread_fiber
        if (m_root_fiber) {
            if (!stopping()) {
                m_root_fiber->call();
            }
        }

        std::vector<Thread::ptr> thrs;
        {
            MutexType::Lock lock(m_mutex);
            thrs.swap(m_threads);
        }

        // 等待线程执行完成
        for (auto &i : thrs) {
            i->join();
        }
    }

    void Scheduler::setThis() {
        t_schedeluer = this;
    }

    void Scheduler::run() {
        LSH_LOG_INFO(g_logger) << "run";
        // return;
        set_hook_enable(true);
        setThis();

        /** ---------------------------------------------------------
         * 不是创建 scheduler 的线程（use_caller = false,或者其他线程）
         * 这个时候的线程就没有 t_thread_fiber 和 t_schedeluer_fiber 之分了，它们是一样的
         * 也就是没有 m_root_fiber 这个调度器调度协程
         * 这个时候也就不需要使用 call() 方法了，直接使用 swapIn()
         * swapcontext(&Scheduler::GetMainFiber()->m_ucontext, &m_ucontext)
         * 其他协程直接与 t_schedeluer_fiber 进行切换
         * 也就是不需要再有 t_schedeluer_fiber 和 t_thread_fiber 之间的切换了
         * ----------------------------------------------------------
         */
        if (GetThreadId() != m_root_threadId) {
            t_schedeluer_fiber = Fiber::GetThis().get();
        }

        // 定义 idle_fiber，当任务队列中的任务执行完之后，执行 idle()
        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
        Fiber::ptr cb_fiber;
        FiberAndThread ft;

        while (true) {
            ft.reset();
            bool tickle_me = false;
            bool is_active = false;
            {
                // 从任务队列中拿 fiber 和 cb
                MutexType::Lock lock(m_mutex);
                auto it = m_fibers.begin();
                while (it != m_fibers.end()) {
                    // 如果当前任务指定的线程不是当前线程，则跳过，并且tickle一下
                    if (it->threadId != -1 && it->threadId != lsh::GetThreadId()) {
                        it++;
                        tickle_me = true;
                        continue;
                    }
                    LSH_ASSERT(it->fiber || it->callback);
                    // 如果该fiber正在执行则跳过
                    if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
                        ++it;
                        continue;
                    }

                    // 取出该任务
                    ft = *it;
                    // 从任务队列中清除
                    m_fibers.erase(it);
                    // 正在执行任务的线程数量+1
                    ++m_active_thread_count;
                    is_active = true;
                    break;
                }
            }

            if (tickle_me) {
                tickle();
            }

            // 如果任务是fiber，并且任务处于可执行状态
            if (ft.fiber && (ft.fiber->getState() != Fiber::TERM || ft.fiber->getState() != Fiber::EXCEP)) {

                ft.fiber->swapIn();
                --m_active_thread_count;

                if (ft.fiber->getState() == Fiber::READY) {
                    schedule(ft.fiber);
                } else if (ft.fiber->getState() != Fiber::EXCEP && ft.fiber->getState() != Fiber::TERM) {
                    ft.fiber->m_state = Fiber::HOLD;
                }

                ft.reset();
            } else if (ft.callback) {
                // 如果任务是 cb
                if (cb_fiber) {
                    cb_fiber->reset(ft.callback);
                } else {
                    cb_fiber.reset(new Fiber(ft.callback));
                }

                ft.reset();
                cb_fiber->swapIn();
                --m_active_thread_count;

                if (cb_fiber->getState() == Fiber::READY) {
                    schedule(cb_fiber);
                    cb_fiber.reset();
                } else if (cb_fiber->getState() == Fiber::EXCEP || cb_fiber->getState() == Fiber::TERM) {
                    cb_fiber->reset(nullptr);
                } else { // if(cb_fiber->getState() != Fiber::TERM){
                    cb_fiber->m_state = Fiber::HOLD;
                    cb_fiber.reset();
                }
            } else {
                if (is_active) {
                    --m_active_thread_count;
                    continue;
                }
                if (idle_fiber->getState() == Fiber::TERM) {
                    LSH_LOG_INFO(g_logger) << "idle fiber treminate";
                    break;
                }

                ++m_idle_thread_count;
                idle_fiber->swapIn();
                --m_idle_thread_count;
                if (idle_fiber->getState() != Fiber::TERM && idle_fiber->getState() != Fiber::EXCEP) {
                    idle_fiber->m_state = (Fiber::HOLD);
                }
            }
        }
    }

    void Scheduler::tickle() {
        LSH_LOG_INFO(g_logger) << "tickle";
    }

    bool Scheduler::stopping() {
        MutexType::Lock lock(m_mutex);
        return m_autoStop && m_stopping && m_fibers.empty() && m_active_thread_count == 0;
    }

    void Scheduler::idle() {
        LSH_LOG_INFO(g_logger) << "idle";
        while (!stopping()) {
            lsh::Fiber::YieldToHold();
        }
    }
} // namespace lsh
