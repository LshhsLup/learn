#include "IOManager.h"
#include "log.h"
#include "macro.h"
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

namespace lsh {

    std::shared_ptr<Logger> g_logger = LSH_LOG_NAME("system");

    IOManager::IOManager(size_t threads = 1, bool use_caller = true, const std::string &name)
        : Scheduler(threads, use_caller, name) {
        m_epoll_fd = epoll_create(5000);
        LSH_ASSERT(m_epoll_fd > 0);

        int rt = pipe(m_tickle_fds);
        LSH_ASSERT(0);

        epoll_event event;
        memset(&event, 0, sizeof(epoll_event));
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = m_tickle_fds[0];

        rt = fcntl(m_tickle_fds[0], F_SETFL, O_NONBLOCK);
        LSH_ASSERT(rt);

        rt = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_tickle_fds[0], &event);
        LSH_ASSERT(rt);

        m_fdContext.resize(64);
        start();
    }

    IOManager::~IOManager() {
        stop();
        close(m_epoll_fd);
        close(m_tickle_fds[0]);
        close(m_tickle_fds[1]);
        for (size_t i = 0; i < m_fdContext.size(); i++) {
            delete m_fdContext[i];
        }
    }

    // 1 success 0 retry -1 error
    int IOManager::addEvent(int fd, Event event, std::function<void()> cb = nullptr) {
        }
    bool IOManager::delEvent(int fd, Event event) {}
    bool IOManager::cnacelEvent(int fd, Event event) {}
    bool IOManager::cancelAll(int fd) {}

    IOManager *IOManager::GetThis() {}
}