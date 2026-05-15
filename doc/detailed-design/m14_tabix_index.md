# M14: TabixIndex 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.15
> **所属层**: Index & Compression

---

## 1. 模块概述

### 1.1 职责

为坐标排序的 BGZF 压缩文件构建和查询 Tabix 索引。Tabix 是基因组区间快速查询的标准索引格式（.tbi 文件）。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/index/tabix_index.h` |
| 实现文件 | `src/index/tabix_index.cpp` |

### 1.3 依赖

- M13 (BGZFStream)
- STL

---

## 2. 背景：Binning 算法

Tabix 使用分层 binning 系统（与 UCSC Genome Browser 兼容）。基因组被划分为不同分辨率的 bin：

| 层级 | bin 大小 | 覆盖范围 |
|------|---------|---------|
| 0 | 512 Mb | 整个基因组 |
| 1 | 128 Mb | |
| 2 | 32 Mb | |
| 3 | 8 Mb | |
| 4 | 2 Mb | |
| 5 | 512 Kb | |
| 6 | 128 Kb | |
| 7 | 32 Kb | |
| 8 | 16 Kb | 最细粒度 |

bin 编号公式: `bin = ((start >> 14) & 0x1FFFFF) + level_offset[level]`

---

## 3. 数据结构设计

### 3.1 内部实现

```cpp
class TabixIndex::Impl {
    struct ChromIndex {
        std::vector<uint64_t> bin_offsets;     // 每个 bin → 虚拟偏移列表
        std::vector<uint64_t> linear_offsets;  // 线性索引 (每 16KB 一个偏移)
    };
    
    std::map<std::string, ChromIndex> m_chroms; // 染色体 → 索引
    std::string m_data_path;
    bool m_loaded;
};
```

### 3.2 索引结构（.tbi 文件）

```
Header:
  magic (4 bytes): "TBI\1"
  n_ref (4 bytes): 参考序列数
  format (4 bytes): 0=generic, 1=SAM, 2=VCF
  col_seq (4 bytes): 染色体列号
  col_beg (4 bytes): 起始列号
  col_end (4 bytes): 终止列号
  meta (4 bytes): 元数据字符数
  skip (4 bytes): 跳过的头部行数
  l_nm (4 bytes): 参考序列名长度和

Per reference:
  name (null-terminated)
  n_bin (4 bytes): bin 数量
  Per bin:
    bin (4 bytes): bin ID
    n_chunk (4 bytes): chunk 数量
    Per chunk:
      chunk_beg (8 bytes): 起始虚拟偏移
      chunk_end (8 bytes): 结束虚拟偏移
  n_intv (4 bytes): 线性索引区间数
  Per interval:
    ioff (8 bytes): 每 16KB 的虚拟偏移
```

---

## 4. 算法设计

### 4.1 build(data_path, tbi_path)

```
输入: data_path (string), tbi_path (string)
处理:
  1. 打开 BGZF 文件，逐行读取
  2. 对每条记录:
     - 提取 chrom, start, end
     - 计算 record 所属的 bin (从最细层级向上)
     - 记录每个 bin 中的 (virtual_offset_begin, virtual_offset_end)
  3. 构建线性索引: 每 16KB 区间记录一个虚拟偏移
  4. 写入 .tbi 文件
输出: Result
复杂度: O(n × log(genome_size / 16384))
```

### 4.2 bin 计算

```
function reg2bins(beg, end):
    bins = []
    end -= 1  // 转换为半开区间
    for level = 0 to 8:
        level_offset = cumulative_offset[level]
        bin_beg = (beg >> (14 + 3*(8-level))) + level_offset
        bin_end = (end >> (14 + 3*(8-level))) + level_offset
        for bin = bin_beg to bin_end:
            bins.append(bin)
    return bins
```

### 4.3 query(chrom, start, end)

```
输入: chrom (string), start (uint64_t), end (uint64_t)
处理:
  1. 从 m_chroms 中查找 chrom
  2. 若未找到 → 返回空 vector
  3. 计算查询区间所属的 bin 列表: reg2bins(start, end)
  4. 对每个 bin:
     - 遍历该 bin 的 chunk 列表
     - 若 chunk 与查询区间重叠 → 收集 chunk 偏移
  5. 合并、排序、去重偏移列表
  6. 使用线性索引过滤未覆盖区域
  7. 返回合并后的偏移列表
输出: vector<uint64_t> (虚拟偏移列表)
复杂度: O(log(genome_size/16384) + num_chunks)
```

### 4.4 load / save

与 .tbi 二进制文件格式兼容的序列化/反序列化。

---

## 5. 测试设计

### 5.1 测试用例

#### TC01: 构建索引 + 查询

| 项目 | 内容 |
|------|------|
| 输入 | 坐标排序的 BGZF VCF 文件 |
| 预期 | query 返回正确区域内的记录偏移 |
| 类型 | 正常路径 |

#### TC02: 查询空区域

| 项目 | 内容 |
|------|------|
| 输入 | 查询不存在记录的区间 |
| 预期 | 返回空 vector |
| 类型 | 边界条件 |

#### TC03: 查询不存在染色体

| 项目 | 内容 |
|------|------|
| 输入 | has_chrom("chrX") 在仅含 chr1 的索引上 |
| 预期 | has_chrom() == false |
| 类型 | 边界条件 |

#### TC04: 加载已有 .tbi 文件

| 项目 | 内容 |
|------|------|
| 输入 | 现成的 .tbi 索引文件 |
| 预期 | load()==true, chrom_list() 正确 |
| 类型 | 正常路径 |

#### TC05: 构建 + 保存 + 加载往返

| 项目 | 内容 |
|------|------|
| 输入 | build → save → load → query |
| 预期 | query 结果一致 |
| 类型 | 正常路径 |

---

## 6. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 5.0.1 | 避免 magic number | bin 偏移常量使用 constexpr |
| Rule 8.0.2 | 禁止隐式转换 | 位移运算显式类型 |
| Rule 9.1.1 | 固定宽度整数 | 文件偏移使用 uint64_t |
| Rule 18.0.1 | 禁用异常 | 错误通过 Result 返回 |
