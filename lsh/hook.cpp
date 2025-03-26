#include "hook.h"
#include "IOManager.h"
#include "config.h"
#include "fd_manager.h"
#include "fiber.h"
#include "log.h"
#include "timer.h"
#include <dlfcn.h>
#include <functional>
#include <stdarg.h>

namespace lsh {
    std::shared_ptr<Logger> g_logger = LSH_LOG_NAME("system");
    static lsh::ConfigVar<int>::ptr g_tcp_connect_timeout =
        lsh::Config::Creat("tcp.connect.timeout", (int)5000, "tcp connect timeout");
    static thread_local bool t_hook_enbale = false;

#define HOOK_FUNC(XX) \
    XX(sleep)         \
    XX(usleep)        \
    XX(nanosleep)     \
    XX(socket)        \
    XX(connect)       \
    XX(accept)        \
    XX(read)          \
    XX(readv)         \
    XX(recv)          \
    XX(recvfrom)      \
    XX(recvmsg)       \
    XX(write)         \
    XX(writev)        \
    XX(send)          \
    XX(sendto)        \
    XX(sendmsg)       \
    XX(close)         \
    XX(fcntl)         \
    XX(ioctl)         \
    XX(getsockopt)    \
    XX(setsockopt)

    void hook_init() {
        static bool is_inited = false;
        if (is_inited) {
            return;
        }

#define XX(name) name##_f = (name##_func)dlsym(RTLD_NEXT, #name);
        HOOK_FUNC(XX); // 要 hook 的系统函数的原来方法
#undef XX
    }

    static uint64_t s_connect_timeout = -1;
    // 实例化静态全局变量，再main()之前调用构造函数
    // 从而再 main() 之前初始化 hook 的函数
    struct _HookIniter {
        _HookIniter() {
            hook_init();
            s_connect_timeout = g_tcp_connect_timeout->getValue();
            g_tcp_connect_timeout->addListener(0xFFFC01, [](const int &old_value, const int &new_value) {
                LSH_LOG_INFO(g_logger) << "tcp connect timeout changed from " << old_value << " to " << new_value;
                s_connect_timeout = new_value;
            });
        }
    };
    static _HookIniter s_hook_initer;

    bool is_hook_enable() {
        return t_hook_enbale;
    }

    void set_hook_enable(bool flag) {
        t_hook_enbale = flag;
    }
}

struct timer_info {
    int cancelled = 0;
};

template <typename OrignName, typename... Args>
static ssize_t do_io(int fd, OrignName func, const char *hook_func_name,
                     uint32_t event, int timeout_so, Args &&...args) {
    if (!lsh::t_hook_enbale) {
        return func(fd, std::forward<Args>(args)...);
    }

    LSH_LOG_DEBUG(lsh::g_logger) << "do_io<" << hook_func_name << "> fd=" << fd;
    lsh::FdCtx::ptr ctx = lsh::fdMgr::GetInstance()->get(fd);
    if (!ctx) {
        return func(fd, std::forward<Args>(args)...);
    }

    if (ctx->isClose()) {
        errno = EBADF;
        return -1;
    }

    if (!ctx->isSocket() || ctx->getUserNonblock()) {
        return func(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    ssize_t n = func(fd, std::forward<Args>(args)...);
    while (n == -1 && errno == EINTR) {
        n = func(fd, std::forward<Args>(args)...);
    }

    if (n == -1 && errno == EAGAIN) {
        LSH_LOG_DEBUG(lsh::g_logger) << "do_io<" << hook_func_name << "> EAGAIN fd=" << fd;
        lsh::IOManager *iom = lsh::IOManager::GetThis();
        lsh::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);

        if (to != (uint64_t)-1) {
            timer = iom->addContionTimer(to, [&]() {
                auto t = winfo.lock();
                if (!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cnacelEvent(fd, (lsh::IOManager::Event)(event)); }, winfo);
        }

        // int c = 0;
        // uint64_t now = 0;

        int rt = iom->addEvent(fd, (lsh::IOManager::Event)(event));
        if (rt) {
            LSH_LOG_ERROR(lsh::g_logger) << hook_func_name << " addEvent(" << fd
                                         << ", " << event << ")";
            if (timer) {
                timer->cancel();
            }
            return -1;
        } else {
            LSH_LOG_DEBUG(lsh::g_logger) << "do_io<" << hook_func_name << "> EAGAIN fd=" << fd;
            lsh::Fiber::YieldToHold();
            LSH_LOG_DEBUG(lsh::g_logger) << "do_io<" << hook_func_name << "> EAGAIN fd=" << fd;
            if (timer) {
                timer->cancel();
            }
            if (tinfo->cancelled) {
                errno = tinfo->cancelled;
                return -1;
            }

            goto retry;
        }
    }
    return n;
}

extern "C" {
#define XX(name) name##_func name##_f = nullptr;
HOOK_FUNC(XX); // sleep_func sleep_f = nullptr; usleep_func usleep_f = nullptr; 初始化函数指针
#undef XX

// hook 系统函数
unsigned int sleep(unsigned int seconds) {
    if (!lsh::t_hook_enbale) {
        return sleep_f(seconds);
    }

    lsh::Fiber::ptr fiber = lsh::Fiber::GetThis();
    lsh::IOManager *iom = lsh::IOManager::GetThis();
    iom->addTimer(seconds * 1000,
                  std::bind((void(lsh::Scheduler::*)(lsh::Fiber::ptr, int thread)) & lsh::IOManager::schedule,
                            iom, fiber, -1));
    // iom->addTimer(seconds * 1000, [&]() {
    //     iom->schedule(fiber);
    // });
    lsh::Fiber::YieldToHold();
    return 0;
}

int usleep(useconds_t usec) {
    if (!lsh::t_hook_enbale) {
        return usleep_f(usec);
    }

    lsh::Fiber::ptr fiber = lsh::Fiber::GetThis();
    lsh::IOManager *iom = lsh::IOManager::GetThis();
    iom->addTimer(usec / 1000,
                  std::bind((void(lsh::Scheduler::*)(lsh::Fiber::ptr, int thread)) & lsh::IOManager::schedule,
                            iom, fiber, -1));
    // iom->addTimer(usec / 1000, [&]() {
    //     iom->schedule(fiber);
    // });
    lsh::Fiber::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!lsh::t_hook_enbale) {
        return nanosleep_f(req, rem);
    }

    int time_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
    lsh::Fiber::ptr fiber = lsh::Fiber::GetThis();
    lsh::IOManager *iom = lsh::IOManager::GetThis();
    iom->addTimer(time_ms,
                  std::bind((void(lsh::Scheduler::*)(lsh::Fiber::ptr, int thread)) & lsh::IOManager::schedule,
                            iom, fiber, -1));
    // iom->addTimer(time_ms, [&]() {
    //     iom->schedule(fiber);
    // });
    lsh::Fiber::YieldToHold();
    return 0;
}

int socket(int domain, int type, int protocol) {
    if (!lsh::t_hook_enbale) {
        return socket_f(domain, type, protocol);
    }

    int fd = socket_f(domain, type, protocol);
    if (fd == -1) {
        return fd;
    }

    lsh::fdMgr::GetInstance()->get(fd, true);
    return fd;
}

int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms) {
    if (!lsh::t_hook_enbale) {
        return connect_f(fd, addr, addrlen);
    }

    lsh::FdCtx::ptr ctx = lsh::fdMgr::GetInstance()->get(fd);
    if (!ctx || ctx->isClose()) {
        errno = EBADF;
        return -1;
    }

    if (!ctx->isSocket()) {
        return connect_f(fd, addr, addrlen);
    }

    if (ctx->getUserNonblock()) {
        return connect_f(fd, addr, addrlen);
    }

    int n = connect_f(fd, addr, addrlen);
    if (n == 0) {
        return 0;
    } else if (n != -1 || errno != EINPROGRESS) {
        return n;
    }

    lsh::IOManager *iom = lsh::IOManager::GetThis();
    lsh::Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);

    std::weak_ptr<timer_info> winfo(tinfo);
    if (timeout_ms != (uint64_t)-1) {
        timer = iom->addContionTimer(timeout_ms, [&]() {
            auto t = winfo.lock();
            if (!t || t->cancelled) {
                return;
            }
            t->cancelled = ETIMEDOUT;
            iom->cnacelEvent(fd, lsh::IOManager::WRITE); }, winfo);
    }

    int rt = iom->addEvent(fd, lsh::IOManager::WRITE);
    if (rt == 0) {
        lsh::Fiber::YieldToHold();
        if (timer) {
            timer->cancel();
        }
        if (tinfo->cancelled) {
            errno = tinfo->cancelled;
            return -1;
        }
    } else {
        if (timer) {
            timer->cancel();
        }
        LSH_LOG_INFO(lsh::g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    }
    int error = 0;
    socklen_t len = sizeof(int);
    if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }
    if (!error) {
        return 0;
    } else {
        errno = error;
        return -1;
    }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, lsh::s_connect_timeout);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    int fd = do_io(sockfd, accept_f, "accept", lsh::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
    if (fd >= 0) {
        lsh::fdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count) {
    return do_io(fd, read_f, "read", lsh::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, readv_f, "readv", lsh::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", lsh::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", lsh::IOManager::READ, SO_RCVTIMEO,
                 buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", lsh::IOManager::READ, SO_RCVTIMEO,
                 msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return do_io(fd, write_f, "write", lsh::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, writev_f, "writev", lsh::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return do_io(sockfd, send_f, "send", lsh::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) {
    return do_io(sockfd, sendto_f, "sendto", lsh::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags,
                 dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    return do_io(sockfd, sendmsg_f, "sendmsg", lsh::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
    if (!lsh::t_hook_enbale) {
        return close_f(fd);
    }

    lsh::FdCtx::ptr ctx = lsh::fdMgr::GetInstance()->get(fd);
    if (ctx) {
        auto iom = lsh::IOManager::GetThis();
        if (iom) {
            iom->cancelAll(fd);
        }
        lsh::fdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */) {
    va_list va;
    va_start(va, cmd);
    switch (cmd) {
    case F_SETFL: {
        int arg = va_arg(va, int);
        va_end(va);
        lsh::FdCtx::ptr ctx = lsh::fdMgr::GetInstance()->get(fd);
        if (!ctx || ctx->isClose() || !ctx->isSocket()) {
            return fcntl_f(fd, cmd, arg);
        }
        ctx->setUserNonblock(arg & O_NONBLOCK);
        if (ctx->getSysNonblock()) {
            arg |= O_NONBLOCK;
        } else {
            arg &= ~O_NONBLOCK;
        }
        return fcntl_f(fd, cmd, arg);
    } break;
    case F_GETFL: {
        va_end(va);
        int arg = fcntl_f(fd, cmd);
        lsh::FdCtx::ptr ctx = lsh::fdMgr::GetInstance()->get(fd);
        if (!ctx || ctx->isClose() || !ctx->isSocket()) {
            return arg;
        }
        if (ctx->getUserNonblock()) {
            return arg | O_NONBLOCK;
        } else {
            return arg & ~O_NONBLOCK;
        }
    } break;
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETOWN:
    case F_SETSIG:
    case F_SETLEASE:
    case F_NOTIFY:
    case F_SETPIPE_SZ: {
        int arg = va_arg(va, int);
        va_end(va);
        return fcntl_f(fd, cmd, arg);
    } break;
    case F_GETFD:
    case F_GETOWN:
    case F_GETSIG:
    case F_GETLEASE:
    case F_GETPIPE_SZ: {
        va_end(va);
        return fcntl_f(fd, cmd);
    } break;

    case F_SETLK:
    case F_SETLKW:
    case F_GETLK: {
        struct flock *arg = va_arg(va, struct flock *);
        va_end(va);
        return fcntl_f(fd, cmd, arg);
    } break;

    case F_GETOWN_EX:
    case F_SETOWN_EX: {
        struct f_owner_ex *arg = va_arg(va, struct f_owner_ex *);
        va_end(va);
        return fcntl_f(fd, cmd, arg);
    } break;

    default:
        va_end(va);
        return fcntl_f(fd, cmd);
        break;
    }
}

int ioctl(int fd, unsigned long request, ...) {
    va_list va;
    va_start(va, request);
    void *arg = va_arg(va, void *);
    va_end(va);

    if (FIONBIO == request) {
        bool user_nonblock = !!*(int *)arg;
        lsh::FdCtx::ptr ctx = lsh::fdMgr::GetInstance()->get(fd);
        if (!ctx || ctx->isClose() || !ctx->isSocket()) {
            return ioctl_f(fd, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(fd, request, arg);
}

int getsockopt(int sockfd, int level, int optname,
               void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname,
               const void *optval, socklen_t optlen) {
    if (!lsh::t_hook_enbale) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }

    if (level == SOL_SOCKET) {
        if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            lsh::FdCtx::ptr ctx = lsh::fdMgr::GetInstance()->get(sockfd);
            if (ctx) {
                const timeval *v = (const timeval *)optval;
                ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}
}