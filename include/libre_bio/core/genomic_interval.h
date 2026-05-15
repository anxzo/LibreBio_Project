/// @file genomic_interval.h
/// @brief 基因组区间模块 — 核心数据类型定义
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 表示基因组上的一个坐标区间，采用 0-based 半开区间 [start, end)。
/// 提供区间之间的关系判断（重叠、包含、距离）和集合运算（交集）。
///
/// @see doc/detailed-design/m04_genomic_interval.md

#ifndef LIBRE_BIO_CORE_GENOMIC_INTERVAL_H_
#define LIBRE_BIO_CORE_GENOMIC_INTERVAL_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace libre_bio {

/// @brief 链方向枚举
///
/// 使用强类型枚举（enum class）以避免隐式整型转换，
/// 符合 MISRA C++:2023 Rule 8.0.1。
enum class Strand : uint8_t {
    kForward = 0,   ///< + 正链 / 正义链
    kReverse = 1,   ///< - 反链 / 反义链
    kUnknown = 2    ///< . 未知 / 不适用
};

/// @brief 基因组坐标区间
///
/// 表示基因组上的一个坐标区间，采用 0-based 半开区间 [start, end)。
/// I/O 层负责格式间的坐标转换（如 GFF 的 1-based 全闭区间）。
///
/// @details
/// ## 设计意图
/// - 构造后不可修改，不可变值对象。
/// - 区间比较按 (chrom, start) 字典序，遵循生物信息学领域标准排序方式。
/// - 跨染色体的区间关系判断统一返回 false（overlaps/contains）或 INT64_MAX（distance）。
///
/// ## 坐标范围
/// - m_start: [0, m_end)，即 m_start < m_end
/// - m_end: (m_start, 2³² - 1]
/// - 使用 uint32_t 上限约 4.3 Gb，覆盖 >99.9% 使用场景。
///
/// ## 线程安全性
/// - 只读对象，多线程同时读取无需同步。
///
/// ## 使用示例
/// @code
/// #include "libre_bio/core/genomic_interval.h"
///
/// libre_bio::GenomicInterval a("chr1", 0, 100);
/// libre_bio::GenomicInterval b("chr1", 50, 150);
///
/// bool ov = a.overlaps(b);              // true
/// libre_bio::GenomicInterval c = a.intersect(b);  // [50, 100)
/// bool ct = a.contains(b);              // false
/// int64_t d = a.distance(b);            // 0 (重叠)
/// @endcode
class GenomicInterval {
public:
    /// @brief 构造基因组区间
    /// @param chrom 染色体名称，如 "chr1", "X", "MT"
    /// @param start 起始位置 — 0-based，包含
    /// @param end 终止位置 — 0-based，不包含
    /// @param strand 链方向，默认为 kUnknown
    ///
    /// @note 构造函数不校验 m_start < m_end，由调用方保证合法性。
    GenomicInterval(std::string chrom, uint32_t start,
                    uint32_t end,
                    Strand strand = Strand::kUnknown);

    /// @name 属性访问器
    /// @{

    /// @brief 获取染色体名称
    /// @return 染色体名称的常量引用
    [[nodiscard]] const std::string& chrom() const noexcept;

    /// @brief 获取起始位置
    /// @return 0-based 起始坐标
    [[nodiscard]] uint32_t start() const noexcept;

    /// @brief 获取终止位置
    /// @return 0-based 终止坐标
    [[nodiscard]] uint32_t end() const noexcept;

    /// @brief 获取链方向
    /// @return 链方向枚举值
    [[nodiscard]] Strand strand() const noexcept;

    /// @}

    /// @name 区间运算
    /// @{

    /// @brief 获取区间长度
    /// @return end - start
    [[nodiscard]] uint32_t length() const noexcept;

    /// @brief 判断与另一区间是否重叠
    /// @param other 待比较区间
    /// @return 同染色体且有交集时返回 true
    ///
    /// @note 紧邻但不重叠的区间 [0, 100) 和 [100, 200) 返回 false。
    [[nodiscard]] bool overlaps(const GenomicInterval& other) const noexcept;

    /// @brief 计算与另一区间的交集
    /// @param other 待求交区间
    /// @return 交集区间；无交集时返回空区间 (chrom, 0, 0)
    ///
    /// @note 返回区间的 strand 取自身的 strand。
    /// @note 调用方可通过 length() == 0 判断是否为空区间。
    [[nodiscard]] GenomicInterval intersect(
        const GenomicInterval& other) const;

    /// @brief 判断是否包含另一区间
    /// @param other 待判断区间
    /// @return 同染色体且完全包含时返回 true
    ///
    /// @note 自我包含 [0, 100).contains([0, 100)) 返回 true。
    [[nodiscard]] bool contains(const GenomicInterval& other) const noexcept;

    /// @brief 计算与另一区间的距离
    /// @param other 待计算区间
    /// @return 重叠返回 0，分离返回最短间隔距离，不同染色体返回 INT64_MAX
    [[nodiscard]] int64_t distance(
        const GenomicInterval& other) const noexcept;

    /// @}

    /// @name 比较运算符
    /// @{

    /// @brief 小于比较 — 按 (chrom, start) 字典序
    /// @param other 待比较区间
    /// @return chrom 字典序较小时返回 true；chrom 相同且 start 较小时返回 true
    bool operator<(const GenomicInterval& other) const noexcept;

    /// @brief 相等比较 — 全部四个字段相等
    /// @param other 待比较区间
    /// @return chrom, start, end, strand 全部相等时返回 true
    bool operator==(const GenomicInterval& other) const noexcept;

    /// @}

private:
    std::string m_chrom;   ///< 染色体名称
    uint32_t m_start;      ///< 起始位置 (0-based, 包含)
    uint32_t m_end;        ///< 终止位置 (0-based, 不包含)
    Strand m_strand;       ///< 链方向
};

} // namespace libre_bio

#endif // LIBRE_BIO_CORE_GENOMIC_INTERVAL_H_
