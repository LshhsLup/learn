#include "config.h"
#include "log.h"
#include "thread.h"
#include <yaml-cpp/yaml.h>

std::shared_ptr<lsh::Logger> g_logger = LSH_LOG_ROOT;

int count = 0;
lsh::RWMutex s_mutex;
lsh::Mutex mutex;

void func1() {
    LSH_LOG_INFO(g_logger) << "name: " << lsh::Thread::GetName()
                           << " this.name: " << lsh::Thread::getThis()->getName()
                           << " id: " << lsh::GetThreadId()
                           << " this.id: " << lsh::Thread::getThis()->getId();
    for (int i = 0; i < 100000; i++) {
        lsh::Mutex::Lock lock(mutex);
        ++count;
    }
}

void func2() {
    while (true) {
        LSH_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    }
}

void func3() {
    while (true) {
        LSH_LOG_INFO(g_logger) << "===========================================================";
    }
}

int main(int argc, char **argv) {
    LSH_LOG_INFO(g_logger) << "thread test begin";
    YAML::Node root = YAML::LoadFile("/home/lsh/server_framework/bin/conf/log2.yml");
    lsh::Config::LoadFromYaml(root);
    std::vector<lsh::Thread::ptr> threads;
    for (int i = 0; i < 1000; i++) {
        lsh::Thread::ptr t = std::make_shared<lsh::Thread>(&func3, "name_" + std::to_string(i * 2));
        lsh::Thread::ptr t1 = std::make_shared<lsh::Thread>(&func2, "name_" + std::to_string(i * 2 + 1));
        threads.push_back(t);
        threads.push_back(t1);
    }
    for (size_t i = 0; i < threads.size(); i++) {
        threads[i]->join();
    }
    LSH_LOG_INFO(g_logger) << "thread test end";
    LSH_LOG_INFO(g_logger) << "count= " << count;
    return 0;
}