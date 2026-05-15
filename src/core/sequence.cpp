/// @file sequence.cpp
/// @brief 生物序列模块 — Sequence 类实现
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.

#include "libre_bio/core/sequence.h"

#include <array>
#include <string>
#include <utility>

namespace libre_bio {

namespace {

// 预计算互补映射表，以空间换时间。
// 使用 std::array<char, 256> 而非 unordered_map，原因：
// 1. char 只有 256 个可能的值，数组直接索引 O(1) 且无分支预测开销
// 2. constexpr 编译期初始化，零运行时开销
// 3. 对于百万级序列的反向互补操作，查表比 switch/if-else 快约 5-10x

constexpr std::array<char, 256> MakeDNAComplementTable() noexcept
{
    std::array<char, 256> table{};
    // 默认所有未定义字符映射到 'N'，以容忍输入中的非法字符
    for (int i = 0; i < 256; ++i) {
        table[i] = 'N';
    }
    table['A'] = 'T'; table['T'] = 'A';
    table['C'] = 'G'; table['G'] = 'C';
    table['a'] = 't'; table['t'] = 'a';
    table['c'] = 'g'; table['g'] = 'c';
    table['N'] = 'N'; table['n'] = 'n';
    // 空格保留，支持序列中带空格的可读格式
    table[' '] = ' ';
    return table;
}

constexpr std::array<char, 256> MakeRNAComplementTable() noexcept
{
    std::array<char, 256> table{};
    for (int i = 0; i < 256; ++i) {
        table[i] = 'N';
    }
    // RNA 与 DNA 的区别仅在于 T → U 的替换
    table['A'] = 'U'; table['U'] = 'A';
    table['C'] = 'G'; table['G'] = 'C';
    table['a'] = 'u'; table['u'] = 'a';
    table['c'] = 'g'; table['g'] = 'c';
    table['N'] = 'N'; table['n'] = 'n';
    table[' '] = ' ';
    return table;
}

constexpr auto kDNAComplement = MakeDNAComplementTable();
constexpr auto kRNAComplement = MakeRNAComplementTable();

// 布尔查找表：O(1) 校验单字符是否属于合法字符集
// 与互补表相同理由：数组直接索引无分支开销，适合长序列扫描
// 注意：非 constexpr — std::string 迭代器在 GCC libstdc++ C++17 下不是 constexpr
std::array<bool, 256> MakeValidateTable(const std::string& valid_chars) noexcept
{
    std::array<bool, 256> table{};
    for (const char ch : valid_chars) {
        table[static_cast<unsigned char>(ch)] = true;
    }
    return table;
}

} // namespace

// ============================================================================
// 构造函数 — 仅移动字符串，不执行校验
// ============================================================================
Sequence::Sequence(std::string id, std::string description,
                   std::string seq, Alphabet alphabet)
    : m_id(std::move(id))
    , m_description(std::move(description))
    , m_seq(std::move(seq))
    , m_alphabet(alphabet)
{
    // 不调用 validate() 的原因：
    // 在 FASTA/FASTQ 流式解析场景中，构造时校验会导致每一条序列被遍历两次。
    // 将校验推迟到调用方按需执行，可避免流式场景下 2x 的性能损失。
}

// ============================================================================
// 属性访问器 — 返回 const 引用以支持不可变设计
// ============================================================================
const std::string& Sequence::id() const noexcept
{
    return m_id;
}

const std::string& Sequence::description() const noexcept
{
    return m_description;
}

const std::string& Sequence::seq() const noexcept
{
    return m_seq;
}

Alphabet Sequence::alphabet() const noexcept
{
    return m_alphabet;
}

size_t Sequence::length() const noexcept
{
    return m_seq.length();
}

// ============================================================================
// sub_seq — 边界自动修正，避免调用方需要提前校验
// ============================================================================
Sequence Sequence::sub_seq(const size_t start, const size_t count) const
{
    // 越界保护：start 超出序列长度时返回空序列，上层无需额外的 if 判断
    if (start >= m_seq.length()) {
        return Sequence(m_id, m_description, std::string{}, m_alphabet);
    }

    // 长度自动截断：避免 substr 抛出 std::out_of_range
    const size_t actual_count = (start + count > m_seq.length())
        ? m_seq.length() - start
        : count;

    return Sequence(m_id, m_description,
                    m_seq.substr(start, actual_count),
                    m_alphabet);
}

// ============================================================================
// reverse_complement — 预计算查找表 + 反向遍历
// ============================================================================
Sequence Sequence::reverse_complement() const
{
    // Protein 序列无互补概念，返回原序列副本
    if (m_alphabet != Alphabet::kDNA && m_alphabet != Alphabet::kRNA) {
        return Sequence(m_id, m_description, m_seq, m_alphabet);
    }

    const auto& table = (m_alphabet == Alphabet::kDNA)
        ? kDNAComplement
        : kRNAComplement;

    std::string result;
    result.reserve(m_seq.length());  // 预分配避免多次扩容

    // 从尾到头遍历，同时查表互补。
    // 使用 reverse_iterator 而非手写索引，意图更清晰。
    for (auto it = m_seq.rbegin(); it != m_seq.rend(); ++it) {
        result.push_back(table[static_cast<unsigned char>(*it)]);
    }

    return Sequence(m_id,
                    m_description + " reverse complement",
                    std::move(result),
                    m_alphabet);
}

// ============================================================================
// gc_content — 使用 double 避免 float 精度不足
// ============================================================================
double Sequence::gc_content() const noexcept
{
    if (m_alphabet != Alphabet::kDNA && m_alphabet != Alphabet::kRNA) {
        return 0.0;
    }

    if (m_seq.empty()) {
        return 0.0;
    }

    size_t gc_count = 0;
    for (const char ch : m_seq) {
        if (ch == 'G' || ch == 'C' || ch == 'g' || ch == 'c') {
            ++gc_count;
        }
    }

    // 使用 double 而非 float：
    // 序列可能长达 10^9 bp，float 有效精度 ≈ 7 位，可能产生累计误差。
    // MISRA C++:2023 Rule 13.0.1 建议在不确定的情况下优先使用 double。
    return static_cast<double>(gc_count) / static_cast<double>(m_seq.length());
}

// ============================================================================
// validate — 静态布尔查找表 O(n) 扫描
// ============================================================================
bool Sequence::validate() const noexcept
{
    // 每个字母表使用 static const 查找表，仅在首次进入对应分支时初始化，
    // 后续调用直接复用，避免每次校验都重建查找表。
    switch (m_alphabet) {
    case Alphabet::kDNA: {
        static const auto kDNAValid = MakeValidateTable("ACGTNacgtn");
        for (const char ch : m_seq) {
            if (!kDNAValid[static_cast<unsigned char>(ch)]) {
                return false;
            }
        }
        return true;
    }
    case Alphabet::kRNA: {
        static const auto kRNAValid = MakeValidateTable("ACGUNacgun");
        for (const char ch : m_seq) {
            if (!kRNAValid[static_cast<unsigned char>(ch)]) {
                return false;
            }
        }
        return true;
    }
    case Alphabet::kProtein: {
        static const auto kProteinValid = MakeValidateTable(
            "ACDEFGHIKLMNPQRSTVWY*acdefghiklmnpqrstvwy*");
        for (const char ch : m_seq) {
            if (!kProteinValid[static_cast<unsigned char>(ch)]) {
                return false;
            }
        }
        return true;
    }
    }

    return true;
}

} // namespace libre_bio
