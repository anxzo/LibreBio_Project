# LibreBio 概要设计文档

> **版本**: 0.1.0
> **日期**: 2026-05-14
> **作者**: LibreBio Team
> **对应需求**: doc/proposal.md v0.1.0

---

## 1. 引言

### 1.1 编写目的

本文档为 LibreBio 项目的概要设计文档。依据需求文档中定义的架构分层与功能目标，将系统分解为模块，定义各模块的公共接口、模块间调用关系与数据流。

本文档面向技术开发者，作为详细设计和编码阶段的依据。

### 1.2 范围

覆盖需求文档中 Phase 0 ~ Phase 3 的全部模块：核心数据类型、文件格式 I/O、索引与压缩、CLI 框架。CLI 工具层属于 Phase 4+，本文档仅描述其与框架层的对接方式。

### 1.3 参考文档

| 文档 | 路径 |
|------|------|
| 需求文档 | doc/proposal.md |
| MISRA C++:2023 编码规范 | doc/MISRA-CPP-2023-Guidelines.md |

---

## 2. 系统总体设计

### 2.1 系统架构分层

```
┌─────────────────────────────────────────────────────────────────┐
│                     CLI Tools Layer (Phase 4+)                   │
│    seqkit    bedops    vcfops    samops    fastqc                │
├─────────────────────────────────────────────────────────────────┤
│                     CLI Framework Layer                          │
│         CLI::App         ToolRegistry         Logger             │
├─────────────────────────────────────────────────────────────────┤
│                Index & Compression Layer                         │
│     BGZFStream         TabixIndex        SequenceIndex          │
├─────────────────────────────────────────────────────────────────┤
│                  File Format I/O Layer                           │
│  FASTA R/W   FASTQ R/W   BED R/W   GFF R/W   VCF R/W   SAM R/W │
├─────────────────────────────────────────────────────────────────┤
│                    Core Data Types Layer                         │
│  Sequence   SequenceStore   QualityScore   GenomicInterval      │
│  GenomicRegion   IntervalTree                                    │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 层间依赖规则

```
依赖方向: 上层依赖下层，下层不感知上层
          ┌──────────┐
          │ CLI Tools │  ──→  CLI Framework + I/O + Core
          ├──────────┤
          │CLI Frame │  ──→  仅依赖自身 (无下层依赖)
          ├──────────┤
          │  Index   │  ──→  Core Data Types
          ├──────────┤
          │  I/O     │  ──→  Core Data Types
          ├──────────┤
          │  Core    │  ──→  无内部依赖 (仅STL)
          └──────────┘
```

每层仅依赖其直接下层和 STL，不跨层调用。I/O 层和 Index 层为平级关系，Index 层可调用 I/O 层读取文件。

### 2.3 模块清单

| 编号 | 模块名 | 所属层 | 职责 |
|------|--------|--------|------|
| M01 | Sequence | Core | 单条生物序列的存储与操作 |
| M02 | QualityScore | Core | Phred 质量分数存储与编码检测 |
| M03 | SequenceStore | Core | 批量序列容器与统计 |
| M04 | GenomicInterval | Core | 基因组区间定义与集合运算 |
| M05 | GenomicRegion | Core | 带元数据的基因组区域 |
| M06 | IntervalTree | Core | 区间重叠查询树 |
| M07 | FASTAReader/Writer | I/O | FASTA 格式流式解析与写入 |
| M08 | FASTQReader/Writer | I/O | FASTQ 格式流式解析与写入 |
| M09 | BEDReader/Writer | I/O | BED 格式流式解析与写入 |
| M10 | GFFReader/Writer | I/O | GFF/GTF 格式流式解析与写入 |
| M11 | VCFReader/Writer | I/O | VCF 格式流式解析与写入 |
| M12 | SAMReader/Writer | I/O | SAM/BAM 格式流式解析与写入 |
| M13 | BGZFStream | Index | Blocked Gzip 压缩/解压流 |
| M14 | TabixIndex | Index | 坐标排序文件的区间索引 |
| M15 | SequenceIndex | Index | FASTA 文件随机访问索引 |
| M16 | CLI::App | CLI | 命令行参数解析 |
| M17 | ToolRegistry | CLI | 工具注册与查找 |
| M18 | Logger | CLI | 统一日志与进度输出 |

---

## 3. 模块详细设计

### 3.1 命名约定

所有公共类型与函数定义于命名空间 `libre_bio`。文件 I/O 模块使用 `libre_bio::io` 子命名空间，索引模块使用 `libre_bio::idx` 子命名空间，CLI 框架使用 `libre_bio::cli` 子命名空间。

---

### 3.2 M01: Sequence

**文件位置**: `include/libre_bio/core/sequence.h`, `src/core/sequence.cpp`

**职责**: 表示一条生物序列（DNA / RNA / Protein），提供序列属性查询和基本操作。

**类型定义**:

```cpp
namespace libre_bio {

enum class Alphabet : uint8_t {
    kDNA,
    kRNA,
    kProtein
};

class Sequence {
public:
    Sequence(std::string id, std::string description,
             std::string seq, Alphabet alphabet);

    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& description() const noexcept;
    [[nodiscard]] const std::string& seq() const noexcept;
    [[nodiscard]] Alphabet alphabet() const noexcept;

    [[nodiscard]] size_t length() const noexcept;
    [[nodiscard]] Sequence sub_seq(size_t start, size_t count) const;
    [[nodiscard]] Sequence reverse_complement() const;
    [[nodiscard]] double gc_content() const noexcept;
    [[nodiscard]] bool validate() const noexcept;

private:
    std::string m_id;
    std::string m_description;
    std::string m_seq;
    Alphabet m_alphabet;
};

} // namespace libre_bio
```

**接口说明**:

| 方法 | 前置条件 | 后置条件 | 异常 |
|------|----------|----------|------|
| constructor | seq 字符须符合 alphabet 对应字母表 | 构造不可变序列对象 | 无（校验由 validate 单独执行） |
| sub_seq(start, count) | start + count ≤ length() | 返回新 Sequence，共享 id/description | start+count 越界时返回空序列 |
| reverse_complement() | alphabet == kDNA 或 kRNA | 返回反向互补序列 | 非 DNA/RNA 时返回原序列副本 |
| gc_content() | alphabet == kDNA 或 kRNA | 返回 [0.0, 1.0] | 非 DNA/RNA 时返回 0.0 |
| validate() | 无 | 检查每个字符是否属于对应字母表 | 永不抛异常 |

**依赖**: 仅 STL。

---

### 3.3 M02: QualityScore

**文件位置**: `include/libre_bio/core/quality_score.h`, `src/core/quality_score.cpp`

**职责**: 存储 Phred 质量分数序列，自动检测编码方案。

**类型定义**:

```cpp
namespace libre_bio {

enum class PhredEncoding : uint8_t {
    kSanger,           // offset = 33
    kIllumina13,       // offset = 64 (Illumina 1.3-1.7)
    kIllumina15,       // offset = 64 (Illumina 1.5-1.7)
    kIllumina18,       // offset = 33 (Illumina 1.8+)
    kUnknown
};

class QualityScore {
public:
    explicit QualityScore(std::vector<uint8_t> scores);
    QualityScore(std::vector<uint8_t> scores, PhredEncoding encoding);

    [[nodiscard]] const std::vector<uint8_t>& scores() const noexcept;
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] PhredEncoding encoding() const noexcept;

    [[nodiscard]] double average_score() const noexcept;
    [[nodiscard]] double q30_ratio() const noexcept;
    [[nodiscard]] uint8_t operator[](size_t idx) const;

    static PhredEncoding detect_encoding(
        const std::vector<uint8_t>& scores) noexcept;

private:
    std::vector<uint8_t> m_scores;
    PhredEncoding m_encoding;
};

} // namespace libre_bio
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| detect_encoding(scores) | 静态方法，根据 ASCII 偏移量推断编码方案。检查 score 范围：[33,126] → Sanger/Illumina18；[64,126] → Illumina13/15 |
| average_score() | 返回所有分数算术均值 |
| q30_ratio() | 返回分数 ≥ 30 的碱基占比 (Q30 = 错误率 ≤ 0.1%) |

**依赖**: 仅 STL。

---

### 3.4 M03: SequenceStore

**文件位置**: `include/libre_bio/core/sequence_store.h`, `src/core/sequence_store.cpp`

**职责**: 批量管理 Sequence 对象，提供查找、迭代和统计功能。支持惰性加载模式。

**类型定义**:

```cpp
namespace libre_bio {

class SequenceStore {
public:
    SequenceStore() = default;

    void add(Sequence seq);
    void add(std::vector<Sequence> seqs);

    [[nodiscard]] const Sequence* find_by_id(
        const std::string& id) const noexcept;
    [[nodiscard]] size_t count() const noexcept;
    [[nodiscard]] size_t total_bases() const noexcept;
    [[nodiscard]] size_t n50() const noexcept;

    [[nodiscard]] auto begin() const noexcept;
    [[nodiscard]] auto end() const noexcept;
    [[nodiscard]] auto begin() noexcept;
    [[nodiscard]] auto end() noexcept;

    [[nodiscard]] bool empty() const noexcept;
    void reserve(size_t capacity);
    void clear() noexcept;

private:
    std::vector<Sequence> m_sequences;
};

} // namespace libre_bio
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| find_by_id(id) | 按序列 ID 查找，返回指针（未找到返回 nullptr） |
| total_bases() | 所有序列碱基总数 |
| n50() | 序列组装 N50 值：将所有序列按长度降序排列，累加达到总长 50% 时的序列长度 |

**依赖**: M01 (Sequence)、STL。

---

### 3.5 M04: GenomicInterval

**文件位置**: `include/libre_bio/core/genomic_interval.h`, `src/core/genomic_interval.cpp`

**职责**: 表示基因组上的一个坐标区间。采用 0-based 半开区间 [start, end)。

**类型定义**:

```cpp
namespace libre_bio {

enum class Strand : uint8_t {
    kForward  = 0,
    kReverse  = 1,
    kUnknown  = 2
};

class GenomicInterval {
public:
    GenomicInterval(std::string chrom, uint32_t start,
                    uint32_t end, Strand strand = Strand::kUnknown);

    [[nodiscard]] const std::string& chrom() const noexcept;
    [[nodiscard]] uint32_t start() const noexcept;
    [[nodiscard]] uint32_t end() const noexcept;
    [[nodiscard]] Strand strand() const noexcept;

    [[nodiscard]] uint32_t length() const noexcept;
    [[nodiscard]] bool overlaps(const GenomicInterval& other) const noexcept;
    [[nodiscard]] GenomicInterval intersect(
        const GenomicInterval& other) const;
    [[nodiscard]] bool contains(const GenomicInterval& other) const noexcept;
    [[nodiscard]] int64_t distance(
        const GenomicInterval& other) const noexcept;

    bool operator<(const GenomicInterval& other) const noexcept;
    bool operator==(const GenomicInterval& other) const noexcept;

private:
    std::string m_chrom;
    uint32_t m_start;
    uint32_t m_end;
    Strand m_strand;
};

} // namespace libre_bio
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| overlaps(other) | 同染色体 + 区间有交集时返回 true。不同染色体返回 false |
| intersect(other) | 返回交集区间；无交集时返回 start=end=0 的空区间 |
| distance(other) | 同染色体：重叠返回 0，否则返回最短间隔距离。不同染色体返回 INT64_MAX |
| operator< | 按 (chrom, start) 字典序比较 |

**依赖**: 仅 STL。

---

### 3.6 M05: GenomicRegion

**文件位置**: `include/libre_bio/core/genomic_region.h`, `src/core/genomic_region.cpp`

**职责**: 带元数据的基因组区域，对应 BED/GFF 格式中的行记录。

**类型定义**:

```cpp
namespace libre_bio {

class GenomicRegion {
public:
    GenomicRegion(GenomicInterval interval, std::string name,
                  double score = 0.0);

    [[nodiscard]] const GenomicInterval& interval() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] double score() const noexcept;

    void set_attribute(const std::string& key, const std::string& value);
    [[nodiscard]] const std::string* get_attribute(
        const std::string& key) const noexcept;
    [[nodiscard]] const std::map<std::string, std::string>&
        attributes() const noexcept;

private:
    GenomicInterval m_interval;
    std::string m_name;
    double m_score;
    std::map<std::string, std::string> m_attributes;
};

} // namespace libre_bio
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| set_attribute | 设置自定义属性（如 GFF 的 attributes 列解析结果） |
| get_attribute | 按 key 查找属性，未找到返回 nullptr |

**依赖**: M04 (GenomicInterval)、STL。

---

### 3.7 M06: IntervalTree

**文件位置**: `include/libre_bio/core/interval_tree.h`, `src/core/interval_tree.cpp`

**职责**: 高效区间重叠查询数据结构。基于红黑树增强实现。

**类型定义**:

```cpp
namespace libre_bio {

template <typename T>
class IntervalTree {
public:
    IntervalTree() = default;

    void insert(const GenomicInterval& interval, const T& data);
    void remove(const GenomicInterval& interval);
    void build(std::vector<std::pair<GenomicInterval, T>> entries);

    [[nodiscard]] std::vector<T> query_overlap(
        const GenomicInterval& query) const;
    [[nodiscard]] std::vector<T> query_nearest(
        const GenomicInterval& query, size_t k = 1) const;

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear() noexcept;

private:
    // 内部使用红黑树 + 子树最大终点值增强
    struct Node {
        GenomicInterval interval;
        T data;
        uint32_t max_end;
        // ...
    };
};

} // namespace libre_bio
```

**接口说明**:

| 方法 | 复杂度 | 说明 |
|------|--------|------|
| insert | O(log n) | 插入单个区间-数据对 |
| build | O(n log n) | 批量构造，比逐个 insert 更高效 |
| query_overlap | O(log n + k) | 返回所有与查询区间重叠的数据，k 为命中数 |
| query_nearest | O(log n + k) | 返回距离最近的 k 个区间 |

**使用约束**: 模板参数 T 必须支持拷贝构造。同一条染色体建一棵树，不支持跨染色体查询。

**依赖**: M04 (GenomicInterval)、STL。

---

### 3.8 M07: FASTAReader / FASTAWriter

**文件位置**: `include/libre_bio/io/fasta_reader.h`, `include/libre_bio/io/fasta_writer.h`, `src/io/fasta_reader.cpp`, `src/io/fasta_writer.cpp`

**职责**: FASTA 格式文件的流式读取与写入。

**FASTAReader 类型定义**:

```cpp
namespace libre_bio::io {

class FASTAReader {
public:
    explicit FASTAReader(const std::string& path);
    FASTAReader(const std::string& path, bool gzip_compressed);

    FASTAReader(const FASTAReader&) = delete;
    FASTAReader& operator=(const FASTAReader&) = delete;
    FASTAReader(FASTAReader&&) = default;
    FASTAReader& operator=(FASTAReader&&) = default;
    ~FASTAReader();

    [[nodiscard]] bool has_next() const;
    Sequence next();
    [[nodiscard]] size_t position() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::io
```

**迭代器模式**:

```cpp
// 使用示例
FASTAReader reader("input.fa");
while (reader.has_next()) {
    Sequence seq = reader.next();
    // 处理 seq
}
```

**FASTAWriter 类型定义**:

```cpp
namespace libre_bio::io {

class FASTAWriter {
public:
    explicit FASTAWriter(const std::string& path);
    FASTAWriter(const std::string& path,
                size_t line_width, bool gzip_compress);

    FASTAWriter(const FASTAWriter&) = delete;
    FASTAWriter& operator=(const FASTAWriter&) = delete;
    FASTAWriter(FASTAWriter&&) = default;
    FASTAWriter& operator=(FASTAWriter&&) = default;
    ~FASTAWriter();

    void write(const Sequence& seq);
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::io
```

**接口说明**:

| 类 | 方法 | 说明 |
|----|------|------|
| FASTAReader | constructor(path, gzip) | gzip=true 时透明解压 |
| FASTAReader | has_next() | 检查是否有下一条序列。达到 EOF 返回 false |
| FASTAReader | next() | 返回下一条 Sequence。has_next()==false 时行为未定义 |
| FASTAWriter | constructor(path, line_width, gzip) | line_width 默认 60；gzip=true 时透明压缩输出 |
| FASTAWriter | write(seq) | 写入一条序列，头部以 '>' 开头 |
| FASTAWriter | close() | 关闭文件句柄，刷新缓冲区 |

**依赖**: M01 (Sequence)、STL。

---

### 3.9 M08: FASTQReader / FASTQWriter

**文件位置**: `include/libre_bio/io/fastq_reader.h`, `include/libre_bio/io/fastq_writer.h`, `src/io/fastq_reader.cpp`, `src/io/fastq_writer.cpp`

**职责**: FASTQ 格式文件的流式读取与写入。

**FASTQReader 类型定义**:

```cpp
namespace libre_bio::io {

struct FASTQEntry {
    Sequence sequence;
    QualityScore quality;
};

class FASTQReader {
public:
    explicit FASTQReader(const std::string& path);

    FASTQReader(const FASTQReader&) = delete;
    FASTQReader& operator=(const FASTQReader&) = delete;
    FASTQReader(FASTQReader&&) = default;
    FASTQReader& operator=(FASTQReader&&) = default;
    ~FASTQReader();

    [[nodiscard]] bool has_next() const;
    FASTQEntry next();
    [[nodiscard]] bool is_valid() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::io
```

**FASTQWriter 类型定义**:

```cpp
namespace libre_bio::io {

class FASTQWriter {
public:
    explicit FASTQWriter(const std::string& path);
    FASTQWriter(const std::string& path, size_t line_width);

    FASTQWriter(const FASTQWriter&) = delete;
    FASTQWriter& operator=(const FASTQWriter&) = delete;
    FASTQWriter(FASTQWriter&&) = default;
    FASTQWriter& operator=(FASTQWriter&&) = default;
    ~FASTQWriter();

    void write(const FASTQEntry& entry);
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::io
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| FASTQReader::next() | 读取四行为一组（@行、序列行、+行、质量行），自动检测 Phred 编码，返回 Sequence + QualityScore |
| FASTQReader::is_valid() | 最后一次 next() 是否成功解析（四行组格式正确） |
| FASTQWriter::write(entry) | 以四行格式写入 |

**依赖**: M01 (Sequence)、M02 (QualityScore)、STL。

---

### 3.10 M09: BEDReader / BEDWriter

**文件位置**: `include/libre_bio/io/bed_reader.h`, `include/libre_bio/io/bed_writer.h`, `src/io/bed_reader.cpp`, `src/io/bed_writer.cpp`

**职责**: BED 格式文件的流式读取与写入。支持 BED3 ~ BED12 格式。

**BEDReader 类型定义**:

```cpp
namespace libre_bio::io {

class BEDReader {
public:
    explicit BEDReader(const std::string& path);

    BEDReader(const BEDReader&) = delete;
    BEDReader& operator=(const BEDReader&) = delete;
    BEDReader(FASTQReader&&) = default;
    BEDReader& operator=(BEDReader&&) = default;
    ~BEDReader();

    [[nodiscard]] bool has_next() const;
    GenomicRegion next();
    [[nodiscard]] size_t column_count() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::io
```

**BEDWriter 类型定义**:

```cpp
namespace libre_bio::io {

class BEDWriter {
public:
    explicit BEDWriter(const std::string& path);
    BEDWriter(const std::string& path, size_t num_columns);

    BEDWriter(const BEDWriter&) = delete;
    BEDWriter& operator=(const BEDWriter&) = delete;
    BEDWriter(BEDWriter&&) = default;
    BEDWriter& operator=(BEDWriter&&) = default;
    ~BEDWriter();

    void write(const GenomicRegion& region);
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::io
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| BEDReader::column_count() | 首行解析后即确定，后续行必须一致 |
| BEDReader::next() | 按列数映射到 GenomicRegion 字段。BED3: chrom/start/end/name(name=.)。BED6: +strand。BED12: +blockSizes/blockStarts 存入 attributes |
| BEDWriter::write(region) | BED3 输出 chrom/start/end/name；BED6 追加 strand/score |

**列数映射规则**:

| 列序 | BED3 | BED4 | BED5 | BED6 | BED12 |
|------|------|------|------|------|-------|
| 1 | chrom | chrom | chrom | chrom | chrom |
| 2 | start | start | start | start | start |
| 3 | end | end | end | end | end |
| 4 | - | name | name | name | name |
| 5 | - | - | score | score | score |
| 6 | - | - | - | strand | strand |
| 7-12 | - | - | - | - | thickStart, thickEnd, itemRgb, blockCount, blockSizes, blockStarts |

**依赖**: M04 (GenomicInterval)、M05 (GenomicRegion)、STL。

---

### 3.11 M10: GFFReader / GFFWriter

**文件位置**: `include/libre_bio/io/gff_reader.h`, `include/libre_bio/io/gff_writer.h`, `src/io/gff_reader.cpp`, `src/io/gff_writer.cpp`

**职责**: GFF2 / GFF3 / GTF 格式文件的流式读取与写入。

**GFFReader 类型定义**:

```cpp
namespace libre_bio::io {

enum class GFFFormat : uint8_t {
    kGFF2,
    kGFF3,
    kGTF
};

class GFFReader {
public:
    explicit GFFReader(const std::string& path);
    GFFReader(const std::string& path, GFFFormat format);

    GFFReader(const GFFReader&) = delete;
    GFFReader& operator=(const GFFReader&) = delete;
    GFFReader(GFFReader&&) = default;
    GFFReader& operator=(GFFReader&&) = default;
    ~GFFReader();

    [[nodiscard]] bool has_next() const;
    GenomicRegion next();
    [[nodiscard]] GFFFormat format() const noexcept;
    [[nodiscard]] const std::vector<std::string>& metadata() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::io
```

**GFFWriter 类型定义**:

```cpp
namespace libre_bio::io {

class GFFWriter {
public:
    explicit GFFWriter(const std::string& path, GFFFormat format);

    GFFWriter(const GFFWriter&) = delete;
    GFFWriter& operator=(const GFFWriter&) = delete;
    GFFWriter(GFFWriter&&) = default;
    GFFWriter& operator=(GFFWriter&&) = default;
    ~GFFWriter();

    void write(const GenomicRegion& region);
    void write_metadata(const std::string& line);
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::io
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| GFFReader::metadata() | 返回 '#' 开头的注释行集合，供调用方保存元信息 |
| GFFReader::next() | 解析 9 列，attributes 列按 ';' 和 '=' 拆分为键值对存入 GenomicRegion::attributes，跳过 '#' 行 |
| GFFWriter::write_metadata | 写入注释/元数据行 |
| GFFWriter::write | GFF3 时 attributes 以 '=' 分隔键值；GTF 时以 ' ' 分隔 |

**依赖**: M04 (GenomicInterval)、M05 (GenomicRegion)、STL。

---

### 3.12 M11: VCFReader / VCFWriter

**文件位置**: `include/libre_bio/io/vcf_reader.h`, `include/libre_bio/io/vcf_writer.h`, `src/io/vcf_reader.cpp`, `src/io/vcf_writer.cpp`

**职责**: VCF v4.0/4.1/4.2 格式文件的流式读取与写入。支持 BGZF 压缩输入和 Tabix 索引区域查询。

**类型定义**:

```cpp
namespace libre_bio::io {

struct VCFRecord {
    std::string chrom;
    uint64_t pos;                        // 1-based
    std::string id;
    std::string ref;
    std::string alt;
    double qual;
    std::string filter;
    std::map<std::string, std::string> info;
    std::vector<std::string> format_fields;
    std::vector<std::map<std::string, std::string>> samples;
};

class VCFReader {
public:
    explicit VCFReader(const std::string& path);

    VCFReader(const VCFReader&) = delete;
    VCFReader& operator=(const VCFReader&) = delete;
    VCFReader(VCFReader&&) = default;
    VCFReader& operator=(VCFReader&&) = default;
    ~VCFReader();

    [[nodiscard]] bool has_next() const;
    VCFRecord next();
    [[nodiscard]] const std::vector<std::string>& header_lines() const noexcept;
    [[nodiscard]] size_t sample_count() const noexcept;

    // 区域查询（需配合 TabixIndex）
    void set_region(const std::string& chrom,
                    uint64_t start, uint64_t end);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class VCFWriter {
public:
    explicit VCFWriter(const std::string& path);

    VCFWriter(const VCFWriter&) = delete;
    VCFWriter& operator=(const VCFWriter&) = delete;
    VCFWriter(VCFWriter&&) = default;
    VCFWriter& operator=(VCFWriter&&) = default;
    ~VCFWriter();

    void write_header(const std::vector<std::string>& lines);
    void write(const VCFRecord& record);
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::io
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| VCFReader::header_lines() | 返回 '##' 开头的元数据行 |
| VCFReader::sample_count() | 从 #CHROM 行解析样本数量 |
| VCFReader::set_region(chrom, start, end) | 设置后，has_next/next 仅返回该区域内的记录。依赖 M13 和 M14 |
| VCFWriter::write_header | 先写入所有 '##' 行和 '#CHROM' 行，再逐条写入记录 |

**依赖**: M13 (BGZFStream)、M14 (TabixIndex)、STL。

---

### 3.13 M12: SAMReader / SAMWriter

**文件位置**: `include/libre_bio/io/sam_reader.h`, `include/libre_bio/io/sam_writer.h`, `src/io/sam_reader.cpp`, `src/io/sam_writer.cpp`

**职责**: SAM 文本格式和 BAM 二进制格式的流式读取与写入。

**类型定义**:

```cpp
namespace libre_bio::io {

enum class SAMFormat : uint8_t {
    kSAM,
    kBAM
};

struct SAMRecord {
    std::string qname;
    uint16_t flag;
    std::string rname;
    uint64_t pos;                       // 1-based
    uint8_t mapq;
    std::string cigar;
    std::string rnext;
    uint64_t pnext;
    int64_t tlen;
    std::string seq;
    std::string qual;
    std::map<std::string, std::string> tags;
};

class SAMReader {
public:
    explicit SAMReader(const std::string& path);

    SAMReader(const SAMReader&) = delete;
    SAMReader& operator=(const SAMReader&) = delete;
    SAMReader(SAMReader&&) = default;
    SAMReader& operator=(SAMReader&&) = default;
    ~SAMReader();

    [[nodiscard]] bool has_next() const;
    SAMRecord next();
    [[nodiscard]] SAMFormat format() const noexcept;
    [[nodiscard]] const std::vector<std::string>& header_lines() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class SAMWriter {
public:
    explicit SAMWriter(const std::string& path, SAMFormat format);

    SAMWriter(const SAMWriter&) = delete;
    SAMWriter& operator=(const SAMWriter&) = delete;
    SAMWriter(SAMWriter&&) = default;
    SAMWriter& operator=(SAMWriter&&) = default;
    ~SAMWriter();

    void write_header(const std::vector<std::string>& lines);
    void write(const SAMRecord& record);
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::io
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| SAMReader::format() | 自动检测 SAM/BAM（检查文件头魔数 BAM: 0x1f 0x8b 0x08 0x04 或 SAM: '@' 开头） |
| SAMReader::header_lines() | 返回 '@' 开头的头部行 |
| SAMReader::next() | 解析 FLAG 为 uint16_t，CIGAR 为字符串，TAG 为键值对 |

**依赖**: M13 (BGZFStream，BAM 格式需要)、STL。

---

### 3.14 M13: BGZFStream

**文件位置**: `include/libre_bio/index/bgzf_stream.h`, `src/index/bgzf_stream.cpp`

**职责**: Blocked Gzip 压缩/解压流，支持块级随机访问。符合 BGZF 规范。

**类型定义**:

```cpp
namespace libre_bio::idx {

struct BGZFBlock {
    uint64_t virtual_offset;     // (block_offset << 16) | data_offset_within_block
    uint64_t uncompressed_size;
    std::vector<uint8_t> data;
};

class BGZFReader {
public:
    explicit BGZFReader(const std::string& path);

    BGZFReader(const BGZFReader&) = delete;
    BGZFReader& operator=(const BGZFReader&) = delete;
    ~BGZFReader();

    [[nodiscard]] bool read_line(std::string& line);
    [[nodiscard]] bool read_block(BGZFBlock& block);
    void seek(uint64_t virtual_offset);
    [[nodiscard]] uint64_t tell() const noexcept;
    [[nodiscard]] bool eof() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class BGZFWriter {
public:
    explicit BGZFWriter(const std::string& path);

    BGZFWriter(const BGZFWriter&) = delete;
    BGZFWriter& operator=(const BGZFWriter&) = delete;
    ~BGZFWriter();

    void write(const std::string& data);
    void flush_block();
    void close();
    [[nodiscard]] uint64_t tell() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::idx
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| BGZFReader::read_line(line) | 从当前虚拟偏移逐行读取，支持跨块读。返回 true 表示成功 |
| BGZFReader::read_block(block) | 读取一个完整 BGZF 块（≤64KB 解压后数据） |
| BGZFReader::seek(virtual_offset) | 跳转到指定虚拟偏移。virtual_offset 高 48 位为块偏移，低 16 位为块内偏移 |
| BGZFReader::tell() | 返回当前虚拟偏移 |
| BGZFWriter::write(data) | 写入数据到内部缓冲区 |
| BGZFWriter::flush_block() | 将缓冲区数据压缩为一个 BGZF 块并写入文件 |

**虚拟偏移编码**:
```
virtual_offset = (block_file_offset << 16) | data_offset_within_block
```
其中 block_file_offset 是 BGZF 块在文件中的起始字节偏移。

**依赖**: zlib（通过系统头文件 `<zlib.h>`，此为 POSIX 标准系统库，不计为外部依赖）。

---

### 3.15 M14: TabixIndex

**文件位置**: `include/libre_bio/index/tabix_index.h`, `src/index/tabix_index.cpp`

**职责**: 对坐标排序的 BGZF 压缩文件进行区间快速查询。兼容 .tbi 索引格式。

**类型定义**:

```cpp
namespace libre_bio::idx {

class TabixIndex {
public:
    TabixIndex() = default;

    bool load(const std::string& tbi_path,
              const std::string& data_path);
    void build(const std::string& data_path,
               const std::string& tbi_path);
    void save(const std::string& tbi_path) const;

    [[nodiscard]] std::vector<uint64_t> query(
        const std::string& chrom,
        uint64_t start,
        uint64_t end) const noexcept;

    [[nodiscard]] bool has_chrom(const std::string& chrom) const noexcept;
    [[nodiscard]] const std::vector<std::string>& chrom_list() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::idx
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| load(tbi_path, data_path) | 加载已有 .tbi 索引和对应数据文件 |
| build(data_path, tbi_path) | 为坐标已排序的 BGZF 文件构建 Tabix 索引 |
| query(chrom, start, end) | 返回落在 [start, end) 区间内的所有记录虚拟偏移列表。调用方使用 BGZFReader::seek 逐条读取 |
| has_chrom(chrom) | 检查索引中是否有该染色体数据 |

**查询流程**:
```
VCFReader.set_region("chr1", 1000, 2000)
  → TabixIndex.query("chr1", 1000, 2000)
    → 返回 virtual_offset 列表
  → VCFReader 按 offset 顺序 seek + 解析
```

**依赖**: M13 (BGZFStream)、STL。

---

### 3.16 M15: SequenceIndex

**文件位置**: `include/libre_bio/index/sequence_index.h`, `src/index/sequence_index.cpp`

**职责**: FASTA 文件随机访问索引 (.fai)。兼容 samtools faidx 格式。

**类型定义**:

```cpp
namespace libre_bio::idx {

struct FAIIndexEntry {
    std::string name;
    uint64_t length;
    uint64_t offset;
    uint32_t line_bases;
    uint32_t line_bytes;
};

class SequenceIndex {
public:
    SequenceIndex() = default;

    bool load(const std::string& fai_path);
    void build(const std::string& fasta_path);
    void save(const std::string& fai_path) const;

    [[nodiscard]] const FAIIndexEntry* find(
        const std::string& seq_name) const noexcept;
    [[nodiscard]] size_t count() const noexcept;

private:
    std::vector<FAIIndexEntry> m_entries;
    std::map<std::string, size_t> m_index;  // name → entries idx
};

} // namespace libre_bio::idx
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| build(fasta_path) | 扫描 FASTA 文件，记录每条序列的名称、长度、文件偏移、行宽 |
| find(seq_name) | 返回索引条目指针，包含序列在文件中的起始偏移和行宽信息 |
| save/load | 与 .fai 格式兼容的读写 |

**随机访问流程**:
```
1. SequenceIndex::find("chr1") → FAIIndexEntry.offset = 100, line_bases = 60, line_bytes = 61
2. 请求区间 [50, 150]:
   - 起始行 = 50 / 60 = 0, 行内偏移 = 50 % 60 = 50
   - 结束行 = 150 / 60 = 2, 行内偏移 = 150 % 60 = 30
   - 文件位置 = offset + 0*61 + 50 → offset + 2*61 + 30
3. seek + 读取对应字节范围
```

**依赖**: 仅 STL。

---

### 3.17 M16: CLI::App

**文件位置**: `include/libre_bio/cli/app.h`, `src/cli/app.cpp`

**职责**: 命令行参数解析器，支持子命令模式。

**类型定义**:

```cpp
namespace libre_bio::cli {

struct Option {
    std::string short_name;       // e.g. "-o"
    std::string long_name;        // e.g. "--output"
    std::string description;
    std::string default_value;
    bool required;
    bool is_flag;                 // 布尔标志，无值
};

struct SubCommand {
    std::string name;
    std::string description;
    std::vector<Option> options;
    std::vector<std::string> positional_args;
};

class App {
public:
    App(const std::string& name, const std::string& version);

    void add_sub_command(const SubCommand& cmd);
    [[nodiscard]] bool parse(int argc, const char* argv[]);

    [[nodiscard]] const std::string& sub_command_name() const noexcept;
    [[nodiscard]] const std::string& get_option(
        const std::string& name) const;
    [[nodiscard]] bool has_option(const std::string& name) const noexcept;
    [[nodiscard]] const std::vector<std::string>&
        positional_args() const noexcept;

    void print_help() const noexcept;
    void print_version() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace libre_bio::cli
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| parse(argc, argv) | 解析命令行。成功返回 true，失败时自动输出错误信息和帮助。自动响应 --help/-h、--version/-V |
| get_option(name) | 获取选项值。未提供时返回 default_value。强制选项未提供时 parse 已失败 |
| sub_command_name() | 返回解析到的子命令名 |
| positional_args() | 返回位置参数列表 |

**使用模式**:
```cpp
App app("libre_bio", "0.1.0");
SubCommand cmd{"seqkit", "序列处理工具",
    {{"", "input", "输入文件", "", true, false},
     {"o", "output", "输出文件", "stdout", false, false},
     {"v", "verbose", "详细输出", "", false, true}},
    {"input"}};
app.add_sub_command(cmd);
app.parse(argc, argv);
// 根据 sub_command_name() 分发到具体处理函数
```

**依赖**: 仅 STL。

---

### 3.18 M17: ToolRegistry

**文件位置**: `include/libre_bio/cli/tool_registry.h`, `src/cli/tool_registry.cpp`

**职责**: 动态注册和查找 CLI 工具入口函数。

**类型定义**:

```cpp
namespace libre_bio::cli {

using ToolEntryFunc = int (*)(int argc, const char* argv[]);

struct ToolEntry {
    std::string name;
    std::string description;
    std::string version;
    ToolEntryFunc func;
};

class ToolRegistry {
public:
    static ToolRegistry& instance() noexcept;

    void register_tool(const ToolEntry& entry);
    [[nodiscard]] const ToolEntry* find_tool(
        const std::string& name) const noexcept;
    [[nodiscard]] std::vector<const ToolEntry*> list_tools() const;

    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

private:
    ToolRegistry() = default;

    std::map<std::string, ToolEntry> m_tools;
};

// 静态注册宏
#define LIBRE_BIO_REGISTER_TOOL(name, desc, ver, func) \
    namespace { \
        struct Register_##func { \
            Register_##func() { \
                libre_bio::cli::ToolRegistry::instance() \
                    .register_tool({name, desc, ver, func}); \
            } \
        }; \
        static Register_##func g_register_##func; \
    }

} // namespace libre_bio::cli
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| instance() | 单例访问，线程安全由调用方保证 |
| register_tool(entry) | 注册工具。同名工具后注册的会覆盖前者 |
| find_tool(name) | 按名称查找，未找到返回 nullptr |
| list_tools() | 返回所有已注册工具，供 --help 列表显示 |

**使用示例**:
```cpp
// 在工具实现文件中
LIBRE_BIO_REGISTER_TOOL("seqkit", "序列处理工具", "0.1.0", seqkit_main);
```

**依赖**: 仅 STL。

---

### 3.19 M18: Logger

**文件位置**: `include/libre_bio/cli/logger.h`, `src/cli/logger.cpp`

**职责**: 统一日志输出与进度显示。

**类型定义**:

```cpp
namespace libre_bio::cli {

enum class LogLevel : uint8_t {
    kError   = 0,
    kWarning = 1,
    kInfo    = 2,
    kDebug   = 3
};

class Logger {
public:
    static Logger& instance() noexcept;

    void set_level(LogLevel level) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;

    void error(const std::string& msg);
    void warning(const std::string& msg);
    void info(const std::string& msg);
    void debug(const std::string& msg);

    void progress(uint64_t current, uint64_t total,
                  const std::string& label = "");

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();

    LogLevel m_level;
    bool m_is_tty;
};

#define LIBRE_BIO_LOG_ERROR(msg)   Logger::instance().error(msg)
#define LIBRE_BIO_LOG_WARNING(msg) Logger::instance().warning(msg)
#define LIBRE_BIO_LOG_INFO(msg)    Logger::instance().info(msg)
#define LIBRE_BIO_LOG_DEBUG(msg)   Logger::instance().debug(msg)

} // namespace libre_bio::cli
```

**接口说明**:

| 方法 | 说明 |
|------|------|
| error/warning/info/debug | 日志级别低于当前设置时不输出。始终输出到 stderr |
| progress(current, total, label) | 仅在 stderr 为 TTY 时输出进度条。非 TTY 时每 10% 输出一行百分比。输出到 stderr |

**日志级别过滤**:
```
set_level(kWarning) → 仅输出 error + warning
set_level(kDebug)   → 输出所有级别
```

**依赖**: 仅 STL。

---

## 4. 模块依赖关系

### 4.1 依赖矩阵

```
            M01 M02 M03 M04 M05 M06 M07 M08 M09 M10 M11 M12 M13 M14 M15 M16 M17 M18
M01  Seq     -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M02  Qual    -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M03  SeqSt   ●   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M04  GenInt  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M05  GenReg  -   -   -   ●   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M06  IntTree -   -   -   ●   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M07  FASTA   ●   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M08  FASTQ   ●   ●   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M09  BED     -   -   -   ●   ●   -   -   -   -   -   -   -   -   -   -   -   -   -
M10  GFF     -   -   -   ●   ●   -   -   -   -   -   -   -   -   -   -   -   -   -
M11  VCF     -   -   -   -   -   -   -   -   -   -   -   -   ●   ●   -   -   -   -
M12  SAM     -   -   -   -   -   -   -   -   -   -   -   -   ●   -   -   -   -   -
M13  BGZF    -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M14  Tabix   -   -   -   -   -   -   -   -   -   -   -   -   ●   -   -   -   -   -
M15  SeqIdx  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M16  CLI::App-   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M17  ToolReg -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
M18  Logger  -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
```

### 4.2 依赖图

```
                    ┌──────────────┐
                    │ M17 ToolReg  │ (独立)
                    ├──────────────┤
                    │ M16 CLI::App │ (独立)
                    ├──────────────┤
                    │ M18 Logger   │ (独立)
                    └──────────────┘

    ┌──────────┐     ┌──────────┐     ┌──────────┐
    │ M01 Seq  │◄────│ M03 StSt │     │ M02 Qual │
    └────┬─────┘     └──────────┘     └────┬─────┘
         │                                 │
    ┌────┴─────┐                     ┌────┴─────┐
    │M07 FASTA │                     │M08 FASTQ │
    └──────────┘                     └──────────┘

    ┌──────────┐     ┌──────────┐     ┌──────────┐
    │M04 GInt  │◄────│M05 GReg  │     │M06 ITree │
    └────┬─────┘     └────┬─────┘     └────┬─────┘
         │                 │                │
    ┌────┴─────┐     ┌────┴─────┐           │
    │M09 BED   │     │M10 GFF   │           │
    └──────────┘     └──────────┘     (M06独立)

    ┌──────────┐     ┌──────────┐     ┌──────────┐
    │M13 BGZF  │◄────│M14 Tabix │     │M15 SeqIdx│
    └────┬─────┘     └────┬─────┘     └──────────┘
         │                 │
    ┌────┴─────┐     ┌────┴─────┐
    │M12 SAM   │     │M11 VCF   │
    └──────────┘     └──────────┘
```

---

## 5. 关键数据流

### 5.1 流式文件读取通用流程

```
┌──────────┐    ┌──────────────┐    ┌──────────────┐
│  文件路径  │───→│ Reader构造函数 │───→│ has_next()?  │
└──────────┘    └──────────────┘    └──┬───────┬───┘
                                       │ Yes   │ No → 结束
                                       ▼       │
                                  ┌─────────┐  │
                                  │ next()  │  │
                                  └────┬────┘  │
                                       │       │
                                  ┌────▼────┐  │
                                  │ 处理记录 │──┘
                                  └─────────┘  (循环)
```

### 5.2 FASTA 解析流程

```
输入: ">seq1 description\nACGTACGT\nACGT\n>seq2\nTGCA\n"

1. 读字符 '>' → 标记序列开始
2. 读至 '\n' → 解析 ID + 描述: id="seq1", desc="description"
3. 读至下一个 '>' 或 EOF → 拼接序列行: seq="ACGTACGTACGT"
4. 构造 Sequence(id, desc, seq, kDNA)
5. yield Sequence

重复 1-5 直至 EOF
```

### 5.3 FASTQ 解析流程

```
输入: "@SEQ1\nACGT\n+\nIIII\n@SEQ2\nTGCA\n+\nJJJJ\n"

1. 读 '@' 行 → 解析 ID
2. 读序列行 → seq="ACGT"
3. 读 '+' 行 → 验证（可为空或重复ID）
4. 读质量行 → qual=[I,I,I,I]
5. 检测 Phred 编码: detect_encoding([I,I,I,I]) → kSanger (offset=33)
6. 构造 FASTQEntry{Sequence, QualityScore}
7. yield FASTQEntry

重复 1-7 直至 EOF
```

### 5.4 BGZF + Tabix 区域查询流程

```
VCFReader::set_region("chr1", 1000000, 2000000)
  │
  ├─→ TabixIndex::query("chr1", 1000000, 2000000)
  │     │
  │     ├─ 查找 chr1 的 bin 列表
  │     ├─ 二分查找起始 bin (1000000 所在的 16KB bin)
  │     ├─ 收集所有重叠 bin 的文件偏移
  │     └─ 合并排序偏移列表 → [voff_1, voff_2, ..., voff_n]
  │
  └─→ 对每个 virtual_offset:
        BGZFReader::seek(voff_i)
        BGZFReader::read_line(line)
        → 解析 VCFRecord
        → 若超出查询区间 → 终止
        → 否则 yield VCFRecord
```

### 5.5 SequenceIndex 随机访问流程

```
查询: 序列 "chr1", 区间 [50000, 50100]

1. SequenceIndex::find("chr1")
   → FAIIndexEntry{length=248956422, offset=100, line_bases=60, line_bytes=61}

2. 计算文件位置:
   start_line = 50000 / 60 = 833,   start_pos_in_line = 50000 % 60 = 20
   end_line   = 50100 / 60 = 835,   end_pos_in_line   = 50100 % 60 = 0
   file_start = 100 + 833 * 61 + 20 = 50913
   file_end   = 100 + 835 * 61 + 0  = 51035

3. seek(file_start), read(file_end - file_start)
4. 去除换行符 → 返回 "ACGT..."  (100 bases)
```

---

## 6. 错误处理策略

### 6.1 总体原则

遵循 MISRA C++:2023 Rule 18.0.1：**禁用 C++ 异常**。所有错误通过返回值传递。

### 6.2 错误码定义

```cpp
namespace libre_bio {

enum class ErrorCode : int32_t {
    kSuccess = 0,
    kFileNotFound = -1,
    kFileReadError = -2,
    kFileWriteError = -3,
    kInvalidFormat = -4,
    kInvalidArgument = -5,
    kOutOfRange = -6,
    kEndOfFile = -7,
    kIndexNotFound = -8,
    kInvalidEncoding = -9,
    kInternalError = -99
};

struct Result {
    ErrorCode code;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::kSuccess; }
};

} // namespace libre_bio
```

### 6.3 各层错误处理约定

| 层 | 策略 |
|----|------|
| Core | 构造函数不校验（由 validate() 显式检查）。越界访问返回空值或默认值。不产生错误码 |
| I/O | 所有 Reader::next() 前须检查 has_next()。文件打开失败由构造函数通过 Result 返回值报告（以输出参数形式）。格式错误时，next() 返回部分填充的对象，is_valid() 返回 false |
| Index | build()/load() 返回 Result。query()/find() 返回空结果（空 vector / nullptr）表示查询无命中 |
| CLI | parse() 失败时自动打印帮助并返回 false |

### 6.4 Reader 错误处理模式

```cpp
// 典型 Reader 构造 + 迭代模式
FASTAReader reader;
Result r = FASTAReader::open("input.fa", reader);
if (!r.ok()) {
    LIBRE_BIO_LOG_ERROR("打开文件失败: " + r.message);
    return static_cast<int>(r.code);
}
while (reader.has_next()) {
    Sequence seq = reader.next();
    // 处理 seq
}
```

---

## 7. 性能设计要点

| 设计点 | 策略 |
|--------|------|
| 流式 I/O | 所有 Reader 使用迭代器模式，内存占用为常数级。不将整个文件加载到内存 |
| 内存映射 | FASTAReader 和 SequenceIndex 对本地文件使用 mmap() 替代 read()，减少内核态拷贝 |
| 字符串避免拷贝 | Sequence.id() / seq() 返回 const 引用。sub_seq 使用 std::string_view 作为中间视图 |
| 动态分配最小化 | SequenceStore 使用 reserve() 预分配。IntervalTree 使用 build() 批量构造 |
| I/O 缓冲 | 所有 Reader/Writer 内部使用 64KB 缓冲区，减少系统调用 |

---

## 8. 构建与编译

### 8.1 CMake 目标

```
libre_bio_core    — 静态库：核心数据类型 (M01-M06)
libre_bio_io      — 静态库：文件格式 I/O (M07-M12)，链接 libre_bio_core
libre_bio_index   — 静态库：索引与压缩 (M13-M15)，链接 libre_bio_core
libre_bio_cli     — 静态库：CLI 框架 (M16-M18)
```

### 8.2 编译要求

- 标准: C++17
- 编译器: GCC 9+ / Clang 10+ / MSVC 2019+
- 系统库: zlib (仅 BGZFStream 需要)
- 构建系统: CMake 3.16+

---

## 9. 附录

### 9.1 模块与实现阶段映射

| 阶段 | 模块 |
|------|------|
| Phase 0 | M16 CLI::App, M17 ToolRegistry, M18 Logger |
| Phase 1 | M01 Sequence, M02 QualityScore, M03 SequenceStore, M04 GenomicInterval, M05 GenomicRegion, M06 IntervalTree |
| Phase 2 | M07 FASTA, M08 FASTQ, M09 BED |
| Phase 3 | M13 BGZFStream, M10 GFF, M12 SAM, M11 VCF, M14 TabixIndex, M15 SequenceIndex |

### 9.2 术语表

| 术语 | 说明 |
|------|------|
| BGZF | Blocked Gzip Format, 支持随机访问的压缩格式 |
| .fai | FASTA index 文件，记录序列名称、长度和文件偏移 |
| .tbi | Tabix index 文件，记录 BGZF 文件的区间索引 |
| Phred Score | 碱基测序质量分数，Q = -10·log₁₀(P_error) |
| N50 | 序列组装质量指标 |
| CIGAR | SAM 格式中的比对操作描述字符串 |
| virtual_offset | BGZF 中的虚拟文件偏移量，编码块偏移和块内偏移 |
| 0-based 半开区间 | 坐标表示法 [start, end)，start 从 0 开始，不包含 end |
