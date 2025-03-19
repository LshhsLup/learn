#include "fiber.h"
#include "log.h"

void run_in_fiber() {
    LSH_LOG_INFO(LSH_LOG_ROOT) << "run in fiber begin";
    lsh::Fiber::GetThis()->YieldToHold();
    LSH_LOG_INFO(LSH_LOG_ROOT) << "run in fiber end";
    lsh::Fiber::GetThis()->YieldToHold();
}

void test_fiber() {
    LSH_LOG_INFO(LSH_LOG_ROOT) << "main begin -1";
    {
        lsh::Fiber::GetThis();
        LSH_LOG_INFO(LSH_LOG_ROOT) << "main begin";
        lsh::Fiber::ptr fiber(new lsh::Fiber(run_in_fiber));
        fiber->swapIn();
        LSH_LOG_INFO(LSH_LOG_ROOT) << "main after swapin";
        fiber->swapIn();
        LSH_LOG_INFO(LSH_LOG_ROOT) << "main end";
        // fiber->swapIn();
    }
    LSH_LOG_INFO(LSH_LOG_ROOT) << "main end2";
}

int main(int argc, char **argv) {
    // lsh::Thread::setName("main");
    // std::vector<lsh::Thread::ptr> thrs;
    // for (int i = 0; i < 3; i++) {
    //     thrs.push_back(lsh::Thread::ptr(new lsh::Thread(&test_fiber, "name_" + std::to_string(i))));
    // }
    // for (auto i : thrs) {
    //     i->join();
    // }
    LSH_LOG_INFO(LSH_LOG_ROOT) << "main begin -1";
    {
        lsh::Fiber::GetThis();
        LSH_LOG_INFO(LSH_LOG_ROOT) << "main begin";
        lsh::Fiber::ptr fiber(new lsh::Fiber(run_in_fiber));
        fiber->swapIn();
        LSH_LOG_INFO(LSH_LOG_ROOT) << "main after swapin";
        fiber->swapIn();
        LSH_LOG_INFO(LSH_LOG_ROOT) << "main end";
        fiber->swapIn();
    }
    LSH_LOG_INFO(LSH_LOG_ROOT) << "main end2";
    return 0;
}