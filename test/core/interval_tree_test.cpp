/// @file interval_tree_test.cpp
/// @brief 区间树模块 — IntervalTree 类单元测试
/// @author LibreBio Team
/// @version 0.3.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 测试覆盖范围：
/// - 正常路径：插入、查询（重叠/最近邻）、删除、批量构造
/// - 边界条件：空树、重复区间、不存在节点删除、清空后查询
/// - 性能基准：百万级区间批量构造与查询
/// - 语义正确性：移动构造/赋值
///
/// @see doc/detailed-design/m06_interval_tree.md §7.2 — 测试用例规格

#include "libre_bio/core/interval_tree.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using libre_bio::GenomicInterval;
using libre_bio::IntervalTree;

// ============================================================================
// TC01: 空树查询 — 边界条件
// ============================================================================

TEST_CASE("TC01: 空树查询", "[interval_tree]")
{
    const IntervalTree<std::string> tree;
    const GenomicInterval query("chr1", 0, 100);

    const auto results = tree.query_overlap(query);

    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
    REQUIRE(results.empty());
}

// ============================================================================
// TC02: 单节点插入 + 查询命中 — 正常路径
// ============================================================================

TEST_CASE("TC02: 单节点插入 + 查询命中", "[interval_tree]")
{
    IntervalTree<std::string> tree;
    tree.insert(GenomicInterval("chr1", 0, 100), "A");

    const GenomicInterval query("chr1", 50, 150);
    const auto results = tree.query_overlap(query);

    REQUIRE(tree.size() == 1);
    REQUIRE_FALSE(tree.empty());
    REQUIRE(results.size() == 1);
    REQUIRE(results[0] == "A");
}

// ============================================================================
// TC03: 单节点插入 + 查询未命中 — 正常路径
// ============================================================================

TEST_CASE("TC03: 单节点插入 + 查询未命中", "[interval_tree]")
{
    IntervalTree<std::string> tree;
    tree.insert(GenomicInterval("chr1", 0, 100), "A");

    const GenomicInterval query("chr1", 100, 200);
    const auto results = tree.query_overlap(query);

    REQUIRE(results.empty());
}

// ============================================================================
// TC04: 多节点 — 查询命中多个 — 正常路径
// ============================================================================

TEST_CASE("TC04: 多节点 — 查询命中多个", "[interval_tree]")
{
    IntervalTree<std::string> tree;
    tree.insert(GenomicInterval("chr1", 0, 50), "A");
    tree.insert(GenomicInterval("chr1", 25, 75), "B");
    tree.insert(GenomicInterval("chr1", 60, 100), "C");

    const GenomicInterval query("chr1", 30, 70);
    const auto results = tree.query_overlap(query);

    REQUIRE(results.size() == 3);

    const bool has_a = std::find(results.begin(), results.end(), "A") != results.end();
    const bool has_b = std::find(results.begin(), results.end(), "B") != results.end();
    const bool has_c = std::find(results.begin(), results.end(), "C") != results.end();

    REQUIRE(has_a);
    REQUIRE(has_b);
    REQUIRE(has_c);
}

// ============================================================================
// TC05: 多节点 — 查询命中部分 — 正常路径
// ============================================================================

TEST_CASE("TC05: 多节点 — 查询命中部分", "[interval_tree]")
{
    IntervalTree<std::string> tree;
    tree.insert(GenomicInterval("chr1", 0, 50), "A");
    tree.insert(GenomicInterval("chr1", 40, 60), "B");
    tree.insert(GenomicInterval("chr1", 200, 300), "C");

    const GenomicInterval query("chr1", 0, 45);
    const auto results = tree.query_overlap(query);

    REQUIRE(results.size() == 2);

    const bool has_a = std::find(results.begin(), results.end(), "A") != results.end();
    const bool has_b = std::find(results.begin(), results.end(), "B") != results.end();

    REQUIRE(has_a);
    REQUIRE(has_b);
}

// ============================================================================
// TC06: 批量 build 构造 — 正常路径
// ============================================================================

TEST_CASE("TC06: 批量 build 构造", "[interval_tree]")
{
    IntervalTree<std::string> tree;

    std::vector<std::pair<GenomicInterval, std::string>> entries;
    entries.push_back({GenomicInterval("chr1", 0, 50), "A"});
    entries.push_back({GenomicInterval("chr1", 25, 75), "B"});
    entries.push_back({GenomicInterval("chr1", 60, 100), "C"});

    tree.build(std::move(entries));

    REQUIRE(tree.size() == 3);

    const GenomicInterval query("chr1", 30, 70);
    const auto results = tree.query_overlap(query);

    REQUIRE(results.size() == 3);
}

// ============================================================================
// TC07: 批量 build 性能（规模测试）— 性能基准
// ============================================================================

TEST_CASE("TC07: 批量 build 性能（100万区间）", "[interval_tree][.perf]")
{
    constexpr size_t kNumIntervals = 1000000;

    std::vector<std::pair<GenomicInterval, std::string>> entries;
    entries.reserve(kNumIntervals);

    for (size_t i = 0; i < kNumIntervals; ++i) {
        uint32_t start = static_cast<uint32_t>(i * 10);
        uint32_t end = static_cast<uint32_t>(start + 5);
        entries.push_back({GenomicInterval("chr1", start, end),
                           std::to_string(i)});
    }

    IntervalTree<std::string> tree;

    const auto build_start = std::chrono::steady_clock::now();
    tree.build(std::move(entries));
    const auto build_end = std::chrono::steady_clock::now();

    const auto build_duration = std::chrono::duration_cast<
        std::chrono::milliseconds>(build_end - build_start).count();

    REQUIRE(tree.size() == kNumIntervals);
    REQUIRE(build_duration < 2000);

    // 100 次随机查询
    const auto query_start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < 100; ++i) {
        uint32_t q_start = static_cast<uint32_t>((i * 97 + 500) % (kNumIntervals * 10));
        uint32_t q_end = q_start + 50;
        GenomicInterval query("chr1", q_start, q_end);

        const auto results = tree.query_overlap(query);
        REQUIRE_FALSE(results.empty());
    }

    const auto query_end = std::chrono::steady_clock::now();

    const auto query_duration = std::chrono::duration_cast<
        std::chrono::microseconds>(query_end - query_start).count();

    const double avg_query_us = static_cast<double>(query_duration) / 100.0;
    REQUIRE(avg_query_us < 100.0);
}

// ============================================================================
// TC08: remove 删除已有节点 — 正常路径
// ============================================================================

TEST_CASE("TC08: remove 删除已有节点", "[interval_tree]")
{
    IntervalTree<std::string> tree;
    tree.insert(GenomicInterval("chr1", 0, 100), "A");
    tree.insert(GenomicInterval("chr1", 50, 150), "B");

    REQUIRE(tree.size() == 2);

    tree.remove(GenomicInterval("chr1", 0, 100));

    REQUIRE(tree.size() == 1);

    const GenomicInterval query("chr1", 0, 60);
    const auto results = tree.query_overlap(query);

    REQUIRE(results.size() == 1);
    REQUIRE(results[0] == "B");
}

// ============================================================================
// TC09: remove 删除不存在的节点 — 边界条件
// ============================================================================

TEST_CASE("TC09: remove 删除不存在的节点", "[interval_tree]")
{
    IntervalTree<std::string> tree;
    tree.insert(GenomicInterval("chr1", 0, 100), "A");

    REQUIRE(tree.size() == 1);

    tree.remove(GenomicInterval("chr1", 999, 1000));

    REQUIRE(tree.size() == 1);
}

// ============================================================================
// TC10: clear 后查询 — 正常路径
// ============================================================================

TEST_CASE("TC10: clear 后查询", "[interval_tree]")
{
    IntervalTree<std::string> tree;
    tree.insert(GenomicInterval("chr1", 0, 100), "A");
    tree.insert(GenomicInterval("chr1", 200, 300), "B");
    tree.insert(GenomicInterval("chr1", 400, 500), "C");

    REQUIRE(tree.size() == 3);

    tree.clear();

    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);

    const GenomicInterval query("chr1", 0, 500);
    const auto results = tree.query_overlap(query);

    REQUIRE(results.empty());
}

// ============================================================================
// TC11: 重复区间插入 — 正常路径
// ============================================================================

TEST_CASE("TC11: 重复区间插入", "[interval_tree]")
{
    IntervalTree<std::string> tree;
    tree.insert(GenomicInterval("chr1", 0, 100), "A");
    tree.insert(GenomicInterval("chr1", 0, 100), "B");

    REQUIRE(tree.size() == 2);

    const GenomicInterval query("chr1", 0, 50);
    const auto results = tree.query_overlap(query);

    REQUIRE(results.size() == 2);

    const bool has_a = std::find(results.begin(), results.end(), "A") != results.end();
    const bool has_b = std::find(results.begin(), results.end(), "B") != results.end();

    REQUIRE(has_a);
    REQUIRE(has_b);
}

// ============================================================================
// TC12: 大范围查询（全命中）— 边界条件
// ============================================================================

TEST_CASE("TC12: 大范围查询（全命中）", "[interval_tree]")
{
    IntervalTree<std::string> tree;

    for (uint32_t i = 0; i < 10; ++i) {
        uint32_t s = i * 10;
        uint32_t e = s + 10;
        tree.insert(GenomicInterval("chr1", s, e),
                    std::string(1, static_cast<char>('A' + static_cast<int>(i))));
    }

    REQUIRE(tree.size() == 10);

    const GenomicInterval query("chr1", 0, 100);
    const auto results = tree.query_overlap(query);

    REQUIRE(results.size() == 10);
}

// ============================================================================
// TC13: query_nearest 基本功能 — 正常路径
// ============================================================================

TEST_CASE("TC13: query_nearest 基本功能", "[interval_tree]")
{
    IntervalTree<std::string> tree;
    tree.insert(GenomicInterval("chr1", 0, 10), "A");
    tree.insert(GenomicInterval("chr1", 20, 30), "B");
    tree.insert(GenomicInterval("chr1", 50, 60), "C");
    tree.insert(GenomicInterval("chr1", 80, 90), "D");
    tree.insert(GenomicInterval("chr1", 200, 300), "E");

    const GenomicInterval query("chr1", 15, 18);

    const auto results = tree.query_nearest(query, 3);

    REQUIRE(results.size() == 3);
    // query [15,18):
    // 到 A [0,10):  距离 = 15-10 = 5 → 距离 5
    // 到 B [20,30): 距离 = 20-18 = 2 → 距离 2
    // 到 C [50,60): 距离 = 50-18 = 32 → 距离 32
    // 到 D [80,90): 距离 = 80-18 = 62
    // 到 E [200,300): 距离 = 200-18 = 182
    // 最近3个: B (2), A (5), C (32)
    REQUIRE(results[0] == "B");
    REQUIRE(results[1] == "A");
    REQUIRE(results[2] == "C");
}

// ============================================================================
// TC14: 移动语义 — 正常路径
// ============================================================================

TEST_CASE("TC14: 移动语义", "[interval_tree]")
{
    IntervalTree<std::string> tree1;
    tree1.insert(GenomicInterval("chr1", 0, 100), "X");
    tree1.insert(GenomicInterval("chr1", 200, 300), "Y");
    tree1.insert(GenomicInterval("chr1", 400, 500), "Z");

    REQUIRE(tree1.size() == 3);

    IntervalTree<std::string> tree2(std::move(tree1));

    REQUIRE(tree1.empty());
    REQUIRE(tree1.size() == 0);
    REQUIRE(tree2.size() == 3);

    const GenomicInterval query("chr1", 50, 250);
    const auto results = tree2.query_overlap(query);

    REQUIRE(results.size() == 2);

    const bool has_x = std::find(results.begin(), results.end(), "X") != results.end();
    const bool has_y = std::find(results.begin(), results.end(), "Y") != results.end();

    REQUIRE(has_x);
    REQUIRE(has_y);

    // 原树查询应返回空
    const auto empty_results = tree1.query_overlap(query);
    REQUIRE(empty_results.empty());
}
