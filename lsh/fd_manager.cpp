#include "fd_manager.h"
#include "hook.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace lsh {
    FdCtx::FdCtx(int fd) {
        m_isInit = false;
        m_isSocket = false;
        m_isClosed = false;
        m_sysNonblock = false;
        m_userNonblock = false;
        m_recvTimeout = -1;
        m_sendTimeout = -1;
        m_fd = fd;

        init();
    }

    FdCtx::~FdCtx() {}

    bool FdCtx::close() {}

    bool FdCtx::init() {
        // 检查是否已经初始化过，如果已经初始化过，直接返回 true
        if (m_isInit) {
            return true;
        }
        // 设置接收超时时间为 -1，表示无超时限制
        m_recvTimeout = -1;
        // 设置发送超时时间为 -1，表示无超时限制
        m_sendTimeout = -1;

        // 定义一个 stat 结构体，用于存储文件描述符的状态信息
        struct stat fd_stat;
        // 使用 fstat 函数获取文件描述符 m_fd 的状态信息
        if (fstat(m_fd, &fd_stat) == -1) {
            // 如果 fstat 调用失败，说明初始化失败
            m_isInit = false;
            // 标记不是套接字
            m_isSocket = false;
        } else {
            // fstat 调用成功，标记初始化成功
            m_isInit = true;
            // 使用 S_ISSOCK 宏判断文件描述符是否为套接字，并更新 m_isSocket 标志
            m_isSocket = S_ISSOCK(fd_stat.st_mode);
        }

        // 如果文件描述符是套接字
        if (m_isSocket) {
            // 使用 fcntl_f 函数获取文件描述符的当前标志
            int flags = fcntl_f(m_fd, F_GETFL, 0);
            // 检查是否已经设置了非阻塞标志
            if (!(flags & O_NONBLOCK)) {
                // 如果没有设置非阻塞标志，使用 fcntl_f 函数设置非阻塞标志
                fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
            }
            // 标记系统层面设置了非阻塞模式
            m_sysNonblock = true;
        } else {
            // 如果不是套接字，标记系统层面未设置非阻塞模式
            m_sysNonblock = false;
        }

        // 标记用户层面未设置非阻塞模式
        m_userNonblock = false;
        // 标记文件描述符未关闭
        m_isClosed = false;
        // 返回初始化是否成功的标志
        return m_isInit;
    }

    void FdCtx::setTimeout(int type, uint64_t v) {
        if (type == SO_RCVTIMEO) {
            m_recvTimeout = v;
        } else {
            m_sendTimeout = v;
        }
    }

    uint64_t FdCtx::getTimeout(int type) {
        if (type == SO_RCVTIMEO) {
            return m_recvTimeout;
        } else {
            return m_sendTimeout;
        }
    }

    FdManager::FdManager() {
        m_datas.resize(64);
    }

    FdCtx::ptr FdManager::get(int fd, bool auto_create) {
        RWMutexType::ReadLock lock(m_mutex);
        if (m_datas.size() <= fd) {
            if (auto_create == false) {
                return nullptr;
            }
        } else {
            if (m_datas[fd] || !auto_create) {
                return m_datas[fd];
            }
        }
        lock.unlock();
        RWMutexType::WriteLock __lock(m_mutex);
        FdCtx::ptr ctx(new FdCtx(fd));
        m_datas[fd] = ctx;
        return ctx;
    }

    void FdManager::del(int fd) {
        RWMutexType::WriteLock __lock(m_mutex);
        if (m_datas.size() <= fd) {
            return;
        }
        m_datas[fd].reset();
    }
}