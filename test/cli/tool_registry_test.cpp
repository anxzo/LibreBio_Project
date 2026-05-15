/// @file tool_registry_test.cpp
/// @brief CLI 框架模块 — ToolRegistry 类单元测试
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 测试覆盖范围：
/// - 正常路径：注册 + 查找、批量注册 + 列表
/// - 边界条件：查找不存在的工具、重复注册后者覆盖、空注册表列表
/// - 移除：单个移除 + 清空
///
/// 注意：TC06（静态注册宏验证）不纳入单元测试范围，
/// 宏的正确性依赖编译期保证（静态对象构造时序）。
///
/// @see doc/detailed-design/m17_tool_registry.md §4 — 测试用例规格

#include "libre_bio/cli/tool_registry.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

using libre_bio::cli::ToolEntry;
using libre_bio::cli::ToolEntryFunc;
using libre_bio::cli::ToolRegistry;

// ============================================================================
// 辅助函数
// ============================================================================

namespace {

/// @brief 测试用工具入口函数 1
int test_func_1(int /*argc*/, const char* /*argv*/[])
{
    return 0;
}

/// @brief 测试用工具入口函数 2
int test_func_2(int /*argc*/, const char* /*argv*/[])
{
    return 1;
}

/// @brief 测试用工具入口函数 3
int test_func_3(int /*argc*/, const char* /*argv*/[])
{
    return 2;
}

/// @brief 构造一个 ToolEntry 对象
ToolEntry make_entry(const std::string& name,
                     const std::string& desc,
                     const std::string& ver,
                     ToolEntryFunc func)
{
    ToolEntry e;
    e.name = name;
    e.description = desc;
    e.version = ver;
    e.func = func;
    return e;
}

} // namespace

// ============================================================================
// TC01: 注册 + 查找 — 注册一个工具后通过 find_tool 查询
// ============================================================================

TEST_CASE("TC01: 注册并查找工具", "[tool_registry]")
{
    auto& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(make_entry("tc01_test", "TC01 test tool", "1.0", test_func_1));

    const ToolEntry* found = reg.find_tool("tc01_test");

    REQUIRE(found != nullptr);
    REQUIRE(found->name == "tc01_test");
    REQUIRE(found->description == "TC01 test tool");
    REQUIRE(found->version == "1.0");
    REQUIRE(found->func == test_func_1);

    reg.clear();
}

// ============================================================================
// TC02: 查找不存在的工具 — find_tool 返回 nullptr
// ============================================================================

TEST_CASE("TC02: 查找不存在的工具", "[tool_registry]")
{
    auto& reg = ToolRegistry::instance();
    reg.clear();

    const ToolEntry* found = reg.find_tool("tc02_nonexistent");

    REQUIRE(found == nullptr);
}

// ============================================================================
// TC03: 重复注册 — 同名工具后者覆盖前者
// ============================================================================

TEST_CASE("TC03: 重复注册后者覆盖", "[tool_registry]")
{
    auto& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(make_entry("tc03_dup", "first registration", "1.0", test_func_1));
    reg.register_tool(make_entry("tc03_dup", "second registration", "2.0", test_func_2));

    const ToolEntry* found = reg.find_tool("tc03_dup");

    REQUIRE(found != nullptr);
    REQUIRE(found->description == "second registration");
    REQUIRE(found->version == "2.0");
    REQUIRE(found->func == test_func_2);

    reg.clear();
}

// ============================================================================
// TC04: list_tools — 空注册表返回空 vector
// ============================================================================

TEST_CASE("TC04: 空注册表列表", "[tool_registry]")
{
    auto& reg = ToolRegistry::instance();
    reg.clear();

    const auto tools = reg.list_tools();

    REQUIRE(tools.empty());
}

// ============================================================================
// TC05: list_tools — 注册多个工具后列出
// ============================================================================

TEST_CASE("TC05: 多工具列表", "[tool_registry]")
{
    auto& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(make_entry("tc05_tool_a", "Tool A", "1.0", test_func_1));
    reg.register_tool(make_entry("tc05_tool_b", "Tool B", "1.0", test_func_2));
    reg.register_tool(make_entry("tc05_tool_c", "Tool C", "1.0", test_func_3));

    const auto tools = reg.list_tools();

    REQUIRE(tools.size() == 3);

    // 验证三个工具都在列表中
    int32_t found_count = 0;
    for (const auto* t : tools) {
        if (t->name == "tc05_tool_a" ||
            t->name == "tc05_tool_b" ||
            t->name == "tc05_tool_c") {
            ++found_count;
        }
    }
    REQUIRE(found_count == 3);

    reg.clear();
}

// ============================================================================
// TC06: remove_tool — 移除已注册工具后无法查找到
// ============================================================================

TEST_CASE("TC06: 移除单个工具", "[tool_registry]")
{
    auto& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(make_entry("tc06_rm", "to be removed", "1.0", test_func_1));

    // 确认已注册
    REQUIRE(reg.find_tool("tc06_rm") != nullptr);

    reg.remove_tool("tc06_rm");

    // 移除后无法查到
    REQUIRE(reg.find_tool("tc06_rm") == nullptr);

    // 移除不存在的工具不应崩溃
    reg.remove_tool("tc06_nonexistent");

    reg.clear();
}
