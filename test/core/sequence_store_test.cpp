/// @file sequence_store_test.cpp
/// @brief 序列容器模块 — SequenceStore 类单元测试
/// @author LibreBio Team
/// @version 0.2.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// 测试覆盖范围：
/// - 正常路径：单条/批量添加、按 ID 查找、迭代器遍历
/// - 边界条件：空容器各项指标、find_by_id 未命中
/// - 统计操作：total_bases、N50（单序列、等长、不均匀、跨多条）
/// - 内存管理：reserve + clear
/// - 性能基准：百万级序列的 total_bases 与 n50 性能
///
/// @see doc/detailed-design/m03_sequence_store.md §5.2 — 测试用例规格

#include "libre_bio/core/sequence.h"
#include "libre_bio/core/sequence_store.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <string>
#include <vector>

using libre_bio::Alphabet;
using libre_bio::Sequence;
using libre_bio::SequenceStore;

// ============================================================================
// TC01: 空 store 基本属性 — 所有统计方法在空容器上返回安全默认值
// ============================================================================

TEST_CASE("TC01: Empty store basic properties", "[sequence_store]")
{
    const SequenceStore store;

    REQUIRE(store.count() == 0);
    REQUIRE(store.empty() == true);
    REQUIRE(store.total_bases() == 0);
    REQUIRE(store.n50() == 0);
    REQUIRE(store.find_by_id("anything") == nullptr);
}

// ============================================================================
// TC02: 单序列 add — 验证基本 CRUD 和统计
// ============================================================================

TEST_CASE("TC02: Single sequence add", "[sequence_store]")
{
    SequenceStore store;
    store.add(Sequence("s1", "", "ATCG", Alphabet::kDNA));

    REQUIRE(store.count() == 1);
    REQUIRE(store.empty() == false);
    REQUIRE(store.total_bases() == 4);
    REQUIRE(store.n50() == 4);
}

// ============================================================================
// TC03: 批量 add — 验证批量插入后统计正确
// ============================================================================

TEST_CASE("TC03: Batch add", "[sequence_store]")
{
    SequenceStore store;

    std::vector<Sequence> vec;
    vec.emplace_back("s1", "", "AAAA", Alphabet::kDNA);
    vec.emplace_back("s2", "", "CC", Alphabet::kDNA);

    store.add(std::move(vec));

    REQUIRE(store.count() == 2);
    REQUIRE(store.total_bases() == 6);
}

// ============================================================================
// TC04: find_by_id 找到 — 返回正确序列的指针
// ============================================================================

TEST_CASE("TC04: find_by_id found", "[sequence_store]")
{
    SequenceStore store;
    store.add(Sequence("chr1", "", "ATCG", Alphabet::kDNA));

    const Sequence* result = store.find_by_id("chr1");

    REQUIRE(result != nullptr);
    REQUIRE(result->id() == "chr1");
    REQUIRE(result->seq() == "ATCG");
}

// ============================================================================
// TC05: find_by_id 未找到 — 返回 nullptr
// ============================================================================

TEST_CASE("TC05: find_by_id not found", "[sequence_store]")
{
    SequenceStore store;
    store.add(Sequence("chr1", "", "ATCG", Alphabet::kDNA));

    const Sequence* result = store.find_by_id("chr2");

    REQUIRE(result == nullptr);
}

// ============================================================================
// TC06: N50 单序列 — 仅有一条序列时 N50 等于该序列长度
// ============================================================================

TEST_CASE("TC06: N50 single sequence", "[sequence_store]")
{
    SequenceStore store;
    store.add(Sequence("s", "", std::string(100, 'A'), Alphabet::kDNA));

    REQUIRE(store.n50() == 100);
}

// ============================================================================
// TC07: N50 等长序列 — 4 条 100bp，total=400，target=200
// ============================================================================

TEST_CASE("TC07: N50 equal-length sequences", "[sequence_store]")
{
    SequenceStore store;

    for (int i = 0; i < 4; ++i) {
        store.add(Sequence("s" + std::to_string(i), "",
                           std::string(100, 'A'), Alphabet::kDNA));
    }

    // total=400, target=200，第 2 条时就达到
    REQUIRE(store.n50() == 100);
}

// ============================================================================
// TC08: N50 不均匀序列 — 第一条就达到 50%
// ============================================================================

TEST_CASE("TC08: N50 uneven sequences — first covers target", "[sequence_store]")
{
    SequenceStore store;

    // 长度分布: [500, 200, 100, 100, 50, 50], total=1000, target=500
    store.add(Sequence("s1", "", std::string(500, 'A'), Alphabet::kDNA));
    store.add(Sequence("s2", "", std::string(200, 'A'), Alphabet::kDNA));
    store.add(Sequence("s3", "", std::string(100, 'A'), Alphabet::kDNA));
    store.add(Sequence("s4", "", std::string(100, 'A'), Alphabet::kDNA));
    store.add(Sequence("s5", "", std::string(50, 'A'), Alphabet::kDNA));
    store.add(Sequence("s6", "", std::string(50, 'A'), Alphabet::kDNA));

    // 第一条 500 就达到了 target=500
    REQUIRE(store.total_bases() == 1000);
    REQUIRE(store.n50() == 500);
}

// ============================================================================
// TC09: N50 需要跨越多个序列 — 验证跨序列累加正确
// ============================================================================

TEST_CASE("TC09: N50 spans multiple sequences", "[sequence_store]")
{
    SequenceStore store;

    // 长度分布: [300, 200, 200, 200, 100], total=1000, target=500
    // 300→500 跨越到第二条长度 200
    store.add(Sequence("s1", "", std::string(300, 'A'), Alphabet::kDNA));
    store.add(Sequence("s2", "", std::string(200, 'A'), Alphabet::kDNA));
    store.add(Sequence("s3", "", std::string(200, 'A'), Alphabet::kDNA));
    store.add(Sequence("s4", "", std::string(200, 'A'), Alphabet::kDNA));
    store.add(Sequence("s5", "", std::string(100, 'A'), Alphabet::kDNA));

    REQUIRE(store.total_bases() == 1000);
    REQUIRE(store.n50() == 200);
}

// ============================================================================
// TC10: reserve + clear — 验证内存管理
// ============================================================================

TEST_CASE("TC10: reserve and clear", "[sequence_store]")
{
    SequenceStore store;
    store.reserve(1000);

    for (int i = 0; i < 100; ++i) {
        store.add(Sequence("s" + std::to_string(i), "",
                           "ATCG", Alphabet::kDNA));
    }

    REQUIRE(store.count() == 100);

    store.clear();

    REQUIRE(store.count() == 0);
    REQUIRE(store.empty() == true);
    REQUIRE(store.total_bases() == 0);
}

// ============================================================================
// TC11: 迭代器遍历 — 验证范围 for 和顺序一致性
// ============================================================================

TEST_CASE("TC11: Iterator traversal", "[sequence_store]")
{
    SequenceStore store;

    const std::vector<std::string> expected_ids = {
        "s1", "s2", "s3", "s4", "s5"
    };

    for (const auto& id : expected_ids) {
        store.add(Sequence(id, "", "ATCG", Alphabet::kDNA));
    }

    REQUIRE(store.count() == 5);

    // 通过 range-for 遍历收集 id
    std::vector<std::string> collected_ids;
    for (const auto& seq : store) {
        collected_ids.push_back(seq.id());
    }

    REQUIRE(collected_ids == expected_ids);

    // 验证 non-const begin/end 可用
    for (auto it = store.begin(); it != store.end(); ++it) {
        // 确保 non-const 迭代器可解引用
        REQUIRE(it->length() == 4);
    }
}

// ============================================================================
// TC12: 大量序列性能测试 — total_bases ≤ 50ms, n50 ≤ 200ms
// ============================================================================

TEST_CASE("TC12: Large-scale performance test", "[sequence_store][.perf]")
{
    static constexpr size_t kNumSequences = 1000000;
    static constexpr size_t kSeqLength = 100;

    SequenceStore store;
    store.reserve(kNumSequences);

    for (size_t i = 0; i < kNumSequences; ++i) {
        store.add(Sequence("seq" + std::to_string(i), "",
                           std::string(kSeqLength, 'A'), Alphabet::kDNA));
    }

    REQUIRE(store.count() == kNumSequences);

    BENCHMARK("total_bases 1M sequences")
    {
        return store.total_bases();
    };

    BENCHMARK("n50 1M sequences")
    {
        return store.n50();
    };

    // 基准校验：确保结果正确
    REQUIRE(store.total_bases() == kNumSequences * kSeqLength);
    REQUIRE(store.n50() == kSeqLength);
}

// ============================================================================
// 额外测试：verify add supports move semantics from temporary
// ============================================================================

TEST_CASE("Add from rvalue via std::move", "[sequence_store]")
{
    SequenceStore store;

    Sequence seq("chr1", "", "AAAA", Alphabet::kDNA);
    store.add(std::move(seq));

    REQUIRE(store.count() == 1);
    REQUIRE(store.find_by_id("chr1") != nullptr);
}

// ============================================================================
// 额外测试：批量 add 空向量
// ============================================================================

TEST_CASE("Batch add empty vector", "[sequence_store]")
{
    SequenceStore store;
    store.add(Sequence("s1", "", "ATCG", Alphabet::kDNA));
    store.add(std::vector<Sequence>{});

    REQUIRE(store.count() == 1);
    REQUIRE(store.empty() == false);
}

// ============================================================================
// 额外测试：N50 奇数总长 — 验证向上取整
// ============================================================================

TEST_CASE("N50 with odd total length", "[sequence_store]")
{
    SequenceStore store;

    // 3+5+1 = 9 (奇数)，target = (9+1)/2 = 5
    // 降序: [5, 3, 1]，累加: 5 >= 5 → N50=5
    store.add(Sequence("s1", "", std::string(3, 'A'), Alphabet::kDNA));
    store.add(Sequence("s2", "", std::string(5, 'A'), Alphabet::kDNA));
    store.add(Sequence("s3", "", std::string(1, 'A'), Alphabet::kDNA));

    REQUIRE(store.total_bases() == 9);
    REQUIRE(store.n50() == 5);
}
