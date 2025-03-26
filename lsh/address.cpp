#include "address.h"
#include "endian.h"
#include "log.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sstream>

namespace lsh {

    static std::shared_ptr<Logger> g_logger = LSH_LOG_NAME("system");

    template <class T>
    // 低 bits 位为 1，其余位为 0
    static T CreateMask(uint32_t bits) {
        return (1 << (sizeof(T) * 8 - bits)) - 1;
    }

    //=======================================================================
    //============================ Address ==================================
    //=======================================================================
    Address::ptr Address::Create(const sockaddr *addr, socklen_t addrlen) {
        if (addr == nullptr) {
            return nullptr;
        }

        Address::ptr result;
        switch (addr->sa_family) {
        case AF_INET:
            result.reset(new IPv4Address(*(const sockaddr_in *)addr));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(*(const sockaddr_in6 *)addr));
            break;
        default:
            result.reset(new UnknownAddress(addr->sa_family));
            break;
        }
        return result;
    }

    // 将 host 转换为 Address::ptr 类型的地址，并存储到 result 中
    bool Address::Lookup(std::vector<Address::ptr> &result, const std::string &host,
                         int family, int type, int protocol) {
        addrinfo hints, *results, *next;
        // 初始化 hints
        hints.ai_flags = 0;
        hints.ai_family = family;
        hints.ai_socktype = type;
        hints.ai_protocol = protocol;
        hints.ai_addrlen = 0;
        hints.ai_canonname = nullptr;
        hints.ai_addr = nullptr;
        hints.ai_next = nullptr;

        std::string node;
        const char *service = nullptr;
        // 检查 ipv6 地址 和 service
        if (!host.empty() && host[0] == '[') {
            const char *endipv6 = (const char *)memchr(host.c_str() + 1, ']', host.size() - 1);
            if (endipv6) {
                // TODO: check out of range
                if (*(endipv6 + 1) == ':') {
                    service = endipv6 + 2;
                }
                node = host.substr(1, endipv6 - host.c_str() - 1);
            }
        }

        // 检查 node 和 service
        if (node.empty()) {
            service = (const char *)memchr(host.c_str(), ':', host.size());
            if (service) {
                if (!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                    node = host.substr(0, service - host.c_str());
                    service++;
                }
            }
        }

        if (node.empty()) {
            node = host;
        }

        int error = getaddrinfo(node.c_str(), service, &hints, &results);
        if (error) {
            LSH_LOG_ERROR(g_logger) << "Address::Lookup getaddrinfo(" << host << ", "
                                    << family << ", " << type << ") error=" << error
                                    << " errstr=" << strerror(error);
            return false;
        }

        next = results;
        while (next) {
            result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
            LSH_LOG_INFO(g_logger) << ((sockaddr_in *)next->ai_addr)->sin_addr.s_addr;
            next = next->ai_next;
        }

        freeaddrinfo(results);
        return true;
    }

    Address::ptr Address::LookupAny(const std::string &host,
                                    int family, int type, int protocol) {
        std::vector<Address::ptr> result;
        if (Lookup(result, host, family, type, protocol)) {
            return result[0];
        }
        return nullptr;
    }

    IPAddress::ptr Address::LookupAnyIPAddress(const std::string &host,
                                               int family, int type, int protocol) {
        std::vector<Address::ptr> result;
        if (Lookup(result, host, family, type, protocol)) {
            for (auto &i : result) {
                IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
                return v;
            }
        }
        return nullptr;
    }

    template <class T>
    static uint32_t CountBytes(T value) {
        uint32_t result = 0;
        while (value) {
            value &= value - 1;
            ++result;
        }
        return result;
    }

    bool Address::GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>> &result,
                                        int family) {
        struct ifaddrs *nexts, *results;
        if (getifaddrs(&results) != 0) {
            LSH_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses getifaddrs error"
                                    << " errno=" << errno << " errstr=" << strerror(errno);
            return false;
        }

        try {
            for (nexts = results; nexts; nexts = nexts->ifa_next) {
                Address::ptr addr;
                uint32_t prefix_len = ~0u;
                if (family != AF_UNSPEC && family != nexts->ifa_addr->sa_family) {
                    continue;
                }
                switch (nexts->ifa_addr->sa_family) {
                case AF_INET: {
                    addr = Create(nexts->ifa_addr, sizeof(sockaddr_in));
                    uint32_t netmask = ((sockaddr_in *)nexts->ifa_netmask)->sin_addr.s_addr;
                    prefix_len = CountBytes(netmask);
                } break;
                case AF_INET6: {
                    addr = Create(nexts->ifa_addr, sizeof(sockaddr_in6));
                    in6_addr &netmask_6 = ((sockaddr_in6 *)nexts->ifa_netmask)->sin6_addr;
                    prefix_len = 0;
                    for (int i = 0; i < 16; ++i) {
                        prefix_len += CountBytes(netmask_6.s6_addr[i]);
                    }
                } break;
                default:
                    break;
                }

                if (addr) {
                    result.insert(std::make_pair(nexts->ifa_name, std::make_pair(addr, prefix_len)));
                }
            }
        } catch (...) {
            LSH_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses exception";
            freeifaddrs(results);
            return false;
        }
        freeifaddrs(results);
        return true;
    }

    bool Address::GetInterfaceAddresses_(std::vector<std::pair<Address::ptr, uint32_t>> &result,
                                         const std::string &iface, int family) {
        if (iface.empty() || iface == "*") {
            if (family == AF_INET || family == AF_UNSPEC) {
                result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
            } else if (family == AF_INET6 || family == AF_UNSPEC) {
                result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
            }
            return true;
        }
        std::multimap<std::string, std::pair<Address::ptr, uint32_t>> results;
        if (!GetInterfaceAddresses(results, family)) {
            return false;
        }

        auto its = results.equal_range(iface);
        for (; its.first != its.second; ++its.first) {
            result.push_back(its.first->second);
        }
        return true;
    }

    int Address::getFamily() const {
        return getAddr()->sa_family;
    }

    std::string Address::toString() const {
        std::stringstream ss;
        this->insert(ss);
        return ss.str();
    }

    bool Address::operator<(const Address &rhs) const {
        socklen_t min_len = std::min(this->getAddrLen(), rhs.getAddrLen());
        int cmp = memcmp(this->getAddr(), rhs.getAddr(), min_len);
        if (cmp < 0) {
            return true;
        } else if (cmp > 0) {
            return false;
        } else {
            return this->getAddrLen() < rhs.getAddrLen();
        }
        return false;
    }

    bool Address::operator==(const Address &rhs) const {
        return this->getAddrLen() == rhs.getAddrLen() &&
               memcmp(this->getAddr(), rhs.getAddr(), this->getAddrLen()) == 0;
    }

    bool Address::operator!=(const Address &rhs) const {
        return !(*this == rhs);
    }

    //=========================================================================
    //============================ IPAddress ==================================
    //=========================================================================
    IPAddress::ptr IPAddress::Create(const char *address, uint16_t port) {
        addrinfo hints, *results;
        memset(&hints, 0, sizeof(hints));
        // hints.ai_flags = AI_NUMERICHOST;
        hints.ai_family = AF_UNSPEC;

        int error = getaddrinfo(address, nullptr, &hints, &results);
        if (error) {
            LSH_LOG_INFO(g_logger) << "IPAddress::Create(" << address << ", "
                                   << port << ") error=" << error
                                   << " errstr=" << strerror(error);
            return nullptr;
        }

        try {
            IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(Address::Create(results->ai_addr,
                                                                                         (socklen_t)results->ai_addrlen));
            if (result) {
                result->setPort(port);
            }
            freeaddrinfo(results);
            return result;
        } catch (...) {
            freeaddrinfo(results);
            return nullptr;
        }
    }
    //=========================================================================
    //============================ IPv4Address ================================
    //=========================================================================
    IPv4Address::ptr IPv4Address::Create(const char *address, uint16_t port) {
        IPv4Address::ptr addr = std::make_shared<IPv4Address>();
        addr->m_addr.sin_port = byteswapOnLittleEndian(port);
        if (inet_pton(AF_INET, address, &addr->m_addr.sin_addr) <= 0) {
            LSH_LOG_ERROR(g_logger) << "IPv4Address::Create(" << address << ", "
                                    << port << ") inet_pton error" << " errno="
                                    << errno << " errstr=" << strerror(errno);
            return nullptr;
        }
        return addr;
    }

    IPv4Address::IPv4Address(const sockaddr_in &address) {
        m_addr = address;
    }

    IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
        memset(&m_addr, 0, sizeof(m_addr)); // Clear the memory
        m_addr.sin_family = AF_INET;        // Address Family Internet ipv4
        /*
         * byteswapOnLittleEndian() 函数用于在小端序机器上将 value 转换为大端序
         * 网络中默认使用大端序，所以需要将小端序的 port 和 address 转换为大端序
         * byteswapOnLittleEndian() 可以在大端序机器上不改变 value 的值
         * 在小端序机器上将 value 转换为大端序
         * 那么调用 byteswapOnLittleEndian() 函数后，无论系统是大端序还是小端序，
         * 都可以正确地将 port 和 address 转换为大端序
         */
        m_addr.sin_port = byteswapOnLittleEndian(port);
        m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
    }

    const sockaddr *IPv4Address::getAddr() const {
        return (const sockaddr *)&m_addr;
    }

    socklen_t IPv4Address::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream &IPv4Address::insert(std::ostream &os) const {
        // 将 m_addr.sin_addr.s_addr 转换为大端序
        uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
        // 将 addr 的每个字节转换为点分十进制，并输出到 os 中
        os << ((addr >> 24) & 0xff) << "."
           << ((addr >> 16) & 0xff) << "."
           << ((addr >> 8) & 0xff) << "."
           << (addr & 0xff);
        os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
        return os;
    }

    IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
        // 得到广播地址
        // 广播地址 = ipv4地址 | ~子网掩码
        // prefix_len 为子网掩码长度
        // 子网掩码为前 prefix_len 位为 1，后 32 - prefix_len 位为 0
        // 子网掩码 = ~(1 << (32 - prefix_len) - 1)
        if (prefix_len > 32) {
            return nullptr;
        }
        sockaddr_in baddr(m_addr);
        baddr.sin_addr.s_addr |= byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
        return std::make_shared<IPv4Address>(baddr);
    }

    IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
        // 网段地址 = ipv4地址 & 子网掩码
        if (prefix_len > 32) {
            return nullptr;
        }
        sockaddr_in baddr(m_addr);
        baddr.sin_addr.s_addr &= ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
        return std::make_shared<IPv4Address>(baddr);
    }

    IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
        // 子网掩码 = ~((1 << (32 - prefix_len)) - 1)
        sockaddr_in subnet;
        memset(&subnet, 0, sizeof(subnet));
        subnet.sin_family = AF_INET;
        subnet.sin_addr.s_addr = ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
        return std::make_shared<IPv4Address>(subnet);
    }

    uint32_t IPv4Address::getPort() const {
        return byteswapOnLittleEndian(m_addr.sin_port);
    }

    void IPv4Address::setPort(uint32_t v) {
        m_addr.sin_port = byteswapOnLittleEndian(v);
    }

    //=========================================================================
    //============================ IPv6Address ================================
    //=========================================================================
    IPv6Address::IPv6Address(const sockaddr_in6 &address) {
        m_addr = address;
    }

    IPv6Address::IPv6Address() {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = AF_INET6;
        // m_addr.sin6_port = byteswapOnLittleEndian(0);
    }

    IPv6Address::ptr IPv6Address::Create(const char *address, uint16_t port) {
        IPv6Address::ptr addr = std::make_shared<IPv6Address>();
        addr->m_addr.sin6_port = byteswapOnLittleEndian(port);
        if (inet_pton(AF_INET6, address, &addr->m_addr.sin6_addr) <= 0) {
            LSH_LOG_ERROR(g_logger) << "IPv6Address::Create(" << address << ", "
                                    << port << ") inet_pton error" << " errno="
                                    << errno << " errstr=" << strerror(errno);
            return nullptr;
        }
        return addr;
    }

    IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = AF_INET6;
        m_addr.sin6_port = byteswapOnLittleEndian(port);
        memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
    }

    const sockaddr *IPv6Address::getAddr() const {
        return (const sockaddr *)&m_addr;
    }

    socklen_t IPv6Address::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream &IPv6Address::insert(std::ostream &os) const {
        // 格式化输出 IPv6 地址
        // 例如：[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:80
        os << "[";
        uint16_t *addr = (uint16_t *)m_addr.sin6_addr.s6_addr;
        bool used_zero = false;
        for (int i = 0; i < 8; i++) {
            if (addr[i] == 0 && !used_zero) {
                continue;
            }
            if (i && addr[i - 1] == 0 && !used_zero) {
                os << ":";
                used_zero = true;
            }
            if (i) {
                os << ":";
            }
            os << std::hex << (int)byteswapOnLittleEndian(addr[i]) << std::dec;
        }
        if (!used_zero && addr[7] == 0) {
            os << "::";
        }
        os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
        return os;
    }

    IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
        sockaddr_in6 baddr(m_addr);
        baddr.sin6_addr.s6_addr[prefix_len / 8] |=
            CreateMask<uint8_t>(prefix_len % 8);
        for (int i = prefix_len / 8 + 1; i < 16; i++) {
            baddr.sin6_addr.s6_addr[i] = 0xff;
        }
        return std::make_shared<IPv6Address>(baddr);
    }

    IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
        sockaddr_in6 baddr(m_addr);
        baddr.sin6_addr.s6_addr[prefix_len / 8] &=
            ~CreateMask<uint8_t>(prefix_len % 8);
        return std::make_shared<IPv6Address>(baddr);
    }

    IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
        sockaddr_in6 subnet;
        memset(&subnet, 0, sizeof(subnet));
        subnet.sin6_family = AF_INET6;
        subnet.sin6_addr.s6_addr[prefix_len / 8] =
            ~CreateMask<uint8_t>(prefix_len % 8);
        for (int i = 0; i < prefix_len / 8; i++) {
            subnet.sin6_addr.s6_addr[i] = 0xff;
        }
        return std::make_shared<IPv6Address>(subnet);
    }

    uint32_t IPv6Address::getPort() const {
        return byteswapOnLittleEndian(m_addr.sin6_port);
    }

    void IPv6Address::setPort(uint32_t v) {
        m_addr.sin6_port = byteswapOnLittleEndian(v);
    }

    //=========================================================================
    //============================ UnixAddress ================================
    //=========================================================================
    static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un *)0)->sun_path) - 1;
    UnixAddress::UnixAddress() {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sun_family = AF_UNIX;
        m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
    }

    UnixAddress::UnixAddress(const std::string &path) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sun_family = AF_UNIX;
        if (path.size() > MAX_PATH_LEN) {
            throw std::invalid_argument("path too long");
        }
        m_length = path.size() + 1;
        if (!path.empty() && path[0] == '\0') {
            m_length--;
        }
        memcpy(m_addr.sun_path, path.c_str(), m_length);
        m_length += offsetof(sockaddr_un, sun_path);
    }

    const sockaddr *UnixAddress::getAddr() const {
        return (const sockaddr *)&m_addr;
    }

    socklen_t UnixAddress::getAddrLen() const {
        return m_length;
    }

    std::ostream &UnixAddress::insert(std::ostream &os) const {
        if (m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
            return os << "\\0"
                      << std::string(m_addr.sun_path + 1,
                                     m_length - offsetof(sockaddr_un, sun_path) - 1);
        }
        return os << m_addr.sun_path;
    }

    //=========================================================================
    //============================ UnknownAddress =============================
    //=========================================================================
    UnknownAddress::UnknownAddress(int family) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sa_family = family;
    }

    const sockaddr *UnknownAddress::getAddr() const {
        return (const sockaddr *)&m_addr;
    }

    socklen_t UnknownAddress::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream &UnknownAddress::insert(std::ostream &os) const {
        os << "[UnknownAddress family=" << m_addr.sa_family << "]";
        return os;
    }

}
