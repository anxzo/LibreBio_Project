# M08: FASTQReader / FASTQWriter 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.9
> **所属层**: File Format I/O

---

## 1. 模块概述

### 1.1 职责

FASTQ 格式文件的流式读取与写入。自动检测 Phred 编码方案。

### 1.2 背景：FASTQ 格式

FASTQ 是测序数据（含质量信息）的标准格式。每条记录固定为 4 行：

```
@SEQ_ID optional description
ACGTACGT
+
IIIIIIII
```

| 行号 | 内容 | 标识 |
|------|------|------|
| 1 | 序列标识符 | 以 `@` 开头 |
| 2 | 序列字符串 | A/C/G/T/N |
| 3 | 分隔符 | 以 `+` 开头，可选重复 ID |
| 4 | 质量字符串 | 每个字符对应序列中同位置碱基的 Phred 质量分数 |

### 1.3 文件位置

| 类型 | 路径 |
|------|------|
| FASTQReader 头文件 | `include/libre_bio/io/fastq_reader.h` |
| FASTQReader 实现文件 | `src/io/fastq_reader.cpp` |
| FASTQWriter 头文件 | `include/libre_bio/io/fastq_writer.h` |
| FASTQWriter 实现文件 | `src/io/fastq_writer.cpp` |

### 1.4 依赖

- M01 (Sequence)
- M02 (QualityScore)
- STL

---

## 2. 数据结构设计

### 2.1 FASTQEntry

```cpp
struct FASTQEntry {
    Sequence sequence;       // 序列对象 (含 ID、序列字母)
    QualityScore quality;    // 质量分数对象
};
```

### 2.2 FASTQReader 内部实现 (PIMPL)

```cpp
class FASTQReader::Impl {
    static constexpr size_t kBufferSize = 65536;
    
    std::ifstream m_file;
    std::array<char, kBufferSize> m_buffer;
    size_t m_buffer_pos;
    size_t m_buffer_len;
    size_t m_bytes_read;
    bool m_eof;
    
    bool m_last_valid;       // 最后一次 next() 是否成功
};
```

---

## 3. 算法设计

### 3.1 next() — 四行组解析

```
输入: 无
前置条件: has_next() == true
处理:
  1. 读取第 1 行 (@ 行):
     - 跳过空行
     - 读取一行，若不以 '@' 开头 → 格式错误 → m_last_valid = false
     - 解析 ID: '@' 后到第一个空白字符
     - 解析描述: 第一个空白字符后（可为空）

  2. 读取第 2 行 (序列行):
     - 读取一行 → seq_str
     - 注意: 序列可能跨多行吗？ → 标准 FASTQ 不跨行，但某些变体允许
     - 本模块: 仅支持标准 4 行格式。若序列跨行 → 格式错误

  3. 读取第 3 行 (+ 行):
     - 读取一行，必须以 '+' 开头
     - 忽略该行内容

  4. 读取第 4 行 (质量行):
     - 读取一行 → qual_str
     - 质量行长度必须等于序列行长度
     - 若不等 → m_last_valid = false

  5. 构造对象:
     - alphabet 推断: 序列中是否含 'U'/'u'
     - Sequence(id, description, seq_str, alphabet)
     - QualityScore(qual_bytes)
       (Encoding 由 QualityScore 构造函数自动检测)

  6. m_last_valid = true
输出: FASTQEntry
复杂度: O(行总长度)
```

### 3.2 is_valid()

```
返回 m_last_valid。
若上次 next() 解析失败（格式错误），返回 false。
调用方应检查此值判断序列质量。
```

### 3.3 has_next()

```
返回 !m_eof（文件未结束）。
注意: 可能返回 true 但 next() 因格式错误而返回无效条目。
```

### 3.4 FASTQWriter::write(entry)

```
输入: entry (const FASTQEntry&)
处理:
  1. 写入 '@' + entry.sequence.id()
     - 若 description 非空: ' ' + entry.sequence.description()
     - '\n'
  2. 写入 entry.sequence.seq() + '\n'
  3. 写入 '+\n'（或 '+' + entry.sequence.id() + '\n'）
  4. 写入 entry.quality.scores() (原始字节) + '\n'
     注意: 写入原始 ASCII 字节，不做偏移转换
输出: void
```

---

## 4. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 文件打开失败 | 构造函数通过 Result 输出参数报错 |
| 不以 '@' 开头的行 | is_valid() 返回 false。跳过至下一个 '@' 行 |
| 序列行与质量行长度不匹配 | is_valid() 返回 false |
| 质量行包含非法字符 | QualityScore 构造函数标记为 kUnknown |

---

## 5. 测试设计

### 5.1 测试环境

- 测试文件: `test/io/fastq_reader_test.cpp`, `test/io/fastq_writer_test.cpp`
- 依赖: M01, M02

### 5.2 测试用例

#### TC01: 单条记录读取

| 项目 | 内容 |
|------|------|
| 输入 | `@SEQ1\nACGT\n+\nIIII\n` |
| 预期 | entry.sequence.id()=="SEQ1", seq()=="ACGT", quality.size()==4 |
| 类型 | 正常路径 |

#### TC02: 多条记录读取

| 项目 | 内容 |
|------|------|
| 输入 | 两条完整记录 |
| 预期 | 每条记录的 id 和序列正确 |
| 类型 | 正常路径 |

#### TC03: Phred 编码自动检测

| 项目 | 内容 |
|------|------|
| 输入 | 质量行为低 ASCII 范围 (Sanger) |
| 预期 | quality.encoding() == kSanger |
| 类型 | 正常路径 |

#### TC04: 序列行与质量行长度不匹配

| 项目 | 内容 |
|------|------|
| 输入 | `@SEQ1\nACGT\n+\nIII\n`（质量行只有 3 个字符） |
| 预期 | is_valid() == false |
| 类型 | 非法输入 |

#### TC05: 不以 @ 开头

| 项目 | 内容 |
|------|------|
| 输入 | `SEQ1\nACGT\n+\nIIII\n`（缺少 @） |
| 预期 | is_valid() == false |
| 类型 | 非法输入 |

#### TC06: 空文件

| 项目 | 内容 |
|------|------|
| 输入 | 空文件 |
| 预期 | has_next() == false |
| 类型 | 边界条件 |

#### TC07: 带描述的记录

| 项目 | 内容 |
|------|------|
| 输入 | `@SEQ1 desc here\nACGT\n+\nIIII\n` |
| 预期 | id()=="SEQ1", description()=="desc here" |
| 类型 | 正常路径 |

#### TC08: 性能基准

| 项目 | 内容 |
|------|------|
| 输入 | 10,000,000 条 FASTQ 记录 |
| 预期 | 解析吞吐 ≥ 100 MB/s（单线程，SSD） |
| 类型 | 性能基准 |

#### TC09: 写入 + 读回往返

| 项目 | 内容 |
|------|------|
| 输入 | 写入 100 条记录 → 读取检查 |
| 预期 | 序列 ID、序列值、质量值一致 |
| 类型 | 正常路径 |

---

## 6. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 5.0.1 | 避免 magic number | 缓冲区大小使用 constexpr |
| Rule 8.0.2 | 禁止隐式转换 | 质量值字节以 uint8_t 处理 |
| Rule 18.0.1 | 禁用异常 | 格式错误通过 is_valid() 标记 |
