/// @file genomic_interval_test.cpp
/// @brief 基因组区间模块 — GenomicInterval 类单元测试
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 测试覆盖范围：
/// - 正常路径：构造与属性查询、区间关系判断、集合运算
/// - 边界条件：紧邻不重叠、无交集空区间、自我包含、不同染色体
/// - 比较运算：字典序排序、相等判断（含 strand 差异）
///
/// @see doc/detailed-design/m04_genomic_interval.md §5.2 — 测试用例规格

#include "libre_bio/core/genomic_interval.h"

#include <catch2/catch_test_macros.hpp>

using libre_bio::GenomicInterval;
using libre_bio::Strand;

// ============================================================================
// 构造与属性访问测试
// ============================================================================

TEST_CASE("TC01: 正常构造", "[genomic_interval]")
{
    const GenomicInterval iv("chr1", 100, 200, Strand::kForward);

    REQUIRE(iv.chrom() == "chr1");
    REQUIRE(iv.start() == 100);
    REQUIRE(iv.end() == 200);
    REQUIRE(iv.strand() == Strand::kForward);
    REQUIRE(iv.length() == 100);
}

TEST_CASE("TC02: length 计算", "[genomic_interval]")
{
    const GenomicInterval iv("chr1", 0, 1000000);

    REQUIRE(iv.length() == 1000000);
}

// ============================================================================
// overlaps — 重叠判断测试
// ============================================================================

TEST_CASE("TC03: 重叠 — 完全包含", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr1", 25, 75);

    REQUIRE(a.overlaps(b));
    REQUIRE(b.overlaps(a));
}

TEST_CASE("TC04: 重叠 — 部分重叠", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr1", 50, 150);

    REQUIRE(a.overlaps(b));
}

TEST_CASE("TC05: 重叠 — 紧邻但不重叠", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr1", 100, 200);

    REQUIRE_FALSE(a.overlaps(b));
}

TEST_CASE("TC06: 重叠 — 完全分离", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr1", 200, 300);

    REQUIRE_FALSE(a.overlaps(b));
}

TEST_CASE("TC07: 重叠 — 不同染色体", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr2", 0, 100);

    REQUIRE_FALSE(a.overlaps(b));
}

// ============================================================================
// intersect — 区间交集测试
// ============================================================================

TEST_CASE("TC08: 交集 — 正常", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr1", 50, 150);

    const GenomicInterval result = a.intersect(b);

    REQUIRE(result.chrom() == "chr1");
    REQUIRE(result.start() == 50);
    REQUIRE(result.end() == 100);
    REQUIRE(result.length() == 50);
}

TEST_CASE("TC09: 交集 — 无交集", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr1", 200, 300);

    const GenomicInterval result = a.intersect(b);

    REQUIRE(result.length() == 0);
}

// ============================================================================
// contains — 包含判断测试
// ============================================================================

TEST_CASE("TC10: 包含 — 完全包含", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr1", 25, 75);

    REQUIRE(a.contains(b));
    REQUIRE_FALSE(b.contains(a));
}

TEST_CASE("TC11: 包含 — 自我", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);

    REQUIRE(a.contains(a));
}

// ============================================================================
// distance — 区间距离测试
// ============================================================================

TEST_CASE("TC12: 距离 — 分离", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr1", 200, 300);

    REQUIRE(a.distance(b) == 100);
    REQUIRE(b.distance(a) == 100);
}

TEST_CASE("TC13: 距离 — 相邻", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr1", 100, 200);

    REQUIRE(a.distance(b) == 0);
}

TEST_CASE("TC14: 距离 — 不同染色体", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 0, 100);
    const GenomicInterval b("chr2", 0, 100);

    REQUIRE(a.distance(b) == INT64_MAX);
}

// ============================================================================
// 比较运算符测试
// ============================================================================

TEST_CASE("TC15: 比较运算符 — chrom 优先", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 500, 600);
    const GenomicInterval b("chr2", 0, 100);

    REQUIRE(a < b);
}

TEST_CASE("TC16: 比较运算符 — start 比较", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 100, 200);
    const GenomicInterval b("chr1", 150, 250);

    REQUIRE(a < b);
}

TEST_CASE("TC17: 比较运算符 — 相等", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 100, 200, Strand::kForward);
    const GenomicInterval b("chr1", 100, 200, Strand::kForward);

    REQUIRE(a == b);
}

TEST_CASE("TC18: 比较运算符 — strand 不同", "[genomic_interval]")
{
    const GenomicInterval a("chr1", 100, 200, Strand::kForward);
    const GenomicInterval b("chr1", 100, 200, Strand::kReverse);

    REQUIRE_FALSE(a == b);
}
