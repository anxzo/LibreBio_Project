/// @file tool_registry.cpp
/// @brief CLI 框架模块 — 工具注册表实现
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 实现 ToolRegistry 单例的 register_tool / find_tool / list_tools。
/// Meyer's Singleton 保证线程安全的延迟初始化。
///
/// @see doc/detailed-design/m17_tool_registry.md

#include "libre_bio/cli/tool_registry.h"

namespace libre_bio {
namespace cli {

// ============================================================================
// Meyer's Singleton
// ============================================================================

ToolRegistry& ToolRegistry::instance() noexcept
{
    static ToolRegistry registry;
    return registry;
}

// ============================================================================
// register_tool — 后者覆盖（O(log n)）
// ============================================================================

void ToolRegistry::register_tool(const ToolEntry& entry)
{
    m_tools[entry.name] = entry;
}

// ============================================================================
// find_tool — 返回指针，未找到返回 nullptr（O(log n)）
// ============================================================================

const ToolEntry* ToolRegistry::find_tool(const std::string& name) const noexcept
{
    const auto it = m_tools.find(name);
    if (it == m_tools.end()) {
        return nullptr;
    }
    return &(it->second);
}

// ============================================================================
// list_tools — 遍历收集所有条目指针（O(n)）
// ============================================================================

std::vector<const ToolEntry*> ToolRegistry::list_tools() const noexcept
{
    std::vector<const ToolEntry*> result;
    result.reserve(m_tools.size());

    for (const auto& pair : m_tools) {
        result.push_back(&(pair.second));
    }

    return result;
}

// ============================================================================
// remove_tool — 按名称移除，不存在时静默忽略（O(log n)）
// ============================================================================

void ToolRegistry::remove_tool(const std::string& name) noexcept
{
    m_tools.erase(name);
}

// ============================================================================
// clear — 清空所有注册工具（O(n)）
// ============================================================================

void ToolRegistry::clear() noexcept
{
    m_tools.clear();
}

} // namespace cli
} // namespace libre_bio
