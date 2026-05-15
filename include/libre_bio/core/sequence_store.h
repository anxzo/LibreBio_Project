/// @file sequence_store.h
/// @brief 序列容器模块 — 核心数据类型定义
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 批量管理 Sequence 对象，提供查找、迭代和统计功能。
/// 本模块为 LibreBio 核心数据类型层组件，依赖 M01 (Sequence)。
///
/// @see doc/detailed-design/m03_sequence_store.md

#ifndef LIBRE_BIO_CORE_SEQUENCE_STORE_H_
#define LIBRE_BIO_CORE_SEQUENCE_STORE_H_

#include <cstddef>
#include <string>
#include <vector>

namespace libre_bio {

class Sequence;

/// @brief 序列容器 — 批量管理与统计
///
/// 以 std::vector<Sequence> 为内部存储，提供按 ID 查找、迭代器遍历、
/// 以及 total_bases / N50 等生物信息学统计指标。
///
/// @details
/// ## 设计意图
/// - 主要用于批量存储和统计，主要操作是遍历而非单点查找。
/// - 内部使用单一 vector 存储，遵循概要设计的零依赖/最小复杂度原则。
/// - 不支持自定义顺序，插入顺序即存储顺序。
///
/// ## 线程安全性
/// - 只读操作（count / total_bases / n50 / find_by_id / 迭代器）可在多线程中安全并发。
/// - 写操作（add / clear / reserve）不是线程安全的，需调用方同步。
///
/// ## 使用示例
/// @code
/// #include "libre_bio/core/sequence.h"
/// #include "libre_bio/core/sequence_store.h"
///
/// libre_bio::SequenceStore store;
/// store.reserve(1000);
///
/// store.add(libre_bio::Sequence("chr1", "", "ATCG", libre_bio::Alphabet::kDNA));
/// store.add(libre_bio::Sequence("chr2", "", "GCTA", libre_bio::Alphabet::kDNA));
///
/// size_t cnt = store.count();          // 2
/// size_t bases = store.total_bases();  // 8
/// size_t n50_val = store.n50();        // 4
///
/// const libre_bio::Sequence* s = store.find_by_id("chr1");
/// if (s) { /* found */ }
///
/// for (const auto& seq : store) {
///     // 遍历所有序列
/// }
/// @endcode
class SequenceStore {
public:
    SequenceStore() = default;

    /// @brief 添加单条序列
    /// @param seq 待添加的 Sequence（按值传参，支持移动语义）
    ///
    /// @note 使用 sink argument idiom：调用方传右值时零拷贝，
    ///       传左值时由调用方决定是否拷贝。
    void add(Sequence seq);

    /// @brief 批量添加序列
    /// @param seqs 待添加的 Sequence 向量（按值传参，支持移动语义）
    ///
    /// @note 若当前容器为空且 seqs 容量充足，直接移动赋值避免逐元素拷贝；
    ///       否则预分配后批量插入。
    void add(std::vector<Sequence> seqs);

    /// @name 查询操作
    /// @{

    /// @brief 按序列 ID 查找
    /// @param id 序列标识符
    /// @return 找到时返回指向该序列的指针，未找到返回 nullptr
    ///
    /// @note 返回原始指针（非 owning），调用方不能持有超过 SequenceStore 生命周期。
    ///       线性查找 O(n)。
    [[nodiscard]] const Sequence* find_by_id(
        const std::string& id) const noexcept;

    /// @brief 获取序列数量
    /// @return 容器中存储的序列总数
    [[nodiscard]] size_t count() const noexcept;

    /// @brief 检查容器是否为空
    /// @return 容器无序列时返回 true
    [[nodiscard]] bool empty() const noexcept;

    /// @}

    /// @name 统计指标
    /// @{

    /// @brief 计算所有序列的碱基总数
    /// @return 所有序列 length() 之和；空容器返回 0
    ///
    /// @note 复杂度 O(n)。使用 size_t 累加，不检查溢出
    ///       （人类基因组 3e9 bases 远小于 size_t 最大值 ~1.8e19）。
    [[nodiscard]] size_t total_bases() const noexcept;

    /// @brief 计算 N50 统计值
    /// @return N50 值（碱基数）；空容器返回 0
    ///
    /// @details
    /// N50 是序列组装质量的核心指标，定义如下：
    /// 1. 将所有序列按长度从大到小排列。
    /// 2. 计算总长度 total，目标值 target = ceil(total / 2)。
    /// 3. 从最长序列开始累加，当累加长度达到 target 时，
    ///    当前序列的长度即为 N50。
    ///
    /// N50 越大表示组装质量越高。
    ///
    /// @note 复杂度 O(n log n)（排序主导）。
    ///       选择完整排序而非部分排序，确保对不均匀分布序列结果的确定性。
    [[nodiscard]] size_t n50() const noexcept;

    /// @}

    /// @name 迭代器
    /// @{

    /// @brief 获取只读起始迭代器
    [[nodiscard]] auto begin() const noexcept
        -> std::vector<Sequence>::const_iterator;

    /// @brief 获取只读终止迭代器
    [[nodiscard]] auto end() const noexcept
        -> std::vector<Sequence>::const_iterator;

    /// @brief 获取可写起始迭代器
    [[nodiscard]] auto begin() noexcept
        -> std::vector<Sequence>::iterator;

    /// @brief 获取可写终止迭代器
    [[nodiscard]] auto end() noexcept
        -> std::vector<Sequence>::iterator;

    /// @}

    /// @name 内存管理
    /// @{

    /// @brief 预分配内部存储容量
    /// @param capacity 预计存储的序列数量
    ///
    /// @note 在加载大型 FASTA 文件前调用可减少 vector 的多次重分配。
    void reserve(size_t capacity);

    /// @brief 清空所有序列并释放内存
    void clear() noexcept;

    /// @}

private:
    std::vector<Sequence> m_sequences;   // 内部序列存储，插入顺序
};

} // namespace libre_bio

#endif // LIBRE_BIO_CORE_SEQUENCE_STORE_H_
