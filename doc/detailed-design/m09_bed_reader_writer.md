# M09: BEDReader / BEDWriter 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.10
> **所属层**: File Format I/O

---

## 1. 模块概述

### 1.1 职责

BED（Browser Extensible Data）格式文件的流式读取与写入。支持 BED3 ~ BED12 格式自动检测。

### 1.2 背景：BED 格式

BED 格式用于描述基因组上的区间（注释、peak、比对区域等）。以 Tab 分隔，最少 3 列，最多 12 列。

| 列 | BED3 | BED6 | BED12 |
|----|------|------|-------|
| 1 | chrom | chrom | chrom |
| 2 | start (0-based) | start | start |
| 3 | end | end | end |
| 4 | — | name | name |
| 5 | — | score (0–1000) | score |
| 6 | — | strand (+/-/.) | strand |
| 7–9 | — | — | thickStart, thickEnd, itemRgb |
| 10–12 | — | — | blockCount, blockSizes, blockStarts |

### 1.3 文件位置

| 类型 | 路径 |
|------|------|
| BEDReader 头文件 | `include/libre_bio/io/bed_reader.h` |
| BEDReader 实现文件 | `src/io/bed_reader.cpp` |
| BEDWriter 头文件 | `include/libre_bio/io/bed_writer.h` |
| BEDWriter 实现文件 | `src/io/bed_writer.cpp` |

### 1.4 依赖

- M04 (GenomicInterval)
- M05 (GenomicRegion)
- STL

---

## 2. 数据结构设计

### 2.1 BEDReader 内部实现

```cpp
class BEDReader::Impl {
    std::ifstream m_file;
    size_t m_column_count;     // 自动检测的列数 (3–12)
    std::array<char, 65536> m_buffer;
    // ...
};
```

### 2.2 列数检测

首行读取后，根据 Tab 分隔的字段数确定列数。

- 3 列 → BED3
- 4 列 → BED4
- 5 列 → BED5
- 6 列 → BED6
- 7–12 列 → BED12 (或 BED6+)
- 其他 → 格式错误

后续行必须保持一致。若列数变化 → 告警或取最小值截断。

---

## 3. 算法设计

### 3.1 next() — 行解析与字段映射

```
输入: 无
处理:
  1. 读取一行，跳过 '#' 注释行和空行
  2. 按 '\t' 分割为 fields[]
  3. 首行时: 根据 fields.size() 确定 m_column_count
  4. 映射字段到 GenomicRegion:
     - chrom   = fields[0]
     - start   = parse_uint32(fields[1])  // 0-based
     - end     = parse_uint32(fields[2])
     - name    = (col >= 4) ? fields[3] : "."
     - score   = (col >= 5) ? parse_double(fields[4]) : 0.0
     - strand  = (col >= 6) ? parse_strand(fields[5]) : kUnknown

  5. BED12 额外字段存入 attributes:
     若 col >= 12:
       attributes["thick_start"]  = fields[6]
       attributes["thick_end"]    = fields[7]
       attributes["item_rgb"]     = fields[8]
       attributes["block_count"]  = fields[9]
       attributes["block_sizes"]  = fields[10]
       attributes["block_starts"] = fields[11]

  6. 构造 GenomicInterval + GenomicRegion
输出: GenomicRegion
复杂度: O(行长度)
```

### 3.2 strand 解析

```
输入: strand_str (string)
处理:
  '+' → kForward
  '-' → kReverse
  '.' → kUnknown
  其他 → kUnknown
```

### 3.3 BEDWriter::write(region)

```
输入: region (const GenomicRegion&)
处理:
  1. 输出 chrom + '\t'
  2. 输出 start + '\t' + end
  3. 若 num_columns >= 4: '\t' + name
  4. 若 num_columns >= 5: '\t' + score
  5. 若 num_columns >= 6: '\t' + strand_char
  6. 若 num_columns >= 12: 输出 thickStart..blockStarts
  7. 输出 '\n'
```

---

## 4. 测试设计

### 4.1 测试用例

#### TC01: BED3 读取

| 项目 | 内容 |
|------|------|
| 输入 | `chr1\t0\t100\n` |
| 预期 | column_count()==3, region.interval().chrom()=="chr1", start==0, end==100 |
| 类型 | 正常路径 |

#### TC02: BED6 读取

| 项目 | 内容 |
|------|------|
| 输入 | `chr1\t0\t100\tgene1\t500\t+\n` |
| 预期 | name()=="gene1", score()==500.0, strand==kForward |
| 类型 | 正常路径 |

#### TC03: BED12 读取

| 项目 | 内容 |
|------|------|
| 输入 | 12 列 BED 行 |
| 预期 | attributes 包含 thickStart, thickEnd, itemRgb, blockCount, blockSizes, blockStarts |
| 类型 | 正常路径 |

#### TC04: 空文件

| 项目 | 内容 |
|------|------|
| 输入 | 空文件 |
| 预期 | has_next() == false |
| 类型 | 边界条件 |

#### TC05: 注释行跳过

| 项目 | 内容 |
|------|------|
| 输入 | `#track name=test\nchr1\t0\t100\n` |
| 预期 | 跳过第一行，正常解析第二行 |
| 类型 | 正常路径 |

#### TC06: 列数不一致

| 项目 | 内容 |
|------|------|
| 输入 | 首行 3 列，第二行 6 列 |
| 预期 | 第二行按 3 列解析（截断模式），或标记错误 |
| 类型 | 非法输入 |

#### TC07: BED3 写入

| 项目 | 内容 |
|------|------|
| 输入 | GenomicRegion([chr1, 0, 100), "gene1") |
| 预期 | 输出 `chr1\t0\t100\tgene1\n` |
| 类型 | 正常路径 |

---

## 6. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 5.0.1 | 避免 magic number | BED 列数常量使用 constexpr |
| Rule 8.0.2 | 禁止隐式转换 | parse_uint32 / parse_double 显式转换 |
| Rule 18.0.1 | 禁用异常 | 解析失败返回部分填充的对象 |
