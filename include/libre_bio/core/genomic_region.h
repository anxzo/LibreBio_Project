/// @file genomic_region.h
/// @brief 基因组区域模块 — 带元数据的基因组区域类型定义
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// GenomicRegion 是带元数据（名称、评分、自定义属性）的基因组区间。
/// 它通过组合（composition）而非继承来持有 GenomicInterval，
/// 遵循 MISRA C++:2023 Rule 11.0.1 对继承层次最小化的要求。
///
/// @see doc/detailed-design/m05_genomic_region.md
/// @see genomic_interval.h

#ifndef LIBRE_BIO_CORE_GENOMIC_REGION_H_
#define LIBRE_BIO_CORE_GENOMIC_REGION_H_

#include "libre_bio/core/genomic_interval.h"

#include <map>
#include <string>

namespace libre_bio {

/// @brief 带元数据的基因组区域
///
/// 对应 BED/GFF 格式中的行记录。组合 GenomicInterval 并附加名称、评分
/// 和自定义属性键值对。
///
/// @details
/// ## 设计意图
/// - **组合而非继承**：GenomicRegion 的语义是"带标签的区域"，不是"一种特殊的区间"。
///   组合允许 GenomicInterval 独立使用（如 IntervalTree 只需要区间，不需要元数据）。
/// - **接口正交**：不暴露 GenomicInterval 的全部接口，调用方通过 interval() 获取
///   底层区间对象后进行操作。
/// - **有序属性**：使用 std::map 保证属性按 key 字典序遍历，利于格式兼容性和可复现性。
///
/// ## 属性存储
/// 使用 std::map（红黑树，有序）而非 std::unordered_map，原因：
/// 1. GFF 格式的 attributes 列通常按固定顺序排列
/// 2. 有序输出利于格式兼容性和可复现性
/// 3. 属性数量通常较小（< 50 对），红黑树的常数因子差异可忽略
///
/// ## 线程安全性
/// - GenomicRegion 构造后不可修改区间字段（name/score/interval）。
/// - 属性集合 m_attributes 可通过 set_attribute 修改，多线程写需外部同步。
/// - 多线程只读访问（get_attribute / attributes）无需同步。
///
/// ## 使用示例
/// @code
/// #include "libre_bio/core/genomic_region.h"
///
/// libre_bio::GenomicInterval iv("chr1", 0, 100);
/// libre_bio::GenomicRegion region(iv, "gene1", 0.05);
///
/// region.set_attribute("gene_type", "protein_coding");
/// const std::string* val = region.get_attribute("gene_type");
/// @endcode
class GenomicRegion {
public:
    /// @brief 构造基因组区域
    /// @param interval 基因组区间
    /// @param name 区域名称 / 标识符
    /// @param score 数值评分，默认 0.0
    GenomicRegion(GenomicInterval interval, std::string name,
                  double score = 0.0);

    /// @name 属性访问器
    /// @{

    /// @brief 获取底层基因组区间
    /// @return GenomicInterval 的常量引用
    [[nodiscard]] const GenomicInterval& interval() const noexcept;

    /// @brief 获取区域名称
    /// @return 名称字符串的常量引用
    [[nodiscard]] const std::string& name() const noexcept;

    /// @brief 获取评分
    /// @return 数值评分
    [[nodiscard]] double score() const noexcept;

    /// @}

    /// @name 自定义属性存取
    /// @{

    /// @brief 设置自定义属性
    /// @param key 属性键名
    /// @param value 属性值
    ///
    /// @note 若 key 已存在则静默覆盖（std::map 默认 insert_or_assign 语义）。
    void set_attribute(const std::string& key, const std::string& value);

    /// @brief 按 key 查找自定义属性
    /// @param key 属性键名
    /// @return 指向属性值的指针；未找到返回 nullptr
    [[nodiscard]] const std::string* get_attribute(
        const std::string& key) const noexcept;

    /// @brief 获取全部自定义属性
    /// @return 属性 map 的常量引用（按 key 字典序排列）
    [[nodiscard]] const std::map<std::string, std::string>&
        attributes() const noexcept;

    /// @}

private:
    GenomicInterval m_interval;
    std::string m_name;
    double m_score;
    std::map<std::string, std::string> m_attributes;
};

} // namespace libre_bio

#endif // LIBRE_BIO_CORE_GENOMIC_REGION_H_
