/// @file genomic_region.cpp
/// @brief 基因组区域模块 — GenomicRegion 类实现
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.

#include "libre_bio/core/genomic_region.h"

#include <map>
#include <string>
#include <utility>

namespace libre_bio {

// ============================================================================
// 构造函数 — 移动语义
// ============================================================================
GenomicRegion::GenomicRegion(GenomicInterval interval, std::string name,
                             const double score)
    : m_interval(std::move(interval))
    , m_name(std::move(name))
    , m_score(score)
{
}

// ============================================================================
// 属性访问器
// ============================================================================
const GenomicInterval& GenomicRegion::interval() const noexcept
{
    return m_interval;
}

const std::string& GenomicRegion::name() const noexcept
{
    return m_name;
}

double GenomicRegion::score() const noexcept
{
    return m_score;
}

// ============================================================================
// 自定义属性存取
// ============================================================================

void GenomicRegion::set_attribute(const std::string& key,
                                  const std::string& value)
{
    m_attributes[key] = value;
}

const std::string* GenomicRegion::get_attribute(
    const std::string& key) const noexcept
{
    auto it = m_attributes.find(key);
    if (it != m_attributes.end()) {
        return &(it->second);
    }
    return nullptr;
}

const std::map<std::string, std::string>&
    GenomicRegion::attributes() const noexcept
{
    return m_attributes;
}

} // namespace libre_bio
