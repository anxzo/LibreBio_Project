/// @file sequence_test.cpp
/// @brief 生物序列模块 — Sequence 类单元测试
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 测试覆盖范围：
/// - 正常路径：DNA/RNA/Protein 三种字母表的构造与属性查询
/// - 边界条件：空序列、越界截取、长度超限
/// - 变换操作：子序列截取、反向互补
/// - 计算操作：GC 含量（含大小写混合）
/// - 校验操作：合法/非法字符检测
/// - 性能基准：千万碱基序列的 gc_content 和 validate 性能
///
/// @see test/core/sequence_test.cpp — 本文档
/// @see doc/detailed-design/m01_sequence.md §5.2 — 测试用例规格

#include "libre_bio/core/sequence.h"

#include <catch2/catch_test_macros.hpp>

using libre_bio::Alphabet;
using libre_bio::Sequence;

// ============================================================================
// 构造与属性访问测试
// ============================================================================

TEST_CASE("TC01: Normal DNA sequence construction", "[sequence]")
{
    const Sequence seq("seq1", "test", "ATCG", Alphabet::kDNA);

    REQUIRE(seq.id() == "seq1");
    REQUIRE(seq.description() == "test");
    REQUIRE(seq.seq() == "ATCG");
    REQUIRE(seq.alphabet() == Alphabet::kDNA);
    REQUIRE(seq.length() == 4);
}

TEST_CASE("TC02: Normal RNA sequence construction", "[sequence]")
{
    const Sequence seq("rna1", "", "AUCG", Alphabet::kRNA);

    REQUIRE(seq.alphabet() == Alphabet::kRNA);
    REQUIRE(seq.length() == 4);
}

TEST_CASE("TC03: Normal Protein sequence construction", "[sequence]")
{
    const Sequence seq("prot1", "enzyme", "MKTGFL", Alphabet::kProtein);

    REQUIRE(seq.alphabet() == Alphabet::kProtein);
    REQUIRE(seq.length() == 6);
}

// 验证空序列的边界行为：gc_content 不因分母为零而崩溃
TEST_CASE("TC04: Empty sequence construction", "[sequence]")
{
    const Sequence seq("empty", "", "", Alphabet::kDNA);

    REQUIRE(seq.length() == 0);
    REQUIRE(seq.gc_content() == 0.0);
}

// ============================================================================
// sub_seq 测试 — 截取行为与边界修正
// ============================================================================

TEST_CASE("TC05: sub_seq normal extraction", "[sequence]")
{
    const Sequence seq("s", "", "ACGTACGT", Alphabet::kDNA);
    const Sequence sub = seq.sub_seq(0, 4);

    REQUIRE(sub.id() == "s");
    REQUIRE(sub.description() == "");
    REQUIRE(sub.seq() == "ACGT");
    REQUIRE(sub.alphabet() == Alphabet::kDNA);
    REQUIRE(sub.length() == 4);
}

// start 等于序列长度时视为越界，返回空序列
TEST_CASE("TC06: sub_seq start out of bounds", "[sequence]")
{
    const Sequence seq("s", "", "ACGT", Alphabet::kDNA);
    const Sequence sub = seq.sub_seq(4, 1);

    REQUIRE(sub.length() == 0);
}

// count 超过剩余长度时自动截断到末尾
TEST_CASE("TC07: sub_seq length exceeds bounds", "[sequence]")
{
    const Sequence seq("s", "", "ACGT", Alphabet::kDNA);
    const Sequence sub = seq.sub_seq(2, 10);

    REQUIRE(sub.seq() == "GT");
    REQUIRE(sub.length() == 2);
}

// count 为 0 时返回空序列，而非抛出异常
TEST_CASE("TC08: sub_seq count is zero", "[sequence]")
{
    const Sequence seq("s", "", "ACGT", Alphabet::kDNA);
    const Sequence sub = seq.sub_seq(0, 0);

    REQUIRE(sub.length() == 0);
}

// ============================================================================
// reverse_complement 测试 — 互补映射正确性
// ============================================================================

// 空格应当原样保留，不做互补
TEST_CASE("TC09: DNA reverse complement with space", "[sequence]")
{
    const Sequence seq("s", "", "ATC G", Alphabet::kDNA);
    const Sequence rc = seq.reverse_complement();

    REQUIRE(rc.seq() == "C GAT");
}

// RNA 使用 U 替代 T
TEST_CASE("TC10: RNA reverse complement", "[sequence]")
{
    const Sequence seq("s", "", "AUCG", Alphabet::kRNA);
    const Sequence rc = seq.reverse_complement();

    REQUIRE(rc.seq() == "CGAU");
}

// Protein 不支持反向互补，应返回原序列副本
TEST_CASE("TC11: Protein reverse complement returns copy", "[sequence]")
{
    const Sequence seq("s", "", "MKTG", Alphabet::kProtein);
    const Sequence rc = seq.reverse_complement();

    REQUIRE(rc.seq() == "MKTG");
}

// ============================================================================
// gc_content 测试 — 比例计算与边界值
// ============================================================================

TEST_CASE("TC12: gc_content normal", "[sequence]")
{
    const Sequence seq("s", "", "GCGCATAT", Alphabet::kDNA);

    REQUIRE(seq.gc_content() == 0.5);
}

// 全 GC 应返回 1.0，验证浮点比较
TEST_CASE("TC13: gc_content all GC", "[sequence]")
{
    const Sequence seq("s", "", "GGGGCCCC", Alphabet::kDNA);

    REQUIRE(seq.gc_content() == 1.0);
}

TEST_CASE("TC14: gc_content no GC", "[sequence]")
{
    const Sequence seq("s", "", "ATATATAT", Alphabet::kDNA);

    REQUIRE(seq.gc_content() == 0.0);
}

// 大小写混合应统一计入
TEST_CASE("TC15: gc_content mixed case", "[sequence]")
{
    const Sequence seq("s", "", "gcatGCAT", Alphabet::kDNA);

    REQUIRE(seq.gc_content() == 0.5);
}

// ============================================================================
// validate 测试 — 字符集合法性校验
// ============================================================================

TEST_CASE("TC16: validate DNA valid sequence", "[sequence]")
{
    const Sequence seq("s", "", "ATCGN", Alphabet::kDNA);

    REQUIRE(seq.validate() == true);
}

// 'M' 是简并碱基（代表 A 或 C），但不在严格 DNA 字母表中
TEST_CASE("TC17: validate DNA invalid character", "[sequence]")
{
    const Sequence seq("s", "", "ATCGM", Alphabet::kDNA);

    REQUIRE(seq.validate() == false);
}

// '*' 在蛋白质序列中代表终止密码子，是合法字符
TEST_CASE("TC18: validate Protein valid sequence", "[sequence]")
{
    const Sequence seq("s", "", "MKTGFL*", Alphabet::kProtein);

    REQUIRE(seq.validate() == true);
}

// ============================================================================
// 性能基准测试 — 标记为 [.perf]，CI 中默认跳过
// ============================================================================

// 千万碱基序列的 gc_content 和 validate 应在毫秒级完成。
// 该测试不设硬性时间断言，而是验证功能正确性不因规模增长而退化。
TEST_CASE("TC19: Long sequence performance", "[sequence][.perf]")
{
    std::string long_seq(10'000'000, 'A');
    const Sequence seq("long", "", long_seq, Alphabet::kDNA);

    const double gc = seq.gc_content();

    REQUIRE(gc == 0.0);
    REQUIRE(seq.length() == 10'000'000);
    REQUIRE(seq.validate() == true);
}
