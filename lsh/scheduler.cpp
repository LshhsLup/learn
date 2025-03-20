#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include <cassert>

namespace lsh {
    std::shared_ptr<Logger> g_logger = LSH_LOG_NAME("system");

    static thread_local Scheduler *t_schedeluer = nullptr;
    static thread_local Fiber *t_fiber = nullptr;

    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name) {
        m_name = name;
        LSH_ASSERT(threads > 0);

        if (use_caller) {
            Fiber::GetThis();                 // 为当前线程初始化一个主协程,这个负责调度其余协程
            --threads;                        // 当前线程已经是一个了
            LSH_ASSERT(GetThis() == nullptr); // 当前线程执行构造函数时应该没有 Scheduler
            t_schedeluer = this;

            // 这个协程是真正执行方法的协程
            m_root_fiber.reset(new Fiber(std::bind(&Scheduler::run, this)));
            Thread::setName(m_name);

            t_fiber = m_root_fiber.get();
            m_root_threadId = GetThreadId();
            m_threadIds.push_back(m_root_threadId);
        } else {
            m_root_threadId = -1;
        }
        m_thread_count = threads;
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
        return t_fiber;
    }

    void Scheduler::start() {
        MutexType::Lock lock(m_mutex);
        if (!m_stopping) {
            return; // 启动了
        }
        m_stopping = false;
        LSH_ASSERT(m_threads.empty());
        m_threads.resize(m_thread_count);
        for (size_t i = 0; i < m_thread_count; i++) {
            m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
            m_threadIds.push_back(m_threads[i]->getId());
        }
    }

    void Scheduler::stop() {
        m_autoStop = true;
        if (m_root_fiber && m_thread_count == 0 &&
            (m_root_fiber->getState() == Fiber::TERM || m_root_fiber->getState() == Fiber::INIT)) {
            LSH_LOG_INFO(g_logger) << this << " stopped";
            m_stopping = true;

            if (stopping()) {
                return;
            }
        }
        bool exit_on_this_fiber = false;
        if (m_root_threadId != -1) {
            LSH_ASSERT(GetThis() == this);
        } else {
            LSH_ASSERT(GetThis() != this);
        }
        m_stopping = true;
        for (size_t i = 0; i < m_thread_count; i++) {
            tickle();
        }

        if (m_root_fiber) {
            tickle();
        }
        if (stopping()) {
            return;
        }
        // if
    }

    void Scheduler::setThis() {
        t_schedeluer = this;
    }

    void Scheduler::run() {
        setThis();
        if (GetThreadId() != m_root_threadId) {
            t_fiber = Fiber::GetThis().get();
        }

        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
        Fiber::ptr cb_fiber;
        FiberAndThread ft;

        while (true) {
            ft.reset();
            bool tickle_me = false;
            {
                MutexType::Lock lock(m_mutex);
                auto it = m_fibers.begin();
                while (it != m_fibers.end()) {
                    if (it->threadId != -1 && it->threadId != lsh::GetThreadId()) {
                        it++;
                        tickle_me = true;
                        continue;
                    }
                    LSH_ASSERT(it->fiber || it->callback);
                    if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
                        ++it;
                        continue;
                    }
                    ft = *it;
                    m_fibers.erase(it);
                }
            }

            if (tickle_me) {
                tickle();
            }

            if (ft.fiber && (ft.fiber->getState() != Fiber::TERM) || ft.fiber->getState() != Fiber::EXCEP) {
                ++m_active_thread_count;
                ft.fiber->swapIn();
                --m_active_thread_count;

                if (ft.fiber->getState() == Fiber::READY) {
                    schedule(ft.fiber);
                } else if (ft.fiber->getState() != Fiber::EXCEP && ft.fiber->getState() != Fiber::TERM) {
                    ft.fiber->setState(Fiber::HOLD);
                }

                ft.reset();
            } else if (ft.callback) {
                if (cb_fiber) {
                    cb_fiber->reset(ft.callback); // ?
                } else {
                    cb_fiber.reset(new Fiber(ft.callback));
                }

                ft.reset();
                ++m_active_thread_count;
                cb_fiber->swapIn();
                --m_active_thread_count;

                if (cb_fiber->getState() == Fiber::READY) {
                    schedule(cb_fiber);
                    cb_fiber.reset();
                } else if (cb_fiber->getState() == Fiber::EXCEP || cb_fiber->getState() == Fiber::TERM) {
                    cb_fiber->reset(nullptr);
                } else { // if(cb_fiber->getState() != Fiber::TERM){
                    cb_fiber->setState(Fiber::HOLD);
                    cb_fiber.reset();
                }
            } else {
                if (idle_fiber->getState() == Fiber::TERM) {
                    break;
                }

                ++m_idle_thread_count;
                idle_fiber->swapIn();
                --m_idle_thread_count;
                if (idle_fiber->getState() != Fiber::TERM || idle_fiber->getState() != Fiber::EXCEP) {
                    idle_fiber->setState(Fiber::HOLD);
                }
            }
        }
    }
} // namespace lsh
