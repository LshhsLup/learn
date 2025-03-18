#include "config.h"
#include <list>

namespace lsh {
    // 静态成员变量 s_datas 需要在 .cpp 文件 中定义
    // Config::ConfigVarMap Config::s_datas;

    ConfigVarBase::ptr Config::LookupBase(const std::string &name) {
        RWMutexType::ReadLock lock(GetMutex());
        auto it = GetDatas().find(name);
        return it == GetDatas().end() ? nullptr : it->second;
    }

    // YAML 文件
    // system:
    //     port: 8080
    //     name: "MyServer"
    //     logs:
    //         level: "DEBUG"
    //         path: "/var/log/server.log"
    //
    // ListAllMember 将嵌套结构转为键值对列表
    // ("system.port", 8080)
    // ("system.name", "MyServer")
    // ("system.ports", {{"level" : "DEBUG"},{"path","/var/log/server.log"}})
    // ("system.logs.level", "DEBUG")
    // ("system.logs.path", "/var/log/server.log")
    static void ListAllMember(const std::string &prefix,
                              const YAML::Node &node,
                              std::list<std::pair<std::string, const YAML::Node>> &output) {
        // 如果 key 含有非法字符，则报错并返回
        if (prefix.find_first_not_of("abcdefghigklmnopqrstuvwxyz._0123456789") != std::string::npos) {
            LSH_LOG_ERROR(LSH_LOG_ROOT) << "Config invalid name: " << prefix << " : " << node;
            return;
        }

        // 将当前 (键名, 对应的 YAML 节点) 存入 output 列表
        output.push_back(std::make_pair(prefix, node));

        // 如果当前节点是一个 Map（即它包含子键值对）
        if (node.IsMap()) {
            for (auto it = node.begin(); it != node.end(); ++it) {
                // 递归调用 ListAllMember，将 key 进行拼接形成 "A.B.C" 形式的键名
                ListAllMember(prefix.empty() ? it->first.Scalar()
                                             : prefix + "." + it->first.Scalar(),
                              it->second, output);
            }
        }
    }

    void Config::LoadFromYaml(const YAML::Node &root) {
        // 存储所有 (键名, YAML 节点)
        std::list<std::pair<std::string, const YAML::Node>> all_nodes;

        // 递归解析 YAML 文件，存入 all_nodes
        // 传入 "" 作为初始前缀，表示从根节点开始，确保最上层的 key 没有前缀
        ListAllMember("", root, all_nodes);
        // for (auto [a, b] : all_nodes) {
        //     std::cout << a << " ";
        // }
        // std::cout << std::endl;
        // std::cout << all_nodes.size() << std::endl;
        // 遍历所有解析出的 (键名, YAML 节点)
        for (auto &i : all_nodes) {
            std::string key = i.first;

            // 如果 key 为空，则跳过
            if (key.empty()) {
                continue;
            }

            // 统一转换 key 为小写，确保大小写不敏感
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            // 查找是否已存在该配置项
            ConfigVarBase::ptr var = LookupBase(key);
            if (var) {
                // 如果是简单值（标量类型），直接解析并赋值
                if (i.second.IsScalar()) {
                    var->fromString(i.second.Scalar());
                } else {
                    // 如果是复杂结构（如列表、Map），转换为字符串再赋值
                    std::stringstream ss;
                    ss << i.second;
                    var->fromString(ss.str());
                }
            }
        }
    }
}