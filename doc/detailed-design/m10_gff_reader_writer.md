# M10: GFFReader / GFFWriter 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.11
> **所属层**: File Format I/O

---

## 1. 模块概述

### 1.1 职责

GFF2 / GFF3 / GTF 格式文件的流式读取与写入。这三种格式用于存储基因组注释信息（基因位置、外显子、CDS 等）。

### 1.2 背景：GFF 格式族

GFF 全称 Generic Feature Format，主要有三种变体：

| 格式 | 分隔 | attributes 分隔符 | 常见后缀 |
|------|------|-------------------|----------|
| GFF2 | Tab | 空格 | .gff |
| GFF3 | Tab | `=` 分隔键值，`;` 分隔对 | .gff3 |
| GTF | Tab + 空格 | `; ` 分隔，属性值加引号 | .gtf |

所有格式均为 9 列（Tab 分隔）：
```
seqid  source  type  start  end  score  strand  phase  attributes
```

### 1.3 文件位置

| 类型 | 路径 |
|------|------|
| GFFReader 头文件 | `include/libre_bio/io/gff_reader.h` |
| GFFReader 实现文件 | `src/io/gff_reader.cpp` |
| GFFWriter 头文件 | `include/libre_bio/io/gff_writer.h` |
| GFFWriter 实现文件 | `src/io/gff_writer.cpp` |

### 1.4 依赖

- M04 (GenomicInterval)
- M05 (GenomicRegion)
- STL

---

## 2. 数据结构设计

### 2.1 GFFReader 内部实现

```cpp
class GFFReader::Impl {
    std::ifstream m_file;
    GFFFormat m_format;         // 检测到的格式
    std::vector<std::string> m_metadata;  // '#' 开头的注释行
    std::array<char, 65536> m_buffer;
};
```

### 2.2 坐标转换

GFF 格式使用 1-based 全闭坐标 [start, end]。读取时需转换为内部 0-based 半开坐标 [start-1, end)。

```
gff_start (1-based) → m_start = gff_start - 1
gff_end   (1-based) → m_end   = gff_end        // 半开: 包含原 gff_end
```

---

## 3. 算法设计

### 3.1 格式检测

```
输入: 第一行非注释行
处理:
  1. 按 '\t' 分割，检查是否为 9 列
  2. 分析 attributes 列 (第 9 列):
     - 含 '=' 且以 ';' 分隔 → GFF3
     - 含空格分隔键值，值加引号 → GTF
     - 其他 → GFF2
  3. 也可由用户通过构造参数显式指定
```

### 3.2 next() — 行解析

```
输入: 无
处理:
  1. 读取下一行
  2. 跳过 '#' 开头的行 → 存入 m_metadata
  3. 跳过空行
  4. 按 '\t' 分割为 9 列
  5. 解析各字段:
     - chrom   = fields[0]
     - source  = fields[1] → 存入 attributes["source"]
     - type    = fields[2] → 存入 attributes["type"]
     - start   = parse_uint32(fields[3]) - 1  // 1-based → 0-based
     - end     = parse_uint32(fields[4])       // 1-based 全闭 → 0-based 半开
     - score   = parse_double(fields[5])
     - strand  = parse_strand(fields[6])
     - phase   = fields[7] → 存入 attributes["phase"]
  6. 解析 attributes 列 (fields[8]):
     GFF3 方式:
       按 ';' 分割 → 每个片段按第一个 '=' 拆键值对
     GTF 方式:
       按 '; ' 分割 → 每个片段按第一个空格拆键值对，值去引号
  7. GenomicInterval interval(chrom, start, end, strand)
     GenomicRegion region(interval, attributes["ID"] 或 type)
  8. 将各属性写入 region.attributes()
输出: GenomicRegion
复杂度: O(行长度)
```

### 3.3 metadata()

返回所有 `#` 开头的注释行集合。供调用方保存元信息或重新写入。

### 3.4 GFFWriter::write(region)

```
输入: region (const GenomicRegion&)
处理:
  1. 从 attributes 中提取 source, type, phase
  2. 坐标转换: start+1 (0-based → 1-based)
  3. 按 9 列 Tab 分隔输出
  4. attributes 列格式化:
     GFF3: key=value;key=value
     GTF: key "value"; key "value"
输出: void
```

---

## 4. 测试设计

### 4.1 测试用例

#### TC01: GFF3 读取

| 项目 | 内容 |
|------|------|
| 输入 | `chr1\tsource\tgene\t1\t100\t.\t+\t.\tID=g1;Name=Gene1\n` |
| 预期 | chrom()=="chr1", start()==0, end()==100, attributes["ID"]=="g1" |
| 类型 | 正常路径 |

#### TC02: GTF 读取

| 项目 | 内容 |
|------|------|
| 输入 | 标准 GTF 9 列行 |
| 预期 | 正确解析 attributes（空格分隔键值） |
| 类型 | 正常路径 |

#### TC03: 注释行收集

| 项目 | 内容 |
|------|------|
| 输入 | `##gff-version 3\n#comment\nchr1\t...\n` |
| 预期 | metadata().size()==2 |
| 类型 | 正常路径 |

#### TC04: 坐标转换

| 项目 | 内容 |
|------|------|
| 输入 | GFF start=1, end=100 |
| 预期 | interval().start()==0, interval().end()==100, length()==100 |
| 类型 | 正常路径 |

#### TC05: GFF3 写入 + 读回

| 项目 | 内容 |
|------|------|
| 输入 | 写入 GenomicRegion → 用 GFFReader 读回 |
| 预期 | 字段一致 |
| 类型 | 正常路径 |

---

## 5. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 5.0.1 | 避免 magic number | 9 列数使用 constexpr |
| Rule 8.0.2 | 禁止隐式转换 | 坐标转换使用显式运算 |
| Rule 18.0.1 | 禁用异常 | 解析失败返回部分填充对象 |
