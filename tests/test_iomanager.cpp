#include "IOManager.h"
#include "log.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

std::shared_ptr<lsh::Logger> g_logger = LSH_LOG_ROOT;
int sock = 0;
void test_fiber() {
    LSH_LOG_INFO(g_logger) << " test fiber";

    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "39.156.66.10", &addr.sin_addr.s_addr);

    if (!connect(sock, (const sockaddr *)&addr, sizeof(addr))) {

    } else if (errno == EINPROGRESS) {
        LSH_LOG_INFO(g_logger) << "add event errno=" << errno << strerror(errno);
        lsh::IOManager::GetThis()->addEvent(sock, lsh::IOManager::READ, [] {
            LSH_LOG_INFO(g_logger) << "read call back";
        });
        lsh::IOManager::GetThis()->addEvent(sock, lsh::IOManager::WRITE, [] {
            LSH_LOG_INFO(g_logger) << "write call back";
            // close(sock);
            lsh::IOManager::GetThis()->cnacelEvent(sock, lsh::IOManager::READ);
        });
    } else {
        LSH_LOG_INFO(g_logger) << "else: " << errno << " " << strerror(errno);
    }
}

void test1() {
    lsh::IOManager iom;
    iom.schedule(&test_fiber);
}

lsh::Timer::ptr s_timer;
void test_timer() {
    lsh::IOManager iom(2);
    s_timer = iom.addTimer(1000, []() { 
        static int i = 0;
        LSH_LOG_INFO(g_logger) << "hello timer" <<" i=" << i;
        if(++i == 3){
            // timer->cancel();
            s_timer->reset(2000,truncate);
        } }, true);
}

int main(int argc, char **argv) {
    // test1();
    test_timer();
    return 0;
}