#include "address.h"
#include "log.h"
#include <map>

std::shared_ptr<lsh::Logger> g_logger = LSH_LOG_ROOT;

void test() {
    LSH_LOG_INFO(g_logger) << "test begin";
    std::vector<lsh::Address::ptr> addrs;
    bool v = lsh::Address::Lookup(addrs, "www.sylar.top");
    if (!v) {
        LSH_LOG_ERROR(g_logger) << "Address::Lookup fail";
        return;
    }
    for (auto &i : addrs) {
        LSH_LOG_INFO(g_logger) << i << " - " << i->toString();
    }
}

void test_iface() {
    LSH_LOG_INFO(g_logger) << "test_iface begin";
    std::multimap<std::string, std::pair<lsh::Address::ptr, uint32_t>> results;
    bool v = lsh::Address::GetInterfaceAddresses(results);
    if (!v) {
        LSH_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses fail";
        return;
    }
    for (auto &i : results) {
        LSH_LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - " << i.second.second;
    }
}

void test_ipv4() {
    LSH_LOG_INFO(g_logger) << "test_ipv4 begin";
    // auto addr = lsh::IPAddress::Create("www.sylar.top");
    auto addr = lsh::IPAddress::Create("127.0.0.8");
    if (addr) {
        LSH_LOG_INFO(g_logger) << "ipv4: " << addr->toString();
    }
}

int main(int argc, char **argv) {
    // test();
    // test_iface();
    test_ipv4();
    return 0;
}