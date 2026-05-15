# M02: QualityScore 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.3
> **所属层**: Core Data Types

---

## 1. 模块概述

### 1.1 职责

存储和操作 Phred 质量分数序列。自动检测编码方案。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/core/quality_score.h` |
| 实现文件 | `src/core/quality_score.cpp` |

### 1.3 依赖

仅 STL（`std::vector`, `std::uint8_t`）。

---

## 2. 背景知识

### 2.1 Phred 质量分数

生物测序中，每个碱基的测序质量通过 Phred 分数表示：

```
Q = -10 × log₁₀(P_error)
```

其中 P_error 是碱基被测错的概率。例如：
- Q = 10 → 错误率 10%
- Q = 20 → 错误率 1%
- Q = 30 → 错误率 0.1%（Q30，行业常用基准）
- Q = 40 → 错误率 0.01%

### 2.2 编码方案

在 FASTQ 文件中，Phred 分数被编码为单个 ASCII 字符：

| 编码方案 | ASCII 偏移 | 分数范围 | 使用时期 |
|----------|-----------|---------|---------|
| Sanger (Phred+33) | 33 | 0–93 | 现代通用标准 |
| Illumina 1.3+ (Phred+64) | 64 | 0–62 | Illumina 1.3 – 1.7 |
| Illumina 1.5+ (Phred+64) | 64 | 2–62 | Illumina 1.5 – 1.7 |
| Illumina 1.8+ (Phred+33) | 33 | 0–93 | Illumina 1.8+（同 Sanger） |

---

## 3. 数据结构设计

### 3.1 内部数据存储

```cpp
class QualityScore {
private:
    std::vector<uint8_t> m_scores;  // 原始 ASCII 值
    PhredEncoding m_encoding;        // 检测到的编码方案
};
```

### 3.2 存储策略

| 字段 | 类型 | 说明 |
|------|------|------|
| m_scores | std::vector\<uint8_t\> | 存储 ASCII 原始值（如 'I' = 73），不做偏移转换 |
| m_encoding | PhredEncoding enum | 标记编码方案，用于解读分数时确定偏移量 |

设计选择: 存储原始 ASCII 值而非 Phred 整数值。
理由:
1. 避免信息丢失——原始字节可直接写回 FASTQ 文件
2. 编码检测基于 ASCII 范围，存储原始值使检测逻辑清晰
3. Phred 值的转换在访问时按需计算（偏移量取决于 encoding）

### 3.3 编码枚举

```cpp
enum class PhredEncoding : uint8_t {
    kSanger      = 0,  // offset = 33
    kIllumina13  = 1,  // offset = 64, score range [0, 62]
    kIllumina15  = 2,  // offset = 64, score range [2, 62]
    kIllumina18  = 3,  // offset = 33 (same as Sanger)
    kUnknown     = 4
};
```

---

## 4. 算法设计

### 4.1 构造函数

```
构造函数1: QualityScore(vector<uint8_t> scores)
  处理: 移动 scores，调用 detect_encoding 自动检测编码

构造函数2: QualityScore(vector<uint8_t> scores, PhredEncoding encoding)
  处理: 移动 scores，直接使用传入的 encoding（跳过检测）
  用途: 用户明确知道编码方案时使用
```

### 4.2 detect_encoding(scores) - 静态方法

```
输入: scores (const vector<uint8_t>&)
处理:
  1. 若 scores 为空 → 返回 kUnknown
  2. 遍历所有分数值，记录最小值 min_val
  3. 根据 min_val 判断:
     若 min_val >= 33 且 min_val < 64  → 返回 kSanger（或 kIllumina18，两者不可区分）
     若 min_val >= 64 且 min_val <= 126 → 返回 kIllumina13（或 kIllumina15，需检查下限）
       若存在 min_val == 64 且 score 2 出现在可能位置 → kIllumina15
       否则 → kIllumina13
  4. 其他情况 → 返回 kUnknown
输出: PhredEncoding
复杂度: O(n)

详细判断逻辑:
  const uint8_t min_val = *std::min_element(scores.begin(), scores.end());
  if (min_val < 33) return kUnknown;          // 低于 Sanger 最低值
  if (min_val < 59) return kSanger;            // 33–58 区间，Sanger/Illumina18
  if (min_val == 59) return kSanger;           // 59 时也可能是 Illumina15 的 score=2
  // 但 59 在 Sanger 范围内（Phred 26），且 Illumina 1.5+ 最低为 2 → offset 64 → 66
  // 实际 detect 逻辑：min_val < 64 → Sanger，min_val >= 64 → Illumina13/15
  if (min_val >= 64) return kIllumina13;
  return kUnknown;

注意: 精确区分 Sanger 和 Illumina18 不可能（两者 offset 相同），返回 kSanger。
      精确区分 Illumina13 和 Illumina15 不完全可靠，默认返回 kIllumina13。
      概要设计未对区分精度提出强制要求。
```

**实现建议**: 使用预计算常量，避免 magic number。
```cpp
static constexpr uint8_t kSangerMinAscii = 33;
static constexpr uint8_t kIllumina13MinAscii = 64;
static constexpr uint8_t kMaxValidAscii = 126;
```

### 4.3 average_score()

```
输入: 无
处理:
  1. 若 m_scores 为空 → 返回 0.0
  2. 计算偏移量 offset:
     kSanger/kIllumina18 → 33
     kIllumina13/kIllumina15 → 64
     kUnknown → 33 (保守假设)
  3. 遍历求和: sum += (m_scores[i] - offset)
  4. 返回: sum / m_scores.size()
输出: double
复杂度: O(n)

注意: 使用 double 累加避免大数组的 float 精度损失。
```

### 4.4 q30_ratio()

```
输入: 无
处理:
  1. 若 m_scores 为空 → 返回 0.0
  2. 计算偏移量 offset（同 average_score）
  3. Q30 的 ASCII 阈值 = 30 + offset
  4. 遍历统计: count += (m_scores[i] >= threshold) ? 1 : 0
  5. 返回: count / static_cast<double>(m_scores.size())
输出: double
复杂度: O(n)
```

### 4.5 operator[](size_t idx)

```
输入: idx (size_t)
处理:
  1. 返回 m_scores[idx] - offset（Phred 整数值）
  注意: 不进行越界检查（MISRA 允许标准库行为：未定义行为由调用方负责）
输出: uint8_t
复杂度: O(1)
```

---

## 5. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 空 scores 构造 | 正常构造，encoding = kUnknown |
| 空 scores 上 average_score/q30_ratio | 返回 0.0 |
| 无法确定的编码 | encoding = kUnknown, average_score/q30_ratio 使用 offset=33 |
| operator[] 越界 | 标准库行为（未定义），调用方负责边界检查 |

---

## 6. 测试设计

### 6.1 测试环境

- 测试文件: `test/core/quality_score_test.cpp`

### 6.2 测试用例

#### TC01: Sanger 编码检测

| 项目 | 内容 |
|------|------|
| 输入 | scores = {'I', 'I', 'I', 'I'} （ASCII 73, Phred 40） |
| 预期 | encoding() == kSanger, scores() == {73, 73, 73, 73} |
| 类型 | 正常路径 |

#### TC02: Illumina 1.3 编码检测

| 项目 | 内容 |
|------|------|
| 输入 | scores = {64, 65, 70, 80} （ASCII 64+, 最低 Phred 0） |
| 预期 | encoding() == kIllumina13 |
| 类型 | 正常路径 |

#### TC03: Illumina 1.8 编码检测

| 项目 | 内容 |
|------|------|
| 输入 | scores = {33, 40, 50, 60} （ASCII 33+, 与 Sanger 相同范围） |
| 预期 | encoding() == kSanger（不可区分，返回 kSanger） |
| 类型 | 正常路径 |

#### TC04: 空数组

| 项目 | 内容 |
|------|------|
| 输入 | scores = {} |
| 预期 | encoding() == kUnknown, size() == 0, average_score() == 0.0, q30_ratio() == 0.0 |
| 类型 | 边界条件 |

#### TC05: 非法编码检测

| 项目 | 内容 |
|------|------|
| 输入 | scores = {10, 20, 30}（都低于 33） |
| 预期 | encoding() == kUnknown |
| 类型 | 非法输入 |

#### TC06: average_score 计算（Sanger）

| 项目 | 内容 |
|------|------|
| 输入 | ASCII 值 {43, 43, 43, 43}（Phred 10, 10, 10, 10） |
| 预期 | average_score() == 10.0 |
| 类型 | 正常路径 |

#### TC07: average_score 计算（混合值）

| 项目 | 内容 |
|------|------|
| 输入 | ASCII 值 {73, 73, 33, 33}（Phred 40, 40, 0, 0, Sanger） |
| 预期 | average_score() == 20.0 |
| 类型 | 正常路径 |

#### TC08: q30_ratio 全部通过

| 项目 | 内容 |
|------|------|
| 输入 | 全部 Phred 40（ASCII 73, Sanger），4 个值 |
| 预期 | q30_ratio() == 1.0 |
| 类型 | 边界条件 |

#### TC09: q30_ratio 全部不通过

| 项目 | 内容 |
|------|------|
| 输入 | 全部 Phred 10（ASCII 43, Sanger），4 个值 |
| 预期 | q30_ratio() == 0.0 |
| 类型 | 边界条件 |

#### TC10: q30_ratio 混合

| 项目 | 内容 |
|------|------|
| 输入 | Phred 值 {40, 20, 35, 10}（Sanger offset 33: ASCII {73, 53, 68, 43}） |
| 预期 | q30_ratio() == 0.5（40 和 35 通过） |
| 类型 | 正常路径 |

#### TC11: 显式指定编码构造

| 项目 | 内容 |
|------|------|
| 输入 | scores={64,64,64}, encoding=kIllumina13 |
| 预期 | encoding()==kIllumina13, average_score()==0.0 |
| 类型 | 正常路径 |

#### TC12: size() 正确返回

| 项目 | 内容 |
|------|------|
| 输入 | 构造 100 个随机质量值 |
| 预期 | size() == 100 |
| 类型 | 正常路径 |

#### TC13: operator[] 返回正确 Phred 值

| 项目 | 内容 |
|------|------|
| 输入 | ASCII 73（= 'I', Sanger），offset 33 |
| 预期 | operator[](0) == 40 |
| 类型 | 正常路径 |

#### TC14: 性能测试 - 1 亿个质量值

| 项目 | 内容 |
|------|------|
| 输入 | 100,000,000 个随机质量值 |
| 预期 | average_score() 在 100ms 内完成 |
| 类型 | 性能基准 |

---

## 7. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 8.0.1 | enum class | PhredEncoding 使用 enum class |
| Rule 5.0.1 | 避免 magic number | 偏移量使用 constexpr 常量 |
| Rule 8.0.2 | 禁止隐式类型转换 | detect_encoding 中显式比较 |
| Rule 18.0.1 | 禁用异常 | 所有方法 noexcept 或通过返回值报告 |
