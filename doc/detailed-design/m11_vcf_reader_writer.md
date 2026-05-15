# M11: VCFReader / VCFWriter 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.12
> **所属层**: File Format I/O

---

## 1. 模块概述

### 1.1 职责

VCF（Variant Call Format）v4.0/4.1/4.2 格式文件的流式读取与写入。支持 BGZF 压缩输入和 Tabix 索引区域查询。

### 1.2 背景

VCF 是存储基因变异（SNP、Indel、SV）检测结果的标准格式。由元数据头部（`##` 行）、列头（`#CHROM` 行）和数据行组成。

### 1.3 文件位置

| 类型 | 路径 |
|------|------|
| VCFReader 头文件 | `include/libre_bio/io/vcf_reader.h` |
| VCFReader 实现文件 | `src/io/vcf_reader.cpp` |
| VCFWriter 头文件 | `include/libre_bio/io/vcf_writer.h` |
| VCFWriter 实现文件 | `src/io/vcf_writer.cpp` |

### 1.4 依赖

- M13 (BGZFStream)
- M14 (TabixIndex)
- STL

---

## 2. 数据结构设计

### 2.1 VCFRecord

```cpp
struct VCFRecord {
    std::string chrom;                              // 染色体
    uint64_t pos;                                   // 1-based 位置
    std::string id;                                 // 变异 ID (如 rs123)
    std::string ref;                                // 参考等位基因
    std::string alt;                                // 替代等位基因 (, 分隔)
    double qual;                                    // 质量值 (Phred)
    std::string filter;                             // 过滤状态 (PASS 或原因)
    std::map<std::string, std::string> info;        // INFO 字段键值对
    std::vector<std::string> format_fields;         // FORMAT 字段列表
    std::vector<std::map<std::string, std::string>> samples; // 每个样本的数据
};
```

### 2.2 VCFReader 内部实现

```cpp
class VCFReader::Impl {
    BGZFReader m_bgzf_reader;                    // BGZF 读取器 (gzip 模式)
    std::ifstream m_file;                        // 文件流 (非压缩模式)
    bool m_is_bgzf;
    
    std::vector<std::string> m_header_lines;      // ## 元数据行
    std::vector<std::string> m_sample_names;      // 样本名列表
    size_t m_column_count;                        // 总列数
    
    // 区域查询
    std::unique_ptr<TabixIndex> m_tabix;
    std::vector<uint64_t> m_region_offsets;       // 区域偏移列表
    size_t m_region_offset_idx;
    bool m_has_region;
    std::string m_region_chrom;
    uint64_t m_region_start;
    uint64_t m_region_end;
};
```

---

## 3. 算法设计

### 3.1 头部解析

```
构造函数处理:
  1. 读取所有 ## 行 → m_header_lines
  2. 读取 #CHROM 行
     - 按 '\t' 分割
     - 前 8 列为固定列: CHROM, POS, ID, REF, ALT, QUAL, FILTER, INFO
     - 若第 9 列为 FORMAT → 后续列为样本名
     - 否则 → 第 9 列起为样本名
  3. 保存样本名到 m_sample_names
```

### 3.2 next() — 数据行解析

```
输入: 无
处理:
  1. 读取一行
  2. 按 '\t' 分割为 fields
  3. 解析必需字段 (前 8 列):
     - chrom = fields[0]
     - pos = parse_uint64(fields[1])
     - id = fields[2] ('.' → "")
     - ref = fields[3]
     - alt = fields[4] (',' 分隔多个等位基因，保持原字符串)
     - qual = parse_double(fields[5])
     - filter = fields[6]
  4. 解析 INFO 列 (fields[7]):
     - 按 ';' 分割
     - 每个片段: 若有 '=' → key=value; 否则 → key 存在但无值
  5. 解析 FORMAT 和样本列:
     - 若 FORMAT 存在: format_fields = split(fields[8], ':')
     - 对每个样本: 按 ':' 分割值 → 与 format_fields 配对 → map
  6. 构造 VCFRecord
输出: VCFRecord
复杂度: O(行长度 × 样本数)
```

### 3.3 set_region(chrom, start, end) — 区域查询

```
输入: chrom (string), start (uint64_t), end (uint64_t)
处理:
  1. 若未加载 Tabix 索引 → 尝试加载 {data_path}.tbi
  2. 调用 m_tabix.query(chrom, start, end) → 获取虚拟偏移列表
  3. 保存偏移列表到 m_region_offsets
  4. 排序偏移列表
  5. has_next() 按偏移列表顺序返回数据行
输出: void
前置条件: 文件为 BGZF 压缩且存在 .tbi 索引
```

### 3.4 VCFWriter::write_header

```
输入: header_lines (const vector<string>&)
处理:
  1. 写入所有 '##' 行
  2. 若提供了样本名 → 构造并写入 '#CHROM' 行
输出: void
```

### 3.5 VCFWriter::write(record)

```
输入: record (const VCFRecord&)
处理:
  1. 按 8 列必需字段 + 可选 FORMAT + 样本列 顺序
  2. Tab 分隔输出
  3. INFO 列格式化: key=value;key=value
  4. FORMAT 列: format_fields ':' 连接
  5. 每个样本: 对应值 ':' 连接
输出: void
```

---

## 4. 测试设计

### 4.1 测试用例

#### TC01: VCF 基本读取

| 项目 | 内容 |
|------|------|
| 输入 | 最小 VCF（头部 + 单条 SNP 记录） |
| 预期 | pos, ref, alt 正确解析 |
| 类型 | 正常路径 |

#### TC02: 头部行收集

| 项目 | 内容 |
|------|------|
| 输入 | 5 行 ## 头部 + #CHROM 行 |
| 预期 | header_lines().size() == 5 |
| 类型 | 正常路径 |

#### TC03: INFO 字段解析

| 项目 | 内容 |
|------|------|
| 输入 | INFO="DP=100;AF=0.5;DB" |
| 预期 | info["DP"]=="100", info["AF"]=="0.5", info["DB"]=="" |
| 类型 | 正常路径 |

#### TC04: 多样本解析

| 项目 | 内容 |
|------|------|
| 输入 | VCF 包含 3 个样本 |
| 预期 | samples.size()==3, sample[0] 包含 FORMAT 对应键值 |
| 类型 | 正常路径 |

#### TC05: 区域查询

| 项目 | 内容 |
|------|------|
| 输入 | set_region("chr1", 1000, 2000) 后遍历 |
| 预期 | 仅返回该区域内记录 |
| 类型 | 正常路径 |

#### TC06: VCF 写入 + 读回

| 项目 | 内容 |
|------|------|
| 输入 | 写入 5 条记录 → 读回 |
| 预期 | 字段一致 |
| 类型 | 正常路径 |

---

## 5. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 5.0.1 | 避免 magic number | VCF 列数使用 constexpr |
| Rule 9.1.1 | 固定宽度整数 | pos 使用 uint64_t |
| Rule 18.0.1 | 禁用异常 | 解析失败返回部分填充对象 |
