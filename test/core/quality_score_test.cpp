/// @file quality_score_test.cpp
/// @brief Phred 质量分数模块 — QualityScore 类单元测试
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 测试覆盖范围：
/// - 编码检测：Sanger、Illumina 1.3+、Illumina 1.8+、空数组、非法值
/// - 统计指标：average_score（全等/混合）、q30_ratio（全通过/全不通过/混合）
/// - 构造函数：自动检测 vs 显式指定编码
/// - 访问方法：size()、operator[]、scores()
/// - 性能基准：1 亿个质量值的 average_score 性能

#include "libre_bio/core/quality_score.h"

#include <catch2/catch_test_macros.hpp>

using libre_bio::PhredEncoding;
using libre_bio::QualityScore;

// ============================================================================
// 编码检测测试 — TC01 ~ TC05
// ============================================================================

TEST_CASE("TC01: Sanger encoding detection", "[quality_score]")
{
    // ASCII 73 ('I') = Phred 40（Sanger offset 33）
    // min_val = 73 > 64 → 保守假设为 Sanger
    const std::vector<uint8_t> scores = {'I', 'I', 'I', 'I'};
    const QualityScore qs(scores);

    REQUIRE(qs.encoding() == PhredEncoding::kSanger);
    REQUIRE(qs.scores().size() == 4);
    REQUIRE(qs.scores()[0] == 73);
    REQUIRE(qs.scores()[1] == 73);
    REQUIRE(qs.scores()[2] == 73);
    REQUIRE(qs.scores()[3] == 73);
}

TEST_CASE("TC02: Illumina 1.3 encoding detection", "[quality_score]")
{
    // min_val = 64 → 唯一判定为 Illumina13（Phred 0 边界特征）
    const std::vector<uint8_t> scores = {64, 65, 70, 80};
    const QualityScore qs(scores);

    REQUIRE(qs.encoding() == PhredEncoding::kIllumina13);
}

TEST_CASE("TC03: Illumina 1.8 encoding detection", "[quality_score]")
{
    // Illumina 1.8+ 与 Sanger 同偏移量，无法区分，返回 kSanger
    const std::vector<uint8_t> scores = {33, 40, 50, 60};
    const QualityScore qs(scores);

    REQUIRE(qs.encoding() == PhredEncoding::kSanger);
}

TEST_CASE("TC04: Empty array", "[quality_score]")
{
    const QualityScore qs(std::vector<uint8_t>{});

    REQUIRE(qs.encoding() == PhredEncoding::kUnknown);
    REQUIRE(qs.size() == 0);
    // 空序列上的统计方法应安全返回 0.0 而非崩溃
    REQUIRE(qs.average_score() == 0.0);
    REQUIRE(qs.q30_ratio() == 0.0);
}

TEST_CASE("TC05: Invalid encoding detection", "[quality_score]")
{
    // 所有值低于 33，不属于任何已知 Phred 编码方案
    const std::vector<uint8_t> scores = {10, 20, 30};
    const QualityScore qs(scores);

    REQUIRE(qs.encoding() == PhredEncoding::kUnknown);
}

// ============================================================================
// average_score 测试 — TC06 ~ TC07
// ============================================================================

TEST_CASE("TC06: average_score calculation Sanger", "[quality_score]")
{
    // ASCII 43 - 33 = Phred 10，四个值相同
    const std::vector<uint8_t> scores = {43, 43, 43, 43};
    const QualityScore qs(scores);

    REQUIRE(qs.average_score() == 10.0);
}

TEST_CASE("TC07: average_score calculation mixed", "[quality_score]")
{
    // Phred 40, 40, 0, 0 → 均值 20.0
    // ASCII 73, 73, 33, 33
    const std::vector<uint8_t> scores = {73, 73, 33, 33};
    const QualityScore qs(scores);

    REQUIRE(qs.average_score() == 20.0);
}

// ============================================================================
// q30_ratio 测试 — TC08 ~ TC10
// ============================================================================

TEST_CASE("TC08: q30_ratio all pass", "[quality_score]")
{
    // 全部 Phred 40 ≥ 30 → 比例 1.0
    const std::vector<uint8_t> scores = {73, 73, 73, 73};
    const QualityScore qs(scores);

    REQUIRE(qs.q30_ratio() == 1.0);
}

TEST_CASE("TC09: q30_ratio all fail", "[quality_score]")
{
    // 全部 Phred 10 < 30 → 比例 0.0
    const std::vector<uint8_t> scores = {43, 43, 43, 43};
    const QualityScore qs(scores);

    REQUIRE(qs.q30_ratio() == 0.0);
}

TEST_CASE("TC10: q30_ratio mixed", "[quality_score]")
{
    // Phred 值 {40, 20, 35, 10} → 40 和 35 通过 → 2/4 = 0.5
    // ASCII: 73, 53, 68, 43
    const std::vector<uint8_t> scores = {73, 53, 68, 43};
    const QualityScore qs(scores);

    REQUIRE(qs.q30_ratio() == 0.5);
}

// ============================================================================
// 构造与访问测试 — TC11 ~ TC13
// ============================================================================

TEST_CASE("TC11: Explicit encoding constructor", "[quality_score]")
{
    // 显式指定 Illumina13 编码，跳过自动检测
    // ASCII 64 - 64 = Phred 0
    const std::vector<uint8_t> scores = {64, 64, 64};
    const QualityScore qs(scores, PhredEncoding::kIllumina13);

    REQUIRE(qs.encoding() == PhredEncoding::kIllumina13);
    REQUIRE(qs.average_score() == 0.0);
}

TEST_CASE("TC12: size returns correct value", "[quality_score]")
{
    std::vector<uint8_t> scores(100, 73);
    const QualityScore qs(std::move(scores));

    REQUIRE(qs.size() == 100);
}

TEST_CASE("TC13: operator[] returns correct Phred value", "[quality_score]")
{
    // ASCII 73 ('I') - Sanger offset 33 = Phred 40
    const std::vector<uint8_t> scores = {73};
    const QualityScore qs(scores);

    REQUIRE(qs[0] == 40);
}

// ============================================================================
// 性能基准测试 — TC14
// ============================================================================

TEST_CASE("TC14: Performance test 100 million values", "[quality_score][.perf]")
{
    // 1 亿个全 Phred 40（ASCII 73）的质量值
    // 使用 std::move 避免测试构建阶段的额外数据拷贝
    std::vector<uint8_t> scores(100'000'000, 73);
    const QualityScore qs(std::move(scores));

    const double avg = qs.average_score();

    REQUIRE(avg == 40.0);
    REQUIRE(qs.size() == 100'000'000);
}
