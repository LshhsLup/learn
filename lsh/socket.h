#ifndef __LSH_SOCKET_H__
#define __LSH_SOCKET_H__

#include "address.h"
#include "noncopyable.h"
#include <memory>

namespace lsh {

    class Socket : public std::enable_shared_from_this<Socket>, public Noncopyable {
    public:
        typedef std::shared_ptr<Socket> ptr;
        typedef std::weak_ptr<Socket> weak_ptr;

        Socket(int family, int type, int protocol = 0);
        ~Socket();

        int64_t getSendTimeout();
        void setSendTimeout(int v);

        int64_t getRecvTimeout();
        void setRecvTimeout(int v);

        bool getOption(int level, int option, void *result, size_t *len);
        template <class T>
        bool getOption(int level, int option, T &result) {
            socklen_t length = sizeof(T);
            return getOption(level, option, &result, &length);
        }

        bool setOption(int level, int option, const void *result, size_t len);
        template <class T>
        bool setOption(int level, int option, const T &result) {
            return setOption(level, option, &result, sizeof(T));
        }

        Socket::ptr accept();

        bool bind(const Address::ptr addr);
        bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);
        bool listen(int backlog = SOMAXCONN);
        bool close();

        int send(const void *buffer, size_t length, int flags = 0);
        int send(const iovec *buffers, size_t length, int flags = 0);
        int sendTo(const void *buffer, size_t length, const Address::ptr to, int flags = 0);
        int sendTo(const iovec *buffers, size_t length, const Address::ptr to, int flags = 0);

        int recv(void *buffer, size_t length, int flags = 0);
        int recv(iovec *buffers, size_t length, int flags = 0);
        int recvFrom(void *buffer, size_t length, Address::ptr from, int flags = 0);
        int recvFrom(iovec *buffers, size_t length, Address::ptr from, int flags = 0);

        Address::ptr getRemoteAddress();
        Address::ptr getLocalAddress();

        int getFamily() const { return m_family; }
        int getType() const { return m_type; }
        int getProtocol() const { return m_protocol; }

        bool isConnected() const { return m_isConnected; }
        bool isValid() const;
        int getError();

        std::ostream &dump(std::ostream &os) const;
        int getSocket() const { return m_sock; }

        bool cancelRead();
        bool cancelWrite();
        bool cancelAccept();
        bool cancelAll();

    private:
        void initSock();
        void newSock();
        bool init(int sock);

    private:
        int m_sock;
        int m_family;
        int m_type;
        int m_protocol;
        bool m_isConnected;

        Address::ptr m_localAddress;
        Address::ptr m_remoteAddress;
    };
}

#endif