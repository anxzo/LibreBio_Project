/// @file logger_test.cpp
/// @brief CLI 框架模块 — Logger 类单元测试
/// @author LibreBio Team
/// @version 0.1.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 测试覆盖范围：
/// - 正常路径：各级别日志输出、级别过滤、便捷宏
/// - TTY 模式：进度条原地刷新（\\r）
/// - 非 TTY 模式：进度条阶段性报告（每 10%）
/// - 边界条件：零总量进度、进度完成
///
/// 使用 std::ostringstream 捕获输出，set_tty() 模拟终端类型。
///
/// @see doc/detailed-design/m18_logger.md §5 — 测试用例规格

#include "libre_bio/cli/logger.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <sstream>
#include <string>

using libre_bio::cli::LogLevel;
using libre_bio::cli::Logger;

// ============================================================================
// 辅助函数
// ============================================================================

namespace {

/// @brief 重置 Logger 状态并重定向到 stringstream 用于测试
///
/// 每次测试前调用此函数确保干净的初始状态。
///
/// @param oss 输出捕获流
void reset_logger(std::ostringstream& oss)
{
    auto& logger = Logger::instance();
    logger.set_level(LogLevel::kInfo);
    logger.set_tty(false);
    logger.set_ostream(oss);
    logger.reset_progress_state();
    oss.str("");
    oss.clear();
}

} // namespace

// ============================================================================
// TC01: 基本日志输出 — 默认级别 kInfo 下所有可见级别均输出
// ============================================================================

TEST_CASE("TC01: 基本日志输出", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();

    logger.error("test error");
    logger.warning("test warning");
    logger.info("test info");

    const std::string output = oss.str();

    REQUIRE(output.find("[ERROR] test error") != std::string::npos);
    REQUIRE(output.find("[WARNING] test warning") != std::string::npos);
    REQUIRE(output.find("[INFO] test info") != std::string::npos);
}

// ============================================================================
// TC02: 日志级别过滤 — set_level(kWarning) 后 debug 和 info 不输出
// ============================================================================

TEST_CASE("TC02: 日志级别过滤", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_level(LogLevel::kWarning);

    logger.debug("debug msg");
    logger.info("info msg");
    logger.warning("warning msg");
    logger.error("error msg");

    const std::string output = oss.str();

    REQUIRE(output.find("debug msg") == std::string::npos);
    REQUIRE(output.find("info msg") == std::string::npos);
    REQUIRE(output.find("[WARNING] warning msg") != std::string::npos);
    REQUIRE(output.find("[ERROR] error msg") != std::string::npos);
}

// ============================================================================
// TC03: 进度条 TTY 模式 — 使用 \\r 原地刷新
// ============================================================================

TEST_CASE("TC03: 进度条 TTY 模式带标签", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_tty(true);

    logger.progress(50, 100, "\u5904\u7406\u4e2d");

    const std::string output = oss.str();

    REQUIRE(output.find("\r") != std::string::npos);
    REQUIRE(output.find("[\u5904\u7406\u4e2d]") != std::string::npos);
    REQUIRE(output.find("50%") != std::string::npos);
    REQUIRE(output.find("(50/100)") != std::string::npos);
}

TEST_CASE("TC03b: 进度条 TTY 模式不带标签", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_tty(true);

    logger.progress(30, 100);

    const std::string output = oss.str();

    REQUIRE(output.find("\r") != std::string::npos);
    REQUIRE(output.find("30%") != std::string::npos);
    REQUIRE(output.find("(30/100)") != std::string::npos);
}

// ============================================================================
// TC04: 进度条非 TTY 模式 — 每 10% 输出一行
// ============================================================================

TEST_CASE("TC04: 进度条非 TTY 模式", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_tty(false);

    logger.progress(10, 100);

    const std::string output = oss.str();
    REQUIRE(output.find("10%") != std::string::npos);
    REQUIRE(output.find("(10/100)") != std::string::npos);
}

TEST_CASE("TC04b: 同一10%区间不重复输出", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_tty(false);

    logger.progress(10, 100);

    oss.str("");
    oss.clear();
    logger.progress(15, 100);

    REQUIRE(oss.str().empty());
}

TEST_CASE("TC04c: 超过10%阈值再次输出", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_tty(false);

    logger.progress(10, 100);

    oss.str("");
    oss.clear();
    logger.progress(20, 100);

    const std::string output = oss.str();
    REQUIRE(output.find("20%") != std::string::npos);
}

// ============================================================================
// TC05: 进度条完成 — current == total 时输出完成信息并换行
// ============================================================================

TEST_CASE("TC05: 进度条完成 TTY", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_tty(true);
    logger.progress(100, 100);

    const std::string output = oss.str();
    REQUIRE(output.find("\u5b8c\u6210") != std::string::npos);
    REQUIRE(output.find("\n") != std::string::npos);
}

TEST_CASE("TC05b: 进度条完成非TTY带标签", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_tty(false);
    logger.progress(100, 100, "\u6d4b\u8bd5");

    const std::string output = oss.str();
    REQUIRE(output.find("[\u6d4b\u8bd5]") != std::string::npos);
    REQUIRE(output.find("\u5b8c\u6210") != std::string::npos);
}

// ============================================================================
// TC06: 零总量进度 — total == 0 时不输出、不崩溃
// ============================================================================

TEST_CASE("TC06: 零总量进度", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();

    logger.progress(0, 0);
    logger.progress(0, 0, "\u6807\u7b7e");

    REQUIRE(oss.str().empty());
}

// ============================================================================
// TC07: set_level / level 读写 — 基本属性操作
// ============================================================================

TEST_CASE("TC07: set_level / level 读写", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();

    REQUIRE(logger.level() == LogLevel::kInfo);

    logger.set_level(LogLevel::kDebug);
    REQUIRE(logger.level() == LogLevel::kDebug);

    logger.set_level(LogLevel::kError);
    REQUIRE(logger.level() == LogLevel::kError);
}

// ============================================================================
// TC08: 便捷宏 — 宏展开正确调用 Logger 方法
// ============================================================================

TEST_CASE("TC08: 便捷宏", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    LIBRE_BIO_LOG_ERROR("macro error");
    LIBRE_BIO_LOG_WARNING("macro warning");
    LIBRE_BIO_LOG_INFO("macro info");

    const std::string output = oss.str();

    REQUIRE(output.find("[ERROR] macro error") != std::string::npos);
    REQUIRE(output.find("[WARNING] macro warning") != std::string::npos);
    REQUIRE(output.find("[INFO] macro info") != std::string::npos);
}

// ============================================================================
// TC09: kDebug 级别 — 设置为 kDebug 后 debug 消息可见
// ============================================================================

TEST_CASE("TC09: kDebug 级别可见", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_level(LogLevel::kDebug);

    logger.debug("debug visible");
    logger.info("info visible");
    logger.error("error visible");

    const std::string output = oss.str();

    REQUIRE(output.find("[DEBUG] debug visible") != std::string::npos);
    REQUIRE(output.find("[INFO] info visible") != std::string::npos);
    REQUIRE(output.find("[ERROR] error visible") != std::string::npos);
}

// ============================================================================
// TC10: kError 级别 — 设为 kError 后仅 error 可见
// ============================================================================

TEST_CASE("TC10: kError 级别 — 仅 error 可见", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_level(LogLevel::kError);

    logger.debug("debug hidden");
    logger.info("info hidden");
    logger.warning("warning hidden");
    logger.error("error visible");

    const std::string output = oss.str();

    REQUIRE(output.find("debug hidden") == std::string::npos);
    REQUIRE(output.find("info hidden") == std::string::npos);
    REQUIRE(output.find("warning hidden") == std::string::npos);
    REQUIRE(output.find("[ERROR] error visible") != std::string::npos);
}

// ============================================================================
// TC11: 进度条完整序列 — 验证 TTY 模式下多次调用的累积输出
// ============================================================================

TEST_CASE("TC11: 进度条完整序列 TTY", "[logger]")
{
    std::ostringstream oss;
    reset_logger(oss);

    auto& logger = Logger::instance();
    logger.set_tty(true);

    logger.progress(0, 100, "\u6d4b\u8bd5");
    logger.progress(50, 100, "\u6d4b\u8bd5");
    logger.progress(100, 100, "\u6d4b\u8bd5");

    const std::string output = oss.str();

    REQUIRE(output.find("\u5b8c\u6210") != std::string::npos);
    REQUIRE(output.find("\r") != std::string::npos);
}
