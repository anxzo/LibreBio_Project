/// @file tool_registry.h
/// @brief CLI 框架模块 — 工具注册表类型定义
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// ToolRegistry 是 CLI 工具注册与查找系统，采用单例模式（Meyer's Singleton）。
/// 支持运行时注册和编译期宏注册两种方式。
///
/// ## 注册方式
/// 1. **运行时注册**：调用 ToolRegistry::instance().register_tool(entry)
/// 2. **编译期宏注册**：在工具 .cpp 文件中使用 LIBRE_BIO_REGISTER_TOOL 宏
///
/// ## SIOF 防护
/// 使用函数内静态变量（Meyer's Singleton）避免静态初始化顺序问题。
///
/// @see doc/detailed-design/m17_tool_registry.md

#ifndef LIBRE_BIO_CLI_TOOL_REGISTRY_H_
#define LIBRE_BIO_CLI_TOOL_REGISTRY_H_

#include <map>
#include <string>
#include <vector>

namespace libre_bio {
namespace cli {

/// @brief 工具入口函数指针类型
///
/// 每个 CLI 工具的 main 函数签名。
/// @param argc 参数个数
/// @param argv 参数数组
/// @return 退出码，0 表示成功
using ToolEntryFunc = int (*)(int argc, const char* argv[]);

/// @brief 工具注册条目
///
/// 描述一个 CLI 工具的元数据与入口函数。
///
/// @details
/// ## 字段说明
/// - name：子命令名，用户通过 `<tool> <name>` 调用
/// - description：帮助文本中显示的工具描述
/// - version：工具版本号
/// - func：入口函数指针，接收 argc/argv
struct ToolEntry {
    std::string name;         ///< 工具名称（子命令名）
    std::string description;  ///< 工具描述（用于帮助信息）
    std::string version;      ///< 工具版本号
    ToolEntryFunc func;       ///< 入口函数指针
};

/// @brief 工具注册表（单例）
///
/// 负责 CLI 工具的注册、查找与列表。
///
/// @details
/// ## 设计意图
/// - **单例模式**（Meyer's Singleton）：全局唯一注册表，函数内静态变量避免 SIOF。
/// - **后者覆盖**：同名工具重复注册时，后者覆盖前者（静默）。
/// - **无异常设计**：查找失败返回 nullptr，不抛出异常。
///
/// ## 线程安全性
/// - 注册阶段（register_tool）非线程安全，建议在 main() 之前完成。
/// - 查找阶段（find_tool / list_tools）在多线程中安全并发（只读）。
///
/// ## 使用示例
/// @code
/// #include "libre_bio/cli/tool_registry.h"
///
/// int my_tool_main(int argc, const char* argv[]) {
///     // 工具逻辑
///     return 0;
/// }
///
/// int main(int argc, const char* argv[]) {
///     auto& reg = libre_bio::cli::ToolRegistry::instance();
///     reg.register_tool({"my_tool", "我的工具", "1.0", my_tool_main});
///
///     const auto* entry = reg.find_tool("my_tool");
///     if (entry != nullptr) {
///         return entry->func(argc, argv);
///     }
///     // 未找到工具，输出帮助
///     return 1;
/// }
/// @endcode
class ToolRegistry {
public:
    /// @brief 获取单例实例（Meyer's Singleton）
    ///
    /// 首次调用时构造，C++11 保证线程安全。
    ///
    /// @return 全局唯一 ToolRegistry 实例引用
    static ToolRegistry& instance() noexcept;

    /// @name 拷贝/移动语义
    /// @{
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;
    ToolRegistry(ToolRegistry&&) = delete;
    ToolRegistry& operator=(ToolRegistry&&) = delete;
    /// @}

    /// @brief 注册工具
    ///
    /// 若 name 已存在，后者覆盖前者（静默）。
    ///
    /// @param entry 工具注册条目
    void register_tool(const ToolEntry& entry);

    /// @brief 按名称查找工具
    /// @param name 工具名称（子命令名）
    /// @return 找到返回条目指针，未找到返回 nullptr
    [[nodiscard]] const ToolEntry* find_tool(
        const std::string& name) const noexcept;

    /// @brief 列出所有已注册工具
    /// @return 所有工具条目的指针列表
    [[nodiscard]] std::vector<const ToolEntry*> list_tools() const noexcept;

    /// @brief 移除指定名称的工具
    ///
    /// 若工具不存在，静默忽略。
    ///
    /// @param name 工具名称
    void remove_tool(const std::string& name) noexcept;

    /// @brief 清空所有已注册工具
    void clear() noexcept;

private:
    /// @brief 私有构造（单例模式）
    ToolRegistry() = default;

    /// @brief 工具名 → 条目映射
    std::map<std::string, ToolEntry> m_tools;
};

} // namespace cli
} // namespace libre_bio

// ============================================================================
// 静态注册宏
// ============================================================================

/// @brief 编译期工具注册宏
///
/// 在工具 .cpp 文件中使用，利用静态对象在 main() 之前构造的特性完成自注册。
///
/// @param name 工具名称（子命令名）
/// @param desc 工具描述
/// @param ver 工具版本号
/// @param func 入口函数名（函数指针）
///
/// @details
/// ## 工作原理
/// 1. 宏展开生成匿名命名空间内的静态注册器类
/// 2. 注册器的构造函数调用 ToolRegistry::instance().register_tool()
/// 3. C++ 保证静态对象在 main() 之前构造
///
/// ## 使用示例
/// @code
/// // my_tool.cpp
/// int my_tool_main(int argc, const char* argv[]) {
///     return 0;
/// }
///
/// LIBRE_BIO_REGISTER_TOOL("my_tool", "我的工具", "1.0", my_tool_main);
/// @endcode
///
/// @note 注册顺序依赖链接顺序，不应在同一翻译单元内使用两个宏注册同名工具。
#define LIBRE_BIO_REGISTER_TOOL(name, desc, ver, func) \
    namespace { \
        struct Register_##func { \
            Register_##func() \
            { \
                libre_bio::cli::ToolRegistry::instance() \
                    .register_tool({name, desc, ver, func}); \
            } \
        }; \
        static const Register_##func g_register_##func; \
    }

#endif // LIBRE_BIO_CLI_TOOL_REGISTRY_H_
