#include "log.h"
#include "macro.h"
#include "util.h"
#include <assert.h>

std::shared_ptr<lsh::Logger> g_logger = LSH_LOG_ROOT;

void test_aasert() {
    LSH_LOG_INFO(g_logger) << lsh::BacktraceToString(10);

    LSH_ASSERT(1);
    LSH_ASSERT_MSG(1, "hahhahah");
    LSH_ASSERT_MSG(false, "xxxxxx");
}

int main(int argc, char **argv) {
    test_aasert();
    return 0;
}