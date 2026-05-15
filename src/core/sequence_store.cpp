/// @file sequence_store.cpp
/// @brief 序列容器模块 — SequenceStore 类实现
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.

#include "libre_bio/core/sequence_store.h"
#include "libre_bio/core/sequence.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

namespace libre_bio {

// ============================================================================
// add(Sequence) — sink argument idiom，调用方决定拷贝或移动
// ============================================================================
void SequenceStore::add(Sequence seq)
{
    // 按值传参 + std::move：调用方传右值时零拷贝，
    // 传左值时由调用方决定是否通过 std::move 移交所有权。
    m_sequences.push_back(std::move(seq));
}

// ============================================================================
// add(vector<Sequence>) — 批量插入优化
// ============================================================================
void SequenceStore::add(std::vector<Sequence> seqs)
{
    // 优化路径：当前容器为空且输入容量充足时，直接移动赋值。
    // 避免先 reserve 再逐个 move-insert 的双重操作。
    if (m_sequences.empty() && seqs.capacity() >= seqs.size()) {
        m_sequences = std::move(seqs);
        return;
    }

    // 通用路径：预分配 + 批量移动插入。
    // make_move_iterator 确保每个元素被移动而非拷贝。
    m_sequences.reserve(m_sequences.size() + seqs.size());
    m_sequences.insert(m_sequences.end(),
                       std::make_move_iterator(seqs.begin()),
                       std::make_move_iterator(seqs.end()));
}

// ============================================================================
// find_by_id — 线性查找，未找到返回 nullptr
// ============================================================================
const Sequence* SequenceStore::find_by_id(
    const std::string& id) const noexcept
{
    // 线性遍历而非哈希表查找的原因：
    // 1. SequenceStore 主要用于批量存储和统计，主要操作是遍历而非单点查找。
    // 2. 大多数生物信息学场景中序列数量在万到百万级，
    //    O(n) 线性查找约 1ms（现代 CPU），可接受。
    // 3. 避免双倍索引维护和哈希开销，遵循最小复杂度原则。
    for (size_t i = 0; i < m_sequences.size(); ++i) {
        if (m_sequences[i].id() == id) {
            return &m_sequences[i];
        }
    }
    return nullptr;
}

// ============================================================================
// count / empty — 直接委托给 vector
// ============================================================================
size_t SequenceStore::count() const noexcept
{
    return m_sequences.size();
}

bool SequenceStore::empty() const noexcept
{
    return m_sequences.empty();
}

// ============================================================================
// total_bases — 累加各序列长度
// ============================================================================
size_t SequenceStore::total_bases() const noexcept
{
    size_t total = 0;
    for (const auto& seq : m_sequences) {
        total += seq.length();
    }
    return total;
}

// ============================================================================
// n50 — 降序排列后累加至总长 50%
// ============================================================================
size_t SequenceStore::n50() const noexcept
{
    if (m_sequences.empty()) {
        return 0;
    }

    const size_t total = total_bases();

    // target = ceil(total / 2)，使用 (total + 1) / 2 避免浮点运算。
    // 符合 MISRA C++:2023 Rule 10.0.1（整数类型表达式的类型一致性）。
    const size_t target = (total + 1U) / 2U;

    // 提取所有序列长度到独立 vector。
    // 不修改 m_sequences 本身以保持线程安全的只读语义。
    std::vector<size_t> lengths;
    lengths.reserve(m_sequences.size());
    for (const auto& seq : m_sequences) {
        lengths.push_back(seq.length());
    }

    // 按降序排列以从最长序列开始累加。
    // 使用 std::greater<size_t> 实现降序：
    //   std::sort 默认升序，传入 greater 得到降序排列。
    std::sort(lengths.begin(), lengths.end(), std::greater<size_t>());

    size_t cumulative = 0;
    for (const size_t len : lengths) {
        cumulative += len;
        if (cumulative >= target) {
            return len;
        }
    }

    // 防御性：理论上不会被触发（所有长度累加必然 >= target），
    // 但保留此分支以应对整数溢出或其他未预期的运行时状态。
    return lengths.back();
}

// ============================================================================
// 迭代器 — 直接委托给 vector
// ============================================================================
auto SequenceStore::begin() const noexcept
    -> std::vector<Sequence>::const_iterator
{
    return m_sequences.begin();
}

auto SequenceStore::end() const noexcept
    -> std::vector<Sequence>::const_iterator
{
    return m_sequences.end();
}

auto SequenceStore::begin() noexcept
    -> std::vector<Sequence>::iterator
{
    return m_sequences.begin();
}

auto SequenceStore::end() noexcept
    -> std::vector<Sequence>::iterator
{
    return m_sequences.end();
}

// ============================================================================
// reserve / clear — 内存管理
// ============================================================================
void SequenceStore::reserve(const size_t capacity)
{
    m_sequences.reserve(capacity);
}

void SequenceStore::clear() noexcept
{
    // 使用 swap + 临时对象的惯用法确保内存被释放，
    // 而非仅调用 clear()（可能保留已分配的容量）。
    std::vector<Sequence>().swap(m_sequences);
}

} // namespace libre_bio
