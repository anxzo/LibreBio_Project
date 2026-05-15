/// @file logger.h
/// @brief CLI 框架模块 — 统一日志与进度输出
/// @author LibreBio Team
/// @version 0.1.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// Logger 是 CLI 框架的统一日志输出组件，采用单例模式（Meyer's Singleton）。
/// 日志输出到 stderr（可配置），数据输出到 stdout。支持多级别日志过滤
/// 和 TTY 感知的进度条显示。
///
/// ## 日志级别
/// | 级别 | 值 | 说明 |
/// |------|-----|------|
/// | kError | 0 | 严重错误（始终输出） |
/// | kWarning | 1 | 警告 |
/// | kInfo | 2 | 一般信息（默认级别） |
/// | kDebug | 3 | 调试信息（最详细） |
///
/// ## 使用示例
/// @code
/// #include "libre_bio/cli/logger.h"
///
/// // 使用便捷宏
/// LIBRE_BIO_LOG_INFO("处理开始");
/// LIBRE_BIO_LOG_ERROR("文件打开失败: input.fa");
///
/// // 直接调用
/// auto& logger = libre_bio::cli::Logger::instance();
/// logger.set_level(libre_bio::cli::LogLevel::kDebug);
/// logger.progress(50, 100, "处理中");
/// @endcode
///
/// @see doc/detailed-design/m18_logger.md

#ifndef LIBRE_BIO_CLI_LOGGER_H_
#define LIBRE_BIO_CLI_LOGGER_H_

#include <cstdint>
#include <iosfwd>
#include <string>

namespace libre_bio {
namespace cli {

/// @brief 日志级别枚举
///
/// 级别过滤规则：仅输出级别 ≤ 当前设置级别的消息。
/// 例如 set_level(kWarning) → 输出 error + warning，不输出 info/debug。
enum class LogLevel : uint8_t {
    kError   = 0,  ///< 严重错误（始终输出）
    kWarning = 1,  ///< 警告
    kInfo    = 2,  ///< 一般信息（默认级别）
    kDebug   = 3,  ///< 调试信息（最详细）
};

/// @brief 统一日志与进度输出（单例）
///
/// 负责 CLI 工具的日志输出与进度显示。日志写入 stderr，
/// 进度条支持 TTY 原地刷新和非 TTY 阶段性报告两种模式。
///
/// @details
/// ## 设计意图
/// - **单例模式**（Meyer's Singleton）：全局唯一实例，函数内静态变量避免 SIOF。
/// - **TTY 感知**：自动检测 stderr 是否为终端，TTY 下进度条使用 \\r 原地刷新。
/// - **级别过滤**：低于当前日志级别的消息静默丢弃。
/// - **无异常设计**：所有输出失败静默忽略，不抛出异常。
///
/// ## 线程安全性
/// - 写操作（error/warning/info/debug/progress）非线程安全，建议单线程使用。
/// - 读操作（level）在多线程中安全（只读）。
///
/// ## 测试支持
/// - set_ostream() 可替换输出流为 std::ostringstream，便于测试验证。
/// - set_tty() 可覆盖 TTY 自动检测结果。
class Logger {
public:
    /// @brief 获取单例实例（Meyer's Singleton）
    ///
    /// 首次调用时构造，C++11 保证线程安全的延迟初始化。
    /// 构造函数自动检测 stderr 是否为终端。
    ///
    /// @return 全局唯一 Logger 实例引用
    static Logger& instance() noexcept;

    /// @name 拷贝/移动语义 — 单例禁止
    /// @{
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    /// @}

    /// @brief 设置当前日志级别
    ///
    /// 低于此级别的消息将被静默丢弃。
    ///
    /// @param level 目标日志级别
    void set_level(LogLevel level) noexcept;

    /// @brief 获取当前日志级别
    ///
    /// @return 当前日志级别（默认 kInfo）
    [[nodiscard]] LogLevel level() const noexcept;

    /// @brief 输出严重错误消息
    ///
    /// 格式：[ERROR] message\\n，写入 stderr。
    ///
    /// @param msg 错误消息内容
    void error(const std::string& msg);

    /// @brief 输出警告消息
    ///
    /// 格式：[WARNING] message\\n，当前级别 ≥ kWarning 时可见。
    ///
    /// @param msg 警告消息内容
    void warning(const std::string& msg);

    /// @brief 输出一般信息消息
    ///
    /// 格式：[INFO] message\\n，当前级别 ≥ kInfo 时可见（默认可见）。
    ///
    /// @param msg 信息消息内容
    void info(const std::string& msg);

    /// @brief 输出调试信息消息
    ///
    /// 格式：[DEBUG] message\\n，仅当前级别为 kDebug 时可见。
    ///
    /// @param msg 调试消息内容
    void debug(const std::string& msg);

    /// @brief 显示进度条
    ///
    /// TTY 模式下使用 \\r 原地刷新进度条。非 TTY 模式下每 10% 输出一行。
    /// current == total 时输出完成信息并换行。
    ///
    /// @param current 当前进度值
    /// @param total 总计数值，为 0 时不输出（避免除零）
    /// @param label 可选的进度标签，显示在进度条前方
    void progress(uint64_t current, uint64_t total,
                  const std::string& label = "");

    /// @brief 替换输出流（用于测试）
    ///
    /// 默认输出到 std::cerr。测试时可替换为 std::ostringstream。
    ///
    /// @param os 目标输出流引用
    void set_ostream(std::ostream& os) noexcept;

    /// @brief 覆盖 TTY 检测结果（用于测试）
    ///
    /// @param is_tty true 表示模拟 TTY 模式
    void set_tty(bool is_tty) noexcept;

    /// @brief 重置进度条内部状态
    ///
    /// 清除上次报告百分比记录，下次 progress() 调用视为新一轮的开始。
    /// 用于测试隔离和多阶段进度场景。
    void reset_progress_state() noexcept;

private:
    /// @brief 私有构造函数 — 检测 isatty 并初始化
    Logger() noexcept;

    /// @brief 内部日志输出实现
    ///
    /// @param level 日志级别
    /// @param prefix 级别前缀字符串（如 "ERROR"）
    /// @param msg 消息内容
    void log(LogLevel level, const std::string& prefix,
             const std::string& msg);

    LogLevel m_level;                    ///< 当前日志级别
    bool m_is_tty;                       ///< stderr 是否为终端
    std::ostream* m_os;                  ///< 输出流指针（默认 &std::cerr）
    uint64_t m_last_reported_percent;    ///< 非 TTY 进度条上次报告百分比
};

} // namespace cli
} // namespace libre_bio

/// @name 日志便捷宏
///
/// 简化日志调用，编译期可根据 NDEBUG 条件编译移除 debug 日志。
///
/// @{
#define LIBRE_BIO_LOG_ERROR(msg)   libre_bio::cli::Logger::instance().error(msg)
#define LIBRE_BIO_LOG_WARNING(msg) libre_bio::cli::Logger::instance().warning(msg)
#define LIBRE_BIO_LOG_INFO(msg)    libre_bio::cli::Logger::instance().info(msg)

#ifdef NDEBUG
#define LIBRE_BIO_LOG_DEBUG(msg)   ((void)0)
#else
#define LIBRE_BIO_LOG_DEBUG(msg)   libre_bio::cli::Logger::instance().debug(msg)
#endif
/// @}

#endif // LIBRE_BIO_CLI_LOGGER_H_
