#include "config.h"
#include "log.h"
#include "singleton.h"
#include <vector>
#include <yaml-cpp/yaml.h>

lsh::ConfigVar<int>::ptr g_int_value_config = lsh::Config::Creat("system.port", (int)8000, "system port");
lsh::ConfigVar<int>::ptr g_int_value_config1 = lsh::Config::Creat("system.port", (int)80001, "system port");

lsh::ConfigVar<std::vector<int>>::ptr g_intVector_value_config = lsh::Config::Creat("system.int_vec", std::vector<int>{1, 2}, "system port");

lsh::ConfigVar<std::list<int>>::ptr g_intList_value_config = lsh::Config::Creat("system.int_list", std::list<int>{1, 2}, "system port");

lsh::ConfigVar<std::set<int>>::ptr g_intSet_value_config = lsh::Config::Creat("system.int_set", std::set<int>{1, 2}, "system port");
lsh::ConfigVar<std::unordered_set<int>>::ptr g_intUNSet_value_config = lsh::Config::Creat("system.int_unset", std::unordered_set<int>{1, 2}, "system port");
lsh::ConfigVar<std::map<std::string, int>>::ptr g_intmap_value_config = lsh::Config::Creat("system.int_map", std::map<std::string, int>{{"k", 2}}, "system port");
lsh::ConfigVar<std::unordered_map<std::string, int>>::ptr g_int1map_value_config = lsh::Config::Creat("system.int_umap", std::unordered_map<std::string, int>{{"f", 2}}, "system port");
void print_yaml(const YAML::Node &node, int level) {
    if (node.IsScalar()) {
        LSH_LOG_INFO(LSH_LOG_ROOT) << std::string(level * 4, ' ') << node.Scalar()
                                   << " - " << node.Type() << " - " << level;
    } else if (node.IsNull()) {
        LSH_LOG_INFO(LSH_LOG_ROOT) << std::string(level * 4, ' ')
                                   << "NULL - " << node.Type() << " - " << level;
    } else if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); it++) {
            LSH_LOG_INFO(LSH_LOG_ROOT) << std::string(level * 4, ' ')
                                       << it->first << " - " << it->second.Type() << " - " << level;
            print_yaml(it->second, level + 1);
        }
    } else if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); i++) {
            LSH_LOG_INFO(LSH_LOG_ROOT) << std::string(level * 4, ' ')
                                       << i << " - " << node[i].Type() << " - " << level;
            print_yaml(node[i], level + 1);
        }
    }
}
void test_yaml() {
    // 加载 yml 文件
    YAML::Node root = YAML::LoadFile("/home/lsh/server_framework/bin/conf/log.yml");
    print_yaml(root, 0);
    // LSH_LOG_INFO(LSH_LOG_ROOT) << root;
}

void test_config() {
    LSH_LOG_INFO(LSH_LOG_ROOT) << "before: " << g_int_value_config->getValue();
    // LSH_LOG_INFO(LSH_LOG_ROOT) << "before: " << g_int_value_config1->getValue();
    LSH_LOG_INFO(LSH_LOG_ROOT) << "before: " << g_int_value_config->toString();

#define XX(g_var, name, prefix)                                                         \
    {                                                                                   \
        auto &it = g_var->getValue();                                                   \
        for (auto i : it) {                                                             \
            LSH_LOG_INFO(LSH_LOG_ROOT) << #prefix " " #name ": " << i;                  \
        }                                                                               \
        LSH_LOG_INFO(LSH_LOG_ROOT) << #prefix " " #name " yaml: " << g_var->toString(); \
    }

#define XX_MAP(g_var, name, prefix)                                                     \
    {                                                                                   \
        auto &it = g_var->getValue();                                                   \
        for (auto i : it) {                                                             \
            LSH_LOG_INFO(LSH_LOG_ROOT) << #prefix " " #name ": {" << i.first            \
                                       << " - " << i.second << "}";                     \
        }                                                                               \
        LSH_LOG_INFO(LSH_LOG_ROOT) << #prefix " " #name " yaml: " << g_var->toString(); \
    }

    XX(g_intList_value_config, int_list, before);
    XX(g_intVector_value_config, int_vector, before);
    XX(g_intSet_value_config, int_Set, before);
    XX(g_intUNSet_value_config, int_UNSet, before);
    XX_MAP(g_intmap_value_config, int_map, before);
    XX_MAP(g_int1map_value_config, int_1map, before);
    YAML::Node root = YAML::LoadFile("/home/lsh/server_framework/bin/conf/log.yml");
    lsh::Config::LoadFromYaml(root);
    LSH_LOG_INFO(LSH_LOG_ROOT) << "after: " << g_int_value_config->getValue();
    // LSH_LOG_INFO(LSH_LOG_ROOT) << "after: " << g_int_value_config1->getValue();
    LSH_LOG_INFO(LSH_LOG_ROOT) << "after: " << g_int_value_config->toString();
    XX(g_intList_value_config, int_list, after);
    XX(g_intVector_value_config, int_vector, after);
    XX(g_intSet_value_config, int_vector, after);
    XX(g_intUNSet_value_config, int_UNSet, after);
    XX_MAP(g_intmap_value_config, int_map, after);
    XX_MAP(g_int1map_value_config, int_1map, after);
}

class Person {
public:
    std::string m_name = "";
    int m_age = 0;
    int m_sex = 0;
    std::string toString() const {
        std::stringstream ss;
        ss << "[person name is " << m_name << " , age is " << m_age << " ,sex is " << m_sex << "]";
        return ss.str();
    }

    bool operator==(const Person &p) const {
        return m_name == p.m_name && m_age == p.m_age && m_sex == p.m_sex;
    }
};

namespace lsh {

    template <>
    class LexicalCast<std::string, Person> {
    public:
        Person operator()(const std::string &str) {
            YAML::Node node = YAML::Load(str);
            Person p;
            p.m_name = node["name"].as<std::string>();
            p.m_age = node["age"].as<int>();
            p.m_sex = node["sex"].as<int>();
            return p;
        }
    };

    template <>
    class LexicalCast<Person, std::string> {
    public:
        std::string operator()(const Person &v) {
            YAML::Node node;
            node["name"] = v.m_name;
            node["age"] = v.m_age;
            node["sex"] = v.m_sex;
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };
}
lsh::ConfigVar<Person>::ptr g_person = lsh::Config::Creat("class.person", Person(), "system person");

void test_class() {
    LSH_LOG_INFO(LSH_LOG_ROOT) << "before: " << g_person->getValue().toString() << " - " << g_person->toString();

    g_person->addListener(10, [](const Person &o, const Person &n) {
        LSH_LOG_INFO(LSH_LOG_ROOT) << "oldVlaue= " << o.toString() << " newValue= " << n.toString();
    });

    YAML::Node root = YAML::LoadFile("/home/lsh/server_framework/bin/conf/log.yml");
    lsh::Config::LoadFromYaml(root);

    LSH_LOG_INFO(LSH_LOG_ROOT) << "after: " << g_person->getValue().toString() << " - " << g_person->toString();
}

void test_log() {
    static std::shared_ptr<lsh::Logger> system_log = LSH_LOG_NAME("system");
    LSH_LOG_INFO(system_log) << "hello system" << std::endl;
    std::cout << lsh::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    YAML::Node root = YAML::LoadFile("/home/lsh/server_framework/bin/conf/logs.yml");
    lsh::Config::LoadFromYaml(root);
    std::cout << "===========================================================" << std::endl;
    std::cout << lsh::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    LSH_LOG_INFO(system_log) << "hello system" << std::endl;
    system_log->setFormatter("%d - %m%n");
    LSH_LOG_INFO(system_log) << "hello system" << std::endl;
}

int main(int argc, char **argv) {
    // LSH_LOG_INFO(LSH_LOG_ROOT) << g_int_value_config->getValue();
    // LSH_LOG_INFO(LSH_LOG_ROOT) << g_int_value_config->toString();

    // test_yaml();
    // test_config();
    // test_class();
    test_log();
    return 0;
}