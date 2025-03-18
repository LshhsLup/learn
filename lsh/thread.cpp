#include "thread.h"
#include "log.h"
#include "util.h"
#include <functional>

namespace lsh {

    //=================================================================================
    /**
     * @brief 构造函数，初始化信号量
     *
     * @param count 初始信号量值，决定了多少线程可以直接执行而无需等待。
     *
     * - `sem_init(&m_semaphore, 0, count)`:
     *   - `&m_semaphore`：信号量对象
     *   - `0`：表示该信号量用于线程同步（非进程间共享）
     *   - `count`：信号量初始值
     * - 若 `sem_init` 失败，则抛出异常。
     */
    Semaphore::Semaphore(uint32_t count) {
        if (sem_init(&m_semaphore, 0, count)) {
            throw std::logic_error("sem_init error");
        }
    }

    /**
     * @brief 析构函数，销毁信号量
     *
     * - `sem_destroy(&m_semaphore)`: 释放信号量资源。
     */
    Semaphore::~Semaphore() {
        sem_destroy(&m_semaphore);
    }

    /**
     * @brief P 操作（等待信号量）
     *
     * - `sem_wait(&m_semaphore)`:
     *   - 若信号量值 > 0，减 1 并继续执行。
     *   - 若信号量值 == 0，当前线程 **阻塞**，直到其他线程调用 `notify()`。
     * - 若 `sem_wait` 失败，则抛出异常。
     */
    void Semaphore::wait() {
        if (sem_wait(&m_semaphore)) {
            throw std::logic_error("sem_wait error");
        }
    }

    /**
     * @brief V 操作（释放信号量）
     *
     * - `sem_post(&m_semaphore)`:
     *   - 信号量值 +1
     *   - 若有线程因 `wait()` 阻塞，则 **唤醒** 一个等待的线程。
     * - 若 `sem_post` 失败，则抛出异常。
     */
    void Semaphore::notify() {
        if (sem_post(&m_semaphore)) {
            throw std::logic_error("sem_post error");
        }
    }

    //==================================================================================
    /*
     * 声明一个静态的线程局部变量 t_thread，它是一个指向 Thread 类对象的指针。
     * 静态（static）意味着该变量的存储期贯穿整个程序的生命周期。
     * 线程局部（thread_local）表明每个线程都会有该变量的独立副本，
     * 即每个线程对 t_thread 的操作不会影响其他线程中的 t_thread。
     * 初始值被设置为 nullptr，表示该指针在初始状态下不指向任何 Thread 对象。
     * 这个变量通常用于在多线程环境中跟踪每个线程对应的 Thread 对象，
     * 以便在当前线程的执行过程中可以方便地访问该 Thread 对象的成员变量和成员函数。
     */
    static thread_local Thread *t_thread = nullptr;
    static thread_local std::string t_thread_name = "UNKNOWN";

    // 系统日志
    static std::shared_ptr<Logger> g_logger = LSH_LOG_NAME("system");

    Thread *Thread::getThis() {
        return t_thread;
    }

    const std::string &Thread::GetName() {
        return t_thread_name;
    }

    void Thread::setName(const std::string &name) {
        if (t_thread) {
            t_thread->m_name = name;
        }
        t_thread_name = name;
    }

    /**
     * @brief 线程类 Thread 的工作流程
     *
     *  构造函数：
     *    - 接受回调函数和线程名称，并存储到成员变量中。
     *    - 调用 `pthread_create` 创建新线程，并传递 `this` 指针作为参数，使线程从 `run` 函数开始执行。
     *    - 如果 `pthread_create` 失败，则记录错误日志并抛出异常。
     */
    Thread::Thread(std::function<void()> call_back, const std::string &name) {
        if (name.empty()) {
            m_name = "UNKNOWN";
        }
        m_name = name;
        m_call_back = call_back;
        /*
         * 调用 pthread_create 函数创建新线程
         *
         * 参数说明：
         *     &m_thread：指向存储新线程标识符的变量的指针
         *     nullptr：使用默认的线程属性
         *     &run：线程启动函数的指针，新线程将从此函数开始执行
         *     this：将当前对象的指针传递给线程启动函数，以便在函数内使用对象的成员
         * 若创建成功，返回值为0
         */
        int rt = pthread_create(&m_thread, nullptr, &run, this);
        // 创建失败
        if (rt) {
            LSH_LOG_ERROR(g_logger) << "pthread_create thread failed, rt= " << rt << " name= " << name;
            throw std::logic_error("pthread_create error");
        }

        /*
         * 在构造函数中调用 m_semaphore.wait(); 阻塞主线程
         *
         * 作用：
         *  - 确保新创建的线程真正开始执行 `run()` 之后，主线程才会继续
         *  - 避免主线程一口气创建多个 `Thread` 对象，确保线程对象是 **按顺序创建**
         *  - 线程对象的创建顺序是确定的，run() 进入执行的顺序也是确定的
         *  - 但是不保证先创建的线程先执行完，这取决于 CPU 调度
         *  - 例如，Thread(1) 可能遇到 I/O 阻塞，而 Thread(2) 的任务很短，执行后直接结束
         *  - 如果任务极短，没有其他复杂操作，所以大概率执行完一个后才创建下一个，表现出“按创建顺序执行”
         *
         * 例如，在 main 函数中创建多个 Thread：
         *   Thread(1) -> Thread(2) -> Thread(3) ...
         *   由于 wait()，只有 Thread(1) 真正启动后，Thread(2) 才会被创建
         */
        m_semaphore.wait();
    }

    /**
     * @brief 析构函数，负责清理线程资源
     *
     * 当 Thread 对象销毁时，如果线程 m_thread 仍然存在，
     * 则调用 pthread_detach 使其分离，确保线程资源在结束时自动释放，
     * 防止产生僵尸线程。
     *
     * 注意：
     * 1. pthread_detach 使线程与主线程分离，线程结束后资源自动回收。
     * 2. 不能再对已分离的线程调用 pthread_join，否则会导致未定义行为。
     * 3. 如果需要确保线程在对象销毁前执行完毕，应使用 pthread_join 而非 pthread_detach。
     * 4. 如果使用 pthread_join,Thread 析构时会阻塞等待线程结束，这样做可能导致阻塞
     */
    Thread::~Thread() {
        if (m_thread) {
            pthread_detach(m_thread);
        }
    }

    void Thread::join() {
        if (m_thread) {
            int rt = pthread_join(m_thread, nullptr);
            if (rt) {
                LSH_LOG_ERROR(g_logger) << "pthread_join thread failed, rt= " << rt << " name= " << m_name;
                throw std::logic_error("pthread_join error");
            }
            m_thread = 0;
        }
    }

    /**
     * @brief 线程执行（run 函数）：
     *    - `run` 是 `pthread_create` 线程的启动函数，接收 `Thread` 对象指针 `arg`。
     *    - 将 `arg` 转换为 `Thread*` 类型，并赋值给 `t_thread`，用于在线程内部访问当前 `Thread` 对象。
     *    - 设置线程 ID，并使用 `pthread_setname_np` 设定线程名称，方便调试和日志分析。
     *    - 交换并执行存储的回调函数，线程执行完成后自动退出。
     * 注意：
     *    - run 在 pthread_create 成功后 立即在新线程中执行，而 join 只是等待线程结束，不会触发 run 执行
     */
    void *Thread::run(void *arg) {
        Thread *thread = (Thread *)arg;
        t_thread = thread;
        thread->m_id = lsh::GetThreadId();
        // 设置线程名称
        pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());
        t_thread_name = thread->m_name;

        /*
         * 交换回调函数 m_call_back 到局部变量 cb
         *
         * 1. 这样做的主要目的是 **防止线程对象 (Thread) 被销毁后访问已失效的成员变量**。
         * 2. `m_call_back` 是 `Thread` 的成员变量，如果 `Thread` 在 `run()` 运行期间被析构，
         *    直接访问 `thread->m_call_back();` 可能导致 **访问已销毁对象**，引发未定义行为。
         * 3. 通过 `swap`，`m_call_back` 的内容被移动到局部变量 `cb`，
         *    这样即使 `Thread` 被销毁，`cb` 仍然是有效的，不会访问 `Thread` 的成员。
         * 4. `thread->m_call_back` 置空，确保 `Thread` 不再持有任务，防止误用。
         */
        std::function<void()> cb;
        cb.swap(thread->m_call_back);

        /*
         * 释放构造函数中的阻塞，使主线程可以继续创建下一个 Thread
         *  - 这一步意味着当前线程已经正确启动，允许主线程继续创建下一个线程
         *  - 但线程任务的实际执行顺序仍然由 CPU 调度决定
         */
        thread->m_semaphore.notify();

        cb(); // 执行线程任务

        return 0;
    }
}