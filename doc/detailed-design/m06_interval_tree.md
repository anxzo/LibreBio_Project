# M06: IntervalTree 详细设计

> **版本**: 0.3.0
> **日期**: 2026-05-15
> **对应概要设计**: doc/high-level-design.md §3.7
> **所属层**: Core Data Types

---

## 1. 模块概述

### 1.1 职责

高效区间重叠查询与最近邻查询模板数据结构。基于红黑树增强实现（Augmented Red-Black Tree）。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/core/interval_tree.h` |

> **变更说明 (v0.3.0)**: 模板实现全部置于头文件，不创建独立的 .cpp 文件。
> C++ 模板类需要编译器在实例化时看到完整定义，头文件包含全部实现是标准做法。

### 1.3 依赖

- M04 (GenomicInterval)
- STL

---

## 2. 背景知识

### 2.1 什么是区间树

区间树（Interval Tree）是一种用于存储区间并在其中快速查找与给定区间重叠的所有区间的数据结构。与朴素 O(n) 遍历不同，区间树通过预处理将重叠查询优化到 O(log n + k)，其中 k 是命中数量。

### 2.2 增强红黑树原理

常规红黑树按区间的起始位置（start）作为键进行排序。每个节点额外存储两个增强字段：

- **max_end**: 子树中区间的最大终点值
- **min_start**: 子树中区间的最小起点值

```
node.max_end = max(
    node.interval.end(),
    left_child.max_end  (若存在),
    right_child.max_end (若存在)
)

node.min_start = min(
    node.interval.start(),
    left_child.min_start  (若存在),
    right_child.min_start (若存在)
)
```

**双重增强字段的作用**:

| 字段 | query_overlap 用途 | query_nearest 用途 |
|------|-------------------|-------------------|
| max_end | 判断左子树是否可能重叠 | 计算 query 到子树的最小距离下界 |
| min_start | — | 计算 query 到子树的最小距离下界 |

查询时，若 `query.start < node.left.max_end`，则左子树可能存在重叠区间，需要递归搜索。若 `query.interval` 与 `node.interval` 重叠，则将 node.data 加入结果。若 `query.end > node.interval.start`，则右子树可能存在重叠区间，需要递归搜索。

---

## 3. 数据结构设计

### 3.1 节点结构

```cpp
template <typename T>
struct IntervalTreeNode {
    GenomicInterval interval;    // 区间键
    T data;                      // 关联数据
    uint32_t max_end;            // 子树中最大 end 值
    uint32_t min_start;          // 子树中最小 start 值
    Color color;                 // 红黑树颜色 (RED / BLACK)
    IntervalTreeNode* parent;    // 父节点指针
    IntervalTreeNode* left;      // 左子节点
    IntervalTreeNode* right;     // 右子节点
};
```

### 3.2 红黑树颜色

```cpp
enum class Color : uint8_t {
    kRed   = 0,
    kBlack = 1
};
```

### 3.3 树结构

```cpp
template <typename T>
class IntervalTree {
public:
    IntervalTree() = default;

    void insert(const GenomicInterval& interval, const T& data);
    void remove(const GenomicInterval& interval);
    void build(std::vector<std::pair<GenomicInterval, T>> entries);

    [[nodiscard]] std::vector<T> query_overlap(
        const GenomicInterval& query) const;
    [[nodiscard]] std::vector<T> query_nearest(
        const GenomicInterval& query, size_t k = 1) const;

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear() noexcept;

    ~IntervalTree();

    IntervalTree(const IntervalTree&) = delete;
    IntervalTree& operator=(const IntervalTree&) = delete;
    IntervalTree(IntervalTree&& other) noexcept;
    IntervalTree& operator=(IntervalTree&& other) noexcept;

private:
    IntervalTreeNode<T>* m_root;   // 根节点指针
    size_t m_size;                  // 节点数量
};
```

> **设计变更 (v0.3.0)**: 不使用哨兵节点（sentinel/NIL），改用 nullptr 表示空子节点。
> 哨兵方案（static s_nil）在模板类中存在静态成员实例化问题。nullptr 方案
> 代码更简洁、更符合现代 C++ 习惯，且不增加指针判空复杂度。

### 3.4 内存管理

使用裸指针 + 显式 new/delete。

**理由**:

1. 红黑树节点间是复杂的多对多指针关系（parent/left/right），不适合 std::unique_ptr
2. 在 C++17 中缺乏标准的图节点内存管理机制
3. 严格遵守 MISRA C++:2023 Rule 18.2.1 的"RAII 封装下允许使用 new/delete"例外条款
4. 所有节点的 new 创建和 delete 释放均在 IntervalTree 的私有方法中，外部调用方不可见
5. 析构函数通过后序遍历确保所有节点被释放，无内存泄漏风险
6. 移动构造函数/移动赋值运算符正确转移所有权，原对象置为空树

> **MISRA Rule 18.2.1 例外说明**: MISRA C++:2023 Rule 18.2.1 禁止裸 new/delete，
> 但允许在 RAII 类内部作为实现细节使用。IntervalTree 满足此例外条件：
> (a) new 仅出现在 insert/build 等私有/公开方法中，外部调用方无需手动管理内存；
> (b) delete 仅出现在 clear/destroy_subtree 方法中，析构函数自动调用；
> (c) 移动语义正确转移所有权，避免 double-free。

**析构函数逻辑**:

```
~IntervalTree():
    destroy_subtree(m_root)
    m_root = nullptr
    m_size = 0
```

---

## 4. 算法设计

### 4.1 insert(interval, data)

```
输入: interval (GenomicInterval), data (T)
处理:
  1. 创建新节点 z
  2. 标准红黑树插入（按 (chrom, start) 字典序为键）
  3. 沿插入路径回溯，更新各节点的 max_end 和 min_start
  4. 调用 insert_fixup(z) 修复红黑树性质
复杂度: O(log n)

增强字段更新公式（节点 x）:
  x.max_end = max(
      x.interval.end(),
      x.left ? x.left->max_end : 0,
      x.right ? x.right->max_end : 0
  )
  
  x.min_start = min(
      x.interval.start(),
      x.left ? x.left->min_start : UINT32_MAX,
      x.right ? x.right->min_start : UINT32_MAX
  )

insert_fixup 处理三种违反红黑树性质的情况:
  Case 1: z 的叔节点为红色 → 重着色，问题上移
  Case 2: z 的叔节点为黑色且 z 为右子 → 左旋
  Case 3: z 的叔节点为黑色且 z 为左子 → 右旋 + 重着色
```

### 4.2 build(entries) — 批量构造

```
输入: entries (vector<pair<GenomicInterval, T>>)
处理:
  1. 按 (chrom, start) 字典序排序 entries
  2. 递归构造平衡红黑树: 取中位元素为根，左右各自递归
  3. O(n) 计算所有节点的 max_end 和 min_start（后序遍历）
复杂度: O(n log n)（排序主导），构造平衡树为 O(n)

算法细节:
  function build_sorted(sorted_entries, parent, start_idx, end_idx):
      if start_idx > end_idx → return nullptr
      mid = (start_idx + end_idx) / 2
      node = new Node(sorted_entries[mid])
      node.parent = parent
      node.color = kBlack  (完美平衡，全黑节点)
      node.left = build_sorted(sorted_entries, node, start_idx, mid - 1)
      node.right = build_sorted(sorted_entries, node, mid + 1, end_idx)
      // 更新增强字段
      update_augmented(node)
      return node

注意: build() 构造的树为完美平衡，节点全部为黑色。
      虽然不完全符合红黑树性质（黑高度不完全相等可能有 1 的偏差），
      但不影响正确性且性能优于逐个 insert 的 O(n log n)。
```

### 4.3 query_overlap(query_interval) — 核心查询

```
输入: query_interval (const GenomicInterval&)
处理:
  递归搜索重叠节点
输出: vector<T> 包含所有重叠区间的关联数据
复杂度: O(min(n, k log n))，实际接近 O(log n + k)，k = 命中数量

算法细节:
  function query_overlap_recursive(node, query, result):
      if node == nullptr → return
      // 剪枝条件 1: 左子树可能重叠
      if node.left != nullptr 且 query.start() < node.left->max_end:
          query_overlap_recursive(node.left, query, result)
      // 检查当前节点
      if node->interval.overlaps(query):
          result.push_back(node->data)
      // 剪枝条件 2: 右子树可能重叠
      if node.right != nullptr 且 query.end() > node->interval.start():
          query_overlap_recursive(node.right, query, result)

剪枝原理:
  左子树剪枝: 若 query.start >= node.left->max_end，
              则 query 的起点在所有左子树区间终点之后，不可能重叠。
  右子树剪枝: 若 query.end <= node->interval.start()，
              query 在所有右子树区间的起点之前结束，不可能重叠。
```

### 4.4 query_nearest(query_interval, k) — 最近邻查询

```
输入: query_interval (const GenomicInterval&), k (size_t)
处理:
  两阶段 Best-first 搜索
输出: vector<T> 包含距离最近的 k 个区间的关联数据（按距离升序）
复杂度: O(log n + k log n)

算法细节:

Phase 1: 收集所有重叠区间（距离 = 0）
  overlaps = query_overlap(query)
  if overlaps.size() >= k → return overlaps[0:k]
  
Phase 2: Best-first 搜索非重叠区间
  使用 std::priority_queue（min-heap），
  每个条目为 (distance_lower_bound, node_ptr)，按下界距离排序。
  
  while heap is not empty:
      (bound, node) = heap.pop()
      
      // 剪枝: 当前下界 >= 第 k 远距离 且已收集 k 个结果
      if results.size() >= k and bound >= kth_distance:
          break
      
      // 计算当前节点区间到 query 的实际距离
      dist = node->interval.distance(query)
      if dist > 0:  // 非重叠区间
          将 (dist, node->data) 插入 results（保持有序，最多 k 个）
      
      // 探索左子树
      if node->left != nullptr:
          lb_left = compute_lower_bound(node->left, query)
          if results.size() < k or lb_left < kth_distance:
              heap.push((lb_left, node->left))
      
      // 探索右子树
      if node->right != nullptr:
          lb_right = compute_lower_bound(node->right, query)
          if results.size() < k or lb_right < kth_distance:
              heap.push((lb_right, node->right))
  
  返回 results

距离下界计算函数 compute_lower_bound(subtree_root, query):
  利用子树增强字段 min_start 和 max_end 计算 query 到子树中
  任意区间距离的理论下界。
  
  情况 1: query.start() >= subtree.max_end
      子树所有区间在 query 左侧 → lower_bound = query.start() - subtree.max_end
  
  情况 2: query.end() <= subtree.min_start
      子树所有区间在 query 右侧 → lower_bound = subtree.min_start - query.end()
  
  情况 3: query 与子树范围重叠
      lower_bound = 0（保守估计，实际可能为非重叠区间）

剪枝原理:
  - max_end 提供"query 在子树右侧"的距离下界
  - min_start 提供"query 在子树左侧"的距离下界
  - 若下界 >= 当前第 k 远距离，子树中不会存在更近的区间
```

### 4.5 旋转操作

```
left_rotate(x):
  y = x->right
  x->right = y->left
  if y->left != nullptr: y->left->parent = x
  y->parent = x->parent
  if x->parent == nullptr: m_root = y
  else if x == x->parent->left: x->parent->left = y
  else: x->parent->right = y
  y->left = x
  x->parent = y
  // 更新增强字段（先子后父）
  update_augmented(x)
  update_augmented(y)

right_rotate 为对称实现
```

### 4.6 update_augmented(node) — 增强字段更新

```
输入: node (IntervalTreeNode<T>*)
处理:
  从 node 的 interval、left、right 重新计算 max_end 和 min_start
前置条件: node != nullptr
  
  node->max_end = node->interval.end()
  node->min_start = node->interval.start()
  
  if node->left != nullptr:
      node->max_end = max(node->max_end, node->left->max_end)
      node->min_start = min(node->min_start, node->left->min_start)
  
  if node->right != nullptr:
      node->max_end = max(node->max_end, node->right->max_end)
      node->min_start = min(node->min_start, node->right->min_start)
```

### 4.7 remove(interval)

```
输入: interval (const GenomicInterval&)
处理:
  1. 按 (chrom, start) 查找节点（红黑树标准二叉查找）
  2. 若未找到匹配的 interval（需完全匹配 chrom + start + end）→ 无操作
  3. 红黑树标准删除 + 修复
  4. 沿删除路径回溯更新 max_end 和 min_start
复杂度: O(log n)

注意: 若存在多个相同 (chrom, start) 的区间（不同 end），查找可能不精确。
      实际实现中按完整的区间匹配（chrom + start + end）。
```

### 4.8 析构函数

```
~IntervalTree():
  function destroy_subtree(node):
      if node == nullptr → return
      destroy_subtree(node->left)
      destroy_subtree(node->right)
      delete node
  destroy_subtree(m_root)
```

### 4.9 clear()

```
清除所有节点，重置为空树。
复用 destroy_subtree 逻辑。
复杂度: O(n)
```

### 4.10 移动语义

```
IntervalTree(IntervalTree&& other) noexcept:
    m_root = other.m_root
    m_size = other.m_size
    other.m_root = nullptr
    other.m_size = 0

IntervalTree& operator=(IntervalTree&& other) noexcept:
    if this != &other:
        clear()
        m_root = other.m_root
        m_size = other.m_size
        other.m_root = nullptr
        other.m_size = 0
    return *this
```

---

## 5. 复杂度总结

| 操作 | 时间复杂度 | 空间复杂度 |
|------|-----------|-----------|
| 空树构造 | O(1) | O(1) |
| insert | O(log n) | O(1) 增加值 |
| build | O(n log n) | O(n) |
| query_overlap | O(log n + k) | O(k) 输出 |
| query_nearest | O(log n + k log n) | O(k + log n) 输出 + 堆 |
| remove | O(log n) | O(1) |
| destroy_subtree | O(n) | — |
| clear | O(n) | — |
| 移动构造/赋值 | O(1) | O(1) |

---

## 6. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 空树查询 | 返回空 vector |
| 重复区间插入 | 允许重复（红黑树支持重复键，放在右子树） |
| 删除不存在的区间 | 无操作（静默） |
| k = 0 的最近邻查询 | 返回空 vector |
| query_nearest 时 k > size() | 返回全部 size() 个结果 |
| 模板参数 T 不支持拷贝 | 编译期错误 |

---

## 7. 测试设计

### 7.1 测试环境

- 测试文件: `test/core/interval_tree_test.cpp`
- 依赖: M04 (GenomicInterval)
- 模板实例化: IntervalTree\<std::string\>

### 7.2 测试用例

#### TC01: 空树查询

| 项目 | 内容 |
|------|------|
| 输入 | 空树, query=[0, 100) |
| 预期 | query_overlap 返回空 vector, size()==0, empty()==true |
| 类型 | 边界条件 |

#### TC02: 单节点插入 + 查询命中

| 项目 | 内容 |
|------|------|
| 输入 | insert([0, 100), "A"), query([50, 150)) |
| 预期 | 返回 {"A"} |
| 类型 | 正常路径 |

#### TC03: 单节点插入 + 查询未命中

| 项目 | 内容 |
|------|------|
| 输入 | insert([0, 100), "A"), query([100, 200)) |
| 预期 | 返回空 vector |
| 类型 | 正常路径 |

#### TC04: 多节点 — 查询命中多个

| 项目 | 内容 |
|------|------|
| 输入 | insert([0, 50, "A"]), insert([25, 75, "B"]), insert([60, 100, "C"]), query([30, 70)) |
| 预期 | 返回 {"A", "B", "C"}（三者均与 [30, 70) 重叠） |
| 类型 | 正常路径 |

#### TC05: 多节点 — 查询命中部分

| 项目 | 内容 |
|------|------|
| 输入 | insert([0, 50, "A"]), insert([40, 60, "B"]), insert([200, 300, "C"]), query([0, 45)) |
| 预期 | 返回 {"A", "B"} |
| 类型 | 正常路径 |

#### TC06: 批量 build 构造

| 项目 | 内容 |
|------|------|
| 输入 | build({([0,50],"A"), ([25,75],"B"), ([60,100],"C")}), query([30, 70)) |
| 预期 | 返回 3 个结果（同 TC04） |
| 类型 | 正常路径 |

#### TC07: 批量 build 性能（规模测试）

| 项目 | 内容 |
|------|------|
| 输入 | build 1,000,000 个区间，执行 100 次随机查询 |
| 预期 | build 在 2s 内完成，每次 query_overlap 平均 < 10μs |
| 类型 | 性能基准 |

#### TC08: remove 删除已有节点

| 项目 | 内容 |
|------|------|
| 输入 | insert([0,100,"A"]), insert([50,150,"B"]), remove([0,100...]) |
| 预期 | size()==1, query([0, 60)) 只返回 {"B"} |
| 类型 | 正常路径 |

#### TC09: remove 删除不存在的节点

| 项目 | 内容 |
|------|------|
| 输入 | insert([0,100,"A"]), remove([999, 1000, ...]) |
| 预期 | size()==1, 无异常 |
| 类型 | 边界条件 |

#### TC10: clear 后查询

| 项目 | 内容 |
|------|------|
| 输入 | insert 多个 → clear() → query |
| 预期 | size()==0, query 返回空 |
| 类型 | 正常路径 |

#### TC11: 重复区间插入

| 项目 | 内容 |
|------|------|
| 输入 | insert([0,100,"A"]), insert([0,100,"B"]) |
| 预期 | size()==2, query([0, 50)) 返回 {"A", "B"} |
| 类型 | 正常路径 |

#### TC12: 大范围查询（全命中）

| 项目 | 内容 |
|------|------|
| 输入 | insert 10 个不重叠区间 [0,10), [10,20)...[90,100), query([0, 100)) |
| 预期 | 返回 10 个结果 |
| 类型 | 边界条件 |

#### TC13: query_nearest 基本功能

| 项目 | 内容 |
|------|------|
| 输入 | insert 5 个不重叠区间 [0,10,"A"], [20,30,"B"], [50,60,"C"], [80,90,"D"], [200,300,"E"] |
| 预期 | query_nearest([15,18), k=3) 返回 {"B", "A", "C"} |
| 类型 | 正常路径 |

#### TC14: 移动语义

| 项目 | 内容 |
|------|------|
| 输入 | 构造 IntervalTree，插入 3 个节点，移动至新对象 |
| 预期 | 新对象 size()==3，原对象 size()==0, empty()==true |
| 类型 | 正常路径 |

---

## 8. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 18.2.1 | new/delete 管控 | 仅在 RAII 封装内使用，析构函数释放。注释中详细说明例外理由 |
| Rule 8.0.1 | enum class | Color 使用 enum class |
| Rule 9.1.1 | 使用固定宽度整数 | max_end / min_start 使用 uint32_t |
| Rule 18.0.1 | 禁用异常 | 所有方法 noexcept 或通过返回值报告 |
| Rule 10.0.3 | const 正确性 | query 类方法声明 const |
| Rule 12.0.1 | 特殊成员函数 | 显式删除拷贝，显式定义移动和析构 |
| Rule 0.0.1 | 不可达语句 | 确保所有代码路径可达 |
