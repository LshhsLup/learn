#include "log.h"
#include "scheduler.h"

std::shared_ptr<lsh::Logger> g_logger = LSH_LOG_ROOT;

void test_fiber() {
    // static int count = 5;
    LSH_LOG_INFO(g_logger) << "test in fiber count=";

    // sleep(1);
    // if (--count >= 0) {
    //     lsh::Scheduler::GetThis()->schedule(&test_fiber, lsh::GetThreadId());
    // }
}

int main(int argc, char **argv) {
    lsh::Scheduler sc(1, false, "test");
    sc.start();
    sc.schedule(&test_fiber);
    sc.stop();
    LSH_LOG_INFO(g_logger) << "over";
    return 0;
}