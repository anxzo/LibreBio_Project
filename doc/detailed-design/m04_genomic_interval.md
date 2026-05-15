# M04: GenomicInterval 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.5
> **所属层**: Core Data Types

---

## 1. 模块概述

### 1.1 职责

表示基因组上的一个坐标区间。采用 0-based 半开区间 [start, end)。提供区间之间的关系判断和集合运算。

### 1.2 背景：坐标系统

基因组区间有两种常用坐标系统：

| 系统 | 表示 | 示例 | 使用场景 |
|------|------|------|----------|
| 0-based 半开 | [start, end) | [0, 100) = 第 1–100 碱基 | BED 格式, 编程语言 |
| 1-based 全闭 | [start, end] | [1, 100] = 第 1–100 碱基 | GFF, VCF 格式 |

LibreBio 内部统一使用 0-based 半开区间 [start, end)。I/O 层负责格式间的坐标转换。

### 1.3 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/core/genomic_interval.h` |
| 实现文件 | `src/core/genomic_interval.cpp` |

### 1.4 依赖

仅 STL。

---

## 2. 数据结构设计

### 2.1 内部数据存储

```cpp
class GenomicInterval {
private:
    std::string m_chrom;     // 染色体名称，如 "chr1", "X", "MT"
    uint32_t m_start;        // 起始位置 (0-based, 包含)
    uint32_t m_end;          // 终止位置 (0-based, 不包含) — m_end > m_start
    Strand m_strand;         // 链方向
};
```

### 2.2 坐标范围

- m_start: [0, m_end)，即 m_start < m_end
- m_end: (m_start, 2³² - 1]
- 使用 uint32_t 上限约 4.3 Gb。对于超过 4Gb 的染色体（极少数），需升级为 uint64_t。当前版本使用 uint32_t，覆盖 >99.9% 使用场景。
- 长度: length() = m_end - m_start，类型为 uint32_t

### 2.3 链方向枚举

```cpp
enum class Strand : uint8_t {
    kForward = 0,  // + (正链 / 正义链)
    kReverse = 1,  // - (反链 / 反义链)
    kUnknown = 2   // . (未知 / 不适用)
};
```

### 2.4 比较运算符

`operator<` 的排序规则: 先按染色体名字典序，再按起始位置升序。
这是基因组区间处理的标准排序方式，用于区间合并、排序和二分查找。

```cpp
bool operator<(const GenomicInterval& other) const noexcept {
    if (m_chrom != other.m_chrom) return m_chrom < other.m_chrom;
    return m_start < other.m_start;
}
```

`operator==` 要求所有四个字段相等。

---

## 3. 算法设计

### 3.1 overlaps(other) — 重叠判断

```
输入: other (const GenomicInterval&)
处理:
  1. 染色体不同 → 返回 false
  2. 区间重叠判断: max(m_start, other.m_start) < min(m_end, other.m_end)
  3. 注意: 紧邻但不重叠的区间 [0, 100) 和 [100, 200) → 返回 false
           因为 max(0, 100)=100, min(100, 200)=100, 100 < 100 为 false
输出: bool
复杂度: O(1)

数学推导:
  两个区间 [A_start, A_end) 和 [B_start, B_end) 重叠 ⇔
  A_start < B_end 且 B_start < A_end
  等价于: max(A_start, B_start) < min(A_end, B_end)
```

### 3.2 intersect(other) — 区间交集

```
输入: other (const GenomicInterval&)
处理:
  1. 若 !overlaps(other) → 返回空区间 GenomicInterval(m_chrom, 0, 0)
     (空区间: start=end=0, 约定 length()==0 表示空)
  2. 计算交集:
     new_start = max(m_start, other.m_start)
     new_end   = min(m_end, other.m_end)
  3. 构造: GenomicInterval(m_chrom, new_start, new_end, m_strand)
     注意: strand 取自身的 strand，非 other 的
     注意: 若 other 的 chrom 不同，进入分支 1 直接返回空
输出: GenomicInterval
复杂度: O(1)

空区间约定:
  使用 (chrom, 0, 0) 表示空区间。调用方通过 length() == 0 判断。
  等价实现: GenomicInterval("", 0, 0) 或提供静态工厂方法 GenomicInterval::empty()。
```

### 3.3 contains(other) — 包含判断

```
输入: other (const GenomicInterval&)
处理:
  1. 染色体不同 → 返回 false
  2. 包含判断: m_start <= other.m_start 且 m_end >= other.m_end
  3. 自我包含: [0, 100).contains([0, 100)) → true
输出: bool
复杂度: O(1)
```

### 3.4 distance(other) — 区间距离

```
输入: other (const GenomicInterval&)
处理:
  1. 染色体不同 → 返回 INT64_MAX（约定为 "无限远"）
  2. 若 overlaps(other) → 返回 0
  3. 若 m_end <= other.m_start → 返回 other.m_start - m_end
     (other 在自身右侧)
  4. 否则 (other.m_end <= m_start) → 返回 m_start - other.m_end
     (other 在自身左侧)
输出: int64_t (有符号，支持大值)
复杂度: O(1)

示例:
  [0, 100).distance([200, 300)) = 200 - 100 = 100
  [200, 300).distance([0, 100)) = 200 - 100 = 100
  [0, 100).distance([50, 150))  = 0  (重叠)
```

---

## 4. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 构造时 m_start >= m_end | 不校验。调用方负责保证合法性 |
| intersect 无交集 | 返回空区间 (start=end=0, length()==0) |
| 不同染色体 distance | 返回 INT64_MAX |
| 不同染色体 overlaps/contains | 返回 false |

---

## 5. 测试设计

### 5.1 测试环境

- 测试文件: `test/core/genomic_interval_test.cpp`

### 5.2 测试用例

#### TC01: 正常构造

| 项目 | 内容 |
|------|------|
| 输入 | chrom="chr1", start=100, end=200, strand=kForward |
| 预期 | chrom()=="chr1", start()==100, end()==200, strand()==kForward, length()==100 |
| 类型 | 正常路径 |

#### TC02: length 计算

| 项目 | 内容 |
|------|------|
| 输入 | start=0, end=1000000 |
| 预期 | length() == 1000000 |
| 类型 | 正常路径 |

#### TC03: 重叠 — 完全包含

| 项目 | 内容 |
|------|------|
| 输入 | A=[0, 100), B=[25, 75) |
| 预期 | A.overlaps(B)==true, B.overlaps(A)==true |
| 类型 | 正常路径 |

#### TC04: 重叠 — 部分重叠

| 项目 | 内容 |
|------|------|
| 输入 | A=[0, 100), B=[50, 150) |
| 预期 | overlaps==true |
| 类型 | 正常路径 |

#### TC05: 重叠 — 紧邻但不重叠

| 项目 | 内容 |
|------|------|
| 输入 | A=[0, 100), B=[100, 200) |
| 预期 | overlaps==false |
| 类型 | 边界条件 |

#### TC06: 重叠 — 完全分离

| 项目 | 内容 |
|------|------|
| 输入 | A=[0, 100), B=[200, 300) |
| 预期 | overlaps==false |
| 类型 | 正常路径 |

#### TC07: 重叠 — 不同染色体

| 项目 | 内容 |
|------|------|
| 输入 | A=chr1:[0, 100), B=chr2:[0, 100) |
| 预期 | overlaps==false |
| 类型 | 正常路径 |

#### TC08: 交集 — 正常

| 项目 | 内容 |
|------|------|
| 输入 | A=[0, 100), B=[50, 150) |
| 预期 | intersect() → [50, 100), length()==50 |
| 类型 | 正常路径 |

#### TC09: 交集 — 无交集

| 项目 | 内容 |
|------|------|
| 输入 | A=[0, 100), B=[200, 300) |
| 预期 | intersect().length() == 0 |
| 类型 | 边界条件 |

#### TC10: 包含 — 完全包含

| 项目 | 内容 |
|------|------|
| 输入 | A=[0, 100), B=[25, 75) |
| 预期 | A.contains(B)==true, B.contains(A)==false |
| 类型 | 正常路径 |

#### TC11: 包含 — 自我

| 项目 | 内容 |
|------|------|
| 输入 | A=[0, 100) |
| 预期 | A.contains(A)==true |
| 类型 | 边界条件 |

#### TC12: 距离 — 分离

| 项目 | 内容 |
|------|------|
| 输入 | A=[0, 100), B=[200, 300) |
| 预期 | A.distance(B)==100, B.distance(A)==100 |
| 类型 | 正常路径 |

#### TC13: 距离 — 相邻

| 项目 | 内容 |
|------|------|
| 输入 | A=[0, 100), B=[100, 200) |
| 预期 | distance==0（相邻但距离为 0，因为起始紧接终止） |
| 类型 | 边界条件 |

> 注意: overlap 返回 false 但 distance 返回 0。语义: [0,100) 和 [100,200) 不重叠但距离为 0。

#### TC14: 距离 — 不同染色体

| 项目 | 内容 |
|------|------|
| 输入 | A=chr1:[0, 100), B=chr2:[0, 100) |
| 预期 | distance==INT64_MAX |
| 类型 | 边界条件 |

#### TC15: 比较运算符 — chrom 优先

| 项目 | 内容 |
|------|------|
| 输入 | A=chr1:[500, 600), B=chr2:[0, 100) |
| 预期 | A < B == true (chr1 < chr2) |
| 类型 | 正常路径 |

#### TC16: 比较运算符 — start 比较

| 项目 | 内容 |
|------|------|
| 输入 | A=chr1:[100, 200), B=chr1:[150, 250) |
| 预期 | A < B == true (100 < 150) |
| 类型 | 正常路径 |

#### TC17: 比较运算符 — 相等

| 项目 | 内容 |
|------|------|
| 输入 | A=chr1:[100, 200, +), B=chr1:[100, 200, +) |
| 预期 | A == B == true |
| 类型 | 正常路径 |

#### TC18: 比较运算符 — strand 不同

| 项目 | 内容 |
|------|------|
| 输入 | A=chr1:[100, 200, +), B=chr1:[100, 200, -) |
| 预期 | A == B == false |
| 类型 | 正常路径 |

---

## 6. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 8.0.1 | enum class | Strand 使用 enum class |
| Rule 8.0.2 | 禁止隐式类型转换 | uint32_t 运算显式处理 |
| Rule 5.0.1 | 避免 magic number | INT64_MAX 通过 \<cstdint\> 获取 |
| Rule 18.0.1 | 禁用异常 | 所有方法 noexcept |
| Rule 10.0.3 | const 正确性 | 所有不修改的方法声明 const |
