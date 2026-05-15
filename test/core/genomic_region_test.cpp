/// @file genomic_region_test.cpp
/// @brief 基因组区域模块 — GenomicRegion 类单元测试
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 测试覆盖范围：
/// - 正常路径：构造与属性查询、属性存取、interval 互操作
/// - 边界条件：默认 score、负 score、空 name、未找到 key、覆盖已有 key
///
/// @see doc/detailed-design/m05_genomic_region.md §5.2 — 测试用例规格

#include "libre_bio/core/genomic_region.h"

#include <catch2/catch_test_macros.hpp>

using libre_bio::GenomicInterval;
using libre_bio::GenomicRegion;
using libre_bio::Strand;

// ============================================================================
// 构造与属性访问测试
// ============================================================================

TEST_CASE("TC01: 基本构造", "[genomic_region]")
{
    const GenomicInterval iv("chr1", 0, 100, Strand::kForward);
    const GenomicRegion region(iv, "gene1", 0.05);

    REQUIRE(region.name() == "gene1");
    REQUIRE(region.score() == 0.05);
    REQUIRE(region.interval().chrom() == "chr1");
    REQUIRE(region.interval().start() == 0);
    REQUIRE(region.interval().end() == 100);
    REQUIRE(region.interval().strand() == Strand::kForward);
}

TEST_CASE("TC02: 默认 score", "[genomic_region]")
{
    const GenomicInterval iv("chr1", 0, 100);
    const GenomicRegion region(iv, "gene1");

    REQUIRE(region.score() == 0.0);
}

// ============================================================================
// 属性存取测试
// ============================================================================

TEST_CASE("TC03: set_attribute 单个", "[genomic_region]")
{
    GenomicInterval iv("chr1", 0, 100);
    GenomicRegion region(std::move(iv), "gene1");

    region.set_attribute("gene_type", "protein_coding");

    const std::string* val = region.get_attribute("gene_type");
    REQUIRE(val != nullptr);
    REQUIRE(*val == "protein_coding");
}

TEST_CASE("TC04: set_attribute 多个", "[genomic_region]")
{
    GenomicRegion region(GenomicInterval("chr1", 0, 100), "gene1");

    region.set_attribute("k1", "v1");
    region.set_attribute("k2", "v2");

    REQUIRE(region.attributes().size() == 2);
}

TEST_CASE("TC05: get_attribute 未找到", "[genomic_region]")
{
    const GenomicRegion region(GenomicInterval("chr1", 0, 100), "gene1");

    REQUIRE(region.get_attribute("not_exist") == nullptr);
}

TEST_CASE("TC06: set_attribute 覆盖", "[genomic_region]")
{
    GenomicRegion region(GenomicInterval("chr1", 0, 100), "gene1");

    region.set_attribute("k", "v1");
    region.set_attribute("k", "v2");

    const std::string* val = region.get_attribute("k");
    REQUIRE(val != nullptr);
    REQUIRE(*val == "v2");
}

TEST_CASE("TC07: attributes 完整遍历", "[genomic_region]")
{
    GenomicRegion region(GenomicInterval("chr1", 0, 100), "gene1");

    region.set_attribute("a", "1");
    region.set_attribute("b", "2");
    region.set_attribute("c", "3");

    REQUIRE(region.attributes().size() == 3);

    // std::map 保证按 key 字典序遍历
    auto it = region.attributes().begin();
    REQUIRE(it->first == "a");
    REQUIRE(it->second == "1");
    ++it;
    REQUIRE(it->first == "b");
    REQUIRE(it->second == "2");
    ++it;
    REQUIRE(it->first == "c");
    REQUIRE(it->second == "3");
}

// ============================================================================
// interval 互操作测试
// ============================================================================

TEST_CASE("TC08: interval 互操作", "[genomic_region]")
{
    const GenomicRegion region1(GenomicInterval("chr1", 0, 100), "gene1");
    const GenomicRegion region2(GenomicInterval("chr1", 50, 150), "gene2");

    REQUIRE(region1.interval().overlaps(region2.interval()));
}

// ============================================================================
// 边界条件测试
// ============================================================================

TEST_CASE("TC09: 负 score", "[genomic_region]")
{
    const GenomicRegion region(GenomicInterval("chr1", 0, 100), "gene1", -1.5);

    REQUIRE(region.score() == -1.5);
}

TEST_CASE("TC10: 空 name", "[genomic_region]")
{
    const GenomicRegion region(GenomicInterval("chr1", 0, 100), "");

    REQUIRE(region.name().empty());
}
