/// @file quality_score.cpp
/// @brief Phred 质量分数模块 — QualityScore 类实现
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.

#include "libre_bio/core/quality_score.h"

#include <algorithm>
#include <numeric>

namespace libre_bio {

namespace {

// ============================================================================
// 编码方案常量 — 使用 constexpr 避免 magic number，符合 MISRA C++:2023 Rule 5.0.1
// ============================================================================

// Sanger / Illumina 1.8+ 使用 ASCII 33 作为 Phred 0 的偏移量。
// 这是现代测序的事实标准（Phred+33）。
constexpr uint8_t kSangerOffset = 33;

// Illumina 1.3–1.7 使用 ASCII 64 作为 Phred 0 的偏移量（Phred+64）。
// 该方案已过时，但仍需支持旧数据的兼容读取。
constexpr uint8_t kIllumina13Offset = 64;

// ASCII 33 是 Sanger 方案的最低合法值（Phred 0 = '!'）。
// 任何低于此值的 ASCII 均不属于已知编码范围。
constexpr uint8_t kSangerMinAscii = 33;

// ASCII 64 是 Illumina 1.3+ 方案的最低合法值（Phred 0 = '@'）。
// 当 min_val 恰好等于 64 时，可以唯一判定为 Illumina13 方案；
// 若 min_val > 64，则与 Sanger 的高分数区间重叠，无法区分。
constexpr uint8_t kIllumina13MinAscii = 64;

// Q30 是行业常用的质量基准阈值：P_error ≤ 0.001（错误率 0.1%）。
// Phred 公式：Q = -10 × log₁₀(P_error)，故 Q=30 时 P_error = 10⁻³。
constexpr uint8_t kQ30Threshold = 30;

} // namespace

// ============================================================================
// 构造函数 — 单参数版本自动检测编码，双参数版本跳过检测
// ============================================================================
QualityScore::QualityScore(std::vector<uint8_t> scores)
    : m_scores(std::move(scores))
    , m_encoding(detect_encoding(m_scores))
{
    // 使用 std::move 避免拷贝：调用方传入右值时零开销，
    // 传入左值时由调用方决定是否拷贝。
}

QualityScore::QualityScore(std::vector<uint8_t> scores, PhredEncoding encoding)
    : m_scores(std::move(scores))
    , m_encoding(encoding)
{
    // 跳过检测的构造函数适用于以下场景：
    // 1. 调用方已通过元数据（如 FASTQ 文件头注释）确定编码；
    // 2. 调用方希望强制以特定偏移量解读分数（如测试用例）；
    // 3. 避免 detect_encoding 的 O(n) 开销。
}

// ============================================================================
// 属性访问器 — 返回 const 引用以支持不可变设计
// ============================================================================
const std::vector<uint8_t>& QualityScore::scores() const noexcept
{
    return m_scores;
}

size_t QualityScore::size() const noexcept
{
    return m_scores.size();
}

PhredEncoding QualityScore::encoding() const noexcept
{
    return m_encoding;
}

// ============================================================================
// detect_encoding — 基于 ASCII 最小值的启发式编码检测
// ============================================================================
PhredEncoding QualityScore::detect_encoding(
    const std::vector<uint8_t>& scores) noexcept
{
    // 空序列无法推断编码，返回 kUnknown。
    // 调用方通过 offset() 方法获取保守的默认偏移量（Sanger 33）。
    if (scores.empty()) {
        return PhredEncoding::kUnknown;
    }

    // 使用 min_element 而非 minmax_element 的原因：
    // 编码检测仅需最小值——只要 min_val 落入某个编码的范围即可判定，
    // 无需最大值。单次遍历找最小值足以，无需两次遍历的开销。
    const uint8_t min_val = *std::min_element(scores.begin(), scores.end());

    // 低于 33 的 ASCII 值不属于任何已知 Phred 编码方案。
    // 例如纯二进制数据或损坏的 FASTQ 文件。
    if (min_val < kSangerMinAscii) {
        return PhredEncoding::kUnknown;
    }

    // 33–63 区间：明确属于 Sanger 方案（含 Illumina18）。
    // 这两个方案的 offset 相同，无法也不需区分，
    // 因为两者对 Phred 值的解读完全一致。
    if (min_val < kIllumina13MinAscii) {
        return PhredEncoding::kSanger;
    }

    // min_val == 64：Illumina 1.3+ 的 Phred 0 边界特征。
    // Sanger 方案下 Phred 0 = ASCII 33 (= '!')，不可能产生 64 的最小值，
    // 因此 min_val == 64 可以唯一判定为 Illumina13 方案。
    if (min_val == kIllumina13MinAscii) {
        return PhredEncoding::kIllumina13;
    }

    // min_val > 64：无法区分 Sanger 与 Illumina13 的高分数区间。
    // 例如全部 Phred 40 = ASCII 73 的数据，Sanger 和 Illumina13 均合法，
    // 保守假设为现代通用标准 Sanger（Phred+33）。
    return PhredEncoding::kSanger;
}

// ============================================================================
// offset — 编码方案到 ASCII 偏移量的映射
// ============================================================================
uint8_t QualityScore::offset() const noexcept
{
    // 对 kUnknown 保守使用 Sanger 偏移量 33。
    // 理由：Sanger（Phred+33）是现代测序的事实标准，
    // 在无法确定编码时使用此偏移量，产生可读结果的可能性最高。
    switch (m_encoding) {
    case PhredEncoding::kSanger:
    case PhredEncoding::kIllumina18:
        return kSangerOffset;
    case PhredEncoding::kIllumina13:
    case PhredEncoding::kIllumina15:
        return kIllumina13Offset;
    default:
        return kSangerOffset;
    }
}

// ============================================================================
// average_score — 使用 double 累加以保证长序列的数值精度
// ============================================================================
double QualityScore::average_score() const noexcept
{
    if (m_scores.empty()) {
        return 0.0;
    }

    const uint8_t off = offset();

    // 使用 double 而非 float 累加的原因：
    // 测序数据可能包含数亿条读段（reads），每条 100–300 bp，
    // float 有效精度约 7 位十进制数，在累加 10⁸ 量级时可能产生舍入误差。
    // double 有效精度约 15 位，可以安全处理 10¹² 量级的累加。
    // 符合 MISRA C++:2023 Dir 0.3.1 对浮点精度使用的建议。
    double sum = 0.0;
    for (const uint8_t qv : m_scores) {
        sum += static_cast<double>(qv - off);
    }

    return sum / static_cast<double>(m_scores.size());
}

// ============================================================================
// q30_ratio — 统计 Phred ≥ 30 的碱基占比
// ============================================================================
double QualityScore::q30_ratio() const noexcept
{
    if (m_scores.empty()) {
        return 0.0;
    }

    const uint8_t off = offset();

    // 将 Q30 的 Phred 阈值转换为对应编码的 ASCII 阈值。
    // 例如 Sanger 方案：30 + 33 = 63（ASCII '?'），
    // 即 ASCII ≥ '?' 的碱基达到 Q30 标准。
    const uint8_t threshold = kQ30Threshold + off;
    size_t count = 0;

    for (const uint8_t qv : m_scores) {
        if (qv >= threshold) {
            ++count;
        }
    }

    // 使用 double 除法以保证精度，与 average_score 一致。
    return static_cast<double>(count) / static_cast<double>(m_scores.size());
}

// ============================================================================
// operator[] — 按需将 ASCII 转换为 Phred 整数值
// ============================================================================
uint8_t QualityScore::operator[](const size_t idx) const
{
    const uint8_t off = offset();

    // 不进行越界检查的原因：
    // - 与 STL 容器约定一致（operator[] 不做边界检查，at() 做）；
    // - 调用方通常在循环中遍历 0..size()-1，双重检查浪费 CPU 周期；
    // - MISRA 不要求对标准库行为级别的未定义操作进行额外保护。
    return m_scores[idx] - off;
}

} // namespace libre_bio
