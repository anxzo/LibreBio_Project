# M03: SequenceStore 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.4
> **所属层**: Core Data Types

---

## 1. 模块概述

### 1.1 职责

批量管理 Sequence 对象，提供查找、迭代和统计功能。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/core/sequence_store.h` |
| 实现文件 | `src/core/sequence_store.cpp` |

### 1.3 依赖

- M01 (Sequence)
- STL

---

## 2. 数据结构设计

### 2.1 内部数据存储

```cpp
class SequenceStore {
private:
    std::vector<Sequence> m_sequences;
};
```

### 2.2 存储策略分析

| 候选方案 | 结构 | 优点 | 缺点 | 选择 |
|----------|------|------|------|------|
| A: 单一 vector | `vector<Sequence>` | 内存连续，迭代快，简单 | 按 ID 查找 O(n) | ✅ 选择 |
| B: vector + hash map | `vector<Sequence>` + `unordered_map<string, size_t>` | 按 ID 查找 O(1) | 双倍索引维护成本，额外的哈希开销 | ❌ 过度设计 |
| C: sorted vector | 按 ID 排序的 `vector<Sequence>` | 二分查找 O(log n) | 每次插入需维护有序 | ❌ 复杂 |

选择方案 A 的理由:
1. SequenceStore 主要用于批量存储和统计（total_bases, N50），主要操作是遍历，而非单点查找。
2. find_by_id 的调用频率远低于迭代遍历。
3. 大多数生物信息学场景中，序列数量在万到百万级。O(n) 线性查找在百万级约 1ms（现代 CPU），可接受。
4. 遵循概要设计的零依赖原则和最小复杂度原则。

### 2.3 内存管理

```cpp
void reserve(size_t capacity);  // 预分配，避免多次 reallocation
void clear() noexcept;           // 清空并释放内存
```

使用 `vector::reserve` 预分配可减少大型文件加载时的内存重分配次数。

---

## 3. 算法设计

### 3.1 add(Sequence seq)

```
输入: seq (Sequence, 移动语义)
处理:
  1. m_sequences.push_back(std::move(seq))
输出: 无
复杂度: 摊还 O(1)

注意: 参数按值传递，调用方可使用 std::move 避免拷贝。
      不使用 emplace_back 直接构造，保持接口简洁。
```

### 3.2 add(vector<Sequence> seqs)

```
输入: seqs (vector<Sequence>, 移动语义)
处理:
  1. 若 m_sequences 为空且 seqs 容量充足 → 直接移动赋值（避免拷贝）
     否则 → reserve + 逐个插入
  2. m_sequences.insert(end, make_move_iterator(seqs.begin()), ...)
输出: 无
复杂度: O(n)，n = seqs.size()
```

### 3.3 find_by_id(id)

```
输入: id (const string&)
处理:
  1. 线性遍历 m_sequences
  2. 比较: m_sequences[i].id() == id
  3. 找到 → 返回 &m_sequences[i]
  4. 未找到 → 返回 nullptr
输出: const Sequence* 或 nullptr
复杂度: O(n)

注意: 返回原始指针（非 owning）。调用方不能持有超过 SequenceStore 生命周期。
      设计选择：返回指针而非 optional<reference>。理由: C++ 无标准 optional<T&>。
```

### 3.4 count() / empty()

```
count(): 返回 m_sequences.size()
empty(): 返回 m_sequences.empty()
复杂度: O(1)
```

### 3.5 total_bases()

```
输入: 无
处理:
  1. 遍历累加: total += m_sequences[i].length()
  2. 注意: 使用 size_t 累加，不检查溢出（总碱基数超出 size_t 范围在实际中不可能：
     人类基因组 3e9 bases，size_t 最大 ~1.8e19 on 64-bit）
输出: size_t
复杂度: O(n)
```

### 3.6 n50() — 核心统计算法

```
输入: 无
处理:
  1. 若 m_sequences 为空 → 返回 0
  2. 计算总长度 total = total_bases()
  3. 计算目标值: target = total / 2（若 total 为奇数，向上取整）
  4. 提取所有序列长度到 vector<size_t>
  5. 按降序排列 lengths
  6. 累加降序长度: cumulative += lengths[i]
     当 cumulative >= target 时 → 返回 lengths[i]
  7. 若循环结束仍未达到（理论上不可能但做防御）→ 返回 lengths.back()
输出: size_t
复杂度: O(n log n)（排序主导）

N50 的生物学定义:
  将所有序列按长度从大到小排列。
  从最长的序列开始累加，当累加长度达到总长度的 50% 时，
  当前序列的长度即为 N50。
  N50 越大表示组装质量越高。

优化选择:
  - 完整排序 vs 部分排序: 选择完整排序。理由: 
    (1) 序列长度分布可能高度不均匀（少数长序列 + 大量短序列）
    (2) std::sort 对小规模数据极快
    (3) 避免 nth_element 带来的额外复杂度和不可预测性
```

**N50 算法伪代码**:
```
function n50():
    if m_sequences.empty() → return 0
    total = total_bases()
    target = (total + 1) / 2
    lengths = [seq.length() for seq in m_sequences]
    sort(lengths, descending)
    cumulative = 0
    for each len in lengths:
        cumulative += len
        if cumulative >= target → return len
    return lengths.back()  // 防御性
```

### 3.7 迭代器

```cpp
auto begin() const noexcept → m_sequences.begin()
auto end() const noexcept   → m_sequences.end()
auto begin() noexcept       → m_sequences.begin()  // 允许修改
auto end() noexcept         → m_sequences.end()
```

提供 const 和 non-const 版本，支持范围 for 循环和 STL 算法。

---

## 4. 错误处理

| 场景 | 处理方式 |
|------|----------|
| find_by_id 未找到 | 返回 nullptr |
| 空 store 上 n50() | 返回 0 |
| 空 store 上 total_bases() | 返回 0 |
| 空 store 上 find_by_id | 返回 nullptr |

---

## 5. 测试设计

### 5.1 测试环境

- 测试文件: `test/core/sequence_store_test.cpp`
- 依赖: M01 (Sequence)

### 5.2 测试用例

#### TC01: 空 store 基本属性

| 项目 | 内容 |
|------|------|
| 输入 | 构造空 SequenceStore |
| 预期 | count()==0, empty()==true, total_bases()==0, n50()==0 |
| 类型 | 边界条件 |

#### TC02: 单序列 add

| 项目 | 内容 |
|------|------|
| 输入 | add(Sequence("s1", "", "ATCG", kDNA)) |
| 预期 | count()==1, empty()==false, total_bases()==4, n50()==4 |
| 类型 | 正常路径 |

#### TC03: 批量 add

| 项目 | 内容 |
|------|------|
| 输入 | add({Sequence("s1", "", "AAAA", kDNA), Sequence("s2", "", "CC", kDNA)}) |
| 预期 | count()==2, total_bases()==6 |
| 类型 | 正常路径 |

#### TC04: find_by_id 找到

| 项目 | 内容 |
|------|------|
| 输入 | 存储 Sequence("chr1", "", "ATCG", kDNA)，查找 "chr1" |
| 预期 | 返回非 nullptr，指针指向的 id() == "chr1" |
| 类型 | 正常路径 |

#### TC05: find_by_id 未找到

| 项目 | 内容 |
|------|------|
| 输入 | 存储 Sequence("chr1", "", "ATCG", kDNA)，查找 "chr2" |
| 预期 | 返回 nullptr |
| 类型 | 正常路径 |

#### TC06: N50 单序列

| 项目 | 内容 |
|------|------|
| 输入 | 1 条长度 100 的序列 |
| 预期 | n50() == 100 |
| 类型 | 正常路径 |

#### TC07: N50 等长序列

| 项目 | 内容 |
|------|------|
| 输入 | 4 条长度均为 100 的序列 (total=400, target=200) |
| 预期 | n50() == 100（第 2 条时就达到 200） |
| 类型 | 正常路径 |

#### TC08: N50 不均匀序列

| 项目 | 内容 |
|------|------|
| 输入 | 长度分布: [500, 200, 100, 100, 50, 50] (total=1000, target=500) |
| 预期 | n50() == 500（第一条就达到 500） |
| 类型 | 正常路径 |

#### TC09: N50 需要跨越多个序列

| 项目 | 内容 |
|------|------|
| 输入 | 长度分布: [300, 200, 200, 200, 100] (total=1000, target=500) |
| 预期 | n50() == 200（300→500 跨越到第二条长度 200） |
| 类型 | 正常路径 |

#### TC10: reserve + clear

| 项目 | 内容 |
|------|------|
| 输入 | reserve(1000), 添加 100 条序列后 clear() |
| 预期 | count()==0, empty()==true |
| 类型 | 正常路径 |

#### TC11: 迭代器遍历

| 项目 | 内容 |
|------|------|
| 输入 | 添加 5 条序列，通过范围 for 遍历并收集 id |
| 预期 | 收集到的 id 列表 = 添加时的 id 列表，顺序一致 |
| 类型 | 正常路径 |

#### TC12: 大量序列性能测试

| 项目 | 内容 |
|------|------|
| 输入 | 1,000,000 条长度 100 的序列，计算 total_bases 和 n50 |
| 预期 | total_bases() 在 50ms 内完成，n50() 在 200ms 内完成 |
| 类型 | 性能基准 |

---

## 6. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 8.0.1 | enum class | 不适用（无自定义枚举） |
| Rule 10.0.3 | 避免非预期副作用 | 返回指针标记 const |
| Rule 8.0.2 | 禁止隐式类型转换 | 所有 size_t 显式使用 |
| Rule 18.0.1 | 禁用异常 | 所有方法 noexcept |
