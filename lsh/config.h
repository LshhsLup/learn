#ifndef __LSH_CONFIG_H__
#define __LSH_CONFIG_H__

#include "log.h"
#include "singleton.h"
#include "thread.h"
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <yaml-cpp/yaml.h> // yaml

// 配置模块的原则：约定优于配置
// 例如约定配置：lsh::ConfigVar<int>::ptr g_int_value_config
//  = lsh::Config::Creat("system.port", (int)8000, "system port");
// 通过 yaml 文件加载配置，在解析过程中，查找约定过的配置进行覆盖

namespace lsh {

    // 配置基本信息
    class ConfigVarBase {
    public:
        typedef std::shared_ptr<ConfigVarBase> ptr;
        ConfigVarBase(const std::string &name, const std::string &description = "")
            : m_name(name), m_description(description) {
            // 转换为小写
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
        }

        virtual ~ConfigVarBase() {}

        const std::string &getName() const { return m_name; }
        const std::string &getDescription() const { return m_description; }

        virtual std::string toString() = 0;                  // 序列化
        virtual bool fromString(const std::string &str) = 0; // 反序列化
        virtual std::string getTypeName() const = 0;

    protected:
        std::string m_name;        // 配置变量的名称
        std::string m_description; // 配置变量的描述信息
    };

    // FromType -> ToType
    // 基本类型转换
    template <class F, class T>
    class LexicalCast {
    public:
        T operator()(const F &v) {
            // 仅支持基本类型，不支持容器
            return boost::lexical_cast<T>(v);
        }
    };

    // 对 vector 进行偏特化，支持 string -> vector
    template <class T>
    class LexicalCast<std::string, std::vector<T>> {
    public:
        std::vector<T> operator()(const std::string &str) {
            std::vector<T> vec;
            YAML::Node node = YAML::Load(str);
            // 如果 node 是标量，那么对应的 vector 就只有一个元素
            if (node.IsScalar()) {
                return {LexicalCast<std::string, T>()(str)};
            }
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); i++) {
                ss.str(""); // 清空内容
                // ss.clear();    // 清除状态，防止错误
                ss << node[i]; // 将 YAML 节点转换为字符串

                // 这里可以递归的处理 Vector 类型
                vec.push_back(LexicalCast<std::string, T>()(ss.str()));
            }
            return vec;
        }
    };

    // 对 vector 进行偏特化，支持 vector -> string
    template <class T>
    class LexicalCast<std::vector<T>, std::string> {
    public:
        std::string operator()(const std::vector<T> &v) {
            YAML::Node node;
            for (auto &i : v) {
                // 递归的处理 Vector 类型
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // 对 list 进行偏特化，支持 string -> vector
    template <class T>
    class LexicalCast<std::string, std::list<T>> {
    public:
        std::list<T> operator()(const std::string &str) {
            std::list<T> vec;
            YAML::Node node = YAML::Load(str);
            // 如果 node 是标量，那么对应的 list 就只有一个元素
            if (node.IsScalar()) {
                return {LexicalCast<std::string, T>()(str)};
            }
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); i++) {
                ss.str(""); // 清空内容
                // ss.clear();    // 清除状态，防止错误
                ss << node[i]; // 将 YAML 节点转换为字符串

                // 这里可以递归的处理 list 类型
                vec.push_back(LexicalCast<std::string, T>()(ss.str()));
            }
            return vec;
        }
    };

    // 对 list 进行偏特化，支持 list -> string
    template <class T>
    class LexicalCast<std::list<T>, std::string> {
    public:
        std::string operator()(const std::list<T> &v) {
            YAML::Node node;
            for (auto &i : v) {
                // 递归的处理 Vector 类型
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // 对 set 进行偏特化，支持 string -> set
    template <class T>
    class LexicalCast<std::string, std::set<T>> {
    public:
        std::set<T> operator()(const std::string &str) {
            std::set<T> vec;
            YAML::Node node = YAML::Load(str);
            // 如果 node 是标量，那么对应的 set 就只有一个元素
            if (node.IsScalar()) {
                return {LexicalCast<std::string, T>()(str)};
            }
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); i++) {
                ss.str(""); // 清空内容
                // ss.clear();    // 清除状态，防止错误
                ss << node[i]; // 将 YAML 节点转换为字符串

                // 这里可以递归的处理 set 类型
                vec.emplace(LexicalCast<std::string, T>()(ss.str()));
            }
            return vec;
        }
    };

    // 对 set 进行偏特化，支持 set -> string
    template <class T>
    class LexicalCast<std::set<T>, std::string> {
    public:
        std::string operator()(const std::set<T> &v) {
            YAML::Node node;
            for (auto &i : v) {
                // 递归的处理 set 类型
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // 对 unordered_set 进行偏特化，支持 string -> unordered_set
    template <class T>
    class LexicalCast<std::string, std::unordered_set<T>> {
    public:
        std::unordered_set<T> operator()(const std::string &str) {
            std::unordered_set<T> vec;
            YAML::Node node = YAML::Load(str);
            // 如果 node 是标量，那么对应的 unordered_set 就只有一个元素
            if (node.IsScalar()) {
                return {LexicalCast<std::string, T>()(str)};
            }
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); i++) {
                ss.str(""); // 清空内容
                // ss.clear();    // 清除状态，防止错误
                ss << node[i]; // 将 YAML 节点转换为字符串

                // 这里可以递归的处理 unordered_set 类型
                vec.emplace(LexicalCast<std::string, T>()(ss.str()));
            }
            return vec;
        }
    };

    // 对 unordered_set 进行偏特化，支持 unordered_set -> string
    template <class T>
    class LexicalCast<std::unordered_set<T>, std::string> {
    public:
        std::string operator()(const std::unordered_set<T> &v) {
            YAML::Node node;
            for (auto &i : v) {
                // 递归的处理 set 类型
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // 对 map 进行偏特化，支持 string -> map
    template <class T>
    class LexicalCast<std::string, std::map<std::string, T>> {
    public:
        std::map<std::string, T> operator()(const std::string &str) {
            std::map<std::string, T> vec;
            YAML::Node node = YAML::Load(str);
            // // 如果 node 是标量，那么对应的 map 就只有一个元素
            // if (node.IsScalar()) {
            //     return {LexicalCast<std::string, T>()(str)};
            // }
            if (node.IsMap()) {
                std::stringstream ss;
                for (auto it = node.begin(); it != node.end(); ++it) {
                    ss.str(""); // 清空内容
                    // ss.clear();    // 清除状态，防止错误
                    ss << it->second; // 将 YAML 节点转换为字符串
                    // 这里可以递归的处理 map 类型
                    vec.emplace(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str()));
                }
            }

            return vec;
        }
    };

    // 对 map 进行偏特化，支持 map -> string
    template <class T>
    class LexicalCast<std::map<std::string, T>, std::string> {
    public:
        std::string operator()(const std::map<std::string, T> &v) {
            YAML::Node node;
            for (auto &[a, b] : v) {
                // 递归的处理 map 类型
                node[a] = (YAML::Load(LexicalCast<T, std::string>()(b)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // 对 unordered_map 进行偏特化，支持 string -> unordered_map
    template <class T>
    class LexicalCast<std::string, std::unordered_map<std::string, T>> {
    public:
        std::unordered_map<std::string, T> operator()(const std::string &str) {
            std::unordered_map<std::string, T> vec;
            YAML::Node node = YAML::Load(str);
            // // 如果 node 是标量，那么对应的 unordered_map 就只有一个元素
            // if (node.IsScalar()) {
            //     return {LexicalCast<std::string, T>()(str)};
            // }
            if (node.IsMap()) {
                std::stringstream ss;
                for (auto it = node.begin(); it != node.end(); ++it) {
                    ss.str(""); // 清空内容
                    // ss.clear();    // 清除状态，防止错误
                    ss << it->second; // 将 YAML 节点转换为字符串
                    // 这里可以递归的处理 unordered_map 类型
                    vec.emplace(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str()));
                }
            }

            return vec;
        }
    };

    // 对 unordered_map 进行偏特化，支持 unordered_map -> string
    template <class T>
    class LexicalCast<std::unordered_map<std::string, T>, std::string> {
    public:
        std::string operator()(const std::unordered_map<std::string, T> &v) {
            YAML::Node node;
            for (auto &[a, b] : v) {
                // 递归的处理 unordered_map 类型
                node[a] = (YAML::Load(LexicalCast<T, std::string>()(b)));
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // FromStr: T operator()(const std::string&)
    // ToSTr: std:string operator()(const T&)
    template <class T, class FromStr = LexicalCast<std::string, T>,
              class ToStr = LexicalCast<T, std::string>>
    class ConfigVar : public ConfigVarBase {
    public:
        typedef RWMutex RWMutexType;
        typedef std::shared_ptr<ConfigVar> ptr;

        // 当一个配置更改时，需要返回到代码层面知道原来的值和新值
        typedef std::function<void(const T &old_value, const T &new_value)> on_change_call_back;

        ConfigVar(const std::string &name, const T &default_value, const std::string &description = "")
            : ConfigVarBase(name, description), m_value(default_value) {}

        // 把 m_value 转换为 std::string，用于存储或显示配置值
        std::string toString() override {
            try {
                // return boost::lexical_cast<std::string>(m_value);
                RWMutexType::ReadLock lock(m_mutex);
                return ToStr()(m_value); // 适用通用类型
            } catch (const std::exception &e) {
                LSH_LOG_ERROR(LoggerMgr::GetInstance()->getRoot())
                    << "ConfigVar::toString exception"
                    << e.what() << " convert: "
                    << typeid(m_value).name() << " to string";
            }
            return "";
        }

        // 从 std::string 解析 m_value，用于读取和设置配置
        bool fromString(const std::string &str) override {
            try {
                // m_value = boost::lexical_cast<T>(str);
                setValue(FromStr()(str)); // // 适用通用类型
                return true;
            } catch (const std::exception &e) {
                LSH_LOG_ERROR(LoggerMgr::GetInstance()->getRoot())
                    << "ConfigVar::fromString exception "
                    << e.what() << " convert: string to "
                    << typeid(m_value).name();
            }
            return false;
        }

        const T getValue() const {
            RWMutexType::ReadLock lock(m_mutex);
            return m_value;
        }

        std::string getTypeName() const override { return typeid(T).name(); }

        void setValue(const T &val) {
            {
                RWMutexType::ReadLock lock(m_mutex);
                if (val == m_value) {
                    return;
                }
                for (auto &i : m_call_back_s) {
                    i.second(m_value, val);
                }
            }
            RWMutexType::WriteLock lock(m_mutex);
            m_value = val;
        }

        // 回调事件相关函数
        void addListener(uint64_t key, on_change_call_back cb) {
            RWMutexType::WriteLock lock(m_mutex);
            m_call_back_s[key] = cb;
        }

        void deleteListener(uint64_t key) {
            RWMutexType::WriteLock lock(m_mutex);
            m_call_back_s.erase(key);
        }

        void clearListener() {
            RWMutexType::WriteLock lock(m_mutex);
            m_call_back_s.clear();
        }

        on_change_call_back getListener(uint64_t key) {
            RWMutexType::ReadLock lock(m_mutex);
            auto it = m_call_back_s.find(key);
            return key == m_call_back_s.end() ? nullptr : it->second;
        }

    private:
        T m_value;

        // 变更回调函数组, uint64_t key, 要求唯一
        std::map<uint64_t, on_change_call_back> m_call_back_s;
        mutable RWMutexType m_mutex;
    };

    class Config {
    public:
        // 如果 Derived<T> 是 Base 的派生类，并且 Base 有虚函数（多态）
        // 容器内需要存放 Base 指针来支持多态
        typedef std::unordered_map<std::string, ConfigVarBase::ptr> ConfigVarMap;
        typedef RWMutex RWMutexType;

        template <class T>
        static typename ConfigVar<T>::ptr Creat(const std::string &name,
                                                const T &default_value,
                                                const std::string &description = "") {
            // // 转换为小写
            // std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            // 直接这样判断 temp 为不为空会有小 bug, 不符合预期效果
            // 我们这里希望的是若是有相同的名字，不会创建新的，并且给出提示信息
            // 但是如果我已经调用create创建了名字为“system”类型int，但是我再想调用create创建类型为float名字是“system”
            // 的时候首先Lookup<float>("system")，这个时候发生了
            // dynatic_pointer_cast<ConfigVar<float>>(ConfigVarBase::ptr)转换，
            // 由于ConfigVarBase::ptr指向的是ConfigVar<int>,所以转换失败，返回nullptr,跳过检查，
            // 又创建了“system”,类型为float，覆盖掉原来的int。
            // 这时候既不会给出提示信息，同时原来的“system” int型的配置没有了，再通过修改 yaml 是改变新创建的 float,"system"
            // 的配置，这时候不易察觉，因为我们再程序中还是通过第一次创建返回的 ConfigVar<int>::ptr 来调用方法
            // 这与我们的目的相违背，我们希望的是只要出现Create方法使用相同的名字就给出提示信息，而不是覆盖掉原来的配置
            // auto temp = Lookup<T>(name);
            // if (temp) {
            //     LSH_LOG_INFO(LoggerMgr::GetInstance()->getRoot())
            //         << "Lookup name = "
            //         << name << "exists";
            //     return temp;
            // }

            RWMutexType::WriteLock lock(GetMutex());
            auto it = GetDatas().find(name);
            if (it != GetDatas().end()) {
                // 有这个名字，进行动态转换，如果为 nullptr,就需要给出信息
                auto temp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
                if (temp) {
                    LSH_LOG_INFO(LoggerMgr::GetInstance()->getRoot())
                        << "Lookup name = "
                        << name << " exists";
                } else {
                    LSH_LOG_ERROR(LSH_LOG_ROOT)
                        << "Lookup name = " << name << " exists but type is not "
                        << typeid(T).name() << "! The real type is "
                        << it->second->getTypeName() << "! Returning nullptr!!!!!";
                    // 抛出异常，提示调用者类型错误
                    throw std::runtime_error("Config Lookup type mismatch for key: " + name +
                                             ". Expected: " + typeid(T).name() +
                                             ", but actual: " + it->second->getTypeName());
                }
                return temp;
            }

            // 查找 name 中第一个不属于指定字符集合的字符。
            if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
                LSH_LOG_ERROR(LoggerMgr::GetInstance()->getRoot())
                    << "Lookup name: " << name << " is invalid";
                throw std::invalid_argument(name);
            }

            typename ConfigVar<T>::ptr v = std::make_shared<ConfigVar<T>>(name, default_value, description);
            GetDatas()[name] = v;
            return v;
        }

        template <class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string &name) {
            // // 转换为小写
            // std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            RWMutexType::ReadLock lock(GetMutex());
            auto it = GetDatas().find(name);
            if (it == GetDatas().end()) {
                return nullptr;
            }
            return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
        }

        // 为了 Yaml 与 config.h 整合
        // 解析 YAML 配置，并加载到 ConfigVar 实例中
        static void LoadFromYaml(const YAML::Node &root);

        static ConfigVarBase::ptr LookupBase(const std::string &name);

        static void Visit(std::function<void(ConfigVarBase::ptr)> cb) {
            RWMutexType::ReadLock lock(GetMutex());
            ConfigVarMap &m = GetDatas();
            for (auto it = m.begin(); it != m.end(); it++) {
                cb(it->second);
            }
        }

    private:
        // 多态
        // 在 creat 方法中向里边添加配置
        // 实际指向 ConfigVar<T> 类型，其中 T 在调用 creat 是被确定
        // 在 C++ 中，不同编译单元（.cpp 文件）里的静态变量初始化顺序是未定义的。
        // 如果 Lookup 函数在 s_datas 还未完成初始化时就被调用，就会引发未定义行为。
        // 例如，在另一个编译单元里的全局变量初始化过程中调用了 Lookup 函数，而此时 s_datas 还没初始化，
        // 那么 s_datas.find(name) 操作就可能会出错。
        // 所以采用 静态方法返回
        // static ConfigVarMap s_datas; // 存储所有配置变量

        static ConfigVarMap &GetDatas() {
            static ConfigVarMap s_datas;
            return s_datas;
        }

        static RWMutexType &GetMutex() {
            static RWMutexType s_mutex;
            return s_mutex;
        }
    };
}

#endif