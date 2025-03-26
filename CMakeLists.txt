# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(lshServer)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 在 C++ 标准设置后添加
set(CMAKE_CXX_FLAGS_DEBUG "-g")

#set(CMAKE_VERBOSE_MAKEFILE ON)

# 添加可执行文件搜索路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# 添加库文件搜索路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# 查找所有的源文件
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/lsh/*.cpp")

# 查找所有的头文件
file(GLOB_RECURSE HEADERS "${PROJECT_SOURCE_DIR}/lsh/*.h")

# 添加头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/lsh)

# 添加 yaml 头文件搜索路径
include_directories(/home/lsh/yaml/yaml-cpp/include)

# 添加库路径
link_directories(/home/lsh/yaml/yaml-cpp/build)

# 生成共享库
add_library(lsh SHARED ${SOURCES})


# 添加可执行文件
add_executable(test tests/log/test.cpp)
add_executable(test_config tests/log/test_config.cpp)
add_executable(test_thread tests/test_thread.cpp)
add_executable(test_util tests/test_util.cpp)
add_executable(test_fiber tests/test_fiber.cpp)
add_executable(test_scheduler tests/test_scheduler.cpp)
add_executable(test_iomanager tests/test_iomanager.cpp)
add_executable(test_hook tests/test_hook.cpp)
add_executable(test_address tests/test_address.cpp)
# 添加依赖关系
add_dependencies(test lsh)
add_dependencies(test_config lsh)
add_dependencies(test_thread lsh)
add_dependencies(test_util lsh)
add_dependencies(test_fiber lsh)
add_dependencies(test_scheduler lsh)
add_dependencies(test_iomanager lsh)
add_dependencies(test_hook lsh)
add_dependencies(test_address lsh)

# 让 test 依赖 lsh yaml-cpp 并正确链接
target_link_libraries(test  lsh yaml-cpp)
target_link_libraries(test_config  lsh yaml-cpp)
target_link_libraries(test_thread lsh yaml-cpp)
target_link_libraries(test_util lsh yaml-cpp)
target_link_libraries(test_fiber lsh yaml-cpp)
target_link_libraries(test_scheduler lsh yaml-cpp)
target_link_libraries(test_iomanager lsh yaml-cpp)
target_link_libraries(test_hook lsh yaml-cpp)
target_link_libraries(test_address lsh yaml-cpp)

