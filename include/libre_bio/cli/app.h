/// @file app.h
/// @brief CLI 框架模块 — 命令行参数解析器类型定义
/// @author LibreBio Team
/// @version 0.3.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// CLI::App 是命令行参数解析器的核心类，支持子命令模式、命名选项（--flag）、
/// 短选项（-f）、位置参数。自动生成帮助和版本信息。
///
/// ## API 设计
/// 采用**游标模式**：add_option 自动关联到最后注册的子命令。
/// 若在未注册任何子命令时调用 add_option，输出警告并忽略。
///
/// ## 测试接口
/// 提供 parse(std::vector<std::string>) 重载，方便测试代码中直接传递
/// 字符串列表，无需手动构造 argc/argv。
///
/// @see doc/detailed-design/m16_cli_app.md

#ifndef LIBRE_BIO_CLI_APP_H_
#define LIBRE_BIO_CLI_APP_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace libre_bio {
namespace cli {

/// @brief 命令行参数解析器
///
/// 支持子命令模式（类似 git 的 `<tool> <subcommand>` 模式），
/// 自动生成 --help / --version 输出。
///
/// @details
/// ## 设计意图
/// - **无运行时依赖**：仅使用 STL，不依赖 getopt 等外部库。
/// - **PIMPL 隔离**：内部实现通过 Impl 类隐藏，保持 ABI 稳定。
/// - **游标注册**：add_sub_command 后通过 add_option 为当前子命令注册选项。
///
/// ## 线程安全性
/// - 注册阶段（add_sub_command / add_option）非线程安全。
/// - 解析后只读访问（get_option / positional_args）可在多线程中安全并发。
///
/// ## 使用示例
/// @code
/// #include "libre_bio/cli/app.h"
///
/// libre_bio::cli::App app("my_tool", "1.0.0");
///
/// app.add_sub_command("seqkit", "序列处理工具");
/// app.add_option("-i", "--input", "输入文件", "", true, false);
/// app.add_option("-v", "--verbose", "详细输出", "", false, true);
///
/// if (app.parse(argc, argv)) {
///     const std::string& input = app.get_option("input");
///     if (app.has_option("verbose")) { /* ... */ }
/// }
/// @endcode
class App {
public:
    /// @brief 构造应用实例
    /// @param name 应用名称（用于帮助信息）
    /// @param version 版本号（用于 --version 输出）
    App(std::string name, std::string version);

    /// @brief 析构
    ~App();

    /// @name 拷贝/移动语义
    /// @{
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&& other) noexcept;
    App& operator=(App&& other) noexcept;
    /// @}

    /// @name 子命令与选项注册
    /// @{

    /// @brief 注册子命令，游标切换至该子命令
    ///
    /// 后续的 add_option 调用将关联到此子命令。
    ///
    /// @param name 子命令名
    /// @param description 子命令描述（用于帮助信息）
    void add_sub_command(std::string name, std::string description);

    /// @brief 为当前子命令注册选项
    ///
    /// 若尚未调用 add_sub_command 注册任何子命令，输出警告并忽略。
    ///
    /// @param short_name 短选项名，如 "o"（对应 -o）。空字符串表示无短选项
    /// @param long_name 长选项名，如 "output"（对应 --output）
    /// @param description 选项描述（用于帮助信息）
    /// @param default_value 默认值，未提供时使用
    /// @param required 是否为必填选项
    /// @param is_flag 是否为布尔标志（无需值参数）
    void add_option(std::string short_name, std::string long_name,
                    std::string description,
                    std::string default_value = "",
                    bool required = false, bool is_flag = false);
    /// @}

    /// @name 解析
    /// @{

    /// @brief 解析命令行参数（标准接口）
    /// @param argc 参数个数
    /// @param argv 参数数组
    /// @return true 解析成功，false 解析失败（--help/--version 或错误）
    bool parse(int argc, const char* argv[]);

    /// @brief 解析命令行参数（测试重载）
    ///
    /// 内部将 std::vector<std::string> 拆解为 argc/argv 后转调主解析接口。
    ///
    /// @param args 命令行参数列表，args[0] 为程序名
    /// @return true 解析成功，false 解析失败
    bool parse(const std::vector<std::string>& args);
    /// @}

    /// @name 查询接口
    /// @{

    /// @brief 获取解析后的子命令名
    /// @return 子命令名字符串引用，尚未解析时返回空字符串引用
    [[nodiscard]] const std::string& sub_command_name() const noexcept;

    /// @brief 检查选项是否存在
    /// @param name 选项的长选项名（不含 -- 前缀）
    /// @return true 选项已提供
    [[nodiscard]] bool has_option(const std::string& name) const noexcept;

    /// @brief 获取选项值
    /// @param name 选项的长选项名（不含 -- 前缀）
    /// @return 选项值字符串引用，未找到时返回空字符串引用
    [[nodiscard]] const std::string& get_option(
        const std::string& name) const noexcept;

    /// @brief 获取位置参数列表
    /// @return 位置参数列表的常引用
    [[nodiscard]] const std::vector<std::string>&
        positional_args() const noexcept;
    /// @}

    /// @name 帮助与版本
    /// @{

    /// @brief 输出帮助信息到 std::cout
    ///
    /// 格式：应用名 + 版本 + 子命令列表 + 当前子命令选项列表。
    void print_help() const noexcept;

    /// @brief 输出版本信息到 std::cout
    void print_version() const noexcept;
    /// @}

private:
    class Impl;

    /// @brief PIMPL 句柄，隐藏实现细节
    std::unique_ptr<Impl> m_impl;
};

} // namespace cli
} // namespace libre_bio

#endif // LIBRE_BIO_CLI_APP_H_
