/// @file app.cpp
/// @brief CLI 框架模块 — 命令行参数解析器实现
/// @author LibreBio Team
/// @version 0.3.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// App::Impl 采用 PIMPL 模式隐藏解析细节。
/// 注册阶段使用游标模式（Cursor Pattern），解析阶段遵循 POSIX 约定。
///
/// @see doc/detailed-design/m16_cli_app.md

#include "libre_bio/cli/app.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace libre_bio {
namespace cli {

// ============================================================================
// 内部数据结构
// ============================================================================

/// @brief 选项定义
struct Option {
    std::string short_name;      ///< 短选项名，如 "o"（对应 -o）
    std::string long_name;       ///< 长选项名，如 "output"（对应 --output）
    std::string description;     ///< 帮助文本
    std::string default_value;   ///< 默认值
    bool required;               ///< 是否必填
    bool is_flag;                ///< 是否为布尔标志
};

/// @brief 子命令定义
struct SubCommand {
    std::string name;                  ///< 子命令名
    std::string description;           ///< 命令描述
    std::vector<Option> options;       ///< 选项列表
    std::string positional_name;       ///< 位置参数显示名（用于帮助）
};

// ============================================================================
// App::Impl
// ============================================================================

/// @brief App 的实现类（PIMPL）
///
/// 持有所有注册数据和解析结果。解析后 m_parsed 标记为 true，
/// 只读查询方法可安全并发。
class App::Impl {
public:
    Impl(std::string name, std::string version)
        : m_name(std::move(name))
        , m_version(std::move(version))
        , m_parsed(false)
    {
    }

    // ---- 注册 ----

    void add_sub_command(std::string name, std::string description)
    {
        SubCommand sub;
        sub.name = std::move(name);
        sub.description = std::move(description);
        m_sub_commands.push_back(std::move(sub));
    }

    void add_option(std::string short_name, std::string long_name,
                    std::string description,
                    std::string default_value,
                    bool required, bool is_flag)
    {
        if (m_sub_commands.empty()) {
            std::cerr << "[WARN] add_option(\"--" << long_name
                      << "\") 被忽略：尚未注册任何子命令\n";
            return;
        }

        Option opt;
        opt.short_name = std::move(short_name);
        opt.long_name = std::move(long_name);
        opt.description = std::move(description);
        opt.default_value = std::move(default_value);
        opt.required = required;
        opt.is_flag = is_flag;

        m_sub_commands.back().options.push_back(std::move(opt));
    }

    // ---- 解析 ----

    bool parse(int argc, const char* argv[])
    {
        if (argc < 1) {
            return false;
        }

        // 无参数 → 帮助
        if (argc < 2) {
            print_help(nullptr);
            return false;
        }

        const std::string arg1(argv[1]);

        // --help / -h
        if (arg1 == "--help" || arg1 == "-h") {
            print_help(nullptr);
            return false;
        }

        // --version / -V
        if (arg1 == "--version" || arg1 == "-V") {
            print_version_impl();
            return false;
        }

        // 查找子命令
        const SubCommand* sub = nullptr;
        for (const auto& sc : m_sub_commands) {
            if (sc.name == arg1) {
                sub = &sc;
                break;
            }
        }

        if (sub == nullptr) {
            std::cerr << "错误: 未知命令 \"" << arg1 << "\"\n\n";
            print_help(nullptr);
            return false;
        }

        m_sub_command_name = sub->name;

        // 构建选项查找表：long_name → Option*
        // 同时构建 short_name → long_name 映射
        std::map<std::string, const Option*> long_opt_map;
        std::map<std::string, std::string> short_to_long;

        for (const auto& opt : sub->options) {
            long_opt_map[opt.long_name] = &opt;
            if (!opt.short_name.empty()) {
                short_to_long[opt.short_name] = opt.long_name;
            }
        }

        // 初始化已解析选项为默认值
        m_parsed_options.clear();
        for (const auto& opt : sub->options) {
            m_parsed_options[opt.long_name] = opt.default_value;
        }
        m_positional_values.clear();

        // 解析参数
        int32_t i = 2;
        bool end_of_options = false;

        while (i < argc) {
            const std::string token(argv[i]);

            if (!end_of_options && token == "--") {
                end_of_options = true;
                ++i;
                continue;
            }

            if (!end_of_options && token.size() >= 2 && token[0] == '-') {
                if (token[1] == '-') {
                    // 长选项: --name 或 --name=value
                    const std::string raw = token.substr(2);
                    size_t eq_pos = raw.find('=');
                    std::string long_name;
                    std::string value;
                    bool has_explicit_value = false;

                    if (eq_pos != std::string::npos) {
                        long_name = raw.substr(0, eq_pos);
                        value = raw.substr(eq_pos + 1);
                        has_explicit_value = true;
                    } else {
                        long_name = raw;
                    }

                    auto it = long_opt_map.find(long_name);
                    if (it == long_opt_map.end()) {
                        std::cerr << "错误: 未知选项 \"--" << long_name
                                  << "\"\n\n";
                        print_help(sub);
                        return false;
                    }

                    const Option* opt = it->second;
                    if (opt->is_flag) {
                        m_parsed_options[opt->long_name] = "true";
                    } else {
                        if (has_explicit_value) {
                            m_parsed_options[opt->long_name] = value;
                        } else {
                            ++i;
                            if (i >= argc) {
                                std::cerr << "错误: 选项 \"--"
                                          << opt->long_name
                                          << "\" 缺少值\n\n";
                                print_help(sub);
                                return false;
                            }
                            m_parsed_options[opt->long_name] = argv[i];
                        }
                    }
                } else {
                    // 短选项: -o value 或 -abc（组合标志）
                    const std::string flags = token.substr(1);

                    if (flags.empty()) {
                        // 单独的 "-"，作为位置参数
                        m_positional_values.push_back(token);
                        ++i;
                        continue;
                    }

                    // 尝试作为组合短选项（-abc）
                    if (flags.size() > 1) {
                        // 检查是否所有字符都是 flag 选项
                        bool all_flags = true;
                        for (size_t ci = 0; ci < flags.size(); ++ci) {
                            std::string ch(1, flags[ci]);
                            auto to_long = short_to_long.find(ch);
                            if (to_long == short_to_long.end()) {
                                all_flags = false;
                                break;
                            }
                            const Option* opt_ptr = long_opt_map[to_long->second];
                            if (!opt_ptr->is_flag) {
                                all_flags = false;
                                break;
                            }
                        }

                        if (all_flags) {
                            for (size_t ci = 0; ci < flags.size(); ++ci) {
                                std::string ch(1, flags[ci]);
                                std::string lname = short_to_long[ch];
                                m_parsed_options[lname] = "true";
                            }
                            ++i;
                            continue;
                        }
                    }

                    // 作为单短选项 + 值
                    std::string ch(1, flags[0]);
                    auto to_long = short_to_long.find(ch);
                    if (to_long == short_to_long.end()) {
                        std::cerr << "错误: 未知选项 \"-" << ch << "\"\n\n";
                        print_help(sub);
                        return false;
                    }

                    const Option* opt_ptr = long_opt_map[to_long->second];
                    if (opt_ptr->is_flag) {
                        m_parsed_options[opt_ptr->long_name] = "true";
                    } else {
                        std::string value_part;
                        if (flags.size() > 1) {
                            // -oVALUE（值紧贴选项）
                            value_part = flags.substr(1);
                        } else {
                            ++i;
                            if (i >= argc) {
                                std::cerr << "错误: 选项 \"-"
                                          << opt_ptr->short_name
                                          << "\" 缺少值\n\n";
                                print_help(sub);
                                return false;
                            }
                            value_part = argv[i];
                        }
                        m_parsed_options[opt_ptr->long_name] = value_part;
                    }
                }
            } else {
                // 位置参数
                m_positional_values.push_back(token);
            }

            ++i;
        }

        // 检查必填选项
        for (const auto& opt : sub->options) {
            if (opt.required) {
                const auto& val = m_parsed_options[opt.long_name];
                if (val.empty()) {
                    std::cerr << "错误: 必填选项 \"--" << opt.long_name
                              << "\" 未提供\n\n";
                    print_help(sub);
                    return false;
                }
            }
        }

        m_parsed = true;
        return true;
    }

    // ---- 查询 ----

    [[nodiscard]] const std::string& sub_command_name() const noexcept
    {
        return m_sub_command_name;
    }

    [[nodiscard]] bool has_option(const std::string& name) const noexcept
    {
        return m_parsed_options.find(name) != m_parsed_options.end();
    }

    [[nodiscard]] const std::string&
        get_option(const std::string& name) const noexcept
    {
        auto it = m_parsed_options.find(name);
        if (it != m_parsed_options.end()) {
            return it->second;
        }
        return kEmptyString;
    }

    [[nodiscard]] const std::vector<std::string>&
        positional_args() const noexcept
    {
        return m_positional_values;
    }

    // ---- 帮助/版本 ----

    void print_help(const SubCommand* active_sub) const noexcept
    {
        std::cout << m_name << " v" << m_version << "\n\n";

        if (active_sub != nullptr) {
            std::cout << "用法: " << m_name << " " << active_sub->name;
        } else {
            std::cout << "用法: " << m_name << " <subcommand>";
        }
        std::cout << " [options] [<args>]\n\n";

        if (active_sub != nullptr) {
            std::cout << active_sub->description << "\n\n";

            if (!active_sub->options.empty()) {
                std::cout << "选项:\n";
                for (const auto& opt : active_sub->options) {
                    std::cout << "  ";
                    if (!opt.short_name.empty()) {
                        std::cout << "-" << opt.short_name << ", ";
                    } else {
                        std::cout << "    ";
                    }
                    std::cout << "--" << opt.long_name;

                    if (!opt.is_flag) {
                        std::cout << " <value>";
                    }

                    if (!opt.description.empty()) {
                        std::cout << "\t" << opt.description;
                    }

                    if (!opt.default_value.empty()) {
                        std::cout << " (默认: " << opt.default_value << ")";
                    }

                    if (opt.required) {
                        std::cout << " [必填]";
                    }

                    std::cout << "\n";
                }
                std::cout << "\n";
            }
        } else {
            // 列出所有子命令
            if (!m_sub_commands.empty()) {
                std::cout << "子命令:\n";
                for (const auto& sc : m_sub_commands) {
                    std::cout << "  " << sc.name;
                    if (!sc.description.empty()) {
                        std::cout << "\t" << sc.description;
                    }
                    std::cout << "\n";
                }
                std::cout << "\n";
            }
        }

        std::cout << "通用选项:\n";
        std::cout << "  -h, --help\t显示帮助\n";
        std::cout << "  -V, --version\t显示版本\n";
    }

    void print_help() const noexcept
    {
        print_help(nullptr);
    }

    void print_version_impl() const noexcept
    {
        std::cout << m_name << " v" << m_version << "\n";
    }

private:
    static const std::string kEmptyString;  ///< get_option 未命中时的返回值

    std::string m_name;
    std::string m_version;
    std::vector<SubCommand> m_sub_commands;

    // 解析结果
    std::string m_sub_command_name;
    std::map<std::string, std::string> m_parsed_options;
    std::vector<std::string> m_positional_values;
    bool m_parsed;
};

const std::string App::Impl::kEmptyString;

// ============================================================================
// App 公开接口实现
// ============================================================================

App::App(std::string name, std::string version)
    : m_impl(std::make_unique<Impl>(
          std::move(name), std::move(version)))
{
}

App::~App() = default;

App::App(App&& other) noexcept
    : m_impl(std::move(other.m_impl))
{
}

App& App::operator=(App&& other) noexcept
{
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

void App::add_sub_command(std::string name, std::string description)
{
    m_impl->add_sub_command(std::move(name), std::move(description));
}

void App::add_option(std::string short_name, std::string long_name,
                     std::string description, std::string default_value,
                     bool required, bool is_flag)
{
    m_impl->add_option(std::move(short_name), std::move(long_name),
                       std::move(description), std::move(default_value),
                       required, is_flag);
}

bool App::parse(int argc, const char* argv[])
{
    return m_impl->parse(argc, argv);
}

bool App::parse(const std::vector<std::string>& args)
{
    if (args.empty()) {
        return false;
    }

    std::vector<const char*> argv;
    argv.reserve(args.size());
    for (const auto& s : args) {
        argv.push_back(s.c_str());
    }

    return m_impl->parse(static_cast<int>(argv.size()), argv.data());
}

const std::string& App::sub_command_name() const noexcept
{
    return m_impl->sub_command_name();
}

bool App::has_option(const std::string& name) const noexcept
{
    return m_impl->has_option(name);
}

const std::string& App::get_option(const std::string& name) const noexcept
{
    return m_impl->get_option(name);
}

const std::vector<std::string>& App::positional_args() const noexcept
{
    return m_impl->positional_args();
}

void App::print_help() const noexcept
{
    m_impl->print_help();
}

void App::print_version() const noexcept
{
    m_impl->print_version_impl();
}

} // namespace cli
} // namespace libre_bio
