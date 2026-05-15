# M05: GenomicRegion 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.6
> **所属层**: Core Data Types

---

## 1. 模块概述

### 1.1 职责

带元数据的基因组区域，对应 BED/GFF 格式中的行记录。组合 GenomicInterval 并附加名称、评分和自定义属性。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/core/genomic_region.h` |
| 实现文件 | `src/core/genomic_region.cpp` |

### 1.3 依赖

- M04 (GenomicInterval)
- STL

---

## 2. 数据结构设计

### 2.1 内部数据存储

```cpp
class GenomicRegion {
private:
    GenomicInterval m_interval;                           // 基因组区间
    std::string m_name;                                   // 区域名称/标识
    double m_score;                                        // 数值评分
    std::map<std::string, std::string> m_attributes;       // 键值对属性
};
```

### 2.2 与 GenomicInterval 的关系

采用**组合**模式（composition），非继承。

理由:
1. GenomicRegion 的语义是 "带标签的区域"，不是 "是一种特殊的区间"（不符合 IS-A 关系）
2. 组合允许 GenomicInterval 独立于 GenomicRegion 使用（如 IntervalTree 只需要区间，不需要元数据）
3. 符合 MISRA C++:2023 对继承层次最小化的要求（Rule 11.0.1）

### 2.3 属性存储

```cpp
std::map<std::string, std::string> m_attributes;
```

使用 `std::map`（有序）而非 `std::unordered_map`。

理由:
1. GFF 格式的 attributes 列通常按固定顺序排列
2. 有序输出利于格式兼容性和可复现性
3. 属性数量通常较小（< 50 对），红黑树的常数因子差异可忽略

### 2.4 接口代理

GenomicRegion 不暴露 GenomicInterval 的全部接口。调用方通过 `interval()` 获取底层区间对象后进行操作：

```cpp
region.interval().overlaps(other.region.interval());
```

这是有意的设计选择——避免 GenomicRegion 成为 GenomicInterval 的薄封装，保持接口正交。

---

## 3. 算法设计

### 3.1 构造函数

```
输入: interval (GenomicInterval), name (string), score (double, 默认 0.0)
处理:
  1. 移动或拷贝 interval 到 m_interval
  2. 移动或拷贝 name 到 m_name
  3. 设置 m_score = score
输出: GenomicRegion 对象
复杂度: O(1) 用于区间 + O(n) 用于 name 字符串
```

### 3.2 set_attribute / get_attribute

```
set_attribute(key, value):
  1. m_attributes[key] = value  （map 的 insert_or_assign 语义）
  复杂度: O(log n)，n = 属性数量

get_attribute(key):
  1. 在 m_attributes 中查找 key
  2. 找到 → 返回 &value
  3. 未找到 → 返回 nullptr
  复杂度: O(log n)
```

---

## 4. 错误处理

| 场景 | 处理方式 |
|------|----------|
| get_attribute 未找到 key | 返回 nullptr |
| set_attribute 覆盖已有 key | 静默覆盖（map 默认行为） |

---

## 5. 测试设计

### 5.1 测试环境

- 测试文件: `test/core/genomic_region_test.cpp`
- 依赖: M04 (GenomicInterval)

### 5.2 测试用例

#### TC01: 基本构造

| 项目 | 内容 |
|------|------|
| 输入 | interval=GenomicInterval("chr1", 0, 100), name="gene1", score=0.05 |
| 预期 | name()=="gene1", score()==0.05, interval().chrom()=="chr1" |
| 类型 | 正常路径 |

#### TC02: 默认 score

| 项目 | 内容 |
|------|------|
| 输入 | interval=GenomicInterval("chr1", 0, 100), name="gene1"（不传 score） |
| 预期 | score() == 0.0 |
| 类型 | 正常路径 |

#### TC03: set_attribute 单个

| 项目 | 内容 |
|------|------|
| 输入 | set_attribute("gene_type", "protein_coding") |
| 预期 | get_attribute("gene_type") 返回 "protein_coding" 指针 |
| 类型 | 正常路径 |

#### TC04: set_attribute 多个

| 项目 | 内容 |
|------|------|
| 输入 | set_attribute("k1","v1"), set_attribute("k2","v2") |
| 预期 | attributes().size() == 2 |
| 类型 | 正常路径 |

#### TC05: get_attribute 未找到

| 项目 | 内容 |
|------|------|
| 输入 | 空 attributes，查找 "not_exist" |
| 预期 | get_attribute("not_exist") == nullptr |
| 类型 | 边界条件 |

#### TC06: set_attribute 覆盖

| 项目 | 内容 |
|------|------|
| 输入 | set_attribute("k","v1"), set_attribute("k","v2") |
| 预期 | get_attribute("k") 返回 "v2" 指针 |
| 类型 | 正常路径 |

#### TC07: attributes 完整遍历

| 项目 | 内容 |
|------|------|
| 输入 | 设置 3 个属性: "a"→"1", "b"→"2", "c"→"3" |
| 预期 | 遍历 attributes() 返回的 map，按 key 字典序排列 |
| 类型 | 正常路径 |

#### TC08: interval 互操作

| 项目 | 内容 |
|------|------|
| 输入 | region1 在 chr1:[0,100), region2 在 chr1:[50,150) |
| 预期 | region1.interval().overlaps(region2.interval()) == true |
| 类型 | 正常路径 |

#### TC09: 负 score

| 项目 | 内容 |
|------|------|
| 输入 | score = -1.5 |
| 预期 | score() == -1.5 |
| 类型 | 边界条件 |

#### TC10: 空 name

| 项目 | 内容 |
|------|------|
| 输入 | name = "" |
| 预期 | name() == "" |
| 类型 | 边界条件 |

---

## 6. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 11.0.1 | 继承层次最小化 | 使用组合而非继承 |
| Rule 13.0.1 | 优先使用 double | score 使用 double |
| Rule 8.0.2 | 禁止隐式转换 | 属性存取使用 const 引用 |
| Rule 10.0.3 | const 正确性 | 所有 getter 声明 const |
