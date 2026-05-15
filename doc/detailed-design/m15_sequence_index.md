# M15: SequenceIndex 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.16
> **所属层**: Index & Compression

---

## 1. 模块概述

### 1.1 职责

FASTA 文件随机访问索引（.fai 格式）。记录每条序列的名称、长度、文件偏移和行宽信息，支持按序列名和区间进行高效 seek 访问。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/index/sequence_index.h` |
| 实现文件 | `src/index/sequence_index.cpp` |

### 1.3 依赖

仅 STL。

---

## 2. 数据结构设计

### 2.1 FAIIndexEntry

```cpp
struct FAIIndexEntry {
    std::string name;       // 序列名称
    uint64_t length;        // 序列长度 (碱基数，不含换行符)
    uint64_t offset;        // 序列数据在文件中的起始字节偏移
    uint32_t line_bases;    // 每行碱基数 (如 60)
    uint32_t line_bytes;    // 每行总字节数 (如 61, 含换行符)
};
```

### 2.2 SequenceIndex 内部实现

```cpp
class SequenceIndex {
private:
    std::vector<FAIIndexEntry> m_entries;      // 索引条目 (保持插入顺序)
    std::map<std::string, size_t> m_index;     // 名称 → entries 下标
};
```

### 2.3 .fai 文件格式

```
name\tlength\toffset\tline_bases\tline_bytes\n
```

示例:
```
chr1\t248956422\t100\t60\t61
chr2\t242193529\t248956779\t60\t61
```

---

## 3. 算法设计

### 3.1 build(fasta_path) — 构建索引

```
输入: fasta_path (string)
处理:
  1. 打开 FASTA 文件
  2. 循环读取:
     a. 若行为 '>' 开头:
        - 记录当前条目: offset = 下一行起始的文件位置
        - 解析 name: '>' 后到第一个空白字符
     b. 否则 (序列行):
        - 记录 line_bases = 当前行去换行后的长度
        - 记录 line_bytes = 当前行总长度 (含换行)
        - 累加 length
  3. 弹出最后一个条目 (或 push_back 时填充前一条目)
  4. 建立 name → index 映射
输出: Result
复杂度: O(fasta 文件行数)
```

具体流程:
```
打开文件 → 定位到第一个 '>'
对于每条序列:
  | 读取 '>' 行 → name = 解析名称
  | offset = 当前文件指针位置 (序列第一行起始)
  | length = 0
  | line_bases = 0, line_bytes = 0
  | 循环读取序列行:
  |   若行为 '>' 开头 → 完成当前序列, 开始下一条
  |   若 line_bases == 0 → 记录 line_bases, line_bytes
  |   length += line_bases (去除换行符后的长度)
  | 保存条目
```

### 3.2 find(seq_name) — 查找

```
输入: seq_name (string)
处理:
  1. 在 m_index 中查找 seq_name
  2. 找到 → 返回 &m_entries[idx]
  3. 未找到 → 返回 nullptr
输出: const FAIIndexEntry*
复杂度: O(log n)
```

### 3.3 随机访问流程

```
给定查询: 序列名="chr1", 区间=[start, end)
1. find("chr1") → FAIIndexEntry
2. 计算文件位置:
   start_line      = start / entry.line_bases
   start_line_pos  = start % entry.line_bases
   end_line        = end / entry.line_bases
   end_line_pos    = end % entry.line_bases

   file_start = entry.offset + start_line * entry.line_bytes + start_line_pos
   file_end   = entry.offset + end_line * entry.line_bytes + end_line_pos

3. 在文件中 seek 到 file_start，读取 (file_end - file_start) 字节
4. 去除换行符 → 返回序列
```

示例: 序列 "chr1", line_bases=60, line_bytes=61, offset=100, 查询 [50, 150)

```
start_line = 50/60 = 0, start_line_pos = 50%60 = 50 → file_start = 100 + 0*61 + 50 = 150
end_line   = 150/60 = 2, end_line_pos = 150%60 = 30 → file_end = 100 + 2*61 + 30 = 252
读取 [150, 252) = 102 字节
去除 2 个换行符 → 97 个碱基
(注意: 预期 100 个碱基 [50,150), 但行宽导致 3 个换行符)
修正: end 应为 100 + 2*61 + 30 + (150%60==0 ? 0 : 1) 根据换行符精细调整
```

---

## 4. 错误处理

| 场景 | 处理方式 |
|------|----------|
| find 未找到序列 | 返回 nullptr |
| 空索引 | count() == 0 |
| build 非 FASTA 文件 | 无 '>' 行 → 返回 0 条目的空索引 |
| .fai 文件损坏 | load() 返回 false |

---

## 5. 测试设计

### 5.1 测试用例

#### TC01: 构建索引

| 项目 | 内容 |
|------|------|
| 输入 | 含 3 条序列的 FASTA 文件 (每序列 300 bases, 行宽 60) |
| 预期 | count()==3, entry.length==300, line_bases==60, line_bytes==61 |
| 类型 | 正常路径 |

#### TC02: 查找存在的序列

| 项目 | 内容 |
|------|------|
| 输入 | find("chr1") |
| 预期 | 返回非 nullptr, entry.name=="chr1" |
| 类型 | 正常路径 |

#### TC03: 查找不存在的序列

| 项目 | 内容 |
|------|------|
| 输入 | find("chr999") |
| 预期 | 返回 nullptr |
| 类型 | 边界条件 |

#### TC04: 空索引查找

| 项目 | 内容 |
|------|------|
| 输入 | 空索引上 find("chr1") |
| 预期 | 返回 nullptr |
| 类型 | 边界条件 |

#### TC05: 保存 + 加载 .fai 文件

| 项目 | 内容 |
|------|------|
| 输入 | build → save → load → find |
| 预期 | 加载后 find 结果与原索引一致 |
| 类型 | 正常路径 |

#### TC06: 随机访问坐标计算

| 项目 | 内容 |
|------|------|
| 输入 | 序列长度 300, line_bases=60, line_bytes=61, offset=100, 查询 [50, 150) |
| 预期 | file_start=150, file_end≈252 (含 3 个换行符) |
| 类型 | 正常路径 |

#### TC07: 性能测试

| 项目 | 内容 |
|------|------|
| 输入 | 人类基因组 FASTA (3GB) 构建索引 |
| 预期 | build 在 2s 内完成 |
| 类型 | 性能基准 |

---

## 6. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 5.0.1 | 避免 magic number | 行宽/偏移使用 constexpr |
| Rule 8.0.2 | 禁止隐式转换 | 文件偏移显式 uint64_t |
| Rule 9.1.1 | 固定宽度整数 | offset/length 使用 uint64_t |
