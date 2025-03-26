#include "socket.h"
#include "fd_manager.h"
#include "hook.h"
#include "log.h"
#include "singleton.h"
#include <iomanip>
#include <netinet/tcp.h>
#include <sys/types.h>

namespace lsh {
    static std::shared_ptr<Logger> g_logger = LSH_LOG_NAME("system");

    Socket::Socket(int family, int type, int protocol) {
        m_sock = -1;
        m_family = family;
        m_type = type;
        m_protocol = protocol;
        m_isConnected = false;
    }

    Socket::~Socket() {
        close();
    }

    int64_t Socket::getSendTimeout() {
        FdCtx::ptr ctx = fdMgr::GetInstance()->get(m_sock);
        if (ctx) {
            return ctx->getTimeout(SO_SNDTIMEO);
        }
        return -1;
    }

    void Socket::setSendTimeout(int v) {
        struct timeval tv{v / 1000, v % 1000 * 1000};
        setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
    }

    int64_t Socket::getRecvTimeout() {
        FdCtx::ptr ctx = fdMgr::GetInstance()->get(m_sock);
        if (ctx) {
            return ctx->getTimeout(SO_RCVTIMEO);
        }
        return -1;
    }

    void Socket::setRecvTimeout(int v) {
        struct timeval tv{v / 1000, v % 1000 * 1000};
        setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
    }

    bool Socket::getOption(int level, int option, void *result, size_t *len) {
        int rt = getsockopt(m_sock, level, option, result, (socklen_t *)len);
        if (rt) {
            LSH_LOG_ERROR(g_logger) << "getOption error: " << rt
                                    << " errno: " << errno << " strerr: "
                                    << strerror(errno);
            return false;
        }
        return true;
    }

    bool Socket::setOption(int level, int option, const void *result, size_t len) {
        int rt = setsockopt(m_sock, level, option, result, len);
        if (rt) {
            LSH_LOG_ERROR(g_logger) << "setOption error: " << rt
                                    << " errno: " << errno << " strerr: "
                                    << strerror(errno);
            return false;
        }
        return true;
    }

    Socket::ptr Socket::accept() {
        Socket::ptr sock(new Socket(m_family, m_type, m_protocol));
        int newsock = ::accept(m_sock, nullptr, nullptr);
        if (newsock == -1) {
            LSH_LOG_ERROR(g_logger) << "accept(" << m_sock << ") errno="
                                    << errno << " errstr=" << strerror(errno);
            return nullptr;
        }
        if (sock->init(newsock)) {
            return sock;
        }
        return nullptr;
    }

    bool Socket::init(int sock) {
        // 判断文件描述符是否有效
        FdCtx::ptr ctx = fdMgr::GetInstance()->get(sock, false);
        if (ctx && ctx->isSocket() && !ctx->isClose()) {
            m_sock = sock;
            m_isConnected = true;
            initSock();
            getLocalAddress();
            getRemoteAddress();
            return true;
        }
        return false;
    }

    bool Socket::bind(const Address::ptr addr) {
        if (!isValid()) {
            newSock();
            if (!isValid()) [[unlikely]] {
                return false;
            }
        }
        if (addr->getFamily() != m_family) [[unlikely]] {
            LSH_LOG_ERROR(g_logger) << "bind address family("
                                    << addr->getFamily() << ") not equal with socket family("
                                    << m_family << ")";
            return false;
        }
        if (::bind(m_sock, addr->getAddr(), addr->getAddrLen())) {
            LSH_LOG_ERROR(g_logger) << "bind error, errno=" << errno
                                    << " errstr=" << strerror(errno);
            return false;
        }
        getLocalAddress();
        return true;
    }

    bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {
        if (!isValid()) {
            newSock();
            if (!isValid()) [[unlikely]] {
                return false;
            }
        }
        if (addr->getFamily() != m_family) [[unlikely]] {
            LSH_LOG_ERROR(g_logger) << "connect address family("
                                    << addr->getFamily() << ") not equal with socket family("
                                    << m_family << ")";
            return false;
        }
        if (timeout_ms == (uint64_t)-1) {
            if (::connect(m_sock, addr->getAddr(), addr->getAddrLen())) {
                LSH_LOG_ERROR(g_logger) << "connect(" << addr->toString() << ") error errno="
                                        << errno << " errstr=" << strerror(errno);
                close();
                return false;
            }
        } else {
            if (::connect_with_timeout(m_sock, addr->getAddr(), addr->getAddrLen(), timeout_ms)) {
                LSH_LOG_ERROR(g_logger) << "connect(" << addr->toString() << ") error errno="
                                        << errno << " errstr=" << strerror(errno);
                close();
                return false;
            }
        }
        m_isConnected = true;
        getLocalAddress();
        getRemoteAddress();
        return true;
    }

    bool Socket::listen(int backlog) {
        if (!isValid()) {
            LSH_LOG_ERROR(g_logger) << "listen error: sock=-1";
            return false;
        }
        if (::listen(m_sock, backlog)) {
            LSH_LOG_ERROR(g_logger) << "listen error, errno=" << errno
                                    << " errstr=" << strerror(errno);
            return false;
        }
        return true;
    }

    bool Socket::close() {
        if (!m_isConnected && m_sock == -1) {
            return true;
        }
        m_isConnected = false;
        if (m_sock != -1) {
            ::close(m_sock);
            m_sock = -1;
        }
        return true;
    }

    int Socket::send(const void *buffer, size_t length, int flags) {
        if (isConnected()) {
            return ::send(m_sock, buffer, length, flags);
        }
        return -1;
    }

    int Socket::send(const iovec *buffers, size_t length, int flags) {
        if (isConnected()) {
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec *)buffers;
            msg.msg_iovlen = length;
            return sendmsg(m_sock, &msg, flags);
        }
        return -1;
    }

    int Socket::sendTo(const void *buffer, size_t length, const Address::ptr to, int flags) {
        if (isConnected()) {
            return ::sendto(m_sock, buffer, length, flags, to->getAddr(), to->getAddrLen());
        }
        return -1;
    }

    int Socket::sendTo(const iovec *buffers, size_t length, const Address::ptr to, int flags) {
        if (isConnected()) {
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec *)buffers;
            msg.msg_iovlen = length;
            msg.msg_name = (sockaddr *)to->getAddr();
            msg.msg_namelen = to->getAddrLen();
            return sendmsg(m_sock, &msg, flags);
        }
        return -1;
    }

    int Socket::recv(void *buffer, size_t length, int flags) {
        if (isConnected()) {
            return ::recv(m_sock, buffer, length, flags);
        }
        return -1;
    }

    int Socket::recv(iovec *buffers, size_t length, int flags) {
        if (isConnected()) {
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec *)buffers;
            msg.msg_iovlen = length;
            return recvmsg(m_sock, &msg, flags);
        }
        return -1;
    }

    int Socket::recvFrom(void *buffer, size_t length, Address::ptr from, int flags) {
        if (isConnected()) {
            socklen_t len = from->getAddrLen();
            return ::recvfrom(m_sock, buffer, length, flags, (sockaddr *)from->getAddr(), &len);
        }
        return -1;
    }

    int Socket::recvFrom(iovec *buffers, size_t length, Address::ptr from, int flags) {
        if (isConnected()) {
            msghdr msg;
            memset(&msg, 0, sizeof(msg));
            msg.msg_iov = (iovec *)buffers;
            msg.msg_iovlen = length;
            msg.msg_name = (sockaddr *)from->getAddr();
            msg.msg_namelen = from->getAddrLen();
            return recvmsg(m_sock, &msg, flags);
        }
        return -1;
    }

    Address::ptr Socket::getRemoteAddress() {
        if (m_remoteAddress) {
            return m_remoteAddress;
        }
        Address::ptr addr;
        switch (m_family) {
        case AF_INET:
            addr.reset(new IPv4Address());
            break;
        case AF_INET6:
            addr.reset(new IPv6Address());
            break;
        case AF_UNIX:
            addr.reset(new UnixAddress());
            break;
        default:
            addr.reset(new UnknownAddress(m_family));
            break;
        }
        socklen_t addrlen = addr->getAddrLen();
        if (getpeername(m_sock, (sockaddr *)addr->getAddr(), &addrlen)) {
            LSH_LOG_ERROR(g_logger) << "getpeername error sock=" << m_sock
                                    << " errno=" << errno << " errstr=" << strerror(errno);
            return Address::ptr(new UnknownAddress(m_family));
        }
        if (m_family == AF_UNIX) {
            UnixAddress::ptr addr_unix = std::dynamic_pointer_cast<UnixAddress>(addr);
            addr_unix->setAddrLen(addrlen);
        }
        m_remoteAddress = addr;
        return m_remoteAddress;
    }

    Address::ptr Socket::getLocalAddress() {
        if (m_localAddress) {
            return m_localAddress;
        }
        Address::ptr addr;
        switch (m_family) {
        case AF_INET:
            addr.reset(new IPv4Address());
            break;
        case AF_INET6:
            addr.reset(new IPv6Address());
            break;
        case AF_UNIX:
            addr.reset(new UnixAddress());
            break;
        default:
            addr.reset(new UnknownAddress(m_family));
            break;
        }
        socklen_t addrlen = addr->getAddrLen();
        if (getsockname(m_sock, (sockaddr *)addr->getAddr(), &addrlen)) {
            LSH_LOG_ERROR(g_logger) << "getsockname error sock=" << m_sock
                                    << " errno=" << errno << " errstr=" << strerror(errno);
            return Address::ptr(new UnknownAddress(m_family));
        }
        if (m_family == AF_UNIX) {
            UnixAddress::ptr addr_unix = std::dynamic_pointer_cast<UnixAddress>(addr);
            addr_unix->setAddrLen(addrlen);
        }
        m_localAddress = addr;
        return m_localAddress;
    }

    bool Socket::isValid() const {
        return m_sock != -1;
    }

    int Socket::getError() {
        int error = 0;
        size_t len = sizeof(error);
        if (!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
            return -1;
        }
        return error;
    }

    std::ostream &Socket::dump(std::ostream &os) const {
        os << "[Socket sock=" << m_sock << " is_connected=" << m_isConnected
           << " family=" << m_family << " type=" << m_type
           << " protocol=" << m_protocol;
        if (m_localAddress) {
            os << " local_address=" << m_localAddress->toString();
        }
        if (m_remoteAddress) {
            os << " remote_address=" << m_remoteAddress->toString();
        }
        os << "]";
        return os;
    }

    bool Socket::cancelRead() {
        return IOManager::GetThis()->cnacelEvent(m_sock, IOManager::READ);
    }

    bool Socket::cancelWrite() {
        return IOManager::GetThis()->cnacelEvent(m_sock, IOManager::WRITE);
    }

    bool Socket::cancelAccept() {
        return IOManager::GetThis()->cnacelEvent(m_sock, IOManager::READ);
    }

    bool Socket::cancelAll() {
        return IOManager::GetThis()->cancelAll(m_sock);
    }

    void Socket::initSock() {
        int val = 1;
        setOption(SOL_SOCKET, SO_REUSEADDR, val);
        if (m_type == SOCK_STREAM) {
            setOption(IPPROTO_TCP, TCP_NODELAY, val);
        }
    }

    void Socket::newSock() {
        m_sock = socket(m_family, m_type, m_protocol);
        bool condition = m_sock != -1;
        if ((condition)) [[likely]] {
            initSock();
        } else {
            LSH_LOG_ERROR(g_logger) << "socket(" << m_family << ", " << m_type
                                    << ", " << m_protocol << ") errno=" << errno
                                    << " errstr=" << strerror(errno);
        }
    }
}