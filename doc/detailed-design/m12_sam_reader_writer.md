# M12: SAMReader / SAMWriter 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.13
> **所属层**: File Format I/O

---

## 1. 模块概述

### 1.1 职责

SAM（Sequence Alignment/Map）文本格式和 BAM 二进制格式的流式读取与写入。

### 1.2 背景

SAM 是存储序列比对结果的标准格式。BAM 是其 BGZF 压缩的二进制版本。

### 1.3 文件位置

| 类型 | 路径 |
|------|------|
| SAMReader 头文件 | `include/libre_bio/io/sam_reader.h` |
| SAMReader 实现文件 | `src/io/sam_reader.cpp` |
| SAMWriter 头文件 | `include/libre_bio/io/sam_writer.h` |
| SAMWriter 实现文件 | `src/io/sam_writer.cpp` |

### 1.4 依赖

- M13 (BGZFStream, BAM 格式需要)
- STL

---

## 2. 数据结构设计

### 2.1 SAMRecord

```cpp
struct SAMRecord {
    std::string qname;       // 查询名称 (read name)
    uint16_t flag;           // 位标记 (FLAG)
    std::string rname;       // 参考序列名称
    uint64_t pos;            // 1-based 比对位置
    uint8_t mapq;            // 比对质量 (MAPQ)
    std::string cigar;       // CIGAR 字符串
    std::string rnext;       // 配对读的参考序列
    uint64_t pnext;          // 配对读的位置
    int64_t tlen;            // 模板长度
    std::string seq;         // 序列
    std::string qual;        // 质量值
    std::map<std::string, std::string> tags;  // 可选标签
};
```

---

## 3. 算法设计

### 3.1 格式自动检测

```
SAMReader 构造函数:
  1. 读取前 4 字节
  2. 若为 0x1f 0x8b 0x08 0x04 → BAM (BGZF 魔数)
  3. 若首字符为 '@' → SAM 文本格式
  4. 否则 → 格式错误
```

### 3.2 SAM 文本解析 (next)

```
输入: 无
处理:
  1. 读取一行
  2. 若以 '@' 开头 → 存入 header_lines，返回 next()
  3. 按 '\t' 分割为 ≥11 列
  4. 解析 11 个必需字段
  5. 解析可选标签: TAG:TYPE:VALUE → tags
输出: SAMRecord
```

### 3.3 BAM 二进制解析

BAM 格式通过 BGZF 块存储。解析需按 BAM 规范逐字段解包二进制数据。核心是 BGZF 解压后的二进制解析。

### 3.4 SAMWriter

文本模式: Tab 分隔，11 列 + 可选标签。
BAM 模式: 构造 BAM 二进制记录 → BGZF 压缩写入。

---

## 4. 测试设计

### 4.1 测试用例

#### TC01: SAM 文本读取

| 项目 | 内容 |
|------|------|
| 输入 | 带有 @HD/@SQ 头部和比对行的 SAM 文本 |
| 预期 | header_lines 正确收集，比对行字段正确 |
| 类型 | 正常路径 |

#### TC02: BAM 读取

| 项目 | 内容 |
|------|------|
| 输入 | 标准 BAM 文件 |
| 预期 | format() == kBAM, 记录正确解析 |
| 类型 | 正常路径 |

#### TC03: FLAG 解析

| 项目 | 内容 |
|------|------|
| 输入 | flag=99 (= 0x63 = paired + properly paired + mate reverse + first in pair) |
| 预期 | flag() == 99, 按位判断各属性正确 |
| 类型 | 正常路径 |

#### TC04: CIGAR 解析

| 项目 | 内容 |
|------|------|
| 输入 | cigar="100M1I50M" |
| 预期 | cigar() == "100M1I50M" (作为字符串保留) |
| 类型 | 正常路径 |

#### TC05: 头部行收集

| 项目 | 内容 |
|------|------|
| 输入 | 3 行 @HD, @SQ, @PG 头部 + 比对行 |
| 预期 | header_lines().size() == 3 |
| 类型 | 正常路径 |

#### TC06: SAM 写入 + 读回

| 项目 | 内容 |
|------|------|
| 输入 | 写入 SAMRecord → 读回 |
| 预期 | 字段一致 |
| 类型 | 正常路径 |

---

## 5. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 5.0.1 | 避免 magic number | FLAG 位值使用命名常量 |
| Rule 18.0.1 | 禁用异常 | 解析失败返回部分填充对象 |
