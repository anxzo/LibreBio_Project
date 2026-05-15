# M16: CLI::App 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.17
> **所属层**: CLI Framework

---

## 1. 模块概述

### 1.1 职责

命令行参数解析器。支持子命令模式、命名选项（`--flag`）、短选项（`-f`）、位置参数。自动生成帮助和版本信息。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/cli/app.h` |
| 实现文件 | `src/cli/app.cpp` |

### 1.3 依赖

仅 STL。

---

## 2. 数据结构设计

### 2.1 配置结构

```cpp
struct Option {
    std::string short_name;      // 短选项名，如 "o" (对应 -o)
    std::string long_name;       // 长选项名，如 "output" (对应 --output)
    std::string description;     // 帮助文本
    std::string default_value;   // 默认值（未提供时使用）
    bool required;               // 是否为必填选项
    bool is_flag;                // 是否为布尔标志（无需值）
};

struct SubCommand {
    std::string name;            // 子命令名，如 "seqkit"
    std::string description;     // 命令描述
    std::vector<Option> options; // 选项列表
    std::vector<std::string> positional_args; // 位置参数名列表
};
```

### 2.2 App 内部实现

```cpp
class App::Impl {
    std::string m_name;                       // 应用名称
    std::string m_version;                    // 版本号
    std::vector<SubCommand> m_sub_commands;   // 已注册的子命令
    
    // 解析结果
    std::string m_sub_command_name;           // 当前子命令名
    std::map<std::string, std::string> m_parsed_options; // 选项名 → 值
    std::vector<std::string> m_positional_values; // 位置参数值
    bool m_parsed;
};
```

---

## 3. 算法设计

### 3.1 parse(argc, argv) — 核心解析

```
输入: argc (int), argv (const char*[])
处理:
  1. 若 argc < 2 → print_help(), 返回 false
  2. 检查 argv[1]:
     - "--help" / "-h" → print_help(), 返回 false
     - "--version" / "-V" → print_version(), 返回 false
     - 否则 → 作为子命令名，查找 m_sub_commands
  3. 若未找到子命令 → 打印 "未知命令: xxx", print_help(), 返回 false
  4. 解析选项和位置参数:
     i = 2
     while i < argc:
       若 argv[i] 以 "--" 开头:
         解析长选项: name = argv[i].substr(2)
         查找匹配的 Option
         若 is_flag → m_parsed_options[name] = "true"
         否则 → i++, m_parsed_options[name] = argv[i]
       若 argv[i] 以 "-" 开头且不是仅 "-":
         解析短选项: name = argv[i].substr(1)
         查找 short_name 匹配的 Option
         若 is_flag → m_parsed_options[long_name] = "true"
         否则 → i++, m_parsed_options[long_name] = argv[i]
       否则:
         位置参数 → m_positional_values.push_back(argv[i])
       i++
  5. 检查必填选项是否都已提供
  6. 对未提供的选项填充默认值
  7. 若必填项缺失 → 打印错误 + 帮助, 返回 false
  8. m_parsed = true, 返回 true
输出: bool (成功/失败)
复杂度: O(argc × m)，m = 选项数量
```

### 3.2 组合短选项

支持 `-abc` 等价于 `-a -b -c`（当 a, b, c 都是 flag 时）。

```
算法: 当检测到短选项以 "-" 开头且长度 > 2 时:
  对每个字符 ch in name:
    查找 short_name == ch 的 Option
    若该 Option 非 flag → 报错（组合短选项中不能有值选项）
    设 m_parsed_options[long_name] = "true"
```

### 3.3 get_option(name)

```
输入: name (string)
处理:
  1. 在 m_parsed_options 中查找
  2. 找到 → 返回 value
  3. 未找到 → 返回 ""（调用方应先用 has_option 检查）
输出: const string&
```

### 3.4 print_help()

```
输出格式:
  <name> v<version>
  用法: <name> <subcommand> [options] <positional_args>

  子命令:
    seqkit    序列处理工具
    bedops    基因组区间运算工具
    ...

  选项:
    -o, --output <file>   输出文件 (默认: stdout)
    -h, --help            显示帮助
    -V, --version         显示版本
```

---

## 4. 错误处理

| 场景 | 处理方式 |
|------|----------|
| 未知子命令 | 打印错误 + 帮助，返回 false |
| 未知选项 | 打印错误 + 帮助，返回 false |
| 必填选项缺失 | 打印错误 + 帮助，返回 false |
| 值选项缺少值 | 打印错误 + 帮助，返回 false |
| 重复选项 | 后者覆盖前者 |

---

## 5. 测试设计

### 5.1 测试用例

#### TC01: 基本子命令解析

| 项目 | 内容 |
|------|------|
| 输入 | `app parse("tool", "subcmd", "--input", "file.txt")` |
| 预期 | sub_command_name()=="subcmd", get_option("input")=="file.txt" |
| 类型 | 正常路径 |

#### TC02: --help

| 项目 | 内容 |
|------|------|
| 输入 | `tool --help` |
| 预期 | parse() 返回 false, 输出帮助信息 |
| 类型 | 正常路径 |

#### TC03: --version

| 项目 | 内容 |
|------|------|
| 输入 | `tool --version` |
| 预期 | parse() 返回 false, 输出版本信息 |
| 类型 | 正常路径 |

#### TC04: 必填选项缺失

| 项目 | 内容 |
|------|------|
| 输入 | 子命令有 required=true 的选项，但未提供 |
| 预期 | parse() 返回 false |
| 类型 | 非法输入 |

#### TC05: 短选项

| 项目 | 内容 |
|------|------|
| 输入 | `tool subcmd -o output.txt` |
| 预期 | get_option("output")=="output.txt" |
| 类型 | 正常路径 |

#### TC06: 标志选项

| 项目 | 内容 |
|------|------|
| 输入 | `tool subcmd --verbose` (is_flag=true) |
| 预期 | has_option("verbose")==true, get_option("verbose")=="true" |
| 类型 | 正常路径 |

#### TC07: 默认值

| 项目 | 内容 |
|------|------|
| 输入 | 选项有 default_value="default.txt"，但未提供 |
| 预期 | get_option("name")=="default.txt" |
| 类型 | 正常路径 |

#### TC08: 位置参数

| 项目 | 内容 |
|------|------|
| 输入 | `tool subcmd input1.txt input2.txt` |
| 预期 | positional_args().size()==2, [0]=="input1.txt" |
| 类型 | 正常路径 |

#### TC09: 未知子命令

| 项目 | 内容 |
|------|------|
| 输入 | `tool unknown_cmd` |
| 预期 | parse() 返回 false |
| 类型 | 非法输入 |

---

## 6. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 5.0.1 | 避免 magic number | 子命令索引使用 constexpr |
| Rule 8.0.2 | 禁止隐式转换 | argc/argv 安全处理 |
| Rule 18.0.1 | 禁用异常 | 错误通过返回值报告 |
