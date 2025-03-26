#include "IOManager.h"
#include "log.h"
#include "macro.h"
#include <algorithm>
#include <fcntl.h>
#include <functional>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

namespace lsh {

    static std::shared_ptr<Logger> g_logger = LSH_LOG_NAME("system");

    IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
        : Scheduler(threads, use_caller, name) {

        // 创建 epoll 实例，最多支持 5000 个文件描述符的监听
        m_epoll_fd = epoll_create(5000);
        // 确保 epoll 创建成功
        LSH_ASSERT(m_epoll_fd > 0);

        // 创建管道，管道包含两个文件描述符
        // m_tickle_fds[0] 是读端，m_tickle_fds[1] 是写端
        int rt = pipe(m_tickle_fds);
        // 确保管道创建成功
        LSH_ASSERT(rt == 0);

        // 初始化 epoll_event 结构体，用于配置 epoll 监听的事件
        epoll_event event;
        memset(&event, 0, sizeof(epoll_event)); // 清空结构体
        // 设置事件类型：EPOLLIN 表示数据可读，EPOLLET 表示边缘触发模式
        event.events = EPOLLIN | EPOLLET;
        // 设置与该事件关联的文件描述符为管道的读端
        event.data.fd = m_tickle_fds[0];

        // 设置管道读端为非阻塞模式，O_NONBLOCK 表示非阻塞读取
        rt = fcntl(m_tickle_fds[0], F_SETFL, O_NONBLOCK);
        // 确保设置成功
        LSH_ASSERT(rt == 0);

        // 将管道读端添加到 epoll 实例中，开始监听事件
        // EPOLL_CTL_ADD 表示将文件描述符添加到 epoll 监听队列中
        rt = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_tickle_fds[0], &event);
        // 确保 epoll_ctl 调用成功
        LSH_ASSERT(rt == 0);

        contextResize(32);

        // 启动调度器
        start();
    }

    IOManager::~IOManager() {
        stop();
        // 关闭 epoll 实例的文件描述符
        close(m_epoll_fd);
        // // 关闭管道的读端和写端
        close(m_tickle_fds[0]);
        close(m_tickle_fds[1]);
        for (size_t i = 0; i < m_fdContext.size(); i++) {
            delete m_fdContext[i];
        }
    }

    void IOManager::contextResize(size_t size) {
        m_fdContext.resize(size);
        for (size_t i = 0; i < m_fdContext.size(); i++) {
            if (!m_fdContext[i]) {
                m_fdContext[i] = new FdContext;
                m_fdContext[i]->fd = i;
            }
        }
    }

    // 1 success 0 retry -1 error
    int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
        FdContext *fd_ctx = nullptr;
        // 从 m_fdContexts 中拿到对应的 FdContext
        RWMutexType::ReadLock lock(m_mutex);
        if (m_fdContext.size() > fd) {
            fd_ctx = m_fdContext[fd];
            lock.unlock();
        } else {
            lock.unlock();
            RWMutexType::WriteLock lock_(m_mutex);
            contextResize(fd * 1.5);
            fd_ctx = m_fdContext[fd];
        }

        FdContext::MutexType::Lock __lock(fd_ctx->mutex);
        // 一个句柄一般不会重复加同一个事件， 可能是两个不同的线程在操控同一个句柄添加事件
        if (fd_ctx->events & event) {
            LSH_LOG_INFO(g_logger) << "addEvent assert fd="
                                   << fd << " event="
                                   << event << " fd_ctx.event="
                                   << fd_ctx->events;
            LSH_ASSERT(!(fd_ctx->events & event));
        }

        // 判断当前 fd_ctx 是否已有事件注册
        // 如果已有事件，则执行修改操作 (EPOLL_CTL_MOD)
        // 如果没有事件，则执行添加操作 (EPOLL_CTL_ADD)
        int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

        // 创建一个 epoll_event 结构，准备填充事件类型和用户数据
        epoll_event epevent;

        // 设置需要监控的事件类型
        // 使用 EPOLLET 表示边缘触发模式 (Edge Triggered)
        // 然后将已有的事件 (fd_ctx->events) 和当前需要添加的事件 (event) 合并
        // 使用按位“或”运算符 (|) 将这些事件组合在一起
        // 这样就能同时监控多个事件（如 READ、WRITE）
        epevent.events = EPOLLET | fd_ctx->events | event;

        // 将 fd_ctx 存储到 epevent.data.ptr 中
        // 这样在 epoll 事件触发时，可以通过 epevent.data.ptr 获取与该事件相关的上下文信息
        epevent.data.ptr = fd_ctx;

        // 注册事件
        int rt = epoll_ctl(m_epoll_fd, op, fd, &epevent);

        if (rt) {
            LSH_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epoll_fd << ", "
                                    << op << ", " << fd << ", " << epevent.events
                                    << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return -1;
        }

        ++m_pendingEventCount;
        // 将 fd_ctx 的注册事件更新
        fd_ctx->events = (Event)(fd_ctx->events | event);
        FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
        LSH_ASSERT(!event_ctx.scheduler && !event_ctx.cb && !event_ctx.fiber);

        event_ctx.scheduler = Scheduler::GetThis();
        if (cb) {
            event_ctx.cb.swap(cb);
        } else {
            event_ctx.fiber = Fiber::GetThis();
            LSH_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
        }

        return 0;
    }

    bool IOManager::delEvent(int fd, Event event) {
        RWMutexType::ReadLock lock(m_mutex);
        if (m_fdContext.size() <= fd) {
            return false;
        }

        FdContext *fd_ctx = m_fdContext[fd];
        lock.unlock();

        FdContext::MutexType::Lock __lock(fd_ctx->mutex);
        if (!(fd_ctx->events & event)) {
            return false;
        }

        // 将事件从注册事件中删除
        Event new_events = (Event)(fd_ctx->events & ~event);
        // 若还有事件则是修改，若没事件了则删除
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

        epoll_event epevent;
        // 水平触发模式，新的注册事件
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;
        // 注册事件
        int rt = epoll_ctl(m_epoll_fd, op, fd, &epevent);
        if (rt) {
            LSH_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epoll_fd << ", "
                                    << op << ", " << fd << ", " << epevent.events
                                    << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }
        // 等待执行的事件数量-1
        --m_pendingEventCount;
        // 更新事件
        fd_ctx->events = new_events;
        // 拿到对应事件的 EventContext
        FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
        // 重置 EventContext
        fd_ctx->resetContext(event_ctx);
        return true;
    }

    bool IOManager::cnacelEvent(int fd, Event event) {
        // 找到对应的事件强制触发执行
        RWMutexType::ReadLock lock(m_mutex);
        if (m_fdContext.size() <= fd) {
            return false;
        }

        FdContext *fd_ctx = m_fdContext[fd];
        lock.unlock();

        FdContext::MutexType::Lock __lock(fd_ctx->mutex);
        if (!(fd_ctx->events & event)) {
            return false;
        }

        Event new_events = (Event)(fd_ctx->events & ~event);
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

        epoll_event epevent;
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;
        int rt = epoll_ctl(m_epoll_fd, op, fd, &epevent);
        if (rt) {
            LSH_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epoll_fd << ", "
                                    << op << ", " << fd << ", " << epevent.events
                                    << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }
        // 触发要取消的事件
        fd_ctx->triggerEvent(event);
        --m_pendingEventCount;

        // 这里再 trigger() 中更新
        // fd_ctx->events = new_events;
        return true;
    }

    bool IOManager::cancelAll(int fd) {
        // 把这个句柄下的所有任务都取消
        RWMutexType::ReadLock lock(m_mutex);
        if (m_fdContext.size() <= fd) {
            return false;
        }

        FdContext *fd_ctx = m_fdContext[fd];
        lock.unlock();

        FdContext::MutexType::Lock __lock(fd_ctx->mutex);
        if (!fd_ctx->events) {
            return false;
        }

        int op = EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = 0;
        epevent.data.ptr = fd_ctx;
        int rt = epoll_ctl(m_epoll_fd, op, fd, &epevent);
        if (rt) {
            LSH_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epoll_fd << ", "
                                    << op << ", " << fd << ", " << epevent.events
                                    << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }

        if (fd_ctx->events & READ) {
            // 触发要取消的事件
            fd_ctx->triggerEvent(READ);
            // 触发要取消的事件
            // event_ctx->trigger(event);
            --m_pendingEventCount;
        }

        if (fd_ctx->events & WRITE) {
            // 触发要取消的事件
            fd_ctx->triggerEvent(WRITE);
            --m_pendingEventCount;
        }

        LSH_ASSERT(fd_ctx->events == 0);
        return true;
    }

    IOManager *IOManager::GetThis() {
        return dynamic_cast<IOManager *>(Scheduler::GetThis());
    }

    // 通知有任务
    void IOManager::tickle() {
        // 没有空闲的线程
        if (!hasIdleThread()) {
            return;
        }

        // 有任务来了，就往 pipe 里发送1个字节的数据，这样 epoll_wait 就会唤醒
        int rt = write(m_tickle_fds[1], "T", 1);
        LSH_ASSERT(rt == 1);
    }

    bool IOManager::stopping(uint64_t &timeout) {
        timeout = getNextTimer();
        return timeout == ~0ull && Scheduler::stopping() && m_pendingEventCount == 0;
    }

    bool IOManager::stopping() {
        uint64_t timeout = 0;
        return stopping(timeout);
    }

    void IOManager::idle() {
        epoll_event *events = new epoll_event[64]();
        // 使用智能指针托管events， 离开idle自动释放
        std::shared_ptr<epoll_event> shared_events(events, [](epoll_event *events) { delete[] events; });

        while (true) {
            uint64_t next_timeout = 0;
            if (stopping(next_timeout)) {
                LSH_LOG_INFO(g_logger) << "name=" << getName() << " idle stopping,exit";
                break;
            }

            int rt = 0;
            do {
                static const int MAX_TIMEOUT = 1000;
                if (next_timeout != ~0ull) {
                    next_timeout = (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
                } else {
                    next_timeout = MAX_TIMEOUT;
                }
                /*
                 * 阻塞在这里，但有3种情况能够唤醒epoll_wait
                 * 1. 超时时间到了
                 * 2. 关注的 fd 有数据来了
                 * 3. 通过 tickle 往 pipe 里发数据，表明有任务来了
                 */
                rt = epoll_wait(m_epoll_fd, events, 64, (int)next_timeout);
                /* 这里就是源码 ep_poll() 中由操作系统中断返回的 EINTR
                 * 需要重新尝试 epoll_Wait */
                if (rt < 0 && errno == EINTR) {

                } else {
                    break;
                }
            } while (true);

            std::vector<std::function<void()>> cbs;
            listExpiredCb(cbs);
            if (!cbs.empty()) {
                schedule(cbs.begin(), cbs.end());
                cbs.clear();
            }

            for (int i = 0; i < rt; i++) {
                // 从 events 中拿一个 event
                epoll_event &event = events[i];
                // 如果获得的这个信息时来自 pipe
                if (event.data.fd == m_tickle_fds[0]) {
                    uint8_t dummy;
                    // 将 pipe 发来的1个字节数据读掉
                    while (read(m_tickle_fds[0], &dummy, 1) == 1)
                        ;
                    continue;
                }

                // 从 ptr 中拿出 FdContext
                FdContext *fd_ctx = (FdContext *)event.data.ptr;
                FdContext::MutexType::Lock lock(fd_ctx->mutex);
                /* 在源码中，注册事件时内核会自动关注POLLERR和POLLHUP */
                if (event.events & (EPOLLERR | EPOLLHUP)) {
                    // 将读写事件都加上
                    event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
                }

                int real_events = NONE;
                // 读事件好了
                if (event.events & EPOLLIN) {
                    real_events |= READ;
                }
                // 写事件好了
                if (event.events & EPOLLOUT) {
                    real_events |= WRITE;
                }

                // 没事件
                if ((fd_ctx->events & real_events) == NONE) {
                    continue;
                }

                // 剩余的事件
                int left_events = (fd_ctx->events & ~real_events);
                // 如果执行完该事件还有事件则修改，若无事件则删除
                int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
                // 更新新的事件
                event.events = EPOLLET | left_events;

                // 重新注册事件
                int rt2 = epoll_ctl(m_epoll_fd, op, fd_ctx->fd, &event);
                if (rt2) {
                    LSH_LOG_INFO(g_logger) << "epoll_ctl(" << m_epoll_fd << ","
                                           << op << ", " << fd_ctx->fd << ", " << event.events << ") :"
                                           << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                    continue;
                }

                // 读事件好了，执行读事件
                if (real_events & READ) {
                    fd_ctx->triggerEvent(READ);
                    --m_pendingEventCount;
                }
                // 写事件好了，执行写事件
                if (real_events & WRITE) {
                    fd_ctx->triggerEvent(WRITE);
                    --m_pendingEventCount;
                }
            }

            // 执行完epoll_wait返回的事件
            // 获得当前协程
            Fiber::ptr cur = Fiber::GetThis();
            // 获得裸指针
            auto raw_ptr = cur.get();
            // 将当前idle协程指向空指针，状态为INIT
            cur.reset();

            // 执行完返回scheduler的MainFiber 继续下一轮
            raw_ptr->swapOut();
        }
    }

    void IOManager::onTimerInsertedAtFront() {
        tickle();
    }
}