#ifndef __LSH_FIBER_H__
#define __LSH_FIBER_H__

#include "thread.h"
#include <functional>
#include <memory>
#include <ucontext.h>

/*
    thread->mainFiber<->sub_fiber
        |
        v
      sub_fiber
*/
namespace lsh {
    // Fiber 类的工作流程:
    //
    // Fiber 类是一个协程的实现，通过轻量级的线程切换来模拟协程的执行。协程是用户级线程，
    // 通过上下文切换实现并发执行，但不需要内核调度器的参与。在 `Fiber` 类中，每个协程都有一个独立的栈和执行上下文。
    // 主要使用 `ucontext_t` 来实现协程的上下文切换。以下是各个方法的交互工作流程：
    //
    // 1. **协程的创建**：
    //    - 当我们创建一个 `Fiber` 对象时，会调用 `Fiber(std::function<void()> cb, size_t stacksize)` 构造函数，
    //      该函数分配一个栈并初始化协程的上下文。
    //    - `m_ucontext` 保存了协程的执行上下文，包括栈指针、程序计数器等信息。
    //    - 协程的回调函数 (`cb`) 会被传递给 `m_clalback`，并设置 `MainFunc` 为协程的起始执行点。
    //
    // 2. **协程的状态管理**：
    //    - 每个协程都有一个状态，`State` 枚举类型表示不同的状态：`INIT`（初始状态）、
    //      `HOLD`（挂起状态）、`EXEC`（执行状态）、`TERM`（终止状态）、`READY`（准备状态）、
    //      `EXCEP`（异常状态）。
    //    - 协程创建后，状态默认是 `INIT`，执行过程中会根据实际情况切换到 `EXEC`、`TERM`、`HOLD` 等状态。
    //
    // 3. **上下文切换**：
    //    - 协程的执行通过 `ucontext_t` 实现上下文切换。
    //    - 使用 `getcontext()` 保存当前执行的上下文。
    //    - 使用 `swapcontext()` 来切换到其他协程的上下文，从而实现协程的执行或挂起。
    //    - `swapIn()` 会将当前线程上下文保存，并恢复目标协程的上下文，开始执行目标协程。
    //    - `swapOut()` 切换回调用者或其他协程，保存当前协程上下文并恢复目标协程的上下文。
    //
    // 4. **协程的执行与生命周期**：
    //    - 协程在执行时会调用 `Fiber::MainFunc()`，该函数会执行协程的回调函数（`m_clalback()`）。
    //    - 协程执行完毕后，状态会更新为 `TERM`（终止），并调用 `swapOut()` 切换回调用协程。
    //    - 如果协程在执行过程中发生异常，则状态会被设置为 `EXCEP`。
    //    - 协程的栈会在 `Fiber` 析构时被释放，避免内存泄漏。
    //
    // 5. **协程的生命周期管理**：
    //    - 当协程执行完成或需要重置时，可以通过 `reset()` 方法重置协程的回调函数，
    //      并重新分配栈内存，初始化协程。
    //    - `reset()` 会清理当前协程的资源，释放栈空间，并为协程重新分配栈，
    //      设置新的执行回调函数，并设置协程状态为 `INIT`。
    //
    // 6. **协程挂起与恢复**：
    //    - `YieldToReady()` 和 `YieldToHold()` 用于挂起当前协程并将其状态设置为 `READY` 或 `HOLD`。
    //      这两个方法内部会调用 `swapOut()` 来切换到其他协程。
    //      - `YieldToReady()`：将协程挂起并设置为 `READY` 状态，准备下一次执行。
    //      - `YieldToHold()`：将协程挂起并设置为 `HOLD` 状态，等待恢复执行。
    //    - `swapIn()` 和 `swapOut()` 是协程切换的核心方法，通过 `swapcontext()` 实现不同协程之间的切换。
    //
    // 7. **协程的统计信息**：
    //    - `TotalFibers()`：返回系统中创建的总协程数。
    //    - `GetFiberId()`：返回当前协程的 ID，方便追踪协程的执行。
    //    - `SetThis()`：设置当前线程的协程指针。
    //    - `GetThis()`：返回当前线程的协程指针。
    //
    // 总结：
    // `Fiber` 类通过 `ucontext_t` 上下文切换实现协程的创建、执行和调度。协程的生命周期包括创建、执行、挂起、
    // 恢复、重置以及销毁。在协程之间进行切换时，通过 `swapcontext()` 来保存和恢复协程的执行上下文，从而模拟
    // 线程的执行。`Fiber` 提供了灵活的协程管理机制，能够有效地模拟并发执行，并使得每个协程在执行时不需要
    // 内核的参与，从而减少了系统资源消耗。

    class Fiber : public std::enable_shared_from_this<Fiber> {
        friend class Scheduler;

    public:
        typedef std::shared_ptr<Fiber> ptr;

        /**
         * 协程的状态枚举类型：
         * INIT    - 协程初始化状态。
         * HOLD    - 协程被挂起，但尚未结束。
         * EXEC    - 协程正在执行。
         * TERM    - 协程执行完毕。
         * READY   - 协程准备好执行，等待调度。
         * EXCEP   - 协程执行中发生异常。
         */
        enum State {
            INIT,  // 初始状态
            HOLD,  // 挂起状态
            EXEC,  // 执行状态
            TERM,  // 终止状态
            READY, // 准备状态
            EXCEP  // 异常状态
        };

    private:
        Fiber(); // 默认构造函数用于创建主协程

    public:
        /**
         * 构造函数：创建一个协程，并指定协程的回调函数和栈大小。
         * @param cb     协程回调函数，协程开始执行时调用。
         * @param stacksize 栈大小，默认为0表示使用默认栈大小。
         */
        Fiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);

        /**
         * 析构函数：销毁协程并释放相关资源（例如栈内存）。
         */
        ~Fiber();

        /**
         * 重置协程的回调函数并重置协程的状态。
         * @param cb    新的协程回调函数。
         */
        void reset(std::function<void()> cb);

        /**
         * 切换到当前协程进行执行。当前协程的上下文会被保存，并切换到目标协程。
         */
        void swapIn();

        void call();
        void back();
        /**
         * 将当前协程切换到后台执行。将当前协程的上下文保存并切换到另一个协程。
         */
        void swapOut();

        /**
         * 获取协程的ID。
         * @return 协程的ID。
         */
        uint64_t getid() const { return m_id; }

        State getState() const { return m_state; }
        // void setState(State s) { m_state = s; }

    public:
        /**
         * 设置当前协程为当前线程的活动协程。
         * @param f 当前协程对象。
         */
        static void SetThis(Fiber *f);

        /**
         * 获取当前线程的活动协程。
         * @return 当前线程的活动协程指针。
         */
        static Fiber::ptr GetThis();

        /**
         * 当前协程切换到后台，并将其状态设置为 READY，等待调度。
         */
        static void YieldToReady();

        /**
         * 当前协程切换到后台，并将其状态设置为 HOLD，等待恢复。
         */
        static void YieldToHold();

        /**
         * 获取当前系统中协程的总数。
         * @return 系统中协程的数量。
         */
        static uint64_t TotalFibers();

        /**
         * 协程的主函数：每个协程从此函数开始执行。会执行协程的回调函数并在执行结束时切换上下文。
         */
        static void MainFunc();

        static void CallerMainFunc();

        /**
         * 获取当前协程的ID。
         * @return 当前协程的ID。
         */
        static uint64_t GetFiberId();

    private:
        uint64_t m_id{0};        // 协程的唯一ID
        uint32_t m_stacksize{0}; // 协程栈的大小
        State m_state{INIT};     // 当前协程的状态

        ucontext_t m_ucontext;   // 协程的上下文，用于保存协程的状态
        void *m_stack = nullptr; // 协程的栈内存

        std::function<void()> m_clalback; // 协程的执行方法
    };
} // namespace lsh

#endif