/// @file sequence.h
/// @brief 生物序列模块 — 核心数据类型定义
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 提供生物序列（DNA / RNA / Protein）的不可变表示及相关操作。
/// 本模块为 LibreBio 核心数据类型层的基础组件，仅依赖 STL。
///
/// @see doc/detailed-design/m01_sequence.md

#ifndef LIBRE_BIO_CORE_SEQUENCE_H_
#define LIBRE_BIO_CORE_SEQUENCE_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace libre_bio {

/// @brief 生物序列字母表类型
///
/// 使用强类型枚举（enum class）以避免隐式整型转换，
/// 符合 MISRA C++:2023 Rule 8.0.1。
enum class Alphabet : uint8_t {
    kDNA     = 0,   ///< DNA 字母表：A, T, C, G, N
    kRNA     = 1,   ///< RNA 字母表：A, U, C, G, N
    kProtein = 2    ///< 蛋白质字母表：20 种标准氨基酸 + *
};

/// @brief 不可变生物序列对象
///
/// 表示一条生物序列（DNA / RNA / Protein），并提供属性查询和基本操作。
///
/// @details
/// ## 设计意图
/// - Sequence 构造后不可修改，所有变换操作返回新对象。
/// - 构造函数不执行校验，由调用方按需调用 validate()，
///   以避免从文件流式读取时的双重遍历开销。
///
/// ## 线程安全性
/// - 只读对象，多线程同时读取无需同步。
/// - 非线程安全的拷贝：拷贝过程中其他线程读取源对象是安全的。
///
/// ## 使用示例
/// @code
/// #include "libre_bio/core/sequence.h"
///
/// libre_bio::Sequence seq("chr1", "Human chromosome 1",
///                          "ATCGATCG", libre_bio::Alphabet::kDNA);
///
/// size_t len = seq.length();               // 8
/// double gc  = seq.gc_content();           // 0.5
/// bool valid = seq.validate();             // true
///
/// libre_bio::Sequence rc = seq.reverse_complement();  // "CGATCGAT"
/// libre_bio::Sequence sub = seq.sub_seq(0, 4);         // "ATCG"
/// @endcode
class Sequence {
public:
    /// @brief 构造一条生物序列
    /// @param id 序列标识符，如 "chr1", "seq001"
    /// @param description 序列描述，如 "Human chromosome 1"
    /// @param seq 序列字符串，建议使用大写字符
    /// @param alphabet 字母表类型，决定字符集校验规则
    ///
    /// @note 构造函数不校验 seq 字符是否合法，
    ///       请按需调用 validate() 进行校验。
    Sequence(std::string id, std::string description,
             std::string seq, Alphabet alphabet);

    /// @name 属性访问器
    /// @{

    /// @brief 获取序列标识符
    /// @return 序列 ID 的常量引用
    [[nodiscard]] const std::string& id() const noexcept;

    /// @brief 获取序列描述
    /// @return 序列描述的常量引用
    [[nodiscard]] const std::string& description() const noexcept;

    /// @brief 获取序列字符串
    /// @return 序列字符串的常量引用
    [[nodiscard]] const std::string& seq() const noexcept;

    /// @brief 获取字母表类型
    /// @return 当前序列的字母表枚举值
    [[nodiscard]] Alphabet alphabet() const noexcept;

    /// @}

    /// @name 基本操作
    /// @{

    /// @brief 获取序列长度（碱基/氨基酸数量）
    /// @return 序列字符串的字符数
    [[nodiscard]] size_t length() const noexcept;

    /// @brief 截取子序列
    /// @param start 起始位置（0-based）
    /// @param count 期望截取长度
    /// @return 新 Sequence 对象，共享原 id 和 description
    ///
    /// @note 若 @p start 越界则返回空序列；
    ///       若 @p start + count 超出序列末尾则自动修正到末尾。
    [[nodiscard]] Sequence sub_seq(size_t start, size_t count) const;

    /// @brief 获取反向互补序列
    /// @return 新 Sequence 对象，description 追加 " reverse complement"
    ///
    /// @note 仅 DNA 和 RNA 支持反向互补；
    ///       Protein 序列调用此方法返回原序列副本。
    ///       内部使用静态预计算查找表实现 O(n) 时间复杂度。
    [[nodiscard]] Sequence reverse_complement() const;

    /// @brief 计算 GC 含量
    /// @return [0.0, 1.0] 范围的 GC 比例（含大小写）
    ///
    /// @note 仅 DNA 和 RNA 支持 GC 含量计算；
    ///       Protein 或空序列返回 0.0。
    [[nodiscard]] double gc_content() const noexcept;

    /// @brief 校验序列字符合法性
    /// @return 所有字符均属于对应字母表时返回 true
    ///
    /// @note 内部使用静态预计算布尔查找表，
    ///       单字符校验为 O(1)，整体复杂度 O(n)。
    [[nodiscard]] bool validate() const noexcept;

    /// @}

private:
    std::string m_id;            // 序列标识符
    std::string m_description;   // 序列描述文本
    std::string m_seq;           // 序列字符串（大写存储）
    Alphabet m_alphabet;         // 字母表类型
};

} // namespace libre_bio

#endif // LIBRE_BIO_CORE_SEQUENCE_H_
