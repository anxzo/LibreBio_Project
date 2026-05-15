# M13: BGZFStream 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.14
> **所属层**: Index & Compression
> **依赖**: zlib (系统库)

---

## 1. 模块概述

### 1.1 职责

实现 BGZF（Blocked Gzip Format）压缩/解压流。BGZF 是标准 gzip 的变体，支持块级随机访问，是 BAM 格式和 Tabix 索引的基础。

### 1.2 背景：BGZF 规范

BGZF 将数据分割为独立的压缩块（每块 ≤ 64KB 原始数据）。每个块是独立且完整的 gzip 流，因此：
- 每个块可独立解压
- 支持从任意块开始随机访问（配合索引）
- 与标准 gunzip 兼容（可串联解压）

### 1.3 文件位置

| 类型 | 路径 |
|------|------|
| BGZFReader 头文件 | `include/libre_bio/index/bgzf_reader.h` |
| BGZFReader 实现文件 | `src/index/bgzf_reader.cpp` |
| BGZFWriter 头文件 | `include/libre_bio/index/bgzf_writer.h` |
| BGZFWriter 实现文件 | `src/index/bgzf_writer.cpp` |

### 1.4 依赖

- zlib（系统 POSIX 标准库，不计为第三方依赖）
- STL

---

## 2. 背景知识

### 2.1 虚拟偏移 (Virtual Offset)

BGZF 使用 64 位虚拟偏移编码块内位置：

```
virtual_offset = (block_offset << 16) | data_offset_within_block

其中:
  block_offset            = 文件中的 BGZF 块起始字节偏移 (48 位)
  data_offset_within_block = 块内解压数据的字节偏移 (16 位)
```

### 2.2 BGZF 块结构

每个 BGZF 块是一个独立的 gzip 成员：

```
[gzip header: 18 bytes]
  | ID1=0x1f, ID2=0x8b, CM=0x08 (deflate)
  | FLG=0x04 (extra field present)
  | MTIME=0, XFL=0, OS=0xff (unknown)
  |
  | XLEN (2 bytes) = 6
  | Extra subfield: SI1=0x42('B'), SI2=0x43('C'), SLEN=2, BSIZE=x
  |   BSIZE = 原始数据大小 (≤ 65536)
  |
[deflate compressed data]
[gzip trailer: 8 bytes]
  | CRC32, ISIZE (原始数据大小 mod 2^32)
```

---

## 3. BGZFReader 数据结构设计

### 3.1 内部实现 (PIMPL)

```cpp
class BGZFReader::Impl {
    std::ifstream m_file;
    std::string m_path;
    
    // 当前块
    std::vector<uint8_t> m_block_buffer;    // 解压后的当前块 (≤ 64KB)
    size_t m_block_pos;                      // 当前块内读取位置
    uint64_t m_block_offset;                 // 当前块在文件中的起始偏移
    
    uint64_t m_virtual_offset;              // 当前虚拟偏移
    bool m_eof;
    bool m_first_block;
};
```

---

## 4. BGZFReader 算法设计

### 4.1 虚拟偏移转换

```
file_offset = virtual_offset >> 16
block_data_pos = virtual_offset & 0xFFFF
```

### 4.2 read_block(block)

```
输入: block (BGZFBlock&, 输出参数)
处理:
  1. 读取并验证 gzip 头部 (18 字节)
  2. 读取 extra subfield，提取 BSIZE
  3. 读取压缩数据至 gzip trailer
  4. 验证 CRC32 和 ISIZE
  5. 解压: inflate() → block.data (≤ 64KB)
  6. 记录 block.virtual_offset = (file_pos_before_header << 16)
  7. block.uncompressed_size = 解压后大小
输出: bool (成功/失败)
复杂度: O(块大小)
```

### 4.3 read_line(line)

```
输入: line (string&, 输出参数)
处理:
  1. line.clear()
  2. 循环读取字符:
     - 若 m_block_pos >= m_block_buffer.size() → read_next_block()
     - 若无法读取下一个块 (EOF) → 返回 false
     - ch = m_block_buffer[m_block_pos++]
     - m_virtual_offset += 1 (仅低 16 位增加)
     - 若 ch == '\n' → 返回 true
     - line.push_back(ch)
  3. 行可以跨块: 一个块结束时没有 '\n'，继续读下一个块
输出: bool (成功读取完整行 → true)
```

### 4.4 seek(virtual_offset)

```
输入: virtual_offset (uint64_t)
处理:
  1. 解析: block_offset = virtual_offset >> 16
            block_pos = virtual_offset & 0xFFFF
  2. m_file.seekg(block_offset)
  3. read_block() 加载对应块
  4. m_block_pos = block_pos
  5. m_virtual_offset = virtual_offset
输出: void
复杂度: O(1) + 块解压开销
```

### 4.5 tell()

```
返回 m_virtual_offset
```

---

## 5. BGZFWriter 算法设计

### 5.1 write(data)

```
输入: data (const string&)
处理:
  1. 追加到内部缓冲区 m_write_buffer
  2. 若 m_write_buffer.size() >= 64KB → flush_block()
输出: void
```

### 5.2 flush_block()

```
输入: 无
处理:
  1. 将 m_write_buffer 压缩为一个 BGZF 块
     - deflate() 压缩数据
     - 构造 gzip 头部 + extra subfield (BSIZE = m_write_buffer.size())
     - 写入压缩数据 + CRC32 + ISIZE
  2. 写入文件
  3. 清空 m_write_buffer
输出: void
```

### 5.3 BGZFBlock 结构

```cpp
struct BGZFBlock {
    uint64_t virtual_offset;        // (block_offset << 16) | 0
    uint64_t uncompressed_size;     // 原始数据大小
    std::vector<uint8_t> data;      // 解压后数据
};
```

---

## 6. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 非 BGZF 文件 | 头部验证失败，返回 false |
| 块 CRC32 校验失败 | 返回 false，数据不可靠 |
| zlib inflate/deflate 错误 | 返回 false |
| seek 到无效位置 | 行为未定义（调用方负责） |

---

## 7. 测试设计

### 7.1 测试用例

#### TC01: 单块写入 + 读取

| 项目 | 内容 |
|------|------|
| 输入 | 写入 100 字节 → flush → 读取 |
| 预期 | 读回内容一致，tell() 正确 |
| 类型 | 正常路径 |

#### TC02: 多块写入

| 项目 | 内容 |
|------|------|
| 输入 | 写入 200KB（触发 3+ 块）→ 全部读回 |
| 预期 | 数据完整一致 |
| 类型 | 正常路径 |

#### TC03: 跨块行读取

| 项目 | 内容 |
|------|------|
| 输入 | 写入使行边界落在块边界上，读取该行 |
| 预期 | 完整读取该行 |
| 类型 | 边界条件 |

#### TC04: seek 跳转

| 项目 | 内容 |
|------|------|
| 输入 | 写入多个块 → seek 到第 2 块 → 读取 |
| 预期 | 读到第 2 块数据 |
| 类型 | 正常路径 |

#### TC05: tell 追踪

| 项目 | 内容 |
|------|------|
| 输入 | 写入 → flush → tell() |
| 预期 | virtual_offset 正确反映当前块位置 |
| 类型 | 正常路径 |

#### TC06: 空文件

| 项目 | 内容 |
|------|------|
| 输入 | 打开空文件 |
| 预期 | eof() == true |
| 类型 | 边界条件 |

---

## 8. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 5.0.1 | 避免 magic number | 块大小、偏移掩码使用 constexpr |
| Rule 8.0.2 | 禁止隐式转换 | 位移运算显式使用 uint64_t |
| Rule 18.0.1 | 禁用异常 | zlib 错误通过返回值报告 |
