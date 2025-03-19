#include "fiber.h"
#include "config.h"
#include "log.h"
#include "macro.h"
#include <atomic>

namespace lsh {

    static std::shared_ptr<Logger> g_logger = LSH_LOG_NAME("system");

    // 用于生成和管理协程的唯一ID
    static std::atomic<uint64_t> s_fiber_id{0};

    // 用于跟踪当前活动的协程数量
    static std::atomic<uint32_t> s_fiber_count{0};

    // 当前线程的协程指针，用于表示当前执行的协程
    static thread_local Fiber *t_fiber = nullptr;

    // 当前线程的主协程指针，表示线程中的主协程（即非用户创建的协程）
    static thread_local Fiber::ptr t_threadFiber = nullptr;

    // 配置文件中的协程栈大小，默认为1MB
    static ConfigVar<uint32_t>::ptr g_fiber_statck_size = Config::Creat<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size");

    // 定义了一个内存分配器类，用于分配和释放协程的栈内存
    class MallocStackAllocator {
    public:
        // 分配栈内存。
        static void *Alloc(size_t size) {
            return malloc(size);
        }

        // 释放栈内存。
        static void Dealloc(void *vp, size_t size) {
            return free(vp);
        }
    };

    // 使用 MallocStackAllocator 来作为栈内存分配器
    using StackAllocator = MallocStackAllocator;

    // 默认构造函数，表示当前线程的主协程
    // 主协程不需要额外分配栈内存或回调函数，是因为它是操作系统级的线程的一部分，
    // 已经由操作系统为其提供了栈和上下文管理
    // 而用户创建的协程则需要由程序自行管理栈内存和回调函数，
    // 因为它们是轻量级线程，独立于操作系统线程的调度和管理
    Fiber::Fiber() {
        m_state = EXEC; // 设置当前协程状态为执行中
        SetThis(this);  // 将当前协程设置为当前线程的活动协程

        // 获取当前协程的上下文，如果失败则触发断言
        if (getcontext(&m_ucontext)) {
            LSH_ASSERT_MSG(false, "getcontext");
        }

        ++s_fiber_count; // 协程数量加一，表示当前主协程
        LSH_LOG_DEBUG(g_logger) << "Fiber:Fiber";
    }

    // 构造函数，创建一个新的协程，指定协程的回调函数和栈大小
    Fiber::Fiber(std::function<void()> cb, size_t stacksize) {
        m_id = ++s_fiber_id;                                                        // 为协程分配唯一的ID
        m_clalback = cb;                                                            // 设置协程的回调函数
        ++s_fiber_count;                                                            // 协程数量加一
        m_stacksize = stacksize != 0 ? stacksize : g_fiber_statck_size->getValue(); // 使用配置的栈大小（默认为1MB）

        // 分配栈内存
        m_stack = StackAllocator::Alloc(m_stacksize);
        if (getcontext(&m_ucontext)) {
            LSH_ASSERT_MSG(false, "getcontext");
        }

        // 设置协程的上下文，指定栈的起始位置和栈的大小
        m_ucontext.uc_link = nullptr;
        m_ucontext.uc_stack.ss_sp = m_stack;
        m_ucontext.uc_stack.ss_size = m_stacksize;

        // 设置协程的执行入口函数
        makecontext(&m_ucontext, &MainFunc, 0);
        LSH_LOG_DEBUG(g_logger) << "Fiber:Fiber id=" << m_id;
    }

    // 析构函数，销毁协程并释放相关资源
    Fiber::~Fiber() {
        --s_fiber_count; // 协程数量减一

        if (m_stack) {
            // 如果协程有栈内存，则进行栈内存的释放
            LSH_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEP);
            StackAllocator::Dealloc(m_stack, m_stacksize);
        } else {
            // 如果协程没有栈内存，说明是主协程，在销毁时需要清理当前线程的活动协程
            LSH_ASSERT(!m_clalback);     // 确保没有回调函数
            LSH_ASSERT(m_state == EXEC); // 确保当前协程是执行状态
            Fiber *cur = t_fiber;
            if (cur == this) {
                SetThis(nullptr); // 清空当前协程指针
            }
        }

        LSH_LOG_DEBUG(g_logger) << "Fiber:~Fiber id=" << m_id;
    }

    // 重置协程的回调函数，并重新初始化栈空间
    void Fiber::reset(std::function<void()> cb) {
        LSH_ASSERT(m_stack);                                                // 确保协程有栈内存
        LSH_ASSERT(m_state == TERM || m_state == INIT || m_state == EXCEP); // 只允许在协程终止、初始化或异常状态下重置

        m_clalback = cb; // 设置新的回调函数

        // 重新分配栈内存
        m_stack = StackAllocator::Alloc(m_stacksize);
        if (getcontext(&m_ucontext)) {
            LSH_ASSERT_MSG(false, "getcontext");
        }

        // 设置新的上下文
        m_ucontext.uc_link = nullptr;
        m_ucontext.uc_stack.ss_sp = m_stack;
        m_ucontext.uc_stack.ss_size = m_stacksize;

        // 设置协程的执行入口函数
        makecontext(&m_ucontext, &MainFunc, 0);
        m_state = INIT; // 将协程状态设置为初始化状态
    }

    // 切换到当前协程执行
    void Fiber::swapIn() {
        SetThis(this);               // 设置当前协程为活动协程
        LSH_ASSERT(m_state != EXEC); // 确保当前协程未在执行中
        m_state = EXEC;              // 设置协程状态为执行中

        // 切换上下文，从当前线程的协程切换到当前协程
        if (swapcontext(&t_threadFiber->m_ucontext, &m_ucontext)) {
            LSH_ASSERT_MSG(false, "swapcontext");
        }
    }

    // 切换到后台执行当前协程
    void Fiber::swapOut() {
        SetThis(t_threadFiber.get()); // 设置当前线程的协程为主协程
        // 切换上下文，从当前协程切换到主协程。
        if (swapcontext(&m_ucontext, &t_threadFiber->m_ucontext)) {
            LSH_ASSERT_MSG(false, "swapcontext");
        }
    }

    // 设置当前协程为活动协程
    void Fiber::SetThis(Fiber *f) {
        t_fiber = f;
    }

    // 返回当前协程的 shared_ptr
    Fiber::ptr Fiber::GetThis() {
        if (t_fiber) {
            return t_fiber->shared_from_this(); // 如果当前协程已存在，返回其 shared_ptr
        }

        // 如果当前协程为空，表示主协程
        Fiber::ptr main_fiber(new Fiber);
        LSH_ASSERT(t_fiber == main_fiber.get()); // 确保主协程是当前线程的活动协程
        t_threadFiber = main_fiber;              // 设置主协程

        return t_fiber->shared_from_this(); // 返回主协程的 shared_ptr
    }

    // 协程切换到后台，并且设置为 READY 状态，等待调度
    void Fiber::YieldToReady() {
        Fiber::ptr cur = GetThis();
        cur->m_state = READY; // 设置当前协程状态为 READY
        cur->swapOut();       // 切换到后台执行
    }

    // 协程切换到后台，并且设置为 HOLD 状态，挂起协程。
    void Fiber::YieldToHold() {
        Fiber::ptr cur = GetThis();
        cur->m_state = HOLD; // 设置当前协程状态为 HOLD
        cur->swapOut();      // 切换到后台执行
    }

    // 获取系统中当前的协程总数
    uint64_t Fiber::TotalFibers() {
        return s_fiber_count; // 返回协程数量
    }

    // 协程的主执行函数，执行协程的回调函数并处理异常
    void Fiber::MainFunc() {
        Fiber::ptr cur = GetThis();
        LSH_ASSERT(cur); // 确保当前协程有效
        try {
            cur->m_clalback();         // 执行协程的回调函数
            cur->m_clalback = nullptr; // 执行完后清空回调函数
            cur->m_state = TERM;       // 将协程状态设置为终止
        } catch (const std::exception &e) {
            cur->m_state = EXCEP; // 发生异常时设置为 EXCEP 状态
            LSH_LOG_ERROR(g_logger) << "Fiber exception: " << e.what();
        } catch (...) {
            cur->m_state = EXCEP; // 发生未知异常时设置为 EXCEP 状态
            LSH_LOG_ERROR(g_logger) << "Fiber exception";
        }

        auto raw_ptr = cur.get();
        cur.reset();
        raw_ptr->swapOut();
    }

    // 获取当前活动协程的ID
    uint64_t Fiber::GetFiberId() {
        if (t_fiber) {
            return t_fiber->getid(); // 返回当前协程的ID
        }
        return 0; // 如果没有活动协程，则返回0
    }

}