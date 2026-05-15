# M01: Sequence 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.2
> **所属层**: Core Data Types

---

## 1. 模块概述

### 1.1 职责

表示一条生物序列（DNA / RNA / Protein），提供序列属性查询和基本操作。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/core/sequence.h` |
| 实现文件 | `src/core/sequence.cpp` |

### 1.3 依赖

仅 STL，无其他模块依赖。

### 1.4 接口摘要

```
Sequence(id, description, seq, alphabet)
  ├── id() → const std::string&
  ├── description() → const std::string&
  ├── seq() → const std::string&
  ├── alphabet() → Alphabet
  ├── length() → size_t
  ├── sub_seq(start, count) → Sequence
  ├── reverse_complement() → Sequence
  ├── gc_content() → double
  └── validate() → bool
```

---

## 2. 数据结构设计

### 2.1 内部数据存储

```cpp
class Sequence {
private:
    std::string m_id;           // 序列标识符，如 "chr1", "seq001"
    std::string m_description;  // 序列描述，如 "Human chromosome 1"
    std::string m_seq;          // 序列字符串，大写字符存储
    Alphabet m_alphabet;        // 字母表类型：kDNA / kRNA / kProtein
};
```

### 2.2 数据存储策略

| 字段 | 存储策略 | 理由 |
|------|----------|------|
| m_id | std::string，值存储 | 通常较短 (< 256 字符)，值存储避免生命周期问题 |
| m_description | std::string，值存储 | 同上 |
| m_seq | std::string，值存储 | 序列可能是长字符串，但 std::string SSO 对小序列友好。大序列的场景由 SequenceStore 惰性加载解决，不在 Sequence 层处理 |
| m_alphabet | enum (1 字节) | 固定大小，值语义 |

### 2.3 不可变设计

Sequence 对象构造后不可修改。所有查询操作返回 `const` 引用。所有变换操作（sub_seq, reverse_complement）返回新对象。

理由: 参考概要设计 §2.2，Core 层无内部依赖，保持纯数据对象语义。不可变设计避免共享状态问题，符合 MISRA C++:2023 Rule 10.0.3（避免非预期的副作用）。

### 2.4 字母表定义

```cpp
enum class Alphabet : uint8_t {
    kDNA     = 0,   // A, T (或 U, 取决于上下文), C, G, N
    kRNA     = 1,   // A, U, C, G, N
    kProtein = 2    // A, C, D, E, F, G, H, I, K, L, M, N, P, Q, R, S, T, V, W, Y, *
};
```

使用 `enum class` 避免隐式整型转换（MISRA C++:2023 Rule 8.0.1）。

---

## 3. 算法设计

### 3.1 构造函数

```
输入: id (string), description (string), seq (string), alphabet (Alphabet)
处理: 逐字段移动复制到成员变量
输出: Sequence 对象
复杂度: O(n)，n = seq 长度（std::string 移动或拷贝）

注意: 构造函数不调用 validate()。校验由调用方按需显式调用。
      这是有意为之——构造不校验允许从文件流式读取时避免双重遍历。
```

### 3.2 sub_seq(start, count)

```
输入: start (size_t) — 起始位置 (0-based)
       count (size_t) — 子序列长度
处理:
  1. 边界检查: 若 start >= m_seq.length()，返回空序列 (seq="", 保持 id/desc/alphabet)
  2. 长度修正: 若 start + count > m_seq.length()，count = m_seq.length() - start
  3. 截取: 从 m_seq 中提取子串
  4. 构造: Sequence(m_id, m_description, sub_string, m_alphabet)
输出: 新 Sequence 对象
复杂度: O(count)

边界条件:
  - start = 0, count = length() → 返回完整副本
  - start = length(), count = 任意 → 返回空序列
  - count = 0 → 返回空序列
```

### 3.3 reverse_complement()

```
输入: 无 (使用自身序列)
处理:
  1. 字母表检查: 若 alphabet != kDNA 且 alphabet != kRNA，返回自身副本
  2. 构造互补映射表:
     DNA: A↔T, T↔A, C↔G, G↔C, N→N, 其他→N
     RNA: A↔U, U↔A, C↔G, G↔C, N→N, 其他→N
  3. 反向遍历: 从 m_seq 尾部到头部，逐字符查表映射
  4. 构造: Sequence(m_id, m_description + " reverse complement", complemented_string, m_alphabet)
输出: 新 Sequence 对象
复杂度: O(n)，n = 序列长度

详细步骤:
  for i = m_seq.length() - 1 down to 0:
      result += complement_table[m_seq[i]]
  // complement_table 使用 std::array<char, 256> 静态预计算

设计选择: 使用静态预计算查找表而非 switch-case 或 if-else 链。
         理由: O(1) 单字符映射，无分支预测开销，提升大序列的吞吐量。
```

**互补映射表（静态常量）**:

```cpp
// DNA 互补表 (静态预计算)
static constexpr std::array<char, 256> kDNAComplement = []() {
    std::array<char, 256> table{};
    for (int i = 0; i < 256; ++i) table[i] = 'N';
    table['A'] = 'T'; table['T'] = 'A';
    table['C'] = 'G'; table['G'] = 'C';
    table['a'] = 't'; table['t'] = 'a';
    table['c'] = 'g'; table['g'] = 'c';
    table['N'] = 'N'; table['n'] = 'n';
    table[' '] = ' ';
    return table;
}();
```

### 3.4 gc_content()

```
输入: 无 (使用自身序列)
处理:
  1. 字母表检查: 若 alphabet != kDNA 且 alphabet != kRNA，返回 0.0
  2. 遍历计数: 统计 m_seq 中 'G', 'g', 'C', 'c' 的出现次数
  3. 计算比例:
     若 m_seq.length() == 0 → 返回 0.0
     否则 → gc_count / static_cast<double>(m_seq.length())
输出: [0.0, 1.0] 范围的双精度浮点数
复杂度: O(n)

浮点处理: 使用 double 而非 float。MISRA C++:2023 Rule 13.0.1 建议优先使用 double。
```

### 3.5 validate()

```
输入: 无 (使用自身序列和字母表)
处理:
  1. 根据 m_alphabet 选择合法字符集:
     kDNA:     {A, C, G, T, N, a, c, g, t, n}
     kRNA:     {A, C, G, U, N, a, c, g, u, n}
     kProtein: {A, C, D, E, F, G, H, I, K, L, M, N, P, Q, R, S, T, V, W, Y, *, 对应小写}
  2. 遍历 m_seq 每个字符，检查是否在合法字符集中
  3. 发现非法字符 → 返回 false
  4. 全部合法 → 返回 true
输出: bool
复杂度: O(n)

实现方式: 使用 std::array<bool, 256> 布尔查找表。
         构造时按 alphabet 初始化，validate 时 O(1) 查表。
```

---

## 4. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 构造时传入非法字符 | 不报错，由 validate() 负责检测 |
| sub_seq 越界 | 自动修正 count，不报错 |
| reverse_complement 在 Protein 上调用 | 返回原序列副本，不报错 |
| gc_content 在 Protein 上调用 | 返回 0.0，不报错 |
| gc_content 在空序列上调用 | 返回 0.0 |
| validate 发现非法字符 | 返回 false |

Core 层不产生 ErrorCode。理由: Core 层为纯数据层，保持简单和无副作用。

---

## 5. 测试设计

### 5.1 测试环境

- 测试框架: Catch2 (header-only) 或自建
- 测试文件: `test/core/sequence_test.cpp`

### 5.2 测试用例

#### TC01: 正常构造 DNA 序列

| 项目 | 内容 |
|------|------|
| 输入 | id="seq1", desc="test", seq="ATCG", alphabet=kDNA |
| 预期 | id()=="seq1", desc()=="test", seq()=="ATCG", alphabet()==kDNA, length()==4 |
| 类型 | 正常路径 |

#### TC02: 正常构造 RNA 序列

| 项目 | 内容 |
|------|------|
| 输入 | id="rna1", desc="", seq="AUCG", alphabet=kRNA |
| 预期 | alphabet()==kRNA, length()==4 |
| 类型 | 正常路径 |

#### TC03: 正常构造 Protein 序列

| 项目 | 内容 |
|------|------|
| 输入 | id="prot1", desc="enzyme", seq="MKTGFL", alphabet=kProtein |
| 预期 | alphabet()==kProtein, length()==6 |
| 类型 | 正常路径 |

#### TC04: 空序列构造

| 项目 | 内容 |
|------|------|
| 输入 | id="empty", desc="", seq="", alphabet=kDNA |
| 预期 | length()==0, gc_content()==0.0 |
| 类型 | 边界条件 |

#### TC05: sub_seq 正常截取

| 项目 | 内容 |
|------|------|
| 输入 | seq="ACGTACGT", sub_seq(0, 4) |
| 预期 | 返回 seq="ACGT", 保持 id/desc/alphabet 不变 |
| 类型 | 正常路径 |

#### TC06: sub_seq 起始越界

| 项目 | 内容 |
|------|------|
| 输入 | seq="ACGT", sub_seq(4, 1) |
| 预期 | 返回 seq="", length()==0 |
| 类型 | 边界条件 |

#### TC07: sub_seq 长度超限

| 项目 | 内容 |
|------|------|
| 输入 | seq="ACGT", sub_seq(2, 10) |
| 预期 | 返回 seq="GT", length()==2（自动修正为到末尾） |
| 类型 | 边界条件 |

#### TC08: sub_seq count 为 0

| 项目 | 内容 |
|------|------|
| 输入 | seq="ACGT", sub_seq(0, 0) |
| 预期 | 返回 seq="" |
| 类型 | 边界条件 |

#### TC09: DNA reverse_complement

| 项目 | 内容 |
|------|------|
| 输入 | seq="ATC G", alphabet=kDNA |
| 预期 | reverse_complement().seq() == "C GAT"（空格原样保留于互补表中） |
| 类型 | 正常路径 |

#### TC10: RNA reverse_complement

| 项目 | 内容 |
|------|------|
| 输入 | seq="AUCG", alphabet=kRNA |
| 预期 | reverse_complement().seq() == "CGAU" |
| 类型 | 正常路径 |

#### TC11: Protein reverse_complement

| 项目 | 内容 |
|------|------|
| 输入 | seq="MKTG", alphabet=kProtein |
| 预期 | reverse_complement().seq() == "MKTG"（返回原序列副本） |
| 类型 | 边界条件 |

#### TC12: gc_content 正常计算

| 项目 | 内容 |
|------|------|
| 输入 | seq="GCGCATAT", alphabet=kDNA |
| 预期 | gc_content() == 0.5 |
| 类型 | 正常路径 |

#### TC13: gc_content 全 GC

| 项目 | 内容 |
|------|------|
| 输入 | seq="GGGGCCCC", alphabet=kDNA |
| 预期 | gc_content() == 1.0 |
| 类型 | 边界条件 |

#### TC14: gc_content 无 GC

| 项目 | 内容 |
|------|------|
| 输入 | seq="ATATATAT", alphabet=kDNA |
| 预期 | gc_content() == 0.0 |
| 类型 | 边界条件 |

#### TC15: gc_content 大小写混合

| 项目 | 内容 |
|------|------|
| 输入 | seq="gcatGCAT", alphabet=kDNA |
| 预期 | gc_content() == 0.5 |
| 类型 | 正常路径 |

#### TC16: validate DNA 合法序列

| 项目 | 内容 |
|------|------|
| 输入 | seq="ATCGN", alphabet=kDNA |
| 预期 | validate() == true |
| 类型 | 正常路径 |

#### TC17: validate DNA 非法字符

| 项目 | 内容 |
|------|------|
| 输入 | seq="ATCGM", alphabet=kDNA（M 不合法但 R/Y 等简并碱基也不合法） |
| 预期 | validate() == false |
| 类型 | 非法输入 |

#### TC18: validate Protein 合法序列

| 项目 | 内容 |
|------|------|
| 输入 | seq="MKTGFL*", alphabet=kProtein |
| 预期 | validate() == true |
| 类型 | 正常路径 |

#### TC19: 长序列性能测试

| 项目 | 内容 |
|------|------|
| 输入 | seq=10,000,000 个 'A' 的 DNA 序列 |
| 预期 | gc_content() 在 10ms 内完成（单线程，现代 CPU） |
| 类型 | 性能基准 |

---

## 6. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 8.0.1 | 使用 enum class | Alphabet 使用 enum class |
| Rule 10.0.3 | 避免非预期副作用 | 所有方法为 const 或返回新对象 |
| Rule 13.0.1 | 优先使用 double | gc_content() 返回 double |
| Rule 18.0.1 | 禁用异常 | 所有方法 noexcept 或通过返回值报告 |
