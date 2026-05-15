/// @file quality_score.h
/// @brief Phred 质量分数模块 — 核心数据类型定义
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 存储测序碱基的 Phred 质量分数序列，自动检测编码方案（Sanger / Illumina），
/// 提供平均质量、Q30 比例等统计指标。
///
/// @see doc/detailed-design/m02_quality_score.md

#ifndef LIBRE_BIO_CORE_QUALITY_SCORE_H_
#define LIBRE_BIO_CORE_QUALITY_SCORE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace libre_bio {

/// @brief Phred 质量分数的编码方案
///
/// 在 FASTQ 文件中，Phred 质量分数被编码为单个 ASCII 字符，偏移量因平台而异。
/// 使用强类型枚举（enum class）以避免隐式整型转换，
/// 符合 MISRA C++:2023 Rule 8.0.1。
enum class PhredEncoding : uint8_t {
    kSanger      = 0,   ///< Phred+33 offset，现代通用标准（同 Illumina 1.8+）
    kIllumina13  = 1,   ///< Phred+64 offset，Illumina 1.3–1.7，分数范围 [0, 62]
    kIllumina15  = 2,   ///< Phred+64 offset，Illumina 1.5–1.7，分数范围 [2, 62]
    kIllumina18  = 3,   ///< Phred+33 offset，Illumina 1.8+（与 Sanger 相同）
    kUnknown     = 4    ///< 无法确定编码方案时使用，默认偏移量 33
};

/// @brief Phred 质量分数序列
///
/// 存储测序碱基的 Phred 质量分数（ASCII 原始值），自动检测编码方案，
/// 并提供按需的 Phred 整数值访问及统计计算。
///
/// @details
/// ## 设计意图
/// - 存储原始 ASCII 值而非 Phred 整数值：
///   ① 避免信息丢失，原始字节可直接写回 FASTQ 文件；
///   ② 编码检测基于 ASCII 范围，存储原始值使检测逻辑清晰；
///   ③ Phred 值的转换在访问时按需计算（偏移量取决于 encoding）。
/// - 自动检测编码方案，同时也提供显式指定 encoding 的构造函数。
///
/// ## 编码检测策略
/// - 基于 ASCII 最小值推断编码方案（min_val < 33 → Unknown，
///   33–63 → Sanger，== 64 → Illumina13，> 64 → Sanger）。
/// - Sanger 与 Illumina18 不可区分（偏移量相同），统一返回 kSanger。
/// - Illumina13 与 Illumina15 不完全可靠地区分，统一返回 kIllumina13。
/// - 无法确定时保守假设为 Sanger 方案（现代通用标准）。
///
/// ## 线程安全性
/// - 只读对象，多线程同时读取无需同步。
/// - 非线程安全的拷贝：拷贝过程中其他线程读取源对象是安全的。
///
/// ## 使用示例
/// @code
/// #include "libre_bio/core/quality_score.h"
///
/// std::vector<uint8_t> scores = {'I', 'I', 'I', 'I'};
///
/// // 自动检测编码
/// libre_bio::QualityScore qs(scores);
/// auto enc = qs.encoding();          // PhredEncoding::kSanger
/// double avg = qs.average_score();   // 40.0
/// double q30 = qs.q30_ratio();       // 1.0
/// uint8_t phred = qs[0];             // 40
///
/// // 显式指定编码（跳过检测）
/// libre_bio::QualityScore qs2(scores, libre_bio::PhredEncoding::kIllumina13);
/// @endcode
class QualityScore {
public:
    /// @brief 构造质量分数序列并自动检测编码
    /// @param scores ASCII 编码的 Phred 质量值（将被移动）
    ///
    /// @note 内部调用 detect_encoding(scores) 推断编码方案。
    explicit QualityScore(std::vector<uint8_t> scores);

    /// @brief 构造质量分数序列并显式指定编码
    /// @param scores ASCII 编码的 Phred 质量值（将被移动）
    /// @param encoding 编码方案，跳过自动检测
    ///
    /// @note 适用于调用方已明确知道编码方案的场景。
    QualityScore(std::vector<uint8_t> scores, PhredEncoding encoding);

    /// @name 属性访问器
    /// @{

    /// @brief 获取原始 ASCII 质量值容器
    /// @return scores 的常量引用
    [[nodiscard]] const std::vector<uint8_t>& scores() const noexcept;

    /// @brief 获取质量分数数量
    /// @return 与序列碱基数等长的分数数量
    [[nodiscard]] size_t size() const noexcept;

    /// @brief 获取检测到的编码方案
    /// @return 构造时确定或自动检测的编码枚举值
    [[nodiscard]] PhredEncoding encoding() const noexcept;

    /// @}

    /// @name 统计指标
    /// @{

    /// @brief 计算平均 Phred 质量分数
    /// @return 所有分数的算术均值；空序列时返回 0.0
    ///
    /// @note 使用 double 累加避免大数组的 float 精度损失，
    ///       符合 MISRA C++:2023 Dir 0.3.1 对浮点精度使用的建议。
    [[nodiscard]] double average_score() const noexcept;

    /// @brief 计算 Q30 比例
    /// @return 分数 ≥ 30 的碱基占比 [0.0, 1.0]；空序列时返回 0.0
    ///
    /// @note Q30 表示碱基被测错的概率 ≤ 0.1%，是行业常用的质量基准。
    ///       例如 Q30 比例 0.85 表示 85% 的碱基达到 Q30 标准。
    [[nodiscard]] double q30_ratio() const noexcept;

    /// @}

    /// @name 元素访问
    /// @{

    /// @brief 按索引访问 Phred 整数值
    /// @param idx 元素索引
    /// @return 解码后的 Phred 整数值（原始 ASCII - 偏移量）
    ///
    /// @note 不进行越界检查，调用方负责保证 idx 在有效范围内。
    [[nodiscard]] uint8_t operator[](size_t idx) const;

    /// @}

    /// @brief 静态编码检测方法
    /// @param scores ASCII 编码的 Phred 质量值
    /// @return 推断的编码方案
    ///
    /// @details
    /// 基于 ASCII 最小值推断编码方案：
    /// - min_val < 33  → kUnknown（低于 Sanger 最低值）
    /// - 33 ≤ min_val < 64  → kSanger（含 Illumina18）
    /// - min_val == 64 → kIllumina13（Illumina 1.3+ 的 Phred 0 边界特征）
    /// - min_val > 64  → kSanger（高分数区间与 Sanger 重叠，保守假设）
    ///
    /// @note 复杂度 O(n)，其中 n = scores.size()。
    ///       不修改传入参数，标记为 noexcept。
    static PhredEncoding detect_encoding(
        const std::vector<uint8_t>& scores) noexcept;

private:
    /// @brief 根据编码方案返回 ASCII 偏移量
    /// @return kSanger/kIllumina18/kUnknown → 33，kIllumina13/kIllumina15 → 64
    ///
    /// @note 对 kUnknown 保守使用 Sanger 偏移量 33，
    ///       此为现代测序的事实标准。
    [[nodiscard]] uint8_t offset() const noexcept;

    std::vector<uint8_t> m_scores;   // 原始 ASCII 值，不做偏移转换
    PhredEncoding m_encoding;        // 检测到或指定的编码方案
};

} // namespace libre_bio

#endif // LIBRE_BIO_CORE_QUALITY_SCORE_H_
