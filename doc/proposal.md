# LibreBio Project — 需求文档 (Proposal)

> **版本**: 0.1.0
> **日期**: 2026-05-14
> **作者**: LibreBio Team

---

## 1. 项目概述

### 1.1 项目名称

**LibreBio** — 自由、开放、高性能的生物信息学工具集合。

### 1.2 项目愿景

打破生物信息学领域工具混乱、资源垄断、内存浪费严重、代码质量差、生态割裂的局面。让任何有志于生物信息领域的人都能用**自己的电脑**分析**自己的数据**。

### 1.3 项目定位

面向生物信息学研究人员的开源 C++ 工具集合。核心理念：

- **UNIX 哲学** — 每个工具只做一件事，做到极致。
- **零依赖** — 不依赖任何第三方运行时库。
- **高计算效率 / 高内存效率** — 以 C++17 实现流式处理、内存映射等高性能策略。
- **长期可维护** — 代码质量至上，遵守 MISRA C++:2023 编码规范。
- **易用性** — 统一的 CLI 接口，清晰的错误信息和日志输出。
- **可复现** — 结果可追溯、可验证。
- **高对接能力** — 标准格式兼容，可作为管道组件嵌入分析流程。

### 1.4 发起动机

本项目由一位非生物信息学背景的项目发起人创建，旨在以**项目管理者 / 开源社区组织者**身份推动，由开发者社区协作实现。不依赖个人学科背景，以工程方法论驱动生信工具建设。

---

## 2. 目标

1. 提供一套**统一的底层 C++ 数据结构库**，覆盖序列、基因组区间、注释、变异等生信核心数据模型。
2. 提供**标准文件格式的高速解析器/写入器**，支持 FASTA/FASTQ/BED/GFF/VCF/SAM。
3. 提供**索引与压缩基础设施**，包括 BGZF、Tabix 和序列索引。
4. 提供**统一的 CLI 框架**，支持子命令注册、参数解析、日志输出。
5. 在此框架之上，分阶段构建**具体分析工具**（序列处理、区间运算、格式转换、变异分析等）。
6. 整个项目以 C++17 标准编写，遵守 MISRA C++:2023 编码规范，CMake 构建，MIT 许可证。

---

## 3. 非目标（第一版明确不包含）

- 图形用户界面（GUI）。
- Web 服务 / REST API。
- 分布式计算 / 集群调度。
- 机器学习 / 深度学习模型训练。
- 实时测序数据流处理。
- Python/R 绑定（未来可扩展，但第一版不纳入）。

---

## 4. 技术决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 编程语言 | C++17 | MISRA C++:2023 基于 C++17；生态成熟，零运行时开销 |
| 编码规范 | MISRA C++:2023 | 全部 179 条规范，强制/必需/建议三级约束 |
| 构建系统 | CMake 3.16+ | C++ 生态事实标准，IDE 支持最好 |
| 开源许可证 | MIT | 最宽松，最大化社区接受度和商业兼容性 |
| 外部依赖 | 零依赖 | 不依赖任何第三方库，降低编译门槛和维护负担 |
| 测试框架 | Catch2 (header-only) | 轻量级，或自建简易测试框架（均满足零运行时依赖） |
| 文档 | Doxygen格式注释 + Markdown | 生成 API 参考 + 用户手册 |

---

## 5. 架构设计

### 5.1 总体架构层次

```
┌─────────────────────────────────────────────────────────────┐
│                       CLI Tools Layer                       │
│    seqkit    bedops    vcfops    samops    fastqc           │
│                      (未来版本)                              │
├─────────────────────────────────────────────────────────────┤
│                    CLI Framework Layer                       │
│         CLI::App    ToolRegistry    Logger                  │
├─────────────────────────────────────────────────────────────┤
│               Index & Compression Layer                     │
│    BGZFStream    TabixIndex    SequenceIndex                │
├─────────────────────────────────────────────────────────────┤
│               File Format I/O Layer                         │
│  FASTA R/W   FASTQ R/W   BED R/W   GFF R/W    VCF R/W     │
│                         SAM R/W                             │
├─────────────────────────────────────────────────────────────┤
│                   Core Data Types Layer                     │
│  ┌──────────────────┐  ┌──────────────────────────┐        │
│  │ Sequence         │  │ GenomicInterval           │        │
│  │ SequenceStore    │  │ GenomicRegion             │        │
│  │ QualityScore     │  │ IntervalTree              │        │
│  └──────────────────┘  └──────────────────────────┘        │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 第一层：核心数据类型 (Core Data Types)

#### Sequence
```
序列对象，存储一条生物序列（DNA/RNA/Protein）。
- 字段: ID (std::string), 描述 (std::string), 序列字符串 (std::string), 字母表类型 (enum)
- 方法: 长度、截取子序列、反向互补、GC含量、字母表校验
- 字母表: DNA (ATCGN), RNA (AUCGN), Protein (20种氨基酸)
```

#### QualityScore
```
Phred 质量分数对象。
- 字段: 分数数组 (std::vector<uint8_t>)
- 方法: 编码检测 (Sanger/Illumina 1.5+/Illumina 1.8+), 平均质量, Q30比例
- 注意: MISRA C++:2023 禁止隐式类型转换，编码检测须显式
```

#### SequenceStore
```
序列容器，批量管理多条序列。
- 内部: std::vector<Sequence> 或惰性加载
- 方法: 按ID查找、迭代器、统计（总数、总碱基数、N50）
- 特性: 支持流式加载，不要求一次性将所有序列读入内存
```

#### GenomicInterval
```
基因组区间: 染色体名 + 起始位置 + 终止位置 + 链方向。
- 字段: chrom (std::string), start (uint32_t), end (uint32_t), strand (enum {+, -, .})
- 方法: 交集、并集、合并、排序、覆盖度计算、距离
- 坐标系统: 0-based 半开区间 [start, end)
```

#### GenomicRegion
```
带元数据的基因组区域，继承或组合 GenomicInterval。
- 额外字段: name (std::string), score (double), 自定义属性 (std::map<std::string, std::string>)
- 用途: BED/GFF 格式中区间+注释的组合表示
```

#### IntervalTree
```
高效区间重叠查询的树数据结构。
- 时间复杂度: 构造 O(n log n)，查询 O(log n + k)，k为命中数
- 方法: 插入、删除、重叠查询、最近邻查询
- 内部实现: 基于红黑树的增强区间树（或分段树）
```

### 5.3 第二层：文件格式解析/写入 (File Format I/O)

所有 Reader/Writer 采用**流式设计**，通过迭代器模式逐个返回元素，不将整个文件加载到内存。

#### FASTAReader / FASTAWriter
```
输入:  .fa / .fasta / .fna / .faa 文件（或 stdin）
输出:  逐个 Sequence 对象

Reader:
- 支持多序列文件
- 支持 gzip 压缩输入（透明解压）
- 支持行宽自动检测（60/80/任意字符）

Writer:
- 可配置行宽（默认60字符）
- 支持 gzip 压缩输出
```

#### FASTQReader / FASTQWriter
```
输入:  .fq / .fastq 文件（或 stdin）
输出:  逐个 Sequence 对象（含质量值）

Reader:
- 自动检测 Phred 编码版本
- 四行一组校验（@开头、序列、+、质量值）
- 支持多路复用 barcode 识别

Writer:
- 行宽可配置
```

#### BEDReader / BEDWriter
```
输入:  .bed 文件（或 stdin）
输出:  逐个 GenomicRegion 对象

Reader:
- 支持 BED3 ~ BED12 等多种列数格式
- 自动检测列数并按默认字段映射
- 包含 BEDPE（双末端）格式变体

Writer:
- 可指定输出列数
```

#### GFFReader / GFFWriter
```
输入:  .gff / .gff3 / .gtf 文件（或 stdin）
输出:  逐个 GenomicRegion 对象（含丰富注释属性）

Reader:
- 支持 GFF2 / GFF3 / GTF 三种格式
- 解析 attributes 列为键值对
- 跳过以 # 开头的注释/元数据行

Writer:
- 区分 GFF3 和 GTF 输出
- attributes 字段格式可配
```

#### VCFReader / VCFWriter
```
输入:  .vcf / .vcf.gz / .bcf 文件（或 stdin）
输出:  逐个变异记录对象

Reader:
- 支持 VCF v4.0 / v4.1 / v4.2
- 解析 INFO 和 FORMAT 字段
- 支持 BGZF 压缩和 Tabix 索引查询

Writer:
- VCF 格式输出
```

#### SAMReader / SAMWriter
```
输入:  .sam / .bam 文件（或 stdin）
输出:  逐条比对记录对象

Reader:
- 支持 SAM 文本格式和 BAM 二进制格式
- 解析 CIGAR 字符串
- 解析 FLAG 位标记

Writer:
- SAM 文本和 BAM 二进制输出均可
```

### 5.4 第三层：索引与压缩 (Index & Compression)

#### BGZFStream
```
Blocked Gzip 压缩/解压流。
- 符合 BGZF 规范（SAM 规范附录）
- 块级随机访问
- 与标准 gzip 兼容（可被 gunzip 解压）
- 支持多线程压缩（可选）

实现策略:
- 使用 zlib 的 deflate/inflate，每块独立压缩
- 每块 ≤ 64KB 压缩前数据
```

#### TabixIndex
```
Tabix 索引：对坐标排序的 BGZF 文件进行区间快速查询。

- 输入: 按坐标排序的 .bgz 文件 + .tbi 索引文件
- 查询: 给定染色体+区间，返回该区域内的所有记录
- 支持: VCF, BED, GFF, SAM 格式
- 索引构建: 基于 binning 算法（与 UCSC bin 系统兼容）
```

#### SequenceIndex
```
序列索引（.fai 格式）：对 FASTA 文件进行随机访问。

- 记录: 每条序列的名称、长度、文件偏移量、行宽、每行碱基数
- 查询: 给定序列名 + 区间，直接 seek 到对应位置读取
- 兼容: samtools faidx 的 .fai 索引格式
```

### 5.5 第四层：CLI 框架 (CLI Framework)

#### CLI::App
```
命令行参数解析器。
- 子命令支持: 类似 git 的 <tool> <subcommand> 模式
- 参数类型: 位置参数、命名选项（--flag）、标志（-f）
- 自动帮助: --help / -h
- 自动版本: --version / -V
- 错误处理: 参数错误时清晰的错误提示 + 退出码
```

#### ToolRegistry
```
工具注册表。
- 动态注册: 各工具在初始化阶段注册自身（类似静态注册模式）
- 查找: 按子命令名称查找对应工具入口函数
- 列表: --help 列出所有已注册工具
```

#### Logger
```
统一日志/进度输出。
- 日志级别: ERROR, WARNING, INFO, DEBUG
- 输出目标: stderr（日志），stdout（数据）
- 进度条: 支持文本进度显示（在处理大型文件时）
```

### 5.6 第五层：CLI 工具 (未来版本)

| 工具名 | 功能 | 对应现有工具 |
|--------|------|-------------|
| `seqkit` | 序列统计、过滤、截取、格式转换 | SeqKit / Seqtk |
| `bedops` | 基因组区间交集、并集、合并、覆盖度 | BEDtools |
| `vcfops` | 变异过滤、注释、统计 | BCFtools / VCFtools |
| `samops` | 比对结果过滤、统计、格式转换 | SAMtools |
| `fastqc` | 测序数据质量报告 | FastQC / Fastp |

---

## 6. 分阶段实现计划

### Phase 0: 项目基础设施 (Week 1-2)

| 任务 | 输出 |
|------|------|
| CMake 工程骨架 | CMakeLists.txt, src/ include/ test/ doc/ 目录 |
| MISRA C++:2023 规则配置 | clang-tidy / cppcheck 配置，CI 规范检查 |
| CLI 框架：CLI::App | 参数解析器实现 |
| CLI 框架：ToolRegistry | 工具注册表实现 |
| CLI 框架：Logger | 日志输出实现 |
| 编译与 CI 验证 | 工具链确认，GitHub Actions 或其他 CI 配置 |

### Phase 1: 核心数据类型 (Week 3-6)

| 优先级 | 类 | 说明 |
|--------|-----|------|
| P0 | Sequence | 序列对象 + 字母表校验 |
| P0 | QualityScore | 质量分数 + Phred 编码检测 |
| P0 | SequenceStore | 序列容器 + 统计 |
| P1 | GenomicInterval | 基因组区间 + 集合运算 |
| P1 | GenomicRegion | 带注释的基因组区域 |
| P2 | IntervalTree | 区间树 + 重叠查询 |

### Phase 2: 基本格式 I/O (Week 7-10)

| 优先级 | 类 | 说明 |
|--------|-----|------|
| P0 | FASTAReader / FASTAWriter | 最常用的序列格式 |
| P0 | FASTQReader / FASTQWriter | 测序数据的标准格式 |
| P1 | BEDReader / BEDWriter | 基因组区间格式 |

### Phase 3: 高级格式 I/O + 索引 (Week 11-16)

| 优先级 | 类 | 说明 |
|--------|-----|------|
| P1 | BGZFStream | Blocked Gzip 压缩流 |
| P1 | GFFReader / GFFWriter | 基因注释格式 |
| P2 | SAMReader / SAMWriter | 比对格式（含 BAM 二进制） |
| P2 | VCFReader / VCFWriter | 变异格式 |
| P2 | TabixIndex | Tabix 索引（依赖 BGZFStream） |
| P2 | SequenceIndex | FASTA 序列索引 (.fai) |

### Phase 4+: CLI 工具实现 (Week 17+)

每个工具独立开发、独立发布，按社区需求优先级排定。

---

## 7. 目录结构

```
LibreBio/
├── CMakeLists.txt              # 顶层 CMake 配置
├── LICENSE                     # MIT 许可证
├── README.md                   # 项目说明
├── doc/
│   ├── proposal.md             # 本文档
│   ├── architecture.md         # 架构详细文档
│   ├── api/                    # Doxygen 生成的 API 文档
│   └── MISRA-CPP-2023-Guidelines.md
├── src/
│   ├── core/                   # 核心数据类型
│   │   ├── sequence.cpp
│   │   ├── sequence_store.cpp
│   │   ├── quality_score.cpp
│   │   ├── genomic_interval.cpp
│   │   ├── genomic_region.cpp
│   │   └── interval_tree.cpp
│   ├── io/                     # 文件格式 I/O
│   │   ├── fasta_reader.cpp
│   │   ├── fasta_writer.cpp
│   │   ├── fastq_reader.cpp
│   │   ├── fastq_writer.cpp
│   │   ├── bed_reader.cpp
│   │   ├── bed_writer.cpp
│   │   ├── gff_reader.cpp
│   │   ├── gff_writer.cpp
│   │   ├── vcf_reader.cpp
│   │   ├── vcf_writer.cpp
│   │   ├── sam_reader.cpp
│   │   └── sam_writer.cpp
│   ├── index/                  # 索引与压缩
│   │   ├── bgzf_stream.cpp
│   │   ├── tabix_index.cpp
│   │   └── sequence_index.cpp
│   └── cli/                    # CLI 框架
│       ├── app.cpp
│       ├── tool_registry.cpp
│       └── logger.cpp
├── include/
│   └── libre_bio/              # 公共头文件
│       ├── core/
│       ├── io/
│       ├── index/
│       └── cli/
├── test/                       # 单元测试
│   ├── core/
│   ├── io/
│   ├── index/
│   └── cli/
├── tools/                      # CLI 工具（Phase 4+）
│   ├── seqkit/
│   ├── bedops/
│   ├── vcfops/
│   ├── samops/
│   └── fastqc/
└── data/                       # 测试数据
    └── test_data/
```

---

## 8. 编码与质量规范

### 8.1 MISRA C++:2023 合规

- 以 `doc/MISRA-CPP-2023-Guidelines.md` 为最高编码准则。
- **Mandatory 规范** 零违反。
- **Required 规范** 原则上零违反，确有需要提供书面 Deviation 说明。
- **Advisory 规范** 尽力遵守。
- CI 中集成 `clang-tidy` 静态分析，检查 MISRA 合规性。

### 8.2 关键编码原则

1. **禁止裸 new/delete** — 使用 `std::unique_ptr`、`std::shared_ptr`、值语义。
2. **禁止 C 风格类型转换** — 使用 `static_cast`、`reinterpret_cast`（限制场景）。
3. **禁用异常** — `noexcept` + 返回错误码（MISRA C++:2023 Rule 18.0.1）。
4. **禁止全局变量** — 所有状态封装在类中。
5. **Rule of Five** — 如自定义了析构/拷贝/移动中的任一，则显式声明全部五个。
6. **无符号整数** — 位运算和长度/计数使用 `size_t`/`uint32_t`/`uint64_t`。
7. **const 正确性** — 不修改的参数声明 `const`，不修改成员的方法声明 `const`。

### 8.3 测试策略

- 每个公共类至少覆盖：正常路径、边界条件、非法输入。
- 测试数据使用小规模合成数据，不依赖外部文件。
- Phase 0 即建立测试框架和 CI 管道。

---

## 9. 性能目标

| 指标 | 目标 |
|------|------|
| FASTA 解析吞吐 | ≥ 200 MB/s（单线程，SSD） |
| FASTQ 解析吞吐 | ≥ 100 MB/s（单线程，SSD，含质量值解析） |
| 内存占用 | 流式处理模式下，常数级内存（非输入的倍数） |
| 二进制大小 | 最终可执行文件 < 50MB（不含测试数据） |

---

## 10. 成功标准

1. **Phase 1 完成**: 所有核心数据类型通过单元测试，可通过代码审查。
2. **Phase 2 完成**: FASTA/FASTQ/BED 读写可处理 GB 级真实数据，性能达标。
3. **Phase 3 完成**: 全部格式 I/O 就绪，BGZF/Tabix 索引可用。
4. **社区接纳**: 有 ≥1 个外部贡献者提交了被合并的 PR。
5. **文档完备**: API 文档、用户指南、示例代码齐全。

---

## 11. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| C++ 开发人才不足 | 进度慢 | 完善的文档和贡献指南降低门槛 |
| MISRA 规范过于严格 | 开发效率低 | Advisory 级别灵活处理，CI 分阶段上线 |
| 零依赖约束过严 | 重复造轮子 | 仅限第一版，后续可评估引入成熟的 header-only 库 |
| 社区活跃度不足 | 无人使用 | 从切实解决痛点的最小工具开始，逐步建立声誉 |

---

## 附录 A: 术语对照

| 术语 | 说明 |
|------|------|
| FASTA | 序列文件格式，以 `>` 开头标记序列 |
| FASTQ | 序列+质量值文件格式，以 `@` 开头 |
| BED | 基因组区间文件格式，Tab 分隔 |
| GFF/GTF | 基因注释文件格式 |
| VCF | 变异检测结果文件格式 |
| SAM/BAM | 序列比对结果文件格式（文本/二进制） |
| BGZF | Blocked Gzip Format，支持随机访问的压缩格式 |
| Phred Score | 碱基测序质量分数，Q = -10·log₁₀(P_error) |
| N50 | 序列组装质量指标，覆盖50%基因组的 contig 最小长度 |
| CIGAR | SAM 格式中的比对操作描述字符串 |
| SNP/Indel | 单核苷酸多态性 / 插入缺失标记 |
