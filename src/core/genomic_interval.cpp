/// @file genomic_interval.cpp
/// @brief 基因组区间模块 — GenomicInterval 类实现
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.

#include "libre_bio/core/genomic_interval.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace libre_bio {

// ============================================================================
// 构造函数 — 移动语义，不校验参数合法性
// ============================================================================
GenomicInterval::GenomicInterval(std::string chrom, const uint32_t start,
                                 const uint32_t end, const Strand strand)
    : m_chrom(std::move(chrom))
    , m_start(start)
    , m_end(end)
    , m_strand(strand)
{
}

// ============================================================================
// 属性访问器
// ============================================================================
const std::string& GenomicInterval::chrom() const noexcept
{
    return m_chrom;
}

uint32_t GenomicInterval::start() const noexcept
{
    return m_start;
}

uint32_t GenomicInterval::end() const noexcept
{
    return m_end;
}

Strand GenomicInterval::strand() const noexcept
{
    return m_strand;
}

uint32_t GenomicInterval::length() const noexcept
{
    return m_end - m_start;
}

// ============================================================================
// overlaps — 重叠判断
//
// 使用 max(start) < min(end) 公式判断。
// 紧邻区间 [0,100) 和 [100,200)：max(0,100)=100, min(100,200)=100, 100<100 = false ✓
// ============================================================================
bool GenomicInterval::overlaps(const GenomicInterval& other) const noexcept
{
    if (m_chrom != other.m_chrom) {
        return false;
    }

    const uint32_t max_start = std::max(m_start, other.m_start);
    const uint32_t min_end   = std::min(m_end, other.m_end);

    return max_start < min_end;
}

// ============================================================================
// intersect — 区间交集
//
// 无交集时返回空区间 (chrom, 0, 0)，调用方通过 length() == 0 判断。
// strand 取自身的 strand。
// ============================================================================
GenomicInterval GenomicInterval::intersect(
    const GenomicInterval& other) const
{
    if (!overlaps(other)) {
        return GenomicInterval(m_chrom, 0, 0);
    }

    const uint32_t new_start = std::max(m_start, other.m_start);
    const uint32_t new_end   = std::min(m_end, other.m_end);

    return GenomicInterval(m_chrom, new_start, new_end, m_strand);
}

// ============================================================================
// contains — 包含判断
//
// 自我包含 [0,100).contains([0,100)) → true。
// ============================================================================
bool GenomicInterval::contains(const GenomicInterval& other) const noexcept
{
    if (m_chrom != other.m_chrom) {
        return false;
    }

    return (m_start <= other.m_start) && (m_end >= other.m_end);
}

// ============================================================================
// distance — 区间距离
//
// 不同染色体 → INT64_MAX
// 重叠 → 0
// other 在右侧 → other.m_start - m_end
// other 在左侧 → m_start - other.m_end
//
// 注意: [0,100) 和 [100,200) 不重叠但距离为 0（紧邻）。
// ============================================================================
int64_t GenomicInterval::distance(
    const GenomicInterval& other) const noexcept
{
    if (m_chrom != other.m_chrom) {
        return INT64_MAX;
    }

    if (overlaps(other)) {
        return 0;
    }

    if (m_end <= other.m_start) {
        return static_cast<int64_t>(other.m_start) - static_cast<int64_t>(m_end);
    }

    return static_cast<int64_t>(m_start) - static_cast<int64_t>(other.m_end);
}

// ============================================================================
// 比较运算符
// ============================================================================
bool GenomicInterval::operator<(const GenomicInterval& other) const noexcept
{
    if (m_chrom != other.m_chrom) {
        return m_chrom < other.m_chrom;
    }
    return m_start < other.m_start;
}

bool GenomicInterval::operator==(const GenomicInterval& other) const noexcept
{
    return (m_chrom == other.m_chrom) &&
           (m_start == other.m_start) &&
           (m_end == other.m_end) &&
           (m_strand == other.m_strand);
}

} // namespace libre_bio
