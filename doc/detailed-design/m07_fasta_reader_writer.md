# M07: FASTAReader / FASTAWriter 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.8
> **所属层**: File Format I/O

---

## 1. 模块概述

### 1.1 职责

FASTA 格式文件的流式读取与写入。支持标准 FASTA 格式、多序列文件、gzip 压缩输入/输出。

### 1.2 背景：FASTA 格式

FASTA 格式是生物信息学中最基础的序列存储格式。每条序列由两行以上组成：

```
>seq_id optional description
ACGTACGTACGT
ACGT
>seq_id_2
TGCATGCA
```

- 以 `>` 开头的一行为**头部行**，包含序列 ID 和可选描述
- 后续行为**序列行**，可能跨多行（行宽通常为 60 或 80 字符）
- 序列可以包含大写或小写字母
- 空行通常被忽略

### 1.3 文件位置

| 类型 | 路径 |
|------|------|
| FASTAReader 头文件 | `include/libre_bio/io/fasta_reader.h` |
| FASTAReader 实现文件 | `src/io/fasta_reader.cpp` |
| FASTAWriter 头文件 | `include/libre_bio/io/fasta_writer.h` |
| FASTAWriter 实现文件 | `src/io/fasta_writer.cpp` |

### 1.4 依赖

- M01 (Sequence)
- STL
- 系统 zlib（仅 gzip 压缩模式）

### 1.5 接口摘要

```
FASTAReader(path, gzip_compressed)
  ├── has_next() → bool
  ├── next() → Sequence
  └── position() → size_t

FASTAWriter(path, line_width, gzip_compress)
  ├── write(seq)
  └── close()
```

---

## 2. FASTAReader 数据结构设计

### 2.1 PIMPL 内部实现

```cpp
class FASTAReader::Impl {
    static constexpr size_t kBufferSize = 65536;  // 64KB 读缓冲区
    
    std::ifstream m_file;              // 文件输入流 (非 gzip 模式)
    gzFile m_gz_file;                  // gzip 文件句柄 (gzip 模式)
    bool m_is_gzip;                    // 是否为 gzip 模式
    
    std::array<char, kBufferSize> m_buffer;  // 读缓冲区
    size_t m_buffer_pos;               // 缓冲区当前位置
    size_t m_buffer_len;               // 缓冲区有效数据长度
    
    size_t m_bytes_read;               // 已读取字节数 (文件位置)
    bool m_eof;                        // 文件结束标记
    
    // 预读的一行
    std::string m_peeked_line;         // 预读行 (用于判断 '>' 边界)
    bool m_has_peeked;
};
```

### 2.2 缓冲区设计

使用 64KB 双缓冲（读入缓冲区 + 预读行缓冲）。

```
文件 ──→ [64KB Buffer] ──→ [Line Parser] ──→ Sequence
                ↑                ↑
           ifstream/gzFile   read_char / read_line
```

读缓冲区大小选择 64KB 的理由:
- 与文件系统块大小（通常 4KB）对齐的倍数
- 与 L1 缓存大小（通常 32KB）匹配的适中大小
- 避免过大缓冲区导致 CPU 缓存失效

### 2.3 迭代器模式

Reader 内部维护一个状态机，has_next() 和 next() 交替调用:

```
状态: READY → has_next() 触发预读 → 若预读到 '>' → has_next=true
      EOFS   → has_next() 发现无更多 '>' → has_next=false
```

---

## 3. FASTAReader 算法设计

### 3.1 构造函数

```
输入: path (const string&), gzip_compressed (bool, 默认 false)
处理:
  1. 若 gzip_compressed:
     - m_gz_file = gzopen(path.c_str(), "rb")
     - 若 m_gz_file == nullptr → 返回 Result{kFileNotFound}
  2. 否则:
     - m_file.open(path, std::ios::binary)
     - 若 !m_file.is_open() → 返回 Result{kFileNotFound}
  3. 初始化缓冲区 (fill_buffer)
  4. 预读第一个字符，跳过空行
输出: Result (ErrorCode + message)
复杂度: O(1)
```

### 3.2 字符读取 — read_char()

```
输入: 无 (使用内部缓冲区)
处理:
  1. 若 m_buffer_pos >= m_buffer_len → fill_buffer()
  2. 若 m_eof → 返回 -1 (int)
  3. 返回 static_cast<unsigned char>(m_buffer[m_buffer_pos++])
输出: int (字符 0-255 或 -1 表示 EOF)
复杂度: O(1) 摊还
```

### 3.3 行读取 — read_line()

```
输入: line (string&, 输出参数)
处理:
  1. line.clear()
  2. 循环: ch = read_char()
     - 若 ch == -1 → break (EOF)
     - 若 ch == '\n' → break (行结束)
     - 若 ch == '\r' → 检查下一个是否为 '\n' (Windows CRLF) → 若是则跳过
     - line += static_cast<char>(ch)
  3. 若 line 为空且 ch == -1 → 到达 EOF
输出: bool (true = 成功读取到内容行, false = EOF)
复杂度: O(line_length)
```

### 3.4 has_next() — 序列边界检测

```
输入: 无
处理:
  1. 若 m_has_peeked 且 m_peeked_line 以 '>' 开头 → 返回 true
  2. 跳过空行: 循环读取下一个非空行
  3. 若到达 EOF → m_eof = true, 返回 false
  4. 若行以 '>' 开头:
     - m_peeked_line = 当前行
     - m_has_peeked = true
     - 返回 true
  5. 否则 (格式错误: 序列行直接出现)
     - m_eof = true 或标记格式错误
     - 返回 false
输出: bool
复杂度: O(line_length)
```

### 3.5 next() — 序列解析

```
输入: 无 (使用预读的头部行)
前置条件: has_next() == true
处理:
  1. 解析头部行 m_peeked_line:
     - 跳过第一个字符 '>'
     - 查找第一个空白字符，之前为 id，之后为 description
     - 去除 description 首尾空白
     - 若行仅为 ">" → id = "", description = ""

  2. 拼接序列行:
     seq.clear()
     循环:
       若 has_next_sequence_char()
         read_line(line)
         若 line 为空 → break
         seq += line
       否则 → break

  3. 字母表推断:
     - 检查序列中是否含有 'U'/'u'
       - 若有 → alphabet = kRNA
       - 否则 → alphabet = kDNA
     - 注意: 不自动推断 Protein（FASTA 可能包含氨基酸序列，但自动区分不可靠）

  4. 清除预读状态: m_has_peeked = false
  5. 构造: Sequence(id, description, seq, alphabet)
输出: Sequence
复杂度: O(序列长度)

空序列处理:
  遇到 ">empty\n>next" 时，seq="" 的序列正常返回。
  调用方可检查 length()==0。
```

### 3.6 性能优化：无拷贝拼接

序列行拼接时，预留容量避免多次 reallocation:

```cpp
// 第一种策略: 估计容量
seq.reserve(estimate_sequence_length());

// 第二种策略: 使用内部缓冲区先收集再拼接
// 概述选择第一种，理由是实现简单且预估通常准确
```

容量预估:
```
估计 = 第一行序列长度 × 预估行数
预估行数 = 默认行宽(60) 或从文件属性推断
```

### 3.7 position()

```
返回 m_bytes_read，表示已从文件中读取的字节数。
用于进度显示。
```

---

## 4. FASTAWriter 数据结构设计

### 4.1 PIMPL 内部实现

```cpp
class FASTAWriter::Impl {
    static constexpr size_t kDefaultLineWidth = 60;
    static constexpr size_t kBufferSize = 65536;
    
    std::ofstream m_file;              // 文件输出流
    gzFile m_gz_file;                  // gzip 输出句柄
    bool m_is_gzip;
    
    size_t m_line_width;               // 行宽（默认 60）
    std::string m_write_buffer;        // 写缓冲区 (64KB)
    bool m_closed;
};
```

---

## 5. FASTAWriter 算法设计

### 5.1 write(seq) — 写入序列

```
输入: seq (const Sequence&)
处理:
  1. 写入头部: ">" + seq.id()
     - 若 seq.description() 非空 → " " + seq.description()
     - "\n"
  2. 折叠序列: 将 seq.seq() 按 m_line_width 分块
     每块后添加 '\n'
  3. 写入序列块:
     for i = 0; i < seq_len; i += m_line_width:
         写入 seq.seq().substr(i, min(m_line_width, seq_len - i))
         写入 '\n'
  4. 写入缓冲区，满时自动刷新
输出: void
复杂度: O(seq.length())
```

### 5.2 close()

```
输入: 无
处理:
  1. 刷新写缓冲区
  2. 若 gzip 模式: gzclose(m_gz_file)
  3. 否则: m_file.close()
  4. m_closed = true
输出: void
```

---

## 6. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 文件打开失败 | 构造函数通过 Result 输出参数报错 |
| 非 FASTA 文件 | has_next() 读到非 '>' 开头行 → 标记格式错 + has_next() 返回 false |
| 格式错误的头部行 | id="" 或截断处理 |
| has_next()==false 时调用 next() | 行为未定义（调用方负责） |
| 写入失败 | 不检测（ofstream 默认不抛异常） |
| gzip 解压失败 | gzread 返回 -1，read_char 返回 -1（EOF） |

---

## 7. 测试设计

### 7.1 测试环境

- 测试文件: `test/io/fasta_reader_test.cpp`, `test/io/fasta_writer_test.cpp`
- 依赖: M01 (Sequence)
- 测试数据文件: `data/test_data/sample.fa`, `data/test_data/empty.fa`

### 7.2 FASTAReader 测试用例

#### TC01: 单序列读取

| 项目 | 内容 |
|------|------|
| 输入 | `>seq1 ACGT\nACGT\n` |
| 预期 | has_next()==true, next().id()=="seq1", seq()=="ACGTACGT", has_next()==false |
| 类型 | 正常路径 |

#### TC02: 多序列读取

| 项目 | 内容 |
|------|------|
| 输入 | `>seq1\nACGT\n>seq2\nTGCA\n` |
| 预期 | 第一次 next: id="seq1", seq="ACGT"; 第二次 next: id="seq2", seq="TGCA" |
| 类型 | 正常路径 |

#### TC03: 带描述的序列

| 项目 | 内容 |
|------|------|
| 输入 | `>seq1 Human chromosome 1\nACGT\n` |
| 预期 | id()=="seq1", description()=="Human chromosome 1" |
| 类型 | 正常路径 |

#### TC04: 空序列

| 项目 | 内容 |
|------|------|
| 输入 | `>empty\n>next_seq\nACGT\n` |
| 预期 | 第一次 next: id="empty", seq="", length()==0; 第二次: id="next_seq", seq="ACGT" |
| 类型 | 边界条件 |

#### TC05: 仅头部无序列

| 项目 | 内容 |
|------|------|
| 输入 | `>only_header\n` (文件以头部结尾，无后续) |
| 预期 | next().id()=="only_header", seq()=="", has_next()==false |
| 类型 | 边界条件 |

#### TC06: 空文件

| 项目 | 内容 |
|------|------|
| 输入 | 空文件 (0 字节) |
| 预期 | has_next()==false |
| 类型 | 边界条件 |

#### TC07: 序列跨多行

| 项目 | 内容 |
|------|------|
| 输入 | `>seq1\nACGT\nTGCA\nGGCC\n` |
| 预期 | next().seq()=="ACGTTGCAGGCC" |
| 类型 | 正常路径 |

#### TC08: 空行处理

| 项目 | 内容 |
|------|------|
| 输入 | `\n>seq1\nACGT\n\n>seq2\nTGCA\n` |
| 预期 | 忽略空行，正常解析两条序列 |
| 类型 | 正常路径 |

#### TC09: Windows 换行符 (CRLF)

| 项目 | 内容 |
|------|------|
| 输入 | `>seq1\r\nACGT\r\n` |
| 预期 | 正常解析 id="seq1", seq="ACGT" |
| 类型 | 正常路径 |

#### TC10: 批量读取性能

| 项目 | 内容 |
|------|------|
| 输入 | 100,000 条序列 FASTA 文件 (每条约 1KB) |
| 预期 | 解析吞吐 ≥ 200 MB/s（单线程，SSD） |
| 类型 | 性能基准 |

### 7.3 FASTAWriter 测试用例

#### TC11: 写入单序列

| 项目 | 内容 |
|------|------|
| 输入 | Sequence("seq1", "desc", "ACGTACGTACGT", kDNA) |
| 预期 | 输出文件包含 `>seq1 desc\nACGTACGT\nACGT\n`（行宽 60） |
| 类型 | 正常路径 |

#### TC12: 写入多序列

| 项目 | 内容 |
|------|------|
| 输入 | 写入 3 条序列，用 Reader 读回 |
| 预期 | 读写往返: 读回的序列数量、ID、序列值与原始一致 |
| 类型 | 正常路径 (往返测试) |

#### TC13: 自定义行宽

| 项目 | 内容 |
|------|------|
| 输入 | line_width=4, seq="ACGTACGT" |
| 预期 | 序列输出为 `ACGT\nACGT\n` |
| 类型 | 正常路径 |

#### TC14: gzip 压缩输出 + 读回

| 项目 | 内容 |
|------|------|
| 输入 | 写入 gzip 压缩 FASTA，用 FASTAReader(gzip_compressed=true) 读回 |
| 预期 | 内容一致 |
| 类型 | 正常路径 |

#### TC15: 空序列写入 + 读回

| 项目 | 内容 |
|------|------|
| 输入 | Sequence("empty", "", "", kDNA) |
| 预期 | 写入 `>empty\n`，读回 id="empty", seq="" |
| 类型 | 边界条件 |

---

## 8. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 5.0.1 | 避免 magic number | 缓冲区大小、默认行宽使用 constexpr |
| Rule 18.2.1 | new/delete 管控 | PIMPL 使用 unique_ptr |
| Rule 8.0.2 | 禁止隐式转换 | 字符读取显式转换为 unsigned char |
| Rule 9.1.1 | 固定宽度整数 | 偏移量使用 size_t |
| Rule 18.0.1 | 禁用异常 | 构造通过 Result 报错 |
