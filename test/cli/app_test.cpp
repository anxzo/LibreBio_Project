/// @file app_test.cpp
/// @brief CLI 框架模块 — CLI::App 类单元测试
/// @author LibreBio Team
/// @version 0.3.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 测试覆盖范围：
/// - 正常路径：子命令解析、选项解析（长短选项/标志/默认值）、位置参数
/// - 边界条件：组合短选项、长选项 = 语法、值贴短选项、-- 终止符、重复选项
/// - 非法输入：未知子命令、未知选项、必填选项缺失、值选项缺值
/// - 防御机制：未注册子命令时 add_option 被忽略
///
/// @see doc/detailed-design/m16_cli_app.md §5 — 测试用例规格

#include "libre_bio/cli/app.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using libre_bio::cli::App;

// ============================================================================
// 辅助函数：为子命令注册一组选项
// ============================================================================

namespace {

/// @brief 为当前游标注册基础选项集（--input 必填, --verbose flag, --count 带默认值）
void register_basic_options(App& app)
{
    app.add_option("i", "input", "输入文件", "", true, false);
    app.add_option("v", "verbose", "详细输出", "", false, true);
    app.add_option("c", "count", "数量", "10", false, false);
}

} // namespace

// ============================================================================
// TC01: 基本子命令解析 — 长选项正常解析
// ============================================================================

TEST_CASE("TC01: 基本子命令解析", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    register_basic_options(app);

    const bool ok = app.parse({"test_tool", "run", "--input", "data.txt"});

    REQUIRE(ok == true);
    REQUIRE(app.sub_command_name() == "run");
    REQUIRE(app.has_option("input") == true);
    REQUIRE(app.get_option("input") == "data.txt");
}

// ============================================================================
// TC02: --help — 无子命令时显示帮助
// ============================================================================

TEST_CASE("TC02: --help 显示帮助", "[cli_app]")
{
    App app("test_tool", "0.1.0");
    app.add_sub_command("run", "执行任务");

    const bool ok = app.parse({"test_tool", "--help"});

    REQUIRE(ok == false);
}

// ============================================================================
// TC03: --version — 显示版本
// ============================================================================

TEST_CASE("TC03: --version 显示版本", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    const bool ok = app.parse({"test_tool", "--version"});

    REQUIRE(ok == false);
}

// ============================================================================
// TC04: 必填选项缺失 — 返回 false
// ============================================================================

TEST_CASE("TC04: 必填选项缺失", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    register_basic_options(app);

    const bool ok = app.parse({"test_tool", "run"});

    REQUIRE(ok == false);
}

// ============================================================================
// TC05: 短选项 — -o 形式
// ============================================================================

TEST_CASE("TC05: 短选项", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    app.add_option("o", "output", "输出文件", "", false, false);

    const bool ok = app.parse({"test_tool", "run", "-o", "result.txt"});

    REQUIRE(ok == true);
    REQUIRE(app.has_option("output") == true);
    REQUIRE(app.get_option("output") == "result.txt");
}

// ============================================================================
// TC06: 标志选项 — is_flag=true 无需值
// ============================================================================

TEST_CASE("TC06: 标志选项", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    app.add_option("v", "verbose", "详细输出", "", false, true);

    const bool ok = app.parse({"test_tool", "run", "--verbose"});

    REQUIRE(ok == true);
    REQUIRE(app.has_option("verbose") == true);
    REQUIRE(app.get_option("verbose") == "true");
}

// ============================================================================
// TC07: 默认值 — 选项未提供时回退到 default_value
// ============================================================================

TEST_CASE("TC07: 默认值", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    app.add_option("o", "output", "输出文件", "default.txt", false, false);

    const bool ok = app.parse({"test_tool", "run"});

    REQUIRE(ok == true);
    REQUIRE(app.has_option("output") == true);
    REQUIRE(app.get_option("output") == "default.txt");
}

// ============================================================================
// TC08: 位置参数
// ============================================================================

TEST_CASE("TC08: 位置参数", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    register_basic_options(app);

    const bool ok = app.parse(
        {"test_tool", "run", "--input", "data.txt", "arg1.txt", "arg2.txt"});

    REQUIRE(ok == true);
    REQUIRE(app.positional_args().size() == 2);
    REQUIRE(app.positional_args()[0] == "arg1.txt");
    REQUIRE(app.positional_args()[1] == "arg2.txt");
}

// ============================================================================
// TC09: 未知子命令 — 返回 false
// ============================================================================

TEST_CASE("TC09: 未知子命令", "[cli_app]")
{
    App app("test_tool", "0.1.0");
    app.add_sub_command("run", "执行任务");

    const bool ok = app.parse({"test_tool", "unknown_cmd"});

    REQUIRE(ok == false);
}

// ============================================================================
// TC10: 游标防御 — 未注册子命令时 add_option 被忽略
// ============================================================================

TEST_CASE("TC10: 游标防御 — add_option 在无子命令时忽略", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    // 不应崩溃
    app.add_option("o", "output", "输出文件", "", false, false);

    // 无子命令，无参数时返回 false（显示帮助）
    const bool ok = app.parse({"test_tool"});
    REQUIRE(ok == false);
}

// ============================================================================
// TC11: 组合短选项 — -abc 等价于 -a -b -c
// ============================================================================

TEST_CASE("TC11: 组合短选项", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    app.add_option("a", "alpha", "Alpha 标志", "", false, true);
    app.add_option("b", "beta", "Beta 标志", "", false, true);
    app.add_option("c", "gamma", "Gamma 标志", "", false, true);

    const bool ok = app.parse({"test_tool", "run", "-abc"});

    REQUIRE(ok == true);
    REQUIRE(app.has_option("alpha") == true);
    REQUIRE(app.has_option("beta") == true);
    REQUIRE(app.has_option("gamma") == true);
    REQUIRE(app.get_option("alpha") == "true");
    REQUIRE(app.get_option("beta") == "true");
    REQUIRE(app.get_option("gamma") == "true");
}

// ============================================================================
// TC12: 未知选项 — 返回 false
// ============================================================================

TEST_CASE("TC12: 未知选项", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    register_basic_options(app);

    const bool ok = app.parse({"test_tool", "run", "--nonexistent"});

    REQUIRE(ok == false);
}

// ============================================================================
// TC13: 值选项缺值 — 返回 false
// ============================================================================

TEST_CASE("TC13: 值选项缺值", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    app.add_option("o", "output", "输出文件", "", false, false);

    const bool ok = app.parse({"test_tool", "run", "--output"});

    REQUIRE(ok == false);
}

// ============================================================================
// TC14: 重复选项 — 后者覆盖前者
// ============================================================================

TEST_CASE("TC14: 重复选项后者覆盖", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    app.add_option("o", "output", "输出文件", "", false, false);

    const bool ok = app.parse(
        {"test_tool", "run", "--output", "first.txt", "-o", "second.txt"});

    REQUIRE(ok == true);
    REQUIRE(app.get_option("output") == "second.txt");
}

// ============================================================================
// TC15: -- 终止符
// ============================================================================

TEST_CASE("TC15: -- 终止符后续均为位置参数", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    app.add_option("v", "verbose", "详细输出", "", false, true);

    const bool ok = app.parse(
        {"test_tool", "run", "--verbose", "--", "--not_an_option", "-f"});

    REQUIRE(ok == true);
    REQUIRE(app.has_option("verbose") == true);
    REQUIRE(app.positional_args().size() == 2);
    REQUIRE(app.positional_args()[0] == "--not_an_option");
    REQUIRE(app.positional_args()[1] == "-f");
}

// ============================================================================
// TC16: 长选项 = 语法 — --input=file.txt
// ============================================================================

TEST_CASE("TC16: 长选项等号语法", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    app.add_option("i", "input", "输入文件", "", false, false);

    const bool ok = app.parse({"test_tool", "run", "--input=data.txt"});

    REQUIRE(ok == true);
    REQUIRE(app.get_option("input") == "data.txt");
}

// ============================================================================
// TC17: 短选项值贴附 — -ofile.txt
// ============================================================================

TEST_CASE("TC17: 短选项值贴附语法", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("run", "执行任务");
    app.add_option("o", "output", "输出文件", "", false, false);

    const bool ok = app.parse({"test_tool", "run", "-ofile.txt"});

    REQUIRE(ok == true);
    REQUIRE(app.get_option("output") == "file.txt");
}

// ============================================================================
// TC18: 无参数 — 返回 false 显示帮助
// ============================================================================

TEST_CASE("TC18: 无参数显示帮助", "[cli_app]")
{
    App app("test_tool", "0.1.0");
    app.add_sub_command("run", "执行任务");

    const bool ok = app.parse({"test_tool"});

    REQUIRE(ok == false);
}

// ============================================================================
// TC19: 多子命令注册 — 选项不跨子命令泄漏
// ============================================================================

TEST_CASE("TC19: 多子命令选项隔离", "[cli_app]")
{
    App app("test_tool", "0.1.0");

    app.add_sub_command("cmd1", "命令1");
    app.add_option("a", "alpha", "Alpha 选项", "", false, true);

    app.add_sub_command("cmd2", "命令2");
    app.add_option("b", "beta", "Beta 选项", "", false, true);

    // cmd2 不应该认识 --alpha
    const bool ok = app.parse({"test_tool", "cmd2", "--alpha"});

    REQUIRE(ok == false);
}
