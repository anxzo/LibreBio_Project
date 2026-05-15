# M06: IntervalTree 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.7
> **所属层**: Core Data Types

---

## 1. 模块概述

### 1.1 职责

高效区间重叠查询模板数据结构。基于红黑树增强实现（Augmented Red-Black Tree）。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/core/interval_tree.h` |
| 实现文件 | `src/core/interval_tree.cpp`（模板实例化） |

### 1.3 依赖

- M04 (GenomicInterval)
- STL

---

## 2. 背景知识

### 2.1 什么是区间树

区间树（Interval Tree）是一种用于存储区间并在其中快速查找与给定区间重叠的所有区间的数据结构。与朴素 O(n) 遍历不同，区间树通过预处理将重叠查询优化到 O(log n + k)，其中 k 是命中数量。

### 2.2 增强红黑树原理

常规红黑树按区间的起始位置（start）作为键进行排序。每个节点额外存储一个**子树最大终点值**（max_end）：

```
node.max_end = max(
    node.interval.end,
    left_child.max_end  (若存在),
    right_child.max_end (若存在)
)
```

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
    Color color;                  // 红黑树颜色 (RED / BLACK)
    IntervalTreeNode* parent;     // 父节点指针
    IntervalTreeNode* left;       // 左子节点
    IntervalTreeNode* right;      // 右子节点
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
private:
    IntervalTreeNode<T>* m_root;     // 根节点 (nullptr 表示空树)
    size_t m_size;                   // 节点数量

    static IntervalTreeNode<T>* s_nil; // 哨兵节点 (sentinel/NIL, 所有叶子指向它)
};
```

### 3.4 哨兵节点（Sentinel/NIL）

使用静态单例哨兵节点表示所有空子节点。哨兵节点特征:
- color = kBlack
- left = right = parent = s_nil（自引用）
- max_end = 0
- interval 未初始化

使用哨兵节点而非 nullptr 可以简化红黑树平衡操作的代码，避免大量空指针检查。

### 3.5 内存管理

使用裸指针 + 显式 new/delete。

理由:
1. 红黑树节点间是复杂的多对多指针关系，不适合 unique_ptr
2. 在 C++17 中缺乏标准的图节点内存管理机制
3. MISRA C++:2023 禁止裸 new/delete (Rule 18.2.1)，但允许在 RAII 封装下使用
4. 所有节点在析构函数中通过后序遍历释放

析构函数:
```
~IntervalTree():
    post_order_delete(m_root)
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
  2. 标准红黑树插入（按 interval.start 为键）
  3. 沿插入路径回溯，更新各节点的 max_end
  4. 调用 insert_fixup(z) 修复红黑树性质
复杂度: O(log n)

max_end 更新公式（节点 x）:
  x.max_end = max(
      x.interval.end(),
      x.left.max_end(),
      x.right.max_end()
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
  1. 按 interval.start 排序 entries
  2. 递归构造平衡红黑树: 取中位元素为根，左右各自递归
  3. O(n) 计算所有节点的 max_end（后序遍历）
复杂度: O(n log n)（排序主导），构造平衡树为 O(n)

算法细节:
  function build_sorted(sorted_entries, start_idx, end_idx):
      if start_idx > end_idx → return s_nil
      mid = (start_idx + end_idx) / 2
      node = new Node(sorted_entries[mid])
      node.left = build_sorted(sorted_entries, start_idx, mid - 1)
      node.right = build_sorted(sorted_entries, mid + 1, end_idx)
      node.color = kBlack  (根路径等长，构造完美平衡)
      return node

注意: build() 构造的树可能不符合红黑树性质（完美平衡可能产生全黑树），
      但功能正确且性能优于逐个 insert。
```

### 4.3 query_overlap(query_interval) — 核心查询

```
输入: query_interval (const GenomicInterval&)
处理:
  递归搜索重叠节点
输出: vector<T> 包含所有重叠区间的关联数据
复杂度: O(log n + k)，k = 命中数量

算法细节:
  function query_overlap_recursive(node, query, result):
      if node == s_nil → return
      // 剪枝条件 1: 左子树可能重叠
      if node.left != s_nil 且 query.start() < node.left.max_end:
          query_overlap_recursive(node.left, query, result)
      // 检查当前节点
      if node.interval.overlaps(query):
          result.push_back(node.data)
      // 剪枝条件 2: 右子树可能重叠
      if node.right != s_nil 且 query.end() > node.interval.start():
          // 使用 start 而非 end 判断: 只要 query.end > node.start，
          // query 可能与右子树中起点更大的区间重叠
          query_overlap_recursive(node.right, query, result)

剪枝原理:
  左子树剪枝: 若 query.start >= node.left.max_end，
              则 query 的起点在所有左子树区间终点之后，不可能重叠。
  右子树剪枝: 若 query.end <= node.interval.start()，
              query 在所有右子树区间的起点之前结束，不可能重叠。
```

### 4.4 旋转操作

```
left_rotate(x):
  y = x.right
  x.right = y.left
  if y.left != s_nil: y.left.parent = x
  y.parent = x.parent
  if x.parent == s_nil: m_root = y
  else if x == x.parent.left: x.parent.left = y
  else: x.parent.right = y
  y.left = x
  x.parent = y
  // 更新 max_end
  x.max_end = max(x.interval.end(), x.left.max_end, x.right.max_end)
  y.max_end = max(y.interval.end(), y.left.max_end, y.right.max_end)

right_rotate 为对称实现
```

### 4.5 remove(interval)

```
输入: interval (const GenomicInterval&)
处理:
  1. 按 interval.start 查找节点（红黑树标准二叉查找）
  2. 若未找到匹配的 interval（需完全匹配 start+end+chrom）→ 无操作
  3. 红黑树标准删除 + 修复
  4. 沿删除路径回溯更新 max_end
复杂度: O(log n)

注意: 若存在多个相同 start 的区间（不同 end），查找可能不精确。
      实际实现中应按完整的区间匹配（chrom + start + end）。
```

### 4.6 析构函数

```
~IntervalTree():
  function post_order_delete(node):
      if node == s_nil → return
      post_order_delete(node.left)
      post_order_delete(node.right)
      delete node
  post_order_delete(m_root)
```

### 4.7 clear()

```
清除所有节点，重置为空树。
复用析构逻辑。
复杂度: O(n)
```

---

## 5. 复杂度总结

| 操作 | 时间复杂度 | 空间复杂度 |
|------|-----------|-----------|
| 空树构造 | O(1) | O(1) |
| insert | O(log n) | O(1) 增加值 |
| build | O(n log n) | O(n) |
| query_overlap | O(log n + k) | O(k) 输出 |
| remove | O(log n) | O(1) |
| 析构 | O(n) | — |
| clear | O(n) | — |

---

## 6. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 空树查询 | 返回空 vector |
| 重复区间插入 | 允许重复（红黑树支持重复键，放在右子树） |
| 删除不存在的区间 | 无操作（静默） |
| 模板参数 T 不支持拷贝 | 编译期错误（SFINAE friendly，在类型使用点报错） |

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
| 预期 | 返回 {"A", "B"}（C 与 query [30,70) 重叠? [60,100) 与 [30,70) 重叠，所以是 {"A", "B", "C"}） |
| 类型 | 正常路径 |

修正预期: [30, 70) 与 [0, 50) 重叠 ✓，与 [25, 75) 重叠 ✓，与 [60, 100) 重叠 ✓ → {"A", "B", "C"}

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
| 预期 | build 在 1s 内完成，每次 query 平均 < 10μs |
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

---

## 8. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 18.2.1 | new/delete 管控 | 仅在 RAII 封装内使用，析构函数释放 |
| Rule 8.0.1 | enum class | Color 使用 enum class |
| Rule 9.1.1 | 使用固定宽度整数 | max_end 使用 uint32_t |
| Rule 18.0.1 | 禁用异常 | 所有方法 noexcept 或通过返回值报告 |
| Rule 10.0.3 | const 正确性 | query 类方法声明 const |
