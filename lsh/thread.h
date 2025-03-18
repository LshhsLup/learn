#ifndef __LSH_THREAD_H__
#define __LSH_THREAD_H__

#include <atomic>
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <thread>

namespace lsh {
    //=================================================================================
    /**
     * @brief 基于 POSIX 信号量 (sem_t) 封装的信号量类，用于线程同步
     *
     * 该信号量用于 **控制多个线程对共享资源的访问**，以及 **保证特定操作的顺序**。
     *
     * 工作方式：
     * 1. `wait()`: 线程调用此函数后：
     *    - 若信号量值 > 0，则信号量值减 1，线程继续执行。
     *    - 若信号量值 == 0，则线程阻塞，直到其他线程调用 `notify()` 释放资源。
     * 2. `notify()`: 线程调用此函数后：
     *    - 信号量值加 1，若有线程因 `wait()` 阻塞，则被唤醒。
     *
     * 使用场景：
     * - **控制线程并发数量**，如限制同时运行的线程数。
     * - **确保线程按特定顺序执行**，如线程创建后按顺序启动 `run()`。
     */
    class Semaphore {
    public:
        /**
         * @brief 构造函数，初始化信号量
         * @param count 初始信号量计数值，默认值为 0
         *
         * count 的作用：
         * - 若 count > 0，则最多有 count 个线程 **无需等待** 可立即执行。
         * - 若 count = 0，则第一个调用 `wait()` 的线程会 **阻塞**，直到其他线程 `notify()`。
         */
        Semaphore(uint32_t count = 0);

        /**
         * @brief 析构函数，销毁信号量
         */
        ~Semaphore();

        /**
         * @brief 等待信号量（P 操作）
         *
         * - 若信号量值 > 0，减 1 并继续执行。
         * - 若信号量值 == 0，线程 **阻塞**，直到其他线程 `notify()`。
         */
        void wait();

        /**
         * @brief 释放信号量（V 操作）
         *
         * - 信号量值加 1，若有线程在 `wait()` 上阻塞，则 **唤醒** 一个。
         */
        void notify();

    private:
        // 禁止拷贝和移动，保证信号量对象的唯一性
        Semaphore(const Semaphore &) = delete;
        Semaphore(Semaphore &&) = delete;
        Semaphore &operator=(const Semaphore &) = delete;
        Semaphore &operator=(Semaphore &&) = delete;

    private:
        sem_t m_semaphore; ///< POSIX 信号量
    };

    //====================================================================================
    /**
     * @brief 通用互斥锁管理类（RAII 机制）
     *
     * @tparam T 需要管理的锁类型（必须提供 lock() 和 unlock() 方法）
     */
    template <class T>
    struct ScopedLockImpl {
    public:
        /**
         * @brief 构造函数，获取锁
         * @param mutex 传入锁的引用
         */
        ScopedLockImpl(T &mutex) : m_mutex(mutex) {
            lock();
            m_locked = true;
        }

        /**
         * @brief 析构函数，释放锁
         */
        ~ScopedLockImpl() {
            unlock();
        }

        /**
         * @brief 手动加锁
         */
        void lock() {
            if (!m_locked) {
                m_mutex.lock();
                m_locked = true;
            }
        }

        /**
         * @brief 手动解锁
         */
        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T &m_mutex;            ///< 互斥锁的引用
        bool m_locked = false; ///< 标记当前是否持有锁
    };

    /**
     * @brief 基于 pthread_mutex_t 的标准互斥锁
     */
    class Mutex {
    public:
        typedef ScopedLockImpl<Mutex> Lock; ///< RAII 方式的锁

        Mutex() {
            pthread_mutex_init(&m_mutex, nullptr);
        }

        ~Mutex() {
            pthread_mutex_destroy(&m_mutex);
        }

        /** @brief 加锁 */
        void lock() {
            pthread_mutex_lock(&m_mutex);
        }

        /** @brief 解锁 */
        void unlock() {
            pthread_mutex_unlock(&m_mutex);
        }

    private:
        pthread_mutex_t m_mutex; ///< 互斥锁
    };

    /**
     * @brief 空互斥锁（不做任何操作），用于无锁情况下的占位
     */
    class NULLMutex {
    public:
        typedef ScopedLockImpl<NULLMutex> Lock;
        NULLMutex() {}
        ~NULLMutex() {}
        void lock() {}
        void unlock() {}
    };

    /**
     * @brief 自旋锁（Spinlock），适用于短时间锁争用场景
     */
    class Spinlock {
    public:
        typedef ScopedLockImpl<Spinlock> Lock;

        Spinlock() {
            pthread_spin_init(&m_mutex, 0);
        }

        ~Spinlock() {
            pthread_spin_destroy(&m_mutex);
        }

        /** @brief 加锁（自旋等待） */
        void lock() {
            pthread_spin_lock(&m_mutex);
        }

        /** @brief 解锁 */
        void unlock() {
            pthread_spin_unlock(&m_mutex);
        }

    private:
        pthread_spinlock_t m_mutex; ///< pthread 自旋锁
    };

    /**
     * @brief CAS（Compare-And-Swap）锁，无需操作系统调用，适用于轻量级同步
     */
    class CASLock {
    public:
        typedef ScopedLockImpl<CASLock> Lock;

        CASLock() {
            m_mutex.clear();
        }
        ~CASLock() = default;

        /** @brief 使用 CAS 进行加锁 */
        void lock() {
            while (m_mutex.test_and_set(std::memory_order_acquire))
                ;
        }

        /** @brief 解锁 */
        void unlock() {
            m_mutex.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag m_mutex = ATOMIC_FLAG_INIT; ///< 原子标志位
    };

    /**
     * @brief 读写锁的 RAII 读锁封装
     */
    template <class T>
    struct ReadScopedLockImpl {
    public:
        ReadScopedLockImpl(T &mutex) : m_mutex(mutex) {
            lock();
            m_locked = true;
        }

        ~ReadScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (!m_locked) {
                m_mutex.rlock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T &m_mutex;
        bool m_locked = false;
    };

    /**
     * @brief 读写锁的 RAII 写锁封装
     */
    template <class T>
    struct WriteScopedLockImpl {
    public:
        WriteScopedLockImpl(T &mutex) : m_mutex(mutex) {
            lock();
            m_locked = true;
        }

        ~WriteScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (!m_locked) {
                m_mutex.wlock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }

    private:
        T &m_mutex;
        bool m_locked = false;
    };

    /**
     * @brief 空的读写锁（不做任何操作），用于无锁环境占位
     */
    class NULLRWMutex {
    public:
        typedef ReadScopedLockImpl<NULLRWMutex> rdlock;
        typedef WriteScopedLockImpl<NULLRWMutex> wrlock;
        NULLRWMutex() {}
        ~NULLRWMutex() {}
        void rlock() {}
        void wlock() {}
        void unlock() {}
    };

    /**
     * @brief 读写锁（RWMutex），允许多个读线程但互斥写
     */
    class RWMutex {
    public:
        typedef ReadScopedLockImpl<RWMutex> ReadLock;
        typedef WriteScopedLockImpl<RWMutex> WriteLock;

        RWMutex() {
            pthread_rwlock_init(&m_rwlock, nullptr);
        }

        ~RWMutex() {
            pthread_rwlock_destroy(&m_rwlock);
        }

        /** @brief 读锁（多个线程可同时持有） */
        void rlock() {
            pthread_rwlock_rdlock(&m_rwlock);
        }

        /** @brief 写锁（互斥） */
        void wlock() {
            pthread_rwlock_wrlock(&m_rwlock);
        }

        /** @brief 解锁（适用于读或写锁） */
        void unlock() {
            pthread_rwlock_unlock(&m_rwlock);
        }

    private:
        pthread_rwlock_t m_rwlock; ///< 读写锁
    };

    //=====================================================================================================
    class Thread {
    public:
        typedef std::shared_ptr<Thread> ptr;

        Thread(std::function<void()> call_back, const std::string &name);
        ~Thread();

        // 获取线程的 ID
        pid_t getId() const { return m_id; }
        // 获取线程的名称
        const std::string &getName() const { return m_name; }

        // 等待线程执行完毕
        void join();

        // 获取当前正在执行的线程所对应的 Thread 对象指针
        static Thread *getThis();
        // 获取线程名称
        static const std::string &GetName();
        // 设置线程名称
        static void setName(const std::string &name);

    private:
        // 线程作为不可以拷贝的实体
        Thread(const Thread &) = delete;
        Thread(const Thread &&) = delete;
        Thread &operator=(const Thread &) = delete;
        Thread &operator=(Thread &&) = delete;

        /**
         * @brief 新线程的启动函数。
         *
         * 作为静态成员函数，可作为 `pthread_create` 的线程启动函数。
         * 它接收从 `pthread_create` 传递过来的参数，并在内部进行类型转换后使用。
         *
         * @param arg 从 `pthread_create` 传递过来的参数，类型为 `void*`。
         * @return 线程执行完毕后返回的结果，类型为 `void*`。
         */
        static void *run(void *arg);

    private:
        pid_t m_id{-1};                    // 线程 id
        pthread_t m_thread{0};             // 线程句柄，用于操作线程，如等待线程结束、取消线程等
        std::function<void()> m_call_back; // 线程要执行的任务
        std::string m_name;                // 线程名称

        Semaphore m_semaphore;
    };
}

#endif