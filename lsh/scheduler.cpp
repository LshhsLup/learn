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
            m_root_fiber.reset(new Fiber(std::bind(&run, this)));
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
            m_threads[i].reset(new Thread(std::bind(&run, this), m_name + "_" + std::to_string(i)));
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
    }
} // namespace lsh
