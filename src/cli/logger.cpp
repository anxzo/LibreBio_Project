/// @file logger.cpp
/// @brief CLI 框架模块 — 统一日志与进度输出实现
/// @author LibreBio Team
/// @version 0.1.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 实现 Logger 单例的日志输出与进度显示。
/// TTY 检测：POSIX 系统使用 isatty()，非 POSIX 系统默认非 TTY。
/// 进度条：TTY 模式原地刷新（\\r），非 TTY 模式每 10% 报告。
///
/// @see doc/detailed-design/m18_logger.md

#include "libre_bio/cli/logger.h"

#include <iostream>

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#else
// 非 POSIX 系统（Windows）：使用 _isatty
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define STDERR_FILENO _fileno(stderr)
#endif
#endif

namespace libre_bio {
namespace cli {

// ============================================================================
// 编译期常量
// ============================================================================

/// @brief 进度条显示宽度（字符数）
constexpr size_t kProgressBarWidth = 40;

/// @brief 非 TTY 模式下每 10% 报告一次进度
constexpr uint64_t kProgressReportInterval = 10;

// ============================================================================
// Meyer's Singleton
// ============================================================================

Logger& Logger::instance() noexcept
{
    static Logger logger;
    return logger;
}

// ============================================================================
// 构造函数 — 检测 isatty，初始化成员
// ============================================================================

Logger::Logger() noexcept
    : m_level(LogLevel::kInfo)
    , m_is_tty(false)
    , m_os(&std::cerr)
    , m_last_reported_percent(0)
{
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
    m_is_tty = (isatty(STDERR_FILENO) != 0);
#endif
}

// ============================================================================
// set_level — 设置日志级别
// ============================================================================

void Logger::set_level(LogLevel level) noexcept
{
    m_level = level;
}

// ============================================================================
// level — 获取当前日志级别
// ============================================================================

LogLevel Logger::level() const noexcept
{
    return m_level;
}

// ============================================================================
// log — 内部日志输出实现
// ============================================================================

void Logger::log(LogLevel level, const std::string& prefix,
                 const std::string& msg)
{
    if (static_cast<uint8_t>(level) > static_cast<uint8_t>(m_level)) {
        return;
    }

    *m_os << "[" << prefix << "] " << msg << "\n";
    m_os->flush();
}

// ============================================================================
// error — 严重错误
// ============================================================================

void Logger::error(const std::string& msg)
{
    log(LogLevel::kError, "ERROR", msg);
}

// ============================================================================
// warning — 警告
// ============================================================================

void Logger::warning(const std::string& msg)
{
    log(LogLevel::kWarning, "WARNING", msg);
}

// ============================================================================
// info — 一般信息
// ============================================================================

void Logger::info(const std::string& msg)
{
    log(LogLevel::kInfo, "INFO", msg);
}

// ============================================================================
// debug — 调试信息
// ============================================================================

void Logger::debug(const std::string& msg)
{
    log(LogLevel::kDebug, "DEBUG", msg);
}

// ============================================================================
// progress — 进度显示
//
// TTY 模式：使用 \\r 原地刷新，格式：
//   \\r[label] ████████░░░░░░░░ 50% (5000/10000)
//
// 非 TTY 模式：每 10% 输出一行：
//   [label] 10% (1000/10000)\\n
//
// current == total 时输出完成信息并换行。
// ============================================================================

void Logger::progress(uint64_t current, uint64_t total,
                      const std::string& label)
{
    if (total == 0) {
        return;
    }

    const uint64_t percent = (current * 100) / total;

    if (current >= total) {
        if (!label.empty()) {
            *m_os << "[" << label << "] ";
        }
        *m_os << "完成\n";
        m_os->flush();
        m_last_reported_percent = 0;
        return;
    }

    if (m_is_tty) {
        *m_os << "\r";

        if (!label.empty()) {
            *m_os << "[" << label << "] ";
        }

        const size_t filled =
            static_cast<size_t>((percent * kProgressBarWidth) / 100);

        for (size_t i = 0; i < filled; ++i) {
            *m_os << "\u2588";
        }
        for (size_t i = filled; i < kProgressBarWidth; ++i) {
            *m_os << "\u2591";
        }

        *m_os << " " << percent << "% (" << current << "/" << total << ")";
        m_os->flush();
    } else {
        if ((percent / kProgressReportInterval) >
            (m_last_reported_percent / kProgressReportInterval)) {
            if (!label.empty()) {
                *m_os << "[" << label << "] ";
            }
            *m_os << percent << "% (" << current << "/" << total << ")\n";
            m_os->flush();
        }
    }

    m_last_reported_percent = percent;
}

// ============================================================================
// set_ostream — 替换输出流（测试支持）
// ============================================================================

void Logger::set_ostream(std::ostream& os) noexcept
{
    m_os = &os;
}

// ============================================================================
// set_tty — 覆盖 TTY 检测（测试支持）
// ============================================================================

void Logger::set_tty(bool is_tty) noexcept
{
    m_is_tty = is_tty;
}

// ============================================================================
// reset_progress_state — 重置进度条内部状态
// ============================================================================

void Logger::reset_progress_state() noexcept
{
    m_last_reported_percent = 0;
}

} // namespace cli
} // namespace libre_bio
