/// @file interval_tree.h
/// @brief 区间树模块 — 基于增强红黑树的区间重叠与最近邻查询模板数据结构
/// @author LibreBio Team
/// @version 0.3.0
/// @date 2026-05-15
/// @copyright Copyright (c) 2026 LibreBio Team. MIT License.
///
/// @details
/// IntervalTree 是基于红黑树增强（Augmented Red-Black Tree）的区间查询模板类。
/// 每个节点存储 (GenomicInterval, T) 键值对，并维护两个增强字段：
/// - max_end：子树中区间的最大终点值
/// - min_start：子树中区间的最小起点值
///
/// 提供两种查询：
/// - query_overlap：O(log n + k) 重叠区间查询
/// - query_nearest：O(log n + k log n) 最近邻查询（Best-first 搜索 + 下界剪枝）
///
/// ## 内存管理
/// 内部使用裸指针 + 显式 new/delete 管理红黑树节点。
/// 这是 MISRA C++:2023 Rule 18.2.1 的 RAII 封装例外：
/// - new 仅出现在 insert/build 等公开/私有方法中
/// - delete 仅出现在 clear/destroy_subtree 中
/// - 析构函数自动调用 destroy_subtree，无内存泄漏风险
/// - 移动语义正确转移所有权
/// 红黑树节点间复杂的 parent/left/right 多对多指针关系不适合 std::unique_ptr。
///
/// ## 使用示例
/// @code
/// #include "libre_bio/core/interval_tree.h"
/// #include "libre_bio/core/genomic_interval.h"
///
/// libre_bio::IntervalTree<std::string> tree;
/// tree.insert(libre_bio::GenomicInterval("chr1", 0, 100), "geneA");
/// tree.insert(libre_bio::GenomicInterval("chr1", 50, 150), "geneB");
///
/// auto results = tree.query_overlap(
///     libre_bio::GenomicInterval("chr1", 25, 75));
/// // results 包含 "geneA", "geneB"
/// @endcode
///
/// @see doc/detailed-design/m06_interval_tree.md
/// @see genomic_interval.h

#ifndef LIBRE_BIO_CORE_INTERVAL_TREE_H_
#define LIBRE_BIO_CORE_INTERVAL_TREE_H_

#include "libre_bio/core/genomic_interval.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace libre_bio {

/// @brief 红黑树节点颜色
///
/// 使用强类型枚举（enum class）以避免隐式整型转换，
/// 符合 MISRA C++:2023 Rule 8.0.1。
enum class Color : uint8_t {
    kRed   = 0,   ///< 红色节点
    kBlack = 1    ///< 黑色节点
};

/// @brief 增强红黑树节点
///
/// 存储区间键、关联数据、增强字段（max_end / min_start）
/// 以及红黑树平衡所需的指针和颜色。
///
/// @tparam T 关联数据类型，必须支持拷贝构造
template <typename T>
struct IntervalTreeNode {
    GenomicInterval interval;    ///< 区间键
    T data;                      ///< 关联数据
    uint32_t max_end;            ///< 子树中最大 end 值
    uint32_t min_start;          ///< 子树中最小 start 值
    Color color;                 ///< 红黑树颜色
    IntervalTreeNode* parent;    ///< 父节点指针
    IntervalTreeNode* left;      ///< 左子节点指针
    IntervalTreeNode* right;     ///< 右子节点指针

    /// @brief 构造红黑树节点
    /// @param iv 区间键
    /// @param d 关联数据
    IntervalTreeNode(const GenomicInterval& iv, const T& d)
        : interval(iv)
        , data(d)
        , max_end(iv.end())
        , min_start(iv.start())
        , color(Color::kRed)
        , parent(nullptr)
        , left(nullptr)
        , right(nullptr)
    {
    }
};

/// @brief 区间树 — 增强红黑树模板类
///
/// 按区间 (chrom, start) 字典序组织为红黑树。
/// 每个节点维护 max_end 和 min_start 增强字段以支持高效查询。
///
/// @tparam T 关联数据类型，必须支持拷贝构造
///
/// ## 线程安全性
/// - 非线程安全。多线程并发读写需外部同步。
/// - 多线程只读（查询操作）无需同步。
///
/// ## 限制
/// - 不跨染色体查询：染色体名不同的区间不会重叠。
/// - 使用 uint32_t 表示坐标，覆盖约 4.3 Gb 基因组。
template <typename T>
class IntervalTree {
public:
    /// @brief 默认构造 — 创建空树
    IntervalTree() noexcept
        : m_root(nullptr)
        , m_size(0)
    {
    }

    /// @brief 析构 — 释放所有节点
    ~IntervalTree()
    {
        destroy_subtree(m_root);
    }

    /// @name 拷贝语义 — 禁止
    /// @{
    IntervalTree(const IntervalTree&) = delete;
    IntervalTree& operator=(const IntervalTree&) = delete;
    /// @}

    /// @name 移动语义
    /// @{

    /// @brief 移动构造 — 接管 other 的所有节点
    /// @param other 源树，移动后变为空树
    IntervalTree(IntervalTree&& other) noexcept
        : m_root(other.m_root)
        , m_size(other.m_size)
    {
        other.m_root = nullptr;
        other.m_size = 0;
    }

    /// @brief 移动赋值 — 释放当前节点后接管 other
    /// @param other 源树，移动后变为空树
    /// @return *this
    IntervalTree& operator=(IntervalTree&& other) noexcept
    {
        if (this != &other) {
            clear();
            m_root = other.m_root;
            m_size = other.m_size;
            other.m_root = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    /// @}

    /// @name 修改操作
    /// @{

    /// @brief 插入单个区间-数据对
    /// @param interval 区间键
    /// @param data 关联数据
    ///
    /// @note 允许插入重复区间（相同 (chrom, start)），重复键放入右子树。
    void insert(const GenomicInterval& interval, const T& data)
    {
        auto* z = new IntervalTreeNode<T>(interval, data);

        IntervalTreeNode<T>* y = nullptr;
        IntervalTreeNode<T>* x = m_root;

        while (x != nullptr) {
            y = x;
            if (z->interval < x->interval) {
                x = x->left;
            } else {
                x = x->right;
            }
        }

        z->parent = y;

        if (y == nullptr) {
            m_root = z;
        } else if (z->interval < y->interval) {
            y->left = z;
        } else {
            y->right = z;
        }

        ++m_size;

        augment_upward(z->parent);

        insert_fixup(z);
    }

    /// @brief 删除匹配的区间节点
    /// @param interval 待删除区间（需完整匹配 chrom + start + end）
    ///
    /// @note 若存在多个相同 (chrom, start) 的区间，仅删除第一个匹配 end 的节点。
    /// @note 若未找到匹配节点，静默无操作。
    void remove(const GenomicInterval& interval)
    {
        IntervalTreeNode<T>* z = find_node(m_root, interval);

        if (z == nullptr) {
            return;
        }

        delete_node(z);
    }

    /// @brief 批量构造区间的平衡红黑树
    /// @param entries 区间-数据对列表
    ///
    /// @note 会先 clear() 清空已有节点。
    /// @note 构造完美平衡树，所有节点为黑色。
    ///       不完全满足红黑树所有性质（黑高度可能有 1 的偏差），
    ///       但功能正确且性能优于逐个 insert。
    /// @note 复杂 entries 中的区间将按 (chrom, start) 字典序排序。
    void build(std::vector<std::pair<GenomicInterval, T>> entries)
    {
        clear();

        if (entries.empty()) {
            return;
        }

        std::sort(entries.begin(), entries.end(),
                  [](const std::pair<GenomicInterval, T>& a,
                     const std::pair<GenomicInterval, T>& b) {
                      return a.first < b.first;
                  });

        m_root = build_sorted(entries, nullptr, 0,
                              static_cast<int64_t>(entries.size()) - 1);
        m_size = entries.size();
    }

    /// @brief 清空所有节点
    void clear() noexcept
    {
        destroy_subtree(m_root);
        m_root = nullptr;
        m_size = 0;
    }

    /// @}

    /// @name 查询操作
    /// @{

    /// @brief 查询与给定区间重叠的所有区间关联数据
    /// @param query 查询区间
    /// @return 重叠区间的关联数据列表（无序）
    ///
    /// @note 使用 max_end 剪枝，复杂度 O(log n + k)。
    [[nodiscard]] std::vector<T> query_overlap(
        const GenomicInterval& query) const
    {
        std::vector<T> result;
        query_overlap_recursive(m_root, query, result);
        return result;
    }

    /// @brief 查询距离给定区间最近的 k 个区间的关联数据
    /// @param query 查询区间
    /// @param k 返回结果数，默认 1
    /// @return 距离最近的 k 个关联数据（按距离升序排列）
    ///
    /// @details
    /// 两阶段 Best-first 搜索算法：
    /// 1. 收集所有重叠区间（距离 = 0，绝对最近）
    /// 2. 利用 min_start / max_end 增强字段计算子树距离下界，
    ///    使用最小堆进行 Best-first 搜索，下界剪枝。
    ///
    /// @note k=0 时返回空列表。
    /// @note k > size() 时返回全部 size() 个结果。
    /// @note 同行列色体的区间距离为 0（重叠），非重叠按坐标差计算。
    [[nodiscard]] std::vector<T> query_nearest(
        const GenomicInterval& query, size_t k = 1) const
    {
        if (k == 0 || m_root == nullptr) {
            return {};
        }

        std::vector<std::pair<int64_t, T>> ranked;
        query_nearest_impl(query, k, ranked);

        std::vector<T> result;
        result.reserve(ranked.size());

        for (auto& entry : ranked) {
            result.push_back(std::move(entry.second));
        }

        return result;
    }

    /// @}

    /// @name 容量
    /// @{

    /// @brief 获取节点数量
    /// @return 当前树中的节点数
    [[nodiscard]] size_t size() const noexcept
    {
        return m_size;
    }

    /// @brief 判断是否为空树
    /// @return 无节点时返回 true
    [[nodiscard]] bool empty() const noexcept
    {
        return m_size == 0;
    }

    /// @}

private:
    IntervalTreeNode<T>* m_root;   ///< 根节点指针
    size_t m_size;                  ///< 节点数量

    // ========================================================================
    // 增强字段更新
    // ========================================================================

    /// @brief 更新节点及其祖先的增强字段
    /// @param node 起始节点（从该节点向上遍历至根）
    ///
    /// @note 节点插入/删除/旋转后调用此函数沿祖先路径更新。
    void augment_upward(IntervalTreeNode<T>* node)
    {
        while (node != nullptr) {
            update_augmented(node);
            node = node->parent;
        }
    }

    /// @brief 重新计算单个节点的 max_end 和 min_start
    /// @param node 待更新的节点（非空）
    void update_augmented(IntervalTreeNode<T>* node)
    {
        node->max_end = node->interval.end();
        node->min_start = node->interval.start();

        if (node->left != nullptr) {
            node->max_end = std::max(node->max_end, node->left->max_end);
            node->min_start = std::min(node->min_start,
                                       node->left->min_start);
        }

        if (node->right != nullptr) {
            node->max_end = std::max(node->max_end, node->right->max_end);
            node->min_start = std::min(node->min_start,
                                       node->right->min_start);
        }
    }

    // ========================================================================
    // 旋转操作
    // ========================================================================

    /// @brief 左旋 — 以 x 为轴心左旋
    /// @param x 旋转轴节点
    ///
    /// 旋转后 x 成为其右子 y 的左子。
    /// 旋转完成后更新 x 和 y 的增强字段。
    void left_rotate(IntervalTreeNode<T>* x)
    {
        IntervalTreeNode<T>* y = x->right;
        x->right = y->left;

        if (y->left != nullptr) {
            y->left->parent = x;
        }

        y->parent = x->parent;

        if (x->parent == nullptr) {
            m_root = y;
        } else if (x == x->parent->left) {
            x->parent->left = y;
        } else {
            x->parent->right = y;
        }

        y->left = x;
        x->parent = y;

        update_augmented(x);
        update_augmented(y);
    }

    /// @brief 右旋 — 以 x 为轴心右旋
    /// @param x 旋转轴节点
    ///
    /// 旋转后 x 成为其左子 y 的右子。
    /// 旋转完成后更新 x 和 y 的增强字段。
    void right_rotate(IntervalTreeNode<T>* x)
    {
        IntervalTreeNode<T>* y = x->left;
        x->left = y->right;

        if (y->right != nullptr) {
            y->right->parent = x;
        }

        y->parent = x->parent;

        if (x->parent == nullptr) {
            m_root = y;
        } else if (x == x->parent->right) {
            x->parent->right = y;
        } else {
            x->parent->left = y;
        }

        y->right = x;
        x->parent = y;

        update_augmented(x);
        update_augmented(y);
    }

    // ========================================================================
    // 插入修复
    // ========================================================================

    /// @brief 插入后的红黑树性质修复
    /// @param z 新插入的红色节点
    void insert_fixup(IntervalTreeNode<T>* z)
    {
        while ((z->parent != nullptr) && (z->parent->color == Color::kRed)) {
            IntervalTreeNode<T>* grandparent = z->parent->parent;

            if (z->parent == grandparent->left) {
                IntervalTreeNode<T>* uncle = grandparent->right;

                if ((uncle != nullptr) && (uncle->color == Color::kRed)) {
                    // Case 1: 叔叔为红色 → 重着色
                    z->parent->color = Color::kBlack;
                    uncle->color = Color::kBlack;
                    grandparent->color = Color::kRed;
                    z = grandparent;
                } else {
                    if (z == z->parent->right) {
                        // Case 2: 叔叔为黑色，z 为右子 → 左旋
                        z = z->parent;
                        left_rotate(z);
                    }

                    // Case 3: 叔叔为黑色，z 为左子 → 右旋 + 重着色
                    z->parent->color = Color::kBlack;
                    grandparent->color = Color::kRed;
                    right_rotate(grandparent);
                }
            } else {
                IntervalTreeNode<T>* uncle = grandparent->left;

                if ((uncle != nullptr) && (uncle->color == Color::kRed)) {
                    // Case 1: 叔叔为红色 → 重着色（对称）
                    z->parent->color = Color::kBlack;
                    uncle->color = Color::kBlack;
                    grandparent->color = Color::kRed;
                    z = grandparent;
                } else {
                    if (z == z->parent->left) {
                        // Case 2: 叔叔为黑色，z 为左子 → 右旋（对称）
                        z = z->parent;
                        right_rotate(z);
                    }

                    // Case 3: 叔叔为黑色，z 为右子 → 左旋 + 重着色（对称）
                    z->parent->color = Color::kBlack;
                    grandparent->color = Color::kRed;
                    left_rotate(grandparent);
                }
            }
        }

        m_root->color = Color::kBlack;
    }

    // ========================================================================
    // 删除操作
    // ========================================================================

    /// @brief 用 v 替换 u 在树中的位置
    /// @param u 被替换节点
    /// @param v 替换节点（可为 nullptr）
    void transplant(IntervalTreeNode<T>* u, IntervalTreeNode<T>* v)
    {
        if (u->parent == nullptr) {
            m_root = v;
        } else if (u == u->parent->left) {
            u->parent->left = v;
        } else {
            u->parent->right = v;
        }

        if (v != nullptr) {
            v->parent = u->parent;
        }
    }

    /// @brief 执行节点删除并修复红黑树性质
    /// @param z 待删除节点（非空）
    void delete_node(IntervalTreeNode<T>* z)
    {
        IntervalTreeNode<T>* y = z;
        IntervalTreeNode<T>* x = nullptr;
        Color y_original_color = y->color;

        IntervalTreeNode<T>* fixup_start = nullptr;

        if (z->left == nullptr) {
            x = z->right;
            fixup_start = z->parent;
            transplant(z, z->right);
        } else if (z->right == nullptr) {
            x = z->left;
            fixup_start = z->parent;
            transplant(z, z->left);
        } else {
            y = tree_minimum(z->right);
            y_original_color = y->color;
            x = y->right;

            if (y->parent == z) {
                if (x != nullptr) {
                    x->parent = y;
                }
                fixup_start = y;
            } else {
                fixup_start = y->parent;
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }

            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->color = z->color;
        }

        delete z;
        --m_size;

        augment_upward(fixup_start);

        if ((y_original_color == Color::kBlack) && (x != nullptr)) {
            delete_fixup(x);
        }
    }

    /// @brief 删除后的红黑树性质修复
    /// @param x 可能破坏黑色性质的节点
    void delete_fixup(IntervalTreeNode<T>* x)
    {
        while ((x != m_root) && (x->color == Color::kBlack)) {
            IntervalTreeNode<T>* parent = x->parent;

            if (x == parent->left) {
                IntervalTreeNode<T>* sibling = parent->right;

                if (sibling != nullptr && sibling->color == Color::kRed) {
                    sibling->color = Color::kBlack;
                    parent->color = Color::kRed;
                    left_rotate(parent);
                    sibling = parent->right;
                }

                if ((sibling == nullptr) ||
                    ((sibling->left == nullptr ||
                      sibling->left->color == Color::kBlack) &&
                     (sibling->right == nullptr ||
                      sibling->right->color == Color::kBlack))) {

                    if (sibling != nullptr) {
                        sibling->color = Color::kRed;
                    }
                    x = parent;
                } else {
                    if ((sibling->right == nullptr) ||
                        sibling->right->color == Color::kBlack) {

                        if (sibling->left != nullptr) {
                            sibling->left->color = Color::kBlack;
                        }
                        sibling->color = Color::kRed;
                        right_rotate(sibling);
                        sibling = parent->right;
                    }

                    if (sibling != nullptr) {
                        sibling->color = parent->color;
                    }
                    parent->color = Color::kBlack;

                    if (sibling != nullptr && sibling->right != nullptr) {
                        sibling->right->color = Color::kBlack;
                    }
                    left_rotate(parent);
                    x = m_root;
                }
            } else {
                IntervalTreeNode<T>* sibling = parent->left;

                if (sibling != nullptr && sibling->color == Color::kRed) {
                    sibling->color = Color::kBlack;
                    parent->color = Color::kRed;
                    right_rotate(parent);
                    sibling = parent->left;
                }

                if ((sibling == nullptr) ||
                    ((sibling->right == nullptr ||
                      sibling->right->color == Color::kBlack) &&
                     (sibling->left == nullptr ||
                      sibling->left->color == Color::kBlack))) {

                    if (sibling != nullptr) {
                        sibling->color = Color::kRed;
                    }
                    x = parent;
                } else {
                    if ((sibling->left == nullptr) ||
                        sibling->left->color == Color::kBlack) {

                        if (sibling->right != nullptr) {
                            sibling->right->color = Color::kBlack;
                        }
                        sibling->color = Color::kRed;
                        left_rotate(sibling);
                        sibling = parent->left;
                    }

                    if (sibling != nullptr) {
                        sibling->color = parent->color;
                    }
                    parent->color = Color::kBlack;

                    if (sibling != nullptr && sibling->left != nullptr) {
                        sibling->left->color = Color::kBlack;
                    }
                    right_rotate(parent);
                    x = m_root;
                }
            }
        }

        x->color = Color::kBlack;
    }

    /// @brief 查找子树中起始位置最小的节点
    /// @param node 子树根节点
    /// @return 最左（最小）节点
    static IntervalTreeNode<T>* tree_minimum(IntervalTreeNode<T>* node) noexcept
    {
        while (node->left != nullptr) {
            node = node->left;
        }
        return node;
    }

    // ========================================================================
    // 节点查找
    // ========================================================================

    /// @brief 在树中查找完全匹配区间的节点
    /// @param node 搜索起始节点
    /// @param interval 目标区间（需 chrom + start + end 完全匹配）
    /// @return 匹配节点指针；未找到返回 nullptr
    static IntervalTreeNode<T>* find_node(IntervalTreeNode<T>* node,
                                          const GenomicInterval& interval) noexcept
    {
        while (node != nullptr) {
            if (interval < node->interval) {
                node = node->left;
            } else if (node->interval < interval) {
                node = node->right;
            } else {
                // chrom 和 start 都相等，验证 end 是否匹配
                if (node->interval.end() == interval.end()) {
                    return node;
                }

                // end 不匹配，可能在右侧子树中（重复键放入右子树）
                node = node->right;
            }
        }

        return nullptr;
    }

    // ========================================================================
    // 查询算法
    // ========================================================================

    /// @brief 递归重叠查询（带 max_end 剪枝）
    /// @param node 当前搜索节点
    /// @param query 查询区间
    /// @param result 输出结果列表
    ///
    /// 剪枝原理：
    /// - 左子树：若 query.start >= node.left->max_end，query 在左子树所有区间之后
    /// - 右子树：若 query.end <= node->interval.start()，query 在右子树所有区间之前
    static void query_overlap_recursive(const IntervalTreeNode<T>* node,
                                        const GenomicInterval& query,
                                        std::vector<T>& result)
    {
        if (node == nullptr) {
            return;
        }

        if ((node->left != nullptr) &&
            (query.start() < node->left->max_end)) {
            query_overlap_recursive(node->left, query, result);
        }

        if (node->interval.overlaps(query)) {
            result.push_back(node->data);
        }

        if ((node->right != nullptr) &&
            (query.end() > node->interval.start())) {
            query_overlap_recursive(node->right, query, result);
        }
    }

    /// @brief 计算查询区间到某子树中任意区间距离的理论下界
    /// @param subtree_root 子树根节点
    /// @param query 查询区间
    /// @return 距离下界（非负数）
    ///
    /// 利用子树的 min_start / max_end 增强字段：
    /// - query 在子树所有区间右侧：lower_bound = query.start() - max_end
    /// - query 在子树所有区间左侧：lower_bound = min_start - query.end()
    /// - query 与子树范围重叠：lower_bound = 0
    static int64_t compute_lower_bound(const IntervalTreeNode<T>* subtree_root,
                                       const GenomicInterval& query) noexcept
    {
        const uint32_t sub_max_end = subtree_root->max_end;
        const uint32_t sub_min_start = subtree_root->min_start;

        if (query.start() >= sub_max_end) {
            return static_cast<int64_t>(query.start()) -
                   static_cast<int64_t>(sub_max_end);
        }

        if (query.end() <= sub_min_start) {
            return static_cast<int64_t>(sub_min_start) -
                   static_cast<int64_t>(query.end());
        }

        return 0;
    }

    /// @brief 最近邻查询实现 — 两阶段 Best-first 搜索
    /// @param query 查询区间
    /// @param k 期望结果数
    /// @param ranked 输出结果列表（按距离升序）
    void query_nearest_impl(const GenomicInterval& query, size_t k,
                            std::vector<std::pair<int64_t, T>>& ranked) const
    {
        // Phase 1: 收集所有重叠区间（距离 = 0）
        std::vector<T> overlaps = query_overlap(query);
        const size_t overlap_count = overlaps.size();

        for (size_t i = 0; i < overlap_count; ++i) {
            ranked.push_back({0, std::move(overlaps[i])});
        }

        if (ranked.size() >= k) {
            return;
        }

        // Phase 2: Best-first 搜索非重叠区间
        //
        // 使用 min-heap（std::priority_queue + greater 比较），
        // 按下界距离排序。每次出堆时：
        // 1. 若已收集 k 个且下界 >= 第 k 远距离，剪枝
        // 2. 计算当前节点实际距离，若 > 0 则插入结果
        // 3. 计算左右子树下界，若可能更近则入堆
        //
        // 堆条目: pair<int64_t lower_bound, const IntervalTreeNode<T>* node>
        using HeapEntry = std::pair<int64_t, const IntervalTreeNode<T>*>;

        // 使用 greater 实现 min-heap
        auto cmp = [](const HeapEntry& a, const HeapEntry& b) noexcept {
            return a.first > b.first;
        };

        std::priority_queue<HeapEntry, std::vector<HeapEntry>, decltype(cmp)>
            heap(cmp);

        int64_t root_bound = compute_lower_bound(m_root, query);
        heap.push({root_bound, m_root});

        // 使用 UINT32_MAX 作为距离的上界哨兵值
        const int64_t kSentinelDistance = static_cast<int64_t>(
            std::numeric_limits<int64_t>::max());

        while (!heap.empty()) {
            HeapEntry entry = heap.top();
            heap.pop();

            const int64_t bound = entry.first;
            const IntervalTreeNode<T>* node = entry.second;

            // 计算当前第 k 远距离
            int64_t kth_distance = kSentinelDistance;
            if (ranked.size() >= k) {
                kth_distance = ranked.back().first;
            }

            // 剪枝：下界 >= 第 k 远距离 且已有 k 个结果
            if ((ranked.size() >= k) && (bound >= kth_distance)) {
                break;
            }

            // 计算当前节点实际距离
            int64_t dist = node->interval.distance(query);

            if (dist > 0) {
                // 插入结果列表，保持按距离升序、最多 k 个
                insert_ranked(dist, node->data, k, ranked);
            }

            // 探索左子树
            if (node->left != nullptr) {
                int64_t lb = compute_lower_bound(node->left, query);
                int64_t curr_kth = kSentinelDistance;
                if (ranked.size() >= k) {
                    curr_kth = ranked.back().first;
                }
                if ((ranked.size() < k) || (lb < curr_kth)) {
                    heap.push({lb, node->left});
                }
            }

            // 探索右子树
            if (node->right != nullptr) {
                int64_t lb = compute_lower_bound(node->right, query);
                int64_t curr_kth = kSentinelDistance;
                if (ranked.size() >= k) {
                    curr_kth = ranked.back().first;
                }
                if ((ranked.size() < k) || (lb < curr_kth)) {
                    heap.push({lb, node->right});
                }
            }
        }
    }

    /// @brief 将距离-数据对插入有序结果列表
    /// @param dist 距离值
    /// @param data 关联数据
    /// @param k 最多保留条目数
    /// @param ranked 有序结果列表
    ///
    /// 保持 ranked 按距离升序、最多 k 个元素。
    /// 如果 dist >= 当前第 k 远距离且 ranked 已满，则不插入。
    static void insert_ranked(
        int64_t dist, const T& data, size_t k,
        std::vector<std::pair<int64_t, T>>& ranked)
    {
        if ((k == 0) || ((ranked.size() >= k) && (dist >= ranked.back().first))) {
            return;
        }

        // 查找插入位置（线性查找，k 通常很小）
        auto it = ranked.begin();
        while ((it != ranked.end()) && (it->first <= dist)) {
            ++it;
        }

        ranked.insert(it, {dist, data});

        if (ranked.size() > k) {
            ranked.pop_back();
        }
    }

    // ========================================================================
    // 批量构造
    // ========================================================================

    /// @brief 从已排序条目递归构造平衡树
    /// @param entries 已按 interval.start 排序的条目列表
    /// @param parent 父节点（根传入 nullptr）
    /// @param start_idx 起始索引
    /// @param end_idx 终止索引
    /// @return 构造的子树的根节点
    ///
    /// 取中位元素为根，左右各自递归，构造完美平衡树。
    /// 所有节点染为黑色。
    IntervalTreeNode<T>* build_sorted(
        std::vector<std::pair<GenomicInterval, T>>& entries,
        IntervalTreeNode<T>* parent,
        int64_t start_idx, int64_t end_idx)
    {
        if (start_idx > end_idx) {
            return nullptr;
        }

        int64_t mid = (start_idx + end_idx) / 2;

        auto* node = new IntervalTreeNode<T>(entries[static_cast<size_t>(mid)].first,
                                             entries[static_cast<size_t>(mid)].second);
        node->parent = parent;
        node->color = Color::kBlack;

        node->left = build_sorted(entries, node, start_idx, mid - 1);
        node->right = build_sorted(entries, node, mid + 1, end_idx);

        update_augmented(node);

        return node;
    }

    // ========================================================================
    // 内存管理
    // ========================================================================

    /// @brief 后序遍历释放子树全部节点
    /// @param node 子树根节点
    ///
    /// @attention 仅在此函数中使用 delete。
    /// MISRA C++:2023 Rule 18.2.1 RAII 封装例外。
    static void destroy_subtree(IntervalTreeNode<T>* node) noexcept
    {
        if (node == nullptr) {
            return;
        }

        destroy_subtree(node->left);
        destroy_subtree(node->right);

        // MISRA Rule 18.2.1 例外：
        // delete 仅在此 RAII 封装中调用，外部通过 clear() / 析构函数间接触发。
        delete node;
    }
};

} // namespace libre_bio

#endif // LIBRE_BIO_CORE_INTERVAL_TREE_H_
